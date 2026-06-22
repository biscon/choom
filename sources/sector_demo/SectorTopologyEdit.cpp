#include "sector_demo/SectorTopologyEdit.h"

#include "sector_demo/SectorTopologyGeometry.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace game {
namespace {

void SetError(std::string* outError, const std::string& error)
{
    if (outError != nullptr) {
        *outError = error;
    }
}

std::string FirstValidationError(const std::vector<SectorTopologyValidationIssue>& issues)
{
    for (const SectorTopologyValidationIssue& issue : issues) {
        if (issue.severity == SectorTopologyValidationSeverity::Error) {
            return FormatSectorTopologyValidationIssue(issue);
        }
    }
    return "Topology validation failed";
}

bool RequireAllocatedId(int id, const char* objectName, std::string* outError)
{
    if (IsValidSectorTopologyId(id)) {
        return true;
    }
    SetError(outError, std::string{"Could not allocate topology "} + objectName + " ID");
    return false;
}

bool FindSideDefCopy(
        const SectorTopologyMap& map,
        int sideDefId,
        SectorTopologySideKind expectedSide,
        SectorTopologySideDef& outSideDef,
        std::string* outError)
{
    if (sideDefId == -1) {
        return false;
    }

    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, sideDefId);
    if (sideDef == nullptr) {
        SetError(outError, "Topology linedef references a missing sidedef");
        return false;
    }
    if (sideDef->side != expectedSide) {
        SetError(outError, "Topology linedef sidedef slot has the wrong side kind");
        return false;
    }

    outSideDef = *sideDef;
    return true;
}

void RemoveSideDefById(SectorTopologyMap& map, int sideDefId)
{
    if (sideDefId == -1) {
        return;
    }
    map.sideDefs.erase(
            std::remove_if(
                    map.sideDefs.begin(),
                    map.sideDefs.end(),
                    [sideDefId](const SectorTopologySideDef& sideDef) {
                        return sideDef.id == sideDefId;
                    }),
            map.sideDefs.end());
}

SectorTopologySideDef DuplicateSideDef(
        const SectorTopologySideDef& source,
        int newSideDefId,
        int newLineDefId)
{
    SectorTopologySideDef duplicate = source;
    duplicate.id = newSideDefId;
    duplicate.lineDefId = newLineDefId;
    return duplicate;
}

bool ExistingSectorName(const SectorTopologyMap& map, const std::string& name)
{
    return std::any_of(map.sectors.begin(), map.sectors.end(), [&name](const SectorTopologySector& sector) {
        return sector.name == name;
    });
}

std::string GenerateSectorName(const SectorTopologyMap& map)
{
    for (int id = 1; id < 10000; ++id) {
        char candidate[32];
        std::snprintf(candidate, sizeof(candidate), "sector_%03d", id);
        if (!ExistingSectorName(map, candidate)) {
            return candidate;
        }
    }
    return "sector_" + std::to_string(map.sectors.size() + 1);
}

SectorTopologySideDef MakeSideDefFromSectorDefaults(
        int sideDefId,
        int lineDefId,
        SectorTopologySideKind side,
        int sectorId,
        const SectorTopologySector& sector)
{
    SectorTopologySideDef sideDef;
    sideDef.id = sideDefId;
    sideDef.lineDefId = lineDefId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    sideDef.wall = sector.defaultWall;
    sideDef.lower = sector.defaultLower;
    sideDef.upper = sector.defaultUpper;
    return sideDef;
}

struct LineEndpointPair {
    int a = -1;
    int b = -1;

    bool operator==(const LineEndpointPair& other) const
    {
        return a == other.a && b == other.b;
    }
};

struct LineEndpointPairHash {
    size_t operator()(const LineEndpointPair& pair) const
    {
        return (static_cast<size_t>(pair.a) << 32U) ^ static_cast<size_t>(pair.b);
    }
};

LineEndpointPair MakeLineEndpointPair(const SectorTopologyLineDef& lineDef)
{
    return LineEndpointPair{
            std::min(lineDef.startVertexId, lineDef.endVertexId),
            std::max(lineDef.startVertexId, lineDef.endVertexId)
    };
}

bool ValidateNoCollapsedLineDefs(const SectorTopologyMap& map, std::string* outError)
{
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.startVertexId == lineDef.endVertexId) {
            SetError(
                    outError,
                    "Merge would collapse topology linedef " + std::to_string(lineDef.id)
                            + " to a single vertex.");
            return false;
        }
    }
    return true;
}

bool ValidateNoDuplicatePhysicalLineDefs(const SectorTopologyMap& map, std::string* outError)
{
    std::unordered_map<LineEndpointPair, int, LineEndpointPairHash> lineIdByEndpointPair;
    lineIdByEndpointPair.reserve(map.lineDefs.size());
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        const LineEndpointPair pair = MakeLineEndpointPair(lineDef);
        const auto inserted = lineIdByEndpointPair.emplace(pair, lineDef.id);
        if (!inserted.second) {
            SetError(
                    outError,
                    "Merge would create duplicate physical linedefs "
                            + std::to_string(inserted.first->second)
                            + " and " + std::to_string(lineDef.id) + ".");
            return false;
        }
    }
    return true;
}

bool SamePoint(SectorTopologyCoordPoint a, SectorTopologyCoordPoint b)
{
    return a.x == b.x && a.y == b.y;
}

SectorTopologyCoordPoint VertexPoint(const SectorTopologyVertex& vertex)
{
    return SectorTopologyCoordPoint{vertex.x, vertex.y};
}

int FindVertexAtPoint(const SectorTopologyMap& map, SectorTopologyCoordPoint point)
{
    for (const SectorTopologyVertex& vertex : map.vertices) {
        if (vertex.x == point.x && vertex.y == point.y) {
            return vertex.id;
        }
    }
    return -1;
}

bool LoopPoints(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& loop,
        std::vector<SectorTopologyCoordPoint>& outPoints,
        std::string* outError)
{
    outPoints.clear();
    outPoints.reserve(loop.vertexIds.size());
    for (int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
        if (vertex == nullptr) {
            SetError(outError, "Could not resolve topology loop vertex " + std::to_string(vertexId));
            return false;
        }
        outPoints.push_back(VertexPoint(*vertex));
    }
    return true;
}

const SectorTopologySideDef* FindSelectedSectorSideDefForEdge(
        const SectorTopologyMap& map,
        const SectorTopologyLoopEdge& edge,
        int sectorId)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, edge.sideDefId);
    if (sideDef == nullptr || sideDef->sectorId != sectorId) {
        return nullptr;
    }
    return sideDef;
}

struct ResolvedBoundaryCutPoint {
    SectorTopologyCoordPoint point;
    int sourceVertexId = -1;
    int sourceLineDefId = -1;
    bool requiresSplit = false;
};

