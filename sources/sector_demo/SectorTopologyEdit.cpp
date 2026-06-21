#include "sector_demo/SectorTopologyEdit.h"

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

} // namespace game
