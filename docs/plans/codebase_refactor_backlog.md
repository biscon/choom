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
| REF-053 | `[x]` | High | Editor god file | Post-tool-migration `SectorEditor.cpp` line map | Audit first | Low | Completed; current size is 10,551 lines; recommends materials/sidedef/decal next |
| REF-020 | `[ ]` | Medium | Editor god file | Extract texture/material action or inspector code | Codex task | Medium | Post-REF-055 work should proceed through picker/material services, not broad generic extraction |
| REF-021 | `[x]` | Medium | Editor god file | Extract runtime object inspector/actions | Codex task | Medium | Completed; runtime object inspector/actions/modal/drag seams extracted |
| REF-022 | `[ ]` | Medium | Editor god file | Extract light/static-light/object-probe inspector/actions | Codex task | Medium/High | Source-hash semantics matter; audit recommends mini audit before source-hash-sensitive extraction |
| REF-023 | `[ ]` | Low | Editor god file | Extract remaining modal draw flows | Codex task | Medium | Most obvious modal draw bodies have moved; remaining work is feature-specific routing |
| REF-024 | `[ ]` | High | Editor god file | Review direct `state.topologyMap` mutations | Audit first | Medium | First-pass map exists in REF-019 audit; keep for narrower follow-up |
| REF-054 | `[x]` | High | Editor god file | Audit materials/sidedef/decal extraction boundary | Audit first | Medium | Completed; report recommends callback-based REF-055 inspector extraction |
| REF-055 | `[x]` | High | Editor god file | Extract SideDef/material/decal inspector into `tools/materials/` | Codex task | Medium/High | Completed; inspector moved to tools/materials while finish wrappers stayed central |
| REF-056 | `[x]` | High | Editor architecture | SectorEditor shared service inventory audit | Audit first | Low | Completed; recommends minimal TexturePickerService first |
| REF-057 | `[x]` | High | Editor architecture | Extract minimal TexturePickerService | Codex task | Medium | Generic picker lifecycle/result mechanics only; preserve feature-specific apply semantics |
| REF-058 | `[ ]` | Medium | Editor architecture | Audit AssetCatalog/TextureCatalog service boundary | Audit first | Medium | Keep texture import/handle/catalog ownership separate from picker apply routing |
| REF-059 | `[x]` | High | Editor architecture | Audit MaterialEditBridge and material-specific picker routing | Audit first | High | Completed; recommends material-specific picker routing extraction before MaterialEditBridge |
| REF-060 | `[ ]` | Medium | Editor architecture | Audit Preview UV/material panel service dependencies | Audit first | Medium/High | Decide dependency on TexturePickerService, MaterialEditBridge, and preview-surface selection |
| REF-061 | `[ ]` | Low | Editor architecture | Evaluate Status/Diagnostics service | Defer | Low/Medium | Only pursue if status/warning callback noise blocks service extraction |
| REF-062 | `[ ]` | Medium | Lightmap/probes | Extract LightmapBakeController | Runner plan | High | Worker lifecycle/result install/source-hash stale-result logic needs runner-level guardrails |
| REF-063 | `[x]` | High | Editor architecture | Extract material-specific texture picker routing | Codex task | High | Completed; material targets route through `services/material_edit`, non-material picker routes stayed out |
| REF-064 | `[x]` | High | Editor architecture | Add minimal MaterialEditBridge for material action wrappers | Codex task | Medium/High | Completed; action wrappers route through bridge while finish paths stay in `SectorEditor.cpp` |
| REF-065 | `[x]` | High | Editor architecture | Promote MaterialEditBridge into MaterialEditingService | Codex task | Medium/High | Completed; bridge removed and material clients use service directly |
| REF-066 | `[x]` | Medium | Editor architecture | Extract Preview3D UV/material panel into `preview/` | Codex task | Medium/High | Completed; panel moved to preview module and uses `MaterialEditingService` directly |
| REF-074 | `[x]` | Medium | Editor architecture | Extract Preview3D overlay/debug UI into `preview/` | Codex task | Medium | Completed; overlay module returns one-frame high-level action requests |
| REF-067 | `[x]` | High | Editor architecture | Authoring-owned topology edit cleanup and direct topology mutation inventory | Codex task | Medium/High | Completed; Blocks Player authoring-owned with strict mapping-gap failure and inventory report |
| REF-068 | `[x]` | High | Editor architecture | Replace SideDef inspector UV topology mutation with authoring material edits | Codex task | Medium/High | Completed; inspector UV apply/reset writes authoring material data and fails without mapping |
| REF-069 | `[x]` | Medium | Editor architecture | Remove invalid no-authoring topology-edit support | Codex task | Medium/High | Completed; normal editor edits require authoring data or fail |
| REF-070 | `[x]` | High | Editor architecture | Write Sector Editor Architectural Principles document | Codex task | Low | Completed; architecture contract added under `docs/architecture/` |
| REF-071 | `[x]` | High | Editor architecture | Remove remaining material topology scratch/writeback routes | Codex task | Medium/High | Completed; material service and picker routes now write authoring material data directly |
| REF-072 | `[x]` | Medium | Editor architecture | Clean up material service debt after authoring-owned material migration | Codex task | Medium | Completed; stale scratch/writeback helper, wrappers, callback remnants, and misleading names removed |
| REF-073 | `[x]` | High | Editor architecture | Rectangle tool authoring-graph split correctness | Codex task | Medium/High | Completed; rectangle insertion now splits authoring graph lines before topology derivation |
| REF-040 | `[x]` | High | Editor architecture | Design SectorEditor tool/module boundaries | Audit first | Low | Completed; feature/tool folders should replace further category extraction |
| REF-041 | `[x]` | High | Editor architecture | Placed-object tool folder pilot with billboards/doors split | Codex task | Low/Medium | Completed; common placed_objects plus concrete billboards/doors folders |
| REF-042 | `[x]` | Medium | Editor architecture | Move document actions/modals into `document/` | Codex task | Medium | Keep lifecycle orchestration central |
| REF-043 | `[x]` | Medium | Editor architecture | Authoring tool module contract and migration series | Codex task | Medium/High | Completed through Select using selection/manipulation services |
| REF-047 | `[x]` | High | Editor architecture | Selection service and manipulation provider contract | Audit first | Low | Completed; Select is the frontend for selection/manipulation, not just another authoring tool |
| REF-048 | `[x]` | High | Editor architecture | Add passive SelectionTarget and provider type definitions | Codex task | Low/Medium | No behavior changes; shared vocabulary before service extraction |
| REF-049 | `[x]` | High | Editor architecture | Extract Selection service helpers | Codex task | Medium | Preserve current selected state fields, stale cleanup, and UI reset behavior |
| REF-050 | `[x]` | High | Editor architecture | Add Manipulation service shell | Codex task | Medium | Own generic drag lifecycle while delegating existing movement paths first |
| REF-051 | `[x]` | Medium | Editor architecture | Pilot first move provider | Codex task | Medium | Completed; placed-object/billboard movement routes through provider while preserving door movement refusal |
| REF-052 | `[x]` | High | Editor architecture | Migrate Select tool using services/providers | Codex task | High | Completed; future provider refinements remain optional |
| REF-044 | `[~]` | Medium | Editor architecture | Migrate material/sidedef/decal editing into `tools/materials/` | Audit first | Medium/High | Umbrella for REF-054/REF-055 materials migration series |
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
  - REF-053 makes this more specific: do REF-054 first, then extract the
    SideDef/material/decal inspector as REF-055. Keep texture catalog/import
    separate from material-specific picker routing.
  - REF-056 marks the next texture/material work as service-shaped: implement
    the minimal TexturePickerService first, then audit material-specific picker
    routing and a MaterialEditBridge. Avoid another broad generic
    texture/material extraction.
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
  - REF-053 marks those notes partly stale: document save/load/confirmation,
    add-map texture, texture picker, sprite picker, decal tint, preview
    settings, lightmap bake, and door texture settings draw bodies now have
    module ownership. Remaining work is mostly feature-specific callback or
    picker routing and should not displace material inspector extraction.
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

