# REF-085 / REF-086 Implementation Review

## Summary Verdict

Overall status: mostly green.

| file | current lines |
| --- | ---: |
| `sources/sector_editor/SectorEditor.cpp` | 5,798 |
| `sources/sector_editor/SectorEditor.h` | 388 |
| `sources/sector_editor/SectorEditorTypes.h` | 163 |

REF-085 and REF-086 improved the architecture in meaningful ownership terms. The actual code moved preview/runtime state and document/source state out of `SectorEditorState`, kept the new roots split into responsibility-based sub-states, avoided broad manager/controller infrastructure, and preserved the key source-of-truth contracts. The result is not a finished editor architecture: `SectorEditor.cpp` is still the orchestration hub, inspector/tool/placed-object code still has broad state and callback debt, and the 2D topology render cache remains central. Those are real debts, but they are not evidence that REF-085/086 made the design worse.

Top 5 improvements:

- Preview-owned fields moved into `SectorEditorPreviewState::{overlay, selection, controller, collision, runtime}`.
- Document-owned fields moved into `SectorEditorDocumentState::{authoring, derivation, map, lifecycle}`.
- `SectorEditorState` no longer owns the authoring graph, topology map, derivation bookkeeping, document lifecycle, preview controller, preview collision, preview selection, or preview runtime world adapter.
- Preview modules/actions no longer take broad `SectorEditorState&` / `SectorEditorUiState&` and do not include `SectorEditor.h`.
- Renderer ownership stayed in `SectorEditor`; document source state stayed out of preview; preview runtime/controller/collision state stayed out of document.

Top 5 regressions or concerns:

- `SectorEditor.cpp` remains large and contains many thin wrapper/orchestration functions after the state splits.
- `SectorEditorState` still mixes 2D view/cache, tool transaction, modal, texture picker, sprite picker, runtime-object editing, and default material/sector state.
- Inspector and placed-object/tool contexts still depend on broad `SectorEditorState&` / `SectorEditorUiState&`.
- Existing callback/userData bridges remain in selection/manipulation, tools, picker/modal, material, and placed-object paths.
- `SectorEditorDocumentState.h` added many inline access-view helpers; acceptable for migration, but it is the main place to watch for document-state accretion.

Recommended next task: split runtime object editing state only: `runtimeObjectDrag`, sprite picker/catalog, billboard metadata repair fields, runtime object inspector UI inputs, and placed-object callback debt. Do not move `SectorTopologyMap::runtimeObjects` authored definitions in that task.

## Evidence Sources

Docs/plans inspected:

- `docs/architecture/sector_editor_architectural_principles.md`
- `docs/audit/sector_editor_state_ownership_and_remaining_map.md`
- `docs/plans/ref084_service_state_ownership_runner_plan.md`
- `docs/plans/ref085_preview_state_ownership_runner_plan.md`
- `docs/plans/ref086_document_state_ownership_runner_plan.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/runner_compatible_plans.md`

Runner logs inspected:

- `.agent-runs/in-place-ref085_preview_state_ownership-2026-07-07-121853/run_all_summary.txt`
- `.agent-runs/in-place-ref085_preview_state_ownership-2026-07-07-125514/run_all_summary.txt`
- `.agent-runs/in-place-ref086_document_state_ownership-2026-07-07-183409/run_all_summary.txt`
- `.agent-runs/in-place-ref086_document_state_ownership-2026-07-07-192604/run_all_summary.txt`

