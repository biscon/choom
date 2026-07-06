# SectorEditor State Ownership and Remaining Code Map

## Summary

- Current `SectorEditor.cpp` line count: 5,305 lines.
- Current `SectorEditor.h` line count: 366 lines.
- `SectorEditorState` is still acting as a god state object. Recent services own meaningful behavior, but most durable state still lives in `SectorEditorState` and `SectorEditorUiState`.
- Top 5 state ownership concerns:
  - Texture catalog data and GPU handle cache are still stored in `SectorEditorState`; `TextureCatalogService` is a context-backed facade.
  - Material editing behavior is service-owned, but picker/modal state, copied material state, selected 3D surface state, and material UI inputs are still central.
  - Light editing behavior is service-owned, but light drag/pilot/selection and all light objects remain central/topology-backed.
  - Preview/runtime/gameplay state is mixed with editor document, modal, and inspector state.
  - Selection/manipulation state remains central, with services mostly operating over `SectorEditorState` through context/callback bridges.
- Top 5 remaining `SectorEditor.cpp` clusters:
  - Top-level lifecycle/update/render/UI orchestration.
  - Canvas/tool routing, hover, picking, and manipulation bridge setup.
  - Preview lifecycle, gameplay controller, collision-world, and spot-light pilot orchestration.
  - Document lifecycle and modal orchestration.
  - Texture picker/add-map/door/sky/sprite routing and remaining service wrappers.
- Recommended next implementation task: split a narrow `TextureCatalogState` out of `SectorEditorState`, then make `TextureCatalogService` a real state-owning service facade over that object while keeping add-map modal lifecycle and feature apply semantics outside the catalog service.

## Current SectorEditor.cpp Size Snapshot

`wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditor.h`:

| file | lines |
| --- | ---: |
| `sources/sector_editor/SectorEditor.cpp` | 5,305 |
| `sources/sector_editor/SectorEditor.h` | 366 |

Major extracted modules/services:

| file/module | lines |
| --- | ---: |
| `sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp` | 2,068 |
| `sources/sector_editor/SectorEditorAuthoringState.cpp` | 2,404 |
| `sources/sector_editor/SectorEditorHelpers.cpp` | 1,590 |
| `sources/sector_editor/SectorEditorTopologyRenderCache.cpp` | 1,331 |
| `sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp` | 1,067 |
| `sources/sector_editor/SectorEditorMaterialActions.cpp` | 970 |
| `sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.cpp` | 929 |
| `sources/sector_editor/SectorEditorLightInspector.cpp` | 923 |
| `sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp` | 918 |
| `sources/sector_editor/selection/SectorEditorSelectionService.cpp` | 896 |
| `sources/sector_editor/preview/SectorEditorPreviewOverlay.cpp` | 851 |
| `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp` | 751 |
| `sources/sector_editor/tools/billboards/SectorEditorBillboardInspector.cpp` | 644 |
| `sources/sector_editor/preview/SectorEditorPreviewUvPanel.cpp` | 536 |
| `sources/sector_editor/SectorEditorSectorInspector.cpp` | 531 |
| `sources/sector_editor/SectorEditorTextureModals.cpp` | 548 |
| `sources/sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.cpp` | 362 |
| `sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.cpp` | 192 |
| `sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp` | 57 |

Current top large files under `sources/sector_editor`:

| file | lines |
| --- | ---: |
| `SectorEditor.cpp` | 5,305 |
| `SectorEditorAuthoringState.cpp` | 2,404 |
| `inspector/SectorEditorInspectorPanel.cpp` | 2,068 |
| `SectorEditorHelpers.cpp` | 1,590 |
| `SectorEditorTopologyRenderCache.cpp` | 1,331 |
| `services/lights/SectorEditorLightEditingService.cpp` | 1,067 |
| `SectorEditorMaterialActions.cpp` | 970 |
| `services/material_edit/SectorEditorMaterialPickerRouting.cpp` | 929 |
| `SectorEditorLightInspector.cpp` | 923 |
| `services/material_edit/SectorEditorMaterialEditingService.cpp` | 918 |

## SectorEditorState Inventory

