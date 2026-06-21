#pragma once

#include "sector_demo/SectorTopologyCreation.h"

#include <string>

namespace game {

bool MoveSectorTopologyVertex(
        SectorTopologyMap& map,
        int vertexId,
        SectorTopologyCoordPoint newPosition,
        std::string* outError = nullptr);

} // namespace game
