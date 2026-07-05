# SectorMeshPreview Post-Refactor Audit

## Summary

The REF-014 through REF-017 extraction campaign materially improved the renderer architecture. `SectorMeshPreview` no longer owns billboard shader/draw traversal, sky mesh/material lifetime, bloom render targets/shaders, dynamic spotlight shadow resources, or door mesh cache/shader/main draw internals directly. Those responsibilities now sit behind focused helpers at `sources/sector_demo/SectorMeshPreview.h:163-167`.

`SectorMeshPreview` is now an acceptable facade/coordinator for the current codebase. It still owns the public editor/demo API, static generated geometry and sector mesh/material draw path, the preview asset scope and texture-handle table, visibility/debug text composition, camera pose, and high-level render sequencing.

Top 5 findings:

- `SectorMeshPreview` is mostly a facade now, but the static sector material/shader path remains a real renderer implementation in the facade.
- Resource ownership is clearer after extraction; helper GPU resources are generally owned and unloaded by their helper.
- Render order remains centralized and readable in `DrawScene()` and `RenderDynamicSpotLightShadowMaps()`.
- The main remaining ownership coupling is centralized texture resolution/asset scope ownership, which helpers observe through contexts rather than owning.
- The name mismatch is now the largest cleanup issue: "Preview" describes a reusable renderer/facade more than a narrow preview-only object.

Recommendation: proceed with a mechanical Preview-to-Renderer terminology rename now. Defer static scene renderer extraction and any full facade rewrite.

## Scope And Method

Inspected files:

- `docs/audit/sector_mesh_preview_extraction_audit.md`
- `docs/plans/codebase_refactor_backlog.md`
- `docs/plans/sector_preview_dynamic_lighting_refactor_plan.md`
- `docs/plans/sector_preview_door_renderer_refactor_plan.md`
- `sources/sector_demo/SectorMeshPreview.h`
- `sources/sector_demo/SectorMeshPreview.cpp`
- `sources/sector_demo/SectorPreviewBillboardRenderer.h/.cpp`
- `sources/sector_demo/SectorPreviewSkyRenderer.h/.cpp`
- `sources/sector_demo/SectorPreviewBloom.h/.cpp`
- `sources/sector_demo/SectorPreviewDynamicLighting.h/.cpp`
- `sources/sector_demo/SectorPreviewDoorRenderer.h/.cpp`
- `sources/sector_demo/SectorMeshBuilder.h/.cpp`
- `sources/sector_demo/SectorMeshTypes.h`
- `sources/sector_demo/SectorGeneratedGeometry.h/.cpp`
- `sources/sector_demo/SectorDynamicPointLightSelection.h/.cpp`
- `sources/sector_editor` call sites
- `sources/sector_demo/SectorDemo.*` call sites

Commands used are listed in the appendix. This is lightweight grep/static analysis only. No runtime render smoke was performed, so visual parity of sky, bloom, billboards, doors, and dynamic shadows still needs manual confirmation if desired.

## Current SectorMeshPreview Responsibilities

