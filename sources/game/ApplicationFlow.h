#pragma once

#include <array>
#include <cstddef>

namespace game {

enum class ApplicationScreen {
    MainMenu,
    Game,
    Editor
};

enum class MainMenuAction {
    StartNewGame,
    Resume,
    LoadGame,
    SaveGame,
    Editor,
    Settings,
    Quit
};

struct MainMenuItems {
    std::array<MainMenuAction, 7> values{};
    size_t count = 0;
};

struct ApplicationFlowState {
    ApplicationScreen screen = ApplicationScreen::MainMenu;
    ApplicationScreen menuReturnScreen = ApplicationScreen::MainMenu;
    bool gameRunning = false;
    bool quitRequested = false;
};

MainMenuItems BuildMainMenuItems(bool gameRunning);
const char* MainMenuActionLabel(MainMenuAction action);

void OpenApplicationMenu(
        ApplicationFlowState& state,
        ApplicationScreen returnScreen);
bool ReturnFromApplicationMenu(ApplicationFlowState& state);
bool ShouldRebuildGameFromEditorOnResume(
        const ApplicationFlowState& state,
        bool editorAttachedToGame);
void MarkApplicationGameStarted(ApplicationFlowState& state);
void MarkApplicationGameStopped(ApplicationFlowState& state);
void ShowApplicationEditor(ApplicationFlowState& state);
void RequestApplicationQuit(ApplicationFlowState& state);
bool IsApplicationDebugConsoleAvailable(
        const ApplicationFlowState& state,
        bool gameRunning,
        bool consoleEnabled);

} // namespace game
