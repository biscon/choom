#pragma once

#include "engine/assets/AssetHandles.h"
#include "game/ApplicationFlow.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace engine { class AssetManager; }

namespace game {

enum class GameCursorMode { Native, Hidden, Arrow };

struct GameCursorPositionState {
    Vector2 logicalPosition{};
    GameCursorMode previousMode = GameCursorMode::Hidden;
    bool hasPosition = false;
    bool restorePending = false;
};

// Called before UI/game updates can change capture and recenter the mouse.
inline void RememberGameCursorPosition(
        GameCursorPositionState& state, GameCursorMode mode,
        Vector2 logicalPosition, bool focused, bool onScreen)
{
    if (mode != GameCursorMode::Arrow) return;
    if (!focused) {
        state.restorePending = state.hasPosition;
        return;
    }
    if (!onScreen || state.restorePending) return;
    state.logicalPosition = logicalPosition;
    state.hasPosition = true;
}

inline bool RequestGameCursorPositionRestore(
        GameCursorPositionState& state, GameCursorMode mode,
        bool nativeCursorVisible, bool focused)
{
    if (mode != GameCursorMode::Arrow) {
        state.previousMode = mode;
        state.restorePending = false;
        return false;
    }
    // EnableCursor recenters even when capture was already released. Native
    // visibility detects those calls even if the arrow never disappeared.
    if (state.hasPosition && (state.previousMode != mode
            || nativeCursorVisible || !focused)) state.restorePending = true;
    state.previousMode = mode;
    return focused && state.hasPosition && state.restorePending;
}

inline Vector2 GameCursorWindowPosition(
        Vector2 logicalMouse, Vector2 logicalSize, Rectangle viewport)
{
    // Invert Main's integer SetMouseOffset and logical SetMouseScale exactly.
    return {
            static_cast<float>(static_cast<int>(viewport.x)) + logicalMouse.x * viewport.width / logicalSize.x,
            static_cast<float>(static_cast<int>(viewport.y)) + logicalMouse.y * viewport.height / logicalSize.y};
}

inline bool BuildGameCursorRestorePosition(
        Vector2 logicalMouse, Vector2 logicalSize, Rectangle viewport,
        Vector2 windowSize, Vector2& windowPosition)
{
    if (logicalSize.x <= 0 || logicalSize.y <= 0
            || viewport.width <= 0 || viewport.height <= 0
            || windowSize.x < 1 || windowSize.y < 1) return false;
    const Vector2 position = GameCursorWindowPosition(logicalMouse, logicalSize, viewport);
    windowPosition = {
            std::clamp(std::round(position.x), 0.0f, windowSize.x - 1),
            std::clamp(std::round(position.y), 0.0f, windowSize.y - 1)};
    return true;
}

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
    const Vector2 mouse = GameCursorWindowPosition(logicalMouse, logicalSize, viewport);
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
