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
    light.instanceId = AllocateSectorDynamicLightInstanceId(map, "point", lightId);
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
    light.instanceId = AllocateSectorDynamicLightInstanceId(map, "spot", lightId);
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

SectorEditorAddStaticRectLightResult AddStaticRectLightToSector(
        SectorTopologyMap& map, int sectorId, Vector2 mapPoint)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr) return {false, -1, "Static rect placement failed: click inside a sector"};
    const int lightId = AllocateSectorTopologyStaticRectLightId(map);
    if (!IsValidSectorTopologyId(lightId)) return {false, -1, "Static rect placement failed: no light IDs available"};
    SectorTopologyStaticRectLight light;
    light.id = lightId;
    light.position = {mapPoint.x, sector->floorZ + SectorWorldToAuthoringDistance(1.8f), mapPoint.y};
    light.target = {mapPoint.x,
            sector->floorZ + SectorWorldToAuthoringDistance(1.0f), mapPoint.y};
    map.staticRectLights.push_back(light);
    return {true, lightId, TextFormat("Added static rect light %d", lightId)};
}

SectorEditorAddDynamicRectLightResult AddDynamicRectLightToSector(
        SectorTopologyMap& map, int sectorId, Vector2 mapPoint)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, sectorId);
    if (sector == nullptr) return {false, -1, "Dynamic rect placement failed: click inside a sector"};
    const int lightId = AllocateSectorTopologyDynamicRectLightId(map);
    if (!IsValidSectorTopologyId(lightId)) return {false, -1, "Dynamic rect placement failed: no light IDs available"};
    SectorTopologyDynamicRectLight light;
    light.id = lightId;
    light.instanceId = AllocateSectorDynamicLightInstanceId(map, "rect", lightId);
    light.position = {mapPoint.x, sector->floorZ + SectorWorldToAuthoringDistance(1.8f), mapPoint.y};
    light.target = {mapPoint.x,
            sector->floorZ + SectorWorldToAuthoringDistance(1.0f), mapPoint.y};
    map.dynamicRectLights.push_back(light);
    return {true, lightId, TextFormat("Added dynamic rect light %d", lightId)};
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

SectorEditorAddDoorResult AddDoorToPortal(
        SectorTopologyMap& map,
        int lineDefId)
{
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, lineDefId);
    if (lineDef == nullptr) {
        return SectorEditorAddDoorResult{
                false,
                -1,
                "Door placement failed: click a two-sided portal"};
    }

    const SectorTopologySideDef* frontSideDef =
            FindSectorTopologySideDef(map, lineDef->frontSideDefId);
    const SectorTopologySideDef* backSideDef =
            FindSectorTopologySideDef(map, lineDef->backSideDefId);
    if (frontSideDef == nullptr || backSideDef == nullptr) {
        return SectorEditorAddDoorResult{
                false,
                -1,
                "Door placement failed: clicked line is not a two-sided portal"};
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(map, *lineDef, start, end)) {
        return SectorEditorAddDoorResult{
                false,
                -1,
                "Door placement failed: portal endpoints are invalid"};
    }

    const int objectId = AllocateSectorPlacedRuntimeObjectId(map);
    if (!IsValidSectorTopologyId(objectId)) {
        return SectorEditorAddDoorResult{
                false,
                -1,
                "Door placement failed: no runtime object IDs available"};
    }

    SectorPlacedRuntimeObject object;
    object.id = objectId;
    object.kind = "door";
    const Vector2 startMap = SectorTopologyVertexToMap(*start);
    const Vector2 endMap = SectorTopologyVertexToMap(*end);
    object.position = Vector3{
            (startMap.x + endMap.x) * 0.5f,
            0.0f,
            (startMap.y + endMap.y) * 0.5f};
    object.yawRadians = 0.0f;
    object.door = SectorPlacedDoor{};
    object.door.instanceId = AllocateSectorDoorInstanceId(map, objectId);
    object.door.anchor.lineDefId = lineDef->id;
    object.door.anchor.frontSectorId = frontSideDef->sectorId;
    object.door.anchor.backSectorId = backSideDef->sectorId;
    object.door.anchor.frontSideDefId = frontSideDef->id;
    object.door.anchor.backSideDefId = backSideDef->id;
    object.door.anchor.endpointAX = start->x;
    object.door.anchor.endpointAY = start->y;
    object.door.anchor.endpointBX = end->x;
    object.door.anchor.endpointBY = end->y;

    const SectorResolvedDoorAnchor resolved = ResolveSectorDoorAnchor(map, object.door);
    if (!resolved.valid) {
        return SectorEditorAddDoorResult{
                false,
                -1,
                resolved.diagnostic.empty()
                        ? "Door placement failed: portal cannot resolve a valid door anchor"
                        : std::string{"Door placement failed: "} + resolved.diagnostic};
    }

    object.door.width = resolved.width;
    object.door.height = resolved.height;
    object.door.openDistance = resolved.height;
    object.position = Vector3{
            SectorWorldToAuthoringDistance(resolved.midpoint.x),
            SectorWorldToAuthoringDistance(resolved.openBottom),
            SectorWorldToAuthoringDistance(resolved.midpoint.y)};

    map.runtimeObjects.push_back(std::move(object));
    return SectorEditorAddDoorResult{
            true,
            objectId,
            TextFormat("Added door %d", objectId)};
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

SectorEditorTopologyActionResult DeleteStaticRectLight(SectorTopologyMap& map, int lightId)
{
    if (!RemoveSectorTopologyStaticRectLight(map, lightId)) {
        return Unchanged("Select a static rect light to delete.");
    }
    return Changed(TextFormat("Deleted static rect light %d", lightId));
}

SectorEditorTopologyActionResult DeleteDynamicRectLight(SectorTopologyMap& map, int lightId)
{
    if (!RemoveSectorTopologyDynamicRectLight(map, lightId)) {
        return Unchanged("Select a dynamic rect light to delete.");
    }
    return Changed(TextFormat("Deleted dynamic rect light %d", lightId));
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

} // namespace game