#### REF-053 `[x]` Post-tool-migration `SectorEditor.cpp` line map

- Source/audit reference: REF-041 through REF-052 moved placed objects,
  document helpers, Line/Rectangle/Insert Vertex/Select tools, Selection
  service helpers, Manipulation service shell, and the placed-object move
  provider.
- Why it helps: replaces stale line ranges from prior audits with a current
  post-migration map of what still remains in `SectorEditor.cpp`.
- Likely files: documentation only.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: `git diff --check`, `git diff --stat`,
  `git status --short`.
- Completion notes:
  - Completed in
    `docs/audit/sector_editor_post_tool_migration_line_map.md`.
  - Current `sources/sector_editor/SectorEditor.cpp` line count: 10,551.
  - Top remaining line-count clusters are main inspector/authoring remnants,
    SideDef/material/decal inspector, preview overlay/UV panel, texture picker
    routing plus material wrappers, and light/lightmap orchestration.
  - Recommended next implementation sequence: REF-054 narrow
    materials/sidedef/decal boundary audit, then REF-055 SideDef/material/decal
    inspector extraction into `tools/materials/`.
  - Source code, tests, CMake, runtime behavior, topology cache behavior,
    lightmap source-hash behavior, collision, and preview behavior were not
    changed by this audit.

#### REF-054 `[x]` Audit materials/sidedef/decal extraction boundary

- Source/audit reference:
  `docs/audit/sector_editor_post_tool_migration_line_map.md`.
- Why it helps: identifies the exact context/callback API needed before moving
  material inspector UI out of `SectorEditor.cpp`.
- Likely files: `SectorEditor.cpp`, `SectorEditor.h`,
  `SectorEditorMaterialActions.*`, `SectorEditorMaterialModals.*`,
  `SectorEditorTextureActions.*`, `SectorEditorTextureModals.*`, and future
  `tools/materials/` candidates.
- Suggested task type: Audit first.
- Risk: Medium.
- Suggested verification: documentation checks only.
- Notes:
  - Inventory `DrawTopologySideDefInspector()`, `DrawPreviewUvPanel()`,
    `ApplyTexturePickerSelection()`, `FinishTopologyMaterialMutation()`,
    `FinishMaterialActionResult()`, authoring-side material routes, and
    authoring-face flat material routes.
  - Decide which picker routing belongs with materials and which remains in
    texture catalog/import modules.
  - Explicitly document topology render-cache invalidation and preview rebuild
    paths.
- Completion notes:
  - Completed in
    `docs/audit/sector_editor_materials_boundary_audit.md`.
  - Recommended REF-055 scope: extract the `DrawTopologySideDefInspector()`
    body and inspector-only helper lambdas into
    `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.h/.cpp`
    using a narrow state/UI/callback context and no direct `SectorEditor.h`
    dependency.
  - Expected `SectorEditor.cpp` reduction from REF-055: roughly 600-740 lines
    if broad material finish wrappers, generic texture picker apply routing,
    preview UV panel, and authoring graph internals remain in `SectorEditor.cpp`
    initially.
  - Source code, tests, CMake, runtime behavior, topology cache behavior,
    lightmap source-hash behavior, collision, and preview behavior were not
    changed by this audit.

#### REF-055 `[x]` Extract SideDef/material/decal inspector into `tools/materials/`

- Source/audit reference: REF-053 and
  `docs/audit/sector_editor_materials_boundary_audit.md`.
- Why it helps: removes the largest straightforward material UI block from
  `SectorEditor.cpp` and starts a materials feature folder instead of growing
  generic action/modal categories.
- Likely files: new
  `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.h/.cpp`,
  `SectorEditor.cpp`, `SectorEditor.h`, and existing material/texture helper
  headers as needed.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: build, ctest, `git diff --check`, and manual
  material/decal/texture picker smoke when practical.
