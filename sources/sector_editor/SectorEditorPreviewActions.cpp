#include "sector_editor/SectorEditorPreviewActions.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorMeshPreview.h"

#include <raymath.h>

#include <cmath>

namespace game {

namespace {

void ClearPreviewGameplayVisualState(SectorEditorState& state)
{
    state.visualStepOffsetY = 0.0f;
    ClearSectorFpsHeadBob(state.headBobState);
    ClearSectorFpsLandingDip(state.landingDipState);
}

void ResetPreviewCollisionState(SectorEditorState& state)
{
    state.previewCollisionSectorId = 0;
    state.fpsControllerState.currentSectorId = 0;
    state.previewVerticalResult = SectorFpsVerticalResult{};
    state.previewMoveResult = SectorCollisionMoveResult{};
    state.previewCollisionNoclipFallback = false;
}

void ResetPreviewCollisionAndVisualState(SectorEditorState& state)
{
    ResetPreviewCollisionState(state);
    ClearPreviewGameplayVisualState(state);
}

} // namespace

SectorViewPose ActiveSectorEditorPreviewPose(
        const SectorEditorState& state,
        const SectorMeshPreview& preview)
{
    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        return SectorFpsControllerVisualPose(
                state.fpsControllerState,
                state.fpsControllerConfig,
                state.visualStepOffsetY,
                state.headBobState.offset,
                state.landingDipState.offsetY);
    }
    (void)preview;
    return state.freeflyController.pose;
}

void ApplySectorEditorGameplayPoseToPreview(
        const SectorEditorState& state,
        SectorMeshPreview& preview)
{
    preview.ApplyPose(SectorFpsControllerVisualPose(
            state.fpsControllerState,
            state.fpsControllerConfig,
            state.visualStepOffsetY,
            state.headBobState.offset,
            state.landingDipState.offsetY));
}

bool ToggleSectorEditorPreviewControlMode(
        SectorEditorState& state,
        SectorMeshPreview& preview)
{
    if (state.mode != SectorEditorMode::Preview3D || !preview.IsReady()) {
        return false;
    }

    state.fpsControllerConfig = NormalizeSectorFpsControllerConfig(state.fpsControllerConfig);
    if (state.previewControlMode == SectorPreviewControlMode::FreeFly) {
        ClearPreviewGameplayVisualState(state);
        state.fpsControllerState = SectorFpsControllerStateFromCameraPose(
                state.freeflyController.pose,
                state.fpsControllerConfig);
        state.fpsControllerState.verticalVelocity = 0.0f;
        state.previewControlMode = SectorPreviewControlMode::Gameplay;
        InitializeSectorEditorGameplayVerticalState(state);
        ApplySectorEditorGameplayPoseToPreview(state, preview);
    } else {
        const bool mouseLookEnabled = state.freeflyController.mouseLookEnabled;
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplySectorEditorGameplayPoseToPreview(state, preview);
        ResetSectorFreeflyController(state.freeflyController, preview.Pose());
        SetSectorFreeflyMouseLookEnabled(state.freeflyController, mouseLookEnabled);
        ClearPreviewGameplayVisualState(state);
        state.previewControlMode = SectorPreviewControlMode::FreeFly;
        ResetPreviewCollisionState(state);
    }
    return true;
}

bool RebuildSectorEditorCollisionWorld(SectorEditorState& state)
{
    std::string error;
    if (!state.sectorCollisionWorld.BuildFromTopology(state.topologyMap, &error)) {
        state.sectorCollisionWorldValid = false;
        ResetPreviewCollisionAndVisualState(state);
        state.sectorCollisionWorldWarning = error.empty()
                ? "Collision world build failed"
                : "Collision world build failed: " + error;
        return false;
    }

    state.sectorCollisionWorldValid = true;
    state.sectorCollisionWorldWarning.clear();
    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        RefreshSectorEditorGameplaySectorAndVerticalContext(state);
        state.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
                state.fpsControllerState,
                state.fpsControllerConfig,
                BuildSectorEditorGameplayVerticalContext(state),
                0.0f);
        ClearPreviewGameplayVisualState(state);
    } else {
        state.previewCollisionSectorId = 0;
        ClearPreviewGameplayVisualState(state);
    }
    return true;
}

