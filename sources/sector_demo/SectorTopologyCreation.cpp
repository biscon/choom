#include "sector_demo/SectorTopologyCreation.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace game {
namespace {

struct TopologySegment {
    SectorTopologyCoordPoint a;
    SectorTopologyCoordPoint b;
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

void SetError(std::string* outError, const std::string& error)
{
    if (outError != nullptr) {
        *outError = error;
    }
}

void ClearOutputs(int* outSectorId, std::string* outError)
{
    if (outSectorId != nullptr) {
        *outSectorId = -1;
    }
    if (outError != nullptr) {
        outError->clear();
    }
}

uint64_t PointKey(SectorTopologyCoordPoint point)
{
    return (static_cast<uint64_t>(static_cast<uint32_t>(point.x)) << 32U)
           | static_cast<uint32_t>(point.y);
}

bool SamePoint(SectorTopologyCoordPoint a, SectorTopologyCoordPoint b)
{
    return a.x == b.x && a.y == b.y;
}

__int128 Cross(
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint c)
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

bool OnSegment(
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint point)
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
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b,
        SectorTopologyCoordPoint c,
        SectorTopologyCoordPoint d)
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

bool NormalizePolygonPoints(
        const std::vector<SectorTopologyCoordPoint>& input,
        std::vector<SectorTopologyCoordPoint>& output,
        std::string& error)
{
    output = input;
    if (output.size() >= 2 && SamePoint(output.front(), output.back())) {
        output.pop_back();
    }

    if (output.size() < 3) {
        error = "Need at least 3 unique points to create a sector";
        return false;
    }

    std::unordered_set<uint64_t> seen;
    seen.reserve(output.size());
    for (size_t i = 0; i < output.size(); ++i) {
        const SectorTopologyCoordPoint current = output[i];
        const SectorTopologyCoordPoint next = output[(i + 1) % output.size()];
        if (SamePoint(current, next)) {
            error = "Duplicate consecutive topology point";
            return false;
        }
        if (!seen.insert(PointKey(current)).second) {
            error = "Repeated topology point";
            return false;
        }
    }

    __int128 signedAreaTwice = 0;
    for (size_t i = 0; i < output.size(); ++i) {
        const SectorTopologyCoordPoint a = output[i];
        const SectorTopologyCoordPoint b = output[(i + 1) % output.size()];
        signedAreaTwice += static_cast<__int128>(a.x) * static_cast<__int128>(b.y)
                           - static_cast<__int128>(b.x) * static_cast<__int128>(a.y);
    }

    if (signedAreaTwice == 0) {
        error = "Sector polygon has zero area";
        return false;
    }
    if (signedAreaTwice < 0) {
        std::reverse(output.begin(), output.end());
    }

    error.clear();
    return true;
}

int FindVertexAt(const SectorTopologyMap& map, SectorTopologyCoordPoint point)
{
    for (const SectorTopologyVertex& vertex : map.vertices) {
        if (vertex.x == point.x && vertex.y == point.y) {
            return vertex.id;
        }
    }
    return -1;
}

SectorTopologyLineDef* FindLineDefByEndpoints(
        SectorTopologyMap& map,
        int startVertexId,
        int endVertexId)
{
    for (SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.startVertexId == startVertexId
            && lineDef.endVertexId == endVertexId) {
            return &lineDef;
        }
    }
    return nullptr;
}

const SectorTopologyLineDef* FindLineDefByEndpoints(
        const SectorTopologyMap& map,
        int startVertexId,
        int endVertexId)
{
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        if (lineDef.startVertexId == startVertexId
            && lineDef.endVertexId == endVertexId) {
            return &lineDef;
        }
    }
    return nullptr;
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

bool RequireAllocatedId(int id, const char* objectName, std::string& error)
{
    if (IsValidSectorTopologyId(id)) {
        return true;
    }
    error = std::string("Could not allocate ") + objectName + " ID";
    return false;
}

