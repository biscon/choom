# Sector Authoring Graph Transition Plan

This is a runner-compatible implementation plan for moving the sector editor from direct closed-topology editing toward a permissive Doom-editor-like authoring graph.

The high-level design is already decided in the topology audit/design docs.

This plan should be committed as a normal repo document before execution.

Suggested path:

`docs/sector_authoring_graph_transition_plan.md`

```plan-state-json
{
  "plan_id": "sector_authoring_graph_transition_plan",
  "items": [
    {
      "id": "phase_01_authoring_graph_model",
      "title": "Add AuthoringGraph data model only",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_02_topology_v2_import",
      "title": "Add topology-v2 to AuthoringGraph import",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_03_planarization_autosplit",
      "title": "Add authoring graph planarization and auto-split",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_04_face_extraction",
      "title": "Add closed face extraction from planar graph",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_05_basic_topology_derivation",
      "title": "Derive basic SectorTopologyMap from authoring graph",
      "type": "phase",
      "status": "Completed",
      "parent": null
    },
    {
      "id": "phase_06_diagnostics_and_mapping",
      "title": "Add derivation diagnostics and ID mapping",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_07_property_projection",
      "title": "Project authoring properties into derived topology",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_08_graph_native_save_load",
      "title": "Add graph-native save/load format",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_09_editor_state_integration",
      "title": "Integrate authoring graph and derivation status into editor state",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_10_authoring_overlay_rendering",
      "title": "Render authoring graph overlay and diagnostics in 2D",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_11_minimal_authoring_tools",
      "title": "Add minimal authoring line and vertex tools",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_12_preview_and_bake_gating",
      "title": "Gate preview and bake through current derived topology",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_13_inspector_porting",
      "title": "Port inspectors and material editing to authoring anchors",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_14_legacy_tool_retirement",
      "title": "Hide or demote legacy direct-topology tools",
      "type": "phase",
      "status": "Planned",
      "parent": null
    },
    {
      "id": "phase_15_cleanup_docs_and_tests",
      "title": "Cleanup, documentation, and final regression pass",
      "type": "phase",
      "status": "Planned",
      "parent": null
    }
  ]
}
```

## Global Runner Instructions

This plan is intentionally large.

If the selected phase is too broad to implement safely in one pass:

* do not partially implement a giant change
* split only the selected phase into focused child pass items
* keep the child items under the selected phase using `parent`
* mark the parent phase `In Progress`
* leave the new child items `Planned`
* stop after updating the plan
* do not modify unrelated future phases

When implementing a selected phase:

* read the topology audit/design docs listed below first
* implement only the selected phase
* do not jump ahead
* keep changes narrow
* prefer pure data/functions/tests before UI
* preserve existing runtime consumers unless the selected phase explicitly says otherwise
* avoid broad refactors
* use simple structs/functions and stable integer IDs
* avoid polymorphic architecture unless already required by nearby code
* do not add 3D doors
* do not implement a giant compatibility system for old saves
* do not preserve old topology-v2 save compatibility if it makes the graph design worse
* do not delete existing assets/test levels unless a selected phase explicitly says to
* do not rewrite mesh generation, collision, lightmaps, or rendering unless a selected phase explicitly requires a narrow integration change
* no fragile GUI automation tests
* no `xdotool`
* no screenshot-based tests
* no launching the game/editor as an interactive GUI test from Codex
* no tests that require manual permissions, desktop focus, window-manager behavior, GPU interaction, or privilege escalation
* all Codex-run verification must be through CMake builds, CTest, unit tests, pure helper tests, or small non-interactive test harnesses
* manual GUI smoke checks may be listed for the user, but Codex must not attempt to perform them

The source-of-truth rule for this whole plan:

* `AuthoringGraph` is the editor source of truth.
* `SectorTopologyMap` is strict derived/runtime topology.
* Runtime systems consume only a valid derived `SectorTopologyMap`.
* Normal editor tools must not mutate derived topology directly once graph-authoritative mode is active.

The existing editor UI rule for this whole plan:

