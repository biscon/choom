# REF-084 Service-Owned Editor State Migration Plan

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
9. After executing a phase/pass, update this plan with status, date, summary, verification results, and behavior notes.
10. Do not claim manual verification unless it was actually performed.
11. Keep this plan self-tracking so future fresh-context runs can resume from it.

```plan-state-json id="ref084-service-state-ownership"
{
  "plan_id": "ref084_service_state_ownership",
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
      "title": "TextureCatalogState",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "MaterialEditingState And Material UI State",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02a",
      "title": "MaterialEditingState only",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02b",
      "title": "MaterialEditingUiState / material input buffers",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "LightEditingState",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "SelectionState",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04a",
      "title": "SelectionState Core 2D Selection Fields",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04b",
      "title": "Authoring Selection Helper Retarget",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04c",
      "title": "Hover And Render-Cache Selection Context",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04d",
      "title": "Selection Service Dependency Narrowing",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "ManipulationState",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_06",
      "title": "InspectorUiState Feature Input Groups",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_07",
      "title": "PreviewState And DocumentState Planning Notes",
      "type": "phase",
      "status": "Completed"
    }
  ]
}
```

## Current Progress

| Phase / Pass | Status | Date | Notes |
| --- | --- | --- | --- |
| Phase 1: TextureCatalogState | Completed | 2026-07-07 | Source changed. Added `TextureCatalogState`; moved editor texture handle cache and editor texture scope out of `SectorEditorState`. `SectorTopologyMap::texturesById` remains the map-level registry. No serialization/schema, source-hash, or topology render-cache invalidation behavior changed. |
| Phase 2: MaterialEditingState And Material UI State | Completed | 2026-07-07 | Source changed across Phase 2A and Phase 2B. Material copied payload and material UI input buffers now live outside the monolithic editor state/UI state. |
| Phase 2A: MaterialEditingState only | Completed | 2026-07-07 | Source changed. Added `MaterialEditingState`; moved copied material payload out of `SectorEditorState` and into the material editing service context. Material UI input buffers, decal tint modal state, and generic texture picker lifecycle stayed in their existing owners. |
| Phase 2B: MaterialEditingUiState / material input buffers | Completed | 2026-07-07 | Source changed. Added `MaterialEditingUiState`; moved sidedef, sector material, and preview material input buffers out of `SectorEditorUiState`. Generic picker lifecycle and decal tint modal state stayed in existing owners. |
| Phase 3: LightEditingState | Completed | 2026-07-07 | Source changed. Added service-owned `LightEditingState`; moved light drag/edit transaction state and light-owned spotlight pilot restore data out of `SectorEditorState`. Preview pose/mouse-look restore stayed editor/preview-owned. |
| Phase 4: SelectionState | Completed | 2026-07-07 | Source changed across Phase 4A through Phase 4D. Selection state now owns core 2D/editor selected and hovered targets; selection service dependencies were narrowed while preview-surface selection ownership and manipulation transaction ownership remain deferred. |
| Phase 4A: SelectionState Core 2D Selection Fields | Completed | 2026-07-07 | Source changed. Added `SelectionState`, composed it in `SectorEditor`, and moved core non-preview selected target fields out of `SectorEditorState`: topology selection kind/IDs, selected side kind/wall part/material layer, selected runtime object ID, inspected topology vertex ID, and selected authoring target. Hover, preview-surface, and manipulation fields stayed in their existing owners/deferred passes. |
| Phase 4B: Authoring Selection Helper Retarget | Completed | 2026-07-07 | Source changed. Retargeted authoring selection helper functions for line, vertex, and face-anchor selection to take `const SectorAuthoringGraph&` plus `SelectionState&` instead of broad `SectorEditorState&`. Hover helper state remains in `SectorEditorState` for Phase 4C. |
| Phase 4C: Hover And Render-Cache Selection Context | Completed | 2026-07-07 | Source changed. Moved 2D hover fields into `SelectionState`; hover/update paths, select tool hover, light-service hover references, and topology render-cache draw-context creation now read/write those values through `SelectionState` without storing selection-state references in the cache. Preview-surface hover stayed deferred. |
| Phase 4D: Selection Service Dependency Narrowing | Completed | 2026-07-07 | Source changed. Narrowed `SectorEditorSelectionServiceContext` around `SelectionState`, topology map, authoring graph, authoring derivation read state, preview-surface references, drag reset references, explicit UI reset/input dependencies, material UI state, and light state. Preview-surface selection ownership stayed deferred. |
| Phase 5: ManipulationState | Completed | 2026-07-07 | Source changed. Added `ManipulationState`; moved select drag-arm and authoring vertex drag transaction state out of `SectorEditorState`. Light drag remains in `LightEditingState`; runtime object drag remains in placed-object/SectorEditor state as explicit debt. |
| Phase 6: InspectorUiState Feature Input Groups | Completed | 2026-07-07 | Source changed. Added `InspectorIdUiState`; moved selected sector/light ID buffers and shared ID edit error out of `SectorEditorUiState`. Other feature input groups stayed in top-level UI state or their existing narrow owners. |
| Phase 7: PreviewState And DocumentState Planning Notes | Completed | 2026-07-07 | Documentation-only checkpoint completed. Preview/runtime state and document/source-of-truth state remain deferred to separate future runner plans; backlog entries were added for those plans. |

Phase 1 verification, 2026-07-07:

- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks passed: texture catalog service has no `SectorEditorState&` and no `#include "sector_editor/SectorEditor.h"`; remaining `editorTextureScope` / `editorTextureHandlesById` references are in `TextureCatalogState`, `SectorEditor` lifecycle unload paths, and texture catalog service implementation.
- Behavior notes: texture scope unload ordering is preserved through the same shutdown/reset/refresh points; add-map modal lifecycle and preview scope stayed outside the catalog service; texture registry mutations still do not invalidate the 2D topology render cache by themselves; lightmap source-hash behavior was unchanged.

Phase 2A verification, 2026-07-07:

- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: `copiedTopologyMaterial` has no remaining matches; no `Scratch`, `writeback`, `no-authoring`, `TopologyOnly`, or `Legacy` matches remain in material service/tool/preview paths; `SectorEditorState&` / `SectorEditorUiState&` references still remain in `services/material_edit` for document/authoring access and Phase 2B UI-buffer reset behavior.
- Behavior notes: authoring graph remains the source of truth for material edits; material edit dirty/cache invalidation and preview rebuild behavior were unchanged; no map JSON/schema, lightmap source-hash, rendering, collision, texture picker lifecycle, or material UI input-buffer ownership behavior changed.

Phase 2B verification, 2026-07-07:

- Source changed. Added `MaterialEditingUiState` with preview surface UV/decal inputs, sidedef material UV/decal inputs, and sector material UV/decal inputs; composed it in `SectorEditor` and passed it to material, selection, inspector, and preview UI contexts that reset or draw those buffers.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: `copiedTopologyMaterial` has no remaining matches; no `Scratch`, `writeback`, `no-authoring`, `TopologyOnly`, or `Legacy` matches remain in material service/tool/preview paths; `SectorEditorUiState&` has no remaining matches in `services/material_edit`; `SectorEditorState&` remains in `services/material_edit` for document/authoring access and material picker routing.
- Behavior notes: authoring graph remains the source of truth for material edits; `ApplyMaterialUiResetFlags()` still resets the same material input buffers, now through `MaterialEditingUiState`; material edit dirty/cache invalidation and preview mesh rebuild behavior were unchanged; texture picker lifecycle stayed in `TexturePickerState` / `TexturePickerService`; no map JSON/schema, lightmap source-hash, rendering, collision, or manual GUI-verified behavior changed.

Phase 3 verification, 2026-07-07:

