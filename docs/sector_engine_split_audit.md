# Sector Engine Split Audit

## 1. Executive Summary

`SectorEditor` currently owns too much of the 3D preview and FPS/gameplay-preview orchestration. The reusable sector backend is already partly present, but the editor still decides when to build preview meshes, when to rebuild collision, how to collect gameplay input, how to apply player pose to the preview camera, how to manage visual camera effects, how to run/install lightmap bakes, and how to surface status/UI feedback.

Already reusable pieces include the topology v2 data model and topology helpers, generated geometry, mesh batch building, topology-derived collision queries, most FPS movement math/state helpers, sky mesh data generation, lightmap layout/bake/hash code, and the freefly-oriented `SectorMeshPreview` renderer. These live mostly under `sources/sector_demo/` despite being broader than a demo.

Editor-specific pieces include document dirty/cache policy, 2D tools and selection, inspectors, modals, texture picker flows, 3D surface selection/editing UI, save/load confirmations, editor status text, and async lightmap install workflow.

A future `SectorEngine` split looks feasible, but the first useful work should be small and behavior-preserving. The biggest risks/goblins are `SectorEditorState` mixing editor and runtime-preview state, FPS helpers depending on `SectorMeshPreviewPose`, editor-owned asset scopes and rebuild timing, visual camera effects living beside collision/physics state, and lightmap hash/persistence rules being easy to disturb.

## 2. Current Responsibility Inventory

Editor UI/tools/document state:

- Key files: `SectorEditor.cpp`, `SectorEditor.h`, `SectorEditorTypes.h`, `SectorEditorDocumentActions.*`, `SectorEditorTopologyActions.*`, `SectorEditorMaterialActions.*`, inspector/modal helper files.
- Key types/functions: `SectorEditor`, `SectorEditorState`, `SectorEditorUiState`, `Update`, `Render`, `RenderUI`, `HandleCanvasInput`, `DrawToolsPanel`, `DrawSectorsPanel`, `MarkTopologyDocumentEdited`, `InvalidateTopologyRenderCache`, `ResetEditorTopologyDocumentState`, `PrepareSaveLevelPlan`, `SaveSectorTopologyDocument`.

Topology data/model/editing:

- Key files: `SectorTopologyMap.*`, `SectorTopologyTypes.h`, `SectorTopologyCreation.*`, `SectorTopologyEdit.*`, `SectorTopologyGeometry.*`, `SectorTopologyValidation.cpp`, `SectorTopologySerialization.*`, `SectorTopologyUnits.*`.
- Key types/functions: `SectorTopologyMap`, `SectorTopologyVertex`, `SectorTopologyLineDef`, `SectorTopologySideDef`, `SectorTopologySector`, `SectorTopologyStaticPointLight`, `BuildSectorTopologyIndexes`, `ValidateSectorTopologyMap`, `ExtractSectorTopologyLoops`, `CreateSectorTopologyPolygon`, `InsertSectorTopologyPolygon`, `MoveSectorTopologyVertex`, `SplitSectorTopologyLineDef`, `CutSectorTopologySectorBetweenBoundaryPoints`, `JoinSectorTopologySectors`, `LoadSectorTopologyMap`, `SaveSectorTopologyMap`.

Generated geometry / meshing:

- Key files: `SectorGeneratedGeometry.*`, `SectorMeshBuilder.*`, `SectorMeshTypes.h`.
- Key types/functions: `SectorGeneratedGeometry`, `SectorGeneratedSurface`, `SectorGeneratedSurfaceRef`, `BuildSectorGeneratedGeometry`, `PickSectorGeneratedGeometry`, `BuildSectorMeshBatchData`, `BuildSectorMeshes`, `UnloadSectorMeshes`.

3D preview rendering:

- Key files: `SectorMeshPreview.*`, plus editor call sites in `SectorEditor.cpp`.
- Key types/functions: `SectorMeshPreview`, `SectorMeshPreviewPose`, `Rebuild`, `Shutdown`, `Enter`, `Leave`, `Update`, `Render`, `ApplyEmissiveDecalBloom`, `Pose`, `ApplyPose`, `SetMouseLookEnabled`, `GeneratedGeometry`.
- Editor-owned preview functions include `TryEnterPreview3D`, `LeavePreview3D`, `UpdatePreview3D`, `RenderPreview3DScene`, `RenderPreview3DOverlays`, `PickSectorSurface3D`, `DrawPreviewSurfaceHighlights`, `RebuildPreviewMeshesPreservingView`.

