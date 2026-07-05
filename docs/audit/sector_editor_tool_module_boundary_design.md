# SectorEditor Tool/Module Boundary Design

## Summary

Category-based extraction has reduced `SectorEditor.cpp`, but it is not enough
as the long-term architecture. The current category modules
(`*Modals`, `*Inspectors`, `*Actions`, `*Helpers`) are useful seams, but they can
become new landfills if every feature adds code to several generic files.

The next architecture direction should be feature/tool-oriented:

- `SectorEditor` stays the coordinator for the working document, global editor
  state, active tool routing, shared services, renderer/preview orchestration,
  high-level lifecycle, and shared mutation/selection/render APIs.
- The working document is the editable level data: topology map, authoring
  graph-derived topology, runtime object definitions, texture/material
  references, static/dynamic light authoring data, lightmap settings/metadata,
  preview settings stored in the map, and future game-mode document data.
- Tool modules own feature-specific input, inspector UI, modals, actions,
  helper functions, and temporary tool state where practical.
- Tools communicate through narrow context/toolbox APIs and callbacks, not by
  depending on `SectorEditor`.
- The first implementation task should be a behavior-preserving
  `tools/runtime_objects/` pilot that moves the already extracted runtime object
  inspector, modal, action, and drag files into a feature folder.

Top 5 migration steps:

1. Move runtime object files into `sources/sector_editor/tools/runtime_objects/`
   as a pilot, preserving their existing context/callback shape.
2. Move document actions/modals into `sources/sector_editor/document/` while
   keeping lifecycle orchestration in `SectorEditor`.
3. Add `sources/sector_editor/shared/` only for narrow toolbox/context types and
   shared UI/selection/mutation declarations that are proven by the pilot.
4. Split authoring tools into `select`, `line`, `rectangle`, and
   `insert_vertex` modules rather than creating an `authoring/` landfill.
5. Defer lights and preview broad migrations until source-hash-sensitive and
   renderer-coupled flows have narrower audits or runner plans.

## Current Problem

`sources/sector_editor/SectorEditor.cpp` is still too large. Current line-count
evidence from this audit:

| File | Lines |
| --- | ---: |
| `sources/sector_editor/SectorEditor.cpp` | 11,358 |
| `sources/sector_editor/SectorEditorAuthoringState.cpp` | 2,198 |
| `sources/sector_editor/SectorEditorHelpers.cpp` | 1,590 |
| `sources/sector_editor/SectorEditorTopologyRenderCache.cpp` | 1,331 |
| `sources/sector_editor/SectorEditorRuntimeObjectInspector.cpp` | 1,288 |
| `sources/sector_editor/SectorEditorTextureActions.cpp` | 1,220 |
| `sources/sector_editor/SectorEditorMaterialActions.cpp` | 1,104 |

`SectorEditor.cpp` still owns canvas routing, selection, pending authoring
tools, light drag, preview overlays, preview UV panel, document lifecycle,
texture picker routing, material wrappers, lightmap bake orchestration, and
many private helper wrappers. The private API in `SectorEditor.h` reflects this:
it declares tool input, drag, modals, document workflows, preview, selection,
materials, runtime objects, and lightmap operations.

The previous extractions were worthwhile, but category files such as
`SectorEditorDocumentModals.*`, `SectorEditorMaterialActions.*`,
`SectorEditorRuntimeObjectInspector.*`, `SectorEditorTextureActions.*`, and
`SectorEditorHelpers.*` show the next risk: future features could be split
across generic "actions", "modals", "inspectors", and "helpers" files instead
of living together. A new runtime-object feature, for example, can reasonably
need input, inspector rows, a modal, picker routing, mutation wrappers, cached
draw refresh, and preview rebuild hooks. Scattering that through five category
files makes ownership harder to see.

Tool/feature ownership is the better organizing principle. Category words can
remain inside a feature folder (`RuntimeObjectInspector`, `RuntimeObjectDrag`,
`RuntimeObjectModals`), but the folder boundary should be the feature.

## Design Goals

- Feature/tool locality: related input, inspector, modal, action, and helper
  code should be near each other.
- Narrow editor toolbox APIs: tools receive only the services they need.
- No direct `SectorEditor` dependency from tools.
- No giant OOP hierarchy.
- No fat no-op tool interface.
- Behavior-preserving migration.
- Plugin-like mental model without actual runtime plugin loading.
- Easy future feature addition: adding one tool should not require edits to five
  generic category files.
- Small incremental refactors that preserve current editor behavior, cache
  invalidation, lightmap source-hash behavior, collision behavior, and renderer
  resource ownership.

## Proposed Ownership Model

`SectorEditor` owns:

- working document instance and global editor state
- active `SectorEditorTool` / `SectorEditorMode`
- frame/update/render/UI orchestration
- shared services: `EngineContext`, `AssetManager` access, UI/input wiring,
  status text, editor texture scope, preview renderer, async lightmap worker
- high-level document lifecycle: new, load, save, reset, preview enter/leave
- shared mutation finish paths such as `MarkTopologyDocumentEdited()`,
  `FinishTopologyMaterialMutation()`, `FinishMaterialActionResult()`,
  `FinishTopologyActionResult()`, preview rebuild, runtime object refresh, and
  render-cache invalidation
- high-level routing between tools, modals, document workflows, and preview
  workflows

Document owns:

- `SectorTopologyMap`
- `SectorAuthoringGraph` and derived/last-valid topology state while graph
  authoring remains editor-owned
- topology vertices/linedefs/sidedefs/sectors
- runtime object definitions stored in the map
- texture/material references stored in the map
- static and dynamic light authoring data stored in the map
- sky, preview, directional light, lightmap settings, and baked metadata stored
  in the map
- current document identity and dirty state at the editor-document boundary

Tools own:

- feature-specific canvas input behavior
- feature-specific inspector UI
- feature-specific modals
- feature-specific action helpers
- feature-specific temporary state where it can be split out safely
- feature-specific picker routing and callbacks where the picker is invoked by
  that feature

Shared APIs own:

- narrow toolbox/context declarations
- shared selection and mutation service declarations
- reusable UI primitives and row helpers
- generic coordinate/picking helpers that are not feature-specific
- render refresh hooks exposed to tools without exposing `SectorEditor`

## Proposed Dependency Direction

Intended direction:

