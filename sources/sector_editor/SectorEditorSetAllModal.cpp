#include "sector_editor/SectorEditorSetAllModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"

namespace game {

void DrawSectorEditorSetAllModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorSetAllModalState& modalState,
        const SectorEditorSetAllModalCallbacks& callbacks)
{
    if (!modalState.open) {
        return;
    }

    bool okayRequested = false;
    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                }
            });

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 640.0f) * 0.5f,
            (EditorHeight - 500.0f) * 0.5f,
            640.0f,
            500.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    const float left = modal.x + 26.0f;
    const float contentW = modal.width - 52.0f;
    const float rowH = 42.0f;
    const float gap = config.rowSpacing;
    const float labelW = 170.0f;
    float y = modal.y + 22.0f;

    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentW, 42.0f},
            font,
            "Set All");
    y += 58.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentW, 34.0f},
            font,
            "Sector Lighting",
            engine::UITextJustify::Left,
            config.textColor);
    y += 40.0f;

    const SectorEditorFloatInputResult intensityResult = DrawLabeledFloatInput(
            ui,
            config,
            input,
            assets,
            font,
            "sector_editor_set_all_ambient_intensity",
            "Intensity:",
            Rectangle{left, y, labelW, rowH},
            Rectangle{left + labelW, y, contentW - labelW, rowH},
            engine::UITextJustify::Right,
            ClampAmbientIntensity(modalState.ambientIntensity),
            modalState.ambientIntensityInput,
            0.0f,
            1.0f,
            3);
    modalState.ambientIntensity = intensityResult.value;
    y += rowH + gap;

    const auto drawChannel = [&](const char* id,
                                 const char* label,
                                 unsigned char& channel,
                                 engine::UIIntInputState& inputState) {
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{left, y, labelW, rowH},
                Rectangle{left + labelW, y, contentW - labelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        channel = result.channel;
        y += rowH + gap;
    };
    drawChannel(
            "sector_editor_set_all_ambient_red",
            "R:",
            modalState.ambientColor.r,
            modalState.ambientRedInput);
    drawChannel(
            "sector_editor_set_all_ambient_green",
            "G:",
            modalState.ambientColor.g,
            modalState.ambientGreenInput);
    drawChannel(
            "sector_editor_set_all_ambient_blue",
            "B:",
            modalState.ambientColor.b,
            modalState.ambientBlueInput);
    modalState.ambientColor.a = 255;

    const float buttonY = modal.y + modal.height - 68.0f;
    const float buttonW = 150.0f;
    okayRequested = engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_set_all_okay",
            Rectangle{modal.x + modal.width - buttonW * 2.0f - 38.0f, buttonY, buttonW, 44.0f},
            font,
            "Okay");
    cancelRequested = cancelRequested || engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_set_all_cancel",
            Rectangle{modal.x + modal.width - buttonW - 26.0f, buttonY, buttonW, 44.0f},
            font,
            "Cancel");

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (cancelRequested) {
        if (callbacks.close) {
            callbacks.close();
        }
        return;
    }
    if (okayRequested && callbacks.applySectorLighting) {
        callbacks.applySectorLighting(
                ClampAmbientIntensity(modalState.ambientIntensity),
                modalState.ambientColor);
    }
}

} // namespace game
