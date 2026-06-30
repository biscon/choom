# Sector Dynamic Spotlight Shadow Map Plan

## How To Use This Plan

This is a living execution plan.

When an agent is asked to execute this plan, it must:

1. Read this section first.
2. Read the `plan-state-json` block.
3. Identify the selected phase/pass.
4. Execute only that selected phase/pass.
5. Do not skip ahead.
6. Do not execute multiple phases/passes in one run unless the selected item explicitly says it is a combined pass.
7. If the selected item is too broad, update this plan with smaller child passes and stop.
8. If smaller passes are added, do not also implement source changes in the same run unless explicitly instructed.
9. After executing a phase/pass, update this plan with status, date, summary, verification results, and behavior notes.
10. Do not claim manual verification unless it was actually performed.
11. Keep this plan self-tracking so future fresh-context runs can resume from it.

```plan-state-json id="dynamic-spot-shadowmaps"
{
  "plan_id": "sector_dynamic_spotlight_shadow_map_plan",
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
      "title": "Shadow-Casting Dynamic Spotlight Data And UI",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_01a",
      "title": "Add Dynamic Spotlight Shadow Settings And Serialization",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_01b",
      "title": "Add Dynamic Spotlight Shadow Inspector Controls",
      "type": "pass",
      "parent": "phase_01",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Shadow Caster Selection And Shadow Map Resources",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02a",
      "title": "Select Shadow-Casting Dynamic Spotlights Under Budget",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_02b",
      "title": "Create Shadow Map Render Targets And Light View Projections",
      "type": "pass",
      "parent": "phase_02",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Shadow Map Depth Rendering",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03a",
      "title": "Render Sector Geometry Into Spotlight Shadow Maps",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_03b",
      "title": "Support Alpha-Tested Middle Textures In Shadow Pass",
      "type": "pass",
      "parent": "phase_03",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Main Shader Shadow Sampling",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04a",
      "title": "Pack Shadow Slots And Light-Space Matrices For Dynamic Lights",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_04b",
      "title": "Apply PCF Shadowing To Dynamic Spotlight Contribution",
      "type": "pass",
      "parent": "phase_04",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Debug Polish Tests And Completion",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_05a",
      "title": "Add Shadow Debug Readout Tune Defaults And Close Plan",
      "type": "pass",
      "parent": "phase_05",
      "status": "Completed"
    }
  ]
}
```

## Current Progress

| Phase / Pass                                                            | Status      | Date | Notes                                                                     |
| ----------------------------------------------------------------------- | ----------- | ---- | ------------------------------------------------------------------------- |
| Phase 1: Shadow-Casting Dynamic Spotlight Data And UI                   | Completed   | 2026-06-30 | Phase 1A and Phase 1B complete.                                           |
| Phase 1A: Add Dynamic Spotlight Shadow Settings And Serialization       | Completed   | 2026-06-30 | Added dynamic spotlight shadow data and JSON persistence only.             |
| Phase 1B: Add Dynamic Spotlight Shadow Inspector Controls               | Completed   | 2026-06-30 | Added dynamic spotlight-only shadow inspector controls.                    |
| Phase 2: Shadow Caster Selection And Shadow Map Resources               | Completed   | 2026-06-30 | Phase 2A and Phase 2B complete.                                           |
| Phase 2A: Select Shadow-Casting Dynamic Spotlights Under Budget         | Completed   | 2026-06-30 | Selects up to 2 shadow-casting dynamic spotlights from selected runtime lights. |
| Phase 2B: Create Shadow Map Render Targets And Light View Projections   | Completed   | 2026-06-30 | Allocates preview-owned 1024x1024 shadow maps and computes finite spotlight matrices. |
| Phase 3: Shadow Map Depth Rendering                                     | Completed   | 2026-06-30 | Phase 3A and Phase 3B complete.                                          |
| Phase 3A: Render Sector Geometry Into Spotlight Shadow Maps             | Completed   | 2026-06-30 | Renders cached sector draw records into selected dynamic spotlight shadow maps. |
| Phase 3B: Support Alpha-Tested Middle Textures In Shadow Pass           | Completed   | 2026-06-30 | Alpha-tested shadow caster records sample base textures and discard below cutoff. |
| Phase 4: Main Shader Shadow Sampling                                    | Completed   | 2026-06-30 | Phase 4A and Phase 4B complete.                                           |
| Phase 4A: Pack Shadow Slots And Light-Space Matrices For Dynamic Lights | Completed   | 2026-06-30 | Main shader receives shadow slot/matrix/bias/strength data and shadow map samplers. |
| Phase 4B: Apply PCF Shadowing To Dynamic Spotlight Contribution         | Completed   | 2026-06-30 | Dynamic spotlight direct lighting is attenuated with 3x3 PCF shadow sampling. |
| Phase 5: Debug Polish Tests And Completion                              | Completed   | 2026-06-30 | Phase 5A complete; dynamic spotlight shadow map plan closed.              |
| Phase 5A: Add Shadow Debug Readout Tune Defaults And Close Plan         | Completed   | 2026-06-30 | Added shadow budget debug readout, inspector budget note, and final docs. |

## Execution Log

### 2026-06-30 - Phase 1A

Status: Completed.

Summary:

* Added persisted dynamic spotlight shadow request fields: `castsShadow`, `shadowPriority`, `shadowBias`, and `shadowStrength`.
* Defaults are `castsShadow=false`, `shadowPriority=0`, `shadowBias=0.002`, and `shadowStrength=1.0`.
* Missing fields load as defaults. Default-valued fields are omitted on save. Finite numeric shadow values are clamped defensively on load/save.
* Added serialization and authoring round-trip coverage, invalid-type rejection coverage, and explicit lightmap source hash coverage for dynamic spotlight shadow edits.

