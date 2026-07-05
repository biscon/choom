# SectorEditor Selection and Manipulation Contract

## Summary

Select must be split from selection and manipulation services because it is
already more than a tool. It currently owns click-to-select behavior, pick
candidate cycling, drag arming, and movement dispatch for authoring vertices,
lights, and placed runtime objects.

The proposed Selection service should own selected IDs, state transitions,
clear/select helpers, stale selection cleanup, and selected-target queries. The
proposed Manipulation service should own generic drag transaction lifecycle and
route begin/update/finish/cancel calls to feature-provided move providers. The
Select tool should own mouse behavior while Select is active: click-vs-drag
policy, pick cycling, and calling Selection/Manipulation services.

The first implementation task should be passive `SelectionTarget` and provider
type definitions only. That creates a shared vocabulary without migrating Select
or changing behavior.

## Current Problem

Select is not just another migrated authoring tool. In the 2D canvas it handles:

- click-to-select and click-empty-to-clear behavior
- pick candidate gathering and cycling
- drag arming for the currently selected movable target
- movement dispatch to authoring vertex, light, and runtime-object drag systems
- hover feedback for the same mixed target set

The current danger is visible in `SectorEditor::StartSelectDrag()`: Select
switches on `SectorEditorPickKind` and starts a different movement path for
runtime objects, lights, and authoring vertices. If Select is migrated directly
into `tools/select/` without a provider contract, that switch will grow as every
new selectable or movable primitive is added.

That would turn Select into a landfill:

```cpp
switch (selected.kind) {
    case Vertex: move vertex;
    case Sector: refuse move;
    case Billboard: move billboard;
    case Door: refuse move;
    case StaticLight: move light;
    case FutureObject: ...
}
```

Feature-provided capabilities are a better boundary. Select should gather and
choose pick candidates through picking/selection services, set selected targets
through a Selection service, and ask a Manipulation service to begin movement.
Feature modules should own their object-specific rules.

## Current Selection Inventory

