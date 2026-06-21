#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <string>
#include <vector>

namespace game {

struct SectorTopologyCoordPoint {
    SectorCoord x = 0;
    SectorCoord y = 0;
};

struct SectorTopologyCreatePolygonOptions {
    std::string sectorName;
    float floorZ = 0.0f;
    float ceilingZ = 24.0f;

    std::string floorTextureId = "floor";
    std::string ceilingTextureId = "ceiling";

    SectorTopologyUvSettings floorUv;
    SectorTopologyUvSettings ceilingUv;

    Color ambientColor = WHITE;
    float ambientIntensity = 1.0f;

    SectorTopologyWallPartSettings defaultWall;
    SectorTopologyWallPartSettings defaultLower;
    SectorTopologyWallPartSettings defaultUpper;
};

bool CreateSectorTopologyPolygon(
        SectorTopologyMap& map,
        const std::vector<SectorTopologyCoordPoint>& points,
        const SectorTopologyCreatePolygonOptions& options,
        int* outSectorId,
        std::string* outError);

} // namespace game
