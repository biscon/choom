#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorManipulationState.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <array>
#include <functional>
#include <string>

namespace game {

// Corners first, then edge midpoints; also used for drawing and hit testing.
constexpr std::array<Vector2, 8> FogVolumeResizeHandleSigns{{
        {-1, -1}, {1, -1}, {1, 1}, {-1, 1},
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
std::array<Vector2, 8> BuildSectorEditorFogVolumeHandleMapPoints(
        SectorTopologyCoordPoint center, Vector2 radiiWorld, float yawDegrees);

struct SectorEditorAuthoringFogVolumeEditingServiceContext {
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    uint64_t& topologyRenderRevision;
    SectorEditorTopologyRenderCache& topologyRenderCache;
    SelectionState& selectionState;
    ManipulationState& manipulationState;
    std::string& statusText;
};

class SectorEditorAuthoringFogVolumeEditingService {
public:
    explicit SectorEditorAuthoringFogVolumeEditingService(
            SectorEditorAuthoringFogVolumeEditingServiceContext context);

    SectorAuthoringFogVolume* Selected();
    const SectorAuthoringFogVolume* Selected() const;
    bool Place(SectorTopologyCoordPoint point, int* outId = nullptr);
    bool MutateById(
            int fogVolumeId,
            const char* status,
            const std::function<bool(SectorAuthoringFogVolume&)>& mutate);
    bool SetInstanceId(int fogVolumeId, const std::string& instanceId, std::string& outError);
    bool SetPosition(int fogVolumeId, SectorTopologyCoordPoint point, const char* status);
    bool DeleteSelected();
    bool FitToSector(int fogVolumeId);

    bool BeginResize(int fogVolumeId, int handleIndex, SectorTopologyCoordPoint startPoint);
    void UpdateResize(SectorTopologyCoordPoint point);
    bool FinishResize();

    bool BeginMove(int fogVolumeId);
    void UpdateMove(SectorTopologyCoordPoint point);
    bool FinishMove();
    // Shared cancellation path for move and resize previews.
    void CancelMove(const char* message = nullptr);

    bool IsResolved(int fogVolumeId, int* outTopologySectorId = nullptr) const;
    int FindAtMapPoint(Vector2 mapPoint, float extraToleranceMap) const;

private:
    bool CanResolvePoint(SectorTopologyCoordPoint point, int* outTopologySectorId = nullptr) const;
    bool CommitGraphMutation(const char* successStatus, const char* failureStatus);

    SectorEditorAuthoringFogVolumeEditingServiceContext context_;
};

} // namespace game