* Keep the current editor application.
* Keep the current editor layout and interaction style unless a selected phase explicitly says otherwise.
* New tools must integrate into the existing 2D editor/tool/inspector workflow.
* New 2D authoring tools should use the existing mouse-driven editor conventions, grid snapping, viewport, status messages, panels, and tool-selection patterns.
* Do not create a replacement editor UI.
* Do not create a separate external editor app.
* Do not create a keyboard-only prototype editor.
* Do not replace the existing tools pane with an unrelated UI.
* Do not remove existing pickers, panels, inspectors, or preview workflow unless a selected phase explicitly retires or ports that specific feature.

## Required Knowledge Repository

Before every phase, read the relevant docs in:

* `docs/topology_audits/01_closed_polygon_editor_assumptions.md`
* `docs/topology_audits/02_runtime_topology_consumers.md`
* `docs/topology_audits/03_stable_ids_and_properties.md`
* `docs/topology_audits/04_authoring_graph_model.md`
* `docs/topology_audits/05_migration_path.md`
* `docs/topology_audits/06_authoring_graph_transition_design.md`

Treat `06_authoring_graph_transition_design.md` as the current architecture decision document.

Important decisions from that document:

* crossing lines auto-split during derivation
* normal segment crossings, T-junctions, and endpoint-on-segment cases are automatically planarized
* duplicate lines, collinear overlaps, near-misses, slivers, and ambiguous anchors become diagnostics instead of silent guessing
* old save compatibility is optional
* graph-native save format may be breaking
* last-valid derived topology is memory-only
* 3D preview requires current valid derivation
* stale derived 2D fills may be shown only if clearly marked
* lightmaps may stale on derivation
* no logical-surface lightmap hash redesign in this transition
* 3D doors are out of scope
* direct topology mutation tools must be hidden, retired, demoted, or reimplemented as graph operations once graph-authoritative mode is active
* inspectors and pickers should be ported to authoring anchors instead of replaced wholesale

## Phase 1: Add AuthoringGraph data model only

Selected item:

`phase_01_authoring_graph_model`

Goal:

Add the basic authoring graph data model without changing editor behavior, save/load behavior, preview, mesh generation, collision, or lightmaps.

The model should represent permissive editor source data, not strict runtime topology.

Likely new files may be placed near existing sector topology code, for example under `sources/sector_demo/`, but choose the location that best fits the existing project layout.

Conceptual model:

* `SectorAuthoringGraph`
* `SectorAuthoringVertex`
* `SectorAuthoringLine`
* `SectorAuthoringLineSide`
* `SectorAuthoringFaceAnchor`
* authoring graph validation/lookup helpers as needed
* ID allocation helpers for authoring vertices, lines, face anchors, and static graph-owned objects if needed

Data requirements:

* authoring vertices have stable positive IDs and exact `SectorCoord` grid coordinates
* authoring lines have stable positive IDs, start/end authoring vertex IDs, directed orientation, line flags, and future-special-friendly space if cheap
* authoring sides are initially identified by `(lineId, side)` rather than separate side IDs
* authoring sides can carry wall/lower/upper/middle material metadata without requiring a live sector ID
* face/room anchors carry sector-like properties and may temporarily fail to resolve to a derived sector
* map-level data stays map-level and should not be duplicated unnecessarily

Reuse existing material/property structs where practical instead of inventing parallel material types.

Do not connect this model to the editor yet except through compile/test visibility if needed.

Tests:

* construct empty graph
* allocate stable positive IDs
* add vertices and lines
* reject or diagnose invalid endpoints in helper tests if helpers exist
* verify `(lineId, side)` identity helpers if added
* verify default face anchor/side material values match current topology defaults where appropriate

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* authoring graph types exist
* basic tests pass
* no editor behavior changed
* no save/load behavior changed
* no runtime consumer changed
* selected plan item marked Completed

## Phase 2: Add topology-v2 to AuthoringGraph import

Selected item:

`phase_02_topology_v2_import`

Goal:

Add a one-way import from existing valid `SectorTopologyMap` into `SectorAuthoringGraph`.

