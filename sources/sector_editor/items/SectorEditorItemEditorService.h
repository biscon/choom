#pragma once

#include "sector_editor/items/SectorEditorItemEditorState.h"

#include <filesystem>
#include <string>

namespace game {

class SectorEditorItemEditorService {
public:
    SectorEditorItemEditorService(
            SectorEditorItemEditorState& state,
            SectorEditorItemEditorSessionState& session,
            ItemRegistry& registry,
            const FpsWeaponRegistry& weapons,
            bool gameSessionExists,
            std::string& statusText,
            std::filesystem::path registryPath,
            std::filesystem::path levelsRoot);

    bool Open();
    void Cancel();
    void Shutdown();
    bool SaveAndClose();
    ItemDefinition* SelectedItem();
    const ItemDefinition* SelectedItem() const;
    bool SelectIndex(int index);
    void AddItem();
    bool RequestDeleteSelected();
    void CancelDelete();
    void ConfirmDeleteSelected();
    void ApplyTitleBuffer();
    void ApplyDescriptionBuffer();
    void ApplyModelPathBuffer();
    void SetModelPath(const std::string& path);
    void SetType(ItemType type);
    void SetWeaponIndex(int index);
    int SelectedWeaponIndex() const;

    SectorEditorItemEditorState& State() { return state_; }
    SectorEditorItemEditorSessionState& Session() { return session_; }

private:
    std::string UniqueId() const;
    void Close();
    void SyncBuffers();
    void RebuildLabels();
    bool ScanDeletedReferences(std::string& error) const;

    SectorEditorItemEditorState& state_;
    SectorEditorItemEditorSessionState& session_;
    ItemRegistry& registry_;
    const FpsWeaponRegistry& weapons_;
    bool gameSessionExists_ = false;
    std::string& statusText_;
    std::filesystem::path registryPath_;
    std::filesystem::path levelsRoot_;
};

} // namespace game
