# Codebase Refactor Backlog

## Purpose

This is a living checklist for architecture and refactor work discovered from
`docs/audit/codebase_architecture_audit.md`.

It is not Jira, not a mandate to refactor everything, and not an executable
plan. It is also not runner-compatible.

Each item should later become one of:

- a simple Codex task
- an audit task
- a dedicated multi-phase runner plan
- a manual checkpoint
- a deliberate deferral

## Status Legend

- `[ ]` Not started
- `[~]` In progress / partial
- `[x]` Completed
- `[>]` Deferred
- `[!]` Blocked / needs decision

Task Type:

- `Codex task`
- `Audit first`
- `Runner plan`
- `Manual checkpoint`
- `Defer`

## Overview Table

| ID | Status | Priority | Area | Item | Task Type | Risk | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| REF-001 | `[x]` | High | Current feature | Finish door UV/settings/layout smoke and checkpoint | Manual checkpoint | Medium | Close current work before refactors |
| REF-002 | `[x]` | High | Utilities | Extract `SectorAssetPaths` | Codex task | Low | Strong quick win |
| REF-003 | `[x]` | Medium | Utilities | Extract finite/clamp/smoothstep helpers | Codex task | Low/Medium | Preserve per-call fallback behavior |
| REF-004 | `[x]` | Medium | Utilities | Extract minimal bounds/AABB helpers | Codex task | Low/Medium | Keep BVH ray math local for now |
| REF-005 | `[x]` | Low | Utilities | Extract trivial color conversion helpers | Codex task | Low | Do not merge lighting evaluation |
| REF-006 | `[ ]` | Low | Utilities | Evaluate tiny UV fit/span validation helper | Audit first | Medium | Avoid generic UV framework |
| REF-007 | `[x]` | Medium | Utilities | Extract lightmap bake report formatting | Codex task | Low | Behavior-preserving string/report split |
| REF-008 | `[x]` | High | Editor header | Move render-cache cached draw structs | Codex task | Low | Narrow `SectorEditorTypes.h` |
| REF-009 | `[x]` | Medium | Editor header | Split editor modal state types | Codex task | Medium | Include churn likely |
| REF-010 | `[x]` | Medium | Editor header | Split selection/picking/drag state types | Codex task | Medium | Keep behavior unchanged |
| REF-011 | `[x]` | Medium | Editor header | Split preview/runtime/lightmap async state types | Codex task | Medium | Watch test source lists |
| REF-012 | `[x]` | Medium | Editor header | Review remaining dependencies after splits | Audit first | Low | Completed; see audit report |
| REF-038 | `[x]` | Low | Editor header | Direct include cleanup after REF-012 | Codex task | Low/Medium | Removed obvious redundant umbrella includes |
| REF-013 | `[x]` | High | Mesh preview | Audit `SectorMeshPreview` extraction seams | Audit first | Medium | Completed; see audit report |
| REF-014 | `[x]` | High | Mesh preview | Extract dynamic lighting/shadow internals | Runner plan | Medium/High | Completed; manual render smoke still recommended |
| REF-015 | `[x]` | High | Mesh preview | Extract runtime door renderer / mesh cache helper | Runner plan | Medium/High | Completed; manual preview smoke still recommended |
| REF-016 | `[x]` | Medium | Mesh preview | Extract runtime billboard renderer helper | Codex task | Medium | Completed; helper owns billboard shader/draw path |
| REF-017 | `[x]` | Medium | Mesh preview | Extract sky/bloom helpers | Codex task | Medium | GPU resource lifetime sensitive |
| REF-018 | `[x]` | Medium | Mesh preview | Review remaining facade and ownership | Audit first | Low | Completed; facade acceptable before rename |
| REF-019 | `[x]` | High | Editor god file | Audit remaining `SectorEditor.cpp` seams | Audit first | Low | Completed; see audit report |
| REF-020 | `[ ]` | Medium | Editor god file | Extract texture/material action or inspector code | Codex task | Medium | Preserve document/cache paths; audit suggests SideDef/material inspector first |
| REF-021 | `[x]` | Medium | Editor god file | Extract runtime object inspector/actions | Codex task | Medium | Completed; runtime object inspector/actions/modal/drag seams extracted |
| REF-022 | `[ ]` | Medium | Editor god file | Extract light/static-light/object-probe inspector/actions | Codex task | Medium/High | Source-hash semantics matter; audit recommends mini audit before source-hash-sensitive extraction |
| REF-023 | `[ ]` | Medium | Editor god file | Extract remaining modal draw flows | Codex task | Medium | Keep boundaries narrow; document/add-texture modals are good first slices |
| REF-024 | `[ ]` | High | Editor god file | Review direct `state.topologyMap` mutations | Audit first | Medium | First-pass map exists in REF-019 audit; keep for narrower follow-up |
| REF-040 | `[x]` | High | Editor architecture | Design SectorEditor tool/module boundaries | Audit first | Low | Completed; feature/tool folders should replace further category extraction |
| REF-041 | `[x]` | High | Editor architecture | Placed-object tool folder pilot with billboards/doors split | Codex task | Low/Medium | Completed; common placed_objects plus concrete billboards/doors folders |
| REF-042 | `[x]` | Medium | Editor architecture | Move document actions/modals into `document/` | Codex task | Medium | Keep lifecycle orchestration central |
| REF-043 | `[~]` | Medium | Editor architecture | Authoring tool module contract and migration series | Codex task | Medium/High | REF-043a adds v0 tool contract and Line tool pilot; rectangle, insert vertex, and select remain |
| REF-044 | `[ ]` | Medium | Editor architecture | Migrate material/sidedef/decal editing into `tools/materials/` | Audit first | Medium/High | Preserve material mutation/cache/preview rebuild paths |
| REF-045 | `[ ]` | Medium | Editor architecture | Audit lights/source-hash-sensitive tool migration | Audit first | High | Static lights, directional light, and object probe settings affect source hash |
| REF-046 | `[ ]` | Low | Editor architecture | Audit preview tool/module migration | Audit first | High | Keep renderer resource orchestration central |
| REF-025 | `[ ]` | Medium | Lightmap/probes | Extract object probe sidecar IO | Codex task | Medium | Preserve sidecar/status behavior |
| REF-026 | `[x]` | Medium | Lightmap/probes | Extract or reuse `SectorAssetPaths` in lightmap | Codex task | Low | Pair with REF-002 if possible |
| REF-027 | `[ ]` | Medium | Lightmap/probes | Audit BVH/raycast/light evaluation extraction candidates | Audit first | Medium/High | Needs audit first |
| REF-028 | `[ ]` | High | Lightmap/probes | Keep source hash semantics explicit and tested | Manual checkpoint | High | Required around lightmap edits |
| REF-029 | `[ ]` | Medium | Build/tests | Audit explicit test target source lists after splits | Audit first | Medium | CMake source list maintenance |
| REF-030 | `[ ]` | Low | Build/tests | Consider subsystem test object libraries later | Defer | Medium | Needs build-system plan |
| REF-031 | `[ ]` | Low | Build/tests | Extract repeated test fixtures when touching domain | Codex task | Low | Avoid asset-tree dependencies |
| REF-032 | `[>]` | Deferred | Architecture | Broad OOP runtime object hierarchy | Defer | High | Probably not worth it |
| REF-033 | `[>]` | Deferred | UV/materials | Generic universal UV/material framework | Defer | High | Avoid over-generalization |
| REF-034 | `[>]` | Deferred | Layout | Move all sector code out of `sector_demo` in one pass | Defer | High | Too much rename churn |
| REF-035 | `[>]` | Deferred | UI | Split `UI.cpp` without a UI-specific reason | Defer | Medium | Not a current dependency problem |
| REF-036 | `[>]` | Deferred | Editor state | Full document-state vs preview-state rewrite | Defer | High | Needs dedicated plan if revived |
| REF-037 | `[>]` | Deferred | Mesh preview | Full `SectorMeshPreview` facade rewrite | Defer | High | Needs dedicated plan if revived |
| REF-039 | `[x]` | Medium | Mesh preview | Renderer terminology rename and renderer subdirectory move | Codex task | Medium | Completed; renderer files moved under `sector_demo/renderer` |

