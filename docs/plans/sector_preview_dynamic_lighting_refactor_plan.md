# SectorPreview Dynamic Lighting Refactor Plan

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

```plan-state-json id="sector-preview-dynamic-lighting-refactor"
{
  "plan_id": "sector_preview_dynamic_lighting_refactor",
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
      "id": "phase_00",
      "title": "Baseline Audit And Behavior Contract",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_01",
      "title": "Introduce Shared Draw And Upload Context Types",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Move Pure CPU Selection And Packing Buffers",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Move Uniform Upload Helpers",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_04",
      "title": "Move Shadow Map Resource Ownership And Lifetime",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_05",
      "title": "Move Dynamic Spotlight Shadow Map Render Orchestration",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_06",
      "title": "Integrate Facade Cleanup",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_07",
      "title": "Verification Docs And Backlog Update",
      "type": "phase",
      "status": "Completed"
    }
  ]
}
```

## Current Progress

| Phase / Pass | Status | Date | Notes |
| --- | --- | --- | --- |
| Phase 0: Baseline Audit And Behavior Contract | Completed | 2026-07-04 | Documentation-only audit completed; behavior contract now names current symbols, ownership, upload paths, render order, and lifetime boundaries. |
| Phase 1: Introduce Shared Draw And Upload Context Types | Completed | 2026-07-04 | Added shared dynamic-light shader-location, shadow-map texture, and billboard upload context structs; ownership remains in `SectorMeshPreview`. |
| Phase 2: Move Pure CPU Selection And Packing Buffers | Completed | 2026-07-04 | Added helper-owned CPU dynamic-light state for sources, candidates, selected lights/IDs, receiver bounds, shadow casters, shadow matrices, and shadow uniform packing. |
| Phase 3: Move Uniform Upload Helpers | Completed | 2026-07-04 | Moved sector, door, and billboard dynamic light/shadow uniform upload helpers into `SectorPreviewDynamicLighting`; shader locations, uniform names, texture bindings, and render order are unchanged. |
| Phase 4: Move Shadow Map Resource Ownership And Lifetime | Completed | 2026-07-04 | Moved dynamic spotlight shadow map render textures, depth-only allocation, filter/wrap setup, partial-failure cleanup, and custom unload into `SectorPreviewDynamicLighting`; shadow material and render orchestration remain in `SectorMeshPreview`. |
| Phase 5: Move Dynamic Spotlight Shadow Map Render Orchestration | Completed | 2026-07-04 | Moved shadow-map begin/clear/draw/end loops into `SectorPreviewDynamicLighting`; `SectorMeshPreview` still owns the public facade, shadow material, and runtime door mesh preparation. |
| Phase 6: Integrate Facade Cleanup | Completed | 2026-07-04 | Moved dynamic spotlight shadow material/shader ownership and readiness checks into `SectorPreviewDynamicLighting`; `SectorMeshPreview` keeps the public facade and high-level sequencing. |
| Phase 7: Verification Docs And Backlog Update | Completed | 2026-07-04 | Final automated verification passed and REF-014 backlog state was marked complete; manual visual smoke was not performed. |

## Execution Tracking Rules

* Each phase must leave the project buildable and runnable.
* Each phase final report must state whether source code changed.
* Each implementation phase must update this document before finishing.
* The update should be small and local: update the JSON status, the Current Progress row, and add or update an execution-log note for the selected phase.
* Do not rewrite unrelated phases when marking progress.
* Do not mark REF-014 complete in `docs/plans/codebase_refactor_backlog.md` until all implementation phases finish and Phase 7 closes the plan.
* If behavior is intended to remain unchanged, explicitly state that.
* If a phase changes rendering, shader uniform upload, resource lifetime, light selection, debug text, render order, source-hash behavior, topology cache behavior, collision, ECS ownership, or build/test behavior, clearly say so.
* Do not claim manual GUI or render smoke verification unless it was actually performed.
* If a phase is too broad, split it into smaller child passes and stop without source changes.
* Keep hard non-goals intact unless a future task explicitly replaces this plan.

## Goal And Desired End State

Extract dynamic point/spot lighting and dynamic spotlight shadow internals behind a focused preview-renderer helper while preserving behavior exactly.

Desired destination module:

* `sources/sector_demo/SectorPreviewDynamicLighting.h`
* `sources/sector_demo/SectorPreviewDynamicLighting.cpp`

`SectorMeshPreview` remains the public facade and owns high-level render sequencing. The helper may own dynamic point light source/candidate/selected buffers, selected dynamic light IDs, dynamic lighting debug/status text, dynamic spotlight shadow caster/matrix/packed uniform buffers, dynamic spotlight shadow map render textures, dynamic spotlight shadow material/shader if moved by this plan, uniform packing/upload helpers, and shadow map load/unload helpers.

The helper must not own the `SectorMeshPreview` public API, static sector meshes, door mesh cache, billboard renderer, sky renderer, bloom renderer, `AssetManager` scopes, runtime ECS lifecycle, topology map/document state, baked lightmap state, collision, or editor schema.

Shadow map resources move late in Phase 4, after context types, CPU buffers, and uniform upload helper boundaries are established. This keeps the highest-risk GPU lifetime changes isolated from earlier behavior-preserving extraction work.

## Behavior Contract

Phase 0 must verify and update this section before implementation starts.

Current dynamic lighting and shadow behavior to preserve:

