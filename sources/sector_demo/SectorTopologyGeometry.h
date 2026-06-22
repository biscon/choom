#pragma once

#include "sector_demo/SectorTopologyCreation.h"

namespace game {

bool SectorTopologyPointOnSegment(
        SectorTopologyCoordPoint point,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b);

bool SectorTopologyPointStrictlyInsideSegment(
        SectorTopologyCoordPoint point,
        SectorTopologyCoordPoint a,
        SectorTopologyCoordPoint b);

} // namespace game
