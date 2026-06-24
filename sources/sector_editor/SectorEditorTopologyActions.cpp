#include "sector_editor/SectorEditorTopologyActions.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTopologyEdit.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>

#include <cmath>
#include <utility>

namespace game {
namespace {

SectorEditorTopologyActionResult Unchanged(std::string status = {})
{
    SectorEditorTopologyActionResult result;
    result.status = std::move(status);
    return result;
}

SectorEditorTopologyActionResult Changed(std::string status)
{
    SectorEditorTopologyActionResult result;
    result.changed = true;
    result.status = std::move(status);
    return result;
}

} // namespace

SectorEditorTopologyActionResult MoveTopologyVertex(
        SectorTopologyMap& map,
        int vertexId,
        SectorTopologyCoordPoint originalPoint,
        SectorTopologyCoordPoint targetPoint)
{
    std::string error;
    if (!MoveSectorTopologyVertex(map, vertexId, targetPoint, &error)) {
        return Unchanged(TextFormat("Move rejected: %s", error.c_str()));
    }

    return Changed(TextFormat(
            "Moved topology vertex %d %.2f,%.2f -> %.2f,%.2f",
            vertexId,
            SectorCoordToVisibleAuthoring(originalPoint.x),
            SectorCoordToVisibleAuthoring(originalPoint.y),
            SectorCoordToVisibleAuthoring(targetPoint.x),
            SectorCoordToVisibleAuthoring(targetPoint.y)));
}

SectorEditorMergeVerticesResult MergeTopologyVertices(
        SectorTopologyMap& map,
        int sourceVertexId,
        int targetVertexId)
{
    SectorTopologyMergeVerticesResult merge;
    std::string error;
    if (!MergeSectorTopologyVertices(map, sourceVertexId, targetVertexId, &merge, &error)) {
        return SectorEditorMergeVerticesResult{
                false,
                SectorTopologyMergeVerticesResult{},
                TextFormat("Merge rejected: %s", error.c_str())};
    }

    return SectorEditorMergeVerticesResult{
            true,
            merge,
            TextFormat(
                    "Merged vertex %d into vertex %d.",
                    merge.removedVertexId,
                    merge.mergedVertexId)};
}

SectorEditorDissolveVertexResult DissolveTopologyVertex(
        SectorTopologyMap& map,
        int vertexId)
{
    SectorTopologyDissolveVertexResult dissolve;
    std::string error;
    if (!DissolveSectorTopologyVertex(map, vertexId, &dissolve, &error)) {
        return SectorEditorDissolveVertexResult{
                false,
                SectorTopologyDissolveVertexResult{},
                error.empty() ? "Cannot dissolve topology vertex" : error};
    }

    return SectorEditorDissolveVertexResult{
            true,
            dissolve,
            TextFormat(
                    "Dissolved topology vertex %d; selected linedef %d",
                    dissolve.removedVertexId,
                    dissolve.replacementLineDefId)};
}

SectorEditorAddStaticLightResult AddStaticLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr) {
        return SectorEditorAddStaticLightResult{
                false,
                -1,
                "Light placement failed: click inside a sector"};
    }

    const int lightId = AllocateSectorTopologyStaticLightId(map);
    if (!IsValidSectorTopologyId(lightId)) {
        return SectorEditorAddStaticLightResult{
                false,
                -1,
                "Light placement failed: no topology light IDs available"};
    }

    SectorTopologyStaticPointLight light;
    light.id = lightId;
    light.position = Vector3{
            mapPoint.x,
            sector->floorZ + SectorWorldToAuthoringDistance(1.8f),
            mapPoint.y};
    light.color = WHITE;
    light.intensity = 1.0f;
    light.radius = SectorWorldToAuthoringDistance(8.0f);
    light.sourceRadius = SectorWorldToAuthoringDistance(0.25f);

    map.staticLights.push_back(light);
    return SectorEditorAddStaticLightResult{
            true,
            lightId,
            TextFormat("Added topology light %d", lightId)};
}

SectorEditorTopologyActionResult DeleteStaticLight(
        SectorTopologyMap& map,
        int lightId)
{
    if (FindSectorTopologyStaticLight(map, lightId) == nullptr) {
        return Unchanged("Select a topology light to delete.");
    }

    if (!RemoveSectorTopologyStaticLight(map, lightId)) {
        return Unchanged("Failed to delete topology light.");
    }

    return Changed(TextFormat("Deleted topology light %d", lightId));
}

SectorEditorTopologyActionResult FinishMoveStaticLight(
        SectorTopologyMap& map,
        int lightId,
        Vector3 originalPosition)
{
    SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(map, lightId);
    if (light == nullptr) {
        return Unchanged();
    }

    if (std::fabs(light->position.x - originalPosition.x) <= GeometryEpsilon
            && std::fabs(light->position.z - originalPosition.z) <= GeometryEpsilon) {
        light->position = originalPosition;
        return Unchanged("Light unchanged");
    }

    return Changed(TextFormat(
            "Moved topology light %d to X %.2f, Z %.2f",
            light->id,
            light->position.x,
            light->position.z));
}

SectorEditorTopologyActionResult SetPortalBlocksPlayer(
        SectorTopologyMap& map,
        int lineDefId,
        bool blocksPlayer)
{
    SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, lineDefId);
    if (lineDef == nullptr) {
        return Unchanged("Selected linedef is no longer valid.");
    }
    if (lineDef->frontSideDefId == -1 || lineDef->backSideDefId == -1) {
        return Unchanged("Blocks Player is only editable on two-sided portals.");
    }
    if (lineDef->flags.blocksPlayer == blocksPlayer) {
        return Unchanged();
    }

    lineDef->flags.blocksPlayer = blocksPlayer;
    return Changed(blocksPlayer
            ? "Enabled player blocking on portal."
            : "Disabled player blocking on portal.");
}

} // namespace game
