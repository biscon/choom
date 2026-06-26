#include "sector_editor/SectorEditorPreviewActions.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorMeshPreview.h"

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
    return preview.Pose();
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
                preview.Pose(),
                state.fpsControllerConfig);
        state.fpsControllerState.verticalVelocity = 0.0f;
        state.previewControlMode = SectorPreviewControlMode::Gameplay;
        InitializeSectorEditorGameplayVerticalState(state);
        ApplySectorEditorGameplayPoseToPreview(state, preview);
    } else {
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplySectorEditorGameplayPoseToPreview(state, preview);
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

} // namespace game
