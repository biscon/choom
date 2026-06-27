#include "sector_demo/SectorAuthoringGraph.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <utility>

namespace game {
namespace {

template<typename T>
int AllocateNextId(const std::vector<T>& values)
{
    int maxId = 0;
    for (const T& value : values) {
        if (value.id > maxId) {
            maxId = value.id;
        }
    }

    if (maxId == std::numeric_limits<int>::max()) {
        return -1;
    }
    return maxId + 1;
}

template<typename T>
const T* FindById(const std::vector<T>& values, int id)
{
    if (!IsValidSectorAuthoringId(id)) {
        return nullptr;
    }

    for (const T& value : values) {
        if (value.id == id) {
            return &value;
        }
    }
    return nullptr;
}

template<typename T>
T* FindById(std::vector<T>& values, int id)
{
    if (!IsValidSectorAuthoringId(id)) {
        return nullptr;
    }

    for (T& value : values) {
        if (value.id == id) {
            return &value;
        }
    }
    return nullptr;
}

void AddIssue(
        std::vector<SectorAuthoringValidationIssue>& issues,
        SectorAuthoringObjectKind objectKind,
        int objectId,
        std::string message)
{
    SectorAuthoringValidationIssue issue;
    issue.objectKind = objectKind;
    issue.objectId = objectId;
    issue.message = std::move(message);
    issues.push_back(std::move(issue));
}

void AddPlanarDiagnostic(
        std::vector<SectorAuthoringPlanarDiagnostic>& diagnostics,
        SectorAuthoringPlanarDiagnosticKind kind,
        int lineId,
        int otherLineId,
        std::string message,
        SectorAuthoringValidationSeverity severity = SectorAuthoringValidationSeverity::Error)
{
    SectorAuthoringPlanarDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.kind = kind;
    diagnostic.lineId = lineId;
    diagnostic.otherLineId = otherLineId;
    diagnostic.message = std::move(message);
    diagnostics.push_back(std::move(diagnostic));
}

void AddFaceDiagnostic(
        std::vector<SectorAuthoringFaceDiagnostic>& diagnostics,
        SectorAuthoringFaceDiagnosticKind kind,
        int planarEdgeId,
        int vertexId,
        std::string message,
        SectorAuthoringValidationSeverity severity = SectorAuthoringValidationSeverity::Error)
{
    SectorAuthoringFaceDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.kind = kind;
    diagnostic.planarEdgeId = planarEdgeId;
    diagnostic.vertexId = vertexId;
    diagnostic.message = std::move(message);
    diagnostics.push_back(std::move(diagnostic));
}

int64_t Cross(
        int64_t ax,
        int64_t ay,
        int64_t bx,
        int64_t by)
{
    return ax * by - ay * bx;
}

SectorAuthoringPlanarRational MakeRational(int64_t numerator, int64_t denominator)
{
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }

    if (denominator == 0) {
        return SectorAuthoringPlanarRational{0, 1};
    }

