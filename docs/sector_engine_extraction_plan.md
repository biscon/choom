# Sector Engine Extraction Plan

## How To Use This Plan

This is a living execution plan. Future Codex runs may be given a minimal
prompt such as:

```text
Read docs/sector_engine_extraction_plan.md and execute the next unfinished step.
```

When that happens, Codex must:

1. Read this "How To Use This Plan" section first.
2. Scan the phase/pass status table.
3. Identify the first phase or pass whose status is not `Completed` or
   `Deferred`.
4. Plan only that next unfinished phase/pass.
5. Do not skip ahead.
6. Do not execute multiple phases/passes in one run unless the plan explicitly
   says they are a single combined pass.
7. If the selected phase is too broad, stop and propose smaller passes under
   that phase instead of implementing it.
8. If Codex proposes smaller passes, update this plan document to add those
   passes under the current phase, then stop. Do not implement source changes in
   the same run unless explicitly instructed.
9. After successfully executing a phase/pass, update this document in the same
   run with status, date, summary, verification results, and behavior notes.
10. Keep the plan document self-tracking so future fresh-context runs can resume
    from it.

```plan-state-json
{
  "plan_id": "sector_engine_extraction_plan",
  "status_values": [
    "Not Started",
    "Planned",
    "In Progress",
    "Completed",
    "Deferred",
    "Blocked",
    "Partial"
  ],
  "items": [
    {
      "id": "phase_01",
      "title": "Decouple FPS Controller From Mesh Preview Pose",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_01a",
      "title": "Introduce SectorViewPose And Switch FPS Controller Call Sites",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_01b",
      "title": "Update Call Sites And Compatibility Helpers",
      "type": "pass",
      "parent": "phase_01",
      "status": "Deferred"
    },
    {
      "id": "phase_02",
      "title": "Extract Gameplay Preview Update Boundary",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Extract Freefly Camera/Input Behavior",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Clarify SectorMeshPreview Responsibilities",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04a",
      "title": "Audit Preview Renderer And Wrapper Responsibilities",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04b",
      "title": "Introduce Renderer-Focused API Names Without Ownership Changes",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04c",
      "title": "Adapt Editor And Demo Preview Wrappers To Renderer API",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04d",
      "title": "Document Renderer Resource Ownership And Rebuild Boundaries",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Define Minimal Sector World Runtime Boundary",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_06",
      "title": "Plan Legacy Folder/File Renames",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_07",
      "title": "Prepare Future SectorGame Consumption",
      "type": "phase",
      "status": "Not Started"
    }
  ]
}
```

## Status Legend

- `Not Started`: no implementation or detailed pass planning has begun.
- `Planned`: the phase/pass has a concrete scope, but no source work has begun.
- `In Progress`: source or document work for the phase/pass has started and is
  not complete.
- `Completed`: the phase/pass was executed, verified, and this plan was updated.
- `Deferred`: the phase/pass is intentionally postponed and the reason is
  recorded.
- `Blocked`: the phase/pass cannot continue without a decision, dependency, or
  external fix.
- `Partial`: some scoped work landed, but one or more intended items remain.

A phase containing passes is only `Completed` when all non-deferred passes
inside it are `Completed`.

`Deferred` must be explicit and must include a reason.

## Current Progress

