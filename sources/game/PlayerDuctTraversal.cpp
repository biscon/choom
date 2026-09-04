#include "game/PlayerDuctTraversal.h"

#include <algorithm>
#include <cmath>

namespace game {

std::string PlayerDuctTraversalSettingsError(
        const PlayerDuctTraversalApplicationSettings& settings)
{
    const float values[] = {
            settings.interactionDistanceWorld,
            settings.enterTransitionSeconds,
            settings.exitTransitionSeconds,
            settings.crawlSpeedWorld,
            settings.crawlRadiusWorld,
            settings.crawlHeightWorld,
            settings.crawlEyeHeightWorld};
    for (float value : values) {
        if (!std::isfinite(value) || value <= 0.0f) {
            return "All duct values must be finite and greater than zero.";
        }
    }
    if (settings.crawlEyeHeightWorld >= settings.crawlHeightWorld) {
        return "Crawl eye height must be lower than crawl collider height.";
    }
    return {};
}

PlayerDuctTraversalApplicationSettings NormalizePlayerDuctTraversalSettings(
        PlayerDuctTraversalApplicationSettings settings)
{
    const PlayerDuctTraversalApplicationSettings defaults;
    if (!PlayerDuctTraversalSettingsError(settings).empty()) return defaults;
    settings.interactionDistanceWorld = std::clamp(
            settings.interactionDistanceWorld, 0.25f, 10.0f);
    settings.enterTransitionSeconds = std::clamp(
            settings.enterTransitionSeconds, 0.05f, 5.0f);
    settings.exitTransitionSeconds = std::clamp(
            settings.exitTransitionSeconds, 0.05f, 5.0f);
    settings.crawlSpeedWorld = std::clamp(
            settings.crawlSpeedWorld, 0.1f, 20.0f);
    settings.crawlRadiusWorld = std::clamp(
            settings.crawlRadiusWorld, 0.05f, 1.0f);
    settings.crawlHeightWorld = std::clamp(
            settings.crawlHeightWorld, 0.1f, 2.0f);
    settings.crawlEyeHeightWorld = std::clamp(
            settings.crawlEyeHeightWorld, 0.02f,
            settings.crawlHeightWorld - 0.01f);
    return settings;
}

} // namespace game
