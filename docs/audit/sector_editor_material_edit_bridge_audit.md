# SectorEditor MaterialEditBridge and Picker Routing Audit

## Summary

This audit is needed because REF-055 moved the SideDef/material/decal inspector into `tools/materials/`, and REF-057 extracted only generic texture picker lifecycle/result mechanics into `TexturePickerService`. The material-specific edit routes still return through broad `SectorEditor.cpp` wrappers and through target branches in `ApplyTexturePickerSelection()`.

The remaining bridge problem is not UI layout. It is orchestration: topology-vs-authoring writeback, preview rebuilds, dirty/cache invalidation, lightmap-source-sensitive topology edits, UI input resets, decal modal state, and picker target routing are still coupled through `SectorEditor.cpp`.

Recommended next task: **REF-063: Extract material-specific texture picker routing from `ApplyTexturePickerSelection()`**. Picker routing should move before a broader `MaterialEditBridge`, because the picker target split is already explicit and can remove the riskiest switch growth while leaving finish wrappers centralized. Expected reduction is roughly 120-220 lines from `SectorEditor.cpp`/`SectorEditorTextureActions.cpp` or 4-6 target branches isolated, with little or no callback reduction yet. Top risks are preserving authoring rollback/apply semantics, `rebuildPreviewOnApply`, and not moving door/sky/runtime behavior.

Completion note: REF-063 implemented the recommended material-specific picker
routing extraction under `services/material_edit/`. Broader `MaterialEditBridge`
work remains future work.

## Scope And Method

Static analysis only. No GUI/render smoke was performed.

Inspected files:

- `docs/audit/sector_editor_materials_boundary_audit.md`
- `docs/audit/sector_editor_shared_service_inventory.md`
- `docs/plans/codebase_refactor_backlog.md`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorSurfaceTypes.h`
- `sources/sector_editor/SectorEditorModalTypes.h`
- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.h`
- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp`
- `sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.h`
- `sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp`
- `sources/sector_editor/SectorEditorTextureActions.h`
- `sources/sector_editor/SectorEditorTextureActions.cpp`
- `sources/sector_editor/SectorEditorTextureModals.h`
- `sources/sector_editor/SectorEditorTextureModals.cpp`
- `sources/sector_editor/SectorEditorMaterialActions.h`
- `sources/sector_editor/SectorEditorMaterialActions.cpp`
- `sources/sector_editor/SectorEditorMaterialModals.h`
- `sources/sector_editor/SectorEditorMaterialModals.cpp`
- `sources/sector_editor/SectorEditorSectorInspector.h`
- `sources/sector_editor/SectorEditorSectorInspector.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.h`
- `sources/sector_editor/SectorEditorAuthoringState.cpp`
- `sources/sector_editor/SectorEditorHelpers.h`
- `sources/sector_editor/SectorEditorHelpers.cpp`
- `sources/sector_editor/SectorEditorUiHelpers.h`
- `sources/sector_editor/SectorEditorUiHelpers.cpp`
- `sources/sector_editor/tools/doors/`
- `sources/sector_editor/tools/billboards/`
- `sources/sector_editor/document/`
- `sources/sector_editor/selection/`
- `sources/sector_demo/SectorTopologyTypes.h`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorLightmap.*`

Commands used are listed in the appendix.

## Current Material Editing Route Inventory