This is primarily a bootstrap/dev convenience, not a long-term compatibility promise.

Import policy:

* topology vertices become authoring vertices with the same IDs and exact coordinates
* topology linedefs become authoring lines with the same IDs, orientation, endpoints, and `blocksPlayer`
* topology sidedefs become authoring side material metadata
* topology sectors become face/room anchors with sector-like properties
* textures, static lights, preview settings, sky settings, directional light, and bake settings remain map-level or are copied into whatever document container later owns them
* baked lightmaps may be ignored, cleared, or marked stale later; do not over-preserve them in this phase

Do not change normal file loading yet.

Do not require old files to start loading as graph-native saves yet.

Tests:

* import an empty or minimal valid topology map
* import a single sector square
* import two adjacent sectors sharing a linedef
* verify vertex/line IDs preserve obvious one-to-one identity
* verify line flags preserve `blocksPlayer`
* verify wall/lower/upper/middle side material metadata is copied
* verify sector/face anchor properties copy floor/ceiling, textures, ambient, ceiling sky, and defaults
* verify map-level properties are not accidentally lost if the import function handles them

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* import helper exists
* tests prove core topology data can seed an authoring graph
* no graph-native save/load added yet
* selected plan item marked Completed

## Phase 3: Add authoring graph planarization and auto-split

Selected item:

`phase_03_planarization_autosplit`

Goal:

Add a pure planarization step for authoring graph lines.

This phase should not yet derive sectors or runtime topology. It should produce normalized planar graph data suitable for later face extraction.

Required behavior:

* normal segment crossings auto-split
* T-junctions auto-split
* endpoint-on-segment cases auto-split
* inserted intersection vertices are deterministic enough for stable test expectations
* line metadata mapping is retained so later phases know which authoring line/side produced each planar edge
* duplicate lines, collinear overlapping lines, zero-length lines, near-miss almost-intersections, and unsupported coincident endpoint cases produce diagnostics rather than silent guessing

Important distinction:

The authored graph may remain permissive. Planarization may produce a normalized derived planar graph for derivation without necessarily mutating the persisted authoring graph in this phase.

Tests:

* two crossing lines split into four planar edge segments with one intersection vertex
* T-junction splits the target segment
* endpoint-on-segment splits the containing segment
* multiple crossings on one line produce ordered split segments
* no split for non-intersecting lines
* exact duplicate lines produce diagnostic
* collinear overlapping lines produce diagnostic
* zero-length line produces diagnostic
* near-miss behavior is deterministic and documented
* metadata mapping from authoring line to planar segments is preserved

If this phase is too broad, split it into child passes such as:

* geometry predicates and segment intersection helpers
* planarization result model
* simple crossing auto-split
* T-junction and endpoint-on-segment handling
* diagnostics and mapping

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* planarization is pure/testable
* auto-split cases are covered
* unsupported cases produce diagnostics
* no editor UI changed
* selected plan item marked Completed

## Phase 4: Add closed face extraction from planar graph

Selected item:

`phase_04_face_extraction`

Goal:

Add pure face extraction from the normalized planar graph produced by planarization.

This phase should still avoid editor UI integration.

Required behavior:

* build directed half-edge style traversal or equivalent from planar graph edges
* find closed bounded faces
* reject or ignore the unbounded outer face
* compute deterministic winding/orientation
* produce face boundaries as ordered loops/edges
* preserve mapping from planar edges back to authoring lines/sides where possible
* produce diagnostics for dangling edges, ambiguous graph topology, tiny sliver faces below threshold, and unsupported overlaps that survived planarization

Initial scope may focus on connected planar graphs and normal crossing-derived faces. If nested disconnected loops/holes are too broad, split the phase or document a diagnostic/deferred behavior, but do not fake support.

Tests:

* single square produces one bounded face
* two adjacent squares produce two bounded faces
* rectangle cut by a line produces two faces if the line connects boundaries
* overlapping/crossing freeform shape produces multiple bounded faces after planarization
* open chain produces diagnostics and no face for the open region
* dangling line attached to a valid square does not corrupt the square face
* outer face is not returned as a derived sector
* face ordering is deterministic enough for tests
* sliver threshold behavior is documented and tested if implemented

