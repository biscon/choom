#include "sector_editor/SectorEditorLightmapModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <cmath>

namespace game {

void DrawLightmapBakeModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        LightmapBakeAsyncState& lightmapBake,
        const SectorEditorLightmapBakeModalCallbacks& callbacks)
{
    if (!callbacks.isBlocking()) {
        return;
    }

    const bool running = lightmapBake.progress.running.load();
    const SectorLightmapBakePhase phase = lightmapBake.progress.phase.load();
    const uint32_t completedWork = lightmapBake.progress.completedWork.load();
    const uint32_t totalWork = lightmapBake.progress.totalWork.load();
    const float progress = LightmapBakeOverallProgress(phase, completedWork, totalWork);
    const double now = GetTime();
    const double elapsed = (lightmapBake.completedTimeSeconds > 0.0 ? lightmapBake.completedTimeSeconds : now)
            - lightmapBake.startTimeSeconds;

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

    const char* phaseText = lightmapBake.awaitingAcknowledgement && !lightmapBake.terminalMessage.empty()
            ? lightmapBake.terminalMessage.c_str()
            : (lightmapBake.cancelButtonPressed && running ? "Cancelling bake..." : LightmapBakePhaseText(phase));
    const Color phaseColor = lightmapBake.awaitingAcknowledgement && !lightmapBake.terminalCancelled
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
        if (lightmapBake.cancelButtonPressed) {
            DrawRectangleRec(button, config.disabledColor);
            DrawRectangleLinesEx(button, config.borderThickness, config.borderColor);
            engine::Text(config, assets, button, font, "Cancel", engine::UITextJustify::Center, config.mutedTextColor);
        } else if (engine::Button(ui, config, input, assets, "sector_editor_lightmap_bake_cancel", button, font, "Cancel")) {
            callbacks.requestCancel();
        }
    } else if (lightmapBake.awaitingAcknowledgement) {
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
