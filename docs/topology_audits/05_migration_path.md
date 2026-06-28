# Migration Path Audit

## Summary

Old/current levels can keep loading if the future authoring graph is added additively and the current topology v2 / linedef JSON remains accepted as a complete derived topology document. The current loader in `sources/sector_demo/SectorTopologySerialization.cpp` is strict about root identity fields and validated runtime topology, but it already supports optional additive sections such as `staticLights`, `lightmapSettings`, `previewSettings`, `skySettings`, `directionalLight`, `bakedLightmap`, `ceilingSky`, decals, middle textures, and linedef flags.

The safest migration path is to keep `SectorTopologyMap` as the loadable derived topology, add an optional authoring graph section later, and synthesize an initial authoring graph from topology v2 when that section is absent. Saving both authoring graph and derived topology during the migration window is lower risk than switching immediately to authoring-only files, because mesh, collision, preview, selection, and lightmap code already consume validated `SectorTopologyMap`.

Implementation note (2026-06-28): The completed transition intentionally chose
the cleaner breaking policy from the later design document instead of this
audit's safest two-track compatibility path. Graph-native authoring documents
use `formatVersion: 3` and `topology: "authoringGraph"` and do not require
persisted topology-v2 `vertices`/`linedefs`/`sidedefs`/`sectors` arrays. A
one-way topology-v2-to-authoring import remains available as bootstrap/dev
compatibility for valid old maps, not as the normal source-of-truth model.

## Current Serialized Format

`ParseMap()` in `SectorTopologySerialization.cpp` requires:

* `formatVersion == 2`
* `topology == "linedef"`
* `coordSubdivisions == SectorCoordSubdivisions`
* object `textures`
* arrays `vertices`, `linedefs`, `sidedefs`, and `sectors`

Vertices persist `id`, integer `x`, and integer `y`. Linedefs persist `id`, `startVertexId`, `endVertexId`, `frontSideDefId`, `backSideDefId`, and optional `flags`. Sidedefs persist `id`, `lineDefId`, `side`, `sectorId`, required `wall`/`lower`/`upper`, and optional `middle`. Sectors persist `id`, `name`, heights, floor/ceiling texture IDs, UVs, ambient settings, required default wall parts, optional `ceilingSky`, and optional decals.

`SerializeMap()` writes the same topology v2 identity fields and sorts textures, vertices, linedefs, sidedefs, sectors, and static lights by stable ID. It validates the map through `ValidateForSerialization()` before writing.

The public file APIs are `LoadSectorTopologyMapFromJsonString()`, `SaveSectorTopologyMapToJsonString()`, `LoadSectorTopologyMap()`, and `SaveSectorTopologyMap()` in `SectorTopologySerialization.cpp`. Editor document wrappers are `LoadSectorTopologyDocumentFromAsset()` and `SaveSectorTopologyDocument()` in `sources/sector_editor/SectorEditorDocumentActions.cpp`.

## Compatibility Requirements

Current files must keep loading as topology v2 linedef maps. `TestSchemaValidation()` in `tests/SectorTopologySerializationTests.cpp` rejects missing root identity fields, `formatVersion` other than `2`, `topology` other than `"linedef"`, and a mismatched `coordSubdivisions`.

The loader also calls `ValidateForSerialization()` after parsing, so legacy topology must be valid runtime topology, not a permissive editing graph. A future authoring graph cannot be stored in place of `vertices`/`linedefs`/`sidedefs`/`sectors` unless the loader learns a separate path.

Failed loads leave the output map unchanged. Tests around invalid linedef flags, invalid decal data, invalid middle data, malformed schema, and invalid static lights explicitly check that behavior. Migration code should preserve that all-or-nothing load contract.

## Existing Default / Omitted-Field Behavior

The required core topology fields are not defaulted. Missing `textures`, `vertices`, `linedefs`, `sidedefs`, or `sectors` is rejected.

Several additive fields are optional:

* Missing `staticLights` loads as an empty vector.
* Missing `lightmapSettings` uses default `SectorLightmapBakeSettings`.
* Missing `previewSettings` uses `DefaultSectorPreviewSettings()`; missing nested preview fields also default and normalize.
* Missing `skySettings` uses `DefaultSectorTopologySkySettings()`; default sky settings are omitted on save.
* Missing `directionalLight` uses `DefaultSectorTopologyDirectionalLightSettings()`; default directional light settings are omitted on save.
* Missing `bakedLightmap` loads empty metadata and is omitted on save unless path, dimensions, and source hash are all valid.
* Missing sector `ceilingSky` loads false and false is omitted on save.
* Missing sidedef `middle` loads default empty middle settings and is omitted unless non-default.
* Missing decals load default no-decal state and empty-texture decals are omitted.
* Missing linedef `flags` or missing `blocksPlayer` loads false and default flags are omitted.

Texture filter loading has one compatibility quirk: `"bilinear"` is accepted as a legacy spelling and maps to `SectorTextureFilter::Anisotropic8x`, while current save writes `"linear"` for `Bilinear` and `"anisotropic8x"` for anisotropic.

## Current Level Files And Samples

Known topology JSON files include:

* `assets/sector_demo/sector_editor_working_level.json`
* `assets/sector_demo/sector_demo_level.json`
* `assets/levels/decal_test/decal_test.json`
* `assets/levels/middle_texture_test/middle_texture_test.json`
* `assets/levels/multi_sector_test/multi_sector_test.json`
* `assets/levels/test01/test01.json`
* `assets/levels/test2/test2.json`
* `assets/levels/topology_smoke/topology_smoke.json`

`SectorDemo::Init()` loads its map through `LoadSectorTopologyMap()`. The editor level browser uses `ScanLevels()` in `SectorEditorDocumentActions.cpp`, which only scans `assets/levels/<name>/<name>.json` style files. Save paths are built by `BuildLevelPaths()` and saving writes through `SaveSectorTopologyDocument()`.

The sample files are already topology v2 shaped. Some current files contain baked lightmap metadata and some omit newer optional fields, so migration tests should include both minimal older-style files and fully populated working levels.

## Additive Authoring Graph Options

An optional root object such as `authoringGraph` would fit the existing additive pattern if the loader ignores or separately parses it while continuing to require valid topology v2 fields. The current parser does not reject unknown root fields, so adding an extra root field is likely backward-compatible for current code paths that use this parser. Older binaries would parse, validate, and save the derived topology while dropping the unknown authoring graph on save.

That drop-on-save behavior is the major compatibility risk for additive data. Once authoring graph data matters, saving with an older or graph-unaware build would silently discard it unless migration adds a preservation strategy or a format/version guard.

Adding a new `formatVersion` immediately would break current loading because `ParseMap()` requires `2`. A safer migration is to keep `formatVersion: 2` while the file still contains complete topology v2, and use an optional graph field or explicit graph sub-version for new data.

## Conversion From Current Topology To Authoring Graph

Topology v2 can initially seed an authoring graph one-to-one:

* vertices become authoring vertices with the same IDs and exact integer coordinates
* linedefs become authoring lines with the same IDs, endpoint IDs, orientation, and `blocksPlayer`
* front/back sidedefs become directed-side metadata with wall/lower/upper/middle materials and decals
* sectors become face/property anchors with the same sector IDs and sector-level properties
* textures, static lights, preview settings, sky settings, directional light, lightmap settings, and baked lightmap metadata remain map-level data

This conversion is straightforward for valid maps because validation already guarantees the linedef/sidedef/sector graph can extract closed loops. It is not enough for future loose-line states, but it lets current files load without requiring an authored graph to have existed historically.

The hard part is identity after re-derivation. `ComputeSectorLightmapSourceHash()` in `sources/sector_demo/SectorLightmap.cpp` hashes topology IDs and bake-relevant properties, so converting and then regenerating topology with different vertex/linedef/sidedef/sector IDs will stale baked lightmaps even if geometry is visually equivalent.

## Save Format Options

Option 1: save only current topology v2 until the authoring graph implementation exists. This is the current behavior and keeps all existing levels compatible, but it cannot persist loose authoring state.

Option 2: save topology v2 plus optional `authoringGraph`. This is the best migration window candidate. Current runtime/editor consumers can keep using `SectorTopologyMap`, old files can synthesize graph data on load, and new files can preserve graph intent. The derived topology should remain the source for runtime consumers until a later implementation plan changes that.

