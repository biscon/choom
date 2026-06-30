# Baked Object Lighting Probes Design

## Scope

This is an audit/design document for a future baked object-lighting probe system
for movable 3D models, billboard sprites, pickups, NPCs, particles, and other
dynamic objects. It does not implement probes and does not change production
rendering, mesh generation, baking, collision, save/load, editor tools, shaders,
or lightmap output.

Future consumer contract:

- Static world geometry continues to use baked surface lightmaps.
- Movable objects never sample static surface lightmap UVs directly.
- 3D model renderers call `SampleBakedObjectLighting()` per object or draw and
  evaluate the returned low-frequency lighting by model normals.
- Billboard renderers call `SampleBakedObjectLighting()` per sprite or actor and
  use stable probe-derived lighting, not the camera-facing quad normal.
- Runtime dynamic point/spot lights and supported dynamic spotlight shadow maps
  are added on top of baked object lighting.

## Current Lightmap Bake Pipeline Audit

Key files audited:

- `sources/sector_demo/SectorLightmap.h`
- `sources/sector_demo/SectorLightmap.cpp`
- `sources/sector_demo/SectorLightmapTypes.h`
- `sources/sector_demo/SectorTopologyTypes.h`
- `sources/sector_demo/SectorTopologySerialization.cpp`
- `sources/sector_demo/SectorGeneratedGeometry.h`
- `sources/sector_demo/SectorGeneratedGeometry.cpp`
- `sources/sector_demo/SectorTopologyGeometry.h`
- `sources/sector_demo/SectorTopologyGeometry.cpp`
- `sources/sector_editor/SectorEditorLightmapModal.cpp`
- `tests/SectorTopologyLightmapTests.cpp`
- `tests/SectorTopologySerializationTests.cpp`

The public bake entry points are in `SectorLightmap.h`:

- `BuildSectorLightmapLayout()`
- `BakeSectorLightmap(const SectorTopologyMap&, const SectorLightmapLayout&, ...)`
- `BakeSectorLightmap(const SectorTopologyLightmapBakeInput&, ...)`
- `ComputeSectorLightmapSourceHash()`
- `GetSectorLightmapStatus()`

`SectorTopologyLightmapBakeInput` snapshots the map, carries an expected source
hash, and provides final/temporary output paths plus an editor revision. The
async editor UI tracks phases in `SectorLightmapBakePhase`; the modal itself is
only progress/cancel/acknowledgement UI.

Bake input collection currently happens inside `BakeSectorLightmapForMap()` in
`SectorLightmap.cpp`:

- Calls `BuildLightmapGeneratedGeometryForBake()`, which wraps
  `BuildSectorGeneratedGeometry()`.
- Verifies the supplied `SectorLightmapLayout` has one chart slot per generated
  surface.
- Collects alpha-tested middle occluders with
  `CollectSectorLightmapAlphaOccluders()`.
- Builds opaque bake triangles with `BuildBakeTriangles()`.
- Builds a CPU BVH with `BuildSectorLightmapBvh()`.
- Converts static point lights and static spotlights to world units with
  `MakeWorldSpaceLight()`.
- Converts directional light settings with `MakeWorldSpaceDirectionalLight()`.
- Rasterizes valid surface texels into `BakeTexel` records by walking chart
  usable rectangles and calling `RasterizeSurfacePoint()`.

Generated geometry facts:

- `BuildSectorGeneratedGeometry()` validates the topology map, extracts
  `SectorTopologyLoopSet` per sector with `ExtractSectorTopologyLoops()`, and
  builds generated floor, ceiling, wall, lower wall, upper wall, and middle
  surfaces.
- Flat floors/ceilings are triangulated with `mapbox::earcut()` in
  `BuildTopologyFlatSurface()`.
- `SectorGeneratedSurface` stores a `SectorGeneratedSurfaceRef`, material IDs,
  alpha-test flags, a `receivesLightmap` flag, generated vertices, and chart
  dimensions.
- Sky ceilings do not generate normal ceiling surfaces. Sky-sky portals suppress
  upper wall strips.
- Middle surfaces receive lightmaps but are excluded from opaque occluder
  triangles by `CastsLightmapOcclusion()`.