| field / group | type | category | current owner | ideal owner | runtime/game-mode relevance | editor-only? | recommended action | notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `topologyMap` | `SectorTopologyMap` | topology-owned global metadata / derived topology data / runtime object state | `SectorEditorState` | `DocumentState` plus compiled/runtime map boundary | High | No | Split later | Contains derived geometry, texture registry, lights, runtime objects, preview settings, sky, directional light, lightmap settings, baked metadata. Per contract, editable geometry should come from authoring graph; texture registry/lights/runtime objects are intentionally topology-owned for now. |
| `authoringGraph` | `SectorAuthoringGraph` | authoring graph data | `SectorEditorState` | `DocumentState` / authoring document model | Editor authoring only | Mostly | Split later | Editable source of truth for normal map geometry/properties. |
| `authoringDerivation`, `lastValidAuthoringDerivedTopology`, `authoringDerivationState`, `authoringDerivedTopologyStale`, `authoringDerivationStatus` | derivation result/status | derived topology data | `SectorEditorState` | `DocumentState` or `AuthoringDocumentState` | Medium: renderer/runtime consume compiled output | Partly | Split with document state | Keeps mapping from derived IDs back to authoring graph; needed by editor and preview, but not by pure runtime after compile. |
| `topologyDocumentInitialized`, `topologyDocumentDirty`, `topologyDocumentStatus`, `currentLevelName`, `currentLevelPath`, `hasCurrentLevelPath`, `hasUnsavedChanges` | bool/string | document lifecycle state | `SectorEditorState` | `DocumentState` / document controller | Low | Yes | Split later | Save/load state and dirty status. |
| `topologyRenderWarning`, `topologyRenderRevision`, `topologyRenderCache` | string/revision/cache | derived topology data / cache data | `SectorEditorState` | `TopologyRenderCacheState` owned by 2D editor view | Low | Yes | Keep or split with 2D view | Cache invalidation already centralized enough; future split can reduce god-state coupling. |
| `currentTool`, `mode` | enums | composition/lifecycle state | `SectorEditorState` | `SectorEditor` or `EditorModeState` | Medium | Yes | Keep in `SectorEditor` for now | High-level routing state is acceptable coordinator glue. |
| `viewCenter`, `viewZoom`, `gridSize`, `showGrid`, `showAxes`, `showSectorIds` | view/grid values | UI transient state | `SectorEditorState` | `Editor2DViewState` | Low | Yes | Split later only if extracting 2D view controller | No runtime need. |
| topology selection fields | selection IDs/kinds | selection/manipulation state | `SectorEditorState` | `SelectionState` | Low | Yes | Split candidate | Includes selected sector/vertex/sidedef/linedef/lights/runtime object, wall part/layer. |
| hover/inspect fields | IDs/flags/points | selection/manipulation state | `SectorEditorState` | `SelectionState` or `Editor2DViewState` | Low | Yes | Split candidate | `hovered*`, `inspectedTopologyVertexId`, authoring hover. |
| `selectedAuthoring`, `hoveredAuthoring` | `SectorAuthoringSelectionTarget` | selection/manipulation state | `SectorEditorState` | `SelectionState` | Low | Yes | Split candidate | Tied to authoring graph selection. |
| `snappedMouseMap`, `rawMouseMap` | `Vector2` | UI transient state | `SectorEditorState` | `Editor2DViewState` | Low | Yes | Defer | Fine while canvas routing remains central. |
| pending authoring tool state | `PendingAuthoringLineDraw`, `PendingAuthoringRectangleDraw`, `PendingAuthoringInsertVertex` | selection/manipulation state | `SectorEditorState` | tool-specific state | Low | Yes | Split with tools later | Line/rectangle/insert tools still rely on central state. |
| `authoringVertexDrag` | `AuthoringVertexDragState` | selection/manipulation state | `SectorEditorState` | `ManipulationState` | Low | Yes | Split candidate | Current manipulation service delegates back to `SectorEditor` for this lifecycle. |
| `lightDrag`, `lightEditing`, `spotLightPilot` | drag/edit/pilot state | light editing state | `SectorEditorState` | `LightEditingState` owned by light service, plus preview pilot coordinator | Medium | Yes | Split candidate after source-hash audit | Behavior moved, state did not. |
| `runtimeObjectDrag` | `RuntimeObjectDragState` | runtime object state / manipulation state | `SectorEditorState` | `RuntimeObjectEditingState` or `ManipulationState` | Medium | Partly | Defer | Runtime objects may matter to game mode, drag state is editor-only. |
| default texture IDs and default sector heights | strings/floats | material editing state / texture/catalog state | `SectorEditorState` | `MaterialDefaultsState` or document/tool defaults | Low | Yes | Defer | Defaults seed new authored geometry/materials. |
| `useBakedAmbientOcclusion`, `showObjectProbeDebugOverlay`, `previewUiHidden`, `activePreviewDebugOverlayTab`, `previewControlMode` | bool/enums | preview/runtime state / UI transient | `SectorEditorState` | `PreviewState` / preview UI state | Medium | Partly | Split later | Runtime preview behavior mixed with UI display toggles. |
| `runtimeObjects` | `SectorRuntimeObjectState` | runtime object state | `SectorEditorState` | `PreviewRuntimeState` or runtime world adapter | High | No | Split later | Object ECS/world runtime data should be reusable without editor state. |
| `freeflyController`, `fpsControllerConfig`, `fpsControllerState`, collision fields, headbob/landing/pose fields | controller/collision structs | preview/runtime state | `SectorEditorState` | `PreviewState` / gameplay preview controller | High | Partly | Split later | Must preserve rule that visual camera effects do not feed collision/physics. |
| `hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D`, `copiedTopologyMaterial` | surface/material refs | preview/runtime state / material editing state | `SectorEditorState` | `PreviewSelectionState` plus `MaterialEditingState` | Medium | Yes | Split candidate | Shared by preview UV panel and material service. |
| `editorTextureScope`, `editorTextureHandlesById` | asset handles/map | texture/catalog state | `SectorEditorState` | `TextureCatalogState` | Medium | Partly | Implement next | Texture registry lives in map, but loaded editor handles are service/cache state. |
| `texturePicker` | `TexturePickerState` | modal state / texture picker UI state | `SectorEditorState` | `TexturePickerState` owned by picker coordinator | Low | Yes | Defer | Generic picker lifecycle already extracted as free helper; still central state. |
| `addMapTexture` | `AddMapTextureState` | modal state / texture/catalog state | `SectorEditorState` | Add-map modal state, not catalog service | Low | Yes | Keep separate from catalog | Includes preview scope lifetime; should not be absorbed into generic catalog. |
| `spriteMetadataCatalog`, `billboardMetadata*`, `spritePicker` | sprite/catalog/modal state | runtime object state / modal state | `SectorEditorState` | Billboard/sprite picker state | Medium | Partly | Defer | Billboard sprite metadata is feature-specific. |
| save/load/confirmation/decal/door/preview modals | modal structs | modal state | `SectorEditorState` | `ModalState` or feature modal state | Low | Yes | Defer | Some modal orchestration should stay central until a modal controller exists. |

