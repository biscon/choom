# SectorEditor.cpp Post-Tool-Migration Line Map

## Summary

- Current `sources/sector_editor/SectorEditor.cpp` line count: **10,551**.
- Major architecture improvements now complete: renderer internals moved behind
  `sector_demo/renderer`, placed-object editor code split into
  `tools/placed_objects`, `tools/billboards`, and `tools/doors`, document
  actions/modals moved under `document/`, Line/Rectangle/Insert Vertex/Select
  migrated through the v0 tool contract, Selection service helpers extracted,
  Manipulation service shell added, and placed-object movement has a provider
  pilot.
- Top 5 remaining line-count clusters:
  1. Main inspector panel and authoring material remnants
  2. `DrawTopologySideDefInspector()` material/decal/UV UI
  3. Preview overlay and preview UV/material panel
  4. Texture picker routing plus material mutation wrappers
  5. Light/static-light drag/actions and lightmap bake orchestration
- Recommended next implementation task: **extract the SideDef/material/decal
  inspector UI into `sources/sector_editor/tools/materials/` after one narrow
  pre-implementation audit of the exact callback/context boundary**.
- Line-count reduction should accelerate only when the next tasks target whole
  remaining UI clusters. Provider refinements are architecturally useful but
  will not remove many lines from `SectorEditor.cpp`.

## Current File Size Snapshot

Line counts from `wc -l`.

| File | Lines |
| --- | ---: |
| `sources/sector_editor/SectorEditor.cpp` | 10,551 |
| `sources/sector_editor/SectorEditor.h` | 432 |
| `sources/sector_editor/SectorEditorTypes.h` | 263 |
| `sources/sector_editor/SectorEditorModalTypes.h` | 186 |
| `sources/sector_editor/SectorEditorSelectionTypes.h` | 143 |
| `sources/sector_editor/SectorEditorPreviewTypes.h` | 42 |
| `sources/sector_editor/SectorEditorAuthoringState.cpp` | 2,198 |
| `sources/sector_editor/SectorEditorHelpers.cpp` | 1,590 |
| `sources/sector_editor/SectorEditorTopologyRenderCache.cpp` | 1,331 |
| `sources/sector_editor/SectorEditorTextureActions.cpp` | 1,220 |
| `sources/sector_editor/SectorEditorMaterialActions.cpp` | 1,104 |
| `sources/sector_editor/SectorEditorLightInspector.cpp` | 784 |
| `sources/sector_editor/SectorEditorTextureModals.cpp` | 547 |
| `sources/sector_editor/SectorEditorSectorInspector.cpp` | 523 |
| `sources/sector_editor/SectorEditorPreviewSettingsModal.cpp` | 406 |
| `sources/sector_editor/SectorEditorPreviewActions.cpp` | 343 |
| `sources/sector_editor/SectorEditorUiHelpers.cpp` | 262 |
| `sources/sector_editor/document/SectorEditorDocumentActions.cpp` | 393 |
| `sources/sector_editor/document/SectorEditorDocumentModals.cpp` | 290 |
| `sources/sector_editor/selection/SectorEditorSelectionService.cpp` | 895 |
| `sources/sector_editor/selection/SectorEditorManipulationService.cpp` | 279 |
| `sources/sector_editor/tools/billboards/SectorEditorBillboardInspector.cpp` | 644 |
| `sources/sector_editor/tools/doors/SectorEditorDoorModals.cpp` | 446 |
| `sources/sector_editor/tools/doors/SectorEditorDoorInspector.cpp` | 393 |
| `sources/sector_editor/tools/insert_vertex/SectorEditorInsertVertexTool.cpp` | 231 |
| `sources/sector_editor/tools/select/SectorEditorSelectTool.cpp` | 198 |
| `sources/sector_editor/tools/rectangle/SectorEditorRectangleTool.cpp` | 182 |
| `sources/sector_editor/tools/line/SectorEditorLineTool.cpp` | 164 |
| `sources/sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.cpp` | 135 |

## What Has Already Moved Out

- Renderer: preview renderer internals now live under
  `sources/sector_demo/renderer/`; `SectorEditor.cpp` keeps high-level preview
  orchestration and overlay/editor UI.
- Placed objects/billboards/doors: common placed-object actions, drag,
  inspector dispatch, billboard actions/inspector, door actions/inspector, and
  door modals live under `sources/sector_editor/tools/`.
