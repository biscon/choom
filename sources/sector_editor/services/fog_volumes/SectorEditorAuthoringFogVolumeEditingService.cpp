#include "sector_editor/services/fog_volumes/SectorEditorAuthoringFogVolumeEditingService.h"

#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace game {

SectorEditorAuthoringFogVolumeEditingService::SectorEditorAuthoringFogVolumeEditingService(
        SectorEditorAuthoringFogVolumeEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorAuthoringFogVolume* SectorEditorAuthoringFogVolumeEditingService::Selected()
{
    if (context_.selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::FogVolume) {
        return nullptr;
    }
    return FindSectorAuthoringFogVolume(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.fogVolumeId);
}

const SectorAuthoringFogVolume* SectorEditorAuthoringFogVolumeEditingService::Selected() const
{
    if (context_.selectionState.selectedAuthoring.kind != SectorAuthoringSelectionKind::FogVolume) {
        return nullptr;
    }
    return FindSectorAuthoringFogVolume(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.fogVolumeId);
}

bool SectorEditorAuthoringFogVolumeEditingService::CanResolvePoint(
        SectorTopologyCoordPoint point,
        int* outTopologySectorId) const
{
    if (!IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        return false;
    }
    return ResolveSectorAuthoringPointToDerivedSector(
            context_.derivation.authoringDerivation,
            point,
            outTopologySectorId);
}

bool SectorEditorAuthoringFogVolumeEditingService::CommitGraphMutation(
        const char* successStatus,
        const char* failureStatus)
{
    MarkSectorEditorAuthoringGraphEdited(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.derivation,
            successStatus);
    const bool refreshed = RefreshSectorEditorAuthoringDerivation(
            context_.lifecycle,
            context_.topologyRenderRevision,
            context_.topologyRenderCache,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            successStatus,
            failureStatus);
    context_.statusText = context_.derivation.authoringDerivationStatus;
    return refreshed;
}

bool SectorEditorAuthoringFogVolumeEditingService::Place(
        SectorTopologyCoordPoint point,
        int* outId)
{
    if (!CanResolvePoint(point)) {
        context_.statusText = "Fog volume placement requires a point strictly inside a current non-void derived face";
        return false;
    }
    const int id = AllocateSectorAuthoringFogVolumeId(context_.authoringGraph);
    if (!IsValidSectorAuthoringId(id)) {
        context_.statusText = "Fog volume placement failed: no authoring ID is available";
        return false;
    }
    SectorAuthoringFogVolume volume;
    volume.id = id;
    volume.x = point.x;
    volume.y = point.y;
    context_.authoringGraph.fogVolumes.push_back(volume);
    SelectSectorEditorAuthoringFogVolume(context_.authoringGraph, context_.selectionState, id);
    if (outId != nullptr) {
        *outId = id;
    }
    return CommitGraphMutation("Placed authoring fog volume", "Fog volume placed; derivation failed");
}

bool SectorEditorAuthoringFogVolumeEditingService::MutateById(
        int fogVolumeId,
        const char* status,
        const std::function<bool(SectorAuthoringFogVolume&)>& mutate)
{
    SectorAuthoringFogVolume* volume = FindSectorAuthoringFogVolume(context_.authoringGraph, fogVolumeId);
    if (volume == nullptr || !mutate || !mutate(*volume)) {
        return false;
    }
    *volume = NormalizeSectorAuthoringFogVolume(*volume);
    return CommitGraphMutation(status, "Fog volume edit saved; derivation failed");
}

bool SectorEditorAuthoringFogVolumeEditingService::SetPosition(
        int fogVolumeId,
        SectorTopologyCoordPoint point,
        const char* status)
{
    if (!CanResolvePoint(point)) {
        context_.statusText = "Fog volume position must be strictly inside a current non-void derived face";
        return false;
    }
    return MutateById(fogVolumeId, status, [point](SectorAuthoringFogVolume& volume) {
        if (volume.x == point.x && volume.y == point.y) {
            return false;
        }
        volume.x = point.x;
        volume.y = point.y;
        return true;
    });
}

bool SectorEditorAuthoringFogVolumeEditingService::DeleteSelected()
{
    const SectorAuthoringFogVolume* selected = Selected();
    if (selected == nullptr) {
        return false;
    }
    const int id = selected->id;
    const auto oldSize = context_.authoringGraph.fogVolumes.size();
    context_.authoringGraph.fogVolumes.erase(
            std::remove_if(
                    context_.authoringGraph.fogVolumes.begin(),
                    context_.authoringGraph.fogVolumes.end(),
                    [id](const SectorAuthoringFogVolume& volume) { return volume.id == id; }),
            context_.authoringGraph.fogVolumes.end());
    if (context_.authoringGraph.fogVolumes.size() == oldSize) {
        return false;
    }
    ClearSectorEditorAuthoringSelection(context_.selectionState);
    return CommitGraphMutation("Deleted authoring fog volume", "Fog volume deleted; derivation failed");
}

bool SectorEditorAuthoringFogVolumeEditingService::BeginMove(int fogVolumeId)
{
    const SectorAuthoringFogVolume* volume = FindSectorAuthoringFogVolume(context_.authoringGraph, fogVolumeId);
    if (volume == nullptr || !IsSectorEditorAuthoringDerivationCurrent(context_.derivation)) {
        context_.statusText = "Fog volume move requires current authoring derivation";
        return false;
    }
    AuthoringFogVolumeDragState& drag = context_.manipulationState.authoringFogVolumeDrag;
    drag = AuthoringFogVolumeDragState{};
    drag.active = true;
    drag.fogVolumeId = fogVolumeId;
    drag.originalPoint = SectorTopologyCoordPoint{volume->x, volume->y};
    drag.previewPoint = drag.originalPoint;
    drag.hasPreviewPoint = true;
    drag.previewResolved = CanResolvePoint(drag.previewPoint);
    return true;
}

void SectorEditorAuthoringFogVolumeEditingService::UpdateMove(SectorTopologyCoordPoint point)
{
    AuthoringFogVolumeDragState& drag = context_.manipulationState.authoringFogVolumeDrag;
    if (!drag.active) {
        return;
    }
    drag.previewPoint = point;
    drag.hasPreviewPoint = true;
    drag.previewResolved = CanResolvePoint(point);
    drag.errorMessage = drag.previewResolved
            ? std::string{}
            : "Fog volume center must be strictly inside a non-void face";
}

bool SectorEditorAuthoringFogVolumeEditingService::FinishMove()
{
    const AuthoringFogVolumeDragState drag = context_.manipulationState.authoringFogVolumeDrag;
    if (!drag.active || !drag.hasPreviewPoint || !drag.previewResolved) {
        context_.statusText = drag.errorMessage.empty() ? "Fog volume move rejected" : drag.errorMessage;
        return false;
    }
    context_.manipulationState.authoringFogVolumeDrag = AuthoringFogVolumeDragState{};
    if (drag.previewPoint.x == drag.originalPoint.x && drag.previewPoint.y == drag.originalPoint.y) {
        context_.statusText = "Fog volume move unchanged";
        return true;
    }
    return SetPosition(drag.fogVolumeId, drag.previewPoint, "Moved authoring fog volume");
}

void SectorEditorAuthoringFogVolumeEditingService::CancelMove(const char* message)
{
    context_.manipulationState.authoringFogVolumeDrag = AuthoringFogVolumeDragState{};
    if (message != nullptr && message[0] != '\0') {
        context_.statusText = message;
    }
}

bool SectorEditorAuthoringFogVolumeEditingService::IsResolved(
        int fogVolumeId,
        int* outTopologySectorId) const
{
    for (const SectorAuthoringDerivedFogVolumeMapping& mapping
            : context_.derivation.authoringDerivation.mapping.fogVolumes) {
        if (mapping.authoringFogVolumeId == fogVolumeId && mapping.resolved) {
            if (outTopologySectorId != nullptr) {
                *outTopologySectorId = mapping.topologySectorId;
            }
            return true;
        }
    }
    return false;
}

int SectorEditorAuthoringFogVolumeEditingService::FindAtMapPoint(
        Vector2 mapPoint,
        float extraToleranceMap) const
{
    int bestId = -1;
    float bestDistance2 = std::numeric_limits<float>::max();
    for (const SectorAuthoringFogVolume& volume : context_.authoringGraph.fogVolumes) {
        const Vector2 center{
                SectorCoordToVisibleAuthoring(volume.x),
                SectorCoordToVisibleAuthoring(volume.y)};
        const float radiusX = SectorWorldToAuthoringDistance(volume.radiusXWorld) + extraToleranceMap;
        const float radiusY = SectorWorldToAuthoringDistance(volume.radiusZWorld) + extraToleranceMap;
        const float dx = mapPoint.x - center.x;
        const float dy = mapPoint.y - center.y;
        const bool boxShape = volume.renderMode == SectorLocalFogRenderMode::Analytic
                && volume.shape == SectorLocalFogShape::Box;
        bool contains = false;
        if (boxShape) {
            const float yaw = volume.yawDegrees * DEG2RAD;
            const float cosine = std::cos(yaw);
            const float sine = std::sin(yaw);
            const float localX = cosine * dx - sine * dy;
            const float localZ = sine * dx + cosine * dy;
            contains = radiusX > 0.0f && radiusY > 0.0f
                    && std::fabs(localX) <= radiusX
                    && std::fabs(localZ) <= radiusY;
        } else {
            contains = radiusX > 0.0f && radiusY > 0.0f
                    && dx * dx / (radiusX * radiusX)
                                    + dy * dy / (radiusY * radiusY)
                            <= 1.0f;
        }
        if (!contains) {
            continue;
        }
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2) {
            bestDistance2 = distance2;
            bestId = volume.id;
        }
    }
    return bestId;
}

} // namespace game
