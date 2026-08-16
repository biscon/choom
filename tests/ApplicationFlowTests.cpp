#include "game/ApplicationFlow.h"
#include "game/GameLevelLoading.h"

#include <cassert>
#include <cstring>
#include <cmath>

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

void DebugConsoleAvailabilityFollowsLiveGameOnly()
{
    game::ApplicationFlowState state;
    assert(!game::IsApplicationDebugConsoleAvailable(state, false, true));
    assert(!game::IsApplicationDebugConsoleAvailable(state, true, true));

    game::MarkApplicationGameStarted(state);
    assert(game::IsApplicationDebugConsoleAvailable(state, true, true));
    assert(!game::IsApplicationDebugConsoleAvailable(state, true, false));

    game::OpenApplicationMenu(state, game::ApplicationScreen::Game);
    assert(game::IsApplicationDebugConsoleAvailable(state, true, true));

    game::ShowApplicationEditor(state);
    assert(!game::IsApplicationDebugConsoleAvailable(state, true, true));
    game::OpenApplicationMenu(state, game::ApplicationScreen::Editor);
    assert(!game::IsApplicationDebugConsoleAvailable(state, true, true));
}

void LevelLoadingProgressAndFadeAreDeterministic()
{
    game::GameLevelLoadingState loading;
    game::BeginGameLevelLoading(loading);
    assert(game::IsGameLevelLoading(loading));
    assert(game::IsGameLevelLoadOverlayVisible(loading));
    assert(game::GameLevelLoadOverlayOpacity(loading) == 1.0f);

    game::UpdateGameLevelLoadingProgress(loading, 0.5f, 0.25f, false);
    assert(std::fabs(loading.displayedProgress - 0.45f) < 0.0001f);
    game::UpdateGameLevelLoadingProgress(loading, 0.1f, 0.0f, false);
    assert(std::fabs(loading.displayedProgress - 0.45f) < 0.0001f);
    game::UpdateGameLevelLoadingProgress(loading, 1.0f, 1.0f, false);
    assert(std::fabs(loading.displayedProgress - 0.99f) < 0.0001f);
    game::UpdateGameLevelLoadingProgress(loading, 1.0f, 1.0f, true);
    assert(loading.displayedProgress == 1.0f);

    game::BeginGameLevelLoadingFade(loading);
    assert(!game::AdvanceGameLevelLoadingFade(loading, 0.125f));
    assert(std::fabs(game::GameLevelLoadOverlayOpacity(loading) - 0.5f)
            < 0.0001f);
    assert(game::AdvanceGameLevelLoadingFade(loading, 0.125f));
    assert(game::GameLevelLoadOverlayOpacity(loading) == 0.0f);
    game::ActivateGameLevel(loading);
    assert(!game::IsGameLevelLoading(loading));
    assert(!game::IsGameLevelLoadOverlayVisible(loading));

    game::StopGameLevelLoading(loading);
    assert(loading.phase == game::GameLevelLoadPhase::Stopped);
}

void NpcNavigationMustBeUsableBeforeActivation()
{
    assert(game::EvaluateGameLevelNavigationGate(true, false, false)
            == game::GameLevelNavigationGate::Waiting);
    assert(game::EvaluateGameLevelNavigationGate(true, true, true)
            == game::GameLevelNavigationGate::Ready);
    assert(game::EvaluateGameLevelNavigationGate(true, true, false)
            == game::GameLevelNavigationGate::Unavailable);
    assert(game::EvaluateGameLevelNavigationGate(false, true, false)
            == game::GameLevelNavigationGate::Ready);
}

} // namespace

int main()
{
    MenuItemsFollowSessionState();
    FlowPreservesReturnTargets();
    DebugConsoleAvailabilityFollowsLiveGameOnly();
    LevelLoadingProgressAndFadeAreDeterministic();
    NpcNavigationMustBeUsableBeforeActivation();
    return 0;
}
