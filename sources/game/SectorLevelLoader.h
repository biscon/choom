#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <string>

namespace game {

bool IsValidApplicationLevelName(const std::string& name);
std::string ApplicationLevelAssetPath(const std::string& name);
bool LoadSectorRuntimeLevel(
        const std::string& path,
        SectorTopologyMap& outMap,
        std::string& error);

} // namespace game
