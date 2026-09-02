#include "game/PlayerOxygen.h"

#include <algorithm>
#include <cmath>

namespace game {

PlayerOxygen MakePlayerOxygen(
        const PlayerLiquidApplicationSettings& settings,
        const PlayerOxygenModifiers& modifiers)
{
    PlayerOxygen oxygen;
    oxygen.baseMaximum = std::max(0.001f, settings.oxygenMaximum);
    oxygen.maximum = std::max(0.001f, oxygen.baseMaximum + modifiers.capacityBonus);
    oxygen.current = oxygen.maximum;
    return oxygen;
}

void ApplyPlayerOxygenModifiers(
        PlayerOxygen& oxygen,
        const PlayerOxygenModifiers& modifiers)
{
    oxygen.baseMaximum = std::max(0.001f, oxygen.baseMaximum);
    oxygen.maximum = std::max(0.001f, oxygen.baseMaximum + modifiers.capacityBonus);
    oxygen.current = std::clamp(oxygen.current, 0.0f, oxygen.maximum);
}

float PlayerOxygenRatio(const PlayerOxygen& oxygen)
{
    return oxygen.maximum > 0.0f
            ? std::clamp(oxygen.current / oxygen.maximum, 0.0f, 1.0f)
            : 0.0f;
}

PlayerOxygenUpdateResult UpdatePlayerOxygen(
        PlayerOxygen& oxygen,
        const PlayerLiquidApplicationSettings& settings,
        const PlayerOxygenModifiers& modifiers,
        bool submerged,
        float dt)
{
    ApplyPlayerOxygenModifiers(oxygen, modifiers);
    PlayerOxygenUpdateResult result;
    if (!std::isfinite(dt) || dt <= 0.0f) {
        result.fullyRegenerated = oxygen.current >= oxygen.maximum;
        return result;
    }

    if (submerged) {
        const float depletionRate = std::max(
                0.0f, settings.oxygenDepletionPerSecond)
                * std::max(0.0f, modifiers.depletionMultiplier);
        const float previousOxygen = oxygen.current;
        const float depletion = depletionRate * dt;
        oxygen.current = std::max(0.0f, oxygen.current - depletion);
        if (oxygen.current <= 0.0f) {
            const float zeroOxygenSeconds = previousOxygen <= 0.0f
                    ? dt
                    : (depletionRate > 0.0f
                            ? std::max(0.0f, dt - previousOxygen / depletionRate)
                            : 0.0f);
            oxygen.drowningTimerSeconds += zeroOxygenSeconds;
            const float interval = std::max(
                    0.001f, settings.drowningDamageIntervalSeconds);
            while (oxygen.drowningTimerSeconds >= interval) {
                oxygen.drowningTimerSeconds -= interval;
                result.drowningDamage += std::max(0, settings.drowningDamage);
            }
        } else {
            oxygen.drowningTimerSeconds = 0.0f;
        }
    } else {
        oxygen.drowningTimerSeconds = 0.0f;
        const float regeneration = std::max(0.0f, settings.oxygenRegenerationPerSecond)
                * std::max(0.0f, modifiers.regenerationMultiplier) * dt;
        oxygen.current = std::min(oxygen.maximum, oxygen.current + regeneration);
    }
    result.fullyRegenerated = oxygen.current >= oxygen.maximum;
    return result;
}

} // namespace game
