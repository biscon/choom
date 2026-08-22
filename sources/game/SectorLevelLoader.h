#pragma once

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorMaterialRegistry.h"

#include <string>
#include <optional>

namespace game {

struct SectorLevelEntryRequest {
    std::string levelName;
    std::optional<std::string> markerId;
};

bool IsValidApplicationLevelName(const std::string& name);
std::string ApplicationLevelAssetPath(const std::string& name);
bool LoadSectorRuntimeLevel(
        const std::string& path,
        const SectorMaterialRegistry& materials,
        SectorTopologyMap& outMap,
        std::string& error);
bool ResolveSectorLevelEntryMarker(
        const SectorTopologyMap& map,
        const std::optional<std::string>& requestedMarkerId,
        const SectorCompiledLevelMarker*& outMarker,
        std::string& error);

} // namespace game
