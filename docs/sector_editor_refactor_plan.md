# Sector Editor Refactor Plan

## Purpose

`sources/sector_editor/SectorEditor.cpp` is the main glue layer for the topology
sector editor. It is currently about 11,800 lines and mixes 2D editor input,
rendering, inspectors, modals, texture/material editing, preview integration,
document I/O, topology actions, lightmap bake control, and gameplay-preview
settings.

This document audits the current structure and proposes phased,
behavior-preserving refactors. It is not a request to implement the refactor
yet. Each phase is intended to compile and run independently and be reviewed on
its own.

## Current State Summary

The active editor format is topology v2 / linedef-based `SectorTopologyMap`.
Core topology, geometry, preview, collision, and lightmap backend logic already
exists outside the editor:

- `SectorTopologyCreation.*`, `SectorTopologyEdit.*`,
  `SectorTopologyValidation.cpp`, and `SectorTopologyMap.*` own topology
  creation/edit/index/validation primitives.
- `SectorGeneratedGeometry.*` owns topology-to-render-geometry generation,
  including sky-ceiling geometry rules.
- `SectorMeshPreview.*` owns 3D preview mesh/shader/sky/lightmap rendering
  resources.
- `SectorCollisionWorld.*` owns topology-based gameplay collision queries.
- `SectorLightmap.*` owns lightmap layout, bake, status, and source-hash logic.

`SectorEditor.cpp` should therefore be reduced by peeling off UI, render/cache,
modal, and editor-action glue, not by moving core topology behavior back into
the editor or inventing a new architecture.

## Responsibility Map

