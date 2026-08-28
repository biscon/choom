#include "sector_editor/npcs/SectorEditorNpcEditorModal.h"

#include "game/npc/ai/NpcAiTypes.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/static_model_picker/SectorEditorModelPickerModal.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace game {
namespace {

constexpr float RowHeight = 42.0f;
constexpr float RowGap = 10.0f;
constexpr float LabelWidth = 220.0f;

float ScrollContentWidth(float boundsWidth, const engine::UIConfig& config)
{
    const float clientWidth = std::max(
            0.0f,
            boundsWidth - config.borderThickness * 2.0f);
    return std::max(
            0.0f,
            clientWidth - config.scrollbarSize
                    - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

void DrawModalFrame(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const SectorEditorNpcEditorModalLayout& layout)
{
    DrawRectangle(
            0,
            0,
            static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight),
            Color{0, 0, 0, 150});
    DrawRectangleRec(layout.modal, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(layout.modal, config.borderThickness, config.borderColor);
    engine::Text(
            config,
            assets,
            Rectangle{
                    layout.modal.x + 24.0f,
                    layout.modal.y + 16.0f,
                    layout.modal.width - 48.0f,
                    42.0f},
            font,
            "NPC Editor");
}

void ConsumeRemainingInput(engine::Input& input)
{
    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

void DrawDeleteConfirmation(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorNpcEditorService& editor,
        const SectorEditorNpcEditorModalLayout& layout)
{
    const Rectangle popup{
            layout.modal.x + (layout.modal.width - 600.0f) * 0.5f,
            layout.modal.y + (layout.modal.height - 250.0f) * 0.5f,
            600.0f,
            250.0f};
    DrawRectangleRec(popup, Color{27, 32, 42, 255});
    DrawRectangleLinesEx(popup, config.borderThickness, config.borderColor);
    engine::Text(
            config, assets,
            Rectangle{popup.x + 24.0f, popup.y + 20.0f, popup.width - 48.0f, 38.0f},
            font, "Delete NPC definition?");
    const std::string message = "Delete '"
            + editor.State().deleteConfirmationId
            + "' when the catalog is saved? The character model will not be deleted.";
    engine::Text(
            config, assets,
            Rectangle{popup.x + 24.0f, popup.y + 70.0f, popup.width - 48.0f, 72.0f},
            font, message.c_str(), engine::UITextJustify::Left,
            config.textColor, true);
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_delete_confirm",
                Rectangle{popup.x + popup.width - 310.0f, popup.y + 178.0f, 130.0f, 44.0f},
                font, "Delete")) {
        editor.ConfirmDeleteSelected();
    }
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_delete_cancel",
                Rectangle{popup.x + popup.width - 160.0f, popup.y + 178.0f, 130.0f, 44.0f},
                font, "Cancel")) {
        editor.CancelDelete();
    }
}

} // namespace

SectorEditorNpcEditorModalLayout BuildSectorEditorNpcEditorModalLayout()
{
    return BuildSectorEditorNpcEditorModalLayoutForViewport(
            EditorWidth,
            EditorHeight);
}