| Function/block | Current file/range | Clients/features | State touched | Topology vs authoring vs preview surface | Dirty/cache/preview rebuild behavior | Picker involvement | Recommended owner | Move readiness | Risk | Notes |
|---|---:|---|---|---|---|---|---|---|---|---|
| `FinishTopologyMaterialMutation()` | `SectorEditor.cpp:2433-2444` | material wrappers, direct inspector UV edits, picker apply finish | `topologyRenderWarning`, dirty flags, status | topology source of truth, preview rebuild if active | clears warning, calls `MarkTopologyDocumentEdited()`, may call `RebuildPreviewMeshesPreservingView()` | post-picker material apply uses it | future `MaterialEditBridge` or central editor finish hook | Needs service first | High | Generic material mutation finish behavior. |
| `FinishMaterialActionResult()` | `SectorEditor.cpp:2446-2486` | material action helpers, SideDef inspector, preview UV panel | UI float input states, decal tint modal, status | all material routes | delegates changed result to `FinishTopologyMaterialMutation()` | no direct picker, but picker wraps into equivalent result | future `MaterialEditBridge` | Needs service first | High | Duplicated reset behavior also exists in authoring finish routes. |
| `FinishAuthoringSideMaterialActionResult()` | `SectorEditor.cpp:9029-9100` | wall material edits under authoring graph | authoring side graph, UI resets, decal modal, status | authoring-graph-sensitive wall routes | clears warning, mutates authoring side, refresh path inside authoring helper, may rebuild preview | picker temporary topology route wraps into this | future `MaterialEditBridge` | Needs service first | High | Preserves side `wall/lower/upper/middle` writeback from edited topology. |
| `ApplyAuthoringSideMaterialAction()` | `SectorEditor.cpp:9103-9135` | wall copy/paste, UV, decal, fit/align/clear | copied temp topology, status | authoring side via derived topology mapping | finish route handles dirty/cache/preview | no direct picker | future `MaterialEditBridge` | Needs service first | High | Requires current valid derived topology and side mapping. |
| `ApplyAuthoringFaceAnchorFlatMaterialAction()` | `SectorEditor.cpp:9137-9196` | floor/ceiling material edits under authoring graph | selected 3D surface fallback, authoring face anchor, UI resets, status | authoring flat/preview-surface-sensitive | authoring helper handles graph edit; may rebuild preview | picker temporary topology flat route wraps into this | future `MaterialEditBridge` | Needs service first | High | Also supports non-selected flat target by synthesizing `SectorSurfaceRef`. |
| `ApplySurface3DUvValue()` | `SectorEditor.cpp:9270-9316` | `DrawPreviewUvPanel()` UV fields | selected surface kind, topology/authoring material fields | preview-3D-surface-sensitive and authoring-sensitive | via material finish wrappers | no direct picker | future `MaterialEditBridge`; UI stays preview panel | Defer | Medium/High | Needs preview surface target semantics before moving panel. |
| `ResetSurface3DUv()` | `SectorEditor.cpp:9466-9492` | `DrawPreviewUvPanel()` reset button | selected surface kind, UI reset result | preview-3D-surface-sensitive and authoring-sensitive | via material finish wrappers | no direct picker | future `MaterialEditBridge` | Defer | Medium/High | Shares same target path as UV values. |
| `ApplySurfaceDecalTint()` | `SectorEditor.cpp:9366-9388` | decal tint modal apply, preview panel/inspector modal route | decal tint fields | topology/authoring/preview | via material finish wrappers; closes modal through result | no direct picker | future `MaterialEditBridge` | Needs service first | Medium | Keep modal UI separate. |
| `OpenDecalTintModal()` | `SectorEditor.cpp:9416-9426` | tools/materials and preview UV panel | `state.decalTintModal`, status | topology lookup for current target | no dirty until apply | no picker | modal helper or `MaterialEditBridge` facade | Defer | Low/Medium | Opening is not mutation; can remain callback. |
| material wrapper group | `SectorEditor.cpp:9198-9586` | SideDef inspector, sector inspector, preview UV panel, picker post-apply | copied material, selected surface kind, authoring graph, status | topology, authoring, preview | all mutation wrappers finish through material finish routes | indirectly through picker apply | future `MaterialEditBridge` | Needs service first | High | Includes copy/paste, UV, decal opacity/emissive/bloom/tint, clear, fit, align. |
| `DrawTopologySideDefInspector()` wrapper | `SectorEditor.cpp:7348-7420` | extracted `tools/materials` inspector | callback lambdas, state/ui refs | topology or authoring by wrappers | callbacks call finish/mark routes | opens side picker | stay thin wrapper until bridge exists | Defer | Medium | Current callback fanout is the visible REF-055 seam. |
| `DrawPreviewUvPanel()` | `SectorEditor.cpp:4203-4689` | 3D selected surface material editing | selected surface refs, active layer, UI inputs, picker flags | preview-3D-surface-specific, topology/authoring mutation | uses material wrappers and sets `rebuildPreviewOnApply` | opens sector/sidedef picker | future preview surface tool after bridge | Defer | High | Should wait for MaterialEditBridge or preview surface service. |
| `ApplyTexturePickerSelection()` wrapper | `SectorEditor.cpp:9743-9941` | texture picker modal apply | picker, runtime objects, status, topology map, authoring graph | material and non-material | door uses runtime mutation; authoring/material paths may rebuild preview and finish material mutation | main picker-specific switch | split material picker routing first | Move next | High | Contains temporary topology rollback/apply for authoring material picker routes. |
| `ApplyTexturePickerSelection()` helper | `SectorEditorTextureActions.cpp:917-956` | generic target apply after modal selection | topology map, preview sky draft, runtime door | topology/sky/runtime | returns `changed`, status, material-finish flag | picker-specific | keep non-material; extract material branches | Move next for material only | Medium/High | MapSky and RuntimeDoor must not move to material module. |
| `ApplyAuthoringTexturePickerSelection()` | `SectorEditorTextureActions.cpp:958-1190` | direct authoring picker targets | authoring face/side graph | authoring-graph-sensitive | authoring mutation helpers mark graph edited/refresh derived topology | picker-specific | material picker route or bridge later | Move next, scoped | High | Has direct ID and topology-mapped authoring paths. |
| `CurrentTextureForPickerTarget()` | `SectorEditorTextureActions.cpp:548-665` | picker open/current preview | sky, door, side, sector, authoring face/side fields | mixed material and non-material | no dirty | picker-specific | split material current-target helper | Move next | Medium | Keep sky/door cases outside material route. |
| `OpenTopologyTexturePicker()` | `SectorEditorTextureActions.cpp:667-692`, wrapper `SectorEditor.cpp:9689-9700` | sector/floor/ceiling/default material pickers | picker state | topology-material-only unless wrapper routes authoring | no dirty until apply | picker-specific | material picker route | Move next | Medium | Decal field eligibility guard belongs with material picker route. |
| `OpenTopologySideDefTexturePicker()` | `SectorEditorTextureActions.cpp:694-721`, wrapper `SectorEditor.cpp:9702-9713` | side/wall picker | picker state | topology-material-only unless wrapper routes authoring | no dirty until apply | picker-specific | material picker route | Move next | Medium | Middle forces base layer. |
| `OpenAuthoringFaceAnchorTexturePicker*()` | `SectorEditorTextureActions.cpp:723-794` | authoring face/material picker | picker state, authoring mapping | authoring-graph-sensitive | no dirty until apply | picker-specific | material picker route | Move next | High | Includes direct face-anchor ID variant. |
| `OpenAuthoringSideTexturePicker*()` | `SectorEditorTextureActions.cpp:796-870` | authoring side/material picker | picker state, authoring mapping | authoring-graph-sensitive | no dirty until apply | picker-specific | material picker route | Move next | High | Includes direct side ID variant. |
| `SectorEditorMaterialActions.*` helpers | `SectorEditorMaterialActions.cpp:1-1104` | pure material operations | passed `SectorTopologyMap&` only | topology-material-only operation kernels | return `SectorEditorMaterialActionResult`; no dirty/cache themselves | no picker | keep as backend helpers | Keep | Low | Good existing boundary; bridge should call these rather than absorb them. |
| `tools/materials` callbacks | `SectorEditorMaterialInspector.h:13-30`, `.cpp:269-721` | SideDef inspector UI | callbacks into editor wrappers | topology UI with authoring-sensitive callbacks | external callbacks own finish behavior | opens side picker | reduce through bridge later | Defer | Medium | The callback count is a symptom, but picker route extraction is safer first. |

