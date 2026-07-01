# Sector Dynamic Per-Pixel Lighting Audit and Design

This started as an audit/design document. The first dynamic point lighting pass
is now implemented; this document records both the original constraints and the
current behavior.

Implemented goal: forward-rendered runtime point and spot lights in the sector
fragment shader. Dynamic lighting blends with the current sector ambient, baked
lightmap direct lighting, baked AO, decals, and portal-culled sector drawing.
Static baked spotlights are authored and baked separately from runtime dynamic
lights. Dynamic spotlights can optionally use runtime shadow maps; dynamic-sector
lighting is future work.

## Implemented First Pass Notes

- Authored dynamic point lights are map-level runtime shader lights, separate
  from existing `staticLights`.
- Authored dynamic spotlights are separate map-level runtime shader lights, also
  separate from existing `staticLights`.
- Static point lights and static spotlights remain baked lightmap inputs.
  Dynamic point and spot lights do not affect
  `ComputeSectorLightmapSourceHash()` and do not stale or recompute baked
  lightmaps.
- Dynamic point lights serialize under `dynamicPointLights`; missing fields load
  as an empty list, and default `enabled: true` is omitted on save.
- Dynamic spotlights serialize under `dynamicSpotLights`; missing fields load as
  an empty list, and default `enabled`, cone, flicker, and shadow fields are
  omitted on save.
- Editor authoring uses a separate Dynamic Light tool and inspector controls for
  enabled state, position, color, radius, and intensity.
- Editor authoring uses a separate Dynamic Spot tool and inspector controls for
  enabled state, position, target, range, inner/outer cone angles, color,
  intensity, flicker, and opt-in shadow-map settings.
- New dynamic lights currently default to white, intensity `1.0`, and radius
  `8.0` world units converted to authoring units.
- New dynamic spotlights default to white, intensity `1.0`, range `8.0` world
  units, inner cone `20` degrees, outer cone `35` degrees, and a target `4.0`
  world units forward in X at `1.0` world units high.
- The runtime renderer supports point lights and cone-filtered dynamic
  spotlights. Dynamic spotlights can opt in to Cast Shadows. Static spotlights
  are baked-only; there are no cookies or normal maps.
- The sector shader evaluates up to `8` selected dynamic lights total using
  fixed-size uniform arrays shared by points and spots.
- Runtime shadow maps are limited to `2` selected dynamic spotlights by default.
  Shadow caster selection is deterministic: higher shadow priority wins, then
  higher selected-light contribution score, then lower stable light ID.
- Shadow maps attenuate only the owning dynamic spotlight direct contribution.
  Dynamic points never cast shadows, static point/spot lights remain baked-only,
  and dynamic spotlights without a shadow slot still illuminate unshadowed.
- Shadow maps use a fixed `1024` resolution, fixed `3x3` PCF, default bias
  `0.00015`, and default strength `1.0`.
- Sector floors, ceilings, walls, and middle textures cast into dynamic
  spotlight shadow maps. Alpha-tested middle textures discard by base texture
  alpha in the shadow pass.
- Sector-world billboard sprites receive selected dynamic spotlight shadow maps
  in their cutout shader. The billboard shader uses each fragment's world
  position for dynamic light attenuation and shadow projection, while baked
  object-probe lighting remains unshadowed. Dynamic point lights and dynamic
  spotlights without a shadow slot remain unshadowed.
- Selected dynamic lights are rebuilt from authored lights, visibility
  candidates, receiver bounds, contribution ranking, and a small runtime-only
  top-N hysteresis step.
- Dynamic spotlight candidates are conservatively selected by owner visible
  sector or range-sphere overlap with visible receiver bounds; the candidate pass
  does not cone-test receiver bounds.
- Dynamic spotlight pilot mode copies the selected spotlight pose to the free-fly
  camera, preserves the original target distance, and only mutates authored
  position/target when Apply is used. Cancel restores the original authored
  transform and camera pose.
- If dynamic lighting is toggled off at runtime, the uploaded dynamic light count
  is zero and the old baked-only lighting path is preserved.
