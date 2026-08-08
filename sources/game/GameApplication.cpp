#include "game/GameApplication.h"

#include "game/GameMainMenu.h"

#include <raylib.h>

namespace game {

bool GameApplication::Init(engine::EngineContext& context)
{
    Shutdown(context);
    std::string settingsError;
    if (!LoadFpsApplicationSettings(
                ASSETS_PATH "config/application_settings.json",
                applicationSettings,
                &settingsError)) {
        TraceLog(
                LOG_WARNING,
                "Application settings ignored: %s",
                settingsError.c_str());
        applicationSettings = FpsApplicationSettings{};
        menuStatus = settingsError;
    }
    std::string weaponError;
    if (!LoadFpsWeaponRegistry(
                ASSETS_PATH "config/weapons.json",
                weaponRegistry,
                &weaponError)) {
        menuStatus = weaponError.empty()
                ? "Weapon registry initialization failed"
                : weaponError;
        return false;
    }
    RequestFpsWeaponAudioAssets(context.assets, weaponRegistry);
    RequestPlayerAudioAssets(
            context.assets,
            applicationSettings.playerSounds,
            playerAudio);
    if (!editor.Init(context)) {
        menuStatus = "Editor initialization failed";
        return false;
    }
    flow = ApplicationFlowState{};
    initialized = true;
    return true;
}

void GameApplication::Shutdown(engine::EngineContext& context)
{
    context.audio.StopAll(context.assets);
    if (gameSession.IsRunning()) {
        gameSession.Shutdown(context, gameScene);
    } else if (gameScene.IsReady()) {
        gameScene.Shutdown(context);
    }
    editor.Shutdown(context);
    flow = ApplicationFlowState{};
    applicationSettings = FpsApplicationSettings{};
    playerAudio = PlayerAudioRuntime{};
    weaponRegistry = FpsWeaponRegistry{};
    menuStatus.clear();
    pendingMenuAction.reset();
    editorAttachedToGame = false;
    initialized = false;
}

void GameApplication::RenderInteractiveUI(
        engine::UIContext& contentUi,
        engine::UIContext& menuUi,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    if (flow.screen == ApplicationScreen::Editor) {
        editor.RenderUI(
                contentUi,
                config,
                input,
                assets,
                font,
                smallFont);
    }
    if (flow.screen == ApplicationScreen::MainMenu) {
        pendingMenuAction = DrawGameMainMenu(
                menuUi,
                config,
                input,
                assets,
                font,
                smallFont,
                flow.gameRunning,
                menuStatus.c_str());
    }
}

void GameApplication::Update(engine::EngineContext& context, float dt)
{
    if (!initialized) {
        return;
    }
    if (pendingMenuAction.has_value()) {
        const MainMenuAction action = *pendingMenuAction;
        pendingMenuAction.reset();
        HandleMenuAction(context, action);
        return;
    }

    if (flow.screen == ApplicationScreen::MainMenu) {
        context.input.ForEachEvent(
                engine::InputEventType::KeyPressed,
                true,
                [this, &context](engine::InputEvent& event) {
                    if (event.key.key != KEY_ESCAPE) {
                        return;
                    }
                    const ApplicationScreen destination =
                            flow.menuReturnScreen;
                    if (!ReturnFromApplicationMenu(flow)) {
                        return;
                    }
                    if (destination == ApplicationScreen::Game) {
                        gameSession.Resume(gameScene);
                        context.audio.ResumeAll(context.assets);
                    }
                    menuStatus.clear();
                    engine::ConsumeEvent(event);
                });
        return;
    }

    if (flow.screen == ApplicationScreen::Game) {
        bool menuRequested = false;
        context.input.ForEachEvent(
                engine::InputEventType::KeyPressed,
                true,
                [&menuRequested](engine::InputEvent& event) {
                    if (event.key.key != KEY_ESCAPE) {
                        return;
                    }
                    menuRequested = true;
                    engine::ConsumeEvent(event);
                });
        if (menuRequested) {
            gameSession.Pause();
            context.audio.PauseAll(context.assets);
            OpenApplicationMenu(flow, ApplicationScreen::Game);
            return;
        }
        gameSession.Update(context, gameScene, dt);
        return;
    }

    editor.Update(context, dt);
    if (editor.IsPreview3DActive()) {
        return;
    }
    bool menuRequested = false;
    context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&menuRequested](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) {
                    return;
                }
                menuRequested = true;
                engine::ConsumeEvent(event);
            });
    if (menuRequested) {
        OpenApplicationMenu(flow, ApplicationScreen::Editor);
    }
}

ApplicationContentKind GameApplication::BackgroundContentKind() const
{
    switch (BackgroundScreen()) {
        case ApplicationScreen::Game:
            return ApplicationContentKind::Sector3D;
        case ApplicationScreen::Editor:
            return editor.IsPreview3DActive()
                    ? ApplicationContentKind::Sector3D
                    : ApplicationContentKind::Editor2D;
        case ApplicationScreen::MainMenu:
            return ApplicationContentKind::Empty;
    }
    return ApplicationContentKind::Empty;
}