- Document: document actions and save/load/confirmation modals live under
  `sources/sector_editor/document/`; reset/load/save orchestration remains in
  `SectorEditor.cpp`.
- Line/Rectangle/Insert Vertex/Select tools: active tool input was migrated
  into `sources/sector_editor/tools/{line,rectangle,insert_vertex,select}`.
- Selection service: selected-state helpers and stale-selection handling live
  in `sources/sector_editor/selection/SectorEditorSelectionService.*`.
- Manipulation service: generic manipulation shell and placed-object provider
  integration live in `sources/sector_editor/selection/`.
- Material/texture/document modals: add-map texture, texture picker, sprite
  picker, decal tint, preview settings, lightmap bake, document save/load, and
  confirmation draw bodies have module ownership. `SectorEditor.cpp` mostly
  keeps callback/wrapper construction for those flows.

## Remaining SectorEditor.cpp Responsibility Inventory

| Responsibility | Current functions / approximate line ranges | Estimated lines | State touched | Existing related modules | Extraction target | Risk | Task type | Expected reduction | Notes |
| --- | --- | ---: | --- | --- | --- | --- | --- | ---: | --- |
| Lifecycle/update/render orchestration | `Init()` 355, `Shutdown()` 364, `Update()` 391, `Render()` 448, `RenderUI()` 474 | 250 | whole editor, assets, world, modal state | renderer, document, tools | keep central | Medium | Defer | 0-80 | Coordinator glue is acceptable; do not chase tiny wrapper lines first. |
| Canvas routing and tool context construction | `BuildToolContext()` 622, `UpdateHoverAndMouse()` 797, `HandleCanvasInput()` 910 | 650 | current tool, hover, selection, manipulation, pending tools | tool modules, selection/manipulation services | `tools/` plus future light/material providers | Medium | Codex task after specific provider slices | 100-250 | Current v0 tool dispatch is working; remaining direct branches are mostly lights and placement tools. |
| 2D topology/cache draw orchestration | `DrawGrid()` 4689, cache helpers 4725-4744, topology draw/overlays 4744-5173 | 480 | topology render cache, hover, selection, light drag | `SectorEditorTopologyRenderCache.*` | keep central or small overlay module | Medium | Defer | 0-150 | Cache rebuild is already extracted. Moving overlays is not the top line-count win. |
| Main tools panel | `DrawToolsPanel()` 5175-5500 | 326 | tool, pending tool state, grid, lightmap settings | tools, lightmap modal/actions | maybe `SectorEditorToolsPanel.*` later | Medium | Defer / Audit first for lightmap settings | 200-300 | Large enough, but it is mostly coordinator UI and includes source-hash-sensitive lightmap settings. |
| Main inspector routing panel | `DrawSectorsPanel()` 5502-7332 | ~1,830 | selection, inspector scroll, authoring graph, placed-object callbacks, light inspectors | sector/vertex/light inspectors, placed-object tools | feature inspectors under `tools/materials` and authoring inspector modules | Medium/High | Codex task by slice | 300-900 | Biggest local cluster; extract by selected target, not as one generic inspector landfill. |
| Authoring line/face/vertex inspector remnants | inside `DrawSectorsPanel()` 5600-7332; helper heights 214-350 | ~700 | authoring graph, selected authoring target, material helpers | `SectorEditorAuthoringState.*` | `tools/materials` or `tools/authoring_*` follow-up | Medium | Audit first | 250-500 | Coupled to derived topology mapping and material wrappers. Good follow-up after material boundary is clearer. |
| SideDef/material/decal inspector | `DrawTopologySideDefInspector()` 7334-8035 | 702 | sidedef wall part, UV inputs, decal tint, material layer, map material fields | `SectorEditorMaterialActions.*`, `SectorEditorMaterialModals.*` | `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.*` | Medium/High | Audit first, then Codex task | 550-700 | Best next line-count target. Must preserve finish wrappers, cache invalidation, and preview rebuild. |
| Texture picker routing/catalog flows | modal wrappers 8037-8093, texture asset functions 8943-9057, picker routing 10257-10549 | ~520 | texture registry, picker state, sprite picker, asset handles, material targets | `SectorEditorTextureActions.*`, `SectorEditorTextureModals.*` | `tools/materials` for material picker routing; keep catalog in texture module | Medium/High | Audit first for material picker route | 150-350 | The authoring rollback/apply logic in `ApplyTexturePickerSelection()` is fragile and should move with material inspector callbacks, not catalog import. |
| Preview render orchestration | `RenderPreview3D*()` 3386-3429 | 44 | renderer facade, world | `sector_demo/renderer` | keep central | Low | Defer | 0-40 | Acceptable coordinator glue. |
| Preview overlay/debug UI | `DrawPreviewSurfaceHighlights()` 3465, spotlight/probe overlay 3508-3617, `DrawPreviewOverlay()` 3640-4195 | ~730 | preview pose, visibility, collision debug, dynamic lights, object probes | renderer facade, preview actions | `sources/sector_editor/preview/SectorEditorPreviewOverlay.*` | Medium/High | Audit first | 450-650 | Line-count juicy, but coupled to renderer/debug/collision labels. |
| Preview UV/material panel | `BuildPreviewUvPanelRect()` 4196, `DrawPreviewUvPanel()` 4201-4688 | ~490 | 3D surface selection, material layer, portal blocks-player, UV/decal inputs | material actions, preview renderer | `tools/materials` or `preview/SectorEditorPreviewUvPanel.*` | Medium/High | Audit first | 350-480 | Likely belongs with materials if the same callbacks can serve SideDef and 3D surface material UI. |
| Light/static-light/spotlight actions and drag | `StartLightDrag()` 1420-1720, add/delete light functions 2636-2859, find light functions 9115-9265 | ~780 | static/dynamic lights, selection, light drag | `SectorEditorLightInspector.*`, topology actions | future `tools/lights/` | High | Audit first | 300-600 | Static lights and static spotlights affect source hash. Do not move casually. |
| Light/object-probe/source-hash-sensitive settings | preview settings 8851-8942, tools panel lightmap settings 5383-5461, preview overlay probe controls | ~350 | preview settings, sky, directional light, object probe settings | preview settings modal, lightmap | future light/preview audit | High | Audit first / Runner plan if broad | 100-250 | Must keep preview/sky visual settings source-hash-neutral and bake-affecting settings hashed. |
| Lightmap bake worker orchestration | `BakeLightmaps()` 2997, `StartLightmapBake()` 3002, poll/cancel/join/shutdown/result install 3115-3321 | 325 | worker thread, bake progress/result, baked metadata, object probes | `SectorEditorLightmapModal.*`, lightmap backend | possible `SectorEditorLightmapBakeController.*` | High | Runner plan | 250-320 | Do not move without source-hash/result-install tests and worker lifecycle guardrails. |
| Document lifecycle orchestration | reset/load/save/open modal functions 8352-8576 | ~225 | whole document, asset scopes, renderer, runtime objects | `document/` actions/modals | keep central | Medium/High | Defer | 0-120 | It coordinates resources and should remain central unless a narrow wrapper is obvious. |
| Material action wrappers | finish helpers 2431-2487, authoring/material helpers 9636-10193 | ~650 | topology map, authoring graph mapping, UI inputs, preview rebuild | `SectorEditorMaterialActions.*` | `tools/materials/SectorEditorMaterialActionsBridge.*` after inspector move | Medium/High | Codex task after audit | 300-500 | Meaningful reduction, but callbacks must preserve cache/preview rebuild semantics. |
| Selection/manipulation glue wrappers | selected getters 2262-2404, service contexts 2158-2259, select wrappers 9445-9555 | ~430 | selected IDs, UI reset state | selection service | selection service refinements | Low/Medium | Normal Codex task | 100-250 | Useful cleanup, not the best line-count reduction. |
| Stale compatibility wrappers | pending authoring cancel wrappers 1925-1973, runtime object wrappers 1722-1743/2880-2995, modal wrapper callbacks | ~300 | temporary state, callback routing | tools/document/modules | fold opportunistically | Low/Medium | Normal Codex task when touching owner | 50-200 | Avoid standalone wrapper cleanup unless it enables a feature-folder extraction. |