| Responsibility | Approximate locations / functions | Coupling | Kind | Extraction safety |
| --- | --- | --- | --- | --- |
| Local helpers and constants | anonymous namespace, roughly lines 30-1390; names, clamps, color helpers, geometry math, path/id validation, texture sorting, surface mapping | Low to medium; many helpers are pure, some reference editor/backend types | Pure helper / formatting / validation | Safe early if limited to stateless helpers |
| Editor lifecycle and orchestration | `Init`, `Shutdown`, `Update`, `Render`, `RenderUI` | High `SectorEditorState`, `SectorMeshPreview`, assets, input, UI | Orchestration | Keep in `SectorEditor.cpp` until later |
| Coordinate and layout helpers | `MapToScreen`, `ScreenToMap`, `Build*PanelRect`, `BuildCanvasRect`, `SnapMapPoint` | Medium state/view coupling | UI/layout helper | Safe only after render helpers have clear APIs |
| Canvas input and pending tools | `HandleCanvasInput`, vertex/light drag, vertex merge, line split at point, sector cut, pending sector creation/finalization | High state/input/topology coupling | Input state machine / mutation | Risky; defer until action helpers are clearer |
| Sector creation/finalization | `AddPendingSectorPoint`, `FinalizePendingSector`, `BuildPendingTopologyPoints`, `BuildTopologyCreateOptions`, `StartInsertSectorInside` | High `SectorEditorState` and `SectorTopologyMap` coupling | Action/mutation plus tool state | Defer; split only after action-result pattern exists |
| Selection and stale-state handling | `SelectedTopology*`, `ClearStaleTopologySelection`, `SelectTopology*`, `ClearSelection`, `ResetSurface3DUiState` | High state/UI-state coupling | Editor state helper | Medium risk; useful after inspectors shrink |
| Dirty/cache invalidation | `MarkTopologyDocumentEdited`, `InvalidateTopologyRenderCache`, `EnsureTopologyRenderCache`, direct dirty flag writes | High state coupling, critical policy | State/cache policy | Do not casually change; document before extracting |
| Topology mutation actions | delete sector/light, add light, set portal flags, split/dissolve/join, material actions | High topology/selection/status/cache coupling | Action/mutation | Extract late with explicit action results |
| Lightmap bake control | `BakeLightmaps`, `StartLightmapBake`, `PollLightmapBakeResult`, `InstallLightmapBakeResult`, `DrawLightmapBakeModal` | High state/thread/assets/filesystem/hash coupling | Document I/O / async action / UI | UI can move earlier; bake logic later |
| 3D preview integration | `TryEnterPreview3D`, `LeavePreview3D`, `UpdatePreview3D`, `RenderPreview3D*`, preview pose helpers | High preview, input, assets, gameplay state | Preview/render/gameplay glue | Medium-high; move after UI/cache |
| Gameplay preview settings | `TogglePreviewControlMode`, `RebuildSectorCollisionWorld`, `BuildGameplayVerticalContext`, `RefreshGameplaySectorAndVerticalContext`, `InitializeGameplayVerticalState` | High collision/preview/gameplay coupling | Preview/gameplay integration | Defer; behavior-sensitive |
| 2D render cache | `RebuildTopologyRenderCache`, `DrawTopologyDocument`, `DrawCachedTopologySector`, `DrawTopologyLineDefs`, `DrawTopologyVertices`, `DrawStaticLights` | Medium state/map/view coupling | Render/cache | Good early seam |
| 2D overlays | pending sector, vertex drag, light drag, split/cut/merge overlays, snap crosshair | High tool state and draw coupling | Immediate overlay rendering | Move after cache extraction |
| 2D picking/hit testing | `FindTopologySectorAt`, `FindTopologyLineNearScreenPoint`, `FindTopologyLightNearScreenPoint`, vertex and boundary-cut picking | Medium-high map/view coupling | Picking / geometry helper | Move after render cache or with dedicated picking helpers |
| Tools panel | `DrawToolsPanel` | High state/action coupling | UI panel | Medium; includes document, lightmap, preview commands |
| Inspector routing | `DrawSectorsPanel` | High selection/UI-state coupling | UI orchestration | Move after smaller inspector sections |
| Sector inspector | `DrawTopologySectorInspector` | High topology/material/light/cache coupling | UI plus mutation | Medium-high; split separately |
| Linedef/sidedef inspector | `DrawTopologySideDefInspector` | High topology/material/action coupling | UI plus mutation | Medium-high; split separately |
| Vertex inspector | inline inside `DrawSectorsPanel` | Medium selection/action coupling | UI plus action buttons | Safer than sector/sidedef |
| Static light inspector | inline inside `DrawSectorsPanel` | Medium static light mutation/hash coupling | UI plus mutation | Good first inspector candidate |
| Texture picker modal | `DrawTexturePickerModal`, `OpenTopologyTexturePicker`, `OpenTopologySideDefTexturePicker`, `OpenMapSkyTexturePicker`, `ApplyTexturePickerSelection` | Medium map/material/preview coupling | Modal UI plus action | Good early modal candidate |
| Add texture modal | `DrawAddMapTextureModal`, `OpenAddMapTextureModal`, scan/preview/add helpers | Medium assets/filesystem/map texture table coupling | Modal UI / asset helper | Good early modal candidate |
| Document modals | save/load/confirmation modal draw/open/apply helpers | Medium document I/O/state coupling | Modal UI / document action | Extract after texture modals |
| Preview settings modal | `DrawPreviewSettingsModal`, `OpenPreviewSettingsModal`, `ApplyPreviewSettingsModal` | Medium-high preview/sky/directional/gameplay coupling | Modal UI plus action | Split UI first; keep apply logic initially |
| Decal tint modal | `DrawDecalTintModal`, `OpenDecalTintModal`, decal tint apply helper | Medium material/action coupling | Modal UI plus action | Extract with modal/material phase |
| Material/UV/decal editing | `CurrentTextureForSurface`, `MutableUvForSurface`, copy/paste, UV apply/reset/fit/align, decal opacity/emissive/tint/bloom/clear, middle texture clear | High map/material/preview/cache coupling | Editor action helper | Extract after inspectors/modals shrink |
| Status and validation feedback | `statusText`, `state.topologyRenderWarning`, status panel | Broad coupling | UI feedback | Keep simple; move only with owning feature |

## Main Pain Points

- `SectorEditor.cpp` combines many unrelated responsibilities, so local changes
  require scanning a very large file.
- Long functions mix UI drawing, input handling, validation, topology mutation,
  preview rebuilding, and status messages.