## tools/materials Callback Inventory

### Picker Opening

| Callback | Current `SectorEditor.cpp` target | Behavior category | TexturePickerService? | MaterialEditBridge? | Remain direct? | Risk | Follow-up |
|---|---|---|---|---|---|---|---|
| `openSideDefTexturePicker` | `OpenTopologySideDefTexturePicker()` | picker opening/material target setup | No, service only opens generic modal once target is configured | Yes, after material picker route exists | Short term yes | Medium | REF-063 can replace with material picker route facade. |

### Material Mutation/Action

| Callback | Current target | Behavior category | TexturePickerService? | MaterialEditBridge? | Remain direct? | Risk | Follow-up |
|---|---|---|---|---|---|---|---|
| `copyTopologyMaterial` | `CopyTopologyMaterial()` | material clipboard | No | Yes | Short term yes | Low | Bridge API can expose copy. |
| `pasteTopologyMaterial` | `PasteTopologyMaterial()` | material mutation/action | No | Yes | No, after bridge | Medium/High | Preserve authoring wall/flat routes. |
| `applyDecalOpacity` | `ApplySurfaceDecalOpacity()` | decal mutation | No | Yes | No, after bridge | Medium | Preserve reset flags and preview rebuild. |
| `applyDecalEmissive` | `ApplySurfaceDecalEmissive()` | decal mutation | No | Yes | No, after bridge | Medium | Same as opacity. |
| `applyDecalBloomIntensity` | `ApplySurfaceDecalBloomIntensity()` | decal mutation | No | Yes | No, after bridge | Medium | Same as opacity. |
| `fitSelectedDecal` | `FitSelectedDecal()` | material action | No | Yes | No, after bridge | Medium | Authoring flat/wall split matters. |
| `clearSurfaceDecal` | `ClearSurfaceDecal()` | material action | No | Yes | No, after bridge | Medium | Reset decal inputs. |
| `clearMiddleTexture` | `ClearMiddleTexture()` | material action | No | Yes | No, after bridge | Medium | Only wall/middle authoring route. |
| `fitSelectedWallMaterial` | `FitSelectedWallMaterial()` | material action | No | Yes | No, after bridge | Medium | Wall-only authoring route. |
| `alignSelectedWallMaterialVertical` | `AlignSelectedWallMaterialVertical()` | material action | No | Yes | No, after bridge | Medium | Wall-only authoring route. |
| `alignSelectedWallMaterialU` | `AlignSelectedWallMaterialU()` | material action | No | Yes | No, after bridge | Medium | Wall-only authoring route. |