- Notes:
  - Preserve `FinishTopologyMaterialMutation()` /
    `FinishMaterialActionResult()` behavior.
  - Preserve `MarkTopologyDocumentEdited()` and 2D topology render-cache
    invalidation behavior.
  - Preserve preview mesh rebuild behavior for material changes made while 3D
    preview is active.
  - Do not move texture catalog/import or add-map-texture ownership in this
    task.
  - Do not move `ApplyTexturePickerSelection()` or `DrawPreviewUvPanel()` in
    this task unless a later implementation explicitly re-scopes the work.
  - Expected `SectorEditor.cpp` reduction: roughly 600-740 lines.
- Completion notes:
  - Completed by extracting the topology SideDef/material/decal inspector into
    `sources/sector_editor/tools/materials/SectorEditorMaterialInspector.h/.cpp`
    with a state/UI/callback context and no concrete `SectorEditor`
    dependency.
  - `SectorEditor.cpp` now keeps a small callback wrapper for
    `DrawTopologySideDefInspector()`; actual line count dropped from 10,551 to
    9,936, a net reduction of 615 lines.
  - Material finish wrappers remain in `SectorEditor.cpp`.
  - `ApplyTexturePickerSelection()` remains in `SectorEditor.cpp`.
  - `DrawPreviewUvPanel()` remains in `SectorEditor.cpp`.
  - No behavior changes were intended.

#### REF-056 `[x]` SectorEditor shared service inventory audit

- Source/audit reference:
  `docs/audit/sector_editor_post_tool_migration_line_map.md`,
  `docs/audit/sector_editor_materials_boundary_audit.md`, and REF-055
  completion notes.
- Why it helps: identifies shared editor services before moving more feature
  modules, so future tools depend on shared APIs instead of long callback
  bridges back into `SectorEditor.cpp`.
- Likely files: documentation only.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: `git diff --check`, `git diff --stat`,
  `git status --short`.
- Notes:
  - Evaluate TexturePickerService, MaterialEditBridge, PreviewSurface/material
    service, LightEditingService, LightmapBakeController, DocumentController,
    AssetCatalog/TextureCatalog, Status/Diagnostics, and selection/manipulation
    refinements.
  - Decide which remaining `SectorEditor.cpp` functions are shared
    capabilities, feature-specific code, or high-level coordinator glue.
  - Keep REF-044 open while material picker routing, preview UV/material panel,
    and wrapper cleanup remain incomplete.
- Completion notes:
  - Completed in
    `docs/audit/sector_editor_shared_service_inventory.md`.
  - Recommended first service implementation: minimal TexturePickerService for
    generic picker lifecycle/result mechanics, preserving feature-specific
    material, door, sky, and sprite apply semantics.
  - Future backlog items were added/refined for TexturePickerService,
    AssetCatalog/TextureCatalog, material-specific picker routing /
    MaterialEditBridge, preview UV/material panel dependencies, status
    diagnostics deferral, and LightmapBakeController runner-plan work.
  - Source code, tests, CMake, runtime behavior, topology cache behavior,
    lightmap source-hash behavior, collision, and preview behavior were not
    changed by this audit.

#### REF-057 `[x]` Extract minimal TexturePickerService

- Source/audit reference:
  `docs/audit/sector_editor_shared_service_inventory.md`.
- Why it helps: centralizes generic texture picker modal/catalog/open/close and
  selected-result mechanics before more material, door, sky, or preview clients
  are moved.
- Likely files: new focused service files under `sources/sector_editor/`,
  `SectorEditorTextureActions.*`, `SectorEditorTextureModals.*`,
  `SectorEditorModalTypes.h`, `SectorEditor.cpp`, and picker call sites.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build, ctest, `git diff --check`; manual texture
  picker smoke for material, sky, door, and preview surfaces when practical.
- Notes:
  - Initial scope is generic picker lifecycle/result mechanics only.
  - Preserve existing picker targets and apply semantics.
  - Do not move material finish wrappers, authoring graph writeback, door
    texture apply, sky apply, sprite picker behavior, add-map texture import, or
    preview rebuild policy.
- Completion notes:
  - Minimal `TexturePickerService` added under
    `sources/sector_editor/services/texture_picker/`.
  - Generic picker lifecycle/result mechanics moved or centralized: close/reset,
    open/modal option population, selected texture extraction, and modal
    callback wiring.
  - Feature-specific apply semantics remain in existing owners.
  - `ApplyTexturePickerSelection()` behavior is unchanged; it now consumes the
    service-selected texture result before running the existing apply routing.
  - No behavior changes intended.

#### REF-058 `[ ]` Audit AssetCatalog/TextureCatalog service boundary

- Source/audit reference:
  `docs/audit/sector_editor_shared_service_inventory.md`.
- Why it helps: separates texture registry scans, imported texture handles,
  sprite metadata catalog, and map texture dictionary helpers from picker apply
  routing before the picker service grows too broad.
- Likely files: `SectorEditorTextureActions.*`,
  `SectorEditorTextureModals.*`, `SectorEditorTypes.h`, texture registry and
  sprite metadata callers.
- Suggested task type: Audit first.
- Risk: Medium.
- Suggested verification: documentation checks only.
- Notes:
  - Decide what belongs in an asset/texture catalog service versus
    TexturePickerService.
  - Keep add-map texture import and sprite metadata repair separate from
    material-specific picker apply semantics.
- Completion notes:

#### REF-059 `[x]` Audit MaterialEditBridge and material-specific picker routing

- Source/audit reference:
  `docs/audit/sector_editor_shared_service_inventory.md` and
  `docs/audit/sector_editor_materials_boundary_audit.md`.
- Why it helps: reduces `tools/materials` callback tentacles without duplicating
  dirty/cache/preview rebuild or authoring-vs-topology routing behavior.
- Likely files: `SectorEditor.cpp`, `SectorEditor.h`,
  `SectorEditorMaterialActions.*`, `tools/materials/`,
  `SectorEditorTextureActions.*`.
