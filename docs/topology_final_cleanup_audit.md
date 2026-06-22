# Topology Final Cleanup Audit

## 1. Executive summary

The topology editor is now the active editor path for document load/save, 2D rendering, drawing, selection, inspectors, 3D preview and picking, static lights, lightmap baking, and 3D UV/texture edits. The normal editor document is `SectorTopologyMap`; `SectorEditor::Render()` calls `DrawTopologyDocument()`, canvas picking uses topology light/linedef/sidedef/sector hit tests, preview rebuild uses `state.topologyMap`, and lightmap baking uses `SectorTopologyLightmapBakeInput`.

The old polygon model is still present and still referenced in `SectorEditor.cpp`, but those references are mostly compatibility/fallback branches, pending draw float-point helpers, and dead old polygon UI/selection code. The old polygon model is still actively used by `SectorDemo`, old geometry/mesh/preview/lightmap overloads, and old JSON load/save helpers.

Final cleanup appears ready as a staged cleanup, not as a single delete. The main risk is that shared types still live in old polygon headers, especially texture definitions, lightmap settings/metadata, generated surface ring fields, and editor pending-draw point helpers. Removing `SectorTypes.h` or `SectorMap.cpp` first would break topology code and tests.

Main risks:

- `SectorEditor.cpp` still contains old polygon selection, inspector, hit-test, render, validation, edge override, and split-edge paths.
- `SectorTypes.h` mixes old polygon model types with shared texture, lightmap, mesh, and helper types used by topology.
- Old overloads remain compiled into tests through CMake, even though tests exercise topology overloads.
- `SectorDemo` is still a real legacy caller of `LoadSectorMap()` and `SectorMeshPreview::Rebuild(const SectorMap&)`.

## 2. Remaining old polygon files and symbols

File: `sources/sector_demo/SectorTypes.h`

Symbol/type/function:
`SectorPoint`, `SectorBoundaryRingKind`, `SectorBoundaryEdgeRef`, `SectorEdgePartUvOverride`, `SectorSurfaceUvOverride`, `SectorEdgeOverride`, `SectorDefinition`, `SectorStaticPointLight`, `SectorMap`.

Current role:
Primary old polygon data model. Also holds shared data used by topology (`SectorTextureDefinition`, `SectorTextureFilter`, `SectorLightmapBakeSettings`, `SectorLightmapMetadata`, `SectorMeshBuildResult`, `SectorTextureBinding`, `EffectiveEdgeSettings`, `EdgeNeighborInfo`).

Likely cleanup action:
Split shared/common types into a neutral header before deleting old polygon model structs. Keep or rename a generic point/ring helper only if topology/editor still needs it.

Risk:
High. Topology includes this header through `SectorTopologyMap.h`, `SectorGeneratedGeometry.h`, `SectorMeshBuilder.h`, `SectorLightmap.h`, and editor headers.

File: `sources/sector_demo/SectorMap.h`

Symbol/type/function:
`FindSectorTexture`, `SortedSectorTextureIds`, `SectorTextureLoadFlags`, `LoadSectorMap`, `SaveSectorMap`, `GetSectorBoundaryRing`, `GetSectorBoundaryEdge`, `GetEffectiveEdgeSettings`, `FindReverseEdgeNeighbor`, `SplitSectorEdge`.

Current role:
Old polygon persistence and polygon boundary/edge helper API. Some texture helper concepts are still useful, but the signatures are tied to `SectorMap`.

Likely cleanup action:
Move texture helpers to a shared texture-definition module if topology still needs them; remove polygon boundary helpers after editor old selection/edge paths are gone.

Risk:
Medium-high. `SectorEditor.cpp`, `SectorDemo.cpp`, old mesh/preview/lightmap code, and old generated geometry include it.

File: `sources/sector_demo/SectorMap.cpp`

Symbol/type/function:
Old polygon JSON parser/writer, winding normalization, holes/edge override parsing, static light parsing, lightmap metadata parsing, edge/ring helpers, split-edge mutation.