- Modal state and modal actions live near unrelated render and topology code.
- Inspector sections directly mutate topology and reset UI state inline, making
  review of behavior-preserving changes difficult.
- Topology mutations rely on discipline around `MarkTopologyDocumentEdited()` and
  `InvalidateTopologyRenderCache()`.
- Hash-sensitive lightmap inputs are edited near visual-only preview and sky
  settings, which increases the risk of accidental source-hash regressions.
- Future feature work will likely make the file larger unless safe extraction
  seams are created first.

## Refactor Principles

- Keep `SectorEditor.cpp` as a thin-ish orchestration layer. It does not need to
  become empty.
- Preserve topology v2 / `SectorTopologyMap`; do not reintroduce the old
  polygon `SectorMap`.
- Use focused helper files with free functions and small data structs:
  UI helpers, editor action helpers, backend topology helpers, preview/render
  helpers, modal helpers.
- Avoid forced MVC/MVVM, polymorphic tool hierarchies, virtual update classes,
  command systems, or broad framework rewrites.
- Keep each phase behavior-preserving, compileable, runnable, and separately
  reviewable.
- Do not mix feature work into cleanup phases.
- Prefer extracting contiguous, low-coupling code before touching mutation-heavy
  tool state machines.

## Cache Invalidation Rules To Preserve

- `MarkTopologyDocumentEdited()` currently sets `topologyDocumentDirty`, sets
  `hasUnsavedChanges`, calls `InvalidateTopologyRenderCache()`, and updates
  `statusText` when supplied.
- Any mutation that changes live topology or visible cached 2D editor state must
  preserve that behavior.
- Over-invalidation is acceptable; missed invalidation is the bug.
- Direct `state.topologyMap` mutations must be audited in every refactor phase.
- Extracted action helpers must either call existing dirty/cache helpers or
  clearly return/document that the caller must do it.
- Texture registry changes currently mark the document dirty but do not need 2D
  render-cache invalidation because the 2D cache does not store texture display
  state.
- Baked lightmap metadata install marks the document dirty but should not
  invalidate the 2D topology render cache unless the 2D editor starts drawing
  baked lightmap data.
- Phases touching topology mutations must mention cache invalidation behavior in
  their final report.

## Lightmap / Source Hash Rules To Preserve

- `ComputeSectorLightmapSourceHash()` includes bake constants/settings,
  directional light settings, `SectorCoordSubdivisions`, referenced texture
  definitions, vertices, linedefs, sidedefs, sectors, `ceilingSky`, and static
  lights.
- Preview settings must remain excluded from the lightmap source hash.
- Sky visual settings must remain excluded from the lightmap source hash.
- `ceilingSky` must remain included because it changes generated/baked geometry.
- Directional light settings must remain included because they affect baked
  lighting.
- Static lights and bake settings must remain included.
- Refactors must not accidentally invalidate lightmaps for purely visual/editor
  settings.
- Middle textures currently receive baked light but do not cast baked shadows or
  occlude lightmap rays; cleanup phases must not change that behavior.
- Phases touching lightmaps, sky, `ceilingSky`, directional light, static
  lights, or bake data must document source-hash behavior in their final report.

## Proposed Phase Overview

Recommended first phase: **Phase 1: Extract Pure Helpers And Constants**.

This is the lowest-risk way to reduce file size and create helper-file patterns
without changing topology mutation, cache invalidation, preview behavior, or UI
flow. The next best early phase is the 2D topology render cache, because its
rebuild path is already a distinct contiguous block and the cache structs are
already separated in `SectorEditorTypes.h`.

## Phase 1: Extract Pure Helpers And Constants

### Goal

Move stateless helper functions and constants out of `SectorEditor.cpp`.

### Status

Completed: 2026-06-24

Summary:
- Moved stateless sector editor constants, labels, clamps, texture/path helpers, lightmap progress helpers, surface mapping helpers, tint/decal helpers, and small geometry helpers into `SectorEditorHelpers.h/.cpp`.
- Preserved editor UI flow, topology mutation behavior, preview behavior, serialization, and build registration.
- Build/tests passed.