- Suggested task type: Audit first.
- Risk: High.
- Suggested verification: audit first; future implementation needs build,
  ctest, picker/material smoke, and explicit cache invalidation report.
- Notes:
  - Preserve `FinishTopologyMaterialMutation()`,
    `FinishMaterialActionResult()`, authoring side/face writeback, UI reset
    flags, preview rebuild behavior, and `MarkTopologyDocumentEdited()`.
  - Decide whether material-specific texture picker apply routing moves into
    `tools/materials` or a smaller `MaterialEditBridge`.
- Completion notes:
  - Completed in `docs/audit/sector_editor_material_edit_bridge_audit.md`.
  - Recommended next implementation: REF-063, extract material-specific texture
    picker routing before adding a broader `MaterialEditBridge`.
  - Expected next-slice reduction: roughly 120-220 lines or 4-6 picker target
    branches isolated; callback reduction likely 0-2 until bridge work follows.
  - Keep REF-044 open because material migration is still incomplete; REF-058
    and REF-060 remain open because this audit did not cover those scopes.

#### REF-063 `[x]` Extract material-specific texture picker routing

- Source/audit reference:
  `docs/audit/sector_editor_material_edit_bridge_audit.md`.
- Why it helps: isolates material picker target routing after REF-057 without
  moving generic picker lifecycle, door, sky, sprite, or add-map texture paths.
- Likely files: `SectorEditor.cpp`, `SectorEditorTextureActions.*`,
  `SectorEditorTextureModals.*`, and a narrow material edit/picker routing file
  under `services/material_edit/` or `tools/materials/` if scoped to material UI.
- Suggested task type: Codex task.
- Risk: High.
- Suggested verification: build, ctest, `git diff --check`, and manual material
  picker smoke for 2D SideDef/sector, authoring side/face, Preview3D surface,
  plus door and sky regression smoke.
- Notes:
  - Keep `TexturePickerService` generic.
  - Move material-specific current/open/apply routing only after clearly
    isolating `Sector`, `SideDef`, `AuthoringFaceAnchor`, and `AuthoringSide`
    target kinds.
  - Preserve authoring temporary topology rollback/apply semantics exactly.
  - Keep material dirty/cache/preview policy out of `TexturePickerService`.
- Completion notes:
  - Material-specific texture picker routing was extracted to
    `sources/sector_editor/services/material_edit/`.
  - `Sector`, `SideDef`, `AuthoringFaceAnchor`, and `AuthoringSide` material
    targets now route through the material picker routing module.
  - `RuntimeDoor`, `MapSky`, sprite picker, and add-map texture scan/import
    behavior stayed out of the material picker route.
  - `TexturePickerService` remains generic lifecycle/result mechanics.
  - Broad `MaterialEditBridge` and finish-wrapper extraction remain future
    work.
  - No behavior changes intended.

#### REF-064 `[x]` Add minimal MaterialEditBridge for material action wrappers

- Source/audit reference:
  `docs/audit/sector_editor_material_edit_bridge_audit.md`.
- Why it helps: centralizes material edit orchestration without moving preview
  UV UI, picker lifecycle, editor lifecycle, renderer lifecycle, or lightmap
  behavior.
- Likely files: `SectorEditor.cpp`, `tools/materials/`, and
  `services/material_edit/`.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: build, ctest, `git diff --check`, dependency grep for
  no concrete `SectorEditor` dependency under `services/material_edit`, and
  manual material inspector smoke if practical.
- Completion notes:
  - Minimal `MaterialEditBridge` was added under `services/material_edit`.
  - Material action wrappers were moved/routed through the bridge while existing
    `SectorEditor` member wrapper names/signatures stayed available for the
    sector inspector and preview UV panel.
  - Broad editor finish paths remain in `SectorEditor.cpp`.
  - `DrawPreviewUvPanel` remains unmoved.
  - `TexturePickerService` remains generic.
  - No behavior changes intended.

#### REF-065 `[x]` Promote MaterialEditBridge into MaterialEditingService

- Source/audit reference:
  `docs/audit/sector_editor_material_edit_bridge_audit.md`.
- Why it helps: replaces the temporary callback bridge with a concrete service
  dependency that tools and panels can call directly.
- Likely files: `SectorEditor.cpp`, `tools/materials/`, and
  `services/material_edit/`.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: build, ctest, `git diff --check`, bridge grep,
  dependency grep for no concrete `SectorEditor` dependency under
  `services/material_edit`, and manual material smoke if practical.
- Completion notes:
  - `MaterialEditBridge` was removed/replaced.
  - Real `SectorEditorMaterialEditingService` was added under
    `services/material_edit`.
  - `tools/materials` now uses the material editing service directly for
    material operations.
  - Material picker routing is used through the material editing service while
    generic picker lifecycle/result mechanics remain in `TexturePickerService`.
  - Old `SectorEditor` material wrapper/callback funnel was removed where
    practical; no remaining material operation wrapper bridges are intended.
  - `DrawPreviewUvPanel` remains in `SectorEditor.cpp`; extraction stays open
    under REF-060.
  - REF-044 remains open as the broader material migration umbrella.
  - REF-058, lights, and lightmap/source-hash-sensitive work remain open.
  - No behavior changes intended.

#### REF-060 `[ ]` Audit Preview UV/material panel service dependencies

- Source/audit reference:
  `docs/audit/sector_editor_shared_service_inventory.md`.
- Why it helps: `DrawPreviewUvPanel()` is line-count-heavy, but it depends on
  picker routing, material finish behavior, selected 3D surface state, and
  preview rebuild hooks.
- Likely files: `SectorEditor.cpp`, `SectorEditorPreviewActions.*`,
  `SectorEditorPreviewSettingsModal.*`, material action files,
  selection service files, and renderer facade headers.
- Suggested task type: Audit first.
- Risk: Medium/High.
- Suggested verification: audit only; future implementation should build,
  ctest, and manually smoke preview UV/material editing if practical.