- The dynamic lighting clamp is `4.0` for frames with active dynamic lights; the
  zero-count path keeps the previous `0..1` baked-lighting clamp.
- Preview debug text reports `selected / candidates / total` dynamic light
  counts and selected stable dynamic light IDs when present.
- Preview debug text also reports dynamic spotlight shadow caster count,
  candidate count, max shadow budget, and active shadow caster IDs.

## Current Shader and Material Pipeline

Key files/functions:

- `sources/sector_demo/SectorMeshPreview.cpp`
  - `SectorLightmapVs`, `SectorLightmapFs`, `SectorBloomSourceFs`,
    `BloomBlurFs`, `BloomCompositeFs`
  - `LoadPreviewMaterial()`, `LoadBloomSourceMaterial()`
  - `SectorMeshPreview::RebuildRendererResources()`
  - `SectorMeshPreview::DrawScene()`
  - `SectorMeshPreview::RenderBloomSource()`
  - `SectorMeshPreview::DrawSkyCylinder()`
- `sources/sector_demo/SectorMeshBuilder.cpp`
  - `BuildSectorMeshes()`, `BuildSectorMeshBatchDataInternal()`,
    `CreateMeshFromBatch()`
- `sources/sector_demo/SectorGeneratedGeometry.cpp`
  - `BuildSectorGeneratedGeometry()`
  - `BuildTopologyFlatSurface()`, `BuildTopologyWallSurface()`,
    `BuildTopologyMiddleSurface()`

Shader source is currently embedded as GLSL strings in
`SectorMeshPreview.cpp`, not loaded from `assets/shaders`. Sector rendering uses
one custom lightmap/decal material shader for normal preview rendering and one
custom bloom-source shader for emissive decals. Bloom blur and composite are
separate fullscreen shaders.

Raylib material map slots currently used by sector rendering:

- `MATERIAL_MAP_DIFFUSE`: base sector texture, bound to shader uniform
  `texture0`.
- `MATERIAL_MAP_SPECULAR`: baked lightmap atlas, bound to shader uniform
  `texture1`. This slot is being used as a lightmap sampler, not specular.
- `MATERIAL_MAP_NORMAL`: decal texture, bound to shader uniform `decalTexture`.
  This slot is being used as a decal sampler, not a normal map.

The same diffuse/specular/normal slot convention is used by the bloom-source
material, although the bloom shader only needs the decal path for emissive
decals. Sky rendering uses raylib's default material and only
`MATERIAL_MAP_DIFFUSE`.

Current sector shader uniforms:

- Samplers: `texture0`, `texture1`, `decalTexture`.
- Lightmap/AO controls: `useLightmap`, `useBakedAmbientOcclusion`,
  `hasLightmap`.
- Alpha test controls: `alphaTest`, `alphaCutoff`.
- Decal controls: `hasDecal`, `decalOpacity`, `decalEmissive`, `decalTint`.

Current bloom-source uniforms:

- Samplers: `texture0`, `texture1`, `decalTexture`.
- Decal controls: `hasDecal`, `decalOpacity`, `decalEmissive`, `decalTint`,
  `decalBloomIntensity`.

Per-batch uniforms are set in `SectorMeshPreview::DrawScene()` immediately
before `DrawMesh(batch.mesh, material, MatrixIdentity())`. Base texture, decal
texture, `hasLightmap`, alpha-test state, and decal state are per draw record.
`useLightmap` and `useBakedAmbientOcclusion` are set once before the draw-record
loop. There is no shader-variant system beyond separate shader strings and
runtime integer/float flags.

## Current Lighting Composition

The current fragment shader does this:

```text
baseColor = texture(texture0, baseUv)
surfaceRgb = baseColor.rgb

if non-emissive decal:
    surfaceRgb = mix(baseColor.rgb, decalRgb, decalAlpha)

if lightmap enabled and this batch receives lightmap:
    bakedSample = texture(texture1, lightmapUv)
else:
    bakedSample = vec4(0, 0, 0, 1)

aoFactor = baked AO alpha if enabled and lightmapped, otherwise 1
ambient = vertexColor.rgb * aoFactor
bakedDirect = bakedSample.rgb
lighting = clamp(ambient + bakedDirect, 0, 1)
litRgb = surfaceRgb * lighting

if emissive decal:
    finalRgb = mix(litRgb, emissiveDecalRgb, emissiveDecalAlpha)
else:
    finalRgb = litRgb
finalA = baseColor.a * vertexColor.a
```

Facts:

- Sector ambient comes from mesh vertex color. `BuildSectorGeneratedGeometry()`
  computes it from `SectorTopologySector::ambientColor * ambientIntensity`,
  clamping `ambientIntensity` to `[0, 1]`.
- Baked direct lighting is stored in lightmap RGB.
- Baked AO is stored in lightmap alpha.
- Baked direct lighting is additive with ambient before the final multiply by
  surface color.
- AO is applied only to sector ambient, not to baked direct lighting.
- Lighting is clamped to `[0, 1]` before multiplying by the base/decal surface
  color. Bloom composite also clamps scene plus bloom to `[0, 1]`.
- There is no runtime directional light in the preview shader. Directional
  light settings are bake inputs only today.
- There is no tonemapping or HDR-style accumulation in the sector shader.

Lightmap receiver behavior:

- `SectorGeneratedSurface::receivesLightmap` defaults to true.
- Floors, non-sky ceilings, one-sided walls, lower walls, and upper walls
  receive lightmaps by default.
- Middle textures are generated as alpha-tested surfaces with
  `receivesLightmap = true`, `alphaTest = true`, and `alphaCutoff = 0.5`.
- Sky ceilings generate no normal ceiling surface. Sky cylinder/top cap are
  drawn separately with the default material and do not receive lightmaps.
- Sky-sky portals can suppress upper wall strips between both-sky sectors.

Decal/emissive behavior:

- Non-emissive decals replace/blend the surface albedo before lighting.
- Emissive decals do not add to lighting; the final RGB is a mix between lit
  surface RGB and raw decal RGB by decal alpha.
- Emissive decal bloom is rendered in `RenderBloomSource()` by drawing only
  visible draw records with emissive decals into a downsampled bloom source.
  Bloom source RGB is `decal.rgb * tint * alpha * (bloomIntensity / 10)`, then
  blur/composite adds it back with `BloomStrength * BloomLdrIntensityScale`.

## Mesh Attributes Available to the Fragment Shader

Uploaded mesh data in `CreateMeshFromBatch()` includes:

- `mesh.vertices`: world-space generated sector positions.
- `mesh.normals`: generated flat surface normals.
- `mesh.texcoords`: base UV.
- `mesh.texcoords2`: lightmap UV.
- `mesh.tangents.xy`: decal UV.
- `mesh.colors`: sector ambient color.

The active sector vertex shader currently declares and passes:

- `vertexPosition` to `gl_Position` only.
- `vertexTexCoord` to `fragTexCoord`.
- `vertexTexCoord2` to `fragTexCoord2`.
- `vertexTangent.xy` to `fragDecalUv`.
- `vertexColor` to `fragColor`.

The shader does not currently declare `vertexNormal`, pass a fragment normal,
or pass fragment/world position. Dynamic per-pixel point lighting needs both.

Today `DrawMesh()` uses `MatrixIdentity()` for sector draw records, and
generated positions are already in preview world space. That means local and
world space are effectively identical for current sector geometry. The minimal
shader change for first-pass dynamic lights is to add `in vec3 vertexNormal`,
pass `fragWorldPosition = vertexPosition`, pass `fragWorldNormal =
normalize(vertexNormal)`, and use those in the fragment shader. If non-identity
sector transforms are introduced later, this must become proper world-space
transform and normal-matrix handling.

Tangent data is not usable for normal mapping today because `vertexTangent.xy`
is repurposed as decal UV and `MATERIAL_MAP_NORMAL` is repurposed as decal
texture.

## Sector Visibility and Draw Culling Integration

Key files/functions:

- `sources/sector_demo/SectorPortalVisibility.h/.cpp`
  - `RuntimePortalVisibilityResult`
  - `BuildRuntimeSectorVisibilityGraph()`
  - `ComputeRuntimeSectorVisibilityFromView()`
  - `TraverseRuntimeSectorVisibility()`
- `sources/sector_demo/SectorMeshBuilder.cpp`
  - `BuildSectorMeshDrawRecordData()`
  - `ShouldDrawSectorMeshRecordForVisibility()`
  - `ShouldDrawEmissiveBloomSectorMeshRecordForVisibility()`
  - `CountSectorMeshDrawRecordsForVisibility()`
- `sources/sector_demo/SectorMeshPreview.cpp`
  - `UpdateVisibilityDebug()`
  - `DrawScene()`
  - `RenderBloomSource()`

`SectorMeshPreview::RebuildRendererResources()` builds a
`RuntimeSectorVisibilityGraph` from topology and a `SectorCollisionWorld` for
camera sector lookup. `UpdateVisibilityDebug()` computes
`visibilityResult` from the current camera position, yaw, and FOV using
`ComputeRuntimeSectorVisibilityFromView()`.

`RuntimePortalVisibilityResult` contains:

- `startSectorId`
- `visibleSectorIds`
- `traversedPortalLineDefIds`
- `totalSectorCount`
- `validStartSector`
- `fallbackDrawAll`
- `mode`
- `status`

`BuildSectorMeshes()` now uploads `sectorDrawRecords` grouped by
`sectorId + material/decal/lightmap key`. `ShouldDrawSectorMeshRecordForVisibility()`
returns true for all records if `!validStartSector` or `fallbackDrawAll`; otherwise
it checks whether `record.sectorId` is in `visibleSectorIds`.

Dynamic light selection should run after `visibilityResult` is current and
before drawing sector records. A first pass should run once per frame globally
for the current camera view, upload one top-N light array, and use that for all
sector draw records. Per-sector/per-record light lists would reduce excess
lighting but are a larger shader/material batching change.

If visibility is invalid or `fallbackDrawAll` is true, dynamic light selection
should consider all runtime dynamic lights. The visible-sector list is stable
enough for conservative candidate selection; false positives are acceptable,
while false negatives would hide both geometry and lights and should be treated
as visibility bugs.

## Existing Static Light Data

Key files/functions:

- `sources/sector_demo/SectorTopologyTypes.h`
  - `SectorTopologyStaticPointLight`
  - `SectorTopologyStaticSpotLight`
- `sources/sector_demo/SectorTopologySerialization.cpp`
  - `ReadMapLevelFields()`, `WriteMapLevelFields()`
- `sources/sector_demo/SectorLightmap.cpp`
  - `MakeWorldSpaceLight()`
  - `EvaluateDirectLight()`
  - `EvaluateDirectionalLight()`
  - `ComputeSectorLightmapSourceHash()`
- `sources/sector_editor/SectorEditorTopologyActions.cpp`
  - `AddStaticLight()`, `DeleteStaticLight()`, `FinishMoveStaticLight()`
- `sources/sector_editor/SectorEditorLightInspector.cpp`
  - static light property editing

`SectorTopologyStaticPointLight` fields:

```cpp
struct SectorTopologyStaticPointLight {
    int id;
    Vector3 position;     // authoring units, converted for bake/render use
    Color color;
    float intensity;
    float radius;         // authoring distance
    float sourceRadius;   // authoring distance, bake soft-shadow source size
};
```

Defaults place new lights 1.8 world units above the clicked sector floor,
with intensity `1.0`, radius `8.0` world units converted to authoring units,
and source radius `0.25` world units converted to authoring units.

Static lights are serialized under root `staticLights`; omitted
`staticLights` loads as empty. Static light changes are topology document edits
in the editor. Static lights are converted to world units by `MakeWorldSpaceLight()`
and used by the lightmap baker for direct lighting with distance attenuation,
Lambert, occlusion, and optional soft-shadow source sampling.

