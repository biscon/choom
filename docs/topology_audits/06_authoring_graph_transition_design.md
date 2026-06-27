# Authoring Graph Transition Design

## Summary

Decision:
  Move editor authoring to a permissive `AuthoringGraph`, and keep `SectorTopologyMap` as the strict derived topology consumed by preview, mesh generation, collision, lightmaps, and runtime-like systems.

Reason:
  The current editor document is a valid topology v2 `SectorTopologyMap`, which works well for runtime consumers but cannot represent normal Doom-editor authoring states such as loose lines, open chains, crossing lines, or temporarily unresolved rooms. The bridge should be derivation: graph in, valid topology out.

Consequences:
  The editor can save and reopen invalid work-in-progress states without forcing loose data into the runtime topology contract. Existing runtime systems can remain focused on valid closed sectors. The hard work moves into a derivation layer, identity mapping, diagnostics, and property anchoring.

Implementation notes:
  Treat this as a source-model change, not a mesh/collision/lightmap rewrite. Build the graph and derivation path first, then move editor tools over in phases.

## Non-Goals

Decision:
  Keep this transition scoped to authoring model and derivation design.

Reason:
  The goal is to make permissive 2D authoring possible without bundling unrelated runtime features or broad rewrites.

Consequences:
  This transition does not add 3D doors. It does not rewrite runtime mesh generation unless derivation proves a narrow change is required. It does not rewrite collision unless derivation proves a narrow change is required. It does not rewrite lightmaps unless source identity or invalidation requires a narrow change. It does not attempt to preserve every old test level. It does not try to become an Ultimate Doom Builder clone in one step.

Implementation notes:
  Future door or linedef special support should influence metadata placement, but no door behavior, 3D door geometry, dynamic runtime light, shadowmap, or runtime special system belongs in this transition.

## User Constraints And Compatibility Policy

Decision:
  Prefer a clean breaking save-format transition when `AuthoringGraph` becomes authoritative. Backward compatibility is optional; old levels are test data.

Reason:
  The project is not released software and has one owner. The migration audit's two-track `topology v2 plus optional authoringGraph` path is safest for shipped compatibility, but it adds complexity that is not justified here if it makes implementation slower or more bug-prone.

Consequences:
  It is acceptable to bump the root format version, replace topology-only save semantics, stop guaranteeing old topology v2 files load directly, provide only a temporary/dev import from current topology, and intentionally clear or stale old baked lightmap metadata. The cost is that current sample levels may need a one-time import, manual recreation, or deletion.

Implementation notes:
  Recommended policy: new saves persist the authoring graph and map-level settings as the document model. Derived `SectorTopologyMap` is generated data. Add a one-way topology-v2 import only if it is cheap and useful for current test levels. Do not design the whole system around old topology-only JSON.

## Core Decision: Source Graph And Derived Topology

Decision:
  `AuthoringGraph` is the editor source of truth. `SectorTopologyMap` is the strict derived/runtime product. Runtime systems should never consume invalid authoring graph data.

Reason:
  Current validation, loop extraction, generated geometry, collision, lightmaps, 3D picking, and save/load all assume valid closed topology. Loose authoring lines violate that contract by design.

Consequences:
  Invalid editing is allowed in the graph, but preview/runtime features are gated by a successful derivation result. Runtime consumers can keep their useful strict assumptions. Editor features need separate authoring selection, authoring rendering, graph diagnostics, and graph-to-derived mapping.

Implementation notes:
  Do not force loose lines into `SectorTopologyMap`. Derivation should produce a normal validated map with vertices, linedefs, sidedefs, sectors, static lights, textures, preview settings, sky settings, directional light, lightmap settings, and any bake metadata that is still considered current.

## Current Architecture To Preserve

Decision:
  Preserve the existing runtime-facing topology consumers wherever practical.

Reason:
  The audits show that the expensive and risky systems already share a coherent strict topology contract. Replacing them is unnecessary for permissive authoring.

