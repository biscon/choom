#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTriggers.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>

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

constexpr float FogBottomOffsetMin = -16.0f;
constexpr float FogBottomOffsetMax = 16.0f;
constexpr float FogRadiusMin = 0.05f;
constexpr float FogRadiusMax = 64.0f;
constexpr float FogHeightMin = 0.05f;
constexpr float FogHeightMax = 32.0f;
constexpr float FogOpacityMin = 0.0f;
constexpr float FogOpacityMax = 1.0f;
constexpr float FogSoftnessMin = 0.0f;
constexpr float FogSoftnessMax = 1.0f;
constexpr float FogNoiseScaleMin = 0.05f;
constexpr float FogNoiseScaleMax = 64.0f;
constexpr float FogNoiseAmountMin = 0.0f;
constexpr float FogNoiseAmountMax = 1.0f;
constexpr float FogFlowSpeedMin = 0.0f;
constexpr float FogFlowSpeedMax = 8.0f;
constexpr float ReflectionProbeExtentMin = 0.10f;
constexpr float ReflectionProbeExtentMax = 128.0f;

float ClampFiniteFogValue(float value, float minimum, float maximum, float fallback)
{
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

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

bool IsGeneratedSectorName(const std::string& name)
{
    constexpr const char* prefix = "Sector ";
    constexpr std::size_t prefixLength = 7;
    if (name.size() <= prefixLength || name.compare(0, prefixLength, prefix) != 0) {
        return false;
    }
    for (std::size_t i = prefixLength; i < name.size(); ++i) {
        if (name[i] < '0' || name[i] > '9') {
            return false;
        }
    }
    return true;
}

std::string NextGeneratedSectorName(const std::set<std::string>& usedNames)
{
    for (int id = 1; id < 10000; ++id) {
        const std::string candidate = "Sector " + std::to_string(id);
        if (usedNames.find(candidate) == usedNames.end()) {
            return candidate;
        }
    }
    return "Sector " + std::to_string(usedNames.size() + 1);
}

std::string ResolveDerivedSectorName(
        const std::string& requestedName,
        std::set<std::string>& usedNames,
        const std::set<std::string>& reservedNames)
{
    if (!requestedName.empty()
            && !IsGeneratedSectorName(requestedName)) {
        usedNames.insert(requestedName);
        return requestedName;
    }

    if (!requestedName.empty()
            && usedNames.insert(requestedName).second) {
        return requestedName;
    }

    std::set<std::string> unavailableNames = usedNames;
    unavailableNames.insert(reservedNames.begin(), reservedNames.end());
    const std::string generatedName = NextGeneratedSectorName(unavailableNames);
    usedNames.insert(generatedName);
    return generatedName;
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

struct FaceContainmentInfo {
    std::map<int, int> parentFaceIdByFaceId;
    std::map<int, std::vector<int>> childFaceIdsByFaceId;
    std::map<int, int> depthByFaceId;
};

constexpr int maxSupportedNestedFaceDepth = 8;

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

bool PointOnSegmentInclusive(
        SectorTopologyCoordPoint point,
        const SectorAuthoringVertex& start,
        const SectorAuthoringVertex& end)
{
    SectorAuthoringVertex pointVertex;
    pointVertex.x = point.x;
    pointVertex.y = point.y;
    return PointOnSegmentInclusive(pointVertex, start, end);
}

bool FindAuthoringVertexIdAtPoint(
        const SectorAuthoringGraph& graph,
        SectorTopologyCoordPoint point,
        int* outVertexId)
{
    for (const SectorAuthoringVertex& vertex : graph.vertices) {
        if (vertex.x == point.x && vertex.y == point.y) {
            if (outVertexId != nullptr) {
                *outVertexId = vertex.id;
            }
            return true;
        }
    }
    return false;
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
    anchor.floorMaterialId = sector.floorMaterialId;
    anchor.ceilingMaterialId = sector.ceilingMaterialId;
    anchor.footstepSet = sector.footstepSet;
    anchor.ceilingSky = sector.ceilingSky;
    anchor.roomtone = sector.roomtone;
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

void CopyFaceAnchorPropertiesToTopologySector(
        const SectorAuthoringFaceAnchor& anchor,
        SectorTopologySector& sector)
{
    sector.name = anchor.name;
    sector.floorZ = anchor.floorZ;
    sector.ceilingZ = anchor.ceilingZ;
    sector.floorMaterialId = anchor.floorMaterialId;
    sector.ceilingMaterialId = anchor.ceilingMaterialId;
    sector.footstepSet = anchor.footstepSet;
    sector.ceilingSky = anchor.ceilingSky;
    sector.roomtone = anchor.roomtone;
    sector.floorUv = anchor.floorUv;
    sector.ceilingUv = anchor.ceilingUv;
    sector.floorDecal = anchor.floorDecal;
    sector.ceilingDecal = anchor.ceilingDecal;
    sector.ambientColor = anchor.ambientColor;
    sector.ambientIntensity = anchor.ambientIntensity;
    sector.defaultWall = anchor.defaultWall;
    sector.defaultLower = anchor.defaultLower;
    sector.defaultUpper = anchor.defaultUpper;
}

void CopyAuthoringSidePropertiesToTopologySideDef(
        const SectorAuthoringLineSide& authoringSide,
        SectorTopologySideDef& sideDef)
{
    sideDef.wall = authoringSide.wall;
    sideDef.lower = authoringSide.lower;
    sideDef.upper = authoringSide.upper;
    sideDef.middle = authoringSide.middle;
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

bool PlanarPointIsOnSegment(
        double px,
        double py,
        double ax,
        double ay,
        double bx,
        double by)
{
    constexpr double epsilon = 1.0e-9;
    const double cross = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    if (std::fabs(cross) > epsilon) {
        return false;
    }
    return px >= std::min(ax, bx) - epsilon && px <= std::max(ax, bx) + epsilon
            && py >= std::min(ay, by) - epsilon && py <= std::max(ay, by) + epsilon;
}

bool FaceContainsPlanarPoint(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringExtractedFace& face,
        const SectorAuthoringPlanarPoint& point)
{
    if (face.boundary.size() < 3) {
        return false;
    }

    const double px = PlanarPointX(point);
    const double py = PlanarPointY(point);
    bool inside = false;
    for (const SectorAuthoringFaceBoundaryEdge& boundary : face.boundary) {
        const SectorAuthoringPlanarVertex* start = FindPlanarVertex(planar, boundary.startVertexId);
        const SectorAuthoringPlanarVertex* end = FindPlanarVertex(planar, boundary.endVertexId);
        if (start == nullptr || end == nullptr) {
            continue;
        }

        const double ax = PlanarPointX(start->point);
        const double ay = PlanarPointY(start->point);
        const double bx = PlanarPointX(end->point);
        const double by = PlanarPointY(end->point);
        if (PlanarPointIsOnSegment(px, py, ax, ay, bx, by)) {
            return true;
        }
        if (((ay > py) != (by > py))
                && (px < (bx - ax) * (py - ay) / (by - ay) + ax)) {
            inside = !inside;
        }
    }
    return inside;
}

bool FaceStrictlyContainsPoint(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringExtractedFace& face,
        double px,
        double py)
{
    if (face.boundary.size() < 3) {
        return false;
    }

    bool inside = false;
    for (const SectorAuthoringFaceBoundaryEdge& boundary : face.boundary) {
        const SectorAuthoringPlanarVertex* start = FindPlanarVertex(planar, boundary.startVertexId);
        const SectorAuthoringPlanarVertex* end = FindPlanarVertex(planar, boundary.endVertexId);
        if (start == nullptr || end == nullptr) {
            continue;
        }

        const double ax = PlanarPointX(start->point);
        const double ay = PlanarPointY(start->point);
        const double bx = PlanarPointX(end->point);
        const double by = PlanarPointY(end->point);
        if (PlanarPointIsOnSegment(px, py, ax, ay, bx, by)) {
            return false;
        }
        if (((ay > py) != (by > py))
                && (px < (bx - ax) * (py - ay) / (by - ay) + ax)) {
            inside = !inside;
        }
    }
    return inside;
}

void AddDerivationDiagnostic(
        std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics,
        SectorAuthoringDerivationDiagnosticKind kind,
        int objectId,
        std::string message,
        SectorAuthoringValidationSeverity severity = SectorAuthoringValidationSeverity::Error,
        int relatedObjectId = -1)
{
    SectorAuthoringDerivationDiagnostic diagnostic;
    diagnostic.severity = severity;
    diagnostic.kind = kind;
    diagnostic.objectId = objectId;
    diagnostic.relatedObjectId = relatedObjectId;
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

SectorAuthoringDerivationDiagnosticKind MapPlanarDiagnosticKind(
        SectorAuthoringPlanarDiagnosticKind kind)
{
    switch (kind) {
    case SectorAuthoringPlanarDiagnosticKind::MissingVertex:
        return SectorAuthoringDerivationDiagnosticKind::AuthoringReference;
    case SectorAuthoringPlanarDiagnosticKind::ZeroLengthLine:
        return SectorAuthoringDerivationDiagnosticKind::ZeroLengthLine;
    case SectorAuthoringPlanarDiagnosticKind::DuplicateLine:
        return SectorAuthoringDerivationDiagnosticKind::DuplicateLine;
    case SectorAuthoringPlanarDiagnosticKind::CollinearOverlap:
        return SectorAuthoringDerivationDiagnosticKind::CollinearOverlap;
    case SectorAuthoringPlanarDiagnosticKind::NearMiss:
        return SectorAuthoringDerivationDiagnosticKind::NearMiss;
    case SectorAuthoringPlanarDiagnosticKind::CoincidentEndpoint:
        return SectorAuthoringDerivationDiagnosticKind::Planarization;
    }
    return SectorAuthoringDerivationDiagnosticKind::Planarization;
}

SectorAuthoringDerivationDiagnosticKind MapFaceDiagnosticKind(
        SectorAuthoringFaceDiagnosticKind kind)
{
    switch (kind) {
    case SectorAuthoringFaceDiagnosticKind::MissingVertex:
        return SectorAuthoringDerivationDiagnosticKind::FaceExtraction;
    case SectorAuthoringFaceDiagnosticKind::DuplicateEdge:
        return SectorAuthoringDerivationDiagnosticKind::FaceExtraction;
    case SectorAuthoringFaceDiagnosticKind::DanglingEdge:
        return SectorAuthoringDerivationDiagnosticKind::DanglingLine;
    case SectorAuthoringFaceDiagnosticKind::TinySliverFace:
        return SectorAuthoringDerivationDiagnosticKind::TinySliverFace;
    case SectorAuthoringFaceDiagnosticKind::AmbiguousTopology:
        return SectorAuthoringDerivationDiagnosticKind::FaceExtraction;
    }
    return SectorAuthoringDerivationDiagnosticKind::FaceExtraction;
}

const SectorAuthoringPlanarEdge* FindPlanarEdge(
        const SectorAuthoringPlanarizationResult& planar,
        int id)
{
    for (const SectorAuthoringPlanarEdge& edge : planar.edges) {
        if (edge.id == id) {
            return &edge;
        }
    }
    return nullptr;
}

void AddFaceDerivationDiagnostic(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringFaceDiagnostic& diagnostic,
        std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics)
{
    const SectorAuthoringDerivationDiagnosticKind kind = MapFaceDiagnosticKind(diagnostic.kind);
    if (diagnostic.kind != SectorAuthoringFaceDiagnosticKind::DanglingEdge) {
        AddDerivationDiagnostic(
                diagnostics,
                kind,
                diagnostic.planarEdgeId,
                diagnostic.message,
                diagnostic.severity,
                diagnostic.vertexId);
        return;
    }

    const SectorAuthoringPlanarEdge* edge = FindPlanarEdge(planar, diagnostic.planarEdgeId);
    if (edge != nullptr && IsValidSectorAuthoringId(edge->sourceLineId)) {
        AddDerivationDiagnostic(
                diagnostics,
                kind,
                edge->sourceLineId,
                diagnostic.message,
                diagnostic.severity,
                diagnostic.planarEdgeId);
        return;
    }

    AddDerivationDiagnostic(
            diagnostics,
            kind,
            -1,
            "Dangling planar edge could not be mapped to a source authoring line",
            diagnostic.severity,
            diagnostic.planarEdgeId);
}

bool PointIsOnSegment(
        double px,
        double py,
        double ax,
        double ay,
        double bx,
        double by)
{
    constexpr double epsilon = 1.0e-9;
    const double cross = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    if (std::fabs(cross) > epsilon) {
        return false;
    }
    return px >= std::min(ax, bx) - epsilon && px <= std::max(ax, bx) + epsilon
            && py >= std::min(ay, by) - epsilon && py <= std::max(ay, by) + epsilon;
}

bool FaceContainsPoint(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringExtractedFace& face,
        double px,
        double py,
        bool includeBoundary)
{
    if (face.boundary.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t index = 0; index < face.boundary.size(); ++index) {
        const SectorAuthoringFaceBoundaryEdge& boundary = face.boundary[index];
        const SectorAuthoringPlanarVertex* start = FindPlanarVertex(planar, boundary.startVertexId);
        const SectorAuthoringPlanarVertex* end = FindPlanarVertex(planar, boundary.endVertexId);
        if (start == nullptr || end == nullptr) {
            continue;
        }

        const double ax = PlanarPointX(start->point);
        const double ay = PlanarPointY(start->point);
        const double bx = PlanarPointX(end->point);
        const double by = PlanarPointY(end->point);
        if (includeBoundary && PointIsOnSegment(px, py, ax, ay, bx, by)) {
            return true;
        }
        if (((ay > py) != (by > py))
                && (px < (bx - ax) * (py - ay) / (by - ay) + ax)) {
            inside = !inside;
        }
    }
    return inside;
}

bool FaceContainsAnchorPoint(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringExtractedFace& face,
        const SectorAuthoringFaceAnchor& anchor)
{
    return FaceContainsPoint(
            planar,
            face,
            static_cast<double>(anchor.x),
            static_cast<double>(anchor.y),
            true);
}

bool FaceContainsFace(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringExtractedFace& container,
        const SectorAuthoringExtractedFace& contained)
{
    if (container.id == contained.id || contained.boundary.empty()) {
        return false;
    }

    int pointCount = 0;
    for (const SectorAuthoringFaceBoundaryEdge& boundary : contained.boundary) {
        const SectorAuthoringPlanarVertex* vertex = FindPlanarVertex(planar, boundary.startVertexId);
        if (vertex == nullptr) {
            continue;
        }
        if (!FaceStrictlyContainsPoint(
                    planar,
                    container,
                    PlanarPointX(vertex->point),
                    PlanarPointY(vertex->point))) {
            return false;
        }
        ++pointCount;
    }
    return pointCount > 0;
}

double FaceAreaById(const SectorAuthoringFaceExtractionResult& faces, int faceId)
{
    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        if (face.id == faceId) {
            return face.signedArea;
        }
    }
    return 0.0;
}

FaceContainmentInfo BuildFaceContainmentInfo(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringFaceExtractionResult& faces)
{
    FaceContainmentInfo info;
    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        int parentFaceId = -1;
        double parentArea = std::numeric_limits<double>::max();
        for (const SectorAuthoringExtractedFace& candidateParent : faces.faces) {
            if (!FaceContainsFace(planar, candidateParent, face)) {
                continue;
            }
            if (candidateParent.signedArea < parentArea) {
                parentArea = candidateParent.signedArea;
                parentFaceId = candidateParent.id;
            }
        }
        if (parentFaceId > 0) {
            info.parentFaceIdByFaceId[face.id] = parentFaceId;
            info.childFaceIdsByFaceId[parentFaceId].push_back(face.id);
        }
    }

    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        int depth = 0;
        int parentFaceId = face.id;
        std::set<int> visited;
        while (true) {
            const auto parentIt = info.parentFaceIdByFaceId.find(parentFaceId);
            if (parentIt == info.parentFaceIdByFaceId.end()) {
                break;
            }
            if (!visited.insert(parentIt->second).second) {
                break;
            }
            parentFaceId = parentIt->second;
            ++depth;
        }
        info.depthByFaceId[face.id] = depth;
    }

    for (auto& entry : info.childFaceIdsByFaceId) {
        std::sort(
                entry.second.begin(),
                entry.second.end(),
                [&faces](int lhs, int rhs) {
                    const double lhsArea = FaceAreaById(faces, lhs);
                    const double rhsArea = FaceAreaById(faces, rhs);
                    if (lhsArea != rhsArea) {
                        return lhsArea > rhsArea;
                    }
                    return lhs < rhs;
                });
    }
    return info;
}

void AddUnsupportedNestedFaceDiagnostics(
        const SectorAuthoringFaceExtractionResult& faces,
        const FaceContainmentInfo& containment,
        std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics)
{
    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        const auto depthIt = containment.depthByFaceId.find(face.id);
        const int depth = depthIt == containment.depthByFaceId.end() ? 0 : depthIt->second;
        if (depth <= maxSupportedNestedFaceDepth) {
            continue;
        }

        const int objectId = face.boundary.empty() ? face.id : face.boundary.front().planarEdgeId;
        AddDerivationDiagnostic(
                diagnostics,
                SectorAuthoringDerivationDiagnosticKind::FaceExtraction,
                objectId,
                "Nested authoring loop depth exceeds the supported maximum of "
                        + std::to_string(maxSupportedNestedFaceDepth));
    }
}

bool FaceContainsAnchorPointRespectingHoles(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringFaceExtractionResult& faces,
        const FaceContainmentInfo& containment,
        const SectorAuthoringExtractedFace& face,
        const SectorAuthoringFaceAnchor& anchor)
{
    if (!FaceContainsAnchorPoint(planar, face, anchor)) {
        return false;
    }

    const auto childIt = containment.childFaceIdsByFaceId.find(face.id);
    if (childIt == containment.childFaceIdsByFaceId.end()) {
        return true;
    }

    for (int childFaceId : childIt->second) {
        const auto childDepthIt = containment.depthByFaceId.find(childFaceId);
        const int childDepth = childDepthIt == containment.depthByFaceId.end() ? 0 : childDepthIt->second;
        const auto faceDepthIt = containment.depthByFaceId.find(face.id);
        const int faceDepth = faceDepthIt == containment.depthByFaceId.end() ? 0 : faceDepthIt->second;
        if (childDepth != faceDepth + 1) {
            continue;
        }
        for (const SectorAuthoringExtractedFace& childFace : faces.faces) {
            if (childFace.id == childFaceId && FaceContainsAnchorPoint(planar, childFace, anchor)) {
                return false;
            }
        }
    }
    return true;
}

std::map<int, int> ResolveFaceAnchorsByFaceId(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringFaceExtractionResult& faces,
        const FaceContainmentInfo& containment,
        std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics)
{
    std::map<int, std::vector<int>> anchorIdsByFaceId;
    for (const SectorAuthoringFaceAnchor& anchor : graph.faceAnchors) {
        std::vector<int> containingFaceIds;
        for (const SectorAuthoringExtractedFace& face : faces.faces) {
            if (FaceContainsAnchorPointRespectingHoles(planar, faces, containment, face, anchor)) {
                containingFaceIds.push_back(face.id);
            }
        }

        if (containingFaceIds.empty()) {
            AddDerivationDiagnostic(
                    diagnostics,
                    SectorAuthoringDerivationDiagnosticKind::UnresolvedFaceAnchor,
                    anchor.id,
                    "Face anchor does not resolve to a derived closed face");
            continue;
        }
        if (containingFaceIds.size() > 1) {
            AddDerivationDiagnostic(
                    diagnostics,
                    SectorAuthoringDerivationDiagnosticKind::AmbiguousFaceAnchor,
                    anchor.id,
                    "Face anchor resolves to multiple derived faces");
            continue;
        }

        anchorIdsByFaceId[containingFaceIds.front()].push_back(anchor.id);
    }

    std::map<int, int> anchorIdByFaceId;
    for (const auto& entry : anchorIdsByFaceId) {
        if (entry.second.size() > 1) {
            AddDerivationDiagnostic(
                    diagnostics,
                    SectorAuthoringDerivationDiagnosticKind::AmbiguousFaceAnchor,
                    entry.second.front(),
                    "Multiple face anchors resolve to the same derived face",
                    SectorAuthoringValidationSeverity::Error,
                    entry.first);
            continue;
        }
        anchorIdByFaceId[entry.first] = entry.second.front();
    }
    return anchorIdByFaceId;
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
        int topologySectorId,
        SectorTopologySector& sector)
{
    sector.id = topologySectorId;
    sector.name = "Sector " + std::to_string(face.id);
    sector.floorMaterialId = "floor";
    sector.ceilingMaterialId = "ceiling";
    sector.defaultWall.materialId = "wall";
    sector.defaultLower.materialId = "wall";
    sector.defaultUpper.materialId = "wall";
}

int AllocateDerivedSectorId(
        int preferredId,
        std::set<int>& usedSectorIds,
        const std::set<int>& reservedSectorIds,
        int* nextGeneratedSectorId)
{
    if (IsValidSectorAuthoringId(preferredId) && usedSectorIds.insert(preferredId).second) {
        return preferredId;
    }

    while (*nextGeneratedSectorId > 0
            && (usedSectorIds.find(*nextGeneratedSectorId) != usedSectorIds.end()
                    || reservedSectorIds.find(*nextGeneratedSectorId) != reservedSectorIds.end())) {
        ++(*nextGeneratedSectorId);
    }
    const int sectorId = *nextGeneratedSectorId;
    usedSectorIds.insert(sectorId);
    ++(*nextGeneratedSectorId);
    return sectorId;
}

void PruneUnusedDerivedTopology(
        SectorTopologyMap& topology,
        SectorAuthoringDerivationMapping& mapping)
{
    std::set<int> usedLineDefIds;
    for (const SectorTopologyLineDef& lineDef : topology.lineDefs) {
        if (lineDef.frontSideDefId > 0 || lineDef.backSideDefId > 0) {
            usedLineDefIds.insert(lineDef.id);
        }
    }

    topology.lineDefs.erase(
            std::remove_if(
                    topology.lineDefs.begin(),
                    topology.lineDefs.end(),
                    [&usedLineDefIds](const SectorTopologyLineDef& lineDef) {
                        return usedLineDefIds.find(lineDef.id) == usedLineDefIds.end();
                    }),
            topology.lineDefs.end());
    mapping.lines.erase(
            std::remove_if(
                    mapping.lines.begin(),
                    mapping.lines.end(),
                    [&usedLineDefIds](const SectorAuthoringDerivedLineMapping& lineMapping) {
                        return usedLineDefIds.find(lineMapping.topologyLineDefId) == usedLineDefIds.end();
                    }),
            mapping.lines.end());

    std::set<int> usedVertexIds;
    for (const SectorTopologyLineDef& lineDef : topology.lineDefs) {
        usedVertexIds.insert(lineDef.startVertexId);
        usedVertexIds.insert(lineDef.endVertexId);
    }
    topology.vertices.erase(
            std::remove_if(
                    topology.vertices.begin(),
                    topology.vertices.end(),
                    [&usedVertexIds](const SectorTopologyVertex& vertex) {
                        return usedVertexIds.find(vertex.id) == usedVertexIds.end();
                    }),
            topology.vertices.end());
    mapping.vertices.erase(
            std::remove_if(
                    mapping.vertices.begin(),
                    mapping.vertices.end(),
                    [&usedVertexIds](const SectorAuthoringDerivedVertexMapping& vertexMapping) {
                        return usedVertexIds.find(vertexMapping.topologyVertexId) == usedVertexIds.end();
                    }),
            mapping.vertices.end());
}

void BuildDerivedTopologyFacesAndLines(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringFaceExtractionResult& faces,
        const FaceContainmentInfo& containment,
        const std::map<int, int>& topologyVertexIdsByPlanarVertexId,
        const std::map<int, int>& faceAnchorIdsByFaceId,
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
        lineMapping.sourceLineId = planarEdge.sourceLineId;
        mapping.lines.push_back(lineMapping);
    }

    topology.sectors.reserve(faces.faces.size());
    std::map<int, int> topologySectorIdsByFaceId;
    std::set<int> usedSectorIds;
    std::set<int> reservedSectorIds;
    for (const auto& entry : faceAnchorIdsByFaceId) {
        if (IsValidSectorAuthoringId(entry.second)) {
            reservedSectorIds.insert(entry.second);
        }
    }
    std::set<std::string> reservedFaceAnchorNames;
    for (const auto& entry : faceAnchorIdsByFaceId) {
        if (const SectorAuthoringFaceAnchor* anchor = FindSectorAuthoringFaceAnchor(graph, entry.second)) {
            if (!anchor->name.empty()) {
                reservedFaceAnchorNames.insert(anchor->name);
            }
        }
    }
    std::set<std::string> usedSectorNames;
    int nextGeneratedSectorId = 1;
    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        int faceAnchorId = -1;
        const auto anchorIt = faceAnchorIdsByFaceId.find(face.id);
        if (anchorIt != faceAnchorIdsByFaceId.end()) {
            faceAnchorId = anchorIt->second;
        }

        const SectorAuthoringFaceAnchor* anchor = FindSectorAuthoringFaceAnchor(graph, faceAnchorId);
        if (anchor != nullptr && anchor->isVoid) {
            SectorAuthoringResolvedFaceMapping resolvedMapping;
            resolvedMapping.extractedFaceId = face.id;
            resolvedMapping.faceAnchorId = faceAnchorId;
            resolvedMapping.topologySectorId = -1;
            resolvedMapping.kind = SectorAuthoringFaceResolutionKind::VoidNoDerivedSector;
            mapping.resolvedFaces.push_back(resolvedMapping);
            continue;
        }

        SectorTopologySector sector;
        const int topologySectorId = AllocateDerivedSectorId(
                faceAnchorId,
                usedSectorIds,
                reservedSectorIds,
                &nextGeneratedSectorId);
        CopyFaceDefaultsToTopologySector(face, topologySectorId, sector);
        if (anchor != nullptr) {
            CopyFaceAnchorPropertiesToTopologySector(*anchor, sector);
        }
        if (anchor != nullptr) {
            sector.name = ResolveDerivedSectorName(
                    sector.name,
                    usedSectorNames,
                    reservedFaceAnchorNames);
        } else {
            std::set<std::string> unavailableNames = usedSectorNames;
            unavailableNames.insert(reservedFaceAnchorNames.begin(), reservedFaceAnchorNames.end());
            sector.name = NextGeneratedSectorName(unavailableNames);
            usedSectorNames.insert(sector.name);
        }
        topologySectorIdsByFaceId[face.id] = sector.id;
        topology.sectors.push_back(sector);

        SectorAuthoringDerivedSectorMapping sectorMapping;
        sectorMapping.extractedFaceId = face.id;
        sectorMapping.faceAnchorId = faceAnchorId;
        sectorMapping.topologySectorId = sector.id;
        mapping.sectors.push_back(sectorMapping);

        SectorAuthoringResolvedFaceMapping resolvedMapping;
        resolvedMapping.extractedFaceId = face.id;
        resolvedMapping.faceAnchorId = faceAnchorId;
        resolvedMapping.topologySectorId = sector.id;
        resolvedMapping.kind = SectorAuthoringFaceResolutionKind::DerivedSector;
        mapping.resolvedFaces.push_back(resolvedMapping);
    }

    for (const SectorAuthoringExtractedFace& face : faces.faces) {
        const auto sectorIt = topologySectorIdsByFaceId.find(face.id);
        if (sectorIt == topologySectorIdsByFaceId.end()) {
            continue;
        }

        std::vector<SectorAuthoringFaceBoundaryEdge> sectorBoundaryEdges = face.boundary;
        const auto childIt = containment.childFaceIdsByFaceId.find(face.id);
        if (childIt != containment.childFaceIdsByFaceId.end()) {
            const int faceDepth = containment.depthByFaceId.find(face.id) == containment.depthByFaceId.end()
                    ? 0
                    : containment.depthByFaceId.find(face.id)->second;
            for (int childFaceId : childIt->second) {
                const auto childDepthIt = containment.depthByFaceId.find(childFaceId);
                const int childDepth = childDepthIt == containment.depthByFaceId.end() ? 0 : childDepthIt->second;
                if (childDepth != faceDepth + 1) {
                    continue;
                }

                for (const SectorAuthoringExtractedFace& childFace : faces.faces) {
                    if (childFace.id != childFaceId) {
                        continue;
                    }
                    for (const SectorAuthoringFaceBoundaryEdge& childBoundaryEdge : childFace.boundary) {
                        SectorAuthoringFaceBoundaryEdge holeBoundaryEdge = childBoundaryEdge;
                        holeBoundaryEdge.sourceSide = childBoundaryEdge.sourceSide == SectorTopologySideKind::Front
                                ? SectorTopologySideKind::Back
                                : SectorTopologySideKind::Front;
                        sectorBoundaryEdges.push_back(holeBoundaryEdge);
                    }
                }
            }
        }

        for (const SectorAuthoringFaceBoundaryEdge& boundaryEdge : sectorBoundaryEdges) {
            const auto lineIt = topologyLineIdsByPlanarEdgeId.find(boundaryEdge.planarEdgeId);
            if (lineIt == topologyLineIdsByPlanarEdgeId.end()) {
                continue;
            }

            SectorTopologySideDef sideDef;
            sideDef.id = nextSideDefId++;
            sideDef.lineDefId = lineIt->second;
            sideDef.side = boundaryEdge.sourceSide;
            sideDef.sectorId = sectorIt->second;
            if (const SectorTopologySector* sector = FindSectorTopologySector(topology, sideDef.sectorId)) {
                sideDef.wall = sector->defaultWall;
                sideDef.lower = sector->defaultLower;
                sideDef.upper = sector->defaultUpper;
            }
            if (const SectorAuthoringLineSide* authoringSide = FindSectorAuthoringLineSide(
                        graph,
                        SectorAuthoringSideId{boundaryEdge.sourceLineId, boundaryEdge.sourceSide})) {
                CopyAuthoringSidePropertiesToTopologySideDef(*authoringSide, sideDef);
            }
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
            sideMapping.topologyLineDefId = sideDef.lineDefId;
            sideMapping.topologySectorId = sideDef.sectorId;
            mapping.sides.push_back(sideMapping);
        }
    }

    PruneUnusedDerivedTopology(topology, mapping);
}

