# Sector Baked Object Lighting Probes Plan

## How To Use This Plan

This is a living execution plan.

When an agent is asked to execute this plan, it must:

1. Read this section first.
2. Read the `plan-state-json` block.
3. Identify the selected phase/pass.
4. Execute only that selected phase/pass.
5. Do not skip ahead.
6. Do not execute multiple phases/passes in one run unless the selected item explicitly says it is a combined pass.
7. If the selected item is too broad, update this plan with smaller child passes and stop.
8. If smaller passes are added, do not also implement source changes in the same run unless explicitly instructed.
9. After executing a phase/pass, update this plan with status, date, summary, verification results, and behavior notes.
10. Do not claim manual verification unless it was actually performed.
11. Keep this plan self-tracking so future fresh-context runs can resume from it.

```plan-state-json id="object-light-probes"
{
  "plan_id": "sector_baked_object_lighting_probes_plan",
  "status_values": [
    "Not Started",
    "Planned",
    "In Progress",
    "Completed",
    "Deferred",
    "Blocked",
    "Partial"
  ],
  "items": [
    {
      "id": "phase_01",
      "title": "Probe Data Metadata And Binary Sidecar Format",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_01a",
      "title": "Add Probe Types Metadata And Binary Read Write Helpers",
      "type": "pass",
      "parent": "phase_01",
      "status": "Not Started"
    },
    {
      "id": "phase_01b",
      "title": "Wire Probe Metadata Into Lightmap Status And Serialization",
      "type": "pass",
      "parent": "phase_01",
      "status": "Not Started"
    },
    {
      "id": "phase_02",
      "title": "Probe Placement Builder",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_02a",
      "title": "Generate Sector Probe Positions At Actor Torso Height",
      "type": "pass",
      "parent": "phase_02",
      "status": "Not Started"
    },
    {
      "id": "phase_02b",
      "title": "Add Probe Placement Settings Hashing And Tests",
      "type": "pass",
      "parent": "phase_02",
      "status": "Not Started"
    },
    {
      "id": "phase_03",
      "title": "Bake Ambient Cube Probe Lighting",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_03a",
      "title": "Bake Direct Static Lighting Into Probe Ambient Cubes",
      "type": "pass",
      "parent": "phase_03",
      "status": "Not Started"
    },
    {
      "id": "phase_03b",
      "title": "Add Probe Bake Output Sidecar And Bake Result Stats",
      "type": "pass",
      "parent": "phase_03",
      "status": "Not Started"
    },
    {
      "id": "phase_04",
      "title": "Runtime Probe Loading And Sampling API",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_04a",
      "title": "Load Probe Sidecar And Build Sector Probe Ranges",
      "type": "pass",
      "parent": "phase_04",
      "status": "Not Started"
    },
    {
      "id": "phase_04b",
      "title": "Add SampleBakedObjectLighting Runtime API",
      "type": "pass",
      "parent": "phase_04",
      "status": "Not Started"
    },
    {
      "id": "phase_05",
      "title": "Probe Debug Visualization And Consumer Documentation",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_05a",
      "title": "Add Probe Debug Overlay And Documentation",
      "type": "pass",
      "parent": "phase_05",
      "status": "Not Started"
    },
    {
      "id": "phase_06",
      "title": "Polish Tests Versioning And Plan Completion",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_06a",
      "title": "Bump Bake Version Strengthen Tests And Close Plan",
      "type": "pass",
      "parent": "phase_06",
      "status": "Not Started"
    }
  ]
}
```

## Current Progress