If too broad, split into child passes such as:

* directed edge/angle sorting
* basic face walk
* diagnostics for dangling/open edges
* deterministic face IDs/order
* connected graph cases
* nested/disconnected loop behavior

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* planar graph can produce closed bounded face results
* tests cover simple and crossing-derived cases
* unsupported graph shapes are diagnostic, not silently guessed
* no editor UI changed
* selected plan item marked Completed

## Phase 5: Derive basic SectorTopologyMap from authoring graph

Selected item:

`phase_05_basic_topology_derivation`

Goal:

Add the first pure authoring-graph-to-`SectorTopologyMap` derivation.

This phase should generate a strict validated `SectorTopologyMap` for simple derived faces with default or minimally projected properties.

Inputs:

* `SectorAuthoringGraph`
* planarization result
* face extraction result
* any map-level data needed to build a valid topology map

Outputs:

* valid `SectorTopologyMap`
* derivation success/failure result
* basic diagnostics
* basic mapping, even if incomplete

Required behavior:

* derive vertices from planar graph vertices
* derive linedefs from planar graph edges
* derive sidedefs for face boundaries
* derive sectors from closed bounded faces
* create one-sided and two-sided linedefs as appropriate
* preserve/assign front/back semantics consistently with existing topology conventions
* validate result with `ValidateSectorTopologyMap()`
* produce no loose lines in derived topology
* fail cleanly with diagnostics instead of producing half-valid topology

Keep this phase minimal. Full property projection can come later.

Tests:

* one square authoring graph derives one valid sector
* two adjacent squares derive two valid sectors sharing a two-sided linedef
* a cut/crossing input derives multiple sectors after planarization
* open graph fails derivation with diagnostics
* duplicate/overlap diagnostics prevent invalid topology output
* derived topology passes existing validation
* generated geometry can build from derived topology for simple cases if that is cheap to test

If this phase is too broad, split it into child passes such as:

* derived vertex/linedef creation
* sidedef/front-back assignment
* sector creation from face loops
* topology validation integration
* basic derivation result object

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* simple authoring graphs derive valid `SectorTopologyMap`
* invalid graphs fail safely
* no runtime consumers rewritten
* selected plan item marked Completed

## Phase 6: Add derivation diagnostics and ID mapping

Selected item:

`phase_06_diagnostics_and_mapping`

Goal:

Make derivation results useful to the editor by adding explicit diagnostics and authoring-to-derived mapping.

Required diagnostics:

* dangling lines
* zero-length lines
* duplicate lines
* collinear overlapping lines
* near-miss almost-intersections if detected
* tiny sliver faces below threshold
* ambiguous face anchors
* unresolved face anchors
* invalid or missing side/property projection
* invalid portals or topology validation failures

Required mapping, as far as practical:

* authoring vertex ID to derived vertex ID(s)
* authoring line ID to derived linedef ID(s)
* `(authoringLineId, side)` to derived sidedef ID(s)
* face anchor ID to derived sector ID when unambiguous
* derived sector/linedef/sidedef back to authoring source where practical

Do not over-engineer perfect ID preservation for old topology-v2 files.

Derived IDs should be deterministic enough for tests and editor usability.

Tests:

* diagnostics include useful type/message/source IDs
* open/dangling graph reports dangling lines
* duplicate/overlapping lines report diagnostics
* planarized crossing maps one authoring line to multiple derived segments/linedefs
* simple face anchor maps to derived sector
* ambiguous or missing anchor reports diagnostic
* derivation failure does not replace successful derived output in any helper that tracks last valid state, if such helper exists in this phase

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* derivation emits useful diagnostics
* mapping exists for future UI/inspector integration
* tests cover mapping for split lines and face anchors
* selected plan item marked Completed

## Phase 7: Project authoring properties into derived topology

Selected item:

`phase_07_property_projection`

Goal:

Project authoring-side, authoring-line, and face-anchor properties into the derived `SectorTopologyMap`.

Required behavior:

* face anchors project sector-like properties to derived sectors
* authoring side material settings project to derived sidedefs
* authoring line flags project to derived linedefs
* line splits duplicate line and directed-side metadata onto derived child segments
* derived sectors receive floor/ceiling heights, textures, UVs, decals, ambient, `ceilingSky`, and default wall parts from anchors
* derived sidedefs receive wall/lower/upper/middle material settings from authoring sides
* `blocksPlayer` projects from authoring line flags
* map-level texture registry/static lights/preview settings/sky settings/directional light/bake settings stay map-level and are copied/owned by the document model later as needed

Conflict policy:

* face split: original anchor stays with face containing anchor seed/label point when available
* new faces clone properties or receive deterministic defaults
* face merge with one anchor keeps it
* face merge with multiple anchors reports diagnostic and does not silently guess
* unresolved anchors keep properties in the graph but do not project to derived topology until resolved

Tests:

* sector properties project to derived sector
* ceiling sky projects and affects derived sector field
* side material settings project to wall/lower/upper/middle
* middle texture settings survive projection
* line flag `blocksPlayer` projects
* split authoring line duplicates side material and line flags to derived child linedefs/sidedefs
* ambiguous multi-anchor face merge reports diagnostic
* unresolved anchor is preserved in authoring graph and reported

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* basic material/sector/line properties survive derivation
* conflict behavior is deterministic or diagnostic
* selected plan item marked Completed

## Phase 8: Add graph-native save/load format

Selected item:

`phase_08_graph_native_save_load`

Goal:

Add graph-native serialization for the new authoritative document model.

Backward compatibility is optional. Prefer a clean format over compatibility complexity.

Required behavior:

* persist `AuthoringGraph` as source of truth
* persist map-level data needed by the editor/runtime
* allow saving invalid graph states
* load graph-native files without requiring strict derived topology arrays
* regenerate derived topology on load when possible
* if derivation fails on load, still open the graph and report diagnostics
* do not persist `SectorTopologyMap` as primary truth
* do not require old `vertices`/`linedefs`/`sidedefs`/`sectors` arrays in new graph-native files
* bump root format/version if that is the cleanest option
* clear or stale baked lightmap metadata when derivation cannot prove it is current

Old topology-v2 import can be exposed through a dev/import path if cheap, but do not build a giant compatibility layer.

Tests:

* save/load empty authoring graph
* save/load graph with loose line
* save/load invalid graph with diagnostics after load
* save/load simple valid graph and regenerate derived topology
* save/load authoring side materials and face anchor properties
* save/load map-level texture registry, static lights, preview settings, sky settings, directional light, bake settings
* graph-native save does not require current strict topology arrays
* old topology-v2 import path still works if present
* failed graph load leaves output unchanged if that is the existing load contract for the new API

If this phase is too broad, split into:

* graph JSON schema/writer
* graph JSON parser
* map-level data persistence
* load-time derivation integration
* tests and sample file updates

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* graph-native files can persist invalid and valid authoring graphs
* load regenerates derived topology when possible
* save format is documented by tests
* selected plan item marked Completed

## Phase 9: Integrate authoring graph and derivation status into editor state

Selected item:

`phase_09_editor_state_integration`

Goal:

Add editor state fields and document flow for authoring graph, derivation status, last-valid derived topology, and mapping.

This phase should integrate behind existing UI as much as possible.

Required state concepts:

* current `SectorAuthoringGraph`
* current derivation result/status
* current valid derived `SectorTopologyMap`
* last-valid derived `SectorTopologyMap` memory-only if useful
* current derivation diagnostics
* authoring-to-derived mapping
* stale/current flags for derived topology
* invalid/no-derived state

Required behavior:

* editor document can hold authoring graph source data
* graph mutation marks document dirty and derived topology stale
* derivation success updates current derived topology
* derivation failure keeps graph editable and records diagnostics
* entering 3D preview should not yet switch behavior unless explicitly done in later phase
* legacy topology editing can remain active temporarily if needed