Current role:
Implementation for old polygon maps. Still compiled into several topology test targets because shared type/function dependencies have not been separated.

Likely cleanup action:
Delete after `SectorDemo` is ported/removed, old editor polygon paths are removed, old overloads are removed, and shared helpers have moved.

Risk:
Medium. CMake currently explicitly lists this file in multiple topology test executables.

File: `sources/sector_demo/SectorGeneratedGeometry.h/.cpp`

Symbol/type/function:
`BuildSectorGeneratedGeometry(const SectorMap&, SectorGeneratedGeometry&)` and old polygon internal helpers such as `BuildEarcutPolygon(const SectorDefinition&)`, `FlattenSectorPoints()`, `WarnAboutPartialEdges(const SectorMap&)`.

Current role:
Contains both old polygon and topology generated-geometry paths. The topology path is active in the editor and tests. The old polygon overload remains for legacy callers and old mesh/lightmap overloads.

Likely cleanup action:
Remove only the `SectorMap` overload and polygon-only helpers after old mesh/lightmap/preview overloads are gone. Keep `SectorGeneratedGeometry`, topology overload, picking, surface labels, and shared generated-surface structs.

Risk:
Medium. `SectorGeneratedSurfaceRef` still contains old polygon index/ring fields as well as topology IDs.

File: `sources/sector_demo/SectorMeshBuilder.h/.cpp`

Symbol/type/function:
`BuildSectorMeshes(const SectorMap&, const SectorLightmapLayout*)`.

Current role:
Old polygon mesh overload remains beside topology mesh overload. Current editor preview uses topology overload.

Likely cleanup action:
Remove polygon overload after `SectorMeshPreview::Rebuild(const SectorMap&)` and `SectorDemo` are removed/ported.

Risk:
Low-medium. Shared mesh result types are declared in `SectorTypes.h`.

File: `sources/sector_demo/SectorMeshPreview.h/.cpp`

Symbol/type/function:
`SectorMeshPreview::Rebuild(engine::AssetManager&, const SectorMap&, const char*, std::string&)`.

Current role:
Legacy preview rebuild path. The active editor calls the topology overload in `RebuildPreviewMeshesPreservingView()` and 3D entry paths. `SectorDemo` still calls the `SectorMap` overload.

Likely cleanup action:
Remove polygon overload after `SectorDemo` no longer needs it.

Risk:
Low-medium. Keep preview class and topology overload.

File: `sources/sector_demo/SectorLightmap.h/.cpp`

Symbol/type/function:
`SectorLightmapBakeInput`, `BuildSectorLightmapLayout(const SectorMap&)`, `BakeSectorLightmap(const SectorMap&)`, `BakeSectorLightmap(const SectorLightmapBakeInput&)`, `ComputeSectorLightmapSourceHash(const SectorMap&)`, `GetSectorLightmapStatus(const SectorMap&)`.

Current role:
Old polygon lightmap overloads remain beside topology overloads. Active editor bake/status/hash use topology overloads.

Likely cleanup action:
Remove polygon input struct and overloads after old preview/mesh paths and legacy callers are gone.

Risk:
Medium. Shared bake result/status/layout/report helpers remain required.

File: `sources/sector_demo/SectorDemo.h/.cpp`

Symbol/type/function:
`SectorDemo::Init()` creates `SectorMap`, calls `LoadSectorMap()`, then calls polygon preview rebuild.

Current role:
Active legacy caller if `SectorDemo` is still reachable from the app.

Likely cleanup action:
Port to topology map loading/preview or remove the demo before deleting old polygon load/save and preview overloads.

Risk:
Medium. This is the clearest non-editor live old-model caller.

## 3. Active editor references to old polygon state

Active and must be migrated/removed carefully:

- `SectorEditorState::map` in `sources/sector_editor/SectorEditorTypes.h` still stores a `SectorMap`. It is reset to `MakeBlankSectorMap()` on new/load, but it is not the topology document.
- Old selection state in `SectorEditorState`: `selectedSectorIndex`, `selectedEdgeRingKind`, `selectedEdgeHoleIndex`, `selectedEdgeIndex`, `selectedEdgeUvPart`, `hoveredSectorIndex`, `hoveredEdgeSectorIndex`, `hoveredEdgeRingKind`, `hoveredEdgeHoleIndex`, `hoveredEdgeIndex`, `hoveredVertexPoint`, and `hoveredVertexRefs`.
- Old polygon inspector fallback in `SectorEditor::DrawSectorsPanel()`. It is skipped when topology sector/sidedef/linedef/light selection exists, but still contains editable polygon sector/edge branches and old UV/edge override mutation.
- Old deletion/rename fallback: `TryRenameSelectedSector()`, `DeleteSelectedSector()`, `DeleteSectorAt()`.
- Old edge split fallback: `SplitSelectedEdge()` calls `SplitSectorEdge(state.map, ...)`. Active topology splitting uses `SplitSelectedTopologyLineDef()` instead.
- Old hit testing and selection functions: `FindSectorAt()`, `FindEdgeNearScreenPoint()`, `FindEdgeHitCandidates()`, `ResolveEdgeHit()`, `SelectSector()`, `SelectEdge()`.
- Old rendering functions: `DrawSectors()` and `DrawSector()` still render polygon sectors, holes, edge highlights, and old polygon labels. The active 2D render path calls `DrawTopologyDocument()`, not `DrawSectors()`.
- Old validation helpers: `ValidateExistingSectorMap()`, `ValidatePendingPoint()`, `ValidateSectorPolygon()`, and `ValidateInsertSectorPolygon()` are polygon-era. Active pending draw finalization routes through topology creation/insert helpers, but some old helpers still exist.
- Edge override helpers in the editor: `FindMutableEdgeOverride()`, `FindEdgeOverride()`, `EnsureEdgeOverride()`, `RemoveEdgeOverrideIfEmpty()` mutate/read `state.map.sectors[*].edgeOverrides`.

Compatibility-only and likely removable:

- `state.map = MakeBlankSectorMap()` during new/load is compatibility initialization.
- `GenerateUniqueSectorId()` still checks `state.map.sectors`, but topology sectors use integer IDs and names. This should be replaced or removed with the old polygon path.
- `TexturePickerTargetKind` old sector/edge targets are mostly legacy; active topology texture picking uses `TopologyTexturePickerTargetKind`, `TopologySectorTextureField`, `TopologyWallPart`, and explicit topology IDs.
- The old polygon texture picker rows in the fallback inspector now report: "Old polygon texture editing is not active in topology documents."
- `SectorSurfaceRef` still carries both old polygon indices and topology IDs. Active 3D picking fills topology IDs; old index fields are compatibility residue.

False positive / harmless shared helper:

- `SectorPoint` is still used as a float authoring/pending-draw point type and conversion bridge to `SectorTopologyCoordPoint`.
- Polygon words in local helper names such as `StrictPointInPolygon()` are geometric helper names, not necessarily old document-model use.
- `holes` in topology loop sets (`SectorTopologyLoopSet::holes`) are topology-derived loops, not old `SectorDefinition::holes`.
- `staticLights`, `lightmapSettings`, and `bakedLightmap` on `SectorTopologyMap` are active topology fields, not old polygon state.

## 4. Old polygon generated geometry / mesh / preview overloads

`BuildSectorGeneratedGeometry(const SectorMap&)`

- Still called by editor? Only indirectly in old polygon validation/triangulation helper code, not in the active topology render/preview path.
- Still called by tests? No direct old-map tests found. Topology tests call the topology overload, but test targets compile `SectorMap.cpp` and the mixed geometry implementation.
- Can remove now? Not safely until old polygon validation helpers, old mesh/lightmap overloads, and `SectorDemo` are addressed.
- Topology replacement: `BuildSectorGeneratedGeometry(const SectorTopologyMap&, SectorGeneratedGeometry&, std::string*)`.

