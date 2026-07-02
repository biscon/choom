#include "sector_editor/SectorEditorTopologyActions.h"

#include "sector_editor/SectorEditorHelpers.h"
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
                "Static light placement failed: click inside a sector"};
    }

    const int lightId = AllocateSectorTopologyStaticLightId(map);
    if (!IsValidSectorTopologyId(lightId)) {
        return SectorEditorAddStaticLightResult{
                false,
                -1,
                "Static light placement failed: no topology light IDs available"};
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
            TextFormat("Added static light %d", lightId)};
}

SectorEditorAddDynamicLightResult AddDynamicLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr) {
        return SectorEditorAddDynamicLightResult{
                false,
                -1,
                "Dynamic light placement failed: click inside a sector"};
    }

    const int lightId = AllocateSectorTopologyDynamicLightId(map);
    if (!IsValidSectorTopologyId(lightId)) {
        return SectorEditorAddDynamicLightResult{
                false,
                -1,
                "Dynamic light placement failed: no topology light IDs available"};
    }

    SectorTopologyDynamicPointLight light;
    light.id = lightId;
    light.position = Vector3{
            mapPoint.x,
            sector->floorZ + SectorWorldToAuthoringDistance(1.8f),
            mapPoint.y};
    light.color = WHITE;
    light.intensity = 1.0f;
    light.radius = SectorWorldToAuthoringDistance(8.0f);
    light.enabled = true;

    map.dynamicPointLights.push_back(light);
    return SectorEditorAddDynamicLightResult{
            true,
            lightId,
            TextFormat("Added dynamic light %d", lightId)};
}

SectorEditorAddStaticSpotLightResult AddStaticSpotLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr) {
        return SectorEditorAddStaticSpotLightResult{
                false,
                -1,
                "Static spot placement failed: click inside a sector"};
    }

    const int lightId = AllocateSectorTopologyStaticSpotLightId(map);
    if (!IsValidSectorTopologyId(lightId)) {
        return SectorEditorAddStaticSpotLightResult{
                false,
                -1,
                "Static spot placement failed: no topology light IDs available"};
    }

    SectorTopologyStaticSpotLight light;
    light.id = lightId;
    light.position = Vector3{
            mapPoint.x,
            sector->floorZ + SectorWorldToAuthoringDistance(1.8f),
            mapPoint.y};
    light.target = Vector3{
            mapPoint.x + SectorWorldToAuthoringDistance(4.0f),
            sector->floorZ + SectorWorldToAuthoringDistance(1.0f),
            mapPoint.y};
    light.color = WHITE;
    light.intensity = 1.0f;
    light.range = SectorWorldToAuthoringDistance(8.0f);
    light.sourceRadius = 0.0f;
    light.innerConeDegrees = 20.0f;
    light.outerConeDegrees = 35.0f;

    map.staticSpotLights.push_back(light);
    return SectorEditorAddStaticSpotLightResult{
            true,
            lightId,
            TextFormat("Added static spot %d", lightId)};
}

SectorEditorAddDynamicSpotLightResult AddDynamicSpotLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr) {
        return SectorEditorAddDynamicSpotLightResult{
                false,
                -1,
                "Dynamic spot placement failed: click inside a sector"};
    }

    const int lightId = AllocateSectorTopologyDynamicSpotLightId(map);
    if (!IsValidSectorTopologyId(lightId)) {
        return SectorEditorAddDynamicSpotLightResult{
                false,
                -1,
                "Dynamic spot placement failed: no topology light IDs available"};
    }

    SectorTopologyDynamicSpotLight light;
    light.id = lightId;
    light.position = Vector3{
            mapPoint.x,
            sector->floorZ + SectorWorldToAuthoringDistance(1.8f),
            mapPoint.y};
    light.target = Vector3{
            mapPoint.x + SectorWorldToAuthoringDistance(4.0f),
            sector->floorZ + SectorWorldToAuthoringDistance(1.0f),
            mapPoint.y};
    light.color = WHITE;
    light.intensity = 1.0f;
    light.range = SectorWorldToAuthoringDistance(8.0f);
    light.innerConeDegrees = 20.0f;
    light.outerConeDegrees = 35.0f;
    light.enabled = true;
    light.flicker = false;
    light.flickerSpeed = DynamicLightFlickerDefaultSpeed;
    light.flickerAmount = DynamicLightFlickerDefaultAmount;

    map.dynamicSpotLights.push_back(light);
    return SectorEditorAddDynamicSpotLightResult{
            true,
            lightId,
            TextFormat("Added dynamic spot %d", lightId)};
}

