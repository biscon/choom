#include "sector_demo/SectorLadderInteraction.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLiquidInteraction.h"
#include "sector_demo/SectorStructuralPrimitives.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>

namespace game {
namespace {

const SectorAuthoringStructuralPrimitive* FindLadder(
        const SectorTopologyMap& map,
        int primitiveId)
{
    for (const SectorCompiledStructuralPrimitive& primitive
            : map.compiledStructuralPrimitives) {
        if (primitive.sourceAuthoringPrimitiveId == primitiveId
                && primitive.authored.enabled
                && primitive.authored.kind == SectorStructuralPrimitiveKind::Ladder) {
            return &primitive.authored;
        }
    }
    return nullptr;
}

float SmoothStep(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float LerpAngle(float from, float to, float t)
{
    return from + std::remainder(to - from, 2.0f * PI) * t;
}

Vector3 Lerp(Vector3 from, Vector3 to, float t)
{
    return Vector3{
            from.x + (to.x - from.x) * t,
            from.y + (to.y - from.y) * t,
            from.z + (to.z - from.z) * t};
}

bool ExitClear(
        const SectorCollisionWorld* world,
        Vector3 feet,
        const SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& config,
        int ignoredStructuralPrimitiveId,
        int ignoredSupportingStructuralPrimitiveId = -1)
{
    if (world == nullptr) return true;
    const SectorFpsControllerConfig effective =
            EffectiveSectorFpsControllerConfig(controller, config);
    return world->AllowsPrismPlacement(
            Vector2{feet.x, feet.z},
            effective.playerRadius,
            feet.y,
            feet.y + effective.playerHeight,
            controller.currentSectorId,
            nullptr,
            ignoredStructuralPrimitiveId,
            ignoredSupportingStructuralPrimitiveId);
}

enum class TopExitResolution {
    Supported,
    Unsupported,
    Blocked
};

TopExitResolution ResolveTopExit(
        const SectorCollisionWorld* world,
        Vector3 target,
        const SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& config,
        float ladderTopY,
        float* targetFeetY,
        int* supportingStructuralPrimitiveId)
{
    if (targetFeetY == nullptr || supportingStructuralPrimitiveId == nullptr) {
        return TopExitResolution::Blocked;
    }
    *targetFeetY = ladderTopY;
    *supportingStructuralPrimitiveId = -1;
    if (world == nullptr) return TopExitResolution::Supported;

    const SectorFpsControllerConfig effective =
            EffectiveSectorFpsControllerConfig(controller, config);
    const Vector2 targetXZ{target.x, target.z};
    const int sectorId = world->FindSectorContainingPointPreferCurrent(
            targetXZ, controller.currentSectorId);
    if (sectorId == 0) return TopExitResolution::Unsupported;

    SectorCollisionHeights heights;
    if (!world->ResolveActorVerticalContext(
                sectorId,
                SectorCollisionVerticalQuery{
                        targetXZ,
                        ladderTopY,
                        effective.playerRadius,
                        effective.playerHeight,
                        effective.eyeHeight,
                        true},
                &heights)) {
        return TopExitResolution::Blocked;
    }
    const float rise = heights.floorZ - ladderTopY;
    if (rise > effective.eyeHeight + 0.0001f) {
        return TopExitResolution::Blocked;
    }
    if (rise < -effective.stepHeight - 0.0001f) {
        return TopExitResolution::Unsupported;
    }
    *targetFeetY = heights.floorZ;
    *supportingStructuralPrimitiveId =
            heights.supportingStructuralPrimitiveId;
    return TopExitResolution::Supported;
}

void DetachForFall(
        SectorLadderTraversalState& traversal,
        SectorFpsControllerState& controller)
{
    ResetSectorLadderTraversal(traversal);
    controller.grounded = false;
    controller.verticalVelocity = 0.0f;
}

void BeginDismount(
        SectorLadderTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& config,
        const SectorCollisionWorld* collisionWorld,
        SectorLadderEndpoint endpoint)
{
    const SectorFpsControllerConfig effective =
            EffectiveSectorFpsControllerConfig(controller, config);
    const float direction = endpoint == SectorLadderEndpoint::Top ? -1.0f : 1.0f;
    const float exitOffset = traversal.ladderHalfDepth
            + effective.playerRadius + 0.20f;
    Vector3 target{
            traversal.ladderCenterXZ.x
                    + traversal.front.x * direction * exitOffset,
            endpoint == SectorLadderEndpoint::Top
                    ? traversal.topY : traversal.bottomY,
            traversal.ladderCenterXZ.y
                    + traversal.front.y * direction * exitOffset};
    int supportingStructuralPrimitiveId = -1;
    if (endpoint == SectorLadderEndpoint::Top) {
        const TopExitResolution resolution = ResolveTopExit(
                collisionWorld,
                target,
                controller,
                config,
                traversal.topY,
                &target.y,
                &supportingStructuralPrimitiveId);
        if (resolution == TopExitResolution::Unsupported) {
            DetachForFall(traversal, controller);
            return;
        }
        if (resolution == TopExitResolution::Blocked) return;
    }
    if (!ExitClear(
                collisionWorld,
                target,
                controller,
                config,
                traversal.ladderPrimitiveId,
                supportingStructuralPrimitiveId)) {
        return;
    }
    traversal.phase = SectorLadderTraversalPhase::Dismounting;
    traversal.mountEndpoint = endpoint;
    traversal.transitionStartFeet = controller.feetPosition;
    traversal.transitionTargetFeet = target;
    traversal.transitionStartYawRadians = controller.yawRadians;
    traversal.transitionStartPitchRadians = controller.pitchRadians;
    traversal.transitionElapsedSeconds = 0.0f;
}

} // namespace

bool IsSectorLadderTraversalActive(const SectorLadderTraversalState& state)
{
    return state.phase != SectorLadderTraversalPhase::Inactive;
}

void ResetSectorLadderTraversal(SectorLadderTraversalState& state)
{
    state = SectorLadderTraversalState{};
}

bool TryDetachSectorLadderTraversal(
        SectorLadderTraversalState& traversal,
        SectorFpsControllerState& controller,
        bool cameraSubmerged)
{
    if (!cameraSubmerged
            || traversal.phase == SectorLadderTraversalPhase::Inactive
            || traversal.phase == SectorLadderTraversalPhase::Dismounting) {
        return false;
    }
    DetachForFall(traversal, controller);
    return true;
}

void UpdateSectorLadderLiquidState(
        SectorLiquidMovementState& liquid,
        const SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& config,
        const SectorTopologyMap& map,
        bool diveHeld)
{
    liquid.exitingWater = false;
    liquid.impactEntryActive = false;
    const SectorLiquidContact contact = SampleSectorLiquidContact(
            map,
            controller.currentSectorId,
            controller.feetPosition,
            config);
    UpdateSectorLiquidMovementState(liquid, contact, diveHeld);
    const float eyeY = SectorFpsControllerEyePosition(controller, config).y;
    liquid.cameraSubmerged = UpdateSectorLiquidCameraSubmersion(
            liquid.cameraSubmerged, contact, eyeY);
}

bool BeginSectorLadderTraversal(
        SectorLadderTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& config,
        const SectorTopologyMap& map,
        const SectorCollisionWorld* collisionWorld,
        int ladderPrimitiveId,
        SectorLadderEndpoint endpoint)
{
    if (IsSectorLadderTraversalActive(traversal)) return false;
    const SectorAuthoringStructuralPrimitive* ladder =
            FindLadder(map, ladderPrimitiveId);
    if (ladder == nullptr) return false;
    const Vector2 center = SectorCoordToWorldPosition2(ladder->x, ladder->z);
    const Vector3 front3 = RotateSectorStructuralPrimitiveVector(
            *ladder, Vector3{0.0f, 0.0f, 1.0f});
    const float frontLength = std::hypot(front3.x, front3.z);
    if (!(frontLength > 0.0001f)) return false;
    const Vector2 front{front3.x / frontLength, front3.z / frontLength};
    const SectorFpsControllerConfig effective =
            EffectiveSectorFpsControllerConfig(controller, config);
    const float depth = SectorStructuralLadderFrameThicknessWorld
            * ladder->ladder.thicknessScale;
    const float railOffset = depth * 0.5f + effective.playerRadius + 0.03f;
    const Vector2 rail{
            center.x + front.x * railOffset,
            center.y + front.y * railOffset};
    const float bottomY = SectorAuthoringToWorldDistance(ladder->ladder.bottom);
    const float topY = SectorAuthoringToWorldDistance(
            ladder->ladder.bottom + ladder->ladder.height);
    Vector3 target{rail.x,
            endpoint == SectorLadderEndpoint::Top ? topY : bottomY,
            rail.y};
    if (!ExitClear(
                collisionWorld,
                target,
                controller,
                config,
                ladderPrimitiveId)) {
        return false;
    }

    traversal = SectorLadderTraversalState{};
    traversal.phase = SectorLadderTraversalPhase::Mounting;
    traversal.ladderPrimitiveId = ladderPrimitiveId;
    traversal.mountEndpoint = endpoint;
    traversal.transitionStartFeet = controller.feetPosition;
    traversal.transitionTargetFeet = target;
    traversal.railXZ = rail;
    traversal.ladderCenterXZ = center;
    traversal.front = front;
    traversal.ladderHalfDepth = depth * 0.5f;
    traversal.bottomY = bottomY;
    traversal.topY = topY;
    traversal.facingYawRadians = std::atan2(-front.y, -front.x);
    traversal.transitionStartYawRadians = controller.yawRadians;
    traversal.transitionStartPitchRadians = controller.pitchRadians;
    controller.grounded = false;
    controller.verticalVelocity = 0.0f;
    ResetSectorFpsCrouch(controller);
    return true;
}

bool UpdateSectorLadderTraversal(
        SectorLadderTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& config,
        const SectorFpsControllerInput& input,
        const SectorTopologyMap& map,
        const SectorCollisionWorld* collisionWorld,
        float dt)
{
    if (!IsSectorLadderTraversalActive(traversal)) return false;
    if (FindLadder(map, traversal.ladderPrimitiveId) == nullptr) {
        ResetSectorLadderTraversal(traversal);
        return false;
    }
    dt = std::isfinite(dt) ? std::max(0.0f, dt) : 0.0f;
    controller.grounded = false;
    controller.verticalVelocity = 0.0f;

    if (traversal.phase == SectorLadderTraversalPhase::Mounting
            || traversal.phase == SectorLadderTraversalPhase::Dismounting) {
        traversal.transitionElapsedSeconds += dt;
        const float linear = std::clamp(
                traversal.transitionElapsedSeconds / SectorLadderTransitionSeconds,
                0.0f, 1.0f);
        const float eased = SmoothStep(linear);
        controller.feetPosition = Lerp(
                traversal.transitionStartFeet,
                traversal.transitionTargetFeet,
                eased);
        controller.yawRadians = LerpAngle(
                traversal.transitionStartYawRadians,
                traversal.facingYawRadians,
                eased);
        controller.pitchRadians = traversal.transitionStartPitchRadians
                * (1.0f - eased);
        if (linear >= 1.0f) {
            if (traversal.phase == SectorLadderTraversalPhase::Mounting) {
                traversal.phase = SectorLadderTraversalPhase::Climbing;
            } else {
                ResetSectorLadderTraversal(traversal);
            }
        }
        return true;
    }

    controller.feetPosition.x = traversal.railXZ.x;
    controller.feetPosition.z = traversal.railXZ.y;
    const float yawDelta = std::clamp(
            std::remainder(controller.yawRadians - traversal.facingYawRadians,
                    2.0f * PI),
            -SectorLadderLookYawArcRadians,
            SectorLadderLookYawArcRadians);
    controller.yawRadians = traversal.facingYawRadians + yawDelta;
    const int direction = (input.moveForward ? 1 : 0)
            - (input.moveBackward ? 1 : 0);
    controller.feetPosition.y = std::clamp(
            controller.feetPosition.y
                    + direction * SectorLadderClimbSpeedWorld * dt,
            traversal.bottomY,
            traversal.topY);
    if (direction > 0
            && controller.feetPosition.y >= traversal.topY - 0.0001f) {
        BeginDismount(
                traversal, controller, config, collisionWorld,
                SectorLadderEndpoint::Top);
    } else if (direction < 0
            && controller.feetPosition.y <= traversal.bottomY + 0.0001f) {
        BeginDismount(
                traversal, controller, config, collisionWorld,
                SectorLadderEndpoint::Bottom);
    }
    return true;
}

} // namespace game
