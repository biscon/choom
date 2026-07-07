# REF-085 PreviewState Ownership Runner Plan

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
10. Do not claim manual GUI verification unless it was actually performed.
11. Keep this plan self-tracking so future fresh-context runs can resume from it.

```plan-state-json id="ref085-preview-state-ownership"
{
  "plan_id": "ref085_preview_state_ownership",
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
      "title": "PreviewState Inventory And Low-Risk Preview Control Flags",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Preview Surface Selection State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Preview Controller And Camera State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Preview Collision And Sector Lookup State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Preview Runtime Object And World Adapter State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_06",
      "title": "Preview Renderer Lifecycle Dependency Cleanup",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_07",
      "title": "Final Dependency Audit And Backlog Refresh",
      "type": "phase",
      "status": "Completed"
    }
  ]
}
```

## Current Progress

| Phase | Status | Date | Summary | Verification | Behavior Notes | Remaining Debt |
| --- | --- | --- | --- | --- | --- | --- |
| Phase 1: PreviewState Inventory And Low-Risk Preview Control Flags | Completed | 2026-07-07 | Added `preview/SectorEditorPreviewState.h` with `SectorEditorPreviewOverlayState`, composed `SectorEditorPreviewState` in `SectorEditor`, and moved `previewUiHidden`, `activePreviewDebugOverlayTab`, `showObjectProbeDebugOverlay`, and `useBakedAmbientOcclusion` out of `SectorEditorState`. `useBakedAmbientOcclusion` moved because inspection found it is a preview renderer/overlay toggle only, not serialized, document-owned, bake-affecting, or source-hash-affecting. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; grep checks for moved fields and preview broad-state includes. | Hidden-preview UI hotkey behavior, preview overlay tab behavior, object probe debug overlay visibility, and baked AO preview toggle behavior were intended unchanged. No game mode was implemented. No topology mutation or 2D topology render-cache invalidation behavior changed. Lightmap source-hash behavior was unchanged. Collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | Broad `SectorEditorState&` / `SectorEditorUiState&` dependencies remain in preview overlay/UV panel for later phases because controller, collision, runtime object, document, modal, and preview selection state were intentionally not moved in Phase 1. |
| Phase 2: Preview Surface Selection State | Completed | 2026-07-07 | Added `SectorEditorPreviewSelectionState` under `preview/SectorEditorPreviewState.h` and moved `hoveredSurface3D`, `selectedSurface3D`, and `selectedTopologySurface3D` out of `SectorEditorState`. Retargeted preview hover/selection, preview surface highlight drawing, preview UV panel, material editing service, light editing service selection refs, authoring stale-mapping cleanup, and affected tests to receive/use preview selection state explicitly. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; grep checks for moved fields and broad preview/material/selection/light dependencies. | Preview3D hover highlights, selected-surface material panel behavior, texture/UV/decal editing targets, and material picker routing were intended unchanged. 2D `SelectionState` was not broadened with preview-only state. No document/source-of-truth ownership moved. No topology mutation or 2D topology render-cache invalidation behavior changed. Lightmap source-hash behavior was unchanged. Rendering output, collision, sector lookup, physics, and camera behavior were intended unchanged. No manual GUI verification was performed. | Broad document/editor dependencies remain in preview overlay/UV panel and material picker routing for later phases. Controller/camera, collision/sector lookup, runtime object preview state, and renderer lifecycle cleanup remain for later phases. |
| Phase 3: Preview Controller And Camera State | Completed | 2026-07-07 | Added `SectorEditorPreviewControllerState` under `preview/SectorEditorPreviewState.h` and moved `previewControlMode`, `freeflyController`, `fpsControllerConfig`, `fpsControllerState`, `visualStepOffsetY`, `headBobState`, `landingDipState`, `hasPreviewPose`, `lastPreviewPose`, and `spotLightPilotPreviewRestore` out of `SectorEditorState`. Retargeted preview action helpers, preview overlay reads, preview settings apply/read paths, document reset/load config hydration, spotlight pilot restore, and `SectorEditor` preview wrappers to use the controller state explicitly. `fpsControllerConfig` remains transient runtime config hydrated from persisted `SectorTopologyMap::previewSettings`; `previewControlMode` is not serialized. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; grep checks for moved fields and broad preview/action dependencies. | Physical/collision pose remains `fpsControllerState.feetPosition` and current-sector state; visual-only step smoothing, headbob, and landing dip remain applied only through visual pose helpers. Spotlight pilot restore still restores preview pose and mouse-look through freefly controller state. No game mode was implemented. No topology mutation or 2D topology render-cache invalidation behavior changed. Lightmap source-hash behavior was unchanged; persisted preview settings are still stored on the topology map. Collision, sector lookup, physics, and camera behavior were intended unchanged. No manual GUI verification was performed. | Collision world/results/current-sector lookup state, runtime object preview state, and broad `SectorEditorState&` dependencies in preview action/overlay/UV modules remain for later phases. |
| Phase 4: Preview Collision And Sector Lookup State | Completed | 2026-07-07 | Added `SectorEditorPreviewCollisionState` under `preview/SectorEditorPreviewState.h` and moved `sectorCollisionWorld`, `sectorCollisionWorldValid`, `sectorCollisionWorldWarning`, `previewCollisionSectorId`, `previewVerticalResult`, `previewMoveResult`, and `previewCollisionNoclipFallback` out of `SectorEditorState`. Retargeted preview action helpers, preview enter/rebuild failure resets, preview settings vertical refresh, and overlay collision/debug status to use preview collision state explicitly. Collision rebuild now receives `SectorTopologyMap` input directly, and gameplay movement receives only the runtime dynamic door collider vector needed for door collision integration. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; grep checks for moved collision fields and broad preview/action dependencies. | Preview collision build/rebuild timing was intended unchanged at preview enter, preview mesh rebuild, settings apply vertical refresh, load/reset-dependent wrapper calls, and explicit rebuild requests. Player movement, collision response, sector lookup, noclip fallback, physics, and camera behavior were intended unchanged; visual offsets still do not feed collision, sector lookup, or physics. No topology mutation or 2D topology render-cache invalidation behavior changed. Lightmap source-hash behavior was unchanged. No manual GUI verification was performed. | Runtime object preview/world adapter state remains in `SectorEditorState`; dynamic door collider integration still reads the existing runtime object collider vector. Broad `SectorEditorState&` dependencies remain in preview overlay/UV panel for document/UI/material/runtime concerns and are deferred to later phases. |
| Phase 5: Preview Runtime Object And World Adapter State | Completed | 2026-07-07 | Added `SectorEditorPreviewRuntimeState` under `preview/SectorEditorPreviewState.h` and moved `SectorRuntimeObjectState runtimeObjects` out of `SectorEditorState` into `SectorEditorPreviewState::runtime`. Retargeted preview runtime object clear/reset/spawn/update, dynamic door collider and portal blocker buffers, object probe debug refresh/draw/status, door runtime lighting, and placed-object inspector/debug reads to use preview runtime state explicitly. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`; grep checks for runtime object fields, `SectorRuntimeObjectState`, broad preview/tool dependencies, and `SectorTopologyMap::runtimeObjects`. | `runtimeObjects` moved because inspection confirmed it is transient preview/runtime world adapter state: spawned entity links, asset scope handle, runtime counters/warnings, dynamic door collision buffers, object probes, and object sector lookup. `SectorTopologyMap::runtimeObjects` map-authored definitions were not moved. `runtimeObjectDrag`, `spritePicker`, `spriteMetadataCatalog`, and billboard metadata repair fields were intentionally left as editing/modal state. Runtime object asset scope/resource lifetime remains routed through existing `ClearSectorRuntimeObjects()`, reset/load/shutdown, preview enter, and preview rebuild paths. Preview runtime object update/render behavior, object probe debug behavior, collision, sector lookup, physics, and camera behavior were intended unchanged. No topology mutation or 2D topology render-cache invalidation behavior changed. Lightmap source-hash behavior was unchanged. No manual GUI verification was performed. | Runtime object editing state and sprite picker/modal state remain in `SectorEditorState`. Broad `SectorEditorState&` dependencies remain in preview overlay/UV panel and placed-object/billboard/door helpers for document/UI/editing concerns. Renderer lifecycle dependency cleanup remains for Phase 6. |
| Phase 6: Preview Renderer Lifecycle Dependency Cleanup | Completed | 2026-07-07 | Narrowed preview overlay and preview UV panel dependencies after the preview state moves. Preview overlay/UV panel headers no longer include `SectorEditorTypes.h` or take broad `SectorEditorState&` / `SectorEditorUiState&`; they now receive explicit topology map, authoring graph/derivation, preview state, selection/manipulation/material/light state, texture picker state, document dirty flag, and narrow UI dependencies as needed. Added a narrow `BuildSectorEditorSurface3DTargetLabel()` overload so the UV panel can build the same label without full editor state. Simplified preview control-mode toggle action to receive a preview-active bool, removing the remaining `SectorEditorTypes.h` dependency from preview actions. No preview fields moved in this phase. | Passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; grep checks for broad preview/action dependencies and renderer lifecycle ownership symbols. `git diff --stat` and `git status --short` reviewed after the plan update. | `SectorMeshRenderer preview` remains owned by `SectorEditor`. Preview renderer GPU resource lifetime, explicit enter/rebuild/leave calls, preview mesh rebuild timing, missing-texture fallback behavior, and rendering output were intended unchanged. No optional preview controller/helper was created. No topology mutation or 2D topology render-cache invalidation behavior changed. Lightmap source-hash behavior was unchanged. Collision, sector lookup, physics, and camera behavior were intended unchanged. No manual GUI verification was performed. | `DocumentState` ownership remains for REF-086. Preview overlay/UV panel still intentionally receive document/topology and modal/picker pieces where behavior crosses document ownership. Runtime object editing state, sprite picker/modal state, and any optional narrow preview controller remain future debt. |
| Phase 7: Final Dependency Audit And Backlog Refresh | Completed | 2026-07-07 | Documentation-only final audit/backlog refresh completed. Confirmed the REF-085 preview-owned fields now live under `SectorEditorPreviewState` sub-states composed by `SectorEditor`, `SectorEditorState` no longer owns those preview fields, preview modules/actions no longer take broad `SectorEditorState&` / `SectorEditorUiState&` or include `SectorEditor.h`, and `SectorMeshRenderer preview` remains owned by `SectorEditor`. Refreshed `docs/audit/sector_editor_state_ownership_and_remaining_map.md` and updated REF-085 backlog completion notes while keeping REF-086 open. | Passed: grep checks for moved fields and broad preview dependencies; `cmake --build cmake-build-debug -j2` (no work to do); `ctest --test-dir cmake-build-debug --output-on-failure` (16/16); `git diff --check`; `git diff --stat`; `git status --short`. | No behavior changes in this phase. No topology mutation or 2D topology render-cache invalidation behavior changed. Lightmap source-hash behavior was unchanged. Serialization/schema behavior was unchanged. Rendering, collision, sector lookup, physics, and camera behavior were unchanged. No manual GUI verification was performed. | REF-086 `DocumentState` ownership remains open. Runtime object editing state (`runtimeObjectDrag`), sprite picker/catalog and billboard metadata repair state, preview settings modal/UI input state, material picker routing document dependencies, placed-object editing broad state dependencies, manipulation broad state dependencies, inspector broad state/UI dependencies, and manual preview smoke remain future debt. |

## Execution Tracking Rules

- Each phase must compile and pass tests before moving to the next phase.
- Each implementation phase must be independently buildable and testable.
- Each phase must update the `plan-state-json` item status and the Current Progress table before finishing.
- Use `In Progress` only while source/doc changes are actively incomplete.
- Mark a phase `Completed` only after implementation, verification, behavior notes, and plan updates are done.
- If a phase is intentionally skipped, mark it `Deferred` and record why.
- If a phase cannot proceed without a decision, mark it `Blocked` and record the exact blocker.
- If a phase proves too broad, split it into child pass items in the JSON block and Current Progress table, then stop.
- If Phase 3, Phase 4, or Phase 5 would touch too many unrelated preview call sites in one run, split that phase into child passes in the `plan-state-json` block and Current Progress table, then stop without source changes.
- Suggested child-pass examples, if needed: `phase_03a` controller/config fields only, `phase_03b` visual pose effects, `phase_03c` spotlight preview restore, `phase_04a` collision world/validity/warning, `phase_04b` collision movement/vertical result state, `phase_04c` sector lookup/noclip fallback state, `phase_05a` runtime object preview state inventory, and `phase_05b` move `runtimeObjects` only if clean.
- Do not rewrite unrelated phase text while updating progress.
- Do not claim manual GUI verification unless it was actually performed.
- Do not mark any PreviewState implementation phase complete from the original planning task alone.

## Summary

REF-085 prepares the sector editor preview/runtime state for reuse by moving preview-owned state out of the monolithic `SectorEditorState` into narrow preview-owned state structs under `sources/sector_editor/preview/`.

This plan is not a game-mode plan. It keeps `SectorEditor` responsible for high-level preview enter/update/render/leave orchestration until later phases prove a smaller preview controller is worthwhile. The immediate goal is to stop preview modules, preview helpers, and preview UI from needing document UI, modal state, inspector buffers, and unrelated editor state just to access preview runtime state.

## Architecture Contract

- `SectorEditor` composes preview state, renderer, services, and document state.
- `SectorEditor` remains the high-level coordinator for entering preview, rebuilding preview meshes, updating preview, rendering preview, and leaving preview.
- `SectorEditorPreviewState` is a small aggregate/root composed by `SectorEditor`; preview fields are grouped into narrow named sub-states by responsibility by default.
- Preview state owns transient preview/runtime/controller/collision state only.
- Document/source-of-truth state remains outside preview state.
- Preview helpers and UI receive explicit state/service/context references.
- Preview modules must not include `SectorEditor.h`.
- Preview services/helpers must not call `SectorEditor::` methods.
- Do not create generic infrastructure. Allowed additions are narrow state structs, small helper functions/modules, and only an optional narrow `SectorEditorPreviewController` if a later phase proves it is just moving existing preview lifecycle orchestration.

## Current Problem

`SectorEditorState` still owns preview/runtime/gameplay state mixed with document state, modal state, inspector state, and 2D editor state.

Current preview-owned candidates in `sources/sector_editor/SectorEditorTypes.h` include:

- `useBakedAmbientOcclusion`
- `showObjectProbeDebugOverlay`
- `runtimeObjects`
- `previewUiHidden`
- `activePreviewDebugOverlayTab`
- `previewControlMode`
- `freeflyController`
- `fpsControllerConfig`
- `fpsControllerState`
- `sectorCollisionWorld`
- `sectorCollisionWorldValid`
- `sectorCollisionWorldWarning`
- `previewCollisionSectorId`
- `previewVerticalResult`
- `previewMoveResult`
- `previewCollisionNoclipFallback`
- `visualStepOffsetY`
- `headBobState`
- `landingDipState`
- `hasPreviewPose`
- `lastPreviewPose`
- `spotLightPilotPreviewRestore`
- `hoveredSurface3D`
- `selectedSurface3D`
- `selectedTopologySurface3D`

Related preview UI/modal fields currently remain outside this plan unless a phase explicitly proves they are preview-owned and transient:

- `previewSettingsModal` remains modal UI state.
- `objectProbeDebugDrawMaxDistanceInput` remains UI input state unless a later UI-state plan moves it.
- Runtime object inspector, sprite picker, billboard metadata, and `runtimeObjectDrag` remain runtime-object editing or modal/tool state unless Phase 5 proves a narrow preview-runtime subset.

## Target Preview State Ownership Model

Default state shape:

- `SectorEditorPreviewState`: small aggregate/root composed by `SectorEditor`. It should group state into narrow named sub-states by responsibility.
- `SectorEditorPreviewUiState` or `SectorEditorPreviewOverlayState`: owns `previewUiHidden`, `activePreviewDebugOverlayTab`, `showObjectProbeDebugOverlay`, and `useBakedAmbientOcclusion` only if verified preview-render-only.
- `SectorEditorPreviewSelectionState`: owns `hoveredSurface3D`, `selectedSurface3D`, and `selectedTopologySurface3D`.
- `SectorEditorPreviewControllerState`: owns `previewControlMode`, `freeflyController`, `fpsControllerConfig`, `fpsControllerState`, `visualStepOffsetY`, `headBobState`, `landingDipState`, `hasPreviewPose`, `lastPreviewPose`, and `spotLightPilotPreviewRestore` only after each field is verified non-serialized preview/controller state.
- `SectorEditorPreviewCollisionState`: owns `sectorCollisionWorld`, `sectorCollisionWorldValid`, `sectorCollisionWorldWarning`, `previewCollisionSectorId`, `previewVerticalResult`, `previewMoveResult`, and `previewCollisionNoclipFallback`.
- `SectorEditorPreviewRuntimeState`: owns `SectorRuntimeObjectState runtimeObjects` only if Phase 5 proves it is preview/runtime adapter state and not document-authored object data or editing UI state.

New preview fields should not be dumped flat into one giant `SectorEditorPreviewState` unless a phase explicitly justifies why the group is tiny and not expected to grow. Sub-states may be embedded in `SectorEditorPreviewState` or moved to separate headers when phase scope and include blast radius justify it. If different names are chosen, the phase must explain why in the final report and Current Progress notes.

## Non-Negotiable Guardrails

- Do not change editor behavior intentionally.
- Do not implement game mode.
- Do not move `DocumentState` as part of REF-085.
- Do not move authoring graph ownership.
- Do not move topology derivation ownership.
- Do not move save/load/reset/import/document dirty/path/status ownership.
- Do not move source-of-truth map editing into `SectorTopologyMap`.
- Do not reintroduce no-authoring topology edit fallback.
- Do not change serialization/schema.
- Do not change lightmap/source-hash behavior.
- Do not change rendering output intentionally.
- Do not change collision, sector lookup, physics, or camera behavior intentionally.
- Preserve the rule that visual camera effects such as step smoothing, headbob, landing dip, and other visual offsets never feed collision, sector lookup, or physics.
- Preserve preview renderer GPU resource lifetime and explicit preview enter/rebuild/leave behavior.
- Do not make preview modules depend on `SectorEditor.h`.
- Do not call `SectorEditor::` methods from preview services/helpers.
- Do not create callback bridges back into `SectorEditor` to hide ownership problems.
- Do not create another god state object under a new name.
- Do not dump unrelated preview fields flat into one large `SectorEditorPreviewState`; narrow sub-states are the default implementation target.
- Do not put editor-only modal/inspector/UI buffer state into runtime-reusable preview state.
- Do not move `SectorMeshRenderer` ownership in REF-085 implementation phases.
- Do not add a generic `EditorStateService`, `PreviewManager`, `GameModeManager`, `SceneManager`, `WorldManager`, `TopologyService`, service locator, event bus, plugin architecture, render graph, new ECS framework, new asset/resource manager abstraction, callback bundle layer, broad inheritance hierarchy, or abstract preview interface hierarchy.

## Baseline Files To Inspect

Before implementing any phase, inspect at least:

- `docs/architecture/sector_editor_architectural_principles.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `docs/plans/ref084_service_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/runner_compatible_plans.md`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorPreviewTypes.h`
- `sources/sector_editor/SectorEditorSurfaceTypes.h`
- `sources/sector_editor/SectorEditorSelectionTypes.h`
- `sources/sector_editor/SectorEditorModalTypes.h`
- `sources/sector_editor/preview/`
- `sources/sector_editor/inspector/`
- `sources/sector_editor/tools/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_demo/renderer/`
- `sources/sector_demo/SectorCollisionWorld.*`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorTopologyTypes.*`
- `sources/sector_demo/SectorRuntimeObjects.*`

