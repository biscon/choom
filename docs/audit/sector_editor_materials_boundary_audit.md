# SectorEditor Materials/Sidedef/Decal Boundary Audit

## Summary

- Materials/sidedef/decal is the next extraction target because `DrawTopologySideDefInspector()` is the largest coherent remaining editor UI block after the tool migrations, and REF-053 identified it as the best line-count win that does not require moving renderer, lightmap, or collision ownership.
- Current estimated cluster size: `DrawTopologySideDefInspector()` is about 702 lines at `SectorEditor.cpp:7334-8035`; related material wrappers are about 560 lines at `9636-10193`; texture picker routing is about 294 lines at `10257-10549`. The implementation should not move all of that at once.
- Recommended REF-055 shape: add `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.h/.cpp`, move the sidedef inspector body and inspector-only helper lambdas into a free function that receives state/UI references plus narrow callbacks, and leave broad material mutation wrappers and broad texture picker apply routing in `SectorEditor.cpp` initially.
- Top 5 risks: breaking topology cache invalidation, changing preview rebuild behavior, splitting authoring graph rollback/apply behavior incorrectly, losing decal tint modal state/reset behavior, and changing UI status/validation strings.
- Expected REF-055 `SectorEditor.cpp` reduction: roughly 600-740 lines if only the sidedef inspector body plus small wrapper call are moved; more should wait for follow-up tasks.

## Scope And Method

Inspected files:

- `docs/audit/sector_editor_post_tool_migration_line_map.md`
- `docs/audit/sector_editor_tool_module_boundary_design.md`
- `docs/audit/sector_editor_cpp_seams_audit.md`
- `docs/plans/codebase_refactor_backlog.md`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditorMaterialActions.h`
- `sources/sector_editor/SectorEditorMaterialActions.cpp`
- `sources/sector_editor/SectorEditorMaterialModals.h`
- `sources/sector_editor/SectorEditorMaterialModals.cpp`
- `sources/sector_editor/SectorEditorTextureModals.h`
- `sources/sector_editor/SectorEditorTextureActions.cpp`
- `sources/sector_editor/SectorEditorSurfaceTypes.h`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_editor/SectorEditorUiHelpers.h/.cpp`
- `sources/sector_editor/SectorEditorHelpers.h/.cpp`
- `sources/sector_editor/SectorEditorAuthoringState.h/.cpp`
- `sources/sector_editor/selection/`
- `sources/sector_editor/tools/`
- `sources/sector_demo/SectorTopologyMap.*`
- `sources/sector_demo/SectorTopologyTypes.h`
- `sources/sector_demo/SectorMeshTypes.*`
- `sources/sector_demo/SectorLightmap.*`

Commands used are listed in the appendix. This was static analysis only. The editor was not run, no GUI/render smoke was performed, and no behavior equivalence was manually verified.

## Current Materials Cluster Inventory

