# SectorEditor.cpp Seams Audit

## Summary

`sources/sector_editor/SectorEditor.cpp` is still the largest god-file risk in
the project at 13,313 lines. The recent campaigns helped: renderer ownership is
now under `sources/sector_demo/renderer/`, editor passive type clusters are
split out of `SectorEditorTypes.h`, and several editor responsibilities already
have action, modal, inspector, helper, and cache modules.

Top 5 remaining extraction opportunities:

1. Runtime object inspector/actions: billboard, door, sprite picker, door UV
   settings, and runtime object mutation/refresh are a coherent seam.
2. Authoring graph inspector material rows: a large UI block remains in
   `DrawSectorsPanel()` despite authoring backend helpers already existing.
3. Remaining modal wrappers/draw flows: save/load/confirmation/decal tint/door
   texture settings still live in `SectorEditor.cpp`; texture, sprite,
   preview-settings, and lightmap bake modals already prove the pattern.
4. Preview overlay/debug UI: renderer calls are behind `SectorMeshRenderer`,
   but editor-side overlay, probe debug UI, spotlight pilot, and preview UV
   panel are still large.
5. Light/static-light/object-probe editing: inspectors are partly extracted,
   but source-hash-sensitive settings and spotlight pilot/application remain
   mixed into editor orchestration.

Recommended first extraction: split runtime object inspector/actions, but split
REF-021 into a mini sequence instead of one broad task. Start with the runtime
object inspector body and small action wrappers that already route through
`MutateSelectedRuntimeObject()`.

Needs runner plans or deeper audits first: light/static-light/object-probe
hash-sensitive flows, preview overlay/debug UI if it tries to move more than
draw-only code, and a full document-state vs preview-state split.

## Scope And Method

Inspected files:

- `docs/audit/codebase_architecture_audit.md`
- `docs/audit/sector_editor_types_dependency_audit.md`
- `docs/plans/codebase_refactor_backlog.md`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditor*Actions.h/.cpp`
- `sources/sector_editor/SectorEditor*Inspector.h/.cpp`
- `sources/sector_editor/SectorEditor*Modal.h/.cpp`
- `sources/sector_editor/SectorEditorTopologyRenderCache.h/.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.h/.cpp`
- `sources/sector_editor/SectorEditorHelpers.h/.cpp`
- `sources/sector_editor/SectorEditorUiHelpers.h/.cpp`
- `sources/sector_demo/renderer/SectorMeshRenderer.h/.cpp`
- related renderer/runtime/lightmap/editor call sites found by `rg`

Commands used are listed in the appendix. This was static analysis only. It did
not run the editor, exercise UI flows, inspect visual output, or prove runtime
equivalence.

## Responsibility Inventory

| Responsibility | Representative methods / ranges | State touched | Related extracted files | Dependencies | Difficulty | Task type | Backlog |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Lifecycle and frame orchestration | `Init()` 441, `Shutdown()` 450, `Update()` 477, `Render()` 534, `RenderUI()` 560 | all editor state, assets, ECS world, preview | renderer helpers, modal files | engine context, input, assets, UI, renderer | Medium | Defer except tiny glue cleanup | REF-036 defer |
| Coordinate transforms and layout | `MapToScreen()` 698, `ScreenToMap()` 703, canvas/panel rect builders 776-791 | view center/zoom, canvas rect | `SectorEditorHelpers`, `SectorEditorUiHelpers` | raylib, editor layout constants | Low | Codex task if needed | possible new small item |
| 2D canvas input and hover | `UpdateHoverAndMouse()` 806, `HandleCanvasInput()` 932 | hover, tool, selection, pending tools | `SectorEditorAuthoringState`, `SectorEditorHelpers` picking helpers | input events, authoring graph, topology | High | Audit first | REF-024 adjacent |
| Selection/picking/drag | pick candidates 1268-1502, authoring drag 1542-1629, light drag 1637-1893, runtime object drag 1939-2011 | selection, drag state, topology positions, runtime objects | `SectorEditorSelectionTypes`, picking helpers | topology, runtime objects, lights | Medium/High | Split by target after audit | REF-021/022/024 |
| Preview update/controller | `UpdatePreview3D()` 2030, `UpdatePreview3DSelection()` 2163 | preview controllers, collision state, selected surface | `SectorEditorPreviewActions` | FPS/freefly, collision, renderer | Medium/High | Audit first for broad move | REF-036 defer |
| Authoring tool operations | pending line/rectangle/insert vertex 2209-2461, selected authoring delete 12129-12186 | authoring graph, derived topology, selection | `SectorEditorAuthoringState` | authoring graph, derived topology | Medium | Codex task only if narrow | no current REF |
| Dirty/cache helpers | `MarkTopologyDocumentEdited()` 2750, `FinishTopologyMaterialMutation()` 2760, `FinishTopologyActionResult()` 2816, `InvalidateTopologyRenderCache()` 5091 | dirty flags, status, render cache revision | `SectorEditorTopologyRenderCache`, authoring invalidation helpers | editor state | Low/Medium | Defer; keep central | REF-024 |
| Light actions | add/delete/move point/spot lights 3005-3228, light drag finish 1817 | `staticLights`, `staticSpotLights`, `dynamicPointLights`, `dynamicSpotLights` | `SectorEditorTopologyActions`, `SectorEditorLightInspector` | topology actions, renderer dynamic-light refresh | Medium/High | Audit first or split static/dynamic | REF-022 |
| Runtime object actions | add/delete/mutate/refresh 3249-3356, drag 1939-2011 | `runtimeObjects`, runtime object selection, ECS world | `SectorEditorTopologyActions`, runtime backend | topology runtime object list, ECS spawn/reset | Medium | Codex task in small slices | REF-021 |
| Lightmap bake orchestration | `StartLightmapBake()` 3368, polling/cancel/join 3481-3518, install 3586 | async bake state, map snapshot, baked metadata | `SectorEditorLightmapModal`, `SectorLightmap*` | worker thread, filesystem, source hash, assets | High | Audit/runner plan before extraction | REF-022/025/028 |
| Preview render orchestration | `RenderPreview3D*()` 3752-3788 | renderer facade, runtime object probes, AO flag | `SectorMeshRenderer` and renderer helpers | renderer facade, ECS world | Low/Medium | Defer or tiny glue task | REF-023 maybe |
| Preview overlays/debug | surface/spot/probe overlays 3831-3980, overlay UI 4006-4567 | selected surface, lights, object probes, preview debug tab | renderer facade, helpers | renderer geometry, visibility, UI | Medium/High | Audit first if broad | REF-023/022 |
| 2D cached drawing | `EnsureTopologyRenderCache()` 5097, `DrawTopologyDocument()` 5110, overlays 5188-5490 | render cache, selection, hover | `SectorEditorTopologyRenderCache` | raylib, cache types | Low/Medium | Defer; cache seam mostly done | REF-024 |
| Tools panel and lightmap settings | `DrawToolsPanel()` 5673 | tools, document modal state, grid, lightmap settings | `SectorEditorUiHelpers`, document actions | UI, topology lightmap settings | Medium | Split only lightmap settings if needed | REF-022 |
| Main inspector panel | `DrawSectorsPanel()` 6000 | selection, inspector scroll, runtime object UI, authoring/material UI | sector/vertex/light inspectors | UI, topology, runtime objects, authoring graph | High | Split by inspector family | REF-020/021/022 |
| SideDef/material inspector | `DrawTopologySideDefInspector()` 8990 | sidedef wall part/layer, material UV/decal state | `SectorEditorMaterialActions` | topology/material helpers | Medium | Codex task if scoped | REF-020 |
| Modal flows | add texture 9693, texture 9710, sprite 9726, save 9751, load 9835, confirmation 9964, decal tint 10023, door UV 10165, preview settings 10400, lightmap 10423 | modal states, picker states, input capture | texture modals, preview settings modal, lightmap modal | UI, assets, document actions | Low/Medium | Codex tasks by modal | REF-023 |
| Document workflow | reset/load/save 10523-10758 | whole editor state, texture scopes, runtime objects, preview resources | `SectorEditorDocumentActions` | filesystem, serialization, assets, runtime objects | Medium/High | Defer or narrow wrapper extraction | no current REF |
| Preview enter/leave/settings | enter/leave 10758-10866, spotlight pilot 10875-10975, settings 11022-11080 | renderer, controllers, collision, sky/directional/probe settings | `SectorEditorPreviewActions`, preview settings modal | renderer, FPS/freefly, source hash | Medium/High | Audit first if broad | REF-022/023 |
| Texture/catalog actions | default/editor textures 11114-11214, picker selection 12855-13306 | texture registry, asset handles, picker state | `SectorEditorTextureActions`, `SectorEditorTextureModals` | assets, topology texture registry | Medium | Codex task | REF-020 |
| Selection accessors | selected-object/topology getters 2543-2675, selectors 11616-12126 | selected IDs, UI buffers | selection types/helpers | topology find helpers | Medium | Defer unless split needs them | REF-021/022 |
| Material action wrappers | surface material helpers 12208-12789 | material/decal/UV state, preview rebuild | `SectorEditorMaterialActions` | material helpers, authoring mapping | Medium | Codex task after REF-020 scoping | REF-020 |
| Door texture settings actions | open/apply/reset/copy/apply-all 12940-13091 | selected door object, door UV modal | door runtime helpers | runtime objects, door renderer fit logic | Medium | Codex task with modal | REF-021/023 |

## Existing Extracted Seams

- `SectorEditorTextureActions.h/.cpp`: owns picker option population, add-map
  texture validation/registration, sprite metadata scans, billboard sprite
  picker state, topology/runtime texture picker open/apply helpers. Remaining
  nearby in `SectorEditor.cpp`: asset-scope refresh, modal wrapper callbacks,
  door picker routing, authoring picker rollback/apply glue in
  `ApplyTexturePickerSelection()` 13112.
- `SectorEditorTextureModals.h/.cpp`: owns texture and sprite picker modal
  drawing. Remaining nearby: add-map-texture modal draw is still local at 9693.
- `SectorEditorMaterialActions.h/.cpp`: owns material/decal/UV mutation
  helpers such as copy/paste, fit, align, clear, decal tint modal build.
  Remaining nearby: inspector UI rows and `SectorEditor` wrappers that decide
  authoring-vs-topology route and preview rebuild.
- `SectorEditorPreviewActions.h/.cpp`: owns preview pose/control-mode,
  collision-world rebuild, gameplay vertical context, and gameplay preview
  update. Remaining nearby: render pass calls, overlays/debug UI, spotlight
  pilot UI/application, preview settings application.
- `SectorEditorPreviewSettingsModal.h/.cpp`: owns preview settings modal draw
  and small object-probe setting apply helpers. Remaining nearby:
  `OpenPreviewSettingsModal()` and `ApplyPreviewSettingsModal()` still mutate
  preview, sky, directional light, and object-probe settings.
- `SectorEditorLightmapModal.h/.cpp`: owns bake modal drawing. Remaining
  nearby: bake worker lifecycle, result install, metadata mutation, and status
  handling.
- `SectorEditorTopologyRenderCache.h/.cpp`: owns expensive 2D cache build/draw
  for sectors, authoring overlay, diagnostics, linedefs, vertices, lights, and
  runtime objects. Remaining nearby: immediate overlays, cache invalidation,
  and picking.
- `SectorEditorTopologyActions.h/.cpp`: owns add/delete light/object/door
  topology operations and simple portal flag actions. Remaining nearby:
  selection cleanup, confirmations, runtime object respawn, renderer refresh.
- `SectorEditorAuthoringState.h/.cpp`: owns authoring graph selection,
  mutation, derivation, mapping, and authoring material mutation. Remaining
  nearby: authoring inspector UI and pending tool UI glue.
- `SectorEditorHelpers.h/.cpp`: owns many shared helpers: coordinate
  conversion, asset scans, clamp helpers, picking sort/choose, material target
  helpers, topology geometry helpers, path helpers. Remaining nearby: local
  helpers in the anonymous namespace for runtime object UI, preview overlays,
  and authoring inspector height.
- `SectorEditorUiHelpers.h/.cpp`: owns common input rows, color swatches,
  wrapped text measurement, small-font config, and inspector row layout helpers.
  Remaining nearby: repeated lambda-heavy inspector row bodies.
- `SectorEditorLightInspector.h/.cpp`: owns selected static/dynamic light and
  spotlight inspectors. Remaining nearby: add/delete/drag/pilot actions and
  lightmap tool settings.
- `SectorEditorSectorInspector.h/.cpp`: owns topology sector inspector.
  Remaining nearby: authoring face/line inspector and SideDef inspector.
- `SectorEditorVertexInspector.h/.cpp`: owns topology vertex inspector.
  Remaining nearby: authoring vertex display and authoring vertex drag.
- `SectorEditorDocumentActions.h/.cpp`: owns level paths, load/save plans,
  document format detection, empty document creation, and modal-state helpers.
  Remaining nearby: reset/load/save orchestration that coordinates renderer,
  runtime objects, texture scopes, selection, and UI state.
- `sources/sector_demo/renderer/SectorMeshRenderer.h/.cpp`: owns renderer
  facade and calls static scene, dynamic spotlight shadows, bloom, sky,
  billboards, doors, lightmap texture/layout, visibility debug. Remaining
  editor-side: when to rebuild, how to provide ECS world, overlay draw, and UI.

## Direct topologyMap Mutation And Cache Invalidation Review

Important central rule: `MarkTopologyDocumentEdited()` at
`sources/sector_editor/SectorEditor.cpp:2750` sets dirty flags and calls
`InvalidateTopologyRenderCache()` at line 2754.

| Group | File/function | Mutated data | Invalidation/refresh path | Risk | Recommendation |
| --- | --- | --- | --- | --- | --- |
| New/blank document | `SectorEditorDocumentActions.cpp:164`, `SectorEditor.cpp:10523` | replaces `state.topologyMap` | `ResetEditorTopologyDocumentState()` initializes state; reset path refreshes textures and state; no separate mark dirty expected for new blank map | Low | Keep as document action; no cache bug observed by static read |
| Load document | `SectorEditor.cpp:10548` | replaces `state.topologyMap`, restores baked metadata | explicit `InvalidateTopologyRenderCache()` at 10596, resets runtime objects at 10631 | Low | Good route; do not mark dirty on load |
| Authoring derivation | `SectorEditorAuthoringState.cpp:2157` | replaces derived `state.topologyMap` | authoring helpers include render-cache invalidation helpers at 59-65; callers generally mark authoring graph edited | Medium | Include in REF-024 deeper mutation audit if authoring changes are touched |
| Light add/delete/move | `SectorEditor.cpp:1817`, 3005-3228 | static/dynamic point/spot light arrays | topology actions plus `FinishTopologyActionResult()` -> `MarkTopologyDocumentEdited()`; dynamic-light pilot refreshes renderer sources | Medium | Good cache route; source-hash-sensitive for static lights only |
| Runtime object add/delete/mutate/drag | `SectorEditor.cpp:1939`, 3249-3356, 12873, 12954-13143 | `runtimeObjects` vector and selected object fields | `MarkTopologyDocumentEdited()` plus `RefreshRuntimeObjectsAfterAuthoringEdit()` respawns runtime ECS objects | Medium | Good route; extract around this wrapper first |
| Door placement | `SectorEditor.cpp:3268` | runtime object door in topology map | `MarkTopologyDocumentEdited()` plus runtime respawn | Medium | Keep with runtime object extraction |
| Topology sector rename | `SectorEditor.cpp:2931` | sector name or mapped authoring face anchor name | direct sector rename calls `MarkTopologyDocumentEdited()`; authoring route uses authoring mutation helper | Medium | Good route; authoring path should stay in authoring state helpers |
| Portal `blocksPlayer` | `SectorEditor.cpp:2829` | authoring line flag, derived topology/collision | `MutateSectorEditorAuthoringLineForTopologyLineDef()`; on change calls `RebuildSectorCollisionWorld()` | Medium | Collision changed only by existing path; keep with authoring helper |
| Material/UV/decal edits | `SectorEditor.cpp:12234-12789`, `DrawTopologySideDefInspector()` 8990 | surface textures, UVs, decals, middle texture | `FinishMaterialActionResult()` or `FinishTopologyMaterialMutation()` -> dirty/cache invalidation; may rebuild preview meshes | Medium | Extract UI only after preserving finish wrappers |
| Texture registry add | `SectorEditorTextureActions.cpp:416` | `texturesById[id]` | result applied by caller; texture registry does not need 2D render-cache invalidation per project rules unless display state starts using it | Low/Medium | Fine; final implementation reports should mention no 2D cache need |
| Texture picker apply | `SectorEditor.cpp:13112`, `SectorEditorTextureActions.cpp:942` | texture/material fields; temporary rollback for authoring material routes at 13262/13279 | result routes to `FinishTopologyMaterialMutation()` or `MarkTopologyDocumentEdited()`; preview may rebuild | Medium/High | REF-020 should keep this rollback/apply logic together |
| Lightmap result install | `SectorEditor.cpp:3586` | `bakedLightmap` metadata/object probe metadata | marks dirty directly but does not call render-cache invalidation; 2D cache does not draw baked metadata | Low | Accept; mention in lightmap work that source hash is checked before install |
| Preview settings modal | `SectorEditor.cpp:11034` | `previewSettings`, `skySettings`, `directionalLight`, object-probe lightmap settings | `MarkTopologyDocumentEdited()`; sky change may rebuild preview; object-probe settings affect hash but no bake result refresh | Medium/High | Needs source-hash-aware audit before broad extraction |
| Probe debug distance overlay | `SectorEditor.cpp:4485-4492` | `previewSettings.objectProbeDebugDrawMaxDistanceWorld` | calls `MarkTopologyDocumentEdited()`; source-hash-neutral preview setting | Low/Medium | Could avoid 2D cache churn in future, but current over-invalidation is allowed |

No missed 2D cache invalidation was obvious from static review. The risky
areas are not clear bugs; they are routes where mutation semantics are easy to
break during extraction.

## Lightmap / Source Hash Sensitive Areas

| Area | File/function | Source-hash sensitivity | Current tests obvious from static read | Extraction risk | Task type |
| --- | --- | --- | --- | --- | --- |
| Bake snapshot/hash gate | `SectorEditor.cpp:3368`, `StartLightmapBake()` | captures `mapSnapshot` and `expectedSourceHash = ComputeSectorLightmapSourceHash(state.topologyMap)` | sector lightmap tests exist under `tests/SectorTopologyLightmapTests.cpp` | High | Audit/runner plan |
| Result install stale check | `SectorEditor.cpp:3586`, `InstallLightmapBakeResult()` | discards bake if current hash differs from expected | likely covered by lightmap status/hash tests, needs confirmation | High | Audit first |
| Baked metadata/object-probe sidecar metadata | `SectorEditor.cpp:3661-3668` | writes bake result `sourceHash` and object probe `sourceHash` | lightmap/object probe tests likely relevant | Medium/High | Codex task only if narrow |
| Lightmap settings in tools panel | `SectorEditor.cpp:5911-5948` | AO radius/strength and indirect bounce radius/strength affect baked lighting and source hash | source-hash tests should cover settings | Medium | REF-022, small extraction |
| Object probe settings in preview settings modal | `SectorEditorPreviewSettingsModal.h:16-54`, `SectorEditor.cpp:11031-11080` | object probe spacing/height affect baked probes and source hash | needs confirmation | Medium/High | Audit first or very narrow Codex task |
| Directional light settings | `SectorEditor.cpp:11027-11080`, `SectorLightmap.cpp:3796-3881` | directional light affects baked lighting and is hashed | source-hash tests likely required | High | Audit first |
| Static point/spot lights | add/delete/move 1817-1893, 3005-3228; inspectors in `SectorEditorLightInspector.cpp` | static point/spot lights are included in `ComputeSectorLightmapSourceHash()` | lightmap tests likely cover static light hash | High | REF-022 split/audit |
| Sky settings | `OpenPreviewSettingsModal()` 11022, `ApplyPreviewSettingsModal()` 11034; renderer sky in `SectorSkyRenderer.cpp` | sky visual settings should remain source-hash-neutral; `ceilingSky` geometry is hash-sensitive elsewhere | source-hash tests should confirm visual settings excluded | Medium/High | Audit first if changing |
| Compute hash backend | `SectorLightmap.cpp:3796` | includes bake constants/settings, directional light, textures, vertices, linedefs, sidedefs, sectors, static lights/spots | `tests/SectorTopologyLightmapTests.cpp` | High | REF-028 checkpoint |

Source-hash behavior was not changed. Future extraction touching this area must
state that `previewSettings` and sky visual settings remain excluded, while
lightmap settings, directional light, static lights, and geometry-affecting sky
ceilings remain included.

## Runtime Object Authoring Areas

Already extracted:

- Add helpers for billboards/doors live in `SectorEditorTopologyActions.cpp`.
- Runtime object backend/spawn/reset lives in sector runtime object files.
- Renderer-side runtime object drawing is now in renderer helpers behind
  `SectorMeshRenderer`.
- Sprite picker scan/open/apply helpers live in `SectorEditorTextureActions.cpp`
  and `SectorEditorTextureModals.cpp`.

Remaining in `SectorEditor.cpp`:

- Runtime object selection/drag: `StartRuntimeObjectDrag()` 1939,
  `UpdateRuntimeObjectDrag()` 1958, `FinishRuntimeObjectDrag()` 1982,
  `CancelRuntimeObjectDrag()` 2011.
- Add/delete/mutate/refresh: 3249-3356.
- Runtime object inspector body inside `DrawSectorsPanel()` roughly
  6049-7430, including door and billboard variants.
- Billboard sprite/clip UI and metadata repair inside the inspector.
- Door property inspector, debug runtime target button, texture picker button,
  and door UV modal opener.
- Door texture settings modal draw and actions: 10165-10399 and 12940-13091.
- Runtime door texture picker handling in `ApplyTexturePickerSelection()` at
  13112-13144.

Recommendation for REF-021: split into at least three normal Codex tasks:

1. Extract runtime object inspector draw body into
   `SectorEditorRuntimeObjectInspector.h/.cpp`.
2. Extract runtime object actions/select/drag helpers only if the inspector
   extraction stays clean.
3. Extract door texture settings modal/actions, or fold them into a
   runtime-object modal module if the first extraction proves stable.

This does not need a runner plan if each slice is small. It does need manual UI
smoke after implementation because most risk is input/layout/selection behavior.

## Preview / Renderer Integration Areas

Renderer-owned now:

- `SectorMeshRenderer::RenderDynamicSpotLightShadowMaps()` at
  `sources/sector_demo/renderer/SectorMeshRenderer.cpp:903`.
- `SectorMeshRenderer::DrawScene()` at line 748.
- `SectorMeshRenderer::ApplyEmissiveDecalBloomToScene()` at line 925.
- Renderer rebuild/resources, lightmap texture/layout, visibility debug, door
  and billboard render paths, sky, bloom, and dynamic light uploads.

Editor-side preview orchestration still in `SectorEditor.cpp`:

- Render pass ordering: shadow maps, scene, overlays at 3752-3788.
- Surface picking/highlights: 3797-3869.
- Spotlight overlay and spotlight pilot action/UI: 3874-3953 and 10875-10975.
- Object probe debug overlay/data: 3956-3982.
- Preview overlay/debug tabs: 4006-4562.
- Preview UV/material panel: 4567 onward.
- Enter/rebuild/leave preview, runtime object spawn, collision world rebuild:
  10758-10856 and 12793.
- Freefly/FPS pose/control-mode glue through `SectorEditorPreviewActions`.

What should remain editor-side:

- Mode transitions, UI routing, input consumption, selected surface state,
  debug tab state, and deciding when to rebuild or respawn runtime objects.

What could move:

- Preview overlay/debug tab drawing into `SectorEditorPreviewOverlay.*`.
- Surface highlight and probe overlay drawing into small preview overlay helpers.
- Preview UV panel into a material/preview inspector module.

What should not move now:

- Renderer internals or `SectorMeshRenderer` facade ownership; the post-refactor
  renderer audit already concluded the facade is acceptable.
- Gameplay collision/sector lookup/physics. This audit found no reason to touch
  those paths.

## Modal / Inspector Remaining Work

| Flow | Current functions | Target module | Risk | Manual smoke |
| --- | --- | --- | --- | --- |
| Add map texture modal | `DrawAddMapTextureModal()` 9693 plus open/close/refresh 11177-11214 | `SectorEditorTextureModals` / texture actions | Low/Medium | yes |
| Save/load modals | `DrawSaveLevelModal()` 9751, `DrawLoadLevelModal()` 9835 | `SectorEditorDocumentModals.*` | Low/Medium | yes |
| Confirmation modal | `DrawConfirmationModal()` 9964 | `SectorEditorDocumentModals.*` or generic editor modal file | Low | yes |
| Decal tint modal | `DrawDecalTintModal()` 10023 | `SectorEditorMaterialModals.*` | Medium | yes |
| Door texture settings modal | `DrawDoorTextureSettingsModal()` 10165 | `SectorEditorRuntimeObjectModals.*` | Medium | yes |
| Runtime object inspector | in `DrawSectorsPanel()` around 6049-7430 | `SectorEditorRuntimeObjectInspector.*` | Medium | yes |
| Authoring line/face inspector | in `DrawSectorsPanel()` around 7870-8990 | `SectorEditorAuthoringInspector.*` | Medium/High | yes |
| SideDef/material inspector | `DrawTopologySideDefInspector()` 8990 | `SectorEditorSideDefInspector.*` or material inspector | Medium | yes |
| Preview overlay/UV panel | `DrawPreviewOverlay()` 4006, `DrawPreviewUvPanel()` 4567 | `SectorEditorPreviewOverlay.*`, material preview panel | Medium/High | yes |

## Private Helper / Duplication Pressure

| Helper group | Current location | Likely destination | Risk | Recommendation |
| --- | --- | --- | --- | --- |
| Runtime object UI reset/aspect/door motion | anonymous namespace lines 169-262 | runtime object inspector/actions | Low | Move with REF-021 |
| Object probe debug color/count/filter | anonymous namespace lines 129-167 | preview overlay helper | Low/Medium | Move with preview overlay extraction |
| Preview pose math | anonymous namespace lines 88-113 | preview actions/helper | Low/Medium | Move only with spotlight pilot/preview task |
| Grid/snap coordinate period helpers | anonymous namespace lines 62-83, 264 | canvas/authoring helper | Low | Defer unless canvas split starts |
| Authoring inspector height helpers | anonymous namespace lines 295-429 | authoring inspector module | Low | Move with authoring inspector extraction |
| Spotlight cone drawing | anonymous namespace line 332 | preview/light overlay helper | Low/Medium | Move with preview overlay or light extraction |
| Door modal layout already helper-based | `SectorEditorUiHelpers` and modal body | runtime object modal | Low | Move with door UV modal extraction |
| SideDef material row lambdas | `DrawTopologySideDefInspector()` | side/material inspector module | Medium | REF-020 |
| Texture/path/catalog helpers | mostly `SectorEditorHelpers` and `SectorEditorTextureActions` | already acceptable | Low | Defer |

## Recommended Extraction Roadmap

### Quick Codex Tasks

1. REF-021a: Extract runtime object inspector draw body to
   `SectorEditorRuntimeObjectInspector.*`, keeping `MutateSelectedRuntimeObject`
   and selection accessors in `SectorEditor` initially.
2. REF-023a: Extract save/load/confirmation document modals into
   `SectorEditorDocumentModals.*`.
3. REF-023b: Move add-map-texture modal draw into `SectorEditorTextureModals`.
4. REF-020a: Extract `DrawTopologySideDefInspector()` into a focused SideDef
   or material inspector file, preserving finish wrappers.
5. REF-021b: Extract door texture settings modal/actions into runtime object
   modal/action files.

### Needs Audit First

1. REF-022: Split into static-light inspector/actions and source-hash-sensitive
   light/object-probe settings. Static/dynamic light UI can be normal Codex
   tasks; source-hash-sensitive settings deserve a mini audit first.
2. Preview overlay/debug UI extraction if it moves more than drawing code.
3. Authoring line/face inspector extraction if it attempts to change authoring
   mutation routing instead of moving UI.
4. REF-024: A narrower mutation-site audit after any large inspector/action
   extraction.

### Needs Runner Plan

1. Full document-state vs preview-state separation.
2. Any broad preview controller/collision/renderer orchestration rewrite.
3. Any broad lightmap bake worker/result/source-hash refactor that changes
   ownership rather than moving wrapper code.

### Defer

1. Full `SectorEditor` state split.
2. Moving all canvas input/picking/drag code in one pass.
3. Rewriting `SectorMeshRenderer` facade from editor needs.
4. Generic material/UV framework.

Backlog recommendations:

- REF-020 should say "SideDef/material inspector and texture/material picker
  glue" rather than generic texture/material action code.
- REF-021 should be split into runtime object inspector, runtime object
  actions/drag, and door texture settings modal.
- REF-022 should be split or at least front-loaded with a mini audit for
  source-hash-sensitive settings.
- REF-023 remains valid for modal draw flows; start with document modals or
  add-map-texture modal.
- REF-024 remains useful, but this audit already gives a first-pass mutation
  map. Keep REF-024 for a narrower post-extraction audit or for direct bugfix
  follow-up if missed invalidation is suspected.

## Recommended First Implementation Task

Extract runtime object inspector/actions first, but start with the inspector
draw body only.

Why it is safe/useful:

- It is a large isolated section inside `DrawSectorsPanel()` with a clear
  selected runtime object target.
- Most topology mutation already routes through `MutateSelectedRuntimeObject()`,
  which calls `MarkTopologyDocumentEdited()` and respawns runtime objects.
- It does not need to touch renderer internals, lightmap source hashing, CMake,
  or schema.

Expected files:

- New `sources/sector_editor/SectorEditorRuntimeObjectInspector.h/.cpp`.
- Possibly small runtime-object inspector callback struct for operations still
  owned by `SectorEditor`.
- `SectorEditor.cpp` reduced to a wrapper/callback call.

Verification:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- manual editor smoke for selecting, editing, deleting, dragging, sprite
  picking, and door controls.

Cache invalidation behavior to preserve:

- Any runtime object mutation must still route through
  `MutateSelectedRuntimeObject()` or equivalent.
- That path must still call `MarkTopologyDocumentEdited()` and
  `RefreshRuntimeObjectsAfterAuthoringEdit()`.

## Backlog Update

REF-019 should be marked complete. Recommended backlog notes:

- Audit path: `docs/audit/sector_editor_cpp_seams_audit.md`.
- One-line conclusion: `SectorEditor.cpp` remains the biggest god-file risk,
  but the safest next extraction is runtime object inspector/actions in small
  slices.
- Recommended first follow-up: split REF-021 and start with runtime object
  inspector extraction.
- Do not mark REF-020 through REF-024 complete.

## Appendix: Evidence

Commands used:

```sh
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditor.h sources/sector_editor/SectorEditorTypes.h docs/audit/codebase_architecture_audit.md docs/audit/sector_editor_types_dependency_audit.md docs/plans/codebase_refactor_backlog.md
rg -n "^(void|bool|int|float|Vector[234]?|Rectangle|Color|std::[A-Za-z0-9_:<>]+|SectorEditor::|[A-Za-z0-9_:<>]+) SectorEditor::" sources/sector_editor/SectorEditor.cpp
rg -n "MarkTopologyDocumentEdited|InvalidateTopologyRenderCache|topologyMap\\.|authoringState|RenderPreview|Draw.*Modal|Draw.*Inspector|Texture|Material|RuntimeObject|Light|Lightmap|ObjectProbe|Door|Billboard|Selection|Pick|Drag|Save|Load|Import|Bake|Preview" sources/sector_editor/SectorEditor.cpp
rg -n "SectorEditor::.*Modal|SectorEditor::.*Inspector|SectorEditor::.*Texture|SectorEditor::.*Material|SectorEditor::.*Runtime|SectorEditor::.*Light|SectorEditor::.*Preview|SectorEditor::.*Document|SectorEditor::.*Selection|SectorEditor::.*Pick|SectorEditor::.*Drag" sources/sector_editor/SectorEditor.cpp
rg -n "state\\.topologyMap\\.|state\\.topologyMap|topologyMap" sources/sector_editor/*.cpp sources/sector_editor/*.h
rg -n "ComputeSectorLightmapSourceHash|BakeSectorLightmap|objectProbe|ObjectProbe|sourceHash|Lightmap|directionalLight|skySettings|staticLights|staticSpotLights|lightmapSettings" sources/sector_editor sources/sector_demo
rg -n "SectorMeshRenderer|RenderPreview3D|DrawScene|RenderDynamicSpotLightShadowMaps|ApplyEmissiveDecalBloom|UpdateVisibilityDebug" sources/sector_editor sources/sector_demo
rg -n "state\\.topologyMap[^;\\n]*(=|\\.push_back|\\.erase|\\.clear|\\.insert|emplace|Apply|Remove|Delete|Add|FinishMove|SetPortal|texturesById\\[|bakedLightmap|previewSettings|skySettings|directionalLight|lightmapSettings)" sources/sector_editor/SectorEditor.cpp sources/sector_editor/*.cpp
rg -n "MarkTopologyDocumentEdited|InvalidateTopologyRenderCache|FinishTopologyActionResult|FinishTopologyMaterialMutation|FinishMaterialActionResult|RefreshRuntimeObjectsAfterAuthoringEdit|ResetSectorRuntimeObjects|RefreshSectorRuntimeObjectMapData|SpawnPlacedRuntimeObjects|RefreshDynamicLightSources|RebuildPreviewMeshesPreservingView" sources/sector_editor/SectorEditor.cpp sources/sector_editor/*.cpp
wc -l sources/sector_editor/SectorEditor*Actions.* sources/sector_editor/SectorEditor*Inspector.* sources/sector_editor/SectorEditor*Modal.* sources/sector_editor/SectorEditorTopologyRenderCache.* sources/sector_editor/SectorEditorAuthoringState.* sources/sector_editor/SectorEditorHelpers.* sources/sector_editor/SectorEditorUiHelpers.* sources/sector_demo/renderer/SectorMeshRenderer.*
```

Selected line-count evidence:

- `sources/sector_editor/SectorEditor.cpp`: 13,313 lines.
- `sources/sector_editor/SectorEditor.h`: 439 lines.
- `sources/sector_editor/SectorEditorTypes.h`: 263 lines.
- `sources/sector_editor/SectorEditorAuthoringState.cpp`: 2,198 lines.
- `sources/sector_editor/SectorEditorHelpers.cpp`: 1,590 lines.
- `sources/sector_editor/SectorEditorTopologyRenderCache.cpp`: 1,331 lines.
- `sources/sector_editor/SectorEditorTextureActions.cpp`: 1,220 lines.
- `sources/sector_editor/SectorEditorMaterialActions.cpp`: 1,104 lines.
- `sources/sector_demo/renderer/SectorMeshRenderer.cpp`: 1,077 lines.

Rough `SectorEditor.cpp` function groupings:

- Lifecycle/render/UI entry: 441-693.
- Coordinate/canvas input: 698-1268.
- Picking/selection/drag: 1268-2030.
- Preview update/authoring tool operations: 2030-2508.
- Selected accessors/stale selection/dirty helpers: 2543-2869.
- Light/runtime object/lightmap actions: 2902-3690.
- Preview render/overlay/UV panel: 3752-5055.
- 2D cache draw wrappers and overlays: 5091-5673.
- Tools panel and main inspector: 5673-8990.
- SideDef/material inspector: 8990-9693.
- Modal flows: 9693-10445.
- Document and preview mode workflow: 10523-11114.
- Texture actions and picking helpers: 11114-11598.
- Selection setters/material wrappers/texture picker apply: 11616-13306.

Selected mutation/cache evidence:

- `MarkTopologyDocumentEdited()` calls `InvalidateTopologyRenderCache()`:
  `sources/sector_editor/SectorEditor.cpp:2750-2754`.
- Runtime object mutation calls dirty/cache path and respawns runtime objects:
  `sources/sector_editor/SectorEditor.cpp:3336-3356`.
- Light add/delete routes through `FinishTopologyActionResult()`:
  `sources/sector_editor/SectorEditor.cpp:3005-3228`.
- Lightmap bake captures and validates source hash:
  `sources/sector_editor/SectorEditor.cpp:3401-3402` and 3592.
- Lightmap install writes baked metadata without render-cache invalidation:
  `sources/sector_editor/SectorEditor.cpp:3661-3668`.
- Preview settings writes preview, sky, directional light, and object-probe
  settings, then calls `MarkTopologyDocumentEdited()`:
  `sources/sector_editor/SectorEditor.cpp:11076-11080`.
- Texture registry mutation occurs in action helper:
  `sources/sector_editor/SectorEditorTextureActions.cpp:416`.

Selected renderer evidence:

- Editor render pass glue calls `SectorMeshRenderer`:
  `sources/sector_editor/SectorEditor.cpp:3761-3788`.
- Renderer facade owns dynamic spot shadow maps, scene draw, bloom, visibility
  debug, and lightmap status APIs:
  `sources/sector_demo/renderer/SectorMeshRenderer.h:54-84`.
- Renderer implementation calls static scene/dynamic lights/doors/billboards:
  `sources/sector_demo/renderer/SectorMeshRenderer.cpp:738-927`.

Static-analysis caveats:

- Regex results do not prove every mutation path or callback branch.
- UI/manual smoke needs are inferred from code shape.
- Test coverage references are based on file names and nearby code; exact
  assertions need manual confirmation before relying on them.