## Phase 1: PreviewState Inventory And Low-Risk Preview Control Flags

### Goal

Create the initial narrow preview state file and move low-risk preview UI/control flags out of `SectorEditorState` without touching controller, collision, renderer, document, or runtime object ownership.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorPreviewTypes.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/preview/SectorEditorPreviewOverlay.*`
- `sources/sector_editor/preview/SectorEditorPreviewUvPanel.*`
- `sources/sector_editor/inspector/SectorEditorInspectorPanel.*`
- `sources/sector_editor/SectorEditorPreviewSettingsModal.*`

### Files Likely To Modify

- Add `sources/sector_editor/preview/SectorEditorPreviewState.h`.
- Modify `sources/sector_editor/SectorEditor.h`.
- Modify `sources/sector_editor/SectorEditorTypes.h`.
- Modify preview overlay/UV panel contexts and call sites.
- Modify focused call sites in `SectorEditor.cpp`.

### Exact Implementation Steps

1. Add `SectorEditorPreviewState` under `sources/sector_editor/preview/` as a small aggregate/root, with a narrow `SectorEditorPreviewUiState` or `SectorEditorPreviewOverlayState` sub-state by default.
2. Move these exact fields from `SectorEditorState` to the preview UI/overlay sub-state: `previewUiHidden`, `activePreviewDebugOverlayTab`, and `showObjectProbeDebugOverlay`.
3. Evaluate `useBakedAmbientOcclusion`. Move it only if inspection confirms it is preview-render-only and not document/source-of-truth, serialized, bake-affecting, or source-hash-affecting. If uncertain, leave it in `SectorEditorState` and record debt.
4. Compose `SectorEditorPreviewState previewState;` in `SectorEditor`.
5. Update call sites to use `previewState.previewUiHidden`, `previewState.activePreviewDebugOverlayTab`, `previewState.showObjectProbeDebugOverlay`, and optionally `previewState.useBakedAmbientOcclusion`.
6. Pass `SectorEditorPreviewState&` into preview overlay/UV panel contexts where those flags are read or changed.
7. Keep `previewControlMode`, controller state, surface selection, collision state, runtime object state, and renderer ownership in their existing locations for later phases.
8. Compile and pass tests before moving to Phase 2.

### Behavior Guardrails

- Hidden-preview UI hotkey behavior must remain unchanged.
- Preview overlay tab selection and overlay interaction rect behavior must remain unchanged.
- Object probe debug overlay visibility must remain unchanged.
- Baked AO preview behavior must remain unchanged if moved; source-hash behavior must remain unchanged.
- No topology mutation or cache invalidation behavior should change.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

### Grep Checks

- `rg -n "previewUiHidden|activePreviewDebugOverlayTab|showObjectProbeDebugOverlay|useBakedAmbientOcclusion" sources/sector_editor`
- `rg -n "SectorEditorState&|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/preview`
- Confirm moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if `useBakedAmbientOcclusion` is serialized, document-owned, or source-hash-affecting.
- Stop if the implementation would flatten unrelated future preview state into `SectorEditorPreviewState` instead of creating the preview UI/overlay sub-state.
- Stop if moving flags requires moving controller/collision/runtime state in the same phase.
- Stop if preview modules would need a new callback bridge to reach `SectorEditor`.