bool GameApplication::ShouldRefreshBackground() const
{
    return flow.screen != ApplicationScreen::MainMenu;
}

bool GameApplication::IsMenuOpen() const
{
    return flow.screen == ApplicationScreen::MainMenu;
}

void GameApplication::Render2D(engine::AssetManager& assets)
{
    editor.Render(assets);
}

void GameApplication::Render3DShadowMaps(engine::EngineContext& context)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameScene.RenderShadowMaps(context);
    } else {
        editor.RenderPreview3DShadowMaps(context.assets);
    }
}

void GameApplication::Render3DScene(engine::EngineContext& context)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameScene.RenderScene(context, gameSession.Map());
    } else {
        editor.RenderPreview3DScene(context);
    }
}

void GameApplication::Render3DViewmodel(engine::AssetManager& assets)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameSession.RenderViewmodel(assets, gameScene);
    } else {
        editor.RenderPreview3DViewmodel(assets);
    }
}

void GameApplication::Render3DOverlays()
{
    if (BackgroundScreen() == ApplicationScreen::Editor) {
        editor.RenderPreview3DOverlays();
    }
}

void GameApplication::Apply3DPostProcessing(
        engine::AssetManager& assets,
        RenderTexture2D& sceneTarget)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameScene.ApplyPostProcessing(
                assets,
                sceneTarget,
                gameSession.Map());
    } else {
        editor.ApplyPreview3DBloom(assets, sceneTarget);
    }
}

void GameApplication::Render3DHud(Rectangle playableViewport) const
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameSession.RenderHud(playableViewport);
    } else {
        editor.RenderPreview3DHud(playableViewport);
    }
}

void GameApplication::HandleMenuAction(
        engine::EngineContext& context,
        MainMenuAction action)
{
    switch (action) {
        case MainMenuAction::StartNewGame:
            StartNewGame(context);
            break;
        case MainMenuAction::Resume:
            ResumeGame(context);
            break;
        case MainMenuAction::LoadGame:
        case MainMenuAction::SaveGame:
        case MainMenuAction::Settings:
            break;
        case MainMenuAction::Editor:
            OpenEditor(context);
            break;
        case MainMenuAction::Quit:
            RequestApplicationQuit(flow);
            break;
    }
}

void GameApplication::StartNewGame(engine::EngineContext& context)
{
    context.audio.StopAll(context.assets);
    editor.SuspendRuntime(context);
    std::string error;
    if (!gameSession.StartNew(
                context,
                gameScene,
                SectorLevelEntryRequest{applicationSettings.firstLevel, std::nullopt},
                weaponRegistry,
                applicationSettings,
                playerAudio,
                error)) {
        menuStatus = error.empty() ? "Could not start a new game" : error;
        return;
    }
    editorAttachedToGame = false;
    menuStatus.clear();
    MarkApplicationGameStarted(flow);
}

void GameApplication::ResumeGame(engine::EngineContext& context)
{
    if (!gameSession.IsRunning()) {
        return;
    }
    if (editorAttachedToGame) {
        const SectorTopologyMap editedMap = editor.CurrentTopologyMap();
        editor.SuspendRuntime(context);
        std::string error;
        if (!gameSession.RebuildFromMap(
                    context,
                    gameScene,
                    editedMap,
                    error)) {
            editor.RestoreRuntimeObjects(context);
            menuStatus = error.empty()
                    ? "Could not rebuild the edited game level"
                    : error;
            return;
        }
    }
    gameSession.Resume(gameScene);
    context.audio.ResumeAll(context.assets);
    flow.screen = ApplicationScreen::Game;
    flow.menuReturnScreen = ApplicationScreen::Game;
    menuStatus.clear();
}

void GameApplication::OpenEditor(engine::EngineContext& context)
{
    if (gameSession.IsRunning()) {
        gameSession.Pause();
        context.audio.StopAll(context.assets);
        if (gameScene.IsReady()) {
            gameScene.Shutdown(context);
        }
        if (editorAttachedToGame) {
            editor.RestoreRuntimeObjects(context);
        } else if (!editor.OpenLevel(
                           context,
                           gameSession.LevelName(),
                           gameSession.LevelPath())) {
            menuStatus = "Could not open the running game's level in the editor";
            const SectorTopologyMap currentMap = gameSession.Map();
            std::string restoreError;
            if (!gameSession.RebuildFromMap(
                        context,
                        gameScene,
                        currentMap,
                        restoreError)
                    && !restoreError.empty()) {
                menuStatus += ": " + restoreError;
            }
            return;
        }
        editorAttachedToGame = true;
    }
    menuStatus.clear();
    ShowApplicationEditor(flow);
}

ApplicationScreen GameApplication::BackgroundScreen() const
{
    return flow.screen == ApplicationScreen::MainMenu
            ? flow.menuReturnScreen
            : flow.screen;
}

} // namespace game
