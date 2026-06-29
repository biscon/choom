# Authoring Graph Model Audit

## Summary

The current topology model is already Doom-like at the runtime/derived level: `SectorTopologyVertex`, directed `SectorTopologyLineDef`, front/back `SectorTopologySideDef`, and `SectorTopologySector` live in `sources/sector_demo/SectorTopologyTypes.h`, and `SectorTopologyMap` in `SectorTopologyMap.h` stores them as the editor document today. That model maps well to final closed sector topology, but it is too strict to be the permissive source-of-truth for loose authoring.

A future authoring graph should likely keep Doom-like vertices, linedefs, directed sides, and line metadata, but it should not require every line to have a sector-owned sidedef or every sector to extract into closed loops while the user is drawing. The current `SectorTopologyMap` should remain the derived topology consumed by mesh, collision, preview, lightmap, and 3D selection until a separate implementation plan says otherwise.

Implementation note (2026-06-28): `SectorAuthoringGraph` has been implemented
with stable authoring vertices, directed authoring lines, line-level flags and
future special data, authoring sides identified by `(lineId, side)`, and
face anchors carrying sector-like properties. Derivation planarizes and
auto-splits normal crossings, T-junctions, and endpoint-on-segment cases,
extracts faces, projects properties, validates the derived `SectorTopologyMap`,
and records diagnostics and authoring-to-derived mappings.

## Current Topology Model

`SectorTopologyVertex` stores a stable positive `id` plus exact integer `SectorCoord` `x`/`y`. Coordinates use `SectorCoordSubdivisions == 16`, and validation rejects invalid IDs and degenerate linedef endpoints.

`SectorTopologyLineDef` stores `id`, `startVertexId`, `endVertexId`, `frontSideDefId`, `backSideDefId`, and `SectorTopologyLineDefFlags`. The type comment in `SectorTopologyTypes.h` defines a directed line: front follows start-to-end, back follows end-to-start, and the sector owning a directed side lies to the left of that side.

`SectorTopologySideDef` stores `id`, `lineDefId`, `side`, `sectorId`, and concrete wall/lower/upper/middle material settings. This is closer to Doom sidedefs than a pure geometric edge because wall properties live on the directed side, not only on the sector.

`SectorTopologySector` stores properties such as name, heights, floor/ceiling textures, `ceilingSky`, floor/ceiling UV and decals, ambient lighting, and default wall materials. It does not store an authored ordered polygon; loops are extracted from the sidedefs that reference the sector.

`ValidateSectorTopologyMap()` in `SectorTopologyValidation.cpp` makes the current map a strict derived/runtime topology. It rejects linedefs with no sidedefs, invalid or duplicate endpoint pairs, sidedefs that do not match front/back slots, sidedefs that reference missing sectors, invalid intersections or overlaps, and sectors whose sidedefs cannot be extracted into loops.

`ExtractSectorTopologyLoops()` is the key boundary: for each sector, its directed sidedefs must form one outer loop and optional holes. Each boundary vertex must have exactly one incoming and one outgoing edge for that sector, loops need at least three edges, areas and orientation must be valid, and holes must be contained correctly.

## Proposed Source / Derived Split

The source model should be an authoring graph: permissive editor data used while drawing, moving, splitting, deleting, and assigning future line/side properties.

The derived model should remain a validated `SectorTopologyMap`: closed sectors/faces, normalized linedefs/sidedefs, stable IDs where possible, and all properties projected into the format already consumed by `BuildSectorGeneratedGeometry()`, `SectorCollisionWorld::BuildFromTopology()`, lightmap baking, preview rendering, and material selection.

The split should keep invalid intermediate states out of runtime consumers. Phase 1 and phase 2 findings show that save/load, render cache, sector picking, 3D preview, collision, and lightmaps expect already-valid topology today. The future bridge should be explicit: authoring graph changed, derivation attempted, derived topology updated only when valid or intentionally replaced by a checked result.

This audit does not propose a schema change. If persisted later, the authoring graph should be additive/backward-compatible rather than replacing topology v2 in one step.

## Candidate Authoring Graph Elements

Authoring vertices should contain stable IDs and exact integer coordinates. They should allow temporarily coincident vertices only if the editor can distinguish intentional overlap from pending merge; current derived topology should continue rejecting duplicate logical endpoint problems.

Authoring lines should contain stable IDs, start/end authoring vertex IDs, and directed orientation. They should allow loose, dangling, crossing, or unsided lines during editing, but derivation should normalize or reject them before producing runtime topology.

