# Sector Door Dynamic Lighting Flicker Audit

## 1. Current Door Rendering Path

Procedural doors are drawn in `sources/sector_demo/SectorMeshPreview.cpp` by `SectorMeshPreview::DrawRuntimeDoors()` after static sector mesh batches and before runtime billboards. The call order is `SectorMeshPreview::Render()` -> `RenderDynamicSpotLightShadowMaps()` -> `DrawScene()` -> sector meshes -> `DrawRuntimeDoors()` -> `DrawRuntimeBillboards()` (`SectorMeshPreview.cpp:1710-1828`).

Doors are not drawn from a persistent `Mesh`. `DrawRuntimeDoors()` iterates ECS runtime objects with `SectorObjectTransform`, `SectorObject`, `SectorDoor`, `SectorDoorResolvedAnchor`, and `SectorDoorRender`, computes a slab box from the door transform, tangent, normal, width, height, and thickness, then emits six `RL_QUADS` faces through `DrawDoorFace()` (`SectorMeshPreview.cpp:1831-2071`). Each face supplies:

- position: `rlVertex3f()` world-space vertices (`SectorMeshPreview.cpp:2007-2068`).
- normal: one constant face normal via `rlNormal3f()` before the four vertices (`SectorMeshPreview.cpp:216-259`).
- UV: procedural `rlTexCoord2f()` using face dimensions as scale (`SectorMeshPreview.cpp:216-259`).
- color: `rlColor4ub()` with tint pre-multiplied by baked object-probe face lighting (`SectorMeshPreview.cpp:216-259`).
- lighting data: no separate lighting attribute; baked/static object lighting is encoded into vertex color per face.

Door drawing reuses the sector lightmap `material.shader`, loaded by `LoadPreviewMaterial()` from `SectorLightmapVs` and `SectorLightmapFs` (`SectorMeshPreview.cpp:1049-1124`). This shader has inputs for `vertexPosition`, `vertexNormal`, two UV sets, `vertexTangent`, and `vertexColor`; it forwards `fragWorldPosition` and `fragWorldNormal` directly from the supplied vertex position and normal (`SectorMeshPreview.cpp:262-289`). Therefore world position and normal are available per fragment for doors as long as the immediate-mode normal attribute is correctly delivered by rlgl.

The sector shader also declares lightmap, decal, dynamic-light, and dynamic spotlight shadow uniforms (`SectorMeshPreview.cpp:292-332`). `DrawRuntimeDoors()` disables lightmaps/AO/alpha test/decals with uniforms before drawing (`SectorMeshPreview.cpp:1845-1881`), uploads dynamic light and shadow uniforms (`SectorMeshPreview.cpp:1882-1909`), binds shadow maps if available (`SectorMeshPreview.cpp:1910-1921`), disables blending, disables backface culling, enables depth test/depth writes, and restores common render state afterward (`SectorMeshPreview.cpp:1923-2080`).

Render-order implications:

- Doors render after sectors, so they depth-test against already drawn sector geometry.
- Doors render before billboards, so door state must be restored before the billboard cutout shader path.
- Dynamic spotlight shadow maps are rendered before the main scene, but `RenderDynamicSpotLightShadowMaps()` renders only `meshes.sectorDrawRecords`; runtime doors are not shadow casters (`SectorMeshPreview.cpp:2401-2474`).
- Bloom source rendering is separate and only iterates sector draw records; doors are not bloom contributors (`SectorMeshPreview.cpp:2484-2565`).
- Debug overlays are outside this door draw path.

## 2. Current Door Static/Object-Probe Lighting

Doors receive static/baked lighting through `SectorObjectLighting`. On spawn, a door entity gets `SectorObjectLighting{SampleBakedObjectLighting(...)}` at its initial world position and current sector (`SectorRuntimeObjects.cpp:842-849`). During runtime updates, `UpdateSectorObjectBakedLightingSystem()` refreshes all object lighting samples from each object's current transform position and current sector (`SectorRuntimeObjects.cpp:1174-1184`).

