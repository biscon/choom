#pragma once

#include "sector_demo/SectorTypes.h"

namespace game {

struct SectorLightmapLayout;

SectorMeshBuildResult BuildSectorMeshes(const SectorMap& map, const SectorLightmapLayout* lightmapLayout = nullptr);
void UnloadSectorMeshes(SectorMeshBuildResult& buildResult);

} // namespace game
