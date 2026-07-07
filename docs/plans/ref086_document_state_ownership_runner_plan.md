# REF-086 DocumentState Ownership Runner Plan

## How To Use This Plan

This is a living execution plan.

When an agent is asked to execute this plan, it must:

1. Read this section first.
2. Read the `plan-state-json` block.
3. Identify the selected phase/pass.
4. Execute only that selected phase/pass.
5. Do not skip ahead.
6. Do not execute multiple phases/passes in one run unless the selected item explicitly says it is a combined pass.
7. If the selected item is too broad, update this plan with smaller child passes and stop.
8. If smaller passes are added, do not also implement source changes in the same run unless explicitly instructed.
9. After executing a phase/pass, update this plan with status, date, summary, verification results, behavior notes, remaining debt, and any phase-specific follow-up.
10. Do not claim manual verification unless it was actually performed.
11. Keep this plan self-tracking so future fresh-context runs can resume from it.

```plan-state-json id="ref086-document-state-ownership"
{
  "plan_id": "ref086_document_state_ownership",
  "status_values": [
    "Not Started",
    "Planned",
    "In Progress",
    "Completed",
    "Deferred",
    "Blocked",
    "Partial"
  ],
  "items": [
    {
      "id": "phase_01",
      "title": "DocumentState Inventory And Root/Sub-State Skeleton",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_02",
      "title": "Authoring Source State",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_03",
      "title": "Document Map And Derivation State",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_03a",
      "title": "Derivation Bookkeeping State",
      "type": "pass",
      "parent": "phase_03",
      "status": "Not Started"
    },
    {
      "id": "phase_03b",
      "title": "Document Map State / topologyMap Ownership",
      "type": "pass",
      "parent": "phase_03",
      "status": "Not Started"
    },
    {
      "id": "phase_03c",
      "title": "Derived Map Consumer Retargeting",
      "type": "pass",
      "parent": "phase_03",
      "status": "Not Started"
    },
    {
      "id": "phase_04",
      "title": "Document Lifecycle Dirty/Path/Status State",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_05",
      "title": "Load/Save/Reset/Import/Migration Retargeting",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_06",
      "title": "Topology Render Cache And Invalidation Ownership",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_07",
      "title": "Document Dependency Cleanup",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_08",
      "title": "Final Dependency Audit And Backlog Refresh",
      "type": "phase",
      "status": "Not Started"
    }
  ]
}
```

## Current Progress

| Phase | Status | Date | Summary | Verification | Behavior Notes | Remaining Debt |
| --- | --- | --- | --- | --- | --- | --- |
| Phase 1: DocumentState Inventory And Root/Sub-State Skeleton | Not Started | | | | | |
| Phase 2: Authoring Source State | Not Started | | | | | |
| Phase 3: Document Map And Derivation State | Not Started | | Parent phase; complete only after Phase 3A, 3B, and 3C complete or are explicitly deferred. | | | |
| Phase 3A: Derivation Bookkeeping State | Not Started | | | | | |
| Phase 3B: Document Map State / topologyMap Ownership | Not Started | | | | | |
| Phase 3C: Derived Map Consumer Retargeting | Not Started | | | | | |
| Phase 4: Document Lifecycle Dirty/Path/Status State | Not Started | | | | | |
| Phase 5: Load/Save/Reset/Import/Migration Retargeting | Not Started | | | | | |
| Phase 6: Topology Render Cache And Invalidation Ownership | Not Started | | | | | |
| Phase 7: Document Dependency Cleanup | Not Started | | | | | |
| Phase 8: Final Dependency Audit And Backlog Refresh | Not Started | | | | | |

## Execution Tracking Rules

- Each phase must compile and pass tests before moving to the next phase.
- Each implementation phase must be independently buildable and testable.
- Each phase must update the `plan-state-json` item status and the Current Progress table before finishing.
- Use `In Progress` only while source/doc changes are actively incomplete.
- Mark a phase `Completed` only after implementation, verification, behavior notes, and plan updates are done.
- If a phase is intentionally skipped, mark it `Deferred` and record why.
- If a phase cannot proceed without a decision, mark it `Blocked` and record the exact blocker.
- If a phase proves too broad, split it into child pass items in the JSON block and Current Progress table, then stop.
- If Phase 2, Phase 5, or Phase 7 would touch too many unrelated call sites in one run, split that phase into child passes in the `plan-state-json` block and Current Progress table, then stop without source changes.
- Phase 3 is pre-split into Phase 3A, 3B, and 3C. Each child pass must compile and pass tests before the next begins. Do not merge these child passes during execution.
- Suggested additional child-pass examples, if needed: `phase_02a` authoringGraph field move only, `phase_02b` authoring graph helper/context retargeting, `phase_05a` save/load helpers retarget, `phase_05b` reset/new document/import retarget, `phase_05c` migration/legacy topology conversion retarget, `phase_07a` tool context document dependency narrowing, and `phase_07b` inspector/service context document dependency narrowing.
- Do not rewrite unrelated phase text while updating progress.
- Do not claim manual GUI verification unless it was actually performed.
- Do not mark any DocumentState implementation phase complete from the original planning task alone.

## Summary

REF-086 prepares the sector editor document/source-of-truth state for ownership outside the monolithic `SectorEditorState`.

The implementation target is a small document-state root composed by `SectorEditor`, with narrow named sub-states for authoring source data, derivation bookkeeping, compiled/document map state, lifecycle dirty/path/status state, and cautiously deferred topology render/cache state. The goal is to make tools, panels, services, preview helpers, load/save/reset/import code, and cache invalidation code receive explicit document state or document views where needed.

This is not a feature plan and not a generic document manager plan. It must preserve current editor behavior, current JSON schema, current authoring graph source-of-truth rules, current topology render-cache invalidation contracts, and current lightmap source-hash behavior.

## Architecture Contract

- `SectorEditor` composes document state, preview state, renderer, services, and UI/modal state.
- `SectorEditor` remains responsible for top-level app/editor orchestration.
- `SectorEditorDocumentState` may be a small root aggregate composed by `SectorEditor`.
- `SectorEditorDocumentState` must group state into narrow named sub-states by responsibility.
- `SectorEditorDocumentState` must not become everything that used to be in `SectorEditorState` with a nicer name.
- Authoring graph data remains the editable source of truth for normal geometry/material editing.
- `SectorTopologyMap` remains derived output plus documented topology-owned global map metadata/runtime definitions for now.
- Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.
- REF-086 must be executable whether REF-085 implementation is complete or not.
- Do not assume PreviewState fields have already moved. If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.
- DocumentState work may pass document/map references to preview call sites, but must not move preview ownership.
- Tools, panels, services, preview helpers, and document helpers receive explicit document state, document sub-state, or read-only document views where needed.
- Document modules must not include `SectorEditor.h`.
- Document helpers/services must not call `SectorEditor::` methods.
- Do not create callback bridges back into `SectorEditor` to hide ownership problems.

