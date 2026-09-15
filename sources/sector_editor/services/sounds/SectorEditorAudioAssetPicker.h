#pragma once

#include "engine/EngineContext.h"
#include "engine/ui/UI.h"
#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
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

// Shared across audio picker callers for the app session; never serialized.
struct SectorEditorAudioAssetPickerSessionState {
    char filterBuffer[256] = {};
    engine::UIScrollState scroll;
    std::string selectedPath;
};

struct SectorEditorAudioAssetPickerState {
    bool open = false;
    bool scanned = false;
    std::string title;
    std::string scanMessage;
    std::string filterMessage;
    SectorEditorAudioAssetPickerSessionState browsing;
    std::vector<std::string> allPaths;
    std::vector<std::string> paths;
    std::vector<const char*> optionLabels;
    int selectedPathIndex = -1;
    SectorSoundType previewType = SectorSoundType::Sound;
    std::string previewMessage;
    SectorEditorAudioPreviewState preview;
};

inline void RestoreSectorEditorAudioAssetPickerSession(
        SectorEditorAudioAssetPickerState& state,
        const SectorEditorAudioAssetPickerSessionState& session)
{
    state.browsing = session;
}

inline void RememberSectorEditorAudioAssetPickerSession(
        SectorEditorAudioAssetPickerState& state,
        SectorEditorAudioAssetPickerSessionState& session)
{
    if (!state.open && !state.scanned) return;
    if (state.selectedPathIndex >= 0
            && state.selectedPathIndex < static_cast<int>(state.paths.size())) {
        state.browsing.selectedPath = state.paths[static_cast<size_t>(state.selectedPathIndex)];
    }
    session = state.browsing;
}

// Returns whether the selected file changed, so the service can stop its preview.
inline bool RebuildSectorEditorAudioAssetPickerOptions(
        SectorEditorAudioAssetPickerState& state,
        const std::string& currentPath = {})
{
    const std::string oldPath = state.selectedPathIndex >= 0
                    && state.selectedPathIndex < static_cast<int>(state.paths.size())
            ? state.paths[static_cast<size_t>(state.selectedPathIndex)] : std::string{};
    if (!oldPath.empty()) state.browsing.selectedPath = oldPath;
    const std::string_view filter = state.browsing.filterBuffer;
    state.optionLabels.clear();
    state.paths.clear();
    state.paths.reserve(state.allPaths.size());
    for (const std::string& path : state.allPaths) {
        if (filter.empty() || std::search(
                    path.begin(), path.end(), filter.begin(), filter.end(),
                    [](char lhs, char rhs) {
                        return std::tolower(static_cast<unsigned char>(lhs))
                                == std::tolower(static_cast<unsigned char>(rhs));
                    }) != path.end()) {
            state.paths.push_back(path);
        }
    }
    state.optionLabels.reserve(state.paths.size());
    for (const std::string& path : state.paths) {
        state.optionLabels.push_back(path.c_str());
    }
    auto selected = std::find(state.paths.begin(), state.paths.end(), state.browsing.selectedPath);
    if (selected == state.paths.end()) {
        selected = std::find(state.paths.begin(), state.paths.end(), currentPath);
    }
    state.selectedPathIndex = selected != state.paths.end()
            ? static_cast<int>(selected - state.paths.begin())
            : (state.paths.empty() ? -1 : 0);
    if (state.selectedPathIndex >= 0) {
        state.browsing.selectedPath = state.paths[static_cast<size_t>(state.selectedPathIndex)];
    }
    state.filterMessage = !state.allPaths.empty() && state.paths.empty()
            ? "No audio files match the filter" : std::string{};
    return oldPath != (state.selectedPathIndex >= 0
            ? state.browsing.selectedPath : std::string{});
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
    void ApplyFilter(SectorEditorAudioAssetPickerState& state);
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
