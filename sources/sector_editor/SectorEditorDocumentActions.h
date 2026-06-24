#pragma once

#include "sector_editor/SectorEditorTypes.h"

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

bool IsValidLevelName(const std::string& name, std::string& error);
bool BuildLevelPaths(const std::string& name, LevelPaths& paths, std::string& error);
SectorTopologyMap CreateEmptySectorTopologyDocument();
void ResetEditorTopologyDocumentState(SectorEditorState& state);
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
bool LoadSectorTopologyDocumentFromAsset(
        const std::string& jsonAssetPath,
        SectorTopologyMap& outMap,
        std::string& errorMessage);
bool PrepareSaveLevelPlan(
        const std::string& name,
        bool hasCurrentLevelPath,
        const std::string& currentLevelPath,
        bool overwriteConfirmed,
        SectorEditorSaveLevelPlan& outPlan,
        std::string& errorMessage);
bool EnsureSaveLevelDirectory(const LevelPaths& paths, std::string& errorMessage);
bool SaveSectorTopologyDocument(
        const LevelPaths& paths,
        const SectorTopologyMap& map,
        std::string& errorMessage);

} // namespace game
