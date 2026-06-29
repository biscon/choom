#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace game {
namespace {

struct TopologyPoint {
    SectorCoord x = 0;
    SectorCoord y = 0;
};

enum class SegmentIntersectionKind {
    None,
    Touch,
    Proper,
    CollinearOverlap
};

enum class PointContainment {
    Outside,
    Inside,
    Boundary
};

void AddIssue(
        std::vector<SectorTopologyValidationIssue>* issues,
        SectorTopologyObjectKind objectKind,
        int objectId,
        std::string message,
        SectorTopologyValidationSeverity severity = SectorTopologyValidationSeverity::Error)
{
    if (issues == nullptr) {
        return;
    }
    issues->push_back(SectorTopologyValidationIssue{
            severity,
            objectKind,
            objectId,
            std::move(message)
    });
}

template<typename T>
const T* FindUnique(
        const std::vector<T>& values,
        const std::unordered_map<int, std::vector<size_t>>& indicesById,
        int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return nullptr;
    }
    const auto found = indicesById.find(id);
    if (found == indicesById.end() || found->second.size() != 1) {
        return nullptr;
    }
    return &values[found->second.front()];
}

const SectorTopologyVertex* FindUniqueVertex(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int id)
{
    return FindUnique(map.vertices, indexes.vertexIndicesById, id);
}

const SectorTopologyLineDef* FindUniqueLineDef(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int id)
{
    return FindUnique(map.lineDefs, indexes.lineDefIndicesById, id);
}

const SectorTopologySideDef* FindUniqueSideDef(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int id)
{
    return FindUnique(map.sideDefs, indexes.sideDefIndicesById, id);
}

const SectorTopologySector* FindUniqueSector(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int id)
{
    return FindUnique(map.sectors, indexes.sectorIndicesById, id);
}

TopologyPoint Point(const SectorTopologyVertex& vertex)
{
    return TopologyPoint{vertex.x, vertex.y};
}

__int128 Cross(TopologyPoint a, TopologyPoint b, TopologyPoint c)
{
    const int64_t abX = static_cast<int64_t>(b.x) - static_cast<int64_t>(a.x);
    const int64_t abY = static_cast<int64_t>(b.y) - static_cast<int64_t>(a.y);
    const int64_t acX = static_cast<int64_t>(c.x) - static_cast<int64_t>(a.x);
    const int64_t acY = static_cast<int64_t>(c.y) - static_cast<int64_t>(a.y);
    return static_cast<__int128>(abX) * static_cast<__int128>(acY)
           - static_cast<__int128>(abY) * static_cast<__int128>(acX);
}

int Sign(__int128 value)
{
    return (value > 0) - (value < 0);
}

bool OnSegment(TopologyPoint a, TopologyPoint b, TopologyPoint point)
{
    return Cross(a, b, point) == 0
           && point.x >= std::min(a.x, b.x)
           && point.x <= std::max(a.x, b.x)
           && point.y >= std::min(a.y, b.y)
           && point.y <= std::max(a.y, b.y);
}

bool RangesOverlapWithLength(SectorCoord a, SectorCoord b, SectorCoord c, SectorCoord d)
{
    return std::max(std::min(a, b), std::min(c, d))
           < std::min(std::max(a, b), std::max(c, d));
}

SegmentIntersectionKind SegmentIntersection(
        TopologyPoint a,
        TopologyPoint b,
        TopologyPoint c,
        TopologyPoint d)
{
    const int abC = Sign(Cross(a, b, c));
    const int abD = Sign(Cross(a, b, d));
    const int cdA = Sign(Cross(c, d, a));
    const int cdB = Sign(Cross(c, d, b));

    if (abC == 0 && abD == 0 && cdA == 0 && cdB == 0) {
        const bool hasPositiveOverlap = RangesOverlapWithLength(a.x, b.x, c.x, d.x)
                                        || RangesOverlapWithLength(a.y, b.y, c.y, d.y);
        if (hasPositiveOverlap) {
            return SegmentIntersectionKind::CollinearOverlap;
        }
        if (OnSegment(a, b, c) || OnSegment(a, b, d)
            || OnSegment(c, d, a) || OnSegment(c, d, b)) {
            return SegmentIntersectionKind::Touch;
        }
        return SegmentIntersectionKind::None;
    }

    if (abC * abD < 0 && cdA * cdB < 0) {
        return SegmentIntersectionKind::Proper;
    }
    if ((abC == 0 && OnSegment(a, b, c))
        || (abD == 0 && OnSegment(a, b, d))
        || (cdA == 0 && OnSegment(c, d, a))
        || (cdB == 0 && OnSegment(c, d, b))) {
        return SegmentIntersectionKind::Touch;
    }
    return SegmentIntersectionKind::None;
}