## SectorEditorUiState Inventory

| field / group | type | category | current owner | ideal owner | runtime/game-mode relevance | editor-only? | recommended action | notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `config` | `engine::UIConfig` | UI transient state | `SectorEditorUiState` | top-level editor UI | None | Yes | Keep | Shared UI config is acceptable central UI state. |
| `gridSizeInput` | `UIIntInputState` | UI transient state | `SectorEditorUiState` | tools/view panel state | None | Yes | Defer | Belongs with tools/main panel if extracted. |
| sector property inputs | floor/ceiling/ambient/sector UV/decal inputs | inspector state / material UI state | `SectorEditorUiState` | `InspectorUiState` / `MaterialEditingUiState` | None | Yes | Split later | Large contributor to panel coupling. |
| light inputs | light position/target/intensity/radius/cones/source/flicker/shadow/color inputs | light UI state | `SectorEditorUiState` | `LightEditingUiState` | None | Yes | Split after light state split | Light service resets these but does not own them. |
| runtime object inputs | runtime object transform/dimensions/door/billboard inputs | runtime object UI state | `SectorEditorUiState` | `RuntimeObjectEditingUiState` | None | Yes | Defer | Tool modules already own much behavior but not the input state. |
| surface 3D material inputs | surface UV/decal inputs | preview panel UI state / material UI state | `SectorEditorUiState` | `PreviewMaterialPanelState` | None | Yes | Split later | Preview UV panel uses central inputs. |
| sidedef UV/decal inputs | sidedef material inputs | material UI state | `SectorEditorUiState` | `MaterialEditingUiState` | None | Yes | Split later | Material service resets them via context. |
| `toolsScroll`, `inspectorScroll` | `UIScrollState` | scroll state | `SectorEditorUiState` | panel state | None | Yes | Defer | Reasonable central UI state until panels own state. |
| selected ID buffers | char buffers and indices | inspector state | `SectorEditorUiState` | `InspectorUiState` | None | Yes | Split later | Sector/light ID editing still central. |
| `idEditError` | string | inspector state | `SectorEditorUiState` | `InspectorUiState` | None | Yes | Split later | Shared by sector/light ID editing. |
| `keyboardCaptured` | bool | UI transient state | `SectorEditorUiState` | top-level UI/input coordinator | None | Yes | Keep | Cross-cutting input routing state. |

