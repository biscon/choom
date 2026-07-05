# SectorPreview Door Renderer Refactor Plan

## How To Use This Plan

This is a living execution plan for REF-015.

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

```plan-state-json id="sector-preview-door-renderer-refactor"
{
  "plan_id": "sector_preview_door_renderer_refactor",
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
      "title": "Introduce Door Renderer Context And Types",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_02",
      "title": "Move Door Mesh Cache Ownership And Preparation",
      "type": "phase",
      "status": "Completed"
    },
    {
      "id": "phase_03",
      "title": "Move Door Opaque Shader And Material Ownership",
      "type": "phase",
      "status": "Planned"
    },
    {
      "id": "phase_04",
      "title": "Move Main Scene Door Draw Path",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_05",
      "title": "Move Door Shadow Caster Drawing Integration",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_06",
      "title": "Integrate Facade Cleanup",
      "type": "phase",
      "status": "Not Started"
    },
    {
      "id": "phase_07",
      "title": "Verification Docs And Backlog Update",
      "type": "phase",
      "status": "Not Started"
    }
  ]
}
```

## Current Progress

| Phase / Pass | Status | Date | Notes |
| --- | --- | --- | --- |
| Phase 0: Baseline Audit And Behavior Contract | Completed | 2026-07-05 | Audited current door rendering, cache, lighting, shadow, debug, render-state, and ECS boundary behavior. Documentation-only; behavior contract updated. |
| Phase 1: Introduce Door Renderer Context And Types | Completed | 2026-07-05 | Added `SectorPreviewDoorRenderer` module with narrow context/data structs. Grouped existing door draw counters into `SectorPreviewDoorRenderStats` still owned by `SectorMeshPreview`. Source changed; behavior intended unchanged. Checks passed. |
| Phase 2: Move Door Mesh Cache Ownership And Preparation | Completed | 2026-07-05 | Moved renderer-owned door mesh cache entry/storage, mesh upload/update/prune/unload, and runtime door mesh preparation into `SectorPreviewDoorRenderer`. `SectorMeshPreview` still owns facade sequencing, shader/material state, main door draw, and the dynamic shadow render bridge. Source changed; behavior intended unchanged. Checks passed. |
| Phase 3: Move Door Opaque Shader And Material Ownership | Planned |  | Move door shader/material/resource state without shader formula or uniform name changes. |
| Phase 4: Move Main Scene Door Draw Path | Not Started |  | Move main-scene door rendering while preserving draw order, object-probe uploads, dynamic lighting/shadows, and render-state restore. |
| Phase 5: Move Door Shadow Caster Drawing Integration | Not Started |  | Let dynamic spotlight shadow rendering draw prepared door meshes through the door renderer boundary. |
| Phase 6: Integrate Facade Cleanup | Not Started |  | `SectorMeshPreview` delegates door preparation, drawing, shadow caster access, debug counters, and shutdown to the helper. |
| Phase 7: Verification Docs And Backlog Update | Not Started |  | Final verification, plan closeout, and REF-015 backlog update after implementation phases finish. |

## Execution Tracking Rules

* Each phase must leave the project buildable and runnable.
* Each phase final report must state whether source code changed.
* Each implementation phase must update this document before finishing.
* The update should be small and local: update the JSON status, the Current Progress row, and add or update an execution-log note for the selected phase.
* Do not rewrite unrelated phases when marking progress.
* Do not mark REF-015 complete in `docs/plans/codebase_refactor_backlog.md` until all implementation phases finish and Phase 7 closes the plan.
* If behavior is intended to remain unchanged, explicitly state that.
* If a phase changes rendering, shader uniform upload, resource lifetime, dynamic light or shadow selection, debug text, render order, source-hash behavior, topology cache behavior, collision, ECS ownership, or build/test behavior, clearly say so.
* Do not claim manual GUI or render smoke verification unless it was actually performed.
* If a phase is too broad, split it into smaller child passes and stop without source changes.
* Keep hard non-goals intact unless a future task explicitly replaces this plan.

## Goal And Desired End State

Extract procedural door rendering and renderer-owned door mesh-cache internals behind a focused preview-renderer helper while preserving behavior exactly.

Desired destination module:

* `sources/sector_demo/SectorPreviewDoorRenderer.h`
* `sources/sector_demo/SectorPreviewDoorRenderer.cpp`

