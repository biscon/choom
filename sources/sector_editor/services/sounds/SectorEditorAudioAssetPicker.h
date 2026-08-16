#pragma once

#include "engine/EngineContext.h"
#include "engine/ui/UI.h"
#include "sector_demo/SectorTopologyMap.h"

#include <string>
#include <vector>

namespace game {

struct SectorEditorAudioPreviewState {
    engine::AssetScopeHandle scope = engine::NullAssetScopeHandle();
    engine::SoundHandle sound = engine::NullSoundHandle();
    engine::MusicHandle music = engine::NullMusicHandle();
    engine::SoundPlaybackHandle soundPlayback = engine::NullSoundPlaybackHandle();
    SectorSoundType type = SectorSoundType::Sound;
    bool pending = false;
    std::string key;
};

struct SectorEditorAudioAssetPickerState {
    bool open = false;
    bool scanned = false;
    std::string title;
    std::string scanMessage;
    engine::UIScrollState scroll;
    std::vector<std::string> paths;
    std::vector<const char*> optionLabels;
    int selectedPathIndex = -1;
    SectorSoundType previewType = SectorSoundType::Sound;
    std::string previewMessage;
    SectorEditorAudioPreviewState preview;
};

struct SectorEditorAudioAssetPickerSessionState {
    engine::UIScrollState scroll;
};

inline void RestoreSectorEditorAudioAssetPickerScroll(
        SectorEditorAudioAssetPickerState& state,
        const SectorEditorAudioAssetPickerSessionState& session)
{
    state.scroll = session.scroll;
}

inline void RememberSectorEditorAudioAssetPickerScroll(
        const SectorEditorAudioAssetPickerState& state,
        SectorEditorAudioAssetPickerSessionState& session)
{
    session.scroll = state.scroll;
}

enum class SectorEditorAudioAssetPickerResult {
    None,
    Selected,
    Cancelled
};

class SectorEditorAudioAssetPickerService {
public:
    SectorEditorAudioAssetPickerService(
            engine::EngineContext& context,
            SectorEditorAudioAssetPickerSessionState& session);

    void Open(
            SectorEditorAudioAssetPickerState& state,
            const std::string& title,
            const std::string& currentPath = {},
            SectorSoundType previewType = SectorSoundType::Sound);
    void Close(SectorEditorAudioAssetPickerState& state);
    bool SelectIndex(SectorEditorAudioAssetPickerState& state, int index);
    bool HasSelection(const SectorEditorAudioAssetPickerState& state) const;
    std::string SelectedPath(const SectorEditorAudioAssetPickerState& state) const;
    void SetPreviewType(
            SectorEditorAudioAssetPickerState& state,
            SectorSoundType type);
    void PreviewSelection(SectorEditorAudioAssetPickerState& state);
    void UpdatePreview(SectorEditorAudioAssetPickerState& state);

    bool DrawList(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::FontHandle font,
            const char* id,
            Rectangle bounds,
            SectorEditorAudioAssetPickerState& state);
    SectorEditorAudioAssetPickerResult DrawModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::FontHandle font,
            SectorEditorAudioAssetPickerState& state);

private:
    void StopPreview(SectorEditorAudioPreviewState& preview);

    engine::EngineContext& context_;
    SectorEditorAudioAssetPickerSessionState& session_;
};

} // namespace game
