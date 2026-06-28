#include "sector_editor/SectorEditorDocumentActions.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorTopologySerialization.h"
#include "util/json.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
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

using Json = nlohmann::ordered_json;

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

bool ReadTextFile(const std::string& filePath, std::string& outText, std::string& errorMessage)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        errorMessage = TextFormat("Failed to open level JSON: %s", filePath.c_str());
        return false;
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof()) {
        errorMessage = TextFormat("Failed to read level JSON: %s", filePath.c_str());
        return false;
    }

    outText = contents.str();
    errorMessage.clear();
    return true;
}

SectorEditorDocumentFormat DetectSectorEditorDocumentFormat(
        const std::string& jsonText,
        std::string& errorMessage)
{
    try {
        const Json root = Json::parse(jsonText);
        if (!root.is_object()) {
            errorMessage = "Level JSON root must be an object";
            return SectorEditorDocumentFormat::Unknown;
        }

        const auto versionIt = root.find("formatVersion");
        const auto topologyIt = root.find("topology");
        if (versionIt == root.end() || topologyIt == root.end()) {
            errorMessage = "Level JSON is missing formatVersion or topology";
            return SectorEditorDocumentFormat::Unknown;
        }
        if (!versionIt->is_number_integer() || !topologyIt->is_string()) {
            errorMessage = "Level JSON has invalid formatVersion or topology marker";
            return SectorEditorDocumentFormat::Unknown;
        }

        const int version = versionIt->get<int>();
        const std::string topology = topologyIt->get<std::string>();
        if (version == 3 && topology == "authoringGraph") {
            errorMessage.clear();
            return SectorEditorDocumentFormat::AuthoringGraph;
        }
        if (version == 2 && topology == "linedef") {
            errorMessage.clear();
            return SectorEditorDocumentFormat::TopologyV2Import;
        }

        errorMessage = TextFormat(
                "Unsupported level document format: formatVersion %d, topology '%s'",
                version,
                topology.c_str());
        return SectorEditorDocumentFormat::Unknown;
    } catch (const std::exception& exception) {
        errorMessage = TextFormat("Could not parse level JSON: %s", exception.what());
        return SectorEditorDocumentFormat::Unknown;
    }
}

SectorAuthoringDocument BuildSectorAuthoringDocumentFromEditorState(
        const SectorEditorState& state)
{
    SectorAuthoringDocument document;
    document.graph = state.authoringGraph;
    document.mapData = state.topologyMap;
    document.mapData.vertices.clear();
    document.mapData.lineDefs.clear();
    document.mapData.sideDefs.clear();
    document.mapData.sectors.clear();
    document.mapData.bakedLightmap = {};
    document.derivation = state.authoringDerivation;
    return document;
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

bool LoadSectorEditorDocumentFromAsset(
        const std::string& jsonAssetPath,
        SectorEditorLoadedDocument& outDocument,
        std::string& errorMessage)
{
    outDocument = SectorEditorLoadedDocument{};
    const std::string filePath = ResolveEditorAssetPath(jsonAssetPath);

    std::string jsonText;
    if (!ReadTextFile(filePath, jsonText, errorMessage)) {
        return false;
    }

    const SectorEditorDocumentFormat format =
            DetectSectorEditorDocumentFormat(jsonText, errorMessage);
    if (format == SectorEditorDocumentFormat::AuthoringGraph) {
        SectorAuthoringDocument document;
        std::string loadError;
        if (!LoadSectorAuthoringDocumentFromJsonString(jsonText, document, &loadError)) {
            errorMessage = loadError.empty()
                    ? "Authoring graph load failed"
                    : TextFormat("Authoring graph load failed: %s", loadError.c_str());
            return false;
        }
        outDocument.format = format;
        outDocument.mapData = std::move(document.mapData);
        outDocument.authoringGraph = std::move(document.graph);
        errorMessage.clear();
        return true;
    }

    if (format == SectorEditorDocumentFormat::TopologyV2Import) {
        SectorTopologyMap map;
        std::string loadError;
        if (!LoadSectorTopologyMapFromJsonString(jsonText, map, &loadError)) {
            errorMessage = loadError.empty()
                    ? "Topology v2 load failed"
                    : TextFormat("Topology v2 load failed: %s", loadError.c_str());
            return false;
        }
        outDocument.format = format;
        outDocument.mapData = map;
        outDocument.authoringGraph = ImportSectorTopologyMapToAuthoringGraph(map);
        errorMessage.clear();
        return true;
    }

    if (errorMessage.empty()) {
        errorMessage = "Unsupported level document format";
    }
    return false;
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

bool SaveSectorEditorAuthoringDocument(
        const LevelPaths& paths,
        const SectorEditorState& state,
        std::string& errorMessage)
{
    if (!HasAuthoringGraphData(state)) {
        errorMessage = "Cannot save authoring graph document: no authoring graph data";
        return false;
    }

    const SectorAuthoringDocument document =
            BuildSectorAuthoringDocumentFromEditorState(state);

    std::string saveError;
    if (!SaveSectorAuthoringDocument(paths.jsonFilePath.string().c_str(), document, &saveError)) {
        errorMessage = saveError.empty()
                ? "Could not write authoring graph level JSON"
                : TextFormat("Could not write authoring graph level JSON: %s", saveError.c_str());
        return false;
    }
    errorMessage.clear();
    return true;
}

} // namespace game
