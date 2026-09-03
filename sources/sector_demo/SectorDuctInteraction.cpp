#include "sector_demo/SectorDuctInteraction.h"

#include "engine/ecs/World.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

float SmoothStep(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

Vector3 Lerp(Vector3 a, Vector3 b, float t)
{
    return Vector3{a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t,
            a.z + (b.z - a.z) * t};
}

float LerpAngle(float a, float b, float t)
{
    return a + std::remainder(b - a, 2.0f * PI) * t;
}

float FacingYaw(Vector2 direction)
{
    return std::atan2(direction.y, direction.x);
}

bool HasOutwardMovementIntent(
        const SectorFpsControllerState& controller,
        const SectorFpsControllerInput& input,
        const SectorDuctAccess& access)
{
    const Vector2 forward{
            std::cos(controller.yawRadians),
            std::sin(controller.yawRadians)};
    const Vector2 right{-forward.y, forward.x};
    Vector2 intended{};
    if (input.moveForward) intended = Vector2Add(intended, forward);
    if (input.moveBackward) intended = Vector2Subtract(intended, forward);
    if (input.strafeRight) intended = Vector2Add(intended, right);
    if (input.strafeLeft) intended = Vector2Subtract(intended, right);
    if (Vector2LengthSqr(intended) <= 0.0001f) return false;
    return Vector2DotProduct(
            intended, access.outsideToCrawlspaceNormal) < -0.1f;
}

float DistanceToAccessXZ(Vector3 feet, const SectorDuctAccess& access)
{
    const Vector2 delta{feet.x - access.centerXZ.x,
            feet.z - access.centerXZ.y};
    const float tangentDistance = std::clamp(
            delta.x * access.tangent.x + delta.y * access.tangent.y,
            -access.width * 0.5f, access.width * 0.5f);
    const Vector2 closest{
            access.centerXZ.x + access.tangent.x * tangentDistance,
            access.centerXZ.y + access.tangent.y * tangentDistance};
    return std::hypot(feet.x - closest.x, feet.z - closest.y);
}

void BeginExit(
        SectorDuctTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorDuctAccess& access,
        const SectorFpsControllerConfig& normalConfig,
        const PlayerDuctTraversalApplicationSettings& settings)
{
    const SectorFpsControllerConfig crawl = SectorDuctCrawlControllerConfig(
            SectorFpsControllerConfig{}, settings);
    const float offset = access.thickness * 0.5f
            + crawl.playerRadius + 0.05f;
    traversal.phase = SectorDuctTraversalPhase::Exiting;
    traversal.accessEntity = engine::NullEntity();
    traversal.transitionStartFeet = controller.feetPosition;
    traversal.transitionTargetFeet = Vector3{
            access.centerXZ.x
                    - access.outsideToCrawlspaceNormal.x * offset,
            controller.feetPosition.y,
            access.centerXZ.y
                    - access.outsideToCrawlspaceNormal.y * offset};
    traversal.transitionStartYawRadians = controller.yawRadians;
    traversal.transitionStartPitchRadians = controller.pitchRadians;
    traversal.transitionTargetYawRadians = FacingYaw(Vector2{
            -access.outsideToCrawlspaceNormal.x,
            -access.outsideToCrawlspaceNormal.y});
    traversal.transitionStartEyeHeightWorld = traversal.viewEyeHeightWorld;
    traversal.transitionTargetEyeHeightWorld =
            EffectiveSectorFpsControllerConfig(
                    controller, normalConfig).eyeHeight;
    traversal.transitionElapsedSeconds = 0.0f;
    controller.verticalVelocity = 0.0f;
    controller.grounded = false;
}

} // namespace

bool IsSectorDuctTraversalActive(const SectorDuctTraversalState& state)
{
    return state.phase != SectorDuctTraversalPhase::Inactive;
}

bool IsSectorDuctCrawling(const SectorDuctTraversalState& state)
{
    return state.phase == SectorDuctTraversalPhase::Crawling;
}

void ResetSectorDuctTraversal(SectorDuctTraversalState& state)
{
    state = SectorDuctTraversalState{};
}

SectorFpsControllerConfig SectorDuctCrawlControllerConfig(
        SectorFpsControllerConfig base,
        const PlayerDuctTraversalApplicationSettings& rawSettings)
{
    const PlayerDuctTraversalApplicationSettings settings =
            NormalizePlayerDuctTraversalSettings(rawSettings);
    base.walkSpeed = settings.crawlSpeedWorld;
    base.runSpeed = settings.crawlSpeedWorld;
    base.playerRadius = settings.crawlRadiusWorld;
    base.playerHeight = settings.crawlHeightWorld;
    base.eyeHeight = settings.crawlEyeHeightWorld;
    base.stepHeight = std::min(base.stepHeight,
            settings.crawlHeightWorld * 0.25f);
    base.jumpHeight = 0.0f;
    base.headBobStrength = 0.0f;
    return base;
}

