#include "game/GameCursor.h"

#include <cassert>
#include <cmath>

namespace {

void PointerVisibilityFollowsInteractiveUi()
{
    game::ApplicationFlowState flow;
    using Mode = game::GameCursorMode;
    assert(game::ResolveGameCursorMode(flow, false, {}) == Mode::Arrow);
    assert(game::ResolveGameCursorMode(flow, true, {}) == Mode::Hidden);

    game::MarkApplicationGameStarted(flow);
    // Gameplay, speech/cutscenes, note reading and the console do not request
    // a pointer merely because they release mouse-look.
    assert(game::ResolveGameCursorMode(flow, false, {}) == Mode::Hidden);
    const game::GameCursorUiState interactiveStates[] = {
            {true, false, false, false, false}, // inventory (including dragging/splitting)
            {false, true, false, false, false}, // held item targeting/pending use
            {false, false, true, false, false}, // dialogue choices, also during cutscenes
            {false, false, false, true, false}, // keypad
            {false, false, false, false, true}, // game over
    };
    for (const auto& ui : interactiveStates) {
        assert(game::ResolveGameCursorMode(flow, false, ui) == Mode::Arrow);
        assert(game::ResolveGameCursorMode(flow, true, ui) == Mode::Hidden);
    }

    // Pausing a cutscene still makes the menu usable, and resuming hides it.
    game::OpenApplicationMenu(flow, game::ApplicationScreen::Game);
    assert(game::ResolveGameCursorMode(flow, false, {}) == Mode::Arrow);
    assert(game::ReturnFromApplicationMenu(flow));
    assert(game::ResolveGameCursorMode(flow, false, {}) == Mode::Hidden);

    game::ShowApplicationEditor(flow);
    assert(game::ResolveGameCursorMode(flow, false, interactiveStates[0]) == Mode::Native);
    game::OpenApplicationMenu(flow, game::ApplicationScreen::Editor);
    assert(game::ResolveGameCursorMode(flow, false, {}) == Mode::Native);
    assert(game::ResolveGameCursorMode(flow, true, {}) == Mode::Native);
    assert(game::ReturnFromApplicationMenu(flow));
    assert(game::ResolveGameCursorMode(flow, false, {}) == Mode::Native);

    game::MarkApplicationGameStarted(flow);
    assert(game::ResolveGameCursorMode(flow, false, {}) == Mode::Hidden);
    game::MarkApplicationGameStopped(flow);
    assert(game::ResolveGameCursorMode(flow, false, {}) == Mode::Arrow);
}

void Near(float actual, float expected)
{
    assert(std::fabs(actual - expected) < 0.001f);
}

void HotspotTracksMouseAtEveryPresentationScale()
{
    const Vector2 logicalSize{1920, 1080};
    const Rectangle viewports[] = {
            {0, 0, 1920, 1080},
            {0, 0, 960, 540},
            {0, 150, 1600, 900},
            {400, 0, 1920, 1080},
            {0, 122.5f, 1000, 562.5f},
    };
    const Vector2 screenPoints[] = {{0, 0}, {615, 410}, {959, 539}};
    for (const auto& sprite : {game::GameArrowCursor, game::GameInteractionCursor}) {
        for (const Rectangle viewport : viewports) {
            for (const Vector2 point : screenPoints) {
                // Same transform as Main's SetMouseOffset/SetMouseScale.
                const Vector2 logicalMouse{
                        (point.x - static_cast<int>(viewport.x)) * logicalSize.x / viewport.width,
                        (point.y - static_cast<int>(viewport.y)) * logicalSize.y / viewport.height};
                const Rectangle bounds = game::BuildGameCursorDestination(
                        sprite, logicalMouse, logicalSize, viewport);
                Near(bounds.height, 40 * viewport.height / 1080);
                Near(bounds.x + (sprite.hotspot.x - sprite.source.x)
                        * bounds.width / sprite.source.width, point.x);
                Near(bounds.y + (sprite.hotspot.y - sprite.source.y)
                        * bounds.height / sprite.source.height, point.y);
                Near(bounds.width / bounds.height, sprite.source.width / sprite.source.height);
            }
        }
    }
    assert(game::BuildGameCursorDestination(game::GameArrowCursor, {}, {}, viewports[0]).width == 0);
    assert(game::BuildGameCursorDestination(game::GameArrowCursor, {}, logicalSize, {}).height == 0);
}

} // namespace

int main()
{
    PointerVisibilityFollowsInteractiveUi();
    HotspotTracksMouseAtEveryPresentationScale();
}
