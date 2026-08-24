#include "sector_editor/SectorEditorMaterialModals.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"

namespace game {

void DrawSectorEditorDecalTintModal(SectorEditorDecalTintModalContext& context)
{
    DecalTintModalState& modalState = context.modalState;
    if (!modalState.open) {
        return;
    }

    engine::UIContext& ui = context.ui;
    const engine::UIConfig& config = context.config;
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    const engine::FontHandle font = context.font;

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
            }
    );

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 560.0f) * 0.5f,
            (EditorHeight - 390.0f) * 0.5f,
            560.0f,
            390.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 22.0f;
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, y, modal.width - 52.0f, 42.0f}, font, "Decal Tint");
    y += 58.0f;

    const float labelW = 72.0f;
    const float inputW = 120.0f;
    const float inputH = 38.0f;
    const float gap = 12.0f;
    auto drawFloat = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState) {
        const SectorEditorTintFloatInputResult result = DrawNormalizedTintFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{modal.x + 28.0f, y, labelW, inputH},
                Rectangle{modal.x + 28.0f + labelW, y, inputW, inputH},
                engine::UITextJustify::Left,
                value,
                inputState);
        if (result.finite) {
            value = result.value;
        }
        if (result.changed) {
            if (!result.finite) {
                modalState.errorMessage = "Tint values must be finite.";
            } else {
                modalState.errorMessage.clear();
            }
        }
        y += inputH + gap;
    };

    drawFloat("sector_editor_decal_tint_r", "R", modalState.tint.x, modalState.redInput);
    drawFloat("sector_editor_decal_tint_g", "G", modalState.tint.y, modalState.greenInput);
    drawFloat("sector_editor_decal_tint_b", "B", modalState.tint.z, modalState.blueInput);

    const Rectangle swatch{modal.x + 270.0f, modal.y + 88.0f, 210.0f, 124.0f};
    DrawColorSwatch(config, swatch, DecalTintPreviewColor(modalState.tint), config.borderThickness);
    engine::Text(
            config,
            assets,
            Rectangle{swatch.x, swatch.y + swatch.height + 10.0f, swatch.width, 26.0f},
            font,
            "Preview",
            engine::UITextJustify::Center,
            config.mutedTextColor);

    if (!modalState.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 28.0f, modal.y + 250.0f, modal.width - 56.0f, 34.0f},
                font,
                modalState.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor);
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 124.0f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_decal_tint_reset",
                Rectangle{modal.x + 28.0f, buttonY, buttonW, 44.0f},
                font,
                "Reset White")) {
        modalState.tint = Vector3{1.0f, 1.0f, 1.0f};
        modalState.redInput = engine::UIFloatInputState{};
        modalState.greenInput = engine::UIFloatInputState{};
        modalState.blueInput = engine::UIFloatInputState{};
        modalState.errorMessage.clear();
    }
    okayRequested = okayRequested || engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_decal_tint_ok",
            Rectangle{modal.x + modal.width - buttonW * 2.0f - 38.0f, buttonY, buttonW, 44.0f},
            font,
            "OK");
    cancelRequested = cancelRequested || engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_decal_tint_cancel",
            Rectangle{modal.x + modal.width - buttonW - 26.0f, buttonY, buttonW, 44.0f},
            font,
            "Cancel");

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (cancelRequested) {
        if (context.callbacks.close) {
            context.callbacks.close();
        }
        return;
    }
    if (!okayRequested) {
        return;
    }
    if (!IsValidDecalTint(modalState.tint)) {
        modalState.errorMessage = "Tint values must be between 0 and 1.";
        context.statusText = modalState.errorMessage;
        return;
    }

    const TopologySurfaceEditTarget target = modalState.target;
    const SectorTopologyDecalLayer* decal = context.callbacks.decalForTarget
            ? context.callbacks.decalForTarget(target)
            : nullptr;
    const bool targetValid = context.callbacks.isTargetValid
            && context.callbacks.isTargetValid(target);
    if (!targetValid || decal == nullptr || decal->materialId.empty()) {
        modalState.errorMessage = "Decal target is no longer valid.";
        context.statusText = modalState.errorMessage;
        return;
    }

    const Vector3 tint = modalState.tint;
    const bool changed = !SameTint(decal->tint, tint);
    if (changed && (!context.callbacks.applyTint || !context.callbacks.applyTint(target, tint))) {
        modalState.errorMessage = context.statusText.empty() ? "Could not set decal tint." : context.statusText;
        return;
    }
    if (context.callbacks.close) {
        context.callbacks.close();
    }
}

} // namespace game