bool FacePointIsOnBoundary(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringExtractedFace& face,
        double px,
        double py)
{
    for (const SectorAuthoringFaceBoundaryEdge& boundary : face.boundary) {
        const SectorAuthoringPlanarVertex* start = FindPlanarVertex(planar, boundary.startVertexId);
        const SectorAuthoringPlanarVertex* end = FindPlanarVertex(planar, boundary.endVertexId);
        if (start != nullptr && end != nullptr
                && PointIsOnSegment(
                        px,
                        py,
                        PlanarPointX(start->point),
                        PlanarPointY(start->point),
                        PlanarPointX(end->point),
                        PlanarPointY(end->point))) {
            return true;
        }
    }
    return false;
}

void CompileAuthoringFogVolumes(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringFaceExtractionResult& faces,
        const FaceContainmentInfo& containment,
        SectorTopologyMap& topology,
        SectorAuthoringDerivationMapping& mapping,
        std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics)
{
    topology.compiledLocalFogVolumes.reserve(graph.fogVolumes.size());
    mapping.fogVolumes.reserve(graph.fogVolumes.size());

    for (const SectorAuthoringFogVolume& authored : graph.fogVolumes) {
        const double px = static_cast<double>(authored.x);
        const double py = static_cast<double>(authored.y);
        const SectorAuthoringExtractedFace* bestFace = nullptr;
        int bestDepth = std::numeric_limits<int>::min();
        bool onBoundary = false;
        for (const SectorAuthoringExtractedFace& face : faces.faces) {
            if (FacePointIsOnBoundary(planar, face, px, py)) {
                onBoundary = true;
                continue;
            }
            if (!FaceContainsPoint(planar, face, px, py, false)) {
                continue;
            }
            const auto depthIt = containment.depthByFaceId.find(face.id);
            const int depth = depthIt == containment.depthByFaceId.end() ? 0 : depthIt->second;
            if (bestFace == nullptr || depth > bestDepth) {
                bestFace = &face;
                bestDepth = depth;
            }
        }

        SectorAuthoringDerivedFogVolumeMapping fogMapping;
        fogMapping.authoringFogVolumeId = authored.id;
        if (!onBoundary && bestFace != nullptr) {
            fogMapping.extractedFaceId = bestFace->id;
            for (const SectorAuthoringResolvedFaceMapping& faceMapping : mapping.resolvedFaces) {
                if (faceMapping.extractedFaceId == bestFace->id
                        && faceMapping.kind == SectorAuthoringFaceResolutionKind::DerivedSector) {
                    fogMapping.topologySectorId = faceMapping.topologySectorId;
                    fogMapping.resolved = true;
                    break;
                }
            }
        }
        mapping.fogVolumes.push_back(fogMapping);

        if (!fogMapping.resolved) {
            AddDerivationDiagnostic(
                    diagnostics,
                    SectorAuthoringDerivationDiagnosticKind::UnresolvedFogVolume,
                    authored.id,
                    onBoundary
                            ? "Authoring fog volume center lies on a face boundary"
                            : "Authoring fog volume center is not inside a non-void derived face",
                    SectorAuthoringValidationSeverity::Warning);
            continue;
        }

        const SectorTopologySector* sector = FindSectorTopologySector(topology, fogMapping.topologySectorId);
        if (sector == nullptr) {
            continue;
        }
        const SectorAuthoringFogVolume volume = NormalizeSectorAuthoringFogVolume(authored);
        const float bottomWorld = SectorAuthoringToWorldDistance(sector->floorZ)
                + volume.bottomOffsetWorld;
        SectorCompiledLocalFogVolume compiled;
        compiled.sourceAuthoringFogVolumeId = volume.id;
        compiled.topologySectorId = sector->id;
        compiled.enabled = volume.enabled;
        compiled.shape = volume.shape;
        compiled.analyticStyle = volume.analyticStyle;
        compiled.yawRadians = volume.yawDegrees * DEG2RAD;
        compiled.centerWorld = Vector3{
                SectorAuthoringToWorldDistance(
                        static_cast<float>(volume.x) / static_cast<float>(SectorCoordSubdivisions)),
                bottomWorld + volume.heightWorld * 0.5f,
                SectorAuthoringToWorldDistance(
                        static_cast<float>(volume.y) / static_cast<float>(SectorCoordSubdivisions))};
        compiled.radiiWorld = Vector3{
                volume.radiusXWorld,
                volume.heightWorld * 0.5f,
                volume.radiusZWorld};
        compiled.color = volume.color;
        compiled.maxOpacity = volume.maxOpacity;
        compiled.analyticStartDistanceWorld = volume.analyticStartDistanceWorld;
        compiled.analyticEndDistanceWorld = volume.analyticEndDistanceWorld;
        compiled.analyticFalloffExponent = volume.analyticFalloffExponent;
        compiled.edgeSoftness = volume.edgeSoftness;
        compiled.noiseScaleWorld = volume.noiseScaleWorld;
        compiled.noiseAmount = volume.noiseAmount;
        compiled.flowDirectionDegrees = volume.flowDirectionDegrees;
        compiled.flowSpeedWorld = volume.flowSpeedWorld;
        topology.compiledLocalFogVolumes.push_back(compiled);
    }
}