    const int64_t divisor = std::gcd(
            numerator < 0 ? -numerator : numerator,
            denominator < 0 ? -denominator : denominator);
    if (divisor == 0) {
        return SectorAuthoringPlanarRational{0, 1};
    }
    return SectorAuthoringPlanarRational{numerator / divisor, denominator / divisor};
}

SectorAuthoringPlanarRational MakeIntegerRational(int64_t value)
{
    return SectorAuthoringPlanarRational{value, 1};
}

bool LessRational(SectorAuthoringPlanarRational lhs, SectorAuthoringPlanarRational rhs)
{
    return static_cast<__int128>(lhs.numerator) * rhs.denominator
            < static_cast<__int128>(rhs.numerator) * lhs.denominator;
}

bool LessOrEqualRational(SectorAuthoringPlanarRational lhs, SectorAuthoringPlanarRational rhs)
{
    return SectorAuthoringPlanarRationalsEqual(lhs, rhs) || LessRational(lhs, rhs);
}

SectorAuthoringPlanarPoint MakeLinePointAt(
        const SectorAuthoringVertex& start,
        const SectorAuthoringVertex& end,
        SectorAuthoringPlanarRational t)
{
    const int64_t dx = static_cast<int64_t>(end.x) - start.x;
    const int64_t dy = static_cast<int64_t>(end.y) - start.y;
    SectorAuthoringPlanarPoint point;
    point.x = MakeRational(
            static_cast<int64_t>(start.x) * t.denominator + dx * t.numerator,
            t.denominator);
    point.y = MakeRational(
            static_cast<int64_t>(start.y) * t.denominator + dy * t.numerator,
            t.denominator);
    return point;
}

SectorAuthoringPlanarPoint MakeVertexPoint(const SectorAuthoringVertex& vertex)
{
    return SectorAuthoringPlanarPoint{
            MakeIntegerRational(vertex.x),
            MakeIntegerRational(vertex.y)};
}

bool PlanarPointLess(
        const SectorAuthoringPlanarPoint& lhs,
        const SectorAuthoringPlanarPoint& rhs)
{
    if (!SectorAuthoringPlanarRationalsEqual(lhs.x, rhs.x)) {
        return LessRational(lhs.x, rhs.x);
    }
    return LessRational(lhs.y, rhs.y);
}

double PlanarRationalToDouble(SectorAuthoringPlanarRational value)
{
    if (value.denominator == 0) {
        return 0.0;
    }
    return static_cast<double>(value.numerator) / static_cast<double>(value.denominator);
}

double PlanarPointX(const SectorAuthoringPlanarPoint& point)
{
    return PlanarRationalToDouble(point.x);
}

double PlanarPointY(const SectorAuthoringPlanarPoint& point)
{
    return PlanarRationalToDouble(point.y);
}

struct PlanarSourceLine {
    const SectorAuthoringLine* line = nullptr;
    const SectorAuthoringVertex* start = nullptr;
    const SectorAuthoringVertex* end = nullptr;
};

struct PlanarSplitPoint {
    SectorAuthoringPlanarRational t;
    SectorAuthoringPlanarPoint point;
    int sourceVertexId = -1;
};

struct FaceHalfEdge {
    int id = -1;
    int twinId = -1;
    int planarEdgeIndex = -1;
    int startVertexId = -1;
    int endVertexId = -1;
    int sourceLineId = -1;
    int componentId = -1;
    bool followsSourceLineDirection = true;
    double angle = 0.0;
};

struct CandidateExtractedFace {
    SectorAuthoringExtractedFace face;
    int componentId = -1;
};

bool SamePhysicalSegment(const PlanarSourceLine& lhs, const PlanarSourceLine& rhs)
{
    return (lhs.start->x == rhs.start->x && lhs.start->y == rhs.start->y
                   && lhs.end->x == rhs.end->x && lhs.end->y == rhs.end->y)
            || (lhs.start->x == rhs.end->x && lhs.start->y == rhs.end->y
                   && lhs.end->x == rhs.start->x && lhs.end->y == rhs.start->y);
}

bool CoordinatesEqual(const SectorAuthoringVertex& lhs, const SectorAuthoringVertex& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

bool PointOnSegmentInclusive(
        const SectorAuthoringVertex& point,
        const SectorAuthoringVertex& start,
        const SectorAuthoringVertex& end)
{
    const int64_t dx = static_cast<int64_t>(end.x) - start.x;
    const int64_t dy = static_cast<int64_t>(end.y) - start.y;
    const int64_t px = static_cast<int64_t>(point.x) - start.x;
    const int64_t py = static_cast<int64_t>(point.y) - start.y;
    if (Cross(dx, dy, px, py) != 0) {
        return false;
    }
    return std::min(start.x, end.x) <= point.x && point.x <= std::max(start.x, end.x)
            && std::min(start.y, end.y) <= point.y && point.y <= std::max(start.y, end.y);
}

bool CollinearSegmentsOverlapBeyondEndpoint(
        const PlanarSourceLine& lhs,
        const PlanarSourceLine& rhs)
{
    const bool useX = lhs.start->x != lhs.end->x;
    const int64_t a0 = useX ? lhs.start->x : lhs.start->y;
    const int64_t a1 = useX ? lhs.end->x : lhs.end->y;
    const int64_t b0 = useX ? rhs.start->x : rhs.start->y;
    const int64_t b1 = useX ? rhs.end->x : rhs.end->y;
    const int64_t minA = std::min(a0, a1);
    const int64_t maxA = std::max(a0, a1);
    const int64_t minB = std::min(b0, b1);
    const int64_t maxB = std::max(b0, b1);
    return std::max(minA, minB) < std::min(maxA, maxB);
}

bool EndpointCoordinatesCoincideWithDifferentVertices(
        const PlanarSourceLine& lhs,
        const PlanarSourceLine& rhs)
{
    const std::pair<const SectorAuthoringVertex*, int> lhsEndpoints[] = {
            {lhs.start, lhs.line->startVertexId},
            {lhs.end, lhs.line->endVertexId}};
    const std::pair<const SectorAuthoringVertex*, int> rhsEndpoints[] = {
            {rhs.start, rhs.line->startVertexId},
            {rhs.end, rhs.line->endVertexId}};
    for (const auto& lhsEndpoint : lhsEndpoints) {
        for (const auto& rhsEndpoint : rhsEndpoints) {
            if (lhsEndpoint.second != rhsEndpoint.second
                    && CoordinatesEqual(*lhsEndpoint.first, *rhsEndpoint.first)) {
                return true;
            }
        }
    }
    return false;
}

int EndpointSourceVertexIdAt(
        const PlanarSourceLine& sourceLine,
        SectorAuthoringPlanarRational t)
{
    if (SectorAuthoringPlanarRationalsEqual(t, MakeIntegerRational(0))) {
        return sourceLine.line->startVertexId;
    }
    if (SectorAuthoringPlanarRationalsEqual(t, MakeIntegerRational(1))) {
        return sourceLine.line->endVertexId;
    }
    return -1;
}

double DistanceSquaredPointToSegmentInterior(
        const SectorAuthoringVertex& point,
        const SectorAuthoringVertex& start,
        const SectorAuthoringVertex& end,
        bool* projectsInside)
{
    const double vx = static_cast<double>(end.x) - start.x;
    const double vy = static_cast<double>(end.y) - start.y;
    const double wx = static_cast<double>(point.x) - start.x;
    const double wy = static_cast<double>(point.y) - start.y;
    const double lengthSquared = vx * vx + vy * vy;
    if (lengthSquared <= 0.0) {
        *projectsInside = false;
        return std::numeric_limits<double>::max();
    }

    const double t = (wx * vx + wy * vy) / lengthSquared;
    *projectsInside = t > 0.0 && t < 1.0;
    const double closestX = static_cast<double>(start.x) + t * vx;
    const double closestY = static_cast<double>(start.y) + t * vy;
    const double dx = static_cast<double>(point.x) - closestX;
    const double dy = static_cast<double>(point.y) - closestY;
    return dx * dx + dy * dy;
}

void AddNearMissDiagnostics(
        const PlanarSourceLine& lhs,
        const PlanarSourceLine& rhs,
        std::vector<SectorAuthoringPlanarDiagnostic>& diagnostics)
{
    constexpr double nearMissDistanceSquared = 1.0;
    const std::pair<const SectorAuthoringVertex*, const PlanarSourceLine*> checks[] = {
            {lhs.start, &rhs},
            {lhs.end, &rhs},
            {rhs.start, &lhs},
            {rhs.end, &lhs}};

    for (const auto& check : checks) {
        bool projectsInside = false;
        const double distanceSquared = DistanceSquaredPointToSegmentInterior(
                *check.first,
                *check.second->start,
                *check.second->end,
                &projectsInside);
        if (projectsInside && distanceSquared > 0.0 && distanceSquared <= nearMissDistanceSquared) {
            AddPlanarDiagnostic(
                    diagnostics,
                    SectorAuthoringPlanarDiagnosticKind::NearMiss,
                    lhs.line->id,
                    rhs.line->id,
                    "Authoring line endpoint passes within one grid coordinate of another line");
            return;
        }
    }
}

void AddUniqueSplitPoint(
        std::vector<PlanarSplitPoint>& splitPoints,
        PlanarSplitPoint splitPoint)
{
    for (PlanarSplitPoint& existing : splitPoints) {
        if (SectorAuthoringPlanarPointsEqual(existing.point, splitPoint.point)) {
            if (!IsValidSectorAuthoringId(existing.sourceVertexId)) {
                existing.sourceVertexId = splitPoint.sourceVertexId;
            }
            return;
        }
    }
    splitPoints.push_back(splitPoint);
}

int FindOrAddPlanarVertex(
        SectorAuthoringPlanarizationResult& result,
        const SectorAuthoringPlanarPoint& point,
        int sourceVertexId)
{
    for (SectorAuthoringPlanarVertex& vertex : result.vertices) {
        if (SectorAuthoringPlanarPointsEqual(vertex.point, point)) {
            if (!IsValidSectorAuthoringId(vertex.sourceVertexId)) {
                vertex.sourceVertexId = sourceVertexId;
            }
            return vertex.id;
        }
    }

    SectorAuthoringPlanarVertex vertex;
    vertex.id = static_cast<int>(result.vertices.size()) + 1;
    vertex.point = point;
    vertex.sourceVertexId = sourceVertexId;
    result.vertices.push_back(vertex);
    return vertex.id;
}

void CopySectorPropertiesToFaceAnchor(
        const SectorTopologySector& sector,
        SectorAuthoringFaceAnchor& anchor)
{
    anchor.id = sector.id;
    anchor.name = sector.name;
    anchor.floorZ = sector.floorZ;
    anchor.ceilingZ = sector.ceilingZ;
    anchor.floorTextureId = sector.floorTextureId;
    anchor.ceilingTextureId = sector.ceilingTextureId;
    anchor.ceilingSky = sector.ceilingSky;
    anchor.floorUv = sector.floorUv;
    anchor.ceilingUv = sector.ceilingUv;
    anchor.floorDecal = sector.floorDecal;
    anchor.ceilingDecal = sector.ceilingDecal;
    anchor.ambientColor = sector.ambientColor;
    anchor.ambientIntensity = sector.ambientIntensity;
    anchor.defaultWall = sector.defaultWall;
    anchor.defaultLower = sector.defaultLower;
    anchor.defaultUpper = sector.defaultUpper;
}

void SetFaceAnchorAveragePosition(
        const SectorTopologyMap& map,
        const SectorTopologySector& sector,
        SectorAuthoringFaceAnchor& anchor)
{
    int64_t xSum = 0;
    int64_t ySum = 0;
    int vertexCount = 0;

    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        if (sideDef.sectorId != sector.id) {
            continue;
        }

        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef.lineDefId);
        if (lineDef == nullptr) {
            continue;
        }

        const SectorTopologyVertex* start = FindSectorTopologyVertex(map, lineDef->startVertexId);
        const SectorTopologyVertex* end = FindSectorTopologyVertex(map, lineDef->endVertexId);
        const SectorTopologyVertex* sideStart = sideDef.side == SectorTopologySideKind::Front ? start : end;
        const SectorTopologyVertex* sideEnd = sideDef.side == SectorTopologySideKind::Front ? end : start;
        if (sideStart == nullptr || sideEnd == nullptr) {
            continue;
        }

        xSum += sideStart->x + sideEnd->x;
        ySum += sideStart->y + sideEnd->y;
        vertexCount += 2;
    }

