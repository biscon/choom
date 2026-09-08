#pragma once

#include "sector_demo/SectorGeneratedGeometry.h"

namespace game {

// Explicit geometry-build phase only. Uses the already extracted sector loops.
bool AppendSectorBaseboardGeometry(const SectorTopologyMap& map,
        const SectorTopologySector& sector, const SectorTopologyLoopSet& loops,
        SectorGeneratedGeometry& geometry, std::string* error);

} // namespace game