`BuildSectorMeshes(const SectorMap&)`

- Still called by editor? No active editor path found; editor 3D rebuild uses topology preview/mesh path.
- Still called by tests? No direct old-map test call found.
- Can remove now? After removing polygon preview rebuild and `SectorDemo`.
- Topology replacement: `BuildSectorMeshes(const SectorTopologyMap&, const SectorLightmapLayout*, std::string*)`.

`SectorMeshPreview::Rebuild(const SectorMap&)`

- Still called by editor? No; `RebuildPreviewMeshesPreservingView()` calls topology overload.
- Still called by tests? No.
- Can remove now? Not until `SectorDemo` is ported or removed.
- Topology replacement: `SectorMeshPreview::Rebuild(engine::AssetManager&, const SectorTopologyMap&, const char*, std::string&)`.

`SectorGeneratedSurfaceRef` old fields

- Still called by editor? Active 3D selection uses topology IDs, but old index/ring fields remain in the struct and some old split-selection code.
- Still called by tests? Topology tests use generated surfaces and refs.
- Can remove now? Only after all old polygon paths and compatibility fields are removed, and after topology-only surface identity is confirmed.
- Topology replacement: `topologySectorId`, `topologyLineDefId`, `topologySideDefId`, `topologySide`.

## 5. Old polygon lightmap APIs

`BuildSectorLightmapLayout(const SectorMap&)`

- Still called by editor? No active editor bake/status path uses topology.
- Still called by tests? No direct old-map tests found; topology tests call the topology overload.
- Can remove now? After old preview/mesh/demo callers are removed and `SectorLightmapBakeInput` is removed.
- Topology replacement: `BuildSectorLightmapLayout(const SectorTopologyMap&, ...)`.
- Risk: Shared `SectorLightmapLayout` and chart structs must remain.

`BakeSectorLightmap(const SectorMap&)`

- Still called by editor? No. `StartLightmapBake()` creates `SectorTopologyLightmapBakeInput`.
- Still called by tests? No old-map calls found.
- Can remove now? After removing `SectorLightmapBakeInput` and polygon layout overload.
- Topology replacement: `BakeSectorLightmap(const SectorTopologyMap&, ...)` and `BakeSectorLightmap(const SectorTopologyLightmapBakeInput&, ...)`.
- Risk: The implementation shares bake internals, report formatting, BVH/raycast code, and status enums with topology.

`ComputeSectorLightmapSourceHash(const SectorMap&)`

- Still called by editor? No; editor uses `ComputeSectorLightmapSourceHash(state.topologyMap)`.
- Still called by tests? Topology tests call the topology overload.
- Can remove now? After old map status/bake overloads are removed.
- Topology replacement: `ComputeSectorLightmapSourceHash(const SectorTopologyMap&)`.
- Risk: Keep shared FNV/hash helper code and bake version constants.

`GetSectorLightmapStatus(const SectorMap&)`

- Still called by editor? No; status panel uses `GetSectorLightmapStatus(state.topologyMap)`.
- Still called by tests? No old-map calls found.
- Can remove now? After polygon preview overload is gone.
- Topology replacement: `GetSectorLightmapStatus(const SectorTopologyMap&)`.
- Risk: `SectorMeshPreview::Rebuild(const SectorMap&)` currently calls it.

`SectorLightmapBakeInput`

- Still called by editor? No; replaced by `SectorTopologyLightmapBakeInput`.
- Still called by tests? No old-map calls found.
- Can remove now? After polygon `BakeSectorLightmap(input, ...)` overload is removed.
- Topology replacement: `SectorTopologyLightmapBakeInput`.
- Risk: Low, but remove with the overload to avoid dangling API.

## 6. Old polygon JSON load/save

`LoadSectorMap`