Consequences:
  These systems should ideally remain unchanged or only receive narrow mapping/gating integration:
  - `BuildSectorGeneratedGeometry`
  - `SectorMeshPreview`
  - `SectorCollisionWorld`
  - lightmap layout, bake, current/stale checks, and source hash
  - 3D generated-surface picking and surface refs
  - gameplay preview movement and sector lookup

Implementation notes:
  Editor paths that will need later changes include pending sector drawing, line/sidedef/sector selection, 2D fills and overlays, sector insertion/cut/delete tools, vertex movement, material inspector writes, save/load document wrappers, preview entry, lightmap bake entry, and cache invalidation around authoring mutations.

## Proposed AuthoringGraph Model

Decision:
  Model authoring as a permissive Doom-like graph with durable authoring IDs and separate property anchors.

Reason:
  Current topology types already map well to final runtime topology, but sidedefs and sectors are too strict for partially authored geometry. Properties need to survive while faces temporarily fail to resolve.

Consequences:
  The graph can represent loose lines, dangling endpoints, crossings, endpoint-on-segment cases, unresolved sides, and unresolved room anchors. Derivation planarizes normal intersections before deciding which closed faces become runtime sectors.

Implementation notes:
  Conceptual data model:

  - `AuthoringVertex`: stable positive `id` plus exact integer coordinate using the current `SectorCoord` grid.
  - `AuthoringLine`: stable positive `id`, start/end authoring vertex IDs, directed orientation, line flags, and future linedef specials.
  - `AuthoringSide`: line ID plus front/back identity, wall/lower/upper/middle material metadata, decals, UV data, and future directed-side metadata. This should not require a live sector ID.
  - `AuthoringFaceAnchor` or `RoomAnchor`: stable positive `id`, sector-like properties, and intended derived face identity. It may temporarily fail to resolve.
  - Map-level data: texture registry, static lights, preview settings, sky visual settings, directional light, bake settings, and any persisted editor metadata that is not a graph element.

  Avoid over-specifying the final C++ layout until derivation tests clarify the smallest useful structs.

## Proposed Derived Topology Contract

Decision:
  The derived product remains a valid `SectorTopologyMap`.

Reason:
  Runtime and preview code already depend on the current topology contract: valid IDs, valid endpoint references, linedefs with sidedefs, sidedefs assigned to sectors, closed loops, portals represented as two-sided linedefs, and no invalid intersections or overlaps.

Consequences:
  Derived topology contains no loose lines. It must satisfy `ValidateSectorTopologyMap()` and support `ExtractSectorTopologyLoops()` for every derived sector. It remains runtime-safe.

Implementation notes:
  Derivation should either return a valid map plus diagnostics/mapping or fail without replacing the current valid derived topology. It should not produce a half-valid `SectorTopologyMap` for runtime consumers.

## Save / Load Strategy

Decision:
  Persist `AuthoringGraph` as the authoritative document model. Do not persist `SectorTopologyMap` as the primary source of truth.

Reason:
  Invalid authoring states must be saveable, and old topology-only compatibility is not worth the added complexity. Persisting derived topology as a coequal source creates stale-data and mismatch problems that are avoidable in this project.

Consequences:
  Save files can contain invalid graphs. Derived topology should be regenerated on load. If derivation fails on load, the graph still opens, 2D authoring remains available, diagnostics are shown, and 3D preview/runtime-like actions are disabled until a valid topology can be derived. Baked lightmap metadata should be cleared or marked stale whenever the derived topology source hash changes or derivation cannot prove it is still current.

Implementation notes:
  Recommended shape:
  - bump the root save format/version when graph saves become authoritative
  - persist `authoringGraph` and map-level data
  - regenerate `SectorTopologyMap` during load or after load
  - allow saving invalid graph states
  - do not require old `vertices`/`linedefs`/`sidedefs`/`sectors` arrays in new graph-native files
  - optionally include a debug-only or cache-only derived topology dump later, but only if profiling or tooling proves it is worth the consistency burden