Central UI state is still practical for `config`, top-level scroll/capture, and short-lived modal focus behavior. The highest-value splits are feature input groups: light inputs, material/surface inputs, inspector ID buffers, and runtime object inputs.

## Service Ownership Matrix

| service/module | classification | owns behavior? | owns state? | uses `SectorEditorState`? | uses `SectorEditorUiState`? | has independent state object? | could be reused outside editor? | should gain own state later? | notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `TexturePickerService` | backend helper module | Yes | No | No | No | No | Yes, with `TexturePickerState` passed in | Maybe | Generic lifecycle/result helpers over caller-owned `TexturePickerState`. |
| `TextureCatalogService` | behavior facade over `SectorEditorState` | Yes | No | Yes | No | No | Not yet | Yes | Catalog behavior is clean, but catalog/cache state remains central. |
| `MaterialEditingService` | behavior facade over `SectorEditorState` | Yes | No | Yes | Yes | No | Not yet | Yes | Owns material behavior and authoring writes, but state/UI buffers are central. |
| `MaterialPickerRouting` | backend helper module | Yes | No | Yes | No | No | Not generally | No | Correctly material-specific; still writes through central state. |
| `LightEditingService` | behavior facade over `SectorEditorState` | Yes | No | Yes | Yes | No | Not yet | Yes | Good behavior owner, but light state stays topology/central. Source-hash behavior matters for static/directional changes. |
| `LightmapBakeController` | real state-owning service | Yes | Yes | No | No | Yes: private `LightmapBakeAsyncState` | Yes, mostly | Already owns | Strongest state-ownership split; `SectorEditor` still installs result into map/assets. |
| `SelectionService` | behavior facade over `SectorEditorState` | Yes | No | Yes | Yes | No | Not yet | Yes | Central selection fields remain in `SectorEditorState`. Has one preview restore callback bridge. |
| `ManipulationService` | coordinator/orchestration module | Partial | No | Yes | Yes | No | Not yet | Yes | Many callbacks delegate drag lifecycle back to `SectorEditor`. |
| Document actions/modals | backend/UI modules | Yes | Mostly caller-owned modal state | Yes via save/load helpers | Some | No | Partly | Maybe | Save/load modals are extracted, lifecycle remains central. |
| Preview overlay/panel modules | UI modules | Yes | No | Yes | Yes | No | Not yet | Maybe | They return requests or call services, but read/write central preview/UI state. |
| Inspector panel module | UI module | Yes | No | Yes | Yes | No | Not runtime | Yes for UI state | Large extracted panel still uses central state/service contexts. |
| Placed object / door / billboard modules | UI/backend helper modules | Yes | No | Yes through contexts | Yes for inspectors | No | Partly | Maybe | Runtime object map data may be reusable; editor drag/modal/UI state is not. |

## State Split Candidates