- Alpha-tested middle surfaces become separate alpha occluder triangles through
  `CastsAlphaTestLightmapOcclusion()` and
  `BuildAlphaTestOccluderTriangles()`.

Static lighting evaluation facts:

- Static point lights use `EvaluateDirectLight()` and
  `EvaluateDirectLightSample()`.
- Static spotlights use overloaded `EvaluateDirectLight()` and
  `EvaluateDirectLightSample()` with cone attenuation from inner/outer cone
  angles.
- Both point and spot lights support `sourceRadius`; nonzero source radius uses
  `kDirectSoftShadowSampleCount` Fibonacci sphere samples and averages the
  resulting direct light.
- Directional light uses `EvaluateDirectionalLight()`. In the current surface
  bake, it is only added for sky-owned lightmap surfaces
  (`IsSkyOwnedLightmapSurface()`).
- Direct shadow rays use `IsOccluded()` / `IsDirectionOccluded()` and the BVH.
- Alpha-tested occlusion uses `RaycastBakeOcclusionAlphaAware()`, which
  alternates closest opaque geometry hits and closest alpha occluder hits. Alpha
  masks are loaded CPU-side by `SectorLightmapAlphaMaskCache`; missing masks are
  conservative opaque.
- Ambient occlusion uses `BakeAmbientOcclusion()` with
  `kAmbientOcclusionSampleCount` cosine-hemisphere rays and stores the result in
  lightmap alpha.
- Indirect bounce uses `kIndirectBounceSampleCount` cosine-hemisphere rays,
  samples nearby direct-light atlas values via `SampleDirectLightingAtLightmapUv()`,
  and applies `kNeutralBounceAlbedo` plus the map's indirect bounce strength.

Output facts:

- The lightmap atlas is a PNG written by `ExportImage()` as
  `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8`.
- Atlas RGB is clamped direct plus indirect bounce.
- Atlas alpha is ambient occlusion.
- Chart gutter dilation happens before export with `DilateChart()`.
- `SectorLightmapBakeResult` reports atlas size, source hash, chart occupancy,
  BVH stats, light counts, ray stats, and timing.

Settings and metadata facts:

- `SectorLightmapBakeSettings` currently stores only AO radius/strength and
  indirect bounce radius/strength.
- `SectorLightmapMetadata` currently stores only `path`, `width`, `height`, and
  `sourceHash`.
- JSON save/load reads and writes `lightmapSettings` and optional `bakedLightmap`
  in `SectorTopologySerialization.cpp`.
- Old maps may omit lightmap settings and baked lightmap metadata.
- `GetSectorLightmapStatus()` reports `None`, `Stale`, or `Valid` based on
  metadata presence, source-hash match, and atlas file existence.

Source hash facts:

- `kSectorLightmapBakeVersion` is currently 9 for alpha-tested middle texture
  direct-light occlusion.
- `ComputeSectorLightmapSourceHash()` includes bake constants, bake settings,
  world-unit scale, sample counts, neutral bounce albedo, directional light,
  coordinate subdivisions, referenced lightmap texture IDs/paths/filter modes,
  vertices, linedefs, sidedefs, sectors, static point lights, and static
  spotlights.
- Sector hash data includes `ceilingSky`, floor/ceiling heights, floor/ceiling
  texture IDs and UVs, ambient color/intensity, and default wall/lower/upper
  parts.
- Static point and spot light source radii are hashed after conversion and
  clamping to the same bake limits used by evaluation.
- Sky visual settings and preview settings are not included in the lightmap
  source hash, matching the existing visual-only rule.

## Where Probe Data Should Live

Object lighting probes should be bake output, not authored topology. They should
not live in linedefs, sidedefs, sectors, or the authoring graph as editable
objects.

Recommended v1 storage:

```cpp
struct SectorBakedObjectLightProbe {
    int sectorId;
    Vector3 position;              // world units
    Vector3 ambientCube[6];         // +X, -X, +Y, -Y, +Z, -Z linear-ish RGB
};

struct SectorBakedObjectLightProbeSet {
    int version;
    std::string sourceHash;
    float probeSpacingWorld;
    float probeHeightWorld;
    std::vector<SectorBakedObjectLightProbe> probes;
};
```