## Current Problem

`SectorEditorState` still owns document/source-of-truth state mixed with preview state, 2D view state, modal state, tool transaction state, runtime object editing state, and top-level coordinator state.

Current document-owned candidates in `sources/sector_editor/SectorEditorTypes.h` include:

- `topologyMap`
- `authoringGraph`
- `authoringDerivation`
- `lastValidAuthoringDerivedTopology`
- `authoringDerivationState`
- `authoringDerivedTopologyStale`
- `authoringDerivationStatus`
- `topologyDocumentInitialized`
- `topologyDocumentDirty`
- `topologyDocumentStatus`
- `currentLevelName`
- `currentLevelPath`
- `hasCurrentLevelPath`
- `hasUnsavedChanges`
- `topologyRenderWarning`
- `topologyRenderRevision`
- `topologyRenderCache`

Current important helper boundaries that still take broad state include `SectorEditorAuthoringState.*`, `SectorEditorDirtyState.*`, `SectorEditorDocumentActions.*`, topology render-cache build paths, material editing/picker routing, selection/manipulation contexts, inspector panels, preview rebuild/bake paths, and central lifecycle methods in `SectorEditor.cpp`.

## Target Document State Ownership Model

Default state shape:

- `SectorEditorDocumentState`: small aggregate/root composed by `SectorEditor`.
- `SectorEditorAuthoringDocumentState`: owns `SectorAuthoringGraph authoringGraph`, unless a later implementation phase proves the root should embed the source graph directly because the group is tiny.
- `SectorEditorDerivationState`: owns `SectorAuthoringDerivationResult authoringDerivation`, `std::optional<SectorTopologyMap> lastValidAuthoringDerivedTopology`, `SectorEditorAuthoringDerivationState authoringDerivationState`, `bool authoringDerivedTopologyStale`, and `std::string authoringDerivationStatus`.
- `SectorEditorDocumentMapState` or `SectorEditorCompiledMapState`: owns `SectorTopologyMap topologyMap` as compiled/derived topology output plus documented topology-owned map metadata/runtime definitions for now.
- `SectorEditorDocumentLifecycleState`: owns `topologyDocumentInitialized`, `topologyDocumentDirty`, `topologyDocumentStatus`, `currentLevelName`, `currentLevelPath`, `hasCurrentLevelPath`, and `hasUnsavedChanges`.
- `SectorEditorDocumentRenderCacheState` or `SectorEditorTopologyCacheState`: considered only if Phase 6 proves `topologyRenderWarning`, `topologyRenderRevision`, and `topologyRenderCache` are clearly document-owned rather than view-owned. The default is to defer them to a future 2D view/cache state or leave them centrally composed.

The plan may choose different names only if the implementation phase explains why in the Current Progress notes and final report. Avoid naming the `topologyMap` owner `SectorEditorDerivedTopologyState` unless the implementation explicitly states that the state includes documented map-level metadata/runtime definitions, not just derived geometry. `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now. Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.

The split must remain responsibility-based. Preview runtime/controller/collision/camera state, modal UI state, inspector input buffers, tool transaction state, texture picker modal state, renderer ownership, and runtime object editing state stay outside DocumentState.

## Non-Negotiable Guardrails

- Do not change editor behavior intentionally.
- Do not implement game mode.
- Do not move `PreviewState` as part of REF-086.
- Do not move preview runtime/controller/collision/camera state as part of REF-086.
- REF-086 must be executable whether REF-085 implementation is complete or not.
- Do not assume PreviewState fields have already moved.
- If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.
- Do not retarget preview state as part of document work.
- DocumentState work may pass document/map references to preview call sites, but must not move preview ownership.
- Do not move modal UI state into DocumentState.
- Do not move inspector input buffers into DocumentState.
- Do not move tool transaction state into DocumentState.
- Do not move renderer ownership into DocumentState.
- Do not move source-of-truth map editing into `SectorTopologyMap`.
- Do not reintroduce no-authoring topology edit fallback.
- Authoring graph remains the editable source of truth for normal geometry/material editing.
- `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now.
- Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.
- Do not change serialization/schema.
- Do not change save/load JSON format.
- Do not change import/migration behavior.
- Do not change topology render-cache invalidation behavior.
- Do not change lightmap/source-hash behavior.
- Do not change rendering, collision, sector lookup, physics, or camera behavior.
- Do not make document modules depend on `SectorEditor.h`.
- Do not call `SectorEditor::` methods from document helpers/services.
- Do not create callback bridges back into `SectorEditor` to hide ownership problems.
- Do not create another god state object under a new name.
- Do not add a generic `EditorStateService`, generic `DocumentManager`, generic `SceneManager`, generic `WorldManager`, generic `TopologyService`, service locator, event bus, plugin architecture, command bus, undo framework, broad repository abstraction, new serialization framework, new asset/resource manager abstraction, callback bundle layer, broad inheritance hierarchy, or abstract `IDocumentSomething` interface hierarchy.

## Baseline Files To Inspect

Before implementing any phase, inspect at least:

- `docs/architecture/sector_editor_architectural_principles.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `docs/plans/ref084_service_state_ownership_runner_plan.md`
- `docs/plans/ref085_preview_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/runner_compatible_plans.md`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/SectorEditorTextureModals.*`
- `sources/sector_editor/SectorEditorSectorInspector.*`
- `sources/sector_editor/SectorEditorMaterialActions.*`
- `sources/sector_editor/document/`
- `sources/sector_editor/inspector/`
- `sources/sector_editor/tools/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/preview/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_demo/SectorAuthoringGraph.*`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorTopologyTypes.*`
- `sources/sector_demo/SectorLightmap.*`
- `sources/sector_demo/SectorRuntimeObjects.*`
- `sources/sector_demo/renderer/`

## Phase 1: DocumentState Inventory And Root/Sub-State Skeleton

### Goal

Create the initial document-state ownership skeleton and confirm the exact sub-state grouping. Phase 1 is skeleton/inventory only.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`

### Files Likely To Modify

- Add `sources/sector_editor/document/SectorEditorDocumentState.h` or `sources/sector_editor/SectorEditorDocumentState.h`.
- Possibly add narrow sub-state headers under `sources/sector_editor/document/`.
- Modify `sources/sector_editor/SectorEditor.h` to compose the new document state.
- Modify `sources/sector_editor/SectorEditorTypes.h` only for declarations/includes needed by the skeleton.
- Do not edit CMake unless a later implementation task explicitly permits it. If new `.cpp` files would require CMake, prefer header-only state structs for this phase or stop.

### Exact Implementation Steps

