#include "game/SectorGameSession.h"

#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorStaticModelLightmap.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {

namespace {

Vector3 GameplayForward(const SectorEditorPreviewControllerState& controller)
{
    const SectorViewPose pose = SectorFpsControllerPose(
            controller.fpsControllerState,
            controller.fpsControllerConfig);
    const float horizontal = std::cos(pose.pitchRadians);
    return Vector3{
            std::cos(pose.yawRadians) * horizontal,
            std::sin(pose.pitchRadians),
            std::sin(pose.yawRadians) * horizontal};
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

SectorScriptAudioApi MakeSectorScriptAudioApi(SectorSceneRuntime& scene)
{
    SectorScriptAudioApi api;
    api.userData = &scene;
    api.playMapSound = [](void* userData, engine::EngineContext& context,
                              const std::string& id, float volume, float pitch,
                              std::string& error) {
        return static_cast<SectorSceneRuntime*>(userData)->PlayLevelSound(
                context, id, volume, pitch, error);
    };
    api.playSoundEmitter = [](void* userData, engine::EngineContext& context,
                                  const std::string& id, const float* volume,
                                  float pitch, std::string& error) {
        return static_cast<SectorSceneRuntime*>(userData)->PlaySoundEmitter(
                context, id, volume, pitch, error);
    };
    api.stopSoundEmitter = [](void* userData, engine::EngineContext& context,
                                  const std::string& id, std::string& error) {
        return static_cast<SectorSceneRuntime*>(userData)->StopSoundEmitter(
                context, id, error);
    };
    return api;
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

bool FirstScriptValueIsTrue(const std::vector<engine::ScriptValue>& values)
{
    return !values.empty()
            && std::holds_alternative<bool>(values.front())
            && std::get<bool>(values.front());
}

} // namespace

void SectorGameSession::ShowCarryRefusal()
{
    std::snprintf(
            itemMessage.data(),
            itemMessage.size(),
            "%s",
            "I can't carry anymore.");
    itemMessageElapsedSeconds = 0.0f;
}

bool SectorGameSession::CommitItemTake(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        engine::Entity entity,
        int placedObjectId,
        const std::string& instanceId)
{
    if (itemRegistry == nullptr || itemCampaign == nullptr
            || applicationSettings == nullptr
            || !context.world.IsAlive(entity)
            || !context.world.Has<SectorItem>(entity)) {
        return false;
    }
    SectorItem& item = context.world.Get<SectorItem>(entity);
    if (item.placedObjectId != placedObjectId
            || item.instanceId != instanceId) {
        return false;
    }
    const ItemPickupPlan plan = PreflightItemPickup(
            itemCampaign->inventory,
            *itemRegistry,
            applicationSettings->playerInventory,
            item.definitionId,
            item.quantity);
    if (plan.result != ItemPickupCapacityResult::Fits) {
        item.takePending = false;
        if (plan.result == ItemPickupCapacityResult::WeightLimit
                || plan.result == ItemPickupCapacityResult::SlotLimit
                || plan.result == ItemPickupCapacityResult::NumericOverflow) {
            ShowCarryRefusal();
        }
        return false;
    }
    ItemLevelCampaignState& level = FindOrCreateItemLevelCampaignState(
            *itemCampaign, levelName, topologyMap.runtimeObjects.size());
    if (item.origin == SectorItemOrigin::SessionDrop) {
        const bool exists = std::any_of(
                level.droppedItems.begin(), level.droppedItems.end(),
                [placedObjectId](const SectorPlacedRuntimeObject& object) {
                    return object.id == placedObjectId;
                });
        if (!exists) {
            item.takePending = false;
            return false;
        }
    }
    const bool runtimeTracked = std::any_of(
            scene.RuntimeObjects().placedObjectEntities.begin(),
            scene.RuntimeObjects().placedObjectEntities.end(),
            [entity](const SectorPlacedRuntimeObjectEntity& entry) {
                return entry.entity == entity;
            });
    if (!runtimeTracked) {
        item.takePending = false;
        return false;
    }
    if (!CommitItemPickup(
                itemCampaign->inventory, plan, item.onUseScript)) {
        item.takePending = false;
        return false;
    }
    if (item.origin == SectorItemOrigin::SessionDrop) {
        RemoveSessionDroppedItem(level, placedObjectId);
    } else {
        RecordAuthoredItemCollected(level, placedObjectId);
    }
    topologyMap.runtimeObjects.erase(
            std::remove_if(
                    topologyMap.runtimeObjects.begin(),
                    topologyMap.runtimeObjects.end(),
                    [placedObjectId](const SectorPlacedRuntimeObject& object) {
                        return object.id == placedObjectId;
                    }),
            topologyMap.runtimeObjects.end());
    if (!QueueRemoveSectorRuntimeObjectByEntity(
                context.world, scene.RuntimeObjects(), entity)) {
        TraceLog(
                LOG_ERROR,
                "Item pickup committed but runtime entity %u could not be queued for removal",
                entity.index);
        return false;
    }
    context.world.FlushDestroyedEntities();
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    return true;
}

bool SectorGameSession::RequestItemTake(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        engine::Entity entity)
{
    if (pendingItemTake.active || itemRegistry == nullptr
            || itemCampaign == nullptr || applicationSettings == nullptr
            || !context.world.IsAlive(entity)
            || !context.world.Has<SectorItem>(entity)) {
        return false;
    }
    SectorItem& item = context.world.Get<SectorItem>(entity);
    if (item.takePending) return false;
    const ItemPickupPlan plan = PreflightItemPickup(
            itemCampaign->inventory,
            *itemRegistry,
            applicationSettings->playerInventory,
            item.definitionId,
            item.quantity);
    if (plan.result != ItemPickupCapacityResult::Fits) {
        if (plan.result == ItemPickupCapacityResult::WeightLimit
                || plan.result == ItemPickupCapacityResult::SlotLimit
                || plan.result == ItemPickupCapacityResult::NumericOverflow) {
            ShowCarryRefusal();
        }
        return true;
    }
    if (item.onTakeScript.empty()) {
        CommitItemTake(
                context, scene, entity, item.placedObjectId, item.instanceId);
        return true;
    }
    const engine::ScriptCallOutcome outcome =
            engine::ScriptSystemCallObservedForegroundHook(
                    scripts, item.onTakeScript);
    if (outcome.result == engine::ScriptCallResult::Completed) {
        if (FirstScriptValueIsTrue(outcome.immediateValues)) {
            CommitItemTake(
                    context, scene, entity,
                    item.placedObjectId, item.instanceId);
        }
        return true;
    }
    if (outcome.result == engine::ScriptCallResult::Started) {
        item.takePending = true;
        pendingItemTake.task = outcome.task;
        pendingItemTake.entity = entity;
        pendingItemTake.placedObjectId = item.placedObjectId;
        pendingItemTake.instanceId = item.instanceId;
        pendingItemTake.active = true;
        return true;
    }
    if (outcome.result == engine::ScriptCallResult::Missing) {
        TraceLog(
                LOG_WARNING,
                "[Lua WARNING] item '%s' has no callable take function '%s'",
                item.instanceId.c_str(), item.onTakeScript.c_str());
        return true;
    }
    if (outcome.result == engine::ScriptCallResult::Error) {
        TraceLog(
                LOG_ERROR,
                "[Lua ERROR] item '%s' take function '%s' failed: %s",
                item.instanceId.c_str(),
                item.onTakeScript.c_str(),
                outcome.error.c_str());
        return true;
    }
    return false;
}

void SectorGameSession::UpdatePendingItemTake(
        engine::EngineContext& context,
        SectorSceneRuntime& scene)
{
    if (!pendingItemTake.active) return;
    engine::ScriptObservedCallOutcome outcome;
    if (!engine::ScriptSystemTakeObservedCallOutcome(
                scripts, pendingItemTake.task, outcome)) {
        return;
    }
    const PendingItemTake pending = pendingItemTake;
    pendingItemTake = {};
    if (context.world.IsAlive(pending.entity)
            && context.world.Has<SectorItem>(pending.entity)) {
        context.world.Get<SectorItem>(pending.entity).takePending = false;
    }
    if (outcome.state == engine::ScriptTaskState::Completed
            && FirstScriptValueIsTrue(outcome.values)) {
        CommitItemTake(
                context,
                scene,
                pending.entity,
                pending.placedObjectId,
                pending.instanceId);
    } else if (outcome.state == engine::ScriptTaskState::Failed) {
        TraceLog(
                LOG_ERROR,
                "[Lua ERROR] item take callback failed: %s",
                outcome.error.c_str());
    }
}

bool SectorGameSession::StartNew(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        const SectorLevelEntryRequest& entry,
        const SectorMaterialRegistry& materials,
        const FpsWeaponRegistry& registry,
        const ItemRegistry& items,
        ItemCampaignState& campaign,
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
    if (!LoadSectorRuntimeLevel(path, materials, loaded, error)) {
        return false;
    }
    ReconcileItemCampaignLevel(campaign, requestedLevelName, loaded);
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
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    itemMessage = {};
    itemMessageElapsedSeconds = 0.0f;
    pendingItemTake = {};
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
    itemRegistry = &items;
    itemCampaign = &campaign;
    materialRegistry = &materials;
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
            &scene.NpcNavigation(),
            MakeSectorScriptAudioApi(scene));
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
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
    itemMessage = {};
    itemMessageElapsedSeconds = 0.0f;
    pendingItemTake = {};
    weaponRegistry = nullptr;
    itemRegistry = nullptr;
    itemCampaign = nullptr;
    materialRegistry = nullptr;
    applicationSettings = nullptr;
    playerAudio = nullptr;
    persistentScripts = nullptr;
}

void SectorGameSession::SuspendForEditor(engine::EngineContext& context)
{
    Pause();
    engine::ScriptSystemShutdownForMap(context, scripts);
    ResetSectorScriptHost(scriptHost);
    pendingItemTake = {};
}

void SectorGameSession::Pause()
{
    if (!running || paused) {
        return;
    }
    paused = true;
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
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
    if (itemMessage[0] != '\0') {
        itemMessageElapsedSeconds += std::max(0.0f, dt);
        if (itemMessageElapsedSeconds >= 2.25f) {
            itemMessage = {};
            itemMessageElapsedSeconds = 0.0f;
        }
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
            controller.fpsControllerState.currentSectorId,
            &playerObstacle);
    UpdateSectorScriptOperations(context, scriptHost);
    SectorRuntimeObjectState& objects = scene.RuntimeObjects();

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
    const SectorViewPose interactionPose = SectorFpsControllerPose(
            controller.fpsControllerState,
            controller.fpsControllerConfig);
    useTarget = FindSectorUseTarget(
            context.world,
            &context.assets,
            interactionPose.position,
            GameplayForward(controller),
            collision.sectorCollisionWorldValid
                    ? &collision.sectorCollisionWorld : nullptr,
            true);
    UpdateSectorUseHighlight(useHighlightState, useTarget, dt);
    usePromptTitle = {};
    const std::string_view promptTitle = SectorUseTargetTitle(
            context.world, useTarget);
    if (!promptTitle.empty()) {
        std::snprintf(
                usePromptTitle.data(),
                usePromptTitle.size(),
                "%.*s",
                static_cast<int>(promptTitle.size()),
                promptTitle.data());
    }
    if (!consoleInputCaptured) context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &context, &scene](engine::InputEvent& event) {
                if (event.key.key != KEY_E
                        || useTarget.kind == SectorUseTargetKind::None) {
                    return;
                }
                bool handled = false;
                if (useTarget.kind == SectorUseTargetKind::Item) {
                    handled = RequestItemTake(
                            context, scene, useTarget.entity);
                } else if (useTarget.kind == SectorUseTargetKind::Door) {
                    handled = RequestSectorScriptDoorUse(
                            context, scriptHost, useTarget.entity);
                } else if (useTarget.kind == SectorUseTargetKind::DynamicProp
                        && context.world.IsAlive(useTarget.entity)
                        && context.world.Has<SectorDynamicModel>(useTarget.entity)) {
                    SectorDynamicModel& prop =
                            context.world.Get<SectorDynamicModel>(useTarget.entity);
                    const engine::ScriptCallOutcome outcome =
                            engine::ScriptSystemCallForegroundHook(
                                    scripts, prop.onUseScript);
                    handled = outcome.result != engine::ScriptCallResult::ForegroundBusy
                            && outcome.result != engine::ScriptCallResult::AlreadyRunning;
                    if (handled && prop.singleUse
                            && (outcome.result == engine::ScriptCallResult::Completed
                                    || outcome.result == engine::ScriptCallResult::Started)) {
                        prop.useConsumed = true;
                    }
                    if (outcome.result == engine::ScriptCallResult::Missing) {
                        TraceLog(
                                LOG_WARNING,
                                "[Lua WARNING] prop '%s' has no callable use function '%s'",
                                prop.instanceId.c_str(),
                                prop.onUseScript.c_str());
                    } else if (outcome.result == engine::ScriptCallResult::Error) {
                        TraceLog(
                                LOG_ERROR,
                                "[Lua ERROR] prop '%s' use function '%s' failed: %s",
                                prop.instanceId.c_str(),
                                prop.onUseScript.c_str(),
                                outcome.error.c_str());
                    }
                }
                if (handled) engine::ConsumeEvent(event);
            });
    engine::ScriptSystemUpdate(context, scripts, dt);
    UpdatePendingItemTake(context, scene);
    UpdateSectorScriptDoorPermission(context, scriptHost);
    if (scriptHost.dynamicLightsDirty) {
        scene.Renderer().RefreshDynamicLightSources(topologyMap);
        scriptHost.dynamicLightsDirty = false;
    }
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
        engine::FontHandle usePromptFont,
        Rectangle playableViewport) const
{
    if (IsActive() && weaponRegistry != nullptr) {
        fpsPlayer.RenderHud(
                playableViewport,
                *weaponRegistry,
                assets.GetFont(font),
                &playerHealth,
                &playerStamina);
        if (itemMessage[0] != '\0') {
            DrawSectorUseMessage(
                    playableViewport,
                    assets.GetFont(usePromptFont),
                    itemMessage.data(),
                    itemMessageElapsedSeconds);
        } else {
            DrawSectorUsePrompt(
                    playableViewport,
                    assets.GetFont(usePromptFont),
                    usePromptTitle.data(),
                    useTarget.kind == SectorUseTargetKind::Item
                            ? "Take" : "Use");
        }
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
    pendingItemTake = {};
    const SectorFpsControllerState savedPlayer = controller.fpsControllerState;
    SectorTopologyMap reconciledMap = map;
    if (itemCampaign != nullptr) {
        ReconcileItemCampaignLevel(
                *itemCampaign, levelName, reconciledMap);
    }
    if (!scene.Rebuild(
            context,
            reconciledMap,
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
    topologyMap = std::move(reconciledMap);
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
            &scene.NpcNavigation(),
            MakeSectorScriptAudioApi(scene));
    useTarget = {};
    ResetSectorUseHighlight(useHighlightState);
    usePromptTitle = {};
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
    const ItemRegistry* savedItemRegistry = itemRegistry;
    ItemCampaignState* savedItemCampaign = itemCampaign;
    const SectorMaterialRegistry* savedMaterialRegistry = materialRegistry;
    const FpsApplicationSettings* savedSettings = applicationSettings;
    PlayerAudioRuntime* savedPlayerAudio = playerAudio;
    engine::PersistentScriptStore* savedPersistent = persistentScripts;
    Shutdown(context, scene);
    if (savedWeaponRegistry == nullptr || savedItemRegistry == nullptr
            || savedItemCampaign == nullptr || savedMaterialRegistry == nullptr
            || savedSettings == nullptr
            || savedPlayerAudio == nullptr || savedPersistent == nullptr) {
        error = "Game services became unavailable during reload";
        return false;
    }
    if (!StartNew(
                context,
                scene,
                SectorLevelEntryRequest{mapId, std::nullopt},
                *savedMaterialRegistry,
                *savedWeaponRegistry,
                *savedItemRegistry,
                *savedItemCampaign,
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
    const ItemRegistry* savedItemRegistry = itemRegistry;
    ItemCampaignState* savedItemCampaign = itemCampaign;
    const SectorMaterialRegistry* savedMaterialRegistry = materialRegistry;
    const FpsApplicationSettings* savedSettings = applicationSettings;
    PlayerAudioRuntime* savedPlayerAudio = playerAudio;
    engine::PersistentScriptStore* savedPersistent = persistentScripts;
    const Health savedHealth = playerHealth;
    const PlayerStamina savedStamina = playerStamina;
    Shutdown(context, scene);
    if (savedWeaponRegistry == nullptr || savedItemRegistry == nullptr
            || savedItemCampaign == nullptr || savedMaterialRegistry == nullptr
            || savedSettings == nullptr
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
                *savedMaterialRegistry,
                *savedWeaponRegistry,
                *savedItemRegistry,
                *savedItemCampaign,
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