| Target kind | Current selected id/state fields | Current pick/hover code | Current inspector code | Selectable | Movable | Movement behavior | Dirty/cache/runtime refresh implications | Likely provider owner |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Topology sector | `topologySelectionKind`, `selectedTopologySectorId`, optional `selectedSurface3D` / `selectedTopologySurface3D` | 3D surfaces via `PickSectorSurface3D()` / `SelectSurface3D()`; 2D sector hit via `FindTopologySectorAt()` for placement | `SectorEditorSectorInspector.*` | Yes | No | Edited by moving authoring/topology vertices, not by moving the sector | Sector edits route through topology/authoring mutation helpers; cache invalidated through `MarkTopologyDocumentEdited()` or `MarkSectorEditorAuthoringGraphEdited()` | topology/authoring shared |
| Topology vertex | `topologySelectionKind`, `selectedTopologyVertexId`, `inspectedTopologyVertexId` | Legacy hover/pick via `FindTopologyVertexNearScreenPoint()` outside current Select candidate list | `SectorEditorVertexInspector.*` | Yes | Not in graph-authoritative Select path | Legacy move tool is unavailable in graph-authoritative mode; current Select movement is authoring-vertex based | Direct topology vertex movement should preserve topology dirty/cache route if revived | topology shared |
| Topology linedef/sidedef | `topologySelectionKind`, `selectedTopologyLineDefId`, `selectedTopologySideDefId`, `selectedTopologySideKind`, `selectedTopologyWallPart` | 3D wall surfaces select sidedefs; 2D line proximity via `FindTopologyLineNearScreenPoint()` | local sidedef/material inspector in `SectorEditor.cpp` | Yes | No | Material/flag edits only; direct delete/move unavailable | Material edits use `FinishTopologyMaterialMutation()` -> dirty/cache and optional preview rebuild | topology/materials |
| Authoring graph vertex | `selectedAuthoring.kind == Vertex`, `selectedAuthoring.vertexId`, `authoringVertexDrag` | Select candidates via `FindAuthoringSelectionNearScreenPoint()` / `BuildSelectPickCandidates()` | local authoring vertex inspector in `SectorEditor.cpp` | Yes | Yes | Drag previews snapped point, then commits through `MoveSectorEditorAuthoringVertex()` | `MoveSectorEditorAuthoringVertex()` mutates `authoringGraph`, calls `MarkSectorEditorAuthoringGraphEdited()`, refreshes derivation, invalidates 2D cache | topology/authoring shared |
| Authoring graph line | `selectedAuthoring.kind == Line`, `selectedAuthoring.lineId` | Select candidates via `FindAuthoringSelectionNearScreenPoint()` | local authoring line/material inspector in `SectorEditor.cpp` | Yes | No | Select/delete/material edits; no direct move | Mutations route through authoring helpers and invalidate cache | topology/authoring shared |
| Authoring graph face anchor | `selectedAuthoring.kind == FaceAnchor`, `selectedAuthoring.faceAnchorId` | Select candidates via `FindAuthoringSelectionNearScreenPoint()` | local authoring face inspector in `SectorEditor.cpp` | Yes | No | Sector-like authoring/material edits; no direct move | Mutations route through authoring helpers and invalidate cache | topology/authoring shared |
| Billboard / placed object | `selectedRuntimeObjectId`, `runtimeObjectDrag` | Select candidates from `topologyMap.runtimeObjects` in `BuildSelectPickCandidates()` | `tools/placed_objects`, `tools/billboards` inspectors | Yes | Yes for non-door objects | Drag mutates `SectorPlacedRuntimeObject::position`; Y is preserved | During drag updates cached runtime-object draw; finish calls `MarkTopologyDocumentEdited()` and respawns runtime ECS objects | `tools/placed_objects` + `tools/billboards` |
| Door / placed object | `selectedRuntimeObjectId`, `runtimeObjectDrag` | Same runtime-object pick path | `tools/placed_objects`, `tools/doors` inspectors/modals | Yes | No | `StartSectorEditorPlacedObjectDrag()` refuses `kind == "door"` with status text | No mutation on refused move; door edits use placed-object mutation route | `tools/placed_objects` + `tools/doors` |
| Static point light | `topologySelectionKind == StaticLight`, `selectedTopologyLightId`, `lightDrag` | Select candidates from `topologyMap.staticLights`; hover field `hoveredTopologyLightId` | `SectorEditorLightInspector.*` | Yes | Yes | Drag mutates X/Z position, preserves Y | Finish calls `FinishMoveStaticLight()` then `FinishTopologyActionResult()` -> dirty/cache; static lights affect lightmap source hash | lights provider |
| Dynamic point light | `topologySelectionKind == DynamicLight`, `selectedTopologyDynamicLightId`, `lightDrag` | Select candidates from `topologyMap.dynamicPointLights` | `SectorEditorLightInspector.*` | Yes | Yes | Drag mutates X/Z position, preserves Y | Finish calls `FinishMoveDynamicLight()` then dirty/cache; preview dynamic-light refresh is separate from drag finish | lights provider |
| Static spotlight | `topologySelectionKind == StaticSpotLight`, `selectedTopologyStaticSpotLightId`, `lightDrag`, `SpotLightHandle` | Select candidates from `topologyMap.staticSpotLights`; handle pick via `FindTopologyStaticSpotLightHandleNearScreenPoint()` | `SectorEditorLightInspector.*` | Yes | Yes | Drag origin moves origin and target together; drag target aims target only | Finish marks changed when origin/target moved; dirty/cache invalidated; static spotlights affect lightmap source hash | lights provider |
| Dynamic spotlight | `topologySelectionKind == DynamicSpotLight`, `selectedTopologyDynamicSpotLightId`, `lightDrag`, `SpotLightHandle` | Select candidates from `topologyMap.dynamicSpotLights`; handle pick via `FindTopologyDynamicSpotLightHandleNearScreenPoint()` | `SectorEditorLightInspector.*` | Yes | Yes | Drag origin moves origin and target together; drag target aims target only | Finish marks dirty/cache on changes; runtime preview dynamic-light upload is handled elsewhere | lights provider |
| Preview surface | `hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D` | `UpdatePreview3DSelection()` uses `PickSectorSurface3D()` | surface/material panel in `SectorEditor.cpp` | Yes | No | Selects material target; no movement | Material edits can mark dirty/cache and rebuild preview meshes | topology/materials or preview-surface provider |

