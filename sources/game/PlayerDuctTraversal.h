#pragma once

#include <string>

namespace game {

struct PlayerDuctTraversalApplicationSettings {
    float interactionDistanceWorld = 1.75f;
    float enterTransitionSeconds = 0.55f;
    float exitTransitionSeconds = 0.45f;
    float crawlSpeedWorld = 1.5f;
    float crawlRadiusWorld = 0.20f;
    float crawlHeightWorld = 0.40f;
    float crawlEyeHeightWorld = 0.32f;
};

PlayerDuctTraversalApplicationSettings NormalizePlayerDuctTraversalSettings(
        PlayerDuctTraversalApplicationSettings settings);
std::string PlayerDuctTraversalSettingsError(
        const PlayerDuctTraversalApplicationSettings& settings);

} // namespace game
