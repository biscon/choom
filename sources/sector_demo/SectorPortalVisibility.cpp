#include "sector_demo/SectorPortalVisibility.h"

#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace game {
namespace {

bool SetError(std::string* outError, const std::string& message)
{
    if (outError != nullptr) {
        *outError = message;
    }
    return false;
}

std::string IdText(int id)
{
    return std::to_string(id);
}

RuntimeSectorNode* FindNode(RuntimeSectorVisibilityGraph& graph, int sectorId)
{
    for (RuntimeSectorNode& node : graph.sectors) {
        if (node.sectorId == sectorId) {
            return &node;
        }
    }
    return nullptr;
}

void SortUnique(std::vector<int>& values)
{
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

bool AppendDirectedPortal(
        RuntimeSectorVisibilityGraph& graph,
        const SectorTopologyLineDef& lineDef,
        const SectorTopologySideDef& fromSideDef,
        const SectorTopologySideDef& toSideDef,
        const SectorTopologySector& fromSector,
        const SectorTopologySector& toSector,
        Vector2 a,
        Vector2 b)
{
    RuntimeSectorNode* node = FindNode(graph, fromSector.id);
    if (node == nullptr) {
        return false;
    }

    const float openBottom = std::max(
            SectorAuthoringToWorldDistance(fromSector.floorZ),
            SectorAuthoringToWorldDistance(toSector.floorZ));
    const float openTop = std::min(
            SectorAuthoringToWorldDistance(fromSector.ceilingZ),
            SectorAuthoringToWorldDistance(toSector.ceilingZ));

    RuntimePortalEdge edge;
    edge.lineDefId = lineDef.id;
    edge.sideDefId = fromSideDef.id;
    edge.fromSectorId = fromSideDef.sectorId;
    edge.toSectorId = toSideDef.sectorId;
    edge.a = a;
    edge.b = b;
    edge.openBottom = openBottom;
    edge.openTop = openTop;
    edge.open = openBottom < openTop;

    node->outgoingPortalEdgeIndices.push_back(static_cast<int>(graph.portals.size()));
    graph.portals.push_back(edge);
    return true;
}

bool BuildLookupTables(
        const SectorTopologyMap& map,
        std::unordered_map<int, const SectorTopologyVertex*>& verticesById,
        std::unordered_map<int, const SectorTopologySideDef*>& sideDefsById,
        std::unordered_map<int, const SectorTopologySector*>& sectorsById,
        std::string* outError)
{
    for (const SectorTopologyVertex& vertex : map.vertices) {
        if (!IsValidSectorTopologyId(vertex.id)) {
            return SetError(outError, "visibility graph has invalid vertex id");
        }
        if (!verticesById.emplace(vertex.id, &vertex).second) {
            return SetError(outError, "visibility graph has duplicate vertex id " + IdText(vertex.id));
        }
    }

    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        if (!IsValidSectorTopologyId(sideDef.id)) {
            return SetError(outError, "visibility graph has invalid sidedef id");
        }
        if (!sideDefsById.emplace(sideDef.id, &sideDef).second) {
            return SetError(outError, "visibility graph has duplicate sidedef id " + IdText(sideDef.id));
        }
    }

    for (const SectorTopologySector& sector : map.sectors) {
        if (!IsValidSectorTopologyId(sector.id)) {
            return SetError(outError, "visibility graph has invalid sector id");
        }
        if (!sectorsById.emplace(sector.id, &sector).second) {
            return SetError(outError, "visibility graph has duplicate sector id " + IdText(sector.id));
        }
    }

    return true;
}

} // namespace

const RuntimeSectorNode* FindRuntimeSectorVisibilityNode(
        const RuntimeSectorVisibilityGraph& graph,
        int sectorId)
{
    for (const RuntimeSectorNode& node : graph.sectors) {
        if (node.sectorId == sectorId) {
            return &node;
        }
    }
    return nullptr;
}