void CompileAuthoringReflectionProbes(
        const SectorAuthoringGraph& graph,
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringFaceExtractionResult& faces,
        const FaceContainmentInfo& containment,
        SectorTopologyMap& topology,
        SectorAuthoringDerivationMapping& mapping,
        std::vector<SectorAuthoringDerivationDiagnostic>& diagnostics)
{
    topology.compiledReflectionProbes.reserve(graph.reflectionProbes.size());
    mapping.reflectionProbes.reserve(graph.reflectionProbes.size());
    for (const SectorAuthoringReflectionProbe& authored : graph.reflectionProbes) {
        const double px = static_cast<double>(authored.x);
        const double py = static_cast<double>(authored.z);
        const SectorAuthoringExtractedFace* bestFace = nullptr;
        int bestDepth = std::numeric_limits<int>::min();
        bool onBoundary = false;
        for (const SectorAuthoringExtractedFace& face : faces.faces) {
            if (FacePointIsOnBoundary(planar, face, px, py)) {
                onBoundary = true;
                continue;
            }
            if (!FaceContainsPoint(planar, face, px, py, false)) continue;
            const auto depthIt = containment.depthByFaceId.find(face.id);
            const int depth = depthIt == containment.depthByFaceId.end() ? 0 : depthIt->second;
            if (bestFace == nullptr || depth > bestDepth) {
                bestFace = &face;
                bestDepth = depth;
            }
        }

        SectorAuthoringDerivedReflectionProbeMapping probeMapping;
        probeMapping.authoringReflectionProbeId = authored.id;
        if (!onBoundary && bestFace != nullptr) {
            probeMapping.extractedFaceId = bestFace->id;
            for (const SectorAuthoringResolvedFaceMapping& faceMapping : mapping.resolvedFaces) {
                if (faceMapping.extractedFaceId == bestFace->id
                        && faceMapping.kind == SectorAuthoringFaceResolutionKind::DerivedSector) {
                    probeMapping.topologySectorId = faceMapping.topologySectorId;
                    probeMapping.resolved = true;
                    break;
                }
            }
        }
        mapping.reflectionProbes.push_back(probeMapping);
        if (!probeMapping.resolved) {
            AddDerivationDiagnostic(
                    diagnostics,
                    SectorAuthoringDerivationDiagnosticKind::UnresolvedReflectionProbe,
                    authored.id,
                    onBoundary
                            ? "Reflection probe capture point lies on a face boundary"
                            : "Reflection probe capture point is not inside a non-void derived face",
                    SectorAuthoringValidationSeverity::Warning);
            continue;
        }

        const SectorAuthoringReflectionProbe probe =
                NormalizeSectorAuthoringReflectionProbe(authored);
        const Vector3 capture{
                SectorAuthoringToWorldDistance(
                        static_cast<float>(probe.x) / static_cast<float>(SectorCoordSubdivisions)),
                probe.yWorld,
                SectorAuthoringToWorldDistance(
                        static_cast<float>(probe.z) / static_cast<float>(SectorCoordSubdivisions))};
        topology.compiledReflectionProbes.push_back(SectorCompiledReflectionProbe{
                probe.id,
                probeMapping.topologySectorId,
                probe.enabled,
                capture,
                Vector3Add(capture, probe.influenceOffsetWorld),
                probe.halfExtentsWorld,
                probe.yawDegrees * DEG2RAD,
                probe.priority,
                probe.intensity,
                probe.resolution});
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

int AllocateSectorAuthoringFogVolumeId(const SectorAuthoringGraph& graph)
{
    return AllocateNextId(graph.fogVolumes);
}

int AllocateSectorAuthoringReflectionProbeId(const SectorAuthoringGraph& graph)
{
    return AllocateNextId(graph.reflectionProbes);
}

int AllocateSectorAuthoringLevelMarkerId(const SectorAuthoringGraph& graph)
{
    return AllocateNextId(graph.levelMarkers);
}

int AllocateSectorAuthoringPatrolId(const SectorAuthoringGraph& graph)
{
    int next = 1;
    for (const SectorAuthoringPatrol& patrol : graph.patrols) {
        if (patrol.editorId >= next
                && patrol.editorId < std::numeric_limits<int>::max()) {
            next = patrol.editorId + 1;
        }
    }
    return next;
}

int AllocateSectorAuthoringSoundEmitterId(const SectorAuthoringGraph& graph)
{
    return AllocateNextId(graph.soundEmitters);
}

int AllocateSectorAuthoringTriggerId(const SectorAuthoringGraph& graph)
{
    int next = 1;
    for (const SectorAuthoringTrigger& trigger : graph.triggers) {
        if (trigger.editorId >= next && trigger.editorId < std::numeric_limits<int>::max()) {
            next = trigger.editorId + 1;
        }
    }
    return next;
}

int AllocateSectorAuthoringStructuralPrimitiveId(const SectorAuthoringGraph& graph)
{
    return AllocateNextId(graph.structuralPrimitives);
}

const SectorAuthoringStructuralPrimitive* FindSectorAuthoringStructuralPrimitive(
        const SectorAuthoringGraph& graph,
        int id)
{
    const auto found = std::find_if(
            graph.structuralPrimitives.begin(),
            graph.structuralPrimitives.end(),
            [id](const SectorAuthoringStructuralPrimitive& primitive) {
                return primitive.id == id;
            });
    return found == graph.structuralPrimitives.end() ? nullptr : &*found;
}

SectorAuthoringStructuralPrimitive* FindSectorAuthoringStructuralPrimitive(
        SectorAuthoringGraph& graph,
        int id)
{
    return const_cast<SectorAuthoringStructuralPrimitive*>(
            FindSectorAuthoringStructuralPrimitive(
                    static_cast<const SectorAuthoringGraph&>(graph), id));
}

bool IsValidSectorAuthoringLevelMarkerReferenceId(const std::string& id)
{
    if (id.empty() || id.size() > 63) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](char ch) {
        return (ch >= 'A' && ch <= 'Z')
                || (ch >= 'a' && ch <= 'z')
                || (ch >= '0' && ch <= '9')
                || ch == '_'
                || ch == '-';
    });
}