    if (vertexCount > 0) {
        anchor.x = static_cast<SectorCoord>(xSum / vertexCount);
        anchor.y = static_cast<SectorCoord>(ySum / vertexCount);
    }
}

const SectorAuthoringPlanarVertex* FindPlanarVertex(
        const SectorAuthoringPlanarizationResult& planar,
        int id)
{
    for (const SectorAuthoringPlanarVertex& vertex : planar.vertices) {
        if (vertex.id == id) {
            return &vertex;
        }
    }
    return nullptr;
}

const FaceHalfEdge* FindHalfEdge(const std::vector<FaceHalfEdge>& halfEdges, int id)
{
    if (id <= 0 || id > static_cast<int>(halfEdges.size())) {
        return nullptr;
    }
    return &halfEdges[static_cast<std::size_t>(id - 1)];
}

std::vector<int> BuildFaceHalfEdges(
        const SectorAuthoringPlanarizationResult& planar,
        std::vector<FaceHalfEdge>& halfEdges,
        SectorAuthoringFaceExtractionResult& result)
{
    std::set<std::pair<int, int>> undirectedEdges;
    std::vector<int> validPlanarEdgeIds;
    halfEdges.reserve(planar.edges.size() * 2);

    for (std::size_t edgeIndex = 0; edgeIndex < planar.edges.size(); ++edgeIndex) {
        const SectorAuthoringPlanarEdge& edge = planar.edges[edgeIndex];
        const SectorAuthoringPlanarVertex* start = FindPlanarVertex(planar, edge.startVertexId);
        const SectorAuthoringPlanarVertex* end = FindPlanarVertex(planar, edge.endVertexId);
        if (start == nullptr || end == nullptr) {
            AddFaceDiagnostic(
                    result.diagnostics,
                    SectorAuthoringFaceDiagnosticKind::MissingVertex,
                    edge.id,
                    start == nullptr ? edge.startVertexId : edge.endVertexId,
                    "Planar edge references a missing vertex");
            continue;
        }
        if (edge.startVertexId == edge.endVertexId || SectorAuthoringPlanarPointsEqual(start->point, end->point)) {
            AddFaceDiagnostic(
                    result.diagnostics,
                    SectorAuthoringFaceDiagnosticKind::DanglingEdge,
                    edge.id,
                    edge.startVertexId,
                    "Planar edge has zero face-walk length");
            continue;
        }

        const std::pair<int, int> undirectedKey{
                std::min(edge.startVertexId, edge.endVertexId),
                std::max(edge.startVertexId, edge.endVertexId)};
        if (!undirectedEdges.insert(undirectedKey).second) {
            AddFaceDiagnostic(
                    result.diagnostics,
                    SectorAuthoringFaceDiagnosticKind::DuplicateEdge,
                    edge.id,
                    -1,
                    "Planar graph contains duplicate edge endpoints");
            continue;
        }

        const double dx = PlanarPointX(end->point) - PlanarPointX(start->point);
        const double dy = PlanarPointY(end->point) - PlanarPointY(start->point);
        const int forwardId = static_cast<int>(halfEdges.size()) + 1;
        const int reverseId = forwardId + 1;

        FaceHalfEdge forward;
        forward.id = forwardId;
        forward.twinId = reverseId;
        forward.planarEdgeIndex = static_cast<int>(edgeIndex);
        forward.startVertexId = edge.startVertexId;
        forward.endVertexId = edge.endVertexId;
        forward.sourceLineId = edge.sourceLineId;
        forward.followsSourceLineDirection = edge.followsSourceLineDirection;
        forward.angle = std::atan2(dy, dx);
        halfEdges.push_back(forward);

        FaceHalfEdge reverse;
        reverse.id = reverseId;
        reverse.twinId = forwardId;
        reverse.planarEdgeIndex = static_cast<int>(edgeIndex);
        reverse.startVertexId = edge.endVertexId;
        reverse.endVertexId = edge.startVertexId;
        reverse.sourceLineId = edge.sourceLineId;
        reverse.followsSourceLineDirection = !edge.followsSourceLineDirection;
        reverse.angle = std::atan2(-dy, -dx);
        halfEdges.push_back(reverse);
        validPlanarEdgeIds.push_back(edge.id);
    }

    return validPlanarEdgeIds;
}

std::vector<std::vector<int>> BuildSortedOutgoingHalfEdges(
        const std::vector<FaceHalfEdge>& halfEdges,
        int maxVertexId)
{
    std::vector<std::vector<int>> outgoing(static_cast<std::size_t>(maxVertexId) + 1);
    for (const FaceHalfEdge& halfEdge : halfEdges) {
        if (halfEdge.startVertexId > 0 && halfEdge.startVertexId <= maxVertexId) {
            outgoing[static_cast<std::size_t>(halfEdge.startVertexId)].push_back(halfEdge.id);
        }
    }

    for (std::vector<int>& vertexOutgoing : outgoing) {
        std::sort(
                vertexOutgoing.begin(),
                vertexOutgoing.end(),
                [&halfEdges](int lhsId, int rhsId) {
                    const FaceHalfEdge& lhs = halfEdges[static_cast<std::size_t>(lhsId - 1)];
                    const FaceHalfEdge& rhs = halfEdges[static_cast<std::size_t>(rhsId - 1)];
                    if (lhs.angle != rhs.angle) {
                        return lhs.angle < rhs.angle;
                    }
                    return lhs.id < rhs.id;
                });
    }

    return outgoing;
}

void AssignFaceHalfEdgeComponents(
        std::vector<FaceHalfEdge>& halfEdges,
        const std::vector<std::vector<int>>& outgoing)
{
    int nextComponentId = 1;
    std::vector<int> stack;
    for (FaceHalfEdge& startHalfEdge : halfEdges) {
        if (startHalfEdge.componentId > 0) {
            continue;
        }

        startHalfEdge.componentId = nextComponentId;
        stack.clear();
        stack.push_back(startHalfEdge.id);
        while (!stack.empty()) {
            const int halfEdgeId = stack.back();
            stack.pop_back();
            FaceHalfEdge* halfEdge = halfEdgeId > 0 && halfEdgeId <= static_cast<int>(halfEdges.size())
                    ? &halfEdges[static_cast<std::size_t>(halfEdgeId - 1)]
                    : nullptr;
            if (halfEdge == nullptr) {
                continue;
            }

            const int twinId = halfEdge->twinId;
            if (twinId > 0 && twinId <= static_cast<int>(halfEdges.size())) {
                FaceHalfEdge& twin = halfEdges[static_cast<std::size_t>(twinId - 1)];
                if (twin.componentId <= 0) {
                    twin.componentId = nextComponentId;
                    stack.push_back(twin.id);
                }
            }

            if (halfEdge->startVertexId > 0 && halfEdge->startVertexId < static_cast<int>(outgoing.size())) {
                for (int connectedId : outgoing[static_cast<std::size_t>(halfEdge->startVertexId)]) {
                    FaceHalfEdge& connected = halfEdges[static_cast<std::size_t>(connectedId - 1)];
                    if (connected.componentId <= 0) {
                        connected.componentId = nextComponentId;
                        stack.push_back(connected.id);
                    }
                }
            }
            if (halfEdge->endVertexId > 0 && halfEdge->endVertexId < static_cast<int>(outgoing.size())) {
                for (int connectedId : outgoing[static_cast<std::size_t>(halfEdge->endVertexId)]) {
                    FaceHalfEdge& connected = halfEdges[static_cast<std::size_t>(connectedId - 1)];
                    if (connected.componentId <= 0) {
                        connected.componentId = nextComponentId;
                        stack.push_back(connected.id);
                    }
                }
            }
        }

        ++nextComponentId;
    }
}

