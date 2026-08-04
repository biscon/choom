#pragma once

#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingState.h"

#include <filesystem>
#include <string>

namespace game {

class SectorEditorStaticModelPickerService {
public:
    explicit SectorEditorStaticModelPickerService(
            StaticModelPickerState& state,
            std::string& statusText);

    void Open(const std::string& currentModelPath);
    void Close();
    bool Refresh();
    bool RefreshFromRoot(
            const std::filesystem::path& modelsRoot,
            const std::string& assetRelativeRoot);
    bool SelectIndex(int index);

    bool HasSelection() const;
    std::string SelectedModelPath() const;
    StaticModelPickerState& State() { return state_; }
    const StaticModelPickerState& State() const { return state_; }

private:
    void RebuildOptionLabels();
    void RestoreRequestedSelection();

    StaticModelPickerState& state_;
    std::string& statusText_;
};

} // namespace game
