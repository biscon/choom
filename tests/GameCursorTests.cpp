#include "game/GameCursor.h"
#include "engine/input/Input.h"

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

void PositionSurvivesDialogueAndOtherUiTransitions()
{
    using Mode = game::GameCursorMode;
    game::GameCursorPositionState state;
    // With no history, first use leaves the current pointer alone.
    assert(!game::RequestGameCursorPositionRestore(state, Mode::Arrow, true, true));
    game::RememberGameCursorPosition(state, Mode::Arrow, {430, 900}, true, true);
    assert(state.hasPosition);

    for (int cycle = 0; cycle < 3; ++cycle) {
        assert(!game::RequestGameCursorPositionRestore(state, Mode::Hidden, false, true));
        // Hidden speech / mouse-look cannot replace the UI location.
        game::RememberGameCursorPosition(state, Mode::Hidden, {960, 540}, true, true);
        assert(game::RequestGameCursorPositionRestore(state, Mode::Arrow, true, true));
        Near(state.logicalPosition.x, 430);
        Near(state.logicalPosition.y, 900);
        state.restorePending = false; // warp completed
        assert(!game::RequestGameCursorPositionRestore(state, Mode::Arrow, false, true));
    }

    // Opening another UI shares the latest real pointer position. Even an
    // arrow-to-arrow capture release (e.g. inventory -> menu) needs restoration.
    game::RememberGameCursorPosition(state, Mode::Arrow, {710, 820}, true, true);
    assert(game::RequestGameCursorPositionRestore(state, Mode::Arrow, true, true));
    Near(state.logicalPosition.x, 710);
    Near(state.logicalPosition.y, 820);
    state.restorePending = false;

    assert(!game::RequestGameCursorPositionRestore(state, Mode::Native, false, true));
    game::RememberGameCursorPosition(state, Mode::Native, {50, 50}, true, true);
    assert(game::RequestGameCursorPositionRestore(state, Mode::Arrow, false, true));
    Near(state.logicalPosition.x, 710);
    Near(state.logicalPosition.y, 820);
}

void FocusLossDefersRestorationWithoutLosingHistory()
{
    using Mode = game::GameCursorMode;
    game::GameCursorPositionState state;
    game::RememberGameCursorPosition(state, Mode::Arrow, {400, 850}, true, true);
    assert(game::RequestGameCursorPositionRestore(state, Mode::Arrow, false, true));
    state.restorePending = false;

    // Leaving the client area must not save off-window positions.
    game::RememberGameCursorPosition(state, Mode::Arrow, {-300, -400}, true, false);
    Near(state.logicalPosition.x, 400);
    game::RememberGameCursorPosition(state, Mode::Arrow, {960, 540}, false, true);
    assert(!game::RequestGameCursorPositionRestore(state, Mode::Arrow, true, false));
    assert(state.restorePending);
    // On the first focused frame, the newly polled desktop position must not
    // overwrite the saved UI position before restoration runs.
    game::RememberGameCursorPosition(state, Mode::Arrow, {10, 20}, true, true);
    assert(game::RequestGameCursorPositionRestore(state, Mode::Arrow, false, true));
    Near(state.logicalPosition.x, 400);
    Near(state.logicalPosition.y, 850);
    state.restorePending = false;
    game::RememberGameCursorPosition(state, Mode::Arrow, {450, 870}, true, true);
    assert(!game::RequestGameCursorPositionRestore(state, Mode::Arrow, false, true));
    Near(state.logicalPosition.x, 450);

    assert(!game::RequestGameCursorPositionRestore(state, Mode::Hidden, false, false));
    assert(!game::RequestGameCursorPositionRestore(state, Mode::Arrow, true, false));
    assert(state.restorePending);
    assert(game::RequestGameCursorPositionRestore(state, Mode::Arrow, false, true));
    Near(state.logicalPosition.y, 870);
}

void RestorationUsesCurrentViewportAndClampsToWindow()
{
    const Vector2 logicalSize{1920, 1080};
    Vector2 position{};
    assert(game::BuildGameCursorRestorePosition(
            {480, 900}, logicalSize, {0, 0, 1920, 1080}, {1920, 1080}, position));
    Near(position.x, 480);
    Near(position.y, 900);
    assert(game::BuildGameCursorRestorePosition(
            {480, 900}, logicalSize, {0, 150, 960, 540}, {960, 840}, position));
    Near(position.x, 240);
    Near(position.y, 600);
    assert(game::BuildGameCursorRestorePosition(
            {480, 900}, logicalSize, {320, 0, 1920, 1080}, {2560, 1080}, position));
    Near(position.x, 800);
    Near(position.y, 900);
    assert(game::BuildGameCursorRestorePosition(
            {480, 900}, logicalSize, {0, 122.5f, 1000, 562.5f}, {1000, 808}, position));
    Near(position.x, 250);
    Near(position.y, 591); // integer offset 122 + 468.75, rounded to a pixel
    assert(game::BuildGameCursorRestorePosition(
            {-500, 5000}, logicalSize, {0, 0, 960, 540}, {960, 540}, position));
    Near(position.x, 0);
    Near(position.y, 539);
    assert(!game::BuildGameCursorRestorePosition({}, logicalSize, {}, {960, 540}, position));
    assert(!game::BuildGameCursorRestorePosition({}, {}, {0, 0, 960, 540}, {960, 540}, position));
    assert(!game::BuildGameCursorRestorePosition({}, logicalSize, {0, 0, 960, 540}, {}, position));
}

void WarpingSynchronizesInputWithoutArtificialMotion()
{
    engine::InputFrameState frame;
    frame.mousePosition = {960, 540};
    frame.previousMousePosition = {900, 500};
    frame.mouseDelta = {60, 40};
    frame.mouseWheelMove = 2;
    frame.windowFocused = true;
    frame.cursorOnScreen = true;
    engine::SynchronizeInputMousePosition(frame, {430, 900});
    Near(frame.mousePosition.x, 430);
    Near(frame.mousePosition.y, 900);
    Near(frame.previousMousePosition.x, 430);
    Near(frame.previousMousePosition.y, 900);
    Near(frame.mouseDelta.x, 0);
    Near(frame.mouseDelta.y, 0);
    Near(frame.mouseWheelMove, 2);
    assert(frame.windowFocused && frame.cursorOnScreen);
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
    PositionSurvivesDialogueAndOtherUiTransitions();
    FocusLossDefersRestorationWithoutLosingHistory();
    RestorationUsesCurrentViewportAndClampsToWindow();
    WarpingSynchronizesInputWithoutArtificialMotion();
    HotspotTracksMouseAtEveryPresentationScale();
}