| candidate | should exist? | current fields that would move | risk | expected benefit | runtime/game-mode relevance | suggested REF item |
| --- | --- | --- | --- | --- | --- | --- |
| `TextureCatalogState` | Yes | `editorTextureScope`, `editorTextureHandlesById`, maybe catalog refresh/default texture cache helpers | Low/Medium | Makes `TextureCatalogService` a real state-owning service without taking over add-map apply semantics | Medium | REF-084 |
| `MaterialEditingState` | Yes, but after texture state | `copiedTopologyMaterial`, material picker target context if separated, material UI reset/input groups from `SectorEditorUiState` | Medium | Reduces material-service dependence on central state/UI | Medium | REF-085 |
| `LightEditingState` | Yes | `lightDrag`, `lightEditing`, maybe light selection helper state; UI light input group later | Medium/High | Makes light service a real owner and reduces central wrappers | Medium | REF-086 after source-hash audit |
| `LightmapBakeState` | Already exists | private `LightmapBakeAsyncState` in controller | Low | Already achieved; keep install/orchestration central | High for baked data | None |
| `SelectionState` | Yes | selected/hovered topology and authoring IDs, `selectedSurface3D`, `selectedTopologySurface3D` | Medium | Reduces global selection coupling and wrapper functions | Low/Medium | REF-087 |
| `ManipulationState` | Maybe | `selectDragArm`, authoring/light/runtime drag states | Medium | Helps remove callback bridges in manipulation service | Low | Pair with `SelectionState` or defer |
| `PreviewState` | Yes, but not first | preview control mode, controllers, collision world, runtime object state, pose/effects, overlay tab/UI hidden | High | Future game-mode/runtime code can use preview/runtime state without editor document/UI | High | Runner plan or audit first |
| `InspectorUiState` | Yes | ID buffers/errors, inspector scroll, sector/material/light/runtime object input groups | Medium | Makes extracted inspector less dependent on monolithic UI state | None | After selection/material/light splits |
| `ModalState` | Maybe | save/load/confirm/decal/door/preview modals | Low/Medium | Cleaner modal routing, but limited runtime benefit | None | Defer |
| `DocumentState` | Yes, but broad | authoring graph, derived topology, dirty/path/status, last-valid derivation | High | Strong source-of-truth boundary and runtime/export clarity | High | Plan first |
| `RuntimeObjectEditingState` | Maybe | selected runtime object, drag state, sprite picker metadata, door modal state, runtime object UI inputs | Medium | Separates placed-object authoring from general editor state | High for runtime object data | Defer until preview/runtime split |

## Remaining SectorEditor.cpp Responsibility Inventory

| function/cluster | approximate line range | responsibility | state touched | existing service/module that could own it | keep in `SectorEditor` / move later / defer | risk | expected line-count reduction | notes |
| --- | --- | --- | --- | --- | --- | --- | ---: | --- |
| Init / Shutdown | 179-214 | Asset/runtime/lightmap cleanup lifecycle | preview, runtime objects, texture scopes, modal preview scopes | none | Keep | Low | 0-30 | Composition/lifecycle state belongs at top level. |
| Update / Render / RenderUI | 215-430 | top-level frame, mode, UI/modal routing | broad | none | Keep | Medium | 0-80 | Coordinator glue; could shrink only after modal/preview state splits. |
| tool context/routing | 446-522, 734-1027 | builds tool callbacks and routes canvas input | tool, selection, manipulation, document | tools + manipulation service | Move later | Medium | 200-350 | Still callback-heavy, especially manipulation bridge. |
| hover/picking helpers | 621-733, 4530-4909 | 2D pick/hit tests and hover state | topology, authoring, selection | selection/picking service | Move later | Medium | 300-500 | Read-only topology use is acceptable, but central state coupling is high. |
| authoring vertex drag | 1157-1249 | drag lifecycle for authoring vertices | authoring graph, drag state | manipulation service/tool state | Move later | Medium | 80-120 | Current manipulation service delegates here. |
| light drag/pilot wrappers | 1252-1381, 4270-4370 | light drag and spot pilot orchestration | light state, preview pose, renderer | light service + preview coordinator | Move later | Medium/High | 180-250 | Source-hash and preview pose restore semantics matter. |
| runtime object drag/actions wrappers | 1384-1405, 2236-2349 | placed object drag/action context and wrappers | runtime objects, cache, selection | placed object modules | Defer | Medium | 80-150 | Mostly callback/context glue now. |
| preview update/lifecycle/collision | 1408-1585, 4153-4483, 5122-5179 | freefly/gameplay update, preview enter/leave/rebuild/settings | preview/runtime/collision/effects | future `PreviewState`/controller | Move later after plan | High | 500-800 | Important runtime reuse target. Collision/physics behavior should not change during extraction. |
| pending authoring line/rectangle/insert | 1587-1780 | cancel/begin/update insert vertex and pending tool state | tool state, authoring graph | tool modules | Move later | Medium | 120-200 | Tools already extracted but state remains central. |
| selection wrapper methods | 1911-2081, 4917-5060 | bridge to selection service | selection/UI | selection service with state object | Move later | Low/Medium | 180-260 | Many functions only forward to service. |
| document lifecycle | 3915-4143 | reset/load/save/modal open/list | document state, runtime cleanup, texture catalog | document controller | Move later | Medium | 250-350 | Worth doing after `DocumentState` plan. |
| lightmap bake wrapper/use | 2353-2462, 3819-3834 | start/poll/install/modal callbacks | topology map, controller, assets | bake controller plus document install helper | Keep/defer | Medium | 40-90 | Controller owns async state; editor should install into current document. Source-hash behavior unchanged in this audit. |
| 2D render/cache/draw overlays | 2725-3208 | grid/cache draw/selection/drag overlays | render cache, selection, view | 2D view renderer | Defer | Medium | 300-450 | Cache invalidation remains explicit; no steady expensive rebuild found in this audit path. |
| main tools/toolbar panel | 3211-3535 | tool buttons, document buttons, preview/lightmap/settings toggles | broad | toolbar module | Defer | Low/Medium | 250-320 | Mostly UI coordinator, not a top runtime blocker. |
| inspector routing | 3538-3589 | compose inspector contexts and replay requests | services, state, ui | already `inspector/` | Keep | Low | 20-50 | Acceptable central composition. |
| modal draw wrappers | 3591-3835 | call extracted modal UIs with callbacks | modal states, services | modal controller/feature modules | Defer | Low/Medium | 150-250 | Many are now thin wrappers. |
| texture picker/add-map/sky/door/sprite routing | 4486-4527, 5184-5302 | service construction and feature picker apply semantics | texture picker, catalog, runtime object, material service | texture/material/feature services | Move selected pieces | Medium | 150-250 | `TextureCatalogState` split is the cleanest next step. |

