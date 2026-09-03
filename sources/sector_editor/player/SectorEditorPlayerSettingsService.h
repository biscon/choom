#pragma once

#include "sector_editor/player/SectorEditorPlayerSettingsState.h"

#include <filesystem>
#include <optional>

namespace engine { struct EngineContext; }

namespace game {

struct SectorEditorPlayerSettingsSaveResult {
    bool saved = false;
    bool playerAudioChanged = false;
    bool footstepsChanged = false;
};

class SectorEditorPlayerSettingsService {
public:
    SectorEditorPlayerSettingsService(
            SectorEditorPlayerSettingsState& state,
            FpsApplicationSettings& settings,
            std::string& statusText,
            SectorEditorAudioAssetPickerSessionState& audioPickerSession,
            std::filesystem::path settingsPath);

    void Open(
            engine::EngineContext& context,
            std::optional<SectorEditorPlayerSettingsTab> selectedTab =
                    std::nullopt);
    void Cancel(engine::EngineContext& context);
    void Shutdown(engine::EngineContext& context);
    SectorEditorPlayerSettingsSaveResult SaveAndClose(
            engine::EngineContext& context);
    void ResetActiveTab();
    void AddSoundEvent();
    void RemoveSoundEvent(size_t index);
    void PreviewFootstepSet(engine::EngineContext& context);
    void PreviewPlayerSoundSet(
            engine::EngineContext& context,
            std::string_view setId);
    void UpdateAudioPreview(engine::EngineContext& context);
    void OpenLiquidAudioPicker(
            engine::EngineContext& context,
            SectorEditorPlayerLiquidAudioPickerTarget target);
    SectorEditorAudioAssetPickerResult DrawLiquidAudioPicker(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::FontHandle font,
            engine::EngineContext& context);

    SectorEditorPlayerSettingsState& State() { return state_; }

private:
    void StopAudioPreview(engine::EngineContext& context);
    void BuildCatalogLabels();
    bool ValidateCatalogReferences(std::string& error) const;

    SectorEditorPlayerSettingsState& state_;
    FpsApplicationSettings& settings_;
    std::string& statusText_;
    SectorEditorAudioAssetPickerSessionState& audioPickerSession_;
    std::filesystem::path settingsPath_;
};

} // namespace game