Door draw then evaluates the baked object-probe ambient cube per face through `BakedDoorFaceLighting()`. That helper chooses one ambient-cube face by the dominant axis of the procedural face normal and returns that single color (`SectorMeshPreview.cpp:176-191`). `DrawDoorFace()` folds the selected color into the face vertex color (`SectorMeshPreview.cpp:216-259`).

The current sample is per object and then per face. It is not per vertex or per fragment. Ambient cube lookup is face-normal based. The result can look flat per face because each face's four vertices receive the same object-probe color and the shader only sees that color as `fragColor.rgb`; there is no interpolation between probe samples across the slab corners.

Later quality upgrade: sample object probes per door corner or per face corner, then pass those colors as vertex colors so the slab can interpolate static lighting spatially. That should be a separate quality task, not part of the first flicker fix.

## 3. Dynamic Light Pipeline For Sector Geometry

Runtime dynamic lights are represented by `SectorPreviewDynamicPointLightUniform` in `SectorDynamicPointLightSelection.h`. The shared cap is `MaxDynamicLights = 8`; dynamic spotlight shadow casters are capped at `MaxDynamicSpotLightShadowCasters = 2` (`SectorDynamicPointLightSelection.h:11-13`). Point and spot lights share the same selected dynamic light list. Spotlights include direction, inner/outer cone cosines, flicker fields, and optional shadow settings (`SectorDynamicPointLightSelection.h:16-45`).

`BuildSectorPreviewDynamicPointLightSources()` packs authored dynamic point lights and dynamic spotlights into source records, assigning an owner sector by querying `SectorCollisionWorld` when available (`SectorDynamicPointLightSelection.cpp:421-464`). `CollectSectorPreviewDynamicPointLightCandidates()` includes lights whose owner sector is visible or whose radius sphere overlaps relevant `SectorReceiverBounds` (`SectorDynamicPointLightSelection.cpp:466-497`). `SelectRankedSectorPreviewDynamicPointLights()` scores and ranks candidates against receiver bounds, with deterministic tie-breaking by light ID and hysteresis for previously selected lights (`SectorDynamicPointLightSelection.cpp:500-590`).

In `SectorMeshPreview::UpdateVisibilityDebug()`, dynamic candidates and selected lights are rebuilt from `dynamicPointLightSources`, `visibilityResult`, and `meshes.sectorReceiverBounds`; the same static receiver bounds are also used for shadow caster selection (`SectorMeshPreview.cpp:2731-2749`).

Sector rendering uploads dynamic light uniforms with `UploadDynamicPointLights()` (`SectorMeshPreview.cpp:850-925`). Uniform names are:

- `dynamicLightCount`
- `dynamicLightPositions[MAX_DYNAMIC_LIGHTS]`
- `dynamicLightColors[MAX_DYNAMIC_LIGHTS]`
- `dynamicLightRadii[MAX_DYNAMIC_LIGHTS]`
- `dynamicLightIntensities[MAX_DYNAMIC_LIGHTS]`
- `dynamicLightTypes[MAX_DYNAMIC_LIGHTS]`
- `dynamicLightDirections[MAX_DYNAMIC_LIGHTS]`
- `dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS]`
- `dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS]`
- `dynamicLightingClamp`

`DynamicLightEffectiveUploadIntensity()` applies CPU-side flicker at upload time (`SectorDynamicPointLightSelection.cpp:272-284`). `UploadDynamicPointLights()` uploads only the active `lightCount`; this is safe because the shader loops only to `dynamicLightCount` (`SectorMeshPreview.cpp:866-925`, `SectorMeshPreview.cpp:438-470`).

