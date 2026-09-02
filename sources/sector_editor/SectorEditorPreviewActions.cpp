#include "sector_editor/SectorEditorPreviewActions.h"

#include "game/npc/NpcCollision.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorLiquidInteraction.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

namespace {

void ClearPreviewGameplayVisualState(SectorEditorPreviewControllerState& controllerState)
{
    controllerState.visualStepOffsetY = 0.0f;
    ClearSectorFpsHeadBob(controllerState.headBobState);
    ClearSectorFpsFootstepCadence(controllerState.footstepCadenceState);
    controllerState.frameEvents = SectorFpsFrameEvents{};
    ClearSectorFpsLandingDip(controllerState.landingDipState);
}

void ResetPreviewCollisionState(
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState)
{
    collisionState.previewCollisionSectorId = 0;
    controllerState.fpsControllerState.currentSectorId = 0;
    collisionState.previewVerticalResult = SectorFpsVerticalResult{};
    collisionState.previewMoveResult = SectorCollisionMoveResult{};
    collisionState.previewCollisionNoclipFallback = false;
}

void ResetPreviewCollisionAndVisualState(
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState)
{
    ResetPreviewCollisionState(collisionState, controllerState);
    ClearPreviewGameplayVisualState(controllerState);
}

SectorFpsVerticalContext BuildSectorOnlyVerticalContext(
        const SectorEditorPreviewCollisionState& collisionState,
        int sectorId,
        Vector2 positionXZ,
        float feetY,
        bool grounded,
        const SectorFpsControllerConfig& config)
{
    SectorFpsVerticalContext context;
    if (!collisionState.sectorCollisionWorldValid || sectorId == 0) {
        return context;
    }
    SectorCollisionHeights heights;
    if (!collisionState.sectorCollisionWorld.ResolveActorVerticalContext(
                sectorId,
                SectorCollisionVerticalQuery{
                        positionXZ,
                        feetY,
                        config.playerRadius,
                        config.playerHeight,
                        config.stepHeight,
                        grounded},
                &heights)) {
        return context;
    }
    context.hasSector = true;
    context.floorZ = heights.floorZ;
    context.ceilingZ = heights.ceilingZ;
    context.continuousFloor = heights.continuousFloor;
    return context;
}

bool HasSectorEditorGameplayStandingClearance(
        const SectorEditorPreviewCollisionState& collisionState,
        const SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorDynamicDoorCollider>& dynamicDoorColliders,
        const std::vector<SectorStaticModelCollider>& staticModelColliders)
{
    const SectorFpsControllerConfig standing =
            NormalizeSectorFpsControllerConfig(controllerState.fpsControllerConfig);
    const SectorFpsControllerState& state = controllerState.fpsControllerState;
    const Vector2 positionXZ{state.feetPosition.x, state.feetPosition.z};
    const SectorFpsVerticalContext sectorContext = BuildSectorOnlyVerticalContext(
            collisionState,
            state.currentSectorId,
            positionXZ,
            state.feetPosition.y,
            state.grounded,
            standing);
    if (!sectorContext.hasSector) {
        return true;
    }
    if (state.feetPosition.y + standing.playerHeight
            > sectorContext.ceilingZ + GameplayFloorSnapEpsilon) {
        return false;
    }
    if (!SectorStaticModelCollidersAllowPlayerHeight(
                positionXZ,
                state.feetPosition.y,
                standing.playerRadius,
                standing.playerHeight,
                staticModelColliders)) {
        return false;
    }
    return SectorDoorDynamicCollidersAllowPlayerHeight(
            positionXZ,
            state.feetPosition.y,
            standing.playerRadius,
            standing.playerHeight,
            dynamicDoorColliders);
}

SectorLiquidPhysicsConfig LiquidPhysicsConfig(
        const PlayerLiquidApplicationSettings& settings)
{
    return SectorLiquidPhysicsConfig{
            settings.entrySlowdownSeconds,
            settings.waterDragPerSecond,
            settings.surfaceRecoveryFrequencyHz};
}

bool NpcCylindersAllowPlayerPlacement(
        Vector2 positionXZ,
        float feetY,
        const SectorFpsControllerConfig& config,
        const std::vector<NpcCollisionCylinder>* obstacles)
{
    if (obstacles == nullptr) return true;
    const float top = feetY + config.playerHeight;
    for (const NpcCollisionCylinder& obstacle : *obstacles) {
        if (obstacle.stableId == 0 || obstacle.radius <= 0.0f
                || obstacle.height <= 0.0f) {
            continue;
        }
        const float obstacleTop = obstacle.feetPosition.y + obstacle.height;
        if (top <= obstacle.feetPosition.y + GameplayFloorSnapEpsilon
                || feetY >= obstacleTop - GameplayFloorSnapEpsilon) {
            continue;
        }
        const float dx = positionXZ.x - obstacle.feetPosition.x;
        const float dz = positionXZ.y - obstacle.feetPosition.z;
        const float combinedRadius = config.playerRadius + obstacle.radius;
        if (dx * dx + dz * dz < combinedRadius * combinedRadius) {
            return false;
        }
    }
    return true;
}

bool TryBeginSectorLiquidExit(
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorDynamicDoorCollider>& dynamicDoorColliders,
        const std::vector<SectorStaticModelCollider>& staticModelColliders,
        const std::vector<NpcCollisionCylinder>* npcCollisionCylinders,
        const SectorFpsControllerInput& input,
        const PlayerLiquidApplicationSettings& settings)
{
    SectorLiquidMovementState& liquid = controllerState.liquidMovement;
    SectorFpsControllerState& player = controllerState.fpsControllerState;
    const SectorFpsControllerConfig config =
            NormalizeSectorFpsControllerConfig(controllerState.fpsControllerConfig);
    const float stableSurfaceFeetY = liquid.contact.surfaceY
            + SectorLiquidSurfaceEyeOffsetWorld - config.eyeHeight;
    if (!collisionState.sectorCollisionWorldValid
            || !liquid.swimming || !liquid.surfaceLatched
            || !liquid.contact.hasLiquid || liquid.exitingWater
            || liquid.impactEntryActive
            || player.feetPosition.y < stableSurfaceFeetY - 0.15f
            || !input.swimUp || !input.moveForward || input.swimDown
            || player.verticalVelocity < -0.5f) {
        return false;
    }

    const float maximumHeight = std::clamp(
            std::isfinite(settings.maximumExitLedgeHeightWorld)
                    ? settings.maximumExitLedgeHeightWorld : 0.75f,
            0.0f, 3.0f);
    const Vector2 forward{
            std::cos(player.yawRadians), std::sin(player.yawRadians)};
    const Vector2 start{player.feetPosition.x, player.feetPosition.z};
    const float maximumDistance = config.playerRadius * 2.0f + 0.75f;
    constexpr int ProbeCount = 6;
    for (int probe = 1; probe <= ProbeCount; ++probe) {
        const float distance = maximumDistance
                * static_cast<float>(probe) / static_cast<float>(ProbeCount);
        const Vector2 candidate{
                start.x + forward.x * distance,
                start.y + forward.y * distance};
        const int sectorId = collisionState.sectorCollisionWorld
                .FindSectorContainingPointPreferCurrent(
                        candidate, player.currentSectorId);
        if (sectorId == 0) continue;

        SectorCollisionHeights heights;
        if (!collisionState.sectorCollisionWorld.ResolveActorVerticalContext(
                    sectorId,
                    SectorCollisionVerticalQuery{
                            candidate,
                            liquid.contact.surfaceY + maximumHeight,
                            config.playerRadius,
                            config.playerHeight,
                            maximumHeight + GameplayFloorSnapEpsilon,
                            true},
                    &heights)) {
            continue;
        }
        const float ledgeHeight = heights.floorZ - liquid.contact.surfaceY;
        if (ledgeHeight < -GameplayFloorSnapEpsilon
                || ledgeHeight > maximumHeight + GameplayFloorSnapEpsilon
                || heights.floorZ + config.playerHeight
                        > heights.ceilingZ + GameplayFloorSnapEpsilon) {
            continue;
        }
        SectorCollisionHeights baseHeights;
        collisionState.sectorCollisionWorld.GetSectorFloorCeiling(
                sectorId, &baseHeights);
        int resolvedSectorId = collisionState.sectorCollisionWorld
                .FindSectorForPlayerFootprint(
                        candidate,
                        sectorId,
                        heights.floorZ,
                        true,
                        SectorCollisionMoveConfig{
                                config.playerRadius,
                                config.playerHeight,
                                config.stepHeight,
                                4});
        const bool structuralSupport = heights.floorZ
                > baseHeights.floorZ + GameplayFloorSnapEpsilon;
        if (resolvedSectorId == 0
                || (!structuralSupport
                        && !collisionState.sectorCollisionWorld
                                .AllowsPrismPlacement(
                                        candidate,
                                        config.playerRadius,
                                        heights.floorZ,
                                        heights.floorZ + config.playerHeight,
                                        sectorId,
                                        &resolvedSectorId))
                || !SectorStaticModelCollidersAllowPlayerHeight(
                        candidate,
                        heights.floorZ,
                        config.playerRadius,
                        config.playerHeight,
                        staticModelColliders)
                || !SectorDoorDynamicCollidersAllowPlayerHeight(
                        candidate,
                        heights.floorZ,
                        config.playerRadius,
                        config.playerHeight,
                        dynamicDoorColliders)
                || !NpcCylindersAllowPlayerPlacement(
                        candidate,
                        heights.floorZ,
                        config,
                        npcCollisionCylinders)) {
            continue;
        }

        liquid.exitingWater = true;
        liquid.impactEntryActive = false;
        liquid.exitStartFeetPosition = player.feetPosition;
        liquid.exitTargetFeetPosition = Vector3{
                candidate.x, heights.floorZ, candidate.y};
        liquid.exitTargetSectorId = resolvedSectorId;
        liquid.exitElapsedSeconds = 0.0f;
        player.verticalVelocity = 0.0f;
        return true;
    }
    return false;
}

enum class SectorLiquidExitTransitionStatus {
    InProgress,
    Completed,
    Aborted,
};

struct SectorLiquidExitTransitionUpdate {
    SectorLiquidExitTransitionStatus status =
            SectorLiquidExitTransitionStatus::InProgress;
    float unusedSeconds = 0.0f;
};

SectorLiquidExitTransitionUpdate UpdateSectorLiquidExitTransition(
        SectorEditorPreviewControllerState& controllerState,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& dynamicDoorColliders,
        const std::vector<SectorStaticModelCollider>& staticModelColliders,
        const std::vector<NpcCollisionCylinder>* npcCollisionCylinders,
        const PlayerLiquidApplicationSettings& settings,
        float dt)
{
    SectorLiquidMovementState& liquid = controllerState.liquidMovement;
    SectorFpsControllerState& player = controllerState.fpsControllerState;
    if (!liquid.exitingWater) {
        return SectorLiquidExitTransitionUpdate{
                SectorLiquidExitTransitionStatus::Completed,
                std::max(0.0f, dt)};
    }
    const SectorFpsControllerConfig config =
            NormalizeSectorFpsControllerConfig(controllerState.fpsControllerConfig);
    const Vector2 targetXZ{
            liquid.exitTargetFeetPosition.x,
            liquid.exitTargetFeetPosition.z};
    SectorCollisionHeights targetHeights;
    const bool targetClear = collisionWorld.ResolveActorVerticalContext(
            liquid.exitTargetSectorId,
            SectorCollisionVerticalQuery{
                    targetXZ,
                    liquid.exitTargetFeetPosition.y,
                    config.playerRadius,
                    config.playerHeight,
                    config.stepHeight,
                    true},
            &targetHeights)
            && std::fabs(
                    targetHeights.floorZ
                            - liquid.exitTargetFeetPosition.y)
                    <= GameplayFloorSnapEpsilon
            && targetHeights.floorZ + config.playerHeight
                    <= targetHeights.ceilingZ + GameplayFloorSnapEpsilon
            && SectorStaticModelCollidersAllowPlayerHeight(
                    targetXZ,
                    liquid.exitTargetFeetPosition.y,
                    config.playerRadius,
                    config.playerHeight,
                    staticModelColliders)
            && SectorDoorDynamicCollidersAllowPlayerHeight(
                    targetXZ,
                    liquid.exitTargetFeetPosition.y,
                    config.playerRadius,
                    config.playerHeight,
                    dynamicDoorColliders)
            && NpcCylindersAllowPlayerPlacement(
                    targetXZ,
                    liquid.exitTargetFeetPosition.y,
                    config,
                    npcCollisionCylinders);
    if (!targetClear) {
        player.feetPosition = liquid.exitStartFeetPosition;
        player.verticalVelocity = 0.0f;
        liquid.exitingWater = false;
        liquid.surfaceLatched = true;
        liquid.swimming = true;
        return SectorLiquidExitTransitionUpdate{
                SectorLiquidExitTransitionStatus::Aborted, 0.0f};
    }
    const float duration = std::clamp(
            std::isfinite(settings.exitTransitionDurationSeconds)
                    ? settings.exitTransitionDurationSeconds : 0.30f,
            0.1f, 2.0f);
    const float safeDt = std::max(0.0f, dt);
    const float remainingDuration = std::max(
            0.0f, duration - liquid.exitElapsedSeconds);
    const float consumedSeconds = std::min(safeDt, remainingDuration);
    liquid.exitElapsedSeconds += consumedSeconds;
    const float progress = std::clamp(
            liquid.exitElapsedSeconds / duration, 0.0f, 1.0f);
    const float liftY = liquid.exitTargetFeetPosition.y
            + GameplayFloorSnapEpsilon * 2.0f;
    const Vector3 proposed = EvaluateSectorLiquidExitTrajectory(
            liquid.exitStartFeetPosition,
            liquid.exitTargetFeetPosition,
            liftY,
            progress);

    player.feetPosition = proposed;
    player.currentSectorId = liquid.exitTargetSectorId;
    player.grounded = progress >= 1.0f;
    player.verticalVelocity = 0.0f;
    if (progress >= 1.0f) {
        liquid.exitingWater = false;
        liquid.swimming = false;
        liquid.surfaceLatched = false;
        return SectorLiquidExitTransitionUpdate{
                SectorLiquidExitTransitionStatus::Completed,
                std::max(0.0f, safeDt - consumedSeconds)};
    }
    return SectorLiquidExitTransitionUpdate{
            SectorLiquidExitTransitionStatus::InProgress, 0.0f};
}

} // namespace