SectorFpsControllerConfig SectorDuctViewControllerConfig(
        SectorFpsControllerConfig base,
        const SectorDuctTraversalState& traversal)
{
    if (IsSectorDuctTraversalActive(traversal)) {
        base.playerRadius = traversal.crawlRadiusWorld;
        base.playerHeight = traversal.crawlHeightWorld;
        base.eyeHeight = traversal.viewEyeHeightWorld;
        base.stepHeight = std::min(
                base.stepHeight, traversal.crawlHeightWorld * 0.25f);
        base.jumpHeight = 0.0f;
        base.headBobStrength = 0.0f;
    }
    return base;
}

bool BeginSectorDuctTraversal(
        SectorDuctTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& normalConfig,
        const SectorDuctAccess& access,
        engine::Entity accessEntity,
        const PlayerDuctTraversalApplicationSettings& settings)
{
    if (traversal.phase != SectorDuctTraversalPhase::Inactive
            || !IsSectorDuctCoverClear(access)) {
        return false;
    }
    const float offset = access.thickness * 0.5f
            + settings.crawlRadiusWorld + 0.05f;
    traversal.phase = SectorDuctTraversalPhase::Entering;
    traversal.accessEntity = accessEntity;
    traversal.crawlspaceSectorId = access.crawlspaceSectorId;
    traversal.transitionStartFeet = controller.feetPosition;
    traversal.transitionTargetFeet = Vector3{
            access.centerXZ.x
                    + access.outsideToCrawlspaceNormal.x * offset,
            access.openingBottom,
            access.centerXZ.y
                    + access.outsideToCrawlspaceNormal.y * offset};
    traversal.transitionStartYawRadians = controller.yawRadians;
    traversal.transitionStartPitchRadians = controller.pitchRadians;
    traversal.transitionTargetYawRadians = FacingYaw(
            access.outsideToCrawlspaceNormal);
    traversal.transitionStartEyeHeightWorld =
            EffectiveSectorFpsControllerConfig(
                    controller, normalConfig).eyeHeight;
    traversal.transitionTargetEyeHeightWorld = settings.crawlEyeHeightWorld;
    traversal.transitionElapsedSeconds = 0.0f;
    traversal.crawlRadiusWorld = settings.crawlRadiusWorld;
    traversal.crawlHeightWorld = settings.crawlHeightWorld;
    traversal.viewEyeHeightWorld = traversal.transitionStartEyeHeightWorld;
    traversal.exitArmed = false;
    traversal.weaponHolsterInitialized = false;
    controller.verticalVelocity = 0.0f;
    controller.grounded = false;
    ResetSectorFpsCrouch(controller);
    return true;
}