### Final Report Requirements

- List exact fields moved and any field intentionally left.
- State that no game mode was implemented.
- State whether `useBakedAmbientOcclusion` moved and why.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior.
- State that collision, sector lookup, physics, and camera behavior were unchanged.

### What Remains For Later Phases

- Preview surface selection.
- Preview control mode and controller/camera state.
- Collision world and sector lookup state.
- Runtime object preview state.
- Renderer lifecycle dependency cleanup.

### Phase Answers

- Fields move from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorPreviewUiState` or `SectorEditorPreviewOverlayState`, composed by `SectorEditorPreviewState` in `preview/SectorEditorPreviewState.h`.
- `SectorEditor` owns the new state and passes it explicitly to preview UI/helpers.
- Changed call sites are expected in `SectorEditor.cpp`, preview overlay, and possibly inspector/settings UI.
- Moved state is editor-only preview UI/control state, not serialized.
- Debt after the phase is all non-flag preview runtime state.

## Phase 2: Preview Surface Selection State

### Goal

Move preview-specific 3D surface hover/selection state out of `SectorEditorState` without mixing it into 2D `SelectionState` or document ownership.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorSurfaceTypes.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/selection/SectorEditorSelectionService.*`
- `sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.*`
- `sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.*`
- `sources/sector_editor/services/lights/SectorEditorLightEditingService.*`
- `sources/sector_editor/preview/SectorEditorPreviewOverlay.*`
- `sources/sector_editor/preview/SectorEditorPreviewUvPanel.*`
- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.*`

### Files Likely To Modify

- `sources/sector_editor/preview/SectorEditorPreviewState.h`
- `sources/sector_editor/preview/SectorEditorPreviewSelectionState.h` or an equivalent narrow selection sub-state embedded in the preview state header.
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- Preview, material, light, and selection service context headers/callers.

### Exact Implementation Steps

1. Move these exact fields from `SectorEditorState`: `hoveredSurface3D`, `selectedSurface3D`, and `selectedTopologySurface3D`.
2. Use `SectorEditorPreviewSelectionState` by default, either in its own header or embedded in `SectorEditorPreviewState.h` if that is the smallest include change.
3. Update selection service context to receive preview-surface references from preview state instead of `SectorEditorState`.
4. Update material editing service and material picker routing to read/write selected preview surface state through explicit preview selection dependencies.
5. Update light editing service dependencies that clear selected preview surfaces.
6. Update `SelectSurface3D()`, `EnsureSelectedSurface3DAuthoringMappingCurrent()`, `ResetSurface3DUiState()`, preview hover, highlight drawing, material panel routing, and UV/decal editing call sites.
7. Preserve selection reset ordering when 2D selections, lights, runtime objects, or material actions clear preview surface selection.
8. Compile and pass tests before moving to Phase 3.

### Behavior Guardrails

- Preview3D hover highlights must remain unchanged.
- Selected surface material panel behavior must remain unchanged.
- Texture, UV, and decal editing targets must remain unchanged.
- Material picker routing behavior must remain unchanged.
- 2D `SelectionState` ownership must not absorb preview 3D surface state.
- Document/source-of-truth ownership must not move.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add or update targeted tests only if existing tests cover selection reset/material routing contexts and need fixture retargeting. Do not add asset-tree-dependent tests.

### Grep Checks

- `rg -n "hoveredSurface3D|selectedSurface3D|selectedTopologySurface3D" sources/sector_editor tests`
- `rg -n "SectorEditorState&|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/preview sources/sector_editor/services/material_edit sources/sector_editor/selection sources/sector_editor/services/lights`
- Confirm the moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if moving surface selection requires moving authoring graph or topology derivation ownership.
- Stop if material edits would fall back to direct topology mutation.
- Stop if callback bridges back to `SectorEditor` are introduced.
- Stop if the implementation would store preview surface selection as unrelated flat fields in a growing root `SectorEditorPreviewState`.

