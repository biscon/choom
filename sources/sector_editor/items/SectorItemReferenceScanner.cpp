#include "sector_editor/items/SectorItemReferenceScanner.h"

#include "util/json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace game {
namespace {

using Json = nlohmann::ordered_json;

bool CollectJsonFiles(
        const std::filesystem::path& root,
        std::vector<std::filesystem::path>& paths,
        std::string& error)
{
    paths.clear();
    std::error_code filesystemError;
    if (!std::filesystem::exists(root, filesystemError)) {
        if (filesystemError) {
            error = "Could not inspect levels directory: "
                    + filesystemError.message();
            return false;
        }
        return true;
    }
    std::filesystem::recursive_directory_iterator iterator(
            root,
            std::filesystem::directory_options::none,
            filesystemError);
    const std::filesystem::recursive_directory_iterator end;
    if (filesystemError) {
        error = "Could not scan levels directory: " + filesystemError.message();
        return false;
    }
    while (iterator != end) {
        if (iterator->is_regular_file(filesystemError)
                && !filesystemError
                && iterator->path().extension() == ".json") {
            paths.push_back(iterator->path());
        }
        if (filesystemError) {
            error = "Could not inspect level candidate: "
                    + filesystemError.message();
            return false;
        }
        iterator.increment(filesystemError);
        if (filesystemError) {
            error = "Could not scan levels directory: " + filesystemError.message();
            return false;
        }
    }
    std::sort(paths.begin(), paths.end());
    return true;
}

bool ReadFile(
        const std::filesystem::path& path,
        std::string& contents,
        std::string& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = "Could not open level candidate '" + path.generic_string() + "'";
        return false;
    }
    std::ostringstream stream;
    stream << input.rdbuf();
    if (!input.good() && !input.eof()) {
        error = "Could not read level candidate '" + path.generic_string() + "'";
        return false;
    }
    contents = stream.str();
    return true;
}

} // namespace

bool CountItemDefinitionReferencesInLevels(
        const std::filesystem::path& levelsRoot,
        const std::unordered_set<std::string>& definitionIds,
        std::unordered_map<std::string, size_t>& outCounts,
        std::string& error)
{
    outCounts.clear();
    for (const std::string& id : definitionIds) outCounts.emplace(id, 0);
    std::vector<std::filesystem::path> paths;
    if (!CollectJsonFiles(levelsRoot, paths, error)) return false;
    try {
        for (const std::filesystem::path& path : paths) {
            std::string contents;
            if (!ReadFile(path, contents, error)) return false;
            const Json root = Json::parse(contents);
            if (!root.is_object()
                    || root.value("formatVersion", 0) != 4
                    || root.value("topology", std::string{})
                            != "authoringGraph") {
                continue;
            }
            const auto runtimeObjects = root.find("runtimeObjects");
            if (runtimeObjects == root.end()) continue;
            if (!runtimeObjects->is_array()) {
                error = "Level candidate '" + path.generic_string()
                        + "' has a non-array runtimeObjects field";
                return false;
            }
            for (size_t index = 0; index < runtimeObjects->size(); ++index) {
                const Json& object = (*runtimeObjects)[index];
                if (!object.is_object()) {
                    error = "Level candidate '" + path.generic_string()
                            + "' has a non-object runtime object";
                    return false;
                }
                const auto kind = object.find("kind");
                if (kind == object.end() || !kind->is_string()
                        || kind->get<std::string>() != "item") {
                    continue;
                }
                const auto item = object.find("item");
                if (item == object.end() || !item->is_object()) {
                    error = "Level candidate '" + path.generic_string()
                            + "' has an item without an item payload";
                    return false;
                }
                const auto definitionId = item->find("definitionId");
                if (definitionId == item->end() || !definitionId->is_string()) {
                    error = "Level candidate '" + path.generic_string()
                            + "' has an item without a string definitionId";
                    return false;
                }
                const auto count = outCounts.find(
                        definitionId->get<std::string>());
                if (count != outCounts.end()) ++count->second;
            }
        }
    } catch (const std::exception& exception) {
        error = std::string{"Could not parse level candidate: "}
                + exception.what();
        return false;
    }
    error.clear();
    return true;
}

} // namespace game