bool LinesShareEndpointId(
        const SectorTopologyLineDef& first,
        const SectorTopologyLineDef& second)
{
    return first.startVertexId == second.startVertexId
           || first.startVertexId == second.endVertexId
           || first.endVertexId == second.startVertexId
           || first.endVertexId == second.endVertexId;
}

bool IsValidSideKind(SectorTopologySideKind side)
{
    switch (side) {
        case SectorTopologySideKind::Front:
        case SectorTopologySideKind::Back:
            return true;
    }
    return false;
}

PointContainment ClassifyPointInLoop(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        const SectorTopologyLoop& loop,
        TopologyPoint point)
{
    bool inside = false;
    for (size_t i = 0; i < loop.vertexIds.size(); ++i) {
        const SectorTopologyVertex* start = FindUniqueVertex(map, indexes, loop.vertexIds[i]);
        const SectorTopologyVertex* end = FindUniqueVertex(
                map,
                indexes,
                loop.vertexIds[(i + 1) % loop.vertexIds.size()]);
        if (start == nullptr || end == nullptr) {
            return PointContainment::Outside;
        }

        const TopologyPoint a = Point(*start);
        const TopologyPoint b = Point(*end);
        if (OnSegment(a, b, point)) {
            return PointContainment::Boundary;
        }

        if ((a.y > point.y) != (b.y > point.y)) {
            const __int128 orientation = Cross(a, b, point);
            const bool crossesToRight = b.y > a.y ? orientation > 0 : orientation < 0;
            if (crossesToRight) {
                inside = !inside;
            }
        }
    }
    return inside ? PointContainment::Inside : PointContainment::Outside;
}

bool CalculateLoopArea(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        SectorTopologyLoop& loop)
{
    __int128 area = 0;
    for (size_t i = 0; i < loop.vertexIds.size(); ++i) {
        const SectorTopologyVertex* start = FindUniqueVertex(map, indexes, loop.vertexIds[i]);
        const SectorTopologyVertex* end = FindUniqueVertex(
                map,
                indexes,
                loop.vertexIds[(i + 1) % loop.vertexIds.size()]);
        if (start == nullptr || end == nullptr) {
            return false;
        }
        area += static_cast<__int128>(start->x) * static_cast<__int128>(end->y)
                - static_cast<__int128>(end->x) * static_cast<__int128>(start->y);
    }
    if (area < std::numeric_limits<int64_t>::min()
        || area > std::numeric_limits<int64_t>::max()) {
        return false;
    }
    loop.signedAreaTwice = static_cast<int64_t>(area);
    return true;
}

int MinimumId(const std::vector<int>& ids)
{
    return ids.empty() ? -1 : *std::min_element(ids.begin(), ids.end());
}