- Notes:
  - Decide whether `DrawPreviewUvPanel()` belongs in `tools/materials/`, a
    preview module, or a small preview-surface material service.
  - Keep collision/physics/sector lookup and renderer resource orchestration
    central unless a later runner plan explicitly scopes them.
  - Expected later `SectorEditor.cpp` reduction if extracted: roughly 350-480
    lines for UV/material panel, separate from preview overlay.
- Completion notes:

#### REF-066 `[x]` Extract Preview3D UV/material panel into `preview/`

- Source/audit reference:
  `docs/audit/sector_editor_material_edit_bridge_audit.md`.
- Why it helps: removes the selected-surface Preview3D UV/material panel from
  `SectorEditor.cpp` after material operations were promoted to
  `SectorEditorMaterialEditingService`.
- Likely files: `SectorEditor.cpp`,
  `sources/sector_editor/preview/SectorEditorPreviewUvPanel.h`,
  `sources/sector_editor/preview/SectorEditorPreviewUvPanel.cpp`.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: build, ctest, `git diff --check`, dependency grep for
  no concrete `SectorEditor` dependency under `preview/`, and manual Preview3D
  material smoke if practical.
- Completion notes:
  - Preview3D UV/material panel extracted to `sources/sector_editor/preview/`.
  - Created `SectorEditorPreviewUvPanel.h` and
    `SectorEditorPreviewUvPanel.cpp`.
  - Panel uses `SectorEditorMaterialEditingService` directly for material
    operations.
  - No new material callback bridge was introduced; the only panel callback is
    the narrow non-material portal `Blocks Player` edit port.
  - `SectorEditor.cpp` line count reduced from 9,172 to 8,710 lines, a
    462-line reduction.
  - Preview renderer/collision/camera ownership stayed in `SectorEditor.cpp`.
  - Preview overlay/debug UI stayed in `SectorEditor.cpp`.
  - `TexturePickerService` stayed generic.
  - REF-044 remains open because material migration is still incomplete.
  - REF-058, lights, and lightmap/source-hash-sensitive work remain open.
  - No behavior changes intended.

#### REF-074 `[x]` Extract Preview3D overlay/debug UI into `preview/`

- Source/audit reference: REF-074 task.
- Why it helps: removes Preview3D overlay/debug display code from
  `SectorEditor.cpp` while keeping preview renderer, camera, collision,
  lifecycle, material, and lightmap ownership in the editor.
- Likely files: `SectorEditor.cpp`,
  `sources/sector_editor/preview/SectorEditorPreviewOverlay.h`,
  `sources/sector_editor/preview/SectorEditorPreviewOverlay.cpp`.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build, ctest, `git diff --check`, dependency grep for
  no concrete `SectorEditor` dependency under `preview/`, and manual Preview3D
  overlay smoke if practical.
- Completion notes:
  - Preview3D overlay/debug UI extracted to `sources/sector_editor/preview/`.
  - Created `SectorEditorPreviewOverlay.h` and
    `SectorEditorPreviewOverlay.cpp`.
  - `SectorEditor.cpp` line count reduced from 8,548 to 7,809 lines, a
    739-line reduction.
  - Preview renderer/collision/camera ownership stayed in `SectorEditor.cpp`.
  - Light/lightmap/source-hash behavior unchanged.
  - No callback bridge was introduced; the overlay returns one-frame action
    requests for high-level editor-owned actions.
  - No behavior changes intended.
  - REF-058, lights, lightmap/source-hash-sensitive work, and main
    tools/inspector extraction work remain open.

#### REF-067 `[x]` Authoring-owned topology edit cleanup and direct topology mutation inventory

- Source/audit reference: REF-067 task and
  `docs/audit/sector_editor_direct_topology_edit_inventory.md`.
- Why it helps: keeps editable portal/player-blocking ownership on the
  authoring graph instead of silently mutating derived topology.
- Likely files: `SectorEditor.cpp`, `SectorEditorTopologyActions.*`,
  material inspector and Preview3D UV panel callback contexts, audit docs.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: full build, full `ctest`, requested topology mutation
  greps, `git diff --check`, and manual Blocks Player smoke in 2D and
  Preview3D if practical.
- Completion notes:
  - Blocks Player edits use `SectorAuthoringLine::flags.blocksPlayer` when
    authoring data exists.
  - Topology derivation already copies authoring line flags into
    `SectorTopologyLineDef::flags`.
  - Invalid no-authoring edit support removed; Blocks Player now fails when
    authoring data is missing instead of mutating derived topology.
  - Authoring-backed linedefs with no derived authoring mapping now fail with a
    clear status instead of falling back to topology mutation.
  - Inventory report:
    `docs/audit/sector_editor_direct_topology_edit_inventory.md`.
  - Remaining direct topology mutation found: inspector-specific sidedef UV
    apply/reset still mutates topology directly and should move under
    authoring material edits in REF-068.
  - No rendering, collision, serialization, material, lightmap, or preview
    behavior changes intended except the Blocks Player ownership correction.
  - REF-044 remains open because material migration is still incomplete.
  - REF-058, lights, and lightmap/source-hash-sensitive work remain open.

#### REF-068 `[x]` Replace SideDef inspector UV topology mutation with authoring material edits

- Source/audit reference:
  `docs/audit/sector_editor_direct_topology_edit_inventory.md`.
- Why it helps: keeps 2D SideDef inspector UV edits on the authoring graph
  instead of mutating derived topology directly.
- Likely files: `services/material_edit/`, `tools/materials/`, authoring graph
  tests, and direct topology edit inventory/backlog docs.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: full build, full `ctest`, requested topology/material
  greps, `git diff --check`, and manual SideDef UV smoke if practical.
- Completion notes:
  - SideDef inspector UV apply/reset now writes authoring side material data.
  - Direct topology UV mutation was removed for this path, including the
    inspector's immediate `selectedUv` pre-write.
  - Missing authoring mapping and no-authoring data now fail instead of
    mutating derived topology.
  - No no-authoring topology edit behavior remains for this scoped UV path.
  - Inventory updated.
  - No behavior changes intended except the ownership correction.
  - REF-044 remains open as the broader material/tool migration umbrella; the
    broader material scratch/writeback routes were removed later by REF-071.
  - REF-058, preview extraction, lights, and lightmap/source-hash-sensitive work
    remain open.