## Backlog

### 1. Current Feature Checkpoint

#### REF-001 `[ ]` Finish door UV/settings/layout smoke and commit/checkpoint

- Source/audit reference: current feature checkpoint; audit cautions that door
  rendering, UVs, dynamic lighting, and preview behavior are regression-prone.
- Why it helps: establishes a known-good baseline before cleanup work.
- Likely files: current door/editor feature files already in progress.
- Suggested task type: Manual checkpoint.
- Risk: Medium.
- Suggested verification: build, tests, `git diff --check`, and manual editor
  smoke for door UV/settings/layout.
- Completion notes:

### 2. Low-Risk Utility Extractions

#### REF-002 `[x]` Extract `SectorAssetPaths`

- Source/audit reference: "Asset / Path Helpers" and quick win 1.
- Why it helps: removes duplicated `assets/` prefix/path resolution logic
  between preview, lightmap, and related asset status flows.
- Likely files: new `sources/sector_demo/SectorAssetPaths.h/.cpp`,
  `SectorMeshPreview.cpp`, `SectorLightmap.cpp`, related tests if needed.
- Suggested task type: Codex task.
- Risk: Low.
- Suggested verification: lightmap, runtime-object, serialization tests, full
  build when practical.
- Completion notes: Completed by adding
  `sources/sector_demo/SectorAssetPaths.h/.cpp` for shared `assets/` path
  detection, resolution, and asset-relative filesystem conversion. Preview and
  lightmap call sites now use the shared helper with behavior intended to be
  preserved.

#### REF-003 `[x]` Extract finite/clamp/smoothstep helpers

- Source/audit reference: "Math Helpers" and "Finite / Sanitize / Validation
  Helpers".
- Why it helps: reduces anonymous helper drift across controller, topology,
  dynamic lighting, doors, and lightmaps.
- Likely files: small sector or engine math helper plus current call sites in
  `SectorFpsController.cpp`, `SectorTopologyMap.cpp`,
  `SectorDynamicPointLightSelection.cpp`, `SectorLightmap.cpp`,
  `SectorDoorRuntime.cpp`.
- Suggested task type: Codex task.
- Risk: Low/Medium.
- Suggested verification: targeted sector/controller/lightmap tests; confirm
  epsilon and fallback semantics are preserved.
- Completion notes: Completed by adding header-only
  `sources/sector_demo/SectorMath.h` for finite checks, finite clamping,
  vector normalization fallback, and 0..1 smoothstep helpers. Matching
  controller, topology, generated-geometry, dynamic-light, lightmap, mesh,
  door-runtime, and runtime-object test helpers now use the shared primitives
  while edge-based smoothstep and door/collision wrappers keep local policy
  visible.

#### REF-004 `[x]` Extract minimal bounds/AABB accumulation helpers

- Source/audit reference: "Bounds / Collision / AABB Helpers".
- Why it helps: centralizes simple Vector3 bounds validity/expansion logic.
- Likely files: new `SectorBounds` helper, `SectorMeshBuilder.cpp`,
  `SectorDynamicPointLightSelection.cpp`, `SectorDoorRuntime.cpp`,
  `SectorMeshPreview.cpp`.
- Suggested task type: Codex task.
- Risk: Low/Medium.
- Suggested verification: mesh builder, runtime object, and dynamic lighting
  tests/manual smoke where relevant.
- Completion notes: Completed by adding header-only
  `sources/sector_demo/SectorBounds.h` for minimal `Vector3` AABB validity,
  expansion, center/extents, and closest-point primitives. Mesh-builder
  receiver accumulation, dynamic-light receiver validity/clamping primitives,
  door receiver slab bounds accumulation, and preview geometry bounds now use
  the shared helpers. Lightmap BVH/raycast AABBs and domain-specific receiver
  selection padding/scoring remain local.