| Phase / Pass | Status | Date | Notes |
| --- | --- | --- | --- |
| Phase 1: Decouple FPS Controller From Mesh Preview Pose | Completed | 2026-06-26 | Completed after Phase 1A; Phase 1B remains deferred. Source code changed only for neutral pose type/call-site cleanup. Behavior intended unchanged. |
| Phase 1A: Introduce SectorViewPose And Switch FPS Controller Call Sites | Completed | 2026-06-26 | Added neutral `SectorViewPose`, made `SectorFpsController` use it instead of `SectorMeshPreviewPose`, and switched preview/editor/test call sites. Verification passed: `cmake --build cmake-build-debug -j2` (CMake regenerated for new header), `ctest --test-dir cmake-build-debug --output-on-failure`. Collision, sector lookup, physics, camera feel, serialization, generated geometry, rendering, and lightmap source-hash behavior intended unchanged. No topology mutation or 2D cache invalidation paths touched. Manual GUI verification not performed. |
| Phase 1B: Update Call Sites And Compatibility Helpers | Deferred |  | Folded into Phase 1A so the first dependency cleanup is buildable and reviewable as one pass. Do not create a long-lived compatibility alias unless needed temporarily during the implementation. |
| Phase 2: Extract Gameplay Preview Update Boundary | Completed | 2026-06-26 | Moved gameplay-preview movement/collision/vertical/visual update orchestration from `SectorEditor::UpdatePreview3D` into `UpdateSectorEditorGameplayPreview()` in preview actions, while leaving editor hotkeys, UI/modal input gating, pose application, and 3D selection timing in the editor. Verification passed: `cmake --build cmake-build-debug -j2`, `ctest --test-dir cmake-build-debug --output-on-failure`. Source code changed. Collision, sector lookup, physics, camera feel, serialization, generated geometry, lightmap source-hash behavior, and 2D editor behavior intended unchanged. Cache invalidation unchanged; no topology mutation paths touched. Manual GUI verification not performed. |
| Phase 3: Extract Freefly Camera/Input Behavior | Completed | 2026-06-26 | Added `SectorFreeflyController` helper/state, moved freefly F11 cursor toggle, mouse-look warmup, yaw/pitch, and movement updates out of `SectorMeshPreview`, and wired editor/demo owners to apply controller poses to the renderer. Verification passed: `cmake --build cmake-build-debug -j2`, `ctest --test-dir cmake-build-debug --output-on-failure`. Source code changed. Freefly camera feel/cursor behavior intended unchanged. Collision, sector lookup, gameplay-preview physics, serialization, generated geometry, lightmap source-hash behavior, topology mutation paths, and 2D cache invalidation unchanged. Manual GUI verification not performed. |
| Phase 4: Clarify SectorMeshPreview Responsibilities | Completed | 2026-06-26 | Completed after Phase 4D. `SectorMeshPreview` now has renderer-focused API names in use by editor/demo wrappers, and the final renderer resource ownership/rebuild boundary is documented below. Source code changed only in earlier Phase 4B/4C passes; Phase 4D was documentation-only. Behavior intended unchanged across Phase 4: resource lifetime, preview rebuild timing, generated geometry, picking/highlights/material editing, collision/physics/camera feel, serialization, lightmap source-hash behavior, topology mutation paths, and 2D cache invalidation. Manual GUI verification not performed. |
| Phase 4A: Audit Preview Renderer And Wrapper Responsibilities | Completed | 2026-06-26 | Audited current `SectorMeshPreview` responsibilities after Phase 3 and recorded renderer/resource duties versus editor/demo wrapper policy below. Verification passed: `git diff --check`, `git diff --stat`, `git status --short`. Documentation-only pass; no source code changed. Behavior unchanged: preview rebuild timing, resource lifetime, generated geometry, picking/highlights/material editing, collision/physics/camera feel, serialization, lightmap source-hash behavior, topology mutation paths, and 2D cache invalidation unchanged. Manual GUI verification not performed. |
| Phase 4B: Introduce Renderer-Focused API Names Without Ownership Changes | Completed | 2026-06-26 | Added compatibility-preserving renderer-focused `SectorMeshPreview` API names for renderer resource rebuild/shutdown, scene drawing, bloom composition, camera/pose/geometry access, asset progress, and lightmap status. Existing API names remain as wrappers for Phase 4C adaptation. Verification passed: `cmake --build cmake-build-debug -j2`, `ctest --test-dir cmake-build-debug --output-on-failure`. Source code changed only in `SectorMeshPreview.*` plus this plan update. Resource ownership, preview rebuild timing, generated geometry output/surface metadata, texture/lightmap/sky/bloom behavior, editor picking/highlights/material editing, collision/physics/camera feel, serialization, lightmap source-hash behavior, topology mutation paths, and 2D cache invalidation unchanged. Manual GUI verification not performed. |
| Phase 4C: Adapt Editor And Demo Preview Wrappers To Renderer API | Completed | 2026-06-26 | Updated editor/demo preview-wrapper call sites to use renderer-focused `SectorMeshPreview` API names for rebuild/shutdown, draw/bloom composition, pose/camera/geometry access, readiness, asset progress, and lightmap status. Verification passed: `cmake --build cmake-build-debug -j2`, `ctest --test-dir cmake-build-debug --output-on-failure`. Source code changed only in allowed wrapper files plus this plan update. Preview rebuild timing, asset scope lifetime, 3D picking/highlights/material editing, texture fallback, sky fallback, bloom, lightmap texture handling, collision/physics/camera feel, serialization, generated geometry behavior, lightmap source-hash behavior, topology mutation paths, and 2D cache invalidation intended unchanged. Manual GUI verification not performed. |
| Phase 4D: Document Renderer Resource Ownership And Rebuild Boundaries | Completed | 2026-06-26 | Recorded final `SectorMeshPreview` renderer/resource ownership, wrapper policy, rebuild, and shutdown boundaries below. Verification passed: `git diff --check`, `git diff --stat`, `git status --short`. Documentation-only pass; no source code changed. Resource lifetime, generated geometry behavior, lightmap source-hash behavior, serialization behavior, collision/physics/camera feel, topology mutation paths, and 2D cache invalidation unchanged. Manual GUI verification not performed. |
| Phase 5: Define Minimal Sector World Runtime Boundary | Not Started |  | Add only proven reusable runtime boundary after previous seams exist. |
| Phase 6: Plan Legacy Folder/File Renames | Not Started |  | Rename/move from `sector_demo` toward `sector_engine` only after dependency cleanup. |
| Phase 7: Prepare Future SectorGame Consumption | Not Started |  | Later phase; no future game implementation yet. |

## Execution Tracking Rules

- Each phase/pass must be independently buildable and testable.
- Each phase/pass final report must state whether source code changed.
- Each implementation phase/pass must update this document before finishing.
- The update should be small and local.
- Do not rewrite unrelated phases when marking progress.
- If a phase/pass changes collision, physics, camera feel, lightmap hash
  behavior, serialization, generated geometry, or editor behavior, clearly say
  so.
- If behavior is intended to remain unchanged, explicitly state that in the
  status note.