## Current Drag / Movement Inventory

| Movement system | Current functions | State touched | Target kind | Mutation route | Dirty/cache route | Runtime refresh route | Cancel behavior | Finish behavior | Risk |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Select drag arm/routing | `UpdateSelectDragArm()`, `ArmSelectedSelectDrag()`, `StartSelectDrag()`, `FindSelectedMovablePickTargetAtScreenPoint()` | `selectDragArm` plus selected target state | Runtime objects, lights, authoring vertices | Dispatch only | None directly | None directly | Clears arm if tool/mouse state changes | Starts one concrete drag system after drag threshold | High growth risk because it switches by target kind |
| Authoring vertex movement | `StartAuthoringVertexDrag()`, `UpdateAuthoringVertexDrag()`, `FinishAuthoringVertexDrag()`, `CancelAuthoringVertexDrag()` | `authoringVertexDrag`, `selectedAuthoring`, topology selection clear | Authoring vertex | `MoveSectorEditorAuthoringVertex()` mutates `authoringGraph` | `MarkSectorEditorAuthoringGraphEdited()` invalidates editor topology cache and marks dirty/stale | Derivation refresh happens in authoring helper; no ECS runtime object respawn | Clears drag preview; does not need rollback because mutation happens only on finish | Commits snapped preview point or rejects unchanged/invalid target | Medium; currently local to `SectorEditor.cpp` |
| Runtime/placed object drag | `StartRuntimeObjectDrag()`, `UpdateRuntimeObjectDrag()`, `FinishRuntimeObjectDrag()`, `CancelRuntimeObjectDrag()`, placed-object drag helpers | `runtimeObjectDrag`, selected runtime object | Runtime object / billboard; door refuses | Mutates `topologyMap.runtimeObjects[*].position` during drag | Drag updates cached runtime-object draw; finish calls `MarkTopologyDocumentEdited()` | Finish calls `RefreshRuntimeObjectsAfterAuthoringEdit()` to respawn runtime ECS objects | Restores original position and cached draw, reselects object | Marks unchanged or dirty; respawns runtime objects after changed move | Low/medium; already has a useful callback context |
| Light drag | `StartLightDrag()`, `UpdateLightDrag()`, `FinishLightDrag()`, `CancelLightDrag()` | `lightDrag`, light selected IDs | Static/dynamic point and spot lights | Mutates light arrays in `topologyMap` during drag | Finish uses `FinishTopologyActionResult()` -> `MarkTopologyDocumentEdited()` -> cache invalidation | No generic ECS respawn; dynamic preview light refresh is handled by other paths | Restores original position/target and reselects light | Marks unchanged or dirty; static point/dynamic point use topology action helpers | High; static lights are source-hash-sensitive |
| Preview surface selection | `UpdatePreview3DSelection()`, `PickSectorSurface3D()`, `SelectSurface3D()` | `hoveredSurface3D`, `selectedSurface3D`, topology/material selection | Preview floor/ceiling/wall surfaces | Selection only | Material mutations later use existing dirty/cache/preview rebuild paths | Preview mesh rebuild can happen after material mutation | Clears hover/selection when invalid | Selects sector or sidedef/material target | Medium; should not become a movement provider |

## Proposed Ownership Model

### Selection Service

The Selection service should own:

- selected IDs and state transitions for current selected targets
- clear/select helpers for topology, authoring, runtime object, light, and
  preview-surface selections
- stale selection cleanup
- selected target queries such as "current selected target" and "is selected"
- normalization between pick results and current selected state
- UI buffer reset hooks only while they are required to preserve behavior