```text
SectorEditor
  -> tool modules
    -> narrow editor toolbox/API
      -> working document / mutation services / selection services / renderer refresh hooks
```

Explicitly forbidden for tool modules:

- including `SectorEditor.h`
- storing `SectorEditor*`
- receiving `SectorEditor&`
- calling `SectorEditor::` methods directly

The replacement is context/toolbox/callback wiring. `SectorEditor` may build a
small context for a tool each frame or each action. The tool calls functions on
that context, or invokes callbacks such as `markTopologyDocumentEdited`,
`selectRuntimeObject`, `mutateSelectedRuntimeObject`, `rebuildPreview`, or
`openTexturePicker`. This keeps `SectorEditor` as the owner of orchestration
while making the tool module independent of the concrete editor class.

The existing runtime object files already prove this shape. The inspected
runtime object modules do not include `SectorEditor.h` and use explicit
`SectorEditorRuntimeObjectActionContext`,
`SectorEditorRuntimeObjectDragContext`,
`SectorEditorRuntimeObjectInspectorCallbacks`, and modal callbacks.

## Proposed Folder Layout

### `sources/sector_editor/document/`

Purpose: document lifecycle helpers, document save/load/new/reset actions, and
document-specific modals.

Likely initial files:

- `SectorEditorDocumentActions.h/.cpp`
- `SectorEditorDocumentModals.h/.cpp`

Current files that would move there:

- `sources/sector_editor/SectorEditorDocumentActions.*`
- `sources/sector_editor/SectorEditorDocumentModals.*`

What should not move there:

- preview enter/leave
- renderer rebuild implementation
- per-tool modals
- generic confirmation modal if it becomes a shared command service used by
  many tool modules
- map mutation helpers that are tool-specific

### `sources/sector_editor/shared/`

Purpose: narrow editor APIs and reusable UI/context helpers used by multiple
tool modules.

Likely initial files:

- future `SectorEditorToolbox.h`
- future `SectorEditorMutationApi.h`
- future `SectorEditorSelectionApi.h`
- future `SectorEditorToolTypes.h`
- existing shared helpers only when proven useful

Current files that may partially move there later:

- parts of `SectorEditorHelpers.*`
- parts of `SectorEditorUiHelpers.*`
- possibly narrow passive types from `SectorEditorSelectionTypes.h`

What should not move there:

- feature-specific inspector rows
- feature-specific modals
- feature-specific action wrappers
- a giant all-access `EditorContext` exposing `SectorEditorState&`,
  `SectorEditorUiState&`, and every service by default

### `sources/sector_editor/tools/select/`

Purpose: select tool input, pick cycling, selection changes, select-drag arming,
and shared selection inspector routing only where genuinely select-owned.

Likely initial files:

- future `SectorEditorSelectTool.h/.cpp`
- future selection input/pick dispatch helpers

Current code involved:

- `UpdateSelectDragArm()`
- `ArmSelectedSelectDrag()`
- `StartSelectDrag()`
- `CurrentPickSelectionTarget()`
- `BuildSelectPickCandidates()`
- `SelectPickTarget()`
- `FindSelectedMovablePickTargetAtScreenPoint()`

What should not move there:

- runtime object drag internals
- light drag internals
- authoring vertex move internals
- generic selection state types before there is a clearer split

### `sources/sector_editor/tools/line/`

Purpose: line drawing tool input and line-chain state transitions. The current
UI text says "Authoring Line"; do not rename it during the first moves, but the
target concept should be "Line".

Likely initial files:

- future `SectorEditorLineTool.h/.cpp`

Current code involved:

- `CancelPendingAuthoringLine()`
- `AddAuthoringLinePoint()`
- `DrawPendingAuthoringLine()`
- `ClickSectorEditorAuthoringLineTool()`
- `CancelSectorEditorAuthoringLineToolChain()`
- line portions of `SectorEditorAuthoringState.*`

What should not move there:

- rectangle and insert-vertex behavior
- all authoring graph material helpers

### `sources/sector_editor/tools/rectangle/`

Purpose: rectangle drawing tool input and pending rectangle overlays.

Likely initial files:

- future `SectorEditorRectangleTool.h/.cpp`

Current code involved:

- `CancelPendingAuthoringRectangle()`
- `AddAuthoringRectanglePoint()`
- `DrawPendingAuthoringRectangle()`
- `CreateSectorAuthoringRectangle()`
- `AddSectorEditorAuthoringRectangle()`

What should not move there:

- line-chain behavior
- generic authoring derivation refresh

### `sources/sector_editor/tools/insert_vertex/`

Purpose: insert vertex tool input, hover, target resolution, commit/cancel, and
pending overlay.

Likely initial files:

- future `SectorEditorInsertVertexTool.h/.cpp`

Current code involved:

- `CancelPendingAuthoringInsertVertex()`
- `BeginPendingAuthoringInsertVertex()`
- `TryResolveAuthoringInsertVertexPoint()`
- `UpdatePendingAuthoringInsertVertex()`
- `CommitAuthoringInsertVertex()`
- `DrawPendingAuthoringInsertVertex()`
- `InsertSectorEditorAuthoringVertexOnLine()`

What should not move there:

- authoring vertex drag/move unless explicitly scoped
- select drag routing

### `sources/sector_editor/tools/runtime_objects/`

Purpose: runtime object tool, inspector, door/billboard modals, object actions,
and object drag.

Likely initial files:

- `SectorEditorRuntimeObjectInspector.h/.cpp`
- `SectorEditorRuntimeObjectModals.h/.cpp`
- `SectorEditorRuntimeObjectActions.h/.cpp`
- `SectorEditorRuntimeObjectDrag.h/.cpp`

Current files that would move there:

- `sources/sector_editor/SectorEditorRuntimeObjectInspector.*`
- `sources/sector_editor/SectorEditorRuntimeObjectModals.*`
- `sources/sector_editor/SectorEditorRuntimeObjectActions.*`
- `sources/sector_editor/SectorEditorRuntimeObjectDrag.*`

What should not move there:

- renderer-side door or billboard rendering
- sector runtime ECS implementation in `sources/sector_demo`
- generic texture picker implementation
- document lifecycle

### `sources/sector_editor/tools/materials/`

Purpose: sidedef/material/decal editing modules, material inspector UI, material
action helpers, decal tint modal, and texture picker routing for material
targets.

