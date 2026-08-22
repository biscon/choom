#pragma once

#include "sector_demo/SectorMaterialRegistry.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace game {

// Counts material references in valid authoring-graph level documents below
// levelsRoot. Non-level JSON files are ignored; malformed v4 level documents
// are reported as errors so a registry edit cannot silently miss references.
bool CountSectorMaterialReferencesInLevels(
        const std::filesystem::path& levelsRoot,
        const std::unordered_set<std::string>& materialIds,
        std::unordered_map<std::string, size_t>& outCounts,
        std::string& error);

// Atomically stages the registry and every affected v4 level document before
// replacing any live file. Deletions are rejected while a material is used.
bool SaveSectorMaterialRegistryWithLevelRefactors(
        const std::filesystem::path& registryPath,
        const std::filesystem::path& levelsRoot,
        const SectorMaterialRegistry& registry,
        const std::unordered_map<std::string, std::string>& renamedIds,
        const std::unordered_set<std::string>& deletedIds,
        std::string& error);

} // namespace game
