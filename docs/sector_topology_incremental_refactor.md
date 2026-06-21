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
