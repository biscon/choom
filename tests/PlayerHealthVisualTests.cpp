#include "game/PlayerHealthVisual.h"

#include <cassert>
#include <cmath>

namespace {

bool Near(float actual, float expected, float tolerance = 0.000001f)
{
    return std::fabs(actual - expected) <= tolerance;
}

} // namespace

int main()
{
    game::PlayerHealthApplicationSettings healthSettings;
    assert(game::PlayerHealthSettingsError(healthSettings).empty());

    const auto& visual = healthSettings.lowHealthVisual;
    assert(Near(game::PlayerLowHealthVisualStrength(
                        game::Health{100, 100, 100}, visual),
                0.0f));
    assert(Near(game::PlayerLowHealthVisualStrength(
                        game::Health{100, 100, 50}, visual),
                0.0f));
    assert(Near(game::PlayerLowHealthVisualStrength(
                        game::Health{100, 100, 25}, visual),
                0.5f));
    assert(Near(game::PlayerLowHealthVisualStrength(
                        game::Health{100, 100, 0}, visual),
                1.0f));
    assert(Near(game::PlayerLowHealthVignetteOpacity(
                        game::Health{100, 100, 100}, visual),
                0.0f));
    assert(Near(game::PlayerLowHealthVignetteOpacity(
                        game::Health{100, 100, 50}, visual),
                0.0f));
    assert(Near(game::PlayerLowHealthVignetteOpacity(
                        game::Health{100, 100, 25}, visual),
                0.544375f));
    assert(Near(game::PlayerLowHealthVignetteOpacity(
                        game::Health{100, 100, 0}, visual),
                0.8775f));
    assert(Near(game::PlayerHealthRatio(game::Health{100, 100, 150}), 1.0f));
    assert(Near(game::PlayerHealthRatio(game::Health{100, 100, -10}), 0.0f));
    assert(Near(game::PlayerHealthRatio(game::Health{100, 0, 0}), 0.0f));

    const auto& heartbeat = healthSettings.heartbeatAudio;
    assert(Near(game::PlayerHeartbeatStrength(
                        game::Health{100, 100, 50}, heartbeat),
                0.0f));
    assert(Near(game::PlayerHeartbeatStrength(
                        game::Health{100, 100, 30}, heartbeat),
                0.5f));
    assert(Near(game::PlayerHeartbeatStrength(
                        game::Health{100, 100, 10}, heartbeat),
                1.0f));
    assert(Near(game::PlayerHeartbeatStrength(
                        game::Health{200, 200, 20}, heartbeat),
                1.0f));
    assert(Near(game::PlayerHeartbeatVolume(
                        game::Health{100, 100, 30}, heartbeat),
                0.5f));
    assert(Near(game::PlayerHeartbeatPitch(
                        game::Health{100, 100, 30}, heartbeat),
                1.25f));
    assert(Near(game::PlayerHeartbeatPitch(
                        game::Health{100, 100, 5}, heartbeat),
                1.5f));

    const auto& movement = healthSettings.lowHealthMovement;
    assert(Near(game::PlayerLowHealthMovementSpeedScale(
                        game::Health{100, 100, 50}, movement),
                1.0f));
    assert(Near(game::PlayerLowHealthMovementSpeedScale(
                        game::Health{100, 100, 25}, movement),
                0.6f));
    assert(Near(game::PlayerLowHealthMovementSpeedScale(
                        game::Health{100, 100, 10}, movement),
                0.36f));
    assert(Near(game::PlayerLowHealthMovementSpeedScale(
                        game::Health{100, 100, 0}, movement),
                0.2f));
    game::PlayerLowHealthMovementApplicationSettings sprintMovement = movement;
    sprintMovement.minimumSprintSpeedScale = 0.75f;
    assert(Near(game::PlayerLowHealthMovementSpeedScale(
                        game::Health{100, 100, 25}, sprintMovement, true),
                0.875f));
    assert(Near(game::PlayerLowHealthMovementSpeedScale(
                        game::Health{100, 100, 10}, sprintMovement, true),
                0.80f));
    assert(Near(game::PlayerLowHealthMovementSpeedScale(
                        game::Health{100, 100, 0}, sprintMovement, true),
                0.75f));

    const auto& camera = healthSettings.lowHealthCamera;
    assert(Near(game::PlayerLowHealthCameraStrength(
                        game::Health{100, 100, 50}, camera),
                0.0f));
    assert(Near(game::PlayerLowHealthCameraStrength(
                        game::Health{100, 100, 30}, camera),
                0.25f));
    assert(Near(game::PlayerLowHealthCameraStrength(
                        game::Health{100, 100, 10}, camera),
                1.0f));
    game::PlayerLowHealthCameraState cameraState;
    for (int i = 0; i < 60; ++i) {
        game::UpdatePlayerLowHealthCamera(
                cameraState,
                camera,
                game::Health{100, 100, 10},
                1.0f / 60.0f);
    }
    assert(cameraState.intensity > 0.9f);
    assert(std::isfinite(cameraState.phase));
    assert(std::isfinite(cameraState.positionOffsetLocal.x));
    assert(std::isfinite(cameraState.positionOffsetLocal.y));
    assert(std::isfinite(cameraState.positionOffsetLocal.z));
    assert(std::isfinite(cameraState.rotationDegrees.x));
    assert(std::isfinite(cameraState.rotationDegrees.y));
    assert(std::isfinite(cameraState.rotationDegrees.z));
    game::PlayerLowHealthCameraApplicationSettings disabledCamera = camera;
    disabledCamera.enabled = false;
    game::UpdatePlayerLowHealthCamera(
            cameraState,
            disabledCamera,
            game::Health{100, 100, 10},
            1.0f / 60.0f);
    assert(Near(cameraState.intensity, 0.0f));
    assert(Near(cameraState.positionOffsetLocal.x, 0.0f));
    assert(Near(cameraState.rotationDegrees.z, 0.0f));

    game::PlayerLowHealthVisualApplicationSettings disabled = visual;
    disabled.enabled = false;
    assert(Near(game::PlayerLowHealthVisualStrength(
                        game::Health{100, 100, 0}, disabled),
                0.0f));
    assert(Near(game::PlayerLowHealthVignetteOpacity(
                        game::Health{100, 100, 0}, disabled),
                0.0f));

    healthSettings.lowHealthVisual.thresholdRatio = 0.0f;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.lowHealthVisual.vignetteOuterRadius =
            healthSettings.lowHealthVisual.vignetteInnerRadius;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.lowHealthVisual.maximumVignetteOpacity = 1.01f;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.lowHealthVisual.maximumDesaturation = -0.01f;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.heartbeatAudio.fullEffectRatio =
            healthSettings.heartbeatAudio.startThresholdRatio;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.heartbeatAudio.maximumPitch = 0.5f;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.lowHealthMovement.minimumSpeedScale = 1.01f;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.lowHealthMovement.minimumSprintSpeedScale = -0.01f;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.lowHealthCamera.fullEffectRatio = 0.75f;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
    healthSettings = {};
    healthSettings.lowHealthCamera.responseSeconds = 0.0f;
    assert(!game::PlayerHealthSettingsError(healthSettings).empty());
}
