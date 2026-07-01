# Baked Object Lighting Probes Design

## Scope

This document describes the implemented v1 baked object-lighting probe system
for movable 3D models, billboard sprites, pickups, NPCs, particles, and other
dynamic objects. It also records the consumer contract. The probe bake, storage,
load, sample, and debug paths exist, and the sector 3D preview billboard path is
the first runtime object consumer. Model rendering, persistent actor/object
authoring, dynamic object shadows, and runtime GI remain deferred.

Future consumer contract:

- Static world geometry continues to use baked surface lightmaps.
- Movable objects never sample static surface lightmap UVs directly.
- 3D model renderers call `SampleBakedObjectLighting()` per object or draw and
  evaluate the returned low-frequency lighting by model normals.
- Billboard renderers call `SampleBakedObjectLighting()` per sprite or actor and
  use stable probe-derived lighting, not the camera-facing quad normal. The
  sector 3D preview billboard path currently uses an upper-hemisphere ambient
  average from the object's sampled ambient cube.
- Runtime dynamic point/spot lights are added on top of baked object lighting
  for preview billboards in the billboard cutout shader. Dynamic light
  attenuation uses each billboard fragment's world position. Supported dynamic
  spotlight shadow maps attenuate only the owning dynamic spotlight
  contribution; baked object-probe lighting is not shadowed by runtime shadow
  maps.

Current implementation notes:

- Successful lightmap bakes write object probe data to a binary sidecar beside
  the lightmap atlas, using magic `SOPB`, version `1`, and format
  `ambientCubeF32LE`.
- JSON stores compact `bakedLightmap.objectProbes` metadata only: path, version,
  source hash, count, spacing, height, and format.
- `LoadSectorBakedObjectLightProbeRuntimeData()` loads valid sidecars and builds
  sorted per-sector probe ranges for allocation-free sampling.
- `SampleBakedObjectLighting()` prefers the caller's sector, includes unique
  adjacent-sector probe ranges near open two-sided portals, blends up to four
  nearest probes by inverse distance, falls back to any loaded probe when the
  preferred sector has no probes, then sector ambient, then neutral RGB `0.15`
  on all cube faces.
- The 3D preview overlay has a read-only `Show Object Probes` toggle. It draws
  small markers at loaded probe positions, colored by average ambient-cube RGB,
  and reports absent/stale/unavailable probe data in the overlay text.
- Probe debug visualization is visual-only. It does not mutate topology, change
  save data, invalidate the 2D topology render cache, or affect the lightmap
  source hash.

Ambient cube face order:

```text
+X RGB, -X RGB, +Y RGB, -Y RGB, +Z RGB, -Z RGB
```

## Implementation Files and Bake Pipeline

Key implementation files inspected:

- `sources/sector_demo/SectorLightmap.h`
- `sources/sector_demo/SectorLightmap.cpp`
- `sources/sector_demo/SectorLightmapTypes.h`
- `sources/sector_demo/SectorTopologyTypes.h`
- `sources/sector_demo/SectorTopologySerialization.cpp`
- `sources/sector_demo/SectorGeneratedGeometry.h`
- `sources/sector_demo/SectorGeneratedGeometry.cpp`
- `sources/sector_demo/SectorTopologyGeometry.h`
- `sources/sector_demo/SectorTopologyGeometry.cpp`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `tests/SectorTopologyLightmapTests.cpp`
- `tests/SectorTopologySerializationTests.cpp`
- `tests/SectorAuthoringGraphTests.cpp`

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

- `SectorLightmapBakeSettings` stores AO radius/strength, indirect bounce
  radius/strength, `objectProbeSpacingWorld`, and `objectProbeHeightWorld`.
- `SectorLightmapMetadata` stores `path`, `width`, `height`, `sourceHash`, and
  optional compact `objectProbes` sidecar metadata.
- JSON save/load reads and writes `lightmapSettings` and optional `bakedLightmap`
  in `SectorTopologySerialization.cpp`.