### Final Report Requirements

- List exact surface fields moved and new owner.
- State which service/UI contexts now receive preview selection state.
- State that 2D `SelectionState` was not broadened with preview-only state.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior.
- State that rendering output, collision, sector lookup, physics, and camera behavior were intended unchanged.

### What Remains For Later Phases

- Controller/camera state.
- Collision/sector lookup state.
- Runtime object preview state.
- Renderer lifecycle and dependency cleanup.

### Phase Answers

- Fields move from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorPreviewSelectionState` under `preview/`, composed by `SectorEditorPreviewState`.
- `SectorEditor` owns the state; preview UI, selection service, light service, and material service receive explicit references.
- Moved state is editor-only preview interaction state, not serialized.
- Debt after the phase is service dependency narrowing that still needs broader document state for material routing.

## Phase 3: Preview Controller And Camera State

### Goal

Move preview controller/camera state into preview-owned state while preserving the separation between physical/collision pose and visual-only camera effects.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorPreviewTypes.h`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/preview/SectorEditorPreviewOverlay.*`
- `sources/sector_editor/services/lights/SectorEditorLightEditingService.*`
- `sources/sector_demo/SectorFreeflyController.*`
- `sources/sector_demo/SectorFpsController.*`
- `sources/sector_demo/renderer/SectorMeshRenderer.*`

### Files Likely To Modify

- `sources/sector_editor/preview/SectorEditorPreviewState.h`
- `sources/sector_editor/preview/SectorEditorPreviewControllerState.h` or an equivalent narrow controller sub-state embedded in the preview state header.
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/SectorEditor.cpp`
- Preview overlay/settings call sites.