SectorViewPose ActiveSectorEditorPreviewPose(
        const SectorEditorPreviewControllerState& controllerState,
        const SectorMeshRenderer& preview)
{
    if (controllerState.previewControlMode == SectorPreviewControlMode::Gameplay) {
        return SectorFpsControllerVisualPose(
                controllerState.fpsControllerState,
                controllerState.fpsControllerConfig,
                controllerState.visualStepOffsetY,
                controllerState.headBobState.offset,
                controllerState.landingDipState.offsetY);
    }
    (void)preview;
    return controllerState.freeflyController.pose;
}

void ApplySectorEditorGameplayPoseToPreview(
        const SectorEditorPreviewControllerState& controllerState,
        SectorMeshRenderer& preview)
{
    preview.ApplyRendererPose(SectorFpsControllerVisualPose(
            controllerState.fpsControllerState,
            controllerState.fpsControllerConfig,
            controllerState.visualStepOffsetY,
            controllerState.headBobState.offset,
            controllerState.landingDipState.offsetY));
}

bool ToggleSectorEditorPreviewControlMode(
        bool preview3DActive,
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorStaticModelCollider>& staticModelColliders,
        SectorMeshRenderer& preview)
{
    if (!preview3DActive || !preview.IsRendererReady()) {
        return false;
    }

    controllerState.fpsControllerConfig = NormalizeSectorFpsControllerConfig(controllerState.fpsControllerConfig);
    if (controllerState.previewControlMode == SectorPreviewControlMode::FreeFly) {
        ClearPreviewGameplayVisualState(controllerState);
        controllerState.fpsControllerState = SectorFpsControllerStateFromCameraPose(
                controllerState.freeflyController.pose,
                controllerState.fpsControllerConfig);
        controllerState.fpsControllerState.verticalVelocity = 0.0f;
        controllerState.previewControlMode = SectorPreviewControlMode::Gameplay;
        InitializeSectorEditorGameplayVerticalState(
                collisionState,
                controllerState,
                staticModelColliders);
        ApplySectorEditorGameplayPoseToPreview(controllerState, preview);
    } else {
        const bool mouseLookEnabled = controllerState.freeflyController.mouseLookEnabled;
        ClearSectorFpsLandingDip(controllerState.landingDipState);
        ApplySectorEditorGameplayPoseToPreview(controllerState, preview);
        ResetSectorFreeflyController(controllerState.freeflyController, preview.RendererPose());
        SetSectorFreeflyMouseLookEnabled(controllerState.freeflyController, mouseLookEnabled);
        ResetSectorFpsCrouch(controllerState.fpsControllerState);
        ClearPreviewGameplayVisualState(controllerState);
        controllerState.previewControlMode = SectorPreviewControlMode::FreeFly;
        ResetPreviewCollisionState(collisionState, controllerState);
    }
    return true;
}

