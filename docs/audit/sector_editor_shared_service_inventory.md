# SectorEditor Shared Service Inventory

## Summary

This audit is needed now because the recent tool/module refactors moved many
feature-owned blocks out of `SectorEditor.cpp`, but the remaining bridges are
not all feature code. Several are shared editor capabilities used by materials,
preview surfaces, doors, billboards, sky, document settings, lights, and future
tools. Moving more clients before naming these services would replace
`SectorEditor.cpp` ownership with long callback chains back into it.

Top service candidates:

- `TexturePickerService`: generic picker modal state, catalog/options, open,
  close, current selection, and selected texture result mechanics.
- `MaterialEditBridge`: shared material finish paths, authoring-vs-topology
  routing, cache invalidation, and preview rebuild hooks.
- `PreviewSurfaceMaterialService`: selected 3D surface material queries, UV
  edits, picker-open requests, and preview rebuild-sensitive material editing.
- `LightEditingService` and `LightmapBakeController`: useful but high-risk
  because static lights, directional light, object probes, and bake settings
  affect lightmap source hashes.

The easiest first service is a minimal `TexturePickerService`. It should own
generic picker mechanics, not feature-specific apply semantics.

The highest-risk service is `LightmapBakeController` because it owns worker
lifecycle, temporary files, source-hash stale-result rejection, object-probe
sidecar install, and preview result refresh.

Recommended next implementation task: extract a minimal `TexturePickerService`
that centralizes open/close/current-selection mechanics and lets feature-owned
handlers apply the selected texture.

## Current Problem

Feature modules are good, but shared capability code should not remain forever
in `SectorEditor.cpp`. REF-041 through REF-055 created better homes for placed
objects, doors, billboards, document helpers, active authoring tools,
selection/manipulation services, and the SideDef material inspector. That
exposed the next dependency smell: generic picker, material finish, preview
surface, light, lightmap, status, and document orchestration code is still
reached through broad callbacks.

Callback bridges are acceptable as short-term migration seams. They become a
problem when every extracted module needs another `SectorEditor` callback to do
the same shared work. Services should be identified before moving more clients
when possible, so tool modules depend on stable shared APIs instead of
recreating bridges back into the coordinator.

Not every shared-looking thing should become a service immediately. High-level
orchestration should stay in `SectorEditor` for now, especially lifecycle,
renderer resource ownership, document load/save/reset orchestration, and
lightmap worker/result installation unless a runner-level migration is scoped.

## Service Candidate Inventory

