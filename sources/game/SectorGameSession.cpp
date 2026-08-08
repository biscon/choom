#include "game/SectorGameSession.h"

#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorStaticModelLightmap.h"

#include <raylib.h>

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

} // namespace

bool SectorGameSession::StartNew(
        engine::EngineContext& context,
        SectorSceneRuntime& scene,
        const std::string& requestedLevelName,
        const FpsWeaponRegistry& registry,
        const FpsApplicationSettings& settings,
        std::string& error)
{
    const std::string path = ApplicationLevelAssetPath(requestedLevelName);
    if (path.empty()) {
        error = "Invalid first level name '" + requestedLevelName + "'";
        return false;
    }

    SectorTopologyMap loaded;
    if (!LoadSectorRuntimeLevel(path, loaded, error)) {
        return false;
    }
    std::string fingerprintError;
    if (!RefreshSectorStaticModelGeometryFingerprints(
                loaded,
                fingerprintError)
            && !fingerprintError.empty()) {
        TraceLog(LOG_WARNING, "%s", fingerprintError.c_str());
    }
    if (!scene.Rebuild(context, loaded, "sector_game", error)) {
        return false;
    }

    topologyMap = std::move(loaded);
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
    if (!BuildCollisionAndPlayer(scene, true)) {
        error = collision.sectorCollisionWorldWarning.empty()
                ? "Could not build the game collision world"
                : collision.sectorCollisionWorldWarning;
        scene.Shutdown(context);
        topologyMap = SectorTopologyMap{};
        levelName.clear();
        levelPath.clear();
        return false;
    }
    EnterSectorFreeflyController(controller.freeflyController);
    weaponRegistry = &registry;
    applicationSettings = &settings;
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
    ApplyPlayerPose(scene);
    if (weaponRegistry != nullptr && applicationSettings != nullptr) {
        fpsPlayer.Update(
                context.assets,
                *weaponRegistry,
                *applicationSettings,
                dt);
        fpsPlayer.HandleInput(
                context.input,
                collision.sectorCollisionWorldValid
                        ? &collision.sectorCollisionWorld
                        : nullptr,
                scene.Renderer(),
                true,
                true,
                false);
        fpsPlayer.UpdateTransformsAndLight(
                scene.Renderer(),
                collision.sectorCollisionWorldValid
                        ? &collision.sectorCollisionWorld
                        : nullptr);
    }
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
    if (!scene.Rebuild(context, map, "sector_game", error)) {
        return false;
    }
    topologyMap = map;
    controller.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            topologyMap.previewSettings);
    controller.fpsControllerState = savedPlayer;
    if (!BuildCollisionAndPlayer(scene, false)) {
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
        bool initializePlayer)
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
        controller.fpsControllerState = SectorFpsControllerStateFromCameraPose(
                scene.Renderer().RendererPose(),
                controller.fpsControllerConfig);
    }
    controller.previewControlMode = SectorPreviewControlMode::Gameplay;
    InitializeSectorEditorGameplayVerticalState(
            collision,
            controller,
            objects.staticModelColliders);
    ApplyPlayerPose(scene);
    return true;
}

void SectorGameSession::ApplyPlayerPose(SectorSceneRuntime& scene)
{
    scene.Renderer().ApplyRendererPose(SectorFpsControllerVisualPose(
            controller.fpsControllerState,
            controller.fpsControllerConfig,
            controller.visualStepOffsetY,
            controller.headBobState.offset,
            controller.landingDipState.offsetY));
    controller.freeflyController.pose = scene.Renderer().RendererPose();
}

} // namespace game