| Phase / Pass                                                         | Status      | Date | Notes                                                                                |
| -------------------------------------------------------------------- | ----------- | ---- | ------------------------------------------------------------------------------------ |
| Phase 1: Probe Data Metadata And Binary Sidecar Format               | Not Started |      | Parent phase.                                                                        |
| Phase 1A: Add Probe Types Metadata And Binary Read Write Helpers     | Not Started |      | First executable pass. Defines data and sidecar format without changing bake output. |
| Phase 1B: Wire Probe Metadata Into Lightmap Status And Serialization | Not Started |      | JSON stores sidecar metadata/path only, not full probe payload.                      |
| Phase 2: Probe Placement Builder                                     | Not Started |      | Parent phase.                                                                        |
| Phase 2A: Generate Sector Probe Positions At Actor Torso Height      | Not Started |      | Multiple probes per sector at floor + 1.2 world units.                               |
| Phase 2B: Add Probe Placement Settings Hashing And Tests             | Not Started |      | Settings/source hash/version integration.                                            |
| Phase 3: Bake Ambient Cube Probe Lighting                            | Not Started |      | Parent phase.                                                                        |
| Phase 3A: Bake Direct Static Lighting Into Probe Ambient Cubes       | Not Started |      | Static point/spot/directional probe direct lighting.                                 |
| Phase 3B: Add Probe Bake Output Sidecar And Bake Result Stats        | Not Started |      | Writes binary sidecar next to lightmap atlas.                                        |
| Phase 4: Runtime Probe Loading And Sampling API                      | Not Started |      | Parent phase.                                                                        |
| Phase 4A: Load Probe Sidecar And Build Sector Probe Ranges           | Not Started |      | Runtime data preparation.                                                            |
| Phase 4B: Add SampleBakedObjectLighting Runtime API                  | Not Started |      | Consumer API for future sprites/models.                                              |
| Phase 5: Probe Debug Visualization And Consumer Documentation        | Not Started |      | Parent phase.                                                                        |
| Phase 5A: Add Probe Debug Overlay And Documentation                  | Not Started |      | Visualization and docs for future renderers.                                         |
| Phase 6: Polish Tests Versioning And Plan Completion                 | Not Started |      | Parent phase.                                                                        |
| Phase 6A: Bump Bake Version Strengthen Tests And Close Plan          | Not Started |      | Final validation and closure.                                                        |

## Execution Tracking Rules

* Each pass must leave the project buildable and runnable.
* Each pass final report must state whether source code changed.
* Each implementation pass must update this document before finishing.
* The update should be small and local.
* Do not rewrite unrelated phases when marking progress.
* If behavior is intended to remain unchanged, explicitly state that.
* If a pass changes bake output, bake versioning, source hash behavior, binary file formats, texture/lightmap metadata, runtime loading, rendering, save/load, cache invalidation, or build/test behavior, clearly say so.
* Do not claim manual GUI verification unless it was actually performed.
* If a pass is too broad, split it into smaller child passes and stop without source changes.

## Goal And Desired End State

Add baked object lighting probes for future movable 3D models, billboard sprites, actors, pickups, particles, and other dynamic objects.

Desired end state:

* Static world geometry continues to use baked surface lightmaps.
* Movable objects do not sample static surface lightmap UVs directly.
* Lightmap baking also produces multiple object-lighting probes per sector.
* Probes are generated at actor/object lighting height, initially floor + 1.2 world units.
* Each probe stores an ambient cube:

    * +X RGB
    * -X RGB
    * +Y RGB
    * -Y RGB
    * +Z RGB
    * -Z RGB
* Probe payload is stored in a binary sidecar file beside the lightmap atlas, not expanded inline into JSON.
* JSON stores only compact metadata/path/source hash/count/settings for the probe sidecar.
* Runtime can load probe data and sample it with a documented API.
* Future billboard renderers and 3D model renderers have a clear lighting contract.
* No 3D model renderer, billboard renderer, entity system, animation system, or dynamic object shadow system is implemented in this plan.

Conceptual future consumer contract:

```text
3D models:
  baked = ambient cube sampled by model normal
  dynamic = runtime dynamic point/spot lights added on top

billboard sprites:
  baked = stable probe average/hemisphere lighting
  dynamic = center-point dynamic lights added on top
  do not use camera-facing billboard normal for baked lighting

static world:
  continues using surface lightmaps
```

## Binary Sidecar Decision

Probe payload should be stored in a binary sidecar from v1.

Reason:

* Probe data is derived bake output, not human-authored topology.
* Ambient cube payload is numeric and not meaningfully human-readable.
* JSON would produce large files, noisy diffs, and wasted labels/ASCII floats.
* The lightmap atlas already uses an external file.
* A sidecar placed beside the atlas keeps baked derived outputs together.

Recommended sidecar path:

```text
<lightmap atlas basename>.object_probes.bin
```

Example:

```text
assets/levels/test5/test5.lightmap.png
assets/levels/test5/test5.object_probes.bin
```