| Service | Current functions/files involved | Current clients/features | Should own | Must not own | Risk | Readiness | Expected benefit | Expected reduction | Backlog item |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `TexturePickerService` | `TexturePickerState`, `DrawTexturePickerModal()`, `PopulateTexturePickerOptions()`, `CurrentTextureForPickerTarget()`, `Open*TexturePicker()`, `ApplyTexturePickerSelection()` in `SectorEditorTextureActions.*`, wrappers in `SectorEditor.cpp` | Sector flats, sidedefs/materials, authoring faces/sides, preview UV panel, sky, doors, future tools | Generic picker state transitions, option/catalog population, close/cancel, selected texture result, current-texture query API | Door/material/sky apply semantics, dirty/cache/preview rebuild policy, sprite picker unless explicitly split | Medium | Clear now | Removes duplicated picker callback plumbing and prevents new target switches from growing in tools | 100-250 lines or several callbacks first; more after routing split | REF-057 |
| `MaterialEditBridge` | `FinishTopologyMaterialMutation()`, `FinishMaterialActionResult()`, `FinishAuthoringSideMaterialActionResult()`, `ApplyAuthoringSideMaterialAction()`, `ApplyAuthoringFaceAnchorFlatMaterialAction()`, material wrappers in `SectorEditor.cpp`, `SectorEditorMaterialActions.*` | SideDef inspector, sector inspector, authoring line/face inspectors, preview UV panel, picker apply routes, decal tint modal | Central material mutation finish behavior, UI reset flags, topology-vs-authoring routing, cache invalidation, preview rebuild hook | Generic picker UI/catalog, feature inspector layout, lightmap bake controller, renderer internals | High | Needs narrow audit | Reduces callback tentacles from `tools/materials` while keeping dangerous finish behavior centralized | 200-450 lines/callback reduction after audit | REF-059 |
| `PreviewSurfaceMaterialService` | `DrawPreviewUvPanel()`, `BuildPreviewUvPanelRect()`, `PickSectorSurface3D()`, `SelectSurface3D()`, `ResetSurface3DUiState()`, `ApplySurface3DUvValue()`, `ResetSurface3DUv()`, `RebuildPreviewMeshesPreservingView()` | 3D selected floor/ceiling/wall surfaces, material picker routing, SideDef material overlaps | Selected 3D surface material target queries, preview UV action API, picker-open request, surface UI reset coordination | Camera/collision/physics, renderer resource ownership, preview overlay debug UI unless separately scoped | Medium/High | Needs narrow audit | Gives `DrawPreviewUvPanel()` a service dependency instead of many `SectorEditor` callbacks | 250-480 lines when panel moves | REF-060 |
| `LightEditingService` | `StartLightDrag()`, `UpdateLightDrag()`, `FinishLightDrag()`, `CancelLightDrag()`, add/delete/find/select light functions, `SectorEditorLightInspector.*`, spotlight pilot | Static/dynamic point lights, static/dynamic spotlights, Select manipulation, inspector callbacks, preview overlays | Light add/delete/select/drag/edit APIs and source-hash-aware mutation finish paths | Lightmap bake worker, dynamic renderer implementation, broad preview settings modal | High | Needs narrow audit | Clarifies light ownership before moving inspector/drag code | 300-600 lines later | REF-045 |
| `LightmapBakeController` | `BakeLightmaps()`, `StartLightmapBake()`, `PollLightmapBakeResult()`, `RequestLightmapBakeCancel()`, `JoinLightmapBakeWorker()`, `ShutdownLightmapBake()`, `ConsumeLightmapBakeResult()`, `InstallLightmapBakeResult()`, `SectorEditorLightmapModal.*` | Bake modal, document save path, topology/light settings, object probes, preview rebuild | Worker lifecycle, cancellation, temporary file cleanup, result install, stale source-hash rejection, progress/status model | Feature light editing, preview-only settings, renderer resource lifecycle beyond a rebuild callback | High | Defer / Runner plan | Encapsulates the riskiest async path, but only with explicit guardrails | 250-330 lines later | REF-062 |
| `DocumentController` | `NewBlankLevel()`, load/save/reload/reset calls, `SectorEditorDocumentActions.*`, `SectorEditorDocumentModals.*`, preview shutdown/load orchestration | File menu, document modals, preview enter/leave, asset scopes, authoring graph import | Narrow document action helpers and modal state transitions if more clients need them | Whole-editor lifecycle, preview resource teardown/rebuild, lightmap worker shutdown without runner scope | Medium/High | Defer | Could reduce lifecycle callback noise, but current central orchestration is acceptable | 0-120 lines now | Existing REF-042/REF-023 notes |
| `AssetCatalog/TextureCatalogService` | `RefreshEditorTextureHandles()`, `EditorTextureHandleForId()`, add-map texture scan/import, texture registry display, sprite metadata catalog, `SectorEditorTextureActions.*` | Texture picker, add-map texture modal, sky, materials, doors, billboards/sprite picker | Texture dictionary lookup, editor texture handle cache, scan/import/catalog helpers | Feature-specific picker apply routes, sprite animation repair policy unless scoped | Medium | Needs narrow audit | Prevents texture picker service from becoming an asset mega-service | 100-250 lines/callback cleanup later | REF-058 |
| `Status/DiagnosticsService` | `statusText`, `topologyRenderWarning`, `sectorCollisionWorldWarning`, runtime object warning/status strings, validation diagnostics | Most tools, preview overlay, lightmap bake, document actions | Shared status sink, warning aggregation, optional severity/category data | Feature mutation policy, validation generation, render-cache ownership | Low/Medium | Defer | Lowers context parameter noise but does not unlock major extraction | Small callback reduction | Defer |
| Selection/manipulation refinements | `selection/`, `BuildSelectionServiceContext()`, `BuildManipulationServiceContext()`, Select provider gaps | Select, placed objects, authoring vertices, lights, preview surface selection | More providers for authoring vertices, lights, preview surface selection/picking | Material mutation, lightmap source-hash policy, feature inspectors | Low/Medium | Clear by slice | Good architecture cleanup; not the largest line-count win | 100-250 lines by provider | Existing REF-047 to REF-052 follow-ups |