Keep this as a separate baked probe payload beside `SectorLightmapMetadata`,
rather than encoding it into the RGBA lightmap atlas. The current atlas is a GPU
surface texture with RGB/A semantics already assigned to direct+indirect and AO;
object probes are CPU/runtime query data and need sector IDs and positions.

Recommended metadata shape for a later implementation:

```cpp
struct SectorLightmapMetadata {
    std::string path;
    int width = 0;
    int height = 0;
    std::string sourceHash;
    SectorBakedObjectLightProbeSet objectProbes;
};
```

Alternatively, if file size becomes a concern, keep only a probe-data path in
metadata and write probes to a sidecar file next to the atlas. For v1, embedding
JSON data under `bakedLightmap.objectProbes` is simpler and keeps map/probe
validity together.

Missing probe data must be nonfatal:

- Old maps with valid surface lightmaps but no probes should load.
- Runtime `SampleBakedObjectLighting()` returns a valid fallback sample.
- Editor/debug status should distinguish "surface lightmap valid, object probes
  missing" once object probes are expected.

Probe data should be considered stale whenever the surface lightmap is stale.
If probe generation settings become configurable, those settings must be included
in the same source hash or in a probe-specific hash that is compared together
with the lightmap source hash.

## Probe Placement

First-pass placement should generate multiple probes per sector at one vertical
layer:

```text
probeHeight = sector floor + 1.2 world units
probeSpacing = 4.0 world units by default
```

Use 4.0 world units as the default starting point. It gives long corridors and
large rooms more than one sample without immediately multiplying bake rays too
hard. Add 2.0 world units as an editor/bake setting later for dense hero spaces
or heavily shadowed maps. If the setting is added in v1, store it in
`SectorLightmapBakeSettings` and include the world-space clamped value in
`ComputeSectorLightmapSourceHash()`.

Placement algorithm:

1. For each `SectorTopologySector`, call `ExtractSectorTopologyLoops()`.
2. Build integer-coordinate outer and hole polygons from loop vertex IDs.
3. Compute the sector's integer-coordinate AABB from the outer loop.
4. Convert `probeSpacing` to topology coordinate units using
   `SectorWorldToAuthoringDistance()` and `SectorCoordSubdivisions`.
5. Walk a 2D grid over the AABB. Offset the grid by half a cell so probes are
   centered in cells.
6. Keep candidates where `SectorTopologyClassifyPointInPolygon(outer, point)`
   is `Inside` and every hole polygon classifies the point as `Outside`.
7. Reject boundary points to avoid probes exactly on walls or portals.
8. Convert accepted X/Y topology coordinates to world X/Z with existing unit
   helpers and set world Y to `SectorAuthoringToWorldDistance(floorZ) + 1.2f`.
9. Reject or clamp probes above sector ceiling. If
   `ceilingWorld - floorWorld < 1.2f`, place at the midpoint between floor and
   ceiling and mark the sector as cramped in bake diagnostics.
10. If a sector is too small for the grid to produce any point, place one probe
    at a polygon-safe representative point. A conservative first fallback is the
    center of the first valid earcut/floor triangle or the first accepted
    inward-biased floor texel position.

Middle blockers:

- V1 should not spend heavy effort solving 2D blocker subtraction for middle
  textures.
- Practical first step: reject candidates very close to any linedef segment and
  rely on alpha-aware visibility rays during lighting evaluation.
- Later, middle textures flagged as solid/blocking can carve out small exclusion
  bands or participate in a 2D clearance test.

This placement is derived data. It should be generated during the bake from the
current topology snapshot and should not invalidate the 2D editor render cache by
itself because no live topology or visible cached 2D editor state changes unless
probe debug drawing is added.

## Probe Lighting Representation

Recommended target representation is an ambient cube per probe:

```text
+X RGB, -X RGB, +Y RGB, -Y RGB, +Z RGB, -Z RGB
```

Why ambient cube:

- It fits the engine's 2008-ish static-lighting direction better than full
  spherical harmonics.
- It is cheaper and easier to debug than SH.
- It gives 3D models directional baked lighting by normal without needing static
  lightmap UVs.
- It can be averaged or hemisphere-weighted for billboards.
- It avoids a single flat sector brightness, which fails in corridors, concave
  rooms, spotlit areas, and spaces with bright/dark ends.

