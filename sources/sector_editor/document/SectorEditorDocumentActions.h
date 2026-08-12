#pragma once

#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "sector_demo/SectorTopologySerialization.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace game {

struct LevelPaths {
    std::string jsonAssetPath;
    std::string lightmapAssetPath;
    std::filesystem::path jsonFilePath;
    std::filesystem::path lightmapFilePath;
    std::filesystem::path directoryPath;
};

struct SectorEditorSaveLevelPlan {
    LevelPaths paths;
    bool savingToCurrentPath = false;
    bool targetExists = false;
    bool needsOverwriteConfirmation = false;
};

enum class SectorEditorDocumentFormat {
    Unknown,
    AuthoringGraph,
    TopologyV2Import
};

struct SectorEditorLoadedDocument {
    SectorEditorDocumentFormat format = SectorEditorDocumentFormat::Unknown;
    SectorTopologyMap mapData;
    SectorAuthoringGraph authoringGraph;
    SectorAuthoringEditorSettings editorSettings;
};

bool IsValidLevelName(const std::string& name, std::string& error);
bool BuildLevelPaths(const std::string& name, LevelPaths& paths, std::string& error);
SectorTopologyMap CreateEmptySectorTopologyDocument();
void ResetEditorTopologyDocumentState(
        SectorEditorDocumentLifecycleAccess lifecycle,
        SectorTopologyMap& topologyMap,
        SectorAuthoringGraph& authoringGraph,
        SectorEditorDerivationDocumentAccess derivation,
        SectorEditorPreviewControllerState& previewControllerState);
void ResetEditorTopologyDocumentState(
        SectorEditorDocumentState& documentState,
        SectorEditorPreviewControllerState& previewControllerState);
std::vector<LevelListEntry> ScanLevels(std::string& error);
void OpenConfirmationModal(
        ConfirmationModalState& modalState,
        const char* title,
        const char* message,
        std::function<void()> onOkay);
void OpenSaveLevelModalState(
        SaveLevelModalState& modalState,
        bool hasCurrentLevelPath,
        const std::string& currentLevelName);
void RefreshLoadLevelModalState(LoadLevelModalState& modalState);
void OpenLoadLevelModalState(LoadLevelModalState& modalState);
bool LoadSectorEditorDocumentFromAsset(
        const std::string& jsonAssetPath,
        SectorEditorLoadedDocument& outDocument,
        std::string& errorMessage);
bool PrepareSaveLevelPlan(
        const std::string& name,
        bool hasCurrentLevelPath,
        const std::string& currentLevelPath,
        bool overwriteConfirmed,
        SectorEditorSaveLevelPlan& outPlan,
        std::string& errorMessage);
bool EnsureSaveLevelDirectory(const LevelPaths& paths, std::string& errorMessage);
bool SaveSectorEditorAuthoringDocument(
        const LevelPaths& paths,
        const SectorAuthoringGraph& authoringGraph,
        const SectorTopologyMap& topologyMap,
        const SectorAuthoringDerivationResult& authoringDerivation,
        const SectorAuthoringEditorSettings& editorSettings,
        std::string& errorMessage);
bool SaveSectorEditorAuthoringDocument(
        const LevelPaths& paths,
        const SectorEditorAuthoringDocumentState& authoring,
        const SectorEditorDocumentMapState& map,
        const SectorEditorDerivationState& derivation,
        const SectorAuthoringEditorSettings& editorSettings,
        std::string& errorMessage);

} // namespace game