## Texture Picker Service Deep Dive

Generic picker mechanics today:

- `TexturePickerState` tracks open state, selected index, target fields, layer,
  scroll state, option IDs, and option labels.
- `DrawTexturePickerModal()` owns the generic modal drawing and accepts
  callbacks for close, apply, current-texture query, and texture handle lookup.
- `PopulateTexturePickerOptions()` fills picker options from the map texture
  registry and highlights the current texture.
- `CurrentTextureForPickerTarget()` can resolve the current texture for map
  sky, runtime door, topology sectors, topology sidedefs, authoring faces, and
  authoring sides.
- `OpenTopologyTexturePicker()`, `OpenTopologySideDefTexturePicker()`,
  `OpenAuthoringFaceAnchorTexturePicker*()`, `OpenAuthoringSideTexturePicker*()`,
  `OpenMapSkyTexturePicker()`, and `OpenRuntimeDoorTexturePicker()` initialize
  picker target state.
- `ApplyTexturePickerSelection()` and
  `ApplyAuthoringTexturePickerSelection()` apply selected texture IDs, but those
  already mix generic selection with feature-specific mutation semantics.

Current features that open texture or sprite pickers:

- Sector flat/base/decal material editing through `SectorEditorSectorInspector`.
- SideDef/wall/lower/upper/middle material editing through
  `tools/materials/SectorEditorMaterialInspector`.
- Preview 3D surface UV/material editing through `DrawPreviewUvPanel()`.
- Authoring line/face/default material editing inside `DrawSectorsPanel()`.
- Sky texture selection through `SectorEditorPreviewSettingsModal`.
- Door texture selection through `tools/doors`.
- Billboard sprite selection through `tools/billboards` and the sprite picker.

Feature-specific apply paths:

- Material/sidedef/sector picker apply must preserve topology dirty flags,
  render-cache invalidation, UI reset flags, preview mesh rebuild behavior, and
  authoring graph writeback.
- Door picker apply mutates a selected runtime object and refreshes runtime
  object authoring state.
- Sky picker apply changes map sky visual settings and should remain visual-only
  relative to lightmap source hash.
- Billboard sprite picker apply mutates sprite animation metadata and clip
  repair state. It should stay separate from texture picker unless a dedicated
  sprite picker service is scoped.

Service-owned code should be:

- Generic `TexturePickerState` open/close lifecycle.
- Option population and selected texture result extraction.
- Current texture query as a callback/API, not a hardcoded global target switch.
- Modal callback construction that feature modules can call without owning the
  modal.
- Shared texture handle lookup hookup to the editor texture catalog.

Feature-owned code should remain:

- Material-specific apply routing in `tools/materials` or a future
  `MaterialEditBridge`.
- Door-specific apply routing in `tools/doors`.
- Billboard/sprite apply routing in `tools/billboards` or a separate sprite
  picker service.
- Sky/document apply routing with preview settings/document ownership.
- Dirty/cache/preview rebuild/source-hash decisions.

`ApplyTexturePickerSelection()` should be split, but not in one large move. The
current function combines runtime door application, authoring direct application,
authoring-side rollback/apply, authoring-flat rollback/apply, topology material
finish behavior, and preview rebuild flags. The first split should extract a
selected texture result and target descriptor, then let feature-owned apply
handlers decide what to mutate.

Minimal first implementation slice:

- Add a `TexturePickerService` API that owns close/cancel, option population,
  selected texture result, current-texture callback, and open target descriptor
  creation.
- Keep all current target kinds and behavior intact.
- Keep `ApplyTexturePickerSelection()` in `SectorEditor.cpp` initially, but make
  it consume a generic selected texture result from the service.
- Do not move add-map texture import, sprite picker, material finish wrappers,
  sky apply semantics, or door apply semantics in the first slice.

This helps `tools/materials` after REF-055 because the material inspector can
request "open picker for this material target" through a picker service API
rather than another `SectorEditor` callback that also knows how the global
modal is wired.

## Material Mutation / Finish Path Deep Dive

Finish wrappers today:

- `FinishTopologyMaterialMutation()` clears topology render warnings, marks the
  topology document edited, invalidates the 2D render cache through
  `MarkTopologyDocumentEdited()`, and rebuilds preview meshes when editing in
  3D preview.
- `FinishMaterialActionResult()` applies UI reset flags, closes decal tint
  state when requested, then delegates changed results to
  `FinishTopologyMaterialMutation()`.
- `FinishAuthoringSideMaterialActionResult()` writes derived topology material
  edits back into authoring side data and refreshes derived topology.
- `ApplyAuthoringSideMaterialAction()` and
  `ApplyAuthoringFaceAnchorFlatMaterialAction()` route material edits through
  topology snapshots when authoring graph data is authoritative.

Current clients include the SideDef material inspector, sector inspector,
authoring line/face inspector rows, preview UV panel, decal tint modal, and
texture picker apply routing.

Callbacks from `tools/materials` still point back to `SectorEditor.cpp` for
texture picker open, copy/paste, decal property edits, decal tint, clear, fit,
align, direct UV finish, and portal `Blocks Player`. This is the callback
cluster REF-055 intentionally left behind.

A service would simplify `tools/materials`, but extracting a broad
`MaterialMutationService` now would be dangerous because it would encode the
most sensitive behavior: cache invalidation, UI reset state, authoring graph
mapping, and preview mesh rebuild. The safer direction is to audit or implement
a smaller `MaterialEditBridge` after texture picker routing is clearer.

Must preserve:

- `MarkTopologyDocumentEdited()` and 2D topology render-cache invalidation for
  topology material mutations.
- Preview mesh rebuild for material changes that currently rebuild in 3D mode.
- Authoring graph writeback and derived-topology refresh behavior.
- UI reset flags for sector, sidedef, preview surface, and decal inputs.
- Decal tint modal close/reset behavior.

Recommendation: do not extract a broad material mutation service before the
picker target/result boundary is understood. Define a smaller
`MaterialEditBridge` later and keep finish behavior centralized until that
audit is complete.

## Preview UV / Surface Editing Dependency

`DrawPreviewUvPanel()` depends on both material editing and preview-surface
selection:

- It uses selected 3D surface state, authoring mapping validation, and
  `BuildSectorEditorSurface3DTargetLabel()`.
- It opens sector or sidedef texture pickers and sets
  `rebuildPreviewOnApply`.
- It edits the same material properties as the SideDef inspector: texture,
  base/decal layer, UV scale/offset, decal opacity/emissive/bloom/tint, fit,
  align, clear middle, and portal `Blocks Player`.
- It depends on preview rebuild semantics through material finish wrappers.

The panel overlaps heavily with SideDef material inspector callbacks, but it
also owns preview-specific layout, selected 3D surface validity, and input event
consumption. It should wait until either `TexturePickerService` exists or the
`MaterialEditBridge` boundary is audited. The service that should exist before
moving it is at least a picker service; ideally a narrow preview-surface
material API also exists so the panel does not include preview renderer or
collision ownership.

## Light / Lightmap Service Risk

Source-hash-sensitive light editing flows:

- Static point lights.
- Static spotlights.
- Directional light settings.
- Object probe spacing and height settings.
- Lightmap bake settings and baked result metadata.
- Geometry-affecting sky flags such as `ceilingSky`, though sky visual settings
  remain source-hash-neutral.

Preview-only or runtime-preview flows:

- Dynamic point lights.
- Dynamic spotlights.
- Dynamic-light preview refresh.
- Spotlight pilot preview interaction before apply.
- Preview overlay debug controls and object probe visualization toggles.

Light editing should be audited before implementation because static and
dynamic light flows currently share selection, drag, inspector, and preview
overlay paths, but only some affect the lightmap source hash. Lightmap bake
controller extraction should be runner-plan level because it has thread
lifecycle, file installation, source-hash stale-result rejection, object-probe
sidecar install, and preview result refresh behavior.

## Recommended Service Roadmap

### Immediate Service Tasks

- REF-057: implement minimal `TexturePickerService` for generic picker
  lifecycle/result mechanics.
- REF-058: audit `AssetCatalog/TextureCatalog` boundary so picker service does
  not become an asset-import mega-service.

### Needs Narrow Audit First

- REF-059: audit a `MaterialEditBridge` boundary for finish paths and
  material-specific picker apply routing.