`SectorMeshPreview` remains the public facade and owns high-level render sequencing. The extracted helper may own renderer-owned door mesh cache, mesh creation/upload/update/pruning/unload, door opaque shader and material state if moved by this plan, door shader uniform locations, door lighting debug mode state and names if the phase decides that is the cleanest boundary, door draw counters/status text, door static object-probe color upload buffers, preparation helpers before shadow passes and main draws, main-scene door drawing, and a narrow helper or callback boundary for drawing prepared door shadow casters.

The helper may observe `engine::World`, `engine::AssetManager`, `SectorDoorRuntime` ECS components, `SectorRuntimeDoorLightingContext`, `SectorPreviewDynamicLighting` draw/upload contexts, and `SectorTopologyMap` fallback data for object-probe sampling.

The helper must not own runtime ECS lifecycle, `SectorRuntimeObjectState`, `AssetManager` scopes, dynamic lighting selection, dynamic spotlight shadow maps, static sector meshes, billboard renderer, sky renderer, bloom renderer, topology map/document state, baked lightmap/object probe bake state, door runtime behavior, collision, interaction, or portal blocker behavior.

The renderer-owned GPU cache boundary is allowed: the helper may own raylib `Mesh`, `Material`, and `Shader` resources needed to draw doors, while runtime door entities/components remain owned by ECS/runtime systems.

## Behavior Contract

Phase 0 must verify and update this section before implementation starts.

Current door rendering behavior to preserve:

* `SectorMeshPreview` is the public facade for `Rebuild()`, `RebuildRendererResources()`, `ShutdownRendererResources()`, `RenderDynamicSpotLightShadowMaps()`, `DrawScene()`, and `Render()`.
* Door rendering is currently private to `SectorMeshPreview` through `PrepareRuntimeDoorMeshes()`, `DrawRuntimeDoors()`, `UnloadDoorMeshes()`, `ResolveDoorShadowCasterMesh()`, and related private data.
* Door mesh cache state is keyed by `SectorDoor::placedObjectId` in `doorMeshCache`. Cache entries store raylib `Mesh`, `SectorDoorSlabMeshData`, width, height, thickness, `SectorDoorFaceUvSet`, static-lighting color buffers, and a per-preparation `seenThisFrame` flag.
* Current door opaque renderer-owned state in `SectorMeshPreview` is `doorOpaqueShader`, `doorOpaqueMaterial`, `doorOpaqueDefaultMaterialTexture`, `doorOpaqueShaderLoaded`, `doorOpaqueMaterialLoaded`, `doorOpaqueTextureLoc`, dynamic light uniform locations, shadow uniform locations, `doorOpaqueDynamicLightingClampLoc`, `doorOpaqueDebugModeLoc`, and `doorOpaqueTintLoc`.
* Door cache preparation iterates ECS entities with `SectorObjectTransform`, `SectorObject`, `SectorDoor`, `SectorDoorResolvedAnchor`, and `SectorDoorRender`.
* Door cache preparation appends `SectorDoorShadowCaster` records through `AppendSectorDoorShadowCaster()`, marks seen cache entries, rebuilds meshes when vertex data is absent or dimensions/face UVs change, and prunes unseen entries by unloading their meshes.
* Door slab mesh creation currently uses `BuildSectorDoorSlabMeshData(render)`, `CreateDoorSlabMesh()`, CPU-side vertex arrays, normals, UVs, color buffers, unsigned-short index buffers, `MemAlloc()`, and `UploadMesh(&mesh, false)`. Allocation failure logs `"[SectorDemo ERROR] Failed to allocate door slab mesh data"` and returns an empty mesh after `UnloadMesh(mesh)`.
* Door mesh dirty keys are absent/empty mesh, width, height, thickness, and `SameSectorDoorFaceUvSet(cacheEntry.faceUvs, render.faceUvs)`.
* Main-scene door draw calls `PrepareRuntimeDoorMeshes()` again so door drawing works even if no shadow pass ran first.
* `Render()` calls `RenderDynamicSpotLightShadowMaps()` before `DrawScene()`.
* `DrawScene()` draws `skyRenderer.Draw()`, then visible static sector mesh batches, then `DrawRuntimeDoors()`, then `SectorPreviewBillboardRenderer::Draw()` while still inside `BeginMode3D(camera)` / `EndMode3D()`.
* Main-scene door draw preserves object visibility, door enabled state, render visibility, positive width/height/thickness checks, missing texture fallback, cache-miss skipping, tint upload, and draw counters.
* Door textures are resolved through `SectorMeshPreview::TextureForId()` and `AssetManager::GetTexture()`, with fallback to the door material default diffuse texture.
* Door static object-probe lighting is rebuilt for the current animated transform with `BuildSectorDoorStaticLightingColors()` and uploaded through `UpdateMeshBuffer()` to the mesh color buffer before drawing.
* Door main draw uses `BuildSectorDoorSlabModelMatrix(transform, anchor)` so current animated door transforms drive the visual slab.
* Door shadow caster records are prepared in `PrepareRuntimeDoorMeshes()` and stored in `runtimeDoorShadowCasters`.
* Door shadow caster drawing uses `SectorPreviewDynamicSpotLightShadowRenderContext::doorShadowCasters`, `userData`, and `doorMeshResolver`; `ResolveDoorShadowCasterMesh()` reads prepared `doorMeshCache` entries, returns cache width/height, and `SectorPreviewDynamicLighting::RenderShadowMaps()` draws them with `BuildSectorDoorShadowCasterModelMatrix()`. Door shadow caster seal margins from `kSectorDoorShadowCasterVerticalSealMarginWorld` and `kSectorDoorShadowCasterHorizontalSealMarginWorld` must not change.
* `SectorPreviewDynamicLighting` owns dynamic light selection, selected lights, dynamic spotlight shadow map resources, shadow material ownership, shadow uniform packing, and shadow map render orchestration after REF-014.
* Door dynamic point/spot light receiving uploads through `UploadSectorPreviewDynamicPointLights()` using door shader locations and `dynamicLightState.SelectedLights()`.
* Door dynamic spotlight shadow receiving uploads through `UploadSectorPreviewDynamicSpotLightShadowUniforms()` using `dynamicLightState.PackShadowUniforms()`.
* Door shadow map depth textures are bound through `doorOpaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS]` and `doorOpaqueMaterial.maps[MATERIAL_MAP_OCCLUSION]`.
* Door lighting debug mode currently lives in `SectorMeshPreview` as `doorLightingDebugMode`, with public `DoorLightingDebugMode()` and `SetDoorLightingDebugMode()` accessors, `DoorLightingDebugModeShaderValue()` using the enum integer value, and `SectorDoorLightingDebugModeName()` returning `Normal`, `AlbedoOnly`, `BakedOnly`, `DynamicOnly`, `NormalVisualize`, and `FlatColorNoTexture`.
* Door render-state setup disables color blending, disables backface culling, enables depth test and depth writes, then restores active texture slot, texture binding, color blend, alpha blend mode, depth test, depth mask, and backface culling after drawing.
* Door draw counters currently live in `SectorMeshPreview` as `doorConsideredCount`, `doorDrawnCount`, and `doorSkippedCount`; debug text is appended with `AppendDoorRenderDebugText()` using the `" | doors:"` suffix convention.
* Door renderer resource lifetime is currently part of `SectorMeshPreview::RebuildRendererResources()` / `ShutdownRendererResources()`: rebuild loads the door opaque shader and default material, while shutdown clears runtime shadow casters, unloads door meshes, restores material map textures, unloads the door material, resets door shader/material handles, and resets door uniform locations.
* Missing, failed, unloaded, or not-ready door textures and shadow maps must not crash rendering.
* `SectorMeshPreview` and any extracted door renderer may observe ECS/runtime objects and own renderer GPU caches, but must not spawn, reset, reserve, destroy, or otherwise own runtime object lifecycle.

Hard non-goals for every phase:

* No runtime ECS lifecycle ownership changes.
* No spawning, destroying, resetting, or reserving runtime objects from the renderer.
* No door runtime behavior changes.
* No door motion, easing, open-close, interaction, or auto-open behavior changes.
* No door collision behavior changes.
* No door portal blocker behavior changes.
* No door JSON/schema changes.
* No door UV, texture authoring, texture ID, tint, or material behavior changes.
* No object probe placement, bake, sidecar, or sampling policy changes.
* No baked lightmap/source hash changes.
* No dynamic lighting or shadow selection changes.
* No shader formula changes.
* No shader uniform name changes.
* No dynamic spotlight shadow-map resource ownership changes.
* No shadow bias, softness, strength, or slot behavior changes.
* No shadow caster seal margin behavior changes.
* No render order changes.
* No billboard, sky, bloom, or static scene extraction.
* No full `SectorMeshPreview` facade rewrite.
* No topology/editor/cache behavior changes.
* No collision, sector lookup, or physics behavior changes.

## Manual Visual Smoke Checklist

