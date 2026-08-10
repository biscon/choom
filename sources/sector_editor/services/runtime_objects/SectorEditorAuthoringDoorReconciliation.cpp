#include "sector_editor/services/runtime_objects/SectorEditorAuthoringDoorReconciliation.h"

#include "sector_demo/SectorTopologyGeometry.h"

#include <algorithm>
#include <utility>

namespace game {
namespace {

const SectorAuthoringDerivedLineMapping* FindLineMappingForTopologyLine(
        const SectorAuthoringDerivationResult& derivation,
        int topologyLineDefId)
{
    const SectorAuthoringDerivedLineMapping* found = nullptr;
    for (const SectorAuthoringDerivedLineMapping& mapping : derivation.mapping.lines) {
        if (mapping.topologyLineDefId != topologyLineDefId) {
            continue;
        }
        if (found != nullptr) {
            return nullptr;
        }
        found = &mapping;
    }
    return found;
}

bool TopologyLineEndpoints(
        const SectorTopologyMap& topology,
        int lineDefId,
        SectorTopologyCoordPoint& outA,
        SectorTopologyCoordPoint& outB)
{
    const SectorTopologyLineDef* line = FindSectorTopologyLineDef(topology, lineDefId);
    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (line == nullptr || !GetSectorTopologyLineVertices(topology, *line, start, end)) {
        return false;
    }
    outA = SectorTopologyCoordPoint{start->x, start->y};
    outB = SectorTopologyCoordPoint{end->x, end->y};
    return true;
}

bool SameUnorderedEndpoints(
        SectorTopologyCoordPoint firstA,
        SectorTopologyCoordPoint firstB,
        SectorTopologyCoordPoint secondA,
        SectorTopologyCoordPoint secondB)
{
    const auto same = [](SectorTopologyCoordPoint lhs, SectorTopologyCoordPoint rhs) {
        return lhs.x == rhs.x && lhs.y == rhs.y;
    };
    return (same(firstA, secondA) && same(firstB, secondB))
            || (same(firstA, secondB) && same(firstB, secondA));
}

} // namespace

bool ReconcileSectorEditorAuthoringCandidateDoors(
        const SectorTopologyMap& currentMap,
        const SectorAuthoringDerivationResult& currentDerivation,
        const SectorAuthoringDerivationResult& candidateDerivation,
        const std::set<int>& removedAuthoringLineIds,
        const std::set<int>& rejectDoorAuthoringLineIds,
        SectorTopologyMap& candidateMapData,
        std::vector<int>& outRemovedDoorIds,
        std::string& outError)
{
    outRemovedDoorIds.clear();
    outError.clear();
    std::vector<SectorPlacedRuntimeObject> reconciled;
    reconciled.reserve(candidateMapData.runtimeObjects.size());
    for (SectorPlacedRuntimeObject object : candidateMapData.runtimeObjects) {
        if (object.kind != "door") {
            reconciled.push_back(std::move(object));
            continue;
        }

        const SectorResolvedDoorAnchor currentResolved =
                ResolveSectorDoorAnchor(currentMap, object.door);
        if (!currentResolved.valid) {
            outError = TextFormat(
                    "Authoring edit unavailable: door %d already has an invalid portal anchor",
                    object.id);
            return false;
        }
        const SectorAuthoringDerivedLineMapping* currentLineMapping =
                FindLineMappingForTopologyLine(
                        currentDerivation,
                        object.door.anchor.lineDefId);
        if (currentLineMapping == nullptr) {
            outError = TextFormat(
                    "Authoring edit unavailable: door %d is not mapped to one authoring line",
                    object.id);
            return false;
        }
        const int authoringLineId = currentLineMapping->authoringLineId;
        if (rejectDoorAuthoringLineIds.find(authoringLineId)
                != rejectDoorAuthoringLineIds.end()) {
            outError = TextFormat(
                    "Cannot dissolve vertex: door %d is attached to an incident portal; remove the door first",
                    object.id);
            return false;
        }
        if (removedAuthoringLineIds.find(authoringLineId)
                != removedAuthoringLineIds.end()) {
            outRemovedDoorIds.push_back(object.id);
            continue;
        }

        SectorTopologyCoordPoint oldA{};
        SectorTopologyCoordPoint oldB{};
        if (!TopologyLineEndpoints(
                    currentMap,
                    object.door.anchor.lineDefId,
                    oldA,
                    oldB)) {
            outError = TextFormat(
                    "Authoring edit unavailable: door %d portal endpoints are invalid",
                    object.id);
            return false;
        }

        const SectorTopologyLineDef* candidateLine = nullptr;
        for (const SectorAuthoringDerivedLineMapping& mapping
                : candidateDerivation.mapping.lines) {
            if (mapping.authoringLineId != authoringLineId) {
                continue;
            }
            SectorTopologyCoordPoint candidateA{};
            SectorTopologyCoordPoint candidateB{};
            if (!TopologyLineEndpoints(
                        candidateDerivation.topology,
                        mapping.topologyLineDefId,
                        candidateA,
                        candidateB)
                    || !SameUnorderedEndpoints(oldA, oldB, candidateA, candidateB)) {
                continue;
            }
            if (candidateLine != nullptr) {
                outError = TextFormat(
                        "Authoring edit unavailable: door %d maps to multiple surviving portal fragments",
                        object.id);
                return false;
            }
            candidateLine = FindSectorTopologyLineDef(
                    candidateDerivation.topology,
                    mapping.topologyLineDefId);
        }

        if (candidateLine == nullptr) {
            outError = TextFormat(
                    "Authoring edit unavailable: door %d cannot be mapped to its surviving portal",
                    object.id);
            return false;
        }
        if (!IsValidSectorTopologyId(candidateLine->frontSideDefId)
                || !IsValidSectorTopologyId(candidateLine->backSideDefId)) {
            outError = TextFormat(
                    "Authoring edit unavailable: door %d surviving line is no longer a portal",
                    object.id);
            return false;
        }
        const SectorTopologySideDef* front = FindSectorTopologySideDef(
                candidateDerivation.topology,
                candidateLine->frontSideDefId);
        const SectorTopologySideDef* back = FindSectorTopologySideDef(
                candidateDerivation.topology,
                candidateLine->backSideDefId);
        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        if (front == nullptr || back == nullptr
                || !GetSectorTopologyLineVertices(
                        candidateDerivation.topology,
                        *candidateLine,
                        start,
                        end)) {
            outError = TextFormat(
                    "Authoring edit unavailable: door %d surviving portal is invalid",
                    object.id);
            return false;
        }

        object.door.anchor.lineDefId = candidateLine->id;
        object.door.anchor.frontSideDefId = front->id;
        object.door.anchor.backSideDefId = back->id;
        object.door.anchor.frontSectorId = front->sectorId;
        object.door.anchor.backSectorId = back->sectorId;
        object.door.anchor.endpointAX = start->x;
        object.door.anchor.endpointAY = start->y;
        object.door.anchor.endpointBX = end->x;
        object.door.anchor.endpointBY = end->y;

        const SectorResolvedDoorAnchor candidateResolved =
                ResolveSectorDoorAnchor(candidateDerivation.topology, object.door);
        if (!candidateResolved.valid) {
            outError = TextFormat(
                    "Authoring edit unavailable: door %d cannot be rebound to its surviving portal",
                    object.id);
            return false;
        }
        object.position = Vector3{
                SectorWorldToAuthoringDistance(candidateResolved.midpoint.x),
                SectorWorldToAuthoringDistance(candidateResolved.openBottom),
                SectorWorldToAuthoringDistance(candidateResolved.midpoint.y)};
        reconciled.push_back(std::move(object));
    }
    candidateMapData.runtimeObjects = std::move(reconciled);
    std::sort(outRemovedDoorIds.begin(), outRemovedDoorIds.end());
    return true;
}

} // namespace game