- REF-060: audit preview UV/material panel dependencies on picker service,
  material edit bridge, preview surface selection, and preview rebuild hooks.
- REF-045: audit lights/source-hash-sensitive tool migration before moving
  static/dynamic light editing.
- Selection/manipulation provider refinements for authoring vertices, lights,
  and preview surfaces if they unblock a specific feature extraction.

### Runner Plan

- REF-062: `LightmapBakeController` extraction.
- Broad light editing migration if it moves static lights, dynamic lights,
  directional light settings, object probe settings, and preview controls
  together.
- Broad preview migration if it moves overlay, UV panel, preview settings,
  control mode, and rebuild orchestration together.

### Defer

- `DocumentController` beyond existing document action/modal helpers.
- `Status/DiagnosticsService` unless status/warning callback noise blocks
  another extraction.
- Generic universal UV/material framework.
- Full document-state vs preview-state rewrite.

## Recommended First Implementation Task

Pick: `TexturePickerService` minimal extraction.

Initial scope:

- Create a small service/API around `TexturePickerState` open/close,
  option population, selected texture extraction, current-texture callback, and
  modal callback wiring.
- Preserve existing target kinds and current picker behavior.
- Keep `ApplyTexturePickerSelection()` behavior equivalent, initially by having
  it consume the service-selected texture result.
- Keep `SectorEditor.cpp` as the high-level coordinator for feature-owned apply
  routes until material and door/sky routing are split.

Do not move:

- Material finish wrappers.
- Authoring graph writeback behavior.
- Door texture apply behavior.
- Sky apply behavior.
- Sprite picker behavior.
- Add-map texture scan/import.
- Lightmap or preview rebuild policy.

This helps `tools/materials` after REF-055 by replacing a broad
`openSideDefTexturePicker` callback bridge with a shared picker dependency that
does not force the materials module to own the whole modal or global picker
state. Behavior and picker apply semantics should be preserved.

## Backlog Update

`docs/plans/codebase_refactor_backlog.md` was updated by this audit.

Required updates made:

- REF-056 was already present as a preview overlay/UV audit. It was refined into
  this shared service inventory audit and marked complete.
- REF-056 completion notes point to this report and name
  `TexturePickerService` minimal extraction as the recommended first service
  implementation.
- Future backlog items were added/refined for `TexturePickerService`,
  `AssetCatalog/TextureCatalog`, material-specific picker routing /
  `MaterialEditBridge`, preview UV/material service dependency audit, and
  lightmap bake controller extraction.
- REF-044 remains open as the materials migration umbrella because material
  picker routing, preview UV/material panel, and material wrapper cleanup remain
  incomplete.

## Appendix: Evidence

Commands used:

```text
rg --files docs/audit docs/plans sources/sector_editor
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditorTextureActions.cpp sources/sector_editor/SectorEditorTextureModals.cpp sources/sector_editor/SectorEditorMaterialActions.cpp sources/sector_editor/tools/materials/SectorEditorMaterialInspector.cpp
sed -n '1,240p' docs/audit/sector_editor_post_tool_migration_line_map.md
sed -n '1,260p' docs/audit/sector_editor_materials_boundary_audit.md
sed -n '1,260p' docs/audit/sector_editor_selection_manipulation_contract.md
sed -n '1,260p' docs/audit/sector_editor_tool_module_boundary_design.md
sed -n '1,260p' docs/plans/codebase_refactor_backlog.md
rg -n "TexturePicker|Open.*TexturePicker|ApplyTexturePickerSelection|CurrentTextureForPickerTarget|DrawTexturePickerModal|AddMapTexture|SpritePicker|TextureCatalog|texture picker|picker target" sources/sector_editor
rg -n "FinishTopologyMaterialMutation|FinishMaterialActionResult|FinishAuthoringSideMaterialActionResult|ApplyAuthoringSideMaterialAction|ApplyAuthoringFaceAnchorFlatMaterialAction|ApplySurface3DUvValue|ResetSurface3DUv|OpenDecalTintModal|ApplySurfaceDecalTint" sources/sector_editor
rg -n "DrawPreviewUvPanel|selectedSurface3D|selectedTopologySurface3D|PreviewUv|Surface3D|PickSectorSurface3D|SelectSurface3D|RebuildPreviewMeshesPreservingView" sources/sector_editor
rg -n "Lightmap|Bake|sourceHash|source-hash|ObjectProbe|Directional|StaticLight|DynamicLight|SpotLight|StartLightDrag|FinishLightDrag|ApplyPreviewSettingsModal" sources/sector_editor
rg -n "statusText|topologyRenderWarning|sectorCollisionWorldWarning|runtimeObjects.*Status|warning|diagnostic" sources/sector_editor/SectorEditor.cpp sources/sector_editor/*.h
```