It must not own:

- object-specific movement implementation
- feature-specific inspector UI
- feature-specific mutation details
- renderer implementation
- lightmap/source-hash policy

### Manipulation / Move Service

The Manipulation service should own:

- generic drag transaction lifecycle
- active manipulation state and target identity
- begin/update/finish/cancel dispatch
- route-to-provider lookup for a selected target
- common no-op and cannot-move handling
- generic status text for unsupported movement where no provider has a more
  specific message

It must not own:

- how to move each object internally
- topology authoring graph mutation details
- runtime object ECS respawn details
- lightmap/source-hash logic

### Select Tool

Select should own:

- mouse behavior while Select is active
- deciding whether input is a click selection or drag start
- pick candidate cycling policy
- initiating manipulation through the Manipulation service
- select-specific overlays, if any

Select must not own:

- object-specific movement implementation
- mutation logic for every selectable kind
- future primitive switch branches

## Provider / Capability Design

Recommended option: Option A, separate small capability providers.

```cpp
struct SectorEditorPickProvider {
    void (*collectPickCandidates)(PickContext&, PickCandidateList&);
};

struct SectorEditorSelectionProvider {
    bool (*select)(SelectionContext&, SelectionTarget);
    bool (*isStillValid)(SelectionContext&, SelectionTarget);
};

struct SectorEditorMoveProvider {
    bool (*canMove)(MoveContext&, SelectionTarget);
    bool (*beginMove)(MoveContext&, SelectionTarget, Vector2 mouse);
    void (*updateMove)(MoveContext&, Vector2 mouse);
    void (*finishMove)(MoveContext&);
    void (*cancelMove)(MoveContext&, const char* message);
};
```

Handlers may be null where a capability is unsupported. A selectable-only target
has pick/select support and no move provider, or a move provider whose
`canMove()` returns false with a specific status message.

Option B, one combined selectable provider, is acceptable for a small feature
folder but should not be the shared service contract because picking, selection,
movement, and inspector behavior have different lifecycles. Option C, manual
switches in Selection/Manipulation services, simply moves the landfill out of
Select. Option D, per-tool callbacks only, keeps ownership too ad hoc and makes
future cross-feature selection behavior harder to reason about.

## Target Kinds and Capabilities

There should be a unified passive `SelectionTarget` type. It should be a tagged
plain data struct, not an inheritance hierarchy.

Conceptual shape:

```cpp
enum class SelectionTargetKind {
    None,
    TopologySector,
    TopologyVertex,
    TopologyLineDef,
    TopologySideDef,
    AuthoringVertex,
    AuthoringLine,
    AuthoringFaceAnchor,
    RuntimeObject,
    StaticLight,
    StaticSpotLight,
    DynamicLight,
    DynamicSpotLight,
    PreviewSurface
};

struct SelectionTarget {
    SelectionTargetKind kind = SelectionTargetKind::None;
    int id = -1;
    int secondaryId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
    TopologyWallPart wallPart = TopologyWallPart::Wall;
    SpotLightHandle spotHandle = SpotLightHandle::Origin;
    SectorSurfaceKind surfaceKind = SectorSurfaceKind::None;
};
```

The first implementation should preserve current selected state fields and add
conversion helpers around them. During migration:

- existing `SectorEditorPickTarget` can map to `SelectionTarget` for Select
  candidates
- existing `SectorAuthoringSelectionTarget` can map to authoring target kinds
- topology selection fields can map to topology target kinds
- `SectorSurfaceRef` can map to `PreviewSurface` or topology material targets

Selectable-but-not-movable targets are represented by a valid `SelectionTarget`
with no move capability. Do not encode movability into the enum name.

## Feature Provider Ownership

- `tools/placed_objects`: common placed object selection/delete helpers and
  possible dispatch for object-kind-specific providers.
- `tools/billboards`: billboard pick/move/inspector provider; likely first move
  pilot because placed-object drag already has a context/callback boundary.
