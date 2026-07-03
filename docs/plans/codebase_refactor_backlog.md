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
| REF-008 | `[ ]` | High | Editor header | Move render-cache cached draw structs | Codex task | Low | Narrow `SectorEditorTypes.h` |
| REF-009 | `[ ]` | Medium | Editor header | Split editor modal state types | Codex task | Medium | Include churn likely |
| REF-010 | `[ ]` | Medium | Editor header | Split selection/picking/drag state types | Codex task | Medium | Keep behavior unchanged |
| REF-011 | `[ ]` | Medium | Editor header | Split preview/runtime/lightmap async state types | Codex task | Medium | Watch test source lists |
| REF-012 | `[ ]` | Medium | Editor header | Review remaining dependencies after splits | Audit first | Low | Needs post-split include review |
| REF-013 | `[ ]` | High | Mesh preview | Audit `SectorMeshPreview` extraction seams | Audit first | Medium | Do before source edits |
| REF-014 | `[ ]` | High | Mesh preview | Extract dynamic lighting/shadow internals | Runner plan | Medium/High | Needs manual render smoke |
| REF-015 | `[ ]` | High | Mesh preview | Extract runtime door renderer / mesh cache helper | Runner plan | Medium/High | Door lighting/shadow risk |
| REF-016 | `[ ]` | Medium | Mesh preview | Extract runtime billboard renderer helper | Codex task | Medium | Keep facade stable |
| REF-017 | `[ ]` | Medium | Mesh preview | Extract sky/bloom helpers | Codex task | Medium | GPU resource lifetime sensitive |
| REF-018 | `[ ]` | Medium | Mesh preview | Review remaining facade and ownership | Audit first | Low | Post-extraction checkpoint |
| REF-019 | `[ ]` | High | Editor god file | Audit remaining `SectorEditor.cpp` seams | Audit first | Low | Do after current feature work |
| REF-020 | `[ ]` | Medium | Editor god file | Extract texture/material action or inspector code | Codex task | Medium | Preserve document/cache paths |
| REF-021 | `[ ]` | Medium | Editor god file | Extract runtime object inspector/actions | Codex task | Medium | Runtime preview behavior sensitive |
| REF-022 | `[ ]` | Medium | Editor god file | Extract light/static-light/object-probe inspector/actions | Codex task | Medium/High | Source-hash semantics matter |
| REF-023 | `[ ]` | Medium | Editor god file | Extract remaining modal draw flows | Codex task | Medium | Keep boundaries narrow |
| REF-024 | `[ ]` | High | Editor god file | Review direct `state.topologyMap` mutations | Audit first | Medium | Cache invalidation audit |
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

#### REF-008 `[ ]` Move topology render-cache cached draw structs out of `SectorEditorTypes.h`

- Source/audit reference: quick win 3 and `SectorEditorTypes.h` god-header
  section.
- Why it helps: narrows the editor state header and follows the existing render
  cache module boundary.
- Likely files: new `SectorEditorTopologyRenderCacheTypes.h`,
  `SectorEditorTypes.h`, `SectorEditorTopologyRenderCache.*`.
- Suggested task type: Codex task.
- Risk: Low.
- Suggested verification: full build and editor-related tests.
- Completion notes:

#### REF-009 `[ ]` Split editor modal state types

- Source/audit reference: `SectorEditorTypes.h` broad state inventory and UI
  modal helper pressure.
- Why it helps: keeps modal-specific state out of unrelated editor modules.
- Likely files: new editor modal state header(s), `SectorEditorTypes.h`, modal
  files.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: full build, editor UI tests if available, manual modal
  smoke when behavior changes are possible.
- Completion notes:

#### REF-010 `[ ]` Split selection/picking/drag state types

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
- Completion notes:

#### REF-011 `[ ]` Split preview/runtime/lightmap async state types

- Source/audit reference: `SectorEditorTypes.h:561-675` and lightmap async
  ownership notes.
- Why it helps: separates transient preview/bake state from core document state.
- Likely files: new preview/runtime/lightmap state headers,
  `SectorEditorTypes.h`, preview and lightmap modal/action files.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: full build, full `ctest`, manual preview/lightmap
  smoke where touched.
- Completion notes:

#### REF-012 `[ ]` Review remaining `SectorEditorTypes.h` dependencies after the splits

- Source/audit reference: include graph high fan-out/fan-in notes.
- Why it helps: confirms the split actually reduced include blast radius.
- Likely files: `SectorEditorTypes.h`, newly split headers, include users.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: include/fan-out review, build, no behavior changes.
- Completion notes:

### 4. SectorMeshPreview Renderer Landfill

#### REF-013 `[ ]` Audit `SectorMeshPreview` extraction seams before touching source

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

#### REF-014 `[ ]` Extract dynamic lighting/shadow internals

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

#### REF-015 `[ ]` Extract runtime door renderer / mesh cache helper

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

#### REF-016 `[ ]` Extract runtime billboard renderer helper

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

#### REF-017 `[ ]` Extract sky/bloom helpers

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

#### REF-018 `[ ]` Review remaining `SectorMeshPreview` facade and ownership after extractions

- Source/audit reference: larger refactor warning for full facade rewrite.
- Why it helps: decides whether the remaining facade is good enough or needs a
  dedicated runner plan.
- Likely files: `SectorMeshPreview.h/.cpp` and extracted helpers.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: post-extraction review only.
- Completion notes:

### 5. SectorEditor.cpp God File

#### REF-019 `[ ]` Audit remaining `SectorEditor.cpp` seams after current feature work

- Source/audit reference: god file section for `SectorEditor.cpp`.
- Why it helps: avoids extracting stale or feature-in-progress code.
- Likely files: `SectorEditor.cpp`, existing editor action/inspector/modal
  files.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: audit output only.
- Completion notes:

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
- Completion notes:

#### REF-021 `[ ]` Extract runtime object inspector/actions

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
- Completion notes:

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
- Completion notes:

#### REF-024 `[ ]` Review direct `state.topologyMap` mutation sites and cache invalidation paths

- Source/audit reference: cache invalidation notes and direct mutation examples.
- Why it helps: catches missed `MarkTopologyDocumentEdited()` /
  `InvalidateTopologyRenderCache()` paths before or after editor refactors.
- Likely files: `sources/sector_editor/*`, especially actions and modals.
- Suggested task type: Audit first.
- Risk: Medium.
- Suggested verification: mutation-site audit; no behavior changes unless a
  follow-up bugfix is created.
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