1. Re-inventory the candidate document fields in `SectorEditorState` and confirm whether each is document/source data, derivation bookkeeping, compiled/document map data, lifecycle state, cache state, or editor-only transient state.
2. Decide the exact initial split. Default to `SectorEditorDocumentState` with authoring/source, derivation, document map, lifecycle, and deferred render-cache sub-states.
3. Add only plain data structs with no behavior.
4. Phase 1 may compose an empty or not-yet-used `SectorEditorDocumentState documentState;` in `SectorEditor` only if that compiles cleanly.
5. Do not move `authoringGraph`, `topologyMap`, derivation fields, lifecycle fields, render-cache fields, load/save behavior, dirty behavior, or cache invalidation behavior.
6. Field moves begin in later dedicated phases only.
7. If adding the skeleton alone requires broad call-site edits, split Phase 1 or stop without source changes.
8. Compile and pass tests before moving to Phase 2.

### Behavior Guardrails

- No editor behavior should change.
- No serialization/schema/save/load/import behavior should change.
- No topology render-cache invalidation behavior should change.
- No lightmap source-hash behavior should change.
- No preview/runtime/controller/collision/camera state should move.
- DocumentState must not absorb modal, inspector input, tool transaction, texture picker, or renderer state.
- No document fields or document behavior may move in Phase 1 unless a human later amends this plan.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add or update tests only if a source field move requires fixture retargeting. Do not add asset-tree-dependent tests.

### Grep Checks

- `rg -n "struct SectorEditorState|topologyMap|authoringGraph|authoringDerivation|lastValidAuthoringDerivedTopology|topologyDocument|currentLevel|hasUnsavedChanges|topologyRender" sources/sector_editor`
- `rg -n "SectorEditorDocumentState|SectorEditorAuthoringDocumentState|SectorEditorDerivationState|SectorEditorDocumentMapState|SectorEditorCompiledMapState|SectorEditorDocumentLifecycleState|SectorEditorDocumentRenderCacheState|SectorEditorTopologyCacheState" sources/sector_editor`
- Confirm any moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if adding the skeleton requires CMake edits.
- Stop if adding the skeleton alone requires broad call-site edits.
- Stop if the state split would create a flat god `SectorEditorDocumentState`.
- Stop if the phase requires moving `authoringGraph`, `topologyMap`, derivation fields, lifecycle fields, render-cache fields, load/save behavior, dirty behavior, cache invalidation behavior, authoring mutation behavior, preview state, modal state, inspector buffers, tool transaction state, or renderer ownership.

### Final Report Requirements

- List exact document state/sub-state structs added.
- Explicitly state that no document fields moved, unless this plan is later amended by a human to allow Phase 1 field movement.
- State source struct/file and destination state object/file only for skeleton structs and composition.
- State that the authoring graph source-of-truth contract was preserved.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior.
- State that serialization/schema, rendering, collision, sector lookup, physics, and camera behavior were unchanged.

### What Remains For Later Phases

- Authoring graph ownership move.
- Derived topology and derivation ownership move.
- Lifecycle dirty/path/status move if not done here.
- Load/save/reset/import retargeting.
- Cache ownership decision.

### Phase Answers

- Candidate fields are from `SectorEditorState` in `SectorEditorTypes.h` to document state under `sources/sector_editor/document/`.
- `SectorEditor` owns the new root state and passes sub-states later.
- No state fields move in Phase 1 unless this plan is later amended by a human.
- Serialized data does not move in format; only in-memory ownership may move.
- Cache invalidation, serialization, and lightmap source-hash behavior must remain unchanged.

## Phase 2: Authoring Source State

### Goal

Move `authoringGraph` ownership out of `SectorEditorState` into document-owned state while preserving the authoring graph as the editable source of truth.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/tools/line/`
- `sources/sector_editor/tools/rectangle/`
- `sources/sector_editor/tools/insert_vertex/`
- `sources/sector_editor/tools/select/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/SectorEditorSectorInspector.*`
- `sources/sector_demo/SectorAuthoringGraph.*`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- Focused tool, selection, material, and inspector context call sites that read or mutate `authoringGraph`.

### Exact Implementation Steps

1. Move `SectorAuthoringGraph authoringGraph` from `SectorEditorState` to the chosen authoring/source document sub-state.
2. Update direct reads such as selection validation, authoring overlays, material target resolution, inspector target resolution, and save document construction to use explicit document state or `SectorAuthoringGraph&`.
3. Update direct mutations such as line draw, rectangle, insert vertex, delete, move, face-anchor reconciliation, and material/sector property mutation helpers to use explicit document state.
4. Keep all normal geometry/material editing writes in authoring graph data.
5. Preserve dirty/cache invalidation calls after authoring graph mutations.
6. Preserve selection mapping and stale selection pruning behavior.
7. Do not add any fallback that edits `SectorTopologyMap` when authoring graph data is missing.
8. Compile and pass tests before moving to Phase 3.

### Behavior Guardrails

- Sector draw, rectangle, line, insert-vertex, select, and authoring selection mapping behavior must remain unchanged.
- Authoring graph mutation helpers must still mark document dirty and invalidate derived/cache state as before.
- No direct topology edit fallback may return.
- Material and sector property edits must still write authoring data where they do today.
- Serialization/schema and saved JSON shape must not change.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update targeted authoring graph tests only if existing fixtures need retargeting.

### Grep Checks

- `rg -n "authoringGraph" sources/sector_editor tests`
- `rg -n "state\\.authoringGraph" sources/sector_editor tests`
- `rg -n "no-authoring|NoAuthoring|TopologyOnly|Legacy|Scratch|writeback" sources/sector_editor/services/material_edit sources/sector_editor/tools sources/sector_editor/SectorEditorAuthoringState.*`
- Confirm `authoringGraph` no longer appears as a member of `SectorEditorState`.

### Stop Conditions

- Stop if the phase would touch too many unrelated call sites; split into child passes and stop without source changes.
- Stop if moving `authoringGraph` requires moving all derivation and topology map ownership in the same run.
- Stop if a caller would need a callback bridge to `SectorEditor`.
- Stop if normal editor actions would edit `SectorTopologyMap` because authoring state is unavailable.

### Final Report Requirements

- List the exact field moved and new owner.
- State which helpers/contexts now receive authoring document state or `SectorAuthoringGraph&`.
- State that authoring graph remains the editable source of truth.
- Mention dirty/cache invalidation behavior.
- Mention serialization/schema behavior.
- Mention lightmap source-hash behavior.
- State that rendering, collision, sector lookup, physics, and camera behavior were unchanged.

### What Remains For Later Phases

- Derived topology ownership.
- Lifecycle path/dirty/status state.
- Load/save/import retargeting may still depend on broader document state.
- Broad tool/service dependencies may remain until Phase 7.

### Phase Answers

- `authoringGraph` moves from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorAuthoringDocumentState` or the equivalent source sub-state.
- `SectorEditor` owns the document root; tools, panels, services, and helpers receive explicit graph/document references.
- The moved state is document/source data and is serialized as part of the authoring document, but the JSON schema must not change.
- The move can affect cache invalidation if dirty paths are missed; preserve existing `MarkSectorEditorAuthoringGraphEdited()` / invalidation behavior.