## Invalid Graph And Last Valid Derived Topology

Decision:
  Invalid graph states are first-class editor states. Last-valid derived topology should be memory-only, not persisted.

Reason:
  Persisting last-valid topology adds a second truth source and makes stale preview state easier to confuse with the saved graph. A memory-only last-valid result gives useful continuity while editing without making file semantics ambiguous.

Consequences:
  The editor can continue drawing authoring lines and diagnostics while the graph is invalid. The 2D editor may show the last valid derived fills only with a clear stale/invalid indicator. Entering or rebuilding 3D preview is disabled unless the graph has a current valid derived topology. A freshly loaded invalid graph has no persisted fallback topology unless derivation succeeds.

Implementation notes:
  Store derivation status in editor state: valid/current, valid/stale, invalid/no-derived, invalid/last-valid-visible. The 2D editor may show stale derived fills or diagnostics if they are clearly marked. Entering or rebuilding 3D preview should require a current valid derivation; do not let the user unknowingly preview stale topology as if it matched the graph. Any edit that changes graph geometry or visible authoring state must invalidate the relevant 2D render cache and mark the derived result stale until derivation succeeds.

## ID Strategy

Decision:
  Primary stable IDs live on authoring objects. Derived IDs should be stable enough for editor usability, but preserving old topology IDs at all costs is not required.

Reason:
  Stable authoring IDs preserve user intent across invalid states. Derived IDs are still useful for generated surfaces, selection, save diffs, and lightmap hash stability, but over-optimizing them for old topology v2 files would complicate the new design.

Consequences:
  Authoring vertex IDs, authoring line IDs, line+side identities, and face/room anchor IDs become the durable persisted identities. Derived vertex/linedef/sidedef/sector IDs are allocated deterministically from derivation where practical. Selection remapping should prefer authoring IDs; derived selection should be cleared or remapped through the derivation map when topology changes.

Implementation notes:
  Practical policy:
  - authoring vertices keep IDs through coordinate moves
  - authoring lines keep IDs through endpoint moves and carry line flags/specials
  - authoring sides start as `(authoringLineId, side)` identities
  - do not add separate authoring side IDs unless derivation or property tests prove they are needed
  - face anchors own room/sector-like IDs and properties
  - derived vertices may reuse authoring vertex IDs for unchanged graph vertices and allocate new IDs for inserted intersection points
  - derived linedefs may reuse authoring line IDs only for unsplit one-to-one segments; split children get deterministic new IDs or generated IDs
  - derived sidedefs map from authoring side plus resolved face
  - derived sectors map from resolved face anchor when unambiguous
  - lightmaps should be considered stale if derivation changes hash-sensitive IDs or geometry

## Property Anchoring Strategy

Decision:
  Store user-authored properties on authoring objects and project copies into derived topology.

Reason:
  `SectorTopologyMap` properties are runtime-ready but not durable enough while the source graph is invalid or being re-derived. Anchors keep properties attached to user intent instead of transient loops.

Consequences:
  Sector/room properties live on face anchors. Wall, lower, upper, middle, UV, and decal materials live on directed authoring sides. `blocksPlayer` and future line specials live on authoring lines. Texture registry, static lights, preview settings, sky visual settings, directional light, and bake settings stay map-level. Derived topology receives projected copies.

Implementation notes:
  Conflict rules should be explicit:
  - line split: child authoring or derived lines inherit line metadata and both directed-side material sets
  - face split: keep the original face anchor on the face containing the old anchor seed or label point when available; new faces clone properties or receive defaults by a deterministic editor rule
  - face merge: if exactly one anchor survives in the merged face, keep it
  - multi-anchor face merge: report a diagnostic and require later user choice instead of silently choosing a winner
  - ambiguous derivation: report a diagnostic and do not silently guess
  - unresolved face anchor: keep the anchor in the graph, do not delete its properties
  - first implementation: prefer conservative defaults and diagnostics over trying to solve every split/merge conflict immediately

