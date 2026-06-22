# Sector Topology Incremental Refactor

## Phase 1

The Doom-like topology model now exists beside the original polygon-sector model. It introduces first-class vertices, linedefs, sidedefs, and sectors with stable positive integer IDs. The existing `SectorMap`, editor workflow, renderer, generated geometry, save/load format, and lightmap pipeline are unchanged and still use the polygon-sector model.

Planar topology coordinates use exact `int32_t` values with 16 stored subdivisions per visible authoring unit. Heights remain floating-point authoring values. Conversion to floating-point world coordinates happens at rendering or generated-geometry boundaries, using the existing sector world scale.

A linedef is directed from its start vertex to its end vertex. Its front side follows start to end, its back side follows end to start, and the owning sector lies to the left of either directed side.

Later phases will add validation and loop extraction, persistence, generated geometry, and editor integration incrementally. No topology UI or legacy conversion is part of Phase 1.

## Phase 2: validation and loop extraction

The topology layer now provides transient indexes for ID lookups and derived sidedef relationships. The indexes retain duplicate-ID entries so validation can diagnose ambiguous references without storing pointers or caches in `SectorTopologyMap`.

Structured validation issues identify severity, topology object kind, stable object ID, and a readable message. Validation covers IDs, references, linedef/sidedef slot consistency, degenerate and duplicate lines, planar segment intersections and overlaps, and sector boundary structure. Malformed or dangling references are reported and excluded from deeper geometric checks rather than dereferenced.

Sector sidedefs are converted to directed boundary edges using the Phase 1 convention: front follows linedef start to end, back follows end to start, and the owning sector is on the left. Loop extraction requires one incoming and outgoing edge per boundary vertex, rejects open, branching, touching, self-intersecting, or zero-area loops, and produces deterministic ordered vertex, sidedef, and edge sequences.

Topology coordinates use the mathematical winding convention: positive signed area is counter-clockwise. Each sector must have exactly one counter-clockwise outer loop; clockwise loops are holes. Holes must be strictly contained by the outer loop and may not intersect, touch, or contain other holes.

This phase is backend-only. The existing editor UI and document workflow still use the polygon-sector model, and topology persistence, generated geometry, rendering, selection, editing, and lightmap integration remain for later phases.

## Phase 3: separate v2 topology JSON persistence

The topology layer now has separate JSON string and file load/save functions for an explicit v2 linedef format. Documents must contain `formatVersion: 2`, `topology: "linedef"`, the fixed `coordSubdivisions` value, the texture table, and all four topology arrays. Vertex coordinates are stored and accepted only as JSON integers.

Loading parses into a temporary map and validates it before replacing the caller's map. Saving validates before producing or writing JSON. Invalid shapes, value types, side names, fixed-coordinate markers, texture definitions, and topology references are reported as readable errors. Output is deterministic: topology records are sorted by stable ID and textures by texture ID.

The existing editor document workflow and polygon `SectorMap` load/save path remain unchanged. There is no legacy conversion, generated topology geometry, topology rendering or editing, editor migration, or lightmap integration in this phase.

## Phase 4: topology generated geometry

Topology maps can now produce `SectorGeneratedGeometry` through a separate overload while the original polygon geometry path remains intact. The topology builder validates the complete map and uses all-or-nothing output: validation, loop extraction, lookup, or triangulation failures return a readable error and no surfaces.

Each sector's deterministic extracted outer loop and holes are triangulated with earcut into one floor surface and one ceiling surface. Directed sidedefs produce full walls for one-sided lines and lower or upper spans where adjacent sector heights differ. Equal-height two-sided portals intentionally produce no wall mesh. Wall lengths retain floating coordinate-space precision when converted to world units.

Generated surface refs now carry stable topology sector, linedef, sidedef, and front/back IDs for future picking, diagnostics, and lightmap identity. This phase does not migrate the editor, renderer, 3D preview, old save/load workflow, or lightmap pipeline to topology geometry.

## Phase 5: topology mesh-building path

Topology maps now have a parallel mesh-building path that routes through the topology generated-geometry overload before batching generated vertices by texture ID into raylib-compatible mesh data. The original polygon `BuildSectorMeshes(const SectorMap&, ...)` path remains in place for existing editor, preview, renderer, and document workflow callers.