Use this checklist for implementation phases that affect visual rendering. Report each item as performed, not performed, or not applicable.

* Closed vertical sliding door renders correctly.
* Opening/closing door animation still moves the visual slab correctly.
* Slide-left and slide-right doors render from current animated position.
* Door `textureId` and per-face UV settings still render correctly.
* Door tint still applies correctly.
* Door object-probe/static lighting still appears.
* Door per-vertex object-probe lighting still updates for current transform.
* Dynamic point lights still affect doors.
* Dynamic spotlights still affect doors.
* Dynamic spotlight shadows still affect doors.
* Doors still cast dynamic spotlight shadows.
* Door shadow caster seal margins still behave the same.
* Door lighting debug modes still isolate terms: Normal, BakedOnly, DynamicOnly, AlbedoOnly, NormalVisualize, FlatColorNoTexture.
* Dynamic shadows on static sectors and billboards still work.
* No render state leaks into billboards, sky, bloom, overlays, or UI.
* Door collision, interaction, auto-open, and portal blocking still behave unchanged.

## Execution Log

* 2026-07-05: Phase 0 completed. Audited `SectorMeshPreview` door rendering/cache/shader/material/debug ownership, `SectorPreviewDynamicLighting` shadow bridge, door mesh creation, dynamic light and shadow uploads, render order, resource lifetime, and ECS/runtime boundaries. Updated the Behavior Contract with exact current symbol names and ownership notes. Documentation-only; no source code, shader, CMake, test, runtime behavior, lightmap source-hash, topology cache, collision, sector lookup, physics, or ECS lifecycle changes. Manual visual smoke was not performed because this is a documentation-only audit phase.
* 2026-07-05: Phase 1 completed. Added `sources/sector_demo/SectorPreviewDoorRenderer.h` and `.cpp` with narrow door lighting, draw, preparation, resource, stats, dynamic-light, texture-resolver, mesh-resolver, and shadow-caster context/data types. Moved `SectorRuntimeDoorLightingContext` declaration into the new header through `SectorMeshPreview.h` and grouped the existing public door draw counters into `SectorPreviewDoorRenderStats` while keeping ownership and all rendering implementation in `SectorMeshPreview`. Source code changed; no shader formulas or uniform names, shader/material/cache ownership, dynamic lighting or shadow selection, render order, runtime behavior, lightmap source-hash behavior, topology cache behavior, collision, sector lookup, physics, ECS lifecycle, CMake source-list, or JSON/schema behavior changed. Manual visual smoke was not performed because this pass only introduced types and mechanical no-op counter grouping. Verification passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`; `ctest --test-dir cmake-build-debug --output-on-failure`.
* 2026-07-05: Phase 2 completed. Moved `DoorMeshCacheEntry`, `doorMeshCache`, prepared door shadow-caster storage, door slab mesh allocation/upload, runtime door mesh preparation, stale-cache pruning, and mesh unload ownership into `SectorPreviewDoorRenderer`. `SectorMeshPreview` delegates preparation/unload and still owns public render sequencing, door opaque shader/material state, main-scene door drawing, texture lookup, dynamic lighting/shadow selection, and the current dynamic shadow render context bridge. Source code changed; cache keying by `placedObjectId`, dirty keys, mesh vertex/color/normal/UV/index upload behavior, allocation error text, unseen-entry pruning, main-draw preparation fallback, and door shadow caster preparation were intended unchanged. No shader formulas or uniform names, dynamic spotlight shadow-map resource ownership, runtime door behavior, door collision/interaction/portal blocking, JSON/schema, lightmap source-hash behavior, topology cache behavior, sector lookup, physics, or ECS lifecycle behavior changed. Manual visual smoke was not performed. Verification passed: `cmake --build cmake-build-debug -j2`; `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`; `ctest --test-dir cmake-build-debug --output-on-failure`.

## Phase 0: Baseline Audit And Behavior Contract

Goal:

Confirm current door rendering, mesh cache, object-probe color upload, dynamic lighting/shadow receiving, dynamic spotlight shadow casting, render order, resource lifetime, debug mode, and ECS boundary.

Files likely touched:

* `docs/plans/sector_preview_door_renderer_refactor_plan.md`

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
* Confirm current door shader/material members, door cache entry fields, mesh dirty keys, static-light upload path, and draw counters.
* Confirm current dynamic-light upload and shadow-map binding paths use the REF-014 dynamic lighting boundary.
* Confirm current door shadow caster bridge, model matrix, and seal margin behavior.
* Confirm render pass order: shadow maps before main draw, and static sectors before doors before billboards in main draw.
* Confirm no lightmap source-hash, topology cache, collision, sector lookup, physics, or ECS lifecycle behavior is touched.

Verification commands:

* `python3 tools/plan_executor.py docs/plans/sector_preview_door_renderer_refactor_plan.md --status`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Not required for this documentation-only audit phase.

Final report expectations:

* State that this was documentation-only.
* List behavior contract updates made.
* State that no source code, shaders, tests, CMake, or runtime behavior changed.
* State that no manual render smoke was performed unless it actually was.
* State that lightmap source-hash behavior, topology cache behavior, collision, sector lookup, physics, and ECS ownership were unchanged.
* Report verification command results.

Backlog/plan-state update instructions:

* Mark `phase_00` `Completed` only after updating the Behavior Contract.
* Leave `phase_01` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-015 complete.

## Phase 1: Introduce Door Renderer Context And Types

Goal:

Add small context/data structs for door rendering inputs and shadow-caster inputs.

Files likely touched:

* `sources/sector_demo/SectorPreviewDoorRenderer.h`
* `sources/sector_demo/SectorPreviewDoorRenderer.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`
* CMake source list only if the project requires new `.cpp` files to be listed; do not otherwise edit CMake.

Allowed changes:

* Add the destination module with minimal declarations.
* Add narrow door draw, preparation, resource, stats, dynamic-light, texture-resolver, and shadow-caster context structs as needed.
* Thread new types through local call sites only where it is a mechanical no-op.
* Keep door cache, shader/material, debug mode, draw counters, and ownership in `SectorMeshPreview`.

Explicit non-goals:

* No ownership move.
* No shader/material/cache move.
* No render order changes.
* No dynamic lighting or shadow selection changes.
* No runtime ECS lifecycle changes.
* No door runtime behavior, collision, interaction, portal blocker, JSON/schema, or UV/texture authoring changes.

Behavior preservation checklist:

* Public `SectorMeshPreview` API remains stable.
* Existing door draw and shadow paths still call the same implementation.
* Dynamic light/shadow upload values and texture bindings remain unchanged.
* Door debug mode names and public accessors remain unchanged.
* Lightmap source-hash behavior is unchanged.
* Topology cache behavior is unchanged.
* Collision, sector lookup, physics, and ECS ownership are unchanged.

Verification commands:

* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Not required if this phase only introduces types and mechanical no-op threading. Use the Manual Visual Smoke Checklist if any visual rendering path changes.

Final report expectations:

* State which context/types were added.
* State that ownership remains in `SectorMeshPreview`.
* State that no shader formulas/names, dynamic selection, render order, source-hash behavior, topology cache behavior, collision, sector lookup, physics, or ECS lifecycle behavior changed.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_01` `Completed` only after checks pass or failures are documented.
* Leave `phase_02` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-015 complete.