## Phase 3: Document Map And Derivation State

### Goal

Move derivation bookkeeping and document map ownership into document-owned state through the pre-split child passes below. Phase 3 is not executable as one monolithic pass; execute Phase 3A, then Phase 3B, then Phase 3C.

`SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now. Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/SectorEditorMaterialActions.*`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_editor/preview/`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorTopologyTypes.*`
- `sources/sector_demo/SectorRuntimeObjects.*`
- `sources/sector_demo/SectorLightmap.*`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- Focused material, light, preview, bake, selection, inspector, and texture call sites.

### Exact Implementation Steps

1. Execute only the selected child pass.
2. Phase 3A moves derivation bookkeeping state and must not move `topologyMap`.
3. Phase 3B moves `topologyMap` to document map state and must preserve its classification as compiled/derived output plus documented map-level metadata/runtime definitions.
4. Phase 3C retargets remaining consumers to explicit document map/derivation references and must not move new fields unless fixing missed integration from Phase 3A or Phase 3B.
5. Each child pass must compile and pass tests before the next begins.

### Behavior Guardrails

- Authoring graph remains the source of truth for editable geometry/material data.
- `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions.
- Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.
- Do not move source-of-truth map editing into `SectorTopologyMap`.
- Do not change derivation timing, last-valid behavior, ID mapping, preview rebuild inputs, bake inputs, or texture registry behavior.
- Do not change light/runtime object/topology-owned metadata behavior.
- Do not change serialization/schema or save/load format.
- REF-086 must be executable whether REF-085 implementation is complete or not. If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update targeted derivation or material mapping tests only if existing tests need retargeting.

### Grep Checks

- `rg -n "topologyMap|authoringDerivation|lastValidAuthoringDerivedTopology|authoringDerivationState|authoringDerivedTopologyStale|authoringDerivationStatus" sources/sector_editor tests`
- `rg -n "state\\.topologyMap|state\\.authoringDerivation|state\\.lastValidAuthoringDerivedTopology|state\\.authoringDerivationState|state\\.authoringDerivedTopologyStale|state\\.authoringDerivationStatus" sources/sector_editor tests`
- `rg -n "ComputeSectorLightmapSourceHash|bakedLightmap|lightmapSettings|directionalLight|skySettings|previewSettings|texturesById|runtimeObjects" sources/sector_editor sources/sector_demo`
- Confirm moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if the selected child pass would touch too many unrelated call sites; split that child pass further and stop without source changes.
- Stop if moving `topologyMap` would require changing serialization/schema or import behavior.
- Stop if map-level metadata ownership is unclear and would be reclassified casually.
- Stop if source-hash affecting settings might be dropped from bake input/hash behavior.
- Stop if preview ownership would need to move or if the pass assumes REF-085 implementation has completed.

### Final Report Requirements

- List exact derivation or map fields moved and new owner.
- State how `topologyMap` is classified: derived output plus documented topology-owned global metadata/runtime definitions.
- State that moving `topologyMap` did not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.
- State derivation timing and last-valid behavior.
- State texture registry/global metadata behavior.
- State whether the current child pass was Phase 3A, 3B, or 3C and whether the later Phase 3 child passes remain.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior explicitly.
- State serialization/schema, rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Lifecycle dirty/path/status state if not moved.
- Load/save/reset/import retargeting.
- Cache ownership decision.
- Remaining broad dependencies in tools/panels/services.

### Phase Answers

- Derivation bookkeeping fields move from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorDerivationState` or equivalent in Phase 3A.
- `topologyMap` moves from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorDocumentMapState` or `SectorEditorCompiledMapState` or equivalent in Phase 3B.
- `SectorEditor` owns the document root; derivation helpers, preview, render-cache, bake, material, light, and selection call sites receive explicit derivation state or document map references.
- Moved derivation state is derived/bookkeeping data. Moved map state is compiled/derived topology output plus documented map-level metadata/runtime definitions. `SectorTopologyMap` is serialized through existing document save/load machinery, but the JSON schema must not change.
- The move can affect cache invalidation, serialization, and lightmap source-hash behavior if call sites are missed; preserve all existing contracts.

### Phase 3A: Derivation Bookkeeping State

Goal:

Move `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, and `authoringDerivationStatus` into `SectorEditorDerivationState` or equivalent. Do not move `topologyMap` in this pass.

Files to inspect and likely modify are the Phase 3 files above, focused on derivation status, last-valid topology, authoring target mapping, and derivation refresh helpers.

Exact implementation steps:

1. Move only `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, and `authoringDerivationStatus`.
2. Retarget `RefreshSectorEditorAuthoringDerivation()`, derivation-current checks, authoring target resolution, inspector target resolution, material mapping, and status display call sites to `SectorEditorDerivationState` or explicit references.
3. Do not move `topologyMap`.
4. Preserve derivation timing, stale/current/invalid status semantics, last-valid topology behavior, topology ID mapping, and authoring target mapping.
5. Compile and pass tests before Phase 3B.

Stop if `topologyMap` movement becomes necessary, if this pass would change serialization/schema/import behavior, or if the pass would require preview ownership changes.

Final report requirements:

- List the five derivation bookkeeping fields moved.
- State that `topologyMap` did not move in Phase 3A.
- State derivation timing, stale/current/invalid status, and last-valid topology behavior.
- Mention topology render-cache invalidation, serialization/schema, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

### Phase 3B: Document Map State / topologyMap Ownership

Goal:

Move `topologyMap` into `SectorEditorDocumentMapState` or equivalent, preserving its classification as compiled/derived output plus documented map-level metadata/runtime definitions. Do not change serialization/schema, metadata copying, source-hash behavior, or import behavior.

`SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now. Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.

Exact implementation steps:

1. Move only `SectorTopologyMap topologyMap` from `SectorEditorState` to `SectorEditorDocumentMapState`, `SectorEditorCompiledMapState`, or equivalent.
2. Preserve `CopyEditorMapLevelFields()` behavior for texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked lightmap metadata.
3. Preserve derivation output installation, last-valid topology updates, preview/render/cache/lightmap/collision inputs, texture registry behavior, light/runtime object/topology-owned metadata behavior, and baked metadata restoration after load.
4. Do not move derivation bookkeeping fields unless fixing missed integration from Phase 3A.
5. Do not move preview/runtime/controller/collision ownership even if preview call sites still read the map.
6. Compile and pass tests before Phase 3C.