bool BuildRuntimeSectorVisibilityGraph(
        const SectorTopologyMap& map,
        RuntimeSectorVisibilityGraph& outGraph,
        std::string* outError)
{
    outGraph = RuntimeSectorVisibilityGraph{};
    if (outError != nullptr) {
        outError->clear();
    }

    std::unordered_map<int, const SectorTopologyVertex*> verticesById;
    std::unordered_map<int, const SectorTopologySideDef*> sideDefsById;
    std::unordered_map<int, const SectorTopologySector*> sectorsById;
    if (!BuildLookupTables(map, verticesById, sideDefsById, sectorsById, outError)) {
        outGraph = RuntimeSectorVisibilityGraph{};
        return false;
    }

    outGraph.sectors.reserve(map.sectors.size());
    for (const SectorTopologySector& sector : map.sectors) {
        RuntimeSectorNode node;
        node.sectorId = sector.id;
        outGraph.sectors.push_back(node);
    }

    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (!IsValidSectorTopologyId(lineDef.id)) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph has invalid linedef id");
        }

        const auto startIt = verticesById.find(lineDef.startVertexId);
        const auto endIt = verticesById.find(lineDef.endVertexId);
        if (startIt == verticesById.end() || endIt == verticesById.end()) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " references a missing vertex");
        }

        const bool hasFront = IsValidSectorTopologyId(lineDef.frontSideDefId);
        const bool hasBack = IsValidSectorTopologyId(lineDef.backSideDefId);
        if (!hasFront && !hasBack) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " has no sidedefs");
        }
        if (!hasFront || !hasBack) {
            continue;
        }

        const auto frontIt = sideDefsById.find(lineDef.frontSideDefId);
        const auto backIt = sideDefsById.find(lineDef.backSideDefId);
        if (frontIt == sideDefsById.end() || backIt == sideDefsById.end()) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " references a missing sidedef");
        }

        const SectorTopologySideDef& frontSideDef = *frontIt->second;
        const SectorTopologySideDef& backSideDef = *backIt->second;
        if (frontSideDef.lineDefId != lineDef.id || backSideDef.lineDefId != lineDef.id) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " references a sidedef owned by another linedef");
        }

        const auto frontSectorIt = sectorsById.find(frontSideDef.sectorId);
        const auto backSectorIt = sectorsById.find(backSideDef.sectorId);
        if (frontSectorIt == sectorsById.end() || backSectorIt == sectorsById.end()) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph linedef " + IdText(lineDef.id)
                                      + " references a missing sector through a sidedef");
        }

        const Vector2 start = SectorCoordToWorldPosition2(startIt->second->x, startIt->second->y);
        const Vector2 end = SectorCoordToWorldPosition2(endIt->second->x, endIt->second->y);

        if (!AppendDirectedPortal(
                    outGraph,
                    lineDef,
                    frontSideDef,
                    backSideDef,
                    *frontSectorIt->second,
                    *backSectorIt->second,
                    start,
                    end)
                || !AppendDirectedPortal(
                    outGraph,
                    lineDef,
                    backSideDef,
                    frontSideDef,
                    *backSectorIt->second,
                    *frontSectorIt->second,
                    end,
                    start)) {
            outGraph = RuntimeSectorVisibilityGraph{};
            return SetError(outError, "visibility graph failed to attach portal for linedef "
                                      + IdText(lineDef.id));
        }
    }

    return true;
}

RuntimePortalVisibilityResult TraverseRuntimeSectorVisibility(
        const RuntimeSectorVisibilityGraph& graph,
        int startSectorId)
{
    RuntimePortalVisibilityResult result;
    result.startSectorId = startSectorId;

    if (FindRuntimeSectorVisibilityNode(graph, startSectorId) == nullptr) {
        result.validStartSector = false;
        result.fallbackDrawAll = true;
        result.status = "invalid start sector; fallback draw all";
        return result;
    }

    result.validStartSector = true;
    result.fallbackDrawAll = false;

    std::unordered_set<int> visited;
    std::deque<int> pending;
    visited.insert(startSectorId);
    pending.push_back(startSectorId);

    const size_t iterationCap = std::max<size_t>(graph.sectors.size() + graph.portals.size(), 1) * 4;
    size_t iterations = 0;
    bool hitIterationCap = false;

    while (!pending.empty()) {
        if (++iterations > iterationCap) {
            hitIterationCap = true;
            break;
        }

        const int sectorId = pending.front();
        pending.pop_front();
        const RuntimeSectorNode* node = FindRuntimeSectorVisibilityNode(graph, sectorId);
        if (node == nullptr) {
            continue;
        }

        for (const int edgeIndex : node->outgoingPortalEdgeIndices) {
            if (edgeIndex < 0 || static_cast<size_t>(edgeIndex) >= graph.portals.size()) {
                continue;
            }

            const RuntimePortalEdge& edge = graph.portals[static_cast<size_t>(edgeIndex)];
            if (!edge.open) {
                continue;
            }

            result.traversedPortalLineDefIds.push_back(edge.lineDefId);
            if (visited.insert(edge.toSectorId).second) {
                pending.push_back(edge.toSectorId);
            }
        }
    }

    result.visibleSectorIds.assign(visited.begin(), visited.end());
    SortUnique(result.visibleSectorIds);
    SortUnique(result.traversedPortalLineDefIds);

    if (hitIterationCap) {
        result.fallbackDrawAll = true;
        result.status = "visibility traversal hit iteration cap; fallback draw all";
    } else {
        result.status = "visibility traversal complete";
    }

    return result;
}

} // namespace game