Do not port all tools yet.

Do not replace the existing editor layout or interaction model.

Tests:

* graph mutation marks document dirty/stale
* successful derivation updates derived topology/status
* failed derivation keeps graph and diagnostics
* last-valid derived topology is memory-only state, not persisted
* current existing editor tests still pass

If this phase is too broad, split into:

* state structs
* mutation/dirty/stale helpers
* derivation invocation helper
* document load/create integration
* tests

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Completion criteria:

* editor state can carry graph + derivation result
* stale/current/invalid states are represented
* no broad UI rewrite yet
* selected plan item marked Completed

## Phase 10: Render authoring graph overlay and diagnostics in 2D

Selected item:

`phase_10_authoring_overlay_rendering`

Goal:

Draw the authoring graph in the existing 2D editor separately from derived topology fills.

Required behavior:

* authoring vertices/lines are visible in the existing 2D editor viewport
* loose/dangling lines are visible
* invalid graph diagnostics are visible enough to debug
* derived sector fills may still render from current valid topology
* stale derived fills must be clearly indicated if shown
* authoring overlay should not require valid derived topology
* do not replace all picking/selection yet unless necessary
* keep existing grid, pan/zoom, snap, panels, and editor styling
* do not create a separate graph viewer or replacement editor

Layering principle:

* authoring graph overlay is source/editor data
* derived sector fills/labels are generated data
* diagnostics belong with authoring graph

Tests:

* helper/unit tests for render-cache records if available
* verify authoring overlay render-cache data can include loose lines and vertices
* verify diagnostic render-cache data can be built without valid derived topology
* verify render cache invalidation when graph changes
* existing 2D editor rendering/helper tests still pass
* do not use screenshot tests
* do not launch the GUI
* do not use `xdotool`

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestion, not for Codex:

* create loose lines
* see authoring overlay
* create invalid graph
* see diagnostics
* verify derived/stale fill state is understandable

Completion criteria:

* authoring graph is visible in existing 2D editor rendering path
* invalid graph can be seen/debugged
* stale derived topology is not visually misleading
* no replacement UI was created
* selected plan item marked Completed

## Phase 11: Add minimal authoring line and vertex tools

Selected item:

`phase_11_minimal_authoring_tools`

Goal:

Add minimal authoring graph editing tools inside the existing editor UI.

This is the first phase where normal editing begins to move from closed sector polygons to freeform graph lines.

Required minimal tools:

* draw authoring line segments with the mouse in the existing 2D editor viewport
* select authoring line
* select authoring vertex
* move authoring vertex with existing mouse/grid conventions
* delete authoring line
* delete authoring vertex when safe or with clear behavior
* snap to grid using existing sector grid conventions
* auto-derive after graph edits or provide a clear derive/update hook depending on existing editor flow
* integrate tool availability into the existing tools pane or existing editor tool-selection pattern

Important behavior:

* drawing lines can create loose/open/invalid graph state
* crossing lines do not need manual splits; derivation planarizes them
* do not mutate `SectorTopologyMap` directly
* derived topology updates only through derivation
* existing old closed-sector tool may remain temporarily but should not be mixed confusingly with graph tools
* do not implement this as a separate app
* do not implement this as a keyboard-only prototype
* do not replace the existing editor with a new UI
* do not remove existing picker/inspector infrastructure

Tests:

* draw/add line data helpers
* moving vertex updates connected authoring lines
* deleting a line updates graph and invalidates derivation
* deleting/moving vertex invalidates derivation
* graph edits mark document dirty
* derivation after crossing line creates expected derived faces if the derivation helper is invoked
* old topology tests still pass
* tests should exercise tool/controller helper logic where possible without launching the GUI
* do not use screenshot tests
* do not launch the GUI
* do not use `xdotool`

If UI implementation is too broad, split into child passes:

* authoring selection state
* draw line tool in existing 2D editor
* select/delete line in existing 2D editor
* select/move vertex in existing 2D editor
* derivation after edits
* existing tools pane integration

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestion, not for Codex:

* draw loose line
* draw crossing lines
* move vertex
* delete line
* see derived sectors update when valid

Completion criteria:

* minimal freeform line workflow exists inside the existing editor
* graph mutations do not directly edit derived topology
* no replacement UI was created
* selected plan item marked Completed

## Phase 12: Gate preview and bake through current derived topology

Selected item:

`phase_12_preview_and_bake_gating`

Goal:

Switch preview/bake entry points to use only current valid derived topology from the authoring graph flow.

Required behavior:

* 3D preview requires current valid derivation
* if graph is invalid or derived topology is stale, preview entry is disabled or reports a clear message
* 3D preview must not silently use stale topology as if it matched the current graph
* lightmap bake requires current valid derivation
* if derivation changes topology/hash-sensitive data, baked lightmap metadata is stale/cleared according to existing logic or a clear local rule
* runtime consumers still consume `SectorTopologyMap`, not `AuthoringGraph`

Do not rewrite mesh/collision/lightmap systems.

Tests:

* preview gate helper allows valid current derived topology
* preview gate helper rejects invalid/no-derived graph
* preview gate helper rejects stale derived topology
* bake gate helper requires current valid derivation
* existing preview/collision/lightmap tests still pass
* derivation failure does not crash preview/bake helper code
* do not launch the GUI
* do not use `xdotool`

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestion, not for Codex:

* valid graph enters preview
* invalid graph refuses preview with clear status
* stale graph refuses preview until re-derived

Completion criteria:

* preview/bake are graph-aware through current derived topology
* no runtime system consumes invalid graph data
* selected plan item marked Completed

## Phase 13: Port inspectors and material editing to authoring anchors

Selected item:

`phase_13_inspector_porting`

Goal:

Port important inspector/editor property writes so they mutate authoring anchors instead of derived topology.

Required ports:

* sector/floor/ceiling property inspector becomes face/room anchor inspector
* sidedef material inspector becomes authoring side inspector
* wall/lower/upper/middle material editing writes to authoring side metadata
* linedef flags such as `blocksPlayer` write to authoring line metadata
* texture picker remains reusable
* import texture workflow remains reusable
* preview settings remain map-level
* static light tools remain map-level unless code structure says otherwise
* 3D surface selection maps derived surface refs back to authoring side/face anchors before editing

Do not add 3D doors.

Important:

Do not allow inspector buttons to mutate derived topology directly once graph-authoritative mode is active.

Keep the existing inspector/picker UI style. Port the data target; do not replace the inspector system with an unrelated UI.

Tests:

* editing face anchor changes projected sector after derivation
* editing authoring side material changes projected sidedef after derivation
* editing `blocksPlayer` on authoring line changes derived linedef flag
* texture picker/material helper behavior preserved
* 3D selected surface can map to authoring anchor where mapping exists
* if mapping is missing/stale, inspector reports unavailable instead of mutating derived topology
* do not launch the GUI
* do not use `xdotool`

If too broad, split into child passes:

* face anchor inspector writes
* authoring side material writes
* line flag writes
* 3D surface ref to authoring mapping
* texture picker reuse/cleanup

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestion, not for Codex:

* edit floor/ceiling on derived face anchor
* edit wall texture
* edit blocksPlayer
* verify preview reflects derivation

Completion criteria:

* core material/sector/line property workflows write through authoring data
* derived topology is no longer the normal mutation target for those inspectors
* existing editor UI style is preserved
* selected plan item marked Completed

## Phase 14: Hide or demote legacy direct-topology tools

Selected item:

`phase_14_legacy_tool_retirement`

Goal:

Prevent legacy tools from mutating derived topology behind the authoring graph’s back.

Legacy tools to hide, disable, retire, or demote to explicit legacy/dev-only mode unless reimplemented as graph operations:

* closed polygon Sector draw tool
* Insert Sector Inside
* Cut Sector as direct topology operation
* Delete Sector as direct topology operation
* Split linedef at midpoint as direct topology operation
* Split linedef at point as direct topology operation
* Merge topology vertices as direct topology operation
* Dissolve topology vertex as direct topology operation