| Responsibility | Main functions/methods | Main member variables/resources | Owned or delegated | Should remain / move / defer | Notes |
| --- | --- | --- | --- | --- | --- |
| Public facade API | `Rebuild()`, `RebuildRendererResources()`, `ShutdownRendererResources()`, `Render()`, `RenderDynamicSpotLightShadowMaps()`, `DrawScene()` at `SectorMeshPreview.h:35-63` | public stable API, `initialized` | Owned by `SectorMeshPreview` | Should remain | Editor and demo call sites depend on this stable boundary. |
| Generated geometry and static sector mesh ownership | `BuildSectorGeneratedGeometry()` and `BuildSectorMeshes()` calls at `SectorMeshPreview.cpp:495-566`; `UnloadSectorMeshes()` at `SectorMeshPreview.cpp:700` | `generatedGeometry`, `meshes` at `SectorMeshPreview.h:119-120` | Owned by `SectorMeshPreview` | Defer move | Static scene extraction is possible later but not needed before rename. |
| AssetManager scope and topology texture handles | asset scope and texture requests at `SectorMeshPreview.cpp:515-535`; progress at `1026-1028` | `assetScope`, `textureHandlesById`, `lightmapTexture` at `SectorMeshPreview.h:129-131` | Owned by `SectorMeshPreview`, observed by helpers | Should remain | Central source for map texture IDs and lightmap texture handles. |
| Static sector material/lightmap/decal draw path | `LoadPreviewMaterial()` at `SectorMeshPreview.cpp:373-446`; static draw loop at `803-850`; material unload at `705-718` | `material`, `defaultMaterialTexture`, shader uniform locs at `SectorMeshPreview.h:132-162` | Owned by `SectorMeshPreview` | Defer move | Largest remaining renderer implementation in facade. |
| Visibility graph/debug state | graph build at `SectorMeshPreview.cpp:504-513`; `UpdateVisibilityDebug()` at `969-1019` | `visibilityGraph`, `visibilityResult`, debug strings, `visibilityLookupWorld` at `SectorMeshPreview.h:121-128` | Owned by `SectorMeshPreview` | Should remain for now | Used by render culling, dynamic light selection, overlays, and debug UI. |
| Render sequencing | `Render()` at `738-746`; `DrawScene()` at `748-873`; shadow facade at `903-917` | camera, helper members, static meshes/material | Owned by `SectorMeshPreview` | Should remain | Keeping order central is useful and avoids hidden pass ordering. |
| Billboard renderer coordination | context build at `SectorMeshPreview.cpp:875-900`; draw at `869-870` | `billboardRenderer` at `SectorMeshPreview.h:165` | Delegated | Should remain | Facade supplies packed dynamic lighting; helper owns shader/draw stats. |
| Sky renderer coordination | rebuild at `SectorMeshPreview.cpp:538-543`; draw at `759`; shutdown at `698` | `skyRenderer` at `SectorMeshPreview.h:163` | Delegated | Should remain | Renderer-only helper; facade decides when map wants sky. |
| Bloom coordination | wrapper at `SectorMeshPreview.cpp:920-934`; shutdown at `695` | `bloomRenderer` at `SectorMeshPreview.h:164` | Delegated | Should remain | Helper owns render targets; facade passes static batches and texture table. |
| Dynamic lighting coordination | rebuild at `SectorMeshPreview.cpp:575-577`; selection at `999`; uploads at `777-802`; shadow pass at `911-917` | `dynamicLightState`, `dynamicLightingEnabled`, `runtimeSeconds` at `SectorMeshPreview.h:167-169` | Delegated with facade toggles | Should remain | Helper owns selection/shadow resources; facade owns global enable and draw timing. |
| Door renderer coordination | resource load at `SectorMeshPreview.cpp:578-599`; draw context at `852-867`; shadow context at `916`; accessors at `SectorMeshPreview.h:97-108` | `doorRenderer` at `SectorMeshPreview.h:166` | Delegated | Should remain | Door renderer owns GPU cache and ECS traversal; facade supplies texture/dynamic-light contexts. |
| Debug/status accessors | accessors at `SectorMeshPreview.h:80-108`; text composition at `SectorMeshPreview.cpp:995-1018` | debug strings, helper stats | Mixed | Defer cleanup | Useful for UI, but text composition is mildly landfill-like. |

## Resource Ownership Review