#### REF-069 `[x]` Remove invalid no-authoring topology-edit support

- Source/audit reference:
  `docs/audit/sector_editor_direct_topology_edit_inventory.md`.
- Why it helps: enforces that editable maps require authoring graph data and
  that `SectorTopologyMap` is not an editable map model.
- Likely files: editor topology/material action services and any explicit
  invalid no-authoring edit-state helpers.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: build, ctest, direct topology mutation greps, and
  targeted invalid no-authoring edit-state checks.
- Completion notes:
  - Normal no-authoring topology edit behavior was removed from map-editing UI
    paths; invalid no-authoring edit states now fail clearly.
  - Blocks Player now uses `SetAuthoringLineDefBlocksPlayer()` and
    `SetSectorEditorAuthoringLineDefBlocksPlayer()`; the old topology mutation
    helper was deleted.
  - Sector name/height/sky/ambient/material/UV callbacks require authoring
    face-anchor data and no longer mutate `SectorTopologySector`.
  - Editor-facing material picker entry points were renamed to
    `OpenMaterialPickerForDerivedSector()` and
    `OpenMaterialPickerForDerivedSideDef()`.
  - Inventory updated with remaining topology-owned runtime/cache/bake state,
    import/migration-only conversion, lights/runtime objects, and then-open
    REF-044 material cleanup work.
  - No missing authoring fields were found for the scoped sector/material/line
    properties.
  - Do not fold this into REF-044 material scratch/writeback cleanup.
  - REF-044 remains open as the broader material/tool migration umbrella.
  - REF-058, preview extraction, lights, and lightmap/source-hash-sensitive work
    remain open.

#### REF-070 `[x]` Write Sector Editor Architectural Principles document

- Source/audit reference:
  `docs/audit/sector_editor_shared_service_inventory.md`,
  `docs/audit/sector_editor_material_edit_bridge_audit.md`,
  `docs/audit/sector_editor_direct_topology_edit_inventory.md`, and recent
  REF-067/REF-068/REF-069 source-of-truth cleanup.
- Why it helps: gives future sector editor tasks a strict architecture contract
  so stale names, transitional code, and legacy fallback remnants are not copied
  forward.
- Likely files:
  `docs/architecture/sector_editor_architectural_principles.md` and this
  backlog.
- Suggested task type: Codex task.
- Risk: Low.
- Suggested verification: `git diff --check`, `git diff --stat`, and
  `git status --short`.
- Completion notes:
  - Architecture principles doc path:
    `docs/architecture/sector_editor_architectural_principles.md`.
  - Source-of-truth invariant documented: editable map data belongs to the
    authoring graph; `SectorTopologyMap` is derived/compiled output and is not
    an editable map model.
  - Service/callback rules documented: tools and panels should depend on real
    services directly, while callback bridges are short-lived migration seams.
  - Future Codex task requirement documented: read the principles before sector
    editor changes, stop on conflicts, avoid copying violations, and document
    temporary exceptions as backlog debt.
  - No source code, tests, CMake, editor/runtime behavior, cache behavior, or
    lightmap source-hash behavior changed.

#### REF-071 `[x]` Remove remaining material topology scratch/writeback routes

- Source/audit reference:
  `docs/architecture/sector_editor_architectural_principles.md` and
  `docs/audit/sector_editor_direct_topology_edit_inventory.md`.
- Why it helps: finishes the material ownership correction by making normal
  material edits write authoring graph material fields directly.
- Likely files: `services/material_edit/`, `SectorEditorMaterialActions.*`,
  authoring graph tests, and direct topology edit inventory/backlog docs.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: full build, full `ctest`, requested material/topology
  greps, `git diff --check`, and material inspector/Preview3D manual smoke if
  practical.
- Completion notes:
  - Remaining known material scratch/writeback routes were removed; no remaining
    normal editor material scratch/writeback route is listed.
  - Direct authoring material edit routes were added for authoring side wall
    parts and authoring face-anchor floor/ceiling flats.
  - Material picker routing now writes selected material texture IDs directly to
    authoring side/face data.
  - No-authoring material edits fail instead of mutating topology.
  - Missing authoring side overrides may still be seeded from current derived
    display values by existing authoring mutation helpers before the direct
    authoring assignment is applied.
  - Inventory updated.
  - No behavior changes intended except the material ownership correction.
  - REF-044 remains open only as the broader material/tool migration umbrella.
  - REF-058, preview extraction, lights, and lightmap/source-hash-sensitive work
    remain open.

#### REF-072 `[x]` Clean up material service debt after authoring-owned material migration

- Source/audit reference:
  `docs/architecture/sector_editor_architectural_principles.md` and
  `docs/audit/sector_editor_direct_topology_edit_inventory.md`.
- Why it helps: removes stale names, wrappers, dead helpers, and callback
  remnants that still implied topology material scratch/writeback.
- Likely files: `services/material_edit/`, material/sector inspector modules,
  `SectorEditorAuthoringState.*`, authoring graph tests, and architecture/audit
  docs.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: full build, full `ctest`, requested material/topology
  greps, `git diff --check`, and material inspector/Preview3D manual smoke if
  practical.
- Completion notes:
  - Removed the obsolete flat material scratch/writeback helper from authoring
    state.
  - Renamed material service internals to `ApplyAuthoringSideMaterialEdit()` and
    `ApplyAuthoringFaceAnchorMaterialEdit()`.
  - Removed material-specific picker wrapper exports from texture actions; tests
    and callers use `services/material_edit` routing/service APIs directly.
  - Removed material operation callback remnants from the sector inspector and
    the sidedef material inspector where `MaterialEditingService` is available.
  - Removed unused `SectorEditor` material picker wrapper methods.
  - Replaced obsolete scratch/writeback implementation tests with equivalent
    authoring-owned material service/picker tests for floor UV, ceiling UV,
    flat texture picker apply, and stale mapping failure.
  - No remaining material source-of-truth debt is currently listed.
  - REF-044 remains open only as the broader material/tool migration umbrella;
    REF-058, preview extraction, lights, and lightmap/source-hash-sensitive work
    remain open.