SectorFpsVerticalContext BuildSectorEditorGameplayVerticalContext(
        const SectorEditorState& state)
{
    SectorFpsVerticalContext context;
    if (!state.sectorCollisionWorldValid || state.fpsControllerState.currentSectorId == 0) {
        return context;
    }

    SectorCollisionHeights heights;
    if (!state.sectorCollisionWorld.GetSectorFloorCeiling(
                state.fpsControllerState.currentSectorId,
                &heights)) {
        return context;
    }

    context.hasSector = true;
    context.floorZ = heights.floorZ;
    context.ceilingZ = heights.ceilingZ;
    return context;
}

void RefreshSectorEditorGameplaySectorAndVerticalContext(SectorEditorState& state)
{
    if (!state.sectorCollisionWorldValid) {
        ResetPreviewCollisionAndVisualState(state);
        return;
    }

    state.fpsControllerState.currentSectorId =
            state.sectorCollisionWorld.FindSectorContainingPointPreferCurrent(
                    Vector2{
                            state.fpsControllerState.feetPosition.x,
                            state.fpsControllerState.feetPosition.z},
                    state.fpsControllerState.currentSectorId);
    state.previewCollisionSectorId = state.fpsControllerState.currentSectorId;
}

void InitializeSectorEditorGameplayVerticalState(SectorEditorState& state)
{
    state.fpsControllerState.grounded = false;
    state.fpsControllerState.verticalVelocity = 0.0f;
    RefreshSectorEditorGameplaySectorAndVerticalContext(state);

    const SectorFpsVerticalContext context = BuildSectorEditorGameplayVerticalContext(state);
    if (!context.hasSector) {
        ResetPreviewCollisionAndVisualState(state);
        return;
    }

    if (state.fpsControllerState.feetPosition.y <= context.floorZ + GameplayFloorSnapEpsilon) {
        state.fpsControllerState.feetPosition.y = context.floorZ;
        state.fpsControllerState.grounded = true;
    }
    state.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
            state.fpsControllerState,
            state.fpsControllerConfig,
            context,
            0.0f);
    ClearPreviewGameplayVisualState(state);
}