bool ResolveBoundaryCutPointOnOuterLoop(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& outer,
        int sectorId,
        const SectorTopologyBoundaryCutPoint& point,
        ResolvedBoundaryCutPoint& outResolved,
        std::string* outError)
{
    outResolved = {};
    outResolved.point = point.point;

    if (IsValidSectorTopologyId(point.vertexId)) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, point.vertexId);
        if (vertex == nullptr) {
            SetError(outError, "Cut endpoint vertex not found.");
            return false;
        }
        const bool belongsToOuter = std::find(
                outer.vertexIds.begin(),
                outer.vertexIds.end(),
                point.vertexId) != outer.vertexIds.end();
        if (!belongsToOuter) {
            SetError(outError, "Cut endpoint vertex is not on the selected sector outer boundary.");
            return false;
        }
        outResolved.point = VertexPoint(*vertex);
        outResolved.sourceVertexId = point.vertexId;
        return true;
    }

    if (!IsValidSectorTopologyId(point.lineDefId)) {
        SetError(outError, "Cut endpoint must identify a boundary vertex or linedef.");
        return false;
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, point.lineDefId);
    if (lineDef == nullptr) {
        SetError(outError, "Cut endpoint linedef not found.");
        return false;
    }
    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(map, *lineDef, start, end)) {
        SetError(outError, "Cut endpoint linedef has invalid endpoints.");
        return false;
    }

    const bool belongsToOuter = std::any_of(
            outer.edges.begin(),
            outer.edges.end(),
            [&](const SectorTopologyLoopEdge& edge) {
                return edge.lineDefId == point.lineDefId
                       && FindSelectedSectorSideDefForEdge(map, edge, sectorId) != nullptr;
            });
    if (!belongsToOuter) {
        SetError(outError, "Cut endpoint linedef is not on the selected sector outer boundary.");
        return false;
    }

    if (!SectorTopologyPointStrictlyInsideSegment(
                point.point,
                VertexPoint(*start),
                VertexPoint(*end))) {
        SetError(outError, "Cut endpoint must be a boundary vertex or a strict interior point on an outer linedef.");
        return false;
    }

    outResolved.sourceLineDefId = point.lineDefId;
    outResolved.requiresSplit = true;
    return true;
}

bool ResolveEndpointVertexAfterSplits(
        const SectorTopologyMap& map,
        SectorTopologyCoordPoint point,
        int& outVertexId,
        std::string* outError)
{
    outVertexId = FindVertexAtPoint(map, point);
    if (!IsValidSectorTopologyId(outVertexId)) {
        SetError(outError, "Could not resolve cut endpoint vertex after splitting boundary.");
        return false;
    }
    return true;
}

bool SplitBoundaryEndpointIfNeeded(
        SectorTopologyMap& candidate,
        const ResolvedBoundaryCutPoint& point,
        int& outVertexId,
        std::string* outError)
{
    outVertexId = -1;
    if (!point.requiresSplit) {
        outVertexId = point.sourceVertexId;
        return true;
    }

    SectorTopologySplitLineResult split;
    if (!SplitSectorTopologyLineDefAtPoint(candidate, point.sourceLineDefId, point.point, &split, outError)) {
        return false;
    }
    outVertexId = split.newVertexId;
    return true;
}

int FindVertexIndexInLoop(const SectorTopologyLoop& loop, int vertexId)
{
    const auto found = std::find(loop.vertexIds.begin(), loop.vertexIds.end(), vertexId);
    if (found == loop.vertexIds.end()) {
        return -1;
    }
    return static_cast<int>(std::distance(loop.vertexIds.begin(), found));
}

std::vector<int> BuildLoopVertexPath(const SectorTopologyLoop& loop, int startIndex, int endIndex)
{
    std::vector<int> path;
    if (loop.vertexIds.empty() || startIndex < 0 || endIndex < 0) {
        return path;
    }
    const int count = static_cast<int>(loop.vertexIds.size());
    int index = startIndex;
    while (true) {
        path.push_back(loop.vertexIds[static_cast<size_t>(index)]);
        if (index == endIndex) {
            break;
        }
        index = (index + 1) % count;
        if (index == startIndex) {
            path.clear();
            break;
        }
    }
    return path;
}

const SectorTopologyLoopEdge* FindDirectedLoopEdge(
        const SectorTopologyLoop& loop,
        int startVertexId,
        int endVertexId)
{
    const SectorTopologyLoopEdge* found = nullptr;
    for (const SectorTopologyLoopEdge& edge : loop.edges) {
        if (edge.startVertexId == startVertexId && edge.endVertexId == endVertexId) {
            if (found != nullptr) {
                return nullptr;
            }
            found = &edge;
        }
    }
    return found;
}

bool AssignPathSideDefsToSector(
        SectorTopologyMap& map,
        const SectorTopologyLoop& outer,
        const std::vector<int>& path,
        int sourceSectorId,
        int targetSectorId,
        std::string* outError)
{
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        const SectorTopologyLoopEdge* edge = FindDirectedLoopEdge(outer, path[i], path[i + 1]);
        if (edge == nullptr) {
            SetError(outError, "Cut boundary sidedef transfer is ambiguous.");
            return false;
        }
        SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, edge->sideDefId);
        if (sideDef == nullptr || sideDef->sectorId != sourceSectorId) {
            SetError(outError, "Cut boundary sidedef transfer is ambiguous.");
            return false;
        }
        sideDef->sectorId = targetSectorId;
    }
    return true;
}

bool ValidateCutSegmentAgainstExistingTopology(
        const SectorTopologyMap& map,
        int firstVertexId,
        int secondVertexId,
        std::string* outError)
{
    const SectorTopologyVertex* first = FindSectorTopologyVertex(map, firstVertexId);
    const SectorTopologyVertex* second = FindSectorTopologyVertex(map, secondVertexId);
    if (first == nullptr || second == nullptr) {
        SetError(outError, "Cut endpoints are invalid.");
        return false;
    }
    const SectorTopologyCoordPoint a = VertexPoint(*first);
    const SectorTopologyCoordPoint b = VertexPoint(*second);

    for (const SectorTopologyVertex& vertex : map.vertices) {
        if (vertex.id == firstVertexId || vertex.id == secondVertexId) {
            continue;
        }
        if (SectorTopologyPointOnSegment(VertexPoint(vertex), a, b)) {
            SetError(outError, "Cut must not pass through an unrelated topology vertex.");
            return false;
        }
    }

    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        const SectorTopologyVertex* lineStart = nullptr;
        const SectorTopologyVertex* lineEnd = nullptr;
        if (!GetSectorTopologyLineVertices(map, lineDef, lineStart, lineEnd)) {
            SetError(outError, "Could not resolve existing topology linedef.");
            return false;
        }
        const SectorTopologySegmentIntersectionKind intersection =
                SectorTopologySegmentIntersection(a, b, VertexPoint(*lineStart), VertexPoint(*lineEnd));
        if (intersection == SectorTopologySegmentIntersectionKind::None) {
            continue;
        }
        if (intersection == SectorTopologySegmentIntersectionKind::CollinearOverlap) {
            SetError(outError, "Cut must not align with existing topology.");
            return false;
        }
        if (intersection == SectorTopologySegmentIntersectionKind::Proper) {
            SetError(outError, "Cut must not cross existing topology.");
            return false;
        }
        const bool touchesAtCutEndpoint =
                lineDef.startVertexId == firstVertexId
                || lineDef.endVertexId == firstVertexId
                || lineDef.startVertexId == secondVertexId
                || lineDef.endVertexId == secondVertexId;
        if (!touchesAtCutEndpoint) {
            SetError(outError, "Cut must not touch existing topology away from its endpoints.");
            return false;
        }
    }
    return true;
}