Behavior notes:

* Source code changed.
* No inspector fields were added in this pass.
* No renderer, shader, shadow map resource, dynamic lighting, static lighting, collision, or preview behavior changed.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores dynamic spotlight shadow settings.
* No topology mutation paths were changed, so topology render-cache invalidation behavior is unchanged.
* No generated artifacts were created.
* No manual GUI verification was performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` reported 5 files changed.
* `git status --short` showed only the active plan, data, serialization, and test files modified.

### 2026-06-30 - Phase 1B

Status: Completed.

Summary:

* Added dynamic spotlight-only inspector controls for `Cast Shadows`, `Shadow Priority`, `Shadow Bias`, and `Shadow Strength`.
* Shadow priority, bias, and strength edits use the existing clamped authoring ranges.
* Inspector content height now accounts for the added dynamic spotlight shadow rows.

Behavior notes:

* Source code changed.
* Editor behavior changed only for selected dynamic spotlights; dynamic point lights, static point lights, and static spotlights do not show these controls.
* Editing these fields calls the existing topology document edited path, so normal document dirty/cache invalidation behavior is followed.
* Serialization field names and defaults remain those added in Phase 1A: `castsShadow=false`, `shadowPriority=0`, `shadowBias=0.002`, and `shadowStrength=1.0`.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores dynamic spotlight shadow settings.
* No renderer, shader, shadow map resource, dynamic lighting, static lighting, collision, or preview behavior changed.
* No generated artifacts were created.
* No manual GUI verification was performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` reported 7 files changed total in the worktree, including prior Phase 1A changes and this Phase 1B inspector update.
* `git status --short` showed the active plan, prior Phase 1A source/test files, and this Phase 1B inspector source files modified.

### 2026-06-30 - Phase 2A

Status: Completed.

Summary:

* Added the named default dynamic spotlight shadow budget: `MaxDynamicSpotLightShadowCasters = 2`.
* Dynamic spotlight runtime uniforms now carry shadow request metadata: `castsShadow`, `shadowPriority`, `shadowBias`, and `shadowStrength`.
* Added `SelectRankedSectorPreviewDynamicSpotLightShadowCasters()` to select shadow casters only from already-selected runtime dynamic lights.
* Shadow caster selection filters to dynamic spotlights with `castsShadow=true`, ranks by higher `shadowPriority`, then higher dynamic light contribution score, then lower stable light ID, and assigns sequential shadow slots.
* Added tests covering selected-light eligibility, `castsShadow=false` filtering, dynamic point filtering, priority/score/ID ordering, slot/index output, and the cap of 2.

Behavior notes:

* Source code changed.
* Dynamic light selection and packing remain otherwise unchanged; selected dynamic lights can still illuminate without a shadow slot.
* Dynamic point lights and static lights do not receive shadow slots.
* No shadow map render targets, light view/projection matrices, depth rendering, shader sampling, resource lifecycle, dynamic lighting visuals, static lighting, serialization, editor UI, topology mutation, collision, sector lookup, or physics behavior changed.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores runtime dynamic spotlight shadow selection.
* No topology mutation paths were changed, so topology render-cache invalidation behavior is unchanged.
* No generated artifacts were created. The plan currently has no `sandbox_dir` field.
* No manual GUI verification was performed.

Verification:

* `ctest --test-dir cmake-build-debug --output-on-failure -R SectorTopologyMeshBuilder` found no tests because the registered test name is lowercase.
* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` reported 10 files changed total in the worktree, including prior Phase 1A/1B changes and this Phase 2A selection update.
* `git status --short` showed the active plan, prior Phase 1A/1B source/test files, and this Phase 2A source/test files modified.

### 2026-06-30 - Phase 2B

Status: Completed.

Summary:

* Added the named dynamic spotlight shadow map resolution: `DynamicSpotLightShadowMapResolution = 1024`.
* Added per-shadow-slot matrix data for selected dynamic spotlight shadow casters: view, projection, and `projection * view` light-view-projection matrices.
* Spotlight shadow matrix construction uses the selected dynamic spotlight position/direction, a perspective projection based on the outer cone, a 0.05 near plane, and the light range as the far plane.
* Degenerate spotlight directions use the existing safe downward fallback and vertical directions choose a non-parallel up vector.
* `SectorMeshPreview` now allocates two preview-owned 1024x1024 render textures during renderer resource setup and unloads them during preview resource teardown.
* Added matrix coverage for finite output, degenerate/vertical directions, invalid point/range rejection, slot metadata, invalid caster-index skipping, and the 1024 default resolution.

Behavior notes:

* Source code changed.
* Resource lifecycle changed: dynamic spotlight shadow maps are created with preview renderer resources and unloaded with preview renderer resources. They are not allocated per frame.
* Dynamic light selection remains otherwise unchanged; selected dynamic lights can still illuminate without a shadow slot.
* No depth rendering, shader sampling, visible dynamic lighting attenuation, static lighting, serialization, editor UI, topology mutation, collision, sector lookup, or physics behavior changed.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores runtime dynamic spotlight shadow resources and matrices.
* No topology mutation paths were changed, so topology render-cache invalidation behavior is unchanged.
* No generated artifacts were created. The plan currently has no `sandbox_dir` field.
* No manual GUI verification was performed.

Verification:

* `cmake --build cmake-build-debug --target sector_topology_mesh_builder_tests -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R sector_topology_mesh_builder_tests` found no tests because the registered test name is `sector_topology_mesh_builder`.
* `ctest --test-dir cmake-build-debug --output-on-failure -R sector_topology_mesh_builder` passed.
* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` reported 12 files changed total in the worktree, including prior Phase 1A/1B and Phase 2A changes plus this Phase 2B update.
* `git status --short` showed the active plan, prior Phase 1A/1B/2A source/test files, and this Phase 2B source/test files modified.

