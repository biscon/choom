#pragma once

#include "sector_demo/SectorTypes.h"

namespace game {

SectorMeshBuildResult BuildSectorMeshes(const SectorMap& map);
void UnloadSectorMeshes(SectorMeshBuildResult& buildResult);

} // namespace game
