#include "game/SectorGameSession.h"

#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorStaticModelLightmap.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace game {

namespace {

Vector3 GameplayForward(const SectorEditorPreviewControllerState& controller)
{
    const SectorViewPose pose = SectorFpsControllerVisualPose(
            controller.fpsControllerState,
            controller.fpsControllerConfig,
            controller.visualStepOffsetY,
            controller.headBobState.offset,
            controller.landingDipState.offsetY);
    return Vector3{
            std::cos(pose.yawRadians),
            0.0f,
            std::sin(pose.yawRadians)};
}

void UpdateAudioListener(
        engine::AudioSystem& audio,
        const Camera3D& camera)
{
    audio.SetListener(engine::AudioListener{
            camera.position,
            Vector3Subtract(camera.target, camera.position),
            camera.up});
}

bool ScopeFinishedOrEmpty(
        const engine::AssetManager& assets,
        engine::AssetScopeHandle scope)
{
    return engine::IsNull(scope) || assets.IsScopeFinished(scope);
}

bool FpsViewmodelLoadFinished(const FpsViewmodelRuntimeState& state)
{
    if (state.loadState == FpsViewmodelLoadState::Failed
            || state.loadState == FpsViewmodelLoadState::Inactive) {
        return true;
    }
    return state.loadState == FpsViewmodelLoadState::Ready
            && state.attachment.loadState
                    != FpsViewmodelAttachmentLoadState::Pending;
}

bool HasRuntimeNpcs(const SectorSceneRuntime& scene)
{
    return std::any_of(
            scene.NpcNavigation().records.begin(),
            scene.NpcNavigation().records.end(),
            [](const NpcNavigationRecord& record) {
                return record.occupied;
            });
}

bool NavigationBuildTerminal(SectorNavigationState state)
{
    return state == SectorNavigationState::Ready
            || state == SectorNavigationState::Empty
            || state == SectorNavigationState::Failed
            || state == SectorNavigationState::Uninitialized
            || state == SectorNavigationState::Stale;
}

float NavigationLoadProgress(const SectorNavigationWorld& navigation)
{
    const SectorNavigationState state = navigation.State();
    if (NavigationBuildTerminal(state)) return 1.0f;
    if (state != SectorNavigationState::Building) return 0.0f;
    const SectorNavigationBuildStatistics& statistics =
            navigation.BuildStatistics();
    if (statistics.tileCoordinateCount <= 0) return 0.0f;
    return std::clamp(
            static_cast<float>(statistics.builtTileCoordinateCount)
                    / static_cast<float>(statistics.tileCoordinateCount),
            0.0f,
            1.0f);
}

bool InitialNavigationObstaclesSettled(
        const SectorNavigationWorld& navigation)
{
    if (navigation.State() != SectorNavigationState::Ready) return true;
    const SectorNavigationDynamicObstacleStatistics& statistics =
            navigation.DynamicObstacleStatistics();
    return statistics.pendingCount == 0
            && statistics.removingCount == 0
            && statistics.backlogCount == 0;
}

std::string NavigationLoadFailure(const SectorNavigationWorld& navigation)
{
    const std::vector<SectorNavigationDiagnostic>& diagnostics =
            navigation.Diagnostics();
    for (auto it = diagnostics.rbegin(); it != diagnostics.rend(); ++it) {
        if (it->severity == SectorNavigationDiagnosticSeverity::Error
                && !it->message.empty()) {
            return it->message;
        }
    }
    return std::string{"navigation finished "}
            + SectorNavigationStateName(navigation.State());
}

} // namespace