bool IsValidSectorAuthoringSoundEmitterReferenceId(const std::string& id)
{
    return IsValidSectorAuthoringLevelMarkerReferenceId(id);
}

bool IsValidSectorAuthoringPatrolReferenceId(const std::string& id)
{
    return IsValidSectorAuthoringLevelMarkerReferenceId(id);
}

bool IsValidSectorTriggerReferenceId(const std::string& id)
{
    return IsValidSectorAuthoringLevelMarkerReferenceId(id);
}

bool IsValidSectorTriggerScriptName(const std::string& name)
{
    if (name.empty()) return true;
    const auto head = [](char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
    };
    if (!head(name.front())) return false;
    return std::all_of(name.begin() + 1, name.end(), [head](char ch) {
        return head(ch) || (ch >= '0' && ch <= '9');
    });
}

std::string AllocateSectorAuthoringLevelMarkerReferenceId(const SectorAuthoringGraph& graph)
{
    if (graph.levelMarkers.empty()
            && FindSectorAuthoringLevelMarkerByReferenceId(graph, "default") == nullptr) {
        return "default";
    }
    for (int suffix = 1; suffix < std::numeric_limits<int>::max(); ++suffix) {
        const std::string candidate = "marker_" + std::to_string(suffix);
        if (FindSectorAuthoringLevelMarkerByReferenceId(graph, candidate) == nullptr) {
            return candidate;
        }
    }
    return {};
}

