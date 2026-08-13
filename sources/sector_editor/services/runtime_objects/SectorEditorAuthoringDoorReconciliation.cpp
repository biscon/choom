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

const SectorTopologyLineDef* FindUniqueCandidatePortal(
        const SectorAuthoringDerivationResult& candidateDerivation,
        SectorTopologyCoordPoint oldA,
        SectorTopologyCoordPoint oldB,
        int authoringLineId,
        bool requireSameEndpoints)
{
    const SectorTopologyLineDef* found = nullptr;
    for (const SectorAuthoringDerivedLineMapping& mapping
            : candidateDerivation.mapping.lines) {
        if (IsValidSectorAuthoringId(authoringLineId)
                && mapping.authoringLineId != authoringLineId) {
            continue;
        }
        SectorTopologyCoordPoint candidateA{};
        SectorTopologyCoordPoint candidateB{};
        if (!TopologyLineEndpoints(
                    candidateDerivation.topology,
                    mapping.topologyLineDefId,
                    candidateA,
                    candidateB)
                || (requireSameEndpoints
                        && !SameUnorderedEndpoints(
                                oldA,
                                oldB,
                                candidateA,
                                candidateB))) {
            continue;
        }
        const SectorTopologyLineDef* line = FindSectorTopologyLineDef(
                candidateDerivation.topology,
                mapping.topologyLineDefId);
        if (line == nullptr
                || !IsValidSectorTopologyId(line->frontSideDefId)
                || !IsValidSectorTopologyId(line->backSideDefId)) {
            continue;
        }
        if (found != nullptr) {
            return nullptr;
        }
        found = line;
    }
    return found;
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
        const SectorAuthoringDerivedLineMapping* currentLineMapping =
                currentResolved.valid
                ? FindLineMappingForTopologyLine(
                        currentDerivation,
                        object.door.anchor.lineDefId)
                : nullptr;
        const int authoringLineId = currentLineMapping == nullptr
                ? -1
                : currentLineMapping->authoringLineId;
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

        SectorTopologyCoordPoint oldA{
                object.door.anchor.endpointAX,
                object.door.anchor.endpointAY};
        SectorTopologyCoordPoint oldB{
                object.door.anchor.endpointBX,
                object.door.anchor.endpointBY};
        if (currentResolved.valid
                && !TopologyLineEndpoints(
                        currentMap,
                        object.door.anchor.lineDefId,
                        oldA,
                        oldB)) {
            outError = TextFormat(
                    "Authoring edit unavailable: door %d portal endpoints are invalid",
                    object.id);
            return false;
        }

        const SectorTopologyLineDef* candidateLine = FindUniqueCandidatePortal(
                candidateDerivation,
                oldA,
                oldB,
                authoringLineId,
                true);
        if (candidateLine == nullptr) {
            candidateLine = FindUniqueCandidatePortal(
                    candidateDerivation,
                    oldA,
                    oldB,
                    -1,
                    true);
        }
        if (candidateLine == nullptr && IsValidSectorAuthoringId(authoringLineId)) {
            candidateLine = FindUniqueCandidatePortal(
                    candidateDerivation,
                    oldA,
                    oldB,
                    authoringLineId,
                    false);
        }

        if (candidateLine == nullptr) {
            if (!currentResolved.valid) {
                reconciled.push_back(std::move(object));
                continue;
            }
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

bool ValidateSectorEditorAuthoringCandidateDoorPortalSpans(
        const SectorTopologyMap& currentMap,
        const SectorAuthoringDerivationResult& currentDerivation,
        const SectorAuthoringGraph& candidateGraph,
        std::string& outError)
{
    outError.clear();
    for (const SectorPlacedRuntimeObject& object : currentMap.runtimeObjects) {
        if (object.kind != "door") {
            continue;
        }

        const SectorResolvedDoorAnchor resolved =
                ResolveSectorDoorAnchor(currentMap, object.door);
        if (!resolved.valid) {
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

        const SectorAuthoringDerivedLineMapping* currentLineMapping =
                FindLineMappingForTopologyLine(
                        currentDerivation,
                        object.door.anchor.lineDefId);
        bool spanSurvives = false;
        for (const SectorAuthoringLine& line : candidateGraph.lines) {
            if (currentLineMapping != nullptr
                    && line.id != currentLineMapping->authoringLineId) {
                continue;
            }
            const SectorAuthoringVertex* start = FindSectorAuthoringVertex(
                    candidateGraph,
                    line.startVertexId);
            const SectorAuthoringVertex* end = FindSectorAuthoringVertex(
                    candidateGraph,
                    line.endVertexId);
            if (start == nullptr || end == nullptr) {
                continue;
            }
            const SectorTopologyCoordPoint lineA{start->x, start->y};
            const SectorTopologyCoordPoint lineB{end->x, end->y};
            if (SectorTopologyPointOnSegment(oldA, lineA, lineB)
                    && SectorTopologyPointOnSegment(oldB, lineA, lineB)) {
                spanSurvives = true;
                break;
            }
        }
        if (!spanSurvives && currentLineMapping != nullptr) {
            for (const SectorAuthoringLine& line : candidateGraph.lines) {
                const SectorAuthoringVertex* start = FindSectorAuthoringVertex(
                        candidateGraph,
                        line.startVertexId);
                const SectorAuthoringVertex* end = FindSectorAuthoringVertex(
                        candidateGraph,
                        line.endVertexId);
                if (start == nullptr || end == nullptr) {
                    continue;
                }
                const SectorTopologyCoordPoint lineA{start->x, start->y};
                const SectorTopologyCoordPoint lineB{end->x, end->y};
                if (SectorTopologyPointOnSegment(oldA, lineA, lineB)
                        && SectorTopologyPointOnSegment(oldB, lineA, lineB)) {
                    spanSurvives = true;
                    break;
                }
            }
        }
        if (!spanSurvives) {
            outError = TextFormat(
                    "Cannot split portal containing door %d; remove the door first",
                    object.id);
            return false;
        }
    }
    return true;
}

} // namespace game