bool SectorGameSession::StartNew(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        const SectorLevelEntryRequest& entry,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        PlayerAudioRuntime& playerAudioRuntime,
        engine::PersistentScriptStore& persistentStore,
        bool loadingSave,
        std::string& error)
{
    failureError.clear();
    playerHealth = MakeHealth(100);
    playerStamina = MakePlayerStamina(settings.playerStamina);
    ClearPlayerWindedCamera(windedCamera);
    breathingAudio = PlayerBreathingAudioRuntime{};
    const std::string& requestedLevelName = entry.levelName;
    const std::string path = ApplicationLevelAssetPath(requestedLevelName);
    if (path.empty()) {
        error = "Invalid first level name '" + requestedLevelName + "'";
        return false;
    }

    SectorTopologyMap loaded;
    if (!LoadSectorRuntimeLevel(path, loaded, error)) {
        return false;
    }
    const SectorCompiledLevelMarker* entryMarker = nullptr;
    if (!ResolveSectorLevelEntryMarker(loaded, entry.markerId, entryMarker, error)) {
        return false;
    }
    const std::string resolvedEntryMarkerId =
            entryMarker != nullptr ? entryMarker->id : std::string{};
    std::string fingerprintError;
    if (!RefreshSectorStaticModelGeometryFingerprints(
                loaded,
                fingerprintError)
            && !fingerprintError.empty()) {
        TraceLog(LOG_WARNING, "%s", fingerprintError.c_str());
    }
    if (!scene.Rebuild(
                context,
                loaded,
                "sector_game",
                settings.footsteps.defaultSet,
                settings.footsteps.volume,
                error)) {
        return false;
    }

    topologyMap = std::move(loaded);
    entryMarker = resolvedEntryMarkerId.empty()
            ? nullptr
            : FindSectorCompiledLevelMarker(topologyMap, resolvedEntryMarkerId);
    levelName = requestedLevelName;
    levelPath = path;
    controller = SectorEditorPreviewControllerState{};
    collision = SectorEditorPreviewCollisionState{};
    navigationDebug = SectorGameNavigationDebugState{};
    controller.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            topologyMap.previewSettings);
    ResetSectorFreeflyController(
            controller.freeflyController,
            scene.Renderer().RendererPose());
    controller.previewControlMode = SectorPreviewControlMode::FreeFly;
    if (!BuildCollisionAndPlayer(scene, true, entryMarker, &error)) {
        if (error.empty()) {
            error = collision.sectorCollisionWorldWarning.empty()
                    ? "Could not build the game collision world"
                    : collision.sectorCollisionWorldWarning;
        }
        scene.Shutdown(context);
        topologyMap = SectorTopologyMap{};
        levelName.clear();
        levelPath.clear();
        return false;
    }
    EnterSectorFreeflyController(controller.freeflyController);
    weaponRegistry = &registry;
    applicationSettings = &settings;
    playerAudio = &playerAudioRuntime;
    persistentScripts = &persistentStore;
    fpsPlayer.Begin(
            context.assets,
            scene.Renderer(),
            registry,
            settings,
            "fps_game_viewmodel");
    running = true;
    paused = false;
    consoleInputCaptured = false;
    InitializeSectorScriptHost(
            scriptHost,
            scene.RuntimeObjects(),
            topologyMap,
            scripts,
            &scene.Navigation(),
            &scene.NpcNavigation());
    pendingLoadingSave = loadingSave;
    BeginGameLevelLoading(loading);
    error.clear();
    return true;
}

void SectorGameSession::Shutdown(
        engine::EngineContext& context,
        SectorSceneRuntime& scene)
{
    if (playerAudio != nullptr) {
        StopPlayerBreathingAudio(
                context.assets,
                context.audio,
                *playerAudio,
                breathingAudio);
    } else {
        breathingAudio = PlayerBreathingAudioRuntime{};
    }
    engine::ScriptSystemShutdownForMap(context, scripts);
    ResetSectorScriptHost(scriptHost);
    fpsPlayer.End(context.assets, scene.Renderer());
    if (running) {
        LeaveSectorFreeflyController();
    }
    scene.Shutdown(context);
    topologyMap = SectorTopologyMap{};
    controller = SectorEditorPreviewControllerState{};
    collision = SectorEditorPreviewCollisionState{};
    navigationDebug = SectorGameNavigationDebugState{};
    playerStamina = PlayerStamina{};
    ClearPlayerWindedCamera(windedCamera);
    StopGameLevelLoading(loading);
    levelName.clear();
    levelPath.clear();
    running = false;
    paused = false;
    consoleInputCaptured = false;
    pendingLoadingSave = false;
    weaponRegistry = nullptr;
    applicationSettings = nullptr;
    playerAudio = nullptr;
    persistentScripts = nullptr;
}

