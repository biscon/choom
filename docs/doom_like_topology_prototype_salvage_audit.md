# Doom-like Topology Prototype Salvage Audit

Audit target: `experimental/doom-like-data-model` (`f8c424e`)  
Baseline: `topology-incremental`

## 1. Executive summary

Use the experimental branch as reference only. Do not continue from it directly.

The branch contains a coherent first pass at a linedef/sidedef topology backend, but it replaced the real editor rather than migrating it. The branch reduces `SectorEditor.cpp` from roughly 6,700 lines to about 300 and reduces `RenderUI()` to a status bar. The tools panel, inspectors, document modals, texture workflows, lightmap workflow, 3D preview/editing, and much of the existing input behavior are gone. Its backend also has correctness and robustness problems that make direct copying risky.

The most useful material is the data relationship design, fixed-point coordinate scheme, topology-aware surface identity, JSON v2 shape, and the copy/validate/commit pattern used by editing operations. These should be reimplemented incrementally inside the existing editor architecture.

## 2. Potentially reusable backend ideas

### Topology data model and fixed-point coordinates

File/function/type: `sources/sector_demo/SectorTypes.h` — `SectorVertex`, `SectorLineDef`, `SectorSideDef`, integer `SectorDefinition::id`, `SectorSideKind`, and `SectorMap` topology arrays; `sources/sector_demo/SectorUnits.h` — `SectorCoord`, `SectorCoordSubdivisions`, and conversion helpers.  
Why it is useful: It separates shared vertices, undirected physical lines, directed sector sides, and sector properties. Stable integer IDs avoid using vector indices as persisted identity. `SectorCoord = int32_t` with 16 subdivisions gives exact equality and exact integer predicates while retaining sub-authoring-unit placement.  
Should it be copied directly, rewritten, or used only as design reference: Rewrite with the same broad relationships and explicit conversion boundary. Keep the current branch's public naming/style and add the model beside the old polygon model first.  
Risks/issues: Public structs are densely formatted and the prototype gives no overflow policy for coordinate arithmetic or ID exhaustion. A `SectorLineDef` stores sidedef IDs while each `SectorSideDef` also stores its line ID, creating bidirectional invariants that validation must enforce. Confirm whether `SectorCoordPoint` is the desired long-term name. Heights remain floating-point while planar coordinates are fixed-point.

### ID allocation and lookup helpers

File/function/type: `sources/sector_demo/SectorMap.cpp` — `FindSectorVertex`, `FindSectorLineDef`, `FindSectorSideDef`, `FindSector`, `FindOppositeSideDef`, `AllocateSectorVertexId`, `AllocateSectorLineDefId`, `AllocateSectorSideDefId`, and `AllocateSectorId`.  
Why it is useful: These helpers centralize ID-based access and make the topology call sites legible. `FindOppositeSideDef` is particularly useful for portal/wall generation.  
Should it be copied directly, rewritten, or used only as design reference: Rewrite. Preserve const/mutable overloads, but define invalid-ID and overflow behavior and consider lookup indexes later only if profiling justifies them.  
Risks/issues: Lookup is linear. The allocator scans for the maximum ID and returns `max + 1`; it does not handle `INT_MAX`, and repeated allocation can become expensive. Call sites frequently assume a successful lookup and dereference without checking.

### Validation and issue reporting

File/function/type: `sources/sector_demo/SectorTypes.h` — `SectorValidationIssue`, `SectorValidationSeverity`, and `SectorTopologyObject`; `sources/sector_demo/SectorMap.cpp` — `ValidateSectorMap`, `HasSectorValidationErrors`, and `FormatSectorValidationIssue`.  
Why it is useful: The prototype identifies the right validation categories: positive/unique IDs, valid endpoints, non-degenerate and non-duplicate linedefs, consistent line slots, valid sector references, closed loops, self-intersection, partial overlap, and boundary intersection. Structured issues are useful for load errors and editor diagnostics.  
Should it be copied directly, rewritten, or used only as design reference: Rewrite defensively, retaining the issue model and validation coverage as a checklist.  
Risks/issues: `ValidateSectorMap` records dangling vertex references in its first linedef pass, then its pairwise intersection pass unconditionally dereferences both lines' endpoints. Invalid input can therefore crash validation instead of returning issues. It is also quadratic for pairwise linedef checks and can emit repeated sector issues.