- Still called by editor document workflow? No; `LoadLevel()` calls `LoadSectorTopologyMap()`.
- Still called by tests? No direct test call found.
- Still useful for legacy conversion? No explicit conversion plan found in current code; old polygon JSON is rejected by topology loader per migration docs.
- Can remove now? Not until `SectorDemo` is ported/removed and old load/save compatibility is intentionally dropped.

`SaveSectorMap`

- Still called by editor document workflow? No; topology documents save through topology serialization.
- Still called by tests? No direct test call found.
- Still useful for legacy conversion? No, unless a separate conversion tool is explicitly planned.
- Can remove now? After no callers remain and old polygon persistence compatibility is intentionally ended.

## 7. Shared types that should not be deleted accidentally

`SectorTextureDefinition`

- Where declared now: `sources/sector_demo/SectorTypes.h`.
- Who uses it: `SectorMap`, `SectorTopologyMap`, topology serialization, editor texture assets/picker, generated geometry/mesh paths.
- Should it be moved before deleting old polygon files? Yes.
- Recommended destination: new shared sector texture/types header, for example `SectorTextureTypes.h` or `SectorCommonTypes.h`.

`SectorTextureFilter`

- Where declared now: `sources/sector_demo/SectorTypes.h`.
- Who uses it: texture definitions, add-texture UI, texture load flags, topology and old map serialization.
- Should it be moved before deleting old polygon files? Yes.
- Recommended destination: same shared texture/common header as `SectorTextureDefinition`.

`SectorTextureLoadFlags()` and `SectorTextureFilterName()`

- Where declared now: `sources/sector_demo/SectorMap.h`; implemented in `SectorMap.cpp`.
- Who uses it: editor asset loading and texture UI code may need equivalent helpers.
- Should it be moved before deleting old polygon files? Yes, if still used after old map removal.
- Recommended destination: shared sector texture helper implementation independent of `SectorMap`.

`SectorLightmapBakeSettings`

- Where declared now: `sources/sector_demo/SectorTypes.h`.
- Who uses it: `SectorMap`, `SectorTopologyMap`, topology serialization, lightmap bake/hash/status code, editor lightmap controls.
- Should it be moved before deleting old polygon files? Yes.
- Recommended destination: `SectorLightmapTypes.h` or a shared lightmap/common header.

`SectorLightmapMetadata`

- Where declared now: `sources/sector_demo/SectorTypes.h`.
- Who uses it: `SectorMap`, `SectorTopologyMap`, topology serialization, lightmap status, preview lightmap loading.
- Should it be moved before deleting old polygon files? Yes.
- Recommended destination: same as `SectorLightmapBakeSettings`.

`SectorMeshBuildResult` / `SectorMeshBatch`

- Where declared now: `sources/sector_demo/SectorTypes.h`.
- Who uses it: mesh builder and mesh preview.
- Should it be moved before deleting old polygon files? Yes if the mesh builder keeps this result shape.
- Recommended destination: `SectorMeshTypes.h` or `SectorMeshBuilder.h`.

`SectorBoundaryRingKind`

- Where declared now: `sources/sector_demo/SectorTypes.h`.
- Who uses it: old polygon edge refs, editor old edge selection, generated-surface refs.
- Should it be moved before deleting old polygon files? Prefer removing from topology-facing APIs instead of moving, once old polygon refs are gone.
- Recommended destination: none long-term; replace with topology IDs/side kind where possible.

`SectorPoint`

- Where declared now: `sources/sector_demo/SectorTypes.h`.
- Who uses it: old polygon model and editor pending draw/conversion helpers.
- Should it be moved before deleting old polygon files? Either move as a generic authoring point type or replace editor pending draw storage with `Vector2`/`SectorTopologyCoordPoint`.
- Recommended destination: if kept, a small neutral authoring geometry header.

`SectorSurfaceUvOverride`, `SectorEdgePartUvOverride`, `EffectiveEdgeSettings`, `EdgeNeighborInfo`