* `SectorMeshPreview` is the public facade for rebuild, renderer-resource rebuild/shutdown, visibility/debug update, dynamic spotlight shadow pass, scene draw, emissive bloom convenience entry points, and render convenience entry points through `Rebuild()`, `RebuildRendererResources()`, `ShutdownRendererResources()`, `UpdateVisibilityDebug()`, `RenderDynamicSpotLightShadowMaps()`, `DrawScene()`, `Render()`, `ApplyEmissiveDecalBloom()`, and `ApplyEmissiveDecalBloomToScene()`.
* `SectorDynamicPointLightSelection.*` already owns core CPU algorithms for building point/spot uniforms, flicker effective upload intensity, candidate collection, ranked dynamic light selection, ranked dynamic spotlight shadow caster selection, shadow matrix building, and packed shadow uniform data.
* `SectorMeshPreview` currently owns live dynamic-light CPU buffers named `dynamicPointLightSources`, `dynamicPointLightCandidates`, `dynamicPointLights`, `selectedDynamicPointLightIds`, `directDynamicLightReceiverBounds`, `dynamicSpotLightShadowCasters`, and `dynamicSpotLightShadowMatrices`; public debug accessors currently expose `SelectedDynamicLights()`, `SelectedDynamicLightIds()`, `DynamicLightCandidateCount()`, and `DynamicLightSourceCount()` from those buffers.
* `SectorMeshPreview` currently owns dynamic spotlight shadow GPU/resource state named `dynamicSpotLightShadowMaps`, `dynamicSpotLightShadowMaterial`, `dynamicSpotLightShadowDefaultTexture`, `dynamicSpotLightShadowMaterialLoaded`, `dynamicSpotLightShadowLightViewProjectionLoc`, `dynamicSpotLightShadowAlphaTestLoc`, and `dynamicSpotLightShadowAlphaCutoffLoc`.
* `SectorMeshPreview` also owns sector shader dynamic-light locations (`dynamicLightCountLoc`, `dynamicLightPositionsLoc`, `dynamicLightColorsLoc`, `dynamicLightRadiiLoc`, `dynamicLightIntensitiesLoc`, `dynamicLightTypesLoc`, `dynamicLightDirectionsLoc`, `dynamicLightInnerConeCosLoc`, `dynamicLightOuterConeCosLoc`, `dynamicLightShadowSlotsLoc`, `shadowLightMatrixLocs`, `shadowBiasLoc`, `shadowStrengthLoc`, `shadowSoftnessLoc`, `dynamicLightingClampLoc`) and equivalent door shader locations prefixed with `doorOpaque`.
* `SectorPreviewBillboardRenderer` owns its own cutout shader locations for `dynamicLightCount`, `dynamicLightPositions`, `dynamicLightColors`, `dynamicLightRadii`, `dynamicLightIntensities`, `dynamicLightTypes`, `dynamicLightDirections`, `dynamicLightInnerConeCos`, `dynamicLightOuterConeCos`, `dynamicLightShadowSlots`, `shadowLightMatrices`, `shadowBias`, `shadowStrength`, `shadowSoftness`, `shadowMap0`, `shadowMap1`, and `dynamicLightingClamp`.
* `UpdateVisibilityDebug()` computes portal visibility with `ComputeRuntimeSectorVisibilityFromView()` when `visibilityGraphValid`, formats portal visibility text, counts visible sector draw records, calls `BuildDirectDynamicLightReceiverBounds()`, collects candidates with `CollectSectorPreviewDynamicPointLightCandidates()`, selects lights with `SelectRankedSectorPreviewDynamicPointLights()`, selects shadow casters with `SelectRankedSectorPreviewDynamicSpotLightShadowCasters()`, builds matrices with `BuildSectorPreviewDynamicSpotLightShadowMatrices()`, and composes `renderDebugText` / `visibilityDebugText` with dynamic light, shadow caster, and billboard debug text.
* Direct receiver bounds are rebuilt by `BuildDirectDynamicLightReceiverBounds()` from `meshes.sectorReceiverBounds` and append runtime door receiver bounds via `CollectSectorDoorReceiverBounds()` only when a runtime world is supplied.
* `RefreshDynamicLightSources()` and rebuild setup both call `BuildSectorPreviewDynamicPointLightSources()` using `visibilityLookupWorld` when valid, reserve candidate/selected/shadow buffers, and then refresh selection/debug state.
* Dynamic light selection must preserve ranking, hysteresis, previous selected ID behavior, max dynamic light count, and selected-ID debug output. The current call intentionally passes `&selectedDynamicPointLightIds` as both previous-ID input and selected-ID output.
* Dynamic light upload must preserve the shared `UploadSectorPreviewDynamicPointLights()` behavior in `SectorPreviewDynamicLighting.cpp`, including disabled-light zero count, `DynamicLightingClamp`, `DynamicLightEffectiveUploadIntensity()`, flicker, and `runtimeSeconds`.
* Dynamic spotlight shadow selection must preserve the max shadow caster count from `MaxDynamicSpotLightShadowCasters`, selected-light-only eligibility, priority/score/stable-ID ordering, slot assignment, bias, strength, and softness packing through `PackSectorPreviewDynamicSpotLightShadowUniforms()`.
* Shadow maps are currently depth-only render textures using `DynamicSpotLightShadowMapResolution` and `LoadDepthOnlyRenderTexture()`. `EnsureDynamicSpotLightShadowMapResources()` creates missing maps, requires both framebuffer and depth texture IDs, calls `SetTextureFilter(..., TEXTURE_FILTER_POINT)` and `SetTextureWrap(..., TEXTURE_WRAP_CLAMP)`, and unloads all shadow maps on partial failure through `UnloadDynamicSpotLightShadowMapResources()`.
* Shadow map unload currently uses the custom `UnloadDepthOnlyRenderTexture()` helper, which unloads the framebuffer ID and resets the whole `RenderTexture2D`; this custom teardown must be preserved unless Phase 4 proves a mechanical helper move requires local naming changes only.
* Shadow map rendering remains in `RenderDynamicSpotLightShadowMaps()`: it returns early without a loaded shadow material, valid `lightViewProjection` location, or shadow matrices; it prepares runtime door meshes only when a runtime world is supplied; it renders each valid matrix to the matrix's assigned shadow slot with `BeginTextureMode()`, `ClearBackground(WHITE)`, depth test enabled, static sector `meshes.sectorDrawRecords`, per-batch alpha-test texture/cutoff state, then runtime door shadow casters with alpha test disabled and `BuildSectorDoorShadowCasterModelMatrix()`.
* Main scene order remains `Render()` shadow pass first, then `DrawScene()` with `BeginMode3D(camera)`, sky via `skyRenderer.Draw()`, static sectors, runtime doors through `DrawRuntimeDoors()`, runtime billboards through `billboardRenderer.Draw()`, then `EndMode3D()`. Caller-owned overlays and bloom remain outside the dynamic-light helper path as currently sequenced.
* Sector and door shaders bind dynamic spotlight shadow maps through `MATERIAL_MAP_ROUGHNESS` / `shadowMap0` and `MATERIAL_MAP_OCCLUSION` / `shadowMap1`; billboard upload uses explicit `SetShaderValueTexture()` for `shadowMap0` and `shadowMap1`.
* Sector, door, and billboard dynamic light uploads use shared `UploadSectorPreviewDynamicPointLights()` and `UploadSectorPreviewDynamicSpotLightShadowUniforms()` helpers in `SectorPreviewDynamicLighting.cpp`; billboard upload is fed by `SectorPreviewBillboardDynamicLightContext` built in `SectorMeshPreview::BuildBillboardDynamicLightContext()`.
* Sector, door, and billboard shader uniform names, location arrays, sampler assumptions, and upload layout must remain unchanged, including array-base lookup fallback through `GetShaderLocationArrayBase()` and per-matrix element lookup through `GetShaderLocationArrayElement()`.
* Existing debug render modes must continue to isolate baked-only, dynamic-only, normal, albedo-only, normal-visualize, and flat-color-no-texture terms in the same way.
* Missing, failed, or not-ready textures and shadow maps must not crash rendering.
* Phase 0 audit touched no lightmap source-hash code, topology/editor cache code, collision behavior, ECS lifecycle ownership, shader formulas, render order, or runtime behavior.