void SectorGameSession::SuspendForEditor(engine::EngineContext& context)
{
    Pause();
    engine::ScriptSystemShutdownForMap(context, scripts);
    ResetSectorScriptHost(scriptHost);
}

void SectorGameSession::Pause()
{
    if (!running || paused) {
        return;
    }
    paused = true;
    LeaveSectorFreeflyController();
}

void SectorGameSession::Resume(SectorSceneRuntime& scene)
{
    if (!running) {
        return;
    }
    paused = false;
    SetSectorFreeflyMouseLookEnabled(
            controller.freeflyController, !consoleInputCaptured);
    ApplyPlayerPose(scene);
}

void SectorGameSession::SetConsoleInputCaptured(bool captured)
{
    if (!running || consoleInputCaptured == captured) return;
    consoleInputCaptured = captured;
    if (!paused) {
        SetSectorFreeflyMouseLookEnabled(
                controller.freeflyController, !consoleInputCaptured);
    }
}

void SectorGameSession::Update(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        float dt)
{
    if (running && IsLoading()) {
        UpdateLoading(context, scene, dt);
        return;
    }
    if (!running || paused) {
        return;
    }

    context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key != KEY_F8) return;
                navigationDebug.visible = !navigationDebug.visible;
                engine::ConsumeEvent(event);
            });

    const Vector3 playerPosition =
            controller.fpsControllerState.feetPosition;
    const SectorFpsControllerConfig obstacleConfig =
            EffectiveSectorFpsControllerConfig(
                    controller.fpsControllerState,
                    controller.fpsControllerConfig);
    const SectorDoorPlayerObstacle playerObstacle{
            controller.fpsControllerState.feetPosition,
            obstacleConfig.playerRadius,
            obstacleConfig.playerHeight};
    scene.Update(
            context,
            topologyMap,
            dt,
            &playerPosition,
            &playerObstacle);
    UpdateSectorScriptOperations(context, scriptHost);
    SectorRuntimeObjectState& objects = scene.RuntimeObjects();

    if (!consoleInputCaptured) context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &context](engine::InputEvent& event) {
                if (event.key.key != KEY_F) {
                    return;
                }
                if (ToggleTargetedSectorDoorInteractionSystem(
                            context.world,
                            controller.fpsControllerState.feetPosition,
                            GameplayForward(controller))) {
                    engine::ConsumeEvent(event);
                }
            });

    SectorFpsControllerInput input;
    if (!consoleInputCaptured) {
        input.moveForward = context.input.IsKeyDown(KEY_W);
        input.moveBackward = context.input.IsKeyDown(KEY_S);
        input.strafeLeft = context.input.IsKeyDown(KEY_A);
        input.strafeRight = context.input.IsKeyDown(KEY_D);
        input.run = context.input.IsKeyDown(KEY_LEFT_SHIFT)
                || context.input.IsKeyDown(KEY_RIGHT_SHIFT);
        input.mouseLookEnabled = true;
        input.mouseDelta = context.input.MouseDelta();
    }
    if (!consoleInputCaptured) context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&input](engine::InputEvent& event) {
                if (event.key.key == KEY_SPACE) {
                    input.jumpPressed = true;
                } else if (event.key.key == KEY_LEFT_CONTROL
                        || event.key.key == KEY_RIGHT_CONTROL) {
                    input.crouchTogglePressed = true;
                } else {
                    return;
                }
                engine::ConsumeEvent(event);
            });

    if (applicationSettings != nullptr) {
        if (input.run && !CanPlayerStaminaSprint(playerStamina)) {
            input.run = false;
        }
        if (input.jumpPressed
                && !CanPlayerStaminaJump(
                        playerStamina,
                        applicationSettings->playerStamina)) {
            input.jumpPressed = false;
        }
    }

    const float previousVisualEyeY = scene.Renderer().RendererPose().position.y;
    UpdateSectorEditorGameplayPreview(
            objects.dynamicDoorColliders,
            objects.staticModelColliders,
            collision,
            controller,
            false,
            input,
            previousVisualEyeY,
            dt,
            &scene.NpcNavigation().collisionCylinders);
    if (applicationSettings != nullptr) {
        UpdatePlayerStamina(
                playerStamina,
                applicationSettings->playerStamina,
                controller.frameEvents.sprinting,
                controller.frameEvents.jumped,
                dt);
        const float staminaRatio = PlayerStaminaRatio(playerStamina);
        UpdatePlayerWindedCamera(
                windedCamera,
                applicationSettings->playerStamina.windedCamera,
                staminaRatio,
                dt);
        if (playerAudio != nullptr) {
            UpdatePlayerBreathingAudio(
                    context.assets,
                    context.audio,
                    *playerAudio,
                    breathingAudio,
                    applicationSettings->playerStamina.breathingAudio,
                    staminaRatio,
                    dt);
        }
    }
    UpdateSectorScriptTriggers(
            scriptHost,
            Vector2{
                    controller.fpsControllerState.feetPosition.x,
                    controller.fpsControllerState.feetPosition.z},
            dt);
    engine::ScriptSystemUpdate(context, scripts, dt);
    if (controller.frameEvents.footstep && applicationSettings != nullptr) {
        scene.PlayFootstepForSector(
                context,
                controller.fpsControllerState.currentSectorId,
                applicationSettings->footsteps.volume);
    }
    if (playerAudio != nullptr) {
        if (controller.frameEvents.jumped) {
            PlayPlayerSound(
                    context.assets,
                    context.audio,
                    *playerAudio,
                    "jump");
        }
        if (controller.frameEvents.landed) {
            PlayPlayerSound(
                    context.assets,
                    context.audio,
                    *playerAudio,
                    "land");
        }
    }
    if (controller.frameEvents.landed && applicationSettings != nullptr) {
        scene.PlayFootstepForSector(
                context,
                controller.fpsControllerState.currentSectorId,
                std::clamp(
                        applicationSettings->footsteps.volume
                                * applicationSettings->footsteps
                                        .landingImpactVolumeMultiplier,
                        0.0f,
                        1.0f));
    }
    bool acceptedShot = false;
    if (weaponRegistry != nullptr && applicationSettings != nullptr) {
        fpsPlayer.Update(
                context.assets,
                scene.Renderer(),
                *weaponRegistry,
                *applicationSettings,
                dt);
    }
    ApplyPlayerPose(scene);
    if (weaponRegistry != nullptr && applicationSettings != nullptr) {
        acceptedShot = fpsPlayer.HandleInput(
                context.input,
                *weaponRegistry,
                context.assets,
                context.audio,
                collision.sectorCollisionWorldValid
                        ? &collision.sectorCollisionWorld
                        : nullptr,
                scene.Renderer(),
                true,
                !consoleInputCaptured,
                consoleInputCaptured);
        if (acceptedShot) {
            const FpsShotResult request = fpsPlayer.State().firing.lastShot;
            FpsShotResult resolvedShot;
            scene.ResolvePlayerWeaponShot(
                    context,
                    collision.sectorCollisionWorldValid
                            ? &collision.sectorCollisionWorld
                            : nullptr,
                    request.rayOrigin,
                    request.rayDirection,
                    fpsPlayer.State().firing.shotSequence,
                    fpsPlayer.State().firing.definition,
                    resolvedShot);
            fpsPlayer.RecordShotResolution(resolvedShot);
            ApplyPlayerPose(scene);
        }
        fpsPlayer.UpdateTransformsAndLight(
                scene.Renderer(),
                collision.sectorCollisionWorldValid
                        ? &collision.sectorCollisionWorld
                        : nullptr);
    }
    UpdateAudioListener(context.audio, scene.Renderer().RenderCamera());
    const SectorFpsControllerConfig visibilityConfig =
            NormalizeSectorFpsControllerConfig(
                    controller.fpsControllerConfig);
    scene.Renderer().UpdateVisibilityDebug(
            controller.fpsControllerState.currentSectorId,
            ClampRuntimeVisibilitySeedRadiusWorld(
                    visibilityConfig.playerRadius),
            true,
            &objects.dynamicPortalBlockers,
            &context.world);
    ConsumeScriptTransitionRequest(context, scene);
}

