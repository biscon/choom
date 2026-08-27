#include "game/PlayerHealthVisual.h"

#include <algorithm>
#include <cmath>

namespace game {

std::string PlayerHealthSettingsError(
        const PlayerHealthApplicationSettings& settings)
{
    const PlayerLowHealthVisualApplicationSettings& visual =
            settings.lowHealthVisual;
    const auto finite = [](float value) { return std::isfinite(value); };
    if (!finite(visual.thresholdRatio)
            || visual.thresholdRatio <= 0.0f
            || visual.thresholdRatio > 1.0f) {
        return "lowHealthVisual.thresholdRatio must be greater than 0 and at most 1";
    }
    if (!finite(visual.vignetteInnerRadius)
            || visual.vignetteInnerRadius < 0.0f
            || visual.vignetteInnerRadius > 2.0f) {
        return "lowHealthVisual.vignetteInnerRadius must be between 0 and 2";
    }
    if (!finite(visual.vignetteOuterRadius)
            || visual.vignetteOuterRadius < 0.0f
            || visual.vignetteOuterRadius > 2.0f) {
        return "lowHealthVisual.vignetteOuterRadius must be between 0 and 2";
    }
    if (visual.vignetteOuterRadius <= visual.vignetteInnerRadius) {
        return "lowHealthVisual.vignetteOuterRadius must be greater than vignetteInnerRadius";
    }
    if (!finite(visual.maximumVignetteOpacity)
            || visual.maximumVignetteOpacity < 0.0f
            || visual.maximumVignetteOpacity > 1.0f) {
        return "lowHealthVisual.maximumVignetteOpacity must be between 0 and 1";
    }
    if (!finite(visual.maximumDesaturation)
            || visual.maximumDesaturation < 0.0f
            || visual.maximumDesaturation > 1.0f) {
        return "lowHealthVisual.maximumDesaturation must be between 0 and 1";
    }
    return {};
}

float PlayerHealthRatio(const Health& health)
{
    if (health.maximum <= 0) return 0.0f;
    return std::clamp(
            static_cast<float>(health.current)
                    / static_cast<float>(health.maximum),
            0.0f,
            1.0f);
}

float PlayerLowHealthVisualStrength(
        const Health& health,
        const PlayerLowHealthVisualApplicationSettings& settings)
{
    if (!settings.enabled
            || !std::isfinite(settings.thresholdRatio)
            || settings.thresholdRatio <= 0.0f) {
        return 0.0f;
    }
    const float threshold = std::clamp(settings.thresholdRatio, 0.0f, 1.0f);
    const float deficit = std::clamp(
            (threshold - PlayerHealthRatio(health)) / threshold,
            0.0f,
            1.0f);
    return deficit * deficit * (3.0f - 2.0f * deficit);
}

float PlayerLowHealthVignetteOpacity(
        const Health& health,
        const PlayerLowHealthVisualApplicationSettings& settings)
{
    const float authoredOpacity = std::clamp(
            settings.maximumVignetteOpacity
                    * PlayerLowHealthVisualStrength(health, settings),
            0.0f,
            1.0f);
    // Treat the authored value as an artist-facing intensity. A quadratic
    // ease-out makes injury readable earlier and lets high values feel
    // substantially less transparent without changing the vignette shape.
    return authoredOpacity * (2.0f - authoredOpacity);
}

} // namespace game