- Old maps may omit lightmap settings and baked lightmap metadata.
- `GetSectorLightmapStatus()` reports `None`, `Stale`, or `Valid` based on
  metadata presence, source-hash match, and atlas file existence.

Source hash facts:

- `kSectorLightmapBakeVersion` is currently 10 because successful bakes now
  include object lighting probe sidecar data.
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

## Data Storage

Object lighting probes are derived bake output, not authored topology. They do
not live in vertices, linedefs, sidedefs, sectors, or the editor authoring graph
as editable objects.

V1 stores the probe payload in a binary sidecar beside the lightmap atlas. JSON
stores compact `bakedLightmap.objectProbes` metadata only. Probe arrays are not
embedded in JSON.

Implemented data structs:

```cpp
struct SectorBakedObjectLightProbe {
    int sectorId = 0;
    Vector3 position = {};
    Vector3 ambientCube[6] = {};
};

struct SectorBakedObjectLightProbeMetadata {
    std::string path;
    int version = 0;
    std::string sourceHash;
    int count = 0;
    float probeSpacingWorld = 4.0f;
    float probeHeightWorld = 1.2f;
    std::string format;
};

struct SectorBakedObjectLightProbeRuntimeData {
    std::vector<SectorBakedObjectLightProbe> probes;
    std::vector<SectorBakedObjectLightProbeSectorRange> sectorRanges;
    SectorBakedObjectLightProbeMetadata metadata;
};
```

`SectorLightmapMetadata` contains `SectorBakedObjectLightProbeMetadata
objectProbes`. The surface atlas remains a GPU texture with RGB/A semantics
assigned to direct+indirect lighting and AO; object probes are CPU/runtime query
data with sector IDs, world positions, and ambient cubes.

Missing probe data is nonfatal:

- Old maps with valid surface lightmaps but no probes load.
- `GetSectorBakedObjectLightProbeStatus()` reports `None` when metadata is
  absent and `Stale` when metadata or the sidecar is invalid.
- Runtime `SampleBakedObjectLighting()` returns documented fallback lighting
  when no probe payload is available.

## Binary Sidecar

Sidecar helpers:

- `MakeSectorObjectProbeSidecarPathForLightmapPath()`
- `WriteSectorBakedObjectLightProbeSidecar()`
- `ReadSectorBakedObjectLightProbeSidecar()`
- `LoadSectorBakedObjectLightProbeRuntimeData()`

Path convention:

- The sidecar path is derived from the lightmap atlas path by replacing the
  extension with `.object_probes.bin`.
- Example: `level.lightmap.png` writes `level.lightmap.object_probes.bin`.
- Runtime loading resolves `assets/...` paths through `ResolveSectorAssetPath()`.

Sidecar format:

- Magic: `SOPB`
- Version: `kSectorBakedObjectLightProbeSidecarVersion == 1`
- Metadata format string: `ambientCubeF32LE`
- Endianness: little-endian integer and float writes/reads

Header layout:

```text
char[4]  magic = "SOPB"
u32      version
u32      probeCount
f32      probeSpacingWorld
f32      probeHeightWorld
u32      reserved0 = 0
u32      reserved1 = 0
```

Record layout, repeated `probeCount` times:

```text
i32      sectorId
f32[3]   position world XYZ
f32[3]   ambientCube +X RGB
f32[3]   ambientCube -X RGB
f32[3]   ambientCube +Y RGB
f32[3]   ambientCube -Y RGB
f32[3]   ambientCube +Z RGB
f32[3]   ambientCube -Z RGB
```

That is 88 bytes per probe record plus a 28-byte header.

Validation and failure behavior:

- Writes reject empty paths, non-finite settings, non-finite positions, non-finite
  ambient cube values, and probe counts that do not fit `u32`.
- Writes create the parent directory when needed.
- Reads reject missing files, bad magic, unsupported version, truncated headers
  or records, non-finite settings, too many probes, non-finite payload values,
  and mismatches against expected metadata version/count/format.