## Biggest Remaining Clusters

1. `DrawSectorsPanel()` and authoring inspector remnants.
   This is large because it still routes every selected target and contains
   authoring graph material UI that predates the tool-folder split. It is a real
   design problem when target-specific inspector code stays local, but the top
   of the function is acceptable coordinator glue. Extract soon by target
   family, starting with materials.

2. `DrawTopologySideDefInspector()`.
   This is large because it combines wall-part selection, texture picker open
   paths, base/decal layer UI, UV inputs, copy/paste, decal controls, fit, and
   align actions. It should be extracted soon into `tools/materials/`; the
   design issue is that material UI and material mutation routing are split
   across the god file and category action modules.

3. Preview overlay and preview UV panel.
   This is large because it mixes debug text generation, collision/visibility
   status, dynamic-light/object-probe debug controls, 3D surface material
   editing, and preview-specific interaction rectangles. It is line-count juicy
   but should be audited before moving because renderer data, surface
   selection, and collision labels are coupled.

4. Material wrappers and texture picker routing.
   These remain large because authoring-graph material edits need derived
   topology mapping and temporary rollback/apply handling. This is a real
   extraction candidate, but it should follow or accompany a material inspector
   context so picker routing does not get separated from the material feature.

5. Lights and lightmap orchestration.
   Lights remain spread across canvas hover/pick, drag, add/delete, inspectors,
   preview overlays, and bake settings. This is a design problem, but it is
   high-risk because static lights, directional light, object probes, and
   lightmap settings affect bake source hashes. Audit first.

