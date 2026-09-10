#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace game {

struct PlayerCameraApplicationSettings {
    float mouseSensitivity = 1.0f;
    float smoothingStrength = 0.20f;
    float deadZonePixels = 0.0f;
    float maxTurnSpeedDegreesPerSecond = 360.0f;
};

inline PlayerCameraApplicationSettings NormalizePlayerCameraSettings(
        PlayerCameraApplicationSettings settings)
{
    const PlayerCameraApplicationSettings defaults;
    const auto clamp = [](float value, float low, float high, float fallback) {
        return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
    };
    settings.mouseSensitivity = clamp(settings.mouseSensitivity,
            0.0f, 5.0f, defaults.mouseSensitivity);
    settings.smoothingStrength = clamp(settings.smoothingStrength,
            0.0f, 1.0f, defaults.smoothingStrength);
    settings.deadZonePixels = clamp(settings.deadZonePixels,
            0.0f, 10.0f, defaults.deadZonePixels);
    settings.maxTurnSpeedDegreesPerSecond = clamp(
            settings.maxTurnSpeedDegreesPerSecond,
            0.0f, 1440.0f, defaults.maxTurnSpeedDegreesPerSecond);
    return settings;
}

inline bool SamePlayerCameraSettings(
        const PlayerCameraApplicationSettings& a,
        const PlayerCameraApplicationSettings& b)
{
    return a.mouseSensitivity == b.mouseSensitivity
            && a.smoothingStrength == b.smoothingStrength
            && a.deadZonePixels == b.deadZonePixels
            && a.maxTurnSpeedDegreesPerSecond == b.maxTurnSpeedDegreesPerSecond;
}

inline std::string PlayerCameraSettingsError(
        const PlayerCameraApplicationSettings& settings)
{
    const auto normalized = NormalizePlayerCameraSettings(settings);
    if (settings.mouseSensitivity != normalized.mouseSensitivity)
        return "mouseSensitivity must be between 0 and 5";
    if (settings.smoothingStrength != normalized.smoothingStrength)
        return "smoothingStrength must be between 0 and 1";
    if (settings.deadZonePixels != normalized.deadZonePixels)
        return "deadZonePixels must be between 0 and 10";
    if (settings.maxTurnSpeedDegreesPerSecond != normalized.maxTurnSpeedDegreesPerSecond)
        return "maxTurnSpeedDegreesPerSecond must be between 0 and 1440";
    return {};
}

// Editor camera controls must never overwrite the player's sensitivity.
inline void ApplyPlayerCameraAuthorSettings(
        PlayerCameraApplicationSettings& target,
        const PlayerCameraApplicationSettings& draft)
{
    target.smoothingStrength = draft.smoothingStrength;
    target.deadZonePixels = draft.deadZonePixels;
    target.maxTurnSpeedDegreesPerSecond = draft.maxTurnSpeedDegreesPerSecond;
}

} // namespace game