### 2026-06-30 - Phase 3A

Status: Completed.

Summary:

* Added a dedicated dynamic spotlight shadow caster shader/material for `SectorMeshPreview`.
* The shadow caster vertex shader transforms sector mesh positions with the selected spotlight `lightViewProjection` matrix.
* The shadow caster pass writes depth into the shadow render target depth texture while normal depth testing resolves nearest sector geometry.
* The shadow render pass explicitly enables depth testing while drawing each shadow map and disables it before returning to the main scene path.
* The active editor preview path now calls `SectorEditor::RenderPreview3DShadowMaps()` before binding `worldTarget` for the visible 3D scene.
* `SectorEditor::RenderPreview3DShadowMaps()` delegates to `SectorMeshPreview::RenderDynamicSpotLightShadowMaps()`; `SectorMeshPreview::DrawScene()` only draws the visible scene and no longer switches render targets internally.
* Each selected dynamic spotlight shadow slot renders all cached sector draw records into its existing preview-owned shadow render target.

Behavior notes:

* Source code changed.
* Main scene visual output is intended to remain unchanged because no main shader shadow sampling was added in this pass.
* Framebuffer ownership changed so shadow maps are rendered before the visible scene target is bound, avoiding hidden render-target changes during `DrawScene()`.
* Geometry included in the shadow pass: cached sector draw records, including floors, ceilings, walls, lower/upper strips, and middle texture records.
* Geometry excluded from the shadow pass: sky cylinder, sky cap, debug overlays, UI, bloom fullscreen passes, and editor gizmos.
* Alpha-tested draw records are included as opaque casters in this pass; alpha-cutout shadow behavior remains explicitly deferred to Phase 3B.
* Dynamic lighting selection, dynamic light contribution, bloom source rendering, static light baking, serialization, editor UI, topology mutation, collision, sector lookup, and physics behavior were not changed.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores runtime dynamic spotlight shadow map rendering.
* No topology mutation paths were changed, so topology render-cache invalidation behavior is unchanged.
* No generated artifacts were created. The plan currently has no `sandbox_dir` field.
* No manual GUI verification was performed.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R sector_topology_mesh_builder` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` reported 15 files changed total in the worktree, including prior Phase 1A/1B/2A/2B changes plus this Phase 3A update.
* `git status --short` showed the active plan plus current source/test scope: `Main.cpp`, dynamic spotlight selection, `SectorMeshPreview`, topology light serialization/types, `SectorEditor` preview/inspector/types, and topology lightmap/mesh builder/serialization tests.

### 2026-06-30 - Phase 3B

Status: Completed.

Summary:

* Updated the dynamic spotlight shadow caster shader to carry base texture UVs into the fragment shader.
* Added shadow caster uniforms for `alphaTest` and `alphaCutoff`.
* Alpha-tested sector draw records now bind the same base/diffuse texture used by visible rendering and discard fragments whose sampled alpha is below the record cutoff.
* Opaque sector draw records continue to render depth normally and do not depend on alpha texture sampling.
* The editor preview shadow-map pass now receives `AssetManager&` so it can resolve the base texture for alpha-tested shadow casters.

Behavior notes:

* Source code changed.
* Shader behavior changed only for the dynamic spotlight shadow caster pass; alpha-tested middle texture records no longer cast solid rectangular shadow slabs when their base texture alpha is available.
* The shadow pass still excludes sky cylinder, sky cap, debug overlays, UI, bloom fullscreen passes, and editor gizmos.
* Decal textures and lightmaps are not used by the shadow pass.
* Main scene visual output is intended to remain unchanged until Phase 4 shader sampling uses the shadow maps.
* Dynamic lighting selection, static light baking, serialization, editor UI, topology mutation, collision, sector lookup, and physics behavior were not changed.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores runtime dynamic spotlight shadow rendering.
* No topology mutation paths were changed, so topology render-cache invalidation behavior is unchanged.
* No generated artifacts were created. The plan currently has no `sandbox_dir` field.
* No manual GUI verification was performed; manual smoke with an alpha-tested grate/bars middle texture and a shadow-casting spotlight remains recommended once Phase 4 makes shadows visible in the main shader.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R sector_topology_mesh_builder` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* Final `git diff --check`, `git diff --stat`, and `git status --short` results are recorded in the final report for this pass.

### 2026-06-30 - Phase 4A

Status: Completed.

Summary:

* Added packed dynamic spotlight shadow uniforms: `dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS]`, `shadowLightMatrices[MAX_DYNAMIC_SHADOW_CASTERS]`, `shadowBias[MAX_DYNAMIC_SHADOW_CASTERS]`, and `shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS]`.
* Dynamic lights without an active shadow map slot pack `shadowSlot=-1`; active dynamic spotlight shadow casters pack deterministic slots `0..1`.
* Dynamic point lights cannot receive non-negative shadow slots even if their local test data has `castsShadow=true`.
* The main sector shader now declares explicit shadow samplers `shadowMap0` and `shadowMap1`.
* `SectorMeshPreview` uploads packed shadow slots, per-slot light-space matrices, per-slot bias/strength, and binds the two preview-owned shadow map textures to the explicit samplers.
* Added mesh-builder test coverage for shadow slot packing, point-light exclusion, bias/strength packing, finite matrix packing, and zero-caster defaults.

Behavior notes:

* Source code changed.
* Shader uniform/sampler layout changed for the main sector shader. Existing material maps remain `texture0` for base/diffuse, `texture1` for lightmap, and `decalTexture` for decals; runtime shadow maps use explicit `shadowMap0` and `shadowMap1` samplers.
* No PCF sampling, shadow comparison, or dynamic light attenuation was added in this pass; visible lighting is intended to remain unchanged until Phase 4B.
* Dynamic point lights remain unshadowed. Dynamic spotlights without shadow slots remain unshadowed but still lit.
* Static baked lighting, AO, ambient, decals, bloom source rendering, serialization, editor UI, topology mutation, collision, sector lookup, and physics behavior were not changed.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores runtime dynamic spotlight shadow slots, matrices, samplers, bias, and strength.
* No topology mutation paths were changed, so topology render-cache invalidation behavior is unchanged.
* No generated artifacts were created. The plan currently has no `sandbox_dir` field.
* No manual GUI verification was performed.

Verification:

* `cmake --build cmake-build-debug --target sector_topology_mesh_builder_tests -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R sector_topology_mesh_builder` passed.
* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* Final `git diff --check`, `git diff --stat`, and `git status --short` results are recorded in the final report for this pass.

### 2026-06-30 - Phase 4B

Status: Completed.

Summary:

* Added main sector fragment shader shadow visibility helpers for selected dynamic spotlight shadow slots.
* Dynamic spotlights with `dynamicLightShadowSlots[i] >= 0` now project the fragment world position through that slot's `shadowLightMatrices` entry.
* Shadow map sampling uses fixed 3x3 PCF and compares projected depth against the selected shadow map's stored depth.
* `shadowBias[slot]` subtracts from projected compare depth, and `shadowStrength[slot]` blends between fully visible lighting and PCF visibility.
* Dynamic spotlight direct contribution is attenuated by the resulting shadow factor; dynamic point lights and unshadowed dynamic spotlights keep their existing contribution path.

Behavior notes:

* Source code changed.
* Shader behavior changed for the main sector shader: runtime dynamic spotlight direct lighting can now be attenuated by selected shadow maps.
* Sampler/uniform layout remains the Phase 4A layout: base/diffuse `texture0`, lightmap `texture1`, decals `decalTexture`, and explicit runtime shadow samplers `shadowMap0` and `shadowMap1`; shadow metadata remains `dynamicLightShadowSlots`, `shadowLightMatrices`, `shadowBias`, and `shadowStrength`.
* The PCF kernel is fixed 3x3 over the shadow map texel size. Out-of-frustum or invalid projected shadow coordinates are treated as fully visible.
* Shadows affect only runtime dynamic spotlight direct lighting. Baked direct lighting, sector ambient, baked AO, decals, bloom source rendering, static lights, and dynamic point lights are not shadowed by this pass.
* Alpha-tested visible rendering behavior is unchanged; alpha-tested shadow caster behavior remains the Phase 3B behavior.
* Serialization, editor UI, topology mutation, collision, sector lookup, and physics behavior were not changed.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores runtime dynamic spotlight shadow sampling.
* No topology mutation paths were changed, so topology render-cache invalidation behavior is unchanged.
* No generated artifacts were created. The plan currently has no `sandbox_dir` field.
* No manual GUI verification was performed; manual smoke with an occluder, alpha-tested middle texture, and shadow-casting dynamic spotlight remains recommended.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R sector_topology_mesh_builder` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* Final `git diff --check`, `git diff --stat`, and `git status --short` results are recorded in the final report for this pass.

### 2026-06-30 - Phase 5A

Status: Completed.

Summary:

* Added runtime preview debug text for dynamic spotlight shadow budget state: active shadow caster count, selected shadow-request candidate count, max budget, and active shadow caster IDs.
* Added a dynamic spotlight inspector note explaining that Cast Shadows requests one of the 2 shadow slots, priority decides budget allocation, and over-budget spotlights still illuminate.
* Kept the established shadow defaults unchanged: `castsShadow=false`, `shadowPriority=0`, `shadowBias=0.002`, `shadowStrength=1.0`, `MaxDynamicSpotLightShadowCasters=2`, `DynamicSpotLightShadowMapResolution=1024`, and fixed 3x3 PCF.
* Updated `docs/sector_dynamic_lighting_design.md` to describe the completed dynamic spotlight shadow-map behavior, exclusions, defaults, debug output, and deferred shadow/volumetric work.
* Closed Phase 5A and the parent Phase 5 in this plan.

Behavior notes:

* Source code changed.
* Runtime debug text changed: `RenderDebugText()` now appends dynamic spotlight shadow caster/candidate/budget state.
* Editor behavior changed only by adding a static explanatory note in the selected dynamic spotlight inspector; no inspector data fields or save/load behavior changed.
* Shader math, dynamic light selection, shadow caster selection, shadow rendering, PCF sampling, static light baking, serialization, public topology schema, collision, sector lookup, and physics behavior were not changed.
* `ComputeSectorLightmapSourceHash()` remains unchanged and ignores runtime dynamic spotlight shadow settings, slots, resources, matrices, and sampling.
* No topology mutation paths were changed, so topology render-cache invalidation behavior is unchanged.
* No generated artifacts were created. The plan currently has no `sandbox_dir` field.
* No manual GUI verification was performed. Manual smoke remains recommended for visible shadows, over-budget behavior, priority, bias/strength, alpha-tested middle textures, dynamic point lights, static baked lights, flicker, pilot/overlay, and portal culling.

Verification:

* `cmake --build cmake-build-debug -j2` passed.
* `ctest --test-dir cmake-build-debug --output-on-failure` passed, 13/13 tests.
* `git diff --check` passed.
* `git diff --stat` reported 11 files changed total in the worktree, including prior Phase 1A through Phase 4B changes plus this Phase 5A update.
* `git status --short` showed the active plan, prior phase source/test files, and this Phase 5A doc/debug/inspector files modified.

Final completion note:

* All planned phases and passes are now marked `Completed`. Deferred work remains limited to later plans: dynamic point-light cubemap shadows, static runtime shadows, flashlight-specific behavior, shadow quality/per-light resolution, volumetric shafts/god rays, projected cookies/gobos, cascaded/variance/contact shadows, and clustered/tiled/deferred lighting.

## Execution Tracking Rules

* Each pass must leave the project buildable and runnable.
* Each pass final report must state whether source code changed.
* Each implementation pass must update this document before finishing.
* The update should be small and local.
* Do not rewrite unrelated phases when marking progress.
* If behavior is intended to remain unchanged, explicitly state that.
* If a pass changes serialization, generated data, public APIs, runtime behavior, cache invalidation, shader behavior, editor behavior, lightmap hashing, baking, or build/test behavior, clearly say so.
* Do not claim manual GUI verification unless it was actually performed.
* If a pass is too broad, split it into smaller child passes and stop without source changes.

## Goal And Desired End State

Add runtime shadow maps for selected **dynamic spotlights only**.

Desired end state:

* Dynamic spotlights can opt in to shadow casting.
* Dynamic point lights do not cast shadows.
* Static point/spot lights do not use runtime shadow maps because they are baked.
* A small global shadow-caster budget is enforced.
* Default shadow budget is 2 dynamic spotlight shadow casters.
* Dynamic lights can still contribute without a shadow slot.
* Shadow maps affect only runtime dynamic spotlight direct lighting.
* Baked direct lighting, sector ambient, baked AO, decals, and emissive bloom remain conceptually separate.
* Alpha-tested middle textures cast correct cutout shadows from the first usable version.
* No point-light cubemap shadows.
* No flashlight gameplay item yet.
* No volumetric shafts/god rays in this plan.

Conceptual composition:

```text id="gi29rg"
lighting = bakedDirect + sectorAmbient * bakedAO + dynamicDirectShadowed
finalRgb = surfaceRgb * lighting
```

Shadow maps only attenuate the relevant dynamic spotlight’s contribution.

## Dependency Direction Rules

* Dynamic spotlight authoring data may request shadows.
* Runtime selected dynamic lights are chosen before shadow caster selection.
* Shadow caster selection may depend on selected dynamic spotlights, shadow settings, priority, and contribution score.
* Shadow rendering may depend on sector draw records and material alpha-test data.
* Main scene rendering may sample shadow maps for dynamic spotlights with assigned shadow slots.
* Static light baking must not depend on runtime shadow maps.
* Runtime shadow maps must not affect lightmap source hash.
* Dynamic point lights must not require shadow data.
* Static point/spot lights must not be packed into dynamic shadow slots.

## Global Defaults And Limits

Use named constants.

Initial recommended defaults:

```text id="3u0flw"
MAX_DYNAMIC_SHADOW_CASTERS = 2
shadow map resolution = 1024
default castsShadow = false
default shadowPriority = 0
default shadowBias = implementation-tuned sane default
default shadowStrength = 1.0
```

Possible future quality settings are out of scope, but this design should not prevent:

```text id="ccere6"
Low = 1
Normal = 2
High = 4
```

## Proposed Phases

### Phase 1: Shadow-Casting Dynamic Spotlight Data And UI

Goal:

Add opt-in shadow settings to dynamic spotlights without rendering shadow maps yet.

Why it helps:

This makes shadow intent explicit in authoring data and keeps the first pass isolated from renderer complexity.

Files/functions likely touched:

* `sources/sector_demo/SectorTopologyTypes.h`
* `sources/sector_demo/SectorTopologySerialization.cpp`
* dynamic spotlight inspector files
* serialization tests
* lightmap hash tests if applicable

Exact behavior that must remain unchanged:

* Dynamic spotlights render as before.
* Dynamic point lights render as before.
* Static point/spot lights bake as before.
* Runtime shadow maps do not exist yet.
* Lightmap source hash remains unchanged by dynamic spotlight shadow settings.
* Existing dynamic spotlight save/load remains compatible.

Risks/goblins:

* Accidentally adding shadow settings to static lights.
* Accidentally including dynamic shadow settings in lightmap source hash.
* Adding too many per-light knobs too early.
* Confusing shadow priority with dynamic light priority.

Non-goals:

* No shadow map render targets.
* No shadow depth pass.
* No shader shadow sampling.
* No point-light shadows.
* No static-light runtime shadows.
* No volumetric shafts.

Suggested checks:

```bash id="w337ka"
git diff --check
git diff --stat
git status --short
```

Run relevant serialization/hash tests.

Final report expectations:

* State files changed.
* State serialized field names.
* State default values.
* State inspector fields.
* Confirm lightmap source hash unchanged by dynamic shadow settings.
* Confirm no renderer behavior changed yet.
* State verification commands/results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 1 `Completed` only after Phase 1A and Phase 1B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 1A: Add Dynamic Spotlight Shadow Settings And Serialization

Goal:

Add persisted shadow request settings to dynamic spotlights.

Implementation guidance:

Extend dynamic spotlight data with fields such as:

```cpp id="8wnkiy"
bool castsShadow = false;
int shadowPriority = 0;
float shadowBias = 0.002f;
float shadowStrength = 1.0f;
```

Use actual project naming/style.

Serialization:

* field names suggested:

    * `castsShadow`
    * `shadowPriority`
    * `shadowBias`
    * `shadowStrength`
* missing fields load as defaults
* omit default fields on save if that matches existing style
* validate wrong/non-finite values consistently with current dynamic light fields
* clamp finite values defensively

Suggested clamps:

```text id="a5r2nf"
shadowPriority: reasonable integer range, e.g. -1000..1000
shadowBias: 0.0..0.1
shadowStrength: 0.0..1.0
```