- Where declared now: `sources/sector_demo/SectorTypes.h`.
- Who uses it: old polygon edge/surface override paths and old editor fallback.
- Should it be moved before deleting old polygon files? No unless a surviving topology API needs them.
- Recommended destination: delete with old polygon edge override model.

## 8. Tests that still depend on old polygon code

No test file appears to construct `SectorMap` or call old polygon load/save directly. The current test files are topology-focused:

- `tests/SectorTopologyValidationTests.cpp`
- `tests/SectorTopologySerializationTests.cpp`
- `tests/SectorTopologyGeneratedGeometryTests.cpp`
- `tests/SectorTopologyMeshBuilderTests.cpp`
- `tests/SectorTopologyLightmapTests.cpp`
- `tests/SectorTopologyCreationTests.cpp`

CMake still compiles `sources/sector_demo/SectorMap.cpp` into these topology test targets:

- `sector_topology_generated_geometry_tests`
- `sector_topology_mesh_builder_tests`
- `sector_topology_lightmap_tests`
- `sector_topology_creation_tests`

Classification:

- Remove test: none identified solely for old polygon behavior.
- Port test to topology: already done for visible topology coverage.
- Keep test until cleanup complete: keep all topology tests; update CMake source lists after shared helpers are moved and old polygon dependencies are removed.

## 9. Documentation references

`docs/sector_editor.md`

- Classification: Update.
- Reason: Still describes the editor as a `SectorMap` JSON editor, polygon holes, old edge overrides, old static light IDs, old polygon move/split behavior, and old 3D sector/edge UV workflow as current behavior.

`docs/sector_topology_incremental_refactor.md`

- Classification: Archive as historical or leave as migration log.
- Reason: Early sections intentionally describe old polygon paths as active at those phases. Later sections document topology migration status and cleanup notes. It should not be used as current user-facing behavior without context.

`docs/doom_like_topology_prototype_salvage_audit.md`

- Classification: Archive as historical.
- Reason: It is a prototype/salvage audit, not current behavior. It includes references to prototype `SectorMap` topology experiments and old migration advice.

## 10. Recommended final cleanup order

Step 1: Move shared types out of old polygon files.

- Move texture definitions/filter helpers and lightmap settings/metadata first.
- Move or replace `SectorPoint` in editor pending draw code.
- Move mesh result types if they remain public.

Step 2: Remove old editor state fields and compatibility map.

- Delete `SectorEditorState::map`, polygon sector/edge selection fields, old hovered polygon fields, and old texture picker targets after all active topology UI paths are confirmed.
- Remove old polygon inspector/render/hit-test/delete/rename/split branches from `SectorEditor.cpp`.
- Remove `DrawSectors()`, `DrawSector()`, `FindSectorAt()`, `FindEdgeHitCandidates()`, `ResolveEdgeHit()`, `SelectSector()`, `SelectEdge()`, `SplitSelectedEdge()`, and old edge override helpers.

Step 3: Remove old polygon save/load.

- Port or remove `SectorDemo` first.
- Delete `LoadSectorMap()`, `SaveSectorMap()`, and old JSON helpers once no callers remain.

Step 4: Remove old polygon generated geometry overload.

- Delete `BuildSectorGeneratedGeometry(const SectorMap&)` and polygon-only geometry helpers.
- Keep topology generated geometry, picking, labels, and shared surface structs.
- Simplify `SectorGeneratedSurfaceRef` after old polygon index/ring fields are no longer needed.

Step 5: Remove old polygon mesh/preview overload.

- Delete `BuildSectorMeshes(const SectorMap&)`.
- Delete `SectorMeshPreview::Rebuild(const SectorMap&)`.
- Keep topology preview and mesh paths.

Step 6: Remove old polygon lightmap overload.

- Delete `SectorLightmapBakeInput`.
- Delete polygon layout, bake, hash, and status overloads.
- Keep shared bake internals, layout/result/report/status types, and topology overloads.

Step 7: Update tests, CMake, and docs.