Static lights are included in `ComputeSectorLightmapSourceHash()` by sorted ID,
world position, color, intensity, world radius, and clamped world source radius.
They are bake-affecting data today.

`SectorTopologyStaticSpotLight` is the baked spotlight counterpart. It stores a
stable ID, position, target, color, intensity, range, source radius, and
inner/outer cone angles. Defaults match dynamic spotlight authoring where
practical: white, intensity `1.0`, range `8.0` world units, source radius `0.0`,
inner cone `20` degrees, outer cone `35` degrees, and a target `4.0` world units
forward in X at `1.0` world units high. Static spotlights serialize under root
`staticSpotLights`; missing data loads as an empty list.

Static spotlights are converted to world units for baking, evaluated with the
same distance/Lambert/occlusion path as static point lights, and multiplied by
smooth cone attenuation between the outer and inner cone. Degenerate targets
fall back to a safe downward baked direction. The lightmap source hash includes
static spotlights by sorted ID, world position, world target, color, intensity,
world range, clamped world source radius, and cone angles. Static spotlights do
not participate in runtime dynamic light selection or shader uniforms.

Recommendation: keep existing `staticLights` as baked/static lights. Add a
separate runtime dynamic light list/type for dynamic lighting. Reusing
`staticLights` as dynamic lights would blur bake-source ownership, make source
hash semantics surprising, and likely double-light scenes unless each light got
new bake/runtime mode flags.

## Texture, Sampler, and Uniform Limits

Current sector shader sampler usage already occupies three raylib material map
slots:

- base texture via diffuse/`texture0`
- lightmap via specular/`texture1`
- decal texture via normal/`decalTexture`

Because raylib material map slots are already piggybacked, first-pass dynamic
lighting should avoid new sampler requirements. Uniform arrays are the simplest
initial path for point lights:

```glsl
const int MAX_DYNAMIC_LIGHTS = 8;
uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
```

Recommended initial maximum: `MAX_DYNAMIC_LIGHTS = 8`.

Justification: this project is using a simple GLSL 330 forward shader through
raylib `SetShaderValue()` calls, with LDR clamped lighting and no UBO/SSBO
infrastructure. Eight point lights keep uniform uploads small and avoid turning
each fragment into a large loop while still proving the data model, culling,
ranking, and shader composition. `16` is a plausible later bump after measuring.

UBOs, SSBOs, texture buffers, clustered forward, tiled lighting, and deferred
rendering are unnecessary for the first implementation.

## Candidate Dynamic Light Selection and Ranking

Implemented first-pass policy:

1. Store each runtime dynamic light with a known stable ID and owner sector ID
   if possible.
2. Drop disabled/invalid lights and lights whose radius or intensity is not
   positive.
3. If visibility is invalid or fallback draw-all is active, collect all valid
   dynamic lights.
4. Otherwise, collect lights whose owner sector is visible as an inclusion
   shortcut.
5. Also collect hidden-owner lights when their influence sphere overlaps the
   padded bounds of visible receivers. This keeps lights just outside the
   visible sector set available when they can still affect visible geometry.
6. Score candidates by estimated contribution at the nearest point on the
   visible receiver bounds, not by camera distance:

```text
brightness = max(color.r, color.g, color.b) in 0..1
distance = nearest distance from light.position to visible receiver bounds
atten = saturate(1 - distance / light.radius)
atten = atten * atten
score = light.intensity * brightness * atten
```

Optionally bias by radius with a small factor if large area lights need to win
over tiny local lights:

```text
score *= sqrt(radius)
```

Keep the top `MAX_DYNAMIC_LIGHTS`, sorted by stable key after ranking for
deterministic upload if scores tie. Runtime selection keeps the previous
frame's selected IDs when scores are close; this hysteresis is not serialized and
does not affect baked lightmaps. Simple hysteresis:

```text
Only replace an already-selected light if the new candidate score is at least
20% higher than the retained light's score.
```

Debug text should report selected dynamic lights versus candidates, for example
`dynamic lights: 6 / 19 candidates`.

## Dynamic Light Types

Initial scope should be point lights only:

```cpp
struct RuntimeDynamicLight {
    int id = -1;
    int sectorId = -1;
    Vector3 position = {};
    Color color = WHITE;
    float radius = 8.0f;     // world units
    float intensity = 1.0f;
    bool enabled = true;
};
```

Use world units in the runtime type so shader upload does not need authoring-unit
knowledge. Authored dynamic lights, if added later, can convert at load/spawn
boundaries just like static bake lights.

Spotlights should be deferred unless a specific gameplay need appears. They
need direction, cone angles, and an extra angular attenuation term, and they
raise editor/debug complexity. They can be added later without changing the
point-light base if dynamic shader upload is structured around fixed arrays.

No shadows, cookies, projected textures, volumetrics, or animation/flicker
systems in the first pass.

## Proposed First Implementation Plan

### Phase 1: Shader/Material Audit Helpers and Debug Plumbing

- Add constants for dynamic light shader limits and uniform names.
- Add explicit uniform location fields to `SectorMeshPreview`.
- Add debug counters for candidate/selected dynamic lights.
- Preserve current output exactly when dynamic light count is zero.
- Add data-level tests for packing defaults if helper code is extracted.

### Phase 2: Dynamic Light Data Model and Runtime List

- Add a small runtime-only dynamic light list near sector preview/runtime state.
- Do not add it to topology save/load yet unless a later task explicitly asks
  for authored dynamic lights.
- Keep existing `staticLights` baked-only.
- Use world-space positions, radius, and intensity in runtime data.
- Provide a simple temporary injection/debug path for smoke testing.

### Phase 3: Forward Per-Pixel Point Light Shader Support

Minimal shader additions:

- Pass fragment world position and normal from vertex to fragment shader.
- Add fixed-size uniform arrays and `dynamicLightCount`.
- Compute dynamic direct light per fragment:

```text
toLight = light.position - fragWorldPosition
distance = length(toLight)
if distance < radius:
    L = toLight / distance
    ndotl = max(dot(normalize(fragWorldNormal), L), 0)
    atten = saturate(1 - distance / radius)
    atten = atten * atten
    dynamicDirect += light.colorRgb * light.intensity * atten * ndotl
```

Recommended composition:

```text
lighting = bakedDirect + sectorAmbient * bakedAO + dynamicDirect
lighting = clamp(lighting, 0, dynamicLightingClamp)
finalRgb = surfaceRgb * lighting
finalRgb = mix(finalRgb, emissiveDecalRgb, emissiveDecalAlpha)
```

First pass should not multiply dynamic direct lighting by baked AO. Dynamic
lights are direct runtime lights and should not inherit stale baked occlusion.
If the current visual style needs heavier contact darkening, add an optional
artist/debug flag later rather than baking that behavior in.

Use a conservative clamp, for example `0..2` or `0..4`, to avoid NaNs and
extreme overbright explosions while permitting dynamic lights to visibly add on
top of baked scenes. Keep `dynamicLightCount = 0` visually equivalent to the
current shader except for any intentional clamp range audit.

### Phase 4: Visible-Sector Candidate Selection and Ranking

- Run once per frame after `UpdateVisibilityDebug()` has refreshed
  `visibilityResult`.
- If fallback draw-all, score all runtime dynamic lights.
- Otherwise score lights assigned to visible sectors.
- Upload the top `MAX_DYNAMIC_LIGHTS` to the sector material before drawing.
- Use stable selection/hysteresis if flicker is visible.
- Keep one global selected set for phase 1. Per-sector light lists can be a
  later optimization.

### Phase 5: Editor/Debug Controls and Smoke Tests

- Add debug readout for candidate and selected dynamic light counts.
- Add non-GUI tests for selection/ranking helpers.
- Add a small manual smoke map/debug setup only when implementation starts.
- Do not add GUI automation or screenshot tests.

## Future Non-GUI Tests

- Ranking chooses closer/stronger lights correctly.
- Lights outside influence radius are dropped.
- Visible-sector filtering excludes lights in hidden sectors.
- `fallbackDrawAll` considers all lights.
- Uniform packing handles zero lights and max lights.
- Dynamic-light disabled/default data path matches current output defaults where
  testable without screenshots.