## Phase 2: Move Door Mesh Cache Ownership And Preparation

Goal:

Move door mesh cache entry type, mesh upload/update/prune/unload, and `PrepareRuntimeDoorMeshes()` logic into `SectorPreviewDoorRenderer`.

Files likely touched:

* `sources/sector_demo/SectorPreviewDoorRenderer.h`
* `sources/sector_demo/SectorPreviewDoorRenderer.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`
* `sources/sector_demo/SectorDoorRuntime.h`
* `sources/sector_demo/SectorDoorRuntime.cpp`

Allowed changes:

* Move `DoorMeshCacheEntry` and `doorMeshCache` ownership into the door renderer.
* Move `CreateDoorSlabMesh()` if it is only needed by the door renderer, preserving allocation, upload, vertex/color/index data, and error behavior.
* Move cache preparation and pruning behavior, including `seenThisFrame`, `AppendSectorDoorShadowCaster()`, dirty keys, mesh unloads, and `runtimeDoorShadowCasters` preparation or equivalent output.
* Keep the helper as a renderer-owned GPU cache that observes ECS.
* Preserve `SectorMeshPreview::RenderDynamicSpotLightShadowMaps()` and `DrawScene()` as public facade sequencing methods.

Explicit non-goals:

* No runtime ECS lifecycle mutation.
* No door runtime, motion, collision, interaction, portal blocker, JSON/schema, UV authoring, or texture authoring changes.
* No shader/material ownership move unless required only to compile and documented as a child pass.
* No dynamic lighting/shadow selection move.
* No dynamic spotlight shadow-map resource ownership changes.
* No static sector, billboard, sky, or bloom extraction.