Dynamic spotlight shadow uniforms are packed by `PackSectorPreviewDynamicSpotLightShadowUniforms()` with `dynamicLightShadowSlots` initialized to `-1`, matrices initialized to identity, bias defaults, strength `0.0`, and softness defaults (`SectorDynamicPointLightSelection.cpp:728-769`). `UploadDynamicSpotLightShadowUniforms()` uploads all shadow slots and shadow arrays (`SectorMeshPreview.cpp:927-965`).

The sector shader computes per-fragment dynamic direct light from `fragWorldPosition` and `fragWorldNormal`, applying inverse linear-squared attenuation, Lambert `ndotl`, optional spotlight cone attenuation, and optional spotlight shadow visibility (`SectorMeshPreview.cpp:434-470`). Dynamic point lights do not shadow. Dynamic spotlights with a valid shadow slot sample `shadowMap0`/`shadowMap1` in `DynamicSpotLightShadowVisibility()` (`SectorMeshPreview.cpp:357-404`, `SectorMeshPreview.cpp:460-468`).

Render-state assumptions for sectors are mostly Raylib `DrawMesh()` material assumptions: textures are supplied through `material.maps`, shader sampler locations are populated in `LoadPreviewMaterial()`, and `DrawMesh()` owns the actual material binding path (`SectorMeshPreview.cpp:1049-1124`, `SectorMeshPreview.cpp:1733-1822`).

## 4. Dynamic Light Pipeline For Billboards

Billboards use a separate shader pair, `SectorBillboardCutoutVs` and `SectorBillboardCutoutFs`, loaded by `LoadBillboardCutoutShader()` (`SectorMeshPreview.cpp:552-728`, `SectorMeshPreview.cpp:1188-1266`). This shader has a smaller surface contract: position, UV, color, `bakedBillboardLighting`, dynamic light arrays, shadow arrays, `shadowMap0`, `shadowMap1`, and `dynamicLightingClamp`.

`DrawRuntimeBillboards()` begins the billboard shader, uploads dynamic point/spot uniforms with the same `UploadDynamicPointLights()` helper, packs and uploads spotlight shadow uniforms, binds shadow maps, then draws immediate quads (`SectorMeshPreview.cpp:2096-2282`). Billboard world position is per-fragment from vertex position (`SectorMeshPreview.cpp:552-571`). Billboards do not use a per-fragment geometric normal for dynamic direct light; they add distance/cone-based dynamic contribution without Lambert `ndotl` (`SectorMeshPreview.cpp:684-720`). For dynamic spotlight shadow bias only, they use a stable world-up approximation (`SectorMeshPreview.cpp:707-716`).

Billboards receive dynamic spotlight shadows when the selected dynamic spotlight has a valid shadow slot; baked object-probe lighting remains unshadowed. This is a useful pattern for doors because the billboard path has a dedicated runtime-object shader contract and avoids sector lightmap/decal state.

## 5. Door Dynamic Light Integration Status

Door drawing does upload dynamic light uniforms every draw through `UploadDynamicPointLights()` using the same selected `dynamicPointLights` list as sectors and billboards (`SectorMeshPreview.cpp:1882-1896`). The reused sector shader declares matching dynamic light uniforms (`SectorMeshPreview.cpp:314-332`). Count, position, color, radius, effective/flickered intensity, type, direction, and cone uniforms are initialized for the active light count by `UploadDynamicPointLights()` (`SectorMeshPreview.cpp:850-925`).

Door drawing also uploads dynamic spotlight shadow uniforms and binds shadow maps (`SectorMeshPreview.cpp:1897-1921`). As written, doors can receive dynamic spotlight shadow maps via the sector shader. They do not cast into those maps.

The door path uses the same selected dynamic light set as sectors and billboards. However, that selected set is computed from static `meshes.sectorReceiverBounds`, not from door receiver bounds (`SectorMeshPreview.cpp:2731-2749`). Procedural doors are runtime objects and are not part of `SectorMeshBuildResult::sectorReceiverBounds`; that struct only carries sector mesh batch bounds (`SectorMeshTypes.h:21-31`). This means a dynamic light that primarily affects a door can be included, excluded, or ranked based on nearby static sector bounds rather than the actual door slab.