Verification:
- `cmake --build cmake-build-debug -j2`: passed
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed
- `git diff --check`: passed
- Manual GUI verification: not performed

Notes:
- Cache invalidation behavior: unchanged
- Lightmap/source-hash behavior: unchanged
- Collision/gameplay behavior: unchanged

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- New helper files such as `sources/sector_editor/SectorEditorHelpers.h` and
  `sources/sector_editor/SectorEditorHelpers.cpp`
- Build files only if the project requires explicit source registration

### Behavior that must remain unchanged

- All editor UI, topology mutation, cache invalidation, lightmap hash behavior,
  preview behavior, and serialization.

### Implementation notes

- Move only helpers that do not mutate `SectorEditorState` or depend on
  `SectorEditor` methods.
- Good candidates include name/label helpers, clamp helpers, texture ID/path
  validation, tint/color helpers, simple surface-kind mapping, and small geometry
  helpers.
- Leave helpers in place if they directly access `state`, `uiState`, `preview`,
  or `statusText`.
- Keep namespace `game`; do not add nested subsystem namespaces.

### Tests / verification

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

Manual verification is optional for this phase because behavior should be
mechanical, but opening the editor and entering 2D/3D modes is useful.

### Risk level

Low.

### Non-goals

- Do not move inspectors, modals, tool state, render cache, or mutation actions.
- Do not change function names or semantics except where necessary for linkage.

### Final report expectations

- List helper categories moved.
- State that no topology mutation/cache invalidation behavior changed.
- State that no lightmap/source-hash behavior changed.

## Phase 2: Extract 2D Topology Render Cache

### Goal

Move 2D topology render-cache building and cached draw helpers into a focused
render/cache helper module.

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- New files such as `SectorEditorTopologyRenderCache.h/.cpp`

### Behavior that must remain unchanged

- `ValidateSectorTopologyMap()`, `ExtractSectorTopologyLoops()`,
  `BuildSectorTopologyIndexes()`, and `mapbox::earcut()` must remain outside the
  steady 2D frame draw path except when the cache is invalid/stale.
- Sector fills, outlines, labels, linedefs, vertices, static lights, selection
  highlights, warnings, and overlays must render as before.
- Picking behavior must remain consistent with what is drawn.

### Implementation notes

- Keep `InvalidateTopologyRenderCache()` and `EnsureTopologyRenderCache()` as
  `SectorEditor` orchestration initially.
- Extract the cache rebuild into a free helper that receives
  `const SectorTopologyMap&` and the render revision.
- Extract cached draw helpers only if their dependencies can be passed cleanly
  through map-to-screen callbacks or a small draw context.
- Do not cache live overlays such as pending sector, hover, drag, split, cut, or
  UI state in this phase.

### Tests / verification

- Standard build/test/diff commands.
- Manually verify 2D editor render: sector fills, holes, labels, linedefs,
  sidedef direction markers, vertices, selected sector/line/vertex/light,
  topology warnings, and static light radii.

### Risk level

Low-medium.

### Non-goals

- Do not change cache structs unless required for a clean helper API.
- Do not move topology mutation actions.
- Do not optimize or change triangulation behavior.

### Final report expectations

- Explicitly mention that cache invalidation still flows through
  `MarkTopologyDocumentEdited()` / `InvalidateTopologyRenderCache()`.
- Confirm expensive derived topology is not rebuilt every steady 2D draw frame.

## Phase 3: Extract Texture Picker And Add-Texture Modals

### Goal

Move texture picker and add-map-texture modal UI/actions into focused modal
helpers.

### Status

Completed: 2026-06-24

Summary:
- Moved texture picker and add-map-texture modal UI/state helpers into `SectorEditorTextureModals.h/.cpp`.
- Preserved picker target behavior, add/update texture behavior, preview texture scope handling, and modal input handling.
- Build/tests passed.

Verification:
- `cmake --build cmake-build-debug -j2`: passed
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed
- `git diff --check`: passed
- Manual GUI verification: not performed

