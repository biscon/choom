#pragma once

#include "sector_demo/SectorTopologyCreation.h"

#include <string>

namespace game {

struct SectorTopologySplitLineResult {
    int midpointVertexId = -1;

    int firstLineDefId = -1;
    int secondLineDefId = -1;

    int firstFrontSideDefId = -1;
    int firstBackSideDefId = -1;

    int secondFrontSideDefId = -1;
    int secondBackSideDefId = -1;
};

bool MoveSectorTopologyVertex(
        SectorTopologyMap& map,
        int vertexId,
        SectorTopologyCoordPoint newPosition,
        std::string* outError = nullptr);

bool SplitSectorTopologyLineDef(
        SectorTopologyMap& map,
        int lineDefId,
        SectorTopologySplitLineResult* outResult = nullptr,
        std::string* outError = nullptr);

} // namespace game