## Risk Review

- Light/static-light/object-probe source-hash-sensitive flows: static point
  lights, static spotlights, directional light settings, object-probe bake
  settings, and lightmap settings affect baked lighting. Sky visual settings
  and preview-only settings should remain source-hash-neutral.
- Lightmap bake worker lifecycle: `StartLightmapBake()`, result polling,
  cancellation, temporary file cleanup, source-hash stale-result rejection, and
  object-probe sidecar install are tightly coupled. This should be a runner
  plan, not casual extraction.
- Preview overlays and UV panel: the overlay reads renderer visibility,
  collision state, dynamic-light selection/debug state, object probes, and
  selected 3D material surfaces. Audit before moving more than draw-only code.
- Material mutation routes: `FinishTopologyMaterialMutation()`,
  `FinishMaterialActionResult()`, `FinishAuthoringSideMaterialActionResult()`,
  and `ApplyAuthoringFaceAnchorFlatMaterialAction()` preserve dirty/cache UI
  reset and preview rebuild paths. Material extraction must keep those finish
  paths equivalent.
- Direct `topologyMap` mutations: material UV edits, picker apply, light moves,
  preview settings, lightmap result install, and document load/reset have
  different dirty/cache rules. Over-invalidation is acceptable; missed
  invalidation is the bug.
- Collision/physics/sector lookup: preview enter/rebuild and gameplay preview
  state touch `SectorCollisionWorld`, vertical context, controller pose, and
  runtime door blockers. Do not move collision/sector lookup with overlay UI.

What should not be moved casually: lightmap bake orchestration, preview
enter/leave/rebuild orchestration, gameplay collision/sector lookup, renderer
resource lifetime, and source-hash-sensitive light settings.

## Recommended Next Tasks

### Immediate Codex Tasks

1. Add `tools/materials/` skeleton and extract `DrawTopologySideDefInspector()`
   through a narrow context/callback API after a short boundary audit. Expected
   `SectorEditor.cpp` reduction: 550-700 lines.
2. Move low-risk material wrapper callbacks that are needed by the inspector
   into the same materials feature folder. Expected reduction: 150-300 lines.
3. Fold stale runtime-object wrapper lines only when touching placed-object
   feature code. Expected reduction: 50-120 lines.
4. Minor selection/manipulation provider refinements can proceed as normal
   Codex tasks, but they should not displace material line-count work.

### Audit First

1. Materials/sidedef/decal boundary audit: exact callbacks, UI state inputs,
   texture picker routing, authoring-graph routes, cache invalidation, and
   preview rebuild behavior.
2. Preview overlay/UV audit: decide whether preview UV belongs in
   `tools/materials/` or a preview module, and separate draw-only overlay from
   collision/renderer status ownership.
3. Lights/source-hash-sensitive audit: static/dynamic light drag, inspectors,
   preview spotlight overlay, directional/object-probe settings, and bake hash
   coverage.
4. Direct topology mutation/cache invalidation audit after the next material
   extraction.

### Runner Plan

1. Lightmap bake worker/controller extraction.
2. Broad lights/source-hash migration if it moves static lights, directional
   light, object probe settings, and preview settings together.
3. Broad preview tool/module migration if it moves preview overlay, UV panel,
   preview settings, control mode, and rebuild orchestration together.