Notes:
- Cache invalidation behavior: unchanged; texture registry edits still mark dirty/unsaved and refresh texture assets without invalidating the 2D topology render cache.
- Lightmap/source-hash behavior: unchanged; referenced texture definitions remain hash-affecting as before.
- Collision/gameplay behavior: unchanged.

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- New files such as `SectorEditorTextureModals.h/.cpp`

### Behavior that must remain unchanged

- Texture picker selection, cancel, Enter/Escape handling, preview image display,
  and selected index behavior.
- Add-texture scan, generated ID behavior, validation, preview asset scope,
  filter selection, and add/update behavior.
- Map sky texture picking from the preview settings modal must continue to edit
  the draft sky settings only.

### Implementation notes

- Keep `TexturePickerState` and `AddMapTextureState` as plain state structs.
- Prefer modal helper functions that accept explicit state/action callbacks
  rather than giving helpers ownership over `SectorEditor`.
- Texture registry edits should keep the current behavior: mark dirty/unsaved
  and refresh editor texture assets, but do not invalidate the 2D render cache
  unless the 2D cache starts storing texture/material display data.

### Tests / verification

- Standard build/test/diff commands.
- Manually verify picking floor, ceiling, sidedef, middle, decal, and sky
  textures.
- Manually verify adding/updating a map texture, preview filter changes, cancel,
  Enter, and Escape.

### Risk level

Medium.

### Non-goals

- Do not change texture registry schema.
- Do not change asset manager behavior.
- Do not add texture thumbnails or new picker features.

### Final report expectations

- State that source files outside the modal extraction were not refactored.
- State whether texture registry edits invalidate the 2D cache; expected answer
  is no unless display caching changes.
- State that lightmap hash behavior for referenced texture definitions remains
  unchanged.

## Phase 4: Extract Shared UI Control Helpers

### Goal

Extract repeated numeric input, color-channel, swatch, and compact row helpers
used by lights, ambient color, sky color, directional light color, UV controls,
and modal settings.

### Status

Completed: 2026-06-24

Summary:
- Moved shared labeled numeric input, RGB8 channel, normalized tint, and swatch helpers into `SectorEditorUiHelpers.h/.cpp`.
- Preserved caller-owned topology/material mutation policy, modal draft behavior, and RGB8-vs-normalized-tint representation boundaries.
- Build/tests passed.

Verification:
- `cmake --build cmake-build-debug -j2`: passed
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed
- `git diff --check`: passed
- Manual GUI verification: not performed

Notes:
- Cache invalidation behavior: unchanged
- Lightmap/source-hash behavior: unchanged
- Collision/gameplay behavior: unchanged

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- New files such as `SectorEditorUiHelpers.h/.cpp`

### Behavior that must remain unchanged

- Numeric ranges, clamp behavior, input state reset behavior, color alpha
  normalization, UI IDs, and status/dirty behavior.

### Implementation notes

- Helpers should collect/apply simple UI intent, not own topology mutation
  policy.
- Callers should still decide whether a change requires
  `MarkTopologyDocumentEdited()`, preview rebuild, collision rebuild, or no
  cache invalidation.
- Avoid over-general helpers that obscure the existing UI flow.

### Tests / verification

- Standard build/test/diff commands.
- Manually verify static light RGB, sector ambient RGB, sky top color,
  directional light color, and UV numeric controls.

### Risk level

Low-medium.

### Non-goals

- Do not move full inspectors or modals in this phase.
- Do not redesign UI layout.

### Final report expectations

- List helper categories extracted.
- Confirm no cache invalidation behavior changed.
- Confirm source-hash policy for light/sky controls did not change.

## Phase 5: Extract Preview Settings And Lightmap Bake Modals

### Goal

Move preview settings modal UI tabs and the lightmap bake modal UI out of
`SectorEditor.cpp`, while keeping apply/bake threading logic initially in
`SectorEditor`.

### Status

Completed: 2026-06-24

Summary:
- Moved preview settings modal UI into `SectorEditorPreviewSettingsModal.h/.cpp`.
- Moved lightmap bake modal UI into `SectorEditorLightmapModal.h/.cpp`.
- Preserved preview apply logic, sky texture picking, and lightmap bake threading/install logic in `SectorEditor`.
- Build/tests passed.

