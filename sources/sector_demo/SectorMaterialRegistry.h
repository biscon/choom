#pragma once

#include "sector_demo/SectorTextureTypes.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace game {

struct SectorTopologyMap;

inline constexpr int kSectorMaterialRegistryFormatVersion = 1;
inline constexpr const char* kSectorMaterialRegistryAssetPath =
        "assets/materials/materials.json";

struct SectorMaterialRegistry {
    std::unordered_map<std::string, SectorMaterialDefinition> materialsById;
};

bool IsValidSectorMaterialId(std::string_view id);
bool ValidateSectorMaterialDefinition(
        const SectorMaterialDefinition& material,
        std::string& error);
const SectorMaterialDefinition* FindSectorMaterial(
        const SectorMaterialRegistry& registry,
        std::string_view id);
std::vector<std::string> SortedSectorMaterialIds(
        const SectorMaterialRegistry& registry);

bool LoadSectorMaterialRegistry(
        const std::filesystem::path& path,
        SectorMaterialRegistry& outRegistry,
        std::string& error);
bool SaveSectorMaterialRegistry(
        const std::filesystem::path& path,
        const SectorMaterialRegistry& registry,
        std::string& error);
bool ParseSectorMaterialRegistryJson(
        std::string_view jsonText,
        SectorMaterialRegistry& outRegistry,
        std::string& error);
bool SerializeSectorMaterialRegistryJson(
        const SectorMaterialRegistry& registry,
        std::string& outJson,
        std::string& error);

// Rebuilds the transient material subset carried by derived/runtime topology.
// Missing definitions are reported but do not make the map unusable.
std::vector<std::string> ResolveSectorMaterialsForMap(
        SectorTopologyMap& map,
        const SectorMaterialRegistry& registry);

} // namespace game
