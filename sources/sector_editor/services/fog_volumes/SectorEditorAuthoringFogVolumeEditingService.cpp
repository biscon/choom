#include "sector_editor/services/fog_volumes/SectorEditorAuthoringFogVolumeEditingService.h"

#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace game {

std::array<Vector2, 8> BuildSectorEditorFogVolumeHandleMapPoints(
        SectorTopologyCoordPoint center, Vector2 radiiWorld, float yawDegrees)
{
    std::array<Vector2, 8> points{};
    const float cosine = std::cos(yawDegrees * DEG2RAD);
    const float sine = std::sin(yawDegrees * DEG2RAD);
    for (std::size_t i = 0; i < points.size(); ++i) {
        const float x = FogVolumeResizeHandleSigns[i].x * radiiWorld.x;
        const float z = FogVolumeResizeHandleSigns[i].y * radiiWorld.y;
        points[i] = {
                SectorCoordToVisibleAuthoring(center.x)
                        + SectorWorldToAuthoringDistance(cosine * x + sine * z),
                SectorCoordToVisibleAuthoring(center.y)
                        + SectorWorldToAuthoringDistance(-sine * x + cosine * z)};
    }
    return points;
}

bool SectorEditorAuthoringFogVolumeEditingService::FitToSector(int fogVolumeId)
{
    const auto* volume = FindSectorAuthoringFogVolume(context_.authoringGraph, fogVolumeId);
    int sectorId = -1;
    if (volume == nullptr || volume->shape != SectorLocalFogShape::Box
            || !CanResolvePoint({volume->x, volume->y}, &sectorId)) {
        context_.statusText = "Fit Sector requires a box fog volume inside a current non-void sector";
        return false;
    }
    const auto* sector = FindSectorTopologySector(context_.topologyMap, sectorId);
    if (sector == nullptr) {
        context_.statusText = "Fit Sector failed: derived sector is missing";
        return false;
    }
    const auto indexes = BuildSectorTopologyIndexes(context_.topologyMap);
    const auto sides = indexes.sideDefIndicesBySectorId.find(sectorId);
    if (sides == indexes.sideDefIndicesBySectorId.end()) {
        context_.statusText = "Fit Sector failed: derived sector boundaries are missing";
        return false;
    }
    SectorCoord minX = std::numeric_limits<SectorCoord>::max();
    SectorCoord minZ = minX;
    SectorCoord maxX = std::numeric_limits<SectorCoord>::min();
    SectorCoord maxZ = maxX;
    for (const auto sideIndex : sides->second) {
        const auto* line = FindSectorTopologyLineDef(context_.topologyMap,
                context_.topologyMap.sideDefs[sideIndex].lineDefId);
        if (line == nullptr) continue;
        for (const int vertexId : {line->startVertexId, line->endVertexId}) {
            const auto* vertex = FindSectorTopologyVertex(context_.topologyMap, vertexId);
            if (vertex == nullptr) continue;
            minX = std::min(minX, vertex->x); maxX = std::max(maxX, vertex->x);
            minZ = std::min(minZ, vertex->y); maxZ = std::max(maxZ, vertex->y);
        }
    }
    if (minX >= maxX || minZ >= maxZ) {
        context_.statusText = "Fit Sector failed: sector has no usable bounds";
        return false;
    }
    SectorAuthoringFogVolume fitted = *volume;
    fitted.x = static_cast<SectorCoord>(std::llround((double(minX) + maxX) * 0.5));
    fitted.y = static_cast<SectorCoord>(std::llround((double(minZ) + maxZ) * 0.5));
    // Point resolution can fall back to an enclosing face when the point is
    // on a nested face boundary. Fog compilation rejects all such boundaries.
    bool onBoundary = false;
    for (const auto& line : context_.topologyMap.lineDefs) {
        const auto* start = FindSectorTopologyVertex(context_.topologyMap, line.startVertexId);
        const auto* end = FindSectorTopologyVertex(context_.topologyMap, line.endVertexId);
        if (start != nullptr && end != nullptr
                && SectorTopologyPointOnSegment({fitted.x, fitted.y},
                        {start->x, start->y}, {end->x, end->y})) {
            onBoundary = true;
            break;
        }
    }
    int fittedSectorId = -1;
    if (onBoundary || !CanResolvePoint({fitted.x, fitted.y}, &fittedSectorId) || fittedSectorId != sectorId) {
        context_.statusText = "Fit Sector failed: bounds center is not strictly inside the original sector";
        return false;
    }
    fitted.yawDegrees = 0.0f;
    fitted.bottomOffsetWorld = 0.0f;
    fitted.radiusXWorld = SectorCoordDistanceToWorldDistance(
            std::max(double(fitted.x) - minX, double(maxX) - fitted.x));
    fitted.radiusZWorld = SectorCoordDistanceToWorldDistance(
            std::max(double(fitted.y) - minZ, double(maxZ) - fitted.y));
    fitted.heightWorld = SectorAuthoringToWorldDistance(sector->ceilingZ - sector->floorZ);
    const auto normalized = NormalizeSectorAuthoringFogVolume(fitted);
    if (normalized.radiusXWorld != fitted.radiusXWorld
            || normalized.radiusZWorld != fitted.radiusZWorld
            || normalized.heightWorld != fitted.heightWorld) {
        context_.statusText = "Fit Sector failed: bounds exceed supported fog dimensions (half extents 0.05-64 m, height 0.05-32 m)";
        return false;
    }
    return MutateById(fogVolumeId, "Fitted fog volume to sector",
            [&fitted](SectorAuthoringFogVolume& value) {
                if (value.x == fitted.x && value.y == fitted.y
                        && value.radiusXWorld == fitted.radiusXWorld
                        && value.radiusZWorld == fitted.radiusZWorld
                        && value.heightWorld == fitted.heightWorld
                        && value.bottomOffsetWorld == 0.0f && value.yawDegrees == 0.0f) return false;
                value = fitted;
                return true;
            });
}

