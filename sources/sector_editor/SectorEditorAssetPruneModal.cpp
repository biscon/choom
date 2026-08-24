#include "sector_editor/SectorEditorAssetPruneModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

namespace game {

SectorEditorAssetPruneModalResult DrawSectorEditorAssetPruneModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorAssetPruneModalState& state)
{
    if (!state.open) return SectorEditorAssetPruneModalResult::None;

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

    DrawRectangle(
            0,
            0,
            static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight),
            Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 620.0f) * 0.5f,
            (EditorHeight - 330.0f) * 0.5f,
            620.0f,
            330.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    const float left = modal.x + 28.0f;
    const float contentWidth = modal.width - 56.0f;
    const float rowHeight = 42.0f;
    float y = modal.y + 22.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentWidth, rowHeight},
            font,
            "Prune Assets",
            engine::UITextJustify::Left);
    y += 52.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentWidth, 34.0f},
            font,
            "Remove map-local sounds that have no level references.",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 38.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentWidth, 34.0f},
            font,
            "Global materials are managed by the Material Editor.",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 44.0f;

    engine::Checkbox(
            ui, config, input, assets,
            "sector_editor_prune_sounds",
            Rectangle{left, y, contentWidth, rowHeight},
            font, "Sounds", state.pruneSounds);

    const float buttonWidth = 140.0f;
    const float buttonHeight = 44.0f;
    const float buttonY = modal.y + modal.height - buttonHeight - 22.0f;
    const Rectangle okayBounds{
            modal.x + modal.width - buttonWidth * 2.0f - 40.0f,
            buttonY,
            buttonWidth,
            buttonHeight};
    bool okayRequested = false;
    if (state.pruneSounds) {
        okayRequested = engine::Button(
                ui, config, input, assets,
                "sector_editor_prune_assets_okay",
                okayBounds,
                font,
                "Okay");
    } else {
        DrawRectangleRec(okayBounds, config.disabledColor);
        DrawRectangleLinesEx(okayBounds, config.borderThickness, config.borderColor);
        engine::Text(
                config,
                assets,
                okayBounds,
                font,
                "Okay",
                engine::UITextJustify::Center,
                config.mutedTextColor);
    }
    cancelRequested = cancelRequested || engine::Button(
            ui, config, input, assets,
            "sector_editor_prune_assets_cancel",
            Rectangle{
                    modal.x + modal.width - buttonWidth - 22.0f,
                    buttonY,
                    buttonWidth,
                    buttonHeight},
            font,
            "Cancel");

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });

    if (cancelRequested) {
        CloseSectorEditorAssetPruneModal(state);
        return SectorEditorAssetPruneModalResult::Cancelled;
    }
    return okayRequested
            ? SectorEditorAssetPruneModalResult::Confirmed
            : SectorEditorAssetPruneModalResult::None;
}

} // namespace game