Source files inspected:

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/preview/SectorEditorPreviewState.h`
- `sources/sector_editor/preview/SectorEditorPreviewOverlay.*`
- `sources/sector_editor/preview/SectorEditorPreviewUvPanel.*`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/document/SectorEditorDocumentState.h`
- `sources/sector_editor/document/SectorEditorDocumentActions.*`
- `sources/sector_editor/SectorEditorAuthoringState.*`
- `sources/sector_editor/SectorEditorDirtyState.*`
- `sources/sector_editor/SectorEditorTopologyRenderCache.*`
- `sources/sector_editor/inspector/SectorEditorInspectorPanel.*`
- `sources/sector_editor/tools/**`
- `sources/sector_editor/selection/**`
- `sources/sector_editor/services/**`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorTopologySerialization.cpp`
- `sources/sector_demo/SectorLightmap.cpp`
- `sources/sector_demo/SectorRuntimeObjects.*`
- `sources/sector_demo/renderer/**`

Commands run are listed in the appendix, with the required validation commands at the end.

## Current State Object Inventory

| state object | file | owns what | category | serialized? | runtime/game-mode relevance | acceptable owner? | too broad? | notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `SectorEditorState` | `SectorEditorTypes.h` | 2D render cache, current tool/mode, 2D view, pending authoring tool state, runtime-object drag, defaults, grid flags, picker/modal/sprite state | editor-only / UI / modal / cache / tool | No direct serialization | Some fields affect preview entry/UI, not reusable runtime | Partly | Yes | Much smaller in source/document/preview terms, still broad. |
| `SectorEditorPreviewState` | `preview/SectorEditorPreviewState.h` | aggregate of preview sub-states | preview-runtime | No | Yes | Yes | No | Small root, not flat. |
| `SectorEditorPreviewOverlayState` | `preview/SectorEditorPreviewState.h` | baked AO preview toggle, object probe overlay toggle, preview UI hidden flag, debug tab | preview UI/overlay | No | Editor preview only | Yes | No | `useBakedAmbientOcclusion` remains preview-only, not source-hash data. |
| `SectorEditorPreviewSelectionState` | `preview/SectorEditorPreviewState.h` | 3D hovered/selected surface and material target | preview-runtime / UI selection | No | Preview surface interaction only | Yes | No | Kept separate from 2D `SelectionState`. |
| `SectorEditorPreviewControllerState` | `preview/SectorEditorPreviewState.h` | preview mode, freefly/FPS config/state, visual offsets, last pose, spotlight preview restore | preview-runtime / camera | Config hydrated from serialized `topologyMap.previewSettings`; state itself no | Yes | Yes | No | Visual effects remain controller state, not collision state. |
| `SectorEditorPreviewCollisionState` | `preview/SectorEditorPreviewState.h` | `SectorCollisionWorld`, validity/warning, current sector/result/fallback state | preview-runtime / collision | No | Yes | Yes | No | Uses topology collision world, not render triangles. |
| `SectorEditorPreviewRuntimeState` | `preview/SectorEditorPreviewState.h` | `SectorRuntimeObjectState runtimeObjects` | preview-runtime / world adapter | No | Yes | Yes | No | Distinct from authored `SectorTopologyMap::runtimeObjects`. |
| `SectorEditorDocumentState` | `document/SectorEditorDocumentState.h` | aggregate of document sub-states | document-source / derived / lifecycle | In-memory owner for serialized document data | Indirectly, via map data | Yes | Watch | Root has access helpers but remains responsibility-split. |
| `SectorEditorAuthoringDocumentState` | `document/SectorEditorDocumentState.h` | `SectorAuthoringGraph authoringGraph` | document-source | Yes, through authoring document save/load | Editor source of truth | Yes | No | Editable source of truth preserved. |
| `SectorEditorDerivationState` | `document/SectorEditorDocumentState.h` | derivation result, last-valid topology, stale/current state, derivation status | derived / document bookkeeping | No direct serialization | Feeds preview/render/cache | Yes | No | Separated from map owner. |
| `SectorEditorDocumentMapState` | `document/SectorEditorDocumentState.h` | `SectorTopologyMap topologyMap` | derived output plus map metadata/runtime definitions | Yes | Yes, as compiled map input | Yes | Watch | Name avoids pretending map is only geometry; metadata remains documented topology-owned for now. |
| `SectorEditorDocumentLifecycleState` | `document/SectorEditorDocumentState.h` | initialized/dirty/status/current path/unsaved fields | document lifecycle | No, but path/status drive save/load | Editor only | Yes | No | Clean move out of `SectorEditorState`. |
| `SelectionState` | `selection/SectorEditorSelectionState.h` | selected/hovered 2D topology, authoring, runtime object, and inspector target IDs | editor-only selection | No | Editor only | Yes | Medium | Preview-surface selection was not folded into this. |
| `ManipulationState` | `selection/SectorEditorManipulationState.h` | select drag arm and authoring vertex drag | editor-only manipulation | No | Editor only | Yes | No | Runtime-object drag still outside. |
| `TextureCatalogState` | `services/texture_catalog/SectorEditorTextureCatalogState.h` | editor texture scope and handle cache | editor cache/resource | No | Preview/editor rendering assets | Yes | No | Map texture registry remains in document map. |
| `MaterialEditingState` | `services/material_edit/SectorEditorMaterialEditingState.h` | copied material payload | editor feature state | No | Editor only | Yes | No | Authoring writes still use material service. |
| `MaterialEditingUiState` | `services/material_edit/SectorEditorMaterialEditingState.h` | material UV/decal UI input buffers | UI | No | Editor only | Yes | Medium | Narrower than old `SectorEditorUiState`. |
| `LightEditingState` | `services/lights/SectorEditorLightEditingState.h` | light drag/edit transaction and light-owned spotlight pilot data | editor feature state | No | Preview spotlight pilot integration | Yes | Medium | Light object storage remains in topology map. |
| `InspectorIdUiState` | `inspector/SectorEditorInspectorUiState.h` | selected sector/light ID buffers and edit error | UI | No | Editor only | Yes | No | Narrowed from `SectorEditorUiState`. |
| `SectorEditorUiState` | `SectorEditorTypes.h` | remaining UI config, grid/sector/light/runtime-object inputs, scroll, keyboard capture | UI | No | Editor only | Partly | Yes | Still broad; no longer owns material ID buffers. |
| `SectorEditorLightmapBakeController` private state | `services/lightmap_bake/SectorEditorLightmapBakeController.*` | async bake worker/result/progress/cancel state | controller/service private | No; installs results into map | Bake workflow only | Yes | No | Already isolated before REF-085/086. |

## SectorEditorState Remaining Fields

| field / group | category | why it remains | should remain? | expected future owner if not | risk | notes |
| --- | --- | --- | --- | --- | --- | --- |
| `topologyRenderWarning`, `topologyRenderRevision`, `topologyRenderCache` | 2D derived render cache | REF-086 intentionally deferred view/cache ownership | Maybe | `TopologyViewCacheState` / 2D view state | Medium | Invalidation still through `InvalidateTopologyRenderCache()` and mutation helpers. |
| `currentTool`, `mode` | coordinator routing | top-level editor routing | Yes for now | `EditorModeState` only if useful | Low | `mode` controls 2D/preview branch. |
| `viewCenter`, `viewZoom`, `gridSize`, mouse map points | 2D view state | not document or preview runtime | No long term | `Editor2DViewState` | Low/Medium | Good candidate after runtime-object editing. |
| pending authoring line/rectangle/insert | tool transaction state | not document source itself | No | tool-specific state | Medium | Still mixed into main state. |
| `runtimeObjectDrag` | runtime object editing transaction | explicitly deferred from REF-085 | No | `RuntimeObjectEditingState` | High | Best next split. |
| default sector heights/textures | authoring defaults | not moved by document plan | Maybe | authoring defaults/material defaults state | Low/Medium | Do not fold into document without a scoped task. |
| `showGrid`, `showAxes`, `showSectorIds` | 2D view toggles | editor view state | No long term | `Editor2DViewState` | Low | Not source data. |
| `texturePicker`, `addMapTexture` | modal/picker | picker lifecycle intentionally separate | No long term | picker/modal state | Medium | Feature apply semantics remain central. |
| sprite metadata/catalog, billboard repair flags, `spritePicker` | runtime object / sprite editing | deferred runtime-object editing work | No | `RuntimeObjectEditingState` / sprite picker state | High | Should move with runtime-object editing. |
| save/load/confirmation/decal/door/preview modals | modal state | document/preview state should not absorb modals | No long term | feature/modal state | Medium | Existing callback modal pattern remains. |

Preview candidates checked: all listed preview candidates moved out of `SectorEditorState` into `SectorEditorPreviewState` except modal/UI or editing state intentionally out of scope (`previewSettingsModal`, object-probe distance UI input, runtime-object editing/sprite state). Document candidates checked: `topologyMap`, `authoringGraph`, derivation fields, lifecycle fields moved into `SectorEditorDocumentState`; topology render-cache fields remain in `SectorEditorState` by design.

## PreviewState Implementation Review

REF-085 expected preview sub-states and the code has exactly these practical groups:

- `SectorEditorPreviewOverlayState`
- `SectorEditorPreviewSelectionState`
- `SectorEditorPreviewControllerState`
- `SectorEditorPreviewCollisionState`
- `SectorEditorPreviewRuntimeState`
- root `SectorEditorPreviewState`

Fields moved: overlay flags, 3D preview selection, freefly/FPS/controller/visual pose state, collision world/result/current-sector state, and transient `SectorRuntimeObjectState runtimeObjects`.

Fields stayed: renderer ownership (`SectorMeshRenderer preview`) stayed in `SectorEditor`; preview settings modal stayed in `SectorEditorState`; authored runtime object definitions stayed in `SectorTopologyMap::runtimeObjects`; runtime-object editing state stayed in `SectorEditorState`.

The sub-states are responsibility-based. A flat preview god object did not appear. `SectorEditorPreviewState` is a small aggregate, while real fields are grouped. Preview helpers avoided `SectorEditor.h`; grep found no `SectorEditor::` calls in preview/document/tools/inspector/selection/services. Preview modules no longer take broad `SectorEditorState&` or `SectorEditorUiState&`.

| REF-085 expected field/group | actual owner | matches plan? | concern | notes |
| --- | --- | --- | --- | --- |
| `previewUiHidden`, `activePreviewDebugOverlayTab`, `showObjectProbeDebugOverlay`, `useBakedAmbientOcclusion` | `previewState.overlay` | Yes | Low | Preview-only overlay/render state. |
| `hoveredSurface3D`, `selectedSurface3D`, `selectedTopologySurface3D` | `previewState.selection` | Yes | Low | Still passed to material/selection services where needed. |
| `previewControlMode`, freefly/FPS state/config | `previewState.controller` | Yes | Low | Serialized settings remain map data; runtime config is hydrated. |
| `visualStepOffsetY`, `headBobState`, `landingDipState` | `previewState.controller` | Yes | Low | Visual-only offsets still separate from collision state. |
| `hasPreviewPose`, `lastPreviewPose`, spotlight preview restore | `previewState.controller` | Yes | Low | Light-owned pilot data remains in `LightEditingState`. |
| collision world/valid/warning/results/current sector/fallback | `previewState.collision` | Yes | Low | `RebuildSectorEditorCollisionWorld(TopologyMap(), previewState.collision, previewState.controller)`. |
| `SectorRuntimeObjectState runtimeObjects` | `previewState.runtime` | Yes | Medium | Correct for runtime adapter; editing state still central. |
| `SectorMeshRenderer preview` | `SectorEditor` member | Yes | Low | GPU lifetime not moved. |
| document/source-of-truth state | `SectorEditorDocumentState` / map | Yes | Low | Preview contexts receive explicit references only. |

Specific checks:

- Renderer ownership and GPU lifetime: pass. `preview` is still a `SectorEditor` member; enter/rebuild/shutdown paths call renderer methods from top-level orchestration.
- Preview enter/rebuild/leave behavior: pass by code structure. `TryEnterPreview3D()`, `LeavePreview3D()`, and `RebuildPreviewMeshesPreservingView()` still orchestrate renderer/collision/runtime resources.
- Preview3D surface selection/material panel behavior: pass by ownership. Selection state moved to preview selection, UV panel receives explicit preview selection/material service dependencies.
- Freefly/gameplay controller state: pass. State moved to controller sub-state.
- Visual camera effects separation: pass. Visual offset/headbob/landing dip are controller fields; collision movement uses FPS feet/current-sector and collision state.
- Collision world/move/vertical/fallback state: pass. State moved to collision sub-state.
- Runtime object preview/world adapter state: pass. `SectorRuntimeObjectState` moved to preview runtime; authored definitions did not.
- Object probe debug state: mostly pass. Probe runtime data is in preview runtime; the UI distance input remains in `SectorEditorUiState`, which is appropriate UI state.
- Baked AO preview toggle ownership: pass. It is preview overlay state and not in `ComputeSectorLightmapSourceHash()`.
- Spotlight pilot preview restore behavior: pass. Preview pose/mouse-look restore moved with preview controller; light-owned pilot data remains in `LightEditingState`.

## DocumentState Implementation Review

REF-086 expected document sub-states and the code has these groups:

- `SectorEditorAuthoringDocumentState`
- `SectorEditorDerivationState`
- `SectorEditorDocumentMapState`
- `SectorEditorDocumentLifecycleState`
- root `SectorEditorDocumentState`

Fields moved: `authoringGraph`, `topologyMap`, `authoringDerivation`, `lastValidAuthoringDerivedTopology`, derivation stale/current/status fields, and document initialized/dirty/status/path/unsaved fields.

Fields stayed: topology render-cache fields remained in `SectorEditorState`; modal UI, inspector UI buffers, tool transactions, texture picker modal state, renderer ownership, runtime-object editing state, and preview state stayed outside `DocumentState`.

The sub-states are responsibility-based. A flat document god object did not appear, but `SectorEditorDocumentState.h` is now a header to watch: it owns real state plus several inline access-view helpers. That is acceptable for the migration, not a new generic manager.

| REF-086 expected field/group | actual owner | matches plan? | concern | notes |
| --- | --- | --- | --- | --- |
| `authoringGraph` | `documentState.authoring.authoringGraph` | Yes | Low | Editable source of truth preserved. |
| `topologyMap` | `documentState.map.topologyMap` | Yes | Medium | Correctly classified as derived output plus documented metadata/runtime definitions. |
| `authoringDerivation` | `documentState.derivation.authoringDerivation` | Yes | Low | Separate from map owner. |
| `lastValidAuthoringDerivedTopology` | `documentState.derivation.lastValidAuthoringDerivedTopology` | Yes | Low | Last-valid behavior retained. |
| derivation state/stale/status | `documentState.derivation` | Yes | Low | Explicit lifecycle around derived output. |
| initialized/dirty/status/path/unsaved | `documentState.lifecycle` | Yes | Low | Save/load path state moved. |
| topology render cache fields | `SectorEditorState` | Yes, deferred | Medium | Correctly not absorbed as document source state. |
| modal/inspector/tool/picker/runtime editing state | outside `DocumentState` | Yes | Medium | Still debt elsewhere, not document pollution. |
| preview/controller/collision/runtime state | `SectorEditorPreviewState` | Yes | Low | No preview state absorbed. |

Specific checks:

- Authoring graph ownership: pass. The graph lives in `SectorEditorAuthoringDocumentState`.
- Topology map ownership/classification: pass with caveat. `SectorEditorDocumentMapState` owns `SectorTopologyMap`, which still contains texture registry, lights, runtime object definitions, preview settings, sky, directional light, lightmap settings, and baked metadata.
- Derivation bookkeeping: pass. Derivation and last-valid topology are grouped separately.
- Last-valid topology behavior: pass by ownership and load/refresh paths inspected.
- Document lifecycle dirty/path/status: pass. `Lifecycle()` returns document lifecycle access.
- Load/save/reset/import/migration retargeting: pass by code inspection. Legacy topology import still converts into authoring graph through `InitializeSectorEditorAuthoringStateFromTopology()`.
- Topology render-cache ownership/invalidation: pass with deferred debt. Cache fields remain central; invalidation still increments revision and marks invalid.
- Texture registry/lights/runtime objects/sky/directional/lightmap metadata classification: pass. Still topology-map-owned metadata/runtime definitions for now, not reclassified as authoring graph.
- Serialization/schema/save JSON behavior: pass by code inspection. Serialization remains in `SectorTopologySerialization.cpp`; save helpers still write authoring document with explicit graph/map/derivation refs.
- Lightmap source-hash behavior: pass by code inspection. `ComputeSectorLightmapSourceHash()` remains in `SectorLightmap.cpp` and includes lightmap settings, directional light, coordinates, referenced textures, and topology; preview/sky visual settings are not added by these refactors.

## Dependency Review

Remaining broad `SectorEditorState` / `SectorEditorUiState` dependencies in requested areas:

| area | remaining broad dependency evidence | assessment |
| --- | --- | --- |
| `preview/` | none from grep | Improved; REF-085 target met. |
| `document/` | none from grep | Improved; document state is explicit. |
| `tools/` | `SectorEditorToolModule.h`, placed-object action/inspector contexts | Existing tool/runtime-object debt. |
| `inspector/` | `SectorEditorInspectorPanel.*` | Inspector still broad. |
| `selection/` | none for `SectorEditorState&`/`SectorEditorUiState&`; callbacks/userData remain in manipulation/selection service | State narrowed, callback debt remains. |
| `services/` | none from grep | Improved. |

Remaining `SectorEditorUiState` dependencies:

| area | evidence | assessment |
| --- | --- | --- |
| `preview/` | none broad; preview overlay receives specific `objectProbeDebugDrawMaxDistanceInput` | Good. |
| `document/` | none broad | Good. |
| `tools/` | placed-object inspector context | Runtime-object UI debt. |
| `inspector/` | `SectorEditorInspectorPanel.*` | Broad inspector UI debt. |
| `selection/` | none broad | Good. |
| `services/` | none broad | Good. |

Includes of `SectorEditor.h` outside `SectorEditor.cpp`/`SectorEditor.h`: none.

Services/helpers calling `SectorEditor::` methods: none in preview/document/tools/inspector/selection/services grep.

Callback bridges/userData:

- Existing `SectorEditorManipulationServiceContext` uses `void* userData` and function pointers for editor drag/pick lifecycle.
- `SectorEditorSelectionServiceContext` has `requestCancelSpotLightPilotWithPreviewRestore` plus `userData`.
- Tool, material, texture picker, document modal, door, billboard, placed-object modules still use `std::function` callback bundles.
- These were not introduced as broad new REF-085/086 infrastructure, but they remain architecture debt.

Generic managers/controllers/framework infrastructure:

- No matches for `DocumentManager`, `PreviewManager`, `GameModeManager`, `SceneManager`, `WorldManager`, `TopologyService`, `EditorStateService`, `ServiceLocator`, `EventBus`, or `CommandBus`.
- No `SectorEditorDocumentController` or `SectorEditorPreviewController` was created.

## Behavior Contract Review

| contract | status | evidence |
| --- | --- | --- |
| authoring graph remains editable source of truth | pass | `SectorEditorAuthoringDocumentState::authoringGraph`; authoring mutation helpers still operate on graph references. |
| no no-authoring topology edit fallback returned | pass | grep only found `LegacyTopology` inspector target naming, not fallback edit routes. |
| `SectorTopologyMap` remains derived output plus documented map metadata/runtime definitions | pass | `SectorEditorDocumentMapState::topologyMap`; map still owns texture registry/lights/runtime objects/settings metadata. |
| JSON serialization/schema unchanged | pass | serialization code remains in `SectorTopologySerialization.cpp`; no schema framework added. |
| save/load/import/migration behavior unchanged | pass | load of topology-v2 path imports to authoring graph; authoring document save still uses graph/map/derivation. |
| topology render-cache invalidation behavior unchanged | pass | `InvalidateTopologyRenderCache()` still increments revision and marks cache invalid; mutation helpers still use cache invalidation paths. |
| no expensive steady-frame render-cache rebuild added | pass | `DrawTopologyDocument()` calls `EnsureTopologyRenderCache()`, which rebuilds only if invalid or revision mismatch. |
| lightmap source-hash behavior unchanged | pass | `ComputeSectorLightmapSourceHash()` unchanged in ownership terms; preview state not included. |
| preview renderer resource lifetime unchanged | pass | `SectorMeshRenderer preview` still owned by `SectorEditor`; reset/load/enter/rebuild call renderer lifecycle methods. |
| preview enter/rebuild/leave behavior unchanged | pass | orchestration remains in `SectorEditor`. |
| collision behavior unchanged | pass | collision world state moved only; still topology-based. |
| sector lookup behavior unchanged | pass | current-sector and lookup state moved only; no render triangle collision. |
| physics behavior unchanged | pass | FPS state/collision calls preserved by code structure. |
| camera behavior unchanged | pass | freefly/FPS pose state moved; renderer pose orchestration remains top-level. |
| visual offsets do not feed collision/sector lookup/physics | pass | visual step/headbob/landing dip live in controller state and are applied through preview pose helpers. |
| dirty/status/path/unsaved behavior unchanged | pass | lifecycle access now points to document lifecycle state. |
| texture registry/global metadata behavior unchanged | pass | `SectorTopologyMap::texturesById` remains map metadata; texture catalog cache is separate. |
| runtime object map definitions not confused with preview runtime object state | pass | authored `TopologyMap().runtimeObjects` and preview `SectorRuntimeObjectState` are distinct. |
| modal/UI/input state did not migrate into runtime/document state incorrectly | pass | preview settings modal, picker modals, inspector UI remain outside document/preview runtime roots. |

## SectorEditor.cpp Responsibility Map After REF-085/086

| cluster/function group | approximate line range | responsibility | state touched | should remain in `SectorEditor`? | move later? | risk | notes |
| --- | ---: | --- | --- | --- | --- | --- | --- |
| Init / Shutdown / frame update/render | 277-533 | lifecycle, update/render routing, modal and preview branch | broad | Yes | No immediate | Medium | Top-level composition remains valid. |
| Tool context/routing/canvas input | 548-1199 | builds tool context, hover/input routing | state, document, selection, manipulation | Partly | Yes | Medium | Tool callback context remains broad. |
| Authoring/light/runtime drag wrappers | 1327-1581 | drag lifecycle wrappers | manipulation, light, runtime object, document | Partly | Yes | High | Runtime object editing split should target this. |
| Preview update/selection | 1587-1769 | preview movement, controller, collision, selection | preview, renderer, document map | Yes | Maybe later | High | Correct owner for now because renderer lifecycle remains here. |
| Pending authoring tool state | 1773-2007 | pending line/rectangle/insert state | `SectorEditorState`, document | No long term | Yes | Medium | Tool-state cleanup. |
| Service context composition | 2008-2123 | manipulation/selection service contexts | many narrow states plus callbacks | Partly | Yes | Medium | UserData callback debt remains. |
| Selection wrappers | 2124-2487, 5293-5503 | selected object lookup/selection clear/sync | selection, document, preview selection | Partly | Yes | Medium | Mostly glue after state split. |
| Runtime object editing/actions | 2499-2618, 5107-5125, 5685-5759 | placed object add/delete/mutate/picker routing | topology map runtime defs, preview runtime, modal/UI | No long term | Yes | High | Recommended next split. |
| Lightmap bake install | 2620-2728, 4136-4153 | bake start/poll/install | document map, bake controller, assets | Yes | Maybe | High | Source-hash sensitive; keep cautious. |
| Preview render wrappers | 2797-3027 | renderer draw, overlay/panel contexts | preview, renderer, document refs | Yes | Maybe | Medium | Preview modules narrowed but wrapper remains. |
| 2D render/cache/grid/overlays | 3028-3519, 4898-5289 | cache rebuild/draw, picking, overlays | cache, document, selection, view | Partly | Yes | Medium | Future 2D view/cache plan. |
| Tools/sector/status panels | 3520-3907, 4154-4231 | top-level UI panels/status | broad UI/state | Partly | Yes | Medium | Inspector/UI state still broad. |
| Modals | 3908-4135, 4375-4489, 4861-4889 | add texture, picker, sprite, save/load/confirm, door, preview settings | modal state, services, document | Partly | Yes | Medium | Feature-specific modal cleanup only. |
| Document lifecycle | 4232-4484 | reset/load/save/list | document, preview runtime, renderer, texture catalog | Yes | Maybe | High | Correct top-level orchestration. |
| Preview enter/leave/settings/collision | 4495-4825, 5591-5654 | preview enter, leave, settings, collision rebuild | preview, renderer, document map | Yes | Maybe | High | Renderer lifetime intentionally central. |
| Texture picker/add-map/sky/door/sprite routing | 4848-4889, 5655-5759 | texture catalog/picker apply semantics | document map, modals, material, runtime object | Partly | Yes | Medium | Avoid generic picker manager. |

## Line Count And Large File Snapshot

`wc -l`:

```text
  5798 sources/sector_editor/SectorEditor.cpp
   388 sources/sector_editor/SectorEditor.h
   163 sources/sector_editor/SectorEditorTypes.h
  6349 total
```

Top large files under `sources/sector_editor`:

```text
   550 sources/sector_editor/preview/SectorEditorPreviewUvPanel.cpp
   644 sources/sector_editor/tools/billboards/SectorEditorBillboardInspector.cpp
   754 sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp
   905 sources/sector_editor/selection/SectorEditorSelectionService.cpp
   924 sources/sector_editor/preview/SectorEditorPreviewOverlay.cpp
   927 sources/sector_editor/SectorEditorLightInspector.cpp
   962 sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp
   970 sources/sector_editor/SectorEditorMaterialActions.cpp
  1075 sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp
  1121 sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.cpp
  1331 sources/sector_editor/SectorEditorTopologyRenderCache.cpp
  1590 sources/sector_editor/SectorEditorHelpers.cpp
  2163 sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp
  2764 sources/sector_editor/SectorEditorAuthoringState.cpp
  5798 sources/sector_editor/SectorEditor.cpp
```

Prior audit snapshot in `docs/audit/sector_editor_state_ownership_and_remaining_map.md` reports the same current line counts after REF-086. Compared with older backlog notes where `SectorEditor.cpp` was above 10k lines, file size has improved substantially. REF-085/086 did not primarily reduce line count; they improved ownership and dependency direction.

## What Improved

- `SectorEditorState` shrank in meaningful ownership terms, not just by moving lines.
- Preview state is separated by overlay, selection, controller, collision, and runtime adapter responsibility.
- Document state is separated by authoring source, derivation, compiled/document map, and lifecycle responsibility.
- Preview renderer ownership and GPU lifetime stayed at top-level `SectorEditor`, avoiding an accidental renderer-owning preview god object.
- Authoring graph remains the editable source of truth; `SectorTopologyMap` remains derived output plus explicitly documented map metadata/runtime definitions.
- Preview/document/services dependencies on broad `SectorEditorState` / `SectorEditorUiState` were reduced.
- No broad manager/controller/framework infrastructure was invented.

## What Got Worse Or Riskier

- `SectorEditorDocumentState.h` now contains concrete state plus many inline access-view helpers; it should not keep absorbing behavior.
- More explicit context wiring exists around document/preview pieces; this is clearer but verbose.
- `SectorEditor.cpp` still has many wrappers that are thin after the state split.
- Existing callback bundles and userData bridges are still prominent in selection/manipulation/tools/placed-object paths.
- Runtime object editing is now the most confusing ownership boundary: authored definitions are in the document map, preview runtime instances are in preview state, and editing/modal/UI state remains central.

## Remaining Debt

### High Priority

- `sources/sector_editor/SectorEditorTypes.h`: split runtime object editing fields: `runtimeObjectDrag`, `spriteMetadataCatalog`, `billboardMetadataObjectId`, `billboardMetadataSpriteAnimationPath`, `billboardMetadataInitialRepairAttempted`, `spritePicker`.
- `sources/sector_editor/SectorEditorTypes.h` and `SectorEditorUiState`: move runtime object inspector inputs into a scoped runtime-object editing UI state.
- `sources/sector_editor/tools/placed_objects/*`: replace broad state/callback context with a narrow runtime-object editing context/service.
- `sources/sector_editor/selection/SectorEditorManipulationService.h`: reduce `void* userData` and drag lifecycle callback debt after runtime-object editing ownership is clearer.

### Medium Priority

- `sources/sector_editor/SectorEditor.cpp`: clean up wrapper clusters made thin by PreviewState/DocumentState, especially selection and document access wrappers.
- `sources/sector_editor/SectorEditorTypes.h`: split 2D view/cache state if a focused cache plan can preserve invalidation and picking behavior.
- `sources/sector_editor/inspector/SectorEditorInspectorPanel.*`: narrow broad `SectorEditorState&` / `SectorEditorUiState&` dependency.
- `sources/sector_editor/document/SectorEditorDocumentState.h`: keep access helpers from growing into a behavior/service layer.

### Low Priority

- Split remaining sector/light inspector UI input buffers if they block other work.
- Split save/load/confirmation/decal/door/preview modals by feature only when touched.
- Add a manual GUI smoke checklist for preview/document behavior after the current refactor series stabilizes.

## Recommended Next Task

Implement a scoped runtime object editing state split:

- Move only editor/runtime-object editing and UI state out of `SectorEditorState` / `SectorEditorUiState`.
- Keep authored `SectorTopologyMap::runtimeObjects` in `SectorEditorDocumentMapState`.
- Keep preview runtime `SectorRuntimeObjectState` in `SectorEditorPreviewRuntimeState`.
- Preserve cache invalidation through existing document-edited paths.
- Preserve collision, sector lookup, physics, camera, renderer lifetime, serialization, and lightmap source-hash behavior.

This is the highest-value next step because it addresses the biggest remaining ownership ambiguity without a broad refactor.

## Backlog Update

Added a completed REF-087 audit item to `docs/plans/codebase_refactor_backlog.md` and recorded the runtime-object editing split as the recommended follow-up debt.

## Appendix: Commands And Grep Evidence

Required and relevant commands run:

```text
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditor.h sources/sector_editor/SectorEditorTypes.h
find sources/sector_editor -maxdepth 4 -type f | sort | xargs -r wc -l | sort -n | tail -n 25
rg -n "struct SectorEditorState|struct SectorEditorPreviewState|struct .*Preview.*State|struct SectorEditorDocumentState|struct .*Document.*State|struct SectorEditorUiState" sources/sector_editor
rg -n "previewUiHidden|activePreviewDebugOverlayTab|showObjectProbeDebugOverlay|useBakedAmbientOcclusion|previewControlMode|freeflyController|fpsControllerConfig|fpsControllerState|sectorCollisionWorld|sectorCollisionWorldValid|sectorCollisionWorldWarning|previewCollisionSectorId|previewVerticalResult|previewMoveResult|previewCollisionNoclipFallback|visualStepOffsetY|headBobState|landingDipState|hasPreviewPose|lastPreviewPose|spotLightPilotPreviewRestore|hoveredSurface3D|selectedSurface3D|selectedTopologySurface3D|runtimeObjects" sources/sector_editor
rg -n "topologyMap|authoringGraph|authoringDerivation|lastValidAuthoringDerivedTopology|authoringDerivationState|authoringDerivedTopologyStale|authoringDerivationStatus|topologyDocumentInitialized|topologyDocumentDirty|topologyDocumentStatus|currentLevelName|currentLevelPath|hasCurrentLevelPath|hasUnsavedChanges|topologyRenderWarning|topologyRenderRevision|topologyRenderCache" sources/sector_editor
rg -n "SectorEditorState&|const SectorEditorState&|SectorEditorState\*|SectorEditorUiState&|const SectorEditorUiState&|SectorEditorUiState\*" sources/sector_editor/preview sources/sector_editor/document sources/sector_editor/tools sources/sector_editor/inspector sources/sector_editor/selection sources/sector_editor/services
rg -n "#include \"sector_editor/SectorEditor.h\"|#include <sector_editor/SectorEditor.h>" sources/sector_editor
rg -n "SectorEditor::" sources/sector_editor/preview sources/sector_editor/document sources/sector_editor/tools sources/sector_editor/inspector sources/sector_editor/selection sources/sector_editor/services
rg -n "std::function|callback|callbacks|userData|void\*" sources/sector_editor/preview sources/sector_editor/document sources/sector_editor/tools sources/sector_editor/inspector sources/sector_editor/selection sources/sector_editor/services
rg -n "DocumentManager|PreviewManager|GameModeManager|SceneManager|WorldManager|TopologyService|EditorStateService|ServiceLocator|EventBus|CommandBus" sources/sector_editor sources/sector_demo
rg -n "NoAuthoring|no-authoring|TopologyOnly|Legacy|Scratch|writeback" sources/sector_editor/services/material_edit sources/sector_editor/tools sources/sector_editor/SectorEditorAuthoringState.* sources/sector_editor/document
rg -n "ComputeSectorLightmapSourceHash|sourceHash|bakedLightmap|lightmapSettings|directionalLight|skySettings|previewSettings|texturesById|runtimeObjects" sources/sector_editor sources/sector_demo
rg -n "ValidateSectorTopologyMap\(|ExtractSectorTopologyLoops\(|BuildSectorTopologyIndexes\(|earcut" sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditorTopologyRenderCache.* sources/sector_editor/document sources/sector_editor/preview
```

Key grep outputs:

```text
sources/sector_editor/preview/SectorEditorPreviewState.h:15:struct SectorEditorPreviewOverlayState
sources/sector_editor/preview/SectorEditorPreviewState.h:22:struct SectorEditorPreviewSelectionState
sources/sector_editor/preview/SectorEditorPreviewState.h:28:struct SectorEditorPreviewControllerState
sources/sector_editor/preview/SectorEditorPreviewState.h:41:struct SectorEditorPreviewCollisionState
sources/sector_editor/preview/SectorEditorPreviewState.h:51:struct SectorEditorPreviewRuntimeState
sources/sector_editor/preview/SectorEditorPreviewState.h:55:struct SectorEditorPreviewState
sources/sector_editor/document/SectorEditorDocumentState.h:77:struct SectorEditorAuthoringDocumentState
sources/sector_editor/document/SectorEditorDocumentState.h:90:struct SectorEditorDocumentMapState
sources/sector_editor/document/SectorEditorDocumentState.h:94:struct SectorEditorDocumentLifecycleState
sources/sector_editor/document/SectorEditorDocumentState.h:104:struct SectorEditorDocumentState
```

Remaining broad state dependency grep:

```text
sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp:56:        SectorEditorState& state,
sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp:300:    SectorEditorUiState& uiState = context.uiState;
sources/sector_editor/inspector/SectorEditorInspectorPanel.h:52:    SectorEditorState& state;
sources/sector_editor/inspector/SectorEditorInspectorPanel.h:58:    SectorEditorUiState& uiState;
sources/sector_editor/tools/SectorEditorToolModule.h:25:    SectorEditorState& state;
sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.h:20:    SectorEditorState& state;
sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h:32:    SectorEditorState& state;
sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h:54:    SectorEditorUiState& uiState;
```

`SectorEditor.h` include grep:

```text
sources/sector_editor/SectorEditor.cpp:1:#include "sector_editor/SectorEditor.h"
```

`SectorEditor::` in preview/document/tools/inspector/selection/services: no matches.

Forbidden generic infrastructure grep: no matches.

No-authoring/topology fallback grep:

```text
sources/sector_editor/SectorEditorAuthoringState.h:240:    LegacyTopology
sources/sector_editor/SectorEditorAuthoringState.cpp:1991:        target.kind = SectorEditorInspectorTargetKind::LegacyTopology;
```

Render-cache expensive rebuild grep:

```text
sources/sector_editor/SectorEditorTopologyRenderCache.cpp:402:    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(map);
sources/sector_editor/SectorEditorTopologyRenderCache.cpp:403:    const auto issues = ValidateSectorTopologyMap(map);
sources/sector_editor/SectorEditorTopologyRenderCache.cpp:452:        if (!ExtractSectorTopologyLoops(map, indexes, sector.id, loops, &loopIssues)) {
sources/sector_editor/SectorEditorTopologyRenderCache.cpp:497:        const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
sources/sector_editor/SectorEditor.cpp:4917:    if (!ExtractSectorTopologyLoops(TopologyMap(), sector.id, loops, &loopIssues)) {
```

The expensive validation/earcut calls remain in cache build code, not steady draw. The `SectorEditor.cpp` loop extraction is in point-in-sector picking logic, not render-cache rebuild.

Validation results:

```text
git diff --check
passed

cmake --build cmake-build-debug -j2
[0/2] Re-checking globbed directories...
ninja: no work to do.

ctest --test-dir cmake-build-debug --output-on-failure
100% tests passed, 0 tests failed out of 16

git diff --stat
48 files changed, 4979 insertions(+), 2170 deletions(-)

git status --short
M docs/audit/sector_editor_state_ownership_and_remaining_map.md
M docs/plans/codebase_refactor_backlog.md
M docs/plans/ref086_document_state_ownership_runner_plan.md
M sources/sector_editor/SectorEditor.cpp
M sources/sector_editor/SectorEditor.h
M sources/sector_editor/SectorEditorAuthoringState.cpp
M sources/sector_editor/SectorEditorAuthoringState.h
M sources/sector_editor/SectorEditorDirtyState.cpp
M sources/sector_editor/SectorEditorDirtyState.h
M sources/sector_editor/SectorEditorSectorInspector.cpp
M sources/sector_editor/SectorEditorSectorInspector.h
M sources/sector_editor/SectorEditorTextureActions.cpp
M sources/sector_editor/SectorEditorTextureModals.h
M sources/sector_editor/SectorEditorTypes.h
M sources/sector_editor/SectorEditorVertexInspector.cpp
M sources/sector_editor/SectorEditorVertexInspector.h
M sources/sector_editor/document/SectorEditorDocumentActions.cpp
M sources/sector_editor/document/SectorEditorDocumentActions.h
M sources/sector_editor/inspector/SectorEditorInspectorPanel.cpp
M sources/sector_editor/inspector/SectorEditorInspectorPanel.h
M sources/sector_editor/selection/SectorEditorManipulationService.cpp
M sources/sector_editor/selection/SectorEditorManipulationService.h
M sources/sector_editor/selection/SectorEditorMoveContext.h
M sources/sector_editor/selection/SectorEditorSelectionState.h
M sources/sector_editor/services/lights/SectorEditorLightEditingService.cpp
M sources/sector_editor/services/lights/SectorEditorLightEditingService.h
M sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.cpp
M sources/sector_editor/services/material_edit/SectorEditorMaterialEditingService.h
M sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.cpp
M sources/sector_editor/services/material_edit/SectorEditorMaterialPickerRouting.h
M sources/sector_editor/tools/SectorEditorToolModule.h
M sources/sector_editor/tools/billboards/SectorEditorBillboardActions.cpp
M sources/sector_editor/tools/doors/SectorEditorDoorActions.cpp
M sources/sector_editor/tools/doors/SectorEditorDoorInspector.cpp
M sources/sector_editor/tools/insert_vertex/SectorEditorInsertVertexTool.cpp
M sources/sector_editor/tools/line/SectorEditorLineTool.cpp
M sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp
M sources/sector_editor/tools/materials/SectorEditorMaterialInspector.h
M sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.cpp
M sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.h
M sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.cpp
M sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.h
M sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h
M sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectMoveProvider.cpp
M sources/sector_editor/tools/rectangle/SectorEditorRectangleTool.cpp
M sources/sector_editor/tools/select/SectorEditorSelectTool.cpp
M tests/SectorAuthoringGraphTests.cpp
M tests/SectorEditorLightEditingServiceTests.cpp
?? docs/audit/ref085_ref086_implementation_review.md
?? sources/sector_editor/document/SectorEditorDocumentState.h
```