bool ValidateCutInteriorInsideSelectedSector(
        const SectorTopologyMap& map,
        const SectorTopologyLoopSet& loops,
        SectorTopologyCoordPoint first,
        SectorTopologyCoordPoint second,
        std::string* outError)
{
    const int64_t midpointXSum = static_cast<int64_t>(first.x) + static_cast<int64_t>(second.x);
    const int64_t midpointYSum = static_cast<int64_t>(first.y) + static_cast<int64_t>(second.y);
    if ((midpointXSum % 2) != 0 || (midpointYSum % 2) != 0) {
        return true;
    }

    const SectorTopologyCoordPoint midpoint{
            static_cast<SectorCoord>(midpointXSum / 2),
            static_cast<SectorCoord>(midpointYSum / 2)
    };

    std::vector<SectorTopologyCoordPoint> outerPoints;
    if (!LoopPoints(map, loops.outer, outerPoints, outError)) {
        return false;
    }
    if (SectorTopologyClassifyPointInPolygon(outerPoints, midpoint)
            != SectorTopologyPointContainment::Inside) {
        SetError(outError, "Cut must stay inside the selected sector.");
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        std::vector<SectorTopologyCoordPoint> holePoints;
        if (!LoopPoints(map, hole, holePoints, outError)) {
            return false;
        }
        if (SectorTopologyClassifyPointInPolygon(holePoints, midpoint)
                != SectorTopologyPointContainment::Outside) {
            SetError(outError, "Cut must not enter a selected sector hole.");
            return false;
        }
    }
    return true;
}

bool BuildPathPoints(
        const SectorTopologyMap& map,
        const std::vector<int>& path,
        std::vector<SectorTopologyCoordPoint>& outPoints,
        std::string* outError)
{
    outPoints.clear();
    outPoints.reserve(path.size());
    for (int vertexId : path) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
        if (vertex == nullptr) {
            SetError(outError, "Could not resolve cut output vertex.");
            return false;
        }
        outPoints.push_back(VertexPoint(*vertex));
    }
    return true;
}

bool AssignHoleLoopToResultSector(
        SectorTopologyMap& map,
        const SectorTopologyLoop& hole,
        const std::vector<SectorTopologyCoordPoint>& firstPolygon,
        const std::vector<SectorTopologyCoordPoint>& secondPolygon,
        SectorTopologyCoordPoint cutA,
        SectorTopologyCoordPoint cutB,
        int firstSectorId,
        int secondSectorId,
        std::string* outError)
{
    std::vector<SectorTopologyCoordPoint> holePoints;
    if (!LoopPoints(map, hole, holePoints, outError)) {
        return false;
    }

    for (size_t i = 0; i < holePoints.size(); ++i) {
        const SectorTopologyCoordPoint a = holePoints[i];
        const SectorTopologyCoordPoint b = holePoints[(i + 1) % holePoints.size()];
        if (SectorTopologySegmentIntersection(a, b, cutA, cutB)
                != SectorTopologySegmentIntersectionKind::None) {
            SetError(outError, "Cut must not touch or cross a hole boundary.");
            return false;
        }
    }

    bool allInFirst = true;
    bool allInSecond = true;
    for (SectorTopologyCoordPoint point : holePoints) {
        const SectorTopologyPointContainment firstContainment =
                SectorTopologyClassifyPointInPolygon(firstPolygon, point);
        const SectorTopologyPointContainment secondContainment =
                SectorTopologyClassifyPointInPolygon(secondPolygon, point);
        if (firstContainment == SectorTopologyPointContainment::Boundary
                || secondContainment == SectorTopologyPointContainment::Boundary) {
            SetError(outError, "Cut leaves a hole ambiguously on a result boundary.");
            return false;
        }
        allInFirst = allInFirst && firstContainment == SectorTopologyPointContainment::Inside;
        allInSecond = allInSecond && secondContainment == SectorTopologyPointContainment::Inside;
    }

    if (allInFirst == allInSecond) {
        SetError(outError, "Cut cannot assign a hole unambiguously to one result sector.");
        return false;
    }

    const int targetSectorId = allInFirst ? firstSectorId : secondSectorId;
    for (const SectorTopologyLoopEdge& edge : hole.edges) {
        SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, edge.sideDefId);
        if (sideDef == nullptr || sideDef->sectorId != firstSectorId) {
            SetError(outError, "Cut hole sidedef transfer is ambiguous.");
            return false;
        }
        sideDef->sectorId = targetSectorId;
    }
    return true;
}

void AddSideDefSectorId(
        const SectorTopologyMap& map,
        int sideDefId,
        std::unordered_set<int>& sectorIds)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, sideDefId);
    if (sideDef != nullptr && IsValidSectorTopologyId(sideDef->sectorId)) {
        sectorIds.insert(sideDef->sectorId);
    }
}

bool SameUvSettings(SectorTopologyUvSettings a, SectorTopologyUvSettings b)
{
    return a.scale.x == b.scale.x
            && a.scale.y == b.scale.y
            && a.offset.x == b.offset.x
            && a.offset.y == b.offset.y;
}

bool SameWallPartSettings(
        const SectorTopologyWallPartSettings& a,
        const SectorTopologyWallPartSettings& b)
{
    return a.textureId == b.textureId && SameUvSettings(a.uv, b.uv);
}

bool SameSideDefMergeSettings(
        const SectorTopologySideDef& a,
        const SectorTopologySideDef& b)
{
    return a.sectorId == b.sectorId
            && SameWallPartSettings(a.wall, b.wall)
            && SameWallPartSettings(a.lower, b.lower)
            && SameWallPartSettings(a.upper, b.upper);
}

struct DirectedSideDefCopy {
    SectorTopologySideDef sideDef;
    int fromVertexId = -1;
    int toVertexId = -1;
};

bool AddDirectedSideDefCopy(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& lineDef,
        int sideDefId,
        SectorTopologySideKind expectedSide,
        std::vector<DirectedSideDefCopy>& sides,
        std::string* outError)
{
    if (sideDefId == -1) {
        return true;
    }

    SectorTopologySideDef sideDef;
    if (!FindSideDefCopy(map, sideDefId, expectedSide, sideDef, outError)) {
        return false;
    }
    if (sideDef.lineDefId != lineDef.id) {
        SetError(outError, "Topology linedef references a sidedef owned by another linedef");
        return false;
    }

    DirectedSideDefCopy directed;
    directed.sideDef = sideDef;
    if (expectedSide == SectorTopologySideKind::Front) {
        directed.fromVertexId = lineDef.startVertexId;
        directed.toVertexId = lineDef.endVertexId;
    } else {
        directed.fromVertexId = lineDef.endVertexId;
        directed.toVertexId = lineDef.startVertexId;
    }
    sides.push_back(std::move(directed));
    return true;
}

const DirectedSideDefCopy* FindDirectedSide(
        const std::vector<DirectedSideDefCopy>& sides,
        const std::vector<bool>& used,
        int fromVertexId,
        int toVertexId)
{
    const DirectedSideDefCopy* found = nullptr;
    for (size_t i = 0; i < sides.size(); ++i) {
        if (used[i]
                || sides[i].fromVertexId != fromVertexId
                || sides[i].toVertexId != toVertexId) {
            continue;
        }
        if (found != nullptr) {
            return nullptr;
        }
        found = &sides[i];
    }
    return found;
}