Sky/render resources:

- Key files: `SectorSkyCylinder.*`, `SectorMeshPreview.*`, `SectorTopologyMap.h`.
- Key types/functions: `SectorTopologySkySettings`, `BuildSkyCylinderMeshData`, `BuildSkyCylinderTopCapMeshData`, `ShouldRenderSkyCylinder`, `NormalizeSectorTopologySkySettings`, `SectorMeshPreview::DrawSkyCylinder`.

Lightmaps/baking:

- Key files: `SectorLightmap.*`, `SectorLightmapTypes.h`, `SectorEditorLightmapModal.*`, lightmap sections in `SectorEditor.cpp`.
- Key types/functions: `SectorLightmapLayout`, `SectorLightmapBakeResult`, `SectorTopologyLightmapBakeInput`, `BuildSectorLightmapLayout`, `BakeSectorLightmap`, `ComputeSectorLightmapSourceHash`, `GetSectorLightmapStatus`, `StartLightmapBake`, `PollLightmapBakeResult`, `ConsumeLightmapBakeResult`, `InstallLightmapBakeResult`, `LightmapBakeAsyncState`.

Collision world:

- Key files: `SectorCollisionWorld.*`.
- Key types/functions: `SectorCollisionWorld`, `SectorCollisionSector`, `SectorCollisionEdge`, `SectorCollisionMoveState`, `SectorCollisionMoveConfig`, `SectorCollisionMoveResult`, `BuildFromTopology`, `FindSectorContainingPointPreferCurrent`, `GetSectorFloorCeiling`, `ResolveMovement`.

FPS/freefly/gameplay-preview controller logic:

- Key files: `SectorFpsController.*`, `SectorMeshPreview.*`, `SectorEditorPreviewActions.*`, `SectorEditor.cpp`.
- Key types/functions: `SectorFpsControllerConfig`, `SectorFpsControllerState`, `SectorFpsControllerInput`, `SectorFpsVerticalContext`, `SectorFpsVerticalResult`, `SectorFpsHeadBobState`, `SectorFpsLandingDipState`, `UpdateSectorFpsMouseLook`, `ComputeSectorFpsHorizontalMovementDelta`, `TryStartSectorFpsJump`, `UpdateSectorFpsVerticalPhysics`, `ApplySectorFpsVisualStepSmoothing`, `UpdateSectorFpsHeadBob`, `UpdateSectorFpsLandingDip`.
- Freefly is owned by `SectorMeshPreview::Update`; gameplay-preview orchestration is owned by `SectorEditor::UpdatePreview3D`.

Input ownership:

- `engine::Input` is passed into `SectorEditor::Update`, `SectorEditor::RenderUI`, and `SectorMeshPreview::Update`.
- Editor UI consumes events before preview/gameplay paths where relevant.
- Gameplay-preview input is collected directly in `SectorEditor::UpdatePreview3D` from keys, mouse delta, modal state, and `uiState.keyboardCaptured`.

Asset/texture ownership:

- `AssetManager` owns GPU resources.
- `SectorMeshPreview` owns its preview asset scope and texture handles for map textures, lightmap texture, sky texture, materials, shaders, meshes, and bloom render textures.
- `SectorEditorState` owns editor texture scope and editor texture handles for 2D/editor UI texture previews.
- Texture definitions live in `SectorTopologyMap::texturesById`.

Persistence/settings:

- Key files: `SectorTopologySerialization.*`, `SectorEditorDocumentActions.*`, `SectorEditor.cpp`.
- `SectorTopologyMap` stores topology, textures, static lights, preview settings, sky settings, directional light, lightmap settings, and baked lightmap metadata.
- Editor document helpers own level path validation/scanning/plans and save/load modal state.

Sector demo/runtime files:

- `SectorDemo.*` loads a topology document and uses `SectorMeshPreview` in freefly mode.
- It does not use `SectorCollisionWorld`, `SectorFpsController`, gameplay-preview physics, editor selection, or lightmap bake orchestration.

## 3. Current Dependency Direction

This section distinguishes direct dependencies from transitive or conceptual dependencies. A direct dependency means an include, member field, parameter type, or direct call. A transitive dependency means a dependency reached through a directly used module. A conceptual dependency means the responsibilities are related, but no direct code dependency is implied.

