#include "sector_editor/services/sounds/SectorEditorAudioAssetPicker.h"

#include "engine/input/InputEvents.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>

namespace game {
namespace {

float ScrollContentWidth(float width, const engine::UIConfig& config)
{
    const float client = std::max(0.0f, width - config.borderThickness * 2.0f);
    return std::max(
            0.0f,
            client - config.scrollbarSize
                    - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

void ConsumeRemainingInput(engine::Input& input)
{
    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

} // namespace

SectorEditorAudioAssetPickerService::SectorEditorAudioAssetPickerService(
        engine::EngineContext& context,
        SectorEditorAudioAssetPickerSessionState& session)
    : context_(context)
    , session_(session)
{
}

void SectorEditorAudioAssetPickerService::Open(
        SectorEditorAudioAssetPickerState& state,
        const std::string& title,
        const std::string& currentPath,
        SectorSoundType previewType)
{
    Close(state);
    state.open = true;
    state.title = title;
    state.previewType = previewType;
    RestoreSectorEditorAudioAssetPickerScroll(state, session_);
    state.paths = ScanAssetAudioFiles(state.scanMessage);
    state.optionLabels.reserve(state.paths.size());
    for (const std::string& path : state.paths) {
        state.optionLabels.push_back(path.c_str());
    }
    state.scanned = true;
    const auto selected = std::lower_bound(
            state.paths.begin(), state.paths.end(), currentPath);
    state.selectedPathIndex = selected != state.paths.end()
                    && *selected == currentPath
            ? static_cast<int>(std::distance(state.paths.begin(), selected))
            : (state.paths.empty() ? -1 : 0);
}

void SectorEditorAudioAssetPickerService::Close(
        SectorEditorAudioAssetPickerState& state)
{
    if (state.open || state.scanned) {
        RememberSectorEditorAudioAssetPickerScroll(state, session_);
    }
    StopPreview(state.preview);
    state = SectorEditorAudioAssetPickerState{};
}

bool SectorEditorAudioAssetPickerService::SelectIndex(
        SectorEditorAudioAssetPickerState& state,
        int index)
{
    if (index < 0 || index >= static_cast<int>(state.paths.size())) {
        state.selectedPathIndex = -1;
        return false;
    }
    if (state.selectedPathIndex != index) {
        StopPreview(state.preview);
        state.previewMessage.clear();
        state.selectedPathIndex = index;
    }
    return true;
}

bool SectorEditorAudioAssetPickerService::HasSelection(
        const SectorEditorAudioAssetPickerState& state) const
{
    return state.selectedPathIndex >= 0
            && state.selectedPathIndex < static_cast<int>(state.paths.size());
}

std::string SectorEditorAudioAssetPickerService::SelectedPath(
        const SectorEditorAudioAssetPickerState& state) const
{
    return HasSelection(state)
            ? state.paths[static_cast<size_t>(state.selectedPathIndex)]
            : std::string{};
}

void SectorEditorAudioAssetPickerService::SetPreviewType(
        SectorEditorAudioAssetPickerState& state,
        SectorSoundType type)
{
    if (state.previewType == type) return;
    StopPreview(state.preview);
    state.previewMessage.clear();
    state.previewType = type;
}

void SectorEditorAudioAssetPickerService::StopPreview(
        SectorEditorAudioPreviewState& preview)
{
    if (!engine::IsNull(preview.soundPlayback)) {
        context_.audio.StopSound(context_.assets, preview.soundPlayback);
    }
    if (!engine::IsNull(preview.music)) {
        context_.audio.StopMusic(context_.assets, preview.music);
    }
    if (!engine::IsNull(preview.scope)) {
        context_.assets.UnloadScope(preview.scope);
    }
    preview = SectorEditorAudioPreviewState{};
}

void SectorEditorAudioAssetPickerService::PreviewSelection(
        SectorEditorAudioAssetPickerState& state)
{
    if (!HasSelection(state)) {
        state.previewMessage = "Select an audio file";
        return;
    }
    StopPreview(state.preview);
    state.preview.scope = context_.assets.CreateScope(
            "sector_editor_audio_asset_preview");
    if (engine::IsNull(state.preview.scope)) {
        state.previewMessage = "Could not create preview scope";
        return;
    }
    state.preview.type = state.previewType;
    state.preview.key = SelectedPath(state);
    const std::string path = ResolveSectorAudioAssetPath(state.preview.key);
    if (state.previewType == SectorSoundType::Music) {
        state.preview.music = context_.assets.RequestMusic(
                state.preview.scope, path.c_str());
        state.preview.pending = !engine::IsNull(state.preview.music);
    } else {
        state.preview.sound = context_.assets.RequestSound(
                state.preview.scope, path.c_str());
        state.preview.pending = !engine::IsNull(state.preview.sound);
    }
    state.previewMessage = state.preview.pending
            ? "Loading preview..."
            : "Preview request failed";
}

void SectorEditorAudioAssetPickerService::UpdatePreview(
        SectorEditorAudioAssetPickerState& state)
{
    SectorEditorAudioPreviewState& preview = state.preview;
    if (!preview.pending) return;
    if (preview.type == SectorSoundType::Music) {
        if (context_.assets.HasFailed(preview.music)) {
            preview.pending = false;
            state.previewMessage = "Preview failed to load";
            return;
        }
        if (!context_.assets.IsReady(preview.music)) return;
        engine::MusicPlaybackSettings settings;
        settings.looping = false;
        preview.pending = false;
        state.previewMessage = context_.audio.PlayMusic(
                context_.assets, preview.music, settings)
                ? "Previewing streaming music"
                : "Preview could not start";
        return;
    }
    if (context_.assets.HasFailed(preview.sound)) {
        preview.pending = false;
        state.previewMessage = "Preview failed to load";
        return;
    }
    if (!context_.assets.IsReady(preview.sound)) return;
    preview.soundPlayback = context_.audio.PlaySound(
            context_.assets, preview.sound);
    preview.pending = false;
    state.previewMessage = engine::IsNull(preview.soundPlayback)
            ? "Preview could not start"
            : "Previewing sound";
}

bool SectorEditorAudioAssetPickerService::DrawList(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::FontHandle font,
        const char* id,
        Rectangle bounds,
        SectorEditorAudioAssetPickerState& state)
{
    const Vector2 contentSize{
            ScrollContentWidth(bounds.width, config),
            std::max(
                    bounds.height,
                    config.listItemHeight
                            * static_cast<float>(state.optionLabels.size()))};
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui, config, input, id, bounds, contentSize, state.scroll);
    const int oldSelection = state.selectedPathIndex;
    if (!state.optionLabels.empty()) {
        engine::List(
                ui, config, input, context_.assets,
                TextFormat("%s_list", id),
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y},
                font,
                state.optionLabels.data(),
                state.optionLabels.size(),
                state.selectedPathIndex);
    }
    engine::EndScrollArea(ui, config, input, scroll, state.scroll);
    RememberSectorEditorAudioAssetPickerScroll(state, session_);
    if (oldSelection != state.selectedPathIndex) {
        StopPreview(state.preview);
        state.previewMessage.clear();
        return true;
    }
    return false;
}

SectorEditorAudioAssetPickerResult
SectorEditorAudioAssetPickerService::DrawModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::FontHandle font,
        SectorEditorAudioAssetPickerState& state)
{
    if (!state.open) return SectorEditorAudioAssetPickerResult::None;
    bool cancelRequested = false;
    bool selectRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested, &selectRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) cancelRequested = true;
                else if (event.key.key == KEY_ENTER
                        || event.key.key == KEY_KP_ENTER) {
                    selectRequested = true;
                } else {
                    return;
                }
                engine::ConsumeEvent(event);
            });
    UpdatePreview(state);

    DrawRectangle(
            0, 0,
            static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight),
            Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 860.0f) * 0.5f,
            (EditorHeight - 650.0f) * 0.5f,
            860.0f,
            650.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(
            config, context_.assets,
            Rectangle{modal.x + 22.0f, modal.y + 18.0f, modal.width - 44.0f, 36.0f},
            font,
            state.title.empty() ? "Pick Sound" : state.title.c_str());

    const Rectangle listBounds{
            modal.x + 22.0f, modal.y + 68.0f, 510.0f, 480.0f};
    DrawList(
            ui, config, input, font,
            "sector_editor_audio_asset_picker_scroll",
            listBounds, state);
    const float rightX = modal.x + 558.0f;
    const std::string selectedPath = SelectedPath(state);
    engine::Text(
            config, context_.assets,
            Rectangle{rightX, modal.y + 82.0f, 278.0f, 128.0f},
            font,
            TextFormat("Path: %s", selectedPath.empty()
                    ? "<none>" : selectedPath.c_str()),
            engine::UITextJustify::Left,
            config.mutedTextColor,
            true);
    if (engine::Button(
                ui, config, input, context_.assets,
                "sector_editor_audio_asset_picker_preview",
                Rectangle{rightX, modal.y + 244.0f, 150.0f, 42.0f},
                font, "Preview")) {
        PreviewSelection(state);
    }
    engine::Text(
            config, context_.assets,
            Rectangle{rightX, modal.y + 302.0f, 278.0f, 80.0f},
            font, state.previewMessage.c_str(),
            engine::UITextJustify::Left,
            config.mutedTextColor,
            true);
    if (!state.scanMessage.empty()) {
        engine::Text(
                config, context_.assets,
                Rectangle{modal.x + 22.0f, modal.y + 556.0f, 510.0f, 44.0f},
                font, state.scanMessage.c_str(),
                engine::UITextJustify::Left,
                state.paths.empty() ? config.invalidColor : config.mutedTextColor,
                true);
    }

    const float buttonY = modal.y + modal.height - 64.0f;
    if (engine::Button(
                ui, config, input, context_.assets,
                "sector_editor_audio_asset_picker_select",
                Rectangle{modal.x + modal.width - 334.0f, buttonY, 150.0f, 44.0f},
                font, "Select")) {
        selectRequested = true;
    }
    if (engine::Button(
                ui, config, input, context_.assets,
                "sector_editor_audio_asset_picker_cancel",
                Rectangle{modal.x + modal.width - 172.0f, buttonY, 150.0f, 44.0f},
                font, "Cancel")) {
        cancelRequested = true;
    }

    SectorEditorAudioAssetPickerResult result =
            SectorEditorAudioAssetPickerResult::None;
    if (cancelRequested) {
        result = SectorEditorAudioAssetPickerResult::Cancelled;
    } else if (selectRequested && HasSelection(state)) {
        result = SectorEditorAudioAssetPickerResult::Selected;
    } else if (selectRequested) {
        state.previewMessage = "Select an audio file";
    }
    ConsumeRemainingInput(input);
    return result;
}

} // namespace game