Behavior preservation checklist:

* Cache keys remain `placedObjectId`.
* Mesh dirty checks still include absent mesh, width, height, thickness, and face UV set.
* Mesh creation still uses the same slab mesh data, vertex colors, normals, UVs, indices, and `UploadMesh(&mesh, false)` behavior.
* Pruning still unloads unseen meshes and erases stale cache entries.
* Runtime door shadow caster records are still prepared before shadow passes and main-scene door draws.
* Main draw can still prepare meshes when no shadow pass ran first.
* Lightmap source-hash behavior is unchanged.
* Topology cache behavior is unchanged.
* Collision, sector lookup, physics, and ECS ownership are unchanged.

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

* State that the door renderer owns the renderer GPU cache and what remains in `SectorMeshPreview`.
* State that ECS lifecycle and runtime door behavior did not change.
* State that cache keying, dirty keys, pruning, mesh upload, shadow caster preparation, and source-hash behavior were preserved.
* State that topology cache behavior, collision, sector lookup, and physics were unchanged.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_02` `Completed` only after checks pass or failures are documented.
* Leave `phase_03` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-015 complete.

## Phase 3: Move Door Opaque Shader And Material Ownership

Goal:

Move door opaque shader source strings, shader load/unload logic, material, uniform locations, and debug mode state if appropriate.

Files likely touched:

* `sources/sector_demo/SectorPreviewDoorRenderer.h`
* `sources/sector_demo/SectorPreviewDoorRenderer.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`

Allowed changes:

* Move `LoadDoorOpaqueShader()` and door opaque shader/material lifetime into the door renderer if the ownership boundary is clear.
* Move door shader uniform location members into the door renderer.
* Move door lighting debug mode state and debug-name helper only if public `SectorMeshPreview` accessors can remain stable through forwarding.
* Keep shader source text, formulas, uniform names, material map slots, and default texture behavior byte-for-byte equivalent except for relocation.
* Keep shutdown/rebuild ordering equivalent through facade calls.

Explicit non-goals:

* No shader formula changes.
* No shader uniform name changes.
* No dynamic lighting upload behavior changes.
* No dynamic spotlight shadow-map resource ownership changes.
* No texture/material authoring behavior changes.
* No render order changes.
* No unrelated renderer extraction.

Behavior preservation checklist:

* Door shader loading succeeds and fails in the same cases.
* Uniform lookup names and array element lookup behavior are unchanged.
* Door diffuse, roughness shadow map, and occlusion shadow map slots remain unchanged.
* Door debug modes still map to the same shader integer values and display names.
* Resource shutdown and rebuild still unload/reset shader, material, default texture, and map textures safely.
* Lightmap source-hash behavior is unchanged.
* Topology cache behavior is unchanged.
* Collision, sector lookup, physics, and ECS ownership are unchanged.

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

* State exactly which shader/material/debug resources moved.
* State that shader formulas, uniform names, material map slots, debug-mode values, dynamic lighting upload behavior, and render order did not change.
* State that source-hash behavior, topology cache behavior, collision, sector lookup, physics, and ECS ownership were unchanged.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_03` `Completed` only after checks pass or failures are documented.
* Leave `phase_04` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-015 complete.

## Phase 4: Move Main Scene Door Draw Path

Goal:

Move `DrawRuntimeDoors()` main-scene rendering into `SectorPreviewDoorRenderer`.

Files likely touched:

* `sources/sector_demo/SectorPreviewDoorRenderer.h`
* `sources/sector_demo/SectorPreviewDoorRenderer.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`
* `sources/sector_demo/SectorPreviewDynamicLighting.h`

Allowed changes:

* Move ECS draw traversal for doors into the door renderer.
* Pass explicit context for `AssetManager`, texture resolution, object-probe lighting, fallback map, dynamic lighting state, runtime seconds, dynamic-light enabled state, and render debug text.
* Use the REF-014 dynamic lighting boundary for selected lights, shadow uniforms, and shadow-map textures.
* Move draw counters/status text into the helper, with `SectorMeshPreview` forwarding public accessors.
* Preserve render-state setup and restore exactly.

Explicit non-goals:

* No render order changes: static sectors, then doors, then billboards.
* No dynamic lighting/shadow selection changes.
* No object probe placement, bake, sidecar, or fallback policy changes.
* No door UV/texture/tint behavior changes.
* No runtime door, collision, interaction, auto-open, portal blocker, JSON/schema, or ECS lifecycle changes.
* No shader formula or uniform name changes.