Hard non-goals for every phase:

* No shader formula changes.
* No shader uniform name changes.
* No max light or shadow slot count changes.
* No dynamic light ranking behavior changes.
* No flicker or effective intensity behavior changes.
* No shadow bias, softness, or strength behavior changes.
* No shadow map resolution, filter, or wrap behavior changes.
* No render order changes.
* No point-light shadow implementation.
* No door renderer extraction.
* No billboard renderer extraction.
* No sky renderer extraction.
* No bloom renderer extraction.
* No static scene renderer extraction.
* No baked lightmap, object probe, or source hash changes.
* No ECS lifecycle ownership changes.
* No topology, editor schema, or collision behavior changes.
* No full `SectorMeshPreview` facade rewrite.

## Manual Visual Smoke Checklist

Use this checklist for implementation phases that affect visual rendering. Report each item as performed, not performed, or not applicable.

* Static sector dynamic point lights still work.
* Static sector dynamic spotlights still work.
* Dynamic spotlight shadows still work on static sector geometry.
* Alpha-tested sector geometry still casts spotlight shadows.
* Doors still receive dynamic spotlight shadows.
* Doors still cast dynamic spotlight shadows.
* Billboards still receive dynamic point/spot lighting and spotlight shadows.
* Debug overlay dynamic light counts, selected IDs, and shadow caster status still make sense.
* BakedOnly, DynamicOnly, Normal, AlbedoOnly, NormalVisualize, and FlatColorNoTexture debug modes still isolate terms correctly where applicable.
* No render state leaks into sky, bloom, doors, billboards, overlays, or UI.
* Bias/softness behavior is unchanged.

## Execution Log

### Phase 0: Baseline Audit And Behavior Contract

Status: Completed
Date: 2026-07-04

Summary:

* Audited `SectorMeshPreview`, `SectorDynamicPointLightSelection`, and `SectorPreviewBillboardRenderer` for current dynamic light CPU ownership, shader upload paths, shadow map lifetime, shadow pass order, scene draw order, and debug text composition.
* Updated the Behavior Contract with exact current symbol names for selected-light buffers, receiver bounds, shadow caster/matrix buffers, shadow map resources, shader/material locations, public facade methods, and sector/door/billboard upload paths.
* Confirmed this phase is documentation-only and does not require splitting into smaller child passes.

Verification results:

* `python3 tools/plan_executor.py docs/plans/sector_preview_dynamic_lighting_refactor_plan.md --status`: passed; next selected item is `phase_01` with status `Planned`.
* `git diff --check`: passed with no output.
* `git diff --stat`: passed with no tracked diff output because this plan file is currently untracked in the worktree.
* `git status --short`: passed; reported untracked `.venv/` and untracked `docs/plans/sector_preview_dynamic_lighting_refactor_plan.md`.

Behavior notes:

* Source code changed: no.
* Runtime behavior changed: no.
* Rendering, shader uniform upload, resource lifetime, light selection, debug text, render order, build/test behavior changed: no.
* Lightmap source-hash behavior changed: no.
* Topology cache behavior changed: no.
* Collision, sector lookup, physics changed: no.
* ECS ownership changed: no.
* Manual render smoke performed: no; not required for this documentation-only audit phase.

### Phase 1: Introduce Shared Draw And Upload Context Types

Status: Completed
Date: 2026-07-04

Summary:

* Added `sources/sector_demo/SectorPreviewDynamicLighting.h` and `.cpp` as the shared dynamic-lighting module boundary for later phases.
* Added `SectorPreviewDynamicLightShaderLocations`, `SectorPreviewDynamicSpotLightShadowShaderLocations`, `SectorPreviewDynamicShadowMapTextures`, and moved `SectorPreviewBillboardDynamicLightContext` into the shared header.
* Threaded the grouped billboard shadow-map texture context through existing local billboard call sites as a mechanical no-op.
* Kept dynamic-light CPU buffers, selected IDs, shadow matrices, shadow map resources, shadow materials, shader locations, and all ownership in `SectorMeshPreview`.

Verification results:

* `cmake --build cmake-build-debug -j2`: passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`: passed, 9/9 tests.
* `ctest --test-dir cmake-build-debug --output-on-failure`: initial concurrent run failed while another CTest command was using the same fixed `/tmp/sector_*` test paths; rerun by itself passed, 15/15 tests.
* `git diff --check`: passed with no output.
* `git diff --stat`: passed; tracked diff reported this plan plus `SectorMeshPreview.*` and `SectorPreviewBillboardRenderer.*`, with new untracked `SectorPreviewDynamicLighting.*` files reported by status.
* `git status --short`: passed; reported modified plan/source files and untracked `sources/sector_demo/SectorPreviewDynamicLighting.cpp` / `.h`.

Behavior notes:

* Source code changed: yes.
* Runtime behavior changed: no intended behavior change.
* Rendering, shader uniform upload values, shader formulas/names, max counts, dynamic light ranking, flicker, bias/softness/strength, shadow map resources/lifetime, render order, debug text, build/test behavior changed: no.
* Lightmap source-hash behavior changed: no.
* Topology cache behavior changed: no.
* Collision, sector lookup, physics changed: no.
* ECS ownership changed: no.
* Manual render smoke performed: no; not applicable because this pass only introduced shared context types and mechanical context field threading.

### Phase 2: Move Pure CPU Selection And Packing Buffers

Status: Completed
Date: 2026-07-04

Summary:

* Added `SectorPreviewDynamicLighting` helper-owned CPU state for dynamic light sources, candidates, selected uniforms, selected IDs, direct receiver bounds, spotlight shadow casters, and spotlight shadow matrices.
* Moved source rebuild, selection buffer reserve/clear, receiver-bound collection, ranked light/shadow-caster selection, shadow matrix building, and packed shadow uniform access behind the helper.
* Preserved `SectorMeshPreview` as the public facade for debug accessors, visibility debug update, shader uploads, shadow map resources, shadow material lifetime, and shadow pass rendering.

Verification results:

* `cmake --build cmake-build-debug -j2`: passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`: passed, 9/9 tests.
* `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 15/15 tests.
* `git diff --check`: passed with no output.
* `git diff --stat`: passed; tracked diff reported this plan plus `SectorMeshPreview.*` and `SectorPreviewDynamicLighting.*`.
* `git status --short`: passed; reported modified plan/source files only.

Behavior notes:

* Source code changed: yes.
* Runtime behavior changed: no intended behavior change.
* Rendering, shader uniform upload values, shader formulas/names, max counts, dynamic light ranking, hysteresis, previous selected ID handling, flicker, bias/softness/strength, shadow map resources/lifetime, render order, build/test behavior changed: no.
* `UpdateVisibilityDebug()` still composes the same visibility/render debug text through `SectorMeshPreview`, using helper accessors for selected lights, counts, IDs, and shadow casters.
* Public selected dynamic light accessors are preserved and now forward to the helper-owned CPU state.
* Shadow maps and dynamic spotlight shadow pass rendering remain controlled by `SectorMeshPreview`.
* Direct receiver bounds still start from sector receiver bounds and append runtime door receiver bounds only when a runtime world is supplied.
* Lightmap source-hash behavior changed: no.
* Topology cache behavior changed: no.
* Collision, sector lookup, physics changed: no.
* ECS ownership changed: no.
* Manual render smoke performed: no; not performed in this pass.

### Phase 3: Move Uniform Upload Helpers

Status: Completed
Date: 2026-07-04

Summary:

* Moved the sector/door dynamic point-light upload helper and dynamic spotlight shadow uniform upload helper into `SectorPreviewDynamicLighting`.
* Moved the billboard dynamic point-light and spotlight shadow uniform upload path to the same shared helpers using the Phase 1 location/context structs.
* Kept sector and door shadow-map receiving through material slots, and kept billboard shadow-map receiving through explicit texture uniforms.

Verification results:

* `cmake --build cmake-build-debug -j2`: passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`: passed, 9/9 tests.
* `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 15/15 tests.
* `git diff --check`: passed with no output.
* `git diff --stat`: passed; tracked diff reported this plan plus `SectorMeshPreview.cpp`, `SectorPreviewBillboardRenderer.cpp`, and `SectorPreviewDynamicLighting.*`.
* `git status --short`: passed; reported modified plan/source files only.

Behavior notes:

* Source code changed: yes.
* Runtime behavior changed: no intended behavior change.
* Rendering, shader uniform upload values, shader formulas/names, max counts, dynamic light ranking, flicker, bias/softness/strength, shadow map resources/lifetime, render order, debug text, build/test behavior changed: no.
* Disabled dynamic lighting still uploads an effective zero dynamic-light count through the shared upload helper.
* Sector and door texture binding remain material-slot based; billboard shadow-map texture binding remains explicit sampler upload based.
* Lightmap source-hash behavior changed: no.
* Topology cache behavior changed: no.
* Collision, sector lookup, physics changed: no.
* ECS ownership changed: no.
* Manual render smoke performed: no; not performed in this pass.

### Phase 4: Move Shadow Map Resource Ownership And Lifetime

Status: Completed
Date: 2026-07-04

Summary:

* Moved helper-owned dynamic spotlight shadow map render textures into `SectorPreviewDynamicLighting`.
* Moved depth-only render texture load/unload helpers, allocation at `DynamicSpotLightShadowMapResolution`, point filtering, clamp wrapping, and partial-failure cleanup into the helper.
* Added narrow helper accessors for render targets used by the still-facade-owned shadow pass and depth textures used by sector, door, and billboard receiving paths.
* Kept the dynamic spotlight shadow material in `SectorMeshPreview` because it is still used by the phase_05 shadow render orchestration loop.

Verification results:

* `cmake --build cmake-build-debug -j2`: passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`: passed, 9/9 tests.
* `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 15/15 tests.
* `git diff --check`: passed with no output.
* `git diff --stat`: passed; tracked diff reported this plan plus `SectorMeshPreview.*` and `SectorPreviewDynamicLighting.*`.
* `git status --short`: passed; reported modified plan/source files only.

Behavior notes:

* Source code changed: yes.
* Runtime behavior changed: no intended behavior change.
* Shadow map resource ownership/lifetime changed internally; resolution, depth-only setup, depth format, point filtering, clamp wrapping, partial-failure cleanup, custom framebuffer unload, shutdown ordering, render order, shader behavior, uniform upload behavior, dynamic light selection, debug text, and build/test behavior changed: no.
* `SectorMeshPreview::ShutdownRendererResources()` remains the high-level shutdown caller and now checks helper-owned shadow map resources before the early return.
* The dynamic spotlight shadow material stayed in `SectorMeshPreview`; material lifetime and shader locations did not move in this phase.
* Lightmap source-hash behavior changed: no.
* Topology cache behavior changed: no.
* Collision, sector lookup, physics changed: no.
* ECS ownership changed: no.
* Manual render smoke performed: no; not performed in this pass.

### Phase 5: Move Dynamic Spotlight Shadow Map Render Orchestration

Status: Completed
Date: 2026-07-04

Summary:

* Added `SectorPreviewDynamicSpotLightShadowRenderContext` for the existing shadow material, sector draw records, door shadow caster list, shader locations, and narrow texture/door mesh resolver callbacks.
* Moved the dynamic spotlight shadow-map begin/clear/draw/end loop into `SectorPreviewDynamicLighting::RenderShadowMaps()`.
* Kept `SectorMeshPreview::RenderDynamicSpotLightShadowMaps()` as the public facade and kept `PrepareRuntimeDoorMeshes()` owned and sequenced by `SectorMeshPreview` before delegation.
* Kept door mesh cache ownership in `SectorMeshPreview`; door renderer extraction remains out of scope.

Verification results:

* `cmake --build cmake-build-debug -j2`: passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`: passed, 9/9 tests.
* `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 15/15 tests.
* `git diff --check`: passed with no output.
* `git diff --stat`: passed; tracked diff reported this plan plus `SectorMeshPreview.*` and `SectorPreviewDynamicLighting.*`.
* `git status --short`: passed; reported modified plan/source files only.
* `python3 tools/plan_executor.py docs/plans/sector_preview_dynamic_lighting_refactor_plan.md --status`: passed; next selected item is `phase_06` with status `Planned`.

Behavior notes:

* Source code changed: yes.
* Runtime behavior changed: no intended behavior change.
* Shadow pass render orchestration moved internally; shadow material ownership, shader locations, shadow map resources, sector mesh ownership, door mesh cache ownership, dynamic light selection, debug text, and main scene render order changed: no.
* Alpha-test handling, alpha cutoff upload, static sector caster drawing, runtime door caster drawing, door model matrix behavior, `BeginTextureMode()`, `ClearBackground(WHITE)`, depth-test enable/disable, and `EndTextureMode()` behavior changed: no.
* Door renderer extraction remains out of scope.
* Lightmap source-hash behavior changed: no.
* Topology cache behavior changed: no.
* Collision, sector lookup, physics changed: no.
* ECS ownership changed: no.
* Manual render smoke performed: no; not performed in this pass.

### Phase 6: Integrate Facade Cleanup

Status: Completed
Date: 2026-07-04

Summary:

* Moved dynamic spotlight shadow material/shader ownership, shader-location lookup, readiness checks, and unload logic from `SectorMeshPreview` into `SectorPreviewDynamicLighting`.
* Removed the moved dynamic spotlight shadow material members and private load helper from `SectorMeshPreview`.
* Kept `SectorMeshPreview::RenderDynamicSpotLightShadowMaps()` as the public facade; it still prepares runtime door meshes before delegating the shadow render pass.

Verification results:

* `cmake --build cmake-build-debug -j2`: passed.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`: passed, 9/9 tests.
* `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 15/15 tests.
* `git diff --check`: passed with no output.
* `git diff --stat`: passed; tracked diff reported this plan plus `SectorMeshPreview.*` and `SectorPreviewDynamicLighting.*`.
* `git status --short`: passed; reported modified plan/source files only.
* `python3 tools/plan_executor.py docs/plans/sector_preview_dynamic_lighting_refactor_plan.md --status`: passed; next selected item is `phase_07` with status `Planned`.

Behavior notes:

* Source code changed: yes.
* Runtime behavior changed: no intended behavior change.
* Final ownership split: `SectorPreviewDynamicLighting` owns dynamic light CPU state, dynamic spotlight shadow map resources, dynamic spotlight shadow material/shader state, shadow uniform packing, uploads, and shadow render internals; `SectorMeshPreview` owns the public facade, static sector meshes/material, door mesh cache, billboard/sky/bloom renderers, runtime sequencing, and debug text composition.
* `SectorMeshPreview` public API and debug accessors remain stable.
* Shader formulas/names, max counts, dynamic light ranking, hysteresis, flicker, bias/softness/strength, shadow map resolution/filter/wrap/setup, alpha-test behavior, door caster model matrices, and render order changed: no.
* Lightmap source-hash behavior changed: no.
* Topology cache behavior changed: no.
* Collision, sector lookup, physics changed: no.
* ECS ownership changed: no.
* Manual render smoke performed: no; not performed in this pass.

### Phase 7: Verification Docs And Backlog Update

Status: Completed
Date: 2026-07-04

Summary:

* Completed final closeout verification after Phases 1 through 6 were recorded as completed.
* Updated REF-014 in `docs/plans/codebase_refactor_backlog.md` using the existing backlog completion style.
* This phase was documentation/backlog-only and did not change source code, shaders, CMake, tests, or runtime behavior.

Verification results:

* `python3 tools/plan_executor.py docs/plans/sector_preview_dynamic_lighting_refactor_plan.md --status`: passed before closeout; selected item was `phase_07` with status `Planned`.
* `cmake --build cmake-build-debug -j2`: passed; ninja reported no work to do.
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`: passed, 9/9 tests.
* `ctest --test-dir cmake-build-debug --output-on-failure`: passed, 15/15 tests.
* `git diff --check`: passed with no output before closeout edits.
* `git diff --stat`: passed with no tracked diff output before closeout edits.
* `git status --short`: passed with no output before closeout edits.
* `python3 tools/plan_executor.py docs/plans/sector_preview_dynamic_lighting_refactor_plan.md --status`: passed after closeout edits; plan reported no unfinished items.
* `git diff --check`: passed with no output after closeout edits.
* `git diff --stat`: passed after closeout edits; reported only this plan and `docs/plans/codebase_refactor_backlog.md`.
* `git status --short`: passed after closeout edits; reported only this plan and `docs/plans/codebase_refactor_backlog.md` as modified.