| Function/block | Current file/range | Responsibility | State touched | Mutation/cache/preview behavior | REF-055 recommendation | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| `DrawTopologySideDefInspector()` | `SectorEditor.cpp:7334-8035` | Sidedef/linedef inspector UI, wall part tabs, material layer tabs, texture picker button, UV rows, decal controls, copy/paste/fit/align/clear buttons | `state.selectedTopologyWallPart`, `state.activeTopologyMaterialLayer`, `uiState.topologySideDefUvInputs`, `uiState.topologySideDefDecal*`, `statusText`, selected sidedef/wall material fields | Mixes direct topology UV mutation with calls to material wrappers and picker/tint open callbacks | Move | Main REF-055 target. Preserve exact strings and finish calls. |
| Material wrapper group | `SectorEditor.cpp:9805-10193` | Bridges UI actions to `SectorEditorMaterialActions.*`, choosing topology vs authoring side vs authoring flat route | `state.topologyMap`, `state.copiedTopologyMaterial`, `selectedSurface3D`, UI input reset state | Uses `FinishMaterialActionResult()`, `ApplyAuthoringSideMaterialAction()`, `ApplyAuthoringFaceAnchorFlatMaterialAction()`; may rebuild preview | Stay initially | Expose as callbacks to new inspector; cleanup later. |
| `FinishTopologyMaterialMutation()` | `SectorEditor.cpp:2431-2442` | Central finish path for topology material changes that require preview mesh rebuild in 3D | dirty flags, render warning, cache, preview renderer | Clears `topologyRenderWarning`, calls `MarkTopologyDocumentEdited()`, rebuilds preview when active/ready | Stay | Required callback for direct middle UV edits. |
| `FinishMaterialActionResult()` | `SectorEditor.cpp:2444-2484` | Applies material action result side effects | UI input resets, decal modal state, status, dirty/cache, preview rebuild | Calls `FinishTopologyMaterialMutation()` when changed | Stay | Do not duplicate in material module for REF-055. |
| `FinishAuthoringSideMaterialActionResult()` | `SectorEditor.cpp:9636-9708` | Writes edited derived topology side material back into authoring line side | authoring graph side materials, UI reset state, status | Clears warning, mutates authoring side, refreshes derived topology through authoring helper, may rebuild preview | Stay | Sensitive authoring mapping path. |
| `ApplyAuthoringSideMaterialAction()` | `SectorEditor.cpp:9710-9742` | Guards authoring side material edits and runs action on topology copy | authoring derivation state, topology copy | Defers final write to `FinishAuthoringSideMaterialActionResult()` | Stay | Expose only through existing wrappers. |
| `ApplyAuthoringFaceAnchorFlatMaterialAction()` | `SectorEditor.cpp:9744-9803` | Routes floor/ceiling material edits to authoring face anchor when graph exists | selected 3D surface fallback, authoring graph, UI reset state | Uses authoring state helper and may rebuild preview | Stay | Shared by sector/3D flat material flows; not sidedef-only. |
| `ApplySurface3DUvValue()` | `SectorEditor.cpp:9877-9923` | Shared 3D surface UV value mutation wrapper | selected 3D surface kind, material target | Topology/authoring route via material finish wrappers | Stay | Used by preview UV panel; not REF-055. |
| `ApplyTexturePickerSelection()` | `SectorEditor.cpp:10350-10549` | Applies texture picker to runtime door, authoring targets, topology material targets, 3D flat authoring fallback | picker state, topology map, authoring graph, status, preview | Contains temporary topology rollback/apply for authoring material routes; may call material finish or mark document | Stay | Too broad and fragile for REF-055. |
| `OpenDecalTintModal()` | `SectorEditor.cpp:10023-10033` | Builds decal tint modal state for a material target | `state.decalTintModal`, `statusText` | No dirty/cache until modal OK applies tint | Stay as callback | Inspector should call callback. |
| `ApplySurfaceDecalTint()` | `SectorEditor.cpp:9973-9995` | Applies decal tint through topology/authoring material route | topology/authoring material, modal close via result | Calls material finish wrappers | Stay | Modal already uses callback in `DrawDecalTintModal()`. |
| `OpenTopologyTexturePicker()` | `SectorEditor.cpp:10296-10307` | Opens sector/authoring face picker depending on graph presence | picker state, status | No mutation until apply | Stay initially | Sector/flat scope is broader than sidedef inspector. |
| `OpenTopologySideDefTexturePicker()` | `SectorEditor.cpp:10309-10320` | Opens sidedef/authoring side picker depending on graph presence | picker state, status | No mutation until apply | Could move later; callback in REF-055 | Good follow-up once inspector module owns material picker routing. |
| `CurrentTextureForPickerTarget()` | `SectorEditor.cpp:10257-10260` plus texture action `565-682` | Returns current texture for modal highlight | picker state and map/authoring graph | Read-only | Stay | Generic picker callback. |
| Decal tint modal wrapper | `SectorEditor.cpp:8167-8196` | Builds `SectorEditorDecalTintModalContext` callbacks | modal state, status, material wrappers | Applies tint through callback, closes modal | Stay | Draw body is already in `SectorEditorMaterialModals.cpp:9-182`. |
| `DrawPreviewUvPanel()` | `SectorEditor.cpp:4201-4688` | 3D selected surface material/UV panel | `selectedSurface3D`, `selectedTopologySurface3D`, surface UI inputs, picker state | Uses same material wrappers and preview rebuild semantics | Later | Do not move in REF-055. |

## DrawTopologySideDefInspector Breakdown

