# Sector Topology Incremental Refactor

## Phase 1

The Doom-like topology model now exists beside the original polygon-sector model. It introduces first-class vertices, linedefs, sidedefs, and sectors with stable positive integer IDs. The existing `SectorMap`, editor workflow, renderer, generated geometry, save/load format, and lightmap pipeline are unchanged and still use the polygon-sector model.

Planar topology coordinates use exact `int32_t` values with 16 stored subdivisions per visible authoring unit. Heights remain floating-point authoring values. Conversion to floating-point world coordinates happens at rendering or generated-geometry boundaries, using the existing sector world scale.

A linedef is directed from its start vertex to its end vertex. Its front side follows start to end, its back side follows end to start, and the owning sector lies to the left of either directed side.

Later phases will add validation and loop extraction, persistence, generated geometry, and editor integration incrementally. No topology UI or legacy conversion is part of Phase 1.