- `tools/doors`: door pick/select provider and no-move/refuse-move provider
  preserving the current status text.
- lights feature area: static/dynamic point and spot pick/move providers later;
  static lights and static spotlights need source-hash caution because moving
  them affects baked lighting inputs.
- `tools/select`: Select frontend only.
- `tools/line`, `tools/rectangle`, `tools/insert_vertex`: active authoring
  creation tools, not selection providers unless their created targets need
  shared pick/select/move participation.
- topology/authoring shared: sector, topology vertex, linedef/sidedef, authoring
  vertex, authoring line, and authoring face providers.
- materials/preview-surface area later: preview surface selection and material
  target handling, without movement.

## API / Context Boundaries

`SelectionContext` should expose:

- `SectorEditorState&` initially as transitional debt
- status text callback/reference
- narrow clear/select callbacks where current UI buffer reset behavior still
  lives in `SectorEditor`
- stale cleanup helpers

It should not expose `SectorEditor&`, renderer internals, or full mutation
services.

`PickContext` should expose:

- screen point, map point, zoom/view conversion callbacks
- read-only document state needed by providers
- candidate list append helpers
- current selected target for cycling if needed

It should not expose mutation callbacks.

`MoveContext` should expose:

- active editor state needed by the provider
- screen/map/snap callbacks
- Selection service access for reselecting/restoring current target
- dirty/cache callbacks: `markTopologyDocumentEdited`,
  authoring-graph-edited route, cached runtime-object draw update, runtime ECS
  refresh, preview rebuild if explicitly needed

It should not expose `SectorEditor&`, lightmap worker state, or renderer
resource ownership. Passing `SectorEditorState&` is acceptable during migration
but should be labelled transitional debt.

`SelectToolContext` should expose:

- input, canvas rect, current mouse positions
- pick service
- Selection service
- Manipulation service
- status text

It should not expose object-specific move functions.

Callbacks should be used first for document dirtying, cache invalidation,
runtime object refresh, and preview rebuild hooks. These can become shared
services later after the provider boundary is proven.

## Migration Plan

Phase 0: This report and backlog update.

- Goal: establish the contract.
- Files: this report and `docs/plans/codebase_refactor_backlog.md`.
- Risk: low.
- Verification: `git diff --check`, `git diff --stat`, `git status --short`.
- Manual smoke: none; docs only.
- Task type: audit/report.

Phase 1: Add passive `SelectionTarget` and provider type definitions.

- Goal: add shared vocabulary without behavior changes.
- Likely files: a new small selection/provider header or
  `SectorEditorSelectionTypes.h`.
- Risk: low/medium include churn.
- Verification: build, ctest, diff checks.
- Manual smoke: not required if no behavior changes.
- Task type: Codex task.
- REF-048 note: passive target/provider definitions were added in
  `sources/sector_editor/selection/SectorEditorSelectionTarget.h`; no behavior was
  migrated.

Phase 2: Extract Selection service helpers.

- Goal: centralize clear/select/query/stale cleanup while preserving existing
  state fields and UI reset behavior.
- Likely files: new selection service files plus `SectorEditor.cpp/.h`.
- Risk: medium because selection helpers reset multiple UI fields and cancel
  spotlight pilot/drag state.
- Verification: build, ctest, diff checks.
- Manual smoke: select sector, sidedef, authoring line/vertex/face, runtime
  object, all light kinds, clear with Escape.
- Task type: Codex task after Phase 1.
- REF-049 note: selection helpers were extracted into
  `sources/sector_editor/selection/SectorEditorSelectionService.h/.cpp`;
  current selected state fields remain the source of truth, and no Select
  migration or provider dispatch was performed.
- REF-050 note: a manipulation service shell was added in
  `sources/sector_editor/selection/SectorEditorManipulationService.h/.cpp`.
  Existing drag implementations remain the source of behavior, and provider
  dispatch remains future work.