| Block | Current range | What it does | Target recommendation | Callback needs | Risk |
| --- | --- | --- | --- | --- | --- |
| Selected line/sidedef validation and title | `7334-7410` | Validates selected linedef, selected sidedef, endpoints; draws title and endpoint text | Move into inspector module | read selected line/sidedef helpers or direct state/map access | Low |
| Retired operation/status text | `7412-7469`, `7483-7543` | Draws retired split/join messages and line-only guidance | Move | none beyond status strings | Low, strings must remain unchanged |
| Opposite side switch | `7512-7530` | Switches selected sidedef to opposite side and updates status | Move with callback `selectTopologySideDef` | `SelectTopologySideDef()` callback, `statusText` | Medium selection/UI reset behavior |
| Portal `Blocks Player` option | `7545-7561` | Toggles portal line `blocksPlayer` | Move with callback | `setLineDefBlocksPlayer` | Medium; this can affect collision through existing path |
| Texture picker row lambda | `7563-7582` | Draws current texture status and picker button | Move | `openSideDefTexturePicker`, texture lookup in map | Medium picker target route |
| Wall part tabs | `7584-7611` | Chooses Wall/Lower/Upper/Middle and resets UV inputs | Move | mutable `state.selectedTopologyWallPart`, UI state, status | Low |
| Material layer tabs | `7613-7667` | Forces middle to base, toggles Base/Decal, resets UV/decal inputs | Move | mutable `state.activeTopologyMaterialLayer`, UI reset fields | Low/Medium |
| Selected material target construction | `7622-7627` | Builds `TopologySurfaceEditTarget` for wall part | Move | none | Low |
| Texture status and missing/empty text | `7673-7704` | Shows current texture, "No middle texture assigned", "No decal assigned" | Move | texture registry read, `clearMiddleTexture` callback for non-default empty middle | Low |
| Copy/paste material buttons | `7706-7732` | Copies/pastes base wall/lower/upper material payload | Move | `copyTopologyMaterial`, `pasteTopologyMaterial` | Medium authoring route on paste |
| UV scale/offset rows | `7734-7811` | Directly edits selected wall/decal UV components; validates finite input | Move with finish callbacks | mutable selected UV, `finishTopologyMaterialMutation`, `markTopologyDocumentEdited`, status | High; currently direct topology mutation |
| Decal opacity/emissive/bloom | `7813-7871` | Edits decal material properties through wrappers | Move | decal action callbacks | Medium; preserve finite behavior and reset state |
| Decal tint control | `7873-7892` | Opens tint modal and draws color swatch | Move | `openDecalTintModal`, `DrawColorSwatch`, scroll offset | Medium modal target validity |
| Fit/clear decal buttons | `7894-7918` | Fits decal UV and clears decal | Move | `fitSelectedDecal`, `clearSurfaceDecal` | Medium authoring route |
| Reset UV | `7920-7944` | Directly resets selected UV and chooses middle vs non-middle finish path | Move | `finishTopologyMaterialMutation`, `markTopologyDocumentEdited` | High; exact finish split must remain |
| Fit width/height/both | `7946-7980` | Fits wall material UV | Move | `fitSelectedWallMaterial` | Medium authoring route |
| Clear middle | `7982-7995` | Clears middle texture | Move | `clearMiddleTexture` | Medium authoring route |
| Align vertical/U prev/U next | `7997-8033` | Aligns wall material UV to neighboring wall geometry | Move | `alignSelectedWallMaterialVertical`, `alignSelectedWallMaterialU` | Medium authoring route |
| Input focus/capture | whole function via UI widgets | Uses normal engine UI widgets; no explicit `keyboardCaptured` handling | Preserve by moving same widget calls | UI/input refs | Low/Medium layout IDs must remain stable |

## Mutation Route Review

