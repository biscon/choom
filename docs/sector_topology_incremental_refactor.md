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