int FindDirectedSideIndex(
        const std::vector<DirectedSideDefCopy>& sides,
        const DirectedSideDefCopy* side)
{
    if (side == nullptr) {
        return -1;
    }
    for (size_t i = 0; i < sides.size(); ++i) {
        if (&sides[i] == side) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

struct DissolveReplacementPlan {
    int startVertexId = -1;
    int endVertexId = -1;
    bool hasFront = false;
    bool hasBack = false;
    SectorTopologySideDef frontSideDef;
    SectorTopologySideDef backSideDef;
};

bool TryBuildDissolveReplacementPlan(
        const std::vector<DirectedSideDefCopy>& sides,
        int removedVertexId,
        int startVertexId,
        int endVertexId,
        DissolveReplacementPlan& outPlan,
        std::string* outError)
{
    if (outError != nullptr) {
        outError->clear();
    }
    std::vector<bool> used(sides.size(), false);
    outPlan = DissolveReplacementPlan{};
    outPlan.startVertexId = startVertexId;
    outPlan.endVertexId = endVertexId;

    auto mergeChain = [&](
            int firstFrom,
            int firstTo,
            int secondFrom,
            int secondTo,
            SectorTopologySideDef& outSideDef) {
        const DirectedSideDefCopy* first = FindDirectedSide(sides, used, firstFrom, firstTo);
        if (first == nullptr) {
            return false;
        }
        const int firstIndex = FindDirectedSideIndex(sides, first);
        if (firstIndex < 0) {
            return false;
        }
        used[static_cast<size_t>(firstIndex)] = true;

        const DirectedSideDefCopy* second = FindDirectedSide(sides, used, secondFrom, secondTo);
        if (second == nullptr) {
            used[static_cast<size_t>(firstIndex)] = false;
            return false;
        }
        const int secondIndex = FindDirectedSideIndex(sides, second);
        if (secondIndex < 0) {
            used[static_cast<size_t>(firstIndex)] = false;
            return false;
        }

        if (!SameSideDefMergeSettings(first->sideDef, second->sideDef)) {
            used[static_cast<size_t>(firstIndex)] = false;
            SetError(outError, "Dissolve would merge sidedefs with different sector, material, or UV settings.");
            return false;
        }

        used[static_cast<size_t>(secondIndex)] = true;
        outSideDef = first->sideDef;
        return true;
    };

    std::string mergeError;
    if (mergeChain(startVertexId, removedVertexId, removedVertexId, endVertexId, outPlan.frontSideDef)) {
        outPlan.hasFront = true;
    } else if (outError != nullptr && !outError->empty()) {
        mergeError = *outError;
        outError->clear();
    }

    if (mergeChain(endVertexId, removedVertexId, removedVertexId, startVertexId, outPlan.backSideDef)) {
        outPlan.hasBack = true;
    } else if (outError != nullptr && !outError->empty()) {
        mergeError = *outError;
        outError->clear();
    }

    for (bool wasUsed : used) {
        if (!wasUsed) {
            if (!mergeError.empty()) {
                SetError(outError, mergeError);
            } else {
                SetError(outError, "Dissolve sidedef transfer is ambiguous.");
            }
            return false;
        }
    }

    if (!outPlan.hasFront && !outPlan.hasBack) {
        SetError(outError, "Dissolve vertex has no sidedef chain to preserve.");
        return false;
    }
    return true;
}

} // namespace

bool MoveSectorTopologyVertex(
        SectorTopologyMap& map,
        int vertexId,
        SectorTopologyCoordPoint newPosition,
        std::string* outError)
{
    if (outError != nullptr) {
        outError->clear();
    }

    if (!IsValidSectorTopologyId(vertexId)) {
        SetError(outError, "Invalid topology vertex ID");
        return false;
    }

    const SectorTopologyVertex* existing = FindSectorTopologyVertex(map, vertexId);
    if (existing == nullptr) {
        SetError(outError, "Topology vertex not found");
        return false;
    }

    for (const SectorTopologyVertex& vertex : map.vertices) {
        if (vertex.id != vertexId
                && vertex.x == newPosition.x
                && vertex.y == newPosition.y) {
            SetError(outError, "Cannot move vertex onto existing vertex without merge support.");
            return false;
        }
    }

    if (existing->x == newPosition.x && existing->y == newPosition.y) {
        return true;
    }

    SectorTopologyMap candidate = map;
    SectorTopologyVertex* moved = FindSectorTopologyVertex(candidate, vertexId);
    if (moved == nullptr) {
        SetError(outError, "Topology vertex not found");
        return false;
    }

    moved->x = newPosition.x;
    moved->y = newPosition.y;

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(candidate);
    if (HasSectorTopologyValidationErrors(issues)) {
        SetError(outError, FirstValidationError(issues));
        return false;
    }

    map = std::move(candidate);
    return true;
}

bool MergeSectorTopologyVertices(
        SectorTopologyMap& map,
        int sourceVertexId,
        int targetVertexId,
        SectorTopologyMergeVerticesResult* outResult,
        std::string* outError)
{
    if (outResult != nullptr) {
        *outResult = SectorTopologyMergeVerticesResult{};
    }
    if (outError != nullptr) {
        outError->clear();
    }

    if (!IsValidSectorTopologyId(sourceVertexId)) {
        SetError(outError, "Invalid source topology vertex ID");
        return false;
    }
    if (!IsValidSectorTopologyId(targetVertexId)) {
        SetError(outError, "Invalid target topology vertex ID");
        return false;
    }
    if (sourceVertexId == targetVertexId) {
        SetError(outError, "Cannot merge topology vertex into itself.");
        return false;
    }

    if (FindSectorTopologyVertex(map, sourceVertexId) == nullptr) {
        SetError(outError, "Source topology vertex not found");
        return false;
    }
    if (FindSectorTopologyVertex(map, targetVertexId) == nullptr) {
        SetError(outError, "Target topology vertex not found");
        return false;
    }

    std::unordered_set<int> affectedSectorIds;
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.startVertexId == sourceVertexId || lineDef.endVertexId == sourceVertexId) {
            AddSideDefSectorId(map, lineDef.frontSideDefId, affectedSectorIds);
            AddSideDefSectorId(map, lineDef.backSideDefId, affectedSectorIds);
        }
    }

    SectorTopologyMap candidate = map;
    for (SectorTopologyLineDef& lineDef : candidate.lineDefs) {
        if (lineDef.startVertexId == sourceVertexId) {
            lineDef.startVertexId = targetVertexId;
        }
        if (lineDef.endVertexId == sourceVertexId) {
            lineDef.endVertexId = targetVertexId;
        }
    }

    candidate.vertices.erase(
            std::remove_if(
                    candidate.vertices.begin(),
                    candidate.vertices.end(),
                    [sourceVertexId](const SectorTopologyVertex& vertex) {
                        return vertex.id == sourceVertexId;
                    }),
            candidate.vertices.end());

    if (!ValidateNoCollapsedLineDefs(candidate, outError)) {
        return false;
    }
    if (!ValidateNoDuplicatePhysicalLineDefs(candidate, outError)) {
        return false;
    }

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(candidate);
    if (HasSectorTopologyValidationErrors(issues)) {
        SetError(outError, FirstValidationError(issues));
        return false;
    }

    for (int sectorId : affectedSectorIds) {
        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(candidate, sectorId, loops, &loopIssues)) {
            SetError(outError, FirstValidationError(loopIssues));
            return false;
        }
    }

    map = std::move(candidate);
    if (outResult != nullptr) {
        outResult->mergedVertexId = targetVertexId;
        outResult->removedVertexId = sourceVertexId;
    }
    return true;
}