The shared CPU batch-data conversion can be tested without opening a window or uploading GPU meshes. The topology mesh builder still preserves existing material assumptions, does not load textures, and does not require texture files to exist.

Triangle winding is covered by non-GUI tests that compare every generated triangle's geometric normal against its emitted surface normal, because the preview renders with backface culling. This phase does not migrate the editor, preview UI, old save/load workflow, renderer callers, or lightmap pipeline to topology meshes.

## Phase 6: dormant editor topology document state

The existing editor shell now owns a dormant `SectorTopologyMap` beside the original polygon `SectorMap`. New blank editor documents and successfully loaded polygon documents reset this topology state to an empty, inactive topology document with the same default texture definitions used by the blank polygon map.

The polygon `SectorMap` remains the active editor document. The existing editor UI, tools, inspector, texture picker, modals, 2D drawing, selection, 3D preview, save/load/reload workflow, renderer callers, and lightmap workflow still use the polygon map.

No topology UI, drawing, selection, editing tools, topology save/load, preview, renderer, or lightmap migration is part of this phase. Future phases will switch document workflow and editor subsystems to topology one piece at a time.

## Phase 7: topology editor document workflow

The existing full editor shell now treats `SectorTopologyMap` as the document for New, Load, Save, Save As, and Reload. Those document actions use the Phase 3 topology v2 JSON serialization path, while the old polygon `SectorMap` remains in editor state only as a blank compatibility canvas for the not-yet-migrated shell, panels, grid, and rendering code.

New creates an empty topology document with the default texture definitions and no vertices, linedefs, sidedefs, or sectors. Loading and reloading parse topology v2 JSON into a temporary map before replacing the editor document, and saving writes topology v2 JSON to the existing `assets/levels/<levelName>/<levelName>.json` layout.

Old polygon JSON is rejected by the topology loader; there is still no legacy polygon-to-topology conversion. Topology 2D rendering, topology selection and tools, inspector migration, 3D preview migration, renderer caller migration, and lightmap migration remain deferred.

## Phase 8: topology 2D editor rendering

The existing 2D editor viewport now renders document geometry from `state.topologyMap`. Topology vertices, linedefs, linedef direction markers, passive front/back sidedef indicators, and sector loop fills/outlines are drawn directly from the topology document. Sector loops use `ExtractSectorTopologyLoops()`, and render warnings are reported in the editor status/viewport instead of opening repeated dialogs.

Topology linedefs are always drawn from the topology data, not from generated wall meshes, so equal-height two-sided portals remain visible in 2D even when they would not produce 3D wall surfaces. The old polygon compatibility map is no longer visible document geometry.

The Add Map Texture workflow now updates `state.topologyMap.texturesById`, marks the topology document dirty, and persists through topology v2 JSON save/reload. Static lights are still deferred because the topology document model does not own static lights yet.

Topology selection, editing tools, deletion, movement, light placement, inspector editing, 3D preview migration, and lightmap migration remain deferred. Old polygon edit actions are blocked with status messages rather than partially ported.

## Phase 9: topology sector drawing

The existing Sector draw workflow now creates topology document geometry. Finalizing a new drawn polygon adds exact integer topology vertices, endpoint-pair linedefs, concrete sidedefs, and one sector to `state.topologyMap`, then saves and reloads through the v2 topology JSON path.

Drawn points are converted to `SectorCoord` values before identity checks or topology mutation. Vertices are reused only when an existing topology vertex has the exact same stored integer coordinate, so adjacent sectors drawn on the same grid endpoints share vertex IDs instead of creating nearly-identical duplicates.

Linedefs are reused by exact endpoint pair. A same-direction shared edge occupies the front sidedef slot, a reversed shared edge occupies the back sidedef slot, and adjacent sectors that share a full edge persist as one linedef with both front and back sidedefs occupied.

Invalid topology creation is rejected transactionally. Duplicate or repeated canonical points, zero-area polygons, occupied linedef side slots, partial overlaps, crossings, and non-shared interior touches fail without partially editing the map. Clockwise input is normalized to the Phase 2 CCW outer-loop convention before creation.