### Direct Topology Edit Finish

| Callback | Current target | Behavior category | TexturePickerService? | MaterialEditBridge? | Remain direct? | Risk | Follow-up |
|---|---|---|---|---|---|---|---|
| `finishTopologyMaterialMutation` | `FinishTopologyMaterialMutation()` | dirty/cache/preview finish | No | Yes | No, after bridge | High | Bridge must preserve warning clear, dirty/cache invalidation, preview rebuild. |
| `markTopologyDocumentEdited` | `MarkTopologyDocumentEdited()` | direct topology edit finish | No | Maybe as callback into bridge | Yes for non-material direct edits | Medium | Direct inspector UV edits should be converted carefully, not hidden in picker service. |

### Selection/Portal Flags

| Callback | Current target | Behavior category | TexturePickerService? | MaterialEditBridge? | Remain direct? | Risk | Follow-up |
|---|---|---|---|---|---|---|---|
| `selectSideDef` | `SelectTopologySideDef()` | selection | No | No | Yes | Low | Selection service/inspector concern. |
| `setLineDefBlocksPlayer` | `SetLineDefBlocksPlayer()` | topology flag, portal behavior | No | No | Yes | Medium | Not material edit; keep outside bridge. |

### Modal/Tint

| Callback | Current target | Behavior category | TexturePickerService? | MaterialEditBridge? | Remain direct? | Risk | Follow-up |
|---|---|---|---|---|---|---|---|
| `openDecalTintModal` | `OpenDecalTintModal()` | modal/tint open | No | Maybe narrow bridge facade | Yes for now | Low/Medium | Actual tint apply belongs in bridge; modal UI can remain separate. |

### Status/Dirty/Cache