int FindNextFaceHalfEdge(
        const std::vector<FaceHalfEdge>& halfEdges,
        const std::vector<std::vector<int>>& outgoing,
        const FaceHalfEdge& current)
{
    const FaceHalfEdge* twin = FindHalfEdge(halfEdges, current.twinId);
    if (twin == nullptr || current.endVertexId <= 0
            || current.endVertexId >= static_cast<int>(outgoing.size())) {
        return -1;
    }

    const std::vector<int>& vertexOutgoing = outgoing[static_cast<std::size_t>(current.endVertexId)];
    const auto reverseIt = std::find(vertexOutgoing.begin(), vertexOutgoing.end(), twin->id);
    if (reverseIt == vertexOutgoing.end() || vertexOutgoing.empty()) {
        return -1;
    }

    const std::size_t reverseIndex = static_cast<std::size_t>(reverseIt - vertexOutgoing.begin());
    const std::size_t nextIndex = reverseIndex == 0 ? vertexOutgoing.size() - 1 : reverseIndex - 1;
    return vertexOutgoing[nextIndex];
}

double ComputeFaceSignedArea(
        const SectorAuthoringPlanarizationResult& planar,
        const std::vector<int>& loopHalfEdgeIds,
        const std::vector<FaceHalfEdge>& halfEdges)
{
    double doubledArea = 0.0;
    for (int halfEdgeId : loopHalfEdgeIds) {
        const FaceHalfEdge* halfEdge = FindHalfEdge(halfEdges, halfEdgeId);
        if (halfEdge == nullptr) {
            continue;
        }

        const SectorAuthoringPlanarVertex* start = FindPlanarVertex(planar, halfEdge->startVertexId);
        const SectorAuthoringPlanarVertex* end = FindPlanarVertex(planar, halfEdge->endVertexId);
        if (start == nullptr || end == nullptr) {
            continue;
        }

        doubledArea += PlanarPointX(start->point) * PlanarPointY(end->point)
                - PlanarPointY(start->point) * PlanarPointX(end->point);
    }

    return doubledArea * 0.5;
}

SectorAuthoringFaceBoundaryEdge MakeFaceBoundaryEdge(
        const SectorAuthoringPlanarizationResult& planar,
        const FaceHalfEdge& halfEdge)
{
    const SectorAuthoringPlanarEdge& edge =
            planar.edges[static_cast<std::size_t>(halfEdge.planarEdgeIndex)];

    SectorAuthoringFaceBoundaryEdge boundaryEdge;
    boundaryEdge.planarEdgeId = edge.id;
    boundaryEdge.startVertexId = halfEdge.startVertexId;
    boundaryEdge.endVertexId = halfEdge.endVertexId;
    boundaryEdge.sourceLineId = halfEdge.sourceLineId;
    boundaryEdge.sourceSide = halfEdge.followsSourceLineDirection
            ? SectorTopologySideKind::Front
            : SectorTopologySideKind::Back;
    return boundaryEdge;
}

bool HasUnsupportedDisconnectedClosedFaces(
        const std::vector<CandidateExtractedFace>& candidateFaces,
        SectorAuthoringFaceExtractionResult& result)
{
    if (candidateFaces.empty()) {
        return false;
    }

    std::set<int> faceComponentIds;
    for (const CandidateExtractedFace& candidate : candidateFaces) {
        faceComponentIds.insert(candidate.componentId);
    }
    if (faceComponentIds.size() <= 1) {
        return false;
    }

    const SectorAuthoringExtractedFace& firstFace = candidateFaces.front().face;
    const int firstPlanarEdgeId = firstFace.boundary.empty() ? -1 : firstFace.boundary.front().planarEdgeId;
    const int firstVertexId = firstFace.boundary.empty() ? -1 : firstFace.boundary.front().startVertexId;
    AddFaceDiagnostic(
            result.diagnostics,
            SectorAuthoringFaceDiagnosticKind::AmbiguousTopology,
            firstPlanarEdgeId,
            firstVertexId,
            "Disconnected closed loops and hole candidates are deferred for a later authoring-graph phase");
    return true;
}

void AddDerivationDiagnostic(
        std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics,
        SectorAuthoringDerivationDiagnosticKind kind,
        int objectId,
        std::string message,
        SectorAuthoringValidationSeverity severity = SectorAuthoringValidationSeverity::Error)
{
    SectorAuthoringDerivationDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.kind = kind;
    diagnostic.objectId = objectId;
    diagnostic.message = std::move(message);
    diagnostics.push_back(std::move(diagnostic));
}

bool HasDerivationErrors(const std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics)
{
    return std::any_of(
            diagnostics.begin(),
            diagnostics.end(),
            [](const SectorAuthoringDerivationDiagnostic& diagnostic) {
                return diagnostic.severity == SectorAuthoringValidationSeverity::Error;
            });
}

int AllocateGeneratedTopologyVertexId(
        const std::set<int>& usedVertexIds,
        int* nextGeneratedVertexId)
{
    while (*nextGeneratedVertexId > 0 && usedVertexIds.find(*nextGeneratedVertexId) != usedVertexIds.end()) {
        ++(*nextGeneratedVertexId);
    }
    return *nextGeneratedVertexId;
}

bool BoundaryUsesPlanarEdge(
        const SectorAuthoringFaceExtractionResult& faces,
        int planarEdgeId)
{
    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        for (const SectorAuthoringFaceBoundaryEdge& boundaryEdge : face.boundary) {
            if (boundaryEdge.planarEdgeId == planarEdgeId) {
                return true;
            }
        }
    }
    return false;
}

void CopyDerivationVertexMappingToTopology(
        const SectorAuthoringPlanarizationResult& planar,
        SectorTopologyMap& topology,
        SectorAuthoringDerivationMapping& mapping,
        std::map<int, int>& topologyVertexIdsByPlanarVertexId,
        std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics)
{
    std::set<int> usedVertexIds;
    for (const SectorAuthoringPlanarVertex& vertex : planar.vertices) {
        if (IsValidSectorAuthoringId(vertex.sourceVertexId)) {
            usedVertexIds.insert(vertex.sourceVertexId);
        }
    }

    int maxSourceVertexId = 0;
    for (int id : usedVertexIds) {
        maxSourceVertexId = std::max(maxSourceVertexId, id);
    }
    int nextGeneratedVertexId = maxSourceVertexId + 1;

    topology.vertices.reserve(planar.vertices.size());
    mapping.vertices.reserve(planar.vertices.size());
    for (const SectorAuthoringPlanarVertex& planarVertex : planar.vertices) {
        if (!SectorAuthoringPlanarRationalIsInteger(planarVertex.point.x)
                || !SectorAuthoringPlanarRationalIsInteger(planarVertex.point.y)) {
            AddDerivationDiagnostic(
                    diagnostics,
                    SectorAuthoringDerivationDiagnosticKind::NonIntegerVertex,
                    planarVertex.id,
                    "Planarized vertex does not land on the exact sector grid");
            continue;
        }

        int topologyVertexId = planarVertex.sourceVertexId;
        if (!IsValidSectorAuthoringId(topologyVertexId)) {
            topologyVertexId = AllocateGeneratedTopologyVertexId(usedVertexIds, &nextGeneratedVertexId);
            usedVertexIds.insert(topologyVertexId);
        }

        SectorTopologyVertex vertex;
        vertex.id = topologyVertexId;
        vertex.x = SectorAuthoringPlanarRationalToSectorCoord(planarVertex.point.x);
        vertex.y = SectorAuthoringPlanarRationalToSectorCoord(planarVertex.point.y);
        topology.vertices.push_back(vertex);
        topologyVertexIdsByPlanarVertexId[planarVertex.id] = topologyVertexId;

        SectorAuthoringDerivedVertexMapping vertexMapping;
        vertexMapping.planarVertexId = planarVertex.id;
        vertexMapping.authoringVertexId = planarVertex.sourceVertexId;
        vertexMapping.topologyVertexId = topologyVertexId;
        mapping.vertices.push_back(vertexMapping);
    }
}

