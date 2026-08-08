#include "game/ApplicationFlow.h"

namespace game {

namespace {

void Add(MainMenuItems& items, MainMenuAction action)
{
    items.values[items.count++] = action;
}

} // namespace

MainMenuItems BuildMainMenuItems(bool gameRunning)
{
    MainMenuItems items;
    if (!gameRunning) {
        Add(items, MainMenuAction::StartNewGame);
    }
    if (gameRunning) {
        Add(items, MainMenuAction::Resume);
    }
    Add(items, MainMenuAction::LoadGame);
    if (gameRunning) {
        Add(items, MainMenuAction::SaveGame);
    }
    Add(items, MainMenuAction::Editor);
    Add(items, MainMenuAction::Settings);
    Add(items, MainMenuAction::Quit);
    return items;
}

const char* MainMenuActionLabel(MainMenuAction action)
{
    switch (action) {
        case MainMenuAction::StartNewGame: return "Start New Game";
        case MainMenuAction::Resume: return "Resume";
        case MainMenuAction::LoadGame: return "Load Game";
        case MainMenuAction::SaveGame: return "Save Game";
        case MainMenuAction::Editor: return "Editor";
        case MainMenuAction::Settings: return "Settings";
        case MainMenuAction::Quit: return "Quit";
    }
    return "";
}

void OpenApplicationMenu(
        ApplicationFlowState& state,
        ApplicationScreen returnScreen)
{
    state.menuReturnScreen = returnScreen;
    state.screen = ApplicationScreen::MainMenu;
}

bool ReturnFromApplicationMenu(ApplicationFlowState& state)
{
    if (state.screen != ApplicationScreen::MainMenu
            || state.menuReturnScreen == ApplicationScreen::MainMenu) {
        return false;
    }
    state.screen = state.menuReturnScreen;
    return true;
}

void MarkApplicationGameStarted(ApplicationFlowState& state)
{
    state.gameRunning = true;
    state.screen = ApplicationScreen::Game;
    state.menuReturnScreen = ApplicationScreen::Game;
}

void ShowApplicationEditor(ApplicationFlowState& state)
{
    state.screen = ApplicationScreen::Editor;
    state.menuReturnScreen = ApplicationScreen::Editor;
}

void RequestApplicationQuit(ApplicationFlowState& state)
{
    state.quitRequested = true;
}

} // namespace game