Verification:
- `cmake --build cmake-build-debug -j2`: passed
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed
- `git diff --check`: passed
- Manual GUI verification: not performed

Notes:
- Cache invalidation behavior: unchanged
- Lightmap/source-hash behavior: unchanged
- Collision/gameplay behavior: unchanged

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- New files such as `SectorEditorPreviewSettingsModal.h/.cpp` and
  `SectorEditorLightmapModal.h/.cpp`

### Behavior that must remain unchanged

- Preview settings draft/reset/cancel/apply behavior.
- General, sky, and lighting tab state.
- Sky texture picker integration.
- Bake progress display, cancel behavior, close/acknowledgement behavior, and
  input event consumption.

### Implementation notes

- Keep `ApplyPreviewSettingsModal()` in `SectorEditor` during this phase unless
  it can be moved without changing gameplay-preview behavior.
- Keep `StartLightmapBake()`, `PollLightmapBakeResult()`, and
  `InstallLightmapBakeResult()` in `SectorEditor` during this phase.
- Preserve that sky visual settings are map-level visual settings and are not
  included in the lightmap source hash.
- Preserve that directional light settings are hash-affecting.

### Tests / verification

- Standard build/test/diff commands.
- Manually verify opening settings from 2D and 3D, switching tabs, reset
  defaults, cancel, OK, sky texture picking, and bake modal cancel/close.

### Risk level

Medium.

### Non-goals

- Do not rewrite lightmap bake threading.
- Do not change preview settings schema.
- Do not change lightmap hash calculation.

### Final report expectations

- Mention source-hash behavior for preview settings, sky settings, and
  directional light.
- State whether preview/collision/gameplay behavior changed; expected answer is
  no.

## Phase 6: Extract Inspectors One At A Time

### Goal

Move inspector sections out of `SectorEditor.cpp` gradually, starting with the
lowest-coupling sections.

### Status

Completed: 2026-06-24

Summary:
- Moved the static-light inspector into `SectorEditorLightInspector.h/.cpp`.
- Moved the topology vertex and vertex-merge summary inspector into `SectorEditorVertexInspector.h/.cpp`.
- Preserved sector and sidedef inspectors in `SectorEditor.cpp` for later Phase 6 passes; build/tests passed.

Verification:
- `cmake --build cmake-build-debug -j2`: passed
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed
- `git diff --check`: passed
- Manual GUI verification: not performed

Notes:
- Cache invalidation behavior: unchanged; moved light edits still route through `MarkTopologyDocumentEdited()`, and moved vertex actions still use existing topology mutation methods.
- Lightmap/source-hash behavior: unchanged; static lights remain hash-affecting through existing lightmap hash code.
- Collision/gameplay behavior: unchanged.

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- New files such as `SectorEditorLightInspector.h/.cpp`,
  `SectorEditorVertexInspector.h/.cpp`, `SectorEditorSectorInspector.h/.cpp`,
  and `SectorEditorSideDefInspector.h/.cpp`

### Behavior that must remain unchanged

- Inspector scroll height and scroll state behavior.
- Selection routing and stale-selection cleanup.
- UI IDs, input states, status text, dirty state, cache invalidation, preview
  rebuilds, and collision rebuilds.

### Implementation notes

- Suggested order:
  1. Static light inspector.
  2. Vertex/line summary/action section.
  3. Sector inspector.
  4. Linedef/sidedef inspector.
- Treat sector and sidedef inspectors as separate implementation tasks because
  they are larger and contain material/UV/decal actions.
- If a helper mutates topology directly, it must preserve existing dirty/cache
  behavior.

### Tests / verification

- Standard build/test/diff commands.
- Manually verify static light edits/deletion/bake prompt, vertex dissolve/merge
  actions, line split/join actions, sector heights/sky/ambient/materials, and
  sidedef wall/lower/upper/middle controls.

### Risk level

Medium-high overall; low-medium for static light and vertex sections.

### Non-goals

