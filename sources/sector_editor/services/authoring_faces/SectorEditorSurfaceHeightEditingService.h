#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <string>

namespace game {

enum class PreviewSurfaceHeightNudgePreset {
    Fine,
    Normal,
    Coarse
};

enum class PreviewSurfaceHeightTarget {
    None,
    Floor,
    Ceiling
};

struct PreviewSurfaceHeightAdjustmentState {
    bool active = false;
    bool changed = false;
    int faceAnchorId = -1;
    int topologySectorId = -1;
    PreviewSurfaceHeightTarget target = PreviewSurfaceHeightTarget::None;
    float originalFloorZ = 0.0f;
    float originalCeilingZ = 0.0f;
    PreviewSurfaceHeightNudgePreset preset =
            PreviewSurfaceHeightNudgePreset::Normal;
    SectorAuthoringGraph stagedGraph;
    SectorAuthoringDerivationResult stagedDerivation;
};

struct PreviewSurfaceHeightNudgeCandidate {
    bool valid = false;
    bool changedFromOriginal = false;
    SectorAuthoringGraph graph;
    SectorAuthoringDerivationResult derivation;
    float height = 0.0f;
};

struct PreviewSurfaceHeightAdjustmentResult {
    bool changed = false;
    bool committed = false;
};

struct SectorEditorSurfaceHeightEditingServiceContext {
    SectorEditorState& state;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    SelectionState& selectionState;
    SectorEditorPreviewSelectionState& previewSelectionState;
    PreviewSurfaceHeightAdjustmentState& adjustmentState;
    std::string& statusText;
};

bool IsSectorEditorPreviewSurfaceHeightAdjustable(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        SectorSurfaceRef surface);

float SectorEditorPreviewSurfaceHeightStepAuthored(
        PreviewSurfaceHeightNudgePreset preset);

class SectorEditorSurfaceHeightEditingService {
public:
    explicit SectorEditorSurfaceHeightEditingService(
            SectorEditorSurfaceHeightEditingServiceContext context);

    bool BeginPreviewAdjustment();
    bool BuildPreviewNudge(
            float deltaHeightAuthored,
            PreviewSurfaceHeightNudgeCandidate& outCandidate);
    void AcceptPreviewNudge(PreviewSurfaceHeightNudgeCandidate candidate);
    PreviewSurfaceHeightAdjustmentResult ApplyPreviewAdjustment();
    PreviewSurfaceHeightAdjustmentResult CancelPreviewAdjustment(
            const char* message);
    void SetPreviewAdjustmentPreset(PreviewSurfaceHeightNudgePreset preset);

    float CurrentHeight() const;
    const char* TargetName() const;

private:
    void ResetPreservingPreset();

    SectorEditorSurfaceHeightEditingServiceContext context_;
};

} // namespace game
