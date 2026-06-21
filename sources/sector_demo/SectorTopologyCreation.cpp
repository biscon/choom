#include "sector_demo/SectorTopologyCreation.h"

#include <algorithm>
#include <cstdio>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace game {
namespace {

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

std::string FirstValidationError(const std::vector<SectorTopologyValidationIssue>& issues)
{
    for (const SectorTopologyValidationIssue& issue : issues) {
        if (issue.severity == SectorTopologyValidationSeverity::Error) {
            return FormatSectorTopologyValidationIssue(issue);
        }
    }
    return "Topology validation failed";
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

} // namespace game