### Loop extraction

File/function/type: `sources/sector_demo/SectorMap.cpp` — `ExtractSectorLoops`, plus `Cross`, `On`, `Intersects`, and `PointInLoop`; `sources/sector_demo/SectorTypes.h` — `SectorLoop` and `SectorLoopSet`.  
Why it is useful: Directed sidedefs become oriented boundary edges, which makes extracting one CCW outer loop and clockwise holes straightforward. Integer cross products avoid epsilon-sensitive planar topology decisions. The returned sidedef sequence is useful for selection and generated walls as well as triangulation.  
Should it be copied directly, rewritten, or used only as design reference: Use as algorithm/design reference and rewrite with explicit preconditions, canonical ordering, and tests.  
Risks/issues: The walk's start and output order follow `m.sidedefs` order; holes are not canonically sorted, so it is repeatable only while container order is unchanged rather than independently deterministic. It assumes exactly one incoming and outgoing side per boundary vertex, rejecting configurations where distinct loops touch at a vertex. Hole validation checks only one hole vertex against the outer loop and does not test hole/hole intersections or all containment relationships here.

### Topology JSON v2

File/function/type: `sources/sector_demo/SectorMap.cpp` — `LoadSectorMap`, `SaveSectorMap`, JSON `Part`/`Uv` helpers, and the `formatVersion: 2`, `topology: "linedef"`, `coordSubdivisions: 16` schema.  
Why it is useful: It provides a concrete separate format for vertices, linedefs, sidedefs, sectors, textures, lights, player start, lightmap settings, and baked-lightmap metadata. It correctly requires vertex coordinates to be JSON integers and validates after load/before save.  
Should it be copied directly, rewritten, or used only as design reference: Reimplement as a separate v2 path, sharing carefully factored property parsing where appropriate. Do not replace the existing format until the existing document workflow has been migrated and verified.  
Risks/issues: The prototype loader rejects all legacy maps rather than coexisting with them. `side` treats every value other than `"front"` as back instead of rejecting invalid strings. Array shapes and color ranges receive little explicit validation. Texture serialization iterates an `unordered_map`, so output order is not deterministic. Atomic/temporary-file save behavior is absent.

### Generated geometry from topology

File/function/type: `sources/sector_demo/SectorGeneratedGeometry.h/.cpp` — `SectorGeneratedSurfaceRef`, `SectorGeneratedSurface`, `Flat`, `Wall`, `BuildSectorGeneratedGeometry`, `SectorGeneratedSurfaceKindName`, and `FormatSectorGeneratedSurfaceLabel`.  
Why it is useful: Floor/ceiling triangulation consumes extracted outer/hole loops; walls consume directed sidedefs; opposite sides determine solid, lower, and upper spans. Surface references use stable `sectorId`, `lineId`, `sideDefId`, side kind, and wall-part kind, which is a much better basis for picking, debugging, and lightmap identity than polygon edge indices.  
Should it be copied directly, rewritten, or used only as design reference: Rewrite more defensively while retaining the data flow and stable surface identity. An overload or parallel topology builder should be introduced before changing existing callers.  
Risks/issues: Several `Find...` results are dereferenced without checks. The wall length calculation truncates a floating coordinate-space length back to `SectorCoord` before world conversion: `SectorCoordToWorldDistance((SectorCoord)raw)`. Diagonal wall length and UV/chart dimensions can therefore be shortened. Floors and ceilings are emitted as one generated surface per triangle, producing repeated logical surface refs and more charts than necessary. The function can return success when only some sectors generated geometry, and it reports errors to stderr as a side effect.

### Lightmap surface identity and source hashing

File/function/type: `sources/sector_demo/SectorLightmap.cpp` — `IsSameLogicalBakeSurface`, `ShouldIgnoreBakeTriangle`, and `ComputeSectorLightmapSourceHash`.  
Why it is useful: Logical wall identity changes from ring/edge indices to stable `(kind, sectorId, lineId, sideDefId, sideKind)`, while floor and ceiling triangles remain one logical surface per sector. The hash includes coordinate subdivisions, vertices, linedefs, sidedefs, sector IDs, and lighting, so topology edits invalidate stale bakes.  
Should it be copied directly, rewritten, or used only as design reference: The identity rules are strong design reference and may be portable with small adaptation after generated surface refs exist. Re-review the complete hash contract when implementing.  
Risks/issues: Hash results depend on vector order, not only logical topology. Wall identity may contain redundant fields whose consistency relies on validation. Surface-chart ordering is still tied to geometry emission order.

