#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <string>

namespace game {

bool LoadSectorTopologyMap(
        const char* path,
        SectorTopologyMap& outMap,
        std::string* outError = nullptr);

bool SaveSectorTopologyMap(
        const char* path,
        const SectorTopologyMap& map,
        std::string* outError = nullptr);

bool LoadSectorTopologyMapFromJsonString(
        const std::string& jsonText,
        SectorTopologyMap& outMap,
        std::string* outError = nullptr);

bool SaveSectorTopologyMapToJsonString(
        const SectorTopologyMap& map,
        std::string& outJsonText,
        std::string* outError = nullptr);

} // namespace game