- Runtime loading also rejects missing metadata, invalid metadata, stale source
  hashes, and unreadable sidecars.

Implemented JSON metadata fields:

```json
"objectProbes": {
  "path": "...object_probes.bin",
  "version": 1,
  "sourceHash": "...",
  "count": 0,
  "probeSpacingWorld": 4.0,
  "probeHeightWorld": 1.2,
  "format": "ambientCubeF32LE"
}
```

`objectProbes` is omitted when the path is empty. On load, present metadata must
have a non-empty path/source hash/format, positive version, non-negative count,
positive spacing, and non-negative height.

## Probe Placement

`BuildSectorBakedObjectLightProbePlacements()` generates one vertical probe
layer with multiple probes per sector:

```text
default probeSpacingWorld = 4.0
default probeHeightWorld = 1.2
probe world Y = sector floor world Y + probeHeightWorld
```

The settings are stored in `SectorLightmapBakeSettings` as
`objectProbeSpacingWorld` and `objectProbeHeightWorld`. Serialization clamps
spacing to `[0.25, 128.0]` world units and height to `[0.0, 16.0]` world units.
The bake path uses the same clamped values and records them in sidecar metadata.

Implemented placement behavior:

1. `BuildSectorTopologyIndexes()` is built once for the map.
2. Each `SectorTopologySector` extracts loops through
   `ExtractSectorTopologyLoops()`.
3. The outer loop and hole loops are converted to integer coordinate polygons
   from their vertex IDs.
4. The outer loop AABB is scanned on a half-cell-offset grid.
5. Grid spacing converts world units through `SectorWorldToAuthoringDistance()`
   and `SectorCoordSubdivisions`.
6. `IsStrictlyInsideProbePolygon()` keeps only candidates classified `Inside`
   the outer polygon and `Outside` every hole. Boundary points are rejected.
7. Accepted topology X/Y coordinates are converted to world X/Z with
   `SectorCoordToWorldDistance()`.
8. If the requested floor-relative Y is at or above the ceiling, Y is clamped to
   the midpoint between floor and ceiling and a placement diagnostic is recorded.
9. If no grid point survives for a sector, `FindRepresentativeProbePoint()` tries
   the AABB center, the outer-vertex centroid, then a 16x16 interior fallback
   search. A successful fallback records a diagnostic.

Current limitations:

- There is one vertical layer only.
- Middle blockers do not carve placement holes or clearance bands. Their alpha
  and occlusion behavior affects lighting rays, not 2D placement.
- Placement is derived bake data and does not invalidate the 2D topology render
  cache by itself.

## Probe Lighting Representation

V1 bakes one ambient cube per probe:

```text
+X RGB, -X RGB, +Y RGB, -Y RGB, +Z RGB, -Z RGB
```

Ambient cubes give 3D models directional baked lighting by normal without static
surface lightmap UVs. Billboards can use an average or stable hemisphere blend.
They must not use the camera-facing quad normal for baked lighting.

Ambient cube evaluation for future model shaders:

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
blend.

## Probe Baking

`BakeSectorBakedObjectLightProbeAmbientCubes()` bakes the placed probes. The
full lightmap bake calls it after the surface atlas has been exported and before
writing the sidecar.

Probe baking builds its own generated geometry, lightmap layout, opaque bake
triangles, BVH, alpha-test occluder triangles, alpha mask cache, world-space
point lights, world-space spotlights, and directional light from the same map
snapshot. `BakeProbeAmbientCube()` then evaluates each of the six cube face
directions as a virtual normal at the probe position.

Implemented lighting contents:

- Sector ambient baseline is added to every cube face through
  `SectorAmbientBaseline()`.
- Static point lights use the existing `EvaluateDirectLight()` point-light path
  through `EvaluateProbePointLight()`.
- Static spotlights use the existing `EvaluateDirectLight()` spotlight path
  through `EvaluateProbeSpotLight()`.
