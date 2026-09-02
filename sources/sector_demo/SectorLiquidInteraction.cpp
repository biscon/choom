#include "sector_demo/SectorLiquidInteraction.h"

#include "sector_demo/SectorUnits.h"
#include "sector_demo/SectorViewPose.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

SectorLiquidContact SampleSectorLiquidContact(
        const SectorTopologyMap& map,
        int sectorId,
        Vector3 feetPosition,
        const SectorFpsControllerConfig& config)
{
    SectorLiquidContact contact;
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr || !sector->liquid.enabled) {
        return contact;
    }

    const SectorLiquidSettings liquid = NormalizeSectorLiquidSettingsForSpan(
            sector->liquid, sector->floorZ, sector->ceilingZ);
    contact.hasLiquid = true;
    contact.sectorId = sector->id;
    contact.bottomY = SectorAuthoringToWorldDistance(sector->floorZ);
    contact.surfaceY = SectorAuthoringToWorldDistance(
            ResolveSectorLiquidSurfaceHeight(liquid, sector->floorZ, sector->ceilingZ));
    contact.settings = liquid;

    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    const float bodyHeight = std::max(normalized.playerHeight, 0.001f);
    const float immersedHeight = std::clamp(contact.surfaceY - feetPosition.y, 0.0f, bodyHeight);
    contact.immersionFraction = immersedHeight / bodyHeight;
    contact.eyeSubmerged = feetPosition.y + normalized.eyeHeight < contact.surfaceY;
    return contact;
}

void UpdateSectorLiquidMovementState(
        SectorLiquidMovementState& state,
        const SectorLiquidContact& contact,
        bool diveHeld)
{
    const bool wasSwimming = state.swimming;
    state.contact = contact;
    if (!contact.hasLiquid) {
        state.swimming = false;
        state.surfaceLatched = false;
        state.impactEntryActive = false;
        state.exitingWater = false;
        return;
    }

    state.swimming = wasSwimming
            ? contact.immersionFraction >= SectorLiquidSwimExitImmersion
            : contact.immersionFraction >= SectorLiquidSwimEnterImmersion;
    if (!state.swimming) {
        state.surfaceLatched = false;
        state.impactEntryActive = false;
        return;
    }

    if (!wasSwimming) {
        state.surfaceLatched = !contact.eyeSubmerged;
    }
    if (diveHeld) {
        state.surfaceLatched = false;
        state.impactEntryActive = false;
    }
}

Vector3 ComputeSectorLiquidSwimMovementDelta(
        const SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        bool surfaceLatched,
        float dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return Vector3{};
    }
    const SectorFpsControllerConfig normalized = NormalizeSectorFpsControllerConfig(config);
    const Vector3 yawForward{std::cos(state.yawRadians), 0.0f, std::sin(state.yawRadians)};
    const Vector3 forward = surfaceLatched
            ? yawForward
            : SectorViewForward(SectorViewPose{{}, state.yawRadians, state.pitchRadians});
    const Vector3 right{-yawForward.z, 0.0f, yawForward.x};
    Vector3 movement{};
    if (input.moveForward) movement = Vector3Add(movement, forward);
    if (input.moveBackward) movement = Vector3Subtract(movement, forward);
    if (input.strafeRight) movement = Vector3Add(movement, right);
    if (input.strafeLeft) movement = Vector3Subtract(movement, right);
    if (input.swimUp) movement.y += 1.0f;
    if (input.swimDown) movement.y -= 1.0f;
    if (Vector3LengthSqr(movement) <= 0.0001f) {
        return Vector3{};
    }
    const float speedScale = std::isfinite(input.movementSpeedScale)
            ? std::max(0.0f, input.movementSpeedScale) : 1.0f;
    Vector3 result = Vector3Scale(
            Vector3Normalize(movement), normalized.swimSpeed * speedScale * dt);
    result.x += input.externalHorizontalMovementDelta.x;
    result.z += input.externalHorizontalMovementDelta.y;
    return result;
}

