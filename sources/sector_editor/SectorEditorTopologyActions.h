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

SectorEditorAddStaticLightResult AddStaticLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint);

SectorEditorTopologyActionResult DeleteStaticLight(
        SectorTopologyMap& map,
        int lightId);

SectorEditorAddDynamicLightResult AddDynamicLightToSector(
        SectorTopologyMap& map,
        int sectorId,
        Vector2 mapPoint);

SectorEditorTopologyActionResult DeleteDynamicLight(
        SectorTopologyMap& map,
        int lightId);

SectorEditorTopologyActionResult FinishMoveStaticLight(
        SectorTopologyMap& map,
        int lightId,
        Vector3 originalPosition);

SectorEditorTopologyActionResult FinishMoveDynamicLight(
        SectorTopologyMap& map,
        int lightId,
        Vector3 originalPosition);

SectorEditorTopologyActionResult SetPortalBlocksPlayer(
        SectorTopologyMap& map,
        int lineDefId,
        bool blocksPlayer);

} // namespace game