| Resource | Owner | Created/rebuilt where | Unloaded where | Ownership clarity | Risks |
| --- | --- | --- | --- | --- | --- |
| AssetManager scope | `SectorMeshPreview` | `assets.CreateScope()` at `SectorMeshPreview.cpp:515` | `assets.UnloadScope()` at `SectorMeshPreview.cpp:723-726` | Clear | Scope is still centralized, so helper texture use depends on facade-owned handles staying valid. |
| `textureHandlesById` | `SectorMeshPreview` | texture request loop at `SectorMeshPreview.cpp:522-535` | cleared at `SectorMeshPreview.cpp:701` | Clear | Central map is passed/queried by bloom and door/dynamic shadow texture resolvers. |
| `generatedGeometry` | `SectorMeshPreview` | `BuildSectorGeneratedGeometry()` at `SectorMeshPreview.cpp:495` | reset at `SectorMeshPreview.cpp:667` | Clear | Also exposed to editor overlays through `RenderedGeometry()`. |
| Static sector meshes | `SectorMeshPreview` | `BuildSectorMeshes()` at `SectorMeshPreview.cpp:566` | `UnloadSectorMeshes(meshes)` at `SectorMeshPreview.cpp:700` | Clear | Static renderer logic remains in facade. |
| Main static sector material/shader | `SectorMeshPreview` | `LoadPreviewMaterial()` at `SectorMeshPreview.cpp:605-633` | reset/unload at `SectorMeshPreview.cpp:705-718` | Clear | Largest remaining GPU resource cluster outside helpers. |
| `defaultMaterialTexture` | `SectorMeshPreview` | captured in `LoadPreviewMaterial()` at `SectorMeshPreview.cpp:445` | reset at `SectorMeshPreview.cpp:705-712` | Clear | Passed to door renderer draw context as fallback at `SectorMeshPreview.cpp:865`. |
| Visibility graph/result/lookup world | `SectorMeshPreview` | `SectorMeshPreview.cpp:504-513`, updated at `982-993` | reset at `SectorMeshPreview.cpp:668-676` | Clear | Visibility debug, culling, and dynamic selection are intentionally coupled for now. |
| `SectorPreviewSkyRenderer` resources | `SectorPreviewSkyRenderer` | header members at `SectorPreviewSkyRenderer.h:26-33`; rebuild API at `17` | `Shutdown()` called at `SectorMeshPreview.cpp:698` | Clear | Renderer-only; no ECS or asset-scope ownership. |
| `SectorPreviewBloom` resources | `SectorPreviewBloom` | material/shaders/render targets at `SectorPreviewBloom.h:46-67`; ensure path in `.cpp` | `Shutdown()` called at `SectorMeshPreview.cpp:695` | Clear | It observes sector batches and texture handles; it owns postprocess targets. |
| `SectorPreviewBillboardRenderer` resources | `SectorPreviewBillboardRenderer` | shader/uniform/debug members at `SectorPreviewBillboardRenderer.h:37-67`; `Load()` API at `19` | `Shutdown()` called at `SectorMeshPreview.cpp:720` | Clear | Observes `engine::World`; does not own runtime object lifecycle. |
| `SectorPreviewDynamicLighting` resources | `SectorPreviewDynamicLighting` | CPU selection and shadow resources at `SectorPreviewDynamicLighting.h:143-157`; shadow resources loaded at `SectorMeshPreview.cpp:581-587` | `Reset()`, `UnloadShadowMaterial()`, `UnloadShadowMapResources()` at `SectorMeshPreview.cpp:677`, `696-697` | Clear | Still observes runtime door receiver bounds through optional `engine::World*`. |
| `SectorPreviewDoorRenderer` resources | `SectorPreviewDoorRenderer` | cache/shader/material/stats at `SectorPreviewDoorRenderer.h:156-165`; load at `SectorMeshPreview.cpp:599` | mesh/shader shutdown at `SectorMeshPreview.cpp:699`, `721` | Clear | Owns renderer GPU cache, not ECS lifecycle. |

## Extracted Helper Boundary Review

### SectorPreviewBillboardRenderer

Owns its cutout shader, billboard shader uniform locations, warning/debug state, and billboard draw counters (`SectorPreviewBillboardRenderer.h:17-68`). It observes `engine::AssetManager`, `engine::World`, the camera, and a `SectorPreviewBillboardDynamicLightContext` passed by the facade (`SectorPreviewBillboardRenderer.h:23-28`). It must not own dynamic light selection, shadow maps, sprite asset lifetimes, or runtime ECS lifecycle.

