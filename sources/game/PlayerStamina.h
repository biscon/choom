#pragma once

namespace game {

struct PlayerWindedCameraApplicationSettings {
    bool enabled = true;
    float startThresholdRatio = 0.40f;
    float verticalAmplitudeWorld = 0.015f;
    float pitchAmplitudeDegrees = 0.60f;
    float frequencyHz = 0.45f;
    float responseSeconds = 0.35f;
};

struct PlayerBreathingAudioApplicationSettings {
    float thresholdRatio = 0.20f;
    float volume = 0.75f;
    float fadeOutSeconds = 2.0f;
};

struct PlayerStaminaApplicationSettings {
    float maximum = 100.0f;
    float sprintDrainPerSecond = 20.0f;
    float jumpCost = 20.0f;
    float regenerationPerSecond = 12.5f;
    float exhaustedRecoveryRatio = 0.20f;
    PlayerWindedCameraApplicationSettings windedCamera;
    PlayerBreathingAudioApplicationSettings breathingAudio;
};

struct PlayerStamina {
    float maximum = 100.0f;
    float current = 100.0f;
    bool exhausted = false;
};

struct PlayerWindedCameraState {
    float phase = 0.0f;
    float intensity = 0.0f;
    float verticalOffsetWorld = 0.0f;
    float pitchOffsetDegrees = 0.0f;
};

PlayerStamina MakePlayerStamina(
        const PlayerStaminaApplicationSettings& settings);
float PlayerStaminaRatio(const PlayerStamina& stamina);
bool CanPlayerStaminaSprint(const PlayerStamina& stamina);
bool CanPlayerStaminaJump(
        const PlayerStamina& stamina,
        const PlayerStaminaApplicationSettings& settings);
void UpdatePlayerStamina(
        PlayerStamina& stamina,
        const PlayerStaminaApplicationSettings& settings,
        bool sprinted,
        bool jumped,
        float dt);

void ClearPlayerWindedCamera(PlayerWindedCameraState& state);
void UpdatePlayerWindedCamera(
        PlayerWindedCameraState& state,
        const PlayerWindedCameraApplicationSettings& settings,
        float staminaRatio,
        float dt);

float AdvancePlayerBreathingAudioVolume(
        float currentVolume,
        const PlayerBreathingAudioApplicationSettings& settings,
        float staminaRatio,
        float dt);

} // namespace game