#### REF-005 `[x]` Extract trivial color conversion helpers

- Source/audit reference: "Lighting / Color Helpers".
- Why it helps: removes repeated color byte/unit conversions without touching
  lighting models.
- Likely files: small color helper plus generated geometry, dynamic light
  selection, lightmap, and door runtime callers.
- Suggested task type: Codex task.
- Risk: Low.
- Suggested verification: generated geometry and lightmap tests; visual smoke
  only if rendered colors are touched.
- Completion notes: Completed by adding header-only
  `sources/sector_demo/SectorColor.h` for unit RGB, color-byte clamp, and
  byte/unit color conversion helpers. Generated geometry, dynamic-light
  selection, lightmap encoding, and decal preview color call sites now use the
  shared helpers without merging lighting evaluation logic.

#### REF-006 `[ ]` Evaluate tiny UV fit/span validation helper

- Source/audit reference: "UV / Texture Helpers" and "Door/Wall/Decal UV
  Fitting".
- Why it helps: may reduce repeated finite positive span checks.
- Likely files: door runtime and editor material/UV action code.
- Suggested task type: Audit first.
- Risk: Medium.
- Suggested verification: needs audit first; if implemented, door/wall/decal UV
  smoke and relevant tests.
- Completion notes:

#### REF-007 `[x]` Extract lightmap bake report formatting

- Source/audit reference: quick win 4 and `SectorLightmap.cpp:3842`.
- Why it helps: trims a low-risk formatting concern from the heavy lightmap
  implementation.
- Likely files: `SectorLightmap.cpp`, possible new report helper files.
- Suggested task type: Codex task.
- Risk: Low.
- Suggested verification: lightmap tests and any report string assertions.
- Completion notes: Completed by moving lightmap bake report formatting and
  printing into `SectorLightmapReport.h/.cpp`; public report function names stay
  compatible through `SectorLightmap.h`, and report output was intended to remain
  unchanged.

### 3. Editor Header / Include Blast Radius

#### REF-008 `[x]` Move topology render-cache cached draw structs out of `SectorEditorTypes.h`

- Source/audit reference: quick win 3 and `SectorEditorTypes.h` god-header
  section.
- Why it helps: narrows the editor state header and follows the existing render
  cache module boundary.
- Likely files: new `SectorEditorTopologyRenderCacheTypes.h`,
  `SectorEditorTypes.h`, `SectorEditorTopologyRenderCache.*`.
- Suggested task type: Codex task.
- Risk: Low.
- Suggested verification: full build and editor-related tests.
- Completion notes: Completed by moving topology render-cache cached draw types
  into `SectorEditorTopologyRenderCacheTypes.h`; `SectorEditorTypes.h` still
  includes the new header as a compatibility umbrella.

#### REF-009 `[x]` Split editor modal state types

- Source/audit reference: `SectorEditorTypes.h` broad state inventory and UI
  modal helper pressure.
- Why it helps: keeps modal-specific state out of unrelated editor modules.
- Likely files: new editor modal state header(s), `SectorEditorTypes.h`, modal
  files.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: full build, editor UI tests if available, manual modal
  smoke when behavior changes are possible.
- Completion notes: Completed by moving modal/picker state into
  `SectorEditorModalTypes.h` and shared passive surface target types into
  `SectorEditorSurfaceTypes.h`; `SectorEditorTypes.h` still includes the new
  headers as a compatibility umbrella.

#### REF-010 `[x]` Split selection/picking/drag state types

- Source/audit reference: `SectorEditorTypes.h` broad editor state and
  `SectorEditor.cpp` god-file sections.
- Why it helps: makes canvas interaction state easier to audit separately from
  document and preview state.
- Likely files: new selection/picking state header, `SectorEditorTypes.h`,
  canvas/input code.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: full build, topology editor selection/drag smoke if
  behavior changes are possible.
- Completion notes: Completed by moving passive selection, picking, hover,
  drag, canvas-interaction, and 3D surface hit/ref types into
  `SectorEditorSelectionTypes.h`; `SectorEditorTypes.h` still includes the new
  header as a compatibility umbrella. `SpotLightPilotState` remains in
  `SectorEditorTypes.h` because it stores preview pose state and belongs with
  the later preview/runtime split; `TopologyMaterialPayload` remains there as
  material clipboard/editing payload rather than selection/picking/drag state.

#### REF-011 `[x]` Split preview/runtime/lightmap async state types

- Source/audit reference: `SectorEditorTypes.h:561-675` and lightmap async
  ownership notes.
- Why it helps: separates transient preview/bake state from core document state.
- Likely files: new preview/runtime/lightmap state headers,
  `SectorEditorTypes.h`, preview and lightmap modal/action files.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: full build, full `ctest`, manual preview/lightmap
  smoke where touched.
- Completion notes: Completed by moving passive preview control/debug overlay
  and spotlight pilot state types into `SectorEditorPreviewTypes.h`, and async
  lightmap bake result/progress/state types into
  `SectorEditorLightmapAsyncTypes.h`. `SectorEditorTypes.h` still includes both
  headers as a compatibility umbrella. Broad editor state and backend-owned
  runtime object state remain in `SectorEditorTypes.h`.

#### REF-012 `[x]` Review remaining `SectorEditorTypes.h` dependencies after the splits

- Source/audit reference: include graph high fan-out/fan-in notes.
- Why it helps: confirms the split actually reduced include blast radius.
- Likely files: `SectorEditorTypes.h`, newly split headers, include users.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: include/fan-out review, build, no behavior changes.
- Completion notes: Completed in
  `docs/audit/sector_editor_types_dependency_audit.md`; the split materially
  reduced `SectorEditorTypes.h`, and the next sensible step is optional direct
  include cleanup rather than another immediate broad split.