Boundary looks clean. The one deliberate dependency is that `SectorMeshPreview` builds the billboard dynamic light context from `SectorPreviewDynamicLighting` at `SectorMeshPreview.cpp:875-900`.

### SectorPreviewSkyRenderer

Owns sky texture handle reference, sky cylinder/top-cap meshes, material/default texture, yaw, top-cap color, and load state (`SectorPreviewSkyRenderer.h:15-34`). It observes `AssetManager` at draw time and the current camera. It must not own map texture requests, collision, picking, lightmaps, or topology lifetime.

Boundary looks clean and renderer-only. It remains correctly sequenced by `SectorMeshPreview::DrawScene()` before static sectors at `SectorMeshPreview.cpp:758-760`.

### SectorPreviewBloom

Owns bloom source material, blur/composite shaders, render targets, cached target sizes, and bloom-specific shader locations (`SectorPreviewBloom.h:20-68`). It observes initialized state, camera, static sector draw records, visibility, texture handles, and caller-owned scene target (`SectorPreviewBloom.h:23-30`).

Boundary looks clean. It does not own the scene render target or topology texture table. The helper intentionally has a local `TextureForId()` over the facade-provided table (`SectorPreviewBloom.h:42-44`).

### SectorPreviewDynamicLighting

Owns dynamic light sources/candidates/selected uniforms/IDs, receiver bounds, spotlight shadow caster selection, shadow matrices, shadow maps, shadow material/default texture, and shadow shader locations (`SectorPreviewDynamicLighting.h:105-157`). It observes topology during source rebuild, visibility/static receiver bounds during selection, optional `engine::World*` for door receiver bounds, and draw contexts for static sector batches and prepared door shadow casters.

Boundary is acceptable. It owns shadow maps/selection/upload/shadow pass, but not door mesh cache. Door caster mesh resolution is supplied by `SectorPreviewDoorRenderer::PrepareShadowRenderContext()` through the shadow render context (`SectorMeshPreview.cpp:911-917`; `SectorPreviewDoorRenderer.h:120-122`).

### SectorPreviewDoorRenderer

Owns door GPU mesh cache, prepared door shadow casters, door opaque shader/material/default texture, door debug mode, and draw stats (`SectorPreviewDoorRenderer.h:103-166`). It observes `engine::World`, door runtime components, `AssetManager`, topology/object-probe fallback data, dynamic lighting context, and a texture resolver (`SectorPreviewDoorRenderer.h:68-76`).

Boundary looks clean. It owns renderer GPU cache but not ECS lifecycle. The facade still supplies map texture lookup through `SectorMeshPreview::ResolveShadowCasterTexture()` at `SectorMeshPreview.cpp:1051-1060`, which is an acceptable bridge until asset/texture lookup is renamed or extracted.

## Render Order And State Review

Current centralized order:

1. `Render()` calls `RenderDynamicSpotLightShadowMaps()` then `DrawScene()` (`SectorMeshPreview.cpp:738-746`).
2. Shadow pass builds a `SectorPreviewDynamicSpotLightShadowRenderContext`, adds static sector draw records and facade texture resolver, lets the door renderer prepare door caster context, then delegates to dynamic lighting (`SectorMeshPreview.cpp:903-917`).
3. `DrawScene()` owns `BeginMode3D(camera)` / `EndMode3D()` (`SectorMeshPreview.cpp:758`, `872`).
4. Sky draws first (`SectorMeshPreview.cpp:759`).
5. Static sectors draw next, with lightmap/shadow textures bound into material map slots and dynamic light uniforms uploaded once before the batch loop (`SectorMeshPreview.cpp:761-850`).
6. Doors draw after static sectors when a runtime world is supplied (`SectorMeshPreview.cpp:852-867`).
7. Billboards draw after doors (`SectorMeshPreview.cpp:869-870`).
8. Bloom is outside the normal scene draw wrapper and is applied by editor/demo call sites through `ApplyEmissiveDecalBloomToScene()` (`SectorMeshPreview.cpp:925-934`; editor calls around `sources/sector_editor/SectorEditor.cpp:3766-3785`).
9. Editor overlays are outside the renderer; they use preview camera/geometry/visibility after scene draw (`sources/sector_editor/SectorEditor.cpp:3800-3968`).