Stop if `topologyMap` ownership is treated as purely derived geometry, if metadata would be reclassified as authoring source data, if source-hash-affecting data could be dropped, or if serialization/import behavior would change.

Final report requirements:

- State exact `topologyMap` destination owner/file.
- State that `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions.
- State that texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata were not reclassified as authoring graph source data.
- Mention serialization/schema, import behavior, metadata copying, topology render-cache invalidation, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

### Phase 3C: Derived Map Consumer Retargeting

Goal:

Retarget remaining consumers to explicit document map/derivation state references and reduce broad `SectorEditorState` dependency where practical. Do not move new fields unless fixing missed integration from Phase 3A or Phase 3B.

Exact implementation steps:

1. Inventory remaining `SectorEditorState&` dependencies that are now present only for derivation state or document map access.
2. Retarget those consumers to explicit `SectorEditorDerivationState&`, `SectorEditorDocumentMapState&`, `SectorTopologyMap&`, or `const SectorTopologyMap&` references where practical.
3. Leave broad dependencies in place when they are caused by preview state, modal UI state, inspector input buffers, tool transaction state, runtime object editing state, or renderer ownership.
4. Preserve preview separation and REF-085 independence. Do not assume PreviewState fields have moved.
5. Compile and pass tests before Phase 4.

Stop if dependency cleanup requires moving new fields, preview ownership, modal state, inspector buffers, tool transaction state, runtime object editing state, or renderer ownership.

Final report requirements:

- List consumers retargeted and broad dependencies intentionally left.
- State no new fields moved except missed integration fixes from Phase 3A or 3B.
- State REF-086 remained executable independently of REF-085 implementation status.
- Mention topology render-cache invalidation, serialization/schema, lightmap source-hash, rendering, collision, sector lookup, physics, and camera behavior.

## Phase 4: Document Lifecycle Dirty/Path/Status State

### Goal

Move dirty/path/status/init state into document-owned lifecycle state.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `sources/sector_editor/document/SectorEditorDocumentModals.*`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_editor/SectorEditorPreviewActions.*`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- Modal/status panel call sites.

### Exact Implementation Steps

1. Move `topologyDocumentInitialized`, `topologyDocumentDirty`, `topologyDocumentStatus`, `currentLevelName`, `currentLevelPath`, `hasCurrentLevelPath`, and `hasUnsavedChanges` to the document lifecycle sub-state.
2. Retarget `MarkSectorEditorTopologyDocumentEdited()` to receive explicit lifecycle state and render-cache/invalidation state rather than broad `SectorEditorState&`, or provide a narrow document-state helper if that is clearer.
3. Update save/load/new/reload modal open paths to read lifecycle state explicitly.
4. Update status panel/window/title/status text paths that display current level, dirty marker, document status, and unsaved changes.
5. Update lightmap bake install and preview/settings mutation paths that mark unsaved changes.
6. Preserve dirty marker behavior, unsaved prompts, current path/name behavior, and initialization behavior.
7. Compile and pass tests before moving to Phase 5.

### Behavior Guardrails

- Save/load prompts and overwrite confirmation behavior must remain unchanged.
- Dirty marker and unsaved-change checks must remain unchanged.
- Current path/name behavior must remain unchanged.
- Document initialized behavior must remain unchanged.
- Status/error message text should remain unchanged unless an exact existing bug is documented and explicitly in scope.
- REF-086 must be executable whether REF-085 implementation is complete or not. If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update document action tests only if existing tests need retargeting.

### Grep Checks

- `rg -n "topologyDocumentInitialized|topologyDocumentDirty|topologyDocumentStatus|currentLevelName|currentLevelPath|hasCurrentLevelPath|hasUnsavedChanges" sources/sector_editor tests`
- `rg -n "state\\.topologyDocument|state\\.currentLevel|state\\.hasCurrentLevelPath|state\\.hasUnsavedChanges" sources/sector_editor tests`
- Confirm moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if moving lifecycle state requires moving modal UI state into DocumentState.
- Stop if dirty/status helpers would need callback bridges to `SectorEditor`.
- Stop if status text behavior would change.
- Stop if unsaved prompt behavior would change.
- Stop if the phase assumes REF-085 implementation has completed or tries to move preview ownership.

### Final Report Requirements

- List exact lifecycle fields moved and new owner.
- State which helpers now receive lifecycle state explicitly.
- State dirty marker, unsaved prompt, current path/name, and document initialized behavior.
- State REF-086 remained executable independently of REF-085 implementation status if preview call sites were touched.
- Mention topology render-cache invalidation behavior.
- Mention serialization/schema and lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Load/save/reset/import/migration helper retargeting.
- Cache ownership decision if not done.
- Remaining broad document dependencies in tools/panels/services.

### Phase Answers

