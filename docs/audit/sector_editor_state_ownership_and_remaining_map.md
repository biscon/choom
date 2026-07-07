# SectorEditor State Ownership and Remaining Code Map

## Summary

- Current `SectorEditor.cpp` line count: 5,560 lines.
- Current `SectorEditor.h` line count: 380 lines.
- REF-084 is complete. The runner moved several service-specific state groups out of the monolithic `SectorEditorState` / `SectorEditorUiState` and into narrow state structs composed by `SectorEditor`.
- REF-085 is complete. The runner moved preview-owned overlay flags, preview 3D surface hover/selection, preview controller/camera/effect state, preview collision/current-sector result state, and preview runtime object/world-adapter state out of `SectorEditorState` and into `SectorEditorPreviewState` sub-states composed by `SectorEditor`.
- `SectorEditorState` is still a large coordinator/document/tool/modal state object, but it is no longer the owner for texture catalog cache state, copied material state, material input buffers, light drag/edit/pilot light state, core 2D selection/hover state, select/authoring drag state, sector/light ID edit buffers, or preview runtime/controller/collision/selection state.
- The largest remaining ownership problem is now `DocumentState`, tracked as REF-086 in `docs/plans/codebase_refactor_backlog.md`.

Top remaining state ownership concerns:

- Document/source-of-truth state remains mixed into `SectorEditorState`: authoring graph, derived topology, last-valid derived topology, dirty/path/status fields, load/save/reset/import/migration lifecycle, and topology document initialization.
- Runtime object editing is still partially central: `runtimeObjectDrag`, authored runtime object map data, sprite/billboard metadata state, sprite picker, and runtime object UI inputs have not been split.
- Modal/UI state remains central where it is not runtime-reusable preview state: `previewSettingsModal`, texture/add-map/sprite picker lifecycle state, save/load/confirmation/decal/door modals, object-probe debug distance input, and most feature inspector inputs.
- Inspector, manipulation, material picker routing, and placed-object editing modules still take broad `SectorEditorState&` / `SectorEditorUiState&` where behavior crosses document, tool, modal, picker, and UI boundaries.
- Preview modules and preview actions no longer take broad `SectorEditorState&` / `SectorEditorUiState&` and no longer include `SectorEditor.h`; remaining preview debt is primarily document-blocked composition and manual smoke coverage rather than preview-owned state still living in `SectorEditorState`.

Recommended next implementation work:

1. REF-086: execute the dedicated `DocumentState` ownership runner plan before moving authoring graph, derived topology, dirty/path/status, load/save/reset/import, or source-of-truth ownership.
2. Runtime object editing state: split `runtimeObjectDrag`, sprite picker/catalog, billboard metadata repair state, and runtime object UI inputs only after the document boundary is clear.
3. Optional preview follow-up: consider a narrow preview controller/helper only if it can move existing lifecycle orchestration without becoming a generic manager.

## REF-084 Outcome

The completed runner plan is `docs/plans/ref084_service_state_ownership_runner_plan.md`.

Run evidence:

- Latest run summary: `.agent-runs/in-place-ref084_service_state_ownership-2026-07-07-084331/run_all_summary.txt`.
- Runner stopped with `plan complete`.
- Phases 04A, 04B, 04C, 04D, 05, 06, and 07 all passed review in the latest run.
- Earlier completed phases in the plan moved texture, material, and light state.

State moved out of `SectorEditorState`:

| new owner | moved fields / state | notes |
| --- | --- | --- |
| `TextureCatalogState` | `editorTextureScope`, `editorTextureHandlesById` | `SectorTopologyMap::texturesById` remains the map-level texture registry. Add-map modal lifecycle and feature-specific picker apply semantics stayed outside the catalog state. |
| `MaterialEditingState` | copied material payload, now `copiedMaterial` | Material writes still go through authoring-owned material routes. Picker/modal lifecycle stayed separate. |
| `LightEditingState` | `lightDrag`, active light edit transaction, light-owned spotlight pilot data | Static/dynamic light objects remain topology-owned. Preview pose and mouse-look restore remain editor/preview-owned. |
| `SelectionState` | topology selection kind/IDs, selected side kind, wall part, active material layer, selected runtime object ID, inspected topology vertex ID, selected authoring target, 2D hover light/vertex/authoring target fields | Preview-surface selection (`hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D`) remains deferred to preview ownership. |
| `ManipulationState` | `selectDragArm`, `authoringVertexDrag` | Light drag moved to `LightEditingState`; runtime object drag remains deferred. |