Render state ownership is understandable but still sensitive:

- `SectorMeshPreview` owns the main 3D mode bracket.
- `SectorPreviewBloom` owns its own texture-mode and temporary 3D-mode brackets for bloom source rendering (`SectorPreviewBloom.cpp:256-331`, `425-465`).
- `SectorPreviewDynamicLighting` owns shadow-map texture mode and depth-state changes (`SectorPreviewDynamicLighting.cpp:460-531`).
- `SectorPreviewSkyRenderer` toggles depth mask around sky meshes (`SectorPreviewSkyRenderer.cpp:150-157`).
- Door and billboard renderers bracket color blend, backface culling, depth test, and depth mask changes and restore them afterward (`SectorPreviewDoorRenderer.cpp:499-658`; `SectorPreviewBillboardRenderer.cpp:484-626`).

Hidden ordering risks are low as long as the facade continues to sequence shadow maps before scene draw, sky before static sectors, doors before billboards, and bloom after the scene target has been rendered.

## Remaining Risks / Anti-Patterns

### Medium: Static sector material/shader path still lives in the facade

Evidence: `SectorMeshPreview.h:132-162` stores the main material, default texture, and static-sector shader locations; `SectorMeshPreview.cpp:373-446` loads them; `SectorMeshPreview.cpp:761-850` binds textures/uniforms and draws static batches.

Why it matters: this is the main remaining renderer implementation inside `SectorMeshPreview`. It is acceptable for now but would be the next extraction if the facade grows again.

Recommended action: defer `SectorStaticSceneRenderer` or similar until after the rename and any visual smoke.

### Medium: Visibility/debug and dynamic light update coupling remains centralized

Evidence: `UpdateVisibilityDebug()` computes visibility, updates dynamic light selection, and composes debug strings at `SectorMeshPreview.cpp:969-1019`.

Why it matters: this mixes culling, lighting selection, and UI-facing text. It is not currently blocking, but it is still a landfill remnant.

Recommended action: leave it until a visibility/debug or UI reporting task needs it.

### Low: Asset scope and texture table are centralized

Evidence: asset scope and map texture requests are owned by `SectorMeshPreview` at `SectorMeshPreview.cpp:515-535`; helpers receive texture handles/tables/resolvers (`SectorMeshPreview.cpp:863-865`, `927-934`).

Why it matters: this is an intentional ownership point, but it means helpers cannot be fully standalone renderers.

Recommended action: keep centralized until there is a real need for a shared sector texture resolver object.

### Low: Public facade still exposes optional `engine::World*` bridges

Evidence: render/shadow/visibility APIs accept optional runtime worlds at `SectorMeshPreview.h:49-61`, `84-89`.

Why it matters: this keeps editor/demo integration simple but blurs renderer vs runtime object boundaries.

Recommended action: acceptable short-term; do not rewrite before mechanical rename.

### Low: Preview terminology is now stale

Evidence: the main class and all helpers retain `Preview` names even though the extracted modules are renderer helpers (`SectorMeshPreview.h:33`, `SectorPreviewBillboardRenderer.h:17`, `SectorPreviewSkyRenderer.h:15`, `SectorPreviewBloom.h:20`, `SectorPreviewDynamicLighting.h:105`, `SectorPreviewDoorRenderer.h:103`).

Why it matters: the names now obscure the ownership model more than the code does.

Recommended action: perform a mechanical rename pass.

## Recommended Next Steps

