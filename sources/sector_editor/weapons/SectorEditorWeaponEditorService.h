#pragma once

#include "sector_editor/weapons/SectorEditorWeaponEditorState.h"
#include "game/items/ItemDefinitions.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace engine { class AssetManager; }

namespace game {

class SectorEditorWeaponEditorService {
public:
    SectorEditorWeaponEditorService(
            SectorEditorWeaponEditorState& state,
            SectorEditorWeaponEditorSessionState& session,
            FpsWeaponRegistry& registry,
            const ItemRegistry& itemRegistry,
            FpsApplicationSettings& applicationSettings,
            std::string& statusText,
            std::filesystem::path registryPath,
            std::filesystem::path applicationSettingsPath);

    bool Open(std::string_view activeWeaponId, bool fromPreview3D);
    void Cancel();
    bool SaveAndClose(engine::AssetManager& assets);
    void Shutdown();

    FpsWeaponDefinition* SelectedWeapon();
    const FpsWeaponDefinition* SelectedWeapon() const;
    bool SelectIndex(int index);
    void AddDefault();
    void DuplicateSelected();
    void RequestDeleteSelected();
    void CancelDelete();
    void ConfirmDeleteSelected();
    bool SetSelectedWeaponSlot(int weaponSlot);

    void ApplyIdBuffer();
    void ApplyArmsModelPathBuffer();
    void ApplyIdleAnimationBuffer();
    void ApplyAttachmentModelPathBuffer();
    void ApplyAttachmentBoneBuffer();
    void ApplyShootSoundBuffer(engine::AssetManager& assets);
    void SetArmsModelPath(const std::string& path);
    void SetAttachmentModelPath(const std::string& path);
    void SetShootSoundPath(const std::string& path, engine::AssetManager& assets);

    bool ConsumePreviewReloadRequest();
    const FpsWeaponRegistry& PreviewRegistry() const { return state_.draftRegistry; }
    const FpsApplicationSettings& PreviewApplicationSettings() const
    {
        return state_.previewApplicationSettings;
    }
    std::string SelectedWeaponId() const;

    SectorEditorWeaponEditorState& State() { return state_; }
    const SectorEditorWeaponEditorState& State() const { return state_; }
    SectorEditorWeaponEditorSessionState& Session() { return session_; }

private:
    std::string UniqueId(std::string base) const;
    void Close();
    void SyncBuffersFromSelection();
    void RebuildListLabels();
    void RequestPreviewReload();

    SectorEditorWeaponEditorState& state_;
    SectorEditorWeaponEditorSessionState& session_;
    FpsWeaponRegistry& registry_;
    const ItemRegistry& itemRegistry_;
    FpsApplicationSettings& applicationSettings_;
    std::string& statusText_;
    std::filesystem::path registryPath_;
    std::filesystem::path applicationSettingsPath_;
};

} // namespace game