### Exact Implementation Steps

1. Before moving `fpsControllerConfig`, `previewControlMode`, or any setting-like preview field, inspect whether it is persisted, stored in map preview settings, used by serialization, or treated as document/source-of-truth data.
2. If the field is transient preview/controller state, move it to `SectorEditorPreviewControllerState`.
3. If the field is persisted document/map preview settings, do not move ownership in REF-085; leave it with document/topology settings and pass it explicitly into preview update/apply code.
4. If ownership is unclear, stop and record the exact field and call sites as debt.
5. Move these exact fields from `SectorEditorState` only after the ownership check passes: `previewControlMode`, `freeflyController`, `fpsControllerConfig`, `fpsControllerState`, `visualStepOffsetY`, `headBobState`, `landingDipState`, `hasPreviewPose`, `lastPreviewPose`, and `spotLightPilotPreviewRestore`.
6. Use `SectorEditorPreviewControllerState` by default, either in its own header or embedded in `SectorEditorPreviewState.h` if that is the smallest include change.
7. Update `ActiveSectorEditorPreviewPose()`, `ApplySectorEditorGameplayPoseToPreview()`, `ToggleSectorEditorPreviewControlMode()`, `InitializeSectorEditorGameplayVerticalState()`, and `UpdateSectorEditorGameplayPreview()` to receive preview controller state explicitly.
8. If Phase 4 has not moved collision state yet, pass collision state fields explicitly from `SectorEditorState` without moving them in this phase.
9. Preserve `SectorEditor` wrapper methods such as `ActivePreviewPose()`, `ApplyGameplayPoseToPreview()`, `TogglePreviewControlMode()`, spotlight pilot apply/cancel/restore, and preview enter/leave orchestration.
10. Update preview settings modal apply/read/write paths for `fpsControllerConfig` without moving modal UI state.
11. Keep renderer ownership in `SectorEditor`.
12. Compile and pass tests before moving to Phase 4.

### Behavior Guardrails

- Collision must continue using physical `fpsControllerState.feetPosition`, not visual offsets.
- Sector lookup must continue using physical position/current sector, not visual offsets.
- Camera rendering may still apply visual-only offsets through `SectorFpsControllerVisualPose()`.
- Step smoothing, headbob, and landing dip must remain visual-only.
- Spotlight pilot apply/cancel/restore behavior must remain unchanged.
- Freefly mouse-look behavior must remain unchanged.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update targeted tests only if existing preview action tests or controller fixtures exist and need retargeting.

### Grep Checks

- `rg -n "previewControlMode|freeflyController|fpsControllerConfig|fpsControllerState|visualStepOffsetY|headBobState|landingDipState|hasPreviewPose|lastPreviewPose|spotLightPilotPreviewRestore" sources/sector_editor tests`
- `rg -n "SectorEditorState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/preview sources/sector_editor/SectorEditorPreviewActions.*`
- Confirm moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if moving controller state changes collision/sector lookup inputs.
- Stop if `fpsControllerConfig`, `previewControlMode`, or any setting-like preview field is persisted, stored in map preview settings, used by serialization, or otherwise document/source-of-truth owned.
- Stop if ownership of a setting-like field is unclear; record the exact field and call sites as debt.
- Stop if spotlight pilot restore would require a callback bridge to `SectorEditor`.
- Stop if preview settings modal state would need to move into reusable preview runtime state.
- Stop if the implementation would store controller/camera fields as unrelated flat fields in a growing root `SectorEditorPreviewState`.

### Final Report Requirements

- List exact controller/camera fields moved and new owner.
- State the serialized/settings ownership check result for `fpsControllerConfig`, `previewControlMode`, and any other setting-like field touched.
- State how physical pose and visual camera effects remain separated.
- State spotlight pilot restore behavior.
- Mention collision/sector lookup/physics/camera behavior explicitly.
- Mention topology render-cache invalidation and lightmap source-hash behavior.

