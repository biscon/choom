#include "sector_editor/SectorEditorColorSettingsModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

namespace game {

SectorEditorColorSettingsModalAction DrawSectorEditorColorSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorColorSettingsModalState& state)
{
    if (!state.open) {
        return SectorEditorColorSettingsModalAction::None;
    }

    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                cancelRequested = true;
                engine::ConsumeEvent(event);
            });

    DrawRectangle(
            0,
            0,
            static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight),
            Color{0, 0, 0, 150});
    const Rectangle modal{
            (EditorWidth - 720.0f) * 0.5f,
            (EditorHeight - 470.0f) * 0.5f,
            720.0f,
            470.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 252});
    DrawRectangleLinesEx(
            modal, config.borderThickness, config.borderColor);

    constexpr float Padding = 34.0f;
    constexpr float RowHeight = 44.0f;
    constexpr float LabelWidth = 270.0f;
    const float controlX = modal.x + Padding + LabelWidth;
    const float controlWidth = modal.width - Padding * 2.0f - LabelWidth;
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + Padding, modal.y + 22.0f,
                    modal.width - Padding * 2.0f, 44.0f},
            font,
            "Color Settings");
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + Padding, modal.y + 74.0f,
                    modal.width - Padding * 2.0f, 58.0f},
            smallFont,
            "Project authoring settings used by editor preview and gameplay. "
            "Players cannot change these from the game settings menu.",
            engine::UITextJustify::Left,
            config.mutedTextColor,
            true);

    float y = modal.y + 154.0f;
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + Padding, y, LabelWidth, RowHeight},
            smallFont,
            "Tone mapper");
    const char* toneMapperOptions[] = {
            "Khronos PBR Neutral",
            "ACES Filmic (fitted)"};
    int selectedToneMapper = state.draft.toneMapper
                    == engine::ToneMappingOperator::AcesFilmicFitted
            ? 1
            : 0;
    if (engine::Option(
                ui,
                config,
                input,
                assets,
                "sector_editor_color_tone_mapper",
                Rectangle{controlX, y, controlWidth, RowHeight},
                smallFont,
                toneMapperOptions,
                2,
                selectedToneMapper)) {
        state.draft.toneMapper = selectedToneMapper == 1
                ? engine::ToneMappingOperator::AcesFilmicFitted
                : engine::ToneMappingOperator::KhronosPbrNeutral;
        state.errorMessage.clear();
    }
    y += RowHeight + 18.0f;

    engine::Text(
            config,
            assets,
            Rectangle{modal.x + Padding, y, LabelWidth, RowHeight},
            smallFont,
            "Exposure compensation (EV)");
    const engine::UINumericInputResult exposureResult = engine::FloatInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_color_exposure_ev",
            Rectangle{controlX, y, controlWidth, RowHeight},
            smallFont,
            state.draft.exposureCompensationEv,
            state.exposureInput,
            engine::MinimumToneMappingExposureEv,
            engine::MaximumToneMappingExposureEv,
            2);
    if (exposureResult.changed) {
        state.errorMessage.clear();
    }
    y += RowHeight + 10.0f;
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + Padding, y,
                    modal.width - Padding * 2.0f, 46.0f},
            smallFont,
            "0 EV preserves the authored exposure; each +1 EV doubles the "
            "linear scene value before tone mapping.",
            engine::UITextJustify::Left,
            config.mutedTextColor,
            true);

    if (!state.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + Padding, modal.y + modal.height - 112.0f,
                        modal.width - Padding * 2.0f, 34.0f},
                smallFont,
                state.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor,
                true);
    }

    const float buttonY = modal.y + modal.height - 64.0f;
    constexpr float ButtonWidth = 136.0f;
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_color_defaults",
                Rectangle{modal.x + Padding, buttonY, 176.0f, 42.0f},
                smallFont,
                "Reset Defaults")) {
        state.draft = engine::ToneMappingSettings{};
        state.exposureInput = {};
        state.errorMessage.clear();
    }
    cancelRequested = cancelRequested || engine::Button(
            ui, config, input, assets,
            "sector_editor_color_cancel",
            Rectangle{modal.x + modal.width - Padding - ButtonWidth * 2.0f
                            - 12.0f,
                    buttonY, ButtonWidth, 42.0f},
            smallFont,
            "Cancel");
    const bool applyRequested = engine::Button(
            ui, config, input, assets,
            "sector_editor_color_apply",
            Rectangle{modal.x + modal.width - Padding - ButtonWidth,
                    buttonY, ButtonWidth, 42.0f},
            smallFont,
            "Apply");

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
    if (cancelRequested) {
        return SectorEditorColorSettingsModalAction::Cancel;
    }
    return applyRequested
            ? SectorEditorColorSettingsModalAction::Apply
            : SectorEditorColorSettingsModalAction::None;
}

} // namespace game
