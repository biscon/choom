#include "game/GameApplication.h"

#include "engine/assets/FontLoadFlags.h"
#include "engine/debug/DebugConsole.h"
#include "engine/debug/DebugConsoleLogBridge.h"
#include "game/GameMainMenu.h"

#include <raylib.h>

namespace game {

namespace {

constexpr const char* ApplicationSettingsPath =
        ASSETS_PATH "config/application_settings.json";

SectorTopologyFogSettings::LocalVolumeQuality VolumetricQuality(
        FpsVolumetricQualityCap quality)
{
    return static_cast<SectorTopologyFogSettings::LocalVolumeQuality>(
            static_cast<int>(quality));
}

int ShadowMapResolution(FpsShadowQuality quality)
{
    return quality == FpsShadowQuality::Low ? 512 : 1024;
}

float ProjectedShadowInterval(FpsShadowQuality quality)
{
    if (quality == FpsShadowQuality::Low) return 1.0f / 15.0f;
    if (quality == FpsShadowQuality::Medium) return 1.0f / 30.0f;
    return 0.0f;
}

int ProjectedShadowResolution(FpsShadowQuality quality)
{
    return quality == FpsShadowQuality::Low ? 128 : 256;
}

} // namespace

bool GameApplication::Init(
        engine::EngineContext& context,
        FpsApplicationSettings initialSettings,
        std::string settingsLoadError)
{
    Shutdown(context);
    applicationSettings = std::move(initialSettings);
    menuStatus = std::move(settingsLoadError);
    applicationSettings.graphics =
            NormalizeFpsGraphicsSettings(applicationSettings.graphics);
    engine::SetDebugConsoleLogCaptureEnabled(
            applicationSettings.consoleEnabled);
    const engine::FontHandle consoleFont = context.assets.RequestFont(
            context.assets.GlobalScope(),
            "debug_console_inconsolata_22",
            ASSETS_PATH "fonts/Inconsolata.otf",
            22,
            engine::FontLoad_BilinearFilter);
    engine::DebugConsoleInitialize(debugConsole, consoleFont);
    if (applicationSettings.consoleEnabled) {
        engine::FlushPendingDebugConsoleLogs(debugConsole);
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
    ApplyPerspectiveFov();
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
    if (debugConsole.initialized) {
        engine::FlushPendingDebugConsoleLogs(debugConsole);
        engine::SetDebugConsoleLogCaptureEnabled(false);
        engine::DebugConsoleShutdown(debugConsole);
    }
    flow = ApplicationFlowState{};
    applicationSettings = FpsApplicationSettings{};
    playerAudio = PlayerAudioRuntime{};
    persistentScripts = engine::PersistentScriptStore{};
    weaponRegistry = FpsWeaponRegistry{};
    menuStatus.clear();
    pendingMenuAction.reset();
    pendingSettingsAction.reset();
    pendingGraphicsSettings.reset();
    graphicsSettingsDraft = FpsApplicationSettings{};
    graphicsSettingsOpen = false;
    editorAttachedToGame = false;
    initialized = false;
}

void GameApplication::UpdateDebugConsole(
        engine::EngineContext& context,
        float dt)
{
    if (!debugConsole.initialized) return;
    if (applicationSettings.consoleEnabled) {
        engine::FlushPendingDebugConsoleLogs(debugConsole);
    }
    const bool available = DebugConsoleAvailable();
    engine::ScriptRuntime* scripts = available
            ? gameSession.ConsoleScriptRuntime() : nullptr;
    engine::DebugConsoleUpdate(
            debugConsole,
            context.input,
            scripts,
            available ? std::string_view{gameSession.LevelName()}
                      : std::string_view{},
            available,
            dt);
    gameSession.SetConsoleInputCaptured(
            available
                    && engine::DebugConsoleCapturesGameplayInput(debugConsole));
    if (applicationSettings.consoleEnabled) {
        engine::FlushPendingDebugConsoleLogs(debugConsole);
    }
}

void GameApplication::ProcessDeferredDebugActions(
        engine::EngineContext& context)
{
    const engine::DeferredDebugAction action =
            engine::DebugConsoleTakeDeferredAction(debugConsole);
    if (action.type == engine::DeferredDebugActionType::None) return;
    if (action.type == engine::DeferredDebugActionType::QuitApplication) {
        RequestApplicationQuit(flow);
        return;
    }
    if (!gameSession.IsRunning() || action.mapId != gameSession.LevelName()) {
        engine::DebugConsoleAddLine(
                debugConsole,
                "reload cancelled: the active game map changed",
                engine::DebugConsoleSeverity::Error);
        return;
    }
    const bool remainPaused = flow.screen == ApplicationScreen::MainMenu
            && flow.menuReturnScreen == ApplicationScreen::Game;
    std::string error;
    if (!gameSession.ReloadCurrentMap(
                context, gameScene, remainPaused, error)) {
        menuStatus = "Reload failed: "
                + (error.empty() ? std::string{"unknown error"} : error);
        engine::DebugConsoleAddLine(
                debugConsole, menuStatus, engine::DebugConsoleSeverity::Error);
        debugConsole.open = false;
        MarkApplicationGameStopped(flow);
        return;
    }
    if (remainPaused) context.audio.PauseAll(context.assets);
    gameSession.SetConsoleInputCaptured(
            engine::DebugConsoleCapturesGameplayInput(debugConsole));
    engine::DebugConsoleAddLine(
            debugConsole,
            "reload completed: " + gameSession.LevelName(),
            engine::DebugConsoleSeverity::Success);
    engine::FlushPendingDebugConsoleLogs(debugConsole);
}

void GameApplication::RenderDebugConsole(
        engine::AssetManager& assets,
        int logicalWidth,
        int logicalHeight)
{
    if (!DebugConsoleAvailable()) return;
    engine::DebugConsoleRender(
            debugConsole, assets, logicalWidth, logicalHeight);
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
        if (graphicsSettingsOpen) {
            const GameGraphicsSettingsAction action = DrawGameGraphicsSettings(
                    menuUi, config, input, assets, font, smallFont,
                    graphicsSettingsDraft, menuStatus.c_str());
            if (action != GameGraphicsSettingsAction::None) {
                pendingSettingsAction = action;
            }
        } else {
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
    if (pendingSettingsAction.has_value()) {
        const GameGraphicsSettingsAction action = *pendingSettingsAction;
        pendingSettingsAction.reset();
        switch (action) {
            case GameGraphicsSettingsAction::Apply:
                graphicsSettingsDraft.graphics = NormalizeFpsGraphicsSettings(
                        graphicsSettingsDraft.graphics);
                pendingGraphicsSettings = graphicsSettingsDraft;
                menuStatus.clear();
                break;
            case GameGraphicsSettingsAction::Cancel:
                graphicsSettingsOpen = false;
                menuStatus.clear();
                break;
            case GameGraphicsSettingsAction::Defaults:
                graphicsSettingsDraft.graphics = FpsGraphicsSettings{};
                graphicsSettingsDraft.hdrBloom.enabled = true;
                menuStatus.clear();
                break;
            case GameGraphicsSettingsAction::None:
                break;
        }
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
                    if (graphicsSettingsOpen) {
                        graphicsSettingsOpen = false;
                        pendingGraphicsSettings.reset();
                        menuStatus.clear();
                        engine::ConsumeEvent(event);
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
        const std::string scriptFailure = gameSession.TakeFailureError();
        if (!scriptFailure.empty()) {
            menuStatus = scriptFailure;
            debugConsole.open = false;
            gameSession.SetConsoleInputCaptured(false);
            MarkApplicationGameStopped(flow);
        }
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
        gameScene.Renderer().SetGraphicsQuality(
                VolumetricQuality(applicationSettings.graphics.volumetricQualityCap),
                applicationSettings.graphics.shadowQuality != FpsShadowQuality::Off,
                ShadowMapResolution(applicationSettings.graphics.shadowQuality),
                ProjectedShadowInterval(applicationSettings.graphics.shadowQuality),
                ProjectedShadowResolution(applicationSettings.graphics.shadowQuality));
        gameScene.RenderShadowMaps(context);
    } else {
        editor.SetPreviewGraphicsQuality(
                VolumetricQuality(applicationSettings.graphics.volumetricQualityCap),
                applicationSettings.graphics.shadowQuality != FpsShadowQuality::Off,
                ShadowMapResolution(applicationSettings.graphics.shadowQuality),
                ProjectedShadowInterval(applicationSettings.graphics.shadowQuality),
                ProjectedShadowResolution(applicationSettings.graphics.shadowQuality));
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

void GameApplication::Apply3DWorldAtmosphere(
        engine::RenderTarget& sceneTarget)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameScene.ApplyWorldAtmosphere(sceneTarget, gameSession.Map());
    } else {
        editor.ApplyPreview3DWorldAtmosphere(sceneTarget);
    }
}

void GameApplication::Apply3DHdrBloom(engine::RenderTarget& sceneTarget)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameScene.ApplyHdrBloom(
                sceneTarget, applicationSettings.hdrBloom, true);
    } else {
        editor.ApplyPreview3DHdrBloom(sceneTarget);
    }
}

bool GameApplication::Composite3DViewmodel(
        engine::RenderTarget& sceneTarget,
        const engine::RenderTarget& viewmodelTarget)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        return gameScene.CompositeViewmodel(sceneTarget, viewmodelTarget);
    }
    return editor.CompositePreview3DViewmodel(sceneTarget, viewmodelTarget);
}