Selected grep results:

- Picker clients/routes: `DrawPreviewUvPanel()` opens topology pickers at
  `SectorEditor.cpp:4402`; sector inspector opens texture pickers through
  `SectorEditorSectorInspector.cpp:265`; material inspector opens sidedef
  pickers through `tools/materials/SectorEditorMaterialInspector.cpp:269`; sky
  picker opens through `SectorEditorPreviewSettingsModal.cpp:215`; door and
  billboard inspectors call picker callbacks in
  `tools/doors/SectorEditorDoorInspector.cpp:373` and
  `tools/billboards/SectorEditorBillboardInspector.cpp:227`.
- Picker implementation: `TexturePickerState` is in
  `SectorEditorModalTypes.h`; generic modal draw is
  `SectorEditorTextureModals.cpp:186`; open/current/apply helpers live in
  `SectorEditorTextureActions.cpp:565-1010`; broad routing wrapper remains in
  `SectorEditor.cpp:9681-9935`.
- Material finish paths: `FinishTopologyMaterialMutation()` and
  `FinishMaterialActionResult()` are in `SectorEditor.cpp:2432-2485`;
  authoring side/face material routes and surface UV/decal wrappers are in
  `SectorEditor.cpp:9021-9580`.
- Preview UV/material panel: `DrawPreviewUvPanel()` is in
  `SectorEditor.cpp:4202-4688` and shares picker/material callbacks with the
  extracted SideDef material inspector.
- Lightmap risk: `StartLightmapBake()` computes
  `ComputeSectorLightmapSourceHash(state.topologyMap)` before starting the
  worker; `InstallLightmapBakeResult()` rejects stale results when the current
  source hash differs from the expected hash.

Current client/function inventory:

| Capability | Current clients | Current home |
| --- | --- | --- |
| Generic texture picker modal | materials, sector flats, preview UV, sky, doors | `SectorEditorTextureModals.*` plus `SectorEditor.cpp` callbacks |
| Texture picker target/current/apply routing | materials, doors, sky, authoring graph, preview UV | `SectorEditorTextureActions.*` plus `SectorEditor.cpp` broad wrapper |
| Material finish behavior | SideDef inspector, sector inspector, preview UV, decal tint, picker apply | `SectorEditor.cpp` |
| Preview surface material editing | selected 3D floor/ceiling/wall surfaces | `SectorEditor.cpp` |
| Light editing | Select, light inspectors, drag, preview overlays | `SectorEditor.cpp`, `SectorEditorLightInspector.*`, selection/manipulation services |
| Lightmap bake | tools panel, modal, worker, result install | `SectorEditor.cpp`, `SectorEditorLightmapModal.*` |
| Status/diagnostics | most editor flows | `SectorEditor::statusText`, state warning fields |

Service candidate scoring:

| Candidate | Clarity | Risk | First-slice value | Recommendation |
| --- | --- | --- | --- | --- |
| Texture picker | High | Medium | High | Implement first |
| Material edit bridge | Medium | High | High | Audit after picker boundary |
| Preview surface/material | Medium | Medium/High | Medium/High | Audit before panel move |
| Light editing | Medium | High | Medium | Audit first |
| Lightmap bake controller | Medium | High | Medium | Runner plan |
| Document controller | Low/Medium | Medium/High | Low | Defer |
| Asset/texture catalog | Medium | Medium | Medium | Narrow audit |
| Status/diagnostics | Medium | Low/Medium | Low | Defer |

Stale backlog notes found:

- REF-056 previously named preview overlay/UV audit as the next audit. That was
  superseded by this shared-service inventory and split into future service
  tasks.
- REF-020 still referred broadly to extracting texture/material action or
  inspector code. It now needs to point at post-REF-055 service work instead of
  another generic extraction.