Simple RGB ambient is too crude as the final design, but it is acceptable as a
temporary staged implementation if bake complexity needs to be reduced. A single
dominant direction plus RGB is more complex for sprites and less robust for
multi-light spaces than an ambient cube.

Future usage:

```cpp
Vector3 EvaluateAmbientCube(const Vector3 cube[6], Vector3 normal)
{
    Vector3 n = Vector3Normalize(normal);
    Vector3 rgb = {};
    rgb += cube[0] * std::max( n.x, 0.0f); // +X
    rgb += cube[1] * std::max(-n.x, 0.0f); // -X
    rgb += cube[2] * std::max( n.y, 0.0f); // +Y
    rgb += cube[3] * std::max(-n.y, 0.0f); // -Y
    rgb += cube[4] * std::max( n.z, 0.0f); // +Z
    rgb += cube[5] * std::max(-n.z, 0.0f); // -Z
    return rgb;
}
```

Billboards should normally use the average cube color or a stable hemisphere
blend. They must not use the camera-facing quad normal for baked lighting.

## Baking Probe Values

Best v1 implementation target: bake ambient cubes directly, but with simple
directional sampling that reuses the existing lightmap bake machinery.

Recommended direct-light probe approximation:

- Treat each cube face direction as a virtual normal at the probe position.
- Evaluate static point lights, static spotlights, and directional light against
  that virtual normal.
- Use the same attenuation, cone attenuation, source-radius soft shadow sampling,
  BVH occlusion, and alpha-tested middle occlusion semantics as surface direct
  light.
- For point and spot lights, only add contribution to faces where
  `dot(faceDirection, directionToLight) > 0`.
- For directional light, use `dot(faceDirection, directionToLight) > 0` and cast
  a shadow ray along `directionToLight`. Unlike surface bake, object probes
  should evaluate directional light anywhere the ray is unoccluded; do not
  restrict it to sky-owned surfaces because probes are not surfaces.

Recommended indirect/ambient approximation:

- Add sector ambient color/intensity as a low baseline to all six cube faces.
  Current generated geometry already uses `MakeTopologySectorVertexColor()` for
  sector ambient vertex color, and the source hash includes sector ambient.
- Add a cheap occlusion term per cube face by casting a small fixed set of rays
  in the face hemisphere, similar to `BakeAmbientOcclusion()` but centered on
  the probe.
- Optionally sample bounced light by tracing rays from the probe, hitting the
  baked geometry BVH, and sampling the dilated direct-light atlas at
  `RayHit.lightmapUv` via `SampleDirectLightingAtLightmapUv()`. This reuses the
  existing indirect-bounce data flow and avoids inventing GI.

Staged recommendation:

- Phase 1 should bake ambient cube probes, not just RGB, because the data format
  and runtime API should settle early.
- The first ambient cube quality can be simple: sector ambient plus direct
  static light evaluation per cube face plus alpha-aware occlusion.
- A later quality phase can add probe indirect bounce from nearby lightmap
  samples and per-face AO.

Implementation detail: existing direct-light helpers currently take
`RasterHit`, a source surface ref/index, and source triangle index so they can
ignore self hits. Probe baking should either:

- extract lower-level "object/probe direct light" helpers that do not require a
  source surface, or
- call the same occlusion path with default source refs and `-1` indices.

The first option is cleaner because probes are not surfaces and have no logical
self surface.

## Runtime Lookup API

Conceptual API:

```cpp
struct BakedObjectLightingSample {
    Vector3 position = {};
    Vector3 ambientCube[6] = {};
    bool valid = false;
};

BakedObjectLightingSample SampleBakedObjectLighting(
        const SectorBakedObjectLightProbeSet& probes,
        Vector3 worldPosition,
        int preferredSectorId);
```

Runtime lookup should prefer sector-local samples:

1. Use `preferredSectorId` when the caller has current sector knowledge from
   movement/collision/sector lookup.
2. Interpolate the nearest probes in that sector by inverse distance, capped to a
   small fixed count such as 4.
3. If there is only one sector probe, return it directly.
4. Near portal edges, optionally include adjacent portal-connected sectors in a
   later phase to reduce discontinuities.
5. Fall back to nearest probe in current sector, then nearest probe in adjacent
   portal-connected sectors, then sector ambient, then neutral dim light.