void CopyFaceDefaultsToTopologySector(
        const SectorAuthoringExtractedFace& face,
        SectorTopologySector& sector)
{
    sector.id = face.id;
    sector.name = "Sector " + std::to_string(face.id);
}

void BuildDerivedTopologyFacesAndLines(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringFaceExtractionResult& faces,
        const std::map<int, int>& topologyVertexIdsByPlanarVertexId,
        SectorTopologyMap& topology,
        SectorAuthoringDerivationMapping& mapping)
{
    std::map<int, int> topologyLineIdsByPlanarEdgeId;
    int nextLineDefId = 1;
    int nextSideDefId = 1;

    for (const SectorAuthoringPlanarEdge& planarEdge : planar.edges) {
        if (!BoundaryUsesPlanarEdge(faces, planarEdge.id)) {
            continue;
        }

        const auto startIt = topologyVertexIdsByPlanarVertexId.find(planarEdge.startVertexId);
        const auto endIt = topologyVertexIdsByPlanarVertexId.find(planarEdge.endVertexId);
        if (startIt == topologyVertexIdsByPlanarVertexId.end()
                || endIt == topologyVertexIdsByPlanarVertexId.end()) {
            continue;
        }

        SectorTopologyLineDef lineDef;
        lineDef.id = nextLineDefId++;
        lineDef.startVertexId = startIt->second;
        lineDef.endVertexId = endIt->second;
        if (const SectorAuthoringLine* sourceLine = FindSectorAuthoringLine(graph, planarEdge.sourceLineId)) {
            lineDef.flags = sourceLine->flags;
        }
        topology.lineDefs.push_back(lineDef);
        topologyLineIdsByPlanarEdgeId[planarEdge.id] = lineDef.id;

        SectorAuthoringDerivedLineMapping lineMapping;
        lineMapping.planarEdgeId = planarEdge.id;
        lineMapping.authoringLineId = planarEdge.sourceLineId;
        lineMapping.topologyLineDefId = lineDef.id;
        mapping.lines.push_back(lineMapping);
    }

    topology.sectors.reserve(faces.faces.size());
    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        SectorTopologySector sector;
        CopyFaceDefaultsToTopologySector(face, sector);
        topology.sectors.push_back(sector);

        SectorAuthoringDerivedSectorMapping sectorMapping;
        sectorMapping.extractedFaceId = face.id;
        sectorMapping.topologySectorId = sector.id;
        mapping.sectors.push_back(sectorMapping);
    }

    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        for (const SectorAuthoringFaceBoundaryEdge& boundaryEdge : face.boundary) {
            const auto lineIt = topologyLineIdsByPlanarEdgeId.find(boundaryEdge.planarEdgeId);
            if (lineIt == topologyLineIdsByPlanarEdgeId.end()) {
                continue;
            }

            SectorTopologySideDef sideDef;
            sideDef.id = nextSideDefId++;
            sideDef.lineDefId = lineIt->second;
            sideDef.side = boundaryEdge.sourceSide;
            sideDef.sectorId = face.id;
            topology.sideDefs.push_back(sideDef);

            if (SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(topology, sideDef.lineDefId)) {
                if (sideDef.side == SectorTopologySideKind::Front) {
                    lineDef->frontSideDefId = sideDef.id;
                } else {
                    lineDef->backSideDefId = sideDef.id;
                }
            }

            SectorAuthoringDerivedSideMapping sideMapping;
            sideMapping.authoringLineId = boundaryEdge.sourceLineId;
            sideMapping.authoringSide = boundaryEdge.sourceSide;
            sideMapping.topologySideDefId = sideDef.id;
            mapping.sides.push_back(sideMapping);
        }
    }
}

} // namespace

bool IsValidSectorAuthoringId(int id)
{
    return id > 0;
}

int AllocateSectorAuthoringVertexId(const SectorAuthoringGraph& graph)
{
    return AllocateNextId(graph.vertices);
}

int AllocateSectorAuthoringLineId(const SectorAuthoringGraph& graph)
{
    return AllocateNextId(graph.lines);
}

int AllocateSectorAuthoringFaceAnchorId(const SectorAuthoringGraph& graph)
{
    return AllocateNextId(graph.faceAnchors);
}

const SectorAuthoringVertex* FindSectorAuthoringVertex(const SectorAuthoringGraph& graph, int id)
{
    return FindById(graph.vertices, id);
}

SectorAuthoringVertex* FindSectorAuthoringVertex(SectorAuthoringGraph& graph, int id)
{
    return FindById(graph.vertices, id);
}

const SectorAuthoringLine* FindSectorAuthoringLine(const SectorAuthoringGraph& graph, int id)
{
    return FindById(graph.lines, id);
}

SectorAuthoringLine* FindSectorAuthoringLine(SectorAuthoringGraph& graph, int id)
{
    return FindById(graph.lines, id);
}

const SectorAuthoringLineSide* FindSectorAuthoringLineSide(
        const SectorAuthoringGraph& graph,
        SectorAuthoringSideId id)
{
    if (!IsValidSectorAuthoringId(id.lineId)) {
        return nullptr;
    }

    for (const SectorAuthoringLineSide& side : graph.lineSides) {
        if (SectorAuthoringSideIdsEqual(side.id, id)) {
            return &side;
        }
    }
    return nullptr;
}

SectorAuthoringLineSide* FindSectorAuthoringLineSide(
        SectorAuthoringGraph& graph,
        SectorAuthoringSideId id)
{
    if (!IsValidSectorAuthoringId(id.lineId)) {
        return nullptr;
    }

    for (SectorAuthoringLineSide& side : graph.lineSides) {
        if (SectorAuthoringSideIdsEqual(side.id, id)) {
            return &side;
        }
    }
    return nullptr;
}

const SectorAuthoringFaceAnchor* FindSectorAuthoringFaceAnchor(
        const SectorAuthoringGraph& graph,
        int id)
{
    return FindById(graph.faceAnchors, id);
}

SectorAuthoringFaceAnchor* FindSectorAuthoringFaceAnchor(SectorAuthoringGraph& graph, int id)
{
    return FindById(graph.faceAnchors, id);
}

bool SectorAuthoringSideIdsEqual(SectorAuthoringSideId lhs, SectorAuthoringSideId rhs)
{
    return lhs.lineId == rhs.lineId && lhs.side == rhs.side;
}

SectorAuthoringSideId OppositeSectorAuthoringSideId(SectorAuthoringSideId id)
{
    id.side = id.side == SectorTopologySideKind::Front
            ? SectorTopologySideKind::Back
            : SectorTopologySideKind::Front;
    return id;
}

bool AddSectorAuthoringVertex(
        SectorAuthoringGraph& graph,
        SectorCoord x,
        SectorCoord y,
        int* outVertexId)
{
    const int id = AllocateSectorAuthoringVertexId(graph);
    if (!IsValidSectorAuthoringId(id)) {
        return false;
    }

    SectorAuthoringVertex vertex;
    vertex.id = id;
    vertex.x = x;
    vertex.y = y;
    graph.vertices.push_back(vertex);

    if (outVertexId != nullptr) {
        *outVertexId = id;
    }
    return true;
}

bool AddSectorAuthoringLine(
        SectorAuthoringGraph& graph,
        int startVertexId,
        int endVertexId,
        int* outLineId)
{
    if (FindSectorAuthoringVertex(graph, startVertexId) == nullptr
            || FindSectorAuthoringVertex(graph, endVertexId) == nullptr
            || startVertexId == endVertexId) {
        return false;
    }

    const int id = AllocateSectorAuthoringLineId(graph);
    if (!IsValidSectorAuthoringId(id)) {
        return false;
    }

    SectorAuthoringLine line;
    line.id = id;
    line.startVertexId = startVertexId;
    line.endVertexId = endVertexId;
    graph.lines.push_back(line);

    if (outLineId != nullptr) {
        *outLineId = id;
    }
    return true;
}

