#include "game/GameCursor.h"

#include "engine/assets/AssetManager.h"

namespace game {

void LoadGameCursorAssets(engine::AssetManager& assets, GameCursorAssets& cursors)
{
    const auto flags = engine::TextureLoad_Mipmaps | engine::TextureLoad_TrilinearFilter;
    cursors.arrow = assets.RequestTexture(
            assets.GlobalScope(), "game_cursor_arrow", ASSETS_PATH "ui/cursors/arrow.png",
            engine::TextureColorUsage::DisplaySrgb, flags);
    cursors.interactionHand = assets.RequestTexture(
            assets.GlobalScope(), "game_cursor_interaction_hand", ASSETS_PATH "ui/cursors/interaction_hand.png",
            engine::TextureColorUsage::DisplaySrgb, flags);
}

void DrawGameCursor(
        engine::AssetManager& assets,
        const GameCursorAssets& cursors,
        Vector2 logicalMouse,
        Vector2 logicalSize,
        Rectangle viewport)
{
    const Rectangle destination = BuildGameCursorDestination(
            GameArrowCursor, logicalMouse, logicalSize, viewport);
    if (destination.width <= 0 || destination.height <= 0) return;
    if (const Texture2D* texture = assets.GetTexture(cursors.arrow)) {
        DrawTexturePro(*texture, GameArrowCursor.source, destination, {}, 0, WHITE);
        return;
    }

    // Missing/pending assets still leave mouse-operated menus usable.
    const GameCursorSprite fallback{{0, 0, 26, 40}, {0, 0}};
    const Rectangle bounds = BuildGameCursorDestination(
            fallback, logicalMouse, logicalSize, viewport);
    const auto point = [&bounds](float x, float y) {
        return Vector2{bounds.x + x * bounds.width / 26,
                bounds.y + y * bounds.height / 40};
    };
    const Vector2 tip = point(0, 0);
    const Vector2 left = point(0, 30);
    const Vector2 right = point(24, 23);
    const float outline = 2.0f * bounds.height / 40;
    DrawLineEx(point(10, 22), point(21, 38), outline * 4, BLACK);
    DrawLineEx(point(10, 22), point(21, 38), outline * 2, WHITE);
    DrawTriangle(tip, left, right, WHITE);
    DrawLineEx(tip, left, outline, BLACK);
    DrawLineEx(left, right, outline, BLACK);
    DrawLineEx(right, tip, outline, BLACK);
}

} // namespace game