Door rendering happens after dynamic light selection is valid for the current `visibilityResult`, but selection is refreshed in `UpdateVisibilityDebug()`, not inside `DrawRuntimeDoors()` (`SectorMeshPreview.cpp:2702-2753`). If the camera/view state changes visibility or dynamic blockers, the selected set changes before rendering. If the player stands still and no visibility update happens, the selected set should stay stable.

Uninitialized uniform risk appears limited for the light arrays because the shader loops to `dynamicLightCount`, and `dynamicLightCount` is always set when the uniform location exists (`SectorMeshPreview.cpp:866-877`). Shadow slots are uploaded for all `MaxDynamicLights` and initialized to `-1` before packing (`SectorDynamicPointLightSelection.cpp:728-769`).

The larger state risk is texture/sampler binding. Door rendering reuses the sector material shader but does not draw via `DrawMesh()`/material maps. It manually calls `SetShaderValueTexture()` for shadow maps and diffuse textures, then also calls `rlSetTexture()` while emitting immediate geometry (`SectorMeshPreview.cpp:1910-1921`, `SectorMeshPreview.cpp:2001-2007`). This mixed path is more fragile than the sector `DrawMesh()` material path and the dedicated billboard shader path. If active texture slot or sampler state is not exactly what rlgl expects, the sector shader can sample the wrong `texture0`, `shadowMap0`, or `shadowMap1`. The symptom can be intermittent and view/pass dependent because preceding sector, shadow, and billboard/bloom paths also bind textures and shaders.

## 6. Flicker Investigation

Likely causes, from strongest to weaker:

1. Fragile immediate-mode texture/sampler state while reusing the sector lightmap/shadow shader.

   Evidence: doors use `BeginShaderMode(material.shader)` and immediate `RL_QUADS`, not `DrawMesh()` with material maps (`SectorMeshPreview.cpp:1927-2073`). The shader expects `texture0`, `texture1`, `decalTexture`, `shadowMap0`, and `shadowMap1` (`SectorMeshPreview.cpp:301-332`). Door code disables lightmap/decal branches, but still binds diffuse and shadow textures manually with a combination of `SetShaderValueTexture()` and `rlSetTexture()` (`SectorMeshPreview.cpp:1910-1921`, `SectorMeshPreview.cpp:2001-2007`). This is the most suspicious source of intermittent dynamic-light flicker because dynamic spot shadow sampling and base texture sampling depend on correct texture units.

2. Dynamic light selection/ranking ignores door receiver bounds.

   Evidence: selected lights are built from `meshes.sectorReceiverBounds` in `UpdateVisibilityDebug()` (`SectorMeshPreview.cpp:2731-2749`). Door bounds are computed at draw time from ECS transform/render state (`SectorMeshPreview.cpp:1952-1983`) and are not fed into candidate/ranking. A light that affects only the door slab can be selected due to static sector bounds, not because the door is a receiver. When the camera/view/portal visibility changes, or when a door opens and moves away from static bounds, the selected light set can change in a way that looks like dynamic lighting flicker on the door.

3. Dynamic spotlight shadow receiving on doors without door shadow casting.

   Evidence: `DrawRuntimeDoors()` uploads shadow slots and binds shadow maps (`SectorMeshPreview.cpp:1897-1921`), and the reused sector shader multiplies spotlight contribution by shadow visibility when `dynamicLightShadowSlots[i] >= 0` (`SectorMeshPreview.cpp:460-468`). `RenderDynamicSpotLightShadowMaps()` renders only sector mesh batches, not doors (`SectorMeshPreview.cpp:2401-2474`). This can create confusing partial shadow response on doors. It does not explain dynamic point-light-only flicker, but it can explain spot-light flicker or angle-dependent changes when a door is near sector geometry.