bool DissolveSectorTopologyVertex(
        SectorTopologyMap& map,
        int vertexId,
        SectorTopologyDissolveVertexResult* outResult,
        std::string* outError)
{
    if (outResult != nullptr) {
        *outResult = SectorTopologyDissolveVertexResult{};
    }
    if (outError != nullptr) {
        outError->clear();
    }

    if (!IsValidSectorTopologyId(vertexId)) {
        SetError(outError, "Invalid topology vertex ID");
        return false;
    }
    if (FindSectorTopologyVertex(map, vertexId) == nullptr) {
        SetError(outError, "Topology vertex not found");
        return false;
    }

    std::vector<SectorTopologyLineDef> incidentLines;
    incidentLines.reserve(2);
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.startVertexId == vertexId || lineDef.endVertexId == vertexId) {
            incidentLines.push_back(lineDef);
        }
    }

    if (incidentLines.size() < 2) {
        SetError(outError, "Dissolve requires exactly two incident topology linedefs.");
        return false;
    }
    if (incidentLines.size() > 2) {
        SetError(outError, "Cannot dissolve a branching topology vertex.");
        return false;
    }

    const SectorTopologyLineDef& firstLine = incidentLines[0];
    const SectorTopologyLineDef& secondLine = incidentLines[1];
    const int firstOtherVertexId = firstLine.startVertexId == vertexId
            ? firstLine.endVertexId
            : firstLine.startVertexId;
    const int secondOtherVertexId = secondLine.startVertexId == vertexId
            ? secondLine.endVertexId
            : secondLine.startVertexId;
    if (firstOtherVertexId == vertexId || secondOtherVertexId == vertexId) {
        SetError(outError, "Dissolve would collapse an incident topology linedef.");
        return false;
    }
    if (firstOtherVertexId == secondOtherVertexId) {
        SetError(outError, "Dissolve replacement linedef would collapse.");
        return false;
    }
    if (FindSectorTopologyVertex(map, firstOtherVertexId) == nullptr
            || FindSectorTopologyVertex(map, secondOtherVertexId) == nullptr) {
        SetError(outError, "Dissolve incident linedef endpoint is missing.");
        return false;
    }

    const LineEndpointPair replacementPair{
            std::min(firstOtherVertexId, secondOtherVertexId),
            std::max(firstOtherVertexId, secondOtherVertexId)
    };
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.id == firstLine.id || lineDef.id == secondLine.id) {
            continue;
        }
        if (MakeLineEndpointPair(lineDef) == replacementPair) {
            SetError(outError, "Dissolve would create duplicate physical linedefs.");
            return false;
        }
    }

    std::vector<DirectedSideDefCopy> sides;
    sides.reserve(4);
    if (!AddDirectedSideDefCopy(map, firstLine, firstLine.frontSideDefId, SectorTopologySideKind::Front, sides, outError)
            || !AddDirectedSideDefCopy(map, firstLine, firstLine.backSideDefId, SectorTopologySideKind::Back, sides, outError)
            || !AddDirectedSideDefCopy(map, secondLine, secondLine.frontSideDefId, SectorTopologySideKind::Front, sides, outError)
            || !AddDirectedSideDefCopy(map, secondLine, secondLine.backSideDefId, SectorTopologySideKind::Back, sides, outError)) {
        return false;
    }

    std::unordered_set<int> affectedSectorIds;
    for (const DirectedSideDefCopy& side : sides) {
        if (IsValidSectorTopologyId(side.sideDef.sectorId)) {
            affectedSectorIds.insert(side.sideDef.sectorId);
        }
    }

    const int preferredStart = firstLine.id <= secondLine.id ? firstOtherVertexId : secondOtherVertexId;
    const int preferredEnd = firstLine.id <= secondLine.id ? secondOtherVertexId : firstOtherVertexId;
    DissolveReplacementPlan plan;
    std::string planError;
    if (!TryBuildDissolveReplacementPlan(sides, vertexId, preferredStart, preferredEnd, plan, &planError)
            && !TryBuildDissolveReplacementPlan(sides, vertexId, preferredEnd, preferredStart, plan, &planError)) {
        SetError(outError, planError.empty() ? "Dissolve sidedef transfer is ambiguous." : planError);
        return false;
    }

    SectorTopologyMap candidate = map;
    SectorTopologyDissolveVertexResult result;
    result.removedVertexId = vertexId;
    result.replacementLineDefId = AllocateSectorTopologyLineDefId(candidate);
    if (!RequireAllocatedId(result.replacementLineDefId, "linedef", outError)) {
        return false;
    }

    candidate.lineDefs.push_back(SectorTopologyLineDef{
            result.replacementLineDefId,
            plan.startVertexId,
            plan.endVertexId,
            -1,
            -1
    });
    SectorTopologyLineDef* replacementLine =
            FindSectorTopologyLineDef(candidate, result.replacementLineDefId);
    if (replacementLine == nullptr) {
        SetError(outError, "Could not resolve replacement topology linedef");
        return false;
    }

    if (plan.hasFront) {
        result.replacementFrontSideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(result.replacementFrontSideDefId, "sidedef", outError)) {
            return false;
        }
        SectorTopologySideDef replacement = DuplicateSideDef(
                plan.frontSideDef,
                result.replacementFrontSideDefId,
                result.replacementLineDefId);
        replacement.side = SectorTopologySideKind::Front;
        candidate.sideDefs.push_back(std::move(replacement));
        replacementLine->frontSideDefId = result.replacementFrontSideDefId;
    }

    if (plan.hasBack) {
        result.replacementBackSideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(result.replacementBackSideDefId, "sidedef", outError)) {
            return false;
        }
        SectorTopologySideDef replacement = DuplicateSideDef(
                plan.backSideDef,
                result.replacementBackSideDefId,
                result.replacementLineDefId);
        replacement.side = SectorTopologySideKind::Back;
        candidate.sideDefs.push_back(std::move(replacement));
        replacementLine->backSideDefId = result.replacementBackSideDefId;
    }

    for (const DirectedSideDefCopy& side : sides) {
        RemoveSideDefById(candidate, side.sideDef.id);
    }
    candidate.lineDefs.erase(
            std::remove_if(
                    candidate.lineDefs.begin(),
                    candidate.lineDefs.end(),
                    [&firstLine, &secondLine](const SectorTopologyLineDef& lineDef) {
                        return lineDef.id == firstLine.id || lineDef.id == secondLine.id;
                    }),
            candidate.lineDefs.end());
    candidate.vertices.erase(
            std::remove_if(
                    candidate.vertices.begin(),
                    candidate.vertices.end(),
                    [vertexId](const SectorTopologyVertex& vertex) {
                        return vertex.id == vertexId;
                    }),
            candidate.vertices.end());

    if (!ValidateNoCollapsedLineDefs(candidate, outError)) {
        return false;
    }
    if (!ValidateNoDuplicatePhysicalLineDefs(candidate, outError)) {
        return false;
    }

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(candidate);
    if (HasSectorTopologyValidationErrors(issues)) {
        SetError(outError, FirstValidationError(issues));
        return false;
    }

    for (int sectorId : affectedSectorIds) {
        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(candidate, sectorId, loops, &loopIssues)) {
            SetError(outError, FirstValidationError(loopIssues));
            return false;
        }
    }

    map = std::move(candidate);
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool SplitSectorTopologyLineDef(
        SectorTopologyMap& map,
        int lineDefId,
        SectorTopologySplitLineResult* outResult,
        std::string* outError)
{
    if (outResult != nullptr) {
        *outResult = SectorTopologySplitLineResult{};
    }
    if (outError != nullptr) {
        outError->clear();
    }

    if (!IsValidSectorTopologyId(lineDefId)) {
        SetError(outError, "Invalid topology linedef ID");
        return false;
    }

    const SectorTopologyLineDef* existing = FindSectorTopologyLineDef(map, lineDefId);
    if (existing == nullptr) {
        SetError(outError, "Topology linedef not found");
        return false;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(map, *existing, start, end)) {
        SetError(outError, "Topology linedef endpoints are invalid");
        return false;
    }

    const int64_t midpointXSum = static_cast<int64_t>(start->x) + static_cast<int64_t>(end->x);
    const int64_t midpointYSum = static_cast<int64_t>(start->y) + static_cast<int64_t>(end->y);
    if ((midpointXSum % 2) != 0 || (midpointYSum % 2) != 0) {
        SetError(outError, "Cannot split this linedef exactly on the integer coordinate grid.");
        return false;
    }

    const int64_t midpointX = midpointXSum / 2;
    const int64_t midpointY = midpointYSum / 2;
    if (midpointX < std::numeric_limits<SectorCoord>::min()
            || midpointX > std::numeric_limits<SectorCoord>::max()
            || midpointY < std::numeric_limits<SectorCoord>::min()
            || midpointY > std::numeric_limits<SectorCoord>::max()) {
        SetError(outError, "Cannot split this linedef exactly on the integer coordinate grid.");
        return false;
    }

    const SectorTopologyCoordPoint midpoint{
            static_cast<SectorCoord>(midpointX),
            static_cast<SectorCoord>(midpointY)
    };
    return SplitSectorTopologyLineDefAtPoint(map, lineDefId, midpoint, outResult, outError);
}