Authoring line flags should include current `blocksPlayer`-style portal metadata because `SectorTopologyLineDefFlags::blocksPlayer` is already consumed by `SectorCollisionWorld.cpp` and edited through `SetPortalBlocksPlayer()` in `SectorEditorTopologyActions.cpp`.

Optional authoring side metadata should exist independently from derived sidedefs. It should be directed-side data that can carry wall/lower/upper/middle material settings before a side is part of a valid closed sector, because current sidedef materials are concrete data and `SplitSectorTopologyLineDefAtPoint()` duplicates them when a line is split.

Face or sector property anchors should exist separately from derived sectors. They need to carry sector-level properties from `SectorTopologySector`: heights, sky flag, floor/ceiling textures and UVs, decals, ambient settings, and default wall materials. Without explicit anchors, re-derivation cannot reliably decide which derived face inherits a room's properties after a split, merge, or ambiguous line edit.

The graph may need derivation metadata: mapping from authoring vertices/lines/sides/faces to derived vertices/linedefs/sidedefs/sectors, plus conflict records for invalid or ambiguous regions. That mapping is also the natural place to remap editor selection and decide lightmap invalidation.

Map-level data should remain map-level: texture registry, static lights, preview settings, sky visual settings, directional light, lightmap bake settings, and baked lightmap metadata. These are not graph elements, although lightmap validity depends on derived topology and source-hash inputs.

## Doom-Like Concepts That Map Well

Vertices map directly. Current `SectorTopologyVertex` is already a sparse stable-ID point with exact authoring coordinates.

Directed linedefs map well. The current front/back convention is explicit and should be retained for authoring-line orientation, side-specific properties, portal metadata, and future action/special assignment.

Sidedef-like directed side data maps well for material ownership. Current `SectorTopologySideDef` is used by material editing, generated geometry refs, middle textures, and portal wall generation. Authoring sides can preserve this concept without requiring a live sector ID during invalid editing.

Two-sided linedefs map well to portals after derivation. Current collision and mesh generation infer portals from front/back sidedefs on the same physical linedef, and `blocksPlayer` already demonstrates line-level portal behavior.

Sector properties map well to derived faces, not directly to loose lines. The current sector owns room properties while boundary geometry is reconstructed from sidedefs, which is a good derived-topology shape if the authoring graph supplies durable face/property anchors.

## Doom-Like Concepts That Do Not Map Cleanly Yet

Current sidedefs cannot be copied directly into authoring state because they require a valid `sectorId` and matching linedef slot. A loose line side may have material or action metadata before it has a derived sector on that side.

Current sectors are not authored as explicit closed shapes. They are property bags plus loop extraction from sidedefs. That is good for derived topology, but an authoring graph needs a way to anchor room properties even when loops are temporarily broken or ambiguous.

Current validation rejects line intersections. A permissive graph may need to allow crossing lines during editing, then either split them, require user confirmation, or mark derivation invalid. Doom-like linedefs usually assume planar split lines in the final map, but the editor source can be looser than final topology.

Current split behavior in `SplitSectorTopologyLineDefAtPoint()` deletes the original linedef and duplicates sidedefs onto two new derived linedefs. That is acceptable for explicit topology edits, but an authoring graph needs a clear rule for whether future specials/actions stay on the parent authoring line or project onto all derived child segments.

Current derived IDs are hash-sensitive. `ComputeSectorLightmapSourceHash()` in `SectorLightmap.cpp` includes vertex, linedef, sidedef, sector, and static light IDs along with bake-relevant properties. Doom-like re-derivation that churns IDs will stale lightmaps and selection even if the geometry looks the same.

## Invalid Intermediate States To Support

The authoring graph should support loose/dangling lines that do not bound any sector yet. `ValidateSectorTopologyMap()` currently rejects this as a linedef with no sidedefs, so such data should not be stored in the derived map.

It should support open chains and partially drawn loops. `PendingSectorDraw` in `SectorEditorTypes.h` is already editor-only state for this, but it is limited to one ordered pending polygon rather than a persistent graph.

It should support crossing lines before validation. The current validator reports invalid intersections and partially overlapping linedefs; an authoring graph can keep them as editor warnings until derivation splits or rejects them.

It should support missing or ambiguous side ownership. A line side may have material metadata without a resolved sector until face extraction succeeds.

It should support invalid face candidates and property anchors that temporarily do not resolve to a derived sector. This lets the editor preserve user intent while showing warnings instead of deleting properties immediately.

## Validation And Derivation Responsibilities

Authoring validation should diagnose graph problems without requiring runtime validity. It can report dangling lines, crossings, overlaps, tiny segments, ambiguous faces, unassigned side properties, and unresolved property anchors.