State moved out of `SectorEditorUiState`:

| new owner | moved fields / state | notes |
| --- | --- | --- |
| `MaterialEditingUiState` | preview surface UV/decal inputs, topology sector material UV/decal inputs, topology sidedef UV/decal inputs | Generic picker lifecycle and decal tint modal state stayed in their prior owners. |
| `InspectorIdUiState` | `selectedSectorIdBuffer`, `idBufferSectorIndex`, `selectedLightIdBuffer`, `idBufferLightIndex`, `idEditError` | Sector/light ID sync and error display now use the narrow inspector ID UI state. |

State objects now composed by `SectorEditor`:

```cpp
SectorEditorState state;
SectorEditorPreviewState previewState;
SelectionState selectionState;
ManipulationState manipulationState;
LightEditingState lightEditingState;
SectorEditorUiState uiState;
InspectorIdUiState inspectorIdUiState;
TextureCatalogState textureCatalogState;
MaterialEditingState materialEditingState;
MaterialEditingUiState materialEditingUiState;
SectorEditorLightmapBakeController lightmapBake;
```

## REF-085 Outcome

The completed runner plan is `docs/plans/ref085_preview_state_ownership_runner_plan.md`.

State moved out of `SectorEditorState`:

| new owner | moved fields / state | notes |
| --- | --- | --- |
| `SectorEditorPreviewOverlayState` | `useBakedAmbientOcclusion`, `showObjectProbeDebugOverlay`, `previewUiHidden`, `activePreviewDebugOverlayTab` | `useBakedAmbientOcclusion` was verified as a preview render/overlay toggle, not serialized, document-owned, bake-affecting, or source-hash-affecting. |
| `SectorEditorPreviewSelectionState` | `hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D` | Preview 3D surface interaction state stayed separate from 2D/editor `SelectionState`. |
| `SectorEditorPreviewControllerState` | `previewControlMode`, `freeflyController`, `fpsControllerConfig`, `fpsControllerState`, `visualStepOffsetY`, `headBobState`, `landingDipState`, `hasPreviewPose`, `lastPreviewPose`, `spotLightPilotPreviewRestore` | `fpsControllerConfig` remains transient runtime config hydrated from persisted map preview settings; visual step smoothing/headbob/landing dip remain visual-only. |
| `SectorEditorPreviewCollisionState` | `sectorCollisionWorld`, `sectorCollisionWorldValid`, `sectorCollisionWorldWarning`, `previewCollisionSectorId`, `previewVerticalResult`, `previewMoveResult`, `previewCollisionNoclipFallback` | Collision build/rebuild timing was intended unchanged; collision still uses topology-based collision, not generated render triangles. |
| `SectorEditorPreviewRuntimeState` | `SectorRuntimeObjectState runtimeObjects` | Preview/runtime world-adapter state moved; `SectorTopologyMap::runtimeObjects` authored map data did not move. |

Preview state objects now composed by `SectorEditor`:

```cpp
SectorEditorPreviewState previewState;
```

`SectorEditorPreviewState` is defined in
`sources/sector_editor/preview/SectorEditorPreviewState.h` and contains
`overlay`, `selection`, `controller`, `collision`, and `runtime` sub-states.
`SectorMeshRenderer preview` remains owned by `SectorEditor`; REF-085 did not
move renderer ownership.

## Current SectorEditor.cpp Size Snapshot

`wc -l` snapshot after REF-085:

| file | lines |
| --- | ---: |
| `sources/sector_editor/SectorEditor.cpp` | 5,560 |
| `sources/sector_editor/SectorEditor.h` | 380 |
| `sources/sector_editor/SectorEditorAuthoringState.cpp` | 2,481 |
| `sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp` | 2,091 |
| `sources/sector_editor/SectorEditorHelpers.cpp` | 1,590 |
| `sources/sector_editor/SectorEditorTopologyRenderCache.cpp` | 1,331 |
| `sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp` | 1,075 |
| `sources/sector_editor/SectorEditorMaterialActions.cpp` | 970 |
| `sources/sector_editor/preview/SectorEditorPreviewOverlay.cpp` | 924 |
| `sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.cpp` | 928 |
| `sources/sector_editor/SectorEditorLightInspector.cpp` | 927 |
| `sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp` | 925 |
| `sources/sector_editor/selection/SectorEditorSelectionService.cpp` | 905 |
| `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp` | 753 |
| `sources/sector_editor/preview/SectorEditorPreviewUvPanel.cpp` | 550 |
| `sources/sector_editor/SectorEditorSectorInspector.cpp` | 534 |
| `sources/sector_editor/SectorEditorPreviewActions.cpp` | 362 |
| `sources/sector_editor/selection/SectorEditorManipulationService.cpp` | 279 |
| `sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.cpp` | 192 |
| `sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp` | 57 |

## Current SectorEditorState Inventory