void SectorGameSession::UpdateLoading(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        float dt)
{
    if (loading.phase == GameLevelLoadPhase::Fading) {
        if (!AdvanceGameLevelLoadingFade(loading, dt)) return;
        std::string error;
        if (!ActivateLoadedMap(context, error)) {
            const std::string reason = error.empty()
                    ? "Map script activation failed" : error;
            Shutdown(context, scene);
            failureError = reason;
            return;
        }
        ActivateGameLevel(loading);
        return;
    }
    if (loading.phase != GameLevelLoadPhase::Loading) return;

    scene.UpdateLoadPreparation(context, topologyMap);
    if (weaponRegistry != nullptr && applicationSettings != nullptr) {
        fpsPlayer.Update(
                context.assets,
                scene.Renderer(),
                *weaponRegistry,
                *applicationSettings,
                0.0f);
    }
    ApplyPlayerPose(scene);

    size_t finishedAssets = 0;
    size_t totalAssets = 0;
    scene.AccumulateLoadAssetProgress(
            context.assets, finishedAssets, totalAssets);
    const engine::AssetScopeHandle viewmodelScope =
            fpsPlayer.State().assetScope;
    if (!engine::IsNull(viewmodelScope)) {
        size_t finished = 0;
        size_t total = 0;
        context.assets.GetScopeProgressCounts(
                viewmodelScope, finished, total);
        finishedAssets += finished;
        totalAssets += total;
    }
    const float assetProgress = totalAssets == 0
            ? 1.0f
            : static_cast<float>(finishedAssets)
                    / static_cast<float>(totalAssets);

    const SectorNavigationState navigationState = scene.Navigation().State();
    const bool navigationTerminal = NavigationBuildTerminal(navigationState);
    const bool hasNpcs = HasRuntimeNpcs(scene);
    const GameLevelNavigationGate navigationGate =
            EvaluateGameLevelNavigationGate(
                    hasNpcs,
                    navigationTerminal,
                    navigationState == SectorNavigationState::Ready);
    if (navigationGate == GameLevelNavigationGate::Unavailable) {
        const std::string reason = "Level '" + levelName
                + "' cannot activate NPCs: "
                + NavigationLoadFailure(scene.Navigation());
        Shutdown(context, scene);
        failureError = reason;
        return;
    }

    const SectorRuntimeObjectState& objects = scene.RuntimeObjects();
    const bool assetsFinished = scene.AreLoadAssetScopesFinished(context.assets)
            && ScopeFinishedOrEmpty(context.assets, viewmodelScope);
    const bool runtimeObjectsFinished =
            objects.spriteAnimationPendingCount == 0
            && objects.staticModelPendingCount == 0;
    const bool viewmodelFinished = FpsViewmodelLoadFinished(
            fpsPlayer.State());
    const bool complete = assetsFinished
            && runtimeObjectsFinished
            && viewmodelFinished
            && navigationGate == GameLevelNavigationGate::Ready
            && InitialNavigationObstaclesSettled(scene.Navigation());
    UpdateGameLevelLoadingProgress(
            loading,
            assetProgress,
            NavigationLoadProgress(scene.Navigation()),
            complete);
    if (complete) BeginGameLevelLoadingFade(loading);
}