Behavior preservation checklist:

* Door draw still prepares meshes for main draw when needed.
* Door filtering for visibility, enabled state, render visibility, and positive dimensions remains unchanged.
* Door texture fallback and missing/not-ready texture behavior remain unchanged.
* Door tint upload remains unchanged.
* Object-probe static-light color generation and `UpdateMeshBuffer()` upload remain unchanged and use the current animated transform.
* Dynamic point/spot lights and dynamic spotlight shadows still receive through the dynamic lighting helper boundary.
* Door counters and render debug text remain equivalent.
* Render state is restored before billboard, sky, bloom, overlay, or UI paths can observe it.
* Lightmap source-hash behavior is unchanged.
* Topology cache behavior is unchanged.
* Collision, sector lookup, physics, and ECS ownership are unchanged.

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

* State that main-scene door drawing moved and how `SectorMeshPreview` delegates it.
* State that draw order, static-light upload, dynamic light/shadow receiving, texture/UV/tint behavior, render-state restore, and debug counters were preserved.
* State that source-hash behavior, topology cache behavior, collision, sector lookup, physics, and ECS ownership were unchanged.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_04` `Completed` only after checks pass or failures are documented.
* Leave `phase_05` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-015 complete.

## Phase 5: Move Door Shadow Caster Drawing Integration

Goal:

Move or delegate the door shadow caster draw helper/path so dynamic spotlight shadow rendering can draw prepared door meshes without reaching into `SectorMeshPreview` internals.

Files likely touched:

* `sources/sector_demo/SectorPreviewDoorRenderer.h`
* `sources/sector_demo/SectorPreviewDoorRenderer.cpp`
* `sources/sector_demo/SectorPreviewDynamicLighting.h`
* `sources/sector_demo/SectorPreviewDynamicLighting.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`

Allowed changes:

* Replace `SectorMeshPreview` door mesh resolver access with a narrow door renderer resolver, callback, or draw-delegate context.
* Keep dynamic spotlight shadow rendering orchestration owned by the REF-014 dynamic lighting helper.
* Keep `SectorMeshPreview::RenderDynamicSpotLightShadowMaps()` as the public facade method.
* Preserve preparation before shadow rendering and preserve door shadow caster model matrix and seal margin behavior.

Explicit non-goals:

* No dynamic spotlight shadow-map resource ownership changes.
* No dynamic light or shadow selection changes.
* No dynamic lighting extraction work.
* No shadow bias, softness, strength, slot, matrix selection, or sampler behavior changes.
* No door shadow caster seal margin behavior changes.
* No static sector shadow caster alpha-test behavior changes.
* No render order changes.

Behavior preservation checklist:

* Prepared door meshes are still available before shadow maps render.
* Door shadow casters still use the same mesh, width, height, model matrix, and seal margins.
* Door shadow casters still draw into the same selected dynamic spotlight shadow slots.
* Static sector and billboard dynamic shadow behavior remains unchanged.
* Missing door meshes still skip safely.
* Lightmap source-hash behavior is unchanged.
* Topology cache behavior is unchanged.
* Collision, sector lookup, physics, and ECS ownership are unchanged.

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

* State how dynamic spotlight shadow rendering now reaches prepared door meshes.
* State that dynamic lighting/shadow selection, shadow-map ownership, seal margins, model matrix behavior, render order, and shader behavior did not change.
* State that source-hash behavior, topology cache behavior, collision, sector lookup, physics, and ECS ownership were unchanged.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_05` `Completed` only after checks pass or failures are documented.
* Leave `phase_06` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-015 complete.

## Phase 6: Integrate Facade Cleanup

Goal:

`SectorMeshPreview` delegates door mesh preparation, door main draw, door shadow draw/access, door debug counters, debug mode accessors, and door resource shutdown to `SectorPreviewDoorRenderer`.

Files likely touched:

* `sources/sector_demo/SectorPreviewDoorRenderer.h`
* `sources/sector_demo/SectorPreviewDoorRenderer.cpp`
* `sources/sector_demo/SectorMeshPreview.h`
* `sources/sector_demo/SectorMeshPreview.cpp`

Allowed changes:

* Remove moved private door members/functions from `SectorMeshPreview`.
* Keep public `SectorMeshPreview` methods and accessors stable through forwarding.
* Keep door renderer helper narrow and renderer-focused.
* Keep include changes minimal and local.
* Preserve rebuild/shutdown sequencing through high-level facade calls.

