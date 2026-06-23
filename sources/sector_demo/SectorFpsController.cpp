#include "sector_demo/SectorFpsController.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float MouseRadiansPerPixel = 0.0030f;
constexpr float PitchLimitRadians = 1.55334306f;

float ClampFinite(float value, float fallback, float minValue, float maxValue)
{
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

} // namespace

SectorFpsControllerConfig DefaultSectorFpsControllerConfig()
{
    return SectorFpsControllerConfigFromPreviewSettings(DefaultSectorPreviewSettings());
}

SectorFpsControllerConfig NormalizeSectorFpsControllerConfig(SectorFpsControllerConfig config)
{
    config = SectorFpsControllerConfigFromPreviewSettings(
            SectorPreviewSettingsFromFpsControllerConfig(config));
    return config;
}

SectorFpsControllerConfig SectorFpsControllerConfigFromPreviewSettings(
        SectorPreviewSettings settings)
{
    settings = NormalizeSectorPreviewSettings(settings);
    SectorFpsControllerConfig config;
    config.walkSpeed = settings.walkSpeed;
    config.runSpeed = settings.runSpeed;
    config.mouseSensitivity = settings.mouseSensitivity;
    config.eyeHeight = settings.eyeHeight;
    return config;
}

SectorPreviewSettings SectorPreviewSettingsFromFpsControllerConfig(
        SectorFpsControllerConfig config)
{
    SectorPreviewSettings settings;
    settings.walkSpeed = config.walkSpeed;
    settings.runSpeed = config.runSpeed;
    settings.mouseSensitivity = config.mouseSensitivity;
    settings.eyeHeight = config.eyeHeight;
    return NormalizeSectorPreviewSettings(settings);
}

float ClampSectorFpsPitch(float pitchRadians)
{
    return ClampFinite(pitchRadians, 0.0f, -PitchLimitRadians, PitchLimitRadians);
}

Vector3 SectorFpsControllerEyePosition(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config)
{
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    return Vector3Add(state.feetPosition, Vector3{0.0f, normalized.eyeHeight, 0.0f});
}

SectorMeshPreviewPose SectorFpsControllerPose(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config)
{
    return SectorMeshPreviewPose{
            SectorFpsControllerEyePosition(state, config),
            state.yawRadians,
            ClampSectorFpsPitch(state.pitchRadians)
    };
}

SectorFpsControllerState SectorFpsControllerStateFromCameraPose(
        const SectorMeshPreviewPose& pose,
        const SectorFpsControllerConfig& config)
{
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    return SectorFpsControllerState{
            Vector3Subtract(pose.position, Vector3{0.0f, normalized.eyeHeight, 0.0f}),
            pose.yawRadians,
            ClampSectorFpsPitch(pose.pitchRadians)
    };
}

void UpdateSectorFpsController(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        float dt)
{
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    if (input.mouseLookEnabled) {
        state.yawRadians += input.mouseDelta.x * MouseRadiansPerPixel * normalized.mouseSensitivity;
        state.pitchRadians -= input.mouseDelta.y * MouseRadiansPerPixel * normalized.mouseSensitivity;
        state.pitchRadians = ClampSectorFpsPitch(state.pitchRadians);
    }

    Vector3 forward{std::cos(state.yawRadians), 0.0f, std::sin(state.yawRadians)};
    Vector3 right{-forward.z, 0.0f, forward.x};
    Vector3 movement{};
    if (input.moveForward) {
        movement = Vector3Add(movement, forward);
    }
    if (input.moveBackward) {
        movement = Vector3Subtract(movement, forward);
    }
    if (input.strafeRight) {
        movement = Vector3Add(movement, right);
    }
    if (input.strafeLeft) {
        movement = Vector3Subtract(movement, right);
    }

    if (Vector3LengthSqr(movement) > 0.0001f && dt > 0.0f) {
        movement = Vector3Normalize(movement);
        const float speed = input.run ? normalized.runSpeed : normalized.walkSpeed;
        state.feetPosition = Vector3Add(state.feetPosition, Vector3Scale(movement, speed * dt));
    }
}

} // namespace game