Status is implicit through wrappers and finish callbacks. `tools/materials` still directly clears `state.topologyRenderWarning` before a few `markTopologyDocumentEdited()` calls at `SectorEditorMaterialInspector.cpp:458` and `:626`; future bridge work should remove those direct state edits or keep them explicitly paired with the same invalidation path.

## Texture Picker Material Branch Review

Material-specific target kinds:

- `Sector`
- `SideDef`
- `AuthoringFaceAnchor`
- `AuthoringSide`

Non-material target kinds:

- `MapSky`
- `RuntimeDoor`
- sprite picker is separate and should not enter this path

Branches that could move to a material picker route:

- `CurrentTextureForPickerTarget()` side/sector/authoring cases at `SectorEditorTextureActions.cpp:561-665`
- `OpenTopologyTexturePicker()` and `OpenTopologySideDefTexturePicker()` at `:667-721`
- `OpenAuthoringFaceAnchorTexturePicker*()` at `:723-794`
- `OpenAuthoringSideTexturePicker*()` at `:796-870`
- `ApplyTexturePickerSelection()` material assignment at `:952`
- `ApplyAuthoringTexturePickerSelection()` at `:958-1190`
- `SectorEditor::ApplyTexturePickerSelection()` authoring/material rollback and finish handling at `SectorEditor.cpp:9778-9940`

Branches that must stay with other owners:

- `MapSky` current/open/apply in texture actions and preview settings modal ownership.
- `RuntimeDoor` open/current/apply and `SectorEditor::ApplyTexturePickerSelection()` runtime object mutation.
- add-map texture modal/import/catalog behavior.
- billboard sprite picker behavior.

Temporary topology rollback/apply for authoring material picker routes is in `SectorEditor::ApplyTexturePickerSelection()` at `SectorEditor.cpp:9796-9925`. It snapshots `state.topologyMap`, lets `game::ApplyTexturePickerSelection(state)` write to topology, restores the original topology, then commits the edited wall/flat material through authoring finish routes.

This can be split safely now if the first slice only extracts material target classification/open/current/apply routing and leaves dirty/cache/preview policy callback-based. The minimal target/result object should consume:

- a copied `TexturePickerState` or narrow material target `{kind, layer, sectorId, field, sideDefId, wallPart, authoring ids, authoringSurface3DFlatTarget, rebuildPreviewOnApply}`
- selected texture id from `TexturePickerService`
- `SectorEditorTexturePickerApplyResult`
- callbacks for material finish, authoring side finish, authoring flat finish, preview rebuild request, status write, and picker close

`TexturePickerService` should remain generic. Do not put material dirty/cache/preview policy inside it.

## MaterialEditBridge Boundary

`MaterialEditBridge` should own material edit orchestration, not UI layout. It can centralize:

- topology material finish behavior
- material action result handling
- topology-vs-authoring-vs-preview surface routing
- material picker apply routing after REF-063, or common material action wrappers after that
- UI reset flag application
- decal tint apply/close behavior

It must not own:

- texture picker modal UI/catalog/options
- renderer internals or mesh generation
- lightmap bake logic
- door, sky, sprite, or add-map texture behavior
- inspector layout
- selection service behavior

Likely API candidates:

```cpp
struct MaterialEditBridge {
    bool FinishTopologyMaterialMutation(const char* status, engine::AssetManager* assets);
    bool FinishMaterialActionResult(const SectorEditorMaterialActionResult& result, engine::AssetManager* assets);
    bool ApplyAuthoringSideMaterialAction(TopologySurfaceEditTarget target, engine::AssetManager* assets, MaterialActionFn action);
    bool ApplyAuthoringFaceAnchorFlatMaterialAction(TopologySurfaceEditTarget target, engine::AssetManager* assets, MaterialActionFn action);
    bool ApplySurfaceUvValue(TopologySurfaceEditTarget target, TopologyMaterialLayer layer, SectorSurfaceKind surfaceKind, int component, float value, engine::AssetManager* assets);
    bool ApplySurfaceDecalTint(TopologySurfaceEditTarget target, Vector3 tint, engine::AssetManager* assets);
    bool ApplyMaterialTexturePickerSelection(const TexturePickerState& picker, const std::string& textureId, engine::AssetManager* assets);
};
```