Likely initial files:

- `SectorEditorMaterialActions.h/.cpp`
- `SectorEditorMaterialModals.h/.cpp`
- future `SectorEditorMaterialInspector.h/.cpp`

Current files/functions that would move there:

- `SectorEditorMaterialActions.*`
- `SectorEditorMaterialModals.*`
- `DrawTopologySideDefInspector()`
- material wrapper functions such as `ApplySurface3DUvValue()`,
  `FitSelectedWallMaterial()`, `OpenDecalTintModal()`

What should not move there:

- runtime door texture settings
- generic texture catalog import
- whole `SectorEditorTextureActions.*` unless split into material-specific
  picker routing and shared texture catalog pieces

### `sources/sector_editor/tools/lights/`

Purpose: light placement, drag, inspectors, static/dynamic point lights,
static/dynamic spotlights, object-probe-adjacent settings where clearly
light-owned, and spotlight pilot if split carefully.

Likely initial files:

- `SectorEditorLightInspector.h/.cpp`
- future `SectorEditorLightActions.h/.cpp`
- future `SectorEditorLightDrag.h/.cpp`

Current files/functions that would move there after audit:

- `SectorEditorLightInspector.*`
- `StartLightDrag()`, `UpdateLightDrag()`, `FinishLightDrag()`,
  `CancelLightDrag()`
- `AddStaticLightAt()`, `DeleteLightById()`,
  `AddStaticSpotLightAt()`, `DeleteStaticSpotLightById()`,
  `AddDynamicLightAt()`, `DeleteDynamicLightById()`,
  `AddDynamicSpotLightAt()`, `DeleteDynamicSpotLightById()`
- possibly `StartSpotLightPilot()`, `ApplySpotLightPilot()`,
  `CancelSpotLightPilot()` after preview coupling is audited

What should not move there without a runner plan:

- full async lightmap bake worker lifecycle
- renderer dynamic-light internals
- source-hash compute logic

### `sources/sector_editor/tools/preview/`

Purpose: editor-side preview controls, preview overlays, preview UV panel, and
preview settings UI where not document-owned.

Likely initial files:

- `SectorEditorPreviewActions.h/.cpp`
- `SectorEditorPreviewSettingsModal.h/.cpp`
- future `SectorEditorPreviewOverlay.h/.cpp`
- future `SectorEditorPreviewUvPanel.h/.cpp`

Current files/functions that may move:

- `SectorEditorPreviewActions.*`
- `SectorEditorPreviewSettingsModal.*`
- `DrawPreviewOverlay()`
- `DrawPreviewUvPanel()`
- `DrawPreviewSurfaceHighlights()`
- `DrawPreviewSpotLightOverlay()`
- `DrawPreviewObjectProbeOverlay()`
- `RefreshPreviewObjectProbeDebugData()`

What should not move there:

- `SectorMeshRenderer` implementation
- GPU resource lifetime ownership
- high-level preview enter/leave orchestration until a narrower plan exists

## Tool Dispatch Model Options

### 1. Fat virtual `IEditorTool` interface

Pros:

- Familiar plugin-like shape.
- Centralizes all possible handlers in one type.

Cons:

- Forces many no-op methods for tools that do not need inspectors, modals,
  previews, drag, or picker routing.
- Encourages a broad base interface that mirrors `SectorEditor`.
- Easy to over-abstract before the first migration proves the necessary shape.

Risk: High. This codebase values plain data and free functions; a broad virtual
hierarchy would fight existing style.

Fit: Poor.

### 2. Thin virtual interface

Pros:

- Smaller than a fat interface.
- Could support a future plugin-like mental model.

Cons:

- Still introduces inheritance and vtables before they are necessary.
- Optional behavior still needs side channels or casts.
- Does not solve toolbox shape by itself.

Risk: Medium.

Fit: Acceptable only if a later plugin boundary truly needs dynamic dispatch.

### 3. Function-pointer/module table

Pros:

- Data-oriented and explicit.
- Optional handlers can be nullable.
- Easy to keep small: `onCanvasInput`, `drawInspector`, `drawModal`,
  `cancel`, `onToolSelected`.
- Compatible with a future plugin-like model without runtime plugin loading.
- Avoids no-op methods.

Cons:

- Requires careful lifetime rules for callbacks/context.
- Can become a hidden registry if the table grows without discipline.
- Debugging through function pointers is slightly less direct than plain calls.

Risk: Low/Medium if kept small.

Fit: Good after one pilot has proven the needed handlers.

### 4. Explicit switch dispatch in `SectorEditor`

Pros:

- Very explicit.
- Easy to debug.
- Lowest abstraction cost.
- Matches current code style.

Cons:

- `SectorEditor` remains the place every tool must touch.
- Large switches can become another coordinator landfill.
- Optional handlers are manual and repeated.

Risk: Low for early moves, Medium if it lasts too long.

Fit: Good as a transitional model.

### 5. Hybrid explicit registry plus direct calls for non-tool workflows

Pros:

- Keeps document workflows, global modals, preview lifecycle, and lightmap bake
  orchestration outside the tool table.
- Lets actual tools use a small optional-handler table.
- Allows the first migrations to remain direct-call based until a table is
  justified.

Cons:

- Requires discipline to decide what is a tool and what is a document/global
  workflow.
- May temporarily mix direct dispatch and table dispatch.

Risk: Low/Medium.

Fit: Best.

Recommendation: use the hybrid. Avoid a fat virtual hierarchy. Start with direct
context/callback calls during the runtime object pilot. Introduce a small
data-driven module table or explicit registry only when at least two or three
tool folders need the same optional handler shape. Non-tool document workflows
should remain directly orchestrated by `SectorEditor` or the document module.

## Proposed Tool Context / Toolbox APIs

These APIs are conceptual and should not be implemented all at once. The
runtime object pilot should move files first and keep its existing context
structs. Generalize only after repeated needs are visible.

### Document API

Exposes:

- read/write access to the working `SectorTopologyMap` through deliberate
  mutation services
- document identity and dirty status where needed
- load/save/new/reset helpers in the document module

Must not expose:

- arbitrary `SectorEditor&`
- renderer internals
- async bake worker internals

Current functions that could move behind it:

- `ResetToBlankMap()`
- `LoadLevel()`
- `SaveLevelFromModal()`
- `RefreshLevelList()`
- `OpenSaveLevelModal()`
- `OpenLoadLevelModal()`