## Acceptable Coordinator Glue

These should stay in `SectorEditor` for now:

- App/editor lifecycle and engine context integration.
- Top-level update/render/render-UI sequencing.
- Service composition and short-lived service construction.
- Mode/tool routing at the highest level.
- Document lifecycle orchestration until a `DocumentState`/controller split is planned.
- Preview enter/leave orchestration and renderer resource rebuild requests.
- Confirmation/modal orchestration for cross-feature actions.
- High-level request replay from extracted panels.
- Asset manager and runtime world integration.

## Remaining Problem Areas

- `TextureCatalogService`, `MaterialEditingService`, `LightEditingService`, `SelectionService`, and `ManipulationService` are mostly behavior owners over caller-owned `SectorEditorState`, not real state-owning services.
- `ManipulationService` still has a broad callback bridge back into `SectorEditor` for picking and drag lifecycle.
- `SelectionService` still has a preview restore callback for spotlight pilot cancellation.
- Direct topology writes remain intentionally allowed for texture registry, lights, runtime objects, bake metadata, preview settings, and derivation output. Normal editable geometry/material routes should continue writing authoring graph data or fail clearly.
- Topology-owned editor/runtime data that may need later authoring/runtime separation: lights, runtime objects, map texture registry, preview settings, sky visual settings, directional light, lightmap settings, and baked metadata.
- Several `SectorEditor` functions are thin wrappers around services, especially selection and material/light service builders.
- Future runtime/game-mode reuse is blocked mainly by preview/runtime controller state, runtime object world state, collision state, and compiled topology being colocated with editor-only UI/modal/document state.

## Recommended Next Tasks

### Implement Next

1. REF-084: split `TextureCatalogState` out of `SectorEditorState`.
   Move editor texture scope and texture handle cache into a narrow state object owned or referenced by `TextureCatalogService`. Keep `SectorTopologyMap::texturesById` as map-level texture registry for now. Do not move add-map modal state, add-map preview scope, door/sky/material/sprite apply semantics, or document dirty/status orchestration into the catalog service.
2. REF-085: split material editing state/UI inputs only after REF-084.
   Move copied material and material-specific UI input reset groups behind `MaterialEditingService` or a `MaterialEditingUiState` without changing authoring-owned material writes.

### Plan First

- Plan a `PreviewState` / preview runtime controller split before implementation. This touches renderer resource lifetime, ECS runtime objects, collision, gameplay controller, visual camera effects, object probes, and preview settings.
- Plan a `DocumentState` split before moving load/save/derivation ownership. It must preserve the architecture contract that authoring graph is the editable source of truth and topology is derived output.

### Defer

- Broad modal-controller extraction.
- Main tools/toolbar extraction unless line-count cleanup becomes the primary goal.
- Full `SelectionState` and `ManipulationState` split until the callback bridge cleanup is scoped.
- Runtime object editing state split until preview/runtime reuse work starts.