Placement should likely be `sources/sector_editor/services/material_edit/`, not `tools/materials/`, because the bridge would serve SideDef inspector, sector inspector, preview UV panel, and picker routes. It may depend on `SectorEditorState` and `SectorEditorUiState` directly at first to avoid a large context object, but preview rebuild and document dirty marking should be callbacks so renderer/editor lifecycle ownership stays central. It should not include `AssetManager` as an owned dependency; pass `engine::AssetManager*` only for rebuild eligibility. It may know about authoring graph derivation at the orchestration level, but should call existing authoring helpers rather than duplicating derivation logic. Keep it narrow by moving one family of routes at a time.

## Dirty/Cache/Preview Rebuild Guardrails

| Guardrail | Current route | Why it matters | Future requirement | Smoke/test implication |
|---|---|---|---|---|
| `MarkTopologyDocumentEdited()` | `SectorEditor.cpp:2423-2431` | sets dirty/unsaved, invalidates 2D cache, updates status | every topology material mutation must still call it or equivalent | edit material in 2D and verify cache/status update |
| `FinishTopologyMaterialMutation()` | `SectorEditor.cpp:2433-2444` | clears warning, marks dirty, optional preview rebuild | bridge must preserve exact conditions | edit material in Preview3D and verify mesh refresh |
| `FinishMaterialActionResult()` | `SectorEditor.cpp:2446-2486` | applies UI reset flags and closes decal modal | no moved action may bypass resets | change/reset UV and decal fields; inputs reset |
| authoring graph edited/derived refresh | authoring helpers in `SectorEditorAuthoringState.cpp`, wrapper finish at `SectorEditor.cpp:9029-9196` | material edits must write back to authoring graph when graph exists | keep existing helper calls; do not write topology only | edit authoring side/face material and reselect/derive |
| `RebuildPreviewMeshesPreservingView()` | `SectorEditor.cpp:9588+`, called from finish routes | generated meshes are cached; rebuild only on edits | preserve `assets != nullptr`, Preview3D, renderer-ready checks | picker apply in Preview3D should update visible surface |
| `topologyRenderWarning` clearing | `SectorEditor.cpp:2435`, `:9078`, `:9932`, inspector direct clears | stale warnings confuse validation feedback | material mutation finish must clear warning | make material edit after warning and verify warning clears |
| UI reset flags | `SectorEditorMaterialActionResult` handled in finish wrappers | stale input buffers cause wrong displayed/edit values | bridge must centralize resets | UV/decal input smoke |
| decal tint modal close/reset | `FinishMaterialActionResult()` and authoring finish routes | modal must close after successful apply | preserve `closeDecalTintModal` | open/apply tint in topology and authoring modes |
| picker `rebuildPreviewOnApply` | `TexturePickerState::rebuildPreviewOnApply`, set by preview panel | preview surface picker needs rebuild; 2D does not always | material picker route must return/consume this flag | apply picker from 3D panel |
| source-hash-sensitive topology fields | `SectorLightmap.cpp:2491-2495`, `:3796-3860` | base texture IDs and UVs affect lightmap hash | material changes must remain topology/authoring data changes, not visual-only settings | rebake/stale-lightmap smoke after material edit |

Lightmap source hash currently includes wall part base `textureId` and `uv`, sector floor/ceiling texture IDs and UVs, default wall/lower/upper parts, geometry, lights, bake settings, and `ceilingSky`. `FnvAppendTopologyWallPart()` does not include decal fields, so decal-only texture/tint/opacity changes do not appear source-hash-sensitive unless the hash policy changes.

## Preview UV Panel Dependency

`DrawPreviewUvPanel()` shares material wrappers with `tools/materials`: copy/paste, UV value/reset, clear middle, clear decal, fit decal, fit/align wall material, decal opacity/emissive/bloom/tint, and texture picker open. It also shares `OpenTopologyTexturePicker()` and `OpenTopologySideDefTexturePicker()`, then adds `authoringSurface3DFlatTarget` and `rebuildPreviewOnApply`.