Recommended JSON metadata under `bakedLightmap`:

```json
{
  "path": "assets/levels/test5/test5.lightmap.png",
  "width": 1024,
  "height": 1024,
  "sourceHash": "...",
  "objectProbes": {
    "path": "assets/levels/test5/test5.object_probes.bin",
    "version": 1,
    "sourceHash": "...",
    "count": 123,
    "probeSpacingWorld": 4.0,
    "probeHeightWorld": 1.2,
    "format": "ambientCubeF32LE"
  }
}
```

Use project-relative path style consistent with the existing lightmap atlas path.

Binary format should be simple and versioned.

Suggested little-endian format:

```text
Header:
  magic[4] = "SOPB"       // Sector Object Probe Binary
  uint32 version = 1
  uint32 probeCount
  float32 probeSpacingWorld
  float32 probeHeightWorld
  uint32 reserved0
  uint32 reserved1

Probe record repeated probeCount times:
  int32 sectorId
  float32 positionX
  float32 positionY
  float32 positionZ
  float32 cubeRgb[6][3]   // +X,-X,+Y,-Y,+Z,-Z
```

Do not compress or quantize in v1.

Do not put labels/JSON structure in the binary payload.

## Dependency Direction Rules

* Probe generation may depend on lightmap bake inputs, generated geometry, static lights, sector ambient, BVH occlusion, and alpha-tested bake occlusion.
* Probe generation must not change authored topology.
* Probe payload is baked output and must not live in linedefs/sidedefs/sectors/authoring graph.
* Runtime probe lookup may depend on loaded baked probe data and a preferred sector ID.
* Runtime probe lookup must not mutate baked data.
* Future object/model/sprite renderers may call the probe sampling API.
* World surface rendering must continue using lightmaps directly.
* Dynamic lights are added on top by future object renderers.
* Probe sidecar validity is tied to the lightmap source hash/bake version.
* Missing or stale probe sidecar data must fail safely with fallback lighting.

## Proposed Phases

### Phase 1: Probe Data Metadata And Binary Sidecar Format

Goal:

Add probe data structures, metadata, and binary sidecar read/write helpers without changing bake output yet.

Why it helps:

This establishes the storage contract before probe placement and lighting computation.

Files/functions likely touched:

* `sources/sector_demo/SectorLightmapTypes.h`
* `sources/sector_demo/SectorLightmap.h`
* `sources/sector_demo/SectorLightmap.cpp`
* `sources/sector_demo/SectorTopologyTypes.h`
* `sources/sector_demo/SectorTopologySerialization.cpp`
* serialization tests
* lightmap tests

Exact behavior that must remain unchanged:

* Current lightmap atlas baking unchanged.
* Current lightmap metadata remains backward compatible.
* Maps without object probe metadata load normally.
* Runtime rendering unchanged.
* Editor tools unchanged.
* No object/model/sprite renderer added.

Risks/goblins:

* Binary format not versioned.
* JSON accidentally embeds full probe payload.
* Probe sidecar path handling inconsistent with lightmap atlas path.
* Old maps incorrectly reported invalid.
* Endianness/struct padding assumptions leaking into file format.

Non-goals:

* No probe placement.
* No probe lighting bake.
* No runtime sampling.
* No debug overlay.
* No model/sprite rendering.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant serialization/lightmap metadata tests.

Final report expectations:

* State binary format/magic/version.
* State metadata fields.
* State path behavior.
* State backward compatibility behavior.
* Confirm bake/render output unchanged.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 1 `Completed` only after Phase 1A and Phase 1B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 1A: Add Probe Types Metadata And Binary Read Write Helpers

Goal:

Define baked object probe data and sidecar read/write helpers.

Implementation guidance:

Add types similar to:

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
```

Use actual project naming/style.

Add sidecar write/read helpers that do not rely on C++ struct binary layout.

Write individual fields explicitly in little-endian order.

Use magic/version validation.

On read:

* bad magic fails gracefully
* unsupported version fails gracefully
* count mismatch fails gracefully
* truncated file fails gracefully
* non-finite floats fail gracefully
* invalid sector IDs can be preserved or rejected according to existing style, but document behavior

Do not wire helpers into the bake path yet unless needed for tests.

Tests:

* write/read round-trip with several probes
* bad magic rejected
* bad version rejected
* truncated file rejected
* metadata count mismatch detected where applicable
* no JSON full-payload writing

#### Phase 1B: Wire Probe Metadata Into Lightmap Status And Serialization

Goal:

Add optional object probe sidecar metadata under baked lightmap metadata.

Implementation guidance:

Add optional `objectProbes` metadata to `SectorLightmapMetadata` / serialization.

JSON should store compact metadata only:

```json
"objectProbes": {
  "path": "...object_probes.bin",
  "version": 1,
  "sourceHash": "...",
  "count": 123,
  "probeSpacingWorld": 4.0,
  "probeHeightWorld": 1.2,
  "format": "ambientCubeF32LE"
}
```

Do not store probe arrays in JSON.

Status behavior:

* old maps without `objectProbes` load
* missing probe metadata should not invalidate current surface lightmaps unless a caller explicitly requires object probes
* add helper/status function if useful, for example:

    * surface lightmap status
    * object probe status
* stale/missing object probes should be detectable later

Tests:

* old baked lightmap metadata without object probes round-trips
* baked lightmap metadata with object probe metadata round-trips
* metadata does not embed full probe payload
* missing sidecar path/status handled safely

### Phase 2: Probe Placement Builder

Goal:

Generate multiple probe positions per sector at actor torso height.

Why it helps:

One probe per sector is too crude for long corridors, large rooms, bars, spotlights, and bright/dark ends.

Files/functions likely touched:

* `sources/sector_demo/SectorLightmap.cpp`
* `sources/sector_demo/SectorTopologyGeometry.*`
* `sources/sector_demo/SectorTopologyTypes.h`
* probe placement tests

Exact behavior that must remain unchanged:

* Bake output unchanged until Phase 3.
* Runtime rendering unchanged.
* Save/load unchanged except metadata from Phase 1.
* No probe sidecar emitted unless explicitly selected later.

Risks/goblins:

* AABB-only placement creating probes outside concave sectors.
* Holes/voids receiving probes.
* Probes placed exactly on walls/portals.
* Small sectors producing no probes.
* Unit conversion mistakes between authoring/topology/world units.
* Probe height above low ceilings.

Non-goals:

* No probe lighting calculation yet.
* No runtime sampling yet.
* No debug overlay yet.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run probe placement tests.

Final report expectations:

* State placement algorithm.
* State default spacing/height.
* State small-sector fallback.
* State unit conversion behavior.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 2A: Generate Sector Probe Positions At Actor Torso Height

Goal:

Add a probe placement builder.

Implementation guidance:

Default placement:

```text
probeSpacingWorld = 4.0
probeHeightWorld = 1.2
```

For each non-void derived topology sector:

1. Extract loops using existing topology loop extraction.
2. Build outer and hole polygons.
3. Compute sector AABB in topology/authoring coordinates.
4. Convert spacing from world to authoring units.
5. Walk grid cell centers over the AABB.
6. Keep points inside outer polygon and outside all holes.
7. Reject boundary points.
8. Convert accepted points to world position.
9. Set Y to sector floor world height + 1.2.
10. If sector ceiling is too low, clamp or place at midpoint and record diagnostic.
11. If no grid point survives, place one safe representative probe inside the sector.

Tests:

* long corridor gets multiple probes
* large room gets multiple probes
* concave sector does not place probes outside polygon
* sector with hole/void rejects hole probes
* small sector gets one fallback probe
* low ceiling does not produce invalid probe height
* world/authoring unit conversion works

#### Phase 2B: Add Probe Placement Settings Hashing And Tests

Goal:

Make probe placement settings part of bake settings/hash.

Implementation guidance:

Add probe settings to lightmap bake settings if appropriate:

```cpp
float objectProbeSpacingWorld = 4.0f;
float objectProbeHeightWorld = 1.2f;
```

Use actual project style.

If adding settings UI is too much for this pass, add the settings data/defaults and leave UI for later.

Include clamped settings in `ComputeSectorLightmapSourceHash()` because they affect probe output.

If object probes share the same source hash as lightmaps, probe setting changes stale the baked output.

Tests:

* changing probe spacing changes source hash
* changing probe height changes source hash
* default settings load when missing
* settings serialize if project style expects lightmapSettings to include them
* old maps load with defaults

### Phase 3: Bake Ambient Cube Probe Lighting

Goal:

Bake ambient cube lighting values for each generated probe.

Why it helps:

This creates the actual static/baked lighting future movable objects can use.

Files/functions likely touched:

* `sources/sector_demo/SectorLightmap.cpp`
* `sources/sector_demo/SectorLightmapTypes.h`
* static light evaluation helpers
* BVH/occlusion helpers
* lightmap tests

Exact behavior that must remain unchanged:

* Surface lightmap atlas RGB/A behavior unchanged except source hash/version if changed.
* Static light evaluation for surfaces unchanged.
* Dynamic shadow maps unchanged.
* Runtime rendering unchanged.
* No model/sprite renderer added.

Risks/goblins:

* Direct-light helpers assume a surface hit/source surface.
* Direction convention mistakes for cube faces.
* Directional light behavior differs from object-probe needs.
* Probe lighting too dark/bright compared to world.
* Bake time increases.
* Alpha occlusion not reused.

Non-goals:

* No ambient cube indirect bounce unless explicitly selected later.
* No runtime sampling yet.
* No object rendering yet.
* No dynamic lights included in probes.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run relevant lightmap/probe tests.

Final report expectations:

* State probe lighting formula.
* State which static light types contribute.
* State occlusion behavior.
* State whether directional light contributes everywhere or only sky-linked conditions.
* State bake stats.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 3 `Completed` only after Phase 3A and Phase 3B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 3A: Bake Direct Static Lighting Into Probe Ambient Cubes

Goal:

Compute ambient cube values at probe positions.

Implementation guidance:

For each probe and each cube face direction:

```text
faceDirection = +X, -X, +Y, -Y, +Z, -Z
cubeFaceRgb = sectorAmbientBaseline + staticDirectLightingAlongFace
```

Static direct contribution:

* static point lights
* static spotlights
* static directional light

Use existing static attenuation/cone/sourceRadius semantics where practical.

For each light:

* compute direction from probe to light or incoming directional vector
* only add to face if `dot(faceDirection, lightDirection) > 0`
* use alpha-aware BVH occlusion
* use sourceRadius samples for static point/spot if practical
* handle degenerate directions safely

Sector ambient:

* add sector ambient color/intensity baseline to all faces
* clamp or scale consistently with current baked/direct lighting expectations

Directional light:

* for object probes, consider evaluating directional light anywhere unoccluded rather than only sky-owned surfaces
* document final behavior clearly

Tests:

* point light contributes strongest to facing cube side
* spotlight cone affects probe cube contribution
* occluder blocks probe contribution
* alpha-tested occluder lets transparent texels pass
* sector ambient appears on all faces
* degenerate cases produce finite output

#### Phase 3B: Add Probe Bake Output Sidecar And Bake Result Stats

Goal:

Write baked probe data to the binary sidecar during lightmap bake and report stats.

Implementation guidance:

During successful lightmap bake:

* generate probe positions
* compute ambient cubes
* write sidecar next to final lightmap atlas
* write temporary sidecar during async/temp bake if current bake flow uses temporary output paths
* atomically/consistently move or finalize sidecar together with atlas where practical
* update baked lightmap metadata with object probe metadata
* record probe count/timing in bake result/debug status

If bake is cancelled or fails:

* do not leave metadata pointing to a missing/partial sidecar
* clean temp sidecar if existing project flow supports it

Tests:

* successful bake writes sidecar and metadata
* cancelled/failed bake does not mark probes valid
* metadata count matches sidecar count
* sidecar source hash matches bake source hash
* old atlas-only bakes handled safely

Manual smoke:

* bake a small map
* confirm `.object_probes.bin` appears next to `.lightmap.png`
* confirm metadata references it
* confirm old maps without sidecar still load

### Phase 4: Runtime Probe Loading And Sampling API

Goal:

Load baked probe sidecars and expose a runtime sampling API for future movable objects.

Why it helps:

Future sprite/model systems need a stable contract before they are built.

Files/functions likely touched:

* `SectorMeshPreview` or runtime resource loading code
* new baked object lighting probe runtime helper files
* lightmap metadata/status code
* tests

Exact behavior that must remain unchanged:

* Existing sector rendering unchanged.
* Existing dynamic lighting unchanged.
* No model/sprite renderer added.
* Missing probe sidecar does not crash.

Risks/goblins:

* Runtime lookup allocating every query.
* Bad fallback when no probe exists.
* Sector edge discontinuities.
* Stale sidecar silently used.
* Large maps doing linear scan over all probes per object.

Non-goals:

* No 3D model renderer.
* No billboard renderer.
* No dynamic object shadows.
* No per-vertex probe sampling yet.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run runtime/probe tests.

Final report expectations:

* State load path.
* State grouping/indexing by sector.
* State sampling algorithm.
* State fallback behavior.
* State verification results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 4 `Completed` only after Phase 4A and Phase 4B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 4A: Load Probe Sidecar And Build Sector Probe Ranges

Goal:

Prepare baked probe data for runtime lookup.

Implementation guidance:

Load the sidecar if metadata exists and source hash/version match.

Build runtime structure:

```text
vector probes
per-sector sorted ranges or map sectorId -> [begin,end)
```

No allocations during sampling queries.

Status/fallback:

* missing sidecar -> probes unavailable
* stale source hash -> probes unavailable/stale
* bad binary read -> probes unavailable
* surface lightmap can still render if probe sidecar missing, but object lighting API should report fallback

Tests:

* valid sidecar loads
* stale hash rejected
* missing sidecar handled
* sector ranges built correctly
* malformed binary rejected safely

#### Phase 4B: Add SampleBakedObjectLighting Runtime API

Goal:

Add future consumer API for sprites/models.

Implementation guidance:

Add conceptual API similar to:

```cpp
struct BakedObjectLightingSample {
    Vector3 ambientCube[6];
    bool valid;
};

