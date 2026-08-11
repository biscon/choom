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

} // namespace

bool SectorGameSession::StartNew(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        const SectorLevelEntryRequest& entry,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        PlayerAudioRuntime& playerAudioRuntime,
        std::string& error)
{
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
    fpsPlayer.Begin(
            context.assets,
            scene.Renderer(),
            registry,
            settings,
            "fps_game_viewmodel");
    running = true;
    paused = false;
    error.clear();
    return true;
}

void SectorGameSession::Shutdown(
        engine::EngineContext& context,
        SectorSceneRuntime& scene)
{
    fpsPlayer.End(context.assets, scene.Renderer());
    if (running) {
        LeaveSectorFreeflyController();
    }
    scene.Shutdown(context);
    topologyMap = SectorTopologyMap{};
    controller = SectorEditorPreviewControllerState{};
    collision = SectorEditorPreviewCollisionState{};
    levelName.clear();
    levelPath.clear();
    running = false;
    paused = false;
    weaponRegistry = nullptr;
    applicationSettings = nullptr;
    playerAudio = nullptr;
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
    SetSectorFreeflyMouseLookEnabled(controller.freeflyController, true);
    ApplyPlayerPose(scene);
}

void SectorGameSession::Update(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        float dt)
{
    if (!running || paused) {
        return;
    }

    const Vector3 playerPosition =
            controller.fpsControllerState.feetPosition;
    scene.Update(context, topologyMap, dt, &playerPosition);
    SectorRuntimeObjectState& objects = scene.RuntimeObjects();

    context.input.ForEachEvent(
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
    input.moveForward = context.input.IsKeyDown(KEY_W);
    input.moveBackward = context.input.IsKeyDown(KEY_S);
    input.strafeLeft = context.input.IsKeyDown(KEY_A);
    input.strafeRight = context.input.IsKeyDown(KEY_D);
    input.run = context.input.IsKeyDown(KEY_LEFT_SHIFT)
            || context.input.IsKeyDown(KEY_RIGHT_SHIFT);
    input.mouseLookEnabled = true;
    input.mouseDelta = context.input.MouseDelta();
    context.input.ForEachEvent(
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

    const float previousVisualEyeY = scene.Renderer().RendererPose().position.y;
    UpdateSectorEditorGameplayPreview(
            objects.dynamicDoorColliders,
            objects.staticModelColliders,
            collision,
            controller,
            false,
            input,
            previousVisualEyeY,
            dt);
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
                *weaponRegistry,
                *applicationSettings,
                dt);
    }
    ApplyPlayerPose(scene);
    if (weaponRegistry != nullptr && applicationSettings != nullptr) {
        acceptedShot = fpsPlayer.HandleInput(
                context.input,
                context.assets,
                context.audio,
                collision.sectorCollisionWorldValid
                        ? &collision.sectorCollisionWorld
                        : nullptr,
                scene.Renderer(),
                true,
                true,
                false);
        if (acceptedShot) {
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
}

void SectorGameSession::RenderViewmodel(
        engine::AssetManager& assets,
        SectorSceneRuntime& scene)
{
    if (!running) {
        return;
    }
    fpsPlayer.Render(
            assets,
            scene.Renderer(),
            topologyMap,
            scene.RuntimeObjects(),
            controller.fpsControllerState.currentSectorId);
}

void SectorGameSession::RenderHud(Rectangle playableViewport) const
{
    if (running && weaponRegistry != nullptr) {
        fpsPlayer.RenderHud(playableViewport, *weaponRegistry);
    }
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
    const SectorFpsControllerState savedPlayer = controller.fpsControllerState;
    if (!scene.Rebuild(
                context,
                map,
                "sector_game",
                applicationSettings != nullptr
                        ? applicationSettings->footsteps.defaultSet
                        : FootstepApplicationSettings{}.defaultSet,
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
    error.clear();
    return true;
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
    scene.Renderer().ApplyRendererPose(ApplySectorFpsViewRotationOffset(
            basePose,
            fpsPlayer.State().firing.cameraRecoil.rotationDegrees),
            false);
    controller.freeflyController.pose = basePose;
}

} // namespace game