bool CanExtractSector(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int sectorId)
{
    if (FindUniqueSector(map, indexes, sectorId) == nullptr) {
        return false;
    }
    const auto foundSides = indexes.sideDefIndicesBySectorId.find(sectorId);
    if (foundSides == indexes.sideDefIndicesBySectorId.end()) {
        return true;
    }
    for (size_t sideIndex : foundSides->second) {
        const SectorTopologySideDef& sideDef = map.sideDefs[sideIndex];
        if (!IsValidSideKind(sideDef.side)
            || FindUniqueSideDef(map, indexes, sideDef.id) == nullptr) {
            return false;
        }
        const SectorTopologyLineDef* lineDef = FindUniqueLineDef(map, indexes, sideDef.lineDefId);
        if (lineDef == nullptr
            || FindUniqueVertex(map, indexes, lineDef->startVertexId) == nullptr
            || FindUniqueVertex(map, indexes, lineDef->endVertexId) == nullptr) {
            return false;
        }
        const int slotId = sideDef.side == SectorTopologySideKind::Front
                           ? lineDef->frontSideDefId
                           : lineDef->backSideDefId;
        if (slotId != sideDef.id) {
            return false;
        }
    }
    return true;
}

template<typename T>
void ValidateIds(
        const std::vector<T>& values,
        SectorTopologyObjectKind kind,
        std::vector<SectorTopologyValidationIssue>& issues)
{
    std::unordered_set<int> seen;
    for (const T& value : values) {
        if (!IsValidSectorTopologyId(value.id)) {
            AddIssue(&issues, kind, value.id, "ID must be positive");
        } else if (!seen.insert(value.id).second) {
            AddIssue(&issues, kind, value.id, "has a duplicate ID");
        }
    }
}

} // namespace

bool ExtractSectorTopologyLoops(
        const SectorTopologyMap& map,
        int sectorId,
        SectorTopologyLoopSet& outLoops,
        std::vector<SectorTopologyValidationIssue>* outIssues)
{
    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(map);
    return ExtractSectorTopologyLoops(map, indexes, sectorId, outLoops, outIssues);
}

