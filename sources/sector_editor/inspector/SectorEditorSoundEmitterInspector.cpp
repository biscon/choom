#include "sector_editor/inspector/SectorEditorSoundEmitterInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/sounds/SectorEditorSoundService.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <cstdio>
#include <utility>

namespace game {

float MeasureSectorEditorSoundEmitterInspectorContentHeight(
        const SoundEmitterEditingUiState& uiState, float rowHeight, float gap)
{
    return 38.0f + (rowHeight + gap) * 8.0f
            + (uiState.referenceIdError.empty() ? 0.0f : 36.0f)
            + (uiState.soundIdError.empty() ? 0.0f : 36.0f);
}

bool DrawSectorEditorSoundEmitterInspector(
        engine::UIContext& ui, const engine::UIConfig& config,
        engine::Input& input, engine::AssetManager& assets,
        engine::FontHandle font, float contentWidth, float rowHeight, float gap,
        const SectorAuthoringSoundEmitter& sourceEmitter,
        SoundEmitterEditingUiState& uiState,
        SectorEditorSoundEmitterEditingService& editing,
        SectorEditorSoundService& sounds)
{
    // Editing may refresh derivation and replace authoring storage mid-draw.
    // Keep this frame's displayed values independent of that storage lifetime.
    const SectorAuthoringSoundEmitter emitter = sourceEmitter;
    if (uiState.bufferedEmitterId != emitter.id
            || uiState.bufferedSoundId != emitter.soundId) {
        std::snprintf(uiState.referenceIdBuffer, sizeof(uiState.referenceIdBuffer),
                "%s", emitter.referenceId.c_str());
        std::snprintf(uiState.soundIdBuffer, sizeof(uiState.soundIdBuffer),
                "%s", emitter.soundId.c_str());
        uiState.bufferedEmitterId = emitter.id;
        uiState.bufferedSoundId = emitter.soundId;
        uiState.referenceIdError.clear();
        uiState.soundIdError.clear();
        uiState.xInput = {}; uiState.yInput = {}; uiState.zInput = {}; uiState.volumeInput = {};
    }

    float y = 0.0f;
    engine::Text(ui, config, assets, {0.0f, y, contentWidth, 34.0f}, font,
            TextFormat("Sound Emitter: %d", emitter.id),
            engine::UITextJustify::Left, config.textColor);
    y += 38.0f;
    constexpr float LabelWidth = 92.0f;
    const auto textRow = [&](const char* controlId, const char* label,
                             char* buffer, std::size_t capacity) {
        engine::Text(ui, config, assets, {0.0f, y, LabelWidth, rowHeight}, font,
                label, engine::UITextJustify::Left, config.mutedTextColor);
        const engine::UITextInputResult result = engine::TextInput(
                ui, config, input, assets, controlId,
                {LabelWidth, y, contentWidth - LabelWidth, rowHeight}, font,
                buffer, capacity, 0, capacity - 1, engine::UITextJustify::Left);
        y += rowHeight + gap;
        return result;
    };

    const engine::UITextInputResult idResult = textRow(
            "sector_editor_sound_emitter_id", "ID",
            uiState.referenceIdBuffer, sizeof(uiState.referenceIdBuffer));
    if (idResult.submitted) {
        const std::string requested{uiState.referenceIdBuffer};
        if (editing.ValidateSelectedReferenceId(requested, uiState.referenceIdError)
                && (requested == emitter.referenceId || editing.RenameSelected(requested))) {
            uiState.referenceIdError.clear();
        }
    }
    if (!uiState.referenceIdError.empty()) {
        engine::Text(ui, config, assets, {0.0f, y, contentWidth, 34.0f}, font,
                uiState.referenceIdError.c_str(), engine::UITextJustify::Left,
                config.invalidColor, true);
        y += 36.0f;
    }

    engine::Text(ui, config, assets, {0.0f, y, LabelWidth, rowHeight}, font,
            "Sound ID", engine::UITextJustify::Left, config.mutedTextColor);
    constexpr float PickButtonWidth = 76.0f;
    const engine::UITextInputResult soundResult = engine::TextInput(
            ui, config, input, assets, "sector_editor_sound_emitter_sound",
            {LabelWidth, y,
                    contentWidth - LabelWidth - PickButtonWidth - gap, rowHeight},
            font, uiState.soundIdBuffer, sizeof(uiState.soundIdBuffer), 0,
            sizeof(uiState.soundIdBuffer) - 1, engine::UITextJustify::Left);
    if (engine::Button(ui, config, input, assets,
                "sector_editor_sound_emitter_pick_sound",
                {contentWidth - PickButtonWidth, y, PickButtonWidth, rowHeight},
                font, "Pick")) {
        sounds.OpenSoundEmitterPicker(emitter.id);
    }
    y += rowHeight + gap;

    const auto restoreSoundIdBuffer = [&]() {
        std::snprintf(uiState.soundIdBuffer, sizeof(uiState.soundIdBuffer),
                "%s", emitter.soundId.c_str());
        uiState.bufferedSoundId = emitter.soundId;
    };
    if (soundResult.cancelled) {
        restoreSoundIdBuffer();
        uiState.soundIdError.clear();
    } else {
        const std::string requested{uiState.soundIdBuffer};
        std::string error;
        const bool valid = editing.ValidateSelectedSoundId(requested, error);
        bool updateFailed = false;
        if (soundResult.changed && valid) {
            if (requested == emitter.soundId) {
                uiState.bufferedSoundId = requested;
                uiState.soundIdError.clear();
            } else {
                editing.SetSelectedSoundId(requested);
                const SectorAuthoringSoundEmitter* selected = editing.Selected();
                if (selected != nullptr && selected->soundId == requested) {
                    uiState.bufferedSoundId = requested;
                    uiState.soundIdError.clear();
                } else {
                    uiState.soundIdError = "Sound ID could not be updated";
                    restoreSoundIdBuffer();
                    updateFailed = true;
                }
            }
        }
        if ((soundResult.submitted || soundResult.focusLost) && !valid) {
            uiState.soundIdError = std::move(error);
            restoreSoundIdBuffer();
        } else if ((soundResult.submitted || soundResult.focusLost)
                && valid && !updateFailed) {
            uiState.soundIdError.clear();
        }
    }
    if (!uiState.soundIdError.empty()) {
        engine::Text(ui, config, assets, {0.0f, y, contentWidth, 34.0f}, font,
                uiState.soundIdError.c_str(), engine::UITextJustify::Left,
                config.invalidColor, true);
        y += 36.0f;
    }

    const auto positionRow = [&](const char* controlId, const char* label,
                                 float current, engine::UIFloatInputState& state, int axis) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentWidth, rowHeight, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui, config, input, assets, font, controlId, label,
                layout.labelRect, layout.inputRect, engine::UITextJustify::Right,
                current, state, -8192.0f, 8192.0f, 3);
        if (result.changed && result.finite && result.value != current) {
            Vector3 position{SectorCoordToVisibleAuthoring(emitter.x), emitter.y,
                    SectorCoordToVisibleAuthoring(emitter.z)};
            if (axis == 0) position.x = result.value;
            if (axis == 1) position.y = result.value;
            if (axis == 2) position.z = result.value;
            editing.SetSelectedPosition(position);
        }
        y += rowHeight + gap;
    };
    positionRow("sector_editor_sound_emitter_x", "X",
            SectorCoordToVisibleAuthoring(emitter.x), uiState.xInput, 0);
    positionRow("sector_editor_sound_emitter_y", "Y", emitter.y, uiState.yInput, 1);
    positionRow("sector_editor_sound_emitter_z", "Z",
            SectorCoordToVisibleAuthoring(emitter.z), uiState.zInput, 2);

    const SectorEditorInspectorNumericRowLayout volumeLayout =
            BuildSectorEditorInspectorRightFloatRowLayout(y, contentWidth, rowHeight, gap);
    const SectorEditorFloatInputResult volumeResult = DrawLabeledFloatInput(
            ui, config, input, assets, font, "sector_editor_sound_emitter_volume", "Volume",
            volumeLayout.labelRect, volumeLayout.inputRect, engine::UITextJustify::Right,
            emitter.volume, uiState.volumeInput, 0.0f, 1.0f, 2);
    if (volumeResult.changed && volumeResult.finite) editing.SetSelectedVolume(volumeResult.value);
    y += rowHeight + gap;

    bool loop = emitter.loop;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_sound_emitter_loop",
                {0.0f, y, contentWidth, rowHeight}, font, "Loop", loop)) {
        editing.SetSelectedLoop(loop);
    }
    y += rowHeight + gap;

    return engine::Button(ui, config, input, assets, "sector_editor_sound_emitter_delete",
            {0.0f, y, contentWidth, rowHeight}, font, "Delete Sound Emitter");
}

} // namespace game