Preview-specific parts are selected 3D surface validation, selected topology target mapping, panel geometry, mouse event consumption, portal blocks-player toggle, and surface-kind-specific UV writes. It should wait until `MaterialEditBridge` exists before moving because its behavior is not only material UI; it depends on preview surface target resolution and preview mesh rebuild policy.

A separate preview surface target resolver may be useful later, but it should follow REF-060. Do not move `DrawPreviewUvPanel()` as part of REF-063.

## Recommended Next Implementation Task

Choose: **REF-063: Extract material-specific picker routing from `ApplyTexturePickerSelection()`**.

Exact scope:

- Add a narrow material picker routing helper/module, probably under `sources/sector_editor/services/material_edit/` or `sources/sector_editor/tools/materials/` only if it has no preview/sector-inspector dependency.
- Move material target classification, material current-texture lookup, material picker open helpers, and material picker apply routing.
- Preserve `TexturePickerService` as generic lifecycle/result mechanics.
- Preserve `SectorEditor.cpp` finish wrappers for now.
- Keep door, sky, sprite, add-map texture, texture catalog/import, and modal drawing outside the new route.
- Preserve authoring temporary topology rollback/apply semantics exactly.

Expected reduction: about 120-220 lines from `SectorEditor.cpp`/`SectorEditorTextureActions.cpp`; callback reduction is likely 0-2 in this slice. The value is reducing picker branch risk before moving material finish callbacks.

Risks:

- authoring side/face picker routes silently writing topology instead of authoring graph
- missing `rebuildPreviewOnApply`
- moving sky/door branches accidentally
- changing current texture display for direct authoring targets

Verification:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- manual picker/material smoke

Manual smoke checklist:

- 2D SideDef base/decal texture picker applies and invalidates cache.
- 2D sector floor/ceiling/default material picker applies.
- Authoring side picker applies to authoring graph and derived topology remains current.
- Authoring face-anchor picker applies to authoring graph.
- Preview3D surface picker rebuilds preview meshes and preserves view.
- Door texture picker still updates doors only.
- Sky texture picker still updates preview settings draft only.
- Missing/unselected picker choice closes or reports status as before.

## Recommended Follow-Up Tasks

- REF-064: Add minimal `MaterialEditBridge` for material action wrappers and finish behavior.
- REF-065: Move `tools/materials` material mutation callbacks onto `MaterialEditBridge`.
- REF-060: Audit Preview UV/material panel dependencies before moving `DrawPreviewUvPanel()`.
- REF-066: Extract preview UV/material panel only after bridge and preview surface target boundaries are clear.
- REF-058: Audit AssetCatalog/TextureCatalog if picker/catalog coupling blocks further routing cleanup.

## Backlog Update

REF-059 already existed and was marked complete by this task. Completion notes point to this report and recommend REF-063 as the next implementation. REF-044 remains open as the umbrella because material migration is incomplete. REF-058 and REF-060 remain open because this task did not perform those audits. Future implementation items were added/refined but not marked complete.

## Appendix: Evidence

Commands used:

```sh
rg -n "FinishTopologyMaterialMutation|FinishMaterialActionResult|FinishAuthoringSideMaterialActionResult|ApplyAuthoringSideMaterialAction|ApplyAuthoringFaceAnchorFlatMaterialAction|ApplySurface3DUvValue|ResetSurface3DUv|ApplySurfaceDecalTint|OpenDecalTintModal|CopyTopologyMaterial|PasteTopologyMaterial|ClearSurfaceDecal|ClearMiddleTexture|FitSelected|AlignSelected|TopologyMaterial|MaterialActionResult" sources/sector_editor
rg -n "ApplyTexturePickerSelection|ApplyAuthoringTexturePickerSelection|CurrentTextureForPickerTarget|OpenTopologyTexturePicker|OpenTopologySideDefTexturePicker|OpenAuthoringFaceAnchorTexturePicker|OpenAuthoringSideTexturePicker|OpenMapSkyTexturePicker|OpenRuntimeDoorTexturePicker|TexturePickerTarget|TexturePickerState|TexturePickerService|rebuildPreviewOnApply" sources/sector_editor
rg -n "SectorEditorMaterialInspectorCallbacks|DrawTopologySideDefMaterialInspector|openSideDefTexturePicker|finishTopologyMaterialMutation|markTopologyDocumentEdited|openDecalTintModal|applyDecal|fitSelected|alignSelected|clearMiddle|clearSurfaceDecal" sources/sector_editor/tools/materials sources/sector_editor/SectorEditor.cpp
rg -n "DrawPreviewUvPanel|selectedSurface3D|selectedTopologySurface3D|ApplySurface3D|ResetSurface3DUv|PreviewUv|Surface3D|RebuildPreviewMeshesPreservingView" sources/sector_editor
rg -n "MarkTopologyDocumentEdited|InvalidateTopologyRenderCache|topologyRenderWarning|MarkSectorEditorAuthoringGraphEdited|RefreshSectorEditorAuthoringDerivedTopology|sourceHash|ComputeSectorLightmapSourceHash|FnvAppendTopologyWallPart" sources/sector_editor sources/sector_demo
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditorTextureActions.cpp sources/sector_editor/SectorEditorMaterialActions.cpp sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp
```

Selected grep results and line ranges:

- `FinishTopologyMaterialMutation()`: `SectorEditor.cpp:2433-2444`
- `FinishMaterialActionResult()`: `SectorEditor.cpp:2446-2486`
- `DrawPreviewUvPanel()`: `SectorEditor.cpp:4203-4689`
- `tools/materials` wrapper callback construction: `SectorEditor.cpp:7348-7420`
- authoring material finish/apply routes: `SectorEditor.cpp:9029-9196`
- material wrapper group: `SectorEditor.cpp:9198-9586`
- texture picker wrappers/apply: `SectorEditor.cpp:9650-9941`
- `CurrentTextureForPickerTarget()`: `SectorEditorTextureActions.cpp:548-665`
- material picker open helpers: `SectorEditorTextureActions.cpp:667-870`
- generic apply helper: `SectorEditorTextureActions.cpp:917-956`
- authoring picker apply helper: `SectorEditorTextureActions.cpp:958-1190`
- `TexturePickerState` target kinds/fields: `SectorEditorModalTypes.h:20-54`
- `TexturePickerService` generic callbacks: `SectorEditorTexturePickerService.h:14-53`
- lightmap wall part hash: `SectorLightmap.cpp:2491-2495`
- lightmap source hash topology material fields: `SectorLightmap.cpp:3796-3860`

Callback inventory:

- `selectSideDef`
- `setLineDefBlocksPlayer`
- `openSideDefTexturePicker`
- `copyTopologyMaterial`
- `pasteTopologyMaterial`
- `applyDecalOpacity`
- `applyDecalEmissive`
- `applyDecalBloomIntensity`
- `openDecalTintModal`
- `fitSelectedDecal`
- `clearSurfaceDecal`
- `clearMiddleTexture`
- `fitSelectedWallMaterial`
- `alignSelectedWallMaterialVertical`
- `alignSelectedWallMaterialU`
- `markTopologyDocumentEdited`
- `finishTopologyMaterialMutation`

Picker branch inventory:

- Material: `Sector`, `SideDef`, `AuthoringFaceAnchor`, `AuthoringSide`
- Non-material: `MapSky`, `RuntimeDoor`
- Separate: billboard sprite picker

Line counts at audit time:

```text
  9943 sources/sector_editor/SectorEditor.cpp
  1192 sources/sector_editor/SectorEditorTextureActions.cpp
  1104 sources/sector_editor/SectorEditorMaterialActions.cpp
   727 sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp
    61 sources/sector_editor/services/texture_picker/SectorEditorTexturePickerService.cpp
 13027 total
```

Service placement recommendation: start material routing under `services/material_edit/` if the route serves inspector, sector, preview, and picker callers. Use `tools/materials/` only for UI-local inspector code.

Stale backlog notes found: REF-020 remains a broad "texture/material action or inspector" item and should continue to defer to the REF-059/REF-063/REF-064 service sequence. REF-044 remains correctly open as the umbrella.
