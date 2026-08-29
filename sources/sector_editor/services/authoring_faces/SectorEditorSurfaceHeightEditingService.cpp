#include "sector_editor/services/authoring_faces/SectorEditorSurfaceHeightEditingService.h"

#include "sector_demo/SectorAuthoringGraph.h"

#include <cmath>
#include <utility>

namespace game {
namespace {

constexpr float MinimumAuthoredHeight = -512.0f;
constexpr float MaximumAuthoredHeight = 512.0f;

PreviewSurfaceHeightTarget TargetForSurface(SectorSurfaceRef surface)
{
    if (surface.kind == SectorSurfaceKind::Floor) {
        return PreviewSurfaceHeightTarget::Floor;
    }
    if (surface.kind == SectorSurfaceKind::Ceiling) {
        return PreviewSurfaceHeightTarget::Ceiling;
    }
    return PreviewSurfaceHeightTarget::None;
}

bool ResolveFaceAnchorTarget(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        SectorSurfaceRef surface,
        int& outFaceAnchorId,
        std::string* outStatus)
{
    outFaceAnchorId = -1;
    if (TargetForSurface(surface) == PreviewSurfaceHeightTarget::None) {
        if (outStatus != nullptr) {
            *outStatus = "Select a sector floor or ceiling before adjusting height";
        }
        return false;
    }

    SectorEditorAuthoringSurfaceTarget target;
    if (!ResolveSectorEditorAuthoringSurfaceTarget(
                topologyMap,
                authoringGraph,
                authoringDerivation,
                authoringDerivationCurrent,
                surface,
                target,
                outStatus)
            || target.kind != SectorEditorAuthoringSurfaceTargetKind::FaceAnchor
            || !IsValidSectorAuthoringId(target.faceAnchorId)) {
        if (outStatus != nullptr && outStatus->empty()) {
            *outStatus = "3D height adjustment unavailable: selected surface has no face anchor mapping";
        }
        return false;
    }

    outFaceAnchorId = target.faceAnchorId;
    return true;
}

} // namespace

bool IsSectorEditorPreviewSurfaceHeightAdjustable(
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringGraph& authoringGraph,
        const SectorAuthoringDerivationResult& authoringDerivation,
        bool authoringDerivationCurrent,
        SectorSurfaceRef surface)
{
    int faceAnchorId = -1;
    return ResolveFaceAnchorTarget(
            topologyMap,
            authoringGraph,
            authoringDerivation,
            authoringDerivationCurrent,
            surface,
            faceAnchorId,
            nullptr);
}

float SectorEditorPreviewSurfaceHeightStepAuthored(
        PreviewSurfaceHeightNudgePreset preset)
{
    switch (preset) {
        case PreviewSurfaceHeightNudgePreset::Fine: return 0.25f;
        case PreviewSurfaceHeightNudgePreset::Normal: return 1.0f;
        case PreviewSurfaceHeightNudgePreset::Coarse: return 5.0f;
    }
    return 1.0f;
}

SectorEditorSurfaceHeightEditingService::SectorEditorSurfaceHeightEditingService(
        SectorEditorSurfaceHeightEditingServiceContext context)
    : context_(std::move(context))
{
}

bool SectorEditorSurfaceHeightEditingService::BeginPreviewAdjustment()
{
    PreviewSurfaceHeightAdjustmentState& adjustment =
            context_.adjustmentState;
    if (adjustment.active) return false;

    int faceAnchorId = -1;
    std::string status;
    const SectorSurfaceRef surface =
            context_.previewSelectionState.selectedSurface3D;
    if (!ResolveFaceAnchorTarget(
                context_.topologyMap,
                context_.authoringGraph,
                context_.derivation.authoringDerivation,
                IsSectorEditorAuthoringDerivationCurrent(context_.derivation),
                surface,
                faceAnchorId,
                &status)) {
        context_.statusText = status.empty()
                ? "Select a sector floor or ceiling before adjusting height"
                : status;
        return false;
    }

    const SectorAuthoringFaceAnchor* anchor =
            FindSectorAuthoringFaceAnchor(context_.authoringGraph, faceAnchorId);
    if (anchor == nullptr) {
        context_.statusText =
                "3D height adjustment unavailable: face anchor is missing";
        return false;
    }

    const PreviewSurfaceHeightNudgePreset preset = adjustment.preset;
    adjustment = PreviewSurfaceHeightAdjustmentState{};
    adjustment.active = true;
    adjustment.faceAnchorId = faceAnchorId;
    adjustment.topologySectorId = surface.topologySectorId;
    adjustment.target = TargetForSurface(surface);
    adjustment.originalFloorZ = anchor->floorZ;
    adjustment.originalCeilingZ = anchor->ceilingZ;
    adjustment.preset = preset;
    adjustment.stagedGraph = context_.authoringGraph;
    adjustment.stagedDerivation = context_.derivation.authoringDerivation;
    context_.statusText = TextFormat(
            "Adjusting sector %d %s height",
            adjustment.topologySectorId,
            TargetName());
    return true;
}

bool SectorEditorSurfaceHeightEditingService::BuildPreviewNudge(
        float deltaHeightAuthored,
        PreviewSurfaceHeightNudgeCandidate& outCandidate)
{
    outCandidate = PreviewSurfaceHeightNudgeCandidate{};
    PreviewSurfaceHeightAdjustmentState& adjustment =
            context_.adjustmentState;
    if (!adjustment.active || !std::isfinite(deltaHeightAuthored)
            || deltaHeightAuthored == 0.0f) {
        return false;
    }

    SectorAuthoringGraph candidateGraph = adjustment.stagedGraph;
    SectorAuthoringFaceAnchor* anchor =
            FindSectorAuthoringFaceAnchor(candidateGraph, adjustment.faceAnchorId);
    if (anchor == nullptr) {
        context_.statusText =
                "Height adjustment cancelled: face anchor is missing";
        return false;
    }

    float* targetHeight = adjustment.target == PreviewSurfaceHeightTarget::Floor
            ? &anchor->floorZ
            : &anchor->ceilingZ;
    const float nextHeight = *targetHeight + deltaHeightAuthored;
    if (!std::isfinite(nextHeight)
            || nextHeight < MinimumAuthoredHeight
            || nextHeight > MaximumAuthoredHeight) {
        context_.statusText =
                "Height nudge blocked: authored height must stay between -512 and 512";
        return false;
    }
    if ((adjustment.target == PreviewSurfaceHeightTarget::Floor
                && nextHeight >= anchor->ceilingZ)
            || (adjustment.target == PreviewSurfaceHeightTarget::Ceiling
                && nextHeight <= anchor->floorZ)) {
        context_.statusText =
                "Height nudge blocked: ceiling must remain above floor";
        return false;
    }
    *targetHeight = nextHeight;

    SectorAuthoringDerivationResult candidateDerivation =
            DeriveSectorTopologyMapFromAuthoringGraph(candidateGraph);
    if (!candidateDerivation.success) {
        context_.statusText =
                "Height nudge failed: authoring derivation rejected the candidate";
        return false;
    }
    CopySectorEditorMapLevelFields(
            candidateDerivation.topology,
            context_.topologyMap);

    outCandidate.valid = true;
    outCandidate.changedFromOriginal =
            anchor->floorZ != adjustment.originalFloorZ
            || anchor->ceilingZ != adjustment.originalCeilingZ;
    outCandidate.graph = std::move(candidateGraph);
    outCandidate.derivation = std::move(candidateDerivation);
    outCandidate.height = nextHeight;
    return true;
}

void SectorEditorSurfaceHeightEditingService::AcceptPreviewNudge(
        PreviewSurfaceHeightNudgeCandidate candidate)
{
    if (!candidate.valid || !context_.adjustmentState.active) return;
    PreviewSurfaceHeightAdjustmentState& adjustment =
            context_.adjustmentState;
    adjustment.changed = candidate.changedFromOriginal;
    adjustment.stagedGraph = std::move(candidate.graph);
    adjustment.stagedDerivation = std::move(candidate.derivation);
    context_.statusText = TextFormat(
            "Adjusting sector %d %s: %.2f authored units",
            adjustment.topologySectorId,
            TargetName(),
            candidate.height);
}

PreviewSurfaceHeightAdjustmentResult
SectorEditorSurfaceHeightEditingService::ApplyPreviewAdjustment()
{
    PreviewSurfaceHeightAdjustmentResult result;
    PreviewSurfaceHeightAdjustmentState& adjustment =
            context_.adjustmentState;
    if (!adjustment.active) return result;

    if (!adjustment.changed) {
        context_.statusText = TextFormat(
                "Sector %d %s height unchanged",
                adjustment.topologySectorId,
                TargetName());
        ResetPreservingPreset();
        return result;
    }

    const int sectorId = adjustment.topologySectorId;
    const PreviewSurfaceHeightTarget target = adjustment.target;
    const bool committed = CommitSectorEditorAuthoringGraphCandidate(
            context_.state,
            context_.lifecycle,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            context_.selectionState,
            std::move(adjustment.stagedGraph),
            std::move(adjustment.stagedDerivation),
            context_.topologyMap,
            TextFormat("Adjusted sector %d %s height in 3D",
                    sectorId,
                    target == PreviewSurfaceHeightTarget::Floor
                            ? "floor"
                            : "ceiling"));
    ResetPreservingPreset();
    result.changed = committed;
    result.committed = committed;
    if (!committed) {
        context_.statusText = "Height adjustment failed during commit";
    }
    return result;
}

PreviewSurfaceHeightAdjustmentResult
SectorEditorSurfaceHeightEditingService::CancelPreviewAdjustment(
        const char* message)
{
    PreviewSurfaceHeightAdjustmentResult result;
    if (!context_.adjustmentState.active) return result;
    result.changed = context_.adjustmentState.changed;
    ResetPreservingPreset();
    context_.statusText = message != nullptr && message[0] != '\0'
            ? message
            : "Height adjustment cancelled";
    return result;
}

void SectorEditorSurfaceHeightEditingService::SetPreviewAdjustmentPreset(
        PreviewSurfaceHeightNudgePreset preset)
{
    context_.adjustmentState.preset = preset;
}

float SectorEditorSurfaceHeightEditingService::CurrentHeight() const
{
    const PreviewSurfaceHeightAdjustmentState& adjustment =
            context_.adjustmentState;
    const SectorAuthoringFaceAnchor* anchor = FindSectorAuthoringFaceAnchor(
            adjustment.stagedGraph,
            adjustment.faceAnchorId);
    if (anchor == nullptr) return 0.0f;
    return adjustment.target == PreviewSurfaceHeightTarget::Floor
            ? anchor->floorZ
            : anchor->ceilingZ;
}

const char* SectorEditorSurfaceHeightEditingService::TargetName() const
{
    return context_.adjustmentState.target == PreviewSurfaceHeightTarget::Floor
            ? "floor"
            : "ceiling";
}

void SectorEditorSurfaceHeightEditingService::ResetPreservingPreset()
{
    const PreviewSurfaceHeightNudgePreset preset =
            context_.adjustmentState.preset;
    context_.adjustmentState = PreviewSurfaceHeightAdjustmentState{};
    context_.adjustmentState.preset = preset;
}

} // namespace game