#### REF-038 `[x]` Direct include cleanup after REF-012

- Source/audit reference:
  `docs/audit/sector_editor_types_dependency_audit.md` recommended a small
  direct include cleanup after REF-008 through REF-012.
- Why it helps: proves selected public editor headers can use narrow split
  headers without dragging in the compatibility umbrella.
- Likely files: selected editor public headers and this backlog entry.
- Suggested task type: Codex task.
- Risk: Low/Medium.
- Suggested verification: build, targeted editor/sector tests, full `ctest`,
  and diff checks.
- Completion notes: Removed redundant `SectorEditorTypes.h` includes from
  `SectorEditorLightmapModal.h` and `SectorEditorPreviewSettingsModal.h`, using
  existing narrow modal/lightmap async headers plus an explicit topology map
  include. Left `SectorEditorTopologyRenderCache.h`,
  `SectorEditorMaterialActions.h`, and `SectorEditorHelpers.h` unchanged
  because they still expose small tool/material types that remain in
  `SectorEditorTypes.h`. `SectorEditorTypes.h` remains the compatibility
  umbrella.

### 4. SectorMeshPreview Renderer Landfill

#### REF-013 `[x]` Audit `SectorMeshPreview` extraction seams before touching source

- Source/audit reference: `SectorMeshPreview` landfill and renderer/runtime
  coupling sections.
- Why it helps: identifies safe file boundaries before moving GPU lifetime,
  dynamic light, runtime door, or billboard code.
- Likely files: `SectorMeshPreview.h/.cpp`, dynamic light, door, billboard,
  sky, bloom related files.
- Suggested task type: Audit first.
- Risk: Medium.
- Suggested verification: documentation/audit output only.
- Completion notes:
  - Completed in `docs/audit/sector_mesh_preview_extraction_audit.md`.
  - Conclusion: billboard and sky are the safest first extraction seams; dynamic
    lighting/shadows and door rendering need dedicated runner plans.

#### REF-014 `[x]` Extract dynamic lighting/shadow internals

- Source/audit reference: medium refactor 2 and dynamic receiver bounds notes.
- Why it helps: isolates point/spot selection, shadow maps, shader uniforms,
  and receiver bounds from the preview facade.
- Likely files: `SectorMeshPreview.*`, `SectorDynamicPointLightSelection.*`,
  new dynamic lighting helper files.
- Suggested task type: Runner plan.
- Risk: Medium/High.
- Suggested verification: build, runtime object/dynamic light tests, manual 3D
  preview smoke for lights and shadows.
- Completion notes:
  - Dedicated runner plan created at
    `docs/plans/sector_preview_dynamic_lighting_refactor_plan.md`.
  - Completed by extracting dynamic light CPU state, uniform uploads, dynamic
    spotlight shadow map resources, shadow material ownership, and shadow render
    internals into `SectorPreviewDynamicLighting` while keeping
    `SectorMeshPreview` as the public facade and high-level sequencing owner.
  - Final automated closeout verification passed on 2026-07-04: build, targeted
    sector tests, and full `ctest`. Manual 3D preview smoke was not performed,
    so visual smoke for dynamic lights, spotlight shadows, doors, and billboards
    remains recommended.

#### REF-015 `[x]` Extract runtime door renderer / mesh cache helper

- Source/audit reference: medium refactor 3 and runtime object vs renderer
  coupling notes.
- Why it helps: separates door mesh cache/rendering from static sector preview
  rendering.
- Likely files: `SectorMeshPreview.*`, `SectorDoorRuntime.*`, new door renderer
  helper.
- Suggested task type: Runner plan.
- Risk: Medium/High.
- Suggested verification: runtime object tests plus manual preview smoke for
  doors, door lighting, and dynamic shadows.
- Completion notes:
  - Dedicated runner plan created at
    `docs/plans/sector_preview_door_renderer_refactor_plan.md`.
  - Completed by extracting renderer-owned door mesh cache/resource lifetime,
    opaque door shader/material/debug state, main-scene door drawing, draw
    counters/debug text, prepared door shadow caster records, and dynamic
    spotlight shadow mesh resolution into `SectorPreviewDoorRenderer` while
    keeping `SectorMeshPreview` as the public facade and high-level render
    sequencing owner.
  - Final automated closeout verification passed on 2026-07-05: plan status,
    build, targeted sector tests, full `ctest`, `git diff --check`,
    `git diff --stat`, and `git status --short`. Manual 3D preview smoke was
    not performed, so visual smoke for door rendering, door lighting, and
    dynamic shadows remains recommended.

#### REF-016 `[x]` Extract runtime billboard renderer helper

- Source/audit reference: `SectorMeshPreview` mixed responsibility notes.
- Why it helps: moves ECS billboard draw traversal behind a focused helper
  while keeping the public preview facade stable.
- Likely files: `SectorMeshPreview.*`, `SectorBillboardRuntime.*`, new
  billboard renderer helper.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: runtime object tests and manual billboard preview
  smoke.
- Completion notes:
  - Added `SectorPreviewBillboardRenderer.h/.cpp` for billboard cutout shader
    ownership, shader uniform locations, ECS billboard traversal, draw counters,
    and missing/failed sprite warning state.
  - `SectorMeshPreview` remains the public facade and still owns/injects dynamic
    light selection, spotlight shadow packing, and shadow map resources.

#### REF-017 `[x]` Extract sky/bloom helpers

- Source/audit reference: `SectorMeshPreview` mixed sky/bloom/resource
  responsibilities.
- Why it helps: separates visual-only sky and postprocess resource management
  from sector mesh preview logic.
- Likely files: `SectorMeshPreview.*`, `SectorSkyCylinder.*`, new sky/bloom
  helper files.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build plus manual sky/bloom preview smoke; confirm no
  lightmap hash change.
