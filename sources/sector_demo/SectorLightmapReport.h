#pragma once

#include <string>

namespace game {

struct SectorLightmapBakeResult;

std::string FormatSectorLightmapBakeReport(const SectorLightmapBakeResult& result);
void PrintSectorLightmapBakeReport(const SectorLightmapBakeResult& result);

} // namespace game
