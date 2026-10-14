#pragma once
#include "sector_demo/particles/SectorParticleSettings.h"
#include "util/json.hpp"

namespace game {
nlohmann::ordered_json WriteSectorParticleSettings(const SectorParticleSettings& settings);
SectorParticleSettings ReadSectorParticleSettings(const nlohmann::ordered_json& value);
} // namespace game