Migration priority: Medium. Move files to `document/` before inventing a full
document API.

### Mutation API

Exposes:

- `markTopologyDocumentEdited(status)`
- `finishTopologyActionResult(result)`
- `finishTopologyMaterialMutation(status, assets)`
- `finishMaterialActionResult(result, assets)`
- `rebuildPreviewMeshesPreservingView(context)`
- `refreshRuntimeObjectsAfterAuthoringEdit()`

Must not expose:

- direct cache internals unless specifically needed
- direct renderer mesh data
- source-hash implementation details

Current functions that could move behind it:

- `MarkTopologyDocumentEdited()`
- `FinishTopologyActionResult()`
- `FinishTopologyMaterialMutation()`
- `FinishMaterialActionResult()`
- `RebuildPreviewMeshesPreservingView()`

Migration priority: High. This is the most important narrow boundary because it
protects cache invalidation and preview refresh semantics.

### Selection API

Exposes:

- selected object/light/surface accessors
- selection commands
- clear stale selection
- pick target selection

Must not expose:

- all UI state
- unrelated modal state

Current functions that could move behind it:

- `SelectTopologySector()`, `SelectTopologyLineDef()`,
  `SelectRuntimeObject()`, `SelectAuthoringLine()`, `ClearSelection()`
- `SelectedRuntimeObject()`, `SelectedTopologyLight()`, etc.

Migration priority: Medium/High. Needed by most tool modules, but it is easy to
make too broad.

### Render/Preview API

Exposes:

- invalidate 2D render cache
- update cached runtime object draw
- request preview mesh rebuild
- refresh dynamic light sources
- preview pose/control helpers if needed

Must not expose:

- generated mesh internals
- GPU resource ownership
- renderer private state

Current functions that could move behind it:

- `InvalidateTopologyRenderCache()`
- anonymous `UpdateCachedRuntimeObjectDraw()`
- `RebuildPreviewMeshesPreservingView()`
- `RefreshPreviewObjectProbeDebugData()`
- `preview.RefreshDynamicLightSources()`

Migration priority: High for cache invalidation; Medium for preview.

### Asset/Texture API

Exposes:

- editor texture handle lookup
- open texture picker for a specific target
- map texture catalog add/import operations
- sprite picker open/apply operations where needed

Must not expose:

- unrestricted `AssetManager` where a tool only needs texture IDs
- GPU resource creation outside the asset manager

Current functions that could move behind it:

- `EditorTextureHandleForId()`
- `OpenTopologyTexturePicker()`
- `OpenTopologySideDefTexturePicker()`
- `OpenMapSkyTexturePicker()`
- `OpenSelectedDoorTexturePicker()`
- `OpenSelectedBillboardSpritePicker()`

Migration priority: Medium.

### UI/Input API

Exposes:

- current frame input where needed
- UI context/config/font handles for draw handlers
- shared row helpers and measuring helpers

Must not expose:

- every field of `SectorEditorUiState` forever
- global modal state unrelated to the feature

Current functions/files that could move behind it:

- `SectorEditorUiHelpers.*`
- inspector input state subsets from `SectorEditorUiState`

Migration priority: Medium. Start feature-specific; split `SectorEditorUiState`
only when a tool folder needs it.

### Command/Modal API

Exposes:

- open confirmation modal
- close modal
- open feature-specific modal through feature callbacks
- queue simple command callbacks

Must not expose:

- all modal states
- direct `SectorEditor` method calls

Current functions that could move behind it:

- `OpenConfirmation()`
- `OpenConfirmationModal()`
- feature modal open wrappers

Migration priority: Low/Medium. Existing modal callbacks are already adequate
for the pilot.

### Status/Diagnostics API

Exposes:

- status text update
- warning/error reporting
- lightmap/source-hash diagnostic text where appropriate

Must not expose:

- global editor state
- long-lived references to transient UI strings

Current fields/functions:

- `statusText`
- `state.topologyDocumentStatus`
- `state.topologyRenderWarning`
- `state.sectorCollisionWorldWarning`
- `state.runtimeObjects.*Status`

Migration priority: Low.

## Tool/Module Inventory

| Module | Purpose | Current code/functions/files involved | Current owner | Target folder | Dependencies needed | Difficulty | Task type | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| select | pick/select/cycle/drag arm routing | `UpdateSelectDragArm`, `StartSelectDrag`, `BuildSelectPickCandidates`, `SelectPickTarget` | `SectorEditor.cpp` | `tools/select/` | selection API, picking helpers, per-tool drag callbacks | High | Audit first | Select routes into many other tools; do after pilot. |
| line | authoring line chain | `CancelPendingAuthoringLine`, `AddAuthoringLinePoint`, `DrawPendingAuthoringLine`, `ClickSectorEditorAuthoringLineTool` | `SectorEditor.cpp`, `SectorEditorAuthoringState.*` | `tools/line/` | mutation API, authoring graph API, coordinate conversion | Medium | Codex task after shared APIs | Keep "Authoring Line" UI text until a rename task. |
| rectangle | authoring rectangle | `CancelPendingAuthoringRectangle`, `AddAuthoringRectanglePoint`, `DrawPendingAuthoringRectangle`, rectangle helpers | `SectorEditor.cpp`, `SectorEditorAuthoringState.*` | `tools/rectangle/` | mutation API, authoring graph API, coordinate conversion | Medium | Codex task | Separate from line. |
| insert vertex | split authoring line | `BeginPendingAuthoringInsertVertex`, `CommitAuthoringInsertVertex`, `DrawPendingAuthoringInsertVertex`, insert helper | `SectorEditor.cpp`, `SectorEditorAuthoringState.*` | `tools/insert_vertex/` | authoring graph API, picking, status | Medium | Codex task | Good simple authoring split after line/rectangle. |
| runtime objects | billboard/door authoring, inspector, modal, drag | `SectorEditorRuntimeObjectInspector.*`, `Modals.*`, `Actions.*`, `Drag.*` | already extracted category files | `tools/runtime_objects/` | state, mutation callbacks, texture picker callbacks, runtime refresh | Low | Codex task | Best pilot because contexts/callbacks already exist. |
| materials/sidedef/decal | material and decal editing | `SectorEditorMaterialActions.*`, `SectorEditorMaterialModals.*`, `DrawTopologySideDefInspector`, texture picker material routing | mixed | `tools/materials/` | material mutation API, texture picker API, authoring/topology routing | Medium/High | Audit first or narrow Codex task | Cache invalidation and preview rebuild paths must be preserved. |
| lights/static/spot/probes | light placement, drag, inspectors, object-probe-adjacent settings | `SectorEditorLightInspector.*`, light add/delete/drag functions, spotlight pilot, preview settings object probe fields | mixed | `tools/lights/` | mutation API, source-hash knowledge, preview dynamic-light refresh | High | Audit first / Runner plan | Static lights and directional/object-probe settings affect lightmap hash. |
| preview overlays/UV | preview overlay/debug UI and UV panel | `DrawPreviewOverlay`, `DrawPreviewUvPanel`, overlay draw functions, `SectorEditorPreviewActions.*` | mixed | `tools/preview/` | preview renderer facade, UI/input, selected surface/material APIs | High | Audit first / Defer broad move | Renderer calls stay behind `SectorMeshRenderer`. |
| document workflows | new/load/save/reset and document modals | `SectorEditorDocumentActions.*`, `SectorEditorDocumentModals.*`, `ResetToBlankMap`, `LoadLevel`, `SaveLevelFromModal` | mixed | `document/` | document API, renderer refresh, runtime object reset, texture refresh | Medium | Codex task | Document is not a tool. |
| texture/catalog workflows | map texture import/catalog/pickers | `SectorEditorTextureActions.*`, `SectorEditorTextureModals.*`, add map texture wrappers | mixed | split between `shared/texture` or `tools/materials` as needed | asset manager, texture registry, picker callbacks | Medium | Audit first | Do not create one giant texture module that absorbs feature picker routing. |

