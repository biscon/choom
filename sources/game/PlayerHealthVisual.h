#pragma once

#include "game/Health.h"

#include <raylib.h>

#include <string>

namespace game {

struct PlayerLowHealthVisualApplicationSettings {
    bool enabled = true;
    float thresholdRatio = 0.50f;
    Color vignetteColor{40, 3, 7, 255};
    float vignetteInnerRadius = 0.50f;
    float vignetteOuterRadius = 1.05f;
    float maximumVignetteOpacity = 0.65f;
    float maximumDesaturation = 0.22f;
};

struct PlayerHeartbeatAudioApplicationSettings {
    bool enabled = true;
    float startThresholdRatio = 0.50f;
    float fullEffectRatio = 0.10f;
    float maximumVolume = 1.0f;
    float startPitch = 1.0f;
    float maximumPitch = 1.5f;
    float responseSeconds = 0.25f;
};

struct PlayerLowHealthMovementApplicationSettings {
    bool enabled = true;
    float startThresholdRatio = 0.50f;
    float minimumSpeedScale = 0.20f;
};

struct PlayerLowHealthCameraApplicationSettings {
    bool enabled = true;
    float startThresholdRatio = 0.50f;
    float fullEffectRatio = 0.10f;
    float lateralAmplitudeWorld = 0.020f;
    float verticalAmplitudeWorld = 0.012f;
    float pitchAmplitudeDegrees = 0.85f;
    float yawAmplitudeDegrees = 0.65f;
    float rollAmplitudeDegrees = 1.50f;
    float frequencyHz = 0.55f;
    float responseSeconds = 0.35f;
};

struct PlayerHealthApplicationSettings {
    PlayerLowHealthVisualApplicationSettings lowHealthVisual;
    PlayerHeartbeatAudioApplicationSettings heartbeatAudio;
    PlayerLowHealthMovementApplicationSettings lowHealthMovement;
    PlayerLowHealthCameraApplicationSettings lowHealthCamera;
};

struct PlayerLowHealthCameraState {
    float phase = 0.0f;
    float intensity = 0.0f;
    Vector3 positionOffsetLocal{};
    Vector3 rotationDegrees{};
};

std::string PlayerHealthSettingsError(
        const PlayerHealthApplicationSettings& settings);
float PlayerHealthRatio(const Health& health);
float PlayerLowHealthVisualStrength(
        const Health& health,
        const PlayerLowHealthVisualApplicationSettings& settings);
float PlayerLowHealthVignetteOpacity(
        const Health& health,
        const PlayerLowHealthVisualApplicationSettings& settings);
float PlayerHeartbeatStrength(
        const Health& health,
        const PlayerHeartbeatAudioApplicationSettings& settings);
float PlayerHeartbeatVolume(
        const Health& health,
        const PlayerHeartbeatAudioApplicationSettings& settings);
float PlayerHeartbeatPitch(
        const Health& health,
        const PlayerHeartbeatAudioApplicationSettings& settings);
float PlayerLowHealthMovementSpeedScale(
        const Health& health,
        const PlayerLowHealthMovementApplicationSettings& settings);
float PlayerLowHealthCameraStrength(
        const Health& health,
        const PlayerLowHealthCameraApplicationSettings& settings);
void ClearPlayerLowHealthCamera(PlayerLowHealthCameraState& state);
void UpdatePlayerLowHealthCamera(
        PlayerLowHealthCameraState& state,
        const PlayerLowHealthCameraApplicationSettings& settings,
        const Health& health,
        float dt);

} // namespace game