```text
SectorEditor / SectorEditor.cpp / SectorEditor.h / SectorEditorTypes.h
  directly uses -> SectorEditorPreviewActions and other sector_editor helpers
  directly uses -> SectorMeshPreview
  directly uses -> SectorCollisionWorld
  directly uses -> SectorFpsController
  directly uses -> SectorLightmap
  directly uses -> SectorGeneratedGeometry
  directly uses -> SectorTopologyMap / creation / edit / geometry / serialization / units
  directly uses -> engine::Input, engine::UI, engine::AssetManager

SectorEditorPreviewActions
  directly uses -> SectorEditorTypes
  directly uses -> SectorMeshPreview
  directly uses -> SectorFpsController
  directly uses -> SectorCollisionWorld conceptually through SectorEditorState fields and method calls on state.sectorCollisionWorld
  directly uses -> SectorTopologyMap conceptually through SectorEditorState.topologyMap

SectorMeshPreview
  directly uses -> AssetManager, engine input, SectorGeneratedGeometry, SectorMeshBuilder, SectorLightmap, SectorSkyCylinder, SectorTextureTypes, SectorTopologyMap, SectorUnits
  does not directly use -> SectorEditor types

SectorCollisionWorld
  directly uses -> SectorTopologyMap, SectorTopologyTypes, SectorTopologyUnits, SectorUnits
  does not directly use -> SectorEditor or generated render meshes

SectorFpsController
  directly uses -> SectorMeshPreviewPose through SectorMeshPreview.h
  directly uses -> SectorTopologyMap for SectorPreviewSettings conversion
  does not directly use -> SectorEditor

SectorLightmap
  directly uses -> SectorTopologyMap, SectorGeneratedGeometry, SectorUnits
  does not directly use -> SectorEditor

SectorDemo
  directly uses -> SectorMeshPreview
  directly uses -> SectorTopologyMap / SectorTopologySerialization
  does not directly use -> SectorEditor, SectorCollisionWorld, or SectorFpsController
```

`SectorMeshPreview`, `SectorCollisionWorld`, generated geometry, mesh building, and lightmaps are already editor-independent in the direct dependency sense. They are still conceptually entangled with editor preview because `SectorEditor` is the main caller and owns when/how those services are built, refreshed, displayed, or installed.

Suspicious dependency direction:

- `SectorFpsController.h` includes `SectorMeshPreview.h` only to use `SectorMeshPreviewPose`. That makes movement/controller helpers depend directly on preview-renderer pose naming.
- `SectorEditorTypes.h` includes many reusable backend headers and stores their state directly inside `SectorEditorState`, which makes editor helper modules include a large mixed state object.
- `SectorEditorPreviewActions.*` looks like a split-out helper, but it still takes `SectorEditorState&`, so it is editor-bound glue rather than reusable runtime code.

## 4. 3D Preview / FPS Gameplay-Preview Flow

Entering 3D preview:

- `SectorEditor::TryEnterPreview3D` cancels pending 2D tools, clears UI active/focus IDs, and clears keyboard capture.
- It calls `preview.Rebuild(assets, state.topologyMap, "sector_editor_preview", error)`.
- `SectorMeshPreview::Rebuild` shuts down old preview resources, builds `SectorGeneratedGeometry`, creates an asset scope, requests map textures, optionally builds sky cylinder meshes, checks lightmap status/layout, requests the lightmap texture, builds render meshes, loads preview material/shaders, initializes camera pose, and calls `Enter`.
- The editor restores `state.lastPreviewPose` when present, resets 3D selection/UI state, sets `state.mode = Preview3D`, starts in `FreeFly`, and calls `RebuildSectorCollisionWorld`.

Leaving 3D preview:

- `SectorEditor::LeavePreview3D` applies the gameplay visual pose back into `SectorMeshPreview` when leaving from gameplay mode.
- It stores `state.lastPreviewPose = ActivePreviewPose()`, switches back to `Edit2D`, clears hover/settings state, calls `preview.Leave()`, and updates status text.

Freefly update path:

- `SectorEditor::UpdatePreview3D` handles F1/F2/F3/Tab/Escape preview hotkeys.
- In `FreeFly`, it delegates movement and mouse look to `SectorMeshPreview::Update`.
- `SectorMeshPreview::Update` owns F11 cursor toggle, mouse yaw/pitch, WASD movement, Space/Ctrl vertical movement, and camera update.

