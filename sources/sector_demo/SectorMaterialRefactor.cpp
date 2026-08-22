#include "sector_demo/SectorMaterialRefactor.h"

#include "util/json.hpp"

#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace game {
namespace {

using Json = nlohmann::ordered_json;

struct PendingFile {
    std::filesystem::path path;
    std::filesystem::path stagedPath;
    std::filesystem::path backupPath;
    std::string contents;
    bool replaced = false;
};

bool ReadFile(const std::filesystem::path& path, std::string& contents, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Could not open '" + path.generic_string() + "'";
        return false;
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    if (!file.good() && !file.eof()) {
        error = "Could not read '" + path.generic_string() + "'";
        return false;
    }
    contents = stream.str();
    return true;
}

bool WriteFile(const std::filesystem::path& path, const std::string& contents, std::string& error)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "Could not stage '" + path.generic_string() + "'";
        return false;
    }
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!file) {
        error = "Could not write staged file '" + path.generic_string() + "'";
        return false;
    }
    return true;
}

bool IsMaterialReferenceKey(const std::string& key)
{
    return key == "materialId"
            || key == "floorMaterialId"
            || key == "ceilingMaterialId";
}

void VisitMaterialReferences(
        Json& value,
        const std::function<void(std::string&)>& visitor)
{
    if (value.is_object()) {
        for (auto& entry : value.items()) {
            if (IsMaterialReferenceKey(entry.key()) && entry.value().is_string()) {
                std::string materialId = entry.value().get<std::string>();
                visitor(materialId);
                entry.value() = materialId;
            } else {
                VisitMaterialReferences(entry.value(), visitor);
            }
        }
    } else if (value.is_array()) {
        for (Json& entry : value) VisitMaterialReferences(entry, visitor);
    }
}

bool IsV4LevelDocument(const Json& root)
{
    return root.is_object()
            && root.value("formatVersion", 0) == 4
            && root.value("topology", std::string{}) == "authoringGraph";
}

bool CollectLevelFiles(
        const std::filesystem::path& levelsRoot,
        std::vector<std::filesystem::path>& outPaths,
        std::string& error)
{
    outPaths.clear();
    std::error_code iteratorError;
    if (!std::filesystem::exists(levelsRoot, iteratorError)) {
        if (iteratorError) {
            error = "Could not inspect levels directory: " + iteratorError.message();
            return false;
        }
        return true;
    }
    for (std::filesystem::recursive_directory_iterator iterator(levelsRoot, iteratorError), end;
            iterator != end;
            iterator.increment(iteratorError)) {
        if (iteratorError) {
            error = "Could not scan levels directory: " + iteratorError.message();
            return false;
        }
        if (iterator->is_regular_file() && iterator->path().extension() == ".json") {
            outPaths.push_back(iterator->path());
        }
    }
    std::sort(outPaths.begin(), outPaths.end());
    return true;
}

void RemoveIfPresent(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

} // namespace

bool CountSectorMaterialReferencesInLevels(
        const std::filesystem::path& levelsRoot,
        const std::unordered_set<std::string>& materialIds,
        std::unordered_map<std::string, size_t>& outCounts,
        std::string& error)
{
    outCounts.clear();
    for (const std::string& id : materialIds) outCounts.emplace(id, 0);
    std::vector<std::filesystem::path> paths;
    if (!CollectLevelFiles(levelsRoot, paths, error)) return false;
    try {
        for (const std::filesystem::path& path : paths) {
            std::string contents;
            if (!ReadFile(path, contents, error)) return false;
            Json root = Json::parse(contents);
            if (!IsV4LevelDocument(root)) continue;
            VisitMaterialReferences(root, [&](std::string& id) {
                const auto it = outCounts.find(id);
                if (it != outCounts.end()) ++it->second;
            });
        }
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
    error.clear();
    return true;
}

bool SaveSectorMaterialRegistryWithLevelRefactors(
        const std::filesystem::path& registryPath,
        const std::filesystem::path& levelsRoot,
        const SectorMaterialRegistry& registry,
        const std::unordered_map<std::string, std::string>& renamedIds,
        const std::unordered_set<std::string>& deletedIds,
        std::string& error)
{
    std::string registryJson;
    if (!SerializeSectorMaterialRegistryJson(registry, registryJson, error)) return false;

    std::vector<std::filesystem::path> levelPaths;
    if (!CollectLevelFiles(levelsRoot, levelPaths, error)) return false;
    std::vector<PendingFile> pending;
    pending.reserve(levelPaths.size() + 1);
    try {
        for (const std::filesystem::path& path : levelPaths) {
            std::string contents;
            if (!ReadFile(path, contents, error)) return false;
            Json root = Json::parse(contents);
            if (!IsV4LevelDocument(root)) continue;
            bool changed = false;
            VisitMaterialReferences(root, [&](std::string& id) {
                if (deletedIds.find(id) != deletedIds.end()) {
                    throw std::runtime_error(
                            "Cannot delete material '" + id
                            + "': it is referenced by " + path.generic_string());
                }
                const auto rename = renamedIds.find(id);
                if (rename != renamedIds.end() && rename->second != id) {
                    id = rename->second;
                    changed = true;
                }
            });
            if (changed) {
                pending.push_back(PendingFile{path, {}, {}, root.dump(2) + "\n"});
            }
        }
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
    pending.push_back(PendingFile{registryPath, {}, {}, std::move(registryJson)});

    for (PendingFile& file : pending) {
        std::error_code directoryError;
        std::filesystem::create_directories(file.path.parent_path(), directoryError);
        if (directoryError) {
            error = "Could not create material directory: " + directoryError.message();
            return false;
        }
        file.stagedPath = file.path;
        file.stagedPath += ".material-edit.tmp";
        file.backupPath = file.path;
        file.backupPath += ".material-edit.bak";
        RemoveIfPresent(file.stagedPath);
        RemoveIfPresent(file.backupPath);
        if (!WriteFile(file.stagedPath, file.contents, error)) {
            for (const PendingFile& cleanup : pending) RemoveIfPresent(cleanup.stagedPath);
            return false;
        }
    }

    for (PendingFile& file : pending) {
        std::error_code operationError;
        if (std::filesystem::exists(file.path)) {
            std::filesystem::rename(file.path, file.backupPath, operationError);
            if (operationError) {
                error = "Could not back up '" + file.path.generic_string()
                        + "': " + operationError.message();
                break;
            }
        }
        std::filesystem::rename(file.stagedPath, file.path, operationError);
        if (operationError) {
            if (std::filesystem::exists(file.backupPath)) {
                std::error_code restoreError;
                std::filesystem::rename(file.backupPath, file.path, restoreError);
            }
            error = "Could not replace '" + file.path.generic_string()
                    + "': " + operationError.message();
            break;
        }
        file.replaced = true;
    }

    if (!error.empty()) {
        for (auto iterator = pending.rbegin(); iterator != pending.rend(); ++iterator) {
            if (!iterator->replaced) continue;
            RemoveIfPresent(iterator->path);
            std::error_code restoreError;
            if (std::filesystem::exists(iterator->backupPath)) {
                std::filesystem::rename(iterator->backupPath, iterator->path, restoreError);
            }
        }
        for (const PendingFile& cleanup : pending) {
            RemoveIfPresent(cleanup.stagedPath);
            RemoveIfPresent(cleanup.backupPath);
        }
        return false;
    }

    for (const PendingFile& cleanup : pending) RemoveIfPresent(cleanup.backupPath);
    error.clear();
    return true;
}

} // namespace game
