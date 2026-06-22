#include "sector_demo/SectorTopologyEdit.h"

#include "sector_demo/SectorTopologyGeometry.h"

#include <algorithm>
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

} // namespace game