## Derivation Pipeline

Decision:
  Derivation should be a pure, testable pipeline from graph to validated topology, diagnostics, and mapping. It auto-planarizes the authoring graph: normal segment crossings, T-junctions, and endpoint-on-segment cases are automatically split into graph vertices/edges for derivation.

Reason:
  The bridge is the core risk. It must be deterministic and testable before UI tools depend on it. Requiring the user to manually split every linedef at every intersection would defeat the purpose of a permissive Doom-editor-like graph.

Consequences:
  Editor tools can mutate graph data freely, then request derivation. The authored graph may remain permissive, while derivation operates on a normalized planar graph. Runtime systems receive only the successful strict result. Diagnostics explain why a graph cannot currently produce runtime topology.

Implementation notes:
  Pipeline:
  - authoring graph
  - normalize and auto-split normal crossings, T-junctions, and endpoint-on-segment cases
  - build planar graph
  - find closed faces
  - assign and resolve face anchors
  - project line, side, and face properties
  - generate `SectorTopologyMap`
  - validate strict topology
  - produce derivation diagnostics and authoring-to-derived ID mapping

  Auto-split cases:
  - normal segment crossings
  - T-junctions
  - endpoint-on-segment cases

  Diagnose, refuse, or require cleanup instead of silently guessing:
  - exact duplicate lines
  - collinear overlapping segments
  - near-miss almost-intersections
  - zero-length lines
  - tiny sliver faces below threshold
  - ambiguous face or property anchors
  - unresolved face anchors
  - unassigned sides
  - invalid portals
  - duplicate/coincident endpoints where unsupported
  - property projection conflicts

## Legacy Tooling And Inspector Transition Strategy

Decision:
  Once `AuthoringGraph` is authoritative, normal editor UI tools must mutate `AuthoringGraph`, not derived `SectorTopologyMap`. `SectorTopologyMap` is generated output and should not be directly edited by normal editor tools.

Reason:
  Directly editing derived topology creates two competing sources of truth. The graph would no longer reliably derive the preview/runtime topology, and save/load, property anchoring, selection remapping, and lightmap invalidation would become confused.

Consequences:
  Legacy direct-topology tools can exist only while the old topology path is still active, or as temporary/dev-only tools outside graph-authoritative mode. Once graph-authoritative mode is enabled, direct `SectorTopologyMap` mutation tools should be hidden, disabled, retired, or reimplemented as authoring graph operations.

Implementation notes:
  Retire or demote to legacy-only unless reimplemented as graph operations:
  - closed polygon Sector draw tool
  - Insert Sector Inside
  - Cut Sector as a direct topology operation
  - Delete Sector as a direct topology operation
  - Split linedef at midpoint as a direct topology operation
  - Split linedef at point as a direct topology operation
  - Merge topology vertices as a direct topology operation
  - Dissolve topology vertex as a direct topology operation

  Reimplement old concepts in graph terms:
  - Draw Sector Polygon: replaced by freeform authoring line drawing or loop drawing
  - Insert Sector Inside: replaced by drawing authoring lines inside an existing face; derivation creates inner faces, holes, or sectors
  - Cut Sector: replaced by authoring line drawing across a face; derivation splits faces
  - Split Linedef: mostly replaced by auto-splitting during derivation, plus an optional explicit split-authoring-line command for user convenience
  - Merge/Dissolve Vertex: reimplemented as authoring vertex merge/dissolve operations, followed by derivation
  - Delete Sector: becomes delete face/room anchor, delete boundary lines, or clear room properties depending on the intended UX

  Preserve and port these workflows so they write through authoring anchors:
  - texture picker
  - material/wall part editors
  - sector/floor/ceiling property inspector
  - sidedef/wall/lower/upper/middle material inspector
  - `blocksPlayer` / line flag editing
  - light tools and static light editing
  - preview settings
  - import texture workflow

  Porting rule:
  - sector inspector becomes `AuthoringFaceAnchor` / `RoomAnchor` inspector
  - sidedef inspector becomes `AuthoringSide` inspector
  - linedef flags become `AuthoringLine` inspector
  - derived 3D surface selection maps back to authoring side or face anchor before editing

  Transitional strategy:
  1. Keep legacy topology tools only while the old topology path is still active.
  2. Introduce authoring graph state and derivation behind the scenes.
  3. Once graph-authoritative mode is enabled, hide or disable direct topology mutation tools.
  4. Add the new minimal graph tools: draw line, select line/vertex, move vertex, delete line/vertex.
  5. Port inspectors to authoring anchors.
  6. Delete or demote old topology tools after graph tools cover the workflow.

  This avoids a hybrid state where some tools mutate the graph and other tools mutate the derived map.