std::vector<SectorAuthoringValidationIssue> ValidateSectorAuthoringGraphReferences(
        const SectorAuthoringGraph& graph)
{
    std::vector<SectorAuthoringValidationIssue> issues;

    std::set<int> vertexIds;
    for (const SectorAuthoringVertex& vertex : graph.vertices) {
        if (!IsValidSectorAuthoringId(vertex.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::Vertex, vertex.id, "Invalid authoring vertex ID");
        } else if (!vertexIds.insert(vertex.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::Vertex, vertex.id, "Duplicate authoring vertex ID");
        }
    }

    std::set<int> lineIds;
    for (const SectorAuthoringLine& line : graph.lines) {
        if (!IsValidSectorAuthoringId(line.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::Line, line.id, "Invalid authoring line ID");
        } else if (!lineIds.insert(line.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::Line, line.id, "Duplicate authoring line ID");
        }
        if (FindSectorAuthoringVertex(graph, line.startVertexId) == nullptr) {
            AddIssue(issues, SectorAuthoringObjectKind::Line, line.id, "Missing start authoring vertex");
        }
        if (FindSectorAuthoringVertex(graph, line.endVertexId) == nullptr) {
            AddIssue(issues, SectorAuthoringObjectKind::Line, line.id, "Missing end authoring vertex");
        }
        if (line.startVertexId == line.endVertexId) {
            AddIssue(issues, SectorAuthoringObjectKind::Line, line.id, "Authoring line has identical endpoints");
        }
    }

    std::set<std::pair<int, SectorTopologySideKind>> sideIds;
    for (const SectorAuthoringLineSide& side : graph.lineSides) {
        const std::pair<int, SectorTopologySideKind> sideId{side.id.lineId, side.id.side};
        if (IsValidSectorAuthoringId(side.id.lineId) && !sideIds.insert(sideId).second) {
            AddIssue(issues, SectorAuthoringObjectKind::Side, side.id.lineId, "Duplicate authoring side identity");
        }
        if (FindSectorAuthoringLine(graph, side.id.lineId) == nullptr) {
            AddIssue(issues, SectorAuthoringObjectKind::Side, side.id.lineId, "Missing authoring line for side");
        }
    }

    std::set<int> faceAnchorIds;
    for (const SectorAuthoringFaceAnchor& anchor : graph.faceAnchors) {
        if (!IsValidSectorAuthoringId(anchor.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::FaceAnchor, anchor.id, "Invalid face anchor ID");
        } else if (!faceAnchorIds.insert(anchor.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::FaceAnchor, anchor.id, "Duplicate face anchor ID");
        }
    }

    return issues;
}

bool HasSectorAuthoringValidationErrors(
        const std::vector<SectorAuthoringValidationIssue>& issues)
{
    return std::any_of(
            issues.begin(),
            issues.end(),
            [](const SectorAuthoringValidationIssue& issue) {
                return issue.severity == SectorAuthoringValidationSeverity::Error;
            });
}

bool SectorAuthoringPlanarRationalsEqual(
        SectorAuthoringPlanarRational lhs,
        SectorAuthoringPlanarRational rhs)
{
    return lhs.numerator == rhs.numerator && lhs.denominator == rhs.denominator;
}

bool SectorAuthoringPlanarPointsEqual(
        const SectorAuthoringPlanarPoint& lhs,
        const SectorAuthoringPlanarPoint& rhs)
{
    return SectorAuthoringPlanarRationalsEqual(lhs.x, rhs.x)
            && SectorAuthoringPlanarRationalsEqual(lhs.y, rhs.y);
}

bool SectorAuthoringPlanarRationalIsInteger(SectorAuthoringPlanarRational value)
{
    return value.denominator == 1;
}

SectorCoord SectorAuthoringPlanarRationalToSectorCoord(SectorAuthoringPlanarRational value)
{
    return static_cast<SectorCoord>(value.numerator / value.denominator);
}

