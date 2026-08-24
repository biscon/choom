#include "sector_editor/services/reflection_probes/SectorEditorReflectionProbeEditingService.h"

#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace game {

SectorEditorReflectionProbeEditingService::SectorEditorReflectionProbeEditingService(
        SectorEditorReflectionProbeEditingServiceContext context)
    : context_(std::move(context))
{
}

SectorAuthoringReflectionProbe* SectorEditorReflectionProbeEditingService::Selected()
{
    if (context_.selectionState.selectedAuthoring.kind
            != SectorAuthoringSelectionKind::ReflectionProbe) return nullptr;
    return FindSectorAuthoringReflectionProbe(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.reflectionProbeId);
}

const SectorAuthoringReflectionProbe* SectorEditorReflectionProbeEditingService::Selected() const
{
    if (context_.selectionState.selectedAuthoring.kind
            != SectorAuthoringSelectionKind::ReflectionProbe) return nullptr;
    return FindSectorAuthoringReflectionProbe(
            context_.authoringGraph,
            context_.selectionState.selectedAuthoring.reflectionProbeId);
}

bool SectorEditorReflectionProbeEditingService::CanResolvePoint(
        SectorTopologyCoordPoint point,
        int* outSectorId) const
{
    return IsSectorEditorAuthoringDerivationCurrent(context_.derivation)
            && ResolveSectorAuthoringPointToDerivedSector(
                    context_.derivation.authoringDerivation, point, outSectorId);
}

bool SectorEditorReflectionProbeEditingService::CommitGraphMutation(
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

bool SectorEditorReflectionProbeEditingService::Place(
        SectorTopologyCoordPoint point,
        int* outId)
{
    int sectorId = -1;
    if (!CanResolvePoint(point, &sectorId)) {
        context_.statusText =
                "Reflection probe placement requires a point strictly inside a current non-void face";
        return false;
    }
    const int id = AllocateSectorAuthoringReflectionProbeId(context_.authoringGraph);
    const SectorTopologySector* sector = FindSectorTopologySector(context_.topologyMap, sectorId);
    if (!IsValidSectorAuthoringId(id) || sector == nullptr) {
        context_.statusText = "Reflection probe placement failed";
        return false;
    }
    const float floorWorld = SectorAuthoringToWorldDistance(sector->floorZ);
    const float ceilingWorld = SectorAuthoringToWorldDistance(sector->ceilingZ);
    SectorAuthoringReflectionProbe probe;
    probe.id = id;
    probe.x = point.x;
    probe.z = point.y;
    probe.yWorld = (floorWorld + ceilingWorld) * 0.5f;
    probe.halfExtentsWorld.y = std::max(0.1f, (ceilingWorld - floorWorld) * 0.5f);
    context_.authoringGraph.reflectionProbes.push_back(probe);
    SelectSectorEditorAuthoringReflectionProbe(
            context_.authoringGraph, context_.selectionState, id);
    if (outId != nullptr) *outId = id;
    const bool committed = CommitGraphMutation(
            "Placed reflection probe", "Reflection probe placed; derivation failed");
    if (committed) FitToSector(id);
    return committed;
}

bool SectorEditorReflectionProbeEditingService::MutateById(
        int probeId,
        const char* status,
        const std::function<bool(SectorAuthoringReflectionProbe&)>& mutate)
{
    SectorAuthoringReflectionProbe* probe = FindSectorAuthoringReflectionProbe(
            context_.authoringGraph, probeId);
    if (probe == nullptr || !mutate || !mutate(*probe)) return false;
    *probe = NormalizeSectorAuthoringReflectionProbe(*probe);
    return CommitGraphMutation(status, "Reflection probe edit saved; derivation failed");
}

bool SectorEditorReflectionProbeEditingService::SetPosition(
        int probeId,
        SectorTopologyCoordPoint point,
        const char* status)
{
    if (!CanResolvePoint(point)) {
        context_.statusText = "Reflection probe must remain inside a non-void face";
        return false;
    }
    return MutateById(probeId, status, [point](SectorAuthoringReflectionProbe& probe) {
        if (probe.x == point.x && probe.z == point.y) return false;
        probe.x = point.x;
        probe.z = point.y;
        return true;
    });
}

bool SectorEditorReflectionProbeEditingService::FitToSector(int probeId)
{
    SectorAuthoringReflectionProbe* probe = FindSectorAuthoringReflectionProbe(
            context_.authoringGraph, probeId);
    if (probe == nullptr) return false;
    int sectorId = -1;
    if (!CanResolvePoint({probe->x, probe->z}, &sectorId)) return false;
    const SectorTopologySector* sector = FindSectorTopologySector(context_.topologyMap, sectorId);
    if (sector == nullptr) return false;
    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(context_.topologyMap);
    const auto sidesIt = indexes.sideDefIndicesBySectorId.find(sectorId);
    if (sidesIt == indexes.sideDefIndicesBySectorId.end()) return false;
    float minX = std::numeric_limits<float>::max();
    float maxX = -std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = -std::numeric_limits<float>::max();
    for (std::size_t sideIndex : sidesIt->second) {
        const SectorTopologySideDef& side = context_.topologyMap.sideDefs[sideIndex];
        const SectorTopologyLineDef* line = FindSectorTopologyLineDef(
                context_.topologyMap, side.lineDefId);
        if (line == nullptr) continue;
        for (int vertexId : {line->startVertexId, line->endVertexId}) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(
                    context_.topologyMap, vertexId);
            if (vertex == nullptr) continue;
            const float x = SectorAuthoringToWorldDistance(
                    SectorCoordToVisibleAuthoring(vertex->x));
            const float z = SectorAuthoringToWorldDistance(
                    SectorCoordToVisibleAuthoring(vertex->y));
            minX = std::min(minX, x); maxX = std::max(maxX, x);
            minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
        }
    }
    if (!(minX <= maxX && minZ <= maxZ)) return false;
    const float captureX = SectorAuthoringToWorldDistance(
            SectorCoordToVisibleAuthoring(probe->x));
    const float captureZ = SectorAuthoringToWorldDistance(
            SectorCoordToVisibleAuthoring(probe->z));
    const float floorWorld = SectorAuthoringToWorldDistance(sector->floorZ);
    const float ceilingWorld = SectorAuthoringToWorldDistance(sector->ceilingZ);
    return MutateById(probeId, "Fitted reflection probe to sector",
            [&](SectorAuthoringReflectionProbe& value) {
                value.yawDegrees = 0.0f;
                value.influenceOffsetWorld = {
                        (minX + maxX) * 0.5f - captureX,
                        (floorWorld + ceilingWorld) * 0.5f - value.yWorld,
                        (minZ + maxZ) * 0.5f - captureZ};
                value.halfExtentsWorld = {
                        std::max(0.1f, (maxX - minX) * 0.5f),
                        std::max(0.1f, (ceilingWorld - floorWorld) * 0.5f),
                        std::max(0.1f, (maxZ - minZ) * 0.5f)};
                return true;
            });
}