## Runtime Objects Pilot Plan

Correction note: REF-041 refined this recommendation. The target is no longer
one flat `tools/runtime_objects/` folder. Use common placed-object
infrastructure in `tools/placed_objects/` plus concrete feature folders such as
`tools/billboards/` and `tools/doors/`. This keeps the document/storage family
separate from editor features and avoids a runtime-object sub-landfill as more
placed object kinds are added.

Runtime objects are still the best pilot after REF-021, but the pilot is now a
folder-boundary migration rather than more extraction from `SectorEditor.cpp`.
REF-021 already created the coherent runtime-object pieces:

- `SectorEditorRuntimeObjectInspector.*`
- `SectorEditorRuntimeObjectModals.*`
- `SectorEditorRuntimeObjectActions.*`
- `SectorEditorRuntimeObjectDrag.*`

Move exactly those files to:

```text
sources/sector_editor/tools/runtime_objects/
```

API boundary:

- Keep `SectorEditorRuntimeObjectActionContext`,
  `SectorEditorRuntimeObjectDragContext`,
  `SectorEditorRuntimeObjectInspectorCallbacks`, and
  `SectorEditorDoorTextureSettingsModalCallbacks`.
- Continue to pass callbacks for selection, mutation, texture picker opening,
  modal opening, deletion, cached draw update, dirty marking, and runtime object
  refresh.
- Do not pass `SectorEditor&`.
- Do not include `SectorEditor.h`.
- Do not add a registry in the pilot unless include/routing pressure demands a
  tiny local module descriptor.

What remains in `SectorEditor.cpp` after the pilot:

- building runtime object contexts
- routing active tool click to add billboard/door objects
- wrapper methods that preserve current public/private editor API
- opening texture/sprite pickers
- selected runtime object accessor until a selection API exists
- global document dirty/cache/preview/runtime refresh orchestration

What must not change:

- runtime object placement behavior
- door placement behavior
- runtime object drag behavior
- inspector layout and modal behavior
- sprite picker and texture picker behavior
- ECS runtime object refresh behavior
- document dirty status and topology render-cache invalidation behavior

Callbacks should be preserved by keeping `SectorEditor` as the context builder.
The moved files should depend on shared editor state/types and the sector
runtime/topology data they already use, but not on `SectorEditor`.

Verification:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Manual smoke recommended after implementation: place billboard, place door,
  drag object, edit billboard sprite/clip, edit door texture settings, delete
  object, enter preview and confirm runtime objects respawn.

## Document Module Plan

Move:

- `SectorEditorDocumentActions.*`
- `SectorEditorDocumentModals.*`

to:

```text
sources/sector_editor/document/
```

Document owns the data being edited and document lifecycle helpers, but document
is not a tool. Save/load/new/reset workflows affect the whole editor:
selection, authoring derivation, texture scopes, runtime object ECS state,
preview renderer resources, collision world, modal state, and dirty flags. That
orchestration should stay central until there is a dedicated document
controller boundary.

Save/load/new/reset should interact with tools by cancellation and state reset,
not by each tool owning document lifecycle. A document reset/load should clear
tool transient state, close relevant modals, refresh editor textures, reset
runtime objects, invalidate the 2D render cache, and rebuild preview/collision
state as the current code does.

The document module should not become a dumping ground for feature-specific
confirmation flows. Generic confirmation modal state may remain shared; feature
modals should live with their feature.

Completion note: REF-042 moved document actions/modals into
`sources/sector_editor/document/`. Document remains a non-tool module, and
high-level lifecycle orchestration remains central in `SectorEditor`.

## Authoring Tool Split Plan

Do not create one giant `authoring/` module unless a later audit proves that
the shared graph backend cannot be split. The current split should be:

- `tools/select/`
- `tools/line/`
- `tools/rectangle/`
- `tools/insert_vertex/`

Current functions involved:

- Select: `UpdateSelectDragArm()`, `ArmSelectedSelectDrag()`,
  `StartSelectDrag()`, `CurrentPickSelectionTarget()`,
  `BuildSelectPickCandidates()`, `SelectPickTarget()`,
  `FindSelectedMovablePickTargetAtScreenPoint()`
- Line: `CancelPendingAuthoringLine()`, `AddAuthoringLinePoint()`,
  `DrawPendingAuthoringLine()`, `ClickSectorEditorAuthoringLineTool()`,
  `CancelSectorEditorAuthoringLineToolChain()`
- Rectangle: `CancelPendingAuthoringRectangle()`,
  `AddAuthoringRectanglePoint()`, `DrawPendingAuthoringRectangle()`,
  `CreateSectorAuthoringRectangle()`,
  `AddSectorEditorAuthoringRectangle()`
