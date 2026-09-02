#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace game {

struct PlayerSneakApplicationSettings {
    float fullVisibilityLightLevel = 1.0f;
    float darknessCutoffNormalized = 0.05f;
    float lightHalfResponseRangeNormalized = 0.05f;
    float visualDetectionBuildSeconds = 0.75f;
    float visualDetectionDecaySeconds = 1.25f;
    float darknessProximityRangeWorld = 4.0f;
    float crouchVisualDetectionMultiplier = 0.85f;
    float crouchMovementNoiseMultiplier = 0.25f;
};

inline std::string PlayerSneakSettingsError(
        const PlayerSneakApplicationSettings& settings)
{
    if (!std::isfinite(settings.fullVisibilityLightLevel)
            || settings.fullVisibilityLightLevel <= 0.0f
            || settings.fullVisibilityLightLevel > 1000.0f) {
        return "fullVisibilityLightLevel must be between 0 and 1000";
    }
    if (!std::isfinite(settings.darknessCutoffNormalized)
            || settings.darknessCutoffNormalized < 0.0f
            || settings.darknessCutoffNormalized >= 1.0f) {
        return "darknessCutoffNormalized must be at least 0 and less than 1";
    }
    if (!std::isfinite(settings.lightHalfResponseRangeNormalized)
            || settings.lightHalfResponseRangeNormalized <= 0.0f
            || settings.lightHalfResponseRangeNormalized
                    >= 1.0f - settings.darknessCutoffNormalized) {
        return "lightHalfResponseRangeNormalized must be greater than 0 and less than 1 - darknessCutoffNormalized";
    }
    if (!std::isfinite(settings.visualDetectionBuildSeconds)
            || settings.visualDetectionBuildSeconds <= 0.0f
            || settings.visualDetectionBuildSeconds > 600.0f) {
        return "visualDetectionBuildSeconds must be between 0 and 600";
    }
    if (!std::isfinite(settings.visualDetectionDecaySeconds)
            || settings.visualDetectionDecaySeconds <= 0.0f
            || settings.visualDetectionDecaySeconds > 600.0f) {
        return "visualDetectionDecaySeconds must be between 0 and 600";
    }
    if (!std::isfinite(settings.darknessProximityRangeWorld)
            || settings.darknessProximityRangeWorld < 0.0f
            || settings.darknessProximityRangeWorld > 1000.0f) {
        return "darknessProximityRangeWorld must be between 0 and 1000";
    }
    if (!std::isfinite(settings.crouchVisualDetectionMultiplier)
            || settings.crouchVisualDetectionMultiplier < 0.0f
            || settings.crouchVisualDetectionMultiplier > 1.0f) {
        return "crouchVisualDetectionMultiplier must be between 0 and 1";
    }
    if (!std::isfinite(settings.crouchMovementNoiseMultiplier)
            || settings.crouchMovementNoiseMultiplier < 0.0f
            || settings.crouchMovementNoiseMultiplier > 1.0f) {
        return "crouchMovementNoiseMultiplier must be between 0 and 1";
    }
    return {};
}

inline float PlayerSneakLightDetectionFactor(
        float normalizedLightLevel,
        float darknessCutoffNormalized,
        float lightHalfResponseRangeNormalized)
{
    const float light = std::isfinite(normalizedLightLevel)
            ? std::clamp(normalizedLightLevel, 0.0f, 1.0f)
            : 0.0f;
    const float cutoff = std::isfinite(darknessCutoffNormalized)
            ? std::clamp(darknessCutoffNormalized, 0.0f, 0.9999f)
            : 0.0f;
    const float availableRange = 1.0f - cutoff;
    const float halfResponseRange =
            std::isfinite(lightHalfResponseRangeNormalized)
                    ? std::clamp(
                            lightHalfResponseRangeNormalized,
                            0.000001f,
                            std::max(0.000001f,
                                    availableRange - 0.000001f))
                    : std::min(0.05f, availableRange * 0.5f);
    const float responsePosition = std::clamp(
            (light - cutoff) / (1.0f - cutoff), 0.0f, 1.0f);
    if (responsePosition <= 0.0f) return 0.0f;
    if (responsePosition >= 1.0f) return 1.0f;

    const float halfResponsePosition = std::clamp(
            halfResponseRange / availableRange,
            0.000001f,
            0.999999f);
    const float exponent = std::log(0.5f)
            / std::log1p(-halfResponsePosition);
    const float factor = -std::expm1(
            exponent * std::log1p(-responsePosition));
    return std::clamp(factor, 0.0f, 1.0f);
}