Behavior notes:

* Source code changed in this phase: no.
* Source code changed in previous implementation phases: yes; Phases 1 through 6 extracted dynamic lighting and dynamic spotlight shadow internals behind `SectorPreviewDynamicLighting`.
* Runtime behavior changed in this phase: no.
* Rendering, shader uniform upload, resource lifetime, light selection, debug text, render order, and build/test behavior changed in this phase: no.
* Lightmap source-hash behavior changed: no.
* Topology cache behavior changed: no.
* Collision, sector lookup, physics changed: no.
* ECS ownership changed: no.
* Manual render smoke performed: no; not performed during closeout, so visual-rendering smoke remains a manual follow-up risk.
* REF-014 backlog status: completed.
* REF-015, REF-018, and later backlog items were not completed by this phase.

## Phase 0: Baseline Audit And Behavior Contract

Goal:

Confirm current dynamic lighting/shadow functions, members, shader locations, render order, resource lifetime, and caller paths before any implementation changes.

Files likely touched:

* `docs/plans/sector_preview_dynamic_lighting_refactor_plan.md`

Allowed changes:

* Documentation updates to this plan only.
* Refine the Behavior Contract with exact current symbol names, ownership, render order, and lifetime notes discovered during the audit.
* If the phase is too broad, split later phases into smaller child passes and stop.

