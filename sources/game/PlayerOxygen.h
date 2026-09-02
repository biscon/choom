#pragma once

#include <string>

namespace game {

struct PlayerLiquidAudioApplicationSettings {
    std::string splashSoundPath = "player/water_splash.wav";
    std::string exitSoundPath;
    std::string swimLoopSoundPath = "player/swimming_loop.ogg";
    float underwaterMuffling = 0.65f;
};

struct PlayerLiquidVisualApplicationSettings {
    float screenDistortionStrength = 1.35f;
    float causticsStrength = 0.16f;
    float causticsScaleMultiplier = 1.0f;
    float causticsSpeedMultiplier = 1.0f;
};

struct PlayerLiquidApplicationSettings {
    float oxygenMaximum = 100.0f;
    float oxygenDepletionPerSecond = 5.0f;
    float oxygenRegenerationPerSecond = 20.0f;
    int drowningDamage = 10;
    float drowningDamageIntervalSeconds = 0.75f;
    float entrySlowdownSeconds = 0.20f;
    float swimCollisionHeightWorld = 0.60f;
    float waterDragPerSecond = 5.0f;
    float surfaceRecoveryFrequencyHz = 0.35f;
    float maximumExitLedgeHeightWorld = 0.75f;
    float exitTransitionDurationSeconds = 0.30f;
    PlayerLiquidAudioApplicationSettings audio;
    PlayerLiquidVisualApplicationSettings visuals;
};

struct PlayerOxygenModifiers {
    float capacityBonus = 0.0f;
    float depletionMultiplier = 1.0f;
    float regenerationMultiplier = 1.0f;
};

struct PlayerOxygen {
    float baseMaximum = 100.0f;
    float maximum = 100.0f;
    float current = 100.0f;
    float drowningTimerSeconds = 0.0f;
};

struct PlayerOxygenUpdateResult {
    int drowningDamage = 0;
    bool fullyRegenerated = true;
};

PlayerOxygen MakePlayerOxygen(
        const PlayerLiquidApplicationSettings& settings,
        const PlayerOxygenModifiers& modifiers = {});
void ApplyPlayerOxygenModifiers(
        PlayerOxygen& oxygen,
        const PlayerOxygenModifiers& modifiers);
float PlayerOxygenRatio(const PlayerOxygen& oxygen);
PlayerOxygenUpdateResult UpdatePlayerOxygen(
        PlayerOxygen& oxygen,
        const PlayerLiquidApplicationSettings& settings,
        const PlayerOxygenModifiers& modifiers,
        bool submerged,
        float dt);

} // namespace game
