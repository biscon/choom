#include "sector_editor/services/sounds/SectorEditorSoundService.h"

#include "engine/input/InputEvents.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"
#include "sector_editor/services/sound_emitters/SectorEditorSoundEmitterEditingService.h"

#include <algorithm>
#include <utility>

namespace game {
namespace {

float ScrollContentWidth(float width, const engine::UIConfig& config)
{
    const float client = std::max(0.0f, width - config.borderThickness * 2.0f);
    return std::max(
            0.0f,
            client - config.scrollbarSize - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

const char* SoundTypeLabel(SectorSoundType type)
{
    return type == SectorSoundType::Music ? "Music (streaming)" : "Sound";
}

} // namespace

SectorEditorSoundService::SectorEditorSoundService(SectorEditorSoundServiceContext context)
    : context_(context)
{
}

const SectorSoundDefinition* SectorEditorSoundService::Find(const std::string& id) const
{
    const auto found = context_.authoringGraph.audioSettings.soundsById.find(id);
    return found == context_.authoringGraph.audioSettings.soundsById.end()
            ? nullptr
            : &found->second;
}

std::vector<std::string> SectorEditorSoundService::SortedIds(SectorSoundType type) const
{
    std::vector<std::string> ids;
    ids.reserve(context_.authoringGraph.audioSettings.soundsById.size());
    for (const auto& entry : context_.authoringGraph.audioSettings.soundsById) {
        if (entry.second.type == type) ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

engine::SoundHandle SectorEditorSoundService::SoundHandleForId(const std::string& id) const
{
    const auto found = context_.catalog.soundsById.find(id);
    return found == context_.catalog.soundsById.end()
            ? engine::NullSoundHandle()
            : found->second;
}

engine::MusicHandle SectorEditorSoundService::MusicHandleForId(const std::string& id) const
{
    const auto found = context_.catalog.musicById.find(id);
    return found == context_.catalog.musicById.end()
            ? engine::NullMusicHandle()
            : found->second;
}

void SectorEditorSoundService::RefreshCatalogHandles()
{
    if (!engine::IsNull(context_.catalog.scope)) {
        context_.engineContext.assets.UnloadScope(context_.catalog.scope);
    }
    context_.catalog = SectorEditorSoundCatalogState{};
    if (context_.authoringGraph.audioSettings.soundsById.empty()) return;

    context_.catalog.scope = context_.engineContext.assets.CreateScope(
            "sector_editor_map_sounds");
    if (engine::IsNull(context_.catalog.scope)) {
        context_.statusText = "Could not create editor map sound scope";
        return;
    }

    std::vector<std::string> ids;
    ids.reserve(context_.authoringGraph.audioSettings.soundsById.size());
    for (const auto& entry : context_.authoringGraph.audioSettings.soundsById) {
        ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    context_.catalog.soundsById.reserve(ids.size());
    context_.catalog.musicById.reserve(ids.size());
    for (const std::string& id : ids) {
        const SectorSoundDefinition& definition =
                context_.authoringGraph.audioSettings.soundsById.at(id);
        const std::string path = ResolveSectorAudioAssetPath(definition.path);
        if (definition.type == SectorSoundType::Music) {
            const engine::MusicHandle handle = context_.engineContext.assets.RequestMusic(
                    context_.catalog.scope,
                    path.c_str());
            if (!engine::IsNull(handle)) context_.catalog.musicById.emplace(id, handle);
        } else {
            const engine::SoundHandle handle = context_.engineContext.assets.RequestSound(
                    context_.catalog.scope,
                    path.c_str());
            if (!engine::IsNull(handle)) context_.catalog.soundsById.emplace(id, handle);
        }
    }
}

void SectorEditorSoundService::StopPreview(
        SectorEditorAudioPreviewState& preview,
        bool unloadScope)
{
    if (!engine::IsNull(preview.soundPlayback)) {
        context_.engineContext.audio.StopSound(
                context_.engineContext.assets,
                preview.soundPlayback);
    }
    if (!engine::IsNull(preview.music)) {
        context_.engineContext.audio.StopMusic(
                context_.engineContext.assets,
                preview.music);
    }
    if (unloadScope && !engine::IsNull(preview.scope)) {
        context_.engineContext.assets.UnloadScope(preview.scope);
    }
    preview = SectorEditorAudioPreviewState{};
}

void SectorEditorSoundService::Shutdown()
{
    SectorEditorAudioAssetPickerService audioPicker{
            context_.engineContext, context_.audioAssetPickerSession};
    StopPreview(context_.state.soundPicker.preview, false);
    if (!engine::IsNull(context_.catalog.scope)) {
        context_.engineContext.assets.UnloadScope(context_.catalog.scope);
    }
    context_.catalog = SectorEditorSoundCatalogState{};
}

void SectorEditorSoundService::UpdatePreview(
        SectorEditorAudioPreviewState& preview,
        std::string& message)
{
    if (!preview.pending) return;
    if (preview.type == SectorSoundType::Music) {
        if (context_.engineContext.assets.HasFailed(preview.music)) {
            preview.pending = false;
            message = "Preview failed to load";
            return;
        }
        if (!context_.engineContext.assets.IsReady(preview.music)) return;
        engine::MusicPlaybackSettings settings;
        settings.looping = false;
        preview.pending = false;
        message = context_.engineContext.audio.PlayMusic(
                context_.engineContext.assets,
                preview.music,
                settings)
                ? "Previewing streaming music"
                : "Preview could not start";
        return;
    }
    if (context_.engineContext.assets.HasFailed(preview.sound)) {
        preview.pending = false;
        message = "Preview failed to load";
        return;
    }
    if (!context_.engineContext.assets.IsReady(preview.sound)) return;
    preview.soundPlayback = context_.engineContext.audio.PlaySound(
            context_.engineContext.assets,
            preview.sound);
    preview.pending = false;
    message = engine::IsNull(preview.soundPlayback)
            ? "Preview could not start"
            : "Previewing sound";
}

bool SectorEditorSoundService::OpenDoorPicker(
        int runtimeObjectId,
        SectorEditorDoorSoundTarget target)
{
    const SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
            context_.map,
            runtimeObjectId);
    if (object == nullptr || object->kind != "door") return false;
    ClosePicker();
    SoundPickerState& picker = context_.state.soundPicker;
    picker.open = true;
    picker.targetKind = target == SectorEditorDoorSoundTarget::Open
            ? SectorEditorSoundPickerTargetKind::DoorOpen
            : SectorEditorSoundPickerTargetKind::DoorClose;
    picker.targetId = runtimeObjectId;
    picker.soundIds.push_back({});
    picker.labelStorage.push_back("<None>");
    const std::vector<std::string> ids = SortedIds(SectorSoundType::Sound);
    picker.soundIds.insert(picker.soundIds.end(), ids.begin(), ids.end());
    picker.labelStorage.insert(picker.labelStorage.end(), ids.begin(), ids.end());
    const std::string& current = target == SectorEditorDoorSoundTarget::Open
            ? object->door.openSoundId
            : object->door.closeSoundId;
    picker.selectedSoundIndex = 0;
    picker.optionLabels.reserve(picker.labelStorage.size());
    for (size_t i = 0; i < picker.labelStorage.size(); ++i) {
        picker.optionLabels.push_back(picker.labelStorage[i].c_str());
        if (picker.soundIds[i] == current) {
            picker.selectedSoundIndex = static_cast<int>(i);
        }
    }
    return true;
}

bool SectorEditorSoundService::OpenSoundEmitterPicker(int emitterId)
{
    const SectorAuthoringSoundEmitter* emitter =
            FindSectorAuthoringSoundEmitter(context_.authoringGraph, emitterId);
    if (emitter == nullptr) return false;
    ClosePicker();
    SoundPickerState& picker = context_.state.soundPicker;
    picker.open = true;
    picker.targetKind = SectorEditorSoundPickerTargetKind::SoundEmitter;
    picker.targetId = emitterId;
    picker.soundIds.push_back({});
    picker.labelStorage.push_back("<None>");

    std::vector<std::string> ids;
    ids.reserve(context_.authoringGraph.audioSettings.soundsById.size());
    for (const auto& entry : context_.authoringGraph.audioSettings.soundsById) {
        ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    for (const std::string& id : ids) {
        const SectorSoundDefinition& definition =
                context_.authoringGraph.audioSettings.soundsById.at(id);
        picker.soundIds.push_back(id);
        picker.labelStorage.push_back(id + "  ["
                + (definition.type == SectorSoundType::Music ? "Music" : "Sound")
                + "]");
    }
    picker.selectedSoundIndex = 0;
    picker.optionLabels.reserve(picker.labelStorage.size());
    for (size_t index = 0; index < picker.labelStorage.size(); ++index) {
        picker.optionLabels.push_back(picker.labelStorage[index].c_str());
        if (picker.soundIds[index] == emitter->soundId) {
            picker.selectedSoundIndex = static_cast<int>(index);
        }
    }
    return true;
}

void SectorEditorSoundService::ClosePicker()
{
    StopPreview(context_.state.soundPicker.preview, false);
    context_.state.soundPicker = SoundPickerState{};
}

bool SectorEditorSoundService::ApplyPickerSelection()
{
    SoundPickerState& picker = context_.state.soundPicker;
    if (!picker.open
            || picker.selectedSoundIndex < 0
            || picker.selectedSoundIndex >= static_cast<int>(picker.soundIds.size())) {
        ClosePicker();
        return false;
    }
    const std::string selected = picker.soundIds[static_cast<size_t>(picker.selectedSoundIndex)];
    const int targetId = picker.targetId;
    const SectorEditorSoundPickerTargetKind targetKind = picker.targetKind;
    ClosePicker();

    if (targetKind == SectorEditorSoundPickerTargetKind::SoundEmitter) {
        if (context_.soundEmitterEditing == nullptr
                || context_.soundEmitterEditing->Selected() == nullptr
                || context_.soundEmitterEditing->Selected()->id != targetId) {
            context_.statusText = "Sound Emitter target unavailable";
            return false;
        }
        const bool changed = context_.soundEmitterEditing->Selected()->soundId != selected
                && context_.soundEmitterEditing->SetSelectedSoundId(selected);
        context_.statusText = changed
                ? "Selected Sound Emitter sound "
                        + (selected.empty() ? std::string{"<none>"} : selected)
                : "Sound Emitter sound unchanged";
        return changed;
    }

    if (context_.runtimeObjectEditing == nullptr) return false;
    const SectorPlacedRuntimeObject* object = context_.runtimeObjectEditing->SelectedObject();
    if (object == nullptr || object->id != targetId || object->kind != "door") {
        context_.statusText = "Door sound target unavailable";
        return false;
    }
    const bool opening = targetKind == SectorEditorSoundPickerTargetKind::DoorOpen;
    const bool changed = context_.runtimeObjectEditing->MutateSelected(
            opening
                    ? "Updated door open sound"
                    : "Updated door close sound",
            [selected, opening](SectorPlacedRuntimeObject& edited) {
                if (edited.kind != "door") return false;
                std::string& field = opening
                        ? edited.door.openSoundId
                        : edited.door.closeSoundId;
                if (field == selected) return false;
                field = selected;
                return true;
            });
    context_.statusText = changed
            ? TextFormat("Selected door %s sound %s",
                    opening ? "open" : "close",
                    selected.empty() ? "<none>" : selected.c_str())
            : "Door sound unchanged";
    return changed;
}

void SectorEditorSoundService::PreviewPickerSelection()
{
    SoundPickerState& picker = context_.state.soundPicker;
    if (picker.selectedSoundIndex <= 0
            || picker.selectedSoundIndex >= static_cast<int>(picker.soundIds.size())) {
        picker.message = "<None> has no preview";
        return;
    }
    StopPreview(picker.preview, false);
    const std::string& id = picker.soundIds[static_cast<size_t>(picker.selectedSoundIndex)];
    const SectorSoundDefinition* definition = Find(id);
    if (definition == nullptr) {
        picker.message = "Sound is unavailable";
        return;
    }
    picker.preview.type = definition->type;
    picker.preview.key = id;
    if (definition->type == SectorSoundType::Music) {
        picker.preview.music = MusicHandleForId(id);
        picker.preview.pending = !engine::IsNull(picker.preview.music);
    } else {
        picker.preview.sound = SoundHandleForId(id);
        picker.preview.pending = !engine::IsNull(picker.preview.sound);
    }
    picker.message = picker.preview.pending
            ? "Loading preview..."
            : "Sound is unavailable";
}

void SectorEditorSoundService::DrawPickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::FontHandle font)
{
    SoundPickerState& picker = context_.state.soundPicker;
    if (!picker.open) return;
    input.ForEachEvent(engine::InputEventType::KeyPressed, true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    ClosePicker();
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    ApplyPickerSelection();
                    engine::ConsumeEvent(event);
                }
            });
    if (!picker.open) return;
    UpdatePreview(picker.preview, picker.message);

    engine::AssetManager& assets = context_.engineContext.assets;
    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{(EditorWidth - 720.0f) * 0.5f, (EditorHeight - 610.0f) * 0.5f, 720.0f, 610.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets, Rectangle{modal.x + 22.0f, modal.y + 18.0f, modal.width - 44.0f, 36.0f},
            font,
            picker.targetKind == SectorEditorSoundPickerTargetKind::SoundEmitter
                    ? "Pick Sound Emitter Audio"
                    : picker.targetKind == SectorEditorSoundPickerTargetKind::DoorOpen
                            ? "Pick Door Open Sound" : "Pick Door Close Sound");
    const Rectangle listBounds{modal.x + 22.0f, modal.y + 68.0f, 330.0f, 400.0f};
    const Vector2 contentSize{
            ScrollContentWidth(listBounds.width, config),
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(picker.optionLabels.size()))};
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui, config, input, "sector_editor_sound_picker_scroll",
            listBounds, contentSize, picker.scroll);
    if (!picker.optionLabels.empty()) {
        const int oldSelection = picker.selectedSoundIndex;
        engine::List(ui, config, input, assets, "sector_editor_sound_picker_list",
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y}, font,
                picker.optionLabels.data(), picker.optionLabels.size(), picker.selectedSoundIndex);
        if (oldSelection != picker.selectedSoundIndex) {
            StopPreview(picker.preview, false);
            picker.message.clear();
        }
    }
    engine::EndScrollArea(ui, config, input, scroll, picker.scroll);