- Source changed. Added `LightEditingState` in `services/lights`; moved `lightDrag` and the active light edit transaction out of `SectorEditorState`; renamed the old transaction payload to `LightEditTransactionState`.
- Split spotlight pilot state: light-owned pilot data (`active`, kind, light ID, original light position/target, and target distance) now lives in `LightEditingState`; preview pose and mouse-look restore remain editor/preview-owned in `SectorEditorState`.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: `services/lights` has no `SectorEditorState&` / `SectorEditorUiState&` references and no `#include "sector_editor/SectorEditor.h"`; remaining `lightDrag` / `spotLightPilot` references are routed through `LightEditingState` or the preview restore field.
- Behavior notes: light object storage remains in `SectorTopologyMap`; light mutation dirty/cache invalidation behavior is preserved through the same topology-document edited semantics; dynamic renderer refresh behavior and lightmap bake behavior were unchanged; source-hash behavior did not change; rendering, collision, sector lookup, and physics were unchanged. No manual GUI verification was performed.

Phase 4A verification, 2026-07-07:

- Source changed. Added `SelectionState` in `sources/sector_editor/selection/`; composed `SelectionState selectionState` in `SectorEditor`; moved topology selection kind/IDs, selected side kind/wall part/material layer, selected runtime object ID, inspected topology vertex ID, and selected authoring target out of `SectorEditorState`.
- Updated selection, authoring helper, light/material service context, inspector, preview UI, select-tool, placed-object action, and affected tests to read/write those moved fields through `SelectionState`.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: moved core fields are routed through `SelectionState`/selection contexts; remaining `selectedTopologySurface3D` matches are deferred preview-surface state by design; `sources/sector_editor/selection` still has broad `SectorEditorState&` / `SectorEditorUiState&` references as allowed for Phase 4A and scheduled for Phase 4D narrowing.
- Behavior notes: selection behavior, picking semantics, selected ID buffer sync, preview-surface selection/highlight behavior, rendering, collision, sector lookup, and physics were intended to remain unchanged. Hover state, preview-surface fields, and manipulation fields were not moved. No map JSON/schema or lightmap source-hash behavior changed. Topology render-cache invalidation behavior was unchanged; this pass changed selection state ownership only and did not add topology mutations. No manual GUI verification was performed.

Phase 4B verification, 2026-07-07:

- Source changed. `SelectSectorEditorAuthoringLine()`, `SelectSectorEditorAuthoringVertex()`, and `SelectSectorEditorAuthoringFaceAnchor()` now validate against `const SectorAuthoringGraph&` and write selected authoring state through `SelectionState&`; call sites were updated accordingly.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: `selectedAuthoring` writes in authoring selection helpers route through `SelectionState`; `hoveredAuthoring` remains in `SectorEditorState` by design for Phase 4C; `sources/sector_editor/selection` still has broad `SectorEditorState&` / `SectorEditorUiState&` references as expected before Phase 4D.
- Behavior notes: authoring graph remains the source of truth for selection validation and authoring edits; selection behavior, picking semantics, rendering, collision, sector lookup, and physics were intended to remain unchanged. No topology mutations were added, so topology render-cache invalidation behavior was unchanged. No map JSON/schema or lightmap source-hash behavior changed. Preview-surface selection stayed deferred. No manual GUI verification was performed.

Phase 4C verification, 2026-07-07:

- Source changed. Moved `hoveredTopologyLightId`, `hoveredTopologyStaticSpotLightId`, `hoveredTopologyDynamicLightId`, `hoveredTopologyDynamicSpotLightId`, `hasHoveredVertex`, `hoveredTopologyVertexId`, `hoveredTopologyVertexPoint`, and `hoveredAuthoring` out of `SectorEditorState` and into `SelectionState`.
- Updated 2D hover reset/update paths, select-tool hover writes, authoring hover helpers, light-service hover references, and topology draw-context construction to use `SelectionState`. The topology render cache still receives copied draw-context values and does not store `SelectionState` references.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: `hoveredTopology`, `hasHoveredVertex`, and `hoveredAuthoring` references are routed through `SelectionState`, light-service transitional reference fields, or topology draw-context copy fields; `selectedAuthoring` / `hoveredAuthoring` render-cache and `SectorEditor.cpp` references use `SelectionState` at context creation and copied draw-context values inside the cache.
- Behavior notes: selection behavior, hover/picking semantics, rendering, collision, sector lookup, and physics were intended to remain unchanged. Preview-surface hover (`hoveredSurface3D`) stayed deferred. No topology mutations were added, so topology render-cache invalidation behavior was unchanged. No map JSON/schema or lightmap source-hash behavior changed. No manual GUI verification was performed.

Phase 4D verification, 2026-07-07:

- Source changed. Narrowed `SectorEditorSelectionServiceContext` so `SectorEditorSelectionService.*` no longer depends on `SectorEditorState&`, `SectorEditorUiState&`, or `SectorEditor.h`; it now receives explicit topology map, authoring graph, authoring derivation read state, `SelectionState`, preview-surface references, drag reset references, light editing state, material UI state, and a narrow UI reset/input dependency bundle.
- Added narrow authoring helper overloads for authoring graph presence and 3D surface authoring target resolution, preserving the existing full-state wrapper behavior for other callers.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: `SectorEditorSelectionService.*` has no `SectorEditorState&`, `SectorEditorUiState&`, or `#include "sector_editor/SectorEditor.h"` matches; selection-service `requestCancelSpotLightPilotWithPreviewRestore` / `userData` remains as documented preview-restore debt. The broader `sources/sector_editor/selection` directory still contains broad state references in `SectorEditorManipulationService` and `SectorEditorMoveContext`, deferred to Phase 5.
- Behavior notes: selection behavior, picking semantics, preview surface highlight behavior, rendering, collision, sector lookup, and physics were intended to remain unchanged. Preview-surface selection fields (`selectedSurface3D`, `selectedTopologySurface3D`, `hoveredSurface3D`) stayed in existing preview/editor state and were only passed by reference to the selection service. No topology mutations were added, so topology render-cache invalidation behavior was unchanged. No map JSON/schema or lightmap source-hash behavior changed. No manual GUI verification was performed.

Phase 5 verification, 2026-07-07:

- Source changed. Added `ManipulationState` in `sources/sector_editor/selection/`; composed it in `SectorEditor`; moved `selectDragArm` and `authoringVertexDrag` out of `SectorEditorState`.
- Updated manipulation, selection, light editing, select-tool, preview-overlay validation, editor lifecycle/reset paths, and affected tests to route moved fields through `ManipulationState`.
- Light drag state stayed in `LightEditingState` from Phase 3. Runtime object drag state stayed in existing placed-object/editor state because moving it would require broader placed-object editing ownership work.
- Removed unused `SelectionState&` and `SectorEditorUiState&` references from `SectorEditorManipulationServiceContext`. Existing manipulation callbacks for authoring vertex, runtime object, and light drag lifecycle remain as cleanup debt.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: `selectDragArm` and `authoringVertexDrag` are routed through `ManipulationState`; `lightDrag` remains in `LightEditingState`; `runtimeObjectDrag` remains in placed-object/editor state. `sources/sector_editor/selection` still has `SectorEditorState&` in `SectorEditorManipulationServiceContext` / `SectorEditorMoveContext` for topology, authoring graph, current tool, and placed-object drag dependencies; `SectorEditorUiState&` no longer appears there. Existing drag lifecycle callback names remain in `SectorEditorManipulationService` as documented debt.
- Behavior notes: drag start/update/finish/cancel behavior, selection/picking semantics, input routing, rendering, collision, sector lookup, and physics were intended to remain unchanged. No topology mutations were added; authoring vertex move completion still uses the existing authoring mutation path, so document edited/topology render-cache invalidation behavior was unchanged. No map JSON/schema or lightmap source-hash behavior changed. No manual GUI verification was performed.

Phase 6 verification, 2026-07-07:

- Source changed. Added `InspectorIdUiState` in `sources/sector_editor/inspector/`; composed it in `SectorEditor`; moved `selectedSectorIdBuffer`, `selectedLightIdBuffer`, `idBufferSectorIndex`, `idBufferLightIndex`, and `idEditError` out of `SectorEditorUiState`.
- Updated inspector, sector inspector, light inspector, selection service, light editing service, preview overlay validation, and affected test fixtures to receive the narrow selected-ID/error state explicitly.
- `cmake --build cmake-build-debug -j2`: passed.
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 16/16 tests.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Grep checks: no `uiState.selectedSectorIdBuffer`, `uiState.selectedLightIdBuffer`, `uiState.idBufferSectorIndex`, `uiState.idBufferLightIndex`, or `uiState.idEditError` matches remain in `sources/sector_editor` or tests. Broad `SectorEditorUiState&` references remain in touched inspector/preview/selection/light modules for the feature input groups intentionally left in place.
- Behavior notes: selected ID buffer sync, ID edit error display/reset timing, UI focus/capture, inspector scroll behavior, rendering, collision, sector lookup, and physics were intended to remain unchanged. No topology mutations were added, so topology render-cache invalidation behavior was unchanged. No map JSON/schema or lightmap source-hash behavior changed. No manual GUI verification was performed.
- Remaining `SectorEditorUiState` feature groups: grid size input, sector floor/ceiling/ambient inputs, light inspector inputs, runtime object inspector inputs, top-level tool/inspector scroll, and keyboard capture. Material input buffers remain in `MaterialEditingUiState`.

Phase 7 verification, 2026-07-07:

- Documentation changed only. No source code, tests, CMake files, generated data, serialization/schema, runtime/editor behavior, rendering, collision, sector lookup, physics, topology render-cache invalidation, or lightmap source-hash behavior was changed.
- Remaining preview state debt recorded: preview control mode, preview UI hidden/debug overlay state, freefly/gameplay controller state, collision world/results/warnings, visual step/headbob/landing pose effects, preview pose restore, preview-surface selection, runtime object preview state, object-probe debug data, renderer resource rebuild/lifetime, and manual render smoke requirements.
- Remaining document state debt recorded: authoring graph, derived topology and derivation status, last-valid derived topology, dirty/path/status fields, unsaved/current-level state, load/save/reset/import/migration flows, topology document initialization, and source-of-truth boundaries.
- Backlog entries added: `REF-085` for a dedicated `PreviewState` runner plan and `REF-086` for a dedicated `DocumentState` runner plan.
- `git diff --check`: passed.
- `git diff --stat` and `git status --short`: reviewed.
- Build and ctest were not run because Phase 7 is documentation-only and the plan lists them as optional for this phase.
- No manual GUI verification was performed.

## Execution Tracking Rules

- Each phase must be independently buildable and testable.
- Each phase must compile and pass checks before moving to the next phase.
- Each phase final report must state whether source code changed.
- Each implementation phase must update this document before finishing.
- The update should be small and local.
- Do not rewrite unrelated phases when marking progress.
- If behavior is intended to remain unchanged, explicitly state that.
- If a phase changes serialization, generated data, public APIs, runtime behavior, cache invalidation, source-hash behavior, or build/test behavior, clearly say so.
- Do not claim manual GUI verification unless it was actually performed.
- If a phase produces only a plan or audit and no source changes, state that clearly.
- If a phase is too broad, add smaller child passes under that phase and stop.

## Summary

REF-084 migrates service-specific editor state out of the monolithic `SectorEditorState` and `SectorEditorUiState` in narrow, independently verifiable phases. The plan follows the current audit recommendation: split texture catalog cache state first, then material, light, selection, and manipulation state. Optional UI input grouping and broad preview/document state work are deferred or planning-only.

This plan is for later implementation. It must not be treated as implementation completion.

## Architecture Contract

- `SectorEditor` composes services and keeps high-level lifecycle, routing, document orchestration, preview orchestration, and modal orchestration until separate plans move them.
- Services own shared editor capabilities and real behavior, but must not include `SectorEditor.h` or call `SectorEditor::` methods.
- Tools and panels should receive services or narrow state/service contexts directly instead of callback bridges back to `SectorEditor`.
- The authoring graph remains the source of truth for editable map geometry and normal editor-facing material edits.
- `SectorTopologyMap` remains derived/runtime/map-metadata output. Its texture registry can remain map-level metadata for now.
- `LightmapBakeController` is the model for a real state-owning service: it owns its bake state and leaves current-document install orchestration to `SectorEditor`.

## Current Problem

`TextureCatalogService`, `MaterialEditingService`, `LightEditingService`, `SelectionService`, and `ManipulationService` own meaningful behavior, but most durable or transient state still lives in `SectorEditorState` or `SectorEditorUiState`. These services still commonly receive broad `SectorEditorState&` and `SectorEditorUiState&`, so extracted tools and panels continue to depend on one shared state object.

The result is a god-state shape: runtime-reusable logic, editor-only UI buffers, modal state, preview selection, light editing state, and document/map data are colocated. Future game-mode/runtime reuse is blocked when renderer/runtime/catalog/light behavior needs editor-only UI and modal state just to compile.

## Target State Ownership Model

- `TextureCatalogState` owns editor texture handle cache first. It may own editor texture asset scope state only if the implementation proves the scope lifetime is texture-catalog-specific and shutdown/unload ordering remains identical. It may read `SectorTopologyMap::texturesById`; it does not own map texture registry serialization or feature apply semantics.
- `MaterialEditingState` owns copied material and material-owned transient edit state. `MaterialEditingUiState` or a later inspector UI state owns material-specific UI buffers when practical. It does not own the authoring graph, topology map, or `TexturePickerService` lifecycle.
- `LightEditingState` owns light drag/edit transaction state and light-data pilot restore state. It does not own preview renderer, camera, collision, or source-hash policy.
- `SelectionState` owns selected/hovered targets and selected/hovered derived IDs. It does not own actual document data.
- `ManipulationState` owns active drag/manipulation transactions. It does not own input routing, renderer, camera, collision, or document lifecycle.
- Optional `InspectorUiState` groups feature input buffers by feature after service-owned data state is split.
- `PreviewState` and `DocumentState` are intentionally separate future runner plans.

## Non-Negotiable Guardrails

- Do not change editor behavior intentionally.
- Do not move data into services by creating callback bridges.
- Do not create a new god state object with a different name.
- Do not store editor-only UI/modal state in runtime-reusable service state.
- Do not make services depend on `SectorEditor.h`.
- Do not call `SectorEditor::` methods from services.
- Do not move source-of-truth map editing into `SectorTopologyMap`.
- Do not reintroduce no-authoring topology edit fallback.
- Do not change serialization/schema unless a phase explicitly says the state is editor-only and not serialized.
- Do not change lightmap/source-hash behavior.
- Do not change rendering/collision behavior.
- Do not move document lifecycle or preview runtime state as part of this plan unless explicitly deferred to separate plans.
- Do not plan or create a generic `EditorStateService`.
- Do not plan or create a broad `TopologyService`.
- Do not move behavior back into `SectorEditor`.

## Baseline Files To Inspect

Every phase must inspect the architecture and runner documents first:

- `docs/architecture/sector_editor_architectural_principles.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/runner_compatible_plans.md`