SectorEditorAddBillboardResult AddBillboardToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr) {
        return SectorEditorAddBillboardResult{
                false,
                -1,
                "Billboard placement failed: click inside a sector"};
    }

    const int objectId = AllocateSectorPlacedRuntimeObjectId(map);
    if (!IsValidSectorTopologyId(objectId)) {
        return SectorEditorAddBillboardResult{
                false,
                -1,
                "Billboard placement failed: no runtime object IDs available"};
    }

    SectorPlacedRuntimeObject object;
    object.id = objectId;
    object.kind = "billboard";
    object.position = Vector3{mapPoint.x, sector->floorZ, mapPoint.y};
    object.yawRadians = 0.0f;
    object.billboard = SectorPlacedBillboard{};

    map.runtimeObjects.push_back(std::move(object));
    return SectorEditorAddBillboardResult{
            true,
            objectId,
            TextFormat("Added billboard %d", objectId)};
}

SectorEditorTopologyActionResult DeleteStaticLight(
        SectorTopologyMap& map,
        int lightId)
{
    if (FindSectorTopologyStaticLight(map, lightId) == nullptr) {
        return Unchanged("Select a static light to delete.");
    }

    if (!RemoveSectorTopologyStaticLight(map, lightId)) {
        return Unchanged("Failed to delete static light.");
    }

    return Changed(TextFormat("Deleted static light %d", lightId));
}

SectorEditorTopologyActionResult DeleteStaticSpotLight(
        SectorTopologyMap& map,
        int lightId)
{
    if (FindSectorTopologyStaticSpotLight(map, lightId) == nullptr) {
        return Unchanged("Select a static spot to delete.");
    }

    if (!RemoveSectorTopologyStaticSpotLight(map, lightId)) {
        return Unchanged("Failed to delete static spot.");
    }

    return Changed(TextFormat("Deleted static spot %d", lightId));
}

SectorEditorTopologyActionResult DeleteDynamicLight(
        SectorTopologyMap& map,
        int lightId)
{
    if (FindSectorTopologyDynamicLight(map, lightId) == nullptr) {
        return Unchanged("Select a dynamic light to delete.");
    }

    if (!RemoveSectorTopologyDynamicLight(map, lightId)) {
        return Unchanged("Failed to delete dynamic light.");
    }

    return Changed(TextFormat("Deleted dynamic light %d", lightId));
}

SectorEditorTopologyActionResult DeleteDynamicSpotLight(
        SectorTopologyMap& map,
        int lightId)
{
    if (FindSectorTopologyDynamicSpotLight(map, lightId) == nullptr) {
        return Unchanged("Select a dynamic spot to delete.");
    }

    if (!RemoveSectorTopologyDynamicSpotLight(map, lightId)) {
        return Unchanged("Failed to delete dynamic spot.");
    }

    return Changed(TextFormat("Deleted dynamic spot %d", lightId));
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
        return Unchanged("Static light unchanged");
    }

    return Changed(TextFormat(
            "Moved static light %d to X %.2f, Z %.2f",
            light->id,
            light->position.x,
            light->position.z));
}

SectorEditorTopologyActionResult FinishMoveDynamicLight(
        SectorTopologyMap& map,
        int lightId,
        Vector3 originalPosition)
{
    SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(map, lightId);
    if (light == nullptr) {
        return Unchanged();
    }

    if (std::fabs(light->position.x - originalPosition.x) <= GeometryEpsilon
            && std::fabs(light->position.z - originalPosition.z) <= GeometryEpsilon) {
        light->position = originalPosition;
        return Unchanged("Dynamic light unchanged");
    }

    return Changed(TextFormat(
            "Moved dynamic light %d to X %.2f, Z %.2f",
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