- Insert vertex: `CancelPendingAuthoringInsertVertex()`,
  `BeginPendingAuthoringInsertVertex()`,
  `TryResolveAuthoringInsertVertexPoint()`,
  `UpdatePendingAuthoringInsertVertex()`,
  `CommitAuthoringInsertVertex()`,
  `DrawPendingAuthoringInsertVertex()`,
  `InsertSectorEditorAuthoringVertexOnLine()`

`SectorEditorAuthoringState.*` currently mixes authoring selection, picking,
line/rectangle/insert-vertex mutation, derivation refresh, material mapping, and
authoring material mutation. It should remain shared for the first tool-folder
migrations. Split it only after the tool modules show where the backend seams
are. Good later splits would be graph selection/picking, line tool backend,
rectangle tool backend, insert-vertex backend, derivation refresh, and
authoring material mapping.

Migration order:

1. Move runtime objects first to prove folder layout and include updates.
2. Move document files.
3. Extract the line tool wrapper code using existing authoring backend helpers.
4. Extract rectangle.
5. Extract insert vertex.
6. Extract select last because it routes to other tools and drag systems.

REF-043a progress: introduced a minimal v0 tool module contract and made
Authoring Line the first migrated authoring tool under `tools/line/`. Rectangle,
insert vertex, and select remain future migrations. The contract is deliberately
small and expected to evolve as the remaining authoring tools move.

Risks:

- pending tool cancellation can be subtly changed
- `currentTool` routing and tool availability in graph-authoritative mode can
  regress
- cache invalidation must remain via authoring graph mutation helpers and
  document-edited paths
- selection and hover behavior must match drawn cached/immediate overlays

## Materials Tool Plan

Material/sidedef/decal editing should become `tools/materials/`, not a generic
actions/modals bucket.

Current files/functions:

- `SectorEditorMaterialActions.*`
- `SectorEditorMaterialModals.*`
- `DrawTopologySideDefInspector()`
- material wrappers in `SectorEditor.cpp`: copy/paste, UV apply, decal apply,
  tint modal open/apply, clear, fit, align
- texture picker routing in `OpenTopologyTexturePicker()`,
  `OpenTopologySideDefTexturePicker()`, `ApplyTexturePickerSelection()`
- authoring material bridge functions:
  `ApplyAuthoringSideMaterialAction()`,
  `ApplyAuthoringFaceAnchorFlatMaterialAction()`,
  `FinishAuthoringSideMaterialActionResult()`

`SectorEditorMaterialActions.*` is already the backend action seam. It should
move into the materials tool folder eventually. `SectorEditorMaterialModals.*`
should move with it because decal tint is material-specific. If a
`SectorEditorMaterialInspector.*` is added, it should own the SideDef/material
inspector UI rather than growing `SectorEditor.cpp`.

Texture picker relationship:

- The generic picker UI can remain shared.
- Material-specific picker target routing should live with materials once the
  texture API exists.
- Texture catalog import/add-map-texture workflows are not material-specific and
  should not be buried in `tools/materials/`.

Authoring-vs-topology routing:

- The materials tool must preserve the current distinction between topology
  material mutation and authoring graph material mutation.
- `FinishTopologyMaterialMutation()` and `FinishMaterialActionResult()` protect
  dirty flags, cache invalidation, UI reset, and preview rebuild behavior.
- Authoring material actions must continue to refresh the derived topology and
  map results back to authoring graph data.

Migration risk: Medium/High. A narrow inspector move is feasible, but broad
texture picker routing should be audited first.

## Lights Tool Plan

Lights need a careful plan because several settings are lightmap
source-hash-sensitive.

Source-hash-sensitive:

- static point lights
- static spotlights
- directional sun/moon light settings
- lightmap AO and indirect bounce settings
- object probe spacing/height settings
- geometry-affecting `ceilingSky`

Source-hash-neutral or visual-only:

- sky visual settings
- preview controller settings
- object probe debug draw distance
- dynamic lights, unless a future task changes bake semantics

Current areas:

- `SectorEditorLightInspector.*` owns selected static/dynamic light and
  spotlight inspector UI.
- `SectorEditor.cpp` owns light placement, deletion, drag, hover/picking,
  spotlight pilot, preview overlay display, preview settings apply, and
  lightmap bake orchestration.
- `ComputeSectorLightmapSourceHash()` in `SectorLightmap.cpp` includes
  directional settings, lightmap object probe settings, `ceilingSky`, static
  lights, and static spotlights.

Static vs dynamic lights:

- Static lights and static spotlights affect baked light and source hash.
- Dynamic lights affect runtime preview rendering and dynamic light renderer
  sources, but should not affect baked light hash under current rules.

Object probe settings:

- Object probe spacing/height live in lightmap settings and affect bake/source
  hash.
- Object probe debug overlay distance is preview/debug behavior and should stay
  source-hash-neutral.

Directional light settings:

- Directional settings affect baked lighting and source hash.
- They currently live in the preview settings modal flow, which mixes visual
  preview settings, sky settings, directional light, and object probe settings.

What needs audit first:

- moving `ApplyPreviewSettingsModal()` because it touches preview settings,
  sky settings, directional light, and object probe lightmap settings in one
  flow
- moving spotlight pilot because it is both light-owned and preview-pose-owned
- moving light drag/actions if static/dynamic refresh paths are split

What should use a runner plan:

- broad lights/source-hash module migration
- any task that moves static light mutation together with bake lifecycle or
  preview settings apply
- any task that changes what the lightmap source hash includes

## Preview Tool/Module Plan

Preview-related editor modules should be split cautiously.

Stay central for now:

- preview mode entry/exit
- renderer resource lifecycle
- preview mesh rebuild orchestration
- collision world rebuild and gameplay controller lifecycle
- high-level calls into `SectorMeshRenderer`

Move later to `tools/preview/`:

- preview overlays and debug UI after a focused audit
- preview UV panel
- preview settings modal draw code
- preview action helpers already in `SectorEditorPreviewActions.*`

Spotlight pilot:

- It is a candidate for `tools/lights/` or `tools/preview/`, but it couples
  selected lights to preview camera pose. Audit it before moving.

Renderer calls:

- `SectorMeshRenderer` should remain in `sources/sector_demo/renderer/`.
- Tools may request preview rebuild or query selected surface information
  through a preview API; they should not own renderer resources.

Deferred:

- full preview facade rewrite
- renderer resource lifetime changes
- generated geometry ownership changes
- collision/physics changes

## Migration Roadmap

### Phase 0: Design/backlog

Goal: complete this report and update backlog.

Likely files touched:

- `docs/audit/sector_editor_tool_module_boundary_design.md`
- `docs/plans/codebase_refactor_backlog.md`

Risk: Low.

Verification: `git diff --check`, `git diff --stat`, `git status --short`.

Manual smoke needs: none.

Task type: Audit/design.

### Phase 1: Runtime objects pilot

Goal: move already extracted runtime object modules into
`tools/runtime_objects/`.

Likely files touched:

- runtime object editor files
- include paths in editor files and CMake only if required by test source lists

Risk: Low/Medium.

Verification: build, ctest, diff checks; manual runtime object smoke.

Task type: Codex task.

### Phase 2: Document folder

Goal: move document action/modal files into `document/`.

Likely files touched:

- document action/modal files
- include paths

Risk: Medium because document lifecycle touches renderer/runtime reset.

Verification: build, ctest, diff checks; manual new/load/save/reload smoke.

Task type: Codex task.

### Phase 3: Simple tool folders

Goal: move small, low-coupling tool wrappers once shared context patterns are
clear.

Likely files touched:

- line/rectangle/insert-vertex wrapper files
- shared toolbox declarations if needed

Risk: Medium.

Verification: build, ctest, manual 2D authoring smoke.

Task type: Codex task.

### Phase 4: Authoring tool split

Goal: split authoring tool behavior into select, line, rectangle, insert vertex
without one authoring landfill.

Likely files touched:

- `SectorEditor.cpp`
- `SectorEditorAuthoringState.*`
- new tool folders

Risk: Medium/High.

Verification: build, ctest, manual selection/line/rectangle/insert-vertex smoke.

Task type: Audit first for select; Codex tasks for narrow line/rectangle/insert
vertex slices.

### Phase 5: Materials

Goal: move material/decal/sidedef editing into `tools/materials/`.

Likely files touched:

- material action/modal files
- SideDef inspector
- texture picker routing

Risk: Medium/High.

Verification: build, ctest, material/decal/texture picker smoke. Mention cache
invalidation in each implementation final report.

Task type: Audit first, then Codex tasks.

### Phase 6: Lights

Goal: design and migrate light/static-light/spotlight/object-probe editor flows
without breaking source-hash semantics.

Likely files touched:

- light inspectors/actions/drag
- preview settings apply
- possibly lightmap tests

Risk: High.

Verification: build, ctest, source-hash tests, manual light/preview/bake smoke.

Task type: Audit first or runner plan.

### Phase 7: Preview

Goal: move preview overlays/UV panel/settings draw logic into preview module
while keeping renderer orchestration central.

Likely files touched:

- preview action/settings files
- overlay/UV panel code
- `SectorEditor.cpp`

Risk: High if broad.

Verification: build, ctest, manual preview render/overlay/UV smoke.

Task type: Audit first; defer broad renderer-coupled moves.

## Recommended First Implementation Task

First implementation task:

Move runtime object editor modules into:

```text
sources/sector_editor/tools/runtime_objects/
```

Exact files to move:

- `sources/sector_editor/SectorEditorRuntimeObjectInspector.h`
- `sources/sector_editor/SectorEditorRuntimeObjectInspector.cpp`
- `sources/sector_editor/SectorEditorRuntimeObjectModals.h`
- `sources/sector_editor/SectorEditorRuntimeObjectModals.cpp`
- `sources/sector_editor/SectorEditorRuntimeObjectActions.h`
- `sources/sector_editor/SectorEditorRuntimeObjectActions.cpp`
- `sources/sector_editor/SectorEditorRuntimeObjectDrag.h`
- `sources/sector_editor/SectorEditorRuntimeObjectDrag.cpp`

What not to move:

- `SectorRuntimeObjects.*`
- renderer door/billboard files
- texture picker implementation
- document files
- material files
- light files

Include updates:

- update includes from `sector_editor/SectorEditorRuntimeObject...` to
  `sector_editor/tools/runtime_objects/SectorEditorRuntimeObject...`
- keep the moved files free of `SectorEditor.h`
- keep existing context/callback structs

Verification:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

Manual smoke checklist:

- place billboard
- place door
- drag runtime object
- edit billboard sprite/clip
- edit door texture/settings
- delete runtime object
- enter preview and verify runtime objects appear

## Backlog Update

REF-040 should be marked complete after this report is created. The backlog
should also be adjusted so future work is feature-folder oriented:

- Runtime objects pilot move to `tools/runtime_objects/`
- Document folder move to `document/`
- Authoring split into select, line, rectangle, insert vertex
- Materials tool migration
- Lights/source-hash-sensitive audit or runner plan
- Preview tool/module migration

Future implementation items should not be marked complete by this design task.

## Appendix: Evidence

Commands used:

```text
wc -l sources/sector_editor/SectorEditor.cpp sources/sector_editor/*.h sources/sector_editor/*.cpp
find sources/sector_editor -maxdepth 2 -type f | sort
rg -n "SectorEditorRuntimeObject|SectorEditorDocument|SectorEditorMaterial|SectorEditorTexture|SectorEditorPreview|SectorEditorLight|SectorEditorAuthoring|Draw.*Modal|Draw.*Inspector|Start.*Drag|Update.*Drag|Finish.*Drag|Cancel.*Drag" sources/sector_editor
rg -n "enum class.*Tool|currentTool|activeTool|SectorEditorTool|Tool::|tool" sources/sector_editor
rg -n "MarkTopologyDocumentEdited|InvalidateTopologyRenderCache|FinishTopologyMaterialMutation|FinishMaterialActionResult|MutateSelectedRuntimeObject|RefreshRuntimeObjectsAfterAuthoringEdit|RebuildPreview|RenderPreview|SectorMeshRenderer" sources/sector_editor
rg -n '#include ".*SectorEditor\.h"|SectorEditor\*|SectorEditor&|SectorEditor::' sources/sector_editor/SectorEditorRuntimeObject* sources/sector_editor/SectorEditorDocumentModals.* sources/sector_editor/SectorEditorMaterialModals.* 2>/dev/null || true
rg -n "^.*SectorEditor::" sources/sector_editor/SectorEditor.cpp
rg -n "ComputeSectorLightmapSourceHash|sourceHash|directionalLight|staticLights|staticSpotLights|objectProbe|previewSettings|skySettings|ceilingSky" sources/sector_demo/SectorLightmap.cpp sources/sector_demo/SectorLightmap.h sources/sector_demo/SectorTopologyMap.h sources/sector_editor/SectorEditor.cpp sources/sector_editor/SectorEditorLightInspector.cpp sources/sector_editor/SectorEditorPreviewSettingsModal.cpp
```