bool RebuildSectorEditorCollisionWorld(
        const SectorTopologyMap& topologyMap,
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorStaticModelCollider>& staticModelColliders)
{
    std::string error;
    if (!collisionState.sectorCollisionWorld.BuildFromTopology(topologyMap, &error)) {
        collisionState.sectorCollisionWorldValid = false;
        ResetPreviewCollisionAndVisualState(collisionState, controllerState);
        collisionState.sectorCollisionWorldWarning = error.empty()
                ? "Collision world build failed"
                : "Collision world build failed: " + error;
        return false;
    }

    collisionState.sectorCollisionWorldValid = true;
    collisionState.sectorCollisionWorldWarning.clear();
    if (controllerState.previewControlMode == SectorPreviewControlMode::Gameplay) {
        RefreshSectorEditorGameplaySectorAndVerticalContext(collisionState, controllerState);
        collisionState.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
                controllerState.fpsControllerState,
                controllerState.fpsControllerConfig,
                BuildSectorEditorGameplayVerticalContext(
                        collisionState,
                        controllerState,
                        staticModelColliders),
                0.0f);
        ClearPreviewGameplayVisualState(controllerState);
    } else {
        collisionState.previewCollisionSectorId = 0;
        ClearPreviewGameplayVisualState(controllerState);
    }
    return true;
}