#### REF-073 `[x]` Rectangle tool authoring-graph split correctness

- Source/audit reference: sector editor architectural principles; editable
  identity must be created in the authoring graph, not derived topology.
- Why it helps: rectangle insertion no longer relies on topology-only splits
  when rectangle edges cross existing authoring lines.
- Likely files: `SectorEditorAuthoringState.*`, rectangle/line tool call paths,
  and authoring graph tests.
- Suggested task type: Codex task.
- Risk: Medium/High.
- Suggested verification: build, ctest, direct topology mutation greps, and
  manual rectangle/selection/material smoke when practical.
- Completion notes:
  - Rectangle insertion now uses an atomic candidate authoring graph and commits
    only after all four edges succeed.
  - Rectangle edges split crossed authoring graph lines and split themselves
    into authoring subsegments before topology derivation.
  - Topology-only edit identity is not used; `SectorTopologyMap` remains derived
    output for rectangle edit identity.
  - Line tool and rectangle tool insertion share the split-aware authoring line
    insertion helper.
  - Tests cover crossed-line splits, rectangle edge splits, shared intersection
    vertices, multiple sorted intersections, corner-on-line splits, overlap
    rejection, non-grid intersection rejection, atomic failure, metadata
    preservation, and derivation through the existing authoring refresh path.
  - No rendering, collision, lightmap, material, texture picker, schema, or
    document lifecycle behavior changes were intended.

#### REF-061 `[>]` Evaluate Status/Diagnostics service

- Source/audit reference:
  `docs/audit/sector_editor_shared_service_inventory.md`.
- Why it helps: may reduce repeated `statusText` and warning callback plumbing
  if service contexts become noisy.
- Likely files: `SectorEditor.cpp`, `SectorEditorTypes.h`, tool contexts,
  preview overlay/status display code.
- Suggested task type: Defer.
- Risk: Low/Medium.
- Suggested verification: audit first if revived.
- Notes:
  - Low priority because it does not unlock major line-count reduction by
    itself.
  - Do not move validation generation, cache warning ownership, or feature
    mutation policy into a generic status sink.
- Completion notes:

#### REF-062 `[ ]` Extract LightmapBakeController

- Source/audit reference:
  `docs/audit/sector_editor_shared_service_inventory.md`.
- Why it helps: could encapsulate async bake worker lifecycle, cancellation,
  temporary file cleanup, result installation, source-hash stale-result
  rejection, object-probe sidecar installation, and bake progress/status.
- Likely files: `SectorEditor.cpp`, `SectorEditorLightmapAsyncTypes.h`,
  `SectorEditorLightmapModal.*`, lightmap backend files, object-probe sidecar
  helpers, and tests around source-hash behavior.
- Suggested task type: Runner plan.
- Risk: High.
- Suggested verification: runner plan with build, ctest, targeted lightmap
  tests, source-hash checks, cancellation/result-install checks, and manual bake
  smoke when practical.
- Notes:
  - Do not combine with broad light editing migration.
  - Preserve source-hash stale-result rejection and object-probe sidecar install
    behavior.
  - Preview result refresh should remain an explicit callback/hook rather than
    exposing renderer internals to the controller.
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

#### REF-043 `[x]` Authoring tool module contract and migration series

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
  - REF-047: Selection service and manipulation provider contract.
  - REF-048: Passive `SelectionTarget` and provider type definitions.
  - REF-049: Selection service extraction.
  - REF-050: Manipulation service shell.
  - REF-051: First move provider pilot.
  - REF-052: Select tool migration using services/providers.
- Notes:
  - Keep current "Authoring Line" terminology until a rename task is explicitly
    scoped.
  - Select migration should wait for Selection service extraction and a
    Manipulation provider contract.
  - Select is not just another authoring tool; it is the frontend for
    selection/manipulation behavior.
  - Do not migrate Select as a giant switch over movable types.
- Completion notes:
  - REF-043a added a minimal tool module descriptor/lookup.
  - REF-043a migrated Authoring Line behavior into `tools/line/`.
  - REF-043b migrated Authoring Rectangle behavior into `tools/rectangle/`.
  - REF-043b registered Rectangle in `FindSectorEditorToolModule()`.
  - REF-043c migrated Authoring Insert Vertex behavior into
    `tools/insert_vertex/`.
  - REF-043c registered Insert Vertex in `FindSectorEditorToolModule()`.
  - `SectorEditor` uses module dispatch for Line, Rectangle, Insert Vertex, and
    Select.
  - REF-047 completed the Selection service and Manipulation provider contract
    report.
  - REF-052 migrated Select using Selection and Manipulation services/providers.
  - Movement providers can continue as future refinements.
  - No behavior changes intended.

#### REF-047 `[x]` Selection service and manipulation provider contract

- Source/audit reference:
  `docs/audit/sector_editor_selection_manipulation_contract.md`.
- Why it helps: prevents Select migration from becoming a giant switch over all
  selectable/movable primitives.
- Likely files: audit/backlog documentation only.
- Suggested task type: Audit first.
- Risk: Low.
- Suggested verification: `git diff --check`, `git diff --stat`,
  `git status --short`.
- Notes:
  - Selection should own selected IDs/state transitions/queries.
  - Manipulation should own generic drag transaction lifecycle and provider
    dispatch.
  - Select should own mouse behavior, pick cycling, and initiating
    manipulation.
- Completion notes:
  - Completed in
    `docs/audit/sector_editor_selection_manipulation_contract.md`.
  - Recommended first implementation task: add passive `SelectionTarget` and
    provider/capability type definitions only.

#### REF-048 `[x]` Add passive SelectionTarget and provider type definitions

