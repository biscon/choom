#include "game/PlayerFlashlight.h"

#include "engine/render/ColorTransfer.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr float ForwardOffsetWorld = 0.05f;
constexpr int ReservedShadowPriority = 1000000;

bool Finite(float value)
{
    return std::isfinite(value);
}

Vector3 NormalizeOr(Vector3 value, Vector3 fallback)
{
    return Vector3LengthSqr(value) > 0.00000001f
            ? Vector3Normalize(value)
            : fallback;
}

} // namespace

void SetPlayerFlashlightEnabled(PlayerFlashlightState& state, bool enabled)
{
    if (state.enabled == enabled) return;
    state.enabled = enabled;
    state.directionValid = false;
}

void TogglePlayerFlashlight(PlayerFlashlightState& state)
{
    SetPlayerFlashlightEnabled(state, !state.enabled);
}

bool UpdatePlayerFlashlight(
        PlayerFlashlightState& state,
        const PlayerFlashlightApplicationSettings& rawSettings,
        const Camera3D& camera,
        int ownerSectorId,
        float dt,
        SectorPreviewDynamicPointLightSource& outLight,
        SectorLightAtmosphereSource& outAtmosphere)
{
    outLight = {};
    outAtmosphere = {};
    if (!state.enabled) {
        state.directionValid = false;
        return false;
    }

    const PlayerFlashlightApplicationSettings settings =
            NormalizePlayerFlashlightSettings(rawSettings);
    const Vector3 cameraDirection = NormalizeOr(
            Vector3Subtract(camera.target, camera.position),
            Vector3{0.0f, 0.0f, -1.0f});
    if (!state.directionValid || settings.aimResponseSeconds <= 0.0f
            || !Finite(dt) || dt <= 0.0f) {
        state.smoothedDirection = cameraDirection;
        state.directionValid = true;
    } else {
        const float response = std::max(0.0001f, settings.aimResponseSeconds);
        const float alpha = 1.0f - std::exp(-dt / response);
        state.smoothedDirection = NormalizeOr(
                Vector3Lerp(state.smoothedDirection, cameraDirection, alpha),
                cameraDirection);
    }

    const Vector3 cameraUp = NormalizeOr(camera.up, Vector3{0.0f, 1.0f, 0.0f});
    const Vector3 origin = Vector3Add(
            Vector3Add(camera.position,
                    Vector3Scale(cameraUp, settings.heightAboveEyeWorld)),
            Vector3Scale(cameraDirection, ForwardOffsetWorld));
    const float outerHalfAngle = std::atan2(
            settings.coneRadiusWorld, settings.reachWorld);
    const float innerHalfAngle = outerHalfAngle
            * std::clamp(1.0f - settings.edgeSoftness, 0.05f, 0.98f);

    outLight.lightId = PlayerFlashlightRuntimeLightId;
    outLight.ownerSectorId = ownerSectorId;
    outLight.light.lightId = outLight.lightId;
    outLight.light.kind = SectorPreviewDynamicLightKind::Spot;
    outLight.light.position = origin;
    outLight.light.direction = state.smoothedDirection;
    outLight.light.color = engine::SrgbColorBytesToLinearSceneRgb(settings.tint);
    outLight.light.radius = settings.reachWorld;
    outLight.light.innerConeCos = std::cos(innerHalfAngle);
    outLight.light.outerConeCos = std::cos(outerHalfAngle);
    outLight.light.intensity = settings.intensity;
    outLight.light.selectionFadeMultiplier = 1.0f;
    outLight.light.selectionFadeEnabled = false;
    outLight.light.castsShadow = true;
    outLight.light.shadowPriority = ReservedShadowPriority;
    outLight.light.shadowStrength = 1.0f;
    outLight.light.shadowSoftness = settings.shadowSoftness;
    outLight.light.profile = SectorDynamicLightProfile::Flashlight;
    outLight.light.profileParameters = Vector3{
            settings.hotspotRadiusRatio,
            settings.spillBrightness,
            settings.edgeSoftness};
    outLight.light.reserveSelection = true;
    outLight.light.reserveShadow = true;

    outAtmosphere.kind = SectorLightAtmosphereSourceKind::DynamicSpot;
    outAtmosphere.shape = SectorLightAtmosphereShape::Cone;
    outAtmosphere.lightId = outLight.lightId;
    outAtmosphere.ownerSectorId = ownerSectorId;
    outAtmosphere.positionWorld = origin;
    outAtmosphere.directionWorld = state.smoothedDirection;
    outAtmosphere.rangeWorld = settings.reachWorld;
    outAtmosphere.outerConeCos = outLight.light.outerConeCos;
    outAtmosphere.color = settings.tint;
    outAtmosphere.intensity = settings.intensity;
    outAtmosphere.atmosphere.proxy.shaft.enabled = settings.beamHaze > 0.0f;
    outAtmosphere.atmosphere.proxy.shaft.lengthScale = 1.0f;
    outAtmosphere.atmosphere.proxy.shaft.widthScale = 1.0f;
    outAtmosphere.atmosphere.proxy.shaft.brightness = settings.beamHaze;
    outAtmosphere.atmosphere.proxy.shaft.maxExtinction = 0.08f;
    outAtmosphere.atmosphere.proxy.shaft.edgeSoftness =
            std::clamp(settings.edgeSoftness * 2.0f, 0.05f, 1.0f);
    outAtmosphere.atmosphere.proxy.shaft.scatteringTint = settings.tint;
    return true;
}

} // namespace game