    const float rightX = modal.x + 378.0f;
    std::string selectedId;
    if (picker.selectedSoundIndex >= 0
            && picker.selectedSoundIndex < static_cast<int>(picker.soundIds.size())) {
        selectedId = picker.soundIds[static_cast<size_t>(picker.selectedSoundIndex)];
    }
    const SectorSoundDefinition* definition = Find(selectedId);
    engine::Text(config, assets, Rectangle{rightX, modal.y + 84.0f, 320.0f, 38.0f}, font,
            TextFormat("ID: %s", selectedId.empty() ? "<None>" : selectedId.c_str()),
            engine::UITextJustify::Left, config.textColor);
    engine::Text(config, assets, Rectangle{rightX, modal.y + 132.0f, 320.0f, 100.0f}, font,
            TextFormat("Path: %s", definition == nullptr ? "<none>" : definition->path.c_str()),
            engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(config, assets, Rectangle{rightX, modal.y + 244.0f, 320.0f, 38.0f}, font,
            definition == nullptr ? "" : SoundTypeLabel(definition->type),
            engine::UITextJustify::Left, config.mutedTextColor);
    if (engine::Button(ui, config, input, assets, "sector_editor_sound_picker_preview",
            Rectangle{rightX, modal.y + 304.0f, 150.0f, 44.0f}, font, "Preview")) {
        PreviewPickerSelection();
    }
    engine::Text(config, assets, Rectangle{rightX, modal.y + 360.0f, 320.0f, 64.0f}, font,
            picker.message.c_str(), engine::UITextJustify::Left, config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 64.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_sound_picker_select",
            Rectangle{modal.x + modal.width - 334.0f, buttonY, 150.0f, 44.0f}, font, "Select")) {
        ApplyPickerSelection();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_sound_picker_cancel",
            Rectangle{modal.x + modal.width - 172.0f, buttonY, 150.0f, 44.0f}, font, "Cancel")) {
        ClosePicker();
    }
    input.ForEachEvent(engine::InputEventType::Any, true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

} // namespace game