### Debug label formatting

File/function/type: `sources/sector_demo/SectorGeneratedGeometry.cpp` — `FormatSectorGeneratedSurfaceLabel`; `sources/sector_demo/SectorMap.cpp` — `FormatSectorValidationIssue`.  
Why it is useful: Labels expose stable sector/linedef/sidedef IDs and front/back orientation, which will make picking and topology failures diagnosable during migration.  
Should it be copied directly, rewritten, or used only as design reference: Reimplement once the final enums and refs are established.  
Risks/issues: `FormatSectorValidationIssue` indexes a fixed string array using an unchecked enum cast. Labels should remain debug presentation, not serialized identity.

### Transactional create, insert, move, split, and delete operations

File/function/type: `sources/sector_demo/SectorMap.cpp` — `CreateSectorPolygon`, `InsertSectorPolygon`, `MoveSectorVertex`, `SplitSectorLineDef`, and `DeleteSector`.  
Why it is useful: Most mutations work on a copy, validate it, and commit only on success. Creation reuses exact-coordinate vertices and exact endpoint-pair linedefs. Insertion creates the child's outer sides and the parent's opposite sides. Moving one shared vertex naturally updates every incident line. Splitting preserves both directed sidedefs and their material settings. Deletion clears side slots and removes orphan lines and vertices. These are useful topology operation semantics.  
Should it be copied directly, rewritten, or used only as design reference: Use as behavioral reference; rewrite each operation independently with tests and editor integration added one operation at a time. Keep copy/validate/commit initially, then optimize only if maps make copying material.  
Risks/issues: `InsertSectorPolygon` checks input vertices but not every new edge against parent/hole boundaries; a concave-parent chord can leave the usable area even when its endpoints are inside. It also does not explicitly reject duplicate consecutive points before building topology. `SplitSectorLineDef` intentionally rejects half-grid midpoints and lacks explicit null checks for endpoint/sidedef lookups. `DeleteSector` does not validate the result and does not describe cleanup of non-topology objects associated with a sector. Full-map copying allocates and may be costly during interactive edits.

## 3. Code that should not be reused directly

File/function/type: `sources/sector_editor/SectorEditor.cpp` — the entire replacement editor, especially `Update`, `Render`, and `RenderUI`.  
Why it should not be reused: It is a keyboard-driven topology harness, not a migration of the real editor. `RenderUI()` draws only a bottom status strip. It removes the tools panel, sector/wall/light inspectors, document New/Load/Save/Reload flow, confirmation and texture-picker modals, texture import, lightmap controls/progress, 3D preview/editing and picking, viewport layout, normal pan behavior, and existing selection/drag workflows. Editing is reduced to `S`, `I`, `M`, `X`, Delete, Enter, and clicks. Screen mapping also hard-codes a 1920x1080 layout center/status bar.  
What the incremental refactor should do instead: Preserve the current editor shell and behavior, introduce topology document state beside it, and migrate tools and workflows individually.

File/function/type: `sources/sector_editor/SectorEditor.h` and `SectorEditorTypes.h`.  
Why it should not be reused: The header collapses the editor API/state to a tiny harness and removes the real editor's UI state, preview state, modal state, async bake state, document identity, texture state, and detailed selection state. The compressed declarations also conflict with the current code style.  
What the incremental refactor should do instead: Extend the current types narrowly as each topology-backed feature is ported. Do not replace the editor state wholesale.

File/function/type: `docs/sector_editor.md` from the experimental branch.  
Why it should not be reused: It replaces extensive documentation of the functioning editor with a short description of the topology harness and therefore documents removed behavior as if it were the product.  
What the incremental refactor should do instead: Keep current workflow documentation and update sections only when their corresponding production workflow has actually migrated.

File/function/type: `sources/sector_demo/SectorMap.cpp` and `SectorGeneratedGeometry.cpp` as complete-file replacements.  
Why it should not be reused: The prototype achieves the refactor by deleting mature legacy compatibility and compressing multiple concerns into terse implementations with unchecked assumptions. Direct replacement would couple model, persistence, mutation, validation, and geometry migration into one risky change.  
What the incremental refactor should do instead: Add parallel topology APIs and switch callers subsystem by subsystem after focused tests and manual editor verification.