- Do not claim manual GUI verification unless it was actually performed.
- If a phase/pass produces only a plan or audit and no source changes, state
  that clearly.
- If a phase is too broad and Codex "roaches out" by producing a subplan instead
  of implementation, that is acceptable only if it writes those subpasses back
  into this plan document and stops.

## 1. Goal And Desired End State

The goal is a cleaner dependency split between three layers:

- `SectorEditor`: editor UI, tools, selection, inspectors, modals, document
  dirty/cache policy, editor status, 2D authoring workflow, and editor-only 3D
  picking/editing.
- `SectorEngine`: reusable sector-world and runtime services usable by editor
  preview and by future game/runtime code.
- Future `SectorGame`: an actual gameplay layer that depends on `SectorEngine`,
  not on `SectorEditor`.

The desired end state is not a grand framework. It is a small set of reusable,
data-oriented sector services with one-way dependencies:

```text
SectorEditor -> SectorEngine
SectorGame   -> SectorEngine
SectorEngine -> engine services and raylib where explicitly renderer-facing
```

`SectorEngine` should own reusable sector topology, generated geometry,
collision query/runtime movement helpers, lightmap layout/bake/hash services,
sky mesh helpers, and proven render/resource pieces. `SectorEditor` should keep
editor policy and UI orchestration. `SectorGame` should be introduced later only
after the runtime boundary is real.

## 2. Current Starting Point

This plan starts from `docs/sector_engine_split_audit.md`.

Relevant findings:

- Topology v2 data and helpers are already reusable under the legacy
  `sources/sector_demo/` directory: `SectorTopologyMap`, topology creation/edit,
  validation, geometry, serialization, and units.
- Generated geometry, mesh building, collision queries, sky mesh data, and
  lightmap layout/bake/hash code are already mostly editor-independent by direct
  dependency.
- `SectorEditor` still owns 3D preview orchestration, gameplay-preview input
  collection, collision-world rebuild timing, player movement application,
  visual camera effects, status text, and editor-specific 3D picking/material
  editing.
- `SectorMeshPreview` is editor-independent, but it mixes render resources,
  generated geometry storage, asset scope ownership, camera pose, freefly input,
  cursor toggling, sky rendering, lightmap application, and bloom resources.
- `SectorFpsController` is mostly reusable, but it directly includes
  `SectorMeshPreview.h` only to use `SectorMeshPreviewPose`.
- `SectorEditorPreviewActions` is useful editor glue, but it accepts
  `SectorEditorState&`, so it is not a reusable runtime boundary.
- `SectorDemo` is currently a thin legacy demo wrapper around
  `SectorMeshPreview` freefly mode. It should not be treated as the desired
  long-term engine boundary or blindly renamed to `SectorEngine`.
- `SectorEditorState` mixes editor and runtime-preview state. Avoid broad
  redesign for now; use small seams first.

## 3. Dependency Direction Rules

Allowed:

- `SectorEditor` may depend on `SectorEngine`.
- Future `SectorGame` may depend on `SectorEngine`.
- `SectorEngine` may depend on reusable `engine` services such as assets and
  input only when that dependency is explicit and appropriate.
- Renderer-facing sector engine code may depend on raylib resource types while
  keeping ownership and lifetime clear.
- Editor glue may adapt editor state to reusable runtime update/input structs.

Forbidden:

- `SectorEngine` must not depend on `SectorEditor`.
- Future `SectorGame` must not depend on `SectorEditor`.
- Reusable movement/controller helpers must not depend on preview renderer
  types just to exchange pose data.
- Do not create cyclic dependencies between editor, runtime, renderer, topology,
  collision, lightmap, and future game code.
- Do not reintroduce the old polygon `SectorMap` fallback.
- Do not add a broad command system, virtual game-object hierarchy, ECS rewrite,
  speculative facade, or OOP-heavy runtime abstraction.

## 4. Proposed Phases

### Phase 1: Decouple FPS Controller From Mesh Preview Pose

Goal:

Introduce a neutral sector view/pose type if appropriate, then make
`SectorFpsController` depend on that neutral type instead of
`SectorMeshPreviewPose`.

Why it helps:

This removes the clearest wrong-way dependency before moving larger update
logic. The FPS/controller helpers should be reusable runtime math, not dependent
on the preview renderer header. This is small enough to review carefully.

Files/functions likely touched:

- `sources/sector_demo/SectorFpsController.h`
- `sources/sector_demo/SectorFpsController.cpp`
- `sources/sector_demo/SectorMeshPreview.h`
- `sources/sector_demo/SectorMeshPreview.cpp`
- `sources/sector_editor/SectorEditor.h`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorPreviewActions.h`
- `sources/sector_editor/SectorEditorPreviewActions.cpp`
- Possibly a small new header such as `SectorViewPose.h` or
  `SectorCameraPose.h` under the current sector backend area.

Exact behavior that must remain unchanged:

- Camera position, yaw, and pitch values.
- Freefly and gameplay preview camera feel.
- Jumping, gravity, step smoothing, headbob, landing dip, and mouse look.
- Collision, sector lookup, portal blocking, and no-clip fallback behavior.
- Preview pose preservation when entering/leaving/rebuilding 3D mode.
- Serialization and lightmap source-hash behavior.
- Generated geometry and rendering output.

Risks/goblins:

- Accidentally changing pitch clamping or eye-height offset conversion.
- Adding duplicate pose types with unclear conversion ownership.
- Pulling more renderer code into controller helpers instead of less.
- Renaming too much while attempting a tiny dependency cleanup.

Non-goals:

- Do not move files to `sector_engine` yet.
- Do not extract gameplay update orchestration yet.
- Do not change `SectorEditorState`.
- Do not change `SectorMeshPreview` resource ownership or freefly behavior yet.

Suggested tests/manual smoke checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Manual smoke, if performed: enter 3D mode, toggle freefly/gameplay, move,
  jump, step across height changes, toggle mouse look, leave/re-enter preview,
  and rebuild preview while preserving view.

Final report expectations:

- State source code changed.
- State that collision/sector lookup/physics/camera feel were intended to remain
  unchanged.
- State that lightmap source-hash behavior and serialization were unchanged.
- State whether manual GUI verification was performed.
- Mention that this phase does not touch topology mutation or 2D cache
  invalidation.

How to update this plan after completion:

- Mark Phase 1A and/or Phase 1B `Completed` with date, summary, verification,
  and behavior notes.
- Mark Phase 1 `Completed` only after all non-deferred Phase 1 passes are
  complete.
- If the pass is split further, add the subpasses under Phase 1 and stop before
  source changes unless explicitly instructed.

### Phase 2: Extract Gameplay Preview Update Boundary

Goal:

Move the gameplay-preview update orchestration out of
`SectorEditor::UpdatePreview3D` into a reusable or semi-reusable boundary that
updates player/controller state from input, collision world, and vertical
context while preserving movement feel exactly.

Why it helps:

`SectorEditor::UpdatePreview3D` currently combines editor hotkeys, UI gating,
input collection, collision resolution, vertical physics, visual camera effects,
and preview pose application. A small update boundary can make player movement
usable outside the editor without pretending to be a full game framework.

Files/functions likely touched:

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- `sources/sector_editor/SectorEditorTypes.h`
- `sources/sector_demo/SectorFpsController.*`
- `sources/sector_demo/SectorCollisionWorld.*`
- Possibly a small new gameplay-preview/runtime update helper in the current
  sector backend area.

Exact behavior that must remain unchanged:

- Input gating by editor UI/modals.
- F1/F2/F3/Tab/Escape/F11 preview behavior.
- WASD, Shift, Space, and mouse-look behavior.
- Collision resolution, portal blocking, step blocking, no-clip fallback,
  current-sector repair, floor/ceiling lookup, jump start, vertical velocity,
  gravity, landing, ceiling clamp, step smoothing, headbob, and landing dip.
- Preview selection update timing after movement.
- Preview pose application.
- Lightmap source hash, serialization, generated geometry, and 2D editor
  behavior.

Risks/goblins:

- Moving editor UI capture rules into reusable code.
- Combining physical player state with visual-only offsets in a way that affects
  collision or sector lookup.
- Changing the order of horizontal collision, sector refresh, jump, vertical
  physics, and visual effects.
- Hiding editor status/debug fields behind an oversized runtime object.

Non-goals:

- Do not implement a command buffer or input abstraction framework.
- Do not introduce a full player entity/component model.
- Do not redesign `SectorEditorState`.
- Do not change collision math unless fixing a separately scoped bug.

Suggested tests/manual smoke checks:

- Usual build/tests/diff checks.
- Manual smoke strongly recommended: compare before/after movement feel in
  gameplay preview, including stairs/steps, drops, ceiling bonk, blocked portal,
  headbob, landing dip, mouse-look toggle, and preview settings modal behavior.

Final report expectations:

- State source code changed.
- Explicitly state whether collision, sector lookup, physics, and camera feel
  changed. Expected answer: unchanged.
- State lightmap source-hash and serialization behavior unchanged.
- State cache invalidation unchanged unless a preview setting mutation path was
  touched.

How to update this plan after completion:

- Mark Phase 2 or its subpasses with date, summary, verification, behavior
  notes, and manual smoke status.
- If Phase 2 proves too broad, add smaller passes such as input collection,
  horizontal/collision update, vertical/visual update, and pose application, then
  stop.

### Phase 3: Extract Freefly Camera/Input Behavior

Goal:

Move freefly camera/input update behavior out of `SectorMeshPreview` so the
renderer no longer owns preview controls.

Why it helps:

`SectorMeshPreview` should become closer to a renderer/resource owner. Freefly
controls are useful for editor/demo preview, but not for every future sector
runtime user.

Files/functions likely touched:

- `sources/sector_demo/SectorMeshPreview.h`
- `sources/sector_demo/SectorMeshPreview.cpp`
- `sources/sector_demo/SectorDemo.*`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorPreviewActions.*`
- Possibly a small freefly controller helper under the sector backend area.

Exact behavior that must remain unchanged:

- Freefly movement speed, keys, yaw/pitch sensitivity, pitch limits, cursor
  toggling, mouse-look warmup, and camera update.
