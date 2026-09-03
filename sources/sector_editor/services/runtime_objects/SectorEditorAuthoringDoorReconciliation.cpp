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
        const bool isDoor = object.kind == "door";
        const bool isWindow = object.kind == "window";
        const bool isDuctAccess = object.kind == "duct_access";
        if (!isDoor && !isWindow && !isDuctAccess) {
            reconciled.push_back(std::move(object));
            continue;
        }

        const char* objectName = isWindow
                ? "window" : (isDuctAccess ? "duct access" : "door");
        const SectorDoorAnchor& currentAnchor = isWindow
                ? object.window.anchor
                : (isDuctAccess
                        ? object.ductAccess.anchor
                        : object.door.anchor);

        const SectorResolvedDoorAnchor currentResolved =
                isWindow
                ? ResolveSectorWindowAnchor(currentMap, object.window)
                : (isDuctAccess
                        ? static_cast<const SectorResolvedDoorAnchor&>(
                                ResolveSectorDuctAccessAnchor(
                                        currentMap, object.ductAccess))
                        : ResolveSectorDoorAnchor(currentMap, object.door));
        const SectorAuthoringDerivedLineMapping* currentLineMapping =
                currentResolved.valid
                ? FindLineMappingForTopologyLine(
                        currentDerivation,
                        currentAnchor.lineDefId)
                : nullptr;
        const int authoringLineId = currentLineMapping == nullptr
                ? -1
                : currentLineMapping->authoringLineId;
        if (rejectDoorAuthoringLineIds.find(authoringLineId)
                != rejectDoorAuthoringLineIds.end()) {
            outError = isDuctAccess
                    ? TextFormat(
                            "Cannot dissolve vertex: Duct Access %d is attached to an incident portal; remove it first",
                            object.id)
                    : isWindow
                            ? TextFormat(
                                    "Cannot dissolve vertex: window %d is attached to an incident portal; remove the window first",
                                    object.id)
                            : TextFormat(
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
                currentAnchor.endpointAX,
                currentAnchor.endpointAY};
        SectorTopologyCoordPoint oldB{
                currentAnchor.endpointBX,
                currentAnchor.endpointBY};
        if (currentResolved.valid
                && !TopologyLineEndpoints(
                        currentMap,
                        currentAnchor.lineDefId,
                        oldA,
                        oldB)) {
            outError = TextFormat(
                    "Authoring edit unavailable: %s %d portal endpoints are invalid",
                    objectName, object.id);
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
                    "Authoring edit unavailable: %s %d cannot be mapped to its surviving portal",
                    objectName, object.id);
            return false;
        }
        if (!IsValidSectorTopologyId(candidateLine->frontSideDefId)
                || !IsValidSectorTopologyId(candidateLine->backSideDefId)) {
            outError = TextFormat(
                    "Authoring edit unavailable: %s %d surviving line is no longer a portal",
                    objectName, object.id);
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
                    "Authoring edit unavailable: %s %d surviving portal is invalid",
                    objectName, object.id);
            return false;
        }

        SectorDoorAnchor& candidateAnchor = isWindow
                ? object.window.anchor
                : (isDuctAccess
                        ? object.ductAccess.anchor
                        : object.door.anchor);
        candidateAnchor.lineDefId = candidateLine->id;
        candidateAnchor.frontSideDefId = front->id;
        candidateAnchor.backSideDefId = back->id;
        candidateAnchor.frontSectorId = front->sectorId;
        candidateAnchor.backSectorId = back->sectorId;
        candidateAnchor.endpointAX = start->x;
        candidateAnchor.endpointAY = start->y;
        candidateAnchor.endpointBX = end->x;
        candidateAnchor.endpointBY = end->y;

        const SectorResolvedDoorAnchor candidateResolved =
                isWindow
                ? ResolveSectorWindowAnchor(
                        candidateDerivation.topology, object.window)
                : (isDuctAccess
                        ? static_cast<const SectorResolvedDoorAnchor&>(
                                ResolveSectorDuctAccessAnchor(
                                        candidateDerivation.topology,
                                        object.ductAccess))
                        : ResolveSectorDoorAnchor(
                                candidateDerivation.topology, object.door));
        if (!candidateResolved.valid) {
            outError = TextFormat(
                    "Authoring edit unavailable: %s %d cannot be rebound to its surviving portal",
                    objectName, object.id);
            return false;
        }
        object.position = Vector3{
                SectorWorldToAuthoringDistance(candidateResolved.midpoint.x),
                SectorWorldToAuthoringDistance(isWindow
                        ? candidateResolved.openBottom
                                + candidateResolved.portalHeight * 0.5f
                                + object.window.verticalOffsetWorld
                        : isDuctAccess
                                ? candidateResolved.openBottom
                                        + candidateResolved.portalHeight * 0.5f
                                        + object.ductAccess.verticalOffsetWorld
                        : candidateResolved.openBottom),
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
        const bool isDoor = object.kind == "door";
        const bool isWindow = object.kind == "window";
        const bool isDuctAccess = object.kind == "duct_access";
        if (!isDoor && !isWindow && !isDuctAccess) {
            continue;
        }

        const char* objectName = isWindow
                ? "window" : (isDuctAccess ? "duct access" : "door");
        const SectorDoorAnchor& anchor = isWindow
                ? object.window.anchor
                : (isDuctAccess
                        ? object.ductAccess.anchor
                        : object.door.anchor);

        const SectorResolvedDoorAnchor resolved =
                isWindow
                ? ResolveSectorWindowAnchor(currentMap, object.window)
                : (isDuctAccess
                        ? static_cast<const SectorResolvedDoorAnchor&>(
                                ResolveSectorDuctAccessAnchor(
                                        currentMap, object.ductAccess))
                        : ResolveSectorDoorAnchor(currentMap, object.door));
        if (!resolved.valid) {
            continue;
        }

        SectorTopologyCoordPoint oldA{};
        SectorTopologyCoordPoint oldB{};
        if (!TopologyLineEndpoints(
                    currentMap,
                    anchor.lineDefId,
                    oldA,
                    oldB)) {
            outError = TextFormat(
                    "Authoring edit unavailable: %s %d portal endpoints are invalid",
                    objectName, object.id);
            return false;
        }

        const SectorAuthoringDerivedLineMapping* currentLineMapping =
                FindLineMappingForTopologyLine(
                        currentDerivation,
                        anchor.lineDefId);
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
            outError = isDuctAccess
                    ? TextFormat(
                            "Cannot split portal containing Duct Access %d; remove it first",
                            object.id)
                    : isWindow
                            ? TextFormat(
                                    "Cannot split portal containing window %d; remove the window first",
                                    object.id)
                            : TextFormat(
                                    "Cannot split portal containing door %d; remove the door first",
                                    object.id);
            return false;
        }
    }
    return true;
}

} // namespace game
