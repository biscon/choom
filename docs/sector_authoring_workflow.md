# Sector Authoring Workflow

The sector editor is now graph-authoritative. The editable document source is
`AuthoringGraph`; `SectorTopologyMap` is the strict derived/runtime topology
used by preview, mesh generation, collision, lightmap layout/bake, and runtime
consumers.

## Source And Derived Data

`AuthoringGraph` stores permissive editor data:

- authoring vertices with stable positive IDs and exact `SectorCoord`
  positions
- authoring lines with stable IDs, directed endpoints, line flags, and future
  special metadata
- authoring sides identified by `(lineId, side)` for wall/lower/upper/middle
  material data
- face anchors for room/sector-like properties such as heights, floor/ceiling
  materials, sky, ambient light, and default wall materials

Map-level data remains map-level: texture definitions, static lights, preview
settings, sky visual settings, directional light settings, bake settings, and
lightmap metadata.

`SectorTopologyMap` is generated from the graph. A successful derivation creates
normal topology vertices, linedefs, sidedefs, sectors, and mapping records back
to authoring IDs. Runtime-facing systems should keep consuming only the current
valid derived topology. Normal editor tools must not mutate derived topology
directly in graph-authoritative mode.

## Graph-Native Saves

Graph-native levels use `formatVersion: 3` and `topology: "authoringGraph"`.
The saved source is `authoringGraph` plus map-level data. Derived topology is
regenerated on load or after graph edits instead of being treated as persisted
truth.

Invalid work-in-progress graph states are saveable. Loading an invalid graph
keeps the authoring data available for 2D editing and diagnostics, but runtime
features remain gated until derivation produces a valid `SectorTopologyMap`.
Topology-v2 import exists as a compatibility/dev bootstrap path for old valid
maps; it is not the normal source model for new editing.

## Derivation And Diagnostics

Derivation is the bridge from permissive authoring data to strict runtime
topology. The pipeline planarizes authoring lines, extracts closed faces,
resolves face anchors, projects line/side/face properties, validates the derived
`SectorTopologyMap`, and records authoring-to-derived mapping.

Normal crossings, T-junctions, and endpoint-on-segment cases are auto-split
during derivation. The persisted authoring graph can stay permissive while the
derived planar graph is normalized.

The editor diagnoses cases that should not be guessed silently, including
duplicate lines, collinear overlaps, zero-length lines, near-misses, tiny sliver
faces, unresolved or ambiguous face anchors, invalid references, and property
projection conflicts. A failed derivation must not replace the current valid
derived topology.

## Editing Workflow

Use authoring tools for normal geometry edits:

- draw authoring lines to create boundaries, cuts, nested loops, and portal
  candidates
- select authoring lines, vertices, and face anchors in the existing 2D editor
- move authoring vertices instead of moving derived topology vertices
- delete authoring lines or safe unconnected authoring vertices
- edit room properties on face anchors
- edit wall/lower/upper/middle materials on authoring sides
- edit line flags such as `blocksPlayer` on authoring lines

Old direct-topology actions have been retired, demoted, or made legacy-only in
graph-authoritative mode. Draw authoring lines instead of using the old closed
Sector polygon tool, draw nested loops instead of `Insert Sector Inside`, draw
cut lines instead of `Cut Sector`, and rely on derivation auto-splitting instead
of direct topology linedef split tools.

## Preview, Bake, And Stale State

3D preview and lightmap bake require a current valid derived topology. If the
authoring graph is invalid, stale, or missing usable authoring-to-derived
mapping, those workflows should report that the derived topology is not current
instead of running against invalid graph data.

Last-valid derived topology is memory-only. It may help the 2D editor keep
context while the graph is temporarily invalid, but it is not persisted and must
not be presented as the current runtime result.

Lightmaps are tied to the derived `SectorTopologyMap` source hash. Derivation
changes that affect generated geometry, hash-sensitive topology IDs,
`ceilingSky`, directional light settings, static lights, bake settings, or
bake-relevant materials should stale or clear baked metadata. Visual-only preview
settings and sky visual settings remain outside the source hash.

Authoring graph mutations that change live topology or visible cached 2D editor
state should go through the authoring edit/refresh path so document-edited state,
derivation state, and the 2D topology render cache are invalidated together.

## Manual Smoke Suggestions

These checks are for a human running the editor, not for automated Codex GUI
verification:

- draw authoring lines that close a simple room and confirm derived fills appear
- draw crossing or T-junction lines and confirm derivation auto-splits them
- create an invalid loose/open graph, save, reload, and confirm diagnostics
  persist while 3D preview is unavailable
- edit a face anchor floor/ceiling/material value and confirm preview reflects
  the derived result after valid derivation
- edit a wall texture or `blocksPlayer` on an authoring side/line and confirm
  the 3D surface or gameplay collision reflects it after derivation
- confirm retired direct-topology buttons are unavailable or clearly marked
  legacy-only in graph-authoritative mode
