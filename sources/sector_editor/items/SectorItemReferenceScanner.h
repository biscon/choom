#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace game {

// Scans authoring-graph v4 level JSON without requiring the item placement
// schema to be installed in the runtime serializer yet.
bool CountItemDefinitionReferencesInLevels(
        const std::filesystem::path& levelsRoot,
        const std::unordered_set<std::string>& definitionIds,
        std::unordered_map<std::string, size_t>& outCounts,
        std::string& error);

} // namespace game
