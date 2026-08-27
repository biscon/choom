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
    assert(Near(game::PlayerHealthRatio(game::Health{100, 100, 150}), 1.0f));
    assert(Near(game::PlayerHealthRatio(game::Health{100, 100, -10}), 0.0f));
    assert(Near(game::PlayerHealthRatio(game::Health{100, 0, 0}), 0.0f));

    game::PlayerLowHealthVisualApplicationSettings disabled = visual;
    disabled.enabled = false;
    assert(Near(game::PlayerLowHealthVisualStrength(
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
}
