#pragma once

#include "sector_editor/SectorEditorPreviewTypes.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorSurfaceTypes.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorFreeflyController.h"

#include <string>

namespace game {

struct SectorEditorPreviewOverlayState {
    bool useBakedAmbientOcclusion = true;
    bool showObjectProbeDebugOverlay = false;
    bool previewUiHidden = false;
    PreviewDebugOverlayTab activePreviewDebugOverlayTab = PreviewDebugOverlayTab::None;
};

struct SectorEditorPreviewSelectionState {
    SectorSurfaceHit hoveredSurface3D;
    SectorSurfaceRef selectedSurface3D;
    TopologySurfaceEditTarget selectedTopologySurface3D;
};

struct SectorEditorPreviewControllerState {
    SectorPreviewControlMode previewControlMode = SectorPreviewControlMode::FreeFly;
    SectorFreeflyControllerState freeflyController;
    SectorFpsControllerConfig fpsControllerConfig;
    SectorFpsControllerState fpsControllerState;
    float visualStepOffsetY = 0.0f;
    SectorFpsHeadBobState headBobState;
    SectorFpsLandingDipState landingDipState;
    bool hasPreviewPose = false;
    SectorViewPose lastPreviewPose = {};
    SpotLightPilotPreviewRestoreState spotLightPilotPreviewRestore;
};

struct SectorEditorPreviewCollisionState {
    SectorCollisionWorld sectorCollisionWorld;
    bool sectorCollisionWorldValid = false;
    std::string sectorCollisionWorldWarning;
    int previewCollisionSectorId = 0;
    SectorFpsVerticalResult previewVerticalResult;
    SectorCollisionMoveResult previewMoveResult;
    bool previewCollisionNoclipFallback = false;
};

struct SectorEditorPreviewState {
    SectorEditorPreviewOverlayState overlay;
    SectorEditorPreviewSelectionState selection;
    SectorEditorPreviewControllerState controller;
    SectorEditorPreviewCollisionState collision;
};

} // namespace game