SectorTopologySideDef MakeSideDef(
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

SectorTopologySector CopySectorForInsertedChild(
        int childSectorId,
        const SectorTopologySector& parent,
        const SectorTopologyMap& map,
        const SectorTopologyInsertPolygonOptions& options)
{
    SectorTopologySector child = parent;
    child.id = childSectorId;
    child.name = options.sectorName.empty()
            ? GenerateSectorName(map)
            : options.sectorName;
    return child;
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

bool LoopPoints(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& loop,
        std::vector<SectorTopologyCoordPoint>& outPoints,
        std::string& error)
{
    outPoints.clear();
    outPoints.reserve(loop.vertexIds.size());
    for (int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
        if (vertex == nullptr) {
            error = "Could not resolve topology loop vertex " + std::to_string(vertexId);
            return false;
        }
        outPoints.push_back(SectorTopologyCoordPoint{vertex->x, vertex->y});
    }
    error.clear();
    return true;
}

PointContainment ClassifyPointInPolygon(
        const std::vector<SectorTopologyCoordPoint>& polygon,
        SectorTopologyCoordPoint point)
{
    bool inside = false;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const SectorTopologyCoordPoint a = polygon[i];
        const SectorTopologyCoordPoint b = polygon[(i + 1) % polygon.size()];
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

bool BuildExistingSegments(
        const SectorTopologyMap& map,
        std::vector<TopologySegment>& outSegments,
        std::string& error)
{
    outSegments.clear();
    outSegments.reserve(map.lineDefs.size());
    for (const SectorTopologyLineDef& lineDef : map.lineDefs) {
        const SectorTopologyVertex* start = FindSectorTopologyVertex(map, lineDef.startVertexId);
        const SectorTopologyVertex* end = FindSectorTopologyVertex(map, lineDef.endVertexId);
        if (start == nullptr || end == nullptr) {
            error = "Could not resolve existing topology linedef " + std::to_string(lineDef.id);
            return false;
        }
        outSegments.push_back(TopologySegment{
                SectorTopologyCoordPoint{start->x, start->y},
                SectorTopologyCoordPoint{end->x, end->y}
        });
    }
    error.clear();
    return true;
}

bool ValidateInsertedPolygonPlacement(
        const SectorTopologyMap& map,
        int parentSectorId,
        const std::vector<SectorTopologyCoordPoint>& normalizedPoints,
        std::string& error)
{
    if (FindSectorTopologySector(map, parentSectorId) == nullptr) {
        error = "Select a topology sector before inserting inside it.";
        return false;
    }

    SectorTopologyLoopSet parentLoops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(map, parentSectorId, parentLoops, &loopIssues)) {
        error = FirstValidationError(loopIssues);
        return false;
    }

    std::vector<SectorTopologyCoordPoint> parentOuter;
    if (!LoopPoints(map, parentLoops.outer, parentOuter, error)) {
        return false;
    }

    std::vector<std::vector<SectorTopologyCoordPoint>> parentHoles;
    parentHoles.reserve(parentLoops.holes.size());
    for (const SectorTopologyLoop& hole : parentLoops.holes) {
        std::vector<SectorTopologyCoordPoint> holePoints;
        if (!LoopPoints(map, hole, holePoints, error)) {
            return false;
        }
        parentHoles.push_back(std::move(holePoints));
    }

    for (SectorTopologyCoordPoint point : normalizedPoints) {
        if (FindVertexAt(map, point) >= 0) {
            error = "Insert polygon must not touch existing topology vertices";
            return false;
        }
        if (ClassifyPointInPolygon(parentOuter, point) != PointContainment::Inside) {
            error = "Insert polygon must be strictly inside the parent sector";
            return false;
        }
        for (const std::vector<SectorTopologyCoordPoint>& hole : parentHoles) {
            if (ClassifyPointInPolygon(hole, point) != PointContainment::Outside) {
                error = "Insert polygon must not overlap an existing parent hole";
                return false;
            }
        }
    }
    for (const std::vector<SectorTopologyCoordPoint>& hole : parentHoles) {
        for (SectorTopologyCoordPoint holePoint : hole) {
            if (ClassifyPointInPolygon(normalizedPoints, holePoint) != PointContainment::Outside) {
                error = "Insert polygon must not overlap an existing parent hole";
                return false;
            }
        }
    }

    std::vector<TopologySegment> existingSegments;
    if (!BuildExistingSegments(map, existingSegments, error)) {
        return false;
    }

    for (size_t i = 0; i < normalizedPoints.size(); ++i) {
        const SectorTopologyCoordPoint a = normalizedPoints[i];
        const SectorTopologyCoordPoint b = normalizedPoints[(i + 1) % normalizedPoints.size()];
        for (const TopologySegment& existing : existingSegments) {
            const SegmentIntersectionKind intersection =
                    SegmentIntersection(a, b, existing.a, existing.b);
            if (intersection == SegmentIntersectionKind::CollinearOverlap) {
                error = "Insert polygon edge overlaps existing topology";
                return false;
            }
            if (intersection != SegmentIntersectionKind::None) {
                error = "Insert polygon edge touches or crosses existing topology";
                return false;
            }
        }
    }

    error.clear();
    return true;
}

} // namespace

bool CreateSectorTopologyPolygon(
        SectorTopologyMap& map,
        const std::vector<SectorTopologyCoordPoint>& points,
        const SectorTopologyCreatePolygonOptions& options,
        int* outSectorId,
        std::string* outError)
{
    ClearOutputs(outSectorId, outError);

    std::vector<SectorTopologyCoordPoint> normalizedPoints;
    std::string error;
    if (!NormalizePolygonPoints(points, normalizedPoints, error)) {
        SetError(outError, error);
        return false;
    }

    SectorTopologyMap candidate = map;

    const int sectorId = AllocateSectorTopologySectorId(candidate);
    if (!RequireAllocatedId(sectorId, "sector", error)) {
        SetError(outError, error);
        return false;
    }

    SectorTopologySector sector;
    sector.id = sectorId;
    sector.name = options.sectorName.empty()
            ? GenerateSectorName(candidate)
            : options.sectorName;
    sector.floorZ = options.floorZ;
    sector.ceilingZ = options.ceilingZ;
    sector.floorTextureId = options.floorTextureId;
    sector.ceilingTextureId = options.ceilingTextureId;
    sector.floorUv = options.floorUv;
    sector.ceilingUv = options.ceilingUv;
    sector.ambientColor = options.ambientColor;
    sector.ambientIntensity = options.ambientIntensity;
    sector.defaultWall = options.defaultWall;
    sector.defaultLower = options.defaultLower;
    sector.defaultUpper = options.defaultUpper;
    candidate.sectors.push_back(sector);

    std::vector<int> vertexIds;
    vertexIds.reserve(normalizedPoints.size());
    for (SectorTopologyCoordPoint point : normalizedPoints) {
        int vertexId = FindVertexAt(candidate, point);
        if (!IsValidSectorTopologyId(vertexId)) {
            vertexId = AllocateSectorTopologyVertexId(candidate);
            if (!RequireAllocatedId(vertexId, "vertex", error)) {
                SetError(outError, error);
                return false;
            }
            candidate.vertices.push_back(SectorTopologyVertex{vertexId, point.x, point.y});
        }
        vertexIds.push_back(vertexId);
    }

    for (size_t i = 0; i < vertexIds.size(); ++i) {
        const int startVertexId = vertexIds[i];
        const int endVertexId = vertexIds[(i + 1) % vertexIds.size()];
        SectorTopologyLineDef* sameDirection = FindLineDefByEndpoints(
                candidate, startVertexId, endVertexId);
        SectorTopologyLineDef* reversed = sameDirection == nullptr
                ? FindLineDefByEndpoints(candidate, endVertexId, startVertexId)
                : nullptr;

        SectorTopologyLineDef* lineDef = sameDirection != nullptr ? sameDirection : reversed;
        const SectorTopologySideKind side = sameDirection != nullptr || reversed == nullptr
                ? SectorTopologySideKind::Front
                : SectorTopologySideKind::Back;

        if (lineDef == nullptr) {
            const int lineDefId = AllocateSectorTopologyLineDefId(candidate);
            if (!RequireAllocatedId(lineDefId, "linedef", error)) {
                SetError(outError, error);
                return false;
            }
            candidate.lineDefs.push_back(SectorTopologyLineDef{
                    lineDefId,
                    startVertexId,
                    endVertexId,
                    -1,
                    -1
            });
            lineDef = &candidate.lineDefs.back();
        }

        int& slot = side == SectorTopologySideKind::Front
                ? lineDef->frontSideDefId
                : lineDef->backSideDefId;
        if (slot != -1) {
            std::ostringstream message;
            message << "Linedef " << lineDef->id << ' '
                    << (side == SectorTopologySideKind::Front ? "front" : "back")
                    << " side is already occupied";
            SetError(outError, message.str());
            return false;
        }

        const int sideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(sideDefId, "sidedef", error)) {
            SetError(outError, error);
            return false;
        }
        slot = sideDefId;
        candidate.sideDefs.push_back(MakeSideDef(sideDefId, lineDef->id, side, sectorId, sector));
    }

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(candidate);
    if (HasSectorTopologyValidationErrors(issues)) {
        SetError(outError, FirstValidationError(issues));
        return false;
    }

    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(candidate, sectorId, loops, &loopIssues)) {
        SetError(outError, FirstValidationError(loopIssues));
        return false;
    }

    map = std::move(candidate);
    if (outSectorId != nullptr) {
        *outSectorId = sectorId;
    }
    if (outError != nullptr) {
        outError->clear();
    }
    return true;
}