bool ExtractSectorTopologyLoops(
        const SectorTopologyMap& map,
        const SectorTopologyIndexes& indexes,
        int sectorId,
        SectorTopologyLoopSet& outLoops,
        std::vector<SectorTopologyValidationIssue>* outIssues)
{
    outLoops = {};
    if (!IsValidSectorTopologyId(sectorId)
        || FindUniqueSector(map, indexes, sectorId) == nullptr) {
        AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                 "does not uniquely identify an existing sector");
        return false;
    }

    const auto foundSideIndices = indexes.sideDefIndicesBySectorId.find(sectorId);
    if (foundSideIndices == indexes.sideDefIndicesBySectorId.end()
        || foundSideIndices->second.empty()) {
        AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                 "has no sidedefs");
        return false;
    }

    std::vector<SectorTopologyLoopEdge> edges;
    edges.reserve(foundSideIndices->second.size());
    for (size_t sideIndex : foundSideIndices->second) {
        const SectorTopologySideDef& sideDef = map.sideDefs[sideIndex];
        if (FindUniqueSideDef(map, indexes, sideDef.id) == nullptr) {
            AddIssue(outIssues, SectorTopologyObjectKind::SideDef, sideDef.id,
                     "does not have a unique positive ID");
            return false;
        }
        if (!IsValidSideKind(sideDef.side)) {
            AddIssue(outIssues, SectorTopologyObjectKind::SideDef, sideDef.id,
                     "has an invalid side kind");
            return false;
        }

        const SectorTopologyLineDef* lineDef = FindUniqueLineDef(map, indexes, sideDef.lineDefId);
        if (lineDef == nullptr) {
            AddIssue(outIssues, SectorTopologyObjectKind::SideDef, sideDef.id,
                     "references a missing or duplicate linedef " + std::to_string(sideDef.lineDefId));
            return false;
        }
        const SectorTopologyVertex* lineStart = FindUniqueVertex(
                map, indexes, lineDef->startVertexId);
        const SectorTopologyVertex* lineEnd = FindUniqueVertex(
                map, indexes, lineDef->endVertexId);
        if (lineStart == nullptr || lineEnd == nullptr) {
            AddIssue(outIssues, SectorTopologyObjectKind::LineDef, lineDef->id,
                     "cannot form a boundary because an endpoint is missing or duplicated");
            return false;
        }
        if (lineStart->x == lineEnd->x && lineStart->y == lineEnd->y) {
            AddIssue(outIssues, SectorTopologyObjectKind::LineDef, lineDef->id,
                     "cannot form a boundary because its endpoints have identical coordinates");
            return false;
        }

        const int expectedSlot = sideDef.side == SectorTopologySideKind::Front
                                 ? lineDef->frontSideDefId
                                 : lineDef->backSideDefId;
        if (expectedSlot != sideDef.id) {
            AddIssue(outIssues, SectorTopologyObjectKind::SideDef, sideDef.id,
                     "is not referenced by its linedef's matching slot");
            return false;
        }

        SectorTopologyLoopEdge edge;
        edge.sideDefId = sideDef.id;
        edge.lineDefId = lineDef->id;
        edge.side = sideDef.side;
        edge.startVertexId = sideDef.side == SectorTopologySideKind::Front
                             ? lineDef->startVertexId : lineDef->endVertexId;
        edge.endVertexId = sideDef.side == SectorTopologySideKind::Front
                           ? lineDef->endVertexId : lineDef->startVertexId;
        if (edge.startVertexId == edge.endVertexId) {
            AddIssue(outIssues, SectorTopologyObjectKind::LineDef, lineDef->id,
                     "has identical endpoint IDs");
            return false;
        }
        edges.push_back(edge);
    }

    std::sort(edges.begin(), edges.end(), [](const auto& first, const auto& second) {
        return std::tie(first.startVertexId, first.endVertexId, first.sideDefId, first.lineDefId)
               < std::tie(second.startVertexId, second.endVertexId, second.sideDefId, second.lineDefId);
    });

    std::unordered_map<int, std::vector<size_t>> outgoing;
    std::unordered_map<int, std::vector<size_t>> incoming;
    for (size_t i = 0; i < edges.size(); ++i) {
        outgoing[edges[i].startVertexId].push_back(i);
        incoming[edges[i].endVertexId].push_back(i);
    }

    std::unordered_set<int> boundaryVertices;
    for (const auto& edge : edges) {
        boundaryVertices.insert(edge.startVertexId);
        boundaryVertices.insert(edge.endVertexId);
    }
    for (int vertexId : boundaryVertices) {
        const size_t outgoingCount = outgoing[vertexId].size();
        const size_t incomingCount = incoming[vertexId].size();
        if (outgoingCount == 0 || incomingCount == 0) {
            AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                     "has an open boundary loop at vertex " + std::to_string(vertexId));
            return false;
        }
        if (outgoingCount != 1 || incomingCount != 1) {
            AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                     "has a branching or touching boundary at vertex " + std::to_string(vertexId));
            return false;
        }
    }

    std::vector<bool> used(edges.size(), false);
    std::vector<SectorTopologyLoop> loops;
    for (size_t startEdgeIndex = 0; startEdgeIndex < edges.size(); ++startEdgeIndex) {
        if (used[startEdgeIndex]) {
            continue;
        }

        SectorTopologyLoop loop;
        size_t currentEdgeIndex = startEdgeIndex;
        for (size_t guard = 0; guard <= edges.size(); ++guard) {
            if (used[currentEdgeIndex]) {
                if (currentEdgeIndex != startEdgeIndex) {
                    AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                             "reuses a boundary edge before closing a loop");
                    return false;
                }
                break;
            }

            used[currentEdgeIndex] = true;
            const SectorTopologyLoopEdge& edge = edges[currentEdgeIndex];
            loop.vertexIds.push_back(edge.startVertexId);
            loop.sideDefIds.push_back(edge.sideDefId);
            loop.edges.push_back(edge);

            const auto next = outgoing.find(edge.endVertexId);
            if (next == outgoing.end() || next->second.size() != 1) {
                AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                         "has an open boundary loop");
                return false;
            }
            currentEdgeIndex = next->second.front();
        }

        if (loop.vertexIds.size() < 3) {
            AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                     "has a boundary loop with fewer than three edges");
            return false;
        }
        if (!CalculateLoopArea(map, indexes, loop)) {
            AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                     "has a boundary loop whose signed area exceeds the supported range");
            return false;
        }
        if (loop.signedAreaTwice == 0) {
            AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                     "has a zero-area boundary loop");
            return false;
        }
        loops.push_back(std::move(loop));
    }

    for (const SectorTopologyLoop& loop : loops) {
        for (size_t i = 0; i < loop.edges.size(); ++i) {
            for (size_t j = i + 1; j < loop.edges.size(); ++j) {
                const bool adjacent = j == i + 1 || (i == 0 && j + 1 == loop.edges.size());
                const auto& first = loop.edges[i];
                const auto& second = loop.edges[j];
                const auto* a = FindUniqueVertex(map, indexes, first.startVertexId);
                const auto* b = FindUniqueVertex(map, indexes, first.endVertexId);
                const auto* c = FindUniqueVertex(map, indexes, second.startVertexId);
                const auto* d = FindUniqueVertex(map, indexes, second.endVertexId);
                const SegmentIntersectionKind intersection = SegmentIntersection(
                        Point(*a), Point(*b), Point(*c), Point(*d));
                if ((adjacent && intersection != SegmentIntersectionKind::Touch)
                    || (!adjacent && intersection != SegmentIntersectionKind::None)) {
                    AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                             "has a self-intersecting boundary loop");
                    return false;
                }
            }
        }
    }

    for (size_t i = 0; i < loops.size(); ++i) {
        for (size_t j = i + 1; j < loops.size(); ++j) {
            for (const auto& first : loops[i].edges) {
                for (const auto& second : loops[j].edges) {
                    const auto* a = FindUniqueVertex(map, indexes, first.startVertexId);
                    const auto* b = FindUniqueVertex(map, indexes, first.endVertexId);
                    const auto* c = FindUniqueVertex(map, indexes, second.startVertexId);
                    const auto* d = FindUniqueVertex(map, indexes, second.endVertexId);
                    if (SegmentIntersection(Point(*a), Point(*b), Point(*c), Point(*d))
                        != SegmentIntersectionKind::None) {
                        AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                                 "has boundary loops that intersect or touch");
                        return false;
                    }
                }
            }
        }
    }

    size_t positiveLoopCount = 0;
    for (const auto& loop : loops) {
        if (loop.signedAreaTwice > 0) {
            ++positiveLoopCount;
        }
    }
    if (positiveLoopCount != 1) {
        AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                 positiveLoopCount == 0
                 ? "has only clockwise hole loops and no CCW outer loop"
                 : "has multiple CCW outer loops");
        return false;
    }

    for (auto& loop : loops) {
        if (loop.signedAreaTwice > 0) {
            outLoops.outer = std::move(loop);
        } else {
            outLoops.holes.push_back(std::move(loop));
        }
    }

    for (const auto& hole : outLoops.holes) {
        const SectorTopologyVertex* representative = FindUniqueVertex(
                map, indexes, hole.vertexIds.front());
        if (ClassifyPointInLoop(map, indexes, outLoops.outer, Point(*representative))
            != PointContainment::Inside) {
            AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                     "has a hole outside its outer loop");
            outLoops = {};
            return false;
        }
    }

    for (size_t i = 0; i < outLoops.holes.size(); ++i) {
        const auto* firstPoint = FindUniqueVertex(
                map, indexes, outLoops.holes[i].vertexIds.front());
        for (size_t j = i + 1; j < outLoops.holes.size(); ++j) {
            const auto* secondPoint = FindUniqueVertex(
                    map, indexes, outLoops.holes[j].vertexIds.front());
            if (ClassifyPointInLoop(map, indexes, outLoops.holes[j], Point(*firstPoint))
                    != PointContainment::Outside
                || ClassifyPointInLoop(map, indexes, outLoops.holes[i], Point(*secondPoint))
                    != PointContainment::Outside) {
                AddIssue(outIssues, SectorTopologyObjectKind::Sector, sectorId,
                         "has nested holes");
                outLoops = {};
                return false;
            }
        }
    }

    std::sort(outLoops.holes.begin(), outLoops.holes.end(), [](const auto& first, const auto& second) {
        return std::make_pair(MinimumId(first.vertexIds), MinimumId(first.sideDefIds))
               < std::make_pair(MinimumId(second.vertexIds), MinimumId(second.sideDefIds));
    });
    return true;
}