- Editor preview freefly/gameplay toggling behavior.
- Demo overlay and freefly demo behavior.
- Collision and gameplay-preview physics, which should not be touched.
- Lightmap source hash, serialization, and generated geometry.

Risks/goblins:

- Cursor state and mouse-look warmup are easy to subtly alter.
- `SectorDemo` may need a wrapper but should not become the new engine boundary.
- Mixing freefly controller state with gameplay controller state would blur the
  desired split.

Non-goals:

- Do not rename `SectorDemo` yet.
- Do not redesign renderer resource ownership.
- Do not make freefly a general gameplay controller.

Suggested tests/manual smoke checks:

- Usual build/tests/diff checks.
- Manual smoke: freefly movement and F11 cursor toggle in editor preview and
  legacy demo path if available.

Final report expectations:

- State source code changed.
- State camera feel/cursor behavior intended unchanged.
- State collision/physics/sector lookup unchanged.
- State lightmap source-hash and serialization unchanged.

How to update this plan after completion:

- Mark Phase 3 or subpasses with date, summary, verification, and behavior
  notes.
- If cursor/input ownership is too broad, split into a planning pass and stop.

### Phase 4: Clarify SectorMeshPreview Responsibilities

Goal:

Split or clarify `SectorMeshPreview` into renderer/resource responsibilities
versus editor/demo preview wrapper responsibilities.

Why it helps:

After freefly extraction, `SectorMeshPreview` can be evaluated honestly. The
renderer/resource part may be a reusable sector renderer, while the wrapper
part may stay editor/demo-specific.

Files/functions likely touched:

- `sources/sector_demo/SectorMeshPreview.*`
- `sources/sector_demo/SectorMeshBuilder.*`
- `sources/sector_demo/SectorSkyCylinder.*`
- `sources/sector_demo/SectorGeneratedGeometry.*`
- `sources/sector_demo/SectorDemo.*`
- `sources/sector_editor/SectorEditor.cpp`

Exact behavior that must remain unchanged:

- Preview rebuild timing and asset scope lifetime.
- Generated geometry output and surface metadata.
- Texture handle lookup, missing texture fallback, lightmap texture handling,
  sky fallback, emissive bloom, and material/shader setup.
- Editor 3D picking/highlights and material editing behavior.
- Collision/physics/camera feel.
- Lightmap source-hash behavior and serialization.

Risks/goblins:

- Resource lifetime regressions around asset scopes, materials, meshes, render
  textures, and raylib unload rules.
- Accidentally changing generated geometry ownership used by 3D picking.
- Treating renderer cleanup as an excuse for broad naming churn.

Non-goals:

- Do not create a broad `SectorEngine` facade in this phase.
- Do not move folders yet unless the phase has already been split and proven.
- Do not change lightmap baking or generated geometry semantics.

Suggested tests/manual smoke checks:

- Usual build/tests/diff checks.
- Manual smoke: enter/rebuild/leave preview repeatedly, confirm textures,
  lightmap status, sky, bloom, and 3D surface picking still work.

Final report expectations:

- State source code changed.
- State generated geometry behavior unchanged.
- State lightmap source-hash unchanged.
- State resource lifetime behavior intended unchanged.
- State manual GUI verification status.

How to update this plan after completion:

- Mark Phase 4 or subpasses with date, summary, verification, and behavior
  notes.
- If the split is too broad, add smaller passes such as resource naming,
  renderer-only API, wrapper adaptation, and demo/editor call-site cleanup.

Planned smaller passes:

#### Phase 4A: Audit Preview Renderer And Wrapper Responsibilities

Goal:

Produce a focused audit of `SectorMeshPreview` after Phase 3, separating:

- Renderer/resource duties: mesh/material/render texture/shader/bloom/lightmap
  resources, generated geometry storage for drawing, texture lookup/fallback,
  sky rendering, and draw-time state.
- Preview wrapper policy: asset scope creation/destruction policy, rebuild
  timing, editor/demo pose ownership, editor picking/highlight integration, and
  material editing adapters.

Allowed source changes:

- Documentation only, preferably in this plan unless a nearby code comment is
  needed to prevent accidental ownership confusion.

Suggested checks:

- `git diff --check`
- `git diff --stat`
- `git status --short`

Completion notes must state that no behavior changed, no source code changed
unless a comment was added, and no manual GUI verification was performed unless
it actually was.

Audit result:

- `SectorMeshPreview` currently owns renderer/resource duties: generated
  geometry and mesh batches created from `SectorTopologyMap`, raylib mesh upload
  and unload, material/shader state for lightmapped surfaces, texture handle
  lookup and missing-texture fallback, optional baked lightmap texture request
  and draw-time enablement, sky cylinder/top-cap mesh and material state,
  emissive decal bloom render textures/shaders/materials, camera pose storage,
  draw calls, and renderer-facing status/progress queries.
- `SectorMeshPreview` also currently owns one wrapper-policy-adjacent detail:
  it creates and unloads an `AssetManager` scope supplied by caller-provided
  scope name during `Rebuild()`/`Shutdown()`. Phase 4B/4C should clarify this
  name/API boundary without changing lifetime or rebuild timing.