bool InsertSectorTopologyPolygon(
        SectorTopologyMap& map,
        int parentSectorId,
        const std::vector<SectorTopologyCoordPoint>& points,
        const SectorTopologyInsertPolygonOptions& options,
        int* outChildSectorId,
        std::string* outError)
{
    ClearOutputs(outChildSectorId, outError);

    std::vector<SectorTopologyCoordPoint> normalizedPoints;
    std::string error;
    if (!NormalizePolygonPoints(points, normalizedPoints, error)) {
        SetError(outError, error);
        return false;
    }

    const std::vector<SectorTopologyValidationIssue> initialIssues = ValidateSectorTopologyMap(map);
    if (HasSectorTopologyValidationErrors(initialIssues)) {
        SetError(outError, FirstValidationError(initialIssues));
        return false;
    }

    if (!ValidateInsertedPolygonPlacement(map, parentSectorId, normalizedPoints, error)) {
        SetError(outError, error);
        return false;
    }

    SectorTopologyMap candidate = map;
    const SectorTopologySector* parent = FindSectorTopologySector(candidate, parentSectorId);
    if (parent == nullptr) {
        SetError(outError, "Select a topology sector before inserting inside it.");
        return false;
    }

    const int childSectorId = AllocateSectorTopologySectorId(candidate);
    if (!RequireAllocatedId(childSectorId, "sector", error)) {
        SetError(outError, error);
        return false;
    }

    const SectorTopologySector child = CopySectorForInsertedChild(
            childSectorId,
            *parent,
            candidate,
            options);
    candidate.sectors.push_back(child);
    parent = FindSectorTopologySector(candidate, parentSectorId);
    const SectorTopologySector* childSector = FindSectorTopologySector(candidate, childSectorId);
    if (parent == nullptr || childSector == nullptr) {
        SetError(outError, "Could not resolve inserted sector data");
        return false;
    }

    std::vector<int> vertexIds;
    vertexIds.reserve(normalizedPoints.size());
    for (SectorTopologyCoordPoint point : normalizedPoints) {
        const int vertexId = AllocateSectorTopologyVertexId(candidate);
        if (!RequireAllocatedId(vertexId, "vertex", error)) {
            SetError(outError, error);
            return false;
        }
        candidate.vertices.push_back(SectorTopologyVertex{vertexId, point.x, point.y});
        vertexIds.push_back(vertexId);
    }

    for (size_t i = 0; i < vertexIds.size(); ++i) {
        const int startVertexId = vertexIds[i];
        const int endVertexId = vertexIds[(i + 1) % vertexIds.size()];
        if (FindLineDefByEndpoints(candidate, startVertexId, endVertexId) != nullptr
                || FindLineDefByEndpoints(candidate, endVertexId, startVertexId) != nullptr) {
            SetError(outError, "Insert polygon edge exactly matches existing topology");
            return false;
        }

        const int lineDefId = AllocateSectorTopologyLineDefId(candidate);
        if (!RequireAllocatedId(lineDefId, "linedef", error)) {
            SetError(outError, error);
            return false;
        }
        const int childSideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(childSideDefId, "sidedef", error)) {
            SetError(outError, error);
            return false;
        }
        candidate.sideDefs.push_back(MakeSideDef(
                childSideDefId,
                lineDefId,
                SectorTopologySideKind::Front,
                childSectorId,
                *childSector));

        const int parentSideDefId = AllocateSectorTopologySideDefId(candidate);
        if (!RequireAllocatedId(parentSideDefId, "sidedef", error)) {
            SetError(outError, error);
            return false;
        }
        candidate.sideDefs.push_back(MakeSideDef(
                parentSideDefId,
                lineDefId,
                SectorTopologySideKind::Back,
                parentSectorId,
                *parent));

        candidate.lineDefs.push_back(SectorTopologyLineDef{
                lineDefId,
                startVertexId,
                endVertexId,
                childSideDefId,
                parentSideDefId
        });
    }

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(candidate);
    if (HasSectorTopologyValidationErrors(issues)) {
        SetError(outError, FirstValidationError(issues));
        return false;
    }

    SectorTopologyLoopSet parentLoops;
    std::vector<SectorTopologyValidationIssue> parentLoopIssues;
    if (!ExtractSectorTopologyLoops(candidate, parentSectorId, parentLoops, &parentLoopIssues)) {
        SetError(outError, FirstValidationError(parentLoopIssues));
        return false;
    }

    SectorTopologyLoopSet childLoops;
    std::vector<SectorTopologyValidationIssue> childLoopIssues;
    if (!ExtractSectorTopologyLoops(candidate, childSectorId, childLoops, &childLoopIssues)) {
        SetError(outError, FirstValidationError(childLoopIssues));
        return false;
    }

    map = std::move(candidate);
    if (outChildSectorId != nullptr) {
        *outChildSectorId = childSectorId;
    }
    if (outError != nullptr) {
        outError->clear();
    }
    return true;
}

} // namespace game
