#include "sector_editor/SectorEditorDocumentActions.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorTopologySerialization.h"

#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <system_error>
#include <utility>

namespace game {

bool IsValidLevelName(const std::string& name, std::string& error)
{
    if (name.empty()) {
        error = "Level name cannot be empty";
        return false;
    }
    for (char ch : name) {
        const bool asciiLetter = (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
        const bool asciiDigit = ch >= '0' && ch <= '9';
        if (!(asciiLetter || asciiDigit || ch == '_' || ch == '-')) {
            error = "Use only letters, digits, underscore, or dash";
            return false;
        }
    }
    error.clear();
    return true;
}

bool BuildLevelPaths(const std::string& name, LevelPaths& paths, std::string& error)
{
    if (!IsValidLevelName(name, error)) {
        return false;
    }

    const std::filesystem::path relativeDirectory = std::filesystem::path("levels") / name;
    paths.directoryPath = std::filesystem::path(ASSETS_PATH) / relativeDirectory;
    paths.jsonFilePath = paths.directoryPath / (name + ".json");
    paths.lightmapFilePath = paths.directoryPath / (name + ".lightmap.png");
    paths.jsonAssetPath = (std::filesystem::path("assets") / relativeDirectory / (name + ".json")).generic_string();
    paths.lightmapAssetPath = (std::filesystem::path("assets") / relativeDirectory / (name + ".lightmap.png")).generic_string();
    error.clear();
    return true;
}

namespace {

template<typename TextureMap>
void PopulateDefaultSectorTextures(TextureMap& texturesById)
{
    const auto addTexture = [&texturesById](const char* id, const char* path) {
        SectorTextureDefinition definition;
        definition.id = id;
        definition.path = path;
        definition.filter = SectorTextureFilter::Anisotropic8x;
        texturesById.emplace(id, std::move(definition));
    };
    addTexture("wall", "assets/images/wall.png");
    addTexture("floor", "assets/images/floor.png");
    addTexture("ceiling", "assets/images/ceiling.png");
    addTexture("step_wall", "assets/images/wall.png");
    addTexture("upper_wall", "assets/images/wall.png");
}

} // namespace

SectorTopologyMap CreateEmptySectorTopologyDocument()
{
    SectorTopologyMap map;
    PopulateDefaultSectorTextures(map.texturesById);
    return map;
}

void ResetEditorTopologyDocumentState(SectorEditorState& state)
{
    state.topologyMap = CreateEmptySectorTopologyDocument();
    InitializeSectorEditorAuthoringStateFromTopology(state, state.topologyMap);
    state.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            state.topologyMap.previewSettings);
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    state.topologyDocumentStatus = "Topology document: empty";
}

std::vector<LevelListEntry> ScanLevels(std::string& error)
{
    std::vector<LevelListEntry> levels;
    error.clear();
    const std::filesystem::path levelsRoot = std::filesystem::path(ASSETS_PATH) / "levels";
    std::error_code ec;
    std::filesystem::create_directories(levelsRoot, ec);
    if (ec) {
        error = TextFormat("Could not create assets/levels: %s", ec.message().c_str());
        return levels;
    }

    std::filesystem::directory_iterator iterator(
            levelsRoot,
            std::filesystem::directory_options::skip_permission_denied,
            ec
    );
    const std::filesystem::directory_iterator end;
    if (ec) {
        error = TextFormat("Could not scan assets/levels: %s", ec.message().c_str());
        return levels;
    }

    for (; iterator != end; iterator.increment(ec)) {
        if (ec) {
            error = TextFormat("Stopped level scan early: %s", ec.message().c_str());
            break;
        }
        const std::filesystem::directory_entry& entry = *iterator;
        const std::filesystem::file_status status = entry.symlink_status(ec);
        if (ec || !std::filesystem::is_directory(status) || std::filesystem::is_symlink(status)) {
            ec.clear();
            continue;
        }

        const std::string name = entry.path().filename().string();
        std::string validationError;
        LevelPaths paths;
        if (!BuildLevelPaths(name, paths, validationError)) {
            continue;
        }
        const std::filesystem::file_status jsonStatus = std::filesystem::symlink_status(paths.jsonFilePath, ec);
        if (ec || !std::filesystem::is_regular_file(jsonStatus) || std::filesystem::is_symlink(jsonStatus)) {
            ec.clear();
            continue;
        }
        levels.push_back(LevelListEntry{name, paths.jsonAssetPath});
    }

    std::sort(levels.begin(), levels.end(), [](const LevelListEntry& a, const LevelListEntry& b) {
        return a.name < b.name;
    });
    return levels;
}

void OpenConfirmationModal(
        ConfirmationModalState& modalState,
        const char* title,
        const char* message,
        std::function<void()> onOkay)
{
    modalState.open = true;
    modalState.title = title == nullptr ? "Confirm" : title;
    modalState.message = message == nullptr ? "" : message;
    modalState.onOkay = std::move(onOkay);
}

void OpenSaveLevelModalState(
        SaveLevelModalState& modalState,
        bool hasCurrentLevelPath,
        const std::string& currentLevelName)
{
    modalState = SaveLevelModalState{};
    modalState.open = true;
    if (hasCurrentLevelPath) {
        std::snprintf(
                modalState.nameBuffer,
                sizeof(modalState.nameBuffer),
                "%s",
                currentLevelName.c_str()
        );
    }
}

void RefreshLoadLevelModalState(LoadLevelModalState& modalState)
{
    modalState.levels = ScanLevels(modalState.errorMessage);
    modalState.optionLabels.clear();
    modalState.optionLabels.reserve(modalState.levels.size());
    for (const LevelListEntry& level : modalState.levels) {
        modalState.optionLabels.push_back(level.name.c_str());
    }
    modalState.selectedIndex = modalState.levels.empty() ? -1 : 0;
    modalState.scroll = engine::UIScrollState{};
}

void OpenLoadLevelModalState(LoadLevelModalState& modalState)
{
    modalState = LoadLevelModalState{};
    modalState.open = true;
    RefreshLoadLevelModalState(modalState);
}

bool LoadSectorTopologyDocumentFromAsset(
        const std::string& jsonAssetPath,
        SectorTopologyMap& outMap,
        std::string& errorMessage)
{
    std::string loadError;
    const std::string filePath = ResolveEditorAssetPath(jsonAssetPath);
    if (!LoadSectorTopologyMap(filePath.c_str(), outMap, &loadError)) {
        errorMessage = loadError.empty()
                ? "Topology v2 load failed"
                : TextFormat("Topology v2 load failed: %s", loadError.c_str());
        return false;
    }
    errorMessage.clear();
    return true;
}

bool PrepareSaveLevelPlan(
        const std::string& name,
        bool hasCurrentLevelPath,
        const std::string& currentLevelPath,
        bool overwriteConfirmed,
        SectorEditorSaveLevelPlan& outPlan,
        std::string& errorMessage)
{
    outPlan = SectorEditorSaveLevelPlan{};
    if (!BuildLevelPaths(name, outPlan.paths, errorMessage)) {
        return false;
    }

    outPlan.savingToCurrentPath = hasCurrentLevelPath
            && currentLevelPath == outPlan.paths.jsonAssetPath;
    std::error_code ec;
    outPlan.targetExists = std::filesystem::exists(outPlan.paths.jsonFilePath, ec);
    if (ec) {
        errorMessage = TextFormat("Could not check target level: %s", ec.message().c_str());
        return false;
    }
    outPlan.needsOverwriteConfirmation = outPlan.targetExists
            && !outPlan.savingToCurrentPath
            && !overwriteConfirmed;
    errorMessage.clear();
    return true;
}

bool EnsureSaveLevelDirectory(const LevelPaths& paths, std::string& errorMessage)
{
    std::error_code ec;
    std::filesystem::create_directories(paths.directoryPath, ec);
    if (ec) {
        errorMessage = TextFormat("Could not create level directory: %s", ec.message().c_str());
        return false;
    }
    errorMessage.clear();
    return true;
}

bool SaveSectorTopologyDocument(
        const LevelPaths& paths,
        const SectorTopologyMap& map,
        std::string& errorMessage)
{
    std::string saveError;
    if (!SaveSectorTopologyMap(paths.jsonFilePath.string().c_str(), map, &saveError)) {
        errorMessage = saveError.empty()
                ? "Could not write topology level JSON"
                : TextFormat("Could not write topology level JSON: %s", saveError.c_str());
        return false;
    }
    errorMessage.clear();
    return true;
}

} // namespace game