std::vector<SectorTopologyValidationIssue> ValidateSectorTopologyMap(
        const SectorTopologyMap& map)
{
    std::vector<SectorTopologyValidationIssue> issues;
    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(map);

    ValidateIds(map.vertices, SectorTopologyObjectKind::Vertex, issues);
    ValidateIds(map.lineDefs, SectorTopologyObjectKind::LineDef, issues);
    ValidateIds(map.sideDefs, SectorTopologyObjectKind::SideDef, issues);
    ValidateIds(map.sectors, SectorTopologyObjectKind::Sector, issues);
    ValidateIds(map.staticLights, SectorTopologyObjectKind::StaticLight, issues);
    ValidateIds(map.dynamicPointLights, SectorTopologyObjectKind::DynamicLight, issues);

    for (const SectorTopologyStaticPointLight& light : map.staticLights) {
        if (!std::isfinite(light.position.x)
                || !std::isfinite(light.position.y)
                || !std::isfinite(light.position.z)) {
            AddIssue(&issues, SectorTopologyObjectKind::StaticLight, light.id,
                     "position values must be finite");
        }
        if (!std::isfinite(light.intensity)) {
            AddIssue(&issues, SectorTopologyObjectKind::StaticLight, light.id,
                     "intensity must be finite");
        }
        if (!std::isfinite(light.radius) || light.radius <= 0.0f) {
            AddIssue(&issues, SectorTopologyObjectKind::StaticLight, light.id,
                     "radius must be finite and positive");
        }
        if (!std::isfinite(light.sourceRadius) || light.sourceRadius < 0.0f) {
            AddIssue(&issues, SectorTopologyObjectKind::StaticLight, light.id,
                     "source radius must be finite and non-negative");
        } else if (std::isfinite(light.radius) && light.sourceRadius > light.radius) {
            AddIssue(&issues, SectorTopologyObjectKind::StaticLight, light.id,
                     "source radius must not exceed radius");
        }
    }

    for (const SectorTopologyDynamicPointLight& light : map.dynamicPointLights) {
        if (!std::isfinite(light.position.x)
                || !std::isfinite(light.position.y)
                || !std::isfinite(light.position.z)) {
            AddIssue(&issues, SectorTopologyObjectKind::DynamicLight, light.id,
                     "position values must be finite");
        }
        if (!std::isfinite(light.intensity)) {
            AddIssue(&issues, SectorTopologyObjectKind::DynamicLight, light.id,
                     "intensity must be finite");
        }
        if (!std::isfinite(light.radius) || light.radius <= 0.0f) {
            AddIssue(&issues, SectorTopologyObjectKind::DynamicLight, light.id,
                     "radius must be finite and positive");
        }
    }

    std::unordered_set<uint64_t> endpointPairs;
    for (size_t lineIndex = 0; lineIndex < map.lineDefs.size(); ++lineIndex) {
        const SectorTopologyLineDef& lineDef = map.lineDefs[lineIndex];
        const SectorTopologyVertex* start = FindUniqueVertex(map, indexes, lineDef.startVertexId);
        const SectorTopologyVertex* end = FindUniqueVertex(map, indexes, lineDef.endVertexId);

        if (start == nullptr) {
            AddIssue(&issues, SectorTopologyObjectKind::LineDef, lineDef.id,
                     "references missing or duplicate start vertex " + std::to_string(lineDef.startVertexId));
        }
        if (end == nullptr) {
            AddIssue(&issues, SectorTopologyObjectKind::LineDef, lineDef.id,
                     "references missing or duplicate end vertex " + std::to_string(lineDef.endVertexId));
        }
        if (lineDef.startVertexId == lineDef.endVertexId) {
            AddIssue(&issues, SectorTopologyObjectKind::LineDef, lineDef.id,
                     "uses the same start and end vertex ID");
        } else if (start != nullptr && end != nullptr
                   && start->x == end->x && start->y == end->y) {
            AddIssue(&issues, SectorTopologyObjectKind::LineDef, lineDef.id,
                     "has endpoints with identical coordinates");
        }

        if (start != nullptr && end != nullptr
            && lineDef.startVertexId != lineDef.endVertexId
            && (start->x != end->x || start->y != end->y)) {
            const uint32_t low = static_cast<uint32_t>(
                    std::min(lineDef.startVertexId, lineDef.endVertexId));
            const uint32_t high = static_cast<uint32_t>(
                    std::max(lineDef.startVertexId, lineDef.endVertexId));
            const uint64_t pairKey = (static_cast<uint64_t>(low) << 32U) | high;
            if (!endpointPairs.insert(pairKey).second) {
                AddIssue(&issues, SectorTopologyObjectKind::LineDef, lineDef.id,
                         "duplicates an existing physical linedef endpoint pair");
            }
        }

        const auto validateSlot = [&](int sideDefId, SectorTopologySideKind expectedSide, const char* slotName) {
            if (sideDefId == -1) {
                return;
            }
            const SectorTopologySideDef* sideDef = FindUniqueSideDef(map, indexes, sideDefId);
            if (sideDef == nullptr) {
                AddIssue(&issues, SectorTopologyObjectKind::LineDef, lineDef.id,
                         std::string("references missing or duplicate ") + slotName
                         + " sidedef " + std::to_string(sideDefId));
            } else if (sideDef->lineDefId != lineDef.id || sideDef->side != expectedSide) {
                AddIssue(&issues, SectorTopologyObjectKind::LineDef, lineDef.id,
                         std::string("has an inconsistent ") + slotName + " sidedef slot");
            }
        };
        validateSlot(lineDef.frontSideDefId, SectorTopologySideKind::Front, "front");
        validateSlot(lineDef.backSideDefId, SectorTopologySideKind::Back, "back");
        if (lineDef.frontSideDefId == -1 && lineDef.backSideDefId == -1) {
            AddIssue(&issues, SectorTopologyObjectKind::LineDef, lineDef.id,
                     "has no sidedefs");
        }
    }

    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        const SectorTopologyLineDef* lineDef = FindUniqueLineDef(map, indexes, sideDef.lineDefId);
        if (lineDef == nullptr) {
            AddIssue(&issues, SectorTopologyObjectKind::SideDef, sideDef.id,
                     "references missing or duplicate linedef " + std::to_string(sideDef.lineDefId));
        }
        if (FindUniqueSector(map, indexes, sideDef.sectorId) == nullptr) {
            AddIssue(&issues, SectorTopologyObjectKind::SideDef, sideDef.id,
                     "references missing or duplicate sector " + std::to_string(sideDef.sectorId));
        }
        if (!IsValidSideKind(sideDef.side)) {
            AddIssue(&issues, SectorTopologyObjectKind::SideDef, sideDef.id,
                     "has an invalid side kind");
        } else if (lineDef != nullptr) {
            const int slotId = sideDef.side == SectorTopologySideKind::Front
                               ? lineDef->frontSideDefId : lineDef->backSideDefId;
            if (slotId != sideDef.id) {
                AddIssue(&issues, SectorTopologyObjectKind::SideDef, sideDef.id,
                         "is not referenced by its linedef's matching slot");
            }
        }
    }

    const auto validateSlotCount = [&](const auto& byLineId, const char* slotName) {
        for (const auto& entry : byLineId) {
            if (entry.second.size() > 1) {
                AddIssue(&issues, SectorTopologyObjectKind::LineDef, entry.first,
                         std::string("has multiple sidedefs claiming its ") + slotName + " slot");
            }
        }
    };
    validateSlotCount(indexes.frontSideDefIndicesByLineDefId, "front");
    validateSlotCount(indexes.backSideDefIndicesByLineDefId, "back");

    for (size_t i = 0; i < map.lineDefs.size(); ++i) {
        const SectorTopologyLineDef& first = map.lineDefs[i];
        const SectorTopologyVertex* a = FindUniqueVertex(map, indexes, first.startVertexId);
        const SectorTopologyVertex* b = FindUniqueVertex(map, indexes, first.endVertexId);
        if (a == nullptr || b == nullptr || first.startVertexId == first.endVertexId
            || (a->x == b->x && a->y == b->y)) {
            continue;
        }
        for (size_t j = i + 1; j < map.lineDefs.size(); ++j) {
            const SectorTopologyLineDef& second = map.lineDefs[j];
            const SectorTopologyVertex* c = FindUniqueVertex(map, indexes, second.startVertexId);
            const SectorTopologyVertex* d = FindUniqueVertex(map, indexes, second.endVertexId);
            if (c == nullptr || d == nullptr || second.startVertexId == second.endVertexId
                || (c->x == d->x && c->y == d->y)) {
                continue;
            }
            const bool duplicatePair = (first.startVertexId == second.startVertexId
                                        && first.endVertexId == second.endVertexId)
                                       || (first.startVertexId == second.endVertexId
                                           && first.endVertexId == second.startVertexId);
            if (duplicatePair) {
                continue;
            }

            const SegmentIntersectionKind intersection = SegmentIntersection(
                    Point(*a), Point(*b), Point(*c), Point(*d));
            if (intersection == SegmentIntersectionKind::None) {
                continue;
            }
            if (intersection == SegmentIntersectionKind::Touch
                && LinesShareEndpointId(first, second)) {
                continue;
            }
            if (intersection == SegmentIntersectionKind::CollinearOverlap) {
                AddIssue(&issues, SectorTopologyObjectKind::Map, -1,
                         "contains partially overlapping linedefs "
                         + std::to_string(first.id) + " and " + std::to_string(second.id));
            } else {
                AddIssue(&issues, SectorTopologyObjectKind::Map, -1,
                         "contains an invalid intersection between linedefs "
                         + std::to_string(first.id) + " and " + std::to_string(second.id));
            }
        }
    }

    for (const SectorTopologySector& sector : map.sectors) {
        if (!CanExtractSector(map, indexes, sector.id)) {
            continue;
        }
        SectorTopologyLoopSet loops;
        ExtractSectorTopologyLoops(map, indexes, sector.id, loops, &issues);
    }

    return issues;
}