bool SplitSectorTopologyLineDefAtPoint(
        SectorTopologyMap& map,
        int lineDefId,
        SectorTopologyCoordPoint point,
        SectorTopologySplitLineResult* outResult,
        std::string* outError)
{
    if (outResult != nullptr) {
        *outResult = SectorTopologySplitLineResult{};
    }
    if (outError != nullptr) {
        outError->clear();
    }

    if (!IsValidSectorTopologyId(lineDefId)) {
        SetError(outError, "Invalid topology linedef ID");
        return false;
    }

    const SectorTopologyLineDef* existing = FindSectorTopologyLineDef(map, lineDefId);
    if (existing == nullptr) {
        SetError(outError, "Topology linedef not found");
        return false;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(map, *existing, start, end)) {
        SetError(outError, "Topology linedef endpoints are invalid");
        return false;
    }

    const SectorTopologyCoordPoint startPoint{start->x, start->y};
    const SectorTopologyCoordPoint endPoint{end->x, end->y};
    if (!SectorTopologyPointOnSegment(point, startPoint, endPoint)) {
        SetError(outError, "Split point must lie exactly on the topology linedef.");
        return false;
    }
    if (!SectorTopologyPointStrictlyInsideSegment(point, startPoint, endPoint)) {
        SetError(outError, "Split point cannot be a linedef endpoint.");
        return false;
    }

    for (const SectorTopologyVertex& vertex : map.vertices) {
        if (vertex.x == point.x && vertex.y == point.y) {
            SetError(outError, "Split point is already occupied by a topology vertex.");
            return false;
        }
    }

    SectorTopologyMap candidate = map;
    SectorTopologySplitLineResult result;

    SectorTopologySideDef originalFront;
    SectorTopologySideDef originalBack;
    std::string error;
    const bool hasFront = FindSideDefCopy(
            candidate,
            existing->frontSideDefId,
            SectorTopologySideKind::Front,
            originalFront,
            &error);
    if (!hasFront && !error.empty()) {
        SetError(outError, error);
        return false;
    }
    const bool hasBack = FindSideDefCopy(
            candidate,
            existing->backSideDefId,
            SectorTopologySideKind::Back,
            originalBack,
            &error);
    if (!hasBack && !error.empty()) {
        SetError(outError, error);
        return false;
    }

    result.newVertexId = AllocateSectorTopologyVertexId(candidate);
    if (!RequireAllocatedId(result.newVertexId, "vertex", outError)) {
        return false;
    }
    result.midpointVertexId = result.newVertexId;
    candidate.vertices.push_back(SectorTopologyVertex{
            result.newVertexId,
            point.x,
            point.y
    });

    result.firstLineDefId = AllocateSectorTopologyLineDefId(candidate);
    if (!RequireAllocatedId(result.firstLineDefId, "linedef", outError)) {
        return false;
    }
    candidate.lineDefs.push_back(SectorTopologyLineDef{
            result.firstLineDefId,
            existing->startVertexId,
            result.newVertexId,
            -1,
            -1
    });

    result.secondLineDefId = AllocateSectorTopologyLineDefId(candidate);
    if (!RequireAllocatedId(result.secondLineDefId, "linedef", outError)) {
        return false;
    }
    candidate.lineDefs.push_back(SectorTopologyLineDef{
            result.secondLineDefId,
            result.newVertexId,
            existing->endVertexId,
            -1,
            -1
    });

    SectorTopologyLineDef* firstLine = FindSectorTopologyLineDef(candidate, result.firstLineDefId);
    SectorTopologyLineDef* secondLine = FindSectorTopologyLineDef(candidate, result.secondLineDefId);
    if (firstLine == nullptr || secondLine == nullptr) {
        SetError(outError, "Could not resolve replacement topology linedefs");
        return false;
    }

    if (hasFront) {
        result.firstFrontSideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(result.firstFrontSideDefId, "sidedef", outError)) {
            return false;
        }
        candidate.sideDefs.push_back(DuplicateSideDef(
                originalFront,
                result.firstFrontSideDefId,
                result.firstLineDefId));
        firstLine->frontSideDefId = result.firstFrontSideDefId;

        result.secondFrontSideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(result.secondFrontSideDefId, "sidedef", outError)) {
            return false;
        }
        candidate.sideDefs.push_back(DuplicateSideDef(
                originalFront,
                result.secondFrontSideDefId,
                result.secondLineDefId));
        secondLine->frontSideDefId = result.secondFrontSideDefId;
    }

    if (hasBack) {
        result.firstBackSideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(result.firstBackSideDefId, "sidedef", outError)) {
            return false;
        }
        candidate.sideDefs.push_back(DuplicateSideDef(
                originalBack,
                result.firstBackSideDefId,
                result.firstLineDefId));
        firstLine->backSideDefId = result.firstBackSideDefId;

        result.secondBackSideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(result.secondBackSideDefId, "sidedef", outError)) {
            return false;
        }
        candidate.sideDefs.push_back(DuplicateSideDef(
                originalBack,
                result.secondBackSideDefId,
                result.secondLineDefId));
        secondLine->backSideDefId = result.secondBackSideDefId;
    }

    RemoveSideDefById(candidate, existing->frontSideDefId);
    RemoveSideDefById(candidate, existing->backSideDefId);
    candidate.lineDefs.erase(
            std::remove_if(
                    candidate.lineDefs.begin(),
                    candidate.lineDefs.end(),
                    [lineDefId](const SectorTopologyLineDef& lineDef) {
                        return lineDef.id == lineDefId;
                    }),
            candidate.lineDefs.end());

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(candidate);
    if (HasSectorTopologyValidationErrors(issues)) {
        SetError(outError, FirstValidationError(issues));
        return false;
    }

    if (hasFront) {
        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(candidate, originalFront.sectorId, loops, &loopIssues)) {
            SetError(outError, FirstValidationError(loopIssues));
            return false;
        }
    }
    if (hasBack && (!hasFront || originalBack.sectorId != originalFront.sectorId)) {
        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(candidate, originalBack.sectorId, loops, &loopIssues)) {
            SetError(outError, FirstValidationError(loopIssues));
            return false;
        }
    }

    map = std::move(candidate);
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool DeleteSectorTopologySector(
        SectorTopologyMap& map,
        int sectorId,
        SectorTopologyDeleteSectorResult* outResult,
        std::string* outError)
{
    if (outResult != nullptr) {
        *outResult = SectorTopologyDeleteSectorResult{};
    }
    if (outError != nullptr) {
        outError->clear();
    }

    if (!IsValidSectorTopologyId(sectorId)) {
        SetError(outError, "Invalid topology sector ID");
        return false;
    }
    if (FindSectorTopologySector(map, sectorId) == nullptr) {
        SetError(outError, "Topology sector not found");
        return false;
    }

    SectorTopologyMap candidate = map;
    SectorTopologyDeleteSectorResult result;
    result.deletedSectorId = sectorId;

    std::unordered_set<int> removedSideDefIds;
    for (const SectorTopologySideDef& sideDef : candidate.sideDefs) {
        if (sideDef.sectorId == sectorId) {
            removedSideDefIds.insert(sideDef.id);
        }
    }
    result.removedSideDefCount = static_cast<int>(removedSideDefIds.size());

    for (SectorTopologyLineDef& lineDef : candidate.lineDefs) {
        if (removedSideDefIds.find(lineDef.frontSideDefId) != removedSideDefIds.end()) {
            lineDef.frontSideDefId = -1;
        }
        if (removedSideDefIds.find(lineDef.backSideDefId) != removedSideDefIds.end()) {
            lineDef.backSideDefId = -1;
        }
    }

    candidate.sideDefs.erase(
            std::remove_if(
                    candidate.sideDefs.begin(),
                    candidate.sideDefs.end(),
                    [&removedSideDefIds](const SectorTopologySideDef& sideDef) {
                        return removedSideDefIds.find(sideDef.id) != removedSideDefIds.end();
                    }),
            candidate.sideDefs.end());

    candidate.sectors.erase(
            std::remove_if(
                    candidate.sectors.begin(),
                    candidate.sectors.end(),
                    [sectorId](const SectorTopologySector& sector) {
                        return sector.id == sectorId;
                    }),
            candidate.sectors.end());

    const size_t lineCountBefore = candidate.lineDefs.size();
    candidate.lineDefs.erase(
            std::remove_if(
                    candidate.lineDefs.begin(),
                    candidate.lineDefs.end(),
                    [](const SectorTopologyLineDef& lineDef) {
                        return lineDef.frontSideDefId == -1 && lineDef.backSideDefId == -1;
                    }),
            candidate.lineDefs.end());
    result.removedLineDefCount = static_cast<int>(lineCountBefore - candidate.lineDefs.size());

    std::unordered_set<int> referencedVertexIds;
    for (const SectorTopologyLineDef& lineDef : candidate.lineDefs) {
        referencedVertexIds.insert(lineDef.startVertexId);
        referencedVertexIds.insert(lineDef.endVertexId);
    }

    const size_t vertexCountBefore = candidate.vertices.size();
    candidate.vertices.erase(
            std::remove_if(
                    candidate.vertices.begin(),
                    candidate.vertices.end(),
                    [&referencedVertexIds](const SectorTopologyVertex& vertex) {
                        return referencedVertexIds.find(vertex.id) == referencedVertexIds.end();
                    }),
            candidate.vertices.end());
    result.removedVertexCount = static_cast<int>(vertexCountBefore - candidate.vertices.size());

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(candidate);
    if (HasSectorTopologyValidationErrors(issues)) {
        SetError(outError, FirstValidationError(issues));
        return false;
    }

    map = std::move(candidate);
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

bool CutSectorTopologySectorBetweenBoundaryPoints(
        SectorTopologyMap& map,
        int sectorId,
        SectorTopologyBoundaryCutPoint firstPoint,
        SectorTopologyBoundaryCutPoint secondPoint,
        SectorTopologyCutSectorResult* outResult,
        std::string* outError)
{
    if (outResult != nullptr) {
        *outResult = SectorTopologyCutSectorResult{};
    }
    if (outError != nullptr) {
        outError->clear();
    }

    if (!IsValidSectorTopologyId(sectorId)) {
        SetError(outError, "Invalid topology sector ID");
        return false;
    }
    const SectorTopologySector* originalSector = FindSectorTopologySector(map, sectorId);
    if (originalSector == nullptr) {
        SetError(outError, "Topology sector not found");
        return false;
    }

    const std::vector<SectorTopologyValidationIssue> initialIssues = ValidateSectorTopologyMap(map);
    if (HasSectorTopologyValidationErrors(initialIssues)) {
        SetError(outError, FirstValidationError(initialIssues));
        return false;
    }

    SectorTopologyLoopSet originalLoops;
    std::vector<SectorTopologyValidationIssue> originalLoopIssues;
    if (!ExtractSectorTopologyLoops(map, sectorId, originalLoops, &originalLoopIssues)) {
        SetError(outError, FirstValidationError(originalLoopIssues));
        return false;
    }

    ResolvedBoundaryCutPoint firstResolved;
    ResolvedBoundaryCutPoint secondResolved;
    if (!ResolveBoundaryCutPointOnOuterLoop(map, originalLoops.outer, sectorId, firstPoint, firstResolved, outError)
            || !ResolveBoundaryCutPointOnOuterLoop(map, originalLoops.outer, sectorId, secondPoint, secondResolved, outError)) {
        return false;
    }
    if (SamePoint(firstResolved.point, secondResolved.point)) {
        SetError(outError, "Cut endpoints must be different.");
        return false;
    }
    if (firstResolved.requiresSplit
            && secondResolved.requiresSplit
            && firstResolved.sourceLineDefId == secondResolved.sourceLineDefId) {
        SetError(outError, "Cut endpoints must not be on the same boundary edge.");
        return false;
    }

    SectorTopologyMap candidate = map;
    int firstEndpointVertexId = -1;
    int secondEndpointVertexId = -1;
    if (!SplitBoundaryEndpointIfNeeded(candidate, firstResolved, firstEndpointVertexId, outError)
            || !SplitBoundaryEndpointIfNeeded(candidate, secondResolved, secondEndpointVertexId, outError)) {
        return false;
    }
    if (!ResolveEndpointVertexAfterSplits(candidate, firstResolved.point, firstEndpointVertexId, outError)
            || !ResolveEndpointVertexAfterSplits(candidate, secondResolved.point, secondEndpointVertexId, outError)) {
        return false;
    }
    if (firstEndpointVertexId == secondEndpointVertexId) {
        SetError(outError, "Cut endpoints must resolve to different vertices.");
        return false;
    }

    SectorTopologyLoopSet splitLoops;
    std::vector<SectorTopologyValidationIssue> splitLoopIssues;
    if (!ExtractSectorTopologyLoops(candidate, sectorId, splitLoops, &splitLoopIssues)) {
        SetError(outError, FirstValidationError(splitLoopIssues));
        return false;
    }

    const int firstIndex = FindVertexIndexInLoop(splitLoops.outer, firstEndpointVertexId);
    const int secondIndex = FindVertexIndexInLoop(splitLoops.outer, secondEndpointVertexId);
    if (firstIndex < 0 || secondIndex < 0) {
        SetError(outError, "Cut endpoints must lie on the selected sector outer boundary.");
        return false;
    }

    const std::vector<int> firstPath = BuildLoopVertexPath(splitLoops.outer, firstIndex, secondIndex);
    const std::vector<int> secondPath = BuildLoopVertexPath(splitLoops.outer, secondIndex, firstIndex);
    if (firstPath.size() < 3 || secondPath.size() < 3) {
        SetError(outError, "Cut would create a sector with fewer than 3 vertices.");
        return false;
    }

    if (!ValidateCutSegmentAgainstExistingTopology(
                candidate,
                firstEndpointVertexId,
                secondEndpointVertexId,
                outError)) {
        return false;
    }
    if (!ValidateCutInteriorInsideSelectedSector(
                candidate,
                splitLoops,
                firstResolved.point,
                secondResolved.point,
                outError)) {
        return false;
    }

    std::vector<SectorTopologyCoordPoint> firstPolygon;
    std::vector<SectorTopologyCoordPoint> secondPolygon;
    if (!BuildPathPoints(candidate, firstPath, firstPolygon, outError)
            || !BuildPathPoints(candidate, secondPath, secondPolygon, outError)) {
        return false;
    }

    const int newSectorId = AllocateSectorTopologySectorId(candidate);
    if (!RequireAllocatedId(newSectorId, "sector", outError)) {
        return false;
    }
    const SectorTopologySector* candidateOriginalSector =
            FindSectorTopologySector(candidate, sectorId);
    if (candidateOriginalSector == nullptr) {
        SetError(outError, "Could not resolve selected sector data.");
        return false;
    }

    SectorTopologySector newSector = *candidateOriginalSector;
    newSector.id = newSectorId;
    newSector.name = GenerateSectorName(candidate);
    candidate.sectors.push_back(newSector);
    candidateOriginalSector = FindSectorTopologySector(candidate, sectorId);
    const SectorTopologySector* candidateNewSector =
            FindSectorTopologySector(candidate, newSectorId);
    if (candidateOriginalSector == nullptr || candidateNewSector == nullptr) {
        SetError(outError, "Could not resolve cut sector data.");
        return false;
    }

    if (!AssignPathSideDefsToSector(candidate, splitLoops.outer, firstPath, sectorId, sectorId, outError)
            || !AssignPathSideDefsToSector(candidate, splitLoops.outer, secondPath, sectorId, newSectorId, outError)) {
        return false;
    }

    const SectorTopologyCoordPoint cutA = firstResolved.point;
    const SectorTopologyCoordPoint cutB = secondResolved.point;
    for (const SectorTopologyLoop& hole : splitLoops.holes) {
        if (!AssignHoleLoopToResultSector(
                    candidate,
                    hole,
                    firstPolygon,
                    secondPolygon,
                    cutA,
                    cutB,
                    sectorId,
                    newSectorId,
                    outError)) {
            return false;
        }
    }

    SectorTopologyCutSectorResult result;
    result.originalSectorId = sectorId;
    result.newSectorId = newSectorId;
    result.firstEndpointVertexId = firstEndpointVertexId;
    result.secondEndpointVertexId = secondEndpointVertexId;

    result.cutLineDefId = AllocateSectorTopologyLineDefId(candidate);
    if (!RequireAllocatedId(result.cutLineDefId, "linedef", outError)) {
        return false;
    }
    result.originalSectorSideDefId = AllocateSectorTopologySideDefId(candidate);
    if (!RequireAllocatedId(result.originalSectorSideDefId, "sidedef", outError)) {
        return false;
    }
    candidate.sideDefs.push_back(MakeSideDefFromSectorDefaults(
            result.originalSectorSideDefId,
            result.cutLineDefId,
            SectorTopologySideKind::Front,
            sectorId,
            *candidateOriginalSector));

    result.newSectorSideDefId = AllocateSectorTopologySideDefId(candidate);
    if (!RequireAllocatedId(result.newSectorSideDefId, "sidedef", outError)) {
        return false;
    }
    candidate.sideDefs.push_back(MakeSideDefFromSectorDefaults(
            result.newSectorSideDefId,
            result.cutLineDefId,
            SectorTopologySideKind::Back,
            newSectorId,
            *candidateNewSector));
    candidate.lineDefs.push_back(SectorTopologyLineDef{
            result.cutLineDefId,
            secondEndpointVertexId,
            firstEndpointVertexId,
            result.originalSectorSideDefId,
            result.newSectorSideDefId
    });

    if (!ValidateNoDuplicatePhysicalLineDefs(candidate, outError)) {
        return false;
    }

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(candidate);
    if (HasSectorTopologyValidationErrors(issues)) {
        SetError(outError, FirstValidationError(issues));
        return false;
    }

    SectorTopologyLoopSet firstResultLoops;
    std::vector<SectorTopologyValidationIssue> firstLoopIssues;
    if (!ExtractSectorTopologyLoops(candidate, sectorId, firstResultLoops, &firstLoopIssues)) {
        SetError(outError, FirstValidationError(firstLoopIssues));
        return false;
    }
    SectorTopologyLoopSet secondResultLoops;
    std::vector<SectorTopologyValidationIssue> secondLoopIssues;
    if (!ExtractSectorTopologyLoops(candidate, newSectorId, secondResultLoops, &secondLoopIssues)) {
        SetError(outError, FirstValidationError(secondLoopIssues));
        return false;
    }

    map = std::move(candidate);
    if (outResult != nullptr) {
        *outResult = result;
    }
    if (outError != nullptr) {
        outError->clear();
    }
    return true;
}

} // namespace game