SectorFpsVerticalContext BuildSectorEditorGameplayVerticalContext(
        const SectorEditorPreviewCollisionState& collisionState,
        const SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorStaticModelCollider>& staticModelColliders)
{
    const SectorFpsControllerConfig config = EffectiveSectorFpsControllerConfig(
            controllerState.fpsControllerState,
            controllerState.fpsControllerConfig);
    const SectorFpsControllerState& state = controllerState.fpsControllerState;
    return BuildSectorStaticModelVerticalContext(
            BuildSectorOnlyVerticalContext(
                    collisionState,
                    state.currentSectorId,
                    {state.feetPosition.x, state.feetPosition.z},
                    state.feetPosition.y,
                    state.grounded,
                    config),
            state,
            config,
            staticModelColliders);
}

void RefreshSectorEditorGameplaySectorAndVerticalContext(
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState)
{
    if (!collisionState.sectorCollisionWorldValid) {
        ResetPreviewCollisionAndVisualState(collisionState, controllerState);
        return;
    }

    const SectorFpsControllerConfig normalizedConfig =
            EffectiveSectorFpsControllerConfig(
                    controllerState.fpsControllerState,
                    controllerState.fpsControllerConfig);
    controllerState.fpsControllerState.currentSectorId =
            collisionState.sectorCollisionWorld.FindSectorForPlayerFootprint(
                    Vector2{
                            controllerState.fpsControllerState.feetPosition.x,
                            controllerState.fpsControllerState.feetPosition.z},
                    controllerState.fpsControllerState.currentSectorId,
                    controllerState.fpsControllerState.feetPosition.y,
                    controllerState.fpsControllerState.grounded,
                    SectorCollisionMoveConfig{
                            normalizedConfig.playerRadius,
                            normalizedConfig.playerHeight,
                            normalizedConfig.stepHeight,
                            4});
    collisionState.previewCollisionSectorId = controllerState.fpsControllerState.currentSectorId;
}

