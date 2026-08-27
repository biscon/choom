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

struct PlayerHealthApplicationSettings {
    PlayerLowHealthVisualApplicationSettings lowHealthVisual;
};

std::string PlayerHealthSettingsError(
        const PlayerHealthApplicationSettings& settings);
float PlayerHealthRatio(const Health& health);
float PlayerLowHealthVisualStrength(
        const Health& health,
        const PlayerLowHealthVisualApplicationSettings& settings);

} // namespace game