void UpdateSectorEditorGameplayPreview(
        SectorEditorState& state,
        const SectorFpsControllerInput& controllerInput,
        float previousVisualEyeY,
        float dt)
{
    if (!std::isfinite(state.landingDipState.offsetY)) {
        ClearSectorFpsLandingDip(state.landingDipState);
    }
    const float previousStepVisualEyeY =
            previousVisualEyeY - state.landingDipState.offsetY;
    UpdateSectorFpsMouseLook(
            state.fpsControllerState,
            state.fpsControllerConfig,
            controllerInput);
    const Vector2 desiredHorizontalMovement = ComputeSectorFpsHorizontalMovementDelta(
            state.fpsControllerState,
            state.fpsControllerConfig,
            controllerInput,
            dt);
    const Vector2 previousFeetXZ{
            state.fpsControllerState.feetPosition.x,
            state.fpsControllerState.feetPosition.z};
    state.previewMoveResult = SectorCollisionMoveResult{};
    state.previewCollisionNoclipFallback = false;
    if (state.sectorCollisionWorldValid) {
        const Vector2 feetXZ{
                state.fpsControllerState.feetPosition.x,
                state.fpsControllerState.feetPosition.z};
        if (state.sectorCollisionWorld.FindSector(
                    state.fpsControllerState.currentSectorId) == nullptr) {
            state.fpsControllerState.currentSectorId =
                    state.sectorCollisionWorld.FindSectorContainingPoint(feetXZ);
        }
        const int previousSectorId = state.fpsControllerState.currentSectorId;
        const float previousFeetY = state.fpsControllerState.feetPosition.y;
        const bool wasGrounded = state.fpsControllerState.grounded;

        if (state.fpsControllerState.currentSectorId != 0) {
            const SectorFpsControllerConfig normalizedConfig =
                    NormalizeSectorFpsControllerConfig(state.fpsControllerConfig);
            SectorCollisionMoveResult moveResult =
                    state.sectorCollisionWorld.ResolveMovement(
                            SectorCollisionMoveState{
                                    feetXZ,
                                    state.fpsControllerState.feetPosition.y,
                                    state.fpsControllerState.currentSectorId,
                                    state.fpsControllerState.grounded},
                            desiredHorizontalMovement,
                            SectorCollisionMoveConfig{
                                    normalizedConfig.playerRadius,
                                    normalizedConfig.playerHeight,
                                    normalizedConfig.stepHeight,
                                    4});
            SectorCollisionHeights movedHeights;
            if (wasGrounded
                    && moveResult.currentSectorId != previousSectorId
                    && state.sectorCollisionWorld.GetSectorFloorCeiling(
                            moveResult.currentSectorId,
                            &movedHeights)
                    && movedHeights.floorZ - previousFeetY
                            > normalizedConfig.stepHeight + GameplayFloorSnapEpsilon) {
                moveResult.positionXZ = feetXZ;
                moveResult.currentSectorId = previousSectorId;
                moveResult.blockedByStep = true;
            }
            state.previewMoveResult = moveResult;
            state.fpsControllerState.feetPosition.x = moveResult.positionXZ.x;
            state.fpsControllerState.feetPosition.z = moveResult.positionXZ.y;
            state.fpsControllerState.currentSectorId = moveResult.currentSectorId;
        } else {
            state.previewCollisionNoclipFallback = true;
            state.fpsControllerState.feetPosition.x += desiredHorizontalMovement.x;
            state.fpsControllerState.feetPosition.z += desiredHorizontalMovement.y;
        }
    } else {
        state.previewCollisionNoclipFallback = true;
        state.fpsControllerState.feetPosition.x += desiredHorizontalMovement.x;
        state.fpsControllerState.feetPosition.z += desiredHorizontalMovement.y;
    }
    RefreshSectorEditorGameplaySectorAndVerticalContext(state);
    bool startedJump = false;
    if (controllerInput.jumpPressed) {
        startedJump = TryStartSectorFpsJump(
                state.fpsControllerState,
                state.fpsControllerConfig);
        if (startedJump) {
            ClearSectorFpsLandingDip(state.landingDipState);
        }
    }
    state.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
            state.fpsControllerState,
            state.fpsControllerConfig,
            BuildSectorEditorGameplayVerticalContext(state),
            dt);
    if (state.previewCollisionNoclipFallback || !state.previewVerticalResult.hasSector) {
        state.visualStepOffsetY = 0.0f;
        ClearSectorFpsLandingDip(state.landingDipState);
    } else if (startedJump) {
        state.visualStepOffsetY = 0.0f;
    } else {
        ApplySectorFpsVisualStepSmoothing(
                state.visualStepOffsetY,
                state.previewVerticalResult.transition,
                previousStepVisualEyeY,
                state.fpsControllerState,
                state.fpsControllerConfig,
                DefaultSectorFpsStepSmoothingRate(),
                dt);
        UpdateSectorFpsLandingDip(
                state.landingDipState,
                state.previewVerticalResult,
                dt);
    }
    const Vector2 resolvedHorizontalMovement{
            state.fpsControllerState.feetPosition.x - previousFeetXZ.x,
            state.fpsControllerState.feetPosition.z - previousFeetXZ.y};
    const float resolvedHorizontalSpeed = dt > 0.0f
            ? Vector2Length(resolvedHorizontalMovement) / dt
            : 0.0f;
    const bool headBobActive = !state.previewCollisionNoclipFallback
            && state.previewVerticalResult.hasSector
            && state.fpsControllerState.grounded
            && !state.previewSettingsModal.open;
    UpdateSectorFpsHeadBob(
            state.headBobState,
            state.fpsControllerConfig,
            headBobActive,
            resolvedHorizontalSpeed,
            state.fpsControllerState.yawRadians,
            dt);
}

} // namespace game