### What Remains For Later Phases

- Collision world/result state if not already moved.
- Runtime object preview/world adapter state.
- Renderer lifecycle dependency cleanup.

### Phase Answers

- Fields move from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorPreviewControllerState` under `preview/`, composed by `SectorEditorPreviewState`, only after serialized/settings ownership checks pass.
- `SectorEditor` owns the state; preview action helpers and overlay/settings paths receive explicit references.
- Moved state is preview-runtime transient and potentially game-mode reusable except spotlight-pilot restore, which is editor preview coordination state. None is serialized; persisted map/document settings stay outside REF-085 preview state.
- Debt after the phase is remaining collision/runtime dependency still needed by controller update.

## Phase 4: Preview Collision And Sector Lookup State

### Goal

Move preview collision world/results and current sector lookup state into preview-owned state without changing collision behavior.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_demo/SectorCollisionWorld.*`
- `sources/sector_demo/SectorFpsController.*`
- `sources/sector_demo/SectorRuntimeObjects.*`
- `sources/sector_demo/renderer/SectorMeshRenderer.*`
- `sources/sector_demo/SectorPortalVisibility.*`

### Files Likely To Modify

- `sources/sector_editor/preview/SectorEditorPreviewState.h`
- `sources/sector_editor/preview/SectorEditorPreviewCollisionState.h` or an equivalent narrow collision sub-state embedded in the preview state header.
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/SectorEditor.cpp`
- Preview overlay/debug display call sites.

### Exact Implementation Steps

1. Move these exact fields from `SectorEditorState`: `sectorCollisionWorld`, `sectorCollisionWorldValid`, `sectorCollisionWorldWarning`, `previewCollisionSectorId`, `previewVerticalResult`, `previewMoveResult`, and `previewCollisionNoclipFallback`.
2. Use `SectorEditorPreviewCollisionState` by default, either in its own header or embedded in `SectorEditorPreviewState.h` if that is the smallest include change.
3. Update `RebuildSectorEditorCollisionWorld()` to receive the topology map as input and preview collision/controller state as mutable outputs.
4. Update vertical context and gameplay update helpers to receive collision state explicitly.
5. Preserve build/rebuild timing at preview enter, preview mesh rebuild, settings apply, load/reset, and explicit inspector requests.
6. Preserve dynamic door collider integration with runtime object state by passing only the needed runtime collider vectors.
7. Preserve dynamic lighting/visibility lookup use of collision world where currently wired.
8. Compile and pass tests before moving to Phase 5.

### Behavior Guardrails

- Collision build/rebuild timing must remain unchanged.
- Sector lookup behavior must remain unchanged.
- Player movement and collision response must remain unchanged.
- Noclip fallback behavior must remain unchanged.
- Cache invalidation/rebuild behavior must remain unchanged.
- Visual offsets must not feed collision, sector lookup, or physics.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update targeted collision/controller tests only if existing tests need context retargeting.

### Grep Checks

- `rg -n "sectorCollisionWorld|sectorCollisionWorldValid|sectorCollisionWorldWarning|previewCollisionSectorId|previewVerticalResult|previewMoveResult|previewCollisionNoclipFallback" sources/sector_editor sources/sector_demo tests`
- `rg -n "SectorEditorState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/preview sources/sector_editor/SectorEditorPreviewActions.*`
- Confirm moved fields no longer appear as members of `SectorEditorState`.

### Stop Conditions

- Stop if movement resolution changes are required.
- Stop if generated render triangles would become collision input.
- Stop if collision state movement would require moving document topology ownership.
- Stop if the implementation would store collision/sector lookup fields as unrelated flat fields in a growing root `SectorEditorPreviewState`.

### Final Report Requirements

- List exact collision fields moved and new owner.
- State build/rebuild timing was preserved.
- State collision, sector lookup, physics, and camera behavior explicitly.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior.

### What Remains For Later Phases

- Runtime object preview/world adapter state.
- Renderer lifecycle dependency cleanup.
- Remaining broad state dependencies in preview modules.

### Phase Answers

- Fields move from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorPreviewCollisionState` under `preview/`, composed by `SectorEditorPreviewState`.
- `SectorEditor` owns the state; preview action helpers, renderer rebuild paths, and overlay/debug paths receive explicit references.
- Moved state is preview-runtime transient and potentially game-mode reusable. It is not serialized.
- Debt after the phase is runtime object collision integration and any remaining broad document dependencies.

## Phase 5: Preview Runtime Object And World Adapter State

### Goal

Move preview runtime object/world adapter state into preview-owned state if practical, while keeping map-authored runtime object definitions and runtime object editing UI/tool state out of preview runtime state.

### Files To Inspect

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/tools/placed_objects/`
- `sources/sector_editor/tools/billboards/`
- `sources/sector_editor/tools/doors/`
- `sources/sector_editor/preview/SectorEditorPreviewOverlay.*`
- `sources/sector_demo/SectorRuntimeObjects.*`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/renderer/SectorMeshRenderer.*`

### Files Likely To Modify

- `sources/sector_editor/preview/SectorEditorPreviewState.h`
- `sources/sector_editor/preview/SectorEditorPreviewRuntimeState.h` or an equivalent narrow runtime sub-state embedded in the preview state header.
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.cpp`
- Preview overlay/debug paths.
- Placed-object action/context files only where they need runtime preview adapter state.

### Exact Implementation Steps

1. Move `runtimeObjects` from `SectorEditorState` only if inspection confirms current uses are preview/runtime world adapter state and not document-authored object definitions or editing UI state.
2. Use `SectorEditorPreviewRuntimeState` with a `SectorRuntimeObjectState runtimeObjects;` member by default if moved, either in its own header or embedded in `SectorEditorPreviewState.h` if that is the smallest include change.
3. Keep `state.topologyMap.runtimeObjects` as map/document data.
4. Do not move `runtimeObjectDrag` in this phase unless a small local context retarget is required and the final report documents why. Default is to leave it for `RuntimeObjectEditingState`.
5. Do not move `spriteMetadataCatalog`, `spritePicker`, billboard metadata repair fields, or runtime object inspector UI inputs unless the phase proves they are preview-runtime state. Default is to leave them as editing/modal state.
6. Update runtime object clear/reset/spawn/update, door collider collection, object-probe debug refresh, object sector lookup, and object baked-lighting paths to use preview runtime state.
7. Preserve runtime object asset scope upload/unload ordering through existing `ClearSectorRuntimeObjects()`, reset, shutdown, enter preview, and load/reset paths.
8. Compile and pass tests before moving to Phase 6.