- Completion notes:
  - Added `SectorPreviewSkyRenderer.h/.cpp` for sky cylinder/top-cap mesh,
    material, texture handle, yaw, top-cap color, draw, and unload ownership.
  - Added `SectorPreviewBloom.h/.cpp` for bloom source material, bloom shaders,
    scene-copy/source/blur render targets, emissive source draw, blur, composite,
    resize, and unload ownership.
  - `SectorMeshPreview` remains the public facade and still owns the asset
    scope plus topology texture requests.
  - Dynamic lighting, doors, billboards, and static scene rendering remain
    separate and were not moved by this task.

#### REF-018 `[x]` Review remaining `SectorMeshPreview` facade and ownership after extractions

- Source/audit reference: larger refactor warning for full facade rewrite.
- Why it helps: decides whether the remaining facade is good enough or needs a
  dedicated runner plan.
- Likely files: `SectorMeshPreview.h/.cpp` and extracted helpers.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: post-extraction review only.
- Completion notes:
  - Completed in `docs/audit/sector_mesh_preview_post_refactor_audit.md`.
  - Conclusion: `SectorMeshPreview` is now an acceptable public facade and
    high-level render coordinator; static sector rendering remains the main
    deferred extraction candidate, but no blocking ownership issue was found
    before the renderer terminology rename.

#### REF-039 `[x]` Renderer terminology rename and renderer subdirectory move

- Source/audit reference:
  `docs/audit/sector_mesh_preview_post_refactor_audit.md`.
- Why it helps: aligns names with the post-extraction ownership model now that
  `SectorMeshPreview` and helpers are renderer/facade objects rather than a
  monolithic preview implementation, and groups renderer-owned implementation
  files under a dedicated directory.
- Likely files: `SectorMeshPreview.*`, extracted `SectorPreview*` helpers,
  nearby renderer-only helpers if inspection shows they are clearly
  renderer-owned, editor/demo includes and call sites, source lists if file
  names change, tests, docs, plans, audits, and backlog references where useful.
- Suggested destination directory: `sources/sector_demo/renderer/`.
- Candidate move/rename mapping:
  - `SectorMeshPreview.h/.cpp` -> `renderer/SectorMeshRenderer.h/.cpp`
  - `SectorPreviewBillboardRenderer.h/.cpp` -> `renderer/SectorBillboardRenderer.h/.cpp`
  - `SectorPreviewSkyRenderer.h/.cpp` -> `renderer/SectorSkyRenderer.h/.cpp`
  - `SectorPreviewBloom.h/.cpp` -> `renderer/SectorBloomRenderer.h/.cpp`
  - `SectorPreviewDynamicLighting.h/.cpp` -> `renderer/SectorDynamicLightingRenderer.h/.cpp` or `renderer/SectorDynamicLighting.h/.cpp`
  - `SectorPreviewDoorRenderer.h/.cpp` -> `renderer/SectorDoorRenderer.h/.cpp`
- Suggested task type: Codex task unless future inspection shows the move needs
  a plan.
- Status: Completed.
- Risk: Medium because the change is mechanical but creates broad include/file
  churn.
- Suggested verification: build, full `ctest`, `git diff --check`, and manual
  render smoke if practical.
- Scope notes:
  - Rename Preview terminology to Renderer terminology.
  - Move renderer files under `sources/sector_demo/renderer/`.
  - Inspect whether nearby renderer-only files should move in the same pass,
    but do not overreach.
  - Do not move topology, runtime, collision, lightmap, or mesh-builder backend
    files just because renderers use them.
  - Keep `SectorMeshPreview` / `SectorPreview*` behavior unchanged during
    implementation.
  - Do not rename `sector_demo` to `sector_engine` in this pass.
  - Do not change shader code, resource ownership, render order, ECS lifecycle,
    lightmap/source hash, collision/physics, schema, or editor behavior.
  - CMake may need explicit test target updates if moved `.cpp` files are
    listed manually.
  - Main app source glob should likely pick up moved `.cpp` files, but explicit
    test targets must be checked.
- Completion notes:
  - Moved the renderer facade and helper implementation files into
    `sources/sector_demo/renderer/` and renamed them to `Sector*Renderer`
    terminology.
  - Updated renderer-owned type names, helper context names, upload helper
    names, and include paths while preserving editor preview workflow naming.
  - Behavior intended unchanged: shader code, resource ownership, render order,
    ECS lifecycle, topology/editor cache behavior, lightmap source-hash behavior,
    collision/physics, and serialization/schema were not intentionally changed.
  - CMake did not require manual source-list edits; the app source glob picked
    up the moved renderer files during the build.

### 5. SectorEditor.cpp God File

#### REF-019 `[x]` Audit remaining `SectorEditor.cpp` seams after current feature work

- Source/audit reference: god file section for `SectorEditor.cpp`.
- Why it helps: avoids extracting stale or feature-in-progress code.
- Likely files: `SectorEditor.cpp`, existing editor action/inspector/modal
  files.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: audit output only.
- Completion notes:
  - Completed in `docs/audit/sector_editor_cpp_seams_audit.md`.
  - Conclusion: `SectorEditor.cpp` remains the largest god-file risk, but
    existing action/modal/inspector/cache seams make small extractions
    practical.
  - Recommended first follow-up: split REF-021 and start by extracting the
    runtime object inspector draw body while preserving
    `MarkTopologyDocumentEdited()` and runtime object refresh behavior.

#### REF-020 `[ ]` Extract texture/material action or inspector code

- Source/audit reference: recommended next action 4 and texture direct mutation
  evidence.
- Why it helps: continues existing editor action-module direction and makes
  material/cache invalidation paths easier to review.
- Likely files: `SectorEditor.cpp`, `SectorEditorTextureActions.*`, material
  inspector/action files.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build, editor material/texture tests if present,
  manual material action smoke.
- Notes:
  - REF-019 recommends narrowing the first slice to the SideDef/material
    inspector or texture/material picker glue, preserving
    `FinishTopologyMaterialMutation()` / cache invalidation paths.
- Completion notes:

#### REF-021 `[x]` Extract runtime object inspector/actions

- Source/audit reference: editor responsibilities and runtime object split
  notes.
- Why it helps: reduces editor file size and keeps runtime-object authoring
  workflows together.
- Likely files: `SectorEditor.cpp`, runtime object inspector/action files,
  runtime object backend files as needed.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: runtime object tests and manual object authoring
  smoke.
- Notes:
  - REF-019 recommends splitting this into runtime object inspector,
    runtime object actions/drag, and door texture settings modal slices.
  - Suggested first post-audit implementation item: extract the runtime object
    inspector draw body into a focused module.
- Completion notes:
  - REF-021a progress: extracted the runtime object inspector draw body into
    `SectorEditorRuntimeObjectInspector.*`. REF-021 remains open for runtime
    object actions/drag and door texture settings modal/action slices.
  - REF-021b progress: extracted the door texture settings modal draw flow and
    focused UV action helpers into `SectorEditorRuntimeObjectModals.*`.
    REF-021 remains open for runtime object actions/drag slices.
  - REF-021c progress: extracted non-drag runtime object action wrappers into
    `SectorEditorRuntimeObjectActions.*`. REF-021 remains open for runtime
    object drag as a future slice.
  - REF-021d progress: extracted runtime object drag helpers into
    `SectorEditorRuntimeObjectDrag.*`. REF-021 is now complete for the planned
    runtime object inspector, modal/action, non-drag action, and drag slices.

#### REF-022 `[ ]` Extract light/static-light/object-probe inspector/actions

- Source/audit reference: lightmap/object probe ownership and preview settings
  source-hash cautions.
- Why it helps: groups light/object-probe authoring and makes hash-affecting
  mutations easier to audit.
- Likely files: `SectorEditor.cpp`, lightmap modal/action files, static light
  inspector/action helpers.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: lightmap tests, source-hash tests, manual light/object
  probe UI smoke.
- Notes:
  - REF-019 recommends a mini audit before moving source-hash-sensitive
    preview/object-probe/directional/static-light settings broadly.
- Completion notes:

#### REF-023 `[ ]` Extract remaining modal draw flows with clear boundaries

- Source/audit reference: UI/modal helper pressure and existing modal modules.
- Why it helps: continues moving modal-specific draw state out of the god file
  without adding a generic UI framework.
- Likely files: `SectorEditor.cpp`, `SectorEditor*Modal.*`,
  `SectorEditorUiHelpers.*`.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build and manual modal smoke for touched flows.
- Notes:
  - REF-019 identifies document save/load/confirmation modals and add-map
    texture modal as good first narrow slices.
- Completion notes:
  - REF-023a progress: extracted document save/load/confirmation modal draw
    flows into `SectorEditorDocumentModals.*`. REF-023 remains open for other
    modal slices such as add-map-texture, decal tint, and preview overlay/UV
    panel flows.
  - REF-023b progress: verified that the add-map-texture modal draw flow already
    lives in `SectorEditorTextureModals.*`; no source move was needed.
    `SectorEditor.cpp` retains only wrapper/context/callback wiring and texture
    action/import ownership.
  - REF-023c progress: extracted the decal tint modal draw flow into
    `SectorEditorMaterialModals.*`. `SectorEditor.cpp` retains only
    wrapper/context/callback wiring and material mutation ownership.

#### REF-024 `[ ]` Review direct `state.topologyMap` mutation sites and cache invalidation paths

- Source/audit reference: cache invalidation notes and direct mutation examples.
- Why it helps: catches missed `MarkTopologyDocumentEdited()` /
  `InvalidateTopologyRenderCache()` paths before or after editor refactors.
- Likely files: `sources/sector_editor/*`, especially actions and modals.
- Suggested task type: Audit first.
- Risk: Medium.
- Suggested verification: mutation-site audit; no behavior changes unless a
  follow-up bugfix is created.
- Notes:
  - REF-019 includes a first-pass mutation/cache map. Keep REF-024 for a
    narrower follow-up audit, especially after future inspector/action
    extractions.
- Completion notes:

#### REF-040 `[x]` Design SectorEditor tool/module boundaries

- Source/audit reference: REF-019 and the risk that category-based extraction
  can create new `Modals`, `Inspectors`, `Actions`, and `Helpers` landfills.
- Why it helps: defines the next architecture direction for reducing
  `SectorEditor.cpp` around feature/tool modules instead of broad category
  files.
- Likely files: audit/backlog documentation only.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: `git diff --check`, `git diff --stat`,
  `git status --short`.
- Completion notes:
  - Completed in `docs/audit/sector_editor_tool_module_boundary_design.md`.
  - Conclusion: future editor work should move toward feature/tool folders,
    narrow toolbox/context APIs, and optional handler dispatch rather than a fat
    `IEditorTool` hierarchy or more category-file growth.
  - Recommended first implementation task: move the already extracted runtime
    object editor files into `sources/sector_editor/tools/runtime_objects/`
    with no behavior changes and no direct `SectorEditor` dependency.

#### REF-041 `[x]` Placed-object tool folder pilot with billboards/doors split

- Source/audit reference:
  `docs/audit/sector_editor_tool_module_boundary_design.md`.
- Why it helps: establishes the first placed-object tool layout using common
  infrastructure plus concrete billboard and door feature folders.
- Likely files: `SectorEditorRuntimeObjectInspector.*`,
  `SectorEditorRuntimeObjectModals.*`,
  `SectorEditorRuntimeObjectActions.*`,
  `SectorEditorRuntimeObjectDrag.*`, include paths.
- Suggested task type: Codex task.
- Risk: Low/Medium.
- Suggested verification: build, ctest, `git diff --check`, and manual runtime
  object authoring smoke.
- Notes:
  - Do not add a full tool registry.
  - Keep existing context/callback APIs.
  - Do not move renderer/runtime backend files.