Suggested fallback hierarchy:

```text
1. inverse-distance interpolation of nearby probes in current sector
2. nearest probe in current sector
3. nearest probe in adjacent portal-connected sector
4. current sector ambient color/intensity
5. neutral dim ambient cube, e.g. RGB 0.15 on all faces
```

For v1, a linear scan over probes in the current sector is acceptable if probe
counts stay modest. Store probes sorted or grouped by sector ID so lookup can
skip unrelated sectors cheaply. Add per-sector probe ranges after the data
format exists. Spatial acceleration is only needed if maps produce thousands of
probes and object counts become high.

## Future Billboard Usage

Billboard sprites should not use their camera-facing quad normal for baked
lighting. That makes brightness change as the camera rotates, which is visibly
wrong for static baked light.

Recommended first billboard lighting:

- Sample baked object lighting at sprite/actor center or torso height.
- Use average ambient cube color, upper-hemisphere average, or an
  artist-selected stable component.
- Add dynamic point/spot lights by center-point attenuation.
- For first pass, dynamic billboard lighting may omit `NdotL` entirely or use a
  stable fake normal. It should not use camera-facing normal.

Later options:

- Cylindrical fake normals.
- Sprite normal maps.
- Directional sprite variants.
- Per-material controls for how much baked directionality affects the sprite.

## Future 3D Model Usage

Recommended model lighting:

```text
model baked = ambient cube sampled by model normals
model dynamic = runtime dynamic point/spot lights
final = material * baked + dynamic contribution
```

3D models should not use static world surface lightmaps. They should sample
object probes per object, per draw, or per coarse model part. Per-vertex probe
sampling can come later if needed, but per-object sampling is a useful first
contract.

Dynamic spotlight shadow maps can later shadow model dynamic-light contribution.
That should not change the baked probe data. Dynamic shadow receiving for models
is a renderer feature layered on top of probe lighting.

## Debug and Editor Visualization

Minimal first debug visualization:

- Add a toggle such as "Show Object Lighting Probes".
- Draw small 3D or 2D markers at probe positions.
- Color each marker by average ambient cube RGB.
- Show selected/hovered probe sector ID, world position, and six RGB face
  values in debug text.

No complex UI is needed for v1. Probe debug drawing should use the baked probe
payload and should not mutate topology. If 2D debug drawing is cached later,
probe-data install/rebake should invalidate only that debug/probe cache, not the
topology render cache unless probe markers are folded into it.

## Performance and Storage

Probe count estimate:

- 4.0 world-unit spacing produces about one probe per 16 square world units.
- 2.0 world-unit spacing produces about one probe per 4 square world units, four
  times as many probes and roughly four times the probe bake work.
- Long corridors benefit from 4.0 spacing because they get multiple samples
  along the length instead of one sector average.
- Large rooms may need 4.0 initially and a future 2.0 override for strong
  spotlights, bars, or sharp shadow patterns.

Storage estimate:

- Float ambient cube: position (3 floats) + six RGB faces (18 floats) + sector ID
  is about 88 bytes per probe before vector/string/JSON overhead.
- JSON embedding is convenient but bloated. It is acceptable for v1 if probe
  counts are low to moderate.
- A later binary or quantized sidecar can store RGB16F, RGB10A2-style packed
  values, or 8-bit/log-encoded colors.

Bake cost:

- Direct probe lighting cost is roughly `probeCount * 6 faces * staticLightCount`
  plus shadow rays, multiplied by source-radius samples when enabled.
- Alpha-tested occlusion can add extra ray traversal and CPU alpha-mask sampling.
- Keep progress reporting separate from surface texel work when probes are added.
- Reuse the existing BVH and alpha mask cache from the surface bake.

Runtime cost:

- Per-object lookup can start with sector-grouped nearest-neighbor or small-count
  inverse-distance interpolation.
- Avoid allocating during runtime lookup.
- Precompute per-sector probe ranges after loading baked data.

## Versioning and Invalidation

When probes are implemented:

- Bump `kSectorLightmapBakeVersion`.
- Include probe-generation constants in the hash if they affect output:
  representation version, default probe height, default or configured spacing,
  per-face sample counts, probe AO/indirect sample counts, and any clamps.