Gameplay-preview update path:

- `SectorEditor::UpdatePreview3D` collects `SectorFpsControllerInput` from WASD, Shift, Space, mouse delta, mouse-look state, modal state, and `uiState.keyboardCaptured`.
- It calls `UpdateSectorFpsMouseLook` and `ComputeSectorFpsHorizontalMovementDelta`.
- If `state.sectorCollisionWorldValid`, it calls `SectorCollisionWorld::ResolveMovement` using `SectorCollisionMoveState` and `SectorCollisionMoveConfig`.
- It applies extra grounded step blocking checks around sector transitions, writes resolved X/Z/current-sector back to `state.fpsControllerState`, then calls `RefreshGameplaySectorAndVerticalContext`.
- It handles jumping with `TryStartSectorFpsJump`, vertical physics with `UpdateSectorFpsVerticalPhysics`, visual step smoothing with `ApplySectorFpsVisualStepSmoothing`, landing dip with `UpdateSectorFpsLandingDip`, and headbob with `UpdateSectorFpsHeadBob`.
- Finally it calls `ApplyGameplayPoseToPreview`, which delegates to `ApplySectorEditorGameplayPoseToPreview`.

Pose ownership:

- Freefly pose lives inside `SectorMeshPreview` as `position`, `yawRadians`, `pitchRadians`, and `Camera3D`.
- Gameplay physical pose lives in `state.fpsControllerState`, especially `feetPosition`, `yawRadians`, `pitchRadians`, `currentSectorId`, `grounded`, and `verticalVelocity`.
- Gameplay visual pose is derived from `SectorFpsControllerVisualPose` using `visualStepOffsetY`, `headBobState.offset`, and `landingDipState.offsetY`, then applied to `SectorMeshPreview`.

Collision world build/refresh:

- `SectorEditor::RebuildSectorCollisionWorld` delegates to `RebuildSectorEditorCollisionWorld`.
- `RebuildSectorEditorCollisionWorld` calls `state.sectorCollisionWorld.BuildFromTopology(state.topologyMap, &error)`, updates validity/warning state, and refreshes gameplay vertical context when in gameplay mode.
- `RebuildPreviewMeshesPreservingView` also calls `RebuildSectorCollisionWorld` after rebuilding preview meshes.

Player body/feet/eye/camera state:

- Feet/body state: `SectorFpsControllerState::feetPosition`, `playerRadius`, `playerHeight`, `stepHeight`.
- Eye state: `SectorFpsControllerEyePosition` and `SectorFpsControllerPose`.
- Camera state: `SectorMeshPreview::camera`, updated by `SectorMeshPreview::ApplyPose`/`UpdateCamera`.
- Visual-only offsets: `state.visualStepOffsetY`, `state.headBobState`, `state.landingDipState`.

Portal blocking and sector lookup:

- Portal and wall classification happens in `SectorCollisionWorld::BuildFromTopology`.
- `lineDef.flags.blocksPlayer` is copied into `SectorCollisionEdge::blocksPlayer`.
- Runtime movement uses `FindSectorContainingPointPreferCurrent`, `ResolveMovement`, `GetSectorFloorCeiling`, and current-sector repair if the previous sector is invalid.

What `SectorFpsController` owns:

- Movement config/state/input structs.
- Mouse look math, horizontal desired movement, jump start, vertical physics, step transition classification, visual step offset helpers, headbob helpers, landing dip helpers, preview-settings conversion helpers.

What `SectorEditor` still owns:

- Input collection, UI/modal gating, collision-world validity/fallback policy, horizontal movement application, no-clip fallback, current-sector refresh timing, pose application to preview, status/debug overlay text, preview mode switching, and rebuild timing.

What `SectorMeshPreview` owns:

- Render resources, generated geometry storage, mesh batches, lightmap texture handle, sky resources, bloom resources, freefly input/camera, mouse cursor state, camera object, and asset progress/status reporting.

What `SectorEditor` still owns around mesh preview:

- Document-to-preview rebuild triggers, preserving view/selection over rebuilds, 3D surface picking conversion to editor surface refs, overlay highlighting, material edit panels, and applying preview bloom in the editor render path.

## 5. Reusable vs Editor-Only Code

Likely reusable / candidate `SectorEngine` pieces:

- `SectorTopologyMap`, topology types, validation, loop extraction, topology creation/edit helpers, and serialization.
- `SectorGeneratedGeometry` and `PickSectorGeneratedGeometry`.
- `SectorMeshBuilder` batch/mesh generation, though the GPU mesh output is raylib-specific.
- `SectorCollisionWorld` and movement collision queries.
- Most of `SectorFpsController`, except the direct `SectorMeshPreviewPose` coupling should be revisited before treating it as a general runtime API.
- `SectorLightmap` layout, bake, status, and source-hash logic.
- `SectorSkyCylinder` mesh-data helpers and sky settings normalization.
- Parts of `SectorMeshPreview` that are not editor-specific, especially rendering generated sector meshes with textures, lightmaps, sky, and emissive decal bloom.
- `SectorDemo` only as proof that `SectorMeshPreview` can be used without `SectorEditor`.

Editor-only pieces:

- `SectorEditorState` document dirty/cache policy and editor mode/tool state.
- `SectorEditorTopologyRenderCache` and 2D cached editor drawing.
- Selection, hover, stale-selection cleanup, inspectors, and modals.
- Texture picker/add-texture modal and editor texture preview scope.
- 2D pending tool state machines for sector draw, insert, move, merge, split, cut, erase, and light drag.
- 3D surface selection/material editing as currently implemented, because it maps generated surface hits into editor `TopologySurfaceEditTarget` and UI state.
- UI input capture, editor hotkeys, status text, save/load confirmations, and document modal flows.
- Async lightmap bake modal/progress/install workflow, even though the bake itself is backend code.
- Editor preview settings modal and apply flow.

Mixed/unclear pieces:

- `SectorMeshPreview` is editor-independent by direct dependency, but its name, freefly input, cursor ownership, asset scope ownership, and pose type make it more of a preview renderer than a clean runtime render-world.
- `SectorFpsController` is mostly reusable logic, but its pose helpers depend on `SectorMeshPreviewPose`.
- `SectorEditorPreviewActions` is extracted from `SectorEditor.cpp`, but because it accepts `SectorEditorState&`, it remains editor glue.
- Preview settings live in `SectorTopologyMap`; they are persisted with the map and used by editor gameplay preview, but may not be the right long-term home for future game-specific player tuning.

## 6. Candidate `SectorEngine` Boundary

A future reusable sector engine layer could own:

- Sector world data passed in as `SectorTopologyMap` or loaded through existing serialization helpers.
- Generated render geometry from topology.
- Render mesh batch data and possibly a raylib render resource wrapper if kept explicitly renderer-facing.
- `SectorCollisionWorld` and movement/collision query helpers.
- Player/FPS movement state and update helpers once pose/render coupling is reduced.
- Common authored-height to runtime/world-unit conversion through existing unit helpers.
- Runtime lightmap metadata/status and render application.
- Sky settings/data needed for rendering, while preserving that sky visuals are visual-only.

It should probably not own:

- Editor UI, tool state, selection, inspectors, modals, save/load confirmations, status text, or document dirty state.
- 2D topology render cache invalidation policy.
- Editor-only 3D surface selection/editing UI.
- Texture picker flows and editor texture preview scope.
- Lightmap bake modal/progress UI and editor install confirmation flow.
- Broad abstract interfaces or a speculative gameplay framework.

This boundary is an audit recommendation, not an implementation plan. The current code supports parts of it, but not a clean cut yet.

## 7. Possible Future API Shape

Minimal possible runtime build/update shape:

```cpp
struct SectorWorldRuntime;
struct SectorWorldRuntimeBuildInput {
    const SectorTopologyMap* map = nullptr;
    engine::AssetManager* assets = nullptr;
    const char* assetScopeName = nullptr;
};

bool BuildSectorWorldRuntime(
        SectorWorldRuntime& runtime,
        const SectorWorldRuntimeBuildInput& input,
        std::string& error);
void ShutdownSectorWorldRuntime(SectorWorldRuntime& runtime, engine::AssetManager& assets);
```

Minimal possible player shape:

```cpp
struct SectorPlayerState;
struct SectorPlayerInput;
struct SectorPlayerSettings;
struct SectorPlayerUpdateResult;

SectorPlayerUpdateResult UpdateSectorPlayer(
        SectorPlayerState& player,
        const SectorPlayerSettings& settings,
        const SectorPlayerInput& input,
        const SectorCollisionWorld& collision,
        float dt);
```