Do not include these fields in `ComputeSectorLightmapSourceHash()`.

Tests:

* missing shadow fields load defaults
* non-default shadow fields save/reload
* default shadow fields omitted if applicable
* dynamic spotlight shadow setting edits do not affect lightmap source hash
* existing dynamic/static light serialization tests still pass

#### Phase 1B: Add Dynamic Spotlight Shadow Inspector Controls

Goal:

Expose shadow request settings for dynamic spotlights in the inspector.

Inspector fields for dynamic spotlights only:

```text id="bx0tbg"
Cast Shadows
Shadow Priority
Shadow Bias
Shadow Strength
```

Do not add these controls to:

* dynamic point lights
* static point lights
* static spotlights

unless explicitly required later.

Changing these fields should mark the document dirty and follow existing dynamic spotlight edit invalidation behavior.

Manual smoke:

* select dynamic spotlight
* toggle Cast Shadows
* edit priority/bias/strength
* save/reload
* confirm persistence
* confirm no visual behavior change yet

### Phase 2: Shadow Caster Selection And Shadow Map Resources

Goal:

Select which dynamic spotlights receive shadow slots and allocate/update shadow map resources.

Why it helps:

This establishes the runtime budget model before depth rendering and shader sampling.

Files/functions likely touched:

* dynamic light selection/ranking helper files
* `SectorMeshPreview.cpp/.h`
* resource lifecycle code
* tests for shadow caster selection

Exact behavior that must remain unchanged:

* Dynamic light selection remains as before.
* Dynamic point/spot lighting remains visually unchanged until shader shadow sampling lands.
* Dynamic spotlights without shadow slots still illuminate normally.
* Static lights ignored by runtime shadow selection.

Risks/goblins:

* Shadow caster selection fighting dynamic light selection.
* Assigning shadow slots to lights not selected for runtime lighting.
* Resource leaks on rebuild/resize/unload.
* Too much per-frame allocation.

Non-goals:

* No depth rendering yet if Phase 2B can stop at resources/matrices.
* No shader shadow attenuation yet.
* No point-light shadows.
* No static shadows.

Suggested checks:

```bash id="m8ttrn"
git diff --check
git diff --stat
git status --short
```

Run relevant tests.

Final report expectations:

* State selection policy.
* State shadow budget.
* State resource lifecycle behavior.
* State verification commands/results.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 2 `Completed` only after Phase 2A and Phase 2B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 2A: Select Shadow-Casting Dynamic Spotlights Under Budget

Goal:

Pick up to `MAX_DYNAMIC_SHADOW_CASTERS` dynamic spotlights for shadow rendering.

Implementation guidance:

Selection input:

* already-selected runtime dynamic lights
* only dynamic spotlights
* only `castsShadow == true`
* enabled/valid lights only

Selection ordering:

```text id="cdt59y"
1. higher shadowPriority wins
2. higher dynamic light contribution/ranking score wins
3. stable light ID tie-breaker
```

If a future flashlight/gameplay light exists later, it should be able to reserve a slot, but do not implement flashlight behavior now.

Important rules:

* A light can be selected for dynamic lighting but not receive a shadow slot.
* A light cannot receive a shadow slot if it is not selected for dynamic lighting.
* Dynamic point lights never receive shadow slots.
* Static lights never receive shadow slots.
* Fallback visibility should remain safe and use the existing selected dynamic light set.

Tests:

* only selected dynamic spotlights can get shadow slots
* castsShadow=false spotlights do not get slots
* dynamic points do not get slots
* static lights are ignored
* priority beats score
* score breaks priority ties
* ID breaks remaining ties
* cap is enforced at 2

#### Phase 2B: Create Shadow Map Render Targets And Light View Projections

Goal:

Allocate shadow map resources and compute per-shadow-slot light matrices.

Implementation guidance:

Create up to `MAX_DYNAMIC_SHADOW_CASTERS` 2D shadow maps.

Initial resolution:

```text id="n0kqdj"
1024 x 1024
```

Use existing raylib/render texture patterns in the project.

For each selected shadow-casting dynamic spotlight:

```text id="8ba9np"
view = look from light.position toward light.target/direction
projection = perspective using outerConeDegrees
near plane = small sane value
far plane = light range
lightViewProjection = view * projection
```

Use the same direction convention as dynamic spotlights.

Handle degenerate target/direction safely:

* no NaNs
* invalid/degenerate spotlights should not get a shadow slot or should use a safe default direction only if consistent with existing dynamic spotlight behavior

Resource lifecycle:

* create resources during preview renderer resource setup or lazily on demand
* unload resources correctly on preview teardown/rebuild
* avoid per-frame allocation

No shader sampling yet.

Tests:

* selected shadow caster produces finite light-space matrix
* degenerate direction is handled safely
* resource count respects budget where testable

Manual smoke:

* no visual shadowing expected yet
* no crashes/resource leaks during 3D mode enter/exit/rebuild

### Phase 3: Shadow Map Depth Rendering

Goal:

Render sector geometry into selected dynamic spotlight shadow maps.

Why it helps:

This produces the depth maps needed by main scene sampling.

Files/functions likely touched:

* `SectorMeshPreview.cpp`
* embedded depth/shadow caster shader strings
* sector draw record iteration
* material/texture binding for alpha-tested records
* render target lifecycle

Exact behavior that must remain unchanged:

* Main scene visual output unchanged until Phase 4 shader sampling.
* Existing sector draw culling unchanged.
* Dynamic lighting unchanged.
* Bloom source unchanged.
* Static light baking unchanged.

Risks/goblins:

* Alpha-tested middle textures casting rectangular slab shadows.
* Shadow pass accidentally drawing sky cylinder.
* Shadow pass ignoring alpha cutoff.
* Backface/cull state producing missing caster geometry.
* Shadow depth texture not usable by main shader later.