const engine::RenderTarget* GameApplication::HdrDebugPresentationSource() const
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        return gameScene.HdrDebugPresentationSource();
    }
    return editor.Preview3DHdrDebugPresentationSource();
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
            break;
        case MainMenuAction::Settings:
            graphicsSettingsDraft = applicationSettings;
            pendingGraphicsSettings.reset();
            graphicsSettingsOpen = true;
            menuStatus.clear();
            break;
        case MainMenuAction::Editor:
            OpenEditor(context);
            break;
        case MainMenuAction::Quit:
            RequestApplicationQuit(flow);
            break;
    }
}

const FpsApplicationSettings* GameApplication::PendingGraphicsSettings() const
{
    return pendingGraphicsSettings.has_value()
            ? &*pendingGraphicsSettings
            : nullptr;
}

bool GameApplication::CommitPendingGraphicsSettings(std::string& error)
{
    if (!pendingGraphicsSettings.has_value()) {
        error = "No graphics settings apply is pending";
        return false;
    }
    FpsApplicationSettings candidate = *pendingGraphicsSettings;
    candidate.graphics = NormalizeFpsGraphicsSettings(candidate.graphics);
    if (!SaveFpsApplicationSettings(ApplicationSettingsPath, candidate, &error)) {
        menuStatus = error;
        pendingGraphicsSettings.reset();
        return false;
    }
    applicationSettings = std::move(candidate);
    ApplyPerspectiveFov();
    graphicsSettingsDraft = applicationSettings;
    pendingGraphicsSettings.reset();
    graphicsSettingsOpen = false;
    menuStatus.clear();
    return true;
}