### Defer

1. Lifecycle/update/render orchestration cleanup.
2. Full document-state vs preview-state rewrite.
3. Generic material/UV framework.
4. 2D topology render-cache draw orchestration, unless a specific cache bug is
   being fixed.

## Recommended First Implementation Task

Pick: **materials/sidedef/decal**, but start with a narrow audit before direct
implementation.

Target folder and likely files:

- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.h`
- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp`
- Possibly later:
  `sources/sector_editor/tools/materials/SectorEditorMaterialPickerRouting.*`

Guardrails:

- Preserve `FinishTopologyMaterialMutation()` and
  `FinishMaterialActionResult()` behavior.
- Preserve `MarkTopologyDocumentEdited()` / render-cache invalidation behavior
  for topology material changes.
- Preserve preview rebuild-on-apply behavior when editing materials in 3D
  preview.
- Preserve authoring-side and authoring-face routes that temporarily apply to
  derived topology and then write back through authoring helpers.
- Keep texture catalog/import and add-map-texture scan/import in the existing
  texture modules. Only material-specific picker open/apply routing should move
  with materials.
- Do not change lightmap source-hash behavior. Material texture/UV/decal edits
  remain topology edits; source-hash behavior should be whatever existing
  lightmap hash code currently defines.

The first implementation should not move preview overlay/debug UI, light
settings, lightmap worker code, or collision/preview rebuild orchestration.

## Backlog Update

`docs/plans/codebase_refactor_backlog.md` was updated by this audit.

Required updates:

- Added REF-053 and marked it complete.
- Completion notes include this report path, the current `SectorEditor.cpp`
  line count, and the recommended next implementation task.
- Fixed stale overview status for REF-052: the detailed backlog already marked
  Select migration complete, while the overview table still showed open.
- Refined future items around materials/sidedef/decal, preview overlay/UV, and
  lights/source-hash-sensitive work.

Future backlog refinements from this report:

- REF-044 should split into a materials boundary audit and implementation
  slices.
- Add a specific SideDef/material inspector extraction item as the next likely
  implementation.
- Add a specific preview overlay/UV audit item before moving that cluster.
- Keep provider refinements as useful, but lower priority than material
  line-count reduction.
- Treat older runtime-object/select notes in REF-019/REF-040/REF-047 as stale
  where they recommend work already completed by REF-041 through REF-052.

## Appendix: Evidence

Commands used:

```sh
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditor.h sources/sector_editor/SectorEditorTypes.h sources/sector_editor/SectorEditorModalTypes.h sources/sector_editor/SectorEditorSelectionTypes.h sources/sector_editor/SectorEditorPreviewTypes.h sources/sector_editor/*.h sources/sector_editor/*.cpp
find sources/sector_editor -maxdepth 4 -type f | sort
rg -n "^.*SectorEditor::" sources/sector_editor/SectorEditor.cpp
rg -n "Draw.*Inspector|Draw.*Panel|Draw.*Modal|DrawPreview|PreviewOverlay|PreviewUv|SideDef|Material|Decal|TexturePicker|Lightmap|Bake|ObjectProbe|SpotLight|StaticLight|DynamicLight|Directional|Select|Selection|Manipulation|Build.*Context|ToolContext|FinishTopologyMaterialMutation|FinishMaterialActionResult|MarkTopologyDocumentEdited|InvalidateTopologyRenderCache" sources/sector_editor/SectorEditor.cpp
wc -l sources/sector_editor/tools/*.cpp sources/sector_editor/tools/*/*.cpp sources/sector_editor/selection/*.cpp sources/sector_editor/document/*.cpp sources/sector_editor/tools/*.h sources/sector_editor/tools/*/*.h sources/sector_editor/selection/*.h sources/sector_editor/document/*.h
rg -n "REF-020|REF-022|REF-023|REF-024|REF-044|REF-045|REF-046|REF-052|stale|source hash|source-hash|cache|collision|runtime refresh" docs/plans/codebase_refactor_backlog.md
```

Selected `wc -l` evidence:

```text
10551 sources/sector_editor/SectorEditor.cpp
  432 sources/sector_editor/SectorEditor.h
  263 sources/sector_editor/SectorEditorTypes.h
 2198 sources/sector_editor/SectorEditorAuthoringState.cpp
 1590 sources/sector_editor/SectorEditorHelpers.cpp
 1331 sources/sector_editor/SectorEditorTopologyRenderCache.cpp
 1220 sources/sector_editor/SectorEditorTextureActions.cpp
 1104 sources/sector_editor/SectorEditorMaterialActions.cpp
  895 sources/sector_editor/selection/SectorEditorSelectionService.cpp
  644 sources/sector_editor/tools/billboards/SectorEditorBillboardInspector.cpp
  446 sources/sector_editor/tools/doors/SectorEditorDoorModals.cpp
  393 sources/sector_editor/document/SectorEditorDocumentActions.cpp
  393 sources/sector_editor/tools/doors/SectorEditorDoorInspector.cpp
  290 sources/sector_editor/document/SectorEditorDocumentModals.cpp
```

Selected function/range inventory:

```text
355:bool SectorEditor::Init(engine::EngineContext& context)
391:void SectorEditor::Update(engine::EngineContext& context, float dt)
474:void SectorEditor::RenderUI(...)
622:SectorEditorToolContext SectorEditor::BuildToolContext(engine::Input* input)
797:void SectorEditor::UpdateHoverAndMouse(engine::Input& input)
910:void SectorEditor::HandleCanvasInput(engine::Input& input, float dt)
1420:void SectorEditor::StartLightDrag(int topologyLightId, SpotLightHandle spotHandle)
1746:void SectorEditor::UpdatePreview3D(engine::Input& input, engine::AssetManager& assets, float dt)
2158:SectorEditorManipulationServiceContext SectorEditor::BuildManipulationServiceContext()
2249:SectorEditorSelectionServiceContext SectorEditor::BuildSelectionServiceContext()
2421:void SectorEditor::MarkTopologyDocumentEdited(const char* status)
2431:bool SectorEditor::FinishTopologyMaterialMutation(const char* status, engine::AssetManager* assets)
2444:bool SectorEditor::FinishMaterialActionResult(...)
2997:bool SectorEditor::BakeLightmaps()
3002:bool SectorEditor::StartLightmapBake()
3220:bool SectorEditor::InstallLightmapBakeResult(...)
3386:void SectorEditor::RenderPreview3D(engine::AssetManager& assets)
3640:void SectorEditor::DrawPreviewOverlay(...)
4201:void SectorEditor::DrawPreviewUvPanel(...)
5175:void SectorEditor::DrawToolsPanel(...)
5502:void SectorEditor::DrawSectorsPanel(...)
7334:bool SectorEditor::DrawTopologySideDefInspector(...)
8037:void SectorEditor::DrawAddMapTextureModal(...)
8352:void SectorEditor::ResetToBlankMap(engine::EngineContext& context)
8851:void SectorEditor::OpenPreviewSettingsModal()
8943:void SectorEditor::RefreshDefaultTextures()
9636:bool SectorEditor::FinishAuthoringSideMaterialActionResult(...)
9877:bool SectorEditor::ApplySurface3DUvValue(...)
10257:std::string SectorEditor::CurrentTextureForPickerTarget() const
10350:void SectorEditor::ApplyTexturePickerSelection(engine::AssetManager& assets)
```

Evidence for recommended next task:

- `DrawTopologySideDefInspector()` spans lines 7334-8035, about 702 lines.
- Material action wrappers span roughly 9636-10193, about 558 lines.
- Preview UV panel spans lines 4201-4688, about 488 lines and overlaps material
  mutation callbacks.
- `ApplyTexturePickerSelection()` spans lines 10350-10549 and contains
  authoring-side/authoring-flat routing with temporary topology rollback.
- Existing modules already provide material mutation helpers and decal tint
  modal ownership, so the missing feature-folder owner is the inspector/picker
  route, not low-level material operations.

Stale backlog notes found:

- REF-052 overview row still showed open, while the detailed REF-052 section
  already showed `[x]`.
- REF-019 and REF-040 recommendations that centered runtime object extraction
  or runtime-object folder migration are now stale after REF-041.
- REF-047 framing that Select still awaited migration is stale after REF-052,
  though its provider-contract cautions remain useful.
- REF-023 notes about document/add-texture/decal modal slices are partly stale
  because those draw flows have moved; remaining modal work is now mostly
  callback routing or feature-specific picker behavior.

Validation for this audit was documentation-only. Build was not run because no
source, tests, CMake, or behavior were changed.