Selected grep results:

- `SectorEditorTool` currently includes `Select`, `AuthoringLine`,
  `AuthoringRectangle`, `AuthoringInsertVertex`, `AuthoringMove`,
  `RuntimeObject`, `Door`, static/dynamic light tools, and legacy `Move`.
- `SectorEditor.cpp` contains direct tool routing for authoring, lights, runtime
  objects, preview, materials, document workflows, and texture picker routing.
- Runtime object files use context/callback structs and did not match the grep
  for `#include ".*SectorEditor.h"`, `SectorEditor*`, `SectorEditor&`, or
  `SectorEditor::`.
- `MarkTopologyDocumentEdited()` calls `InvalidateTopologyRenderCache()`.
- `FinishTopologyMaterialMutation()` calls `MarkTopologyDocumentEdited()` and
  can rebuild preview meshes.
- `ComputeSectorLightmapSourceHash()` includes directional light settings,
  object probe lightmap settings, sector `ceilingSky`, static point lights, and
  static spotlights.

Current file line counts:

| File | Lines |
| --- | ---: |
| `SectorEditor.cpp` | 11,358 |
| `SectorEditor.h` | 438 |
| `SectorEditorTypes.h` | 263 |
| `SectorEditorUiHelpers.h` | 388 |
| `SectorEditorAuthoringState.cpp` | 2,198 |
| `SectorEditorHelpers.cpp` | 1,590 |
| `SectorEditorTopologyRenderCache.cpp` | 1,331 |
| `SectorEditorRuntimeObjectInspector.cpp` | 1,288 |
| `SectorEditorTextureActions.cpp` | 1,220 |
| `SectorEditorMaterialActions.cpp` | 1,104 |
| `SectorEditorLightInspector.cpp` | 784 |

Current module/file inventory:

- Document: `SectorEditorDocumentActions.*`,
  `SectorEditorDocumentModals.*`
- Runtime objects: `SectorEditorRuntimeObjectActions.*`,
  `SectorEditorRuntimeObjectDrag.*`,
  `SectorEditorRuntimeObjectInspector.*`,
  `SectorEditorRuntimeObjectModals.*`
- Materials: `SectorEditorMaterialActions.*`,
  `SectorEditorMaterialModals.*`
- Textures: `SectorEditorTextureActions.cpp`,
  `SectorEditorTextureModals.*`
- Lights: `SectorEditorLightInspector.*`
- Preview: `SectorEditorPreviewActions.*`,
  `SectorEditorPreviewSettingsModal.*`,
  `SectorEditorLightmapModal.*`
- Topology/cache: `SectorEditorTopologyActions.*`,
  `SectorEditorTopologyRenderCache.*`,
  `SectorEditorTopologyRenderCacheTypes.h`
- Shared types/helpers: `SectorEditorTypes.h`,
  `SectorEditorModalTypes.h`,
  `SectorEditorSelectionTypes.h`,
  `SectorEditorPreviewTypes.h`,
  `SectorEditorSurfaceTypes.h`,
  `SectorEditorHelpers.*`,
  `SectorEditorUiHelpers.*`

Current direct dependency findings:

- Tool-like extracted runtime object modules do not depend on `SectorEditor.h`.
- `SectorEditor.h` still includes runtime object action/drag headers to build
  contexts and expose private wrapper methods.
- Several helper/action headers still include the broad `SectorEditorTypes.h`
  because `SectorEditorState` and `SectorEditorUiState` remain broad
  aggregates.
- `SectorEditorPreviewActions.h` forward declares `SectorMeshRenderer`, which
  is a good example of avoiding unnecessary renderer header exposure.

Examples of category-landfill risk:

- `SectorEditorHelpers.*` mixes coordinate, picking, path, clamp, material
  target, texture, and tool label helpers.
- `SectorEditorMaterialActions.*` is already over 1,100 lines and could absorb
  every future material/decal/UV operation if not moved behind a feature folder.
- `SectorEditorTextureActions.*` mixes texture catalog/import, picker setup,
  sprite metadata, billboard picker, sky picker, topology picker, and runtime
  door picker concerns.
- `SectorEditorModalTypes.h` groups unrelated modal states, including texture,
  sprite, document, decal tint, door texture settings, and preview settings.

Examples of good existing callback/context boundaries:

- `SectorEditorRuntimeObjectActionContext` passes only state, status, selected
  callbacks, picking callbacks, and dirty marking.
- `SectorEditorRuntimeObjectDragContext` passes only state, status, coordinate
  callbacks, cached draw update, dirty marking, and runtime refresh.
- `SectorEditorRuntimeObjectInspectorCallbacks` decouples inspector UI from
  concrete editor methods.
- `SectorEditorDocumentModals.*` and `SectorEditorMaterialModals.*` draw through
  callback structs rather than calling `SectorEditor`.
- `SectorEditorPreviewActions.h` forward declares `SectorMeshRenderer`.

Examples of remaining `SectorEditor.cpp` responsibilities that should become
tool-owned:

- authoring line/rectangle/insert-vertex pending input and overlays
- select drag routing
- light placement/drag wrappers
- SideDef/material inspector body
- runtime object context construction can remain central, but runtime object
  behavior now belongs in a tool folder
- preview overlay and UV panel UI
- texture picker target routing once material/runtime/sky ownership is clearer

Cache invalidation note: this report does not change topology mutation code.
Future topology/tool migrations must preserve `MarkTopologyDocumentEdited()` /
`InvalidateTopologyRenderCache()` routes, and over-invalidation remains
acceptable.

Lightmap source-hash note: this report does not change lightmap code. Future
lights/materials/preview migrations must explicitly state whether source-hash
behavior changed; the intended behavior is unchanged.