bool SectorEditorReflectionProbeEditingService::DeleteSelected()
{
    const SectorAuthoringReflectionProbe* selected = Selected();
    if (selected == nullptr) return false;
    const int id = selected->id;
    auto& probes = context_.authoringGraph.reflectionProbes;
    probes.erase(std::remove_if(probes.begin(), probes.end(),
            [id](const SectorAuthoringReflectionProbe& probe) { return probe.id == id; }),
            probes.end());
    ClearSectorEditorAuthoringSelection(context_.selectionState);
    return CommitGraphMutation(
            "Deleted reflection probe", "Reflection probe deleted; derivation failed");
}

bool SectorEditorReflectionProbeEditingService::BeginMove(int probeId)
{
    const SectorAuthoringReflectionProbe* probe = FindSectorAuthoringReflectionProbe(
            context_.authoringGraph, probeId);
    if (probe == nullptr) return false;
    auto& drag = context_.manipulationState.authoringReflectionProbeDrag;
    drag = {};
    drag.active = true;
    drag.reflectionProbeId = probeId;
    drag.originalPoint = {probe->x, probe->z};
    drag.previewPoint = drag.originalPoint;
    drag.hasPreviewPoint = true;
    drag.previewResolved = true;
    return true;
}

void SectorEditorReflectionProbeEditingService::UpdateMove(SectorTopologyCoordPoint point)
{
    auto& drag = context_.manipulationState.authoringReflectionProbeDrag;
    if (!drag.active) return;
    drag.previewPoint = point;
    drag.hasPreviewPoint = true;
    drag.previewResolved = CanResolvePoint(point);
    drag.errorMessage = drag.previewResolved ? std::string{}
            : "Reflection probe must remain inside a non-void face";
}

bool SectorEditorReflectionProbeEditingService::FinishMove()
{
    const AuthoringReflectionProbeDragState drag =
            context_.manipulationState.authoringReflectionProbeDrag;
    if (!drag.active || !drag.hasPreviewPoint || !drag.previewResolved) {
        context_.statusText = drag.errorMessage.empty()
                ? "Reflection probe move rejected" : drag.errorMessage;
        return false;
    }
    context_.manipulationState.authoringReflectionProbeDrag = {};
    if (drag.previewPoint.x == drag.originalPoint.x
            && drag.previewPoint.y == drag.originalPoint.y) return true;
    return SetPosition(drag.reflectionProbeId, drag.previewPoint, "Moved reflection probe");
}

void SectorEditorReflectionProbeEditingService::CancelMove(const char* message)
{
    context_.manipulationState.authoringReflectionProbeDrag = {};
    if (message != nullptr && message[0] != '\0') context_.statusText = message;
}

int SectorEditorReflectionProbeEditingService::FindAtMapPoint(
        Vector2 mapPoint,
        float toleranceMap) const
{
    int bestId = -1;
    float bestDistance2 = toleranceMap * toleranceMap;
    for (const SectorAuthoringReflectionProbe& probe
            : context_.authoringGraph.reflectionProbes) {
        const float dx = mapPoint.x - SectorCoordToVisibleAuthoring(probe.x);
        const float dz = mapPoint.y - SectorCoordToVisibleAuthoring(probe.z);
        const float distance2 = dx * dx + dz * dz;
        if (distance2 <= bestDistance2) {
            bestDistance2 = distance2;
            bestId = probe.id;
        }
    }
    return bestId;
}

} // namespace game