- Completion notes:
  - Created `sources/sector_editor/tools/placed_objects/`,
    `sources/sector_editor/tools/billboards/`, and
    `sources/sector_editor/tools/doors/`.
  - Moved/split the existing runtime object editor modules into common
    placed-object dispatch/actions/drag, billboard actions/inspector, and door
    actions/inspector/modals.
  - Preserved context/callback boundaries with no direct `SectorEditor`
    dependency in the moved modules.
  - No behavior changes intended.

#### REF-042 `[x]` Move document actions/modals into `document/`

- Source/audit reference:
  `docs/audit/sector_editor_tool_module_boundary_design.md`.
- Why it helps: separates document lifecycle helpers from tool modules while
  keeping high-level editor lifecycle orchestration central.
- Likely files: `SectorEditorDocumentActions.*`,
  `SectorEditorDocumentModals.*`, include paths.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build, ctest, `git diff --check`, and manual
  new/load/save/reload smoke.
- Notes:
  - Document is not a tool.
  - Do not move preview enter/leave or renderer/resource lifecycle into the
    document folder.
- Completion notes:
  - Moved document actions/modals into
    `sources/sector_editor/document/`.
  - Document remains separate from tools.
  - High-level document lifecycle orchestration remains in `SectorEditor`.
  - No behavior changes intended.

#### REF-043 `[~]` Authoring tool module contract and migration series

- Source/audit reference:
  `docs/audit/sector_editor_tool_module_boundary_design.md`.
- Why it helps: avoids one giant authoring module and lets each tool own its
  input/overlay/temporary state where practical.
- Likely files: `SectorEditor.cpp`, `SectorEditorAuthoringState.*`,
  `tools/`, `tools/select/`, `tools/line/`, `tools/rectangle/`,
  `tools/insert_vertex/`.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: build, ctest, and manual select/line/rectangle/insert
  vertex authoring smoke after implementation tasks.
- Series:
  - REF-043a: Minimal authoring tool contract and Line tool pilot.
  - REF-043b: Rectangle tool migration.
  - REF-043c: Insert Vertex tool migration.
  - REF-043d: Selection service and manipulation provider contract.
  - REF-043e: Select tool migration using the selection/manipulation contract.
- Notes:
  - Keep current "Authoring Line" terminology until a rename task is explicitly
    scoped.
  - Select migration should wait for a Selection service and Manipulation
    provider contract.
  - Select is both an active tool and the frontend for selection/move behavior.
  - Do not migrate Select as a giant switch over movable types.
- Completion notes:
  - REF-043a added a minimal tool module descriptor/lookup.
  - REF-043a migrated Authoring Line behavior into `tools/line/`.
  - REF-043b migrated Authoring Rectangle behavior into `tools/rectangle/`.
  - REF-043b registered Rectangle in `FindSectorEditorToolModule()`.
  - REF-043c migrated Authoring Insert Vertex behavior into
    `tools/insert_vertex/`.
  - REF-043c registered Insert Vertex in `FindSectorEditorToolModule()`.
  - `SectorEditor` uses module dispatch for Line, Rectangle, and Insert Vertex.
  - Legacy fallback remains for Select.
  - No behavior changes intended.
  - REF-043 remains open for selection/manipulation design and Select migration.

#### REF-044 `[ ]` Migrate material/sidedef/decal editing into `tools/materials/`

- Source/audit reference:
  `docs/audit/sector_editor_tool_module_boundary_design.md`.
- Why it helps: keeps material inspector UI, decal modal, material actions, and
  material-specific picker routing together instead of expanding generic
  material/action/modal categories.
- Likely files: `SectorEditorMaterialActions.*`,
  `SectorEditorMaterialModals.*`, SideDef/material inspector code, material
  picker routing.
- Suggested task type: Audit first.
- Risk: Medium/High.
- Suggested verification: build, ctest, material/decal/texture picker smoke;
  implementation reports must mention topology render-cache invalidation.
- Notes:
  - Preserve `FinishTopologyMaterialMutation()` /
    `FinishMaterialActionResult()` paths.
  - Keep texture catalog/import workflows separate from material-specific
    routing.
- Completion notes:

#### REF-045 `[ ]` Audit lights/source-hash-sensitive tool migration

- Source/audit reference:
  `docs/audit/sector_editor_tool_module_boundary_design.md`.
- Why it helps: lights, directional settings, object probe settings, and
  lightmap bake coupling are high-risk boundaries that should not be moved
  casually.
- Likely files: `SectorEditorLightInspector.*`, light action/drag code,
  preview settings apply code, lightmap tests.
- Suggested task type: Audit first.
- Risk: High.
- Suggested verification: source-hash audit first; future implementation needs
  build, ctest, source-hash tests, and manual light/preview/bake smoke.
- Notes:
  - Static lights, static spotlights, directional light, object probe spacing,
    object probe height, and `ceilingSky` are source-hash-sensitive.
  - Sky visual settings and preview settings should remain source-hash-neutral.
- Completion notes:

#### REF-046 `[ ]` Audit preview tool/module migration

- Source/audit reference:
  `docs/audit/sector_editor_tool_module_boundary_design.md`.
- Why it helps: identifies which preview overlays, UV panel, and preview
  settings UI can move without disturbing renderer resource lifecycle.
- Likely files: `SectorEditorPreviewActions.*`,
  `SectorEditorPreviewSettingsModal.*`, preview overlay/UV panel code,
  `SectorEditor.cpp`.
- Suggested task type: Audit first.
- Risk: High.
- Suggested verification: audit first; future implementation needs build,
  ctest, and manual preview render/overlay/UV smoke.
- Notes:
  - Keep `SectorMeshRenderer` implementation and GPU resource orchestration
    outside tool modules.
  - Defer broad preview facade rewrites.
- Completion notes:

### 6. Lightmap / Object Probe Cleanup

#### REF-025 `[ ]` Extract object probe sidecar IO

- Source/audit reference: medium refactor 4 and lightmap/object-probe ownership
  notes.