- Lifecycle fields move from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorDocumentLifecycleState` or equivalent.
- `SectorEditor` owns lifecycle state; document actions, modals, status panel, bake install, and mutation helpers receive explicit lifecycle references.
- Moved state is document lifecycle state. It is not directly serialized as map data, though current path/name control save/load behavior.
- The move can affect cache invalidation if dirty helpers are wrong; preserve existing invalidation and source-hash behavior.

## Phase 5: Load/Save/Reset/Import/Migration Retargeting

### Goal

Move or prepare narrow document lifecycle helpers for existing load/save/reset/import/migration behavior.

### Files To Inspect

- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `sources/sector_editor/document/SectorEditorDocumentModals.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorAuthoringGraph.*`
- `sources/sector_demo/SectorLightmap.*`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- Focused helper/context headers that still accept `SectorEditorState&` only for document fields.

### Exact Implementation Steps

1. Retarget `BuildSectorAuthoringDocumentFromEditorState()`, `SaveSectorEditorAuthoringDocument()`, `ResetEditorTopologyDocumentState()`, and load/import document install paths to receive explicit document state or document sub-states.
2. Preserve `LoadSectorEditorDocumentFromAsset()` format detection for authoring graph documents and topology-v2 import documents.
3. Preserve old topology-v2 import conversion into authoring graph first.
4. Preserve reset/new document behavior, default texture population, missing-file/sample fallback behavior where currently implemented, and status/error messages.
5. Preserve texture registry behavior and map-level metadata copying during derivation and save.
6. Preserve lightmap metadata/source hash behavior, including loaded baked lightmap restoration after successful derivation.
7. Do not introduce `SectorEditorDocumentController` by default.
8. Prefer retargeting existing document action/helper functions to explicit document state/sub-state references.
9. If a controller seems necessary, stop and add a future backlog item or child planning pass. Do not implement it casually inside REF-086.
10. A controller may only be introduced in REF-086 if a human explicitly amends this plan to allow it after reviewing the proposed shape.
11. Keep `SectorEditor` responsible for top-level orchestration around world/runtime cleanup, asset scopes, preview rebuild, and UI modal routing.
12. REF-086 must remain executable whether REF-085 implementation is complete or not; do not move or retarget preview state as part of this phase.
13. Compile and pass tests before moving to Phase 6.

### Behavior Guardrails

- Do not change current JSON schema.
- Do not change save/load JSON format.
- Do not change import/migration behavior.
- Do not reintroduce no-authoring fallback.
- Missing-file/sample fallback and status/error messages must remain unchanged.
- Texture registry, baked lightmap metadata, and source-hash behavior must remain unchanged.
- Runtime/preview cleanup orchestration remains top-level unless explicitly proven document-only.
- `SectorTopologyMap` remains derived output plus documented map-level metadata/runtime definitions for now. Moving `topologyMap` does not reclassify texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, or baked metadata as authoring graph source data.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update tests with generated temporary JSON or immutable fixtures outside the engine asset tree if coverage is needed. Do not depend on user-edited levels under `assets/levels` or `assets/sector_demo`.

### Grep Checks

- `rg -n "ResetEditorTopologyDocumentState|SaveSectorEditorAuthoringDocument|LoadSectorEditorDocumentFromAsset|BuildSectorAuthoringDocumentFromEditorState|InitializeSectorEditorAuthoringStateFromTopology|ImportSectorTopologyMapToAuthoringGraph" sources/sector_editor sources/sector_demo tests`
- `rg -n "formatVersion|authoringGraph|linedef|TopologyV2Import|LoadSectorTopologyMapFromJsonString|SaveSectorAuthoringDocument" sources/sector_editor sources/sector_demo tests`
- `rg -n "SectorEditorState&|const SectorEditorState&" sources/sector_editor/document sources/sector_editor/SectorEditorDirtyState.*`

### Stop Conditions

- Stop if the phase would touch too many unrelated call sites; split into child passes and stop without source changes.
- Stop if a controller seems necessary; add a future backlog item or child planning pass instead of implementing it casually inside REF-086.
- Stop if a controller/helper starts becoming a generic `DocumentManager`.
- Stop if schema, save/load output, or migration behavior would change.
- Stop if import would install topology data without authoring graph conversion.
- Stop if preview/runtime/controller cleanup would need to move into DocumentState.
- Stop if the phase assumes REF-085 implementation has completed.

### Final Report Requirements

- State that no `SectorEditorDocumentController` was added, unless a human-amended plan explicitly allowed one.
- If a controller seemed necessary, state that the phase stopped and recorded future planning/backlog debt.
- List exact load/save/reset/import helpers retargeted.
- State JSON schema/save/load/import behavior.
- State old topology-v2 import still converts into authoring graph first.
- State texture registry and lightmap metadata/source hash behavior.
- Mention topology render-cache invalidation behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Topology render cache ownership decision.
- Dependency cleanup in tools/panels/services.
- Audit/backlog refresh.

### Phase Answers

- This phase may move no fields if earlier phases already moved ownership; it retargets lifecycle behavior from broad `SectorEditorState&` to document state.
- `SectorEditor` remains top-level owner/orchestrator and document helpers receive document state explicitly.
- Moved or retargeted state is document source, derived, lifecycle, and serialized map state depending on helper.
- The phase is serialization and source-hash sensitive; behavior must remain unchanged and tests/grep checks must prove no schema fallback returned.

## Phase 6: Topology Render Cache And Invalidation Ownership

### Goal

Preserve and document topology render-cache invalidation/rebuild contracts. The default expectation is that topology render cache probably belongs to a future 2D view/cache state or remains centrally composed until that exists.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/tools/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/texture_catalog/`

### Files Likely To Modify

- `sources/sector_editor/document/SectorEditorDocumentState.h`
- Possibly a new `sources/sector_editor/document/SectorEditorDocumentRenderCacheState.h` or `sources/sector_editor/SectorEditorTopologyCacheState.h`.
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- Focused invalidation/build call sites in `SectorEditor.cpp` and authoring helpers.

### Exact Implementation Steps

1. Re-evaluate `topologyRenderWarning`, `topologyRenderRevision`, and `topologyRenderCache` ownership.
2. Default to leaving `topologyRenderWarning`, `topologyRenderRevision`, and `topologyRenderCache` where they are, or deferring them to a future 2D view/cache state.
3. Do not move these fields into DocumentState unless inspection proves the cache is document-owned rather than view-owned and the move is small.
4. If ownership is ambiguous, leave the fields where they are and record debt.
5. Retarget `InvalidateTopologyRenderCache()`, `MarkSectorEditorTopologyDocumentEdited()`, `MarkSectorEditorAuthoringGraphEdited()`, `EnsureTopologyRenderCache()`, and cache draw paths to explicit cache/document inputs only if that retargeting is needed and small.
6. Preserve explicit invalidation behavior and over-invalidation tolerance.
7. Preserve no expensive steady draw-path rebuild behavior: do not call `ValidateSectorTopologyMap()`, `ExtractSectorTopologyLoops()`, `BuildSectorTopologyIndexes()`, or `mapbox::earcut()` from the steady 2D frame draw path except through the existing cache rebuild path.
8. Preserve warning/status behavior and topology render output.
9. The most important outcome of Phase 6 is preserving and documenting invalidation/rebuild contracts, not forcing a state move.
10. Compile and pass tests before moving to Phase 7.

### Behavior Guardrails

- Topology render-cache invalidation behavior must not change.
- Over-invalidation remains acceptable; missed invalidation remains the bug.
- No expensive derived topology/cache rebuild may be added to the steady draw path.
- Picking behavior must remain consistent with what is drawn.
- Texture registry changes still do not require 2D cache invalidation unless display-state caching changes.
- Baked lightmap result changes should not invalidate the 2D topology render cache unless that cache starts drawing baked data.
- Do not move cache fields into DocumentState unless they are clearly document-owned rather than view-owned and the move is small.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update render-cache tests only if existing tests need retargeting.

### Grep Checks

- `rg -n "topologyRenderWarning|topologyRenderRevision|topologyRenderCache|InvalidateTopologyRenderCache|MarkSectorEditorTopologyDocumentEdited|MarkSectorEditorAuthoringGraphEdited|EnsureTopologyRenderCache" sources/sector_editor tests`
- `rg -n "ValidateSectorTopologyMap\\(|ExtractSectorTopologyLoops\\(|BuildSectorTopologyIndexes\\(|earcut" sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditorTopologyRenderCache.*`
- Confirm moved cache fields no longer appear as members of `SectorEditorState`, if moved.