1. Proceed to the final mechanical rename pass.
2. Do no required cleanup before rename beyond this audit/backlog update.
3. Defer static scene renderer extraction.
4. Defer full facade rewrite (`REF-037`) unless future work creates a concrete need.
5. Add a backlog item for the final renderer terminology rename.

It is safe to proceed to a mechanical rename pass after this audit. No major ownership leak was found. The helper boundaries are clean enough for the rename, provided the rename avoids behavior/resource/shader changes.

## Proposed Final Rename Scope

Candidate mapping:

- `SectorMeshPreview` -> `SectorMeshRenderer`
- `SectorPreviewBillboardRenderer` -> `SectorBillboardRenderer`
- `SectorPreviewSkyRenderer` -> `SectorSkyRenderer`
- `SectorPreviewBloom` -> `SectorBloomRenderer`
- `SectorPreviewDynamicLighting` -> `SectorDynamicLightingRenderer` or `SectorDynamicLighting`
- `SectorPreviewDoorRenderer` -> `SectorDoorRenderer`

The rename pass should include:

- source files and headers under `sources/sector_demo`
- includes in `sources/sector_demo`, `sources/sector_editor`, tests, and docs
- class/type/function names that contain the old terminology
- comments/docs/plans/audits/backlog references where useful
- CMake/source-list names only if file names are renamed
- tests only for include/type name updates

The rename pass should be mechanical only:

- no behavior changes
- no ownership moves
- no shader/resource changes
- no topology/cache changes
- no lightmap source-hash changes
- build/test after rename
- manual render smoke after rename if practical

Suggested naming choice: use `SectorDynamicLightingRenderer` only if keeping the renderer emphasis matters more than brevity. `SectorDynamicLighting` is also defensible because the helper owns CPU selection and shadow resources, not just draw calls.

## Backlog Update

`docs/plans/codebase_refactor_backlog.md` should mark REF-018 complete with this audit path and one-line conclusion. A new REF-039 item should track the mechanical renderer terminology rename. No rename item should be marked complete by this audit.

## Appendix: Evidence

Commands used:

```sh
rg -n "class SectorMeshPreview|struct SectorMeshPreview|SectorMeshPreview" sources/sector_demo sources/sector_editor tests docs
rg -n "SectorPreviewBillboardRenderer|SectorPreviewSkyRenderer|SectorPreviewBloom|SectorPreviewDynamicLighting|SectorPreviewDoorRenderer" sources docs
rg -n "Mesh|Material|Shader|RenderTexture|Unload|Load|Rebuild|Shutdown|Draw|Render|Visibility|Bloom|Sky|Door|Billboard|Dynamic|Light|Shadow|World|AssetScope|textureHandlesById|generatedGeometry|meshes" sources/sector_demo/SectorMeshPreview.h sources/sector_demo/SectorMeshPreview.cpp
rg -n "engine::World|ForEach|runtimeObjectWorld|SectorRuntimeObject|SectorDoor|SectorBillboard" sources/sector_demo/SectorMeshPreview.h sources/sector_demo/SectorMeshPreview.cpp sources/sector_demo/SectorPreviewBillboardRenderer.cpp sources/sector_demo/SectorPreviewBillboardRenderer.h sources/sector_demo/SectorPreviewSkyRenderer.cpp sources/sector_demo/SectorPreviewSkyRenderer.h sources/sector_demo/SectorPreviewBloom.cpp sources/sector_demo/SectorPreviewBloom.h sources/sector_demo/SectorPreviewDynamicLighting.cpp sources/sector_demo/SectorPreviewDynamicLighting.h sources/sector_demo/SectorPreviewDoorRenderer.cpp sources/sector_demo/SectorPreviewDoorRenderer.h
rg -n "BeginMode3D|EndMode3D|BeginTextureMode|EndTextureMode|rlDisable|rlEnable|SetShaderValue|SetShaderValueTexture|DrawMesh" sources/sector_demo/SectorMeshPreview.cpp sources/sector_demo/SectorPreviewBillboardRenderer.cpp sources/sector_demo/SectorPreviewSkyRenderer.cpp sources/sector_demo/SectorPreviewBloom.cpp sources/sector_demo/SectorPreviewDynamicLighting.cpp sources/sector_demo/SectorPreviewDoorRenderer.cpp
rg -n "preview\.|RenderDynamicSpotLightShadowMaps|DrawScene|ApplyEmissiveDecalBloom|UpdateVisibilityDebug|RebuildRendererResources|Rebuild\(" sources/sector_demo/SectorDemo.cpp sources/sector_editor
rg -n "LoadShaderFromMemory|LoadMaterialDefault|UnloadMaterial|UnloadShader|LoadRenderTexture|UnloadRenderTexture|UploadMesh|UnloadMesh" sources/sector_demo/SectorMeshPreview.cpp sources/sector_demo/SectorPreview*.cpp
wc -l sources/sector_demo/SectorMeshPreview.cpp sources/sector_demo/SectorPreviewBillboardRenderer.cpp sources/sector_demo/SectorPreviewSkyRenderer.cpp sources/sector_demo/SectorPreviewBloom.cpp sources/sector_demo/SectorPreviewDynamicLighting.cpp sources/sector_demo/SectorPreviewDoorRenderer.cpp
```

