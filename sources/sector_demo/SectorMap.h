#pragma once

#include "sector_demo/SectorTypes.h"

namespace game {

bool LoadSectorMap(const char* path, SectorMap& outMap);
bool SaveSectorMap(const char* path, const SectorMap& map);
EffectiveEdgeSettings GetEffectiveEdgeSettings(const SectorDefinition& sector, int edgeIndex);
EdgeNeighborInfo FindReverseEdgeNeighbor(const SectorMap& map, int sectorIndex, int edgeIndex);

} // namespace game