- Why it helps: separates file IO/status handling from bake math.
- Likely files: `SectorLightmap.cpp`, `SectorLightmap.h`,
  `SectorLightmapTypes.h`, new sidecar IO files.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: lightmap and runtime object tests; confirm source
  hash semantics are unchanged.
- Completion notes:

#### REF-026 `[x]` Extract or reuse `SectorAssetPaths` in lightmap

- Source/audit reference: asset/path helper duplication.
- Why it helps: keeps lightmap asset lookup consistent with preview and other
  sector asset code.
- Likely files: `SectorLightmap.cpp`, new/existing `SectorAssetPaths` helper.
- Suggested task type: Codex task.
- Risk: Low.
- Suggested verification: lightmap tests and asset path regression cases if
  available.
- Completion notes: Completed by replacing lightmap-local texture/lightmap/probe
  asset path resolution with `SectorAssetPaths`. Lightmap sidecar extension
  derivation remains local because it is sidecar-specific behavior, not generic
  asset path resolution.

#### REF-027 `[ ]` Audit BVH/raycast/light evaluation extraction candidates before implementation

- Source/audit reference: lightmap god-file section and bounds/AABB risk notes.
- Why it helps: prevents accidental bake/shadow/epsilon behavior changes during
  a large internal split.
- Likely files: `SectorLightmap.cpp`, lightmap tests.
- Suggested task type: Audit first.
- Risk: Medium/High.
- Suggested verification: needs audit first; future implementation needs a
  dedicated runner plan if broad.
- Completion notes:

#### REF-028 `[ ]` Keep source hash semantics explicit and tested

- Source/audit reference: lightmap source-hash notes.
- Why it helps: protects the distinction between visual preview settings and
  geometry/light-affecting bake settings.
- Likely files: `SectorLightmap.*`, topology serialization/map settings,
  lightmap tests.
- Suggested task type: Manual checkpoint.
- Risk: High.
- Suggested verification: source-hash tests around any lightmap or
  object-probe refactor.
- Completion notes:

### 7. Build/Test Structure

#### REF-029 `[ ]` Audit explicit test target source lists after more source splits

- Source/audit reference: build-system note and test CMake explicit source list
  risk.
- Why it helps: catches missing production sources in test targets after
  refactors.
- Likely files: `CMakeLists.txt`, test target definitions.
- Suggested task type: Audit first.
- Risk: Medium.
- Suggested verification: full build and full `ctest`.
- Completion notes:

#### REF-030 `[ ]` Consider subsystem test object libraries later

- Source/audit reference: larger refactor 3.
- Why it helps: may reduce repeated manual production source lists in tests.
- Likely files: `CMakeLists.txt`.
- Suggested task type: Defer.
- Risk: Medium.
- Suggested verification: needs dedicated build-system plan before changes.
- Completion notes:

#### REF-031 `[ ]` Extract repeated test fixtures only when already touching that test domain

- Source/audit reference: large tests section and project test-data rules.
- Why it helps: reduces test duplication without broad fixture churn.
- Likely files: specific test file/domain being modified.
- Suggested task type: Codex task.
- Risk: Low.
- Suggested verification: affected test target and full `ctest` when practical.
- Completion notes:

### 8. Deferred / Probably Not Worth It Yet

#### REF-032 `[>]` Broad OOP runtime object hierarchy

- Source/audit reference: "Things Not Worth Doing Yet".
- Why it helps: it probably does not; current data/free-function style fits the
  project.
- Likely files: none.
- Suggested task type: Defer.
- Risk: High.
- Suggested verification: none.
- Completion notes:

#### REF-033 `[>]` Generic universal UV/material framework

- Source/audit reference: UV helper cautions and "Things Not Worth Doing Yet".
- Why it helps: deferred because door, wall, decal, and flat authoring semantics
  differ enough that a generic framework is likely premature.
- Likely files: none.
- Suggested task type: Defer.
- Risk: High.
- Suggested verification: none.
- Completion notes:

#### REF-034 `[>]` Move all sector code out of `sector_demo` in one pass

- Source/audit reference: "Things Not Worth Doing Yet".
- Why it helps: deferred because behavior-preserving internal splits are more
  useful than broad rename churn.
- Likely files: none.
- Suggested task type: Defer.
- Risk: High.
- Suggested verification: none.
- Completion notes:

#### REF-035 `[>]` Split `UI.cpp` without a UI-specific reason

- Source/audit reference: engine UI file-size section.
- Why it helps: deferred because `UI.cpp` is large but not currently a
  cross-subsystem dependency problem.
- Likely files: none.
- Suggested task type: Defer.
- Risk: Medium.
- Suggested verification: none.
- Completion notes:

#### REF-036 `[>]` Full editor document-state vs preview-state rewrite without a dedicated plan

- Source/audit reference: larger refactor 2.
- Why it helps: deferred because this would touch most editor workflows and
  cache invalidation paths.
- Likely files: none until planned.
- Suggested task type: Defer.
- Risk: High.
- Suggested verification: requires dedicated runner plan if revived.
- Completion notes:

#### REF-037 `[>]` Full `SectorMeshPreview` facade rewrite without a dedicated plan

- Source/audit reference: larger refactor 1.
- Why it helps: deferred because shader/resource lifetime and manual preview
  verification need a scoped plan.
- Likely files: none until planned.
- Suggested task type: Defer.
- Risk: High.
- Suggested verification: requires dedicated runner plan if revived.
- Completion notes:

## Guardrails

- Prefer behavior-preserving refactors.
- Work one item at a time.
- Commit/checkpoint after a successful item.
- Do not change schema, hash, rendering, collision, or camera/physics semantics
  unless the item explicitly says so.
- For `SectorMeshPreview`, doors, shadows, lightmaps, and editor cache
  invalidation, require manual smoke or a dedicated plan where appropriate.
- Do not introduce inheritance-heavy architecture.
- Do not convert this backlog into a runner plan.