Non-goals:

* No main scene shadow sampling in Phase 3.
* No caster culling optimization unless trivial.
* No point-light shadows.
* No static shadow maps.
* No volumetric shafts.

Suggested checks:

```bash id="ba4dkw"
git diff --check
git diff --stat
git status --short
```

Run relevant build/tests.

Final report expectations:

* State shadow caster shader behavior.
* State geometry included/excluded.
* State alpha-test behavior.
* State verification commands/results.
* State manual smoke status.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 3 `Completed` only after Phase 3A and Phase 3B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 3A: Render Sector Geometry Into Spotlight Shadow Maps

Goal:

Render sector draw records into selected shadow maps using light view/projection.

Implementation guidance:

First pass caster set may be conservative:

```text id="vx66bn"
render all sector draw records into each selected shadow map
```

Later optimization can cull by light frustum/range, but do not require it now.

Include:

* floors
* ceilings
* walls
* lower/upper strips
* middle textures

Exclude:

* sky cylinder
* sky caps
* debug overlays
* UI
* bloom fullscreen passes
* non-sector editor gizmos

Use a depth/shadow caster shader/material separate from the main lighting shader.

No main scene sampling yet.

Manual smoke:

* entering/exiting 3D mode works
* no visible output change expected
* no render-state leakage into main scene

#### Phase 3B: Support Alpha-Tested Middle Textures In Shadow Pass

Goal:

Complete alpha-tested middle texture cutout shadow casting in this plan by replacing Phase 3A's temporary opaque-caster behavior for alpha-tested draw records.

This is required before Phase 3 can be marked complete.

Implementation guidance:

For alpha-tested sector draw records:

* bind the same base/diffuse texture used by the visible draw record
* pass `alphaTest = true`
* pass same/suitable `alphaCutoff`, currently likely 0.5
* discard fragments below cutoff in the shadow caster fragment shader
* write depth only for surviving fragments

For opaque records:

* do not require alpha texture sampling
* render depth normally

The alpha-tested shadow pass must not use decal textures or lightmaps.

Important:

```text id="ylpo40"
middle texture with magenta/cutout/alpha behavior should not cast a solid rectangular shadow slab
```

Tests/manual smoke:

* create/use alpha-tested middle texture bars/grate
* shadow-casting spotlight behind/near it
* resulting shadow respects cutout shape
* opaque walls/floors still cast shadows

If automated GPU tests are impractical, state manual smoke requirement clearly.

### Phase 4: Main Shader Shadow Sampling

Goal:

Use selected shadow maps to attenuate dynamic spotlight direct lighting in the main sector shader.

Why it helps:

This is the actual visual dynamic shadow feature.

Files/functions likely touched:

* `SectorMeshPreview.cpp`
* embedded sector shader strings
* dynamic light uniform packing
* material/shader uniform setup
* shadow map sampler binding
* tests for packing if available

Exact behavior that must remain unchanged:

* Dynamic point lights remain unshadowed and behave as before.
* Dynamic spotlights without shadow slots behave as before.
* Static point/spot baked lighting unchanged.
* AO/ambient/baked direct unchanged.
* Emissive decals and bloom source unchanged.
* Alpha-tested visible rendering unchanged.

Risks/goblins:

* Sampler slot conflicts with existing diffuse/lightmap/decal usage.
* Shadow bias acne/peter-panning.
* PCF too expensive or too soft.
* Shadow projection matrix convention mismatch.
* Applying shadows to baked lighting by mistake.
* Broken behavior when only one/no shadow maps are active.

Non-goals:

* No point-light shadows.
* No static light runtime shadows.
* No volumetric shafts.
* No quality settings beyond constants/defaults.

Suggested checks:

```bash id="otr5r1"
git diff --check
git diff --stat
git status --short
```

Run relevant build/tests.

Final report expectations:

* State sampler/uniform layout.
* State PCF kernel.
* State bias/strength behavior.
* State what lighting term shadows affect.
* State verification commands/results.
* State manual smoke status.

How to update this plan after completion:

* Mark completed pass in JSON and table.
* Mark Phase 4 `Completed` only after Phase 4A and Phase 4B are complete.
* Add date, summary, verification results, and behavior notes.

#### Phase 4A: Pack Shadow Slots And Light-Space Matrices For Dynamic Lights

Goal:

Connect selected shadow maps to selected dynamic lights.

Implementation guidance:

For each selected dynamic light sent to the shader, add/pack a shadow slot:

```text id="r3h2dl"
shadowSlot = -1 for no shadow
shadowSlot = 0..MAX_DYNAMIC_SHADOW_CASTERS-1 for assigned shadow map
```

Only dynamic spotlights can have non-negative shadow slots.

Add uniforms for:

```text id="ayufnt"
dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS]
shadowLightMatrices[MAX_DYNAMIC_SHADOW_CASTERS]
shadowBias[MAX_DYNAMIC_SHADOW_CASTERS]
shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS]
```

Sampler binding:

For simplicity, prefer explicit samplers over sampler arrays at first:

```glsl id="4whypo"
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;
```

Avoid conflicts with existing material map usage:

* base texture
* lightmap
* decal texture

Document which texture units/samplers are used.

No PCF/shadow attenuation yet if this pass is kept strictly packing-only, but shader should compile.

Tests:

* unshadowed lights get slot -1
* selected shadow casters get deterministic slots
* points never get slots
* slot count capped
* uniforms are safe when zero shadow casters active

#### Phase 4B: Apply PCF Shadowing To Dynamic Spotlight Contribution

Goal:

Attenuate dynamic spotlight contribution using shadow map sampling.

