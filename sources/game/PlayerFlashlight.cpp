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
        SectorPreviewDynamicPointLightSource& outLight)
{
    outLight = {};
    if (!state.enabled) {
        state.directionValid = false;
        return false;
    }

    const PlayerFlashlightApplicationSettings settings =
            NormalizePlayerFlashlightSettings(rawSettings);
    const Vector3 cameraDirection = NormalizeOr(
            Vector3Subtract(camera.target, camera.position),
            Vector3{0.0f, 0.0f, -1.0f});
    const Vector3 cameraUp = NormalizeOr(camera.up, Vector3{0.0f, 1.0f, 0.0f});
    const Vector3 cameraRight = NormalizeOr(
            Vector3CrossProduct(cameraDirection, cameraUp),
            Vector3{1.0f, 0.0f, 0.0f});
    const Vector3 origin = Vector3Add(
            Vector3Add(
                    Vector3Add(camera.position,
                            Vector3Scale(cameraUp,
                                    settings.heightAboveEyeWorld)),
                    Vector3Scale(cameraRight,
                            settings.lateralOffsetWorld)),
            Vector3Scale(cameraDirection, ForwardOffsetWorld));
    const Vector3 convergenceTarget = Vector3Add(
            camera.position,
            Vector3Scale(cameraDirection,
                    settings.aimConvergenceDistanceWorld));
    const Vector3 targetDirection = NormalizeOr(
            Vector3Subtract(convergenceTarget, origin),
            cameraDirection);
    if (!state.directionValid || settings.aimResponseSeconds <= 0.0f
            || !Finite(dt) || dt <= 0.0f) {
        state.smoothedDirection = targetDirection;
        state.directionValid = true;
    } else {
        const float response = std::max(0.0001f, settings.aimResponseSeconds);
        const float alpha = 1.0f - std::exp(-dt / response);
        state.smoothedDirection = NormalizeOr(
                Vector3Lerp(state.smoothedDirection, targetDirection, alpha),
                targetDirection);
    }

    const float outerHalfAngle = std::atan2(
            settings.coneRadiusWorld, settings.reachWorld);

    outLight.lightId = PlayerFlashlightRuntimeLightId;
    outLight.ownerSectorId = ownerSectorId;
    outLight.light.lightId = outLight.lightId;
    outLight.light.kind = SectorPreviewDynamicLightKind::Spot;
    outLight.light.position = origin;
    outLight.light.direction = state.smoothedDirection;
    outLight.light.color = engine::SrgbColorBytesToLinearSceneRgb(settings.tint);
    outLight.light.radius = settings.reachWorld;
    // The flashlight profile owns its only edge transition. Matching the
    // generic inner and outer cones prevents the renderer from fading it twice.
    outLight.light.innerConeCos = std::cos(outerHalfAngle);
    outLight.light.outerConeCos = std::cos(outerHalfAngle);
    outLight.light.intensity = settings.intensity;
    outLight.light.selectionFadeMultiplier = 1.0f;
    outLight.light.selectionFadeEnabled = false;
    outLight.light.castsShadow = settings.castsShadows;
    outLight.light.shadowPriority = ReservedShadowPriority;
    outLight.light.shadowStrength = settings.shadowStrength;
    outLight.light.shadowSoftness = settings.shadowSoftness;
    // Flashlight receivers interpret this profile-specific bias in world units.
    outLight.light.shadowBias = settings.shadowContactOffsetWorld;
    outLight.light.profile = SectorDynamicLightProfile::Flashlight;
    outLight.light.profileParameters = Vector3{
            settings.hotspotRadiusRatio,
            settings.spillBrightness,
            settings.edgeSoftness};
    outLight.light.reserveSelection = true;
    outLight.light.reserveShadow = settings.castsShadows;
    return true;
}

} // namespace game
