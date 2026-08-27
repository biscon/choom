#pragma once

#include "sector_editor/player/SectorEditorPlayerSettingsState.h"

#include <filesystem>

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
            std::filesystem::path settingsPath);

    void Open(engine::EngineContext& context);
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

    SectorEditorPlayerSettingsState& State() { return state_; }

private:
    void StopAudioPreview(engine::EngineContext& context);
    void BuildCatalogLabels();
    bool ValidateCatalogReferences(std::string& error) const;

    SectorEditorPlayerSettingsState& state_;
    FpsApplicationSettings& settings_;
    std::string& statusText_;
    std::filesystem::path settingsPath_;
};

} // namespace game