Topology selection, inspector editing, Insert Sector Inside, Move, Erase, Light placement, 3D preview migration, and lightmaps remain deferred. The old polygon `state.map` remains compatibility state and is not mutated or saved as document geometry by the Sector draw workflow.

## Phase 10: topology sector selection and inspector

The Select tool can now select topology sectors in the 2D viewport. Picking uses extracted topology sector loops from `state.topologyMap`, respects holes, and resolves boundary or overlap ties deterministically by choosing the lowest stable topology sector ID. Sidedef, wall, edge, and vertex selection remain deferred.

The existing inspector panel now displays and edits selected `SectorTopologySector` data, including the sector name, heights, floor and ceiling textures/UVs, ambient color/intensity, and default wall/lower/upper texture and UV settings. Texture picker targets use the topology texture table and a stable topology sector ID plus explicit sector field, not old polygon sector indexes.

Successful inspector edits mark the topology document dirty and persist through topology v2 JSON Save/Reload. Invalid height edits are rejected before they can make the topology sector invalid.

Sector default wall/lower/upper edits only update the selected sector defaults for future sidedefs. Existing `SectorTopologySideDef` wall/lower/upper records are not rewritten by the inspector path.

The 3D preview, lightmap baking, sidedef/wall inspector editing, Move, Erase, Insert Sector Inside, Split Linedef, vertex dragging, light placement, and polygon-to-topology conversion remain deferred.

## Phase 11: topology sidedef selection and inspector

The Select tool can now select topology linedefs and sidedefs in the 2D viewport before falling back to sector picking. Linedef hit testing uses screen-space distance for stable zoom-independent picking, while front/back side choice uses the click position relative to the directed linedef in topology authoring space. If a clicked side has no sidedef but the opposite side exists, the editor selects the existing opposite sidedef; if neither side exists, it selects the linedef as line-only.

Selected sidedefs render as a thick translucent offset halo on the selected side of the linedef, while line-only selections render as a centered neutral halo. These highlights draw above sector fills and the selected-sector halo, but below normal linedefs, direction arrows, front/back ticks, vertices, labels, pending draw overlays, and the snap crosshair.

The inspector now edits concrete `SectorTopologySideDef` wall, lower, and upper texture and UV settings independently per side. Texture picker targets use stable topology sidedef IDs plus an explicit wall/lower/upper part and continue to draw choices from `state.topologyMap.texturesById`. Reset UV actions restore scale to `(1,1)` and offset to `(0,0)` for the selected part without changing its texture or other parts.

Two-sided linedefs expose a Switch to opposite side action when the opposite sidedef exists. Equal-height portals remain selectable and editable because 2D selection uses topology linedefs and sidedefs, not generated wall meshes. Sector selection and the topology sector inspector remain working. The 3D preview, lightmaps, Move, Erase, Insert Sector Inside, Split Linedef, vertex dragging, light placement, sidedef creation, and polygon-to-topology conversion remain deferred.

## Phase 12: topology 3D preview and picking

The existing 3D preview mode now builds from topology generated geometry and the topology mesh path using `state.topologyMap`. The old polygon compatibility map is not used for editor preview rendering, while the old polygon preview overload remains in place for legacy callers.

Preview texture requests now come from `state.topologyMap.texturesById`. Topology preview stores CPU-side generated surface triangles for 3D picking and highlight overlays, and clears that CPU-side geometry on rebuild failure or shutdown so stale surface refs cannot be picked.

3D picking maps floor and ceiling surfaces to topology sector selection. Wall, lower-wall, and upper-wall surfaces map to topology sidedef selection with the matching wall part, preserving the generated stable sector, linedef, sidedef, and side IDs. Equal-height portals remain 2D-selectable only because they intentionally generate no 3D wall mesh.

The preview preserves the existing camera controls, entry/exit flow, and F11 cursor/mouselook behavior. Topology preview renders without lightmaps; lightmap baking and topology lightmap layout remain deferred.

## Phase 13: topology Insert Sector Inside

The selected topology sector inspector now exposes Insert Sector Inside near the top of the sector panel. Pressing it starts the existing pending polygon draw workflow for the selected topology parent sector, with the usual left-click, first-point/Enter finalize, Backspace, Escape, and right-click behavior.