| field / group | category | current owner | expected future owner | status / notes |
| --- | --- | --- | --- | --- |
| `topologyMap` | topology-owned global metadata, derived topology data, runtime object data | `SectorEditorState` | `DocumentState` plus compiled/runtime map boundary | Still central. Contains derived geometry, texture registry, lights, runtime objects, preview settings, sky, directional light, lightmap settings, and baked metadata. Per architecture contract, normal editable geometry/material changes should come from authoring graph; texture registry/lights/runtime objects remain intentionally topology-owned for now. |
| `authoringGraph` | authoring source of truth | `SectorEditorState` | `DocumentState` / authoring document model | Still central. Needs REF-086 plan before moving. |
| `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, `authoringDerivationStatus` | derived topology state | `SectorEditorState` | `DocumentState` or `AuthoringDocumentState` | Still central. Needs source-of-truth and derivation lifecycle plan. |
| `topologyDocumentInitialized`, `topologyDocumentDirty`, `topologyDocumentStatus`, `currentLevelName`, `currentLevelPath`, `hasCurrentLevelPath`, `hasUnsavedChanges` | document lifecycle | `SectorEditorState` | `DocumentState` / document controller | Still central. Needs REF-086 plan. |
| `topologyRenderWarning`, `topologyRenderRevision`, `topologyRenderCache` | 2D derived render cache | `SectorEditorState` | 2D view/cache state or document-view adapter | Still central but acceptable. Existing invalidation helpers remain the important contract. |
| `currentTool`, `mode` | coordinator routing | `SectorEditorState` | `SectorEditor` or `EditorModeState` | Still central. Acceptable as top-level coordinator glue. |
| `viewCenter`, `viewZoom`, `gridSize`, `showGrid`, `showAxes`, `showSectorIds`, `snappedMouseMap`, `rawMouseMap` | 2D view/UI transient | `SectorEditorState` | `Editor2DViewState` | Still central. Lower priority than preview/document splits. |
| pending authoring line/rectangle/insert state | tool transaction state | `SectorEditorState` | tool-specific state | Still central. Can move with future tool-state cleanup. |
| `runtimeObjectDrag` | runtime object editing transaction | `SectorEditorState` | `RuntimeObjectEditingState` or placed-object editing state | Still central. Explicit REF-084 debt. |
| default sector heights and default texture IDs | authoring defaults/material defaults | `SectorEditorState` | material/defaults/document state | Still central. Lower priority. |
| preview control, preview UI, controller, collision, pose/effects fields | preview/runtime state | `SectorEditorPreviewState` sub-states | Done by REF-085 | No longer members of `SectorEditorState`. Overlay, controller, collision, and preview pose/effect state now live under `previewState`. |
| `runtimeObjects` | runtime object preview/world adapter state | `SectorEditorPreviewRuntimeState` | Done by REF-085 | Preview runtime adapter moved. Authored map object definitions remain in `SectorTopologyMap::runtimeObjects`; runtime object editing state remains separate debt. |
| `hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D` | preview-surface selection/material refs | `SectorEditorPreviewSelectionState` | Done by REF-085 | Kept out of 2D/editor `SelectionState` intentionally because it is preview-specific. |
| `spotLightPilotPreviewRestore` | preview pose restore state | `SectorEditorPreviewControllerState` | Done by REF-085 | Light-owned pilot data remains in `LightEditingState`; preview pose and mouse-look restore now live with preview controller state. |
| `texturePicker`, `addMapTexture` | modal/picker state | `SectorEditorState` | picker/modal coordinator | Still central. Not part of texture catalog state by design. |
| sprite/billboard metadata and sprite picker | runtime object / sprite picker state | `SectorEditorState` | runtime object editing state or sprite picker state | Still central. Defer until runtime object/preview ownership is scoped. |
| save/load/confirmation/decal/door/preview modals | modal state | `SectorEditorState` | modal or feature state | Still central. Reasonable to defer. |

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

Moved out:

- Material preview/sidedef/sector material input buffers now live in `MaterialEditingUiState`.
- Sector/light selected-ID buffers and shared ID edit error now live in `InspectorIdUiState`.

## Service Ownership Matrix

| service/module | classification after REF-085 | owns behavior? | owns state? | broad `SectorEditorState` dependency? | broad `SectorEditorUiState` dependency? | notes |
| --- | --- | --- | --- | --- | --- | --- |
| `TextureCatalogService` | state-backed service facade | Yes | Yes, via `TextureCatalogState` | No | No | Texture handle cache and texture asset scope are no longer in `SectorEditorState`. Map texture registry remains document/topology data. |
| `TexturePickerService` | backend helper module | Yes | No | No | No | Operates over caller-owned `TexturePickerState`; picker lifecycle remains central. |
| `MaterialEditingService` | behavior service with narrow material state | Yes | Partly, via `MaterialEditingState` and `MaterialEditingUiState` | Yes | No | Still needs document/authoring access through `SectorEditorState`; preview-surface access is now via explicit `SectorEditorPreviewSelectionState`. |
| `MaterialPickerRouting` | backend/material routing module | Yes | No | Yes | No | Still state-coupled because material picker apply crosses document, authoring derivation, preview-surface, and picker state. |
| `LightEditingService` | state-backed service facade | Yes | Yes, via `LightEditingState` | No | No | Light object storage remains topology-owned. UI input reset uses narrow inspector ID state plus explicit UI dependencies. |
| `LightmapBakeController` | real state-owning service | Yes | Yes, private async state | No | No | Still strongest isolated controller; editor installs bake results into the current document. |
| `SelectionService` | narrowed selection service | Yes | Yes, via `SelectionState` | No | No | No broad state/UI dependency remains in the service context; preview-surface references are still passed explicitly. |
| `ManipulationService` | manipulation coordinator | Partial | Yes, via `ManipulationState`, plus light state | Yes | No | Still has broad state for topology/authoring/current-tool/placed-object drag dependencies and callback bridges. |
| Inspector panel modules | UI modules | Yes | Partly, via `InspectorIdUiState` and `MaterialEditingUiState` | Yes | Yes | Still broad because most feature input groups, document state, modal state, and runtime object editing state are central. |
| Preview overlay / UV panel | UI modules | Yes | Partly, via `SectorEditorPreviewState`, material/selection/manipulation/light narrow states, and explicit document/UI dependencies | No | No | REF-085 removed broad editor state/UI dependencies from preview modules. They still receive document/topology and modal/picker/UI pieces explicitly where behavior crosses ownership boundaries. |
| Placed object / door / billboard modules | UI/backend helper modules | Yes | No | Yes | Yes for inspectors | Runtime object editing state remains a future split. |

## State Split Candidates

| candidate | status | likely fields / scope | risk | suggested REF item |
| --- | --- | --- | --- | --- |
| `TextureCatalogState` | Done | editor texture scope and handle cache | Low/Medium | REF-084 complete |
| `MaterialEditingState` | Done | copied material payload | Medium | REF-084 complete |
| `MaterialEditingUiState` | Done | material-specific UV/decal input buffers | Medium | REF-084 complete |
| `LightEditingState` | Done | light drag, light edit transaction, light-owned spotlight pilot data | Medium/High | REF-084 complete |
| `SelectionState` | Done for 2D/editor selection | topology/authoring selected and hovered target fields | Medium | REF-084 complete; preview-surface selection intentionally moved to `SectorEditorPreviewSelectionState` in REF-085 |
| `ManipulationState` | Partly done | select drag arm and authoring vertex drag | Medium | REF-084 complete; runtime object drag deferred |
| `InspectorIdUiState` | Done for selected ID buffers | sector/light ID buffers and shared edit error | Low/Medium | REF-084 complete |
| `LightmapBakeState` | Already done | private bake async state in controller | Low | Already complete |
| `PreviewState` | Done | preview control mode, controllers, collision world/results, preview pose/effects, overlay tabs, object probes, preview-surface selection, runtime object preview state | High | REF-085 complete; renderer ownership intentionally stayed in `SectorEditor` |
| `DocumentState` | Planned next | authoring graph, derived topology, derivation status, dirty/path/status, last-valid topology, load/save/reset/import/migration lifecycle | High | REF-086 remains open |
| `RuntimeObjectEditingState` | Deferred | runtime object drag, sprite/billboard metadata, sprite picker, runtime object UI inputs | Medium/High | Defer until document ownership boundary is clearer |
| `LightEditingUiState` | Deferred | light inspector input buffers | Medium | After preview/document risk is clearer |
| `SectorInspectorUiState` | Deferred | non-material sector property input buffers | Low/Medium | Optional inspector cleanup |
| `ModalState` | Deferred | save/load/confirm/decal/door/preview modal state | Low/Medium | Defer |

## Remaining SectorEditor.cpp Responsibility Inventory

| function/cluster | responsibility | state touched | keep / move later | notes |
| --- | --- | --- | --- | --- |
| Init / Shutdown | asset/runtime/lightmap cleanup lifecycle | preview, runtime objects, texture scopes, modals | Keep | Composition and lifecycle still belong at top level until preview/document splits exist. |
| Update / Render / RenderUI | top-level frame, mode, UI/modal routing | broad | Keep | Coordinator glue. |
| tool context/routing | builds callbacks and routes canvas input | tool, selection, manipulation, document | Move later | Still callback-heavy around manipulation and placed objects. |
| hover/picking helpers | 2D pick/hit tests and hover updates | topology, authoring, selection | Move later | 2D selected/hover state moved, but picking logic still lives centrally. |
| authoring vertex drag wrappers | drag lifecycle for authoring vertices | authoring graph, `ManipulationState` | Move later | State moved, lifecycle callbacks still central. |
| light drag/pilot wrappers | light drag and spotlight pilot orchestration | `LightEditingState`, preview restore, renderer | Move later | Light-owned state moved; preview restore remains central. Source-hash behavior matters for future light edits. |
| runtime object drag/actions wrappers | placed object drag/action context and wrappers | runtime objects, selection, `runtimeObjectDrag` | Defer | Needs runtime object editing state or preview/runtime plan. |
| preview update/lifecycle/collision | freefly/gameplay update, preview enter/leave/rebuild/settings | `SectorEditorPreviewState`, renderer, document map/settings | Keep/defer | Preview-owned state moved, but high-level lifecycle orchestration and `SectorMeshRenderer preview` remain in `SectorEditor`. Optional narrow preview controller/helper is future debt only if justified. |
| pending authoring line/rectangle/insert | tool pending transaction state | tool state, authoring graph | Move later | Tool state remains central. |
| selection wrapper methods | bridge to selection service | `SelectionState`, preview-surface refs, UI states | Move later | Service is narrower now; central wrappers still compose context. |
| document lifecycle | reset/load/save/modal open/list | document state, runtime cleanup, texture catalog | Move after REF-086 | Needs source-of-truth plan. |
| lightmap bake wrapper/use | start/poll/install/modal callbacks | topology map, bake controller, assets | Keep/defer | Controller owns async state; editor installs into document. Source-hash behavior unchanged by this audit. |
| 2D render/cache/draw overlays | grid/cache draw/selection/drag overlays | render cache, view, selection/manipulation/light state | Defer | Cache invalidation remains explicit; do not rebuild expensive topology in steady draw path. |
| main tools/toolbar panel | tool buttons and high-level toggles | broad | Defer | Mostly UI coordinator. |
| inspector routing | compose inspector contexts and replay requests | services, state, UI | Keep | Acceptable central composition until more state splits exist. |
| modal draw wrappers | call extracted modal UIs with callbacks | modal states, services | Defer | Many wrappers are already thin. |
| texture picker/add-map/sky/door/sprite routing | feature picker apply semantics | picker, catalog, runtime object, material service | Move selected pieces | Texture catalog cache moved; feature apply semantics remain central by design. |

## Backlog Situation

Current relevant backlog status:

- REF-083 is complete: this audit exists and is now refreshed after REF-085.
- REF-084 is complete: service-owned editor state migration runner plan has completed all phases.
- REF-085 is complete: preview state ownership runner plan and implementation phases have completed.
- REF-086 remains open: `DocumentState` ownership runner plan and implementation are still pending.

## Behavior Notes

- Topology render-cache invalidation behavior was not changed by this audit update. REF-084 and REF-085 state moves did not add new topology mutation semantics; existing mutation paths still need `MarkTopologyDocumentEdited()` / `InvalidateTopologyRenderCache()` or equivalent.
- Texture registry changes still do not require 2D topology render-cache invalidation unless the cache starts storing texture/material display state.
- Lightmap source-hash behavior was not changed by this audit update. REF-085 kept visual-only preview/sky settings out of source-hash ownership changes and did not alter bake-affecting data. Future document, light, directional-light, sky, or preview-setting moves must continue distinguishing bake-affecting settings from visual-only settings.
- Serialization/schema behavior was not changed by this audit update.
- Rendering, collision, sector lookup, physics, and camera behavior were not changed by this audit update. REF-085 intended to preserve renderer output, collision build/rebuild timing, sector lookup inputs, physics inputs, and the separation between physical pose and visual-only camera effects.
- No manual GUI verification was performed for this audit update.

## Appendix: Evidence

Commands and files inspected for this refresh:

```text
sed -n '1,260p' docs/plans/ref084_service_state_ownership_runner_plan.md
sed -n '1,940p' docs/plans/ref085_preview_state_ownership_runner_plan.md
sed -n '1,260p' docs/plans/codebase_refactor_backlog.md
sed -n '1,220p' sources/sector_editor/SectorEditorTypes.h
sed -n '1,240p' sources/sector_editor/preview/SectorEditorPreviewState.h
rg -n "previewUiHidden|activePreviewDebugOverlayTab|showObjectProbeDebugOverlay|previewControlMode|freeflyController|fpsControllerConfig|fpsControllerState|sectorCollisionWorld|previewCollisionSectorId|previewVerticalResult|previewMoveResult|previewCollisionNoclipFallback|visualStepOffsetY|headBobState|landingDipState|hasPreviewPose|lastPreviewPose|spotLightPilotPreviewRestore|hoveredSurface3D|selectedSurface3D|selectedTopologySurface3D|runtimeObjects" sources/sector_editor/SectorEditorTypes.h
rg -n "SectorEditorState&|SectorEditorUiState&|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/preview sources/sector_editor/SectorEditorPreviewActions.cpp sources/sector_editor/SectorEditorPreviewActions.h
rg -n "REF-085|REF-086" docs/plans/codebase_refactor_backlog.md
rg -n "SectorEditorPreviewState|previewState|SectorMeshRenderer preview|SectorEditorState state|SectorEditorUiState uiState" sources/sector_editor/SectorEditor.h sources/sector_editor/SectorEditor.cpp
rg -n "SectorEditorState&|SectorEditorUiState&|SectorEditorState\*|SectorEditorUiState\*|#include \"sector_editor/SectorEditor.h\"" sources/sector_editor/services/material_edit sources/sector_editor/selection sources/sector_editor/tools/placed_objects sources/sector_editor/preview sources/sector_editor/inspector
rg -n "runtimeObjectDrag|spritePicker|spriteMetadataCatalog|billboardMetadata|previewSettingsModal|objectProbeDebugDrawMaxDistanceInput" sources/sector_editor/SectorEditorTypes.h sources/sector_editor sources/sector_demo tests
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditor.h sources/sector_editor/SectorEditorAuthoringState.cpp sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp sources/sector_editor/preview/SectorEditorPreviewOverlay.cpp sources/sector_editor/preview/SectorEditorPreviewUvPanel.cpp sources/sector_editor/SectorEditorPreviewActions.cpp sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp sources/sector_editor/selection/SectorEditorSelectionService.cpp sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.cpp
```