## Backlog Update

Added and completed REF-083 in `docs/plans/codebase_refactor_backlog.md` with completion notes covering this report path, current `SectorEditor.cpp` line count, god-state status, top split candidates, top remaining clusters, and recommended next implementation task.

## Appendix: Evidence

Commands used:

```text
sed -n '1,240p' docs/architecture/sector_editor_architectural_principles.md
sed -n '1,240p' docs/audit/sector_editor_direct_topology_edit_inventory.md
sed -n '1,260p' docs/plans/codebase_refactor_backlog.md
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditor.h sources/sector_editor/*.h sources/sector_editor/*.cpp
find sources/sector_editor -maxdepth 4 -type f | sort | xargs -r wc -l | sort -n
find sources/sector_editor/services -maxdepth 3 -type f | sort | xargs -r wc -l
rg -n "struct SectorEditorState|struct SectorEditorUiState|struct .*State|class .*Service|struct .*ServiceContext|class .*Controller" sources/sector_editor
rg -n "SectorEditorState&|SectorEditorUiState&|SectorEditorState\*|SectorEditorUiState\*" sources/sector_editor/services sources/sector_editor/tools sources/sector_editor/preview sources/sector_editor/inspector sources/sector_editor/selection
rg -n "^.*SectorEditor::" sources/sector_editor/SectorEditor.cpp
rg -n "TextureCatalog|TexturePicker|MaterialEditing|LightEditing|LightmapBake|SelectionService|ManipulationService|PreviewOverlay|PreviewUvPanel|InspectorPanel|Document|Modal|Confirm|Bake|RuntimeObject|Door|Billboard|Tool" sources/sector_editor/SectorEditor.cpp
rg -n "state\.|uiState\." sources/sector_editor/SectorEditor.cpp | head -n 300
rg -n "state\.|uiState\." sources/sector_editor/SectorEditor.cpp | tail -n 300
sed -n '1,280p' sources/sector_editor/SectorEditorTypes.h
sed -n '1,430p' sources/sector_editor/SectorEditor.h
sed -n '1,280p' sources/sector_demo/SectorAuthoringGraph.h
sed -n '1,260p' sources/sector_demo/SectorTopologyMap.h
```

Selected `wc -l` output:

```text
  5305 sources/sector_editor/SectorEditor.cpp
   366 sources/sector_editor/SectorEditor.h
  2404 sources/sector_editor/SectorEditorAuthoringState.cpp
  2068 sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp
  1590 sources/sector_editor/SectorEditorHelpers.cpp
  1331 sources/sector_editor/SectorEditorTopologyRenderCache.cpp
  1067 sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp
   918 sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp
   362 sources/sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.cpp
   192 sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.cpp
    57 sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp
```

Selected dependency observations:

```text
sources/sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h: SectorEditorTextureCatalogServiceContext { SectorEditorState& state; }
sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.h: context owns SectorEditorState&, SectorEditorUiState&, TexturePickerState&, status callback/rebuild callback.
sources/sector_editor/services/lights/SectorEditorLightEditingService.h: context owns SectorEditorState&, SectorEditorUiState&, status text.
sources/sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.h: class owns private LightmapBakeAsyncState state_.
sources/sector_editor/selection/SectorEditorManipulationService.h: context owns SectorEditorState&, SectorEditorUiState&, status text, and many callbacks into SectorEditor.
sources/sector_editor/selection/SectorEditorSelectionService.h: context owns SectorEditorState&, SectorEditorUiState&, optional status and spotlight preview restore callback.
```

Field inventories came from `SectorEditorTypes.h`, `SectorEditorSelectionTypes.h`, `SectorEditorModalTypes.h`, `SectorEditorPreviewTypes.h`, `SectorEditorLightmapAsyncTypes.h`, `SectorAuthoringGraph.h`, and `SectorTopologyMap.h`.

Cache invalidation note: this audit did not change topology mutation code. Existing cache invalidation ownership remains as documented; future topology mutations still need `MarkTopologyDocumentEdited()` / `InvalidateTopologyRenderCache()` or equivalent.

Lightmap source-hash note: this audit did not change lightmap code or source-hash behavior. Future light/directional/preview-setting work must continue distinguishing bake-affecting settings from visual-only settings.
