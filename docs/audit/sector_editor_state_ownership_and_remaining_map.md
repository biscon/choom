# SectorEditor State Ownership and Remaining Code Map

## Summary

- Current `SectorEditor.cpp` line count: 5,798 lines.
- Current `SectorEditor.h` line count: 388 lines.
- REF-084 is complete. The runner moved several service-specific state groups out of the monolithic `SectorEditorState` / `SectorEditorUiState` and into narrow state structs composed by `SectorEditor`.
- REF-085 is complete. The runner moved preview-owned overlay flags, preview 3D surface hover/selection, preview controller/camera/effect state, preview collision/current-sector result state, and preview runtime object/world-adapter state out of `SectorEditorState` and into `SectorEditorPreviewState` sub-states composed by `SectorEditor`.
- REF-086 is complete. The runner moved document source, derivation bookkeeping, document map, and lifecycle state out of `SectorEditorState` and into `SectorEditorDocumentState` sub-states composed by `SectorEditor`.
- `SectorEditorState` is still a large coordinator/tool/modal/view state object, but it no longer owns texture catalog cache state, copied material state, material input buffers, light drag/edit/pilot light state, core 2D selection/hover state, select/authoring drag state, sector/light ID edit buffers, preview runtime/controller/collision/selection state, or document source/lifecycle state.

Top remaining state ownership concerns:

- The 2D topology render cache still lives in `SectorEditorState`. REF-086 intentionally deferred it because it is a derived 2D editor/view cache, not document source data; existing invalidation helpers remain the important contract.
- Runtime object editing is still partially central: `runtimeObjectDrag`, sprite/billboard metadata state, sprite picker, and runtime object UI inputs have not been split. Authored runtime object definitions remain intentionally under `SectorTopologyMap::runtimeObjects` for now.
- Modal/UI state remains central where it is not runtime-reusable preview state: `previewSettingsModal`, texture/add-map/sprite picker lifecycle state, save/load/confirmation/decal/door modals, object-probe debug distance input, and most feature inspector inputs.
- Inspector, tool routing, and placed-object editing modules still take broad `SectorEditorState&` / `SectorEditorUiState&` where behavior crosses tool transaction, modal, picker, runtime-object editing, and UI input boundaries.
- Preview modules and preview actions no longer take broad `SectorEditorState&` / `SectorEditorUiState&` and no longer include `SectorEditor.h`; REF-086 remained executable independently of REF-085 implementation status.

Recommended next implementation work:

1. Runtime object editing state: split `runtimeObjectDrag`, sprite picker/catalog, billboard metadata repair state, and runtime object UI inputs under a scoped runner plan.
2. Optional 2D view/cache state: move `topologyRenderWarning`, `topologyRenderRevision`, and `topologyRenderCache` only if a dedicated plan can preserve cache invalidation and picking/drawing consistency.
3. Optional modal/UI state cleanup: split save/load/confirmation/texture/sprite/door/decal modal state only in small feature-specific passes.

## REF-084 Outcome

The completed runner plan is `docs/plans/ref084_service_state_ownership_runner_plan.md`.

State moved out of `SectorEditorState`:

| new owner | moved fields / state | notes |
| --- | --- | --- |
| `TextureCatalogState` | `editorTextureScope`, `editorTextureHandlesById` | `SectorTopologyMap::texturesById` remains the map-level texture registry. Add-map modal lifecycle and feature-specific picker apply semantics stayed outside the catalog state. |
| `MaterialEditingState` | copied material payload, now `copiedMaterial` | Material writes still go through authoring-owned material routes. Picker/modal lifecycle stayed separate. |
| `LightEditingState` | `lightDrag`, active light edit transaction, light-owned spotlight pilot data | Static/dynamic light objects remain topology-owned. Preview pose and mouse-look restore moved with preview controller state in REF-085. |
| `SelectionState` | topology selection kind/IDs, selected side kind, wall part, active material layer, selected runtime object ID, inspected topology vertex ID, selected authoring target, 2D hover light/vertex/authoring target fields | Preview-surface selection moved to `SectorEditorPreviewSelectionState` in REF-085. |
| `ManipulationState` | `selectDragArm`, `authoringVertexDrag` | Light drag moved to `LightEditingState`; runtime object drag remains deferred. |