bool SectorGameSession::ActivateLoadedMap(
        engine::EngineContext& context,
        std::string& error)
{
    if (persistentScripts == nullptr) {
        error = "Persistent script store is unavailable";
        return false;
    }
    if (!engine::ScriptSystemCreateForMap(
                context,
                scripts,
                *persistentScripts,
                levelName,
                levelPath,
                ASSETS_PATH,
                &scriptHost,
                RegisterSectorScriptBindings,
                pendingLoadingSave,
                error)) {
        ResetSectorScriptHost(scriptHost);
        return false;
    }
    pendingLoadingSave = false;
    error.clear();
    return true;
}

void SectorGameSession::RenderViewmodel(
        engine::AssetManager& assets,
        SectorSceneRuntime& scene)
{
    if (!running || IsLoadScreenOpaque()) {
        return;
    }
    fpsPlayer.Render(
            assets,
            scene.Renderer(),
            topologyMap,
            scene.RuntimeObjects(),
            controller.fpsControllerState.currentSectorId);
}

void SectorGameSession::RenderHud(
        engine::AssetManager& assets,
        engine::FontHandle font,
        Rectangle playableViewport) const
{
    if (IsActive() && weaponRegistry != nullptr) {
        fpsPlayer.RenderHud(
                playableViewport,
                *weaponRegistry,
                assets.GetFont(font),
                &playerHealth,
                &playerStamina);
    }
}