void InitializeSectorEditorGameplayVerticalState(
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorStaticModelCollider>& staticModelColliders)
{
    controllerState.fpsControllerState.grounded = false;
    controllerState.fpsControllerState.verticalVelocity = 0.0f;
    RefreshSectorEditorGameplaySectorAndVerticalContext(collisionState, controllerState);

    const SectorFpsVerticalContext context = BuildSectorEditorGameplayVerticalContext(
            collisionState,
            controllerState,
            staticModelColliders);
    if (!context.hasSector) {
        ResetPreviewCollisionAndVisualState(collisionState, controllerState);
        return;
    }

    if (controllerState.fpsControllerState.feetPosition.y <= context.floorZ + GameplayFloorSnapEpsilon) {
        controllerState.fpsControllerState.feetPosition.y = context.floorZ;
        controllerState.fpsControllerState.grounded = true;
    }
    collisionState.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
            controllerState.fpsControllerState,
            controllerState.fpsControllerConfig,
            context,
            0.0f);
    ClearPreviewGameplayVisualState(controllerState);
}

void UpdateSectorEditorGameplayPreview(
        const std::vector<SectorDynamicDoorCollider>& dynamicDoorColliders,
        const std::vector<SectorStaticModelCollider>& staticModelColliders,
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const SectorTopologyMap* topologyMap,
        bool previewSettingsModalOpen,
        const SectorFpsControllerInput& controllerInput,
        const PlayerLiquidApplicationSettings& liquidSettings,
        float previousVisualEyeY,
        float dt,
        const std::vector<NpcCollisionCylinder>* npcCollisionCylinders)
{
    controllerState.frameEvents = SectorFpsFrameEvents{};
    if (!std::isfinite(controllerState.landingDipState.offsetY)) {
        ClearSectorFpsLandingDip(controllerState.landingDipState);
    }
    const float previousStanceEyeHeight = EffectiveSectorFpsControllerConfig(
            controllerState.fpsControllerState,
            controllerState.fpsControllerConfig).eyeHeight;
    UpdateSectorFpsMouseLook(
            controllerState.fpsControllerState,
            controllerState.fpsControllerConfig,
            controllerInput);
    if (topologyMap != nullptr
            && UpdateSectorLadderTraversal(
                    controllerState.ladderTraversal,
                    controllerState.fpsControllerState,
                    controllerState.fpsControllerConfig,
                    controllerInput,
                    *topologyMap,
                    collisionState.sectorCollisionWorldValid
                            ? &collisionState.sectorCollisionWorld : nullptr,
                    dt)) {
        collisionState.previewMoveResult = {};
        collisionState.previewCollisionNoclipFallback = false;
        ClearPreviewGameplayVisualState(controllerState);
        RefreshSectorEditorGameplaySectorAndVerticalContext(
                collisionState, controllerState);
        return;
    }
    SectorLiquidContact liquidContact;
    if (topologyMap != nullptr) {
        liquidContact = SampleSectorLiquidContact(
                *topologyMap,
                controllerState.fpsControllerState.currentSectorId,
                controllerState.fpsControllerState.feetPosition,
                controllerState.fpsControllerConfig);
    }
    const bool wasSwimming = controllerState.liquidMovement.swimming;
    if (!controllerState.liquidMovement.exitingWater) {
        UpdateSectorLiquidMovementState(
                controllerState.liquidMovement,
                liquidContact,
                controllerInput.swimDown);
    }
    if (!wasSwimming && controllerState.liquidMovement.swimming) {
        if (!controllerInput.swimDown
                && controllerState.fpsControllerState.verticalVelocity < 0.0f) {
            controllerState.liquidMovement.surfaceLatched = true;
        }
        const SectorFpsVerticalContext entryContext =
                BuildSectorEditorGameplayVerticalContext(
                        collisionState, controllerState, staticModelColliders);
        BeginSectorLiquidImpactEntry(
                controllerState.liquidMovement,
                controllerState.fpsControllerState,
                controllerState.fpsControllerConfig,
                LiquidPhysicsConfig(liquidSettings),
                entryContext.hasSector
                        ? entryContext.floorZ
                        : controllerState.liquidMovement.contact.bottomY);
    }
    if (controllerState.liquidMovement.impactEntryActive
            && controllerInput.swimUp) {
        controllerState.liquidMovement.impactEntryActive = false;
        controllerState.fpsControllerState.verticalVelocity = 0.0f;
    }
    TryBeginSectorLiquidExit(
            collisionState,
            controllerState,
            dynamicDoorColliders,
            staticModelColliders,
            npcCollisionCylinders,
            controllerInput,
            liquidSettings);
    bool completedLiquidExitThisFrame = false;
    if (controllerState.liquidMovement.exitingWater) {
        const SectorLiquidExitTransitionUpdate exitUpdate =
                UpdateSectorLiquidExitTransition(
                        controllerState,
                        collisionState.sectorCollisionWorld,
                        dynamicDoorColliders,
                        staticModelColliders,
                        npcCollisionCylinders,
                        liquidSettings,
                        dt);
        RefreshSectorEditorGameplaySectorAndVerticalContext(
                collisionState, controllerState);
        if (topologyMap != nullptr) {
            liquidContact = SampleSectorLiquidContact(
                    *topologyMap,
                    controllerState.fpsControllerState.currentSectorId,
                    controllerState.fpsControllerState.feetPosition,
                    controllerState.fpsControllerConfig);
            controllerState.liquidMovement.contact = liquidContact;
            const float eyeY = SectorFpsControllerEyePosition(
                    controllerState.fpsControllerState,
                    controllerState.fpsControllerConfig).y;
            controllerState.liquidMovement.cameraSubmerged =
                    UpdateSectorLiquidCameraSubmersion(
                            controllerState.liquidMovement.cameraSubmerged,
                            liquidContact,
                            eyeY);
        }
        collisionState.previewMoveResult = {};
        collisionState.previewVerticalResult = {};
        ClearPreviewGameplayVisualState(controllerState);
        if (exitUpdate.status != SectorLiquidExitTransitionStatus::Completed
                || exitUpdate.unusedSeconds <= 0.0f) {
            return;
        }
        completedLiquidExitThisFrame = true;
        dt = exitUpdate.unusedSeconds;
        previousVisualEyeY = SectorFpsControllerEyePosition(
                controllerState.fpsControllerState,
                controllerState.fpsControllerConfig).y;
    }
    const bool swimming = controllerState.liquidMovement.swimming;
    if (swimming) {
        ResetSectorFpsCrouch(controllerState.fpsControllerState);
        controllerState.fpsControllerState.grounded = false;
    }
    const bool standingClearance = HasSectorEditorGameplayStandingClearance(
            collisionState,
            controllerState,
            dynamicDoorColliders,
            staticModelColliders);
    if (!swimming && controllerInput.crouchTogglePressed) {
        TryToggleSectorFpsCrouch(
                controllerState.fpsControllerState,
                standingClearance);
    }
    if (!swimming) {
        UpdateSectorFpsCrouch(
                controllerState.fpsControllerState,
                standingClearance,
                dt);
    }
    const SectorFpsControllerConfig effectiveConfig = EffectiveSectorFpsControllerConfig(
            controllerState.fpsControllerState,
            controllerState.fpsControllerConfig);
    const float previousStepVisualEyeY =
            previousVisualEyeY
            - controllerState.landingDipState.offsetY
            + (effectiveConfig.eyeHeight - previousStanceEyeHeight);
    const Vector3 swimMovement = swimming
            ? ComputeSectorLiquidSwimMovementDelta(
                    controllerState.fpsControllerState,
                    controllerState.fpsControllerConfig,
                    controllerInput,
                    controllerState.liquidMovement.surfaceLatched,
                    dt)
            : Vector3{};
    const Vector2 desiredHorizontalMovement = swimming
            ? Vector2{swimMovement.x, swimMovement.z}
            : ComputeSectorFpsHorizontalMovementDelta(
                    controllerState.fpsControllerState,
                    controllerState.fpsControllerConfig,
                    controllerInput,
                    dt);
    const Vector2 previousFeetXZ{
            controllerState.fpsControllerState.feetPosition.x,
            controllerState.fpsControllerState.feetPosition.z};
    collisionState.previewMoveResult = SectorCollisionMoveResult{};
    collisionState.previewCollisionNoclipFallback = false;
    if (collisionState.sectorCollisionWorldValid) {
        const Vector2 feetXZ{
                controllerState.fpsControllerState.feetPosition.x,
                controllerState.fpsControllerState.feetPosition.z};
        if (collisionState.sectorCollisionWorld.FindSector(
                    controllerState.fpsControllerState.currentSectorId) == nullptr) {
            controllerState.fpsControllerState.currentSectorId =
                    collisionState.sectorCollisionWorld.FindSectorContainingPoint(feetXZ);
        }
        const int previousSectorId = controllerState.fpsControllerState.currentSectorId;
        const float previousFeetY = controllerState.fpsControllerState.feetPosition.y;
        const bool wasGrounded = controllerState.fpsControllerState.grounded;

        if (controllerState.fpsControllerState.currentSectorId != 0) {
            SectorCollisionMoveResult moveResult =
                    collisionState.sectorCollisionWorld.ResolveMovement(
                            SectorCollisionMoveState{
                                    feetXZ,
                                    controllerState.fpsControllerState.feetPosition.y,
                                    controllerState.fpsControllerState.currentSectorId,
                                    controllerState.fpsControllerState.grounded},
                            desiredHorizontalMovement,
                            SectorCollisionMoveConfig{
                                    effectiveConfig.playerRadius,
                                    effectiveConfig.playerHeight,
                                    effectiveConfig.stepHeight,
                                    4});
            moveResult = ResolveSectorDoorDynamicCollidersForPlayerMovement(
                    SectorCollisionMoveState{
                            feetXZ,
                            controllerState.fpsControllerState.feetPosition.y,
                            controllerState.fpsControllerState.currentSectorId,
                            controllerState.fpsControllerState.grounded},
                    moveResult,
                    SectorCollisionMoveConfig{
                            effectiveConfig.playerRadius,
                            effectiveConfig.playerHeight,
                            effectiveConfig.stepHeight,
                            4},
                    dynamicDoorColliders);
            moveResult = ResolveSectorStaticModelCollidersForPlayerMovement(
                    SectorCollisionMoveState{
                            feetXZ,
                            controllerState.fpsControllerState.feetPosition.y,
                            controllerState.fpsControllerState.currentSectorId,
                            controllerState.fpsControllerState.grounded},
                    moveResult,
                    SectorCollisionMoveConfig{
                            effectiveConfig.playerRadius,
                            effectiveConfig.playerHeight,
                            effectiveConfig.stepHeight,
                            4},
                    BuildSectorOnlyVerticalContext(
                            collisionState,
                            moveResult.currentSectorId,
                            moveResult.positionXZ,
                            controllerState.fpsControllerState.feetPosition.y,
                            controllerState.fpsControllerState.grounded,
                            effectiveConfig),
                    staticModelColliders);
            if (npcCollisionCylinders != nullptr
                    && !npcCollisionCylinders->empty()) {
                moveResult = ResolveNpcCollisionCylindersForMovement(
                        SectorCollisionMoveState{
                                feetXZ,
                                controllerState.fpsControllerState.feetPosition.y,
                                controllerState.fpsControllerState.currentSectorId,
                                controllerState.fpsControllerState.grounded},
                        moveResult,
                        SectorCollisionMoveConfig{
                                effectiveConfig.playerRadius,
                                effectiveConfig.playerHeight,
                                effectiveConfig.stepHeight,
                                4},
                        -2,
                        npcCollisionCylinders->data(),
                        npcCollisionCylinders->size());
            }
            SectorCollisionHeights movedHeights;
            if (wasGrounded
                    && moveResult.currentSectorId != previousSectorId
                    && collisionState.sectorCollisionWorld.GetSectorFloorCeiling(
                            moveResult.currentSectorId,
                            &movedHeights)
                    && movedHeights.floorZ - previousFeetY
                            > effectiveConfig.stepHeight + GameplayFloorSnapEpsilon) {
                moveResult.positionXZ = feetXZ;
                moveResult.currentSectorId = previousSectorId;
                moveResult.blockedByStep = true;
            }
            collisionState.previewMoveResult = moveResult;
            controllerState.fpsControllerState.feetPosition.x = moveResult.positionXZ.x;
            controllerState.fpsControllerState.feetPosition.z = moveResult.positionXZ.y;
            controllerState.fpsControllerState.currentSectorId = moveResult.currentSectorId;
        } else {
            collisionState.previewCollisionNoclipFallback = true;
            controllerState.fpsControllerState.feetPosition.x += desiredHorizontalMovement.x;
            controllerState.fpsControllerState.feetPosition.z += desiredHorizontalMovement.y;
        }
    } else {
        collisionState.previewCollisionNoclipFallback = true;
        controllerState.fpsControllerState.feetPosition.x += desiredHorizontalMovement.x;
        controllerState.fpsControllerState.feetPosition.z += desiredHorizontalMovement.y;
    }
    RefreshSectorEditorGameplaySectorAndVerticalContext(collisionState, controllerState);
    bool startedJump = false;
    if (!swimming && !completedLiquidExitThisFrame
            && controllerInput.jumpPressed) {
        startedJump = TryStartSectorFpsJump(
                controllerState.fpsControllerState,
                controllerState.fpsControllerConfig);
        if (startedJump) {
            ClearSectorFpsLandingDip(controllerState.landingDipState);
        }
    }
    if (swimming) {
        if (topologyMap != nullptr) {
            liquidContact = SampleSectorLiquidContact(
                    *topologyMap,
                    controllerState.fpsControllerState.currentSectorId,
                    controllerState.fpsControllerState.feetPosition,
                    controllerState.fpsControllerConfig);
            controllerState.liquidMovement.contact = liquidContact;
        }
        const SectorFpsVerticalContext verticalContext =
                BuildSectorEditorGameplayVerticalContext(
                        collisionState, controllerState, staticModelColliders);
        const float desiredVerticalVelocity = dt > 0.0f
                ? swimMovement.y / dt : 0.0f;
        if (verticalContext.hasSector) {
            const float maximumFeetY = std::max(
                    verticalContext.floorZ,
                    verticalContext.ceilingZ - effectiveConfig.playerHeight);
            UpdateSectorLiquidSwimmingVerticalMotion(
                    controllerState.fpsControllerState,
                    controllerState.fpsControllerConfig,
                    controllerState.liquidMovement,
                    LiquidPhysicsConfig(liquidSettings),
                    desiredVerticalVelocity,
                    verticalContext.floorZ,
                    maximumFeetY,
                    dt);
        } else {
            UpdateSectorLiquidSwimmingVerticalMotion(
                    controllerState.fpsControllerState,
                    controllerState.fpsControllerConfig,
                    controllerState.liquidMovement,
                    LiquidPhysicsConfig(liquidSettings),
                    desiredVerticalVelocity,
                    -INFINITY,
                    INFINITY,
                    dt);
        }
        controllerState.fpsControllerState.grounded = false;
        collisionState.previewVerticalResult = SectorFpsVerticalResult{
                verticalContext.hasSector,
                false,
                verticalContext.floorZ,
                verticalContext.ceilingZ,
                0.0f,
                SectorFpsVerticalTransition::None};
        if (topologyMap != nullptr) {
            liquidContact = SampleSectorLiquidContact(
                    *topologyMap,
                    controllerState.fpsControllerState.currentSectorId,
                    controllerState.fpsControllerState.feetPosition,
                    controllerState.fpsControllerConfig);
            controllerState.liquidMovement.contact = liquidContact;
            const float eyeY = SectorFpsControllerEyePosition(
                    controllerState.fpsControllerState,
                    controllerState.fpsControllerConfig).y;
            if (!controllerInput.swimDown
                    && !controllerState.liquidMovement.surfaceLatched
                    && liquidContact.hasLiquid
                    && eyeY >= liquidContact.surfaceY
                            - SectorLiquidSurfaceEyeOffsetWorld) {
                controllerState.liquidMovement.surfaceLatched = true;
            }
            controllerState.liquidMovement.cameraSubmerged =
                    UpdateSectorLiquidCameraSubmersion(
                            controllerState.liquidMovement.cameraSubmerged,
                            liquidContact,
                            eyeY);
        }
    } else {
        collisionState.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
                controllerState.fpsControllerState,
                controllerState.fpsControllerConfig,
                BuildSectorEditorGameplayVerticalContext(
                        collisionState,
                        controllerState,
                        staticModelColliders),
                dt);
        if (topologyMap != nullptr) {
            liquidContact = SampleSectorLiquidContact(
                    *topologyMap,
                    controllerState.fpsControllerState.currentSectorId,
                    controllerState.fpsControllerState.feetPosition,
                    controllerState.fpsControllerConfig);
            controllerState.liquidMovement.contact = liquidContact;
        }
        const float eyeY = SectorFpsControllerEyePosition(
                controllerState.fpsControllerState,
                controllerState.fpsControllerConfig).y;
        controllerState.liquidMovement.cameraSubmerged =
                UpdateSectorLiquidCameraSubmersion(
                        controllerState.liquidMovement.cameraSubmerged,
                        liquidContact,
                        eyeY);
    }
    controllerState.frameEvents = BuildSectorFpsFrameEvents(
            startedJump,
            collisionState.previewVerticalResult);
    if (swimming) {
        controllerState.visualStepOffsetY = 0.0f;
        ClearSectorFpsLandingDip(controllerState.landingDipState);
        ClearSectorFpsHeadBob(controllerState.headBobState);
        ClearSectorFpsFootstepCadence(controllerState.footstepCadenceState);
    } else if (collisionState.previewCollisionNoclipFallback || !collisionState.previewVerticalResult.hasSector) {
        controllerState.visualStepOffsetY = 0.0f;
        ClearSectorFpsLandingDip(controllerState.landingDipState);
    } else if (startedJump) {
        controllerState.visualStepOffsetY = 0.0f;
    } else {
        ApplySectorFpsVisualStepSmoothing(
                controllerState.visualStepOffsetY,
                collisionState.previewVerticalResult.transition,
                previousStepVisualEyeY,
                controllerState.fpsControllerState,
                controllerState.fpsControllerConfig,
                DefaultSectorFpsStepSmoothingRate(),
                dt);
        UpdateSectorFpsLandingDip(
                controllerState.landingDipState,
                collisionState.previewVerticalResult,
                dt);
    }
    const Vector2 resolvedHorizontalMovement{
            controllerState.fpsControllerState.feetPosition.x - previousFeetXZ.x,
            controllerState.fpsControllerState.feetPosition.z - previousFeetXZ.y};
    const float resolvedHorizontalSpeed = dt > 0.0f
            ? Vector2Length(resolvedHorizontalMovement) / dt
            : 0.0f;
    controllerState.frameEvents.sprinting =
            SectorFpsInputUsesRunSpeed(controllerInput)
            && Vector2Length(resolvedHorizontalMovement) > 0.0001f;
    const bool headBobActive = !swimming
            && !collisionState.previewCollisionNoclipFallback
            && collisionState.previewVerticalResult.hasSector
            && controllerState.fpsControllerState.grounded
            && !previewSettingsModalOpen;
    UpdateSectorFpsHeadBob(
            controllerState.headBobState,
            controllerState.fpsControllerConfig,
            headBobActive,
            resolvedHorizontalSpeed,
            controllerState.fpsControllerState.yawRadians,
            dt);
    controllerState.frameEvents.footstep = UpdateSectorFpsFootstepCadence(
            controllerState.footstepCadenceState,
            controllerState.fpsControllerConfig,
            headBobActive && !controllerState.frameEvents.landed,
            Vector2Length(resolvedHorizontalMovement),
            resolvedHorizontalSpeed);
}

} // namespace game