SectorAuthoringPlanarizationResult PlanarizeSectorAuthoringGraph(
        const SectorAuthoringGraph& graph)
{
    SectorAuthoringPlanarizationResult result;
    std::vector<PlanarSourceLine> sourceLines;
    std::set<int> invalidLineIds;

    sourceLines.reserve(graph.lines.size());
    for (const SectorAuthoringLine& line : graph.lines) {
        const SectorAuthoringVertex* start = FindSectorAuthoringVertex(graph, line.startVertexId);
        const SectorAuthoringVertex* end = FindSectorAuthoringVertex(graph, line.endVertexId);
        if (start == nullptr || end == nullptr) {
            AddPlanarDiagnostic(
                    result.diagnostics,
                    SectorAuthoringPlanarDiagnosticKind::MissingVertex,
                    line.id,
                    -1,
                    "Authoring line references a missing vertex");
            invalidLineIds.insert(line.id);
            continue;
        }

        sourceLines.push_back(PlanarSourceLine{&line, start, end});
        if (start->x == end->x && start->y == end->y) {
            AddPlanarDiagnostic(
                    result.diagnostics,
                    SectorAuthoringPlanarDiagnosticKind::ZeroLengthLine,
                    line.id,
                    -1,
                    "Authoring line has zero coordinate length");
            invalidLineIds.insert(line.id);
        }
    }

    std::vector<std::vector<PlanarSplitPoint>> splitPoints(sourceLines.size());
    for (std::size_t lineIndex = 0; lineIndex < sourceLines.size(); ++lineIndex) {
        const PlanarSourceLine& sourceLine = sourceLines[lineIndex];
        if (invalidLineIds.find(sourceLine.line->id) != invalidLineIds.end()) {
            continue;
        }

        AddUniqueSplitPoint(
                splitPoints[lineIndex],
                PlanarSplitPoint{
                        MakeIntegerRational(0),
                        MakeVertexPoint(*sourceLine.start),
                        sourceLine.line->startVertexId});
        AddUniqueSplitPoint(
                splitPoints[lineIndex],
                PlanarSplitPoint{
                        MakeIntegerRational(1),
                        MakeVertexPoint(*sourceLine.end),
                        sourceLine.line->endVertexId});
    }

    for (std::size_t i = 0; i < sourceLines.size(); ++i) {
        const PlanarSourceLine& a = sourceLines[i];
        if (invalidLineIds.find(a.line->id) != invalidLineIds.end()) {
            continue;
        }

        const int64_t ax = static_cast<int64_t>(a.end->x) - a.start->x;
        const int64_t ay = static_cast<int64_t>(a.end->y) - a.start->y;

        for (std::size_t j = i + 1; j < sourceLines.size(); ++j) {
            const PlanarSourceLine& b = sourceLines[j];
            if (invalidLineIds.find(b.line->id) != invalidLineIds.end()) {
                continue;
            }

            const int64_t bx = static_cast<int64_t>(b.end->x) - b.start->x;
            const int64_t by = static_cast<int64_t>(b.end->y) - b.start->y;
            const int64_t qpx = static_cast<int64_t>(b.start->x) - a.start->x;
            const int64_t qpy = static_cast<int64_t>(b.start->y) - a.start->y;
            const int64_t denominator = Cross(ax, ay, bx, by);

            if (denominator == 0) {
                if (Cross(ax, ay, qpx, qpy) != 0) {
                    AddNearMissDiagnostics(a, b, result.diagnostics);
                    continue;
                }

                if (SamePhysicalSegment(a, b)) {
                    AddPlanarDiagnostic(
                            result.diagnostics,
                            SectorAuthoringPlanarDiagnosticKind::DuplicateLine,
                            a.line->id,
                            b.line->id,
                            "Duplicate authoring lines occupy the same physical segment");
                    invalidLineIds.insert(a.line->id);
                    invalidLineIds.insert(b.line->id);
                    continue;
                }

                if (CollinearSegmentsOverlapBeyondEndpoint(a, b)) {
                    AddPlanarDiagnostic(
                            result.diagnostics,
                            SectorAuthoringPlanarDiagnosticKind::CollinearOverlap,
                            a.line->id,
                            b.line->id,
                            "Collinear authoring lines overlap along a segment");
                    invalidLineIds.insert(a.line->id);
                    invalidLineIds.insert(b.line->id);
                    continue;
                }

                if (EndpointCoordinatesCoincideWithDifferentVertices(a, b)) {
                    AddPlanarDiagnostic(
                            result.diagnostics,
                            SectorAuthoringPlanarDiagnosticKind::CoincidentEndpoint,
                            a.line->id,
                            b.line->id,
                            "Authoring lines share endpoint coordinates through different vertex IDs");
                }
                continue;
            }

            const SectorAuthoringPlanarRational t = MakeRational(Cross(qpx, qpy, bx, by), denominator);
            const SectorAuthoringPlanarRational u = MakeRational(Cross(qpx, qpy, ax, ay), denominator);
            const SectorAuthoringPlanarRational zero = MakeIntegerRational(0);
            const SectorAuthoringPlanarRational one = MakeIntegerRational(1);
            if (!LessOrEqualRational(zero, t) || !LessOrEqualRational(t, one)
                    || !LessOrEqualRational(zero, u) || !LessOrEqualRational(u, one)) {
                AddNearMissDiagnostics(a, b, result.diagnostics);
                continue;
            }

            const SectorAuthoringPlanarPoint point = MakeLinePointAt(*a.start, *a.end, t);
            const int aEndpointVertexId = EndpointSourceVertexIdAt(a, t);
            const int bEndpointVertexId = EndpointSourceVertexIdAt(b, u);
            if (IsValidSectorAuthoringId(aEndpointVertexId)
                    && IsValidSectorAuthoringId(bEndpointVertexId)
                    && aEndpointVertexId != bEndpointVertexId) {
                AddPlanarDiagnostic(
                        result.diagnostics,
                        SectorAuthoringPlanarDiagnosticKind::CoincidentEndpoint,
                        a.line->id,
                        b.line->id,
                        "Authoring lines share endpoint coordinates through different vertex IDs");
            }

            int sourceVertexId = aEndpointVertexId;
            if (!IsValidSectorAuthoringId(sourceVertexId)) {
                sourceVertexId = bEndpointVertexId;
            }
            AddUniqueSplitPoint(splitPoints[i], PlanarSplitPoint{t, point, sourceVertexId});
            AddUniqueSplitPoint(splitPoints[j], PlanarSplitPoint{u, point, sourceVertexId});
        }
    }

    for (std::size_t lineIndex = 0; lineIndex < sourceLines.size(); ++lineIndex) {
        const PlanarSourceLine& sourceLine = sourceLines[lineIndex];
        if (invalidLineIds.find(sourceLine.line->id) != invalidLineIds.end()) {
            continue;
        }

        std::vector<PlanarSplitPoint>& lineSplitPoints = splitPoints[lineIndex];
        std::sort(
                lineSplitPoints.begin(),
                lineSplitPoints.end(),
                [](const PlanarSplitPoint& lhs, const PlanarSplitPoint& rhs) {
                    if (!SectorAuthoringPlanarRationalsEqual(lhs.t, rhs.t)) {
                        return LessRational(lhs.t, rhs.t);
                    }
                    return PlanarPointLess(lhs.point, rhs.point);
                });

        for (std::size_t splitIndex = 1; splitIndex < lineSplitPoints.size(); ++splitIndex) {
            const PlanarSplitPoint& start = lineSplitPoints[splitIndex - 1];
            const PlanarSplitPoint& end = lineSplitPoints[splitIndex];
            if (SectorAuthoringPlanarPointsEqual(start.point, end.point)) {
                continue;
            }

            const int startVertexId = FindOrAddPlanarVertex(result, start.point, start.sourceVertexId);
            const int endVertexId = FindOrAddPlanarVertex(result, end.point, end.sourceVertexId);

            SectorAuthoringPlanarEdge edge;
            edge.id = static_cast<int>(result.edges.size()) + 1;
            edge.startVertexId = startVertexId;
            edge.endVertexId = endVertexId;
            edge.sourceLineId = sourceLine.line->id;
            edge.followsSourceLineDirection = true;
            result.edges.push_back(edge);
        }
    }

    return result;
}

SectorAuthoringFaceExtractionResult ExtractSectorAuthoringFaces(
        const SectorAuthoringPlanarizationResult& planar)
{
    SectorAuthoringFaceExtractionResult result;
    std::vector<FaceHalfEdge> halfEdges;
    std::vector<int> validPlanarEdgeIds = BuildFaceHalfEdges(planar, halfEdges, result);
    if (halfEdges.empty()) {
        return result;
    }

    int maxVertexId = 0;
    for (const SectorAuthoringPlanarVertex& vertex : planar.vertices) {
        maxVertexId = std::max(maxVertexId, vertex.id);
    }

    const std::vector<std::vector<int>> outgoing = BuildSortedOutgoingHalfEdges(halfEdges, maxVertexId);
    AssignFaceHalfEdgeComponents(halfEdges, outgoing);
    std::vector<bool> visited(halfEdges.size() + 1, false);
    std::set<int> positiveFaceEdgeIds;
    std::vector<CandidateExtractedFace> candidateFaces;

    for (const FaceHalfEdge& startHalfEdge : halfEdges) {
        if (visited[static_cast<std::size_t>(startHalfEdge.id)]) {
            continue;
        }

        std::vector<int> loopHalfEdgeIds;
        int currentId = startHalfEdge.id;
        bool closed = false;
        for (std::size_t step = 0; step <= halfEdges.size(); ++step) {
            if (currentId <= 0 || currentId > static_cast<int>(halfEdges.size())) {
                break;
            }
            if (currentId == startHalfEdge.id && !loopHalfEdgeIds.empty()) {
                closed = true;
                break;
            }
            if (visited[static_cast<std::size_t>(currentId)]) {
                break;
            }

            visited[static_cast<std::size_t>(currentId)] = true;
            loopHalfEdgeIds.push_back(currentId);

            const FaceHalfEdge& current = halfEdges[static_cast<std::size_t>(currentId - 1)];
            currentId = FindNextFaceHalfEdge(halfEdges, outgoing, current);
        }

        if (!closed || loopHalfEdgeIds.size() < 3) {
            for (int halfEdgeId : loopHalfEdgeIds) {
                const FaceHalfEdge& halfEdge = halfEdges[static_cast<std::size_t>(halfEdgeId - 1)];
                AddFaceDiagnostic(
                        result.diagnostics,
                        SectorAuthoringFaceDiagnosticKind::DanglingEdge,
                        planar.edges[static_cast<std::size_t>(halfEdge.planarEdgeIndex)].id,
                        halfEdge.startVertexId,
                        "Planar edge does not bound a closed face");
            }
            continue;
        }

        const double signedArea = ComputeFaceSignedArea(planar, loopHalfEdgeIds, halfEdges);
        if (signedArea <= 0.0) {
            continue;
        }

        constexpr double tinySliverAreaThreshold = 1.0;
        if (signedArea < tinySliverAreaThreshold) {
            const FaceHalfEdge& firstHalfEdge = halfEdges[static_cast<std::size_t>(loopHalfEdgeIds.front() - 1)];
            AddFaceDiagnostic(
                    result.diagnostics,
                    SectorAuthoringFaceDiagnosticKind::TinySliverFace,
                    planar.edges[static_cast<std::size_t>(firstHalfEdge.planarEdgeIndex)].id,
                    firstHalfEdge.startVertexId,
                    "Closed face area is below the sliver threshold");
            continue;
        }

        SectorAuthoringExtractedFace face;
        face.id = static_cast<int>(candidateFaces.size()) + 1;
        face.signedArea = signedArea;
        face.boundary.reserve(loopHalfEdgeIds.size());
        for (int halfEdgeId : loopHalfEdgeIds) {
            const FaceHalfEdge& halfEdge = halfEdges[static_cast<std::size_t>(halfEdgeId - 1)];
            face.boundary.push_back(MakeFaceBoundaryEdge(planar, halfEdge));
            positiveFaceEdgeIds.insert(planar.edges[static_cast<std::size_t>(halfEdge.planarEdgeIndex)].id);
        }
        CandidateExtractedFace candidate;
        candidate.componentId = startHalfEdge.componentId;
        candidate.face = std::move(face);
        candidateFaces.push_back(std::move(candidate));
    }

    for (int planarEdgeId : validPlanarEdgeIds) {
        if (positiveFaceEdgeIds.find(planarEdgeId) == positiveFaceEdgeIds.end()) {
            AddFaceDiagnostic(
                    result.diagnostics,
                    SectorAuthoringFaceDiagnosticKind::DanglingEdge,
                    planarEdgeId,
                    -1,
                    "Planar edge is not part of any bounded face");
        }
    }

    if (HasUnsupportedDisconnectedClosedFaces(candidateFaces, result)) {
        return result;
    }

    result.faces.reserve(candidateFaces.size());
    for (CandidateExtractedFace& candidate : candidateFaces) {
        result.faces.push_back(std::move(candidate.face));
    }

    std::sort(
            result.faces.begin(),
            result.faces.end(),
            [&planar](const SectorAuthoringExtractedFace& lhs, const SectorAuthoringExtractedFace& rhs) {
                const SectorAuthoringPlanarVertex* lhsStart = lhs.boundary.empty()
                        ? nullptr
                        : FindPlanarVertex(planar, lhs.boundary.front().startVertexId);
                const SectorAuthoringPlanarVertex* rhsStart = rhs.boundary.empty()
                        ? nullptr
                        : FindPlanarVertex(planar, rhs.boundary.front().startVertexId);
                if (lhsStart != nullptr && rhsStart != nullptr
                        && !SectorAuthoringPlanarPointsEqual(lhsStart->point, rhsStart->point)) {
                    return PlanarPointLess(lhsStart->point, rhsStart->point);
                }
                return lhs.id < rhs.id;
            });
    for (std::size_t index = 0; index < result.faces.size(); ++index) {
        result.faces[index].id = static_cast<int>(index) + 1;
    }

    return result;
}