void SectorGameSession::RenderNavigationDebugWorld(
        const SectorSceneRuntime& scene) const
{
    if (!IsActive() || !navigationDebug.visible) return;
    DrawSectorNavigationDebugWorld(
            navigationDebug.drawSettings,
            scene.Navigation(),
            scene.NpcNavigation(),
            scene.Renderer());
}

void SectorGameSession::RenderNavigationDebugPanel(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle smallFont,
        const SectorSceneRuntime& scene) const
{
    if (!IsActive() || !navigationDebug.visible) return;
    DrawSectorGameNavigationDebugPanel(
            config,
            assets,
            smallFont,
            scene.Navigation(),
            scene.NpcNavigation(),
            scriptHost);
}

bool SectorGameSession::RebuildFromMap(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        const SectorTopologyMap& map,
        std::string& error)
{
    if (!running) {
        error = "No game session is running";
        return false;
    }
    engine::ScriptSystemShutdownForMap(context, scripts);
    ResetSectorScriptHost(scriptHost);
    const SectorFpsControllerState savedPlayer = controller.fpsControllerState;
    if (!scene.Rebuild(
                context,
                map,
                "sector_game",
                applicationSettings != nullptr
                        ? applicationSettings->footsteps.defaultSet
                        : FootstepApplicationSettings{}.defaultSet,
                applicationSettings != nullptr
                        ? applicationSettings->footsteps.volume
                        : FootstepApplicationSettings{}.volume,
                error)) {
        return false;
    }
    topologyMap = map;
    ResetFpsCameraRecoil(fpsPlayer.State().firing.cameraRecoil);
    controller.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            topologyMap.previewSettings);
    controller.fpsControllerState = savedPlayer;
    if (!BuildCollisionAndPlayer(scene, false, nullptr, &error)) {
        error = collision.sectorCollisionWorldWarning.empty()
                ? "Could not rebuild the game collision world"
                : collision.sectorCollisionWorldWarning;
        return false;
    }
    ApplyPlayerPose(scene);
    if (persistentScripts == nullptr) {
        error = "Persistent script store is unavailable";
        return false;
    }
    InitializeSectorScriptHost(
            scriptHost,
            scene.RuntimeObjects(),
            topologyMap,
            scripts,
            &scene.Navigation(),
            &scene.NpcNavigation());
    pendingLoadingSave = false;
    BeginGameLevelLoading(loading);
    error.clear();
    return true;
}

bool SectorGameSession::ReloadCurrentMap(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        bool remainPaused,
        std::string& error)
{
    if (!running) {
        error = "No game session is running";
        return false;
    }
    const std::string mapId = levelName;
    const FpsWeaponRegistry* savedWeaponRegistry = weaponRegistry;
    const FpsApplicationSettings* savedSettings = applicationSettings;
    PlayerAudioRuntime* savedPlayerAudio = playerAudio;
    engine::PersistentScriptStore* savedPersistent = persistentScripts;
    Shutdown(context, scene);
    if (savedWeaponRegistry == nullptr || savedSettings == nullptr
            || savedPlayerAudio == nullptr || savedPersistent == nullptr) {
        error = "Game services became unavailable during reload";
        return false;
    }
    if (!StartNew(
                context,
                scene,
                SectorLevelEntryRequest{mapId, std::nullopt},
                *savedWeaponRegistry,
                *savedSettings,
                *savedPlayerAudio,
                *savedPersistent,
                false,
                error)) {
        return false;
    }
    if (remainPaused) Pause();
    error.clear();
    return true;
}

std::string SectorGameSession::TakeFailureError()
{
    std::string result = std::move(failureError);
    failureError.clear();
    return result;
}