inline float PlayerSneakCrouchVisualMultiplier(
        const PlayerSneakApplicationSettings& settings,
        float crouchBlend)
{
    const float blend = std::isfinite(crouchBlend)
            ? std::clamp(crouchBlend, 0.0f, 1.0f)
            : 0.0f;
    return 1.0f + (settings.crouchVisualDetectionMultiplier - 1.0f) * blend;
}

inline float PlayerSneakVisualLightLevel(
        float sampledNormalizedLight,
        bool flashlightEnabled)
{
    return flashlightEnabled
            ? 1.0f
            : (std::isfinite(sampledNormalizedLight)
                    ? std::clamp(sampledNormalizedLight, 0.0f, 1.0f)
                    : 0.0f);
}

inline float PlayerSneakVisualCrouchBlend(
        float crouchBlend,
        bool flashlightEnabled)
{
    return flashlightEnabled
            ? 0.0f
            : (std::isfinite(crouchBlend)
                    ? std::clamp(crouchBlend, 0.0f, 1.0f)
                    : 0.0f);
}

inline float PlayerSneakProximityDetectionFactor(
        float distanceWorld,
        float proximityRangeWorld)
{
    if (!std::isfinite(distanceWorld)
            || !std::isfinite(proximityRangeWorld)
            || proximityRangeWorld <= 0.0f) return 0.0f;
    return std::clamp(
            1.0f - std::max(0.0f, distanceWorld) / proximityRangeWorld,
            0.0f,
            1.0f);
}

inline float PlayerSneakMovementNoiseMultiplier(
        const PlayerSneakApplicationSettings& settings,
        float crouchBlend)
{
    const float blend = std::isfinite(crouchBlend)
            ? std::clamp(crouchBlend, 0.0f, 1.0f)
            : 0.0f;
    return 1.0f + (settings.crouchMovementNoiseMultiplier - 1.0f) * blend;
}

struct PlayerVisualDetectionStep {
    float progress = 0.0f;
    float lightFactor = 0.0f;
    float proximityFactor = 0.0f;
    float visibilityFactor = 0.0f;
    float rateFactor = 0.0f;
    bool detected = false;
    bool building = false;
};

inline PlayerVisualDetectionStep AdvancePlayerVisualDetection(
        float progress,
        bool geometricSight,
        float playerDistanceWorld,
        float normalizedLightLevel,
        float crouchBlend,
        const PlayerSneakApplicationSettings& settings,
        float dt)
{
    PlayerVisualDetectionStep result;
    result.progress = std::isfinite(progress)
            ? std::clamp(progress, 0.0f, 1.0f)
            : 0.0f;
    const float safeDt = std::isfinite(dt) ? std::max(0.0f, dt) : 0.0f;
    result.lightFactor = PlayerSneakLightDetectionFactor(
            normalizedLightLevel,
            settings.darknessCutoffNormalized,
            settings.lightHalfResponseRangeNormalized);
    result.proximityFactor = PlayerSneakProximityDetectionFactor(
            playerDistanceWorld,
            settings.darknessProximityRangeWorld);
    result.visibilityFactor = std::max(
            result.lightFactor, result.proximityFactor);
    result.rateFactor = result.visibilityFactor
            * PlayerSneakCrouchVisualMultiplier(settings, crouchBlend);
    result.building = geometricSight && result.rateFactor > 0.0f;
    if (result.building) {
        result.progress = std::min(
                1.0f,
                result.progress + safeDt * result.rateFactor
                        / settings.visualDetectionBuildSeconds);
    } else {
        result.progress = std::max(
                0.0f,
                result.progress - safeDt
                        / settings.visualDetectionDecaySeconds);
    }
    result.detected = result.progress >= 1.0f;
    return result;
}

} // namespace game