### Behavior Guardrails

- Map runtime object definitions must remain document/topology data.
- Runtime object editing UI state must not move into preview runtime state.
- Sprite picker/modal state must not move unless explicitly justified.
- Runtime object drag remains editing/manipulation debt by default.
- Preview runtime object update/render behavior must remain unchanged.
- Object probe debug data and status behavior must remain unchanged.
- Asset scope lifetime and main-thread GPU resource rules must remain unchanged.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Add/update runtime-object tests only if existing tests need context retargeting.

### Grep Checks

- `rg -n "runtimeObjects|runtimeObjectDrag|spritePicker|spriteMetadataCatalog|billboardMetadata" sources/sector_editor sources/sector_demo tests`
- `rg -n "SectorRuntimeObjectState" sources/sector_editor sources/sector_demo tests`
- `rg -n "SectorEditorState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/preview sources/sector_editor/tools/placed_objects sources/sector_editor/tools/billboards sources/sector_editor/tools/doors`
- Confirm `SectorTopologyMap::runtimeObjects` was not moved.

### Stop Conditions

- Stop if runtime object editing state and preview runtime state cannot be separated in one buildable phase.
- Stop if moving runtime state would change asset lifetime or world reservation timing.
- Stop if this phase starts becoming a generic runtime world manager.
- Stop if the implementation would store runtime object/world adapter fields as unrelated flat fields in a growing root `SectorEditorPreviewState`.

### Final Report Requirements

- State whether `runtimeObjects` moved or was deferred, and why.
- List any exact fields moved and exact fields intentionally left.
- State that topology map runtime object definitions were not moved.
- State asset scope/resource lifetime behavior.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior.
- State collision, sector lookup, physics, and camera behavior explicitly.

### What Remains For Later Phases

- Runtime object editing state, if left.
- Sprite picker/modal state, if left.
- Renderer lifecycle dependency cleanup.
- Any placed-object dependency cleanup requiring `DocumentState`.

### Phase Answers

- Candidate field moves from `SectorEditorState` in `SectorEditorTypes.h` to `SectorEditorPreviewRuntimeState` under `preview/`, composed by `SectorEditorPreviewState`, only if Phase 5 proves it is clean preview/runtime adapter state.
- `SectorEditor` owns the state; runtime object helpers receive explicit runtime state and document map references.
- Moved state is preview-runtime transient and potentially game-mode reusable. It is not serialized.
- Debt after the phase is runtime object editing/modals and document-owned definitions.

## Phase 6: Preview Renderer Lifecycle Dependency Cleanup

### Goal

Account for renderer resource lifetime, preview mesh rebuild, enter/leave behavior, and GPU resource ownership after preview state moves. Reduce broad `SectorEditorState` dependencies in preview modules where practical. REF-085 does not move renderer ownership.

### Files To Inspect

- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/preview/SectorEditorPreviewOverlay.*`
- `sources/sector_editor/preview/SectorEditorPreviewUvPanel.*`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_demo/renderer/SectorMeshRenderer.*`
- `sources/sector_demo/renderer/`
- `sources/sector_demo/SectorCollisionWorld.*`
- `sources/sector_demo/SectorRuntimeObjects.*`

### Files Likely To Modify

- `sources/sector_editor/preview/SectorEditorPreviewState.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- Preview overlay/UV panel/action context headers.
- Small preview helper modules only if they extract existing behavior without changing semantics.

### Exact Implementation Steps

1. Inventory all remaining `SectorEditorState&` and `SectorEditorUiState&` dependencies under `sources/sector_editor/preview/`.
2. Replace broad preview-module dependencies with `SectorEditorPreviewState&`, `SelectionState&`, `MaterialEditingUiState&`, `LightEditingState&`, `SectorTopologyMap&`, and explicit services where practical.
3. Keep `SectorMeshRenderer preview;` owned by `SectorEditor`. REF-085 implementation phases must not move `SectorMeshRenderer` ownership.
4. Preserve explicit preview enter/rebuild/leave calls: `TryEnterPreview3D()`, `RebuildPreviewMeshesPreservingView()`, `RenderPreview3D*()`, and `LeavePreview3D()`.
5. Do not introduce a generic manager. If an optional `SectorEditorPreviewController` is proposed, it must be narrow, use existing lifecycle/update code, and be recorded as future debt rather than implemented casually.
6. If moving renderer ownership seems desirable, create a separate future runner plan or backlog item and stop.
7. Remove unnecessary `SectorEditorTypes.h` includes from preview modules where possible.
8. Compile and pass tests before moving to Phase 7.

### Behavior Guardrails

- Preview renderer GPU resource lifetime must remain unchanged.
- GPU upload/unload/resource lifetime must be preserved exactly.
- Preview mesh rebuild timing must remain unchanged.
- Preview enter/leave behavior must remain unchanged.
- Rendering output must not change intentionally.
- Missing/unloaded/failed textures must keep existing fallback behavior.
- No collision, sector lookup, physics, or camera behavior changes.
- No lightmap/source-hash behavior changes.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Manual preview smoke is recommended before claiming visual parity, but do not claim it unless performed.

### Grep Checks

- `rg -n "SectorEditorState&|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"|#include \"sector_editor/SectorEditorTypes.h\"" sources/sector_editor/preview sources/sector_editor/SectorEditorPreviewActions.*`
- `rg -n "SectorMeshRenderer preview|TryEnterPreview3D|LeavePreview3D|RebuildPreviewMeshesPreservingView|RenderPreview3D" sources/sector_editor`
- Confirm renderer ownership and lifecycle are intentionally documented.

### Stop Conditions

- Stop if renderer ownership movement appears necessary or desirable; create a separate future runner plan or backlog item instead.
- Stop if any change would alter GPU upload/unload/resource lifetime.
- Stop if the cleanup requires `DocumentState` extraction.
- Stop if a generic preview manager/controller starts accumulating unrelated editor state.

### Final Report Requirements

- State what remains owned by `SectorEditor`.
- State what moved into preview state.
- State whether any optional preview controller/helper was created or deferred.
- State that `SectorMeshRenderer` ownership stayed in `SectorEditor`.
- State preview renderer GPU resource lifetime and enter/rebuild/leave behavior.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior.
- State collision, sector lookup, physics, camera, and rendering behavior explicitly.

### What Remains For Later Phases

- `DocumentState` ownership work under REF-086.
- Any runtime object editing state split.
- Optional narrow preview controller if still justified.
- Manual GUI/preview smoke if not performed.

### Phase Answers