bool UpdateSectorDuctTraversal(
        engine::World& world,
        SectorDuctTraversalState& traversal,
        SectorFpsControllerState& controller,
        const SectorFpsControllerConfig& normalConfig,
        const SectorFpsControllerInput& input,
        const PlayerDuctTraversalApplicationSettings& rawSettings,
        const SectorTopologyMap& map,
        const SectorCollisionWorld* collisionWorld,
        float dt)
{
    const PlayerDuctTraversalApplicationSettings settings =
            NormalizePlayerDuctTraversalSettings(rawSettings);
    if (IsSectorDuctTraversalActive(traversal)) {
        traversal.crawlRadiusWorld = settings.crawlRadiusWorld;
        traversal.crawlHeightWorld = settings.crawlHeightWorld;
        if (traversal.phase == SectorDuctTraversalPhase::Crawling) {
            traversal.viewEyeHeightWorld = settings.crawlEyeHeightWorld;
        }
    }
    if (traversal.phase == SectorDuctTraversalPhase::Inactive) {
        const SectorTopologySector* sector =
                FindSectorTopologySector(map, controller.currentSectorId);
        if (sector != nullptr && sector->crawlspace) {
            traversal.phase = SectorDuctTraversalPhase::Crawling;
            traversal.crawlspaceSectorId = sector->id;
            traversal.crawlRadiusWorld = settings.crawlRadiusWorld;
            traversal.crawlHeightWorld = settings.crawlHeightWorld;
            traversal.viewEyeHeightWorld = settings.crawlEyeHeightWorld;
            traversal.exitArmed = true;
            ResetSectorFpsCrouch(controller);
        } else {
            return false;
        }
    }

    if (traversal.phase == SectorDuctTraversalPhase::Entering
            || traversal.phase == SectorDuctTraversalPhase::Exiting) {
        const bool entering = traversal.phase
                == SectorDuctTraversalPhase::Entering;
        const float duration = entering ? settings.enterTransitionSeconds
                                        : settings.exitTransitionSeconds;
        traversal.transitionElapsedSeconds += std::max(0.0f, dt);
        const float progress = SmoothStep(
                traversal.transitionElapsedSeconds / duration);
        controller.feetPosition = Lerp(traversal.transitionStartFeet,
                traversal.transitionTargetFeet, progress);
        controller.yawRadians = LerpAngle(
                traversal.transitionStartYawRadians,
                traversal.transitionTargetYawRadians, progress);
        controller.pitchRadians = traversal.transitionStartPitchRadians
                + (0.0f - traversal.transitionStartPitchRadians) * progress;
        traversal.viewEyeHeightWorld =
                traversal.transitionStartEyeHeightWorld
                + (traversal.transitionTargetEyeHeightWorld
                        - traversal.transitionStartEyeHeightWorld) * progress;
        controller.verticalVelocity = 0.0f;
        controller.grounded = false;
        if (progress >= 1.0f) {
            if (entering) {
                traversal.phase = SectorDuctTraversalPhase::Crawling;
                controller.currentSectorId = traversal.crawlspaceSectorId;
                controller.grounded = true;
            } else {
                traversal.phase = SectorDuctTraversalPhase::Inactive;
                traversal.crawlspaceSectorId = 0;
                controller.currentSectorId = collisionWorld != nullptr
                        ? collisionWorld->FindSectorContainingPointPreferCurrent(
                                Vector2{controller.feetPosition.x,
                                        controller.feetPosition.z},
                                controller.currentSectorId)
                        : controller.currentSectorId;
            }
        }
        return true;
    }

    if (traversal.phase != SectorDuctTraversalPhase::Crawling) return false;
    const SectorTopologySector* sector =
            FindSectorTopologySector(map, controller.currentSectorId);
    if (sector == nullptr || !sector->crawlspace) {
        const bool weaponHolsterInitialized =
                traversal.weaponHolsterInitialized;
        ResetSectorDuctTraversal(traversal);
        traversal.weaponHolsterInitialized = weaponHolsterInitialized;
        return false;
    }

    SectorDuctAccess* exitAccess = nullptr;
    engine::Entity exitAccessEntity = engine::NullEntity();
    float entryAccessDistance = INFINITY;
    const float exitDistance = settings.crawlRadiusWorld + 0.12f;
    world.ForEach<SectorDuctAccess>(
            [&](engine::Entity entity, SectorDuctAccess& access) {
                if (access.crawlspaceSectorId != controller.currentSectorId
                        || !IsSectorDuctCoverClear(access)) return;
                const float distance = DistanceToAccessXZ(
                        controller.feetPosition, access);
                const bool isEntryAccess = entity == traversal.accessEntity;
                if (isEntryAccess) entryAccessDistance = distance;
                const bool entrySuppressed = isEntryAccess
                        && !traversal.exitArmed
                        && !HasOutwardMovementIntent(
                                controller, input, access);
                if (!entrySuppressed && exitAccess == nullptr
                        && distance <= exitDistance) {
                    exitAccess = &access;
                    exitAccessEntity = entity;
                }
            });
    if (!traversal.exitArmed) {
        if (entryAccessDistance > exitDistance + 0.15f) {
            traversal.exitArmed = true;
        }
    }
    if (exitAccess != nullptr) {
        const Vector3 target{
                exitAccess->centerXZ.x
                        - exitAccess->outsideToCrawlspaceNormal.x
                                * (normalConfig.playerRadius + 0.05f),
                controller.feetPosition.y,
                exitAccess->centerXZ.y
                        - exitAccess->outsideToCrawlspaceNormal.y
                                * (normalConfig.playerRadius + 0.05f)};
        const bool clear = collisionWorld == nullptr
                || collisionWorld->AllowsPrismPlacement(
                        Vector2{target.x, target.z}, normalConfig.playerRadius,
                        target.y, target.y + normalConfig.playerHeight,
                        exitAccess->outsideSectorId);
        if (clear) {
            traversal.accessEntity = exitAccessEntity;
            BeginExit(traversal, controller, *exitAccess, normalConfig, settings);
        }
        return traversal.phase == SectorDuctTraversalPhase::Exiting;
    }
    return false;
}

} // namespace game