Implementation guidance:

In the main sector fragment shader:

* only evaluate shadow when current dynamic light is a spotlight and has `shadowSlot >= 0`
* project fragment world position through that slot’s `shadowLightMatrix`
* compare projected depth to sampled shadow map depth
* apply bias
* use fixed PCF

Recommended first PCF:

```text id="ozddlk"
3x3 PCF
```

or a small fixed tap pattern if easier/cheaper.

Shadow factor:

```text id="gomgp6"
shadowFactor = lerp(1.0, visibilityFromShadowMap, shadowStrength)
```

where `visibilityFromShadowMap` is 0..1 from PCF.

Then:

```text id="pjtmv8"
dynamicSpotContribution *= shadowFactor
```

Do not shadow:

* bakedDirect
* sectorAmbient
* bakedAO
* emissive decals
* bloom source
* dynamic point lights
* dynamic spotlights without a shadow slot

Handle out-of-shadow-map projection coordinates safely:

* fragments outside the spotlight shadow projection should be treated as lit or should already have zero spotlight cone contribution
* no NaNs

Manual smoke:

* one dynamic spotlight with Cast Shadows enabled casts visible shadows
* disabling Cast Shadows restores unshadowed spotlight
* changing bias affects acne/peter-panning
* changing strength controls darkness
* two shadow-casting spotlights can both cast shadows
* third shadow-casting spotlight illuminates unshadowed if budget is 2
* dynamic point lights unaffected
* static baked lighting unaffected
* alpha-tested middle texture casts cutout shadow

### Phase 5: Debug Polish Tests And Completion

Goal:

Make shadow caster state debuggable, tune defaults, and close the plan.

Why it helps:

Shadow maps need visible/debuggable budget behavior and clear authoring feedback.

Files/functions likely touched:

* debug text/status rendering
* dynamic spotlight inspector labels
* docs
* tests
* this plan document

Exact behavior that must remain unchanged:

* Shadow math unchanged except tuning.
* Dynamic light selection unchanged.
* Static light baking unchanged.
* Save/load unchanged except docs/tests if needed.

Risks/goblins:

* Debug text too noisy.
* Bias defaults bad on common maps.
* Per-light settings confusing.
* Budget behavior invisible to level author.

Non-goals:

* No volumetric shafts.
* No flashlight gameplay.
* No point-light shadows.
* No static runtime shadows.
* No quality settings UI unless trivial.

Suggested checks:

```bash id="m9cf93"
git diff --check
git diff --stat
git status --short
```

Run relevant full test suite.

Final report expectations:

* State final defaults.
* State debug text.
* State docs updated.
* State verification results.
* State manual smoke results if performed.
* State known deferred work.

How to update this plan after completion:

* Mark Phase 5A and Phase 5 `Completed`.
* If all phases are complete, ensure parent phases are `Completed`.
* Leave a final completion note.

#### Phase 5A: Add Shadow Debug Readout Tune Defaults And Close Plan

Goal:

Finish dynamic spotlight shadow maps cleanly.

Implementation guidance:

Add debug text near existing dynamic light debug, for example:

```text id="zu74z3"
shadow casters: 2 / 3 candidates / max 2
shadow ids: 5,12
```

Inspector/tooltips/comments should make budget behavior clear:

```text id="k5xhry"
Cast Shadows requests a shadow map. Only the highest-priority selected dynamic spotlights up to the shadow budget actually cast runtime shadows.
```

Tune:

* confirm the established default shadow bias (`0.002`) or intentionally update related tests/docs/compat notes if changing it
* confirm the established default shadow strength (`1.0`) or intentionally update related tests/docs/compat notes if changing it
* confirm the established shadow map resolution constant (`1024`) or intentionally update related tests/docs/compat notes if changing it
* PCF kernel if too expensive/soft/hard

Document:

* dynamic spotlights only
* opt-in Cast Shadows
* max shadow casters default 2
* static lights are baked and do not use runtime shadow maps
* dynamic point shadows are deferred
* alpha-tested middle textures are supported in shadow pass
* volumetric shafts/god rays are deferred

Manual smoke checklist:

* dynamic spotlight with Cast Shadows casts shadow
* two dynamic spotlights cast shadows
* third over-budget spotlight remains unshadowed
* priority controls which lights get slots
* bias/strength controls behave
* alpha-tested middle texture casts cutout shadow
* dynamic point lights unaffected
* static point/spot baked lighting unaffected
* dynamic light flicker still works
* dynamic spot pilot/overlay still works
* portal culling still works

## Deferred Decisions For Later Plans

These are intentionally out of scope:

* Dynamic point-light cubemap shadows.
* Static runtime shadow maps.
* Flashlight gameplay item.
* Flashlight forced shadow slot.
* Shadow quality settings UI.
* Per-light shadow resolution.
* Cascaded shadow maps.
* Variance shadow maps.
* Contact shadows.
* Volumetric spot shafts.
* God rays.
* Projected cookies/gobos.
* Clustered/tiled/deferred lighting.

## Final Completion Criteria

This plan is complete when:

* dynamic spotlights can opt in to Cast Shadows
* at most 2 dynamic spotlight shadow maps are rendered by default
* shadow caster selection is priority/score based and deterministic
* dynamic points never cast shadows
* static point/spot lights do not use runtime shadow maps
* sector geometry casts into shadow maps
* alpha-tested middle textures cast cutout shadows
* main shader applies shadow maps only to dynamic spotlight direct contribution
* dynamic spotlights without shadow slots remain unshadowed but lit
* dynamic point lights still behave as before
* baked lighting, AO, decals, bloom, and lightmap source hash remain correct
* debug output shows shadow budget/caster state
* no point-shadow/volumetric/flashlight scope leaks into this implementation
