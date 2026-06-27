#include "sector_demo/SectorFpsController.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float MouseRadiansPerPixel = 0.0030f;
constexpr float PitchLimitRadians = 1.55334306f;
constexpr float FloorTransitionEpsilon = 0.001f;
constexpr float StepSmoothingRate = 16.0f;
constexpr float VisualStepOffsetEpsilon = 0.0001f;
constexpr float HeadBobBlendRate = 12.0f;
constexpr float HeadBobOffsetEpsilon = 0.0001f;
constexpr float HeadBobPhaseWrap = 100000.0f;
constexpr float MinLandingImpactSpeed = 0.5f;
constexpr float FullLandingDipImpactSpeed = 12.0f;
constexpr float MaxLandingDip = 0.45f;
constexpr float LandingDipRecoveryRate = 7.0f;
constexpr float LandingDipCurvePower = 2.25f;
constexpr float LandingDipOffsetEpsilon = 0.0001f;
constexpr float TwoPi = 6.28318530717958647692f;

float ClampFinite(float value, float fallback, float minValue, float maxValue)
{
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

} // namespace

float DefaultSectorFpsStepSmoothingRate()
{
    return StepSmoothingRate;
}

float DefaultSectorFpsHeadBobBlendRate()
{
    return HeadBobBlendRate;
}

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
    config.jumpHeight = settings.jumpHeight;
    config.headBobStrength = settings.headBobStrength;
    config.headBobFrequency = settings.headBobFrequency;
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
    settings.jumpHeight = config.jumpHeight;
    settings.headBobStrength = config.headBobStrength;
    settings.headBobFrequency = config.headBobFrequency;
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

SectorViewPose SectorFpsControllerPose(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config)
{
    return SectorViewPose{
            SectorFpsControllerEyePosition(state, config),
            state.yawRadians,
            ClampSectorFpsPitch(state.pitchRadians)
    };
}

SectorViewPose SectorFpsControllerVisualPose(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        float visualStepOffsetY)
{
    return SectorFpsControllerVisualPose(
            state,
            config,
            visualStepOffsetY,
            Vector3{});
}

SectorViewPose SectorFpsControllerVisualPose(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        float visualStepOffsetY,
        Vector3 headBobOffset)
{
    return SectorFpsControllerVisualPose(
            state,
            config,
            visualStepOffsetY,
            headBobOffset,
            0.0f);
}

SectorViewPose SectorFpsControllerVisualPose(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        float visualStepOffsetY,
        Vector3 headBobOffset,
        float landingDipOffsetY)
{
    SectorViewPose pose = SectorFpsControllerPose(state, config);
    pose.position.y += visualStepOffsetY;
    pose.position = Vector3Add(pose.position, headBobOffset);
    pose.position.y += landingDipOffsetY;
    return pose;
}