Explicit non-goals:

* No public `SectorMeshPreview` API rename/removal.
* No full `SectorMeshPreview` facade rewrite.
* No unrelated cleanup, formatting churn, file moves, or architecture rewrite.
* No static scene, billboard, sky, bloom, topology, collision, lightmap, or runtime lifecycle extraction.
* No behavior changes.

Behavior preservation checklist:

* Existing callers still compile without public API changes.
* Door debug mode accessors and draw counter accessors still work.
* Rebuild and shutdown release door resources in the same safe order relative to asset scope and other renderer resources.
* Shadow pass and main-scene draw sequencing remain equivalent.
* Door visual behavior, lighting, shadows, textures, UVs, tint, and debug modes remain equivalent.
* Lightmap source-hash behavior is unchanged.
* Topology cache behavior is unchanged.
* Collision, sector lookup, physics, and ECS ownership are unchanged.

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

* State the final ownership split between `SectorMeshPreview` and `SectorPreviewDoorRenderer`.
* State that the `SectorMeshPreview` public facade remains stable.
* State that no shader formulas/names, dynamic selection, shadow maps, render order, source-hash behavior, topology cache behavior, collision, sector lookup, physics, ECS lifecycle, or runtime door behavior changed.
* Report automated verification and manual smoke status.

Backlog/plan-state update instructions:

* Mark `phase_06` `Completed` only after checks pass or failures are documented.
* Leave `phase_07` `Planned`.
* Update the Current Progress rows to match the JSON state.
* Do not mark REF-015 complete until Phase 7 closes verification and backlog state.

## Phase 7: Verification Docs And Backlog Update

Goal:

Complete verification, update plan state, and update `docs/plans/codebase_refactor_backlog.md` for REF-015 after implementation phases finish.

Files likely touched:

* `docs/plans/sector_preview_door_renderer_refactor_plan.md`
* `docs/plans/codebase_refactor_backlog.md`

Allowed changes:

* Update this plan with final execution status, verification notes, source-hash notes, topology cache notes, collision/physics notes, ECS ownership notes, and manual smoke status.
* Update REF-015 backlog notes using the existing backlog style.
* Mark REF-015 complete only if Phases 1 through 6 have actually completed and verification is documented.
* If implementation is incomplete, leave REF-015 not started, planned, partial, or in progress according to the existing backlog style; do not invent a new status convention.

Explicit non-goals:

* No source code changes.
* No shader edits.
* No CMake edits.
* No test edits.
* No runtime behavior changes.
* Do not mark REF-018 or any later item complete.
* Do not broadly restructure the backlog.

Behavior preservation checklist:

* Confirm final notes state whether source code changed in previous phases.
* Confirm final notes explicitly state lightmap source-hash behavior.
* Confirm final notes explicitly state topology cache behavior.
* Confirm final notes explicitly state collision, sector lookup, and physics behavior.
* Confirm final notes explicitly state ECS ownership and runtime door behavior.
* Confirm final notes state manual smoke status without overclaiming.

Verification commands:

* `python3 tools/plan_executor.py docs/plans/sector_preview_door_renderer_refactor_plan.md --status`
* `cmake --build cmake-build-debug -j2`
* `ctest --test-dir cmake-build-debug --output-on-failure -R "sector_runtime_object|sector_light|sector_topology|sector_editor|sector_authoring"`
* `ctest --test-dir cmake-build-debug --output-on-failure`
* `git diff --check`
* `git diff --stat`
* `git status --short`

Manual smoke checklist:

* Use the Manual Visual Smoke Checklist if implementation phases changed visual rendering.

Final report expectations:

* Report final plan status and REF-015 backlog status.
* Report whether REF-015 was completed or left open.
* Report automated verification results.
* Report manual smoke status.
* Confirm no REF-018 or later backlog item was completed.

Backlog/plan-state update instructions:

* Mark `phase_07` `Completed` only after final verification and backlog updates are complete.
* If all phases are complete, mark REF-015 complete using the existing backlog style.
* If any implementation phase remains incomplete, do not mark REF-015 complete.

## Initial Planning Task Validation

This runner plan was created as a planning/documentation-only task. During plan creation, REF-015 should remain `[ ]` and only receive a note that this dedicated runner plan exists.

Validation commands for the planning task:

* `python3 tools/plan_executor.py docs/plans/sector_preview_door_renderer_refactor_plan.md --status`
* `git diff --check`
* `git diff --stat`
* `git status --short`