## Editor Integration Strategy

Decision:
  Integrate graph authoring in phases, with separate authoring overlay and selection state.

Reason:
  Current editor UI assumes topology sectors are valid closed loops. Mixing authoring and derived selection without separation would make invalid states and stale previews confusing.

Consequences:
  The 2D editor draws authoring lines/vertices/diagnostics separately from derived sector fills and labels. Selection distinguishes authoring graph objects from derived topology objects. Preview and bake actions are gated by valid derived topology.

Implementation notes:
  Add the authoring graph model first. Add derivation tests before UI. Keep the current closed-sector workflow temporarily if it helps bootstrap graph data. Then add line drawing, split, delete, and move tools. Convert inspector editing so material and room edits write to authoring anchors and are projected into derived topology. Authoring graph mutations should use document-edited/cache-invalidation helpers analogous to `MarkTopologyDocumentEdited()` and `InvalidateTopologyRenderCache()`.

## Runtime / Preview / Lightmap Strategy

Decision:
  Runtime, preview, collision, and lightmaps keep consuming derived `SectorTopologyMap`.

Reason:
  The current consumers are already strict, validated, and shared. The authoring graph should not leak invalid data into systems that need closed sectors.

Consequences:
  Preview build, gameplay collision, generated mesh, 3D picking, and lightmap bake remain derived-topology operations. Generated surface refs can remain topology refs, but editing a selected 3D surface must map back to authoring side or face anchors before mutating source data.

Implementation notes:
  Do not over-preserve old lightmaps. Stale-on-derivation is acceptable for this transition. Clear or mark baked metadata stale when derived topology changes, when derivation fails, or when `ComputeSectorLightmapSourceHash()` changes. Do not redesign lightmap hashing around logical surface IDs in this transition. Preserve current source-hash behavior unless a later lightmap-specific task explicitly changes it. Sky visual settings remain visual-only and excluded from the hash; `ceilingSky` remains geometry-affecting and included through derived sectors.

## Migration / Import Strategy For Existing Test Levels

Decision:
  Provide a low-effort one-way topology-v2 import only if it is useful. Do not build a large compatibility system.

Reason:
  Current levels are test data. A simple importer can save time, but it should not dictate the new save model.

Consequences:
  Some old maps may stop loading directly after the format break. Existing test levels may be imported, manually recreated, or discarded. Lightmap hashes, baked metadata, selections, and derived IDs do not need perfect preservation.

Implementation notes:
  Cheap import path:
  - topology vertices become authoring vertices
  - topology linedefs become authoring lines with orientation and `blocksPlayer`
  - topology sidedefs become directed-side material metadata
  - topology sectors become face anchors with sector-like properties
  - textures, static lights, preview settings, sky settings, directional light, and bake settings stay map-level
  - baked lightmaps are cleared unless the import and derivation prove the source hash is unchanged

  This importer may be temporary dev tooling rather than a long-term loader branch.