### Stop Conditions

- Stop if ownership is ambiguous between document and 2D view state; defer the move and record the debt.
- Stop if moving cache fields would be the main objective rather than preserving/documenting invalidation contracts.
- Stop if invalidation ordering would change.
- Stop if cache rebuilds would move into steady frame draw.
- Stop if draw output or picking behavior would change.

### Final Report Requirements

- State whether cache fields moved or were deferred, and why.
- If deferred, state whether the likely future owner is a 2D view/cache state or central composition until that exists.
- List exact cache fields moved and new owner, if any.
- State invalidation behavior and rebuild timing.
- State warning/status behavior.
- Mention lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Dependency cleanup based on the chosen cache ownership.
- Any future 2D view/cache state plan if cache ownership is deferred.

### Phase Answers

- Candidate cache fields default to remaining central with documented debt or deferring to a future 2D view/cache state. They move from `SectorEditorState` only if proven document-owned and small.
- `SectorEditor` owns the cache state; render-cache builders/draw paths receive explicit document and cache references.
- Moved state is derived editor-only cache state, not serialized.
- The phase directly affects cache invalidation risk; serialization and lightmap source hash must remain unchanged.

## Phase 7: Document Dependency Cleanup

### Goal

Reduce broad `SectorEditorState` dependencies in tools, panels, services, and helpers now that document state is explicit.

### Files To Inspect

- `sources/sector_editor/tools/`
- `sources/sector_editor/inspector/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/preview/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/document/`

### Files Likely To Modify

- Tool/service/panel context headers that still accept broad `SectorEditorState&` for document fields.
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- Focused helper headers in `sources/sector_editor/document/`, `selection/`, `tools/`, `inspector/`, and `services/`.

### Exact Implementation Steps

1. Inventory remaining broad `SectorEditorState&` and `SectorEditorUiState&` dependencies caused by document state.
2. Replace broad dependencies with explicit `SectorEditorDocumentState&`, sub-state references, `SectorAuthoringGraph&`, `SectorTopologyMap&`, lifecycle state, cache state, selection state, material state, light state, or services as appropriate.
3. Keep dependencies that are broad because of preview state, modal state, inspector buffers, runtime object editing state, or tool transaction state out of scope and record them as debt.
4. REF-086 must remain executable whether REF-085 implementation is complete or not. If preview/runtime/controller/collision fields still live in `SectorEditorState`, leave them untouched.
5. DocumentState work may pass document/map references to preview call sites, but must not move preview ownership.
6. Do not create callback bridges to route document actions back through `SectorEditor`.
7. Do not create generic state/service infrastructure.
8. Remove unnecessary `SectorEditorTypes.h` or `SectorEditor.h` includes from document-adjacent modules where practical.
9. Compile and pass tests before moving to Phase 8.

### Behavior Guardrails

- Tool/service/panel behavior must remain unchanged.
- Authoring graph source-of-truth contract must remain unchanged.
- Preview separation must remain unchanged.
- Do not assume PreviewState fields have already moved.
- Modal and inspector input state must not move into DocumentState.
- No callback bridges.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update tests only if existing context fixtures need retargeting.

### Grep Checks

- `rg -n "SectorEditorState&|const SectorEditorState&|SectorEditorState\\*|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/document sources/sector_editor/tools sources/sector_editor/inspector sources/sector_editor/selection sources/sector_editor/preview sources/sector_editor/services`
- `rg -n "DocumentState|AuthoringDocumentState|DerivationState|DocumentMapState|CompiledMapState|DocumentLifecycleState|DocumentRenderCacheState|TopologyCacheState" sources/sector_editor`
- `rg -n "callback|std::function" sources/sector_editor/document sources/sector_editor/services/material_edit sources/sector_editor/tools sources/sector_editor/selection`

### Stop Conditions

- Stop if the phase would touch too many unrelated call sites; split into child passes and stop without source changes.
- Stop if dependency cleanup requires moving preview, modal, inspector buffer, tool transaction, runtime object editing, or renderer state.
- Stop if the cleanup assumes REF-085 implementation has completed.
- Stop if a callback bridge to `SectorEditor` would be needed.
- Stop if a helper starts becoming a generic manager/service locator.

### Final Report Requirements

- List broad dependencies removed.
- List broad dependencies intentionally left and why.
- State exact document state/sub-states received by tools/panels/services.
- State REF-086 remained executable independently of REF-085 implementation status.
- State no callback bridges were added, or list any unavoidable temporary bridge as debt.
- Mention topology render-cache invalidation behavior.
- Mention serialization/schema and lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.

### What Remains For Later Phases

- Final audit/backlog refresh.
- Any deferred preview/modal/inspector/tool/runtime-object dependency cleanup.

### Phase Answers

- This phase should move no new document fields unless a missed field is discovered; it narrows receivers from broad state to explicit document state and related narrow states.
- `SectorEditor` remains composer; tools/panels/services receive explicit references.
- Dependency cleanup state classifications depend on the referenced sub-state and must be recorded per changed context.
- Serialization, cache invalidation, and source-hash behavior must remain unchanged.

## Phase 8: Final Dependency Audit And Backlog Refresh

### Goal

Update the audit and backlog after implementation phases complete.

### Files To Inspect

