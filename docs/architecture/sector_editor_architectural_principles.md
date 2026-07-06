# Sector Editor Architectural Principles

## Purpose

This document defines the intended architecture for future sector editor work.
Read it before writing Codex tasks, planning refactors, or implementing sector
editor features.

This is an architecture contract, not a style guide. When existing code
conflicts with this document, call out the conflict explicitly instead of
copying the old pattern forward.

## Core Source-of-Truth Model

- Editable maps require authoring graph data.
- The authoring graph is the source of truth for user-authored map geometry and
  editable map properties.
- `SectorTopologyMap` is not an editable map model.
- `SectorTopologyMap` is derived/compiled output for renderer, preview,
  collision, runtime, lightmap, hit-testing, and display.
- No-authoring topology maps are not a supported editor mode.
- Import/migration may read old topology-v2 data only to produce authoring graph
  data.
- After load/import, editor UI actions must edit authoring graph data or fail
  clearly.

## Authoring Graph Owns

The authoring graph owns normal user-editable map state, including:

- editable lines/boundaries
- line splits that create independently editable elements
- portal/line flags such as Blocks Player
- sectors/faces/face anchors
- sector names and editable sector properties
- floor/ceiling heights
- ceiling sky flag
- sector ambient
- wall/lower/upper/middle material settings
- floor/ceiling/default material settings
- UV scale/offset
- decal settings
- any future editable map property exposed in normal editor UI

## Topology Map Owns

`SectorTopologyMap` may own derived/editor-runtime state, including:

- derived linedefs/sidedefs/sectors/surfaces
- derived geometry used by renderer/preview/collision/lightmaps
- derived IDs and mappings back to authoring graph
- render/cache/bake/runtime outputs
- texture registry / map-level asset dictionary, while it remains intentionally
  global map metadata rather than authoring geometry
- lights/runtime objects only if they are intentionally topology-map-owned for
  now and tracked as future design decisions
- imported topology data only as transient input before conversion to authoring
  graph

Topology ownership does not include normal editor mutations of geometry, sector
properties, line flags, or material data.

## Editing Rules

- Editor tools edit authoring graph data.
- Editor UI must not silently mutate `SectorTopologyMap` when authoring data is
  missing.
- If authoring graph data is missing, normal editor actions fail clearly.
- If derivation mapping is missing, actions fail clearly and the mapping gap is
  a bug.
- Do not add no-authoring topology fallback behavior.
- Do not preserve old topology-editing mode.
- Do not classify editable map properties as topology-owned state to avoid
  adding authoring fields.
- Direct topology mutation is allowed only for derivation, runtime/cache/bake
  outputs, explicit import/migration code, or intentionally topology-owned
  global state.

## Derived Topology Read Rules

Reading derived topology is expected for:

- hit testing
- selection display
- preview surface picking
- render/cache/lightmap/collision input
- measuring geometry for fit/align
- mapping selected derived surfaces back to authoring targets
- status labels and inspector display

After deriving a target from topology, edits must write authoring data.

## Editable Identity Rule

Any geometry subdivision that creates independently selectable/editable editor
elements must happen in the authoring graph before topology derivation.

Examples:

- Line tool should split authoring graph lines, not rely on post-derivation
  topology splits.
- Rectangle or future shape tools that cross existing lines must update/split
  authoring graph data if the result should be editable.
- Topology-only splits are render/runtime implementation details, not editor
  identity.

## Material Editing Rules

- `MaterialEditingService` owns shared material editing behavior.
- Material edits must prefer authoring material targets.
- Derived topology may be read to identify selected side/surface and compute
  fit/align values.
- Do not mutate `state.topologyMap` and copy the result back into authoring
  graph.
- Topology scratch/writeback routes are transitional bugs unless explicitly
  isolated and scheduled for removal.
- `TexturePickerService` remains generic picker lifecycle/result machinery.
- `MaterialEditingService` owns material-specific picker semantics.
- Door, sky, sprite, add-map texture/import behavior must not be absorbed into
  `MaterialEditingService`.

## Service Rules

`SectorEditor`:

- composes services
- owns high-level lifecycle/routing
- owns preview renderer/collision/camera/document/lightmap orchestration until
  specific services are created

Services:

- own shared editor capabilities and real behavior
- are below both `SectorEditor` and tools/panels
- do not include `SectorEditor.h`
- do not call `SectorEditor::` methods
- should be passed to tools/panels directly when they need the capability

Tools/panels:

- depend on services directly
- must not call back into `SectorEditor` for normal service-owned operations
- should not receive one `std::function` callback per operation

## Callback Bridge Rule

- Callback bridges are allowed only as short-lived migration seams.
- Do not create new broad callback bundles for material, picker, selection, or
  editing operations.
- Do not replace direct service usage with callback-to-`SectorEditor`-to-service
  funnels.
- If a temporary callback remains, it must be listed in the task final report
  and backlog as cleanup debt.

Bad:

```text
tool -> callbacks.applyDecalOpacity -> SectorEditor -> MaterialEditingService
```

Good:

```text
tool -> MaterialEditingService.ApplyDecalOpacity
```

## Naming Rules

- Function names must not imply topology is editable when the operation writes
  authoring data.
- Avoid normal editor-facing names like `OpenTopologyTexturePicker` or
  `SetLineDefBlocksPlayer` when they hide authoring-owned behavior.
- Prefer names that distinguish derived target reads from authoring writes,
  such as:
  - `OpenMaterialPickerForDerivedSideDef`
  - `SetAuthoringLineDefBlocksPlayer`
  - `ApplyAuthoringSideMaterial...`
- Names containing `Legacy`, `TopologyOnly`, `Import`, or `Migration` must not
  be used by normal editor UI paths.

## Accepted Direct Topology Writes

Acceptable direct `SectorTopologyMap` write categories are:

- authoring graph derivation output
- cache invalidation/rebuild data
- bake result metadata
- runtime/preview outputs
- texture registry/map asset dictionary if intentionally global
- lights/runtime objects if currently intentionally topology-owned
- import/migration conversion code

Every accepted category should be clear from naming and ownership.

## Disallowed Patterns

- mutating `state.topologyMap` in editor UI because authoring data is missing
- topology mutation followed by copying payload back to authoring graph
- topology-only editor fallback paths
- adding a broad `TopologyService` dumping ground
- callback bundles that just bounce into `SectorEditor`
- services that secretly depend on `SectorEditor`
- deriving editor-editable identity only in topology
- reclassifying editable map properties as topology-owned to avoid authoring
  model work

## Future Codex Task Requirements

Future sector editor Codex tasks should include:

- Read `docs/architecture/sector_editor_architectural_principles.md` before
  making changes.
- If the requested change conflicts with the principles, stop and report the
  conflict.
- If code currently violates the principles, do not copy the violation into new
  code.
- Prefer fixing the violation locally when it is in scope.
- Any temporary exception must be named, documented, and added to backlog
  cleanup.

Copy/paste snippet for future tasks:

```text
Before editing code, read and obey:
docs/architecture/sector_editor_architectural_principles.md

This document is an architecture contract. If this task conflicts with it, stop and report the conflict instead of implementing around it.

Authoring graph is the source of truth. SectorTopologyMap is derived output, not an editable model. Normal editor UI must edit authoring graph data or fail clearly.
```

## Current Known Transitional Debt

- REF-044: remaining material scratch/writeback routes should become direct
  authoring material edits.
- REF-069/direct topology inventory: no-authoring fallback remnants should stay
  removed; any newly found remnants are bugs.
- REF-060/REF-066: Preview UV/material panel extraction is complete; remaining
  debt is limited to preview surface material/service boundary follow-up if
  callbacks grow.
- REF-045: lights/runtime object topology ownership remains a future design
  decision.
- REF-062: lightmap bake controller extraction remains future work.

References:

- `docs/audit/sector_editor_shared_service_inventory.md`
- `docs/audit/sector_editor_material_edit_bridge_audit.md`
- `docs/audit/sector_editor_direct_topology_edit_inventory.md`
- `docs/plans/codebase_refactor_backlog.md`