- Do not redesign inspector layout.
- Do not combine feature work with inspector extraction.
- Do not change topology or material schema.

### Final report expectations

- State exactly which inspector section moved.
- Mention cache invalidation behavior for any moved topology mutation paths.
- Mention lightmap source-hash behavior for lights, `ceilingSky`, materials, or
  bake settings if touched.

## Phase 7: Extract Material / UV / Decal Action Helpers

### Goal

Move surface target lookup and material/UV/decal editing actions into focused
editor action helpers.

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- New files such as `SectorEditorMaterialActions.h/.cpp`

### Behavior that must remain unchanged

- 2D and 3D material selection behavior.
- Copy/paste, UV apply/reset/fit/align, decal opacity/emissive/tint/bloom/clear,
  middle texture clear, and texture picker apply behavior.
- Preview mesh rebuild behavior in 3D.
- Dirty/cache invalidation behavior.

### Implementation notes

- Prefer small action result structs such as success/failure, status text,
  needs preview rebuild, needs collision rebuild, and needs cache invalidation.
- Keep the caller responsible for mode-specific preview rebuild if that avoids
  passing too much editor state.
- Avoid changing lightmap behavior for middle textures or decals.

### Tests / verification

- Standard build/test/diff commands.
- Manually verify floor/ceiling/default wall/lower/upper/sidedef/middle
  materials, decal assignment and clearing, UV reset/fit/align in 2D and 3D.

### Risk level

Medium-high.

### Non-goals

- Do not add new material features.
- Do not add alpha-aware shadows.
- Do not change generated geometry or lightmap bake rules.

### Final report expectations

- Mention material cache invalidation behavior.
- Mention source-hash implications for texture/UV/material changes.
- State whether preview mesh rebuild behavior changed; expected answer is no.

## Phase 8: Extract Document And Preview Integration Helpers

### Goal

Move New/Load/Save/Reload helpers and 3D preview/gameplay integration helpers
into focused modules after lower-risk UI/render extractions are complete.

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- New files such as `SectorEditorDocumentActions.h/.cpp` and
  `SectorEditorPreviewActions.h/.cpp`

### Behavior that must remain unchanged

- Asset scope cleanup, preview shutdown, level path rules, save/load/reload modal
  flow, dirty flags, current level path/name state, and status text.
- 3D preview rebuild, pose preservation, free-fly/gameplay switching, collision
  world rebuild, and visual-only camera effects.

### Implementation notes

- Keep `SectorEditor` owning `SectorMeshPreview`, asset scopes, and top-level
  mode transitions unless a narrow ownership move is clearly safe.
- Preserve that visual-only step smoothing, headbob, and landing dip do not feed
  collision, sector lookup, or physics.
- Preserve topology-based collision via `SectorCollisionWorld`; do not use render
  triangles for gameplay collision.

### Tests / verification

- Standard build/test/diff commands.
- Manually verify new, save, load, reload, unsaved-change confirmations,
  entering/leaving 3D, free-fly/gameplay toggle, and preview settings apply.

### Risk level

Medium-high.

### Non-goals

- Do not redesign `SectorEditorState`.
- Do not change collision, physics, or camera semantics.
- Do not change level JSON schema.

### Final report expectations

- State whether collision/sector lookup/physics changed; expected answer is no.
- State cache invalidation behavior for load/reset.
- State no source-hash behavior changed unless lightmap settings were touched.

## Phase 9: Extract Topology Mutation Wrappers And Invalidation Pattern

### Goal

Consolidate topology action wrappers that call backend topology edit functions
and make dirty/cache/selection cleanup expectations explicit.

