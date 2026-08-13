#include "game/ApplicationFlow.h"

#include <cassert>
#include <cstring>

namespace {

bool Contains(
        const game::MainMenuItems& items,
        game::MainMenuAction action)
{
    for (size_t i = 0; i < items.count; ++i) {
        if (items.values[i] == action) {
            return true;
        }
    }
    return false;
}

void MenuItemsFollowSessionState()
{
    const game::MainMenuItems idle = game::BuildMainMenuItems(false);
    assert(idle.count == 5);
    assert(Contains(idle, game::MainMenuAction::StartNewGame));
    assert(!Contains(idle, game::MainMenuAction::Resume));
    assert(!Contains(idle, game::MainMenuAction::SaveGame));
    assert(idle.values[0] == game::MainMenuAction::StartNewGame);
    assert(idle.values[1] == game::MainMenuAction::LoadGame);

    const game::MainMenuItems running = game::BuildMainMenuItems(true);
    assert(running.count == 6);
    assert(!Contains(running, game::MainMenuAction::StartNewGame));
    assert(Contains(running, game::MainMenuAction::Resume));
    assert(Contains(running, game::MainMenuAction::SaveGame));
    assert(running.values[0] == game::MainMenuAction::Resume);
    assert(running.values[1] == game::MainMenuAction::LoadGame);
    assert(running.values[2] == game::MainMenuAction::SaveGame);
    assert(std::strcmp(
            game::MainMenuActionLabel(game::MainMenuAction::StartNewGame),
            "Start New Game") == 0);
}

void FlowPreservesReturnTargets()
{
    game::ApplicationFlowState state;
    assert(!game::ReturnFromApplicationMenu(state));

    game::MarkApplicationGameStarted(state);
    assert(state.gameRunning);
    assert(state.screen == game::ApplicationScreen::Game);

    game::OpenApplicationMenu(state, game::ApplicationScreen::Game);
    assert(state.screen == game::ApplicationScreen::MainMenu);
    assert(game::ReturnFromApplicationMenu(state));
    assert(state.screen == game::ApplicationScreen::Game);

    game::ShowApplicationEditor(state);
    assert(state.screen == game::ApplicationScreen::Editor);
    game::OpenApplicationMenu(state, game::ApplicationScreen::Editor);
    assert(game::ReturnFromApplicationMenu(state));
    assert(state.screen == game::ApplicationScreen::Editor);

    game::RequestApplicationQuit(state);
    assert(state.quitRequested);

    game::MarkApplicationGameStopped(state);
    assert(!state.gameRunning);
    assert(state.screen == game::ApplicationScreen::MainMenu);
    assert(state.menuReturnScreen == game::ApplicationScreen::MainMenu);
}

} // namespace

int main()
{
    MenuItemsFollowSessionState();
    FlowPreservesReturnTargets();
    return 0;
}
