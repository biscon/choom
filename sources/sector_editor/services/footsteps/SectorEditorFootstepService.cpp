#include "sector_editor/services/footsteps/SectorEditorFootstepService.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorAssetPaths.h"

#include <algorithm>

namespace game {

namespace {

float ScrollContentWidth(float width, const engine::UIConfig& config)
{
    const float client = std::max(0.0f, width - config.borderThickness * 2.0f);
    return std::max(
            0.0f,
            client - config.scrollbarSize - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

} // namespace

bool SectorEditorFootstepService::OpenForAuthoringFaceAnchor(int faceAnchorId)
{
    const SectorAuthoringFaceAnchor* anchor = FindSectorAuthoringFaceAnchor(
            context_.authoringGraph,
            faceAnchorId);
    if (anchor == nullptr) {
        context_.statusText = "Footstep picker unavailable: authoring face was not found";
        return false;
    }

    Close();
    FootstepPickerState& picker = context_.editorState.footstepPicker;
    picker.open = true;
    picker.authoringFaceAnchorId = faceAnchorId;
    picker.catalog = DiscoverFootstepCatalog(ASSETS_PATH "audio/footsteps");
    picker.message = picker.catalog.warning;
    picker.setIds.reserve(picker.catalog.sets.size() + 1u);
    picker.labelStorage.reserve(picker.catalog.sets.size() + 1u);
    picker.setIds.emplace_back();
    picker.labelStorage.push_back(
            "Default (" + context_.applicationSettings.footsteps.defaultSet + ")");
    for (const FootstepCatalogSet& set : picker.catalog.sets) {
        picker.setIds.push_back(set.id);
        picker.labelStorage.push_back(set.id);
    }
    picker.optionLabels.reserve(picker.labelStorage.size());
    picker.selectedSetIndex = 0;
    for (size_t i = 0; i < picker.labelStorage.size(); ++i) {
        picker.optionLabels.push_back(picker.labelStorage[i].c_str());
        if (picker.setIds[i] == anchor->footstepSet) {
            picker.selectedSetIndex = static_cast<int>(i);
        }
    }
    if (!anchor->footstepSet.empty()
            && FindFootstepCatalogSet(picker.catalog, anchor->footstepSet) == nullptr) {
        picker.message = "Assigned set is missing: " + anchor->footstepSet;
    }
    return true;
}

std::string SectorEditorFootstepService::EffectiveSetId(
        std::string_view overrideId) const
{
    return overrideId.empty()
            ? context_.applicationSettings.footsteps.defaultSet
            : std::string{overrideId};
}

void SectorEditorFootstepService::ClosePreview()
{
    FootstepPickerState& picker = context_.editorState.footstepPicker;
    for (const engine::SoundHandle sound : picker.previewSet.sounds) {
        context_.engineContext.audio.StopSoundAsset(
                context_.engineContext.assets,
                sound);
    }
    if (!engine::IsNull(picker.previewScope)) {
        context_.engineContext.assets.UnloadScope(picker.previewScope);
    }
    picker.previewScope = engine::NullAssetScopeHandle();
    picker.previewSet = LoadedFootstepSet{};
    picker.previewPlayback = FootstepPlaybackState{};
    picker.previewPending = false;
}

void SectorEditorFootstepService::Close()
{
    ClosePreview();
    context_.editorState.footstepPicker = FootstepPickerState{};
}

std::string SectorEditorFootstepService::SelectedSetId() const
{
    const FootstepPickerState& picker = context_.editorState.footstepPicker;
    return picker.selectedSetIndex >= 0
            && picker.selectedSetIndex < static_cast<int>(picker.setIds.size())
            ? picker.setIds[static_cast<size_t>(picker.selectedSetIndex)]
            : std::string{};
}

std::string SectorEditorFootstepService::EffectiveSelectedSetId() const
{
    const std::string selected = SelectedSetId();
    return selected.empty()
            ? context_.applicationSettings.footsteps.defaultSet
            : selected;
}

bool SectorEditorFootstepService::ApplySelection()
{
    FootstepPickerState& picker = context_.editorState.footstepPicker;
    if (!picker.open || picker.authoringFaceAnchorId <= 0) return false;
    const std::string selected = SelectedSetId();
    const bool changed = MutateSectorEditorAuthoringFaceAnchorById(
            context_.editorState,
            context_.lifecycle,
            context_.topologyMap,
            context_.authoringGraph,
            context_.derivation,
            picker.authoringFaceAnchorId,
            "Updated authoring face footstep set",
            [&selected](SectorAuthoringFaceAnchor& anchor) {
                if (anchor.footstepSet == selected) return false;
                anchor.footstepSet = selected;
                return true;
            });
    if (!changed) context_.statusText = "Footstep assignment unchanged";
    Close();
    return changed;
}

void SectorEditorFootstepService::PreviewSelection()
{
    FootstepPickerState& picker = context_.editorState.footstepPicker;
    if (!picker.open) return;
    const std::string id = EffectiveSelectedSetId();
    const FootstepCatalogSet* set = FindFootstepCatalogSet(picker.catalog, id);
    if (set == nullptr) {
        picker.message = "Footstep set is unavailable: " + id;
        return;
    }

    if (!picker.previewPending
            && picker.previewSet.id == id
            && !picker.previewSet.sounds.empty()
            && std::all_of(
                    picker.previewSet.sounds.begin(),
                    picker.previewSet.sounds.end(),
                    [this](engine::SoundHandle sound) {
                        return context_.engineContext.assets.IsReady(sound);
                    })) {
        PlayFootstep(
                context_.engineContext.assets,
                context_.engineContext.audio,
                picker.previewSet,
                picker.previewPlayback,
                context_.applicationSettings.footsteps.volume);
        picker.message = "Previewing " + picker.previewSet.id;
        return;
    }

    ClosePreview();
    picker.previewScope = context_.engineContext.assets.CreateScope(
            "sector_editor_footstep_preview");
    if (engine::IsNull(picker.previewScope)) {
        picker.message = "Could not create footstep preview asset scope";
        return;
    }
    picker.previewSet.id = set->id;
    picker.previewSet.sounds.reserve(set->relativePaths.size());
    for (const std::string& relativePath : set->relativePaths) {
        const std::string path = ResolveSectorAudioAssetPath(relativePath);
        const engine::SoundHandle sound = context_.engineContext.assets.RequestSound(
                picker.previewScope,
                path.c_str());
        if (!engine::IsNull(sound)) picker.previewSet.sounds.push_back(sound);
    }
    ReserveFootstepPlaybackState(
            picker.previewPlayback,
            picker.previewSet.sounds.size(),
            picker.previewSet.id.size());
    picker.previewPending = !picker.previewSet.sounds.empty();
    picker.message = picker.previewPending
            ? "Loading preview..."
            : "No variants could be requested for " + id;
}

void SectorEditorFootstepService::UpdatePreview()
{
    FootstepPickerState& picker = context_.editorState.footstepPicker;
    if (!picker.open || !picker.previewPending) return;
    const bool finished = std::all_of(
            picker.previewSet.sounds.begin(),
            picker.previewSet.sounds.end(),
            [this](engine::SoundHandle sound) {
                return context_.engineContext.assets.IsFinished(sound);
            });
    if (!finished) return;
    picker.previewSet.sounds.erase(
            std::remove_if(
                    picker.previewSet.sounds.begin(),
                    picker.previewSet.sounds.end(),
                    [this](engine::SoundHandle sound) {
                        return !context_.engineContext.assets.IsReady(sound);
                    }),
            picker.previewSet.sounds.end());
    picker.previewPending = false;
    if (picker.previewSet.sounds.empty()) {
        picker.message = "All variants failed to load";
        return;
    }
    PlayFootstep(
            context_.engineContext.assets,
            context_.engineContext.audio,
            picker.previewSet,
            picker.previewPlayback,
            context_.applicationSettings.footsteps.volume);
    picker.message = "Previewing " + picker.previewSet.id;
}

void SectorEditorFootstepService::DrawModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    FootstepPickerState& picker = context_.editorState.footstepPicker;
    if (!picker.open) return;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    Close();
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    ApplySelection();
                    engine::ConsumeEvent(event);
                }
            });
    if (!picker.open) return;

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 660.0f) * 0.5f,
            (EditorHeight - 610.0f) * 0.5f,
            660.0f,
            610.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + 22.0f, modal.y + 18.0f, modal.width - 44.0f, 36.0f},
            font,
            "Pick Footstep Set");

    const Rectangle listBounds{modal.x + 22.0f, modal.y + 68.0f, modal.width - 44.0f, 400.0f};
    const float contentWidth = ScrollContentWidth(listBounds.width, config);
    const Vector2 contentSize{
            contentWidth,
            std::max(
                    listBounds.height,
                    config.listItemHeight * static_cast<float>(picker.optionLabels.size()))};
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_footstep_picker_scroll",
            listBounds,
            contentSize,
            picker.scroll);
    if (!picker.optionLabels.empty()) {
        const int previousSelection = picker.selectedSetIndex;
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_footstep_picker_list",
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y},
                font,
                picker.optionLabels.data(),
                picker.optionLabels.size(),
                picker.selectedSetIndex);
        if (picker.selectedSetIndex != previousSelection) {
            ClosePreview();
            picker.message.clear();
        }
    }
    engine::EndScrollArea(ui, config, input, scroll, picker.scroll);

    engine::Text(
            config,
            assets,
            Rectangle{modal.x + 22.0f, modal.y + 478.0f, modal.width - 44.0f, 44.0f},
            font,
            picker.message.c_str(),
            engine::UITextJustify::Left,
            config.mutedTextColor);
    const float buttonY = modal.y + modal.height - 64.0f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_footstep_preview",
                Rectangle{modal.x + 22.0f, buttonY, 150.0f, 44.0f},
                font,
                "Preview")) {
        PreviewSelection();
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_footstep_select",
                Rectangle{modal.x + modal.width - 322.0f, buttonY, 140.0f, 44.0f},
                font,
                "Select")) {
        ApplySelection();
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_footstep_cancel",
                Rectangle{modal.x + modal.width - 172.0f, buttonY, 150.0f, 44.0f},
                font,
                "Cancel")) {
        Close();
    }
    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

} // namespace game
