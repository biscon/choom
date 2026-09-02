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
        state.exitingWater = false;
        return;
    }

    state.swimming = wasSwimming
            ? contact.immersionFraction >= SectorLiquidSwimExitImmersion
            : contact.immersionFraction >= SectorLiquidSwimEnterImmersion;
    if (!state.swimming) {
        state.surfaceLatched = false;
        return;
    }

    if (!wasSwimming) {
        state.surfaceLatched = !contact.eyeSubmerged;
    }
    if (diveHeld) {
        state.surfaceLatched = false;
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
    if (input.swimUp && !surfaceLatched) movement.y += 1.0f;
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

void ApplySectorLiquidEntryResistance(
        SectorFpsControllerState& state,
        const SectorFpsControllerConfig& config,
        const SectorLiquidContact& contact,
        const SectorLiquidPhysicsConfig& physics,
        float dt)
{
    if (!contact.hasLiquid || contact.immersionFraction <= 0.0f
            || state.verticalVelocity >= 0.0f
            || !std::isfinite(dt) || dt <= 0.0f) {
        return;
    }
    const SectorFpsControllerConfig normalized =
            NormalizeSectorFpsControllerConfig(config);
    const float influence = std::clamp(
            contact.immersionFraction / SectorLiquidSwimEnterImmersion,
            0.0f, 1.0f);
    const float drag = std::clamp(
            std::isfinite(physics.waterDragPerSecond)
                    ? physics.waterDragPerSecond : 5.0f,
            0.0f, 40.0f);
    // The ordinary airborne solver has already applied full gravity. As the
    // body enters liquid, progressively cancel it and damp the retained impact
    // velocity instead of treating the surface as a floor.
    state.verticalVelocity += normalized.gravity * influence * dt;
    state.verticalVelocity *= std::exp(-drag * influence * dt);
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
    const int stepCount = std::clamp(
            static_cast<int>(std::ceil(dt * 120.0f)), 1, 64);
    const float stepDt = dt / static_cast<float>(stepCount);
    for (int step = 0; step < stepCount; ++step) {
        state.verticalVelocity *= std::exp(-drag * stepDt);
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
                            ? physics.surfaceRecoveryFrequencyHz : 1.5f,
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
        }

        state.feetPosition.y += state.verticalVelocity * stepDt;
        if (std::isfinite(minimumFeetY)
                && state.feetPosition.y < minimumFeetY) {
            state.feetPosition.y = minimumFeetY;
            if (state.verticalVelocity < 0.0f) state.verticalVelocity = 0.0f;
        }
        if (std::isfinite(maximumFeetY)
                && state.feetPosition.y > maximumFeetY) {
            state.feetPosition.y = maximumFeetY;
            if (state.verticalVelocity > 0.0f) state.verticalVelocity = 0.0f;
        }
    }
}

} // namespace game