void GameApplication::RejectPendingGraphicsSettings(const std::string& error)
{
    pendingGraphicsSettings.reset();
    menuStatus = error;
}

void GameApplication::TogglePerformanceOverlay()
{
    applicationSettings.graphics.performanceOverlay =
            !applicationSettings.graphics.performanceOverlay;
    graphicsSettingsDraft.graphics.performanceOverlay =
            applicationSettings.graphics.performanceOverlay;
    std::string error;
    if (!SaveFpsApplicationSettings(ApplicationSettingsPath, applicationSettings, &error)) {
        menuStatus = error;
    }
}

void GameApplication::StartNewGame(engine::EngineContext& context)
{
    context.audio.StopAll(context.assets);
    editor.SuspendRuntime(context);
    std::string error;
    persistentScripts = engine::PersistentScriptStore{};
    if (!gameSession.StartNew(
                context,
                gameScene,
                SectorLevelEntryRequest{applicationSettings.firstLevel, std::nullopt},
                weaponRegistry,
                applicationSettings,
                playerAudio,
                persistentScripts,
                false,
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
        gameSession.SuspendForEditor(context);
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

bool GameApplication::DebugConsoleAvailable() const
{
    return IsApplicationDebugConsoleAvailable(
            flow,
            gameSession.IsRunning(),
            applicationSettings.consoleEnabled);
}

void GameApplication::ApplyPerspectiveFov()
{
    constexpr float WorldRenderAspect = 16.0f / 9.0f;
    const float verticalFov = FpsVerticalFovDegrees(
            applicationSettings.graphics.horizontalFovDegrees,
            WorldRenderAspect);
    gameScene.Renderer().SetVerticalFovDegrees(verticalFov);
    editor.SetPreviewVerticalFovDegrees(verticalFov);
}

} // namespace game