Option 3: save authoring graph only plus derive topology on load. This is higher risk because all existing runtime consumers and `ValidateForSerialization()` expect complete valid topology. It also makes load failure modes harder: a graph may parse but fail derivation, leaving no validated preview/collision/lightmap topology.

Option 4: bump root `formatVersion` and replace topology semantics. This is the most disruptive option and would require an explicit old-version loader branch plus save/load tests for both versions.

## Backward Compatibility Risks

Unknown graph data would be discarded by the current save path. If additive graph data becomes authoritative, graph-unaware saves become destructive.

`formatVersion` is strict. A root version bump without a compatibility loader will break current levels or new levels in older builds.

Derived topology ID churn can invalidate selections, produce large file diffs due to sorted ID output, and stale baked lightmaps because the source hash includes IDs.

Lightmap metadata can look syntactically valid while becoming semantically stale after graph derivation. `IsSectorLightmapCurrent()` compares `bakedLightmap.sourceHash` to `ComputeSectorLightmapSourceHash(map)`, so migration must either preserve hash-sensitive derived topology exactly or intentionally clear/stale baked metadata when derived topology changes.

Persisted sidedef and sector properties need explicit anchors. Without graph-side and face anchors, wall materials, middle textures, decals, `ceilingSky`, heights, ambient settings, and default wall parts can be lost or assigned to the wrong derived object.

Strict validation means old topology files with malformed but previously tolerated data will not load. The current tests intentionally reject invalid topology and malformed fields, so migration should not depend on accepting invalid current topology as source graph data unless it adds a separate legacy-repair path.

## Test Coverage Needed

Add load tests for topology v2 files without `authoringGraph` once graph persistence exists. They should verify graph synthesis preserves vertex, linedef, sidedef, sector, texture, static light, preview, sky, directional light, lightmap, decal, middle, and `ceilingSky` data.

Add round-trip tests for files containing both topology v2 and `authoringGraph`, including unknown/extra graph fields if forward compatibility is desired.

Add tests that old topology-only files continue to save as valid topology v2 when no graph is present.

Add tests for graph-aware save behavior that prevents accidental graph loss, either by preserving the graph section or by rejecting save when graph data exists but cannot be written.

Add lightmap migration tests. They should distinguish no-op conversion that preserves `bakedLightmap.sourceHash` from re-derivation that intentionally makes the bake stale or clears metadata.

Add sample-level load coverage for representative files under `assets/levels/` and `assets/sector_demo/`, especially files with baked lightmaps, decals, middle textures, sky settings, and directional light.

## Recommended Migration Strategy Candidates

Prefer a two-track file during migration: keep complete validated topology v2 as `SectorTopologyMap`, and add optional authoring graph data under its own root key and sub-version. Continue to let runtime consumers use derived topology.

When `authoringGraph` is absent, synthesize it from topology v2 using stable IDs one-to-one. Treat this as a compatibility import, not as a reason to change the saved topology immediately.

When both graph and derived topology are present, load both and validate the derived topology normally. If a consistency check is added later, it should report graph/derived mismatch without preventing old topology-only loads.

Keep `formatVersion: 2` while the file still contains complete topology v2. Add a graph-local version instead of bumping the root version for the first migration step.

Preserve baked lightmap metadata only when derived topology and hash-sensitive data are unchanged. If graph derivation changes IDs, geometry, bake settings, `ceilingSky`, directional light, static lights, or bake-relevant materials, intentionally stale or clear the bake rather than pretending it is current.

## Recommended Follow-Up Questions

Should graph-aware saves preserve unknown future graph fields, or is it acceptable to normalize them away?

Should the editor reject saving with graph data when the derived topology is stale or invalid, or should it save the graph plus the last valid derived topology?

Should root `formatVersion` remain `2` until topology v2 is no longer persisted, or should a new root version be introduced as soon as `authoringGraph` appears?

What exact ID-preservation guarantees are required for lightmap reuse after importing topology v2 into an authoring graph?

Should generated authoring graph data be written back immediately after loading an old file, or only after the user performs a graph-editing action?