Derivation should be the only step that produces a runtime `SectorTopologyMap`. It should split intersections, normalize duplicate endpoint usage, extract closed faces, assign front/back sidedefs, choose sector ownership, and copy map/line/side/face properties into derived topology.

Derived validation should continue using `ValidateSectorTopologyMap()` and `ExtractSectorTopologyLoops()`. The existing runtime contract is valuable and should remain strict.

Editor mutation wrappers should continue to own document-edited/cache invalidation behavior. Existing topology mutations call `MarkTopologyDocumentEdited()` / `InvalidateTopologyRenderCache()` from `SectorEditor.cpp`; future authoring graph mutations need an equivalent path and should invalidate any cached derived 2D render data when visible graph or derived topology changes.

Serialization should not save invalid data through `SerializeMap()` because `ValidateForSerialization()` requires valid topology. If authoring graph persistence is added later, it should have its own validation/defaulting rules while preserving current topology v2 compatibility.

Implementation note (2026-06-28): Authoring graph persistence was added as a
graph-native document path rather than through `SerializeMap()`. New authoring
documents use root `formatVersion: 3` and `topology: "authoringGraph"`, persist
the permissive graph plus map-level data, and regenerate derived topology on
load when possible.

## Property Anchoring Options

Line-anchored properties work well for `blocksPlayer` and future linedef-level specials. If a line is split by derivation, the property can project to each child derived linedef unless the special needs a single logical activator.

Directed-side-anchored properties work well for wall/lower/upper/middle textures, UVs, and decals. If one authoring side maps to multiple derived sidedefs, current split behavior suggests duplicating the side material settings across all children.

Face-anchored properties are needed for sector heights, sky flag, floor/ceiling materials, UVs, decals, ambient settings, and default wall materials. Geometry-only face matching is fragile; explicit property anchors or user-managed room IDs would preserve intent better.

Coordinate and topology matching can be fallback strategies for migration from existing maps, but they should not be the primary identity model. Exact coordinate matching cannot resolve coincident vertices, split lines, or face merge conflicts.

A derivation map should record old-to-new derived IDs when possible. This is needed for selection preservation, 3D surface refs, save diffs, and lightmap reuse. If stable derived IDs cannot be preserved, derivation should deliberately stale baked lightmaps rather than pretending the old `bakedLightmap.sourceHash` is still valid.

## Future Door / Linedef Special Constraints

This audit does not add doors or linedef specials.

Future door/action metadata should be treated as a constraint on the model: authored linedefs need durable identity even if the derived topology splits them. A door special attached to a line should not disappear just because an intersection inserted a derived vertex.

Portal-like behavior currently lives on topology linedefs and sidedefs. `SetPortalBlocksPlayer()` edits `SectorTopologyLineDefFlags::blocksPlayer`, and collision consumes that flag on portal edges. Future specials should follow the same principle: authoring metadata anchors on a line or directed side, then derivation projects it to the validated topology consumed by runtime systems.

If a future special depends on exactly one sector on one or both sides, derivation must detect ambiguous cases and surface an editor error instead of guessing. This is especially important when one authoring line borders multiple derived faces after splitting.

## Risks / Unknowns

The biggest risk is identity churn. Selection, generated surface refs, save order, and lightmap source hashes all observe stable topology IDs today.

Sector property inheritance is unresolved for graph edits that split, merge, or delete faces. Existing tools have local rules, such as `CutSectorTopologySectorBetweenBoundaryPoints()` cloning the original sector, but whole-graph derivation needs general conflict behavior.

Side material inheritance is unresolved when intersections split a line or when line direction changes. Current `SplitSectorTopologyLineDefAtPoint()` duplicates material settings, but a general derivation pass needs deterministic ownership and ID rules.

It is unknown whether the editor should update derived topology continuously as graph edits happen, keep the last valid derived topology while the graph is invalid, or require an explicit rebuild/validate action.

It is unknown whether persisted files should eventually contain only an authoring graph, both authoring graph and derived topology, or topology v2 plus optional authoring graph data during migration.

## Recommended Follow-Up Questions

Should authoring graph IDs become the primary persisted IDs, with derived topology IDs treated as cache output, or must derived IDs remain stable and persisted?

When a graph edit splits one face into two, which derived sector keeps the old sector ID and which sector properties are cloned, defaulted, or user-selected?

Should line intersections be automatically split during derivation, or should crossing lines remain invalid until the user explicitly resolves them?

Should 3D preview show the last valid derived topology while the authoring graph is invalid?

What minimal authoring-side metadata is needed before adding future linedef specials, so current `blocksPlayer` and middle texture behavior do not need another migration?