SectorFpsControllerState SectorFpsControllerStateFromCameraPose(
        const SectorViewPose& pose,
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

bool SectorFpsTransitionStartsVisualStepSmoothing(SectorFpsVerticalTransition transition)
{
    return transition == SectorFpsVerticalTransition::SteppedUp
            || transition == SectorFpsVerticalTransition::SnappedDown;
}

bool SectorFpsTransitionClearsVisualStepSmoothing(SectorFpsVerticalTransition transition)
{
    return transition == SectorFpsVerticalTransition::StartedDrop
            || transition == SectorFpsVerticalTransition::Landed
            || transition == SectorFpsVerticalTransition::CeilingBonk
            || transition == SectorFpsVerticalTransition::CannotFit;
}

float CaptureSectorFpsVisualStepOffset(
        float previousVisualEyeY,
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config)
{
    return previousVisualEyeY - SectorFpsControllerEyePosition(state, config).y;
}

void DecaySectorFpsVisualStepOffset(
        float& visualStepOffsetY,
        float smoothingRate,
        float dt)
{
    if (visualStepOffsetY == 0.0f) {
        return;
    }
    if (smoothingRate <= 0.0f || dt <= 0.0f) {
        return;
    }

    visualStepOffsetY *= std::exp(-smoothingRate * dt);
    if (std::fabs(visualStepOffsetY) < VisualStepOffsetEpsilon) {
        visualStepOffsetY = 0.0f;
    }
}

void ApplySectorFpsVisualStepSmoothing(
        float& visualStepOffsetY,
        SectorFpsVerticalTransition transition,
        float previousVisualEyeY,
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        float smoothingRate,
        float dt)
{
    if (SectorFpsTransitionStartsVisualStepSmoothing(transition)) {
        visualStepOffsetY = CaptureSectorFpsVisualStepOffset(
                previousVisualEyeY,
                state,
                config);
        return;
    }

    if (SectorFpsTransitionClearsVisualStepSmoothing(transition)) {
        visualStepOffsetY = 0.0f;
        return;
    }

    DecaySectorFpsVisualStepOffset(visualStepOffsetY, smoothingRate, dt);
}

void ClearSectorFpsHeadBob(SectorFpsHeadBobState& headBob)
{
    headBob = SectorFpsHeadBobState{};
}

void UpdateSectorFpsHeadBob(
        SectorFpsHeadBobState& headBob,
        const SectorFpsControllerConfig& config,
        bool active,
        float horizontalSpeed,
        float yawRadians,
        float dt,
        float blendRate)
{
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    if (normalized.headBobStrength <= 0.0f || normalized.headBobFrequency <= 0.0f) {
        headBob.blend = 0.0f;
        headBob.offset = Vector3{};
        return;
    }

    if (dt <= 0.0f || blendRate <= 0.0f) {
        return;
    }

    const bool moving = horizontalSpeed > 0.001f;
    const bool bobActive = active && moving;
    const float blendTarget = bobActive ? 1.0f : 0.0f;
    const float blendStep = 1.0f - std::exp(-blendRate * dt);
    headBob.blend += (blendTarget - headBob.blend) * blendStep;
    headBob.blend = std::clamp(headBob.blend, 0.0f, 1.0f);

    if (bobActive) {
        const float runSpeed = std::max(normalized.runSpeed, 0.001f);
        const float speed01 = std::clamp(horizontalSpeed / runSpeed, 0.0f, 1.0f);
        const float cadence = 0.65f + (1.25f - 0.65f) * speed01;
        headBob.phase += dt * normalized.headBobFrequency * cadence * TwoPi;
        if (headBob.phase > HeadBobPhaseWrap || headBob.phase < -HeadBobPhaseWrap) {
            headBob.phase = std::fmod(headBob.phase, TwoPi);
        }
    }

    if (headBob.blend <= HeadBobOffsetEpsilon) {
        headBob.blend = 0.0f;
        headBob.offset = Vector3{};
        return;
    }

    const float verticalBob = std::sin(headBob.phase) * normalized.headBobStrength;
    const float lateralBob = std::cos(headBob.phase * 0.5f) * normalized.headBobStrength * 0.35f;
    const Vector3 right{-std::sin(yawRadians), 0.0f, std::cos(yawRadians)};
    headBob.offset = Vector3Add(
            Vector3Scale(right, lateralBob * headBob.blend),
            Vector3{0.0f, verticalBob * headBob.blend, 0.0f});
}

void ClearSectorFpsLandingDip(SectorFpsLandingDipState& landingDip)
{
    landingDip = SectorFpsLandingDipState{};
}

float ComputeSectorFpsLandingDipAmount(
        float landingImpactSpeed,
        float minImpactSpeed,
        float fullImpactSpeed,
        float maxDip,
        float curvePower)
{
    if (!std::isfinite(landingImpactSpeed)
            || !std::isfinite(minImpactSpeed)
            || !std::isfinite(fullImpactSpeed)
            || !std::isfinite(maxDip)
            || !std::isfinite(curvePower)
            || fullImpactSpeed <= minImpactSpeed
            || maxDip <= 0.0f
            || curvePower <= 0.0f) {
        return 0.0f;
    }

    const float impactSpeed = std::max(0.0f, landingImpactSpeed);
    const float t = std::clamp(
            (impactSpeed - minImpactSpeed) / (fullImpactSpeed - minImpactSpeed),
            0.0f,
            1.0f);
    const float curved = std::pow(t, curvePower);
    if (!std::isfinite(curved)) {
        return 0.0f;
    }

    return std::clamp(curved * maxDip, 0.0f, maxDip);
}

void UpdateSectorFpsLandingDip(
        SectorFpsLandingDipState& landingDip,
        const SectorFpsVerticalResult& verticalResult,
        float dt)
{
    if (!std::isfinite(landingDip.offsetY)) {
        landingDip.offsetY = 0.0f;
    }
    if (dt <= 0.0f) {
        return;
    }

    if (!verticalResult.hasSector
            || verticalResult.transition == SectorFpsVerticalTransition::StartedDrop
            || verticalResult.transition == SectorFpsVerticalTransition::CannotFit) {
        landingDip.offsetY = 0.0f;
        return;
    }

    if (verticalResult.transition == SectorFpsVerticalTransition::Landed) {
        const float dip = ComputeSectorFpsLandingDipAmount(
                verticalResult.landingImpactSpeed,
                MinLandingImpactSpeed,
                FullLandingDipImpactSpeed,
                MaxLandingDip,
                LandingDipCurvePower);
        if (dip <= 0.0f) {
            landingDip.offsetY = 0.0f;
            return;
        }

        landingDip.offsetY = -dip;
        return;
    }

    if (landingDip.offsetY == 0.0f) {
        return;
    }

    landingDip.offsetY *= std::exp(-LandingDipRecoveryRate * dt);
    if (std::fabs(landingDip.offsetY) < LandingDipOffsetEpsilon) {
        landingDip.offsetY = 0.0f;
    }
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
    if (input.jumpPressed) {
        TryStartSectorFpsJump(state, config);
    }
    const Vector2 movement = ComputeSectorFpsHorizontalMovementDelta(state, config, input, dt);
    state.feetPosition.x += movement.x;
    state.feetPosition.z += movement.y;
}

bool TryStartSectorFpsJump(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config)
{
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    if (!state.grounded || normalized.gravity <= 0.0f || normalized.jumpHeight <= 0.0f) {
        return false;
    }

    state.verticalVelocity = std::sqrt(2.0f * normalized.gravity * normalized.jumpHeight);
    state.grounded = false;
    return true;
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
        const float verticalVelocityBeforeLanding = state.verticalVelocity;
        state.feetPosition.y = context.floorZ;
        state.grounded = true;
        result.landingImpactSpeed = std::max(0.0f, -verticalVelocityBeforeLanding);
        state.verticalVelocity = 0.0f;
        result.transition = SectorFpsVerticalTransition::Landed;
    }

    if (state.feetPosition.y > maxFeetY) {
        state.feetPosition.y = maxFeetY;
        if (state.verticalVelocity > 0.0f) {
            state.verticalVelocity = 0.0f;
            result.transition = SectorFpsVerticalTransition::CeilingBonk;
        }
    }

    return result;
}

} // namespace game