- Directional light uses `EvaluateDirectionalLight()` through
  `EvaluateProbeDirectionalLight()`. Probe directional light is evaluated at the
  probe position as virtual faces, not only on sky-owned surface texels.
- Point and spot source radii use the same soft-shadow sampling behavior as the
  surface bake (`kDirectSoftShadowSampleCount == 8`) because the existing direct
  light helpers are reused.
- Opaque generated surfaces use the bake BVH for shadow/visibility rays.
- Alpha-tested middle occluders use `RaycastBakeOcclusionAlphaAware()` through
  the same direct-light paths. Missing alpha masks are conservative opaque.
- Each face result is clamped to RGB `[0, 1]` before storage.

Deferred quality work:

- Probe-specific indirect bounce from nearby lightmap samples is not implemented.
- Probe-specific per-face AO is not implemented.
- Middle textures currently participate in alpha-aware light occlusion but do
  not carve placement or cast non-alpha-aware baked shadows beyond existing
  lightmap behavior.

## Runtime Lookup API

Implemented API:

```cpp
struct BakedObjectLightingSample {
    Vector3 ambientCube[6] = {};
    bool valid = false;
};

BakedObjectLightingSample SampleBakedObjectLighting(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        Vector3 worldPosition,
        int preferredSectorId,
        const SectorTopologyMap* mapForFallback);
```

`LoadSectorBakedObjectLightProbeRuntimeData()` loads the sidecar, validates the
metadata/source hash, sorts probes by `sectorId`, and builds
`SectorBakedObjectLightProbeSectorRange` entries. The current implementation
allocates while loading the vectors. `SampleBakedObjectLighting()` itself uses
fixed local arrays and performs no heap allocation.

Sampling behavior:

- If probes are loaded and `preferredSectorId` has a range, stream that range
  once.
- If a topology map is supplied and the sample point is within
  `kObjectProbeAdjacentPortalBlendDistanceWorld` (`1.0` world unit) of an open
  two-sided portal segment from the preferred sector, also stream each adjacent
  sector's probe range once.
- Adjacent sector discovery is capped by
  `kObjectProbeMaxAdjacentBlendSectors` (`8`) and uses a fixed local array.
  Duplicate adjacent sector IDs and accidental `preferredSectorId` matches are
  skipped, so a sector sharing multiple portal edges is not overweighted.
- If the preferred sector has probes but adjacency is unavailable, missing,
  capped, malformed, or has no probe ranges, the query still samples preferred
  sector probes only.
- If the preferred sector has no probes, sample all loaded probes as a
  cross-sector fallback.
- The sampler selects up to four nearest probes.
- If the closest probe is effectively exact, return that cube directly with
  `valid = true`.
- Otherwise, blend selected probes by inverse distance and clamp faces to RGB
  `[0, 1]`, with `valid = true`.
- If no loaded probe can be sampled and `mapForFallback` contains the preferred
  sector, return sector ambient on all six faces with `valid = false`.
- Otherwise, return neutral RGB `0.15` on all six faces with `valid = false`.

Adjacent portal filtering:

- Only valid two-sided linedefs can add adjacent probe sectors.
- The point-to-portal test measures distance from world-position XZ to the
  actual portal segment XZ using closest-point-on-segment distance.
- Vertical openness matches runtime portal visibility:
  `openBottom = max(floorA, floorB)`, `openTop = min(ceilingA, ceilingB)`, and
  the portal is open only when `openBottom < openTop`.
- One-sided walls, void boundaries, malformed linedefs/sidedefs, vertically
  closed portals, and merely nearby non-neighbor sectors do not blend.

Current fallback order:

```text
1. up to four nearest probes from preferred sector plus unique near-portal
   adjacent sectors
2. preferred sector only, when adjacency is unavailable or adds no usable probes
3. up to four nearest probes from all loaded probes, when preferred sector has
   no probes
4. preferred-sector ambient, if a fallback map is supplied
5. neutral RGB 0.15 cube
```

## Future Billboard Usage

