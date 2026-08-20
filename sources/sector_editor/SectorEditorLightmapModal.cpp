#include "sector_editor/SectorEditorLightmapModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <cmath>

namespace game {

SectorEditorLightmapBakeSetupModalResult DrawLightmapBakeSetupModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorLightmapBakeSetupModalState& state)
{
    if (!state.open) {
        return SectorEditorLightmapBakeSetupModalResult::None;
    }

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
            (EditorWidth - 660.0f) * 0.5f,
            (EditorHeight - 410.0f) * 0.5f,
            660.0f,
            410.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    const float left = modal.x + 28.0f;
    const float contentWidth = modal.width - 56.0f;
    float y = modal.y + 22.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentWidth, 38.0f},
            font,
            "Bake Lightmaps",
            engine::UITextJustify::Left);
    y += 50.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, 150.0f, 38.0f},
            font,
            "Quality preset",
            engine::UITextJustify::Left,
            config.mutedTextColor);

    static const std::vector<std::string> qualityOptions{
            "Draft", "Standard", "High"};
    int selectedIndex = static_cast<int>(
            NormalizeSectorLightmapBakeQualityPreset(state.selectedQuality));
    if (engine::Option(
                ui,
                config,
                input,
                assets,
                "sector_editor_lightmap_bake_quality",
                Rectangle{left + 170.0f, y, contentWidth - 170.0f, 40.0f},
                font,
                qualityOptions,
                selectedIndex)) {
        state.selectedQuality = static_cast<SectorLightmapBakeQualityPreset>(
                std::clamp(selectedIndex, 0, 2));
        state.errorMessage.clear();
    }
    y += 62.0f;

    const SectorLightmapBakeQualityParameters quality =
            ResolveSectorLightmapBakeQuality(state.selectedQuality);
    const char* relativeCost = "1x";
    switch (NormalizeSectorLightmapBakeQualityPreset(state.selectedQuality)) {
        case SectorLightmapBakeQualityPreset::Draft:
            relativeCost = "~0.125x";
            break;
        case SectorLightmapBakeQualityPreset::High:
            relativeCost = "~6x";
            break;
        case SectorLightmapBakeQualityPreset::Standard:
            break;
    }
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentWidth, 34.0f},
            font,
            TextFormat("Resolution: %.0f texels per world unit", quality.texelsPerWorldUnit),
            engine::UITextJustify::Left);
    y += 38.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentWidth, 34.0f},
            font,
            TextFormat(
                    "Samples: %d soft-shadow / %d AO / %d bounce",
                    quality.directSoftShadowSampleCount,
                    quality.ambientOcclusionSampleCount,
                    quality.indirectBounceSampleCount),
            engine::UITextJustify::Left);
    y += 38.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentWidth, 34.0f},
            font,
            TextFormat("Approximate ray work on the same map: %s", relativeCost),
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 44.0f;
    engine::Text(
            config,
            assets,
            Rectangle{left, y, contentWidth, 34.0f},
            font,
            "Atlases stay 2048 x 2048 RGBA16F (~32 MiB each).",
            engine::UITextJustify::Left,
            config.mutedTextColor);

    if (!state.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{left, modal.y + modal.height - 112.0f, contentWidth, 32.0f},
                font,
                state.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor);
    }

    const float buttonWidth = 140.0f;
    const float buttonHeight = 44.0f;
    const float buttonY = modal.y + modal.height - buttonHeight - 22.0f;
    const bool bakeRequested = engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_lightmap_bake_setup_confirm",
            Rectangle{modal.x + modal.width - buttonWidth * 2.0f - 40.0f, buttonY, buttonWidth, buttonHeight},
            font,
            "Bake");
    cancelRequested = cancelRequested || engine::Button(
            ui,
            config,
            input,
            assets,
            "sector_editor_lightmap_bake_setup_cancel",
            Rectangle{modal.x + modal.width - buttonWidth - 22.0f, buttonY, buttonWidth, buttonHeight},
            font,
            "Cancel");

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            });

    if (cancelRequested) {
        CloseSectorEditorLightmapBakeSetupModal(state);
        return SectorEditorLightmapBakeSetupModalResult::Cancelled;
    }
    return bakeRequested
            ? SectorEditorLightmapBakeSetupModalResult::BakeRequested
            : SectorEditorLightmapBakeSetupModalResult::None;
}

void DrawLightmapBakeModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const SectorEditorLightmapBakeModalView& view,
        const SectorEditorLightmapBakeModalCallbacks& callbacks)
{
    if (!view.blocking) {
        return;
    }

    const bool running = view.running;
    const SectorLightmapBakePhase phase = view.phase;
    const uint32_t completedWork = view.completedWork;
    const uint32_t totalWork = view.totalWork;
    const float progress = LightmapBakeOverallProgress(phase, completedWork, totalWork);
    const double now = GetTime();
    const double elapsed = (view.completedTimeSeconds > 0.0 ? view.completedTimeSeconds : now)
            - view.startTimeSeconds;

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});

    const Rectangle modal{
            (EditorWidth - 620.0f) * 0.5f,
            (EditorHeight - 300.0f) * 0.5f,
            620.0f,
            300.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 24.0f;
    engine::Text(config, assets, Rectangle{modal.x + 28.0f, y, modal.width - 56.0f, 42.0f}, font, "Baking lightmap", engine::UITextJustify::Left);
    y += 58.0f;

    const char* phaseText = view.awaitingAcknowledgement && !view.terminalMessage.empty()
            ? view.terminalMessage.c_str()
            : (view.cancelButtonPressed && running ? "Cancelling bake..." : LightmapBakePhaseText(phase));
    const Color phaseColor = view.awaitingAcknowledgement && !view.terminalCancelled
            ? config.invalidColor
            : config.textColor;
    engine::Text(config, assets, Rectangle{modal.x + 28.0f, y, modal.width - 56.0f, 38.0f}, font, phaseText, engine::UITextJustify::Left, phaseColor);
    y += 52.0f;

    const Rectangle track{modal.x + 28.0f, y, modal.width - 128.0f, 28.0f};
    DrawRectangleRec(track, config.widgetColor);
    DrawRectangleLinesEx(track, 1.0f, config.borderColor);
    const Rectangle fill{track.x, track.y, track.width * progress, track.height};
    DrawRectangleRec(fill, config.accentColor);
    engine::Text(
            config,
            assets,
            Rectangle{track.x + track.width + 14.0f, y - 4.0f, 72.0f, 36.0f},
            font,
            TextFormat("%d%%", static_cast<int>(std::round(progress * 100.0f))),
            engine::UITextJustify::Right
    );
    y += 56.0f;

    engine::Text(
            config,
            assets,
            Rectangle{modal.x + 28.0f, y, modal.width - 56.0f, 38.0f},
            font,
            TextFormat("Elapsed: %.1fs", std::max(0.0, elapsed)),
            engine::UITextJustify::Left,
            config.mutedTextColor
    );

    const float buttonW = 150.0f;
    const float buttonH = 44.0f;
    const Rectangle button{modal.x + modal.width - buttonW - 28.0f, modal.y + modal.height - buttonH - 24.0f, buttonW, buttonH};
    if (running) {
        if (view.cancelButtonPressed) {
            DrawRectangleRec(button, config.disabledColor);
            DrawRectangleLinesEx(button, config.borderThickness, config.borderColor);
            engine::Text(config, assets, button, font, "Cancel", engine::UITextJustify::Center, config.mutedTextColor);
        } else if (engine::Button(ui, config, input, assets, "sector_editor_lightmap_bake_cancel", button, font, "Cancel")) {
            callbacks.requestCancel();
        }
    } else if (view.awaitingAcknowledgement) {
        if (engine::Button(ui, config, input, assets, "sector_editor_lightmap_bake_close", button, font, "Close")) {
            callbacks.closeAcknowledgement();
        }
    }

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            }
    );
}

} // namespace game