void SectorGameSession::ConsumeScriptTransitionRequest(
        engine::EngineContext& context,
        SectorSceneRuntime& scene)
{
    if (scripts.mapAbortRequested) {
        const std::string reason = scripts.mapAbortError.empty()
                ? "Map init() failed" : scripts.mapAbortError;
        Shutdown(context, scene);
        failureError = reason;
        return;
    }
    if (!scripts.mapChangeRequested) return;

    const std::string requestedMap = std::move(scripts.requestedMapId);
    const std::string requestedSpawn = std::move(scripts.requestedSpawnId);
    scripts.mapChangeRequested = false;
    scripts.requestedMapId.clear();
    scripts.requestedSpawnId.clear();

    const FpsWeaponRegistry* savedWeaponRegistry = weaponRegistry;
    const FpsApplicationSettings* savedSettings = applicationSettings;
    PlayerAudioRuntime* savedPlayerAudio = playerAudio;
    engine::PersistentScriptStore* savedPersistent = persistentScripts;
    const Health savedHealth = playerHealth;
    const PlayerStamina savedStamina = playerStamina;
    Shutdown(context, scene);
    if (savedWeaponRegistry == nullptr || savedSettings == nullptr
            || savedPlayerAudio == nullptr || savedPersistent == nullptr) {
        failureError = "Map change failed because session services are unavailable";
        return;
    }

    std::string error;
    const SectorLevelEntryRequest entry{
            requestedMap,
            requestedSpawn.empty()
                    ? std::optional<std::string>{}
                    : std::optional<std::string>{requestedSpawn}};
    if (!StartNew(
                context,
                scene,
                entry,
                *savedWeaponRegistry,
                *savedSettings,
                *savedPlayerAudio,
                *savedPersistent,
                false,
                error)) {
        failureError = "Map change to '" + requestedMap + "' failed: "
                + (error.empty() ? "unknown error" : error);
    } else {
        playerHealth = savedHealth;
        playerStamina = savedStamina;
    }
}

bool SectorGameSession::BuildCollisionAndPlayer(
        SectorSceneRuntime& scene,
        bool initializePlayer,
        const SectorCompiledLevelMarker* entryMarker,
        std::string* error)
{
    SectorRuntimeObjectState& objects = scene.RuntimeObjects();
    if (!RebuildSectorEditorCollisionWorld(
                topologyMap,
                collision,
                controller,
                objects.staticModelColliders)) {
        return false;
    }
    if (initializePlayer) {
        if (entryMarker != nullptr) {
            controller.fpsControllerState = SectorFpsControllerState{};
            controller.fpsControllerState.feetPosition =
                    SectorAuthoringToWorldPosition(entryMarker->position);
            controller.fpsControllerState.yawRadians = entryMarker->yawRadians;
            controller.fpsControllerState.pitchRadians = 0.0f;
        } else {
            controller.fpsControllerState = SectorFpsControllerStateFromCameraPose(
                    scene.Renderer().RendererPose(),
                    controller.fpsControllerConfig);
        }
    }
    controller.previewControlMode = SectorPreviewControlMode::Gameplay;
    InitializeSectorEditorGameplayVerticalState(
            collision,
            controller,
            objects.staticModelColliders);
    if (entryMarker != nullptr
            && (controller.fpsControllerState.currentSectorId == 0
                    || !collision.previewVerticalResult.hasSector
                    || collision.previewVerticalResult.cannotFit)) {
        if (error != nullptr) {
            *error = "Level entry marker '" + entryMarker->id
                    + "' is not a valid player position";
        }
        return false;
    }
    ApplyPlayerPose(scene);
    return true;
}

void SectorGameSession::ApplyPlayerPose(SectorSceneRuntime& scene)
{
    const SectorViewPose basePose = SectorFpsControllerVisualPose(
            controller.fpsControllerState,
            controller.fpsControllerConfig,
            controller.visualStepOffsetY,
            controller.headBobState.offset,
            controller.landingDipState.offsetY);
    SectorViewPose windedPose = basePose;
    windedPose.position.y += windedCamera.verticalOffsetWorld;
    windedPose.pitchRadians = ClampSectorFpsPitch(
            windedPose.pitchRadians
                    + windedCamera.pitchOffsetDegrees * DEG2RAD);
    scene.Renderer().ApplyRendererPose(ApplySectorFpsViewRotationOffset(
            windedPose,
            fpsPlayer.State().firing.cameraRecoil.rotationDegrees),
            false);
    controller.freeflyController.pose = basePose;
}

} // namespace game
