#include "sector_demo/SectorFpsController.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float MouseRadiansPerPixel = 0.0030f;
constexpr float PitchLimitRadians = 1.55334306f;
constexpr float FloorTransitionEpsilon = 0.001f;

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
    config.gravity = settings.gravity;
    config.playerRadius = settings.playerRadius;
    config.playerHeight = settings.playerHeight;
    config.stepHeight = settings.stepHeight;
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
    settings.gravity = config.gravity;
    settings.playerRadius = config.playerRadius;
    settings.playerHeight = config.playerHeight;
    settings.stepHeight = config.stepHeight;
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
            ClampSectorFpsPitch(pose.pitchRadians),
            0,
            false,
            0.0f
    };
}

void UpdateSectorFpsMouseLook(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input)
{
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    if (input.mouseLookEnabled) {
        state.yawRadians += input.mouseDelta.x * MouseRadiansPerPixel * normalized.mouseSensitivity;
        state.pitchRadians -= input.mouseDelta.y * MouseRadiansPerPixel * normalized.mouseSensitivity;
        state.pitchRadians = ClampSectorFpsPitch(state.pitchRadians);
    }
}

Vector2 ComputeSectorFpsHorizontalMovementDelta(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        float dt)
{
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
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
        movement = Vector3Scale(movement, speed * dt);
        return Vector2{movement.x, movement.z};
    }
    return Vector2{};
}

void UpdateSectorFpsController(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        float dt)
{
    UpdateSectorFpsMouseLook(state, config, input);
    const Vector2 movement = ComputeSectorFpsHorizontalMovementDelta(state, config, input, dt);
    state.feetPosition.x += movement.x;
    state.feetPosition.z += movement.y;
}

SectorFpsVerticalResult UpdateSectorFpsVerticalPhysics(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsVerticalContext& context,
        float dt)
{
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    SectorFpsVerticalResult result;
    result.hasSector = context.hasSector;
    result.floorZ = context.floorZ;
    result.ceilingZ = context.ceilingZ;

    if (!context.hasSector) {
        state.grounded = false;
        return result;
    }

    const float maxFeetY = context.ceilingZ - normalized.playerHeight;
    const bool canFit = maxFeetY >= context.floorZ;
    if (!canFit) {
        state.feetPosition.y = context.floorZ;
        state.grounded = true;
        state.verticalVelocity = 0.0f;
        result.cannotFit = true;
        result.transition = SectorFpsVerticalTransition::CannotFit;
        return result;
    }

    if (state.grounded) {
        const float floorDelta = context.floorZ - state.feetPosition.y;
        if (std::fabs(floorDelta) <= FloorTransitionEpsilon) {
            state.feetPosition.y = context.floorZ;
            state.verticalVelocity = 0.0f;
            result.transition = SectorFpsVerticalTransition::StayedGrounded;
            return result;
        }
        if (floorDelta > 0.0f) {
            if (floorDelta <= normalized.stepHeight + FloorTransitionEpsilon) {
                state.feetPosition.y = context.floorZ;
                state.verticalVelocity = 0.0f;
                result.transition = SectorFpsVerticalTransition::SteppedUp;
                return result;
            }
            state.verticalVelocity = 0.0f;
            result.transition = SectorFpsVerticalTransition::BlockedStep;
            return result;
        }

        const float drop = -floorDelta;
        if (drop <= normalized.stepHeight + FloorTransitionEpsilon) {
            state.feetPosition.y = context.floorZ;
            state.verticalVelocity = 0.0f;
            result.transition = SectorFpsVerticalTransition::SnappedDown;
            return result;
        }

        state.grounded = false;
        state.verticalVelocity = 0.0f;
        result.transition = SectorFpsVerticalTransition::StartedDrop;
        return result;
    } else if (normalized.gravity > 0.0f && dt > 0.0f) {
        state.verticalVelocity -= normalized.gravity * dt;
        state.feetPosition.y += state.verticalVelocity * dt;
    }

    if (state.feetPosition.y <= context.floorZ) {
        state.feetPosition.y = context.floorZ;
        state.grounded = true;
        state.verticalVelocity = 0.0f;
        result.transition = SectorFpsVerticalTransition::Landed;
    }

    if (state.feetPosition.y > maxFeetY) {
        state.feetPosition.y = maxFeetY;
        if (state.verticalVelocity > 0.0f) {
            state.verticalVelocity = 0.0f;
        }
    }

    return result;
}

} // namespace game