Billboard sprites should not use their camera-facing quad normal for baked
lighting. That makes brightness change as the camera rotates, which is visibly
wrong for static baked light.

Implemented first billboard lighting:

- Sample baked object lighting at the ECS object's world position and current
  sector.
- Use an upper-hemisphere ambient cube average for stable baked brightness.
- Add selected dynamic point/spot lights in the cutout shader using
  per-fragment world position.
- Shadow only dynamic spotlight contribution when the selected spotlight has an
  existing runtime shadow-map slot. Dynamic point lights remain unshadowed.
- Do not use the camera-facing quad normal for baked lighting.

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
That should not change the baked probe data. Billboard dynamic shadow receiving
is already layered on top of probe lighting and does not modify object probe
sidecars, bake output, or the lightmap source hash.

## Debug and Editor Visualization

Current debug visualization:

- The 3D preview overlay exposes the `Show Object Probes` checkbox with UI id
  `sector_editor_show_object_probe_debug_overlay`.
- Entering or rebuilding 3D preview loads the baked sidecar into editor debug
  runtime data with `LoadSectorBakedObjectLightProbeRuntimeData()`.
- `DrawPreviewObjectProbeOverlay()` draws small read-only 3D sphere markers at
  baked probe positions when the preview renderer is ready, mouse-look is not
  active, the toggle is enabled, and debug probe data is loaded.
- Marker fill color is the average RGB of the six ambient-cube faces, clamped to
  8-bit color with alpha 235.
- Markers also draw a white wire sphere outline.
- Status text appears beside the toggle. It reports `Object probes: none`,
  `Object probes: N loaded`, `Object probes: unavailable`, or the concrete load
  failure such as stale source hash or missing sidecar.

The debug overlay is 3D-only in v1. It uses the baked probe payload and does not
mutate topology, change save data, invalidate the 2D topology render cache, or
affect the lightmap source hash.

## Performance and Storage

Probe count estimate:

- 4.0 world-unit spacing produces about one probe per 16 square world units.
- 2.0 world-unit spacing, if used later, would produce about one probe per 4
  square world units, four times as many probes and roughly four times the probe
  bake work.
- Long corridors benefit from 4.0 spacing because they get multiple samples
  along the length instead of one sector average.
- Large rooms or strong localized lighting may need future density controls.

Storage estimate:

- V1 uses the binary sidecar, not JSON embedding, for probe payloads.
- Each probe record is 88 bytes: one `i32` sector ID, three `f32` position
  values, and eighteen `f32` ambient-cube RGB values.
- The sidecar header is 28 bytes.
- Future sidecar revisions can add quantization or compression, such as RGB16F,
  packed/log-encoded RGB, or chunk compression.

Bake cost:

- Direct probe lighting cost is roughly `probeCount * 6 faces * staticLightCount`
  plus shadow rays, multiplied by source-radius samples when enabled.
- Alpha-tested occlusion can add extra ray traversal and CPU alpha-mask sampling.
- Probe baking currently rebuilds generated geometry, layout, BVH, and alpha
  occluder data for the probe pass.
- `SectorLightmapBakeResult` reports probe count metadata, placement diagnostic
  count, object probe bake seconds, and sidecar write seconds.

Runtime cost:

- Runtime loading allocates the probe and sector-range vectors.
- Sampling scans the preferred sector range or all loaded probes, keeps up to
  four nearest candidates in a fixed local array, and blends without heap
  allocation.
- Spatial acceleration is deferred until probe/object counts justify it.

## Versioning and Invalidation

Implemented probe versioning behavior:

- `kSectorLightmapBakeVersion` was bumped to 10 when object probe sidecar output
  became part of successful lightmap bakes. Old bakes do not contain object
  probes and stale through the source hash.
- `ComputeSectorLightmapSourceHash()` includes the bake version, probe placement
  spacing/height settings after clamping, and the static world/light inputs that
  affect surface and probe bake results.
