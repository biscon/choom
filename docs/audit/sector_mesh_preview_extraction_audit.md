# SectorMeshPreview Extraction Audit

## Summary

`SectorMeshPreview` is already a useful editor/demo facade: callers can rebuild preview resources, update a renderer pose, compute visibility/debug state, draw the 3D scene, render dynamic spotlight shadow maps, and apply emissive decal bloom without knowing the mesh-builder and asset details. It also keeps the public editor/demo boundary stable while most preview state remains private.

The risk is that this facade is also the renderer lifetime hotspot. `sources/sector_demo/SectorMeshPreview.h:137-309` combines generated geometry, sector meshes, asset handles, raylib `Mesh`/`Material`/`Shader`/`RenderTexture2D` ownership, sky resources, bloom resources, dynamic light selection buffers, dynamic spotlight shadow maps, runtime door mesh caches, billboard shader state, debug counters, and camera pose. `sources/sector_demo/SectorMeshPreview.cpp:221-940` embeds all relevant shader strings, so shader/uniform changes are tightly coupled to renderer extraction.

Recommended extraction order:

1. `SectorPreviewBillboardRenderer` first, if dynamic-light upload inputs stay facade-owned or are passed in as a small context.
2. `SectorPreviewSkyRenderer` next; it is renderer-only and isolated from ECS.
3. `SectorPreviewBloom` as its own task or a very small paired sky/bloom task.
4. `SectorPreviewDynamicLighting` only with a dedicated runner plan.
5. `SectorPreviewDoorRenderer` only with a dedicated runner plan, preferably after dynamic lighting/shadow boundaries are clearer.
6. Avoid a full facade rewrite until smaller extractions land.

Runner plans are needed for dynamic lighting/shadows and runtime doors because both cross shader uniform layout, shadow map resource lifetime, render pass order, runtime receiver bounds, and manual visual tuning.

## Scope And Method

Inspected files:

- `sources/sector_demo/SectorMeshPreview.h`
- `sources/sector_demo/SectorMeshPreview.cpp`
- `sources/sector_demo/SectorDynamicPointLightSelection.h/.cpp`
- `sources/sector_demo/SectorSkyCylinder.h/.cpp`
- `sources/sector_demo/SectorDoorRuntime.h/.cpp`
- `sources/sector_demo/SectorBillboardRuntime.h/.cpp`
- `sources/sector_demo/SectorRuntimeObjects.h/.cpp`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_demo/SectorDemo.*`
- `docs/audit/codebase_architecture_audit.md`
- `docs/audit/sector_editor_types_dependency_audit.md`
- `docs/plans/codebase_refactor_backlog.md`

Commands used are listed in the appendix. Shaders relevant to this audit are embedded C string literals in `SectorMeshPreview.cpp` (`SectorLightmapVs/Fs`, `SectorBloomSourceFs`, `SectorSpotLightShadowVs/Fs`, `SectorDoorOpaqueVs/Fs`, `SectorBillboardCutoutVs/Fs`, `BloomBlurFs`, `BloomCompositeFs`). No separate shader files were found for this path.

Limitations:

- Static source audit only.
- No source refactor was attempted.
- No manual render smoke was performed.
- No claim is made that regex results are complete call graphs.

## Responsibility Inventory

| Responsibility | Main functions/methods | Main members/resources | Dependencies | Boundary | Difficulty | Candidate destination |
| --- | --- | --- | --- | --- | --- | --- |
| Static sector mesh preview/rendering | `RebuildRendererResources()`, `DrawScene()` | `meshes`, `generatedGeometry`, `material`, `defaultMaterialTexture` | `BuildSectorGeneratedGeometry()`, `BuildSectorMeshes()`, `AssetManager`, raylib `DrawMesh()` | Renderer-only/editor-facing facade | Medium | Keep in facade for now or later `SectorPreviewStaticMeshRenderer` |
| Generated mesh/material ownership | `RebuildRendererResources()`, `ShutdownRendererResources()` | `SectorMeshBuildResult meshes`, raylib `Mesh` batches, `Material material` | `SectorMeshBuilder`, raylib load/unload rules | Renderer-only | Medium | Keep until smaller extractions reduce pressure |
| Shader/material loading | local `LoadPreviewMaterial()`, `LoadBloomSourceMaterial()`, `LoadDynamicSpotLightShadowMaterial()`, `LoadDoorOpaqueShader()`, `LoadBillboardCutoutShader()` | embedded shader strings, uniform location members | raylib shader APIs, shader uniform names | Renderer-only but shared by many passes | High | Shared renderer shader helper only after runner plan |
| Texture/material asset requests | `RebuildRendererResources()`, `TextureForId()`, `RendererAssetProgress()` | `assetScope`, `textureHandlesById`, `lightmapTexture`, `skyTextureHandle` | `AssetManager`, `SectorAssetPaths`, topology texture table | Renderer-only/editor-facing | Medium | Keep facade-owned initially |
| Lightmap texture/status | `RebuildRendererResources()`, `RendererLightmapStatusText()`, `DrawScene()` | `lightmapTexture`, `lightmapStatus`, material specular map slot | `SectorLightmap`, `BuildSectorLightmapLayout()` | Renderer-only | Medium | Static scene renderer if later needed |
| Visibility/debug | `UpdateVisibilityDebug()`, `BuildDirectDynamicLightReceiverBounds()` | `visibilityGraph`, `visibilityResult`, debug strings, `visibilityLookupWorld` | `SectorPortalVisibility`, `SectorCollisionWorld`, optional runtime doors | Editor-facing/runtime bridge | Medium | Possibly `SectorPreviewVisibilityState` later |
| Sky | `RebuildRendererResources()`, `DrawSkyCylinder()`, `UnloadSkyCylinderMesh()` | `skyCylinderMesh`, `skyTopCapMesh`, `skyMaterial`, `skyYawOffsetDegrees`, `skyTopCapColor` | `SectorSkyCylinder`, map sky settings, texture handles | Renderer-only | Low/Medium | `SectorPreviewSkyRenderer` |
| Bloom/emissive postprocess | `EnsureBloomResources()`, `RenderBloomSource()`, `ApplyEmissiveDecalBloomToScene()`, `UnloadBloomResources()` | bloom material/shaders, `bloomSceneCopy`, `bloomSource`, `bloomBlurA/B` | embedded bloom shaders, scene render target, sector mesh batches | Renderer-only | Medium | `SectorPreviewBloom` |
| Dynamic light source/candidate selection | `RefreshDynamicLightSources()`, `UpdateVisibilityDebug()` | `dynamicPointLightSources`, `dynamicPointLightCandidates`, `dynamicPointLights`, `selectedDynamicPointLightIds` | `SectorDynamicPointLightSelection`, visibility result, receiver bounds | Renderer selection state | Medium/High | `SectorPreviewDynamicLighting` |
| Dynamic spotlight shadow maps | `EnsureDynamicSpotLightShadowMapResources()`, `RenderDynamicSpotLightShadowMaps()`, `UnloadDynamicSpotLightShadowMapResources()` | `dynamicSpotLightShadowMaps`, `dynamicSpotLightShadowMaterial`, shadow matrix/caster vectors | raylib `RenderTexture2D`, depth-only rlgl helpers, dynamic light selection | Renderer-only plus runtime door casters | High | `SectorPreviewDynamicLighting` with runner plan |
| Dynamic light shader uniform upload | local `UploadDynamicPointLights()`, `UploadDynamicSpotLightShadowUniforms()` | duplicated uniform locations for sector/door/billboard shaders | shader strings, `SetShaderValue*`, shadow map slots | Renderer-only, shared by passes | High | Dynamic lighting helper, but only with runner plan |
| Runtime billboard rendering | `DrawRuntimeBillboards()` plus local frame helpers | `billboardCutoutShader`, billboard uniform locations, debug counters | `engine::World`, `AssetManager`, `SectorBillboardRuntime`, dynamic lights/shadows | Runtime bridge | Medium | `SectorPreviewBillboardRenderer` |
| Runtime door rendering/mesh cache | `PrepareRuntimeDoorMeshes()`, `DrawRuntimeDoors()`, `UnloadDoorMeshes()` | `doorMeshCache`, `doorOpaqueShader`, `doorOpaqueMaterial`, debug counters | `engine::World`, `SectorDoorRuntime`, object probes, dynamic lights/shadows | Runtime bridge plus renderer GPU cache | High | `SectorPreviewDoorRenderer` with runner plan |
| Door static-light color upload | `DrawRuntimeDoors()` | `DoorMeshCacheEntry::staticLightingColors`, `UpdateMeshBuffer()` | object probe runtime data, fallback map, door mesh data | Runtime bridge/renderer cache | High | Door renderer runner plan |
| Receiver bounds / shadow caster runtime bridge | `BuildDirectDynamicLightReceiverBounds()`, `PrepareRuntimeDoorMeshes()`, `RenderDynamicSpotLightShadowMaps()` | `directDynamicLightReceiverBounds`, `runtimeDoorShadowCasters` | `CollectSectorDoorReceiverBounds()`, `AppendSectorDoorShadowCaster()` | Runtime bridge | High | Dynamic lighting + door renderer interface |
| Debug overlay support | `RenderDebugText()`, `VisibilityDebugText()`, counters used by editor UI | strings/counters and selected dynamic light accessors | `SectorEditor.cpp` preview UI | Editor-facing | Medium | Keep facade accessors stable |

## Current Render Pass / Call Order

Editor render call order:

- `SectorEditor::RenderPreview3D()` calls shadow maps, scene, then overlays at `sources/sector_editor/SectorEditor.cpp:3752-3758`.
- `RenderPreview3DShadowMaps()` delegates to `preview.RenderDynamicSpotLightShadowMaps()` at `SectorEditor.cpp:3761-3768`.
- `RenderPreview3DScene()` delegates to `preview.DrawScene()` and passes `engine::World` plus `SectorRuntimeDoorLightingContext` at `SectorEditor.cpp:3771-3777`.
- `ApplyPreview3DBloom()` calls `preview.ApplyEmissiveDecalBloomToScene()` on the caller-owned scene target at `SectorEditor.cpp:3780-3785`.
- Overlays are outside `SectorMeshPreview` at `SectorEditor.cpp:3788-3794`.

Demo render/update call order:

- Runtime objects update first, then `preview.AdvanceRuntime()`, controller pose application, and `preview.UpdateVisibilityDebug(..., &context.world)` at `sources/sector_demo/SectorDemo.cpp:99-104`.
- `SectorDemo::Render()` calls `preview.DrawScene()` with runtime world and door lighting context at `SectorDemo.cpp:107-113`.

Inside `SectorMeshPreview`:

- `Render()` is a convenience wrapper: `RenderDynamicSpotLightShadowMaps()` then `DrawScene()` at `SectorMeshPreview.cpp:2085-2092`.
- `DrawScene()` begins 3D mode, draws sky if ready, binds lightmap/shadow textures, uploads dynamic light/shadow uniforms, draws visible sector mesh batches, then draws runtime doors and billboards if a world is provided, and ends 3D mode at `SectorMeshPreview.cpp:2095-2209`.
- `RenderDynamicSpotLightShadowMaps()` prepares runtime door meshes before drawing shadow maps if a runtime world exists, then draws sector mesh batches and door shadow casters into each selected shadow map at `SectorMeshPreview.cpp:2793-2885`.
- Bloom is separate: `ApplyEmissiveDecalBloomToScene()` copies the scene target, renders emissive decal source from sector batches, ping-pongs blur passes, then composites back into the same scene target at `SectorMeshPreview.cpp:2986-3070`.

Ordering constraints future tasks must preserve:

- Door mesh preparation must happen before shadow-map door casting and before main-scene door drawing (`SectorMeshPreview.cpp:2803-2807`, `2288`).
- Dynamic light/shadow selection is updated by `UpdateVisibilityDebug()` before render; it also mixes static receiver bounds with runtime door bounds when a world is passed (`SectorMeshPreview.cpp:3146-3169`, `3191-3201`).
- Sky draws before sector geometry and disables depth writes around its meshes (`SectorMeshPreview.cpp:2105-2112`, `2964-2978`).
- Doors and billboards are drawn after static sector meshes and restore render state manually (`SectorMeshPreview.cpp:2299-2446`, `2477-2636`).
- Shadow map textures are bound through material map slots for sector/door paths and explicit shader texture uniforms for billboard path (`SectorMeshPreview.cpp:2120-2121`, `2331-2332`, `2511-2521`).
- Bloom owns temporary render targets but receives the scene target from its caller; extraction must preserve Begin/EndTextureMode restoration behavior.

## Resource Ownership And Lifetime

| Resource | Created/loaded | Updated/used | Unloaded | Owner today | Extraction concerns |
| --- | --- | --- | --- | --- | --- |
| Generated geometry | `BuildSectorGeneratedGeometry()` in `RebuildRendererResources()` at `SectorMeshPreview.cpp:1684-1690` | camera initial bounds and public access | reset in shutdown at `SectorMeshPreview.cpp:1940` | `SectorMeshPreview` | Keep generated data lifetime tied to mesh rebuild. |
| Sector render meshes | `BuildSectorMeshes()` at `SectorMeshPreview.cpp:1773-1780` | static draw, shadow pass, bloom source | `UnloadSectorMeshes(meshes)` at `SectorMeshPreview.cpp:1980` | `SectorMeshPreview` | Static scene extraction should not duplicate mesh ownership. |
| Asset scope and texture handles | asset scope at `SectorMeshPreview.cpp:1704-1709`; texture requests at `1711-1725`; lightmap texture at `1764-1771` | `TextureForId()`, `DrawScene()`, bloom source | `assets.UnloadScope(assetScope)` at `SectorMeshPreview.cpp:2070-2073` | `SectorMeshPreview` via `AssetManager` handles | Keep scope unload after render resource unload so materials reset texture references first. |
| Main sector material/shader | `LoadPreviewMaterial()` at `SectorMeshPreview.cpp:1217-1292`, called at `1878-1909` | sector draw, dynamic uniforms, lightmap/decal/shadow textures | reset maps and `UnloadMaterial()` at `SectorMeshPreview.cpp:1988-2001` | `SectorMeshPreview` | Uniform names and material map slots are coupled to embedded shader strings. |
| Sky meshes/material | meshes/material built at `SectorMeshPreview.cpp:1727-1751` | `DrawSkyCylinder()` at `2964-2978` | `UnloadSkyCylinderMesh()` at `2932-2951`, called during shutdown | `SectorMeshPreview` | Good extraction target; preserve depth-mask restore and camera-centered transform. |
| Door mesh cache | cache reserved at `SectorMeshPreview.cpp:1797`; entries created/pruned in `PrepareRuntimeDoorMeshes()` at `2212-2273` | main door draw and shadow caster draw | `UnloadDoorMeshes()` at `2953-2962`, called during shutdown | `SectorMeshPreview` | GPU cache is renderer-owned, but source ECS data is runtime-owned. |
| Door opaque shader/material | shader loaded at `SectorMeshPreview.cpp:1848-1876` | `DrawRuntimeDoors()` | material unloaded/reset at `SectorMeshPreview.cpp:2040-2068` | `SectorMeshPreview` | Door extraction must keep shader/material lifetime and texture slot setup intact. |
| Billboard cutout shader | loaded at `SectorMeshPreview.cpp:1820-1846` | `DrawRuntimeBillboards()` | `UnloadShader()` and loc reset at `SectorMeshPreview.cpp:2014-2038` | `SectorMeshPreview` | Billboard extraction needs either shader ownership or a small shader context. |
| Dynamic spotlight shadow maps | `EnsureDynamicSpotLightShadowMapResources()` at `2765-2784`, called during rebuild at `1802-1806` | shadow pass and lighting texture bindings | `UnloadDynamicSpotLightShadowMapResources()` at `2786-2791` | `SectorMeshPreview` | High risk because resource count, texture slots, and uniform packing must stay aligned. |
| Dynamic shadow material | `LoadDynamicSpotLightShadowMaterial()` at `1327-1353`, called at `1808-1818` | shadow map rendering | reset/unload at `SectorMeshPreview.cpp:2003-2012` | `SectorMeshPreview` | Preserve alpha-test behavior for static geometry and non-alpha door caster path. |
| Bloom resources | `EnsureBloomResources()` at `2651-2718` lazily allocates material, shaders, render targets | `RenderBloomSource()` and composite path | `UnloadBloomResources()` at `2720-2763` | `SectorMeshPreview` | Preserve resize teardown, scene-copy dimensions, and caller-owned scene target semantics. |
| Visibility lookup and graph | built in rebuild at `SectorMeshPreview.cpp:1693-1702` | visibility and dynamic light candidate selection | reset in shutdown at `1941-1949` | `SectorMeshPreview` | Visibility is editor/debug-facing and lighting-relevant; avoid moving casually. |

Shutdown paths are centralized in `ShutdownRendererResources()` at `SectorMeshPreview.cpp:1938-2076`. Rebuild calls shutdown first at `SectorMeshPreview.cpp:1671`, so reload behavior depends on teardown being complete before asset scope recreation. Door cache pruning happens during `PrepareRuntimeDoorMeshes()` when objects disappear from the world (`SectorMeshPreview.cpp:2263-2272`). Shadow-map texture unload uses custom depth-only rlgl teardown (`SectorMeshPreview.cpp:991-997`, `2786-2791`).

## Runtime Object Boundary Review

`SectorMeshPreview` observes `engine::World` in these places:

- Public render/shadow/visibility entry points accept `engine::World*` at `SectorMeshPreview.h:60-72` and `95-100`.
- `DrawScene()` passes a non-null world to `DrawRuntimeDoors()` and `DrawRuntimeBillboards()` at `SectorMeshPreview.cpp:2205-2208`.
- `RenderDynamicSpotLightShadowMaps()` calls `PrepareRuntimeDoorMeshes()` when a world is supplied at `SectorMeshPreview.cpp:2803-2807`.
- `BuildDirectDynamicLightReceiverBounds()` calls `CollectSectorDoorReceiverBounds()` at `SectorMeshPreview.cpp:3191-3201`.
- Door and billboard draw helpers traverse ECS with `World::ForEach()` at `SectorMeshPreview.cpp:2219-2261`, `2342-2435`, and `2524-2627`.

The preview renderer does not appear to spawn, destroy, reset, reserve, or own runtime object lifecycle. Those operations live in `SectorRuntimeObjects`: clear/reset/spawn at `sources/sector_demo/SectorRuntimeObjects.cpp:243`, `302`, and `313`; runtime updates at `520`; object lighting at `578`. Door systems mutate runtime door motion/derived state in `SectorDoorRuntime.cpp:988`, `1133`, and collect collision/blocker data at `1183` and `1216`. Billboard animation advances in `SectorBillboardRuntime.cpp:320`.

Renderer-owned copies/caches from runtime data:

- `doorMeshCache` stores raylib meshes and static-light color vectors keyed by placed object id.
- `runtimeDoorShadowCasters` stores per-frame door caster records for shadow rendering.
- `directDynamicLightReceiverBounds` copies static mesh receiver bounds and appends runtime door receiver bounds.
- Billboard rendering copies per-frame frame/UV/quad data into immediate rlgl draw calls but does not store long-lived billboard meshes.

Boundary rule for future work:

`SectorMeshPreview` and extracted preview renderers may observe ECS/runtime objects and own renderer GPU caches, but must not spawn, reset, own, reserve, destroy, or mutate runtime object lifecycle.

The current design is mostly a practical renderer-runtime bridge rather than a lifecycle violation. The most concerning boundary is not ownership; it is that runtime door receiver/caster data affects dynamic light selection and shadow rendering inside the same class that owns shader resources.

## Dynamic Lighting And Shadows Review

Flow:

- Map dynamic point/spot lights are converted to preview light sources by `BuildSectorPreviewDynamicPointLightSources()` (`SectorDynamicPointLightSelection.cpp:400`, called from `SectorMeshPreview.cpp:1783-1786` and `3097-3102`).
- `UpdateVisibilityDebug()` computes portal visibility, visible draw-record count, receiver bounds, candidate lights, selected lights, selected shadow casters, shadow matrices, and debug text at `SectorMeshPreview.cpp:3116-3188`.
- Candidate/ranking/shadow helper declarations live in `SectorDynamicPointLightSelection.h:102-143`; implementations include candidate collection at `.cpp:445`, point-light selection at `.cpp:479`, shadow caster selection at `.cpp:583`, matrix building at `.cpp:677`, and uniform packing at `.cpp:702`.
- CPU-side flicker is applied during upload through `DynamicLightEffectiveUploadIntensity()` (`SectorDynamicPointLightSelection.cpp:293-305`) from `UploadDynamicPointLights()` (`SectorMeshPreview.cpp:1016-1091`).
- Shadow maps are allocated as two depth-only render textures with `DynamicSpotLightShadowMapResolution` from `SectorDynamicPointLightSelection.h:14-16` and `SectorMeshPreview.cpp:2765-2784`.
- Shadow map rendering draws static sector batches with alpha-test handling and runtime door casters without alpha test at `SectorMeshPreview.cpp:2829-2880`.
- Sector, door, and billboard receiving paths each upload the same selected dynamic light data and shadow uniforms, but through separate shader/location sets (`SectorMeshPreview.cpp:2128-2155`, `2303-2332`, `2483-2521`).

Receiving differences:

- Static sectors use the main sector lightmap shader, per-batch lightmap/decal/alpha-test uniforms, identity model matrices, and shadow maps bound via material roughness/occlusion slots.
- Doors use a dedicated opaque shader/material, model matrices from door anchors/transforms, static object-probe colors uploaded into mesh color buffers, dynamic lights, and the same shadow map depth textures.
- Billboards use a dedicated cutout shader, camera-facing quads through rlgl immediate mode, baked billboard lighting from `SectorObjectLighting`, dynamic lights, and shadow maps via explicit `SetShaderValueTexture()` calls.

Risks:

- Shader shadow code and dynamic light uniforms are duplicated across embedded sector, door, and billboard shaders (`SectorMeshPreview.cpp:251-437`, `538-722`, `745-898`).
- Texture binding differs by pass: material map slots for sector/door, explicit shader texture uniforms for billboards.
- Shadow map slot assumptions are fixed to `MaxDynamicSpotLightShadowCasters == 2` and two named samplers.
- Receiver bounds mix static mesh receiver bounds and runtime door bounds, so lighting extraction and door extraction cannot be totally independent.
- Static sector shadow caster draw uses `MatrixIdentity()`, while door shadow caster draw uses a model matrix.
- Bias, softness, strength, door seal margins, and alpha-test behavior are visual tuning. Future extractions must not "clean up" these constants or shader formulas.

## Candidate Extraction Seams

### 1. SectorPreviewDynamicLighting

- Likely moved: dynamic source/candidate/selected light buffers, selected id debug text, shadow caster/matrix buffers, shadow map resources, shadow map material, dynamic uniform upload helpers, shadow uniform packing/upload call sites.
- Likely source functions: `EnsureDynamicSpotLightShadowMapResources()`, `UnloadDynamicSpotLightShadowMapResources()`, `RenderDynamicSpotLightShadowMaps()`, `UploadDynamicPointLights()`, `UploadDynamicSpotLightShadowUniforms()`, dynamic debug formatting, parts of `UpdateVisibilityDebug()`.
- Public API sketch:
  - `RebuildResources(std::string& error)`
  - `ShutdownResources()`
  - `RefreshSources(const SectorTopologyMap&, const SectorCollisionWorld*)`
  - `UpdateSelection(const RuntimePortalVisibilityResult&, const std::vector<SectorReceiverBounds>&, float runtimeSeconds)`
  - `RenderShadowMaps(assetManager, sectorBatches, textureResolver, optionalDoorCasters)`
  - `UploadToSectorShader(...)`, `UploadToDoorShader(...)`, `UploadToBillboardShader(...)`
- Dependencies: visibility result, receiver bounds, texture resolver, sector draw records, runtime door caster list, shader location structs.
- Must remain unchanged: max light counts, ranking, hysteresis, flicker, shadow slot assignment, shadow map resolution/filter/wrap, shader uniform names, shadow bias/softness/strength behavior.
- Tests/manual smoke: build/tests; manual preview with point lights, spotlights, shadow-casting spotlights, alpha-tested wall shadows, door shadows, billboards receiving light, dynamic-light debug UI.
- Risk: High.
- Recommendation: runner plan required. Maps to REF-014.

### 2. SectorPreviewDoorRenderer

- Likely moved: door mesh cache entry type, `PrepareRuntimeDoorMeshes()`, `DrawRuntimeDoors()`, `UnloadDoorMeshes()`, door opaque shader/material state, door shadow caster draw helper, door debug counters.
- Likely source functions: `CreateDoorSlabMesh()`, `LoadDoorOpaqueShader()`, `SectorDoorLightingDebugModeName()` may remain public or move with facade wrappers.
- Public API sketch:
  - `RebuildResources(std::string& error)`
  - `ShutdownResources()`
  - `PrepareMeshes(engine::World&)`
  - `Draw(engine::AssetManager&, engine::World&, const SectorPreviewLightingDrawContext&, SectorRuntimeDoorLightingContext)`
  - `DrawShadowCasters(const std::vector<SectorDoorShadowCaster>&, Material&)`
  - `SetLightingDebugMode()/DebugMode()`
  - `Stats()`
- Dependencies: `AssetManager`, texture resolver, dynamic lighting upload context, shadow maps, object probes, topology map fallback, `SectorDoorRuntime`.
- Must remain unchanged: cache keying/pruning by placed object id, mesh dirty checks, static-light color upload and `UpdateMeshBuffer()`, draw after static sector batches and before billboards, render-state restore, door shadow seal margins.
- Tests/manual smoke: runtime object tests; manual door preview with closed/open/sliding doors, door textures/UVs/tint, object-probe lighting, dynamic lights, dynamic spotlight shadows, debug modes.
- Risk: High.
- Recommendation: runner plan required. Maps to REF-015.

### 3. SectorPreviewBillboardRenderer

- Likely moved: billboard cutout shader state, local billboard playback/frame helpers, `BakedBillboardLighting()`, `DrawRuntimeBillboards()`, billboard debug counters/warning flag.
- Public API sketch:
  - `RebuildResources(std::string& error)`
  - `ShutdownResources()`
  - `Draw(engine::AssetManager&, engine::World&, const Camera3D&, const SectorPreviewLightingDrawContext&)`
  - `Stats()/DebugText()`
- Dependencies: `AssetManager`, `engine::World`, `SectorBillboardRuntime`, camera, dynamic light/shadow upload context.
- Must remain unchanged: directional clip selection, frame resolution fallback, cutout-only no-blend/depth-writing path, world-up shadow bias approximation, single warning for missing/failed assets.
- Tests/manual smoke: runtime object tests if available; manual billboard preview with animated sprites, directional clips, failed/missing asset fallback, dynamic light receiving, shadow receiving.
- Risk: Medium.
- Recommendation: one Codex task is plausible if it keeps dynamic lighting as an injected context and does not move shader formulas. Maps to REF-016.

### 4. SectorPreviewSkyRenderer

- Likely moved: sky meshes/material/default texture/yaw/top-cap color, `CreateSkyCylinderMesh()`, `DrawSkyCylinder()`, `UnloadSkyCylinderMesh()`, sky setup from map settings.
- Public API sketch:
  - `Rebuild(const SectorTopologyMap&, textureResolver)`
  - `Shutdown()`
  - `Draw(const Camera3D&, const Texture2D&)`
  - `TextureHandle()`
- Dependencies: `SectorSkyCylinder`, normalized sky settings, texture handle resolver, raylib mesh/material APIs.
- Must remain unchanged: only render when map has sky ceiling and texture, camera-centered transform, depth-mask disable/enable, top cap color, fallback to clear color when texture/mesh unavailable.
- Tests/manual smoke: manual sky map preview with sky ceiling, no sky texture, yaw/vertical settings, top cap visible.
- Risk: Low/Medium.
- Recommendation: Codex task after billboard or before bloom. Part of REF-017.

### 5. SectorPreviewBloom

- Likely moved: bloom constants, bloom material/shaders, render textures, `EnsureBloomResources()`, `UnloadBloomResources()`, `RenderBloomSource()`, `ApplyEmissiveDecalBloomToScene()`, texture rect helpers.
- Public API sketch:
  - `Apply(engine::AssetManager&, RenderTexture2D& sceneTarget, const Camera3D&, const std::vector<SectorMeshBatch>&, visibility, textureResolver)`
  - `Shutdown()`
- Dependencies: sector draw records, visibility result, texture resolver, embedded bloom shaders, caller-owned scene target.
- Must remain unchanged: lazy allocation, resize teardown, downsample, iteration count, source filtering, render target copy/composite order.
- Tests/manual smoke: manual emissive decal bloom preview at initial size and after window resize if applicable; verify non-emissive decals do not bloom.
- Risk: Medium.
- Recommendation: separate task if it grows beyond moving a self-contained block. Part of REF-017 or its own follow-up.

### 6. SectorMeshPreviewStaticScene / SectorPreviewStaticMeshRenderer

- Likely moved: main static sector draw loop, main material/shader, lightmap/decal texture binding, per-batch uniforms, sector mesh ownership.
- Public API sketch:
  - `DrawStaticScene(assetManager, camera, batches, textureResolver, lightmap, lightingContext, visibility)`
  - resource lifetime wrappers for main material only.
- Dependencies: almost all static mesh resources and lighting uniforms.
- Must remain unchanged: per-batch texture fallback, decal behavior, alpha-test, lightmap/AO toggles, visibility filtering, identity model path.
- Tests/manual smoke: broad 3D preview smoke across textured sectors, alpha-test, decals, lightmaps, dynamic lights/shadows.
- Risk: Medium/High.
- Recommendation: do not prioritize unless smaller extractions expose a clear benefit.

## Suggested Extraction Order

1. Finish this audit and mark REF-013 complete.
2. Extract billboard renderer first if the task can inject dynamic-light draw context without moving dynamic lighting internals.
3. Extract sky renderer next; it is renderer-only and does not need ECS.
4. Extract bloom separately, with special attention to render texture lifetime and scene target restore.
5. Plan dynamic lighting/shadows with a runner plan; this should define shader location structs and texture binding contracts before implementation.
6. Plan door renderer with a runner plan; this should define the interface between runtime ECS data, renderer-owned door mesh cache, object-probe color upload, and shadow caster drawing.
7. Revisit the remaining facade under REF-018 after the smaller extractions.

This order removes isolated renderer responsibilities first while delaying the two areas where visual tuning and cross-system ordering are most fragile.

## Anti-Patterns / Risks Found

| Severity | Risk | Evidence | Mitigation |
| --- | --- | --- | --- |
| High | God renderer / mixed responsibility | `SectorMeshPreview.h:137-309` stores static meshes, sky, bloom, shaders, shadow maps, doors, billboards, visibility, lighting, and camera state. | Extract narrow renderer helpers behind the stable facade. |
| High | Renderer-runtime bridge affects lighting | `BuildDirectDynamicLightReceiverBounds()` appends door receiver bounds from `engine::World` at `SectorMeshPreview.cpp:3191-3201`. | Keep runtime lifecycle outside renderer; define explicit receiver/caster data inputs for lighting. |
| High | Shader uniform duplication | Similar dynamic light/shadow uniforms appear in sector, door, and billboard shaders at `SectorMeshPreview.cpp:251-437`, `538-722`, and `745-898`. | Use explicit uniform-location structs before moving upload helpers. |
| High | Door cache and shadows are intertwined | `PrepareRuntimeDoorMeshes()` builds GPU cache and shadow casters at `SectorMeshPreview.cpp:2212-2273`; shadow pass consumes both at `2803-2880`. | Door renderer extraction needs a runner plan and visual smoke. |
| Medium | Stateful render order coupling | Scene order is sky, static sectors, doors, billboards at `SectorMeshPreview.cpp:2095-2209`; editor adds overlays and bloom outside at `SectorEditor.cpp:3752-3785`. | Preserve pass order in any helper API; do not let helpers call editor overlays. |
| Medium | Render-state restore assumptions | Sky toggles depth mask; doors/billboards toggle blend/cull/depth and restore manually (`SectorMeshPreview.cpp:2299-2446`, `2477-2636`, `2964-2978`). | Add manual smoke per extraction; avoid changing rlgl state calls. |
| Medium | Texture binding assumptions differ by pass | Shadow maps use material slots for sector/door but explicit uniforms for billboard (`SectorMeshPreview.cpp:2120-2121`, `2331-2332`, `2511-2521`). | Document and preserve binding mode per shader. |
| Medium | Resource lifetime clustering | Shutdown sequence unloads bloom, shadow maps, sky, doors, sector meshes, materials, and asset scope in one method at `SectorMeshPreview.cpp:1938-2076`. | Extract one resource family at a time; keep shutdown ordering visible. |
| Low | Broad optional `engine::World*` API | Public render/visibility functions accept optional runtime world pointers at `SectorMeshPreview.h:60-72`, `95-100`. | Keep facade stable short-term; pass explicit runtime draw contexts internally later. |
| Low | Debug/runtime behavior coupling | Render debug text combines visibility, dynamic lights, shadow casters, and billboard/door text at `SectorMeshPreview.cpp:3170-3188`, `2448-2460`, `2638-2648`. | Preserve public debug accessors while moving counters with their renderers. |

## Recommended Follow-Up Backlog Updates

- REF-014 should remain a runner-plan item. The audit supports splitting the task around shader location structs, dynamic light selection state, shadow map resources, and upload helpers.
- REF-015 should remain a runner-plan item. Door rendering is coupled to runtime ECS traversal, renderer-owned mesh cache, static object-probe color uploads, dynamic lighting, and dynamic spotlight shadows.
- REF-016 can stay a Codex task if it explicitly avoids dynamic lighting/shadow refactors and accepts a dynamic-light draw context from the facade.
- REF-017 may be safer if split into two tasks: sky first, bloom second. This audit does not update the backlog broadly; it only marks REF-013 complete.
- REF-018 remains useful after smaller extractions to reassess whether the facade is still too broad.

## Appendix: Evidence

Commands used:

```sh
pwd && rg --files | rg '(^docs/(plans|audit)/|^sources/sector_demo/|^sources/sector_editor/|AGENTS.md$)'
git status --short
rg -n "REF-013|REF-014|REF-015|REF-016|REF-017|SectorMeshPreview|Overview" docs/plans/codebase_refactor_backlog.md docs/audit/codebase_architecture_audit.md docs/audit/sector_editor_types_dependency_audit.md
rg -n "class SectorMeshPreview|struct SectorMeshPreview|Render|Draw|Rebuild|Shutdown|Load|Unload|Shader|Material|Mesh|RenderTexture|Bloom|Sky|Door|Billboard|Shadow|Dynamic|Light|Visibility|Receiver|Runtime|World|ECS|objectLightProbes|SetShader|rl" sources/sector_demo/SectorMeshPreview.h sources/sector_demo/SectorMeshPreview.cpp
rg -n "SectorMeshPreview" sources tests docs
rg -n "DrawRuntime|Door|Billboard|Shadow|DynamicSpot|ReceiverBounds|Bloom|SkyCylinder|LightmapTexture|Visibility" sources/sector_demo/SectorMeshPreview.cpp
rg -n "LoadShaderFromMemory|const char\\* .*Fs|const char\\* .*Vs|assets/shaders|\\.fs|\\.vs|\\.glsl" sources/sector_demo sources/sector_editor assets docs
rg -n "BuildSectorPreviewDynamicSpotLightShadowMatrices|PackSectorPreviewDynamicSpotLightShadowUniforms|SelectRankedSectorPreviewDynamicSpotLightShadowCasters|BuildSectorPreviewDynamicPointLightSources|CollectSectorPreviewDynamicPointLightCandidates|SelectRankedSectorPreviewDynamicPointLights" sources/sector_demo/SectorDynamicPointLightSelection.cpp sources/sector_demo/SectorDynamicPointLightSelection.h
rg -n "UpdateSectorRuntimeObjects|SpawnPlacedRuntimeObjects|ClearSectorRuntimeObjects|ResetSectorRuntimeObjectsForMap|UpdateSectorObjectBakedLightingSystem|AdvanceSectorBillboardAnimatorSystem|UpdateSectorDoor|CollectSectorDoorDynamic" sources/sector_demo/SectorRuntimeObjects.cpp sources/sector_demo/SectorBillboardRuntime.cpp sources/sector_demo/SectorDoorRuntime.cpp
rg -n "RebuildRendererResources|ShutdownRendererResources|RefreshPreview|preview\\.Rebuild|preview\\.Shutdown|ApplyRendererPose|UpdateVisibilityDebug|RefreshDynamicLightSources" sources/sector_editor sources/sector_demo/SectorDemo.cpp
nl -ba sources/sector_demo/SectorMeshPreview.h | sed -n '1,330p'
nl -ba sources/sector_demo/SectorMeshPreview.cpp | sed -n '1,3255p'
nl -ba sources/sector_demo/SectorDynamicPointLightSelection.h | sed -n '1,260p'
nl -ba sources/sector_demo/SectorDynamicPointLightSelection.cpp | sed -n '1,520p'
nl -ba sources/sector_demo/SectorSkyCylinder.h sources/sector_demo/SectorSkyCylinder.cpp | sed -n '1,360p'
nl -ba sources/sector_demo/SectorDoorRuntime.h sources/sector_demo/SectorDoorRuntime.cpp | sed -n '1,1040p'
nl -ba sources/sector_demo/SectorBillboardRuntime.h sources/sector_demo/SectorBillboardRuntime.cpp | sed -n '1,360p'
nl -ba sources/sector_demo/SectorRuntimeObjects.h sources/sector_demo/SectorRuntimeObjects.cpp | sed -n '1,420p'
nl -ba sources/sector_editor/SectorEditor.cpp | sed -n '3750,3795p'
nl -ba sources/sector_demo/SectorDemo.cpp | sed -n '90,120p'
```

Selected file/line references:

- Public API and private member clustering: `sources/sector_demo/SectorMeshPreview.h:44-309`.
- Embedded shader strings: `sources/sector_demo/SectorMeshPreview.cpp:221-940`.
- Shared dynamic upload helpers: `sources/sector_demo/SectorMeshPreview.cpp:1016-1139`.
- Shader/material load helpers: `sources/sector_demo/SectorMeshPreview.cpp:1217-1516`.
- Sky/door mesh upload helpers: `sources/sector_demo/SectorMeshPreview.cpp:1533-1633`.
- Rebuild path: `sources/sector_demo/SectorMeshPreview.cpp:1665-1930`.
- Shutdown path: `sources/sector_demo/SectorMeshPreview.cpp:1938-2076`.
- Main render path: `sources/sector_demo/SectorMeshPreview.cpp:2095-2209`.
- Door cache/render path: `sources/sector_demo/SectorMeshPreview.cpp:2212-2460`.
- Billboard render path: `sources/sector_demo/SectorMeshPreview.cpp:2462-2648`.
- Bloom resources/pass: `sources/sector_demo/SectorMeshPreview.cpp:2651-2763`, `2887-3070`.
- Dynamic shadow resources/pass: `sources/sector_demo/SectorMeshPreview.cpp:2765-2885`.
- Visibility and dynamic light selection bridge: `sources/sector_demo/SectorMeshPreview.cpp:3116-3201`.
- Dynamic light selection helper boundary: `sources/sector_demo/SectorDynamicPointLightSelection.h:102-143`, `.cpp:400-702`.
- Sky helper boundary: `sources/sector_demo/SectorSkyCylinder.h:25-39`, `.cpp:69-204`.
- Door receiver/caster helper boundary: `sources/sector_demo/SectorDoorRuntime.h:195-227`, `.cpp:571-648`.
- Runtime object lifecycle ownership: `sources/sector_demo/SectorRuntimeObjects.cpp:243`, `302`, `313`, `520`, `578`.
- Editor call order: `sources/sector_editor/SectorEditor.cpp:3752-3785`.
- Demo call order: `sources/sector_demo/SectorDemo.cpp:99-113`.

Rough function/member grouping:

- Facade/rebuild/lifetime: `RebuildRendererResources()`, `ShutdownRendererResources()`, `RendererAssetProgress()`, `RendererLightmapStatusText()`.
- Static scene: `DrawScene()` sector mesh loop, `TextureForId()`, main material uniform locations.
- Runtime bridge: `DrawRuntimeDoors()`, `DrawRuntimeBillboards()`, `PrepareRuntimeDoorMeshes()`, `BuildDirectDynamicLightReceiverBounds()`.
- Dynamic lighting: source/candidate/selected light vectors, shadow caster/matrix vectors, shadow map resources, upload helpers.
- Visual-only extras: sky draw/lifetime and bloom postprocess.

Call graph notes from manual inspection:

- `SectorEditor::RenderPreview3D()` splits shadow pass, scene pass, overlays, and bloom into separate editor calls rather than using `SectorMeshPreview::Render()`.
- `SectorDemo` does not call the shadow pass wrapper in the inspected render path; it updates visibility and draws the scene directly.
- `DrawRuntimeDoors()` calls `PrepareRuntimeDoorMeshes()` again even if the shadow pass already did so. This is redundant but behaviorally relevant because the main draw path can be called without a prior shadow pass.
- `UpdateVisibilityDebug()` is both visibility/debug update and dynamic light selection update. It should not be split casually without preserving selected dynamic lights before render.
