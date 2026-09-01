#pragma once

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>

namespace game {

struct PlayerFlashlightApplicationSettings {
    float intensity = 4.0f;
    float reachWorld = 18.0f;
    float coneRadiusWorld = 5.0f;
    Color tint = Color{225, 238, 255, 255};
    float hotspotRadiusRatio = 0.30f;
    float spillBrightness = 0.22f;
    float edgeSoftness = 0.18f;
    float beamHaze = 0.04f;
    float shadowSoftness = 1.5f;
    float shadowContactOffsetWorld = 0.005f;
    float heightAboveEyeWorld = 0.12f;
    float lateralOffsetWorld = 0.10f;
    float aimConvergenceDistanceWorld = 10.0f;
    float aimResponseSeconds = 0.055f;
};

inline PlayerFlashlightApplicationSettings NormalizePlayerFlashlightSettings(
        PlayerFlashlightApplicationSettings settings)
{
    const PlayerFlashlightApplicationSettings defaults;
    const auto finiteOr = [](float value, float fallback) {
        return std::isfinite(value) ? value : fallback;
    };
    settings.intensity = std::clamp(
            finiteOr(settings.intensity, defaults.intensity), 0.1f, 32.0f);
    settings.reachWorld = std::clamp(
            finiteOr(settings.reachWorld, defaults.reachWorld), 1.0f, 64.0f);
    settings.coneRadiusWorld = std::clamp(
            finiteOr(settings.coneRadiusWorld, defaults.coneRadiusWorld),
            0.25f,
            std::min(32.0f,
                    settings.reachWorld * std::tan(60.0f * DEG2RAD)));
    settings.hotspotRadiusRatio = std::clamp(
            finiteOr(settings.hotspotRadiusRatio,
                    defaults.hotspotRadiusRatio),
            0.05f, 0.90f);
    settings.spillBrightness = std::clamp(
            finiteOr(settings.spillBrightness, defaults.spillBrightness),
            0.0f, 1.0f);
    settings.edgeSoftness = std::clamp(
            finiteOr(settings.edgeSoftness, defaults.edgeSoftness),
            0.02f, 0.50f);
    settings.beamHaze = std::clamp(
            finiteOr(settings.beamHaze, defaults.beamHaze), 0.0f, 0.25f);
    settings.shadowSoftness = std::clamp(
            finiteOr(settings.shadowSoftness, defaults.shadowSoftness),
            0.0f, 4.0f);
    settings.shadowContactOffsetWorld = std::clamp(
            finiteOr(settings.shadowContactOffsetWorld,
                    defaults.shadowContactOffsetWorld),
            0.0f, 0.05f);
    settings.heightAboveEyeWorld = std::clamp(
            finiteOr(settings.heightAboveEyeWorld,
                    defaults.heightAboveEyeWorld),
            -0.25f, 0.50f);
    settings.lateralOffsetWorld = std::clamp(
            finiteOr(settings.lateralOffsetWorld,
                    defaults.lateralOffsetWorld),
            -0.25f, 0.25f);
    settings.aimConvergenceDistanceWorld = std::clamp(
            finiteOr(settings.aimConvergenceDistanceWorld,
                    defaults.aimConvergenceDistanceWorld),
            1.0f, 64.0f);
    settings.aimResponseSeconds = std::clamp(
            finiteOr(settings.aimResponseSeconds,
                    defaults.aimResponseSeconds),
            0.0f, 0.25f);
    settings.tint.a = 255;
    return settings;
}

inline std::string PlayerFlashlightSettingsError(
        const PlayerFlashlightApplicationSettings& settings)
{
    const PlayerFlashlightApplicationSettings normalized =
            NormalizePlayerFlashlightSettings(settings);
    if (settings.intensity != normalized.intensity) return "intensity must be between 0.1 and 32";
    if (settings.reachWorld != normalized.reachWorld) return "reachWorld must be between 1 and 64";
    if (settings.coneRadiusWorld != normalized.coneRadiusWorld) return "coneRadiusWorld must be between 0.25 and the 60 degree cone limit";
    if (settings.hotspotRadiusRatio != normalized.hotspotRadiusRatio) return "hotspotRadiusRatio must be between 0.05 and 0.9";
    if (settings.spillBrightness != normalized.spillBrightness) return "spillBrightness must be between 0 and 1";
    if (settings.edgeSoftness != normalized.edgeSoftness) return "edgeSoftness must be between 0.02 and 0.5";
    if (settings.beamHaze != normalized.beamHaze) return "beamHaze must be between 0 and 0.25";
    if (settings.shadowSoftness != normalized.shadowSoftness) return "shadowSoftness must be between 0 and 4";
    if (settings.shadowContactOffsetWorld != normalized.shadowContactOffsetWorld) return "shadowContactOffsetWorld must be between 0 and 0.05";
    if (settings.heightAboveEyeWorld != normalized.heightAboveEyeWorld) return "heightAboveEyeWorld must be between -0.25 and 0.5";
    if (settings.lateralOffsetWorld != normalized.lateralOffsetWorld) return "lateralOffsetWorld must be between -0.25 and 0.25";
    if (settings.aimConvergenceDistanceWorld != normalized.aimConvergenceDistanceWorld) return "aimConvergenceDistanceWorld must be between 1 and 64";
    if (settings.aimResponseSeconds != normalized.aimResponseSeconds) return "aimResponseSeconds must be between 0 and 0.25";
    if (settings.tint.a != 255) return "tint alpha must be 255";
    return {};
}

} // namespace game
