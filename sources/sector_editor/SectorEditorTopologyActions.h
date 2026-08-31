#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <string>

namespace game {

struct SectorEditorTopologyActionResult {
    bool changed = false;
    std::string status;
};

struct SectorEditorAddStaticLightResult {
    bool changed = false;
    int lightId = -1;
    std::string status;
};

struct SectorEditorAddDynamicLightResult {
    bool changed = false;
    int lightId = -1;
    std::string status;
};

struct SectorEditorAddStaticSpotLightResult {
    bool changed = false;
    int lightId = -1;
    std::string status;
};

struct SectorEditorAddDynamicSpotLightResult {
    bool changed = false;
    int lightId = -1;
    std::string status;
};

using SectorEditorAddStaticRectLightResult = SectorEditorAddStaticSpotLightResult;
using SectorEditorAddDynamicRectLightResult = SectorEditorAddDynamicSpotLightResult;

struct SectorEditorAddBillboardResult {
    bool changed = false;
    int objectId = -1;
    std::string status;
};

struct SectorEditorAddDoorResult {
    bool changed = false;
    int objectId = -1;
    std::string status;
};

using SectorEditorAddWindowResult = SectorEditorAddDoorResult;

SectorEditorAddStaticLightResult AddStaticLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint);

SectorEditorTopologyActionResult DeleteStaticLight(
        SectorTopologyMap& map,
        int lightId);

SectorEditorAddStaticSpotLightResult AddStaticSpotLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint);

SectorEditorTopologyActionResult DeleteStaticSpotLight(
        SectorTopologyMap& map,
        int lightId);

SectorEditorAddDynamicLightResult AddDynamicLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint);

SectorEditorTopologyActionResult DeleteDynamicLight(
        SectorTopologyMap& map,
        int lightId);

SectorEditorAddDynamicSpotLightResult AddDynamicSpotLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint);

SectorEditorTopologyActionResult DeleteDynamicSpotLight(
        SectorTopologyMap& map,
        int lightId);

SectorEditorAddStaticRectLightResult AddStaticRectLightToSector(
        SectorTopologyMap& map, int sectorId, Vector2 mapPoint);
SectorEditorTopologyActionResult DeleteStaticRectLight(
        SectorTopologyMap& map, int lightId);
SectorEditorAddDynamicRectLightResult AddDynamicRectLightToSector(
        SectorTopologyMap& map, int sectorId, Vector2 mapPoint);
SectorEditorTopologyActionResult DeleteDynamicRectLight(
        SectorTopologyMap& map, int lightId);

SectorEditorAddBillboardResult AddBillboardToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint);

SectorEditorAddDoorResult AddDoorToPortal(
        SectorTopologyMap& map,
        int lineDefId);

SectorEditorAddWindowResult AddWindowToPortal(
        SectorTopologyMap& map,
        int lineDefId);

SectorEditorTopologyActionResult FinishMoveStaticLight(
        SectorTopologyMap& map,
        int lightId,
        Vector3 originalPosition);

SectorEditorTopologyActionResult FinishMoveDynamicLight(
        SectorTopologyMap& map,
        int lightId,
        Vector3 originalPosition);

} // namespace game