Graph equivalents should exist or be clearly planned:

* draw lines instead of draw sector polygon
* draw lines inside existing faces instead of Insert Sector Inside
* draw cut lines instead of Cut Sector
* auto-split derivation or explicit authoring-line split instead of direct linedef split
* authoring vertex merge/dissolve instead of topology vertex operations
* delete lines or face anchors instead of directly deleting derived sectors

Required behavior:

* in graph-authoritative mode, direct topology mutation buttons are not available as normal editing actions
* old tools do not silently corrupt source/derived consistency
* if a tool remains for dev/migration, label it clearly as legacy/dev-only
* status messages should explain why legacy direct-topology tools are unavailable
* do not replace the whole tools pane; update the existing tools pane/tool availability in the existing style

Tests:

* tool availability helper/state reflects graph-authoritative mode
* direct topology mutation commands are blocked/hidden where appropriate
* new graph tools remain available
* old tests adjusted only where the old behavior is intentionally retired
* no accidental derived-only mutation path remains in normal UI for listed tools
* do not launch the GUI
* do not use `xdotool`

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestion, not for Codex:

* legacy buttons gone/disabled in graph mode
* graph tools still usable
* no mixed-source-of-truth editing

Completion criteria:

* normal UI no longer exposes dangerous direct-topology mutation tools in graph-authoritative workflow
* old tools are retired, demoted, or reimplemented as graph operations
* existing editor style remains intact
* selected plan item marked Completed

## Phase 15: Cleanup, documentation, and final regression pass

Selected item:

`phase_15_cleanup_docs_and_tests`

Goal:

Clean up the transition and document the new architecture/workflow.

Required cleanup:

* remove dead legacy paths that are no longer used
* remove obsolete direct-topology UI code where safe
* keep dev/import helpers only if still useful and clearly named
* update docs to explain the source/derived split
* update docs to explain graph-native save behavior
* update docs to explain invalid graph states and preview gating
* update docs to explain which old tools were replaced
* ensure topology audit/design docs still reflect the implemented direction or add a short implementation note
* ensure tests cover the main workflow

Suggested docs:

* update or add an editor topology/authoring graph note under `docs/`
* mention `AuthoringGraph` source of truth
* mention `SectorTopologyMap` derived/runtime product
* mention auto-splitting crossings during derivation
* mention preview requires current valid derived topology
* mention old topology-v2 save compatibility is not guaranteed

Regression checklist:

* authoring graph model tests
* topology-v2 import tests if kept
* planarization tests
* face extraction tests
* derivation tests
* diagnostics/mapping tests
* save/load tests
* editor state tests
* overlay/tool helper tests
* inspector/property projection tests
* existing mesh/collision/lightmap/preview tests
* no GUI automation
* no `xdotool`
* no screenshot tests
* no interactive editor launch from Codex

Verification:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Manual user-only smoke suggestions may be documented, but Codex must not execute them.

Completion criteria:

* new workflow documented
* obsolete code removed or clearly marked
* regression suite passes
* manual smoke instructions documented for the user
* all plan items completed

## Final Plan Completion Criteria

The transition plan is complete when:

* `AuthoringGraph` is the editor source of truth
* graph-native save/load works
* invalid authoring graph states can be saved/reopened
* crossing lines auto-split during derivation
* derivation produces valid `SectorTopologyMap`
* runtime/preview/collision/lightmap systems consume derived topology
* 3D preview requires current valid derived topology
* core material/sector/line property editing writes through authoring anchors
* old direct-topology mutation tools are hidden, retired, demoted, or reimplemented as graph tools
* new authoring tools live inside the existing editor UI and interaction style
* no replacement editor UI was created
* no keyboard-only/editor-sidecar prototype was created
* no GUI automation tests were added
* no `xdotool` tests were added
* no 3D doors were added
* no giant backward compatibility system was added
* tests pass through CMake/CTest/non-interactive harnesses
* docs explain the new source/derived architecture