Minimal possible render shape:

```cpp
void RenderSectorWorld(
        const SectorWorldRuntime& runtime,
        engine::AssetManager& assets,
        const Camera3D& camera);
```

Current code does not support this cleanly yet because:

- `SectorMeshPreview` owns camera/freefly input and render resources together.
- `SectorFpsController` pose helpers use `SectorMeshPreviewPose`.
- `SectorEditor` owns collision/player update orchestration rather than calling one reusable player update function.
- Asset scopes and texture request timing are owned inside `SectorMeshPreview`.
- Editor 3D picking and material editing rely on `preview.GeneratedGeometry()`.

## 8. Entanglement / Risk Audit

`SectorEditor` owns too much preview gameplay state:

- Where: `SectorEditorState` fields for `fpsControllerConfig`, `fpsControllerState`, `sectorCollisionWorld`, `previewVerticalResult`, `previewMoveResult`, `visualStepOffsetY`, `headBobState`, `landingDipState`; `SectorEditor::UpdatePreview3D`.
- Why it matters: a future game would need similar movement/collision orchestration without editor UI/modal/status assumptions.

Editor-only UI/status assumptions around reusable-looking flows:

- Where: `TryEnterPreview3D`, `LeavePreview3D`, `ApplyPreviewSettingsModal`, `StartLightmapBake`, `InstallLightmapBakeResult`.
- Why it matters: runtime code should not inherit editor status text, modal state, dirty flags, or confirmation flows.

Direct access to `SectorEditorState` in helper modules:

- Where: `SectorEditorPreviewActions.*`, `SectorEditorDocumentActions.*`, `SectorEditorMaterialActions.*`, modal and inspector helpers.
- Why it matters: these helpers reduce file size, but they do not create reusable boundaries.

Asset scope ownership coupling:

- Where: `SectorMeshPreview::Rebuild` creates an asset scope and requests textures/lightmaps; `SectorEditorState` also has `editorTextureScope`.
- Why it matters: future runtime/editor sharing needs a clear policy for who owns map texture scopes and when GPU uploads/unloads happen.

Collision/preview rebuild timing:

- Where: `TryEnterPreview3D`, `RebuildPreviewMeshesPreservingView`, material edit completion, preview settings apply, lightmap install.
- Why it matters: behavior depends on rebuilding render meshes and collision together at the right moments.

Visual camera offsets versus collision/physics state:

- Where: `visualStepOffsetY`, `SectorFpsHeadBobState`, `SectorFpsLandingDipState`, `ApplySectorFpsVisualStepSmoothing`, `SectorFpsControllerVisualPose`.
- Why it matters: project rules require visual effects to stay out of collision, sector lookup, and physics.

Map authoring units versus runtime world units:

- Where: `SectorUnits.h`, `SectorTopologyUnits.*`, generated geometry, collision, lightmap hash, static-light conversion.
- Why it matters: a split must preserve boundary conversions and avoid mixing authored heights with runtime world units.

Lightmap source hash/persistence concerns:

- Where: `ComputeSectorLightmapSourceHash`, `GetSectorLightmapStatus`, `InstallLightmapBakeResult`, `ApplyPreviewSettingsModal`.
- Why it matters: `ceilingSky`, directional light, static lights, geometry, textures, and bake settings affect the hash; preview and sky visual settings should not accidentally become hash inputs.

Cyclic dependency risks:

- Where: current backend modules avoid including `SectorEditor`, but `SectorFpsController` includes `SectorMeshPreview`.
- Why it matters: introducing `SectorEngine` carelessly could create editor/runtime/render cycles.

Premature abstraction risks:

- Where: the current code is concrete and data-oriented.
- Why it matters: a grand framework would add churn before the proven seams are small enough to extract safely.

## 9. Recommended Refactor Strategy

Phase 1: clarify runtime-player/FPS preview state boundary.

- Goal: group the gameplay-preview movement inputs/state/results behind a small free-function boundary without changing behavior.
- Why it helps: reduces `SectorEditor::UpdatePreview3D` responsibility and tests the actual player/collision seam.
- Risks: movement feel, jumping, step smoothing, landing dip, no-clip fallback, and sector lookup can regress easily.
- Non-goals: do not rewrite movement, change collision, add a game framework, or move preview rendering.
- Rough files: `SectorEditor.cpp`, `SectorEditorPreviewActions.*`, `SectorFpsController.*`, maybe tests around FPS/collision.
- Behavior: unchanged.