Selected grep results and line references:

- `SectorMeshPreview` public facade API and members: `sources/sector_demo/SectorMeshPreview.h:33-178`.
- Extracted helper members: `sources/sector_demo/SectorMeshPreview.h:163-167`.
- Rebuild path: `sources/sector_demo/SectorMeshPreview.cpp:476-657`.
- Shutdown path: `sources/sector_demo/SectorMeshPreview.cpp:665-729`.
- Main draw order: `sources/sector_demo/SectorMeshPreview.cpp:748-873`.
- Shadow facade/delegation: `sources/sector_demo/SectorMeshPreview.cpp:903-917`.
- Bloom facade/delegation: `sources/sector_demo/SectorMeshPreview.cpp:925-934`.
- Visibility/debug composition: `sources/sector_demo/SectorMeshPreview.cpp:969-1019`.
- Texture resolver bridge: `sources/sector_demo/SectorMeshPreview.cpp:1041-1060`.
- Billboard helper API/resource members: `sources/sector_demo/SectorPreviewBillboardRenderer.h:17-68`.
- Sky helper API/resource members: `sources/sector_demo/SectorPreviewSkyRenderer.h:15-34`.
- Bloom helper API/resource members: `sources/sector_demo/SectorPreviewBloom.h:20-68`.
- Dynamic lighting helper API/resource members: `sources/sector_demo/SectorPreviewDynamicLighting.h:105-157`.
- Door renderer helper API/resource members: `sources/sector_demo/SectorPreviewDoorRenderer.h:103-166`.
- Editor calls split shadow, scene, overlays, and bloom: `sources/sector_editor/SectorEditor.cpp:3766-3785`; overlay use starts around `sources/sector_editor/SectorEditor.cpp:3800`.
- Demo call sites use the facade: `sources/sector_demo/SectorDemo.cpp:36-44`, `100-141`.

Line count snapshot:

```text
1077 sources/sector_demo/SectorMeshPreview.cpp
 641 sources/sector_demo/SectorPreviewBillboardRenderer.cpp
 160 sources/sector_demo/SectorPreviewSkyRenderer.cpp
 480 sources/sector_demo/SectorPreviewBloom.cpp
 561 sources/sector_demo/SectorPreviewDynamicLighting.cpp
 754 sources/sector_demo/SectorPreviewDoorRenderer.cpp
3673 total
```

Static-analysis limitation: regex results confirm ownership and call boundaries, but they do not prove visual correctness, GPU state parity, or all driver-specific render-state restoration behavior. Manual render smoke is still recommended after the rename or before depending on risk tolerance.