- `docs/plans/ref086_document_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/document/`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/tools/`
- `sources/sector_editor/inspector/`
- `sources/sector_editor/services/`
- `sources/sector_editor/preview/`

### Files Likely To Modify

- `docs/plans/ref086_document_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`

### Exact Implementation Steps

1. Run grep checks for all moved fields and broad document dependencies.
2. Update Current Progress for every completed/deferred/blocked phase.
3. Update the backlog REF-086 completion notes only after implementation phases are truly complete.
4. Refresh `docs/audit/sector_editor_state_ownership_and_remaining_map.md`.
5. The refreshed audit must include post-REF-086 `SectorEditorState` document fields remaining, new document state/sub-state objects composed by `SectorEditor`, remaining broad `SectorEditorState` / `SectorEditorUiState` dependencies caused by document state, remaining DocumentState debt, whether preview state remained separate, whether renderer ownership stayed outside DocumentState, whether authoring graph remains source of truth, whether Phase 3 split `topologyMap` from derivation bookkeeping, whether `SectorTopologyMap` remains derived output plus documented map metadata/runtime definitions, whether `topologyMap` still contains documented map-level metadata/runtime definitions, whether topology render cache was moved or deferred and why, whether any `SectorEditorDocumentController` was created with the expected normal answer being no, whether REF-086 was executable independently of REF-085 implementation status, whether topology render-cache invalidation behavior was unchanged, whether serialization/schema behavior was unchanged, whether lightmap source-hash behavior was unchanged, and whether rendering/collision/sector lookup/physics/camera behavior was unchanged.
6. Record any remaining document debt, especially cache ownership if deferred, modal/UI state boundaries, runtime object editing boundaries, preview-blocked dependencies, and manual smoke gaps.
7. Do not make source code changes in this final audit phase unless fixing a build break from the immediately preceding phase.
8. Compile and pass tests before closing the plan.

### Behavior Guardrails

- No behavior changes in the final audit phase.
- Do not mark unrelated backlog items complete.
- Do not mark preview implementation phases complete.
- Do not claim manual GUI verification unless performed.
- Do not assume REF-085 implementation has completed.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

### Grep Checks

- `rg -n "topologyMap|authoringGraph|authoringDerivation|lastValidAuthoringDerivedTopology|authoringDerivationState|authoringDerivedTopologyStale|authoringDerivationStatus|topologyDocumentInitialized|topologyDocumentDirty|topologyDocumentStatus|currentLevelName|currentLevelPath|hasCurrentLevelPath|hasUnsavedChanges|topologyRenderWarning|topologyRenderRevision|topologyRenderCache" sources/sector_editor/SectorEditorTypes.h`
- `rg -n "SectorEditorState&|const SectorEditorState&|SectorEditorState\\*|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/document sources/sector_editor/tools sources/sector_editor/inspector sources/sector_editor/selection sources/sector_editor/preview sources/sector_editor/services`
- `rg -n "REF-085|REF-086|DocumentState|PreviewState" docs/plans/codebase_refactor_backlog.md docs/audit/sector_editor_state_ownership_and_remaining_map.md`

### Stop Conditions

- Stop if implementation phases are incomplete; mark this phase `Blocked` or `Partial` instead of closing the plan.
- Stop if validation fails and source changes are required beyond small immediate fixes.
- Stop if backlog changes would imply preview implementation completion.
- Stop if audit refresh cannot accurately classify remaining document ownership.

### Final Report Requirements

- Summarize final DocumentState ownership.
- List remaining document debt.
- State whether REF-085 implementation remains separate.
- State whether Phase 3 split `topologyMap` from derivation bookkeeping.
- State whether `topologyMap` still contains documented map-level metadata/runtime definitions.
- State whether topology render cache was moved or deferred, and why.
- State whether any `SectorEditorDocumentController` was created; expected answer should normally be no.
- State whether REF-086 was executable independently of REF-085 implementation status.
- Summarize the required audit refresh.
- Mention topology render-cache invalidation behavior.
- Mention serialization/schema behavior.
- Mention lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.
- Report build, ctest, diff check, diff stat, and status results.

### What Remains For Later Phases

- Any deferred preview/modal/inspector/tool/runtime-object editing state plan.
- Any future document lifecycle controller planning item only if a human later approves that direction.
- Any future 2D view/cache state plan if topology cache ownership was deferred.

### Phase Answers

- This phase moves no fields unless correcting immediately discovered integration mistakes.
- It updates plan/backlog/audit documentation only.
- It verifies the plan is complete and self-tracking.
- It records remaining debt after the document state migration.

## Tests And Verification

Every implementation phase must run:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Documentation-only phase updates may skip build/ctest only when no source, test, CMake, generated data, or behavior changed, and the final report must say they were skipped.

The planning task that created this file must run:

```bash
git diff --check
git diff --stat
git status --short
```

`cmake --build cmake-build-debug -j2` is optional for the planning task because it changes documentation only.

## Manual Smoke Suggestions

Manual editor smoke is recommended after phases that touch authoring, derivation, lifecycle, cache invalidation, load/save/import, material routing, or status behavior:

- Create a new blank map, draw lines, draw rectangles, insert vertices, move vertices, and delete authoring elements.
- Edit sector names, floor/ceiling heights, sky, Blocks Player, and material/UV/decal values through existing UI paths.
- Save a new level, reload it, overwrite it, and verify dirty markers/status text.
- Load an existing authoring graph level.
- Import a topology-v2 level and verify it converts into authoring graph data before editing.
- Enter and leave Preview3D after document edits.
- Start a lightmap bake only when intentionally verifying bake-sensitive document changes.

Do not claim manual smoke verification unless it was actually performed.

## Backlog Updates

- The planning task that created this file may mark REF-086 as runner-plan written/planning complete only.
- Implementation phases must not be marked complete until this runner plan executes those phases.
- REF-085 must not be modified except to preserve its existing status if already complete as runner-plan written.
- REF-086 implementation must not depend on REF-085 implementation status.
- Do not mark unrelated backlog items complete.
- After final implementation, update REF-086 completion notes with exact moved state, verification results, behavior notes, and remaining debt.

## Stop Conditions

- Stop if a requested phase requires moving preview/runtime/controller/collision/camera state.
- Stop if a phase assumes REF-085 implementation has completed.
- Stop if a phase would retarget preview state as part of document work. Passing document/map references to preview call sites is allowed; moving preview ownership is not.
- Stop if a phase would change serialization/schema or save/load JSON format.
- Stop if a phase would change import/migration behavior.
- Stop if a phase would change topology render-cache invalidation behavior.
- Stop if a phase would change lightmap/source-hash behavior.
- Stop if a phase would change rendering, collision, sector lookup, physics, or camera behavior intentionally.
- Stop if a phase would implement game mode.
- Stop if a phase would introduce broad infrastructure listed in the guardrails.
- Stop if a phase requires document modules to include `SectorEditor.h` or call `SectorEditor::` methods.
- Stop if a phase needs callback bridges back into `SectorEditor` to hide ownership problems.
- Stop if a phase would create a flat god `SectorEditorDocumentState` instead of narrow responsibility-based sub-states.
- Stop if a phase would move modal UI state, inspector input buffers, tool transaction state, texture picker modal state, runtime object editing state, or renderer ownership into DocumentState.
- Stop if Phase 2, Phase 5, or Phase 7 would touch too many unrelated call sites in one run; split that phase into child passes in the `plan-state-json` block and Current Progress table, then stop without source changes. Phase 3 is already split into child passes and must stay split.

## Per-Phase Final Report Requirements

Each phase final report must include:

- exact fields moved
- source struct/file and destination state object/file
- changed owners/receivers
- major call sites changed
- behavior preserved
- tests/checks run
- grep checks run
- remaining debt
- whether moved state is document/source data, derived data, lifecycle state, cache state, or editor-only transient state
- whether moved state is serialized
- whether cache invalidation, serialization, or lightmap source-hash behavior could be affected and how it was preserved
- topology render-cache invalidation behavior
- lightmap source-hash behavior
- rendering, collision, sector lookup, physics, and camera behavior
- REF-085 implementation independence when the phase touches preview call sites or broad state shared with preview
- manual verification performed, or a clear statement that none was performed
