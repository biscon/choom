#include "sector_editor/services/sounds/SectorEditorSoundService.h"

#include "engine/input/InputEvents.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"

#include <algorithm>
#include <cstdio>
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
    const auto found = context_.map.audioSettings.soundsById.find(id);
    return found == context_.map.audioSettings.soundsById.end()
            ? nullptr
            : &found->second;
}

std::vector<std::string> SectorEditorSoundService::SortedIds(SectorSoundType type) const
{
    std::vector<std::string> ids;
    ids.reserve(context_.map.audioSettings.soundsById.size());
    for (const auto& entry : context_.map.audioSettings.soundsById) {
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
    if (context_.map.audioSettings.soundsById.empty()) return;

    context_.catalog.scope = context_.engineContext.assets.CreateScope(
            "sector_editor_map_sounds");
    if (engine::IsNull(context_.catalog.scope)) {
        context_.statusText = "Could not create editor map sound scope";
        return;
    }

    std::vector<std::string> ids;
    ids.reserve(context_.map.audioSettings.soundsById.size());
    for (const auto& entry : context_.map.audioSettings.soundsById) {
        ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    context_.catalog.soundsById.reserve(ids.size());
    context_.catalog.musicById.reserve(ids.size());
    for (const std::string& id : ids) {
        const SectorSoundDefinition& definition = context_.map.audioSettings.soundsById.at(id);
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
    StopPreview(context_.state.addMapSound.preview, true);
    StopPreview(context_.state.soundPicker.preview, false);
    if (!engine::IsNull(context_.catalog.scope)) {
        context_.engineContext.assets.UnloadScope(context_.catalog.scope);
    }
    context_.catalog = SectorEditorSoundCatalogState{};
}

void SectorEditorSoundService::SelectAddPath(int pathIndex)
{
    AddMapSoundState& modal = context_.state.addMapSound;
    if (pathIndex < 0 || pathIndex >= static_cast<int>(modal.paths.size())) {
        modal.selectedPathIndex = -1;
        modal.soundIdBuffer[0] = '\0';
        return;
    }
    modal.selectedPathIndex = pathIndex;
    const std::string base = GeneratedSoundIdBase(modal.paths[static_cast<size_t>(pathIndex)]);
    std::string id = base;
    int suffix = 1;
    while (Find(id) != nullptr) {
        char suffixBuffer[16] = {};
        std::snprintf(suffixBuffer, sizeof(suffixBuffer), "_%03d", suffix++);
        id = base + suffixBuffer;
    }
    std::snprintf(modal.soundIdBuffer, sizeof(modal.soundIdBuffer), "%s", id.c_str());
}

bool SectorEditorSoundService::ValidateAdd(std::string& error) const
{
    const AddMapSoundState& modal = context_.state.addMapSound;
    error.clear();
    if (modal.selectedPathIndex < 0
            || modal.selectedPathIndex >= static_cast<int>(modal.paths.size())) {
        error = "Select an audio file";
        return false;
    }
    const std::string id = modal.soundIdBuffer;
    if (id.empty()) {
        error = "Sound ID is required";
        return false;
    }
    if (!IsValidTextureId(id)) {
        error = "Sound ID may only contain letters, digits, underscores, and dashes";
        return false;
    }
    return true;
}

void SectorEditorSoundService::OpenAddModal()
{
    CloseAddModal();
    AddMapSoundState& modal = context_.state.addMapSound;
    modal.open = true;
    modal.paths = ScanAssetAudioFiles(modal.scanMessage);
    modal.optionLabels.reserve(modal.paths.size());
    for (const std::string& path : modal.paths) modal.optionLabels.push_back(path.c_str());
    modal.scanned = true;
    SelectAddPath(modal.paths.empty() ? -1 : 0);
    context_.statusText = "Add map sound";
}

void SectorEditorSoundService::CloseAddModal()
{
    StopPreview(context_.state.addMapSound.preview, true);
    context_.state.addMapSound = AddMapSoundState{};
}

bool SectorEditorSoundService::AddSelected()
{
    AddMapSoundState& modal = context_.state.addMapSound;
    std::string error;
    if (!ValidateAdd(error)) {
        modal.validationMessage = error;
        return false;
    }
    const std::string id = modal.soundIdBuffer;
    const bool replacing = Find(id) != nullptr;
    SectorSoundDefinition definition;
    definition.id = id;
    definition.path = modal.paths[static_cast<size_t>(modal.selectedPathIndex)];
    definition.type = modal.type;
    context_.map.audioSettings.soundsById[id] = std::move(definition);
    context_.lifecycle.topologyDocumentDirty = true;
    context_.lifecycle.hasUnsavedChanges = true;
    RefreshCatalogHandles();
    context_.statusText = TextFormat(
            "%s map sound %s",
            replacing ? "Updated" : "Added",
            id.c_str());
    CloseAddModal();
    return true;
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

void SectorEditorSoundService::PreviewAddSelection()
{
    AddMapSoundState& modal = context_.state.addMapSound;
    if (modal.selectedPathIndex < 0
            || modal.selectedPathIndex >= static_cast<int>(modal.paths.size())) {
        modal.previewMessage = "Select an audio file";
        return;
    }
    StopPreview(modal.preview, true);
    modal.preview.scope = context_.engineContext.assets.CreateScope(
            "sector_editor_add_sound_preview");
    if (engine::IsNull(modal.preview.scope)) {
        modal.previewMessage = "Could not create preview scope";
        return;
    }
    modal.preview.type = modal.type;
    modal.preview.key = modal.paths[static_cast<size_t>(modal.selectedPathIndex)];
    const std::string path = ResolveSectorAudioAssetPath(modal.preview.key);
    if (modal.type == SectorSoundType::Music) {
        modal.preview.music = context_.engineContext.assets.RequestMusic(
                modal.preview.scope,
                path.c_str());
        modal.preview.pending = !engine::IsNull(modal.preview.music);
    } else {
        modal.preview.sound = context_.engineContext.assets.RequestSound(
                modal.preview.scope,
                path.c_str());
        modal.preview.pending = !engine::IsNull(modal.preview.sound);
    }
    modal.previewMessage = modal.preview.pending
            ? "Loading preview..."
            : "Preview request failed";
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
    picker.target = target;
    picker.runtimeObjectId = runtimeObjectId;
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
    const int objectId = picker.runtimeObjectId;
    const SectorEditorDoorSoundTarget target = picker.target;
    ClosePicker();
    if (context_.runtimeObjectEditing == nullptr) return false;
    const SectorPlacedRuntimeObject* object = context_.runtimeObjectEditing->SelectedObject();
    if (object == nullptr || object->id != objectId || object->kind != "door") {
        context_.statusText = "Door sound target unavailable";
        return false;
    }
    const bool changed = context_.runtimeObjectEditing->MutateSelected(
            target == SectorEditorDoorSoundTarget::Open
                    ? "Updated door open sound"
                    : "Updated door close sound",
            [selected, target](SectorPlacedRuntimeObject& edited) {
                if (edited.kind != "door") return false;
                std::string& field = target == SectorEditorDoorSoundTarget::Open
                        ? edited.door.openSoundId
                        : edited.door.closeSoundId;
                if (field == selected) return false;
                field = selected;
                return true;
            });
    context_.statusText = changed
            ? TextFormat("Selected door %s sound %s",
                    target == SectorEditorDoorSoundTarget::Open ? "open" : "close",
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
    picker.preview.type = SectorSoundType::Sound;
    picker.preview.key = id;
    picker.preview.sound = SoundHandleForId(id);
    picker.preview.pending = !engine::IsNull(picker.preview.sound);
    picker.message = picker.preview.pending
            ? "Loading preview..."
            : "Sound is unavailable";
}

void SectorEditorSoundService::DrawAddModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::FontHandle font)
{
    AddMapSoundState& modalState = context_.state.addMapSound;
    if (!modalState.open) return;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    CloseAddModal();
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    AddSelected();
                    engine::ConsumeEvent(event);
                }
            });
    if (!modalState.open) return;
    UpdatePreview(modalState.preview, modalState.previewMessage);

    engine::AssetManager& assets = context_.engineContext.assets;
    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{(EditorWidth - 1100.0f) * 0.5f, (EditorHeight - 660.0f) * 0.5f, 1100.0f, 660.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets, Rectangle{modal.x + 22.0f, modal.y + 18.0f, modal.width - 44.0f, 36.0f}, font, "Add Map Sound");

    const Rectangle listBounds{modal.x + 22.0f, modal.y + 68.0f, 610.0f, 470.0f};
    const Vector2 contentSize{
            ScrollContentWidth(listBounds.width, config),
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(modalState.optionLabels.size()))};
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui, config, input, "sector_editor_add_sound_scroll",
            listBounds, contentSize, modalState.scroll);
    if (!modalState.optionLabels.empty()) {
        const int oldSelection = modalState.selectedPathIndex;
        engine::List(ui, config, input, assets, "sector_editor_add_sound_list",
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y}, font,
                modalState.optionLabels.data(), modalState.optionLabels.size(), modalState.selectedPathIndex);
        if (oldSelection != modalState.selectedPathIndex) {
            StopPreview(modalState.preview, true);
            modalState.previewMessage.clear();
            SelectAddPath(modalState.selectedPathIndex);
        }
    }
    engine::EndScrollArea(ui, config, input, scroll, modalState.scroll);
    if (!modalState.scanMessage.empty()) {
        engine::Text(config, assets,
                Rectangle{listBounds.x, listBounds.y + listBounds.height + 8.0f, listBounds.width, 40.0f},
                font, modalState.scanMessage.c_str(), engine::UITextJustify::Left,
                modalState.paths.empty() ? config.invalidColor : config.mutedTextColor);
    }

    const float rightX = modal.x + 658.0f;
    float y = modal.y + 82.0f;
    const std::string path = modalState.selectedPathIndex >= 0
            && modalState.selectedPathIndex < static_cast<int>(modalState.paths.size())
            ? modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)]
            : std::string{};
    engine::Text(config, assets, Rectangle{rightX, y, 410.0f, 86.0f}, font,
            TextFormat("Path: %s", path.empty() ? "<none>" : path.c_str()),
            engine::UITextJustify::Left, config.mutedTextColor);
    y += 104.0f;
    engine::Text(config, assets, Rectangle{rightX, y, 110.0f, 38.0f}, font,
            "Sound ID", engine::UITextJustify::Left, config.mutedTextColor);
    engine::TextInput(ui, config, input, assets, "sector_editor_add_sound_id",
            Rectangle{rightX + 126.0f, y, 284.0f, 38.0f}, font,
            modalState.soundIdBuffer, sizeof(modalState.soundIdBuffer), 0,
            sizeof(modalState.soundIdBuffer) - 1);
    y += 56.0f;
    engine::Text(config, assets, Rectangle{rightX, y, 110.0f, 38.0f}, font,
            "Type", engine::UITextJustify::Left, config.mutedTextColor);
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_sound_type_sound",
            Rectangle{rightX + 126.0f, y, 136.0f, 38.0f}, font, "Sound",
            modalState.type == SectorSoundType::Sound)) {
        StopPreview(modalState.preview, true);
        modalState.previewMessage.clear();
        modalState.type = SectorSoundType::Sound;
    }
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_sound_type_music",
            Rectangle{rightX + 272.0f, y, 138.0f, 38.0f}, font, "Music",
            modalState.type == SectorSoundType::Music)) {
        StopPreview(modalState.preview, true);
        modalState.previewMessage.clear();
        modalState.type = SectorSoundType::Music;
    }
    y += 62.0f;
    engine::Text(config, assets, Rectangle{rightX, y, 410.0f, 38.0f}, font,
            TextFormat("Loads as: %s", SoundTypeLabel(modalState.type)),
            engine::UITextJustify::Left, config.mutedTextColor);
    y += 48.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_add_sound_preview",
            Rectangle{rightX, y, 150.0f, 42.0f}, font, "Preview")) {
        PreviewAddSelection();
    }
    engine::Text(config, assets, Rectangle{rightX + 166.0f, y, 244.0f, 50.0f}, font,
            modalState.previewMessage.c_str(), engine::UITextJustify::Left, config.mutedTextColor);
    y += 70.0f;
    ValidateAdd(modalState.validationMessage);
    if (!modalState.validationMessage.empty()) {
        engine::Text(config, assets, Rectangle{rightX, y, 410.0f, 56.0f}, font,
                modalState.validationMessage.c_str(), engine::UITextJustify::Left, config.invalidColor);
    }

    const float buttonY = modal.y + modal.height - 64.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_add_sound_add",
            Rectangle{modal.x + modal.width - 334.0f, buttonY, 150.0f, 44.0f}, font, "Add")) {
        AddSelected();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_add_sound_cancel",
            Rectangle{modal.x + modal.width - 172.0f, buttonY, 150.0f, 44.0f}, font, "Cancel")) {
        CloseAddModal();
    }
    input.ForEachEvent(engine::InputEventType::Any, true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
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
            font, picker.target == SectorEditorDoorSoundTarget::Open
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