Every implementation phase should inspect these editor files before editing:

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorPreviewTypes.h`
- `sources/sector_editor/SectorEditorModalTypes.h`
- `sources/sector_editor/SectorEditorSelectionTypes.h`
- `sources/sector_editor/SectorEditorSurfaceTypes.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/services/texture_catalog/`
- `sources/sector_editor/services/texture_picker/`
- `sources/sector_editor/services/material_edit/`
- `sources/sector_editor/services/lights/`
- `sources/sector_editor/services/lightmap_bake/`
- `sources/sector_editor/selection/`
- `sources/sector_editor/inspector/`
- `sources/sector_editor/preview/`
- `sources/sector_editor/tools/`

## Phase 1: TextureCatalogState

Goal:

Move texture catalog/cache state out of `SectorEditorState` so `TextureCatalogService` becomes state-owning or receives a narrow `TextureCatalogState` instead of broad editor state.

State object:

- Name: `TextureCatalogState`.
- Owning service: `SectorEditorTextureCatalogService` or `SectorEditor` composing `TextureCatalogState` and passing it to the service.
- Fields moved: `editorTextureHandlesById` first, and any default texture ID cache if inspection proves it is catalog/cache state. Move `editorTextureScope` only if ownership/lifetime is clearly texture-catalog-specific and shutdown/unload ordering remains identical.
- Editor-only or runtime-reusable: mostly editor/runtime-preview cache state. It must not contain modal UI state.
- Serialized: no.
- Constructed by: `SectorEditor` member default construction, likely near existing service composition.
- Tool/panel access: tools and panels continue receiving `SectorEditorTextureCatalogService&`.
- Test construction: instantiate `TextureCatalogState`, `SectorTopologyMap`, and a service context without constructing `SectorEditor`.

Files to inspect:

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.*`
- `sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.*`
- `sources/sector_editor/SectorEditorTextureModals.*`
- `sources/sector_editor/inspector/SectorEditorInspectorPanel.*`
- `sources/sector_editor/preview/SectorEditorPreviewUvPanel.*`
- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.*`
- `sources/sector_editor/tools/doors/SectorEditorDoorInspector.*`

Files likely to modify:

- New `sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogState.h`, or equivalent local service state header in the texture catalog folder.
- `sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h/.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- Focused texture catalog tests if a suitable test target already exists.

Exact implementation steps:

1. Add a narrow `TextureCatalogState` with `std::unordered_map<std::string, engine::TextureHandle> editorTextureHandlesById`.
2. Move `editorTextureHandlesById` out of `SectorEditorState` first.
3. Evaluate `editorTextureScope` ownership separately. Move it into `TextureCatalogState` only if the service owns the full editor texture cache lifetime and shutdown/unload ordering remains byte-for-byte equivalent at the behavior level.
4. If `editorTextureScope` is entangled with broader editor asset lifetime or shutdown ordering, leave it in `SectorEditor` and list it as remaining texture catalog state debt.
5. Add a `TextureCatalogState textureCatalogState;` member to `SectorEditor`, or another narrow composition point that does not put it back into `SectorEditorState`.
6. Change `SectorEditorTextureCatalogServiceContext` from `SectorEditorState& state` to `SectorTopologyMap& map` or `const SectorTopologyMap& map` as appropriate plus `TextureCatalogState& textureState`.
7. Update service methods to read the map texture registry from the map reference and editor handles from `TextureCatalogState`; read editor scope from `TextureCatalogState` only if the scope moved.
8. Update `MakeTextureCatalogService()` and shutdown/unload paths while preserving texture scope unload behavior exactly.
9. Keep `SectorTopologyMap::texturesById` as the map-level texture registry. Do not change serialization, schema, or lightmap source-hash behavior.
10. Keep add-map modal lifecycle, add-map preview scope, selected path behavior, document dirty/status, and feature-specific apply semantics outside `TextureCatalogService`.
11. Add or update tests only if a focused existing test target covers texture catalog service state without CMake changes. If CMake would need edits, document the gap instead of changing CMake in this phase unless explicitly instructed.
12. Run the required checks before marking complete.

Behavior guardrails:

- Texture picker options, missing-texture checks, editor texture handle refresh, add-map registration, default texture ID refresh, and shutdown scope unload behavior must remain unchanged.
- Texture scope unload behavior must be preserved exactly whether `editorTextureScope` moves or stays.
- No map JSON/schema changes.
- No source-hash changes.
- No topology render-cache invalidation behavior changes unless an existing texture registry mutation path already marks document edited.
- No new callback bridge from texture clients to `SectorEditor`.

Tests/checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "editorTextureScope|editorTextureHandlesById" sources/sector_editor`
  - `rg -n "SectorEditorState&" sources/sector_editor/services/texture_catalog`
  - `rg -n "#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/services/texture_catalog`

Stop conditions:

- Stop if texture registry ownership cannot be kept in `SectorTopologyMap` without changing serialization.
- Stop if add-map modal preview scope and editor texture scope are entangled and need separate planning.
- Stop if moving `editorTextureScope` would change shutdown/unload ordering or make ownership ambiguous. Leave it in `SectorEditor` and record debt instead.
- Stop if CMake edits are required for tests and the runner invocation did not authorize test/build metadata changes.

Final report requirements:

- List moved fields and new owner.
- Explicitly state whether `editorTextureScope` moved, stayed, and why.
- State that `SectorTopologyMap::texturesById` remained map-level registry or explicitly report any blocked reason.
- State cache invalidation behavior for touched texture registry paths.
- State source-hash behavior did not change.
- Report grep results proving `services/texture_catalog` no longer depends on `SectorEditorState`.

What remains for later phases:

- Material picker semantics remain in material editing.
- Add-map modal lifecycle remains outside the catalog service.
- Sprite/billboard metadata remains separate.
- Feature input buffers remain in later UI phases.

## Phase 2: MaterialEditingState And Material UI State

Goal:

Move material-service-specific state out of `SectorEditorState` and `SectorEditorUiState` where practical, so `MaterialEditingService` owns or receives narrow material edit state instead of full editor state/UI state.

Execution order:

- Phase 2A must complete before Phase 2B starts.
- Phase 2A moves material data/edit state only and must not move UI input buffers.
- Phase 2B moves sidedef/preview material input buffers only after Phase 2A is complete.
- Phase 2B must not change material behavior or texture picker lifecycle.
- If Phase 2B proves too broad, split it further into smaller child passes and stop.

State objects:

- Name: `MaterialEditingState`.
- Owning service: `SectorEditorMaterialEditingService` or `SectorEditor` composing state and passing it to the service.
- Fields moved: `copiedTopologyMaterial`; material picker target helper state if inspection proves it is material-owned; selected material layer/wall part state only if it is service-specific and not selection-owned.
- Editor-only or runtime-reusable: editor material editing state. Do not mix generic picker lifecycle or modal orchestration into it.
- Serialized: no.
- Constructed by: `SectorEditor` member default construction.
- Tool/panel access: tools/panels receive `SectorEditorMaterialEditingService&`; the service context receives `MaterialEditingState&`.
- Test construction: instantiate `MaterialEditingState`, narrow document/material dependencies, and service context without `SectorEditor`.

Optional UI state:

- Name: `MaterialEditingUiState`.
- Owning service/panel: material service can receive it for reset/apply helpers, or material/preview panels can own it if that is cleaner.
- Fields to consider: `topologySideDefUvInputs`, `topologySideDefDecalOpacityInput`, `topologySideDefDecalBloomIntensityInput`, `surface3DUvScaleUInput`, `surface3DUvScaleVInput`, `surface3DUvOffsetUInput`, `surface3DUvOffsetVInput`, `surface3DDecalOpacityInput`, `surface3DDecalBloomIntensityInput`, material-specific sector UV/decal inputs if they are not broader sector inspector state.
- Defer if UI extraction would touch too many inspector paths.

Files to inspect:

- `sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.*`
- `sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.*`
- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.*`
- `sources/sector_editor/preview/SectorEditorPreviewUvPanel.*`
- `sources/sector_editor/inspector/SectorEditorInspectorPanel.*`
- `sources/sector_editor/SectorEditorMaterialActions.*`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorModalTypes.h`

Files likely to modify:

- New `sources/sector_editor/services/material_edit/SectorEditorMaterialEditingState.h`, or equivalent service state header.
- `sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.h/.cpp`
- `sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h/.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- Material inspector and preview UV panel contexts only where needed.

Exact implementation steps:

Phase 2A, `MaterialEditingState only`:

1. Add `MaterialEditingState` with `TopologyMaterialPayload copiedMaterial` or a similarly named field preserving current copied-material semantics.
2. Move `copiedTopologyMaterial` out of `SectorEditorState`.
3. Update `SectorEditorMaterialEditingServiceContext` to receive `MaterialEditingState&` and the narrow document/map/authoring references still required by material operations.
4. Keep authoring graph writes as the source-of-truth material route. Do not reintroduce topology scratch/writeback.
5. Decide whether material picker target helper state in `TexturePickerState` remains generic picker state or needs material-owned wrappers. If it cannot be moved without changing picker semantics, document it as debt.
6. Do not move material UI input buffers in Phase 2A.
7. Keep `DecalTintModalState` outside Phase 2A unless it is clearly material-owned data and not modal orchestration; default to deferring modal state.
8. Run required checks before marking Phase 2A complete.

Phase 2B, `MaterialEditingUiState / material input buffers`:

1. Confirm Phase 2A is complete in the JSON block and progress table before starting.
2. Move only material-owned sidedef and preview material input buffers, such as `topologySideDefUvInputs`, `topologySideDefDecalOpacityInput`, `topologySideDefDecalBloomIntensityInput`, `surface3DUvScaleUInput`, `surface3DUvScaleVInput`, `surface3DUvOffsetUInput`, `surface3DUvOffsetVInput`, `surface3DDecalOpacityInput`, and `surface3DDecalBloomIntensityInput`.
3. Preserve `ApplyMaterialUiResetFlags()` behavior exactly, either by passing `MaterialEditingUiState&` or keeping a temporary narrow UI adapter with explicit debt.
4. Do not change material behavior, authoring graph write policy, preview mesh rebuild policy, or texture picker lifecycle.
5. If material input buffers are too entangled with inspector or preview panel ownership, add smaller child passes under Phase 2B and stop without source changes.
6. Run required checks before marking Phase 2B complete.

Behavior guardrails:

- No material behavior changes.
- No topology scratch/writeback routes.
- No no-authoring topology edit fallback.
- No texture picker lifecycle ownership moved into `MaterialEditingService`.
- Phase 2A must not move UI input buffers.
- Phase 2B must not change texture picker lifecycle or material behavior.
- No map JSON/schema changes.
- No source-hash changes.
- Material changes must preserve current document edited/cache invalidation/preview mesh rebuild behavior.

Tests/checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "copiedTopologyMaterial" sources/sector_editor`
  - `rg -n "SectorEditorState&|SectorEditorUiState&" sources/sector_editor/services/material_edit`
  - `rg -n "Scratch|writeback|no-authoring|TopologyOnly|Legacy" sources/sector_editor/services/material_edit sources/sector_editor/tools/materials sources/sector_editor/preview`

Stop conditions:

- Stop if material UI buffer extraction requires broad inspector or preview panel restructuring.
- Stop if service state would need to own `TexturePickerState` lifecycle.
- Stop if authoring graph material writes cannot remain the source of truth.
- Stop Phase 2B and split it further if sidedef and preview material input buffers cannot move together cleanly.

Final report requirements:

- List moved fields and new owner.
- State authoring graph remains source of truth.
- State cache invalidation and preview rebuild behavior for material edits.
- State source-hash behavior did not change.
- Report remaining material UI or picker debt.

What remains for later phases:

- Generic picker lifecycle remains in `TexturePickerService`.
- Preview-surface selection may remain in `SelectionState` or deferred preview-selection state.
- Broader inspector UI grouping remains Phase 6.

## Phase 3: LightEditingState

Goal:

Move light edit, drag, and light-data pilot restore state out of `SectorEditorState` where practical, while keeping preview renderer/camera/collision orchestration outside `LightEditingService`.

State object:

- Name: `LightEditingState`.
- Owning service: `SectorEditorLightEditingService` or `SectorEditor` composing state and passing it to the service.
- Fields moved: current `lightEditing`; current `lightDrag`; light-data portions of `spotLightPilot` such as active kind, light ID, original light position, original target, and target distance if used only by light edit.
- Editor-only or runtime-reusable: editor light editing transaction state. Do not store preview renderer/camera ownership here.
- Serialized: no.
- Constructed by: `SectorEditor` member default construction.
- Tool/panel access: light tools and inspectors receive `SectorEditorLightEditingService&`; the service receives `LightEditingState&`.
- Test construction: instantiate `LightEditingState`, topology map/light references, UI state if still needed, and service context without `SectorEditor`.

Files to inspect:

- `sources/sector_editor/services/lights/SectorEditorLightEditingService.*`
- `sources/sector_editor/SectorEditorLightInspector.*`
- `sources/sector_editor/inspector/SectorEditorInspectorPanel.*`
- `sources/sector_editor/SectorEditor.cpp` light drag/pilot functions
- `sources/sector_editor/SectorEditorSelectionTypes.h`
- `sources/sector_editor/SectorEditorPreviewTypes.h`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/services/lightmap_bake/`

Files likely to modify:

- New `sources/sector_editor/services/lights/SectorEditorLightEditingState.h`, or equivalent service state header.
- `sources/sector_editor/services/lights/SectorEditorLightEditingService.h/.cpp`
- `sources/sector_editor/SectorEditorSelectionTypes.h` if type definitions move.
- `sources/sector_editor/SectorEditorPreviewTypes.h` only if pilot state is split carefully.
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`

Exact implementation steps:

1. Add `LightEditingState` in the light service area with fields for light drag and edit transactions.
2. Move `lightDrag` and current light edit transaction state out of `SectorEditorState`.
3. Avoid naming conflicts with the existing `LightEditingState` type. If necessary, rename the old transaction type to `LightEditTransactionState` in the same phase as a mechanical behavior-preserving rename.
4. Split `SpotLightPilotState` only if the split is clean: light-data restore state may move to light editing, while original preview pose, mouse-look restore, camera/pilot orchestration, and preview control remain preview/editor-owned.
5. Update `SectorEditorLightEditingServiceContext` to receive the new light state plus narrow map/UI/status dependencies.
6. Keep light object storage where it is for now. Do not move static/dynamic light arrays out of `SectorTopologyMap` in this phase.
7. Preserve dirty/document edited behavior for static/directional/bake-affecting light mutations and dynamic renderer refresh behavior for dynamic lights.
8. Preserve source-hash behavior. Static lights and directional/source-hash-affecting settings must still affect bake hashes exactly as before; visual-only settings must not be newly added.
9. Run required checks before marking complete.

Behavior guardrails:

- No light behavior changes.
- No preview renderer/camera/collision ownership moved into light service.
- No source-hash changes.
- No lightmap bake behavior changes.
- No rendering/collision behavior changes.
- No callback bridges from light service to `SectorEditor`.

Tests/checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "lightDrag|lightEditing|spotLightPilot" sources/sector_editor`
  - `rg -n "SectorEditorState&|SectorEditorUiState&" sources/sector_editor/services/lights`
  - `rg -n "#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/services/lights`

Stop conditions:

- Stop if spotlight pilot cannot be split without mixing preview pose/camera ownership into light service.
- Stop if preserving source-hash behavior is unclear.
- Stop if light UI buffer extraction becomes necessary to move data state; defer UI buffers to Phase 6 instead.

Final report requirements:

- List moved fields and new owner.
- State whether spotlight pilot was split or deferred.
- State source-hash behavior explicitly.
- State rendering/collision behavior was unchanged.
- Report dynamic renderer refresh and lightmap behavior.

What remains for later phases:

- Light input buffers likely remain in `SectorEditorUiState` until Phase 6.
- Light object document ownership remains a future design decision.
- Preview/camera pilot orchestration remains outside light service.

## Phase 4: SelectionState

Goal:

Move selected and hovered target state into a narrow `SelectionState` owned by or passed to `SelectionService`, without changing selection behavior or picking semantics.

State object:

- Name: `SelectionState`.
- Owning service: `SectorEditorSelectionService` or `SectorEditor` composing state and passing it to the service.
- Fields moved: `topologySelectionKind`, `selectedTopologySectorId`, `selectedTopologyVertexId`, `selectedTopologySideDefId`, `selectedTopologyLineDefId`, `selectedTopologySideKind`, `selectedTopologyWallPart`, `activeTopologyMaterialLayer`, selected light IDs, selected runtime object ID if selection service owns the semantics, hovered light IDs, hovered vertex data, `inspectedTopologyVertexId`, `selectedAuthoring`, `hoveredAuthoring`.
- Deferred preview fields by default: `selectedSurface3D`, `selectedTopologySurface3D`, `hoveredSurface3D`, and other preview-surface selection fields should stay out of Phase 4 unless the implementation proves the split is trivial and does not pull in preview renderer, camera, or material panel ownership.
- Editor-only or runtime-reusable: editor selection state only.
- Serialized: no.
- Constructed by: `SectorEditor` member default construction.
- Tool/panel access: selection tools/panels call `SelectionService` or receive `SelectionState&` only where data display requires it.
- Test construction: instantiate `SelectionState`, map/authoring graph references, and service context without `SectorEditor`.

Files to inspect:

- `sources/sector_editor/selection/SectorEditorSelectionService.*`
- `sources/sector_editor/selection/SectorEditorManipulationService.*`
- `sources/sector_editor/tools/select/SectorEditorSelectTool.*`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/SectorEditor.cpp` selection wrappers
- `sources/sector_editor/preview/SectorEditorPreviewUvPanel.*`
- `sources/sector_editor/preview/SectorEditorPreviewOverlay.*`
- `sources/sector_editor/inspector/SectorEditorInspectorPanel.*`

Files likely to modify:

- New `sources/sector_editor/selection/SectorEditorSelectionState.h`, or equivalent selection state header.
- `sources/sector_editor/selection/SectorEditorSelectionService.h/.cpp`
- `sources/sector_editor/selection/SectorEditorManipulationService.h/.cpp`
- `sources/sector_editor/tools/select/SectorEditorSelectTool.*`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.*` if authoring selection helpers are retargeted.
- `sources/sector_editor/SectorEditorTopologyRenderCache.*` contexts if selected/hovered fields are read there.

Execution order:

- Phase 4A must complete before Phase 4B starts.
- Phase 4B must complete before Phase 4C starts.
- Phase 4C must complete before Phase 4D starts.
- Phase 4 remains a parent/planning item until all child passes complete.
- Do not execute more than one Phase 4 child pass in one runner invocation.

Phase 4 split rationale, 2026-07-07:

- The original Phase 4 scope was too broad for one runner pass. Inspection showed selected/hovered state reaches `SectorEditorSelectionService`, `SectorEditorManipulationService`, `SectorEditorLightEditingService`, `SectorEditorAuthoringState`, `SectorEditorTopologyRenderCache`, `SectorEditorSelectTool`, inspector routing, preview UV/overlay modules, and central `SectorEditor` routing/drawing.
- Preview-surface selection (`hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D`) is intentionally deferred by default per the Phase 4 guardrails.
- This split is documentation-only; no source code changed in the split pass.

### Phase 4A: SelectionState Core 2D Selection Fields

Goal:

Add `SelectionState`, compose it in `SectorEditor`, and move core non-preview selected target state out of `SectorEditorState` without changing behavior.

Fields to move:

- `topologySelectionKind`
- `selectedTopologySectorId`
- `selectedTopologyVertexId`
- `selectedTopologySideDefId`
- `selectedTopologyLineDefId`
- `selectedTopologySideKind`
- `selectedTopologyWallPart`
- `activeTopologyMaterialLayer`
- `selectedTopologyLightId`
- `selectedTopologyStaticSpotLightId`
- `selectedTopologyDynamicLightId`
- `selectedTopologyDynamicSpotLightId`
- `selectedRuntimeObjectId`
- `inspectedTopologyVertexId`
- `selectedAuthoring`

Fields to leave for later Phase 4 child passes:

- Hover fields: `hoveredTopologyLightId`, `hoveredTopologyStaticSpotLightId`, `hoveredTopologyDynamicLightId`, `hoveredTopologyDynamicSpotLightId`, `hasHoveredVertex`, `hoveredTopologyVertexId`, `hoveredTopologyVertexPoint`, `hoveredAuthoring`.
- Preview-surface fields: `hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D`.
- Manipulation fields: `selectDragArm`, `authoringVertexDrag`, `runtimeObjectDrag`.

Implementation notes:

1. Add `sources/sector_editor/selection/SectorEditorSelectionState.h`.
2. Add `SelectionState selectionState;` to `SectorEditor`.
3. Retarget direct reads/writes of the moved fields to `selectionState`.
4. Keep `SectorEditorSelectionServiceContext` broad if necessary for this first pass, but pass `SelectionState&` and use it inside the service for moved fields.
5. Preserve selected ID buffer sync behavior through the existing UI state.
6. Do not move preview-surface selection, hover state, manipulation state, picking, preview renderer ownership, collision state, or document lifecycle.

Phase 4A checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "topologySelectionKind|selectedTopology|selectedRuntimeObjectId|activeTopologyMaterialLayer|inspectedTopologyVertexId|selectedAuthoring" sources/sector_editor`
  - `rg -n "SectorEditorState&|SectorEditorUiState&" sources/sector_editor/selection`

### Phase 4B: Authoring Selection Helper Retarget

Goal:

Retarget authoring selection helper functions so selected authoring state is owned by `SelectionState`, while authoring graph data remains document/source-of-truth state.

Fields in scope:

- `selectedAuthoring`
- Any helper-local use of moved core selected target fields from Phase 4A.

Implementation notes:

1. Update `SectorEditorAuthoringState.*` helper signatures only where needed to take `SelectionState&` plus `SectorEditorState&` or narrower authoring/document references.
2. Preserve authoring graph source-of-truth behavior.
3. Avoid a broad authoring document split.
4. Do not move hover fields in this pass unless a helper signature must mention them; prefer leaving hover for Phase 4C.

Phase 4B checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "selectedAuthoring|hoveredAuthoring" sources/sector_editor/SectorEditorAuthoringState.* sources/sector_editor`
  - `rg -n "SectorEditorState&|SectorEditorUiState&" sources/sector_editor/selection`

### Phase 4C: Hover And Render-Cache Selection Context

Goal:

Move 2D hover fields into `SelectionState` and update drawing/picking-visible context construction so cached topology rendering reads the same selected/hovered state from the new owner.

Fields to move:

- `hoveredTopologyLightId`
- `hoveredTopologyStaticSpotLightId`
- `hoveredTopologyDynamicLightId`
- `hoveredTopologyDynamicSpotLightId`
- `hasHoveredVertex`
- `hoveredTopologyVertexId`
- `hoveredTopologyVertexPoint`
- `hoveredAuthoring`

Implementation notes:

1. Retarget hover updates in `SectorEditor::UpdateHoverAndMouse()` and `tools/select`.
2. Update `SectorEditorTopologyDrawContext` creation to read selection and hover values from `SelectionState`.
3. Do not store references to `SelectionState` inside the render cache.
4. Keep picking behavior consistent with what is drawn.
5. Do not move preview-surface hover (`hoveredSurface3D`) in this pass.