- Editor-owned policy remains outside `SectorMeshPreview`: entering/leaving 3D
  mode, canceling active 2D tools, UI focus reset, F1/F2/F3/Tab/Escape hotkeys,
  freefly/gameplay mode selection, freefly and FPS controller updates, view-pose
  preservation across rebuilds, collision-world rebuild timing, preview status
  text, modal gating, and whether baked ambient occlusion is enabled for a draw.
- Editor 3D picking/highlight/material editing uses
  `preview.Camera()` and `preview.GeneratedGeometry()` as renderer-owned data
  accessors, while selection state and material mutation policy remain editor
  state/actions. This dependency should remain read-only from editor picking
  code until a later pass deliberately introduces a clearer renderer API.
- `SectorDemo` is a legacy wrapper around `SectorMeshPreview`: it loads a map,
  chooses the `"sector_demo"` asset scope name, drives the freefly controller,
  applies poses to the preview renderer, calls `Render()`, and draws its overlay.
  It should not become the reusable sector engine boundary during Phase 4.
- Behavior boundary for the next passes: preview rebuild timing, asset scope
  lifetime, generated geometry output/surface metadata, texture fallback, sky
  fallback, bloom, lightmap texture handling, 3D picking/highlights/material
  editing, collision/physics/camera feel, serialization, lightmap source-hash
  behavior, topology mutation paths, and 2D cache invalidation must remain
  unchanged.

#### Phase 4B: Introduce Renderer-Focused API Names Without Ownership Changes

Goal:

Add the smallest renderer-focused names, helpers, or method aliases needed to
make current `SectorMeshPreview` render/resource responsibilities explicit
without moving files, changing ownership, or altering rebuild timing.

Allowed source changes:

- `sources/sector_demo/SectorMeshPreview.*`
- Directly required build/test call-site updates.

Non-goals:

- Do not move folders or rename files.
- Do not change asset scope lifetime.
- Do not change generated geometry creation, storage, or surface metadata.
- Do not change editor picking or material editing behavior.

Suggested checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

#### Phase 4C: Adapt Editor And Demo Preview Wrappers To Renderer API

Goal:

Move editor/demo call sites toward the clarified renderer API while keeping
wrapper-owned policy in the editor/demo layer.

Allowed source changes:

- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorPreviewActions.*` if directly needed.
- `sources/sector_demo/SectorDemo.*`
- Narrow `SectorMeshPreview.*` follow-up edits required by the adaptation.

Behavior that must remain unchanged:

- Preview rebuild timing and asset scope lifetime.
- 3D picking/highlights/material editing.
- Texture fallback, sky fallback, bloom, and lightmap texture handling.
- Collision, physics, camera feel, serialization, generated geometry, and
  lightmap source-hash behavior.

Suggested checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

#### Phase 4D: Document Renderer Resource Ownership And Rebuild Boundaries

Goal:

Record the resulting `SectorMeshPreview` responsibility boundary after the
source passes, including what remains wrapper/editor/demo policy and what is
renderer/resource behavior.

Allowed source changes:

- Documentation only unless a small code comment is needed near an ownership or
  rebuild boundary.

Suggested checks:

- `git diff --check`
- `git diff --stat`
- `git status --short`

Completion notes must state resource lifetime behavior, generated geometry
behavior, lightmap source-hash behavior, serialization behavior, and manual GUI
verification status. Mark Phase 4 `Completed` only after all non-deferred Phase
4 passes are completed.

Final responsibility boundary:

- `SectorMeshPreview` is the preview renderer/resource owner. It owns generated
  geometry storage used for rendering and read-only editor picking,
  `SectorMeshBuildResult` mesh batches, uploaded raylib meshes/materials,
  shader uniform locations, texture handle lookup by topology texture ID,
  optional baked lightmap texture handle/status, sky cylinder/top-cap render
  resources, emissive decal bloom render textures/shaders/materials, and the
  renderer camera/pose.
- `SectorMeshPreview::RebuildRendererResources()` is the only normal rebuild
  entry point for those renderer resources. It first shuts down existing
  renderer resources, validates the topology inputs needed by preview rendering,
  rebuilds generated geometry and render mesh batches from the topology map,
  creates the caller-named asset scope, requests topology textures/lightmap
  texture through `AssetManager`, creates sky render resources when the map has
  outdoor sky geometry, loads preview materials, resets the renderer pose from
  geometry bounds, and marks the renderer ready.
- `SectorMeshPreview::ShutdownRendererResources()` is the matching owner-side
  cleanup boundary. It clears generated geometry, unloads bloom resources, sky
  meshes/materials, sector meshes, preview materials, texture-handle lookup
  state, lightmap/sky handles, and unloads the renderer-owned asset scope
  through `AssetManager`.
- The `AssetManager` still owns actual texture/font/GPU asset lifetime; the
  preview renderer owns only the scope handle it created for preview resources
  and the raylib meshes/materials/render textures it directly loads. Texture
  requests and scope unloads still happen on the main thread through
  `AssetManager`.
- Editor/demo wrappers own preview policy rather than renderer resources:
  entering/leaving preview, deciding when topology changes require preview
  rebuilds, choosing the asset scope name (`"sector_editor_preview"` or
  `"sector_demo"`), preserving/restoring poses across rebuilds, driving freefly
  or gameplay controllers, modal/input gating, status text, and deciding
  whether baked ambient occlusion is enabled for a draw.
- Editor 3D picking/highlight/material-editing policy remains outside the
  renderer. It may read `RenderCamera()` and `RenderedGeometry()` for hit tests
  against what the renderer generated, but selection state and topology/material
  mutations remain editor/editor-action responsibilities.
- Rebuild timing remains wrapper-driven. The renderer does not watch document
  dirty flags, topology cache invalidation, lightmap bake completion, texture
  registry edits, or editor tool state. Callers explicitly request a rebuild at
  existing preview entry/rebuild points.
- Generated geometry behavior is unchanged. The renderer still stores the same
  generated geometry and surface metadata built from `SectorTopologyMap`; no
  ownership transfer, schema change, picking change, or collision dependency was
  introduced in Phase 4.
- Lightmap source-hash behavior is unchanged. Phase 4 only clarified preview
  renderer API/boundaries; it did not change baked-light inputs, receiver
  layout, occluders, sky-geometry hash behavior, directional/static light data,
  or preview-only setting exclusions.
- Serialization behavior is unchanged. Phase 4 did not change topology JSON,
  generated geometry serialization, texture registry schema, sky settings
  schema, or lightmap metadata persistence.

### Phase 5: Define Minimal Sector World Runtime Boundary

Goal:

Only after the previous seams are proven, consider a minimal
`SectorWorldRuntime` or `SectorEngine` boundary that groups reusable runtime
state and services without hiding editor policy inside it.

Why it helps:

This is where a small runtime object may become useful, but only after pose,
gameplay update, freefly, and preview rendering responsibilities are cleaner.
The boundary should reflect working code rather than a speculative architecture.

Files/functions likely touched:

- Proven reusable files currently under `sources/sector_demo/`.
- `SectorCollisionWorld.*`
- `SectorFpsController.*`
- `SectorGeneratedGeometry.*`
- `SectorMeshBuilder.*`
- `SectorLightmap.*`
- `SectorSkyCylinder.*`
- Editor and demo call sites that consume those services.

Exact behavior that must remain unchanged:

- Topology v2 schema and serialization.
- Collision, physics, camera feel, jumping, step smoothing, headbob, landing
  dip, portal blocking, generated geometry, lightmap source-hash policy, and
  editor behavior.
- Resource upload/unload rules through `AssetManager`.

Risks/goblins:

- Creating a facade before there is a real need.
- Accidentally moving editor dirty/cache policy into engine code.
- Making a runtime wrapper own save/load UI, modals, or editor status.
- Introducing cycles between runtime, renderer, and editor helper modules.

Non-goals:

- Do not redesign `SectorEditorState`.
- Do not implement `SectorGame`.
- Do not add virtual interfaces or game-object hierarchies.
- Do not hide topology mutation/cache invalidation policy in runtime code.

Suggested tests/manual smoke checks:

- Usual build/tests/diff checks.
- Manual smoke across 2D editing, 3D freefly, gameplay preview, lightmap status,
  sky, and save/load if touched.

Final report expectations:

- State source code changed.
- State exactly what the new boundary owns and what it does not own.
- State collision/physics/camera/generated geometry/lightmap hash/serialization
  behavior.
- State cache invalidation behavior if topology/editor mutation paths were
  touched.

How to update this plan after completion:

- Mark Phase 5 or subpasses with date, summary, verification, and behavior
  notes.
- Record any newly proven dependency rules before planning folder renames.

### Phase 6: Plan Legacy Folder/File Renames Toward SectorEngine

Goal:

After dependency cleanup, plan and then execute small rename/move passes from
legacy `sources/sector_demo/` naming toward `sources/sector_engine/` where that
matches the proven boundary.

Why it helps:

The current folder name is legacy from the old tech demo and does not describe
the reusable backend. But folder churn before dependency cleanup would make
review harder and may hide behavior changes.

Files/functions likely touched:

- Build registration files.
- Reusable sector backend/runtime files currently under `sources/sector_demo/`.
- Includes in `sources/sector_editor/`, legacy demo code, tests, and docs.
- `SectorDemo.*` only after deciding whether it remains a demo wrapper, becomes
  a runtime sample, or is renamed differently.

Exact behavior that must remain unchanged:

- All runtime, editor, serialization, generated geometry, collision, preview,
  sky, and lightmap behavior.
- Include paths and build output should change only as required by the rename.

Risks/goblins:

- Large noisy diffs.
- Breaking includes/build files.
- Renaming `SectorDemo` into `SectorEngine` even though it is currently only a
  freefly demo wrapper.
- Combining folder churn with behavior changes.

Non-goals:

- Do not change architecture during rename-only passes.
- Do not add `SectorGame`.
- Do not create a new facade solely because a directory was renamed.

Suggested tests/manual smoke checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`
- Manual smoke optional if no behavior code changes occur, but useful after a
  large include/build move.

Final report expectations:

- State source code changed, but behavior intended unchanged.
- List moved/renamed modules.
- State no collision/physics/camera/lightmap hash/serialization/editor behavior
  changes were intended.

How to update this plan after completion:

- Mark each rename pass separately so review remains practical.
- Record any deferred files with reasons, especially `SectorDemo.*`.

### Phase 7: Prepare Future SectorGame Consumption

Goal:

Later, use the resulting engine boundary from both `SectorEditor` and a future
`SectorGame`.

Why it helps:

This validates that `SectorEngine` is not merely editor extraction, but a real
reusable runtime layer. It should happen after the boundary has proven itself in
the editor preview and legacy demo paths.

Files/functions likely touched:

- Future `sources/sector_game/` files if and when a real gameplay task exists.
- Reusable `SectorEngine` headers and runtime helpers.
- Project/game integration code that selects editor, demo, or game modes.

Exact behavior that must remain unchanged:

- Existing editor behavior unless the future task explicitly changes it.
- Existing sector engine runtime behavior.
- Topology v2, serialization, collision, camera feel, generated geometry, sky,
  and lightmap source-hash policy.

Risks/goblins:

- Building speculative game abstractions before there is a concrete game need.
- Making `SectorGame` depend on editor types for convenience.
- Pulling editor UI/status/cache policy into runtime code.

Non-goals:

- Do not start this phase until a concrete game/runtime task exists.
- Do not invent a general gameplay framework.
- Do not rewrite ECS or introduce inheritance-based gameplay entities.

Suggested tests/manual smoke checks:

- Define per-task when this phase becomes real.
- At minimum, usual build/tests/diff checks and editor preview smoke.

Final report expectations:

- State source code changed.
- State whether `SectorGame` depends only on `SectorEngine` and lower layers.
- State any behavior changes explicitly.

How to update this plan after completion:

- Add concrete passes once a real future game/runtime task exists.
- Mark this phase `Deferred` if no concrete game task exists, with that reason.

## 5. First Recommended Execution Phase

The safest first executable phase is Phase 1:
Decouple `SectorFpsController` from `SectorMeshPreviewPose`.

Reason:

- The audit found a concrete direct dependency problem:
  `SectorFpsController.h` includes `SectorMeshPreview.h` only for
  `SectorMeshPreviewPose`.
- The change can be narrow and behavior-preserving.
- It reduces dependency direction risk before extracting larger gameplay update
  orchestration.
- It does not require folder renames, `SectorEditorState` redesign, renderer
  resource changes, or gameplay movement rewrites.

Recommended execution shape:

- Execute Phase 1A as the combined first dependency cleanup pass if the diff is
  small. Phase 1B is deferred because its call-site and compatibility-helper
  scope is folded into Phase 1A.
- If the call-site impact is larger than expected, first update this plan with
  smaller subpasses under Phase 1, then stop.

## 6. Things To Defer

- Folder/file renames from `sector_demo` to `sector_engine` until dependency
  cleanup has reduced review risk.
- A broad `SectorEngine` facade before the actual seams are proven.
- Treating `SectorDemo` as the desired long-term boundary or blindly renaming it
  to `SectorEngine`.
- Full preview rewrite.
- FPS movement rewrite.
- Collision behavior changes.
- Physics/camera feel changes.
- Full `SectorEditorState` redesign.
- Command system or editor action framework.
- ECS rewrite.
- Virtual interfaces, game-object hierarchy, or inheritance-based gameplay
  entities.
- Future `SectorGame` implementation until there is a concrete game/runtime
  task.
- Lightmap hash policy changes unless a task is explicitly about lightmaps.
- Topology schema changes or any reintroduction of old polygon `SectorMap`.

## Resolved Decisions

- Phase 1 uses the neutral name `SectorViewPose`.
- `SectorViewPose` should represent renderer-neutral view state such as
  position, yaw, and pitch.
- `SectorFpsController` should depend on `SectorViewPose`, not
  `SectorMeshPreviewPose` or `SectorMeshPreview.h`.
- Prefer switching call sites directly in Phase 1A if the diff is small.
- Avoid a long-lived `SectorMeshPreviewPose` alias. A temporary compatibility
  helper is acceptable only if it keeps the pass small and buildable, and should
  be removed or documented before Phase 1 is marked complete.

## 7. Deferred Decisions For Later Phases

These are not blockers for Phase 1. Resolve each decision at the start of the
phase where it becomes relevant.

- Phase 2: What is the smallest useful data struct for gameplay-preview update
  results without copying all of `SectorEditorState`?
- Phase 2: Which editor-only input gates should remain in `SectorEditor` before
  calling reusable movement update code?
- Phase 2: Should visual-only offsets live in reusable controller state, or
  remain in editor/preview wrapper state with reusable helper functions?
- Phase 3: Should freefly controls live beside the legacy `SectorDemo` wrapper,
  beside editor preview glue, or as a small reusable debug-camera helper?
- Phase 4: What part of `SectorMeshPreview` is a renderer, and what part is a
  preview wrapper responsible for asset scope/rebuild policy?
- Phase 5: Does a `SectorWorldRuntime` object remove real duplication, or are
  free functions and small structs still enough?
- Phase 6: Which legacy `sector_demo` files are backend/runtime files, and which
  should remain demo/sample wrappers?
- Phase 7: What concrete gameplay need should drive the first `SectorGame`
  integration?
