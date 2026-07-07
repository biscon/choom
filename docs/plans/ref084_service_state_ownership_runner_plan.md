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
      "status": "Not Started"
    },
    {
      "id": "phase_05",
      "title": "ManipulationState",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_06",
      "title": "InspectorUiState Feature Input Groups",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_07",
      "title": "PreviewState And DocumentState Planning Notes",
      "type": "phase",
      "status": "Not Started"
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
| Phase 4: SelectionState | Not Started |  | Move selected/hovered target state behind selection service boundaries. |
| Phase 5: ManipulationState | Not Started |  | Move active drag/manipulation transaction state behind manipulation service boundaries. |
| Phase 6: InspectorUiState Feature Input Groups | Not Started |  | Optional later split for feature-specific input buffers. |
| Phase 7: PreviewState And DocumentState Planning Notes | Not Started |  | Planning-only checkpoint for separate future runner plans. |

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