### Files likely touched

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorTypes.h`
- New files such as `SectorEditorTopologyActions.h/.cpp`

### Behavior that must remain unchanged

- Vertex move, merge, dissolve.
- Linedef split and split-at-point.
- Sector create, insert, cut, delete, join.
- Static light add/delete and edits.
- Portal player-blocking edits and collision rebuild.
- Selection after each action, status text, pending-tool cancellation, UI input
  state reset, cache invalidation, and preview rebuild behavior.

### Implementation notes

- Do this after render cache, modals, inspectors, and material helpers are
  smaller.
- Do not add a command buffer or undo system.
- Use backend topology functions already present in `SectorTopologyEdit.*` and
  `SectorTopologyCreation.*`.
- Each extracted mutation helper must either call `MarkTopologyDocumentEdited()`
  or return a result requiring the caller to call it.

### Tests / verification

- Standard build/test/diff commands.
- Manually exercise every topology action: create sector, insert inside, move
  vertex, merge vertices, split line, split line at point, cut sector, dissolve
  vertex, delete sector, join sectors, add/delete/move light, and portal
  blocking.

### Risk level

High.

### Non-goals

- No command system, undo/redo, tool hierarchy, or topology semantic changes.
- Do not refactor pending tool state machines in the same phase.

### Final report expectations

- Include a cache invalidation audit for every moved mutation path.
- Mention source-hash behavior for geometry, `ceilingSky`, materials, static
  lights, directional light, and bake settings if any are touched.
- State whether collision/sector lookup/physics changed; expected answer is no
  unless a specific collision bugfix was scoped.

## Deferred / Do Not Do Yet

- Do not rewrite pending tool state machines until surrounding rendering,
  inspector, modal, and action helpers are smaller.
- Do not introduce a command system, undo/redo, MVC/MVVM framework, polymorphic
  editor tools, virtual update classes, or a broad `SectorEditorState` redesign.
- Do not split `SectorEditor.cpp` into many files in one phase.
- Do not change topology schema, lightmap hash policy, collision behavior,
  generated geometry behavior, or serialization style as part of cleanup.
- Do not move old polygon `SectorMap` concepts back into the editor.

## Phase Execution Tracking

After a refactor phase is successfully executed, update this document as part
of that same phase. Keep the update small, factual, and local to the completed
phase.

For each completed phase, add a short status block under that phase:

```md
### Status

Completed: YYYY-MM-DD

Summary:
- Moved/extracted ...
- Preserved ...
- Build/tests passed ...

Verification:
- `cmake --build cmake-build-debug -j2`: passed
- `ctest --test-dir cmake-build-debug --output-on-failure`: passed
- `git diff --check`: passed
- Manual GUI verification: performed / not performed

Notes:
- Cache invalidation behavior: unchanged / updated as described
- Lightmap/source-hash behavior: unchanged / updated as described
- Collision/gameplay behavior: unchanged / updated as described
```

- Do not rewrite the whole refactor plan when marking a phase complete.
- Do not mark a phase complete until implementation and required checks have
  finished.
- If a phase is partially completed, mark it as partial and describe what
  remains.
- If a phase changes the recommended order of later phases, add a short note
  rather than rewriting the whole plan.
- Keep completion notes concise.
- Each future phase final report should mention how this plan document was
  updated.
- If a phase is executed in subpasses, do not mark the whole phase complete until all planned subpasses are done. For each subpass, either add a short progress note or simply mention in the final report what remains. Avoid documentation churn unless the phase status would otherwise become misleading.

## Notes For Future Codex Tasks

- Start each phase by grepping direct `state.topologyMap` writes and confirming
  dirty/cache/hash expectations.
- Keep phases behavior-preserving and independently reviewable.
- After successfully executing a phase, update this document to mark that phase
  completed with date, summary, verification results, and any
  cache/hash/collision notes.
- Use `MarkTopologyDocumentEdited()` for topology-visible edits unless the
  current behavior intentionally only marks dirty.
- Mention cache invalidation in the final report for any phase touching topology
  mutations.
- Mention source-hash behavior in the final report for any phase touching
  lightmaps, sky, `ceilingSky`, directional light, static lights, bake settings,
  materials, or texture definitions.
- Mention whether collision/sector lookup/physics changed for any phase touching
  preview, gameplay settings, camera, or collision world code.
- Do not claim manual GUI verification unless it was actually performed.
- Required checks after implementation:
  - `cmake --build cmake-build-debug -j2`
  - `ctest --test-dir cmake-build-debug --output-on-failure`
  - `git diff --check`
  - `git diff --stat`
  - `git status --short`