bool HasSectorTopologyValidationErrors(
        const std::vector<SectorTopologyValidationIssue>& issues)
{
    return std::any_of(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == SectorTopologyValidationSeverity::Error;
    });
}

std::string FormatSectorTopologyValidationIssue(
        const SectorTopologyValidationIssue& issue)
{
    const char* severityName = "Unknown";
    switch (issue.severity) {
        case SectorTopologyValidationSeverity::Warning:
            severityName = "Warning";
            break;
        case SectorTopologyValidationSeverity::Error:
            severityName = "Error";
            break;
    }

    const char* objectName = "Unknown";
    switch (issue.objectKind) {
        case SectorTopologyObjectKind::Map:
            objectName = "Map";
            break;
        case SectorTopologyObjectKind::Vertex:
            objectName = "Vertex";
            break;
        case SectorTopologyObjectKind::LineDef:
            objectName = "LineDef";
            break;
        case SectorTopologyObjectKind::SideDef:
            objectName = "SideDef";
            break;
        case SectorTopologyObjectKind::Sector:
            objectName = "Sector";
            break;
        case SectorTopologyObjectKind::StaticLight:
            objectName = "StaticLight";
            break;
        case SectorTopologyObjectKind::DynamicLight:
            objectName = "DynamicLight";
            break;
    }

    std::ostringstream formatted;
    formatted << severityName << ": " << objectName;
    if (issue.objectId >= 0) {
        formatted << ' ' << issue.objectId;
    }
    if (!issue.message.empty()) {
        formatted << ' ' << issue.message;
    }
    return formatted.str();
}

} // namespace game