- If probe spacing/height become configurable fields in
  `SectorLightmapBakeSettings`, serialize them in `lightmapSettings` and include
  the clamped world-space values in `ComputeSectorLightmapSourceHash()`.
- Store `objectProbes.sourceHash` equal to the final lightmap source hash.
- Treat probes as stale if their source hash does not match the current
  `ComputeSectorLightmapSourceHash()`.
- Old maps without probe data should load. They should report missing/stale
  object probes once a renderer requests probe lighting, but they should not
  crash or invalidate unrelated topology data.

Lightmap source-hash behavior recommendation: object probes are baked output tied
to the lightmap source hash. Adding probe generation should bump the bake version
because old bakes do not contain object probes. Probe visual/debug settings
should not enter the source hash.

## Implementation Plan

Phase 1: Data structures, metadata, serialization, versioning

- Add `SectorBakedObjectLightProbe` and `SectorBakedObjectLightProbeSet`.
- Add optional probe data under `SectorLightmapMetadata` or a sidecar metadata
  path.
- Add JSON read/write with backward-compatible optional fields.
- Bump bake version and include probe constants/settings in the source hash.
- Add serialization and stale/missing probe tests.

Phase 2: Probe placement per sector

- Add a probe placement builder that uses `ExtractSectorTopologyLoops()` and
  `SectorTopologyClassifyPointInPolygon()`.
- Generate one height layer at floor + 1.2 world units.
- Start with 4.0 world-unit spacing, with a path to expose 2.0 or 4.0 as a bake
  setting.
- Reject outer-boundary and hole points.
- Add tests for long corridors, large rooms, concave sectors, holes, and small
  sectors.

Phase 3: Bake simple ambient cube lighting

- Reuse generated geometry, BVH, alpha occluders, alpha mask cache, and world
  static light conversion from the current surface bake.
- Evaluate sector ambient plus direct static point/spot/directional light per
  cube face.
- Use alpha-aware occlusion for probe shadow rays.
- Add bake result counts/timing for probes.

Phase 4: Runtime sampling API

- Add a no-allocation `SampleBakedObjectLighting()` helper.
- Group probes by sector ID at load/preview rebuild time.
- Implement current-sector nearest or inverse-distance interpolation.
- Implement safe fallback to sector ambient and neutral dim light.

Phase 5: Quality extension

- Add per-face AO and optional nearby surface-lightmap indirect gather.
- Tune sample counts and progress reporting.
- Consider adjacent portal-sector interpolation near portals.

Phase 6: Debug visualization and docs

- Add probe marker toggle.
- Color markers by average cube lighting.
- Show selected probe details.
- Document expected model and billboard renderer usage.

## Goblins / Risks

- One probe per sector is too crude for large rooms, corridors, spotlights, bars,
  shadows, and bright/dark sector ends.
- Probe density trades quality against bake time, storage, and runtime lookup
  work.
- Large or concave sectors need polygon-aware placement; AABB center samples are
  not enough.
- Holes and void areas must reject probes.
- Portals and sector edges can create lighting discontinuities unless adjacent
  sector probes are considered later.
- Actor/object height at floor + 1.2 world units is a good first torso-height
  default, but short pickups, floor particles, flying actors, and tall monsters
  may need offsets.
- Tall spaces may need multiple vertical probe layers later.
- Ambient cube quality depends on enough directional samples; simple RGB is
  cheaper but loses model normal response.
- Billboards using camera-facing normals for baked lighting will visibly pulse as
  the camera rotates.
- Dynamic lights can be double-counted if static-authored lights also remain as
  runtime dynamic lights for the same fixture.
- Stale probe data must not be silently used as if valid.
- Missing probe data needs graceful fallback for old maps and partial bakes.
- CPU JSON floats are easy first; GPU buffers, binary sidecars, compression, and
  quantization can wait.
- Source hash and bake version changes are easy to miss; probe-affecting
  settings and constants must be hashed.

## Explicit Out Of Scope

- Implementing probes.
- 3D model renderer.
- Billboard sprite renderer.
- Actor/entity system.
- Skeletal animation.
- Dynamic object shadows.
- Runtime GI.
- Spherical harmonics except as a future alternative.
- Reflection cubemaps.
- Volumetric lights/god rays.
- Shadow map changes.
- Clustered, tiled, or deferred lighting.