Explicit non-goals:

* No source code changes.
* No shader edits.
* No CMake edits.
* No test edits.
* No runtime behavior changes.
* No backlog completion update.

Behavior preservation checklist:

* Confirm `SectorMeshPreview` remains the public facade.
* Confirm current dynamic light source, candidate, selected, selected ID, receiver bounds, shadow caster, shadow matrix, shadow map, and shadow material members.
* Confirm sector, door, and billboard upload paths and texture binding differences.
* Confirm `UpdateVisibilityDebug()` still owns selection/debug text composition at the start of implementation.
* Confirm render pass order and shadow pass order.
* Confirm no lightmap source-hash, topology cache, collision, or ECS lifecycle behavior is touched.

Verification commands:

* `python3 tools/plan_executor.py docs/plans/sector_preview_dynamic_lighting_refactor_plan.md --status`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Not required for this documentation-only audit phase.

Final report expectations:

* State that this was documentation-only.
* List the behavior contract updates made.
* State that no source code, shaders, tests, CMake, or runtime behavior changed.
* State that no manual render smoke was performed unless it actually was.
* Report the verification command results.

Backlog/plan-state update instructions:

* Mark `phase_00` `Completed` only after updating the Behavior Contract.
* Mark `phase_01` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-014 complete.

## Phase 1: Introduce Shared Draw And Upload Context Types

Goal:

Add small data/context structs for dynamic light draw/upload inputs and shadow uniforms so later phases can pass explicit data between `SectorMeshPreview`, the dynamic lighting helper, doors, and billboards.

Files likely touched:

* `sources/sector_demo/SectorPreviewDynamicLighting.h`
* `sources/sector_demo/SectorPreviewDynamicLighting.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`
* CMake source list only if the project requires new `.cpp` files to be listed; do not otherwise edit CMake.

Allowed changes:

* Add `SectorPreviewDynamicLighting` declarations and minimal context structs.
* Add explicit shader-location structs or upload-context structs that mirror existing sector, door, and billboard locations without changing names, counts, or upload policy.
* Thread the new types through local call sites only where it is a mechanical no-op.
* Keep all existing ownership in `SectorMeshPreview`.

Explicit non-goals:

* No ownership move.
* No shader formula or uniform name changes.
* No dynamic light ranking or flicker behavior changes.
* No shadow map resource changes.
* No render order changes.
* No door, billboard, sky, bloom, or static scene extraction.

Behavior preservation checklist:

* Sector/door/billboard upload paths still use the same shader locations and texture binding assumptions.
* `SectorMeshPreview::SelectedDynamicLights()` and `SelectedDynamicLightIds()` keep working.
* `UpdateVisibilityDebug()` output remains unchanged.
* Shadow map resources and shadow pass rendering remain owned and called exactly as before.
* Lightmap source-hash behavior is unchanged.
* Topology/editor/cache/collision/ECS behavior is unchanged.

Verification commands:

* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Use the Manual Visual Smoke Checklist if any upload call shape or visual rendering path changes beyond type introduction.

Final report expectations:

* State which context types were added.
* State that ownership remains in `SectorMeshPreview`.
* State that shader names/formulas, max counts, render order, source-hash behavior, topology cache behavior, collision, and ECS lifecycle did not change.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_01` `Completed` only after checks pass or failures are documented.
* Mark `phase_02` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-014 complete.

## Phase 2: Move Pure CPU Selection And Packing Buffers

Goal:

Move or wrap dynamic source/candidate/selected light buffers, selected light IDs, direct receiver bounds, selected shadow casters, shadow matrices, and packed shadow uniform data behind `SectorPreviewDynamicLighting`.

Files likely touched:

* `sources/sector_demo/SectorPreviewDynamicLighting.h`
* `sources/sector_demo/SectorPreviewDynamicLighting.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`
* `sources/sector_demo/SectorDynamicPointLightSelection.h`
* `sources/sector_demo/SectorDynamicPointLightSelection.cpp`

Allowed changes:

* Add a helper-owned state object for CPU dynamic lighting buffers.
* Move reserve/clear/update routines for dynamic point/spot sources, candidates, selected lights, selected IDs, shadow casters, and shadow matrices when the move is mechanical.
* Keep receiver-bound collection behavior identical, including runtime door receiver bounds.
* Preserve `UpdateVisibilityDebug()` behavior and debug text by delegating to helper methods or returning equivalent text/data.

Explicit non-goals:

* No shadow map resource move.
* No shadow rendering orchestration move.
* No shader upload helper move unless a tiny accessor is required.
* No ranking, hysteresis, flicker, max count, shadow slot, bias, softness, or strength behavior changes.
* No door renderer extraction.

Behavior preservation checklist:

* Dynamic source count, candidate count, selected count, selected IDs, and shadow caster debug text remain equivalent.
* Previous selected IDs still feed ranked selection as before.
* Direct receiver bounds still include sector receiver bounds and runtime door receiver bounds in the same cases.
* Shadow matrix and packed uniform values remain equivalent for the same selected lights.
* Public debug accessors still return valid data.
* Lightmap source-hash behavior is unchanged.
* Topology/editor/cache/collision/ECS behavior is unchanged.

Verification commands:

* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Use the Manual Visual Smoke Checklist.

Final report expectations:

* State which CPU buffers moved or were wrapped.
* State how `UpdateVisibilityDebug()` and selected dynamic light accessors are preserved.
* State that shadow maps and shadow pass rendering remain controlled by `SectorMeshPreview`.
* State that lightmap source-hash, topology cache, collision, and ECS lifecycle behavior did not change.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_02` `Completed` only after checks pass or failures are documented.
* Mark `phase_03` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-014 complete.