- Remove `SectorMap.cpp` from topology test targets once shared dependencies are gone.
- Update `docs/sector_editor.md` to topology-current behavior.
- Keep historical migration docs clearly historical, or archive them.

Step 8: Build, CTest, and manual smoke tests.

- Build the app and run all CTest targets.
- Manually smoke test: new/load/save/reload, draw sector, insert inside, select sector/sidedef/light, move vertex/light, split linedef, delete sector, 3D preview/picking, 3D UV/texture edits, lightmap bake/status, and texture asset loading.

## 11. Risks and rollback notes

- `SectorEditor.cpp` is large and still contains many old fallback branches. Remove old editor state and behavior in small commits so failures are easy to isolate.
- Shared declarations live in old polygon headers. Do not delete `SectorTypes.h` or `SectorMap.cpp` before shared type relocation is complete.
- CMake uses a global source glob for the main executable and explicit test source lists for topology tests. Removing files will affect both paths differently.
- Old overloads may not be directly called by tests, but mixed implementation files still compile both old and topology code. Remove overloads only after headers no longer expose them.
- `SectorDemo` is a concrete legacy caller; decide whether to port it to topology or remove it before deleting old JSON/preview APIs.
- Rollback strategy: perform cleanup in staged commits matching the order above. If behavior breaks, revert the smallest stage rather than mixing shared-type moves with old-code deletion.

## Verification notes

This audit is report-only. Build and CTest are optional because no source code should change.

Observed before writing the report: `git status --short` already showed an unrelated untracked file:

```text
?? assets/levels/test2/test2.lightmap.png
```

The intended audit change is this file only:

```text
docs/topology_final_cleanup_audit.md
```

## Phase 18A note: shared sector type extraction

Shared, topology-safe definitions were moved out of the old polygon model header into neutral files:

- `SectorPoint` moved to `sources/sector_demo/SectorPointTypes.h`.
- `SectorTextureFilter`, `SectorTextureDefinition`, `SectorTextureBinding`, `SectorTextureLoadFlags()`, and `SectorTextureFilterName()` moved to `sources/sector_demo/SectorTextureTypes.h/.cpp`.
- `SectorLightmapBakeSettings` and `SectorLightmapMetadata` moved to `sources/sector_demo/SectorLightmapTypes.h`.
- `SectorMeshBatch` and `SectorMeshBuildResult` moved to `sources/sector_demo/SectorMeshTypes.h`.

`SectorTextureLoadFlags()` and `SectorTextureFilterName()` no longer live in `SectorMap.cpp`; `SectorMap.cpp` still owns old polygon map helpers such as `FindSectorTexture()`, `SortedSectorTextureIds()`, boundary/ring helpers, load/save, and split-edge logic.

`SectorTopologyMap.h` now includes the shared texture/lightmap headers directly instead of depending on `SectorTypes.h`. Mesh preview/builder headers now include the shared mesh types directly and forward-declare `SectorMap` where possible.

The old polygon files and APIs still remain intentionally: `SectorTypes.h`, `SectorMap.h/.cpp`, `SectorMap`, `SectorDefinition`, old save/load, old generated geometry/mesh/preview/lightmap overloads, `SectorDemo`, and editor fallback state. Remaining `SectorTypes.h` / `SectorMap.h` includes are expected in files that still expose or compile old polygon overloads, legacy editor fallback fields, or `SectorDemo`.

Topology test targets still compile `SectorMap.cpp` because mixed implementation files still compile old polygon overloads that reference polygon helpers. The new `SectorTextureTypes.cpp` is explicitly linked into those targets so shared texture helpers are no longer supplied by `SectorMap.cpp`.

## Phase 18B note: topology-only editor state

Old polygon editor state was removed from the sector editor. `SectorEditorState::map` is gone, along with the old polygon sector/edge selection fields and old polygon hover fields.

`SectorEditor.cpp` no longer contains the old polygon inspector, renderer, hit-test, split, delete, rename, or edge override editor branches. The editor document behavior now uses `state.topologyMap` only, including topology sector/sidedef/linedef/light selection, topology texture picker routing, 3D surface editing, and lightmap bake/status UI.