Vector3 EvaluateSectorLiquidExitTrajectory(
        Vector3 startFeetPosition,
        Vector3 targetFeetPosition,
        float liftY,
        float progress)
{
    const float t = std::clamp(
            std::isfinite(progress) ? progress : 0.0f,
            0.0f,
            1.0f);
    if (t >= 1.0f) return targetFeetPosition;

    const float liftProgress = t * (2.0f - t);
    return Vector3{
            startFeetPosition.x
                    + (targetFeetPosition.x - startFeetPosition.x) * t,
            startFeetPosition.y
                    + (liftY - startFeetPosition.y) * liftProgress,
            startFeetPosition.z
                    + (targetFeetPosition.z - startFeetPosition.z) * t};
}

bool UpdateSectorLiquidCameraSubmersion(
        bool wasSubmerged,
        const SectorLiquidContact& contact,
        float eyeY)
{
    if (!contact.hasLiquid || !std::isfinite(eyeY)) {
        return false;
    }
    const float threshold = contact.surfaceY
            + (wasSubmerged ? SectorLiquidSubmersionHysteresisWorld
                            : -SectorLiquidSubmersionHysteresisWorld);
    return eyeY < threshold;
}

bool BeginSectorLiquidImpactEntry(
        SectorLiquidMovementState& liquid,
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorLiquidPhysicsConfig& physics,
        float minimumFeetY)
{
    liquid.impactEntryActive = false;
    if (!liquid.swimming || !liquid.surfaceLatched
            || !liquid.contact.hasLiquid
            || !std::isfinite(state.verticalVelocity)
            || state.verticalVelocity >= -0.001f) {
        return false;
    }
    const SectorFpsControllerConfig normalized =
            NormalizeSectorFpsControllerConfig(config);
    const float slowdown = std::clamp(
            std::isfinite(physics.entrySlowdownSeconds)
                    ? physics.entrySlowdownSeconds : 0.20f,
            0.0f, 2.0f);
    if (slowdown <= 0.0f) {
        state.verticalVelocity = 0.0f;
        return false;
    }

    const float impactSpeed = -state.verticalVelocity;
    const float stableSurfaceFeetY = liquid.contact.surfaceY
            + SectorLiquidSurfaceEyeOffsetWorld - normalized.eyeHeight;
    const float unclampedTarget = state.feetPosition.y
            - 0.5f * impactSpeed * slowdown;
    const float targetFeetY = std::max(unclampedTarget, minimumFeetY);
    // Gentle entries use the regular surface spring. The impact phase is only
    // needed when momentum would carry the body below its stable swim height.
    if (targetFeetY >= stableSurfaceFeetY - 0.001f
            || targetFeetY >= state.feetPosition.y - 0.001f) {
        state.verticalVelocity = 0.0f;
        return false;
    }
    const float distance = state.feetPosition.y - targetFeetY;
    const float duration = std::min(
            slowdown, 2.0f * distance / impactSpeed);
    if (!(duration > 0.0f) || !std::isfinite(duration)) return false;

    liquid.impactEntryActive = true;
    liquid.impactEntryStartFeetY = state.feetPosition.y;
    liquid.impactEntryTargetFeetY = targetFeetY;
    liquid.impactEntrySpeed = impactSpeed;
    liquid.impactEntryElapsedSeconds = 0.0f;
    liquid.impactEntryDurationSeconds = duration;
    return true;
}

