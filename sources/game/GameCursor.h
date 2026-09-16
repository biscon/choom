#pragma once

#include "engine/assets/AssetHandles.h"
#include "game/ApplicationFlow.h"

#include <raylib.h>

namespace engine { class AssetManager; }

namespace game {

enum class GameCursorMode { Native, Hidden, Arrow };

struct GameCursorUiState {
    bool inventoryOpen = false;
    bool heldItemActive = false;
    bool dialogueChoicesActive = false;
    bool keypadActive = false;
    bool gameOver = false;
};

inline GameCursorMode ResolveGameCursorMode(
        const ApplicationFlowState& flow,
        bool loadingOverlayVisible,
        const GameCursorUiState& ui)
{
    if (flow.screen == ApplicationScreen::Editor
            || (flow.screen == ApplicationScreen::MainMenu
                    && flow.menuReturnScreen == ApplicationScreen::Editor)) {
        return GameCursorMode::Native;
    }
    if (loadingOverlayVisible) return GameCursorMode::Hidden;
    if (flow.screen == ApplicationScreen::MainMenu
            || ui.inventoryOpen || ui.heldItemActive
            || ui.dialogueChoicesActive || ui.keypadActive || ui.gameOver) {
        return GameCursorMode::Arrow;
    }
    return GameCursorMode::Hidden;
}

struct GameCursorAssets {
    engine::TextureHandle arrow{};
    engine::TextureHandle interactionHand{};
};

struct GameCursorSprite {
    Rectangle source{};
    // Hotspot in source-image coordinates, including source.x/y.
    Vector2 hotspot{};
};

// Preserve the original generated PNGs; trim transparent margins at draw time.
inline constexpr GameCursorSprite GameArrowCursor{{256, 54, 727, 1139}, {264, 71}};
inline constexpr GameCursorSprite GameInteractionCursor{{210, 51, 819, 1120}, {536, 109}};
inline constexpr float GameCursorLogicalHeight = 40.0f;

inline Rectangle BuildGameCursorDestination(
        const GameCursorSprite& sprite,
        Vector2 logicalMouse,
        Vector2 logicalSize,
        Rectangle viewport)
{
    if (logicalSize.x <= 0 || logicalSize.y <= 0
            || viewport.width <= 0 || viewport.height <= 0
            || sprite.source.width <= 0 || sprite.source.height <= 0) return {};
    const float scaleX = viewport.width / logicalSize.x;
    const float scaleY = viewport.height / logicalSize.y;
    const float spriteScale = GameCursorLogicalHeight / sprite.source.height;
    // Main's SetMouseOffset uses integer viewport offsets. Invert that exact
    // transform so fractional letterbox margins do not shift the cursor tip.
    const Vector2 mouse{
            static_cast<float>(static_cast<int>(viewport.x)) + logicalMouse.x * scaleX,
            static_cast<float>(static_cast<int>(viewport.y)) + logicalMouse.y * scaleY};
    return {
            mouse.x - (sprite.hotspot.x - sprite.source.x) * spriteScale * scaleX,
            mouse.y - (sprite.hotspot.y - sprite.source.y) * spriteScale * scaleY,
            sprite.source.width * spriteScale * scaleX,
            GameCursorLogicalHeight * scaleY};
}

void LoadGameCursorAssets(engine::AssetManager& assets, GameCursorAssets& cursors);
void DrawGameCursor(
        engine::AssetManager& assets,
        const GameCursorAssets& cursors,
        Vector2 logicalMouse,
        Vector2 logicalSize,
        Rectangle viewport);

} // namespace game
