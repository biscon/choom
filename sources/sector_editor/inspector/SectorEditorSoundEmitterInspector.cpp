#include "sector_editor/inspector/SectorEditorSoundEmitterInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <cstdio>

namespace game {

float MeasureSectorEditorSoundEmitterInspectorContentHeight(
        const SoundEmitterEditingUiState& uiState, float rowHeight, float gap)
{
    return 38.0f + (rowHeight + gap) * 8.0f
            + (uiState.referenceIdError.empty() ? 0.0f : 36.0f);
}

bool DrawSectorEditorSoundEmitterInspector(
        engine::UIContext& ui, const engine::UIConfig& config,
        engine::Input& input, engine::AssetManager& assets,
        engine::FontHandle font, float contentWidth, float rowHeight, float gap,
        const SectorAuthoringSoundEmitter& emitter,
        SoundEmitterEditingUiState& uiState,
        SectorEditorSoundEmitterEditingService& editing)
{
    if (uiState.bufferedEmitterId != emitter.id) {
        std::snprintf(uiState.referenceIdBuffer, sizeof(uiState.referenceIdBuffer),
                "%s", emitter.referenceId.c_str());
        std::snprintf(uiState.soundIdBuffer, sizeof(uiState.soundIdBuffer),
                "%s", emitter.soundId.c_str());
        uiState.bufferedEmitterId = emitter.id;
        uiState.referenceIdError.clear();
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

    const engine::UITextInputResult soundResult = textRow(
            "sector_editor_sound_emitter_sound", "Sound ID",
            uiState.soundIdBuffer, sizeof(uiState.soundIdBuffer));
    if (soundResult.submitted) editing.SetSelectedSoundId(uiState.soundIdBuffer);

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