State moved out of `SectorEditorUiState`:

| new owner | moved fields / state | notes |
| --- | --- | --- |
| `MaterialEditingUiState` | preview surface UV/decal inputs, topology sector material UV/decal inputs, topology sidedef UV/decal inputs | Generic picker lifecycle and decal tint modal state stayed in their prior owners. |
| `InspectorIdUiState` | `selectedSectorIdBuffer`, `idBufferSectorIndex`, `selectedLightIdBuffer`, `idBufferLightIndex`, `idEditError` | Sector/light ID sync and error display now use the narrow inspector ID UI state. |

## REF-085 Outcome

The completed runner plan is `docs/plans/ref085_preview_state_ownership_runner_plan.md`.

State moved out of `SectorEditorState`:

| new owner | moved fields / state | notes |
| --- | --- | --- |
| `SectorEditorPreviewOverlayState` | `useBakedAmbientOcclusion`, `showObjectProbeDebugOverlay`, `previewUiHidden`, `activePreviewDebugOverlayTab` | `useBakedAmbientOcclusion` is a preview render/overlay toggle, not serialized, document-owned, bake-affecting, or source-hash-affecting. |
| `SectorEditorPreviewSelectionState` | `hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D` | Preview 3D surface interaction state stayed separate from 2D/editor `SelectionState`. |
| `SectorEditorPreviewControllerState` | `previewControlMode`, `freeflyController`, `fpsControllerConfig`, `fpsControllerState`, `visualStepOffsetY`, `headBobState`, `landingDipState`, `hasPreviewPose`, `lastPreviewPose`, `spotLightPilotPreviewRestore` | `fpsControllerConfig` remains transient runtime config hydrated from persisted map preview settings; visual offsets remain visual-only. |
| `SectorEditorPreviewCollisionState` | `sectorCollisionWorld`, `sectorCollisionWorldValid`, `sectorCollisionWorldWarning`, `previewCollisionSectorId`, `previewVerticalResult`, `previewMoveResult`, `previewCollisionNoclipFallback` | Collision still uses topology-based collision, not generated render triangles. |
| `SectorEditorPreviewRuntimeState` | `SectorRuntimeObjectState runtimeObjects` | Preview/runtime world-adapter state moved; `SectorTopologyMap::runtimeObjects` map-authored definitions did not move. |

`SectorMeshRenderer preview` remains owned by `SectorEditor`; REF-085 did not move renderer ownership.

## REF-086 Outcome

The completed runner plan is `docs/plans/ref086_document_state_ownership_runner_plan.md`.

State moved out of `SectorEditorState`:

| new owner | moved fields / state | classification | notes |
| --- | --- | --- | --- |
| `SectorEditorAuthoringDocumentState` | `authoringGraph` | document source data | The authoring graph remains the editable source of truth for normal geometry, sector property, and material editing. |
| `SectorEditorDerivationState` | `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, `authoringDerivationStatus` | derivation bookkeeping | Phase 3 split derivation bookkeeping from the document map state so stale/current/last-valid status is not conflated with the compiled map owner. |
| `SectorEditorDocumentMapState` | `topologyMap` | compiled/derived document map plus documented map-level metadata/runtime definitions | `SectorTopologyMap` remains derived output plus intentionally topology-owned global metadata/runtime definitions. It still contains texture registry, lights, runtime objects, preview settings, sky settings, directional light, lightmap settings, and baked metadata. Moving ownership did not reclassify those as authoring graph source data. |
| `SectorEditorDocumentLifecycleState` | `topologyDocumentInitialized`, `topologyDocumentDirty`, `topologyDocumentStatus`, `currentLevelName`, `currentLevelPath`, `hasCurrentLevelPath`, `hasUnsavedChanges` | document lifecycle state | Load/save/reset/import/migration paths now operate through document-owned lifecycle state while preserving JSON shape and dirty/status behavior. |

Document state objects now composed by `SectorEditor`:

```cpp
SectorEditorDocumentState documentState;
```

`SectorEditorDocumentState` is defined in
`sources/sector_editor/document/SectorEditorDocumentState.h` and contains
`authoring`, `derivation`, `map`, and `lifecycle` sub-states plus narrow access
views. No `SectorEditorDocumentController` was created.

## Current SectorEditor.cpp Size Snapshot

`wc -l` snapshot after REF-086:

| file | lines |
| --- | ---: |
| `sources/sector_editor/SectorEditor.cpp` | 5,798 |
| `sources/sector_editor/SectorEditor.h` | 388 |
| `sources/sector_editor/SectorEditorAuthoringState.cpp` | 2,764 |
| `sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp` | 2,163 |
| `sources/sector_editor/SectorEditorHelpers.cpp` | 1,590 |
| `sources/sector_editor/SectorEditorTopologyRenderCache.cpp` | 1,331 |
| `sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp` | 1,075 |
| `sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.cpp` | 1,121 |
| `sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp` | 962 |
| `sources/sector_editor/SectorEditorMaterialActions.cpp` | 970 |
| `sources/sector_editor/preview/SectorEditorPreviewOverlay.cpp` | 924 |
| `sources/sector_editor/SectorEditorLightInspector.cpp` | 927 |
| `sources/sector_editor/selection/SectorEditorSelectionService.cpp` | 905 |
| `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp` | 754 |
| `sources/sector_editor/preview/SectorEditorPreviewUvPanel.cpp` | 550 |
| `sources/sector_editor/SectorEditorSectorInspector.cpp` | 535 |
| `sources/sector_editor/SectorEditorPreviewActions.cpp` | 362 |
| `sources/sector_editor/selection/SectorEditorManipulationService.cpp` | 281 |
| `sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.cpp` | 192 |
| `sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp` | 57 |

## Current SectorEditorState Inventory

| field / group | category | current owner | expected future owner | status / notes |
| --- | --- | --- | --- | --- |
| `topologyRenderWarning`, `topologyRenderRevision`, `topologyRenderCache` | 2D derived render cache | `SectorEditorState` | 2D view/cache state or document-view adapter | Still central and intentionally deferred by REF-086. The cache is editor/view-derived, not document source data. Existing invalidation helpers remain the important contract. |
| `currentTool`, `mode` | coordinator routing | `SectorEditorState` | `SectorEditor` or `EditorModeState` | Still central. Acceptable as top-level coordinator glue. |
| `viewCenter`, `viewZoom`, `gridSize`, `showGrid`, `showAxes`, `showSectorIds`, `snappedMouseMap`, `rawMouseMap` | 2D view/UI transient | `SectorEditorState` | `Editor2DViewState` | Still central. Lower priority than document and preview splits. |
| pending authoring line/rectangle/insert state | tool transaction state | `SectorEditorState` | tool-specific state | Still central. Can move with future tool-state cleanup. |
| `runtimeObjectDrag` | runtime object editing transaction | `SectorEditorState` | `RuntimeObjectEditingState` or placed-object editing state | Still central. Explicit future debt. |
| default sector heights and default texture IDs | authoring defaults/material defaults | `SectorEditorState` | material/defaults/document-adjacent state | Still central. Lower priority; do not fold into DocumentState without a scoped task. |
| `texturePicker`, `addMapTexture` | modal/picker state | `SectorEditorState` | picker/modal coordinator | Still central. Not part of texture catalog state by design. |
| sprite/billboard metadata and sprite picker | runtime object / sprite picker state | `SectorEditorState` | runtime object editing state or sprite picker state | Still central. Defer until runtime object editing ownership is scoped. |
| save/load/confirmation/decal/door/preview modals | modal state | `SectorEditorState` | modal or feature state | Still central. Reasonable to defer. |

Moved out:

- Document source, derivation bookkeeping, compiled/document map, and document lifecycle state now live under `SectorEditorDocumentState`.
- Preview control, preview UI, controller, collision, pose/effects, preview runtime, and preview-surface selection state now live under `SectorEditorPreviewState`.
- Service state from REF-084 remains outside `SectorEditorState` as documented above.

## Current SectorEditorUiState Inventory

| field / group | current owner | expected future owner | status / notes |
| --- | --- | --- | --- |
| `config` | `SectorEditorUiState` | top-level editor UI | Keep central for now. |
| `gridSizeInput` | `SectorEditorUiState` | 2D view/tools panel state | Still central. |
| sector floor/ceiling/ambient inputs | `SectorEditorUiState` | sector inspector UI state | Still central. Sector material inputs moved to `MaterialEditingUiState`; non-material sector property inputs did not. |
| light inspector inputs | `SectorEditorUiState` | `LightEditingUiState` | Still central. `LightEditingState` owns behavior/transaction state, not UI input buffers. |
| runtime object inspector inputs | `SectorEditorUiState` | `RuntimeObjectEditingUiState` | Still central. |
| `toolsScroll`, `inspectorScroll` | `SectorEditorUiState` | top-level panel state | Still central and acceptable. |
| `keyboardCaptured` | `SectorEditorUiState` | top-level input/UI coordinator | Keep central for now. |

## Service Ownership Matrix

| service/module | classification after REF-086 | owns behavior? | owns state? | broad `SectorEditorState` dependency? | broad `SectorEditorUiState` dependency? | notes |
| --- | --- | --- | --- | --- | --- | --- |
| `TextureCatalogService` | state-backed service facade | Yes | Yes, via `TextureCatalogState` | No | No | Texture handle cache and texture asset scope are no longer in `SectorEditorState`. Map texture registry remains document/topology data. |
| `TexturePickerService` | backend helper module | Yes | No | No | No | Operates over caller-owned `TexturePickerState`; picker lifecycle remains central. |
| `MaterialEditingService` | behavior service with narrow material state and document references | Yes | Partly, via `MaterialEditingState` and `MaterialEditingUiState` | No | No | Uses explicit authoring graph, derivation, document map, lifecycle, preview-surface, texture-picker, and material state references. |
| `MaterialPickerRouting` | backend/material routing module | Yes | No | No | No | Uses explicit document/authoring/derivation/map/lifecycle and picker references. |
| `LightEditingService` | state-backed service facade | Yes | Yes, via `LightEditingState` | No | No | Light object storage remains topology-owned in the document map. UI input reset uses narrow inspector ID state plus explicit UI dependencies. |
| `LightmapBakeController` | real state-owning service | Yes | Yes, private async state | No | No | Still strongest isolated controller; editor installs bake results into the current document map. |
| `SelectionService` | narrowed selection service | Yes | Yes, via `SelectionState` | No | No | No broad state/UI dependency remains in the service context; preview-surface references are passed explicitly. |
| `ManipulationService` | manipulation coordinator | Partial | Yes, via `ManipulationState`, plus light state | No | No | Broad state was removed; placed-object drag remains a runtime-object editing boundary. |
| Inspector panel modules | UI modules | Yes | Partly, via `InspectorIdUiState` and `MaterialEditingUiState` | Yes | Yes | Still broad because modal state, current-tool routing, runtime-object editing state, and non-material input groups are central. |
| Preview overlay / UV panel | UI modules | Yes | Partly, via `SectorEditorPreviewState`, material/selection/manipulation/light narrow states, and explicit document/UI dependencies | No | No | Preview modules receive document/topology pieces explicitly where behavior crosses ownership boundaries. |
| Placed object / door / billboard modules | UI/backend helper modules | Yes | No | Yes | Yes for inspectors | Runtime object editing state remains a future split. |

## State Split Candidates

| candidate | status | likely fields / scope | risk | suggested REF item |
| --- | --- | --- | --- | --- |
| `TextureCatalogState` | Done | editor texture scope and handle cache | Low/Medium | REF-084 complete |
| `MaterialEditingState` | Done | copied material payload | Medium | REF-084 complete |
| `MaterialEditingUiState` | Done | material-specific UV/decal input buffers | Medium | REF-084 complete |
| `LightEditingState` | Done | light drag, light edit transaction, light-owned spotlight pilot data | Medium/High | REF-084 complete |
| `SelectionState` | Done for 2D/editor selection | topology/authoring selected and hovered target fields | Medium | REF-084 complete; preview-surface selection moved to `SectorEditorPreviewSelectionState` in REF-085 |
| `ManipulationState` | Partly done | select drag arm and authoring vertex drag | Medium | REF-084 complete; runtime object drag deferred |
| `InspectorIdUiState` | Done for selected ID buffers | sector/light ID buffers and shared edit error | Low/Medium | REF-084 complete |
| `PreviewState` | Done | preview control mode, controllers, collision world/results, preview pose/effects, overlay tabs, object probes, preview-surface selection, runtime object preview state | High | REF-085 complete; renderer ownership intentionally stayed in `SectorEditor` |
| `DocumentState` | Done | authoring graph, derivation bookkeeping, document map, dirty/path/status lifecycle | High | REF-086 complete; topology render cache deferred |
| `TopologyViewCacheState` | Deferred | `topologyRenderWarning`, `topologyRenderRevision`, `topologyRenderCache` | Medium | Future 2D view/cache plan only if useful |
| `RuntimeObjectEditingState` | Deferred | runtime object drag, sprite/billboard metadata, sprite picker, runtime object UI inputs | Medium/High | Defer until document ownership boundary is stable |
| `LightEditingUiState` | Deferred | light inspector input buffers | Medium | After document/runtime-object risk is clearer |
| `SectorInspectorUiState` | Deferred | non-material sector property input buffers | Low/Medium | Optional inspector cleanup |
| `ModalState` | Deferred | save/load/confirm/decal/door/preview modal state | Low/Medium | Defer |

## Remaining SectorEditor.cpp Responsibility Inventory

| function/cluster | responsibility | state touched | keep / move later | notes |
| --- | --- | --- | --- | --- |
| Init / Shutdown | asset/runtime/lightmap cleanup lifecycle | preview, runtime objects, texture scopes, modals, document | Keep | Composition and lifecycle still belong at top level until narrower feature controllers are justified. |
| Update / Render / RenderUI | top-level frame, mode, UI/modal routing | broad | Keep | Coordinator glue. |
| tool context/routing | builds contexts and routes canvas input | tool, selection, manipulation, document | Move later | Tool transaction state and top-level routing remain central. |
| hover/picking helpers | 2D pick/hit tests and hover updates | document map/authoring, selection | Move later | 2D selected/hover state moved, but picking logic still lives centrally. |
| authoring vertex drag wrappers | drag lifecycle for authoring vertices | document authoring graph, `ManipulationState` | Move later | State moved, lifecycle orchestration still central. |
| light drag/pilot wrappers | light drag and spotlight pilot orchestration | document map, `LightEditingState`, preview restore, renderer | Move later | Light-owned state moved; source-hash behavior matters for future light edits. |
| runtime object drag/actions wrappers | placed object drag/action context and wrappers | document map runtime definitions, selection, `runtimeObjectDrag` | Defer | Needs runtime object editing state plan. |
| preview update/lifecycle/collision | freefly/gameplay update, preview enter/leave/rebuild/settings | `SectorEditorPreviewState`, renderer, document map/settings | Keep/defer | Preview-owned state moved, but high-level lifecycle orchestration and `SectorMeshRenderer preview` remain in `SectorEditor`. |
| pending authoring line/rectangle/insert | tool pending transaction state | tool state, document authoring graph | Move later | Tool state remains central. |
| selection wrapper methods | bridge to selection service | `SelectionState`, preview-surface refs, UI states, document | Move later | Service is narrower now; central wrappers still compose context. |
| document lifecycle | reset/load/save/modal open/list | `SectorEditorDocumentState`, runtime cleanup, texture catalog | Keep | Document state moved; top-level orchestration remains central. No document controller was created. |
| lightmap bake wrapper/use | start/poll/install/modal callbacks | document map, bake controller, assets | Keep/defer | Controller owns async state; editor installs into the current document. Source-hash behavior unchanged by this audit. |
| 2D render/cache/draw overlays | grid/cache draw/selection/drag overlays | render cache, view, selection/manipulation/light state, document | Defer | Cache invalidation remains explicit; do not rebuild expensive topology in steady draw path. |
| main tools/toolbar panel | tool buttons and high-level toggles | broad | Defer | Mostly UI coordinator. |
| inspector routing | compose inspector contexts and replay requests | services, document, state, UI | Keep | Acceptable central composition until more state splits exist. |
| modal draw wrappers | call extracted modal UIs with callbacks | modal states, services, document | Defer | Many wrappers are already thin. |
| texture picker/add-map/sky/door/sprite routing | feature picker apply semantics | picker, catalog, runtime object, document, material service | Move selected pieces | Texture catalog cache moved; feature apply semantics remain central by design. |

## Backlog Situation

Current relevant backlog status:

- REF-083 is complete: this audit exists and is now refreshed after REF-086.
- REF-084 is complete: service-owned editor state migration runner plan has completed all phases.
- REF-085 is complete: preview state ownership runner plan and implementation phases have completed.
- REF-086 is complete: document state ownership runner plan and implementation phases have completed.

## Behavior Notes

- Topology render-cache invalidation behavior was not changed by this audit update. REF-086 did not move the 2D topology render cache and did not change topology mutation semantics; existing mutation paths still need `MarkTopologyDocumentEdited()` / `InvalidateTopologyRenderCache()` or equivalent.
- Texture registry changes still do not require 2D topology render-cache invalidation unless the cache starts storing texture/material display state.
- Lightmap source-hash behavior was not changed by this audit update. REF-086 preserved the existing split between bake-affecting data and visual-only preview/sky settings; `ceilingSky` remains geometry-affecting through the map data and sky visual settings remain excluded.
- Serialization/schema behavior was not changed by this audit update. REF-086 moved in-memory ownership only and preserved save/load JSON shape and import/migration behavior.
- Rendering, collision, sector lookup, physics, and camera behavior were not changed by this audit update. Preview state remained separate, renderer ownership stayed outside DocumentState, and collision/sector lookup/physics/camera paths were not intentionally changed.
- No manual GUI verification was performed for this audit update.

## Appendix: Evidence

Commands and files inspected for this refresh:

```text
sed -n '1,260p' docs/plans/ref086_document_state_ownership_runner_plan.md
sed -n '1,180p' docs/architecture/sector_editor_architectural_principles.md
sed -n '1,120p' docs/plans/ref084_service_state_ownership_runner_plan.md
sed -n '1,140p' docs/plans/ref085_preview_state_ownership_runner_plan.md
sed -n '1,120p' docs/plans/codebase_refactor_backlog.md
sed -n '1160,1225p' docs/plans/codebase_refactor_backlog.md
sed -n '1,220p' sources/sector_editor/document/SectorEditorDocumentState.h
sed -n '1,150p' sources/sector_editor/SectorEditorTypes.h
sed -n '1,150p' sources/sector_editor/SectorEditor.h
rg -n "topologyMap|authoringGraph|authoringDerivation|lastValidAuthoringDerivedTopology|authoringDerivationState|authoringDerivedTopologyStale|authoringDerivationStatus|topologyDocumentInitialized|topologyDocumentDirty|topologyDocumentStatus|currentLevelName|currentLevelPath|hasCurrentLevelPath|hasUnsavedChanges|topologyRenderWarning|topologyRenderRevision|topologyRenderCache" sources/sector_editor/SectorEditorTypes.h
rg -n "SectorEditorState&|const SectorEditorState&|SectorEditorState\\*|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/document sources/sector_editor/tools sources/sector_editor/inspector sources/sector_editor/selection sources/sector_editor/preview sources/sector_editor/services
rg -n "REF-085|REF-086|DocumentState|PreviewState" docs/plans/codebase_refactor_backlog.md docs/audit/sector_editor_state_ownership_and_remaining_map.md
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditor.h sources/sector_editor/SectorEditorAuthoringState.cpp sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp sources/sector_editor/SectorEditorHelpers.cpp sources/sector_editor/SectorEditorTopologyRenderCache.cpp sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp sources/sector_editor/SectorEditorMaterialActions.cpp sources/sector_editor/preview/SectorEditorPreviewOverlay.cpp sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.cpp sources/sector_editor/SectorEditorLightInspector.cpp sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp sources/sector_editor/selection/SectorEditorSelectionService.cpp sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp sources/sector_editor/preview/SectorEditorPreviewUvPanel.cpp sources/sector_editor/SectorEditorSectorInspector.cpp sources/sector_editor/SectorEditorPreviewActions.cpp sources/sector_editor/selection/SectorEditorManipulationService.cpp sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.cpp sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp
```