Finalizing the insert creates one child topology sector in `state.topologyMap`. The child boundary uses one linedef per inserted edge, with the child owning the front sidedef and the parent owning the back sidedef. Parent holes are implied by those parent back-sidedefs around the child boundary; no `sector.holes` array exists in the topology model.

The child initially copies the parent sector fields, including heights, floor/ceiling textures and UVs, ambient settings, and default wall/lower/upper settings. The child/front and parent/back sidedefs are independent concrete values initialized from their owning sector defaults, so later sidedef edits do not affect the opposite side.

Insert validation is strict and transactional. The inserted polygon must be wholly inside the selected parent sector's usable area, outside existing parent holes, and disjoint from existing topology. Insert boundaries are not auto-split, auto-merged, or reused in this phase; touching, crossing, partial overlap, and exact matches with existing topology are rejected.

Nested inserts work naturally by selecting a child sector and inserting inside it. Generated topology geometry sees parent holes through `ExtractSectorTopologyLoops()`, so raised/lowered inserted children produce platform, pit, and riser behavior through the existing topology preview path.

Move, Erase, Split Linedef, sector deletion, vertex dragging, Light placement, lightmap baking, and topology lightmap layout remain deferred.

## Phase 14: topology Move tool vertex dragging

The existing Move tool now edits topology vertices directly by stable vertex ID. Dragging moves exactly one `SectorTopologyVertex`; connected linedefs and every sector loop that references those linedefs update through the shared vertex reference instead of through coordinate-group edits or duplicate vertex creation.

Vertex movement previews in the 2D editor without mutating the live topology document during drag. On release, the editor commits through a reusable transactional topology helper that copies the candidate map, changes only the target vertex coordinate, validates with `ValidateSectorTopologyMap()`, and replaces the live map only on success. Moves that would place a vertex exactly on another existing vertex are rejected because merge support remains a separate deferred operation.

Invalid moves that create collapsed edges, crossings, overlaps, invalid touches, or invalid sector/hole loops are rejected transactionally and leave the original topology unchanged. Vertex merge, split, delete, a persistent vertex inspector, 3D vertex movement, Erase, Light placement, Split Linedef, sector deletion, lightmap baking, and topology lightmap layout remain deferred.

## Phase 15: topology Split Linedef

The topology sidedef/linedef inspector now exposes Split Linedef near the top when a topology sidedef or line-only linedef is selected. Splitting is inspector-driven only; there is no toolbar mode, arbitrary click-to-split behavior, or 3D line splitting in this phase.

Splitting creates one exact midpoint vertex and replaces the original linedef with two new linedefs from A to midpoint and midpoint to B. The original linedef and its original sidedefs are removed, and existing front/back sidedefs are duplicated onto both replacement lines with fresh stable IDs.

Duplicated sidedefs preserve their original sector IDs, side kind, and wall/lower/upper texture and UV settings independently. Front sidedefs remain front sidedefs on both replacement lines, and back sidedefs remain back sidedefs; side ownership is not reinterpreted from geometry.

Midpoints must be exactly representable on the integer topology coordinate grid. If either endpoint coordinate sum is odd, the split is rejected without rounding, without half-coordinate floats, and without mutating the map.

After a successful split, selection moves to the second replacement line. Sidedef selection preserves the selected wall/lower/upper part and selects the corresponding duplicated sidedef on the second half; line-only selection selects the second line.

Erase, Delete, Light placement, lightmaps, vertex merge/delete, arbitrary line cutting, automatic overlap splitting, and 3D line splitting remain deferred.

## Phase 16: topology sector deletion

Topology sector deletion now works through a reusable transactional edit helper. Deleting a sector removes sidedefs that reference that sector, clears those sidedef slots from linedefs, removes linedefs that no longer have either side, removes vertices no remaining linedef references, validates the candidate topology, and commits only when validation succeeds.

The editor routes selected topology sector deletion through the existing confirmation dialog before mutating the document. The Delete key, the topology sector inspector's Delete Sector button, and simple Erase-tool sector clicks all open the same destructive confirmation flow.

Surviving opposite sides on shared linedefs are preserved with their existing material and UV settings and become one-sided boundaries. Deleting an inserted child sector leaves the parent's former-hole boundary as one-sided topology rather than healing the hole back into a solid floor. Deleting a parent sector is non-cascading when the remaining child topology validates.