## 4. Quality concerns in the prototype

- `SectorMap.cpp`, `SectorGeneratedGeometry.cpp`, `SectorEditor.cpp`, `SectorEditor.h`, and `SectorEditorTypes.h` are heavily compressed. Names such as `m`, `c`, `s`, `l`, `x`, `r`, `p`, `g`, `ls`, `is`, `es`, `E`, `Flat`, `Wall`, `Col`, and `Pos` obscure nontrivial topology and geometry logic.
- `ValidateSectorMap` can dereference null endpoint lookups in its pairwise linedef pass after already diagnosing a dangling endpoint. A validator must be safe on malformed data.
- `PointInLoop`, loop area calculation, `InsertSectorPolygon`, `SplitSectorLineDef`, editor hit testing/rendering, and generated geometry contain `Find...` dereferences whose safety depends on validation or caller ordering rather than local checks.
- `SectorGeneratedGeometry.cpp::Wall` computes `raw` as a floating Euclidean length in stored-coordinate space, casts it to `SectorCoord`, and only then converts to world distance. This is the concrete floating-to-integer truncation problem anticipated by the audit request.
- `SectorGeneratedGeometry.cpp::Wall` divides by `raw` when creating a normal. Validation should reject zero-length lines, but the function itself has no guard and is unsafe if called with bad data.
- `InsertSectorPolygon` validates that each submitted point is inside the parent, but endpoint containment is insufficient for a concave parent: an edge between two inside points may cross outside. It also does not comprehensively test the new polygon against existing holes/boundaries before mutation.
- `ExtractSectorLoops` describes an oriented loop model well, but loop ordering is driven by storage order rather than canonicalized by stable IDs or coordinates. Calling it deterministic requires this qualification.
- `LoadSectorMap` has strict format gating, which is useful for a separate v2 loader but destructive as a replacement for the existing load path. Some enum and array inputs are accepted without explicit shape/value validation.
- `SaveSectorMap` writes textures from an `unordered_map`, so otherwise equivalent maps may produce differently ordered JSON.
- `BuildSectorGeneratedGeometry` skips sectors whose loops fail, can still return true if another surface exists, and mixes diagnostic printing with generation. Callers need an unambiguous partial-result policy.
- `SectorEditor.cpp::Update` ignores the boolean return from both `SectorAuthoringToCoord` calls. Extreme mouse/view coordinates could leave snapped components unchanged/defaulted without a reported error.
- The experiment replaced editor migration with a test harness. The diff removes thousands of lines of usable editor behavior and provides no compatibility bridge.
- No focused automated tests for the new topology operations were found in the repository. The experimental editor has no GUI access to most former workflows, so it cannot provide manual verification of inspectors, modals, texture editing, document handling, lightmap UI, or 3D behavior.

## 5. Recommended incremental migration order