SectorEditorNpcEditorModalResult DrawSectorEditorNpcEditorModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorNpcEditorService& editor,
        SectorEditorStaticModelPickerService& modelPicker,
        SectorEditorAudioAssetPickerService& audioPicker)
{
    SectorEditorNpcEditorState& state = editor.State();
    if (!state.open) return SectorEditorNpcEditorModalResult::None;
    const SectorEditorNpcEditorModalLayout layout =
            BuildSectorEditorNpcEditorModalLayout();
    DrawModalFrame(config, assets, font, layout);

    if (modelPicker.State().open
            && modelPicker.State().target == ModelPickerTarget::NpcDefinition) {
        const SectorEditorModelPickerModalResult result =
                DrawSectorEditorModelPickerModal(
                        ui, config, input, assets, font, modelPicker);
        if (result == SectorEditorModelPickerModalResult::Selected) {
            editor.SetSelectedModelPath(modelPicker.SelectedModelPath(), assets);
            modelPicker.State().open = false;
        }
        return SectorEditorNpcEditorModalResult::None;
    }

    if (state.audioPicker.assetPicker.open) {
        const SectorEditorAudioAssetPickerResult result = audioPicker.DrawModal(
                ui, config, input, font, state.audioPicker.assetPicker);
        if (result == SectorEditorAudioAssetPickerResult::Selected) {
            const std::string selected = audioPicker.SelectedPath(
                    state.audioPicker.assetPicker);
            switch (state.audioPicker.target) {
                case SectorEditorNpcAudioPickerTarget::PlayerDetected:
                    editor.SetSelectedPlayerDetectedSound(selected);
                    break;
                case SectorEditorNpcAudioPickerTarget::Action:
                    editor.SetSelectedActionSound(
                            state.audioPicker.action, selected);
                    break;
                case SectorEditorNpcAudioPickerTarget::Attack:
                    editor.SetSelectedAttackSound(selected);
                    break;
                case SectorEditorNpcAudioPickerTarget::AmbientAdd:
                    editor.AddSelectedAmbientSound(selected);
                    break;
                case SectorEditorNpcAudioPickerTarget::AmbientReplace:
                    editor.ReplaceSelectedAmbientSound(
                            state.audioPicker.ambientIndex, selected);
                    break;
                case SectorEditorNpcAudioPickerTarget::None:
                    break;
            }
            audioPicker.Close(state.audioPicker.assetPicker);
            state.audioPicker.target = SectorEditorNpcAudioPickerTarget::None;
        } else if (result == SectorEditorAudioAssetPickerResult::Cancelled) {
            audioPicker.Close(state.audioPicker.assetPicker);
            state.audioPicker.target = SectorEditorNpcAudioPickerTarget::None;
        }
        return SectorEditorNpcEditorModalResult::None;
    }

    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested, &state](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                if (state.deleteConfirmationOpen) {
                    state.deleteConfirmationOpen = false;
                    state.deleteConfirmationId.clear();
                } else {
                    cancelRequested = true;
                }
                engine::ConsumeEvent(event);
            });
    if (cancelRequested) {
        editor.Cancel(&assets);
        ConsumeRemainingInput(input);
        return SectorEditorNpcEditorModalResult::Cancelled;
    }

    if (state.deleteConfirmationOpen) {
        DrawDeleteConfirmation(ui, config, input, assets, font, editor, layout);
        ConsumeRemainingInput(input);
        return SectorEditorNpcEditorModalResult::None;
    }

    const float listContentW = ScrollContentWidth(layout.listBounds.width, config);
    const Vector2 listContentSize{
            listContentW,
            std::max(
                    layout.listBounds.height,
                    config.listItemHeight
                            * static_cast<float>(state.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input,
            "sector_editor_npc_list_scroll",
            layout.listBounds,
            listContentSize,
            editor.Session().listScroll);
    if (!state.listLabels.empty()) {
        int selectedIndex = state.selectedIndex;
        engine::List(
                ui, config, input, assets,
                "sector_editor_npc_list",
                Rectangle{0.0f, 0.0f, listScroll.viewport.width, listContentSize.y},
                smallFont,
                state.listLabels.data(),
                state.listLabels.size(),
                selectedIndex);
        if (selectedIndex != state.selectedIndex && editor.SelectIndex(selectedIndex)) {
            ui.focusedId = 0;
            ui.openOptionId = 0;
            editor.EnsureSelectedModelRequested(assets);
        }
    }
    engine::EndScrollArea(
            ui, config, input, listScroll, editor.Session().listScroll);

    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_add",
                layout.addButton,
                font, "Add")) {
        editor.AddDefinition();
        ui.focusedId = 0;
        ui.openOptionId = 0;
    }
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_delete",
                layout.deleteButton,
                font, "Delete")
            && editor.SelectedDraft() != nullptr) {
        editor.RequestDeleteSelected();
    }

    SectorEditorNpcDefinitionDraft* selected = editor.SelectedDraft();
    if (selected == nullptr) {
        engine::Text(
                config, assets,
                Rectangle{
                        layout.formBounds.x + 24.0f,
                        layout.formBounds.y + 24.0f,
                        layout.formBounds.width - 48.0f,
                        80.0f},
                font,
                "No NPC definitions. Press Add to create one.",
                engine::UITextJustify::Left,
                config.mutedTextColor,
                true);
    } else {
        editor.RefreshAnimationOptions(assets);
        const float contentW = ScrollContentWidth(layout.formBounds.width, config);
        const float actionSectionHeight = 9.0f * (RowHeight + RowGap) + 84.0f;
        const float contentHeight = 21.0f * (RowHeight + RowGap)
                + static_cast<float>(
                        selected->definition.ambientVocalizations.soundPaths.size())
                        * (RowHeight + RowGap)
                + actionSectionHeight * static_cast<float>(kNpcActionCount)
                + 120.0f;
        engine::UIScrollAreaResult formScroll = engine::BeginScrollArea(
                ui, config, input,
                "sector_editor_npc_form_scroll",
                layout.formBounds,
                Vector2{contentW, contentHeight},
                editor.Session().formScroll);
        float y = 0.0f;
        const float fieldX = LabelWidth + 16.0f;
        const float fieldW = std::max(160.0f, formScroll.viewport.width - fieldX);
        const auto drawLabel = [&](const char* label) {
            engine::Text(
                    ui, config, assets,
                    Rectangle{0.0f, y, LabelWidth, RowHeight},
                    font, label, engine::UITextJustify::Left,
                    config.mutedTextColor);
        };

        drawLabel("ID");
        const engine::UITextInputResult idResult = engine::TextInput(
                ui, config, input, assets,
                "sector_editor_npc_id",
                Rectangle{fieldX, y, fieldW, RowHeight},
                font,
                state.idBuffer,
                sizeof(state.idBuffer),
                0,
                sizeof(state.idBuffer) - 1);
        if (idResult.changed) editor.ApplyIdBuffer();
        y += RowHeight + RowGap;

        drawLabel("Name");
        const engine::UITextInputResult nameResult = engine::TextInput(
                ui, config, input, assets,
                "sector_editor_npc_name",
                Rectangle{fieldX, y, fieldW, RowHeight},
                font,
                state.nameBuffer,
                sizeof(state.nameBuffer),
                0,
                sizeof(state.nameBuffer) - 1);
        if (nameResult.changed) editor.ApplyNameBuffer();
        y += RowHeight + RowGap;

        bool hostile = selected->definition.hostile;
        if (engine::Checkbox(
                    ui, config, input, assets,
                    "sector_editor_npc_hostile",
                    Rectangle{fieldX, y, 260.0f, RowHeight},
                    font, "Hostile", hostile)) {
            editor.SetSelectedHostile(hostile);
        }
        y += RowHeight + RowGap;

        drawLabel("AI Type");
        std::vector<std::string> aiOptionStorage{"None"};
        std::vector<std::string> aiOptionIds{""};
        for (const NpcAiTypeDescriptor& type : NpcAiTypeRegistry()) {
            if (!IsNpcAiTypeCompatible(type, selected->definition.hostile)) continue;
            aiOptionStorage.emplace_back(type.displayName);
            aiOptionIds.emplace_back(type.id);
        }
        int selectedAi = 0;
        for (size_t index = 1; index < aiOptionIds.size(); ++index) {
            if (aiOptionIds[index] == selected->definition.aiType) {
                selectedAi = static_cast<int>(index);
                break;
            }
        }
        if (!selected->definition.aiType.empty() && selectedAi == 0) {
            aiOptionStorage.push_back(
                    "<Incompatible: " + selected->definition.aiType + ">");
            aiOptionIds.push_back(selected->definition.aiType);
            selectedAi = static_cast<int>(aiOptionIds.size()) - 1;
        }
        const int previousAi = selectedAi;
        if (engine::Option(
                    ui, config, input, assets,
                    "sector_editor_npc_ai_type",
                    Rectangle{fieldX, y, fieldW, RowHeight},
                    font, aiOptionStorage, selectedAi)
                && selectedAi != previousAi
                && selectedAi >= 0
                && selectedAi < static_cast<int>(aiOptionIds.size())) {
            editor.SetSelectedAiType(
                    aiOptionIds[static_cast<size_t>(selectedAi)]);
        }
        y += RowHeight + RowGap;

        if (selected->definition.hostile) {
            drawLabel("Player detected sound");
            engine::Text(
                    ui, config, assets,
                    Rectangle{
                            fieldX,
                            y,
                            std::max(0.0f, fieldW - 206.0f),
                            RowHeight},
                    smallFont,
                    selected->definition.playerDetectedSoundPath.empty()
                            ? "<none>"
                            : selected->definition
                                    .playerDetectedSoundPath.c_str(),
                    engine::UITextJustify::Left,
                    selected->definition.playerDetectedSoundPath.empty()
                            ? config.mutedTextColor
                            : config.textColor);
            if (engine::Button(
                        ui, config, input, assets,
                        "sector_editor_npc_pick_player_detected_sound",
                        Rectangle{
                                fieldX + fieldW - 196.0f,
                                y,
                                92.0f,
                                RowHeight},
                        font, "Pick")) {
                state.audioPicker.target =
                        SectorEditorNpcAudioPickerTarget::PlayerDetected;
                audioPicker.Open(
                        state.audioPicker.assetPicker,
                        "Pick Player Detected Sound",
                        selected->definition.playerDetectedSoundPath);
            }
            if (engine::Button(
                        ui, config, input, assets,
                        "sector_editor_npc_clear_player_detected_sound",
                        Rectangle{
                                fieldX + fieldW - 96.0f,
                                y,
                                96.0f,
                                RowHeight},
                        font, "Clear")) {
                editor.SetSelectedPlayerDetectedSound({});
            }
            y += RowHeight + RowGap;
        }

        drawLabel("Vision range world");
        float visionRange = selected->definition.perception.visionRangeWorld;
        auto perceptionFloat = engine::FloatInput(
                ui, config, input, assets, "sector_editor_npc_vision_range",
                Rectangle{fieldX, y, 190.0f, RowHeight}, font, visionRange,
                state.visionRangeWorldInput, 0.0f, 10000.0f, 2);
        if (perceptionFloat.changed) {
            editor.SetSelectedPerception(
                    visionRange,
                    selected->definition.perception.visionAngleDegrees,
                    selected->definition.perception.hearingRangeWorld,
                    selected->definition.perception.investigationDurationMilliseconds);
        }
        y += RowHeight + RowGap;

        drawLabel("Vision angle degrees");
        float visionAngle = selected->definition.perception.visionAngleDegrees;
        perceptionFloat = engine::FloatInput(
                ui, config, input, assets, "sector_editor_npc_vision_angle",
                Rectangle{fieldX, y, 190.0f, RowHeight}, font, visionAngle,
                state.visionAngleDegreesInput, 0.01f, 359.99f, 2);
        if (perceptionFloat.changed) {
            editor.SetSelectedPerception(
                    selected->definition.perception.visionRangeWorld,
                    visionAngle,
                    selected->definition.perception.hearingRangeWorld,
                    selected->definition.perception.investigationDurationMilliseconds);
        }
        y += RowHeight + RowGap;

        drawLabel("Hearing range world");
        float hearingRange = selected->definition.perception.hearingRangeWorld;
        perceptionFloat = engine::FloatInput(
                ui, config, input, assets, "sector_editor_npc_hearing_range",
                Rectangle{fieldX, y, 190.0f, RowHeight}, font, hearingRange,
                state.hearingRangeWorldInput, 0.0f, 10000.0f, 2);
        if (perceptionFloat.changed) {
            editor.SetSelectedPerception(
                    selected->definition.perception.visionRangeWorld,
                    selected->definition.perception.visionAngleDegrees,
                    hearingRange,
                    selected->definition.perception.investigationDurationMilliseconds);
        }
        y += RowHeight + RowGap;

        drawLabel("Investigation ms");
        int investigationMs = selected->definition.perception
                .investigationDurationMilliseconds;
        const engine::UINumericInputResult investigationResult = engine::IntInput(
                ui, config, input, assets,
                "sector_editor_npc_investigation_ms",
                Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                investigationMs,
                state.investigationDurationMillisecondsInput,
                0, 600000, 100);
        if (investigationResult.changed) {
            editor.SetSelectedPerception(
                    selected->definition.perception.visionRangeWorld,
                    selected->definition.perception.visionAngleDegrees,
                    selected->definition.perception.hearingRangeWorld,
                    investigationMs);
        }
        y += RowHeight + RowGap;

        bool canOpenDoors = selected->definition.canOpenDoors;
        if (engine::Checkbox(
                    ui, config, input, assets,
                    "sector_editor_npc_can_open_doors",
                    Rectangle{fieldX, y, 260.0f, RowHeight},
                    font, "Can open doors", canOpenDoors)) {
            editor.SetSelectedCanOpenDoors(canOpenDoors);
        }
        y += RowHeight + RowGap;

        drawLabel("Base health");
        int baseHealth = selected->definition.baseHealth;
        const engine::UINumericInputResult healthResult = engine::IntInput(
                ui, config, input, assets,
                "sector_editor_npc_base_health",
                Rectangle{fieldX, y, 240.0f, RowHeight},
                font,
                baseHealth,
                state.baseHealthInput,
                kMinimumNpcBaseHealth,
                kMaximumNpcBaseHealth,
                10);
        if (healthResult.changed) editor.SetSelectedBaseHealth(baseHealth);
        y += RowHeight + RowGap;

        bool despawnOnDeath = selected->definition.despawnOnDeath;
        if (engine::Checkbox(
                    ui, config, input, assets,
                    "sector_editor_npc_despawn_on_death",
                    Rectangle{fieldX, y, 300.0f, RowHeight},
                    font, "Fade and despawn corpse", despawnOnDeath)) {
            editor.SetSelectedDespawnOnDeath(despawnOnDeath);
        }
        y += RowHeight + RowGap;

        if (selected->definition.despawnOnDeath) {
            drawLabel("Despawn delay (ms)");
            int delayMilliseconds = static_cast<int>(std::lround(
                    selected->definition.corpseDespawnDelaySeconds * 1000.0f));
            const engine::UINumericInputResult delayResult = engine::IntInput(
                    ui, config, input, assets,
                    "sector_editor_npc_corpse_despawn_delay_ms",
                    Rectangle{fieldX, y, 240.0f, RowHeight},
                    font,
                    delayMilliseconds,
                    state.corpseDespawnDelayMillisecondsInput,
                    0,
                    static_cast<int>(
                            kMaximumNpcCorpseDespawnDelaySeconds * 1000.0f),
                    100);
            if (delayResult.changed) {
                editor.SetSelectedCorpseDespawnDelayMilliseconds(
                        delayMilliseconds);
            }
            y += RowHeight + RowGap;

            drawLabel("Fade duration (ms)");
            int fadeMilliseconds = static_cast<int>(std::lround(
                    selected->definition.corpseFadeDurationSeconds * 1000.0f));
            const engine::UINumericInputResult fadeResult = engine::IntInput(
                    ui, config, input, assets,
                    "sector_editor_npc_corpse_fade_duration_ms",
                    Rectangle{fieldX, y, 240.0f, RowHeight},
                    font,
                    fadeMilliseconds,
                    state.corpseFadeDurationMillisecondsInput,
                    1,
                    static_cast<int>(
                            kMaximumNpcCorpseFadeDurationSeconds * 1000.0f),
                    50);
            if (fadeResult.changed) {
                editor.SetSelectedCorpseFadeDurationMilliseconds(
                        fadeMilliseconds);
            }
            y += RowHeight + RowGap;
        }

        drawLabel("Model");
        engine::Text(
                ui, config, assets,
                Rectangle{fieldX, y, std::max(0.0f, fieldW - 190.0f), RowHeight},
                smallFont,
                selected->definition.modelPath.empty()
                        ? "<none>"
                        : selected->definition.modelPath.c_str(),
                engine::UITextJustify::Left,
                selected->definition.modelPath.empty()
                        ? config.invalidColor
                        : config.textColor);
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_npc_choose_model",
                    Rectangle{fieldX + fieldW - 176.0f, y, 176.0f, RowHeight},
                    font, "Choose Model")) {
            modelPicker.Open(
                    selected->definition.modelPath,
                    ModelPickerTarget::NpcDefinition);
        }
        y += RowHeight + RowGap;

        drawLabel("Animation blend");
        float animationBlendSeconds = selected->definition.animationBlendSeconds;
        const engine::UINumericInputResult blendResult = engine::FloatInput(
                ui, config, input, assets,
                "sector_editor_npc_animation_blend_seconds",
                Rectangle{fieldX, y, 190.0f, RowHeight},
                font,
                animationBlendSeconds,
                state.animationBlendSecondsInput,
                kMinimumNpcAnimationBlendSeconds,
                kMaximumNpcAnimationBlendSeconds,
                2);
        if (blendResult.changed) {
            editor.SetSelectedAnimationBlendSeconds(animationBlendSeconds);
        }
        y += RowHeight + RowGap;

        const char* modelStatus = !state.warningMessage.empty()
                ? state.warningMessage.c_str()
                : (selected->definition.modelPath.empty()
                ? "Select a character model before saving"
                : (editor.SelectedModelReady(assets)
                        ? (state.animationOptionStorage.size() <= 1
                                ? "Model contains no named animations"
                                : "Model animations loaded")
                        : (editor.SelectedModelFailed(assets)
                                ? "Model failed to load; saved action names are preserved"
                                : "Model animations loading...")));
        engine::Text(
                ui, config, assets,
                Rectangle{fieldX, y, fieldW, RowHeight},
                smallFont, modelStatus, engine::UITextJustify::Left,
                !state.warningMessage.empty()
                        || editor.SelectedModelFailed(assets)
                        || selected->definition.modelPath.empty()
                        ? config.invalidColor
                        : config.mutedTextColor);
        y += RowHeight + RowGap;

        engine::Separator(
                config,
                Rectangle{
                        formScroll.viewport.x,
                        formScroll.viewport.y
                                - editor.Session().formScroll.offset.y + y,
                        formScroll.viewport.width,
                        12.0f});
        y += 18.0f;
        engine::Text(
                ui, config, assets,
                Rectangle{0.0f, y, formScroll.viewport.width, 34.0f},
                font, "Ambient vocalizations",
                engine::UITextJustify::Left,
                config.accentColor);
        y += 38.0f;

        drawLabel("Minimum quiet time (s)");
        float ambientMinimum =
                selected->definition.ambientVocalizations.minimumDelaySeconds;
        const engine::UINumericInputResult ambientMinimumResult =
                engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_ambient_minimum_delay",
                        Rectangle{fieldX, y, 190.0f, RowHeight},
                        font,
                        ambientMinimum,
                        state.ambientMinimumDelaySecondsInput,
                        0.0f,
                        kMaximumNpcAmbientDelaySeconds,
                        2);
        if (ambientMinimumResult.changed) {
            editor.SetSelectedAmbientDelayRange(
                    ambientMinimum,
                    selected->definition.ambientVocalizations.maximumDelaySeconds);
        }
        y += RowHeight + RowGap;

        drawLabel("Maximum quiet time (s)");
        float ambientMaximum =
                selected->definition.ambientVocalizations.maximumDelaySeconds;
        const engine::UINumericInputResult ambientMaximumResult =
                engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_ambient_maximum_delay",
                        Rectangle{fieldX, y, 190.0f, RowHeight},
                        font,
                        ambientMaximum,
                        state.ambientMaximumDelaySecondsInput,
                        0.0f,
                        kMaximumNpcAmbientDelaySeconds,
                        2);
        if (ambientMaximumResult.changed) {
            editor.SetSelectedAmbientDelayRange(
                    selected->definition.ambientVocalizations.minimumDelaySeconds,
                    ambientMaximum);
        }
        y += RowHeight + RowGap;

        for (size_t soundIndex = 0;
                soundIndex
                        < selected->definition.ambientVocalizations.soundPaths.size();
                ++soundIndex) {
            const std::string& soundPath =
                    selected->definition.ambientVocalizations
                            .soundPaths[soundIndex];
            const std::string changeId =
                    "sector_editor_npc_ambient_change_"
                    + std::to_string(soundIndex);
            const std::string removeId =
                    "sector_editor_npc_ambient_remove_"
                    + std::to_string(soundIndex);
            engine::Text(
                    ui, config, assets,
                    Rectangle{fieldX, y, std::max(0.0f, fieldW - 206.0f), RowHeight},
                    smallFont, soundPath.c_str(),
                    engine::UITextJustify::Left,
                    config.textColor);
            if (engine::Button(
                        ui, config, input, assets,
                        changeId.c_str(),
                        Rectangle{fieldX + fieldW - 196.0f, y, 92.0f, RowHeight},
                        font, "Change")) {
                state.audioPicker.target =
                        SectorEditorNpcAudioPickerTarget::AmbientReplace;
                state.audioPicker.ambientIndex = soundIndex;
                audioPicker.Open(
                        state.audioPicker.assetPicker,
                        "Replace Ambient Vocalization",
                        soundPath);
            }
            if (engine::Button(
                        ui, config, input, assets,
                        removeId.c_str(),
                        Rectangle{fieldX + fieldW - 96.0f, y, 96.0f, RowHeight},
                        font, "Remove")) {
                editor.RemoveSelectedAmbientSound(soundIndex);
                break;
            }
            y += RowHeight + RowGap;
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_npc_ambient_add",
                    Rectangle{fieldX, y, 170.0f, RowHeight},
                    font, "Add Sound")) {
            state.audioPicker.target =
                    SectorEditorNpcAudioPickerTarget::AmbientAdd;
            audioPicker.Open(
                    state.audioPicker.assetPicker,
                    "Add Ambient Vocalization");
        }
        y += RowHeight + RowGap;

        for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
            engine::Separator(
                    config,
                    Rectangle{
                            formScroll.viewport.x,
                            formScroll.viewport.y
                                    - editor.Session().formScroll.offset.y + y,
                            formScroll.viewport.width,
                            12.0f});
            y += 18.0f;
            engine::Text(
                    ui, config, assets,
                    Rectangle{0.0f, y, formScroll.viewport.width, 34.0f},
                    font, metadata.displayName,
                    engine::UITextJustify::Left,
                    config.accentColor);
            y += 38.0f;

            NpcActionDefinition& action =
                    GetNpcAction(selected->definition, metadata.action);
            drawLabel("Animation");
            if (!state.animationOptions.empty()) {
                int selectedAnimation = 0;
                bool authoredAnimationFound = action.animation.empty();
                for (size_t index = 1; index < state.animationOptionStorage.size(); ++index) {
                    if (state.animationOptionStorage[index] == action.animation) {
                        selectedAnimation = static_cast<int>(index);
                        authoredAnimationFound = true;
                        break;
                    }
                }
                std::string missingOptionLabel;
                std::vector<const char*> actionOptions = state.animationOptions;
                if (!authoredAnimationFound) {
                    missingOptionLabel = "<Missing: " + action.animation + ">";
                    actionOptions.push_back(missingOptionLabel.c_str());
                    selectedAnimation = static_cast<int>(actionOptions.size()) - 1;
                }
                const std::string optionId = std::string{"sector_editor_npc_animation_"}
                        + metadata.jsonKey;
                if (engine::Option(
                            ui, config, input, assets,
                            optionId.c_str(),
                            Rectangle{fieldX, y, fieldW, RowHeight},
                            font,
                            actionOptions.data(),
                            actionOptions.size(),
                            selectedAnimation)) {
                    editor.SetSelectedAnimation(
                            metadata.action,
                            selectedAnimation <= 0
                                    || selectedAnimation
                                            >= static_cast<int>(
                                                    state.animationOptionStorage.size())
                                    ? std::string{}
                                    : state.animationOptionStorage[
                                            static_cast<size_t>(selectedAnimation)]);
                }
            } else {
                engine::Text(
                        ui, config, assets,
                        Rectangle{fieldX, y, fieldW, RowHeight},
                        smallFont,
                        action.animation.empty()
                                ? "<Unassigned>"
                                : action.animation.c_str(),
                        engine::UITextJustify::Left,
                        config.mutedTextColor);
            }
            y += RowHeight + 2.0f;

            const bool missingAnimation = !action.animation.empty()
                    && editor.SelectedModelReady(assets)
                    && !editor.SelectedAnimationExists(assets, action.animation);
            const char* mappingMessage = action.animation.empty()
                    ? "Unassigned (allowed)"
                    : (missingAnimation
                            ? "Saved animation is missing from this model"
                            : "");
            engine::Text(
                    ui, config, assets,
                    Rectangle{fieldX, y, fieldW, 28.0f},
                    smallFont, mappingMessage,
                    engine::UITextJustify::Left,
                    missingAnimation ? config.invalidColor : config.mutedTextColor);
            y += 30.0f;

            drawLabel("Animation speed");
            float animationSpeed = action.animationSpeed;
            const std::string speedId = std::string{"sector_editor_npc_animation_speed_"}
                    + metadata.jsonKey;
            const engine::UINumericInputResult speedResult = engine::FloatInput(
                    ui, config, input, assets,
                    speedId.c_str(),
                    Rectangle{fieldX, y, 190.0f, RowHeight},
                    font,
                    animationSpeed,
                    state.animationSpeedInputs[static_cast<size_t>(metadata.action)],
                    0.01f,
                    10.0f,
                    3);
            if (speedResult.changed) {
                editor.SetSelectedAnimationSpeed(metadata.action, animationSpeed);
            }
            y += RowHeight + RowGap;

            if (metadata.hasSound) {
                drawLabel(metadata.action == NpcAction::Attack
                        ? "Player impact sound" : "Sound");
                engine::Text(
                        ui, config, assets,
                        Rectangle{
                                fieldX,
                                y,
                                std::max(0.0f, fieldW - 206.0f),
                                RowHeight},
                        smallFont,
                        action.soundPath.empty()
                                ? "<none>"
                                : action.soundPath.c_str(),
                        engine::UITextJustify::Left,
                        action.soundPath.empty()
                                ? config.mutedTextColor
                                : config.textColor);
                const std::string pickSoundId =
                        std::string{"sector_editor_npc_pick_sound_"}
                        + metadata.jsonKey;
                if (engine::Button(
                            ui, config, input, assets,
                            pickSoundId.c_str(),
                            Rectangle{
                                    fieldX + fieldW - 196.0f,
                                    y,
                                    92.0f,
                                    RowHeight},
                            font, "Pick")) {
                    state.audioPicker.target =
                            SectorEditorNpcAudioPickerTarget::Action;
                    state.audioPicker.action = metadata.action;
                    audioPicker.Open(
                            state.audioPicker.assetPicker,
                            std::string{"Pick "} + metadata.displayName + " Sound",
                            action.soundPath);
                }
                const std::string clearSoundId =
                        std::string{"sector_editor_npc_clear_sound_"}
                        + metadata.jsonKey;
                if (engine::Button(
                            ui, config, input, assets,
                            clearSoundId.c_str(),
                            Rectangle{
                                    fieldX + fieldW - 96.0f,
                                    y,
                                    96.0f,
                                    RowHeight},
                            font, "Clear")) {
                    editor.SetSelectedActionSound(metadata.action, {});
                }
                y += RowHeight + RowGap;
            }

            if (metadata.hasMovementSpeed) {
                drawLabel("Movement speed");
                float movementSpeed = action.movementSpeed;
                const std::string movementId =
                        std::string{"sector_editor_npc_movement_speed_"}
                        + metadata.jsonKey;
                const engine::UINumericInputResult movementResult = engine::FloatInput(
                        ui, config, input, assets,
                        movementId.c_str(),
                        Rectangle{fieldX, y, 190.0f, RowHeight},
                        font,
                        movementSpeed,
                        state.movementSpeedInputs[static_cast<size_t>(metadata.action)],
                        0.1f,
                        200.0f,
                        2);
                if (movementResult.changed) {
                    editor.SetSelectedMovementSpeed(metadata.action, movementSpeed);
                }
                y += RowHeight + RowGap;

                for (size_t phaseIndex = 0; phaseIndex < 2; ++phaseIndex) {
                    const std::string label = std::string{"Footstep "}
                            + std::to_string(phaseIndex + 1) + " phase";
                    drawLabel(label.c_str());
                    float phase = action.footstepPhases[phaseIndex];
                    const std::string phaseId =
                            std::string{"sector_editor_npc_footstep_phase_"}
                            + metadata.jsonKey + "_"
                            + std::to_string(phaseIndex);
                    const engine::UINumericInputResult phaseResult =
                            engine::FloatInput(
                                    ui, config, input, assets,
                                    phaseId.c_str(),
                                    Rectangle{fieldX, y, 190.0f, RowHeight},
                                    font,
                                    phase,
                                    state.footstepPhaseInputs[
                                            static_cast<size_t>(metadata.action)]
                                            [phaseIndex],
                                    0.0f,
                                    0.999f,
                                    3);
                    if (phaseResult.changed) {
                        editor.SetSelectedFootstepPhase(
                                metadata.action,
                                phaseIndex,
                                phase);
                    }
                    y += RowHeight + RowGap;
                }
            }

            if (metadata.action == NpcAction::Attack) {
                drawLabel("Attack sound");
                engine::Text(
                        ui, config, assets,
                        Rectangle{
                                fieldX,
                                y,
                                std::max(0.0f, fieldW - 206.0f),
                                RowHeight},
                        smallFont,
                        action.attackSoundPath.empty()
                                ? "<none>"
                                : action.attackSoundPath.c_str(),
                        engine::UITextJustify::Left,
                        action.attackSoundPath.empty()
                                ? config.mutedTextColor
                                : config.textColor);
                if (engine::Button(
                            ui, config, input, assets,
                            "sector_editor_npc_pick_attack_sound",
                            Rectangle{
                                    fieldX + fieldW - 196.0f,
                                    y,
                                    92.0f,
                                    RowHeight},
                            font, "Pick")) {
                    state.audioPicker.target =
                            SectorEditorNpcAudioPickerTarget::Attack;
                    audioPicker.Open(
                            state.audioPicker.assetPicker,
                            "Pick Attack Sound",
                            action.attackSoundPath);
                }
                if (engine::Button(
                            ui, config, input, assets,
                            "sector_editor_npc_clear_attack_sound",
                            Rectangle{
                                    fieldX + fieldW - 96.0f,
                                    y,
                                    96.0f,
                                    RowHeight},
                            font, "Clear")) {
                    editor.SetSelectedAttackSound({});
                }
                y += RowHeight + RowGap;

                drawLabel("Hit phase (0-1)");
                float value = action.hitPhase;
                auto floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_hit_phase",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackHitPhaseInput, 0.0f, 1.0f, 3);
                if (floatResult.changed) {
                    editor.SetSelectedAttack(
                            value, action.rangeWorld, action.damage,
                            action.knockbackImpulseWorldPerSecond,
                            action.stunMilliseconds);
                }
                y += RowHeight + RowGap;

                drawLabel("Range world");
                value = action.rangeWorld;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_range",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackRangeWorldInput, 0.001f, 10000.0f, 3);
                if (floatResult.changed) {
                    editor.SetSelectedAttack(
                            action.hitPhase, value, action.damage,
                            action.knockbackImpulseWorldPerSecond,
                            action.stunMilliseconds);
                }
                y += RowHeight + RowGap;

                drawLabel("Advance speed multiplier");
                value = action.advanceSpeedMultiplier;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_advance_speed_multiplier",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackAdvanceSpeedMultiplierInput,
                        0.0f, kMaximumNpcAttackAdvanceSpeedMultiplier, 3);
                if (floatResult.changed) {
                    editor.SetSelectedAttackMotion(
                            value,
                            action.aimTrackingEndPhase,
                            action.hitArcDegrees);
                }
                y += RowHeight + RowGap;

                drawLabel("Aim tracking end phase");
                value = action.aimTrackingEndPhase;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_aim_tracking_end_phase",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackAimTrackingEndPhaseInput,
                        0.0f, action.hitPhase, 3);
                if (floatResult.changed) {
                    editor.SetSelectedAttackMotion(
                            action.advanceSpeedMultiplier,
                            value,
                            action.hitArcDegrees);
                }
                y += RowHeight + RowGap;

                drawLabel("Hit arc degrees");
                value = action.hitArcDegrees;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_hit_arc_degrees",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackHitArcDegreesInput,
                        0.001f, 360.0f, 2);
                if (floatResult.changed) {
                    editor.SetSelectedAttackMotion(
                            action.advanceSpeedMultiplier,
                            action.aimTrackingEndPhase,
                            value);
                }
                y += RowHeight + RowGap;

                drawLabel("Damage");
                int integerValue = action.damage;
                auto intResult = engine::IntInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_damage",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        integerValue, state.attackDamageInput, 0, 1000000, 1);
                if (intResult.changed) {
                    editor.SetSelectedAttack(
                            action.hitPhase, action.rangeWorld, integerValue,
                            action.knockbackImpulseWorldPerSecond,
                            action.stunMilliseconds);
                }
                y += RowHeight + RowGap;

                drawLabel("Knockback impulse");
                value = action.knockbackImpulseWorldPerSecond;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_knockback",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackKnockbackInput, 0.0f, 100.0f, 3);
                if (floatResult.changed) {
                    editor.SetSelectedAttack(
                            action.hitPhase, action.rangeWorld, action.damage,
                            value, action.stunMilliseconds);
                }
                y += RowHeight + RowGap;

                drawLabel("Stun ms");
                integerValue = action.stunMilliseconds;
                intResult = engine::IntInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_stun",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        integerValue, state.attackStunMillisecondsInput,
                        0, 60000, 10);
                if (intResult.changed) {
                    editor.SetSelectedAttack(
                            action.hitPhase, action.rangeWorld, action.damage,
                            action.knockbackImpulseWorldPerSecond,
                            integerValue);
                }
                y += RowHeight + RowGap;

                engine::Text(
                        ui, config, assets,
                        Rectangle{fieldX, y, fieldW, RowHeight},
                        smallFont, "Player camera impact",
                        engine::UITextJustify::Left,
                        config.accentColor);
                y += RowHeight + RowGap;

                bool cameraEnabled = action.cameraImpact.enabled;
                if (engine::Checkbox(
                            ui, config, input, assets,
                            "sector_editor_npc_attack_camera_enabled",
                            Rectangle{fieldX, y, 260.0f, RowHeight},
                            font, "Enabled", cameraEnabled)) {
                    NpcAttackCameraImpactDefinition camera =
                            action.cameraImpact;
                    camera.enabled = cameraEnabled;
                    editor.SetSelectedAttackCameraImpact(camera);
                }
                y += RowHeight + RowGap;

                drawLabel("Pitch kick degrees");
                value = action.cameraImpact.pitchKickDegrees;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_camera_pitch",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackCameraPitchKickInput,
                        0.0f, kMaximumNpcAttackCameraImpactKickDegrees, 3);
                if (floatResult.changed) {
                    NpcAttackCameraImpactDefinition camera =
                            action.cameraImpact;
                    camera.pitchKickDegrees = value;
                    editor.SetSelectedAttackCameraImpact(camera);
                }
                y += RowHeight + RowGap;

                drawLabel("Roll kick degrees");
                value = action.cameraImpact.rollKickDegrees;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_camera_roll",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackCameraRollKickInput,
                        0.0f, kMaximumNpcAttackCameraImpactKickDegrees, 3);
                if (floatResult.changed) {
                    NpcAttackCameraImpactDefinition camera =
                            action.cameraImpact;
                    camera.rollKickDegrees = value;
                    editor.SetSelectedAttackCameraImpact(camera);
                }
                y += RowHeight + RowGap;

                drawLabel("Spring frequency Hz");
                value = action.cameraImpact.springFrequencyHz;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_camera_frequency",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackCameraSpringFrequencyInput,
                        kMinimumNpcAttackCameraImpactSpringFrequencyHz,
                        kMaximumNpcAttackCameraImpactSpringFrequencyHz, 3);
                if (floatResult.changed) {
                    NpcAttackCameraImpactDefinition camera =
                            action.cameraImpact;
                    camera.springFrequencyHz = value;
                    editor.SetSelectedAttackCameraImpact(camera);
                }
                y += RowHeight + RowGap;

                drawLabel("Spring damping ratio");
                value = action.cameraImpact.springDampingRatio;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_camera_damping",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackCameraSpringDampingInput,
                        kMinimumNpcAttackCameraImpactSpringDampingRatio,
                        kMaximumNpcAttackCameraImpactSpringDampingRatio, 3);
                if (floatResult.changed) {
                    NpcAttackCameraImpactDefinition camera =
                            action.cameraImpact;
                    camera.springDampingRatio = value;
                    editor.SetSelectedAttackCameraImpact(camera);
                }
                y += RowHeight + RowGap;

                drawLabel("Maximum pitch degrees");
                value = action.cameraImpact.maxPitchDegrees;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_camera_max_pitch",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackCameraMaxPitchInput,
                        0.0f, kMaximumNpcAttackCameraImpactLimitDegrees, 3);
                if (floatResult.changed) {
                    NpcAttackCameraImpactDefinition camera =
                            action.cameraImpact;
                    camera.maxPitchDegrees = value;
                    editor.SetSelectedAttackCameraImpact(camera);
                }
                y += RowHeight + RowGap;

                drawLabel("Maximum roll degrees");
                value = action.cameraImpact.maxRollDegrees;
                floatResult = engine::FloatInput(
                        ui, config, input, assets,
                        "sector_editor_npc_attack_camera_max_roll",
                        Rectangle{fieldX, y, 190.0f, RowHeight}, font,
                        value, state.attackCameraMaxRollInput,
                        0.0f, kMaximumNpcAttackCameraImpactLimitDegrees, 3);
                if (floatResult.changed) {
                    NpcAttackCameraImpactDefinition camera =
                            action.cameraImpact;
                    camera.maxRollDegrees = value;
                    editor.SetSelectedAttackCameraImpact(camera);
                }
                y += RowHeight + RowGap;
            }
        }
        engine::EndScrollArea(
                ui, config, input, formScroll, editor.Session().formScroll);
    }

    std::string message = state.validationMessage;
    if (message.empty()) message = state.warningMessage;
    if (message.empty() && !state.catalogErrors.empty()) {
        message = state.catalogErrors.front().path + ": "
                + state.catalogErrors.front().message;
    }
    engine::Text(
            config, assets,
            Rectangle{
                    layout.listPane.x,
                    layout.modal.y + layout.modal.height - 66.0f,
                    layout.saveButton.x - layout.listPane.x - 18.0f,
                    44.0f},
            smallFont,
            message.c_str(),
            engine::UITextJustify::Left,
            message.empty() ? config.mutedTextColor : config.invalidColor,
            true);

    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_save",
                layout.saveButton,
                font, "Save")) {
        if (editor.SaveAndClose(&assets)) {
            ConsumeRemainingInput(input);
            return SectorEditorNpcEditorModalResult::Saved;
        }
    }
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_cancel",
                layout.cancelButton,
                font, "Cancel")) {
        editor.Cancel(&assets);
        ConsumeRemainingInput(input);
        return SectorEditorNpcEditorModalResult::Cancelled;
    }
    ConsumeRemainingInput(input);
    return SectorEditorNpcEditorModalResult::None;
}

} // namespace game