- REF-051 note: the first move provider pilot was added for placed objects.
  Provider dispatch is now proven for one movement family, and Select migration
  remains future work.

Phase 3: Add Manipulation service shell.

- Goal: own active manipulation routing while delegating to existing drag
  functions.
- Likely files: new manipulation service files plus select input glue.
- Risk: medium because finish/cancel order must stay identical.
- Verification: build, ctest, diff checks.
- Manual smoke: drag authoring vertex, billboard, static/dynamic lights, cancel
  with right click/Escape.
- Task type: Codex task.

Phase 4: Add first move-provider pilot.

- Goal: migrate one movement family behind a provider.
- Recommended pilot: placed object/billboard movement, preserving door refusal.
- Risk: medium; runtime object cached draw and ECS respawn must stay intact.
- Verification: build, ctest, diff checks.
- Manual smoke: select/move billboard, cancel drag, unchanged click, attempt to
  drag door and confirm refusal status.
- Task type: Codex task.

Phase 5: Migrate Select tool to services/providers.

- Goal: move Select frontend into `tools/select/` without object-specific
  movement logic.
- Risk: high if done before service/provider seams.
- Verification: build, ctest, diff checks.
- Manual smoke: pick cycling, clear selection, hover, drag threshold, every
  existing movable/non-movable target.
- Task type: Codex task after service pilot.

Phase 6: Add remaining providers.

- Goal: migrate authoring vertex, lights, doors, sectors, topology surfaces, and
  remaining target kinds.
- Risk: medium/high for lights and preview surfaces.
- Verification: build, ctest, targeted source-hash tests for static light
  changes, manual editor smoke.
- Task type: split Codex tasks; audit first for source-hash-sensitive light
  expansion if scope grows.

## Recommended First Implementation Task

The first implementation task should add passive `SelectionTarget` and provider
type definitions only. It should not migrate Select, not change selection state,
and not route any movement through providers yet.

This is narrower than implementing a provider pilot first and safer than
extracting the Selection service immediately. It lets later tasks refer to one
target vocabulary while preserving existing state and behavior.

## Backlog Update

`docs/plans/codebase_refactor_backlog.md` should contain:

- `REF-047` marked complete for this report.
- `REF-043` left open.
- notes that `REF-043c` migrated Insert Vertex.
- notes that Select migration waits for Selection service and Manipulation
  provider contract.
- future unchecked implementation items for passive target/provider definitions,
  Selection service extraction, Manipulation service shell, first provider
  pilot, and Select migration.

## Appendix: Evidence

Static analysis only. No GUI/render smoke was performed.

Commands used:

```sh
rg -n "Select|Selection|selected|Selected|Pick|PickTarget|PickCandidate|Hover|hover|Start.*Drag|Update.*Drag|Finish.*Drag|Cancel.*Drag|Move|Movable|Drag" sources/sector_editor
rg -n "SelectTopology|SelectAuthoring|SelectRuntime|Select.*Light|ClearSelection|ClearStale|CurrentPickSelectionTarget|BuildSelectPickCandidates|SelectPickTarget|StartSelectDrag|ArmSelectedSelectDrag|UpdateSelectDragArm|FindSelectedMovablePickTargetAtScreenPoint" sources/sector_editor
rg -n "MarkTopologyDocumentEdited|MarkSectorEditorAuthoringGraphEdited|InvalidateTopologyRenderCache|RefreshRuntimeObjectsAfterAuthoringEdit|UpdateCachedRuntimeObjectDraw|RebuildSectorCollisionWorld|RefreshDynamicLightSources" sources/sector_editor
rg -n "SectorEditorSelection|SectorEditor.*Drag|SectorEditor.*Pick|RuntimeObjectDrag|LightDrag|Authoring.*Drag|Vertex.*Drag" sources/sector_editor
rg -n "FindTopologySectorAt|FindTopologyLineNearScreenPoint|FindTopologyVertexNearScreenPoint|FindTopology.*Light.*NearScreenPoint|FindAuthoringSelectionNearScreenPoint|PickSectorSurface3D|SelectSurface3D|UpdatePreview3DSelection" sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditor.h
```