BakedObjectLightingSample SampleBakedObjectLighting(
    const SectorBakedObjectLightProbeRuntimeData& probes,
    Vector3 worldPosition,
    int preferredSectorId,
    const SectorTopologyMap* mapForFallback);
```

Use actual style and dependency direction.

Sampling v1:

* prefer probes in `preferredSectorId`
* select nearest N probes, suggested N = 4
* inverse-distance weighted interpolation
* if one probe exists, use it
* if no probes in preferred sector, fallback to nearest adjacent/any probe only if cheap and deterministic
* fallback to sector ambient if map/sector available
* fallback to neutral dim cube if nothing else exists

Neutral fallback:

```text
ambient cube RGB 0.15 on all faces
```

Tests:

* nearest/interpolated sample works
* same-sector probes preferred
* no-probe fallback uses sector ambient
* neutral fallback safe
* no allocations if testable
* result is finite

### Phase 5: Probe Debug Visualization And Consumer Documentation

Goal:

Make object probes visible/debuggable and document how future systems should use them.

Why it helps:

Probe lighting is hard to reason about without seeing where probes are and what values they carry.

Files/functions likely touched:

* debug drawing/overlay code
* docs
* runtime probe helpers
* tests if doc generation is not relevant

Exact behavior that must remain unchanged:

* Rendering output unchanged unless debug toggle is enabled.
* Probe sampling unchanged.
* Bake output unchanged.

Risks/goblins:

* Debug overlay clutter.
* Accidentally making probe display mutate topology/cache.
* Future consumers misusing billboard normals.

Non-goals:

* No model/sprite renderer.
* No editor editing of probes.
* No probe gizmos.
* No runtime GI.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Final report expectations:

* State debug toggle/overlay behavior.
* State docs updated.
* State verification results.
* State manual GUI smoke status if performed.

How to update this plan after completion:

* Mark Phase 5A and Phase 5 `Completed`.
* Add date, summary, verification results, and behavior notes.

#### Phase 5A: Add Probe Debug Overlay And Documentation

Goal:

Add minimal debug visualization and documentation for future users.

Implementation guidance:

Debug overlay:

* toggle `Show Object Probes` or equivalent
* draw small markers at probe positions
* marker color = average ambient cube RGB
* optionally selected/hovered probe debug text:

    * sector ID
    * world position
    * six cube face RGB values

Do not allow editing baked probes.

Documentation:

Create or update:

```text
docs/sector_baked_object_lighting_probes_design.md
```

and/or a shorter consumer doc such as:

```text
docs/sector_dynamic_object_lighting_contract.md
```

Document future consumer contract:

* static world surfaces use lightmaps
* models call `SampleBakedObjectLighting()`
* billboards call `SampleBakedObjectLighting()`
* billboards should not use camera-facing normals for baked lighting
* dynamic lights are added on top
* missing probes fallback behavior
* ambient cube face order

### Phase 6: Polish Tests Versioning And Plan Completion

Goal:

Finalize versioning, tests, docs, and plan closure.

Why it helps:

Object probes affect bake output and future runtime systems; stale or undocumented behavior will produce subtle lighting goblins.

Files/functions likely touched:

* bake version/source hash constants
* tests
* docs
* this plan document

Exact behavior that must remain unchanged:

* World surface lightmap rendering unchanged.
* Dynamic lights unchanged.
* Dynamic shadows unchanged.
* Save/load backward compatible.
* No model/sprite renderer added.

Risks/goblins:

* Forgetting to bump bake version.
* Incomplete stale/missing sidecar detection.
* Weak tests around binary files.
* Probe output using stale source hash.
* Too much polish scope.

Non-goals:

* No runtime GI.
* No entity system.
* No model/sprite renderer.
* No volumetric/god rays.

Suggested checks:

```bash
git diff --check
git diff --stat
git status --short
```

Run full relevant test suite.

Final report expectations:

* State bake version/source hash behavior.
* State binary sidecar behavior.
* State tests added/updated.
* State docs updated.
* State manual smoke status.
* State known deferred work.

How to update this plan after completion:

* Mark Phase 6A and Phase 6 `Completed`.
* If all phases are complete, ensure all parent phases are `Completed`.
* Leave a final completion note.

#### Phase 6A: Bump Bake Version Strengthen Tests And Close Plan

Goal:

Complete baked object lighting probes.

Implementation guidance:

Bump `kSectorLightmapBakeVersion` or equivalent.

Reason:

```text
Baked output now includes object lighting probe data.
Old bakes do not contain object probes and should not be treated as complete for object lighting.
```

Ensure tests cover:

* sidecar binary round-trip
* metadata round-trip
* old metadata without probes loads
* probe status detects missing sidecar
* source hash mismatch marks probes stale
* probe placement for large/concave/hole/small sectors
* ambient cube bake finite values
* static point contribution
* static spot contribution
* directional contribution behavior
* alpha-tested occlusion behavior where practical
* runtime sampling interpolation
* sector ambient fallback
* neutral fallback
* docs mention billboard/model usage contract

Manual smoke checklist:

* bake a map
* confirm `.object_probes.bin` sidecar is created
* confirm metadata references sidecar
* toggle probe debug overlay
* see multiple probes in long sectors/rooms
* see probe colors vary between bright/dark areas
* confirm old maps without sidecar still load safely
* confirm world lightmap rendering still works

## Deferred Decisions For Later Plans

These are intentionally out of scope:

* 3D model renderer.
* Billboard sprite renderer.
* Actor/entity system.
* Skeletal animation.
* Dynamic object shadow receiving.
* Per-vertex probe sampling.
* Multiple vertical probe layers.
* Adjacent-sector portal interpolation.
* Spherical harmonics.
* Reflection cubemaps.
* Compressed/quantized probe formats.
* Probe sidecar compression.
* Runtime GI.
* Volumetric lights/god rays.
* Shadow map changes.

## Final Completion Criteria

This plan is complete when:

* probe metadata is stored compactly in JSON
* probe payload is stored in a binary sidecar next to the lightmap atlas
* old maps without probe sidecars load safely
* probes are generated per sector at floor + 1.2 world units by default
* large sectors/corridors receive multiple probes
* probes store ambient cube lighting
* baked static point/spot/directional lighting contributes to probes as designed
* alpha-aware static occlusion is reused for probe lighting
* probe sidecar is written during successful bake
* probe sidecar is not marked valid on failed/cancelled bake
* runtime can load probe sidecars
* `SampleBakedObjectLighting()` exists for future sprites/models
* missing/stale probes fall back safely
* debug visualization and consumer documentation exist
* bake version/source hash behavior is correct
* no model/sprite/entity/volumetric/shadow-map scope leaks into this implementation