Phase 2: separate editor-only 3D surface picking from reusable generated geometry picking.

- Goal: keep `PickSectorGeneratedGeometry` reusable and isolate conversion to `SectorSurfaceRef`/`TopologySurfaceEditTarget` as editor-only.
- Why it helps: prevents runtime rendering from depending on editor material selection concepts.
- Risks: 3D material selection/highlighting must remain consistent with drawn geometry.
- Non-goals: do not change generated geometry or surface metadata.
- Rough files: `SectorEditor.cpp`, `SectorEditorMaterialActions.*`, `SectorGeneratedGeometry.*` only if a tiny neutral helper is needed.
- Behavior: unchanged.

Phase 3: clarify `SectorMeshPreview` responsibilities versus a reusable render-world object.

- Goal: identify whether freefly input/cursor ownership can remain preview-only while mesh/lightmap/sky render resources become reusable.
- Why it helps: future runtime code may want rendering without editor/freefly camera behavior.
- Risks: asset scope lifetime, texture fallback behavior, sky fallback, bloom resources, lightmap status.
- Non-goals: do not create `SectorEngine` folders or new build targets yet.
- Rough files: `SectorMeshPreview.*`, `SectorDemo.*`, `SectorEditor.cpp`.
- Behavior: unchanged.

Phase 4: create a runtime collision/player update wrapper only after the current flow is understood.

- Goal: wrap current editor gameplay-preview movement order into a reusable function that receives explicit state/input/collision/config.
- Why it helps: gives a future `SectorGame` a concrete player update path.
- Risks: subtle order dependencies around horizontal movement before vertical physics, step blocking, current-sector repair, and visual effects.
- Non-goals: do not introduce ECS entities, command systems, or dynamic runtime lights.
- Rough files: `SectorFpsController.*`, `SectorCollisionWorld.*`, `SectorEditorPreviewActions.*`, tests.
- Behavior: unchanged.

Phase 5: consider module/folder naming after seams are proven.

- Goal: decide whether `sources/sector_demo/` should be renamed or split only after code boundaries are already clean.
- Why it helps: avoids churn and cyclic dependencies.
- Risks: broad include/build churn.
- Non-goals: do not move all preview/editor code in one pass.
- Rough files: build files and selected sector backend files, only in a dedicated cleanup task.
- Behavior: unchanged.

## 10. Do Not Do Yet

- Do not create a grand `SectorEngine` framework before concrete seams exist.
- Do not move the entire 3D preview in one pass.
- Do not rewrite FPS movement.
- Do not change collision, physics, camera feel, step smoothing, headbob, landing dip, jumping, falling, or portal blocking.
- Do not change topology schema or reintroduce old polygon `SectorMap`.
- Do not change generated geometry or mesh output.
- Do not change lightmap hash policy, bake behavior, or persistence format.
- Do not make sky visual settings lightmap hash inputs.
- Do not introduce cyclic dependencies between editor, runtime, rendering, and backend modules.
- Do not create broad abstract interfaces just in case.
- Do not add virtual tool hierarchies, command systems, MVC/MVVM rewrites, or OOP-heavy runtime/game object structures.
- Do not move save/load confirmation flows or document dirty/cache policy into a runtime layer.

## 11. Open Questions

- Should editor preview and a future game use the exact same renderer object, or should `SectorMeshPreview` remain an editor/demo wrapper around lower-level render resources?
- Should editor-only surface picking stay separate from runtime rendering, with only generated-geometry ray hits shared?
- Should FPS controller own headbob/landing dip/step smoothing as visual outputs, or should those remain preview/camera effects outside physical movement state?
- How should map texture asset scope ownership work between editor preview, future runtime, and temporary editor texture previews?
- What is the minimum runtime API a future `SectorGame` needs: render only, collision only, player movement, lightmap application, or full world build/shutdown?
- Should persisted `SectorPreviewSettings` remain in `SectorTopologyMap` for game use, or stay an editor-preview convenience?
- Should `SectorFpsController` stop depending directly on `SectorMeshPreviewPose` before any runtime boundary is attempted?
- Should lightmap baking remain editor-only while lightmap status/application stays reusable?