- Source/audit reference:
  `docs/audit/sector_editor_selection_manipulation_contract.md`.
- Why it helps: gives selection, picking, and manipulation code one target
  vocabulary before behavior migration.
- Likely files: `SectorEditorSelectionTypes.h` or a new narrow selection
  provider header.
- Suggested task type: Codex task.
- Risk: Low/Medium.
- Suggested verification: build, ctest, `git diff --check`.
- Notes:
  - No behavior changes.
  - Preserve existing selection state fields during this slice.
- Completion notes:
  - Added passive `SectorEditorSelectionTarget` and provider/capability type
    definitions in `sources/sector_editor/selection/SectorEditorSelectionTarget.h`.
  - No behavior migration was performed.
  - Current selected state fields are preserved.
  - Future REF-049/050/051/052 work will use these definitions.

#### REF-049 `[x]` Extract Selection service helpers

- Source/audit reference:
  `docs/audit/sector_editor_selection_manipulation_contract.md`.
- Why it helps: centralizes clear/select/query/stale cleanup before Select
  migration.
- Likely files: new selection service files, `SectorEditor.cpp/.h`.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build, ctest, `git diff --check`, and manual
  selection smoke for topology, authoring, runtime object, light, and surface
  targets.
- Notes:
  - Preserve current selected state fields and UI reset behavior.
  - Keep object-specific movement out of the Selection service.
- Completion notes:
  - Extracted selection service helpers into
    `sources/sector_editor/selection/SectorEditorSelectionService.h/.cpp`.
  - Moved passive SelectionTarget definitions to
    `sources/sector_editor/selection/SectorEditorSelectionTarget.h`.
  - Current selected state fields remain preserved as the source of truth.
  - Select tool input/pick/drag behavior was not migrated.
  - Manipulation service and providers remain future work.
  - No behavior changes intended.

#### REF-050 `[x]` Add Manipulation service shell

- Source/audit reference:
  `docs/audit/sector_editor_selection_manipulation_contract.md`.
- Why it helps: separates generic drag transaction lifecycle from Select input
  behavior.
- Likely files: new manipulation service files, select input glue.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build, ctest, `git diff --check`, and manual drag
  finish/cancel smoke for current movable targets.
- Notes:
  - Initially delegate to existing drag functions.
  - Do not move object-specific mutation logic into the service.
- Completion notes:
  - Added the Manipulation service shell in
    `sources/sector_editor/selection/SectorEditorManipulationService.h/.cpp`.
  - Current drag state fields remain preserved as the source of truth.
  - Existing movement implementations still own object-specific movement.
  - The service delegates to existing drag paths through callbacks.
  - No provider dispatch was added yet.
  - No behavior changes intended.

#### REF-051 `[x]` Pilot first move provider

- Source/audit reference:
  `docs/audit/sector_editor_selection_manipulation_contract.md`.
- Why it helps: proves the provider contract on one bounded movement family.
- Likely files: `tools/placed_objects/`, `tools/billboards/`, manipulation
  service glue.
- Suggested task type: Codex task.
- Risk: Medium.
- Suggested verification: build, ctest, `git diff --check`, and manual
  billboard move/cancel plus door refusal smoke.
- Notes:
  - Prefer placed-object/billboard movement because its drag code already has a
    callback context.
  - Preserve the current door movement refusal status text.
- Completion notes:
  - Added the first move provider pilot for placed objects.
  - Placed-object/billboard movement now routes through the provider from the
    Manipulation service.
  - Door movement refusal is preserved by the existing placed-object drag path.
  - Authoring vertex and light movement remain on fallback callbacks.
  - No Select migration was performed.
  - No behavior changes intended.

#### REF-052 `[x]` Migrate Select tool using services/providers

- Source/audit reference:
  `docs/audit/sector_editor_selection_manipulation_contract.md`.
- Why it helps: finishes Select migration without making Select own
  object-specific movement.
- Likely files: `tools/select/`, Selection service, Manipulation service,
  `SectorEditor.cpp/.h`.
- Suggested task type: Codex task.
- Risk: High.
- Suggested verification: build, ctest, `git diff --check`, and manual smoke
  for pick cycling, hover, clear selection, drag threshold, movable targets, and
  selectable-only targets.
- Notes:
  - Do after REF-048 through REF-051.
  - Select should call services/providers rather than switching over every
    primitive.
- Completion notes:
  - Migrated Select into `sources/sector_editor/tools/select/`.
  - Select uses Selection and Manipulation services for selection state changes
    and drag/manipulation lifecycle.
  - Select does not own object-specific movement implementation.
  - Placed-object movement still routes through the provider.
  - Authoring vertex and light movement remain fallback movement paths.
  - Future provider refinements remain open for authoring vertex movement,
    light movement, and topology/preview selection providers if useful later.
  - No behavior changes intended.

#### REF-044 `[~]` Migrate material/sidedef/decal editing into `tools/materials/`

- Source/audit reference:
  `docs/audit/sector_editor_tool_module_boundary_design.md` and
  `docs/audit/sector_editor_materials_boundary_audit.md`.
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
  - REF-053 split this broad item into REF-054
    materials/sidedef/decal boundary audit and REF-055 SideDef/material/decal
    inspector extraction. Treat REF-044 as the umbrella, not the next direct
    implementation task.
- Completion notes:
  - REF-054 completed the materials/sidedef/decal boundary audit at
    `docs/audit/sector_editor_materials_boundary_audit.md`.
  - REF-055 extracted the SideDef/material/decal inspector into
    `tools/materials/`; follow-up material picker routing, preview UV panel, and
    wrapper cleanup tasks were completed by later service/panel cleanup tasks.
  - REF-044 remains open only as a broad umbrella for future material/tool
    module cleanup if new concrete debt appears.

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
  - REF-056 keeps broad light editing as audit-first and separates it from the
    LightmapBakeController runner-plan work.
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
  - REF-056 split the immediate preview question into REF-060 for the preview
    UV/material panel dependency audit; keep preview overlay/debug UI migration
    separate from material editing.
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