Relevant functions and ranges:

- `SectorEditorSelectionTypes.h:31-143`: selection, pick, drag passive state.
- `SectorEditorTypes.h:60-132`: current selected and drag fields in
  `SectorEditorState`.
- `SectorEditor.cpp:801-925`: hover routing, including Select hover candidate
  handling.
- `SectorEditor.cpp:927-1225`: canvas input, Escape/Delete, active drag update,
  click selection, and tool routing.
- `SectorEditor.cpp:1252-1519`: Select drag arm, current pick target, pick
  candidate build, select target, movable selected hit test.
- `SectorEditor.cpp:1526-1619`: authoring vertex drag lifecycle.
- `SectorEditor.cpp:1621-1919`: light drag lifecycle.
- `SectorEditor.cpp:1923-1944`: runtime object drag wrappers.
- `SectorEditor.cpp:2499-2569`: stale topology/runtime selection cleanup.
- `SectorEditor.cpp:2571-2579`: `MarkTopologyDocumentEdited()` dirty/cache route.
- `SectorEditor.cpp:3070-3092`: runtime object drag context callbacks.
- `SectorEditor.cpp:4915-4931`: topology render cache invalidation/rebuild.
- `SectorEditor.cpp:9270-9634`: topology, light, runtime object, and authoring
  pick helpers.
- `SectorEditor.cpp:9635-10203`: select/clear helpers for topology, runtime,
  surfaces, and authoring selections.
- `SectorEditorAuthoringState.cpp:1438-1461`: authoring vertex mutation route.
- `SectorEditorAuthoringState.cpp:1559-1574`: authoring dirty/cache route.
- `SectorEditorTopologyActions.cpp:384-426`: static/dynamic light finish-move
  action helpers.
- `tools/placed_objects/SectorEditorPlacedObjectDrag.cpp:12-133`: placed-object
  drag lifecycle and door movement refusal.

Current selectable target inventory:

- topology sectors, vertices, sidedefs, linedefs
- authoring vertices, lines, face anchors
- placed runtime objects including billboards and doors
- static point lights, dynamic point lights, static spotlights, dynamic
  spotlights
- preview floor/ceiling/wall surfaces for material editing

Current movement/drag inventory:

- selected authoring vertex movement
- runtime/placed object drag with door refusal
- light/spotlight drag
- Select drag arm/routing

Dirty/cache/runtime refresh examples:

- `MarkTopologyDocumentEdited()` sets document dirty flags and calls
  `InvalidateTopologyRenderCache()`.
- `MarkSectorEditorAuthoringGraphEdited()` sets document dirty/stale state and
  calls `InvalidateEditorTopologyRenderCache(state)`.
- placed-object drag updates `UpdateCachedRuntimeObjectDraw()` during drag and
  calls `RefreshRuntimeObjectsAfterAuthoringEdit()` after committed movement.
- authoring vertex movement refreshes authoring derivation after mutation.
- light movement finish uses topology action results; static light movement
  changes lightmap source inputs because static lights affect baked lighting.

Examples where Select currently routes movement:

- `StartSelectDrag()` routes runtime objects to `StartRuntimeObjectDrag()`.
- `StartSelectDrag()` routes all four light kinds to `StartLightDrag()`.
- `StartSelectDrag()` routes authoring vertices to
  `StartAuthoringVertexDrag()`.
- `IsSectorEditorPickTargetMovable()` currently returns movable for runtime
  objects, all light pick kinds, and authoring vertices; not movable for
  authoring lines or face anchors.

Movable vs non-movable examples:

- Sector: selectable, not directly movable.
- Authoring vertex: selectable and movable.
- Billboard/runtime object: selectable and movable.
- Door/runtime object: selectable, movement refused.
- Static light/dynamic light/static spot/dynamic spot: selectable and movable.
- Authoring line/face anchor and topology linedef/sidedef: selectable, not
  directly movable.