- `objectProbes.sourceHash` is stored equal to the final lightmap source hash.
- Object probes are stale if their metadata source hash does not match the
  current `ComputeSectorLightmapSourceHash()`.
- `GetSectorBakedObjectLightProbeStatus()` also reports stale for invalid
  metadata, wrong sidecar version/format, negative counts, invalid settings, or
  missing sidecar files.
- Old maps without probe data still load. They report missing object probes to
  probe consumers without crashing or invalidating unrelated topology data.
- Probe visual/debug settings remain excluded from the source hash.

## Implementation Status

Implemented:

- Probe payload structs, metadata structs, runtime data, sector ranges, and
  sample struct.
- Binary probe sidecar write/read helpers with versioned validation.
- Compact JSON metadata under `bakedLightmap.objectProbes`.
- Lightmap bake version/source-hash integration for probe-affecting settings.
- Per-sector polygon-aware placement with holes, fallback points, and low-ceiling
  diagnostics.
- Ambient cube baking from sector ambient, static point lights, static spotlights,
  directional light, opaque occlusion, source-radius soft shadows, and
  alpha-tested middle occlusion.
- Runtime sidecar loading, source-hash validation, sector-range construction, and
  allocation-free sampling.
- 3D preview debug toggle, marker drawing, and load/status text.
- Sector 3D preview billboard consumption through ECS runtime object lighting.

Deferred:

- Future 3D model renderer, persistent actor/object authoring, and production
  object shaders.
- Multiple vertical probe layers.
- Spatial acceleration for very high probe counts.
- Probe-specific indirect bounce and per-face AO.
- Probe placement exclusion bands for middle blockers.
- Selected-probe detail UI.
- Quantized or compressed sidecar formats.

## Tests

Probe-related coverage is in:

- `tests/SectorTopologyLightmapTests.cpp`
- `tests/SectorTopologySerializationTests.cpp`
- `tests/SectorAuthoringGraphTests.cpp`

Covered behaviors include:

- Source hash changes for probe spacing and height.
- Old bake-version/source-hash invalidation.
- Sidecar round-trip and rejection of invalid files.
- Runtime loading, sector-range construction, unavailable-input rejection, and
  sampling fallback behavior.
- Successful bake sidecar output, metadata, timing/report fields, and
  cancellation cleanup.
- Placement counts, concave-sector rejection, hole rejection, small-sector
  fallback, and low-ceiling clamping.
- Static point, static spotlight, directional, wall-occluded, alpha-occluded,
  ambient, and degenerate finite probe lighting.
- JSON serialization defaults and compact object probe metadata without embedded
  payload arrays.

## Known Gaps / Follow-Ups

- The current consumer is the prototype sector 3D preview billboard path only.
- No multiple-height probe layers for tall spaces, flying actors, or very short
  pickups.
- No probe-specific indirect bounce or per-face AO.
- The probe pass rebuilds bake geometry/BVH instead of sharing the surface bake's
  already-built structures.
- Runtime sampling is a linear scan over a sector range or all probes.

## Explicit Out Of Scope

- 3D model renderer.
- Billboard sprite renderer.
- Actor/entity system.
- Skeletal animation.
- Dynamic object shadows.
- Multiple vertical probe layers.
- Runtime GI.
- Spherical harmonics except as a future alternative.
- Reflection cubemaps.
- Volumetric lights/god rays.
- Shadow map changes.
- Clustered, tiled, or deferred lighting.

## Future Consumer Checklist

- Static world geometry continues to use surface lightmaps.
- Movable models, sprites, pickups, particles, NPCs, and actors sample probes
  through `SampleBakedObjectLighting()`.
- Model shaders should evaluate the ambient cube by model normals.
- Billboard shaders should use average, upper-hemisphere, or other stable
  probe-derived lighting.
- Billboards must not use camera-facing quad normals for baked lighting.
- Runtime dynamic lights are added on top of baked object probe lighting.
- Missing, stale, or unavailable probes must use the documented fallback behavior.
- Movable objects must not sample static surface lightmaps.