- Fields should already have moved in earlier phases; this phase mainly retargets module dependencies and records renderer ownership.
- `SectorEditor` remains renderer owner and passes preview state to helpers.
- Renderer state is GPU-resource/lifecycle state, editor-composed and not serialized.
- Debt after the phase is any remaining broad dependencies blocked by document ownership.

## Phase 7: Final Dependency Audit And Backlog Refresh

### Goal

Audit the resulting ownership shape, update this plan, backlog, and refreshed ownership audit, and record remaining debt without implementing new feature or cleanup work.

### Files To Inspect

- `docs/plans/ref085_preview_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/preview/`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/tools/placed_objects/`

### Files Likely To Modify

- `docs/plans/ref085_preview_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`

### Exact Implementation Steps

1. Run grep checks for all moved fields and broad preview dependencies.
2. Update Current Progress for every completed/deferred/blocked phase.
3. Update the backlog REF-085 completion notes only after implementation phases are truly complete.
4. Keep REF-086 open.
5. Refresh `docs/audit/sector_editor_state_ownership_and_remaining_map.md`.
6. The refreshed audit must include post-REF-085 `SectorEditorState` preview fields remaining, new preview state/sub-state objects composed by `SectorEditor`, remaining broad `SectorEditorState` / `SectorEditorUiState` dependencies in preview modules, remaining PreviewState debt, whether renderer ownership stayed in `SectorEditor`, whether collision/sector lookup/physics/camera behavior was intended unchanged, whether serialization/schema and lightmap source-hash behavior were unchanged, and that REF-086 `DocumentState` remains open.
7. Record any remaining preview debt, especially document-blocked dependencies, runtime object editing state, modal/UI state, and manual smoke gaps.
8. Do not make source code changes in this final audit phase unless fixing a build break from the immediately preceding phase.
9. Compile and pass tests before closing the plan.

### Behavior Guardrails

- No behavior changes in the final audit phase.
- Do not mark unrelated backlog items complete.
- Do not mark DocumentState work complete.
- Do not claim manual GUI verification unless performed.

### Tests And Checks

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

### Grep Checks

- `rg -n "previewUiHidden|activePreviewDebugOverlayTab|showObjectProbeDebugOverlay|previewControlMode|freeflyController|fpsControllerConfig|fpsControllerState|sectorCollisionWorld|previewCollisionSectorId|previewVerticalResult|previewMoveResult|previewCollisionNoclipFallback|visualStepOffsetY|headBobState|landingDipState|hasPreviewPose|lastPreviewPose|spotLightPilotPreviewRestore|hoveredSurface3D|selectedSurface3D|selectedTopologySurface3D|runtimeObjects" sources/sector_editor/SectorEditorTypes.h`
- `rg -n "SectorEditorState&|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/preview sources/sector_editor/SectorEditorPreviewActions.*`
- `rg -n "REF-085|REF-086" docs/plans/codebase_refactor_backlog.md`

### Stop Conditions

- Stop if implementation phases are incomplete; mark this phase `Blocked` or `Partial` instead of closing the plan.
- Stop if validation fails and source changes are required beyond small immediate fixes.
- Stop if backlog changes would imply REF-086 completion.

### Final Report Requirements

- Summarize final PreviewState ownership.
- List remaining preview debt.
- State REF-086 remains open.
- Summarize the required audit refresh.
- Mention topology render-cache invalidation behavior.
- Mention lightmap source-hash behavior.
- State rendering, collision, sector lookup, physics, and camera behavior.
- Report build, ctest, diff check, diff stat, and status results.

### What Remains For Later Phases

- REF-086 DocumentState ownership runner plan and implementation.
- Any runtime object editing/modal state plan.
- Any optional narrow preview controller follow-up.

### Phase Answers

- This phase moves no fields unless correcting immediately discovered integration mistakes.
- It updates plan/backlog/audit documentation only.
- It verifies the plan is complete and self-tracking.
- It records remaining debt after the preview state migration.

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

## Manual Smoke Suggestions

Manual preview smoke is recommended after phases that touch controller, collision, runtime object, or renderer lifecycle state:

- Enter Preview3D, leave Preview3D, and re-enter.
- Toggle preview UI hidden state.
- Switch FreeFly and Gameplay modes.
- Move through sectors with floor height changes and portals.
- Confirm step smoothing, headbob, and landing dip remain visual-only.
- Select 3D surfaces and edit material/UV/decal values.
- Start/cancel/apply spotlight pilot.
- Confirm runtime doors, billboards, object-probe overlay, and baked AO preview still display as expected.

Do not claim manual smoke verification unless it was actually performed.

## Backlog Updates

- The planning task that created this file may mark REF-085 as runner-plan written/planning complete only.
- Implementation phases must not be marked complete until this runner plan executes those phases.
- REF-086 must remain open for `DocumentState`.
- Do not mark unrelated backlog items complete.
- After final implementation, update REF-085 completion notes with exact moved state, verification results, behavior notes, and remaining debt.

## Stop Conditions

- Stop if a requested phase requires moving document/source-of-truth ownership.
- Stop if a phase would change serialization/schema.
- Stop if a phase would change lightmap source-hash behavior.
- Stop if a phase would change rendering, collision, sector lookup, physics, or camera behavior intentionally.
- Stop if a phase would implement game mode.
- Stop if a phase would introduce broad infrastructure listed in the guardrails.
- Stop if a phase requires preview modules to include `SectorEditor.h` or call `SectorEditor::` methods.
- Stop if a phase needs callback bridges back into `SectorEditor` to hide ownership problems.
- Stop if a phase would create a flat god `SectorEditorPreviewState` instead of narrow responsibility-based sub-states.
- Stop if Phase 3, Phase 4, or Phase 5 would touch too many unrelated preview call sites in one run; split that phase into child passes in the `plan-state-json` block and Current Progress table, then stop without source changes.
- Stop if a phase would move `SectorMeshRenderer` ownership; create a separate future runner plan or backlog item instead.

## Per-Phase Final Report Requirements

Each phase final report must include:

- exact fields moved
- source struct/file and destination state object/file
- changed owners/receivers
- major call sites changed
- behavior preserved
- tests added/updated, if any
- grep checks performed
- remaining debt
- compile/test results
- whether moved state is editor-only, preview-runtime, or potentially game-mode reusable
- whether moved state is serialized, expected to be no unless explicitly justified
- topology render-cache invalidation behavior
- lightmap source-hash behavior
- rendering behavior
- collision, sector lookup, physics, and camera behavior
- manual GUI verification, only if actually performed