Direct linedef deletion, direct sidedef deletion, standalone vertex deletion, vertex merge, Light placement, lightmap baking/topology lightmap layout, and broader Erase tool behavior remain deferred.

## Phase 16.5: topology static light placement and persistence

Topology documents now own static point lights directly on `SectorTopologyMap`.
The old polygon map's static light list remains compatibility-only and is no
longer used as editor document light data.

The Light tool places topology static lights at the clicked snapped X/Z map
position, using the clicked topology sector floor plus the existing old-editor
default light height for Y. Topology light positions, radius, and source radius
are stored and serialized in the same authoring-coordinate convention used by
the old static light editor; topology v2 JSON does not mix in world-space
converted values.

The 2D viewport renders topology lights from the topology document, including
the center marker, radius overlay, selected/hovered highlight, and source-radius
overlay. The Select tool can pick topology lights before linedefs/sidedefs and
sectors, and the Move tool can drag topology lights by X/Z while preserving Y.

The existing inspector can edit selected topology light position, radius,
source radius, intensity, and color. Topology v2 JSON persists lights in a
deterministic `staticLights` array with stable positive integer IDs, and strict
load validation rejects malformed or duplicate light records transactionally.

Lightmap baking, topology lightmap layout, bake source hashing, and bake version
changes remain deferred to Phase 17. Pressing Bake Lightmaps still reports that
topology lightmap baking is not migrated yet.

## Phase 17: topology lightmap baking

The editor Bake Lightmaps workflow now consumes `state.topologyMap` instead of
the old polygon compatibility map. Topology baking routes through topology
generated geometry, topology static lights, topology lightmap settings, and
topology baked-lightmap metadata while preserving the existing atlas packing,
BVH, ambient occlusion, bounce lighting, progress, cancellation, and PNG output
layout.

Topology v2 JSON persists lightmap settings and baked-lightmap metadata as
top-level fields. Older v2 topology files without those fields still load with
default bake settings and no baked lightmap.

Topology lightmap source hashes are deterministic over topology IDs and sorted
records: coord subdivisions, texture definitions, vertices, linedefs, sidedefs,
sectors, bake settings, and static light position/color/intensity/radius/source
radius. The hash does not rely on `unordered_map` iteration order or transient
editor state.

Generated surface refs and logical self-surface comparison now use topology
sector, linedef, sidedef, and side IDs for topology bakes. Floors and ceilings
compare by kind and topology sector ID; wall-like surfaces compare by kind,
topology sector ID, linedef ID, sidedef ID, and side. Debug labels report
readable topology IDs such as `Floor sector=3` and
`Wall sector=3 line=12 sideDef=18 front`.

The sector lightmap bake version was bumped to account for topology lightmap
identity and topology static lights. Old polygon-baked metadata is therefore
stale/incompatible for topology documents. Equal-height two-sided portals still
emit no wall mesh and intentionally produce no lightmap chart.

The old polygon lightmap APIs and polygon model remain in place for later
cleanup. The editor document workflow no longer uses the old polygon lightmap
path.

## Phase 17.5: topology 3D surface texture and UV panel

The existing compact 3D-mode surface texture/UV panel now works with topology
preview selections. The panel uses the generated 3D pick surface ref to keep an
explicit topology edit target, so floor and ceiling selections edit the selected
topology sector's `floorTextureId`/`floorUv` or `ceilingTextureId`/`ceilingUv`,
while wall, lower, and upper selections edit the selected topology sidedef's
concrete `wall`, `lower`, or `upper` texture and UV settings.

The 3D Texture button reuses the topology texture picker and draws choices from
`topologyMap.texturesById`. Texture picks opened from the 3D panel update only
the selected surface target and rebuild the 3D preview while preserving the
preview pose and F11 mouse-look state. UV numeric edits follow the existing UI
commit behavior: raw keystrokes stay local to the input field, and topology data
plus preview meshes update only after a valid committed value. Reset UV restores
scale to `(1, 1)` and offset to `(0, 0)`, preserves the texture ID, and affects
only the selected target.

Equal-height two-sided portals remain 2D-only for material editing because they
emit no 3D wall surface to pick. The old polygon 3D edit and split-edge workflow
remains deferred and unused for topology documents until final cleanup.
