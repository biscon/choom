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