4. Z-fighting or near-coplanar depth competition.

   Evidence: doors render after sectors with depth test and depth writes enabled (`SectorMeshPreview.cpp:1923-1926`). Door slabs are portal-attached and may be close to portal/wall geometry. The draw code does not apply a depth offset. If a closed or partly opened door face is nearly coplanar with generated sector geometry, the visible result can shimmer or flicker with small view changes. This is a geometry/depth issue, not a texture asset issue, and would be more visible under strong dynamic light.

Lower-probability items:

- Uninitialized light uniforms: unlikely for active lights because `dynamicLightCount` gates the shader loop and active arrays are uploaded for `lightCount` (`SectorMeshPreview.cpp:866-925`, `SectorMeshPreview.cpp:438-470`).
- Stale uniform locations: unlikely; locations are captured when the material and billboard shader are loaded (`SectorMeshPreview.cpp:1049-1124`, `SectorMeshPreview.cpp:1188-1266`).
- Light count mismatch: unlikely; CPU count is clamped to `MaxDynamicLights` and shader also clamps the loop (`SectorMeshPreview.cpp:866-868`, `SectorMeshPreview.cpp:438`).
- Door normals: normals are deterministic from resolved anchor tangent/normal with fallbacks (`SectorMeshPreview.cpp:1952-1963`). Bad or flipped normals could cause one face to be dark, but not intermittent flicker while standing still.
- Door world positions: positions are deterministic from `SectorObjectTransform` and updated by `UpdateSectorDoorDerivedStateSystem()` (`SectorRuntimeObjects.cpp:1327-1375`). Motion can change lighting, but the reported stationary flicker points elsewhere.
- Shader branch for fallback/fill color: the fallback path still samples `defaultMaterialTexture`; the issue reproducing without a real texture does not clear sampler-state risk, because the fallback texture still uses the same `texture0` sampler path.
- CPU-side flicker: flicker is deliberately applied during upload via `DynamicLightEffectiveUploadIntensity()` (`SectorDynamicPointLightSelection.cpp:272-284`). If authored dynamic light flicker is enabled, doors correctly see flicker like other surfaces. The bug report should distinguish authored flicker from unintended door-only flicker.
- NaN/precision in attenuation/cone math: source packing rejects invalid positions/radii/intensities/directions and shader normalization uses safe fallbacks (`SectorDynamicPointLightSelection.cpp:300-418`, `SectorMeshPreview.cpp:351-354`). Still worth sanity-checking with extreme authored values.

Most likely root cause: the door path is a runtime-object immediate-mode renderer sharing a sector lightmap/shadow material that was designed for `DrawMesh()` batches. That creates sampler/render-state fragility exactly where dynamic spotlight shadows and dynamic light uniforms are active. The second likely root cause is selection instability from missing door receiver bounds; this can make the selected dynamic-light set fail to represent the actual door slab.

## 7. Dynamic Shadows And Shadow Casting Status

Doors currently receive dynamic spotlight shadow maps indirectly because they use the sector shader and upload/bind dynamic spotlight shadow uniforms in `DrawRuntimeDoors()` (`SectorMeshPreview.cpp:1897-1921`). This receiving path is not isolated or explicitly designed as a door feature.

Doors currently do not cast into dynamic shadow maps. `RenderDynamicSpotLightShadowMaps()` iterates only static `meshes.sectorDrawRecords` and draws those batches with `dynamicSpotLightShadowMaterial` (`SectorMeshPreview.cpp:2401-2474`). There is no runtime door iteration in the shadow pass.

Door shadow casting should remain out of scope for the first fix. Recommended staged order:

1. Fix deterministic dynamic light receiving / flicker.
2. Optionally add explicit dynamic spotlight shadow receiving for doors after the base path is stable.
3. Later add door shadow casting.

## 8. Recommended Fix Plan

First fix target: deterministic door shader state and stable dynamic point/spot receiving, with no shadow casting, no probe-quality upgrade, and no door motion/collision changes.

Recommended implementation plan for the next task:

1. Add a dedicated opaque runtime-object/door shader or small shared runtime-object lighting shader path.
   - Likely files: `sources/sector_demo/SectorMeshPreview.cpp`, `sources/sector_demo/SectorMeshPreview.h`.
   - Use the billboard dynamic-light uniform pattern and `UploadDynamicPointLights()` helper.
   - Keep shader inputs minimal: `vertexPosition`, `vertexNormal`, `vertexTexCoord`, `vertexColor`, `texture0`, dynamic light arrays, and optionally `dynamicLightingClamp`.
   - Do not include sector lightmap/decal samplers in the door shader.
   - Initially omit dynamic spotlight shadow receiving, or gate it explicitly after sampler state is stable.

2. Stabilize texture binding for doors.
   - Prefer drawing a generated/cached door mesh with a material, or create a tiny immediate-mode helper that explicitly resets active texture slot to 0 before binding `texture0`.
   - Avoid relying on sector material map state from the previous sector batch.
   - Bind only the samplers the door shader actually declares.

3. Decide whether doors should affect dynamic light selection.
   - If repro shows selected light IDs changing when a door flickers, add runtime object receiver bounds to the dynamic light candidate/ranking input.
   - Likely helper: build door AABBs from the same slab corners computed in `DrawRuntimeDoors()` or from `SectorDoorCollider` plus vertical interval.
   - Feed combined static sector receiver bounds plus runtime object receiver bounds into `CollectSectorPreviewDynamicPointLightCandidates()`, `SelectRankedSectorPreviewDynamicPointLights()`, and shadow caster selection.
   - Keep ownership in `EngineContext::world`; `SectorMeshPreview` may observe runtime door ECS state but must not own or mutate door lifecycle.

4. Keep static/object-probe lighting unchanged.
   - Continue sampling `SectorObjectLighting`.
   - Continue face-normal ambient-cube lookup for this fix.

5. Leave shadow casting out.
   - Do not add runtime doors to `RenderDynamicSpotLightShadowMaps()` in the first fix.
   - If dynamic spotlight shadow receiving remains enabled for doors, make it explicit and verify sampler bindings.

## 9. Test / Verification Strategy

Useful non-GUI checks:

- Add unit coverage for any new door/runtime-object receiver bounds helper if introduced. Use generated door anchor/render data, not user-edited levels.
- Add selection tests if receiver bounds are extended: a light overlapping only a runtime door bound should be a candidate; selection should be deterministic with stable IDs; previous-selected hysteresis should still behave.
- If a shared runtime-object lighting upload wrapper is added, unit-test packing/count behavior where possible.
- Add lightweight render-state assertions only if existing patterns support them; avoid fragile GUI automation.

Manual smoke checklist for the fix task:

- Fallback/fill-color door under a dynamic point light.
- Textured door under a dynamic point light.
- Textured door under a dynamic spotlight.
- Dynamic light authored flicker enabled and disabled, confirming intended flicker is not mistaken for door-only instability.
- Camera/player standing still looking at a lit door for several seconds.
- Door partly open and fully open near adjacent/coplanar sector surfaces.
- Door with no dynamic lights.
- Sectors still render correctly after door draw.
- Billboards still render correctly after door draw.
- If spotlight shadow receiving is kept, verify a spotlight with shadows on and off.

Do not add fragile GUI automation for this issue.

## 10. Non-Goals / Guardrails

- Do not change door serialization/schema.
- Do not change door collision.
- Do not change door portal visibility blockers.
- Do not change door motion/open-close behavior.
- Do not implement door shadow casting in this audit.
- Do not implement per-vertex probe lighting in this audit.
- Do not implement transparent/glass doors.
- Do not change lightmap source hash.
- Do not move ECS ownership into `SectorMeshPreview`.
- Do not perform broad renderer refactors.

This audit does not touch topology mutations, 2D topology render-cache behavior, lightmap source-hash behavior, collision, sector lookup, or physics.