void UpdateSectorLiquidSwimmingVerticalMotion(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        SectorLiquidMovementState& liquid,
        const SectorLiquidPhysicsConfig& physics,
        float desiredVerticalVelocity,
        float minimumFeetY,
        float maximumFeetY,
        float dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    const SectorFpsControllerConfig normalized =
            NormalizeSectorFpsControllerConfig(config);
    const float drag = std::clamp(
            std::isfinite(physics.waterDragPerSecond)
                    ? physics.waterDragPerSecond : 5.0f,
            0.0f, 40.0f);
    if (liquid.impactEntryActive) {
        liquid.impactEntryElapsedSeconds += dt;
        const float duration = std::max(
                liquid.impactEntryDurationSeconds, 0.001f);
        const float progress = std::clamp(
                liquid.impactEntryElapsedSeconds / duration,
                0.0f, 1.0f);
        const float easeOut = progress * (2.0f - progress);
        const float distance = liquid.impactEntryStartFeetY
                - liquid.impactEntryTargetFeetY;
        state.feetPosition.y = liquid.impactEntryStartFeetY
                - distance * easeOut;
        state.verticalVelocity = -(2.0f * distance / duration)
                * (1.0f - progress);
        if (state.feetPosition.y <= minimumFeetY
                || progress >= 1.0f) {
            state.feetPosition.y = std::max(
                    state.feetPosition.y, minimumFeetY);
            state.verticalVelocity = 0.0f;
            liquid.impactEntryActive = false;
        }
        return;
    }
    float effectiveMaximumFeetY = maximumFeetY;
    if (liquid.surfaceLatched
            && liquid.contact.hasLiquid
            && std::isfinite(desiredVerticalVelocity)
            && desiredVerticalVelocity > 0.001f) {
        const float stableSurfaceFeetY = liquid.contact.surfaceY
                + SectorLiquidSurfaceEyeOffsetWorld - normalized.eyeHeight;
        const float surfaceCap = std::max(
                stableSurfaceFeetY, state.feetPosition.y);
        effectiveMaximumFeetY = std::isfinite(effectiveMaximumFeetY)
                ? std::min(effectiveMaximumFeetY, surfaceCap)
                : surfaceCap;
    }
    const int stepCount = std::clamp(
            static_cast<int>(std::ceil(dt * 120.0f)), 1, 64);
    const float stepDt = dt / static_cast<float>(stepCount);
    for (int step = 0; step < stepCount; ++step) {
        if (std::isfinite(desiredVerticalVelocity)
                && std::fabs(desiredVerticalVelocity) > 0.001f) {
            const float acceleration = std::max(
                    8.0f * normalized.swimSpeed, 1.0f);
            const float difference = desiredVerticalVelocity
                    - state.verticalVelocity;
            state.verticalVelocity += std::clamp(
                    difference,
                    -acceleration * stepDt,
                    acceleration * stepDt);
        } else if (liquid.surfaceLatched && liquid.contact.hasLiquid) {
            const float frequency = std::clamp(
                    std::isfinite(physics.surfaceRecoveryFrequencyHz)
                            ? physics.surfaceRecoveryFrequencyHz : 0.35f,
                    0.1f, 10.0f);
            const float omega = 2.0f * PI * frequency;
            const float targetFeetY = liquid.contact.surfaceY
                    + SectorLiquidSurfaceEyeOffsetWorld - normalized.eyeHeight;
            const float error = targetFeetY - state.feetPosition.y;
            state.verticalVelocity += (omega * omega * error
                    - 2.0f * omega * state.verticalVelocity) * stepDt;
            state.verticalVelocity = std::clamp(
                    state.verticalVelocity,
                    -std::max(normalized.swimSpeed * 2.0f, 1.0f),
                    std::max(normalized.swimSpeed, 1.0f));
            if (std::fabs(error) < 0.002f
                    && std::fabs(state.verticalVelocity) < 0.02f) {
                state.feetPosition.y = targetFeetY;
                state.verticalVelocity = 0.0f;
            }
        } else {
            state.verticalVelocity *= std::exp(-drag * stepDt);
        }

        state.feetPosition.y += state.verticalVelocity * stepDt;
        if (std::isfinite(minimumFeetY)
                && state.feetPosition.y < minimumFeetY) {
            state.feetPosition.y = minimumFeetY;
            if (state.verticalVelocity < 0.0f) state.verticalVelocity = 0.0f;
        }
        if (std::isfinite(effectiveMaximumFeetY)
                && state.feetPosition.y > effectiveMaximumFeetY) {
            state.feetPosition.y = effectiveMaximumFeetY;
            if (state.verticalVelocity > 0.0f) state.verticalVelocity = 0.0f;
        }
    }
}

} // namespace game