Phase 4C checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "hoveredTopology|hasHoveredVertex|hoveredAuthoring" sources/sector_editor`
  - `rg -n "selectedAuthoring|hoveredAuthoring" sources/sector_editor/SectorEditorTopologyRenderCache.* sources/sector_editor/SectorEditor.cpp`

### Phase 4D: Selection Service Dependency Narrowing

Goal:

Narrow `SectorEditorSelectionServiceContext` around `SelectionState` and required narrow dependencies after the selected/hovered state has moved.

Implementation notes:

1. Replace broad selection service state access with `SelectionState&`, `SectorTopologyMap&`, `SectorAuthoringGraph&`, material UI reset state, light editing state, runtime object access, and specific UI buffer/reset dependencies as needed.
2. Remove `SectorEditorState&` and `SectorEditorUiState&` from `SectorEditorSelectionServiceContext` only if selected ID buffer sync and reset behavior can remain behavior-preserving.
3. Do not move inspector UI buffers; defer those to Phase 6 unless a very small explicit dependency object is required.
4. Keep the existing spotlight preview-restore callback bridge if removing it would require preview/camera ownership changes. Report it as remaining debt.
5. Keep preview-surface selection fields deferred unless the remaining service dependency cleanup proves their move is trivial and does not pull in preview renderer, camera, or material panel ownership.

Phase 4D checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "selectedAuthoring|hoveredAuthoring|selectedTopology|hoveredTopology|selectedSurface3D|hoveredSurface3D|selectedRuntimeObjectId" sources/sector_editor`
  - `rg -n "SectorEditorState&|SectorEditorUiState&" sources/sector_editor/selection`
  - `rg -n "requestCancelSpotLightPilotWithPreviewRestore|userData" sources/sector_editor/selection`

Exact implementation steps:

1. Add `SelectionState` with selected/hovered 2D/editor authoring, topology, light, and runtime object IDs.
2. Move 2D/editor selected/hovered fields out of `SectorEditorState` first.
3. Update `SectorEditorSelectionServiceContext` to receive `SelectionState&` plus narrow map/authoring/runtime object references required for validation and lookup.
4. Retarget selection wrappers and helper functions to use `SelectionState`.
5. Update render-cache context creation to read selection from `SelectionState` without storing references in the cache.
6. Preserve selected ID buffer sync behavior. If buffers remain in `SectorEditorUiState`, pass only the specific UI buffer state needed or defer to Phase 6.
7. Do not move picking, preview renderer ownership, or collision state.
8. Leave `selectedSurface3D`, `selectedTopologySurface3D`, `hoveredSurface3D`, and other preview-surface selection fields in place by default. The expected home is a later `PreviewState` or `PreviewSelectionState` plan.
9. Move a preview-surface selection field in Phase 4 only if the implementation proves the split is trivial and does not pull in preview renderer, camera, or material panel ownership.
10. Run required checks before marking complete.

Behavior guardrails:

- Selection behavior unchanged.
- Picking semantics unchanged.
- Preview surface highlight behavior unchanged if those fields move.
- Default expectation: preview-surface selection remains deferred to a later `PreviewState` or `PreviewSelectionState` plan.
- No preview renderer ownership moved.
- No callback bridges added.
- No document/map serialization changes.

Tests/checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "selectedAuthoring|hoveredAuthoring|selectedTopology|hoveredTopology|selectedSurface3D|hoveredSurface3D|selectedRuntimeObjectId" sources/sector_editor`
  - `rg -n "SectorEditorState&|SectorEditorUiState&" sources/sector_editor/selection`
  - `rg -n "requestCancelSpotLightPilotWithPreviewRestore|userData" sources/sector_editor/selection`

Stop conditions:

- Stop if selection state extraction requires moving preview runtime/camera ownership.
- Stop if moving preview-surface selection would pull in preview renderer, camera, or material panel ownership.
- Stop if selected ID buffer sync cannot be kept behavior-preserving without a UI-state child pass.
- Stop if authoring selection helpers would require a broad authoring document split.

Final report requirements:

- List moved fields and new owner.
- State whether 3D surface selection moved or was deferred. If any preview-surface selection field moved, justify why it was safe and list behavior checks for Preview3D surface selection, material panel behavior, and surface highlights.
- State picking behavior and preview highlight behavior were unchanged.
- Report any remaining callback bridge debt, especially spotlight preview restore.

What remains for later phases:

- Manipulation transaction state moves in Phase 5.
- Inspector UI buffers move in Phase 6.
- Preview-specific selection may require a future `PreviewState` runner plan.

## Phase 5: ManipulationState

Goal:

Move drag/manipulation transaction state into `ManipulationState` owned by or passed to `ManipulationService`, and reduce broad callback dependence where practical without moving input routing, camera, preview, or collision ownership.

State object:

- Name: `ManipulationState`.
- Owning service: `SectorEditorManipulationService` or `SectorEditor` composing state and passing it to the service.
- Fields moved: `selectDragArm`, `authoringVertexDrag`, light drag transaction state only if not already moved to `LightEditingState`, runtime object drag state only if it is not better owned by placed-object editing, and manipulation provider transaction state.
- Editor-only or runtime-reusable: editor manipulation transaction state.
- Serialized: no.
- Constructed by: `SectorEditor` member default construction.
- Tool/panel access: select/move tools call `ManipulationService`; movement providers receive only narrow transaction dependencies.
- Test construction: instantiate `ManipulationState`, selection state, relevant provider context, and service context without `SectorEditor`.

Files to inspect:

- `sources/sector_editor/selection/SectorEditorManipulationService.*`
- `sources/sector_editor/selection/SectorEditorMoveContext.h`
- `sources/sector_editor/selection/SectorEditorSelectionTarget.h`
- `sources/sector_editor/tools/select/SectorEditorSelectTool.*`
- `sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.*`
- `sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectMoveProvider.*`
- `sources/sector_editor/SectorEditor.cpp` drag lifecycle functions
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/services/lights/SectorEditorLightEditingService.*`

Files likely to modify:

- New `sources/sector_editor/selection/SectorEditorManipulationState.h`, or equivalent manipulation state header.
- `sources/sector_editor/selection/SectorEditorManipulationService.h/.cpp`
- `sources/sector_editor/tools/select/SectorEditorSelectTool.*`
- `sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.*`
- `sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectMoveProvider.*`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`

Exact implementation steps:

1. Add `ManipulationState` with select drag arm and active transaction fields.
2. Move `selectDragArm` out of `SectorEditorState`.
3. Move `authoringVertexDrag` if manipulation service can own its lifecycle while still calling narrow authoring graph mutation helpers.
4. Keep light drag state in `LightEditingState` if Phase 3 already moved it. Do not duplicate it.
5. Decide whether `runtimeObjectDrag` belongs in `ManipulationState` or a future placed-object editing state. Move it only if existing placed-object drag providers can accept the narrow state without broad rewrites.
6. Replace broad callbacks in `SectorEditorManipulationServiceContext` only where the target operation can be expressed as a narrow dependency. Do not add new callback bundles.
7. Keep input routing in `SectorEditor`/tool layer. Keep camera, preview renderer, and collision out of manipulation state.
8. Preserve document edited/cache invalidation behavior for moved drag completion paths.
9. Run required checks before marking complete.

Behavior guardrails:

- Drag start/update/finish/cancel behavior unchanged.
- Selection and picking semantics unchanged.
- No input routing ownership moved.
- No camera/preview/collision ownership moved.
- No callback bridge expansion.
- No document lifecycle changes.

Tests/checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks:
  - `rg -n "selectDragArm|authoringVertexDrag|runtimeObjectDrag|lightDrag" sources/sector_editor`
  - `rg -n "SectorEditorState&|SectorEditorUiState&" sources/sector_editor/selection`
  - `rg -n "startAuthoringVertexDrag|startRuntimeObjectDrag|startLightDrag|updateAuthoringVertexDrag|finishAuthoringVertexDrag|cancelAuthoringVertexDrag" sources/sector_editor/selection`

Stop conditions:

- Stop if active drag ownership cannot be moved without moving input routing or preview/collision.
- Stop if runtime object drag clearly needs a separate placed-object state plan.
- Stop if callback removal requires broad tool or document rewrites.

Final report requirements:

- List moved fields and new owner.
- State which callbacks were removed and which remain as debt.
- State cache invalidation behavior for drag-completion mutations.
- State rendering/collision behavior was unchanged.

What remains for later phases:

- Runtime object editing state may need a separate plan.
- Remaining callback bridges should be documented in the backlog.
- Preview/document state remains out of scope.

## Optional Later Phases / Deferred Work

## Phase 6: InspectorUiState Feature Input Groups

Goal:

Extract feature-specific input buffers from `SectorEditorUiState` after data ownership is split, if the scope remains practical.

Possible groups:

- Material input buffers: `surface3D*`, `topologySideDef*`, and material-specific sector UV/decal inputs.
- Light input buffers: light position/target/intensity/radius/cones/source/flicker/shadow/color inputs.
- Runtime object input buffers: runtime object transform, dimensions, door, billboard, and origin inputs.
- Sector inspector inputs: floor/ceiling/ambient/sector material inputs if not moved with material UI state.
- Selected ID buffers and inspector errors: `selectedSectorIdBuffer`, `selectedLightIdBuffer`, `idBufferSectorIndex`, `idBufferLightIndex`, `idEditError`.

Files to inspect:

- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/inspector/SectorEditorInspectorPanel.*`
- `sources/sector_editor/SectorEditorLightInspector.*`
- `sources/sector_editor/SectorEditorSectorInspector.*`
- `sources/sector_editor/preview/SectorEditorPreviewUvPanel.*`
- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.*`
- `sources/sector_editor/tools/placed_objects/`
- `sources/sector_editor/tools/billboards/`
- `sources/sector_editor/tools/doors/`

Files likely to modify:

- New feature UI state headers only if they stay narrow.
- Existing panel/tool context headers.
- `SectorEditorTypes.h`, `SectorEditor.h`, `SectorEditor.cpp`.

Exact implementation steps:

1. Inspect actual remaining `SectorEditorUiState` usage after Phases 1-5.
2. Choose one feature input group only. If more than one group is needed, add child passes and stop.
3. Add a narrow UI state object for that group.
4. Move only that group's fields out of `SectorEditorUiState`.
5. Update panel/tool/service contexts to receive the narrow UI state.
6. Keep `config`, `keyboardCaptured`, and broad panel scroll state in top-level UI unless a focused panel owner exists.
7. Run required checks before marking complete.

Behavior guardrails:

- UI input behavior, focus/capture, reset timing, validation messages, and inspector scroll behavior unchanged.
- Do not put modal lifecycle state into runtime-reusable service state.
- Do not create a generic `EditorUiState` replacement.

Tests/checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Grep checks for the moved UI field names and remaining broad `SectorEditorUiState&` use in touched modules.

Stop conditions:

- Stop if this phase would become a broad inspector rewrite.
- Stop if feature UI state cannot be separated from modal/document/preview orchestration.

Final report requirements:

- List moved UI fields and new owner.
- State UI behavior unchanged.
- State whether any manual GUI smoke was actually performed.
- Report remaining `SectorEditorUiState` feature groups.

What remains for later phases:

- Additional feature UI groups can become child passes or separate backlog items.

## Phase 7: PreviewState And DocumentState Planning Notes

Goal:

Record that preview/runtime state and document/source-of-truth state are broad enough to require separate runner plans. Do not implement them in REF-084.

Planning notes:

- `PreviewState` requires a separate runner plan because it touches preview control mode, controllers, collision world, runtime object state, preview pose, visual camera effects, overlay tabs, object probes, renderer resource lifetime, and manual render smoke expectations.
- `DocumentState` requires a separate runner plan because it touches authoring graph, derived topology, dirty/path/status, derivation outputs, last-valid topology, load/save, reset, import/migration, and source-of-truth boundaries.
- Do not fold either plan into Phases 1-6.

Files to inspect:

- `docs/architecture/sector_editor_architectural_principles.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorPreviewTypes.h`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/document/` if present
- `sources/sector_demo/renderer/`
- `sources/sector_demo/SectorCollisionWorld.*`

Files likely to modify:

- This plan document only, or a new future runner-plan document if explicitly requested.
- Backlog only if adding future planning items.

Exact implementation steps:

1. Do not move preview or document state in this phase.
2. Summarize remaining preview/document state debt after prior phases.
3. Add or update backlog entries for separate `PreviewState` and `DocumentState` runner plans if they do not exist.
4. Mark this phase completed only as a planning/debt-recording checkpoint.

Behavior guardrails:

- No source code changes.
- No CMake changes.
- No editor/runtime behavior changes.
- No source-hash, collision, rendering, serialization, or cache behavior changes.

Tests/checks:

- `git diff --check`
- `git diff --stat`
- `git status --short`
- Build optional because this is documentation-only.

Stop conditions:

- Stop if a runner invocation asks to implement preview or document state as part of REF-084. Create a separate plan instead.

Final report requirements:

- State no source code changed.
- State preview/document state remain deferred to separate runner plans.
- List backlog entries added or updated.

What remains for later phases:

- Separate `PreviewState` runner plan.
- Separate `DocumentState` runner plan.

## Tests And Verification

Every implementation phase must run:

```bash
cmake --build cmake-build-debug -j2
ctest --test-dir cmake-build-debug --output-on-failure
git diff --check
git diff --stat
git status --short
```

Documentation-only or planning-only phases must run at least:

```bash
git diff --check
git diff --stat
git status --short
```

Each phase must include its grep checks in the final report. The grep checks are not substitutes for build/tests; they prove dependency reduction and help reviewers see whether broad state references remain.

## Manual Smoke Suggestions

Manual smoke is recommended after implementation phases that touch UI or editor behavior, but must not be claimed unless actually performed.

- Phase 1: open texture picker, add a map texture, verify missing texture labels, enter/leave editor shutdown path.
- Phase 2: copy/paste material, edit sidedef UV/decal fields, edit preview surface UV/decal fields, use material texture picker.
- Phase 3: add/delete static and dynamic lights, drag point/spot lights, start/apply/cancel spotlight pilot, bake lightmap if light source-hash-sensitive code was touched.
- Phase 4: select/hover authoring line/vertex/face anchor, topology sector/line/sidedef, light, runtime object, and 3D surface.
- Phase 5: select-drag authoring vertex, light, and runtime object if those transactions moved.
- Phase 6: exercise the specific inspector input group moved in that pass.

## Backlog Updates

This plan was created for REF-084. The backlog entry should say planning is complete and point to this file. It must not mark any implementation phase complete.

Future backlog items may be added for:

- Phase 1 implementation: `TextureCatalogState`.
- Phase 2 implementation: `MaterialEditingState` and material UI state.
- Phase 3 implementation: `LightEditingState`.
- Phase 4 implementation: `SelectionState`.
- Phase 5 implementation: `ManipulationState`.
- Separate `PreviewState` runner plan.
- Separate `DocumentState` runner plan.

## Stop Conditions

- Stop if a phase would need a broad `SectorEditor` rewrite to finish.
- Stop if a phase would require source-of-truth changes to authoring graph vs topology ownership.
- Stop if a phase would change serialization/schema without explicit approval.
- Stop if a phase would change lightmap source-hash behavior without explicit approval and tests.
- Stop if a phase would change rendering, collision, camera physics, or preview runtime behavior.
- Stop if the only way forward is a callback bridge that hides state ownership.
- Stop if a phase would create another broad god state object under a new name.

## Per-Phase Final Report Requirements

Every phase final report must include:

- Selected phase ID and title.
- Files changed.
- Fields moved and their new owner.
- Which service owns or receives the new state.
- Call sites changed.
- Behavior intended unchanged.
- Tests/checks run and results.
- Grep checks run and what they prove.
- Stop conditions encountered, if any.
- Remaining debt for later phases.
- Whether manual GUI verification was performed.

Additional required statements:

- When touching topology mutation code, state cache invalidation behavior.
- When touching lightmaps or light/source-hash-sensitive state, state source-hash behavior.
- When touching gameplay/collision/camera/preview movement, state whether collision, sector lookup, or physics changed.