1. **Add the topology model beside the old model.** Prototype reference: strong for struct relationships, stable integer IDs, and `SectorCoord`; rewrite naming, invariants, and overflow policy.
2. **Add topology validation and loop extraction.** Prototype reference: strong checklist and algorithm sketch; rewrite null-safely, canonicalize loop order, and add malformed-map/concavity/hole tests.
3. **Add v2 topology JSON load/save separately.** Prototype reference: strong schema draft; keep legacy loading and current document workflow operational, validate all input shapes/enums, and make output ordering deterministic.
4. **Add a generated-geometry overload/path for topology maps.** Prototype reference: strong data flow for loops and sidedefs; rewrite wall math and error handling.
5. **Add topology mesh preview without replacing the editor.** Prototype reference: generated surface refs are useful; the replacement editor provides no reusable preview integration.
6. **Introduce topology document state into the existing editor shell.** Prototype reference: minimal `SectorEditorState::map` only; preserve current UI/modal/preview/bake state.
7. **Switch New/Load/Save/Reload to v2 topology.** Prototype reference: JSON functions only; do not reuse its removed document UI.
8. **Port 2D topology rendering.** Prototype reference: linedef/vertex drawing and two-sided color are a small visual sketch; adapt to the existing canvas, pan/zoom, panels, and resolution-independent layout.
9. **Port sector drawing to topology.** Prototype reference: `CreateSectorPolygon` is useful behaviorally, especially exact vertex/linedef reuse and copy/validate/commit.
10. **Port sector selection and the sector inspector.** Prototype reference: loop-based point containment and ID selection are useful; inspector code was removed and must stay based on the current editor.
11. **Port sidedef selection and the wall inspector.** Prototype reference: cross-product side choice and sidedef identity are useful; preserve current texture picker, UV controls, and edge-hit disambiguation.
12. **Port 3D preview and 3D picking.** Prototype reference: stable generated surface refs and labels are useful; no replacement-editor integration is reusable.
13. **Port Insert Sector Inside.** Prototype reference: child/opposite-parent sidedef construction and property inheritance are useful; rewrite containment/intersection checks.
14. **Port shared vertex movement.** Prototype reference: `MoveSectorVertex` demonstrates the desired topology semantics and transactional validation; integrate the existing drag/cancel UX.
15. **Port Split Linedef.** Prototype reference: preserving both sidedefs and returning the corresponding new side is useful; decide and expose the exact-grid midpoint policy in the UI.
16. **Port sector deletion.** Prototype reference: side-slot clearing and orphan linedef/vertex cleanup are useful; add post-validation and define cleanup for lights, selection, and editor state.
17. **Port lightmaps and debug identity.** Prototype reference: topology hash inputs, logical surface self-hit rules, and stable debug labels are strong references; define order-independent/canonical hashing where appropriate.
18. **Remove the old polygon model last.** Prototype reference: none—the experimental branch removed the bridge too early. Remove only after all current editor workflows and representative maps pass automated and manual verification.

## 6. Files worth looking at later

`sources/sector_demo/SectorTypes.h`  
Useful for topology struct relationships, stable integer IDs, validation issue types, and loop result types.

`sources/sector_demo/SectorUnits.h`  
Useful for the signed fixed-point coordinate scheme, subdivision constant, checked authoring conversion, and world conversion boundary.

`sources/sector_demo/SectorMap.h`  
Useful as a compact inventory of topology lookup, validation, persistence, and edit operations.

`sources/sector_demo/SectorMap.cpp`  
Useful for validation and loop-extraction ideas, the v2 JSON schema, transactional mutation, exact shared-topology reuse, splitting, and orphan cleanup. Review carefully rather than copying.

`sources/sector_demo/SectorGeneratedGeometry.h`  
Useful for stable topology-based generated surface identity.

`sources/sector_demo/SectorGeneratedGeometry.cpp`  
Useful for the topology-to-floor/ceiling/wall flow and labels, but should be rewritten defensively and with corrected wall length math.

`sources/sector_demo/SectorLightmap.cpp`  
Useful specifically for `IsSameLogicalBakeSurface`, topology additions to `ComputeSectorLightmapSourceHash`, and propagating stable refs through bake triangles/texels.

`sources/sector_demo/SectorLightmap.h`  
Only a minor reference: the bake version bump records that topology changes alter the lightmap compatibility contract.

`sources/sector_editor/SectorEditor.cpp`  
Worth consulting only as a tiny interaction harness for topology selection, creation, insertion, movement, splitting, and deletion—not as editor implementation.

## 7. Files likely damaged by the experiment

`sources/sector_editor/SectorEditor.cpp`  
Do not reuse directly. It replaces the full GUI editor and 3D/document/lightmap workflows with a roughly 300-line hotkey harness and status bar.

`sources/sector_editor/SectorEditor.h`  
Do not reuse directly. It removes the production editor's workflow methods and state-bearing integration points.

`sources/sector_editor/SectorEditorTypes.h`  
Do not reuse directly. It discards most production editor, UI, preview, selection, drag, modal, and bake state.

`docs/sector_editor.md`  
Do not reuse directly. It removes documentation for the real editor and describes the reduced prototype instead.

The eight modified `sources/sector_demo` files are not "damaged" in the same UI-loss sense, but they are complete experimental replacements rather than safe patches. Treat them as reference material and migrate their ideas in small changes.

## Verification scope

This audit was performed with read-only Git comparisons and `git show` against the experimental branch. No build was needed because no source, asset, or build-system file was modified by this audit.