| Route | Current function path | Dirty/cache route | Preview rebuild route | Authoring derivation route | Lightmap source-hash relevance | REF-055 guardrail |
| --- | --- | --- | --- | --- | --- | --- |
| Topology sidedef texture via picker | Inspector -> `OpenTopologySideDefTexturePicker()` -> modal -> `ApplyTexturePickerSelection()` -> `game::ApplyTexturePickerSelection()` | `FinishTopologyMaterialMutation()` for middle; otherwise `MarkTopologyDocumentEdited()` | Picker can rebuild when `rebuildPreviewOnApply`; middle uses material finish | If graph exists, routed to authoring side branch | Wall part texture IDs are in lightmap hash; decals are not currently appended by `FnvAppendTopologyWallPart()` | Do not change picker apply path. |
| Direct topology side UV input | Inspector direct `selectedUv.scale/offset = value` | Middle: `FinishTopologyMaterialMutation()`; non-middle: clear warning + `MarkTopologyDocumentEdited()` | Middle rebuilds in 3D; non-middle currently does not force rebuild unless existing behavior does so elsewhere | None in direct topology mode | Wall part base UV is hashed; decal UV is not hashed by current wall-part hash helper | Preserve middle vs non-middle split exactly. |
| Authoring side material wrapper | Inspector -> paste/decal/fit/align/clear wrapper -> `ApplyAuthoringSideMaterialAction()` | `MutateSectorEditorAuthoringSideForTopologySideDef()` marks authoring graph edited and refreshes derived topology | Rebuilds preview if active/ready and assets present | Requires valid current derivation and sidedef-to-authoring-side mapping | Resulting derived topology material contributes according to existing hash code | Use existing wrapper callbacks only. |
| Authoring face flat material wrapper | Preview/sector flat flows -> `ApplyAuthoringFaceAnchorFlatMaterialAction()` | Authoring helper marks graph edited and refreshes derived topology | Rebuilds preview if active/ready and changed | Resolves selected/fallback face anchor | Floor/ceiling texture and UV are hashed; decal data currently not appended in source hash | Leave for later; not a sidedef extraction concern. |
| 3D selected surface UV | `DrawPreviewUvPanel()` -> `ApplySurface3DUvValue()` | Material finish wrappers | Rebuilds through material finish if active/ready | Side or flat authoring routes possible | Same as target material | Do not move preview UV panel in REF-055. |
| Decal tint | Inspector opens modal -> `DrawDecalTintModal()` -> `ApplySurfaceDecalTint()` | Material finish wrappers close modal via result | Rebuilds through material finish if active/ready | Side or flat authoring routes possible | Tint is not currently appended to lightmap source hash | Keep modal implementation and apply wrapper unchanged. |
| Fit/reset/copy/paste/align | Inspector callbacks -> material wrappers or direct reset | Material wrappers or direct reset finish paths | Existing wrapper-dependent behavior | Authoring side where wall target and graph exists | Base wall texture/UV hash relevant; decal data not currently hashed | Do not invent new mutation route or generic material service. |

## Texture Picker Boundary

Material-specific picker targets are `TopologyTexturePickerTargetKind::Sector`, `SideDef`, `AuthoringFaceAnchor`, and `AuthoringSide` when used for material base/decal fields. General picker/catalog/import targets are add-map texture scan/import, texture registry display, `MapSky`, `RuntimeDoor`, and sprite picker flows.

Recommendation:

- Keep generic texture catalog/import/picker modal drawing in `SectorEditorTextureModals.*`.
- Keep add-map texture scan/import and sprite picker logic in texture modules.
- In REF-055, pass `openSideDefTexturePicker(sideDefId, wallPart, layer)` as a callback. Do not move `ApplyTexturePickerSelection()` yet.
- Move material-specific picker open/current/apply routing only in a later task after the inspector extraction proves stable.
- Avoid splitting temporary authoring rollback/apply behavior: `SectorEditor.cpp:10404-10533` snapshots topology before picker apply, applies to topology, restores, then writes back through authoring helpers. That block should remain intact until a dedicated material picker routing extraction.

## Preview UV / 3D Surface Relation

`DrawPreviewUvPanel()` shares material wrapper callbacks with the sidedef inspector, but it is a separate workflow. It validates selected 3D surfaces, resolves current authoring mapping, owns a bottom-panel layout, opens sector/sidedef texture pickers with preview rebuild flags, exposes portal `Blocks Player`, and calls `ApplySurface3DUvValue()` / `ResetSurface3DUv()`.

Recommendation:

- Do not move `DrawPreviewUvPanel()` in REF-055.
- Preserve shared mutation wrappers in `SectorEditor.cpp` for now.
- Share callback names/semantics where useful: `openTexturePicker`, `applyUvValue`, `resetUv`, `clearMiddleTexture`, decal actions, and preview rebuild-sensitive material wrappers.
- Plan a later task to decide whether 2D sidedef and 3D surface material UI should unify behind one material UI helper. That task should also handle `selectedSurface3D` UI input reset behavior and picker `rebuildPreviewOnApply`.