bool SectorEditorAuthoringFogVolumeEditingService::BeginResize(
        int fogVolumeId, int handleIndex, SectorTopologyCoordPoint startPoint)
{
    const auto* volume = FindSectorAuthoringFogVolume(context_.authoringGraph, fogVolumeId);
    if (volume == nullptr || volume->shape != SectorLocalFogShape::Box
            || handleIndex < 0 || handleIndex >= static_cast<int>(FogVolumeResizeHandleSigns.size())
            || !BeginMove(fogVolumeId)) return false;
    auto& drag = context_.manipulationState.authoringFogVolumeDrag;
    drag.resizing = true;
    drag.resizeStartPoint = startPoint;
    drag.resizeSigns = FogVolumeResizeHandleSigns[static_cast<std::size_t>(handleIndex)];
    drag.originalRadii = {volume->radiusXWorld, volume->radiusZWorld};
    drag.previewRadii = drag.originalRadii;
    drag.yawDegrees = volume->yawDegrees;
    return true;
}

void SectorEditorAuthoringFogVolumeEditingService::UpdateResize(SectorTopologyCoordPoint point)
{
    auto& drag = context_.manipulationState.authoringFogVolumeDrag;
    if (!drag.active || !drag.resizing) return;
    const float dx = SectorCoordDistanceToWorldDistance(double(point.x) - drag.resizeStartPoint.x);
    const float dz = SectorCoordDistanceToWorldDistance(double(point.y) - drag.resizeStartPoint.y);
    const float cosine = std::cos(drag.yawDegrees * DEG2RAD);
    const float sine = std::sin(drag.yawDegrees * DEG2RAD);
    SectorAuthoringFogVolume candidate;
    candidate.radiusXWorld = drag.resizeSigns.x == 0 ? drag.originalRadii.x
            : drag.originalRadii.x + drag.resizeSigns.x * (cosine * dx - sine * dz);
    candidate.radiusZWorld = drag.resizeSigns.y == 0 ? drag.originalRadii.y
            : drag.originalRadii.y + drag.resizeSigns.y * (sine * dx + cosine * dz);
    candidate = NormalizeSectorAuthoringFogVolume(candidate);
    drag.previewRadii = {candidate.radiusXWorld, candidate.radiusZWorld};
}

bool SectorEditorAuthoringFogVolumeEditingService::FinishResize()
{
    const auto drag = context_.manipulationState.authoringFogVolumeDrag;
    if (!drag.active || !drag.resizing) return false;
    CancelMove();
    const auto* volume = FindSectorAuthoringFogVolume(context_.authoringGraph, drag.fogVolumeId);
    if (volume == nullptr || volume->shape != SectorLocalFogShape::Box
            || !IsSectorEditorAuthoringDerivationCurrent(context_.derivation)
            || volume->x != drag.originalPoint.x || volume->y != drag.originalPoint.y
            || volume->radiusXWorld != drag.originalRadii.x
            || volume->radiusZWorld != drag.originalRadii.y || volume->yawDegrees != drag.yawDegrees) {
        context_.statusText = "Fog volume resize cancelled: source changed";
        return false;
    }
    if (drag.previewRadii.x == drag.originalRadii.x && drag.previewRadii.y == drag.originalRadii.y) {
        context_.statusText = "Fog volume size unchanged";
        return true;
    }
    return MutateById(drag.fogVolumeId, "Resized fog volume", [&drag](SectorAuthoringFogVolume& value) {
        value.radiusXWorld = drag.previewRadii.x;
        value.radiusZWorld = drag.previewRadii.y;
        return true;
    });
}

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
    volume.instanceId = AllocateSectorAuthoringFogVolumeInstanceId(context_.authoringGraph, id);
    if (volume.instanceId.empty()) {
        context_.statusText = "Fog volume placement failed: no instance ID is available";
        return false;
    }
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

bool SectorEditorAuthoringFogVolumeEditingService::SetInstanceId(
        int fogVolumeId, const std::string& instanceId, std::string& outError)
{
    outError.clear();
    if (!IsValidSectorScriptInstanceId(instanceId)) {
        outError = "Instance ID must contain 1-63 letters, digits, underscores, or dashes";
        return false;
    }
    for (const auto& volume : context_.authoringGraph.fogVolumes) {
        if (volume.id != fogVolumeId && volume.instanceId == instanceId) {
            outError = "Instance ID must be unique among fog volumes in this map";
            return false;
        }
    }
    return MutateById(fogVolumeId, "Updated fog volume instance ID",
            [&instanceId](SectorAuthoringFogVolume& volume) {
                if (volume.instanceId == instanceId) return false;
                volume.instanceId = instanceId;
                return true;
            });
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
        const bool boxShape = volume.shape == SectorLocalFogShape::Box;
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