## Phase 3: Move Uniform Upload Helpers

Goal:

Move `UploadDynamicPointLights`, `UploadDynamicSpotLightShadowUniforms`, or equivalent upload helpers into `SectorPreviewDynamicLighting` while preserving sector, door, and billboard shader upload behavior exactly.

Files likely touched:

* `sources/sector_demo/SectorPreviewDynamicLighting.h`
* `sources/sector_demo/SectorPreviewDynamicLighting.cpp`
* `sources/sector_demo/SectorMeshPreview.cpp`
* `sources/sector_demo/SectorPreviewBillboardRenderer.h`
* `sources/sector_demo/SectorPreviewBillboardRenderer.cpp`

Allowed changes:

* Move upload helper implementations into the dynamic lighting helper.
* Use explicit location/context structs created in Phase 1.
* Support sector material, door material, and billboard shader upload paths without changing shader code or uniform names.
* Keep texture binding mode unchanged: sector and door through material slots, billboard through explicit texture uniforms.

Explicit non-goals:

* No shader code edits.
* No uniform name or location lookup changes except moving the storage/access wrappers created earlier.
* No dynamic light effective intensity changes.
* No shadow map resource ownership changes.
* No render order changes.
* No billboard renderer extraction beyond calling the shared upload helper.

Behavior preservation checklist:

* Uploaded dynamic light count is unchanged.
* Position, color, radius, intensity, type, direction, cone cosine, shadow slot, matrix, bias, strength, and softness arrays match previous packing.
* Flicker uses the same `runtimeSeconds` and `DynamicLightEffectiveUploadIntensity()` path.
* Disabled dynamic lighting still uploads an effective zero count exactly as before.
* Sector, door, and billboard shader upload paths remain visually equivalent.
* Lightmap source-hash behavior is unchanged.
* Topology/editor/cache/collision/ECS behavior is unchanged.

Verification commands:

* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Use the Manual Visual Smoke Checklist.

Final report expectations:

* State which upload helpers moved.
* State that shader formulas, uniform names, counts, texture bindings, and render order did not change.
* State that lightmap source-hash, topology cache, collision, and ECS lifecycle behavior did not change.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_03` `Completed` only after checks pass or failures are documented.
* Mark `phase_04` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-014 complete.

## Phase 4: Move Shadow Map Resource Ownership And Lifetime

Goal:

Move `EnsureDynamicSpotLightShadowMapResources()` and matching unload logic into `SectorPreviewDynamicLighting`, preserving depth render texture setup, filter/wrap behavior, resource teardown, and shutdown ordering.

Files likely touched:

* `sources/sector_demo/SectorPreviewDynamicLighting.h`
* `sources/sector_demo/SectorPreviewDynamicLighting.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`

Allowed changes:

* Move helper-owned `std::array<RenderTexture2D, MaxDynamicSpotLightShadowCasters>` shadow maps.
* Move ensure/unload logic mechanically, including partial-failure cleanup.
* Expose narrow accessors for shadow map depth textures needed by sector, door, and billboard receiving paths.
* Preserve shutdown ordering by keeping `SectorMeshPreview::ShutdownRendererResources()` as the high-level caller.

Explicit non-goals:

* No shadow map resolution, filter, wrap, depth format, or unload behavior changes.
* No render pass order changes.
* No shadow material move unless Phase 4 explicitly documents that keeping material separate would create unsafe lifetime coupling.
* No shadow rendering orchestration move.
* No shader or upload behavior changes.

Behavior preservation checklist:

* Shadow maps still allocate at `DynamicSpotLightShadowMapResolution`.
* Depth texture IDs and material/billboard bindings are valid in the same cases as before.
* Failure to allocate any shadow map still cleans up already-created maps.
* Shutdown and rebuild still unload shadow maps before dependent resources become invalid.
* Missing or unallocated shadow maps still do not crash render paths.
* Lightmap source-hash behavior is unchanged.
* Topology/editor/cache/collision/ECS behavior is unchanged.

Verification commands:

* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Use the Manual Visual Smoke Checklist.

Final report expectations:

* State that shadow map resources moved late in the plan.
* State whether the dynamic spotlight shadow material stayed in `SectorMeshPreview` or moved, and why.
* State that resolution, filter/wrap, teardown, shutdown ordering, render order, and shader behavior did not change.
* State that lightmap source-hash, topology cache, collision, and ECS lifecycle behavior did not change.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_04` `Completed` only after checks pass or failures are documented.
* Mark `phase_05` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-014 complete.

## Phase 5: Move Dynamic Spotlight Shadow Map Render Orchestration

Goal:

Move `RenderDynamicSpotLightShadowMaps()` internals, or delegate most of them, to `SectorPreviewDynamicLighting` while preserving shadow map begin/end behavior, static sector caster drawing, alpha-test behavior, runtime door caster drawing, and render pass order.

Files likely touched:

* `sources/sector_demo/SectorPreviewDynamicLighting.h`
* `sources/sector_demo/SectorPreviewDynamicLighting.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`
* `sources/sector_demo/SectorDoorRuntime.h`
* `sources/sector_demo/SectorDoorRuntime.cpp`

Allowed changes:

* Add a narrow shadow-render context containing only the existing sector draw records, materials, door caster data, and matrices needed by the helper.
* Delegate shadow map begin/end and draw loops to the helper, or move the internal loop while keeping `SectorMeshPreview::RenderDynamicSpotLightShadowMaps()` as the public facade method.
* Keep `PrepareRuntimeDoorMeshes()` owned and sequenced by `SectorMeshPreview`.
* Keep door shadow caster data as an input; do not extract the door renderer.

Explicit non-goals:

* No render order changes.
* No static sector mesh ownership move.
* No door mesh cache ownership move.
* No alpha-test formula, cutoff, texture binding, model matrix, or caster selection changes.
* No point-light shadows.
* No shader code edits.

Behavior preservation checklist:

* Runtime door meshes are still prepared before door shadow casters are drawn.
* Each selected shadow matrix renders to the same shadow slot as before.
* Static sector shadow casters still use current alpha-test handling.
* Door shadow casters still use the current non-alpha path and model matrix behavior.
* Begin/end texture mode and clear behavior remain unchanged.
* Main scene render order remains unchanged.
* Lightmap source-hash behavior is unchanged.
* Topology/editor/cache/collision/ECS behavior is unchanged.

Verification commands:

* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Use the Manual Visual Smoke Checklist.

Final report expectations:

* State which shadow-pass responsibilities moved and which stayed in `SectorMeshPreview`.
* State that door renderer extraction remains out of scope.
* State that alpha test, door caster, model matrix, begin/end, render order, and shader behavior did not change.
* State that lightmap source-hash, topology cache, collision, and ECS lifecycle behavior did not change.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_05` `Completed` only after checks pass or failures are documented.
* Mark `phase_06` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-014 complete.

## Phase 6: Integrate Facade Cleanup

Goal:

Make `SectorMeshPreview` delegate dynamic lighting and dynamic spotlight shadow duties to `SectorPreviewDynamicLighting`, remove moved private members/functions from `SectorMeshPreview`, and keep public `SectorMeshPreview` API and debug accessors stable.

Files likely touched:

* `sources/sector_demo/SectorPreviewDynamicLighting.h`
* `sources/sector_demo/SectorPreviewDynamicLighting.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`

Allowed changes:

* Replace moved members in `SectorMeshPreview` with a focused helper member.
* Keep existing public facade methods and signatures stable.
* Preserve debug accessors by forwarding to helper-owned state where needed.
* Remove only private functions and members that were fully moved by earlier phases.
* Keep include changes minimal and local.

Explicit non-goals:

* No full `SectorMeshPreview` facade rewrite.
* No public API rename/removal.
* No broad file split beyond the dynamic lighting helper.
* No unrelated cleanup, formatting churn, or architecture rewrite.
* No door, billboard, sky, bloom, static scene, lightmap, source-hash, topology, collision, or ECS extraction.

Behavior preservation checklist:

* Existing callers still compile without public API changes.
* Public selected dynamic light and selected ID accessors still work.
* Rebuild, shutdown, update visibility/debug, shadow pass, draw scene, and render sequencing remain equivalent.
* Debug text remains equivalent.
* Dynamic lighting and shadow visual behavior remains equivalent.
* Lightmap source-hash behavior is unchanged.
* Topology/editor/cache/collision/ECS behavior is unchanged.

Verification commands:

* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Use the Manual Visual Smoke Checklist.

Final report expectations:

* State the final ownership split between `SectorMeshPreview` and `SectorPreviewDynamicLighting`.
* State that the `SectorMeshPreview` public facade remains stable.
* State that no shader formulas/names, max counts, ranking, flicker, bias/softness/strength, shadow map setup, render order, source-hash behavior, topology cache behavior, collision, or ECS lifecycle changed.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_06` `Completed` only after checks pass or failures are documented.
* Mark `phase_07` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-014 complete until Phase 7 closes verification and backlog state.

## Phase 7: Verification Docs And Backlog Update

Goal:

Complete final verification, update this plan state, and update `docs/plans/codebase_refactor_backlog.md` for REF-014 after implementation phases finish.

Files likely touched:

* `docs/plans/sector_preview_dynamic_lighting_refactor_plan.md`
* `docs/plans/codebase_refactor_backlog.md`

Allowed changes:

* Update this plan with final execution status, verification notes, and manual smoke status.
* Update REF-014 backlog notes using the existing backlog style.
* Mark REF-014 complete only if Phases 1 through 6 have actually completed and verification is documented.
* If implementation is incomplete, leave REF-014 not started, planned, partial, or in progress according to the existing backlog style; do not invent a new status convention.

Explicit non-goals:

* No source code changes.
* No shader edits.
* No CMake edits.
* No test edits.
* No runtime behavior changes.
* Do not mark REF-015, REF-018, or any later item complete.
* Do not broadly restructure the backlog.

Behavior preservation checklist:

* Confirm the final notes state whether source code changed in previous phases.
* Confirm final notes explicitly state lightmap source-hash behavior.
* Confirm final notes explicitly state topology cache behavior if any topology/editor code was touched.
* Confirm final notes explicitly state collision/sector lookup/physics behavior.
* Confirm final notes state manual smoke status without overclaiming.

Verification commands:

* `python3 tools/plan_executor.py docs/plans/sector_preview_dynamic_lighting_refactor_plan.md --status`
* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Use the Manual Visual Smoke Checklist if implementation phases changed visual rendering.

Final report expectations:

* Report final plan status and REF-014 backlog status.
* Report whether REF-014 was completed or left open.
* Report automated verification results.
* Report manual smoke status.
* Confirm no REF-015, REF-018, or later backlog item was completed.

Backlog/plan-state update instructions:

* Mark `phase_07` `Completed` only after final verification and backlog updates are complete.
* If all phases are complete, mark REF-014 complete using the existing backlog style.
* If any implementation phase remains incomplete, do not mark REF-014 complete.

## Initial Planning Task Validation

This runner plan was created as a planning/documentation-only task. During plan creation, REF-014 should remain `[ ]` and only receive a note that this dedicated runner plan exists.

Validation commands for the planning task:

* `python3 tools/plan_executor.py docs/plans/sector_preview_dynamic_lighting_refactor_plan.md --status`
* `git diff --check`
* `git diff --stat`
* `git status --short`