## Proposed REF-055 Context/API

Target files:

- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.h`
- `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp`

Proposed function/context:

```cpp
struct SectorEditorMaterialInspectorCallbacks {
    std::function<void(int, TopologyWallPart)> selectTopologySideDef;
    std::function<bool(int, bool)> setLineDefBlocksPlayer;
    std::function<void(int, TopologyWallPart, TopologyMaterialLayer)> openSideDefTexturePicker;
    std::function<bool(TopologySurfaceEditTarget)> copyTopologyMaterial;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager&)> pasteTopologyMaterial;
    std::function<bool(TopologySurfaceEditTarget, float, engine::AssetManager*)> applyDecalOpacity;
    std::function<bool(TopologySurfaceEditTarget, bool, engine::AssetManager*)> applyDecalEmissive;
    std::function<bool(TopologySurfaceEditTarget, float, engine::AssetManager*)> applyDecalBloomIntensity;
    std::function<bool(TopologySurfaceEditTarget)> openDecalTintModal;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager*)> fitSelectedDecal;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager*)> clearSurfaceDecal;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager*)> clearMiddleTexture;
    std::function<bool(TopologySurfaceEditTarget, TopologyUvFitMode, engine::AssetManager*, TopologyMaterialLayer)> fitSelectedWallMaterial;
    std::function<bool(TopologySurfaceEditTarget, engine::AssetManager*, TopologyMaterialLayer)> alignSelectedWallMaterialVertical;
    std::function<bool(TopologySurfaceEditTarget, TopologyUAlignDirection, engine::AssetManager*, TopologyMaterialLayer)> alignSelectedWallMaterialU;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<bool(const char*, engine::AssetManager*)> finishTopologyMaterialMutation;
};

struct SectorEditorMaterialInspectorContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;
    engine::UIScrollAreaResult scroll;
    float contentW = 0.0f;
    float rowH = 0.0f;
    float gap = 0.0f;
    SectorEditorState& state;
    SectorEditorUiState& uiState;
    std::string& statusText;
    const SectorEditorMaterialInspectorCallbacks& callbacks;
};