std::string AllocateSectorAuthoringPatrolReferenceId(
        const SectorAuthoringGraph& graph)
{
    for (int suffix = 1; suffix < std::numeric_limits<int>::max(); ++suffix) {
        const std::string candidate = suffix == 1
                ? "patrol" : "patrol_" + std::to_string(suffix);
        if (FindSectorAuthoringPatrolByReferenceId(graph, candidate) == nullptr) {
            return candidate;
        }
    }
    return {};
}

std::string AllocateSectorAuthoringTriggerReferenceId(const SectorAuthoringGraph& graph)
{
    for (int suffix = 1; suffix < std::numeric_limits<int>::max(); ++suffix) {
        const std::string candidate = "trigger_" + std::to_string(suffix);
        if (FindSectorAuthoringTriggerByReferenceId(graph, candidate) == nullptr) return candidate;
    }
    return {};
}

std::string AllocateSectorAuthoringSoundEmitterReferenceId(
        const SectorAuthoringGraph& graph)
{
    for (int suffix = 1; suffix < std::numeric_limits<int>::max(); ++suffix) {
        const std::string candidate = "sound_emitter_" + std::to_string(suffix);
        if (FindSectorAuthoringSoundEmitterByReferenceId(graph, candidate) == nullptr) {
            return candidate;
        }
    }
    return {};
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

const SectorAuthoringFogVolume* FindSectorAuthoringFogVolume(
        const SectorAuthoringGraph& graph,
        int id)
{
    return FindById(graph.fogVolumes, id);
}

SectorAuthoringFogVolume* FindSectorAuthoringFogVolume(SectorAuthoringGraph& graph, int id)
{
    return FindById(graph.fogVolumes, id);
}

const SectorAuthoringReflectionProbe* FindSectorAuthoringReflectionProbe(
        const SectorAuthoringGraph& graph,
        int id)
{
    return FindById(graph.reflectionProbes, id);
}

SectorAuthoringReflectionProbe* FindSectorAuthoringReflectionProbe(
        SectorAuthoringGraph& graph,
        int id)
{
    return FindById(graph.reflectionProbes, id);
}

SectorAuthoringReflectionProbe NormalizeSectorAuthoringReflectionProbe(
        SectorAuthoringReflectionProbe probe)
{
    const SectorAuthoringReflectionProbe defaults;
    auto finiteClamp = [](float value, float minimum, float maximum, float fallback) {
        return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
    };
    probe.yWorld = finiteClamp(probe.yWorld, -128.0f, 128.0f, defaults.yWorld);
    probe.yawDegrees = std::isfinite(probe.yawDegrees)
            ? std::fmod(probe.yawDegrees, 360.0f) : defaults.yawDegrees;
    if (probe.yawDegrees < 0.0f) probe.yawDegrees += 360.0f;
    probe.influenceOffsetWorld.x = finiteClamp(
            probe.influenceOffsetWorld.x, -128.0f, 128.0f, 0.0f);
    probe.influenceOffsetWorld.y = finiteClamp(
            probe.influenceOffsetWorld.y, -128.0f, 128.0f, 0.0f);
    probe.influenceOffsetWorld.z = finiteClamp(
            probe.influenceOffsetWorld.z, -128.0f, 128.0f, 0.0f);
    probe.halfExtentsWorld.x = finiteClamp(
            probe.halfExtentsWorld.x, ReflectionProbeExtentMin,
            ReflectionProbeExtentMax, defaults.halfExtentsWorld.x);
    probe.halfExtentsWorld.y = finiteClamp(
            probe.halfExtentsWorld.y, ReflectionProbeExtentMin,
            ReflectionProbeExtentMax, defaults.halfExtentsWorld.y);
    probe.halfExtentsWorld.z = finiteClamp(
            probe.halfExtentsWorld.z, ReflectionProbeExtentMin,
            ReflectionProbeExtentMax, defaults.halfExtentsWorld.z);
    probe.intensity = finiteClamp(probe.intensity, 0.0f, 8.0f, defaults.intensity);
    if (probe.resolution != 64 && probe.resolution != 128 && probe.resolution != 256) {
        probe.resolution = defaults.resolution;
    }
    probe.priority = std::clamp(probe.priority, -1000, 1000);
    return probe;
}

const SectorAuthoringLevelMarker* FindSectorAuthoringLevelMarker(
        const SectorAuthoringGraph& graph,
        int id)
{
    return FindById(graph.levelMarkers, id);
}

SectorAuthoringLevelMarker* FindSectorAuthoringLevelMarker(
        SectorAuthoringGraph& graph,
        int id)
{
    return FindById(graph.levelMarkers, id);
}

const SectorAuthoringLevelMarker* FindSectorAuthoringLevelMarkerByReferenceId(
        const SectorAuthoringGraph& graph,
        const std::string& referenceId)
{
    for (const SectorAuthoringLevelMarker& marker : graph.levelMarkers) {
        if (marker.referenceId == referenceId) {
            return &marker;
        }
    }
    return nullptr;
}

const SectorAuthoringPatrol* FindSectorAuthoringPatrol(
        const SectorAuthoringGraph& graph,
        int editorId)
{
    if (!IsValidSectorAuthoringId(editorId)) return nullptr;
    for (const SectorAuthoringPatrol& patrol : graph.patrols) {
        if (patrol.editorId == editorId) return &patrol;
    }
    return nullptr;
}

SectorAuthoringPatrol* FindSectorAuthoringPatrol(
        SectorAuthoringGraph& graph,
        int editorId)
{
    return const_cast<SectorAuthoringPatrol*>(FindSectorAuthoringPatrol(
            static_cast<const SectorAuthoringGraph&>(graph), editorId));
}

const SectorAuthoringPatrol* FindSectorAuthoringPatrolByReferenceId(
        const SectorAuthoringGraph& graph,
        const std::string& id)
{
    for (const SectorAuthoringPatrol& patrol : graph.patrols) {
        if (patrol.id == id) return &patrol;
    }
    return nullptr;
}

const SectorAuthoringSoundEmitter* FindSectorAuthoringSoundEmitter(
        const SectorAuthoringGraph& graph,
        int id)
{
    return FindById(graph.soundEmitters, id);
}

SectorAuthoringSoundEmitter* FindSectorAuthoringSoundEmitter(
        SectorAuthoringGraph& graph,
        int id)
{
    return FindById(graph.soundEmitters, id);
}

const SectorAuthoringSoundEmitter* FindSectorAuthoringSoundEmitterByReferenceId(
        const SectorAuthoringGraph& graph,
        const std::string& referenceId)
{
    for (const SectorAuthoringSoundEmitter& emitter : graph.soundEmitters) {
        if (emitter.referenceId == referenceId) return &emitter;
    }
    return nullptr;
}

const SectorAuthoringTrigger* FindSectorAuthoringTrigger(
        const SectorAuthoringGraph& graph,
        int editorId)
{
    for (const SectorAuthoringTrigger& trigger : graph.triggers) {
        if (trigger.editorId == editorId) return &trigger;
    }
    return nullptr;
}

SectorAuthoringTrigger* FindSectorAuthoringTrigger(SectorAuthoringGraph& graph, int editorId)
{
    for (SectorAuthoringTrigger& trigger : graph.triggers) {
        if (trigger.editorId == editorId) return &trigger;
    }
    return nullptr;
}

const SectorAuthoringTrigger* FindSectorAuthoringTriggerByReferenceId(
        const SectorAuthoringGraph& graph,
        const std::string& id)
{
    for (const SectorAuthoringTrigger& trigger : graph.triggers) {
        if (trigger.id == id) return &trigger;
    }
    return nullptr;
}

SectorAuthoringFogVolume NormalizeSectorAuthoringFogVolume(SectorAuthoringFogVolume volume)
{
    const SectorAuthoringFogVolume defaults;
    if (volume.shape != SectorLocalFogShape::Ellipsoid
            && volume.shape != SectorLocalFogShape::Box) {
        volume.shape = defaults.shape;
    }
    if (volume.analyticStyle != SectorAnalyticFogStyle::Cloudy
            && volume.analyticStyle != SectorAnalyticFogStyle::Room) {
        volume.analyticStyle = defaults.analyticStyle;
    }
    if (!std::isfinite(volume.yawDegrees)) {
        volume.yawDegrees = defaults.yawDegrees;
    }
    volume.yawDegrees = std::fmod(volume.yawDegrees, 360.0f);
    if (volume.yawDegrees < 0.0f) {
        volume.yawDegrees += 360.0f;
    }
    volume.bottomOffsetWorld = ClampFiniteFogValue(
            volume.bottomOffsetWorld, FogBottomOffsetMin, FogBottomOffsetMax, defaults.bottomOffsetWorld);
    volume.radiusXWorld = ClampFiniteFogValue(
            volume.radiusXWorld, FogRadiusMin, FogRadiusMax, defaults.radiusXWorld);
    volume.radiusZWorld = ClampFiniteFogValue(
            volume.radiusZWorld, FogRadiusMin, FogRadiusMax, defaults.radiusZWorld);
    volume.heightWorld = ClampFiniteFogValue(
            volume.heightWorld, FogHeightMin, FogHeightMax, defaults.heightWorld);
    volume.color.a = 255;
    volume.maxOpacity = ClampFiniteFogValue(
            volume.maxOpacity, FogOpacityMin, FogOpacityMax, defaults.maxOpacity);
    volume.analyticStartDistanceWorld = ClampFiniteFogValue(
            volume.analyticStartDistanceWorld, 0.0f, 128.0f, defaults.analyticStartDistanceWorld);
    volume.analyticEndDistanceWorld = ClampFiniteFogValue(
            volume.analyticEndDistanceWorld,
            volume.analyticStartDistanceWorld + 0.01f,
            128.0f,
            std::max(defaults.analyticEndDistanceWorld,
                    volume.analyticStartDistanceWorld + 0.01f));
    volume.analyticFalloffExponent = ClampFiniteFogValue(
            volume.analyticFalloffExponent, 0.05f, 8.0f, defaults.analyticFalloffExponent);
    volume.edgeSoftness = ClampFiniteFogValue(
            volume.edgeSoftness, FogSoftnessMin, FogSoftnessMax, defaults.edgeSoftness);
    volume.noiseScaleWorld = ClampFiniteFogValue(
            volume.noiseScaleWorld, FogNoiseScaleMin, FogNoiseScaleMax, defaults.noiseScaleWorld);
    volume.noiseAmount = ClampFiniteFogValue(
            volume.noiseAmount, FogNoiseAmountMin, FogNoiseAmountMax, defaults.noiseAmount);
    volume.flowSpeedWorld = ClampFiniteFogValue(
            volume.flowSpeedWorld, FogFlowSpeedMin, FogFlowSpeedMax, defaults.flowSpeedWorld);
    if (!std::isfinite(volume.flowDirectionDegrees)) {
        volume.flowDirectionDegrees = defaults.flowDirectionDegrees;
    }
    volume.flowDirectionDegrees = std::fmod(volume.flowDirectionDegrees, 360.0f);
    if (volume.flowDirectionDegrees < 0.0f) {
        volume.flowDirectionDegrees += 360.0f;
    }
    return volume;
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

bool InsertSectorAuthoringVertexOnLine(
        SectorAuthoringGraph& graph,
        int lineId,
        SectorTopologyCoordPoint point,
        SectorAuthoringInsertVertexResult* outResult)
{
    SectorAuthoringInsertVertexResult result;
    result.status = SectorAuthoringInsertVertexStatus::InvalidLine;

    const SectorAuthoringLine* originalLine = FindSectorAuthoringLine(graph, lineId);
    if (originalLine == nullptr) {
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    const SectorAuthoringVertex* start =
            FindSectorAuthoringVertex(graph, originalLine->startVertexId);
    const SectorAuthoringVertex* end =
            FindSectorAuthoringVertex(graph, originalLine->endVertexId);
    if (start == nullptr || end == nullptr || originalLine->startVertexId == originalLine->endVertexId) {
        result.status = SectorAuthoringInsertVertexStatus::InvalidEndpoint;
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    if (!PointOnSegmentInclusive(point, *start, *end)) {
        result.status = SectorAuthoringInsertVertexStatus::OffLine;
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    if ((point.x == start->x && point.y == start->y)
            || (point.x == end->x && point.y == end->y)) {
        result.status = SectorAuthoringInsertVertexStatus::Endpoint;
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }

    SectorAuthoringGraph candidate = graph;
    SectorAuthoringLine* candidateLine = FindSectorAuthoringLine(candidate, lineId);
    if (candidateLine == nullptr) {
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }
    const SectorAuthoringLine lineCopy = *candidateLine;

    std::vector<SectorAuthoringLineSide> sideCopies;
    for (const SectorAuthoringLineSide& side : candidate.lineSides) {
        if (side.id.lineId == lineId) {
            sideCopies.push_back(side);
        }
    }

    int insertedVertexId = -1;
    if (FindAuthoringVertexIdAtPoint(candidate, point, &insertedVertexId)) {
        if (insertedVertexId == lineCopy.startVertexId || insertedVertexId == lineCopy.endVertexId) {
            result.status = SectorAuthoringInsertVertexStatus::Endpoint;
            if (outResult != nullptr) {
                *outResult = result;
            }
            return false;
        }
        result.reusedExistingVertex = true;
    } else {
        insertedVertexId = AllocateSectorAuthoringVertexId(candidate);
        if (!IsValidSectorAuthoringId(insertedVertexId)) {
            result.status = SectorAuthoringInsertVertexStatus::IdAllocationFailed;
            if (outResult != nullptr) {
                *outResult = result;
            }
            return false;
        }
        SectorAuthoringVertex vertex;
        vertex.id = insertedVertexId;
        vertex.x = point.x;
        vertex.y = point.y;
        candidate.vertices.push_back(vertex);
        result.reusedExistingVertex = false;
    }

    const int firstLineId = AllocateSectorAuthoringLineId(candidate);
    if (!IsValidSectorAuthoringId(firstLineId)) {
        result.status = SectorAuthoringInsertVertexStatus::IdAllocationFailed;
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }
    SectorAuthoringLine firstLine = lineCopy;
    firstLine.id = firstLineId;
    firstLine.startVertexId = lineCopy.startVertexId;
    firstLine.endVertexId = insertedVertexId;
    candidate.lines.push_back(firstLine);

    const int secondLineId = AllocateSectorAuthoringLineId(candidate);
    if (!IsValidSectorAuthoringId(secondLineId)) {
        result.status = SectorAuthoringInsertVertexStatus::IdAllocationFailed;
        if (outResult != nullptr) {
            *outResult = result;
        }
        return false;
    }
    SectorAuthoringLine secondLine = lineCopy;
    secondLine.id = secondLineId;
    secondLine.startVertexId = insertedVertexId;
    secondLine.endVertexId = lineCopy.endVertexId;
    candidate.lines.push_back(secondLine);

    candidate.lines.erase(
            std::remove_if(
                    candidate.lines.begin(),
                    candidate.lines.end(),
                    [lineId](const SectorAuthoringLine& line) {
                        return line.id == lineId;
                    }),
            candidate.lines.end());
    candidate.lineSides.erase(
            std::remove_if(
                    candidate.lineSides.begin(),
                    candidate.lineSides.end(),
                    [lineId](const SectorAuthoringLineSide& side) {
                        return side.id.lineId == lineId;
                    }),
            candidate.lineSides.end());

    for (const SectorAuthoringLineSide& oldSide : sideCopies) {
        SectorAuthoringLineSide firstSide = oldSide;
        firstSide.id.lineId = firstLineId;
        candidate.lineSides.push_back(firstSide);

        SectorAuthoringLineSide secondSide = oldSide;
        secondSide.id.lineId = secondLineId;
        candidate.lineSides.push_back(secondSide);
    }

    result.status = SectorAuthoringInsertVertexStatus::Inserted;
    result.vertexId = insertedVertexId;
    result.firstLineId = firstLineId;
    result.secondLineId = secondLineId;
    graph = std::move(candidate);
    if (outResult != nullptr) {
        *outResult = result;
    }
    return true;
}

std::vector<SectorAuthoringValidationIssue> ValidateSectorAuthoringGraphReferences(
        const SectorAuthoringGraph& graph)
{
    std::vector<SectorAuthoringValidationIssue> issues;

    const std::vector<SectorStructuralDiagnostic> structuralDiagnostics =
            ValidateSectorAuthoringStructuralPrimitives(graph.structuralPrimitives);
    for (const SectorStructuralDiagnostic& diagnostic : structuralDiagnostics) {
        issues.push_back(SectorAuthoringValidationIssue{
                diagnostic.severity == SectorStructuralDiagnosticSeverity::Error
                        ? SectorAuthoringValidationSeverity::Error
                        : SectorAuthoringValidationSeverity::Warning,
                SectorAuthoringObjectKind::StructuralPrimitive,
                diagnostic.primitiveId,
                diagnostic.message});
    }

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

    std::set<int> fogVolumeIds;
    for (const SectorAuthoringFogVolume& volume : graph.fogVolumes) {
        if (!IsValidSectorAuthoringId(volume.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::FogVolume, volume.id, "Invalid authoring fog volume ID");
        } else if (!fogVolumeIds.insert(volume.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::FogVolume, volume.id, "Duplicate authoring fog volume ID");
        }
        const float values[] = {
                volume.yawDegrees,
                volume.bottomOffsetWorld,
                volume.radiusXWorld,
                volume.radiusZWorld,
                volume.heightWorld,
                volume.maxOpacity,
                volume.analyticStartDistanceWorld,
                volume.analyticEndDistanceWorld,
                volume.analyticFalloffExponent,
                volume.edgeSoftness,
                volume.noiseScaleWorld,
                volume.noiseAmount,
                volume.flowDirectionDegrees,
                volume.flowSpeedWorld};
        if (std::any_of(std::begin(values), std::end(values), [](float value) { return !std::isfinite(value); })) {
            AddIssue(issues, SectorAuthoringObjectKind::FogVolume, volume.id, "Authoring fog volume has non-finite settings");
        } else if ((volume.shape != SectorLocalFogShape::Ellipsoid
                            && volume.shape != SectorLocalFogShape::Box)
                || (volume.analyticStyle != SectorAnalyticFogStyle::Cloudy
                            && volume.analyticStyle != SectorAnalyticFogStyle::Room)
                || volume.yawDegrees < 0.0f || volume.yawDegrees >= 360.0f
                || volume.radiusXWorld < FogRadiusMin || volume.radiusXWorld > FogRadiusMax
                || volume.radiusZWorld < FogRadiusMin || volume.radiusZWorld > FogRadiusMax
                || volume.heightWorld < FogHeightMin || volume.heightWorld > FogHeightMax
                || volume.maxOpacity < FogOpacityMin || volume.maxOpacity > FogOpacityMax
                || volume.analyticStartDistanceWorld < 0.0f
                || volume.analyticEndDistanceWorld <= volume.analyticStartDistanceWorld
                || volume.analyticEndDistanceWorld > 128.0f
                || volume.analyticFalloffExponent < 0.05f
                || volume.analyticFalloffExponent > 8.0f
                || volume.edgeSoftness < FogSoftnessMin || volume.edgeSoftness > FogSoftnessMax
                || volume.noiseScaleWorld < FogNoiseScaleMin || volume.noiseScaleWorld > FogNoiseScaleMax
                || volume.noiseAmount < FogNoiseAmountMin || volume.noiseAmount > FogNoiseAmountMax
                || volume.flowSpeedWorld < FogFlowSpeedMin || volume.flowSpeedWorld > FogFlowSpeedMax
                || volume.bottomOffsetWorld < FogBottomOffsetMin || volume.bottomOffsetWorld > FogBottomOffsetMax) {
            AddIssue(issues, SectorAuthoringObjectKind::FogVolume, volume.id, "Authoring fog volume settings are outside supported ranges");
        }
    }

    std::set<int> reflectionProbeIds;
    for (const SectorAuthoringReflectionProbe& source : graph.reflectionProbes) {
        if (!IsValidSectorAuthoringId(source.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::ReflectionProbe, source.id,
                    "Invalid reflection probe ID");
        } else if (!reflectionProbeIds.insert(source.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::ReflectionProbe, source.id,
                    "Duplicate reflection probe ID");
        }
        const SectorAuthoringReflectionProbe probe =
                NormalizeSectorAuthoringReflectionProbe(source);
        if (probe.yWorld != source.yWorld || probe.yawDegrees != source.yawDegrees
                || probe.influenceOffsetWorld.x != source.influenceOffsetWorld.x
                || probe.influenceOffsetWorld.y != source.influenceOffsetWorld.y
                || probe.influenceOffsetWorld.z != source.influenceOffsetWorld.z
                || probe.halfExtentsWorld.x != source.halfExtentsWorld.x
                || probe.halfExtentsWorld.y != source.halfExtentsWorld.y
                || probe.halfExtentsWorld.z != source.halfExtentsWorld.z
                || probe.priority != source.priority
                || probe.intensity != source.intensity
                || probe.resolution != source.resolution) {
            AddIssue(issues, SectorAuthoringObjectKind::ReflectionProbe, source.id,
                    "Reflection probe settings are outside supported ranges");
        }
    }

    std::set<int> markerIds;
    std::set<std::string> markerReferenceIds;
    for (const SectorAuthoringLevelMarker& marker : graph.levelMarkers) {
        if (!IsValidSectorAuthoringId(marker.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::LevelMarker, marker.id,
                     "Invalid authoring level marker ID");
        } else if (!markerIds.insert(marker.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::LevelMarker, marker.id,
                     "Duplicate authoring level marker ID");
        }
        if (!IsValidSectorAuthoringLevelMarkerReferenceId(marker.referenceId)) {
            AddIssue(issues, SectorAuthoringObjectKind::LevelMarker, marker.id,
                     "Level marker reference ID must contain 1-63 letters, digits, underscores, or dashes");
        } else if (!markerReferenceIds.insert(marker.referenceId).second) {
            AddIssue(issues, SectorAuthoringObjectKind::LevelMarker, marker.id,
                     "Duplicate level marker reference ID");
        }
        if (!std::isfinite(marker.y) || !std::isfinite(marker.orientationDegrees)) {
            AddIssue(issues, SectorAuthoringObjectKind::LevelMarker, marker.id,
                     "Authoring level marker transform must be finite");
        }
    }

    std::set<int> patrolEditorIds;
    std::set<std::string> patrolReferenceIds;
    for (const SectorAuthoringPatrol& patrol : graph.patrols) {
        if (!IsValidSectorAuthoringId(patrol.editorId)) {
            AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                    "Invalid authoring patrol editor ID");
        } else if (!patrolEditorIds.insert(patrol.editorId).second) {
            AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                    "Duplicate authoring patrol editor ID");
        }
        if (!IsValidSectorAuthoringPatrolReferenceId(patrol.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                    "Patrol ID must contain 1-63 letters, digits, underscores, or dashes");
        } else if (!patrolReferenceIds.insert(patrol.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                    "Duplicate patrol ID");
        }
        if (patrol.waypoints.empty()) {
            AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                    "Patrol must contain at least one waypoint");
        }
        if (patrol.mode != SectorPatrolMode::Once
                && patrol.mode != SectorPatrolMode::Loop
                && patrol.mode != SectorPatrolMode::PingPong) {
            AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                    "Patrol has an invalid playback mode");
        }
        if (patrol.shuffleWaypoints
                && patrol.mode == SectorPatrolMode::PingPong) {
            AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                    "Shuffled patrols cannot use ping-pong playback");
        }
        for (const SectorAuthoringPatrolWaypoint& waypoint : patrol.waypoints) {
            if (FindSectorAuthoringLevelMarker(graph, waypoint.levelMarkerId) == nullptr) {
                AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                        "Patrol references a missing Level Marker");
            }
            if (waypoint.delayMilliseconds < 0) {
                AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                        "Patrol waypoint delay must be non-negative");
            }
            if (waypoint.gait != SectorPatrolGait::Walk
                    && waypoint.gait != SectorPatrolGait::Run) {
                AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                        "Patrol waypoint has an invalid gait");
            }
            if (!std::isfinite(waypoint.lookArcDegrees)
                    || waypoint.lookArcDegrees < 0.0f
                    || waypoint.lookArcDegrees > 360.0f) {
                AddIssue(issues, SectorAuthoringObjectKind::Patrol, patrol.editorId,
                        "Patrol waypoint look arc must be between 0 and 360 degrees");
            }
        }
    }

    std::set<int> emitterIds;
    std::set<std::string> emitterReferenceIds;
    for (const SectorAuthoringSoundEmitter& emitter : graph.soundEmitters) {
        if (!IsValidSectorAuthoringId(emitter.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::SoundEmitter, emitter.id,
                     "Invalid authoring sound emitter ID");
        } else if (!emitterIds.insert(emitter.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::SoundEmitter, emitter.id,
                     "Duplicate authoring sound emitter ID");
        }
        if (!IsValidSectorAuthoringSoundEmitterReferenceId(emitter.referenceId)) {
            AddIssue(issues, SectorAuthoringObjectKind::SoundEmitter, emitter.id,
                     "Sound emitter ID must contain 1-63 letters, digits, underscores, or dashes");
        } else if (!emitterReferenceIds.insert(emitter.referenceId).second) {
            AddIssue(issues, SectorAuthoringObjectKind::SoundEmitter, emitter.id,
                     "Duplicate sound emitter ID");
        }
        if (!std::isfinite(emitter.y) || !std::isfinite(emitter.volume)
                || emitter.volume < 0.0f || emitter.volume > 1.0f) {
            AddIssue(issues, SectorAuthoringObjectKind::SoundEmitter, emitter.id,
                     "Sound emitter transform and volume must be finite and volume must be between 0 and 1");
        }
    }

    std::set<int> triggerEditorIds;
    std::set<std::string> triggerReferenceIds;
    for (const SectorAuthoringTrigger& trigger : graph.triggers) {
        if (!IsValidSectorAuthoringId(trigger.editorId)) {
            AddIssue(issues, SectorAuthoringObjectKind::Trigger, trigger.editorId,
                     "Invalid authoring trigger editor ID");
        } else if (!triggerEditorIds.insert(trigger.editorId).second) {
            AddIssue(issues, SectorAuthoringObjectKind::Trigger, trigger.editorId,
                     "Duplicate authoring trigger editor ID");
        }
        if (!IsValidSectorTriggerReferenceId(trigger.id)) {
            AddIssue(issues, SectorAuthoringObjectKind::Trigger, trigger.editorId,
                     "Trigger ID must contain 1-63 letters, digits, underscores, or dashes");
        } else if (!triggerReferenceIds.insert(trigger.id).second) {
            AddIssue(issues, SectorAuthoringObjectKind::Trigger, trigger.editorId,
                     "Duplicate trigger ID");
        }
        if (trigger.delayMilliseconds < 0) {
            AddIssue(issues, SectorAuthoringObjectKind::Trigger, trigger.editorId,
                     "Trigger delay must be non-negative");
        }
        if (!IsValidSectorTriggerScriptName(trigger.script)) {
            AddIssue(issues, SectorAuthoringObjectKind::Trigger, trigger.editorId,
                     "Trigger script must be empty or a valid Lua global identifier");
        }
        std::string geometryError;
        if (!ValidateSectorTriggerPolygon(trigger.points, trigger.shape, &geometryError)) {
            AddIssue(issues, SectorAuthoringObjectKind::Trigger, trigger.editorId,
                     "Invalid trigger geometry: " + geometryError);
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
    result.topology.audioSettings = graph.audioSettings;

    const std::vector<SectorAuthoringValidationIssue> referenceIssues =
            ValidateSectorAuthoringGraphReferences(graph);
    for (const SectorAuthoringValidationIssue& issue : referenceIssues) {
        const SectorAuthoringDerivationDiagnosticKind kind =
                issue.objectKind == SectorAuthoringObjectKind::Side
                ? SectorAuthoringDerivationDiagnosticKind::InvalidSideProjection
                : SectorAuthoringDerivationDiagnosticKind::AuthoringReference;
        AddDerivationDiagnostic(
                result.diagnostics,
                kind,
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
                MapPlanarDiagnosticKind(diagnostic.kind),
                diagnostic.lineId,
                diagnostic.message,
                diagnostic.severity,
                diagnostic.otherLineId);
    }
    if (HasDerivationErrors(result.diagnostics)) {
        return result;
    }

    result.faces = ExtractSectorAuthoringFaces(result.planar);
    for (const SectorAuthoringFaceDiagnostic& diagnostic : result.faces.diagnostics) {
        AddFaceDerivationDiagnostic(result.planar, diagnostic, result.diagnostics);
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

    const FaceContainmentInfo faceContainment = BuildFaceContainmentInfo(result.planar, result.faces);
    AddUnsupportedNestedFaceDiagnostics(result.faces, faceContainment, result.diagnostics);
    if (HasDerivationErrors(result.diagnostics)) {
        return result;
    }

    const std::map<int, int> faceAnchorIdsByFaceId = ResolveFaceAnchorsByFaceId(
            graph,
            result.planar,
            result.faces,
            faceContainment,
            result.diagnostics);
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
            faceContainment,
            topologyVertexIdsByPlanarVertexId,
            faceAnchorIdsByFaceId,
            result.topology,
            result.mapping);

    CompileAuthoringFogVolumes(
            graph,
            result.planar,
            result.faces,
            faceContainment,
            result.topology,
            result.mapping,
            result.diagnostics);
    CompileAuthoringReflectionProbes(
            graph,
            result.planar,
            result.faces,
            faceContainment,
            result.topology,
            result.mapping,
            result.diagnostics);

    result.topology.levelMarkers.reserve(graph.levelMarkers.size());
    constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
    for (const SectorAuthoringLevelMarker& marker : graph.levelMarkers) {
        SectorCompiledLevelMarker compiled;
        compiled.sourceAuthoringMarkerId = marker.id;
        compiled.id = marker.referenceId;
        compiled.position = Vector3{
                SectorCoordToVisibleAuthoring(marker.x),
                marker.y,
                SectorCoordToVisibleAuthoring(marker.z)};
        compiled.yawRadians = marker.orientationDegrees * DegreesToRadians;
        result.topology.levelMarkers.push_back(std::move(compiled));
    }

    result.topology.patrols.reserve(graph.patrols.size());
    for (const SectorAuthoringPatrol& patrol : graph.patrols) {
        SectorCompiledPatrol compiled;
        compiled.sourceAuthoringPatrolId = patrol.editorId;
        compiled.id = patrol.id;
        compiled.mode = patrol.mode;
        compiled.waypoints.reserve(patrol.waypoints.size());
        for (const SectorAuthoringPatrolWaypoint& waypoint : patrol.waypoints) {
            compiled.waypoints.push_back(SectorCompiledPatrolWaypoint{
                    waypoint.levelMarkerId,
                    waypoint.delayMilliseconds,
                    waypoint.gait,
                    waypoint.lookAround,
                    waypoint.lookArcDegrees});
        }
        compiled.shuffleWaypoints = patrol.shuffleWaypoints;
        compiled.faceWaypointOrientation = patrol.faceWaypointOrientation;
        result.topology.patrols.push_back(std::move(compiled));
    }

    result.topology.soundEmitters.reserve(graph.soundEmitters.size());
    for (const SectorAuthoringSoundEmitter& emitter : graph.soundEmitters) {
        SectorCompiledSoundEmitter compiled;
        compiled.sourceAuthoringEmitterId = emitter.id;
        compiled.id = emitter.referenceId;
        compiled.positionWorld = SectorAuthoringToWorldPosition(Vector3{
                SectorCoordToVisibleAuthoring(emitter.x),
                emitter.y,
                SectorCoordToVisibleAuthoring(emitter.z)});
        compiled.soundId = emitter.soundId;
        compiled.volume = emitter.volume;
        compiled.loop = emitter.loop;
        result.topology.soundEmitters.push_back(std::move(compiled));
    }

    result.topology.triggers.reserve(graph.triggers.size());
    for (const SectorAuthoringTrigger& trigger : graph.triggers) {
        result.topology.triggers.push_back(SectorCompiledTrigger{
                trigger.editorId,
                trigger.id,
                trigger.shape,
                trigger.points,
                trigger.enabled,
                trigger.repeat,
                trigger.delayMilliseconds,
                trigger.script});
    }

    std::vector<SectorStructuralDiagnostic> structuralDiagnostics;
    if (!CompileSectorStructuralPrimitives(
                graph.structuralPrimitives,
                result.topology,
                result.topology.compiledStructuralPrimitives,
                structuralDiagnostics)) {
        for (const SectorStructuralDiagnostic& diagnostic : structuralDiagnostics) {
            AddDerivationDiagnostic(
                    result.diagnostics,
                    SectorAuthoringDerivationDiagnosticKind::InvalidStructuralPrimitive,
                    diagnostic.primitiveId,
                    diagnostic.message,
                    diagnostic.severity == SectorStructuralDiagnosticSeverity::Error
                            ? SectorAuthoringValidationSeverity::Error
                            : SectorAuthoringValidationSeverity::Warning);
        }
        result.topology = SectorTopologyMap{};
        result.mapping = SectorAuthoringDerivationMapping{};
        return result;
    }
    for (const SectorStructuralDiagnostic& diagnostic : structuralDiagnostics) {
        AddDerivationDiagnostic(
                result.diagnostics,
                diagnostic.severity == SectorStructuralDiagnosticSeverity::Warning
                        ? SectorAuthoringDerivationDiagnosticKind::UnresolvedStructuralPrimitiveMembership
                        : SectorAuthoringDerivationDiagnosticKind::InvalidStructuralPrimitive,
                diagnostic.primitiveId,
                diagnostic.message,
                diagnostic.severity == SectorStructuralDiagnosticSeverity::Error
                        ? SectorAuthoringValidationSeverity::Error
                        : SectorAuthoringValidationSeverity::Warning);
    }
    result.mapping.structuralPrimitives.reserve(
            result.topology.compiledStructuralPrimitives.size());
    for (size_t index = 0;
            index < result.topology.compiledStructuralPrimitives.size();
            ++index) {
        const SectorCompiledStructuralPrimitive& primitive =
                result.topology.compiledStructuralPrimitives[index];
        result.mapping.structuralPrimitives.push_back(
                SectorAuthoringDerivedStructuralPrimitiveMapping{
                        primitive.sourceAuthoringPrimitiveId,
                        static_cast<int>(index),
                        primitive.owningSectorIds});
    }

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
    graph.audioSettings = map.audioSettings;
    graph.vertices.reserve(map.vertices.size());
    graph.lines.reserve(map.lineDefs.size());
    graph.lineSides.reserve(map.sideDefs.size());
    graph.faceAnchors.reserve(map.sectors.size());
    graph.levelMarkers.reserve(map.levelMarkers.size());
    graph.soundEmitters.reserve(map.soundEmitters.size());
    graph.triggers.reserve(map.triggers.size());
    graph.reflectionProbes.reserve(map.compiledReflectionProbes.size());

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

    constexpr float RadiansToDegreesForProbe = 180.0f / 3.14159265358979323846f;
    for (const SectorCompiledReflectionProbe& compiled : map.compiledReflectionProbes) {
        SectorCoord x = 0;
        SectorCoord z = 0;
        if (!VisibleAuthoringToSectorCoord(
                    SectorWorldToAuthoringDistance(compiled.capturePositionWorld.x), x)
                || !VisibleAuthoringToSectorCoord(
                    SectorWorldToAuthoringDistance(compiled.capturePositionWorld.z), z)) {
            continue;
        }
        SectorAuthoringReflectionProbe probe;
        probe.id = IsValidSectorAuthoringId(compiled.sourceAuthoringProbeId)
                ? compiled.sourceAuthoringProbeId
                : AllocateSectorAuthoringReflectionProbeId(graph);
        probe.x = x;
        probe.z = z;
        probe.yWorld = compiled.capturePositionWorld.y;
        probe.enabled = compiled.enabled;
        probe.yawDegrees = compiled.yawRadians * RadiansToDegreesForProbe;
        probe.influenceOffsetWorld = Vector3Subtract(
                compiled.influenceCenterWorld, compiled.capturePositionWorld);
        probe.halfExtentsWorld = compiled.halfExtentsWorld;
        probe.priority = compiled.priority;
        probe.intensity = compiled.intensity;
        probe.resolution = compiled.resolution;
        graph.reflectionProbes.push_back(probe);
    }


    constexpr float RadiansToDegrees = 180.0f / 3.14159265358979323846f;
    for (const SectorCompiledLevelMarker& compiled : map.levelMarkers) {
        SectorCoord x = 0;
        SectorCoord z = 0;
        if (!VisibleAuthoringToSectorCoord(compiled.position.x, x)
                || !VisibleAuthoringToSectorCoord(compiled.position.z, z)) {
            continue;
        }
        SectorAuthoringLevelMarker marker;
        marker.id = IsValidSectorAuthoringId(compiled.sourceAuthoringMarkerId)
                ? compiled.sourceAuthoringMarkerId
                : AllocateSectorAuthoringLevelMarkerId(graph);
        marker.referenceId = compiled.id;
        marker.x = x;
        marker.z = z;
        marker.y = compiled.position.y;
        marker.orientationDegrees = compiled.yawRadians * RadiansToDegrees;
        graph.levelMarkers.push_back(std::move(marker));
    }

    for (const SectorCompiledSoundEmitter& compiled : map.soundEmitters) {
        SectorAuthoringSoundEmitter emitter;
        emitter.id = IsValidSectorAuthoringId(compiled.sourceAuthoringEmitterId)
                ? compiled.sourceAuthoringEmitterId
                : AllocateSectorAuthoringSoundEmitterId(graph);
        emitter.referenceId = compiled.id;
        const Vector3 authored = SectorWorldToAuthoringPosition(compiled.positionWorld);
        VisibleAuthoringToSectorCoord(authored.x, emitter.x);
        emitter.y = authored.y;
        VisibleAuthoringToSectorCoord(authored.z, emitter.z);
        emitter.soundId = compiled.soundId;
        emitter.volume = compiled.volume;
        emitter.loop = compiled.loop;
        graph.soundEmitters.push_back(std::move(emitter));
    }

    for (const SectorCompiledTrigger& compiled : map.triggers) {
        SectorAuthoringTrigger trigger;
        trigger.editorId = IsValidSectorAuthoringId(compiled.sourceAuthoringTriggerId)
                ? compiled.sourceAuthoringTriggerId
                : AllocateSectorAuthoringTriggerId(graph);
        trigger.id = compiled.id;
        trigger.shape = compiled.shape;
        trigger.points = compiled.points;
        trigger.enabled = compiled.enabled;
        trigger.repeat = compiled.repeat;
        trigger.delayMilliseconds = compiled.delayMilliseconds;
        trigger.script = compiled.script;
        graph.triggers.push_back(std::move(trigger));
    }

    return graph;
}

bool SectorAuthoringFaceContainsMapPoint(
        const SectorAuthoringPlanarizationResult& planar,
        const SectorAuthoringExtractedFace& face,
        Vector2 mapPoint)
{
    return FaceContainsPoint(
            planar,
            face,
            static_cast<double>(mapPoint.x) * SectorCoordSubdivisions,
            static_cast<double>(mapPoint.y) * SectorCoordSubdivisions,
            true);
}

bool ResolveSectorAuthoringPointToDerivedSector(
        const SectorAuthoringDerivationResult& derivation,
        SectorTopologyCoordPoint point,
        int* outTopologySectorId)
{
    if (!derivation.success) {
        return false;
    }
    const SectorAuthoringExtractedFace* bestFace = nullptr;
    double bestArea = std::numeric_limits<double>::max();
    for (const SectorAuthoringExtractedFace& face : derivation.faces.faces) {
        if (FacePointIsOnBoundary(
                    derivation.planar,
                    face,
                    static_cast<double>(point.x),
                    static_cast<double>(point.y))
                || !FaceContainsPoint(
                        derivation.planar,
                        face,
                        static_cast<double>(point.x),
                        static_cast<double>(point.y),
                        false)) {
            continue;
        }
        const double area = std::fabs(face.signedArea);
        if (area < bestArea) {
            bestFace = &face;
            bestArea = area;
        }
    }
    if (bestFace == nullptr) {
        return false;
    }
    for (const SectorAuthoringResolvedFaceMapping& mapping : derivation.mapping.resolvedFaces) {
        if (mapping.extractedFaceId == bestFace->id
                && mapping.kind == SectorAuthoringFaceResolutionKind::DerivedSector
                && FindSectorTopologySector(derivation.topology, mapping.topologySectorId) != nullptr) {
            if (outTopologySectorId != nullptr) {
                *outTopologySectorId = mapping.topologySectorId;
            }
            return true;
        }
    }
    return false;
}

} // namespace game