## Dynamic Spotlight Shadow Maps

Shadow-casting dynamic spotlights are opt-in through the inspector's Cast
Shadows setting. Cast Shadows requests a shadow map; only the highest-priority
selected dynamic spotlights up to the shadow budget receive runtime shadow
slots. Over-budget dynamic spotlights still illuminate without shadows.

PCF means percentage-closer filtering; it samples neighboring shadow-map depths
to soften shadow edges. The current dynamic spotlight shadow path uses a fixed
3x3 PCF kernel. Per-light bias controls acne/peter-panning tradeoffs, and
per-light strength blends between unshadowed lighting and the sampled shadow
visibility.

Point-light shadows remain deferred because cubemap shadows need multiple faces
or equivalent layered rendering. Static point and spot lights do not use runtime
shadow maps because they are baked. Flashlights, volumetric shafts, god rays,
projected cookies/gobos, per-light shadow resolution, and shadow quality
settings are also deferred.

## Dynamic Sectors and Moving Geometry

Dynamic per-pixel lights help future moving floors, lifts, doors, or other
dynamic sector geometry because those surfaces may not have valid baked
lightmaps. For dynamic sectors, baked lightmaps may be omitted, stale, or only
partially usable, while runtime lights can still illuminate the surfaces
consistently. This document does not design dynamic sectors or moving geometry.

## Goblins / Risks

- Forward light uniform limits can cap visual richness; start at 8 and measure
  before increasing.
- Raylib material map slots are already repurposed: specular is lightmap and
  normal is decal. Do not assume normal-map support exists.
- Baked lightmap RGB/intensity scale may not match dynamic light intensity.
- Dynamic lights can overbrighten already baked scenes; clamp and debug toggles
  are needed.
- Fragment world position and normal are not currently available in the shader.
- Non-identity transforms would require proper world-space normal handling.
- Alpha-tested middle textures need the same discard-before-lighting behavior.
- Emissive decals currently bypass lighting by mixing to decal RGB; dynamic
  lights should not accidentally light emissive bloom source passes.
- Visible-sector culling false positives waste light slots; false negatives hide
  valid lights.
- Top-N selection can flicker without stable ordering or hysteresis.
- Dynamic sectors can invalidate baked lightmaps or make them stale.
- Shadow maps are limited to dynamic spotlights in this implementation; point
  shadows, static runtime shadows, volumetric effects, and flashlight-specific
  behavior remain separate future work.

## Explicit Out of Scope

- Dynamic point-light cubemap shadows.
- Static runtime shadow maps.
- Flashlight gameplay or forced flashlight shadow slots.
- Shadow quality settings.
- Per-light shadow resolution.
- Deferred rendering.
- Clustered/tiled forward lighting.
- Dynamic floors/doors.
- Full object/entity ownership system.
- Volumetric lights/fog.
- Projected light cookies.
- Changing lightmap baking/source hash policy.
- Replacing raylib renderer/material system.
- GUI automation tests.

## Source Hash, Cache, and Behavior Notes

This audit/design task changes only documentation. It does not touch topology
mutation code, 2D topology render-cache invalidation, lightmap baking, or
lightmap source-hash logic.

Runtime dynamic point lights and dynamic spotlights are excluded from
`ComputeSectorLightmapSourceHash()` because they do not affect baked receiver
layout, occlusion, or static lighting results. Existing `staticLights` and
`staticSpotLights` remain bake lights and continue to affect the lightmap source
hash. Static baked spotlights are excluded from runtime dynamic light selection,
shader uniforms, shadow maps, and volumetric effects.

Runtime dynamic spotlight shadow settings, selected shadow slots, shadow map
resources, light-space matrices, PCF sampling, and visual shadow strength/bias
remain runtime preview behavior and do not affect the lightmap source hash.
Billboard dynamic shadow receiving reuses those runtime shadow slots and does
not affect bake inputs, object probe sidecars, or the lightmap source hash.