bool DrawTopologySideDefMaterialInspector(SectorEditorMaterialInspectorContext& context);
```

Required for REF-055: state/UI/status refs, UI/input/asset/font refs, selection callback, portal flag callback, texture picker open callback, copy/paste callbacks, decal action callbacks, wall fit/align callbacks, and the two direct mutation finish callbacks.

Optional/follow-up: callbacks for `CurrentTextureForSurface()`, `ApplySurface3DUvValue()`, `ResetSurface3DUv()`, preview rebuild, generic picker apply, and authoring face flat material actions.

Should not be exposed: `SectorEditor&`, `engineContext`, preview renderer internals, lightmap bake/hash internals, document load/save lifecycle, generic texture catalog/import internals, and authoring graph backend mutation helpers beyond existing callbacks.

The new module must not include `SectorEditor.h`.

## Proposed REF-055 Scope

Move in REF-055:

- `DrawTopologySideDefInspector()` body.
- Inspector-local lambdas currently inside that function: texture row drawing and UV input drawing.
- Material-inspector-only layout constants or tiny helper functions that fall out of the move.
- A small wrapper call in `SectorEditor.cpp` that builds `SectorEditorMaterialInspectorContext` and callbacks.

Keep in `SectorEditor.cpp` initially:

- `FinishTopologyMaterialMutation()`, `FinishMaterialActionResult()`, `FinishAuthoringSideMaterialActionResult()`.
- `ApplyAuthoringSideMaterialAction()` and `ApplyAuthoringFaceAnchorFlatMaterialAction()`.
- Material wrapper methods such as paste, decal properties, clear, fit, align.
- `ApplyTexturePickerSelection()`.
- `DrawPreviewUvPanel()`, preview overlay, and preview renderer/collision orchestration.
- Generic texture picker modal/catalog/import and add-map-texture behavior.
- Authoring graph backend internals.

Expected wrappers left in `SectorEditor.cpp`: a small `DrawTopologySideDefInspector()` method that constructs callbacks/context and calls `game::DrawTopologySideDefMaterialInspector()`, plus all current material action wrappers.

## Risk And Guardrails

| Risk | How to verify in REF-055 | REF-055 final report must state |
| --- | --- | --- |
| Cache invalidation | Code review direct UV/reset paths still call `MarkTopologyDocumentEdited()` or `FinishTopologyMaterialMutation()` exactly as before | Whether topology mutation/cache invalidation behavior changed; expected: unchanged |
| Preview rebuild behavior | Check middle texture/UV and picker apply routes still use existing finish paths and `rebuildPreviewOnApply` flags | Whether preview rebuild behavior changed; expected: unchanged |
| Authoring graph derived topology mapping | Verify callbacks still hit current wrappers, not direct authoring mutations | Authoring side material routes preserved |
| Texture picker rollback/apply behavior | Confirm `ApplyTexturePickerSelection()` remains intact unless explicitly scoped | Picker apply rollback behavior unchanged |
| Decal tint modal state | Verify modal open/apply/cancel callbacks and input reset result flags still work | Decal tint modal behavior unchanged |
| UI focus/input capture | Preserve same widget IDs and UI calls; build plus manual smoke | Manual GUI smoke status, if performed |
| Lightmap source-hash assumptions | No lightmap code changes; material edits use same topology paths | Source-hash behavior unchanged |
| Status text regressions | Keep exact strings in moved code | Status/validation strings preserved |

## Manual Smoke Checklist For REF-055

- Select a topology sidedef and switch wall/lower/upper/middle part tabs.
- Open/apply texture picker for wall, lower, upper, and middle texture.
- Edit UV scale U/V and offset U/V.
- Use reset UV.
- Use copy/paste material.
- Use fit width, fit height, fit both.
- Use align vertical, align U prev, and align U next.
- Assign/edit base material layer and decal material layer.
- Open/apply texture picker from decal layer.
- Edit decal opacity, emissive, bloom intensity.
- Open decal tint modal, apply tint, reset white, cancel.
- Fit decal and clear decal.
- Clear middle texture.
- Exercise authoring side material edit with current authoring graph data.
- Exercise authoring face flat material edit if shared wrappers were touched.
- Regression-smoke 3D preview material panel if shared wrappers were touched.
- Verify dirty/cache behavior: 2D render cache invalidates after topology edits, and preview meshes rebuild when existing behavior requires it.

## Recommended Follow-Up Tasks

- REF-055 completion note: the SideDef/material/decal inspector was extracted
  into `sources/sector_editor/tools/materials/`; material picker routing,
  preview UV panel extraction, and material wrapper cleanup remain follow-up
  tasks.
- Extract material-specific texture picker open/current/apply routing after REF-055, keeping generic catalog/import in texture modules.
- Audit/extract preview UV panel or unify 2D sidedef and 3D surface material UI.
- Extract authoring material inspector rows from `DrawSectorsPanel()` if still line-count-heavy after sidedef extraction.
- Clean up material wrapper callbacks into a material bridge only after picker and preview boundaries are clearer.
- Run direct topology mutation/cache invalidation audit after material extraction.

## Backlog Update

`docs/plans/codebase_refactor_backlog.md` was updated by this audit.

Required updates made:

- REF-054 exists and is marked complete.
- REF-054 completion notes point to this report, recommend REF-055 scope, and record expected line-count reduction.
- REF-055 is now completed: Extract SideDef/material/decal inspector into `tools/materials/`.
- REF-044 remains an umbrella item that points to the REF-054/REF-055 materials migration series instead of a vague stale item.
- No unrelated backlog items were marked complete.

## Appendix: Evidence

Commands used:

```text
rg -n "DrawTopologySideDefInspector|FinishTopologyMaterialMutation|FinishMaterialActionResult|FinishAuthoringSideMaterialActionResult|ApplyAuthoringSideMaterialAction|ApplyAuthoringFaceAnchorFlatMaterialAction|ApplySurface3DUvValue|ApplyTexturePickerSelection|OpenDecalTintModal|ApplySurfaceDecalTint|DecalTint|TexturePicker|MaterialAction|WallPart|SideDef|Sidedef|UV|uvScale|uvOffset|decal" sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditorMaterialActions.* sources/sector_editor/SectorEditorMaterialModals.* sources/sector_editor/SectorEditorTextureActions.* sources/sector_editor/SectorEditorTextureModals.*
rg -n "DrawPreviewUvPanel|selectedSurface3D|selectedTopologySurface3D|ApplySurface3D|PreviewUv|Surface3D|PickSectorSurface3D|SelectSurface3D" sources/sector_editor/SectorEditor.cpp sources/sector_editor/selection sources/sector_editor/tools
rg -n "FinishTopologyMaterialMutation|FinishMaterialActionResult|MarkTopologyDocumentEdited|InvalidateTopologyRenderCache|RebuildPreviewMeshesPreservingView|MarkSectorEditorAuthoringGraphEdited|RefreshSectorEditorAuthoringDerivedTopology" sources/sector_editor
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditorMaterialActions.cpp sources/sector_editor/SectorEditorMaterialModals.cpp sources/sector_editor/SectorEditorTextureActions.cpp sources/sector_editor/SectorEditorTextureModals.cpp
rg -n "enum class TopologyWallPart|enum class TopologyMaterialLayer|struct SectorTopologyUvSettings|struct SectorTopologyDecalLayer|struct SectorTopologyWallPartSettings|struct SectorTopologySideDef|struct SectorTopologySector" sources/sector_demo/SectorTopologyTypes.h sources/sector_demo/SectorTopologyMap.h
rg -n "ComputeSectorLightmapSourceHash|Hash|sidedef|decal|textureId|uv" sources/sector_demo/SectorLightmap.cpp sources/sector_demo/SectorLightmap.h
find sources/sector_editor/tools -maxdepth 2 -type f | sort | xargs -r wc -l
```

Function line ranges:

- `MarkTopologyDocumentEdited()`: `SectorEditor.cpp:2421-2429`
- `FinishTopologyMaterialMutation()`: `SectorEditor.cpp:2431-2442`
- `FinishMaterialActionResult()`: `SectorEditor.cpp:2444-2484`
- `DrawPreviewUvPanel()`: `SectorEditor.cpp:4201-4688`
- `DrawTopologySideDefInspector()`: `SectorEditor.cpp:7334-8035`
- `DrawTexturePickerModal()` wrapper: `SectorEditor.cpp:8054-8068`
- `DrawDecalTintModal()` wrapper: `SectorEditor.cpp:8167-8196`
- `FinishAuthoringSideMaterialActionResult()`: `SectorEditor.cpp:9636-9708`
- `ApplyAuthoringSideMaterialAction()`: `SectorEditor.cpp:9710-9742`
- `ApplyAuthoringFaceAnchorFlatMaterialAction()`: `SectorEditor.cpp:9744-9803`
- Material wrappers: `SectorEditor.cpp:9805-10193`
- Texture picker wrappers/apply routing: `SectorEditor.cpp:10257-10549`
- Material action backend helpers: `SectorEditorMaterialActions.cpp:49-1104`
- Decal tint modal draw body: `SectorEditorMaterialModals.cpp:9-182`
- Texture picker open/apply helpers: `SectorEditorTextureActions.cpp:565-1055`

Line-count evidence:

- `sources/sector_editor/SectorEditor.cpp`: 10,551 lines.
- `sources/sector_editor/SectorEditorMaterialActions.cpp`: 1,104 lines.
- `sources/sector_editor/SectorEditorMaterialModals.cpp`: 184 lines.
- `sources/sector_editor/SectorEditorTextureActions.cpp`: 1,220 lines.
- `sources/sector_editor/SectorEditorTextureModals.cpp`: 547 lines.
- `DrawTopologySideDefInspector()` spans about 702 lines.

Dependency observations:

- `SectorTopologyWallPartSettings` owns `textureId`, base `uv`, and `decal` (`SectorTopologyTypes.h:116-120`).
- `SectorTopologySideDef` owns `wall`, `lower`, `upper`, and `middle` wall parts (`SectorTopologyTypes.h:138-148`).
- Lightmap source hash currently appends wall part base `textureId` and base `uv` through `FnvAppendTopologyWallPart()` (`SectorLightmap.cpp:2491-2495`) and appends all sidedef wall parts in `ComputeSectorLightmapSourceHash()` (`3832-3842`). It does not currently append decal layer fields in that helper.
- REF-055 is recommended because it removes one coherent UI block, follows the existing tool-folder architecture, and can preserve central mutation finish paths via callbacks without moving broad texture picker or preview ownership.