The old polygon subsystem files and APIs still remain intentionally for later cleanup: `SectorMap`, `SectorDefinition`, old save/load, old generated geometry/mesh/preview/lightmap overloads, and `SectorDemo`.

## Phase 18C note: SectorDemo topology port

`SectorDemo` was inspected for reachability. It is compiled into the main executable because `CMakeLists.txt` glob-compiles `sources/*.cpp`, but it is not reachable from the running app: `sources/Main.cpp` constructs `SectorEditor` only, and no `SectorDemo` construction or `Init()` call exists in `sources`, `tests`, or `CMakeLists.txt`.

`SectorDemo` was ported in place to load `SectorTopologyMap` with `LoadSectorTopologyMap()` and rebuild the existing topology preview overload. It still receives its map path through `SectorDemo::Init()`; no topology map path is hardcoded. For future manual wiring, the existing topology v2 smoke map `assets/levels/topology_smoke/topology_smoke.json` is a suitable sample.

`SectorDemo` no longer calls `LoadSectorMap()` or the polygon `SectorMeshPreview::Rebuild(const SectorMap&)` overload. The old polygon save/load APIs and old generated geometry, mesh, preview, and lightmap overloads still remain intentionally for later cleanup.

## Phase 18D note: old polygon persistence removal

The old polygon `LoadSectorMap()` and `SaveSectorMap()` public APIs were removed from `SectorMap.h`, and their old polygon JSON parser/writer implementations were removed from `SectorMap.cpp`.

The old polygon JSON read/write helpers and file IO dependencies were removed with those functions. Topology v2 serialization remains the only active sector editor persistence path; no old polygon JSON fallback, compatibility loader, or old-polygon-to-topology conversion was added.

`SectorMap`, `SectorDefinition`, and the old polygon generated geometry, mesh, preview, and lightmap overloads still remain intentionally for later cleanup. `SectorMap.cpp` also still remains because non-persistence old polygon helpers such as texture lookup, boundary/ring access, effective edge settings, neighbor lookup, and edge splitting are still compiled by old overloads.

## Phase 18F note: old polygon runtime overload chain removal

The old polygon generated geometry overload was removed. `BuildSectorGeneratedGeometry()` now exposes only the topology map entry point, and old polygon-only geometry helpers for `SectorMap`, `SectorDefinition`, old boundary refs, partial-edge warnings, old edge override UVs, and old polygon wall generation were removed.

The old polygon mesh overload and preview overload were removed. `BuildSectorMeshes()` and `SectorMeshPreview::Rebuild()` now expose only topology map entry points; preview lightmap status/layout use the topology map path only.

The old polygon lightmap overloads were removed, including `SectorLightmapBakeInput`, polygon lightmap layout/bake entry points, polygon source hashing, and polygon lightmap status. Shared topology bake internals, reports, status text, atlas layout, raycast/BVH code, progress/cancellation, PNG export/install helpers, and topology hashing were kept.

Old `SectorMap.cpp` helper APIs were no longer referenced after the overload removal, so `SectorMap.h` and `SectorMap.cpp` were deleted. Topology test target source lists no longer include `SectorMap.cpp`.

`SectorGeneratedSurfaceRef` no longer carries old polygon identity fields (`sectorIndex`, `ringKind`, `holeIndex`, `edgeIndex`). It keeps topology identity fields for sector, linedef, sidedef, and side.

`SectorTypes.h` still contains deferred old polygon model declarations such as `SectorMap`, `SectorDefinition`, edge override structs, `SectorBoundaryRingKind`, and `SectorBoundaryEdgeRef`. Current searches show those names are not referenced by active editor/demo/test/topology code outside that declaration header, so final model declaration deletion is deferred to a later phase.

Topology editor, preview, serialization, JSON format, and lightmap behavior were not intentionally changed in this phase beyond removing dead old polygon paths.