SectorAuthoringDerivationResult DeriveSectorTopologyMapFromAuthoringGraph(
        const SectorAuthoringGraph& graph)
{
    SectorAuthoringDerivationResult result;

    const std::vector<SectorAuthoringValidationIssue> referenceIssues =
            ValidateSectorAuthoringGraphReferences(graph);
    for (const SectorAuthoringValidationIssue& issue : referenceIssues) {
        AddDerivationDiagnostic(
                result.diagnostics,
                SectorAuthoringDerivationDiagnosticKind::AuthoringReference,
                issue.objectId,
                issue.message,
                issue.severity);
    }
    if (HasDerivationErrors(result.diagnostics)) {
        return result;
    }

    result.planar = PlanarizeSectorAuthoringGraph(graph);
    for (const SectorAuthoringPlanarDiagnostic& diagnostic : result.planar.diagnostics) {
        AddDerivationDiagnostic(
                result.diagnostics,
                SectorAuthoringDerivationDiagnosticKind::Planarization,
                diagnostic.lineId,
                diagnostic.message,
                diagnostic.severity);
    }
    if (HasDerivationErrors(result.diagnostics)) {
        return result;
    }

    result.faces = ExtractSectorAuthoringFaces(result.planar);
    for (const SectorAuthoringFaceDiagnostic& diagnostic : result.faces.diagnostics) {
        AddDerivationDiagnostic(
                result.diagnostics,
                SectorAuthoringDerivationDiagnosticKind::FaceExtraction,
                diagnostic.planarEdgeId,
                diagnostic.message,
                diagnostic.severity);
    }
    if (result.faces.faces.empty()) {
        AddDerivationDiagnostic(
                result.diagnostics,
                SectorAuthoringDerivationDiagnosticKind::FaceExtraction,
                -1,
                "Authoring graph does not contain any closed bounded faces");
    }
    if (HasDerivationErrors(result.diagnostics)) {
        return result;
    }

    std::map<int, int> topologyVertexIdsByPlanarVertexId;
    CopyDerivationVertexMappingToTopology(
            result.planar,
            result.topology,
            result.mapping,
            topologyVertexIdsByPlanarVertexId,
            result.diagnostics);
    if (HasDerivationErrors(result.diagnostics)) {
        result.topology = SectorTopologyMap{};
        result.mapping = SectorAuthoringDerivationMapping{};
        return result;
    }

    BuildDerivedTopologyFacesAndLines(
            graph,
            result.planar,
            result.faces,
            topologyVertexIdsByPlanarVertexId,
            result.topology,
            result.mapping);

    const std::vector<SectorTopologyValidationIssue> topologyIssues =
            ValidateSectorTopologyMap(result.topology);
    for (const SectorTopologyValidationIssue& issue : topologyIssues) {
        AddDerivationDiagnostic(
                result.diagnostics,
                SectorAuthoringDerivationDiagnosticKind::InvalidTopology,
                issue.objectId,
                FormatSectorTopologyValidationIssue(issue),
                issue.severity == SectorTopologyValidationSeverity::Error
                        ? SectorAuthoringValidationSeverity::Error
                        : SectorAuthoringValidationSeverity::Warning);
    }
    if (HasDerivationErrors(result.diagnostics)) {
        result.topology = SectorTopologyMap{};
        result.mapping = SectorAuthoringDerivationMapping{};
        return result;
    }

    result.success = true;
    return result;
}

SectorAuthoringGraph ImportSectorTopologyMapToAuthoringGraph(const SectorTopologyMap& map)
{
    SectorAuthoringGraph graph;
    graph.vertices.reserve(map.vertices.size());
    graph.lines.reserve(map.lineDefs.size());
    graph.lineSides.reserve(map.sideDefs.size());
    graph.faceAnchors.reserve(map.sectors.size());

    for (const SectorTopologyVertex& topologyVertex : map.vertices) {
        SectorAuthoringVertex vertex;
        vertex.id = topologyVertex.id;
        vertex.x = topologyVertex.x;
        vertex.y = topologyVertex.y;
        graph.vertices.push_back(vertex);
    }

    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        SectorAuthoringLine line;
        line.id = lineDef.id;
        line.startVertexId = lineDef.startVertexId;
        line.endVertexId = lineDef.endVertexId;
        line.flags = lineDef.flags;
        graph.lines.push_back(line);
    }

    for (const SectorTopologySideDef& sideDef : map.sideDefs) {
        SectorAuthoringLineSide side;
        side.id.lineId = sideDef.lineDefId;
        side.id.side = sideDef.side;
        side.wall = sideDef.wall;
        side.lower = sideDef.lower;
        side.upper = sideDef.upper;
        side.middle = sideDef.middle;
        graph.lineSides.push_back(side);
    }

    for (const SectorTopologySector& sector : map.sectors) {
        SectorAuthoringFaceAnchor anchor;
        CopySectorPropertiesToFaceAnchor(sector, anchor);
        SetFaceAnchorAveragePosition(map, sector, anchor);
        graph.faceAnchors.push_back(anchor);
    }

    return graph;
}

} // namespace game