## Future Door / Linedef Special Constraints

Decision:
  Do not add doors in this transition, but place future special metadata on authoring lines or directed sides.

Reason:
  Door/action/special metadata needs durable identity across splits and derivation. Anchoring it on transient derived linedefs would force another migration later.

Consequences:
  Derivation projects future specials onto derived linedefs, portals, or sidedefs when the resolved topology supports them. If a special requires a clear neighboring sector relation and the graph is ambiguous, derivation reports an error instead of guessing.

Implementation notes:
  `blocksPlayer` is the current example of line-level metadata. Future specials should follow the same pattern: authoring owns intent; derived topology receives runtime-ready copies.

## Risks And Tradeoffs

Decision:
  Accept a save-format break and some lightmap/test-level churn to keep the architecture simple.

Reason:
  The biggest long-term risk is confusing source-of-truth ownership, not losing old test files.

Consequences:
  Important risks remain:
  - deriving stable faces is hard
  - property conflict rules can be tricky
  - invalid graph UX matters
  - last-valid topology can confuse the user if stale
  - lightmap reuse may suffer
  - save format break simplifies implementation but loses direct old test-map compatibility
  - derived ID churn can clear selections and stale bakes
  - intersection splitting can make line and side metadata projection non-obvious

Implementation notes:
  Keep diagnostics visible and conservative. Prefer clearing stale selections/lightmaps over pretending mappings are valid. Add focused derivation tests before editor UI changes.

## Recommended Implementation Phases

Decision:
  Implement in staged vertical slices, starting with data and pure derivation before changing core editor tools.

Reason:
  The transition touches persistence, selection, rendering, and preview gating. Small phases keep behavior understandable and testable.

Consequences:
  The existing editor can continue working while the graph path is built behind it, then tools can move one at a time.

Implementation notes:
  Recommended phases:

  1. Add `AuthoringGraph` data model only.
  2. Add topology-v2 import into `AuthoringGraph`, if still useful.
  3. Add a pure derivation module for simple closed graphs.
  4. Add derivation diagnostics and result mapping.
  5. Add save/load for the new authoring format.
  6. Add editor state fields for authoring graph, derivation status, last-valid derived topology, and mapping.
  7. Add 2D rendering overlay for authoring graph lines, vertices, anchors, and diagnostics.
  8. Add authoring line draw, split, delete, and move tools.
  9. Hide or disable direct topology mutation tools in graph-authoritative mode.
  10. Project properties and inspector editing through line, side, and face anchors.
  11. Switch preview, bake, and save to the derived-from-authoring path.
  12. Remove or demote the old closed-sector authoring path once graph tools cover the needed workflow.

## Open Questions

Decision:
  Close the major design questions now. Leave only narrow implementation details for derivation tests and editor prototypes.

Reason:
  Future runner plans need concrete direction. The remaining uncertainty should be about thresholds and UI details, not source-of-truth ownership or basic graph semantics.

Consequences:
  Closed decisions:
  - crossing lines auto-split during derivation for normal crossings, T-junctions, and endpoint-on-segment cases
  - face split keeps the original anchor on the face containing the old anchor seed/label point when available
  - face merge keeps a single surviving anchor, but multiple anchors in one merged face produce a diagnostic and require later user choice
  - authoring side identity starts as `(lineId, side)`; no separate side IDs unless tests prove they are needed
  - last-valid derived topology is memory-only; 2D may show it only when clearly stale, and 3D preview requires current valid derivation
  - lightmaps may stale on derivation; do not redesign hashing around logical surface IDs in this transition

Implementation notes:
  Narrow implementation details still to decide:
  - exact tolerance or threshold for tiny sliver face diagnostics
  - exact UI wording and visual treatment for stale 2D derived fills
  - whether an explicit split-authoring-line command is useful even though derivation auto-splits
  - initial default/clone rule for new faces created by a split when no anchor seed is available
