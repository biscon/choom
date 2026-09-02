#include "game/GameApplication.h"

#include "engine/assets/FontLoadFlags.h"
#include "engine/debug/DebugConsole.h"
#include "engine/debug/DebugConsoleLogBridge.h"
#include "engine/render/ColorTransfer.h"
#include "game/GameMainMenu.h"
#include "game/SectorLevelLoader.h"
#include "game/save/GameSaveRuntime.h"
#include "game/save/GameSaveStorage.h"

#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>

namespace game {

namespace {

constexpr const char* ApplicationSettingsPath =
        ASSETS_PATH "config/application_settings.json";

int ShadowMapResolution(FpsShadowQuality quality)
{
    return quality == FpsShadowQuality::Low ? 512 : 1024;
}

} // namespace

bool GameApplication::Init(
        engine::EngineContext& context,
        FpsApplicationSettings initialSettings,
        std::string settingsLoadError)
{
    Shutdown(context);
    applicationSettings = std::move(initialSettings);
    saveRoot = ResolveGameSaveRoot("sector_engine");
    menuStatus = std::move(settingsLoadError);
    applicationSettings.graphics =
            NormalizeFpsGraphicsSettings(applicationSettings.graphics);
    applicationSettings.toneMapping = engine::NormalizeToneMappingSettings(
            applicationSettings.toneMapping);
    engine::SetDebugConsoleLogCaptureEnabled(
            applicationSettings.consoleEnabled);
    const engine::FontHandle consoleFont = context.assets.RequestFont(
            context.assets.GlobalScope(),
            "debug_console_inconsolata_22",
            ASSETS_PATH "fonts/Inconsolata.otf",
            22,
            engine::FontLoad_BilinearFilter);
    engine::DebugConsoleInitialize(debugConsole, consoleFont);
    usePromptFont = context.assets.RequestFont(
            context.assets.GlobalScope(),
            "game_use_prompt_ibm_plex_sans_bold_48",
            ASSETS_PATH "fonts/IBMPlexSans-Bold.ttf",
            48,
            engine::FontLoad_BilinearFilter);
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
    std::string itemError;
    if (!LoadItemRegistry(
                ASSETS_PATH "config/items.json",
                weaponRegistry,
                itemRegistry,
                itemError)) {
        menuStatus = itemError.empty()
                ? "Item registry initialization failed"
                : itemError;
        return false;
    }
    RebuildItemModelAssets(context.assets, itemRegistry, itemModelAssets);
    gameScene.SetItemRuntimeAssets(&itemRegistry, &itemModelAssets);
    RequestFpsWeaponAudioAssets(context.assets, weaponRegistry);
    RequestPlayerAudioAssets(
            context.assets,
            applicationSettings.playerSounds,
            playerAudio);
    std::string materialError;
    if (!LoadSectorMaterialRegistry(
                ASSETS_PATH "materials/materials.json",
                materialRegistry,
                materialError)) {
        menuStatus = materialError.empty()
                ? "Material registry initialization failed"
                : materialError;
        return false;
    }
    if (!editor.Init(context)) {
        menuStatus = "Editor initialization failed";
        return false;
    }
    ApplyPerspectiveFov();
    flow = ApplicationFlowState{};
    initialized = true;
    return true;
}

void GameApplication::UpdateMainThreadPreparation(
        engine::EngineContext& context)
{
    if (!initialized) return;
    UpdateItemIconPreparation(context.assets, itemRegistry, itemModelAssets);
    if (itemModelAssets.iconPreparation
            == ItemIconPreparationState::WaitingForModels) {
        itemIconDiagnosticReported = false;
    }
    if (itemModelAssets.iconPreparation == ItemIconPreparationState::Failed
            && !itemIconDiagnosticReported) {
        itemIconDiagnosticReported = true;
        const std::string diagnostic = itemModelAssets.iconDiagnostic.empty()
                ? "Item icon preparation failed"
                : itemModelAssets.iconDiagnostic;
        TraceLog(LOG_WARNING, "%s", diagnostic.c_str());
        if (menuStatus.empty()) menuStatus = diagnostic;
    }
}

bool GameApplication::IsGlobalPreparationFinished() const
{
    return IsItemIconPreparationTerminal(itemModelAssets);
}

void GameApplication::Shutdown(engine::EngineContext& context)
{
    CloseGameSaveMenu(saveMenu, context.assets);
    context.audio.StopAll(context.assets);
    if (gameSession.IsRunning()) {
        gameSession.Shutdown(context, gameScene);
    } else if (gameScene.IsReady()) {
        gameScene.Shutdown(context);
    }
    editor.Shutdown(context);
    ShutdownItemModelAssets(context.assets, itemModelAssets);
    if (debugConsole.initialized) {
        engine::FlushPendingDebugConsoleLogs(debugConsole);
        engine::SetDebugConsoleLogCaptureEnabled(false);
        engine::DebugConsoleShutdown(debugConsole);
    }
    flow = ApplicationFlowState{};
    applicationSettings = FpsApplicationSettings{};
    playerAudio = PlayerAudioRuntime{};
    persistentScripts = engine::PersistentScriptStore{};
    levelSaveStates.clear();
    saveRoot.clear();
    pendingSaveMenuAction.reset();
    pendingGameSave.reset();
    weaponRegistry = FpsWeaponRegistry{};
    itemRegistry = ItemRegistry{};
    itemCampaign = ItemCampaignState{};
    materialRegistry = SectorMaterialRegistry{};
    menuStatus.clear();
    pendingMenuAction.reset();
    pendingSettingsAction.reset();
    pendingGraphicsSettings.reset();
    graphicsSettingsDraft = FpsApplicationSettings{};
    graphicsSettingsOpen = false;
    editorAttachedToGame = false;
    itemIconDiagnosticReported = false;
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
    if (action.type == engine::DeferredDebugActionType::SetGodMode
            || action.type == engine::DeferredDebugActionType::SetInvisible
            || action.type == engine::DeferredDebugActionType::SetFreezeAi
            || action.type == engine::DeferredDebugActionType::SetDebugAi) {
        if (!gameSession.IsRunning()
                || action.mapId != gameSession.LevelName()) {
            engine::DebugConsoleAddLine(
                    debugConsole,
                    "debug toggle cancelled: the active game map changed",
                    engine::DebugConsoleSeverity::Error);
            return;
        }
        const bool current = action.type
                        == engine::DeferredDebugActionType::SetGodMode
                ? gameSession.GodMode()
                : action.type
                        == engine::DeferredDebugActionType::SetInvisible
                ? gameSession.Invisible()
                : action.type
                        == engine::DeferredDebugActionType::SetFreezeAi
                ? gameSession.AiFrozen()
                : gameSession.AiDebugVisible();
        const bool enabled = action.booleanMode
                        == engine::DeferredDebugBooleanMode::Toggle
                ? !current
                : action.booleanMode
                        == engine::DeferredDebugBooleanMode::Enable;
        if (action.type == engine::DeferredDebugActionType::SetGodMode) {
            gameSession.SetGodMode(enabled);
        } else if (action.type
                == engine::DeferredDebugActionType::SetInvisible) {
            gameSession.SetInvisible(enabled);
        } else if (action.type
                == engine::DeferredDebugActionType::SetFreezeAi) {
            gameSession.SetAiFrozen(enabled);
        } else {
            gameSession.SetAiDebugVisible(enabled);
        }
        engine::DebugConsoleAddLine(
                debugConsole,
                std::string{action.type
                                == engine::DeferredDebugActionType::SetGodMode
                            ? "god mode "
                            : action.type
                                    == engine::DeferredDebugActionType::SetInvisible
                            ? "invisibility "
                            : action.type
                                    == engine::DeferredDebugActionType::SetFreezeAi
                            ? "AI freeze " : "AI diagnostics "}
                        + (enabled ? "on" : "off"),
                engine::DebugConsoleSeverity::Success);
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
            "reload loading: " + gameSession.LevelName(),
            engine::DebugConsoleSeverity::Info);
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
    if (gameSession.IsLoadOverlayVisible()) return;
    if (flow.screen == ApplicationScreen::Editor) {
        editor.SetGameSessionExists(gameSession.IsRunning());
        editor.RenderUI(
                contentUi,
                config,
                input,
                assets,
                font,
                smallFont);
    }
    if (flow.screen == ApplicationScreen::Game) {
        gameSession.RenderInventoryUI(
                contentUi,
                config,
                input,
                assets,
                font,
                smallFont,
                usePromptFont);
        gameSession.RenderNavigationDebugPanel(
                config, assets, smallFont, gameScene);
        if (gameSession.IsGameOver()) {
            pendingGameOverMainMenu = DrawGameOverOverlay(
                    menuUi, config, input, assets, font, smallFont);
        }
    }
    if (flow.screen == ApplicationScreen::MainMenu) {
        if (saveMenu.mode != GameSaveMenuMode::Closed) {
            const GameSaveMenuAction action = DrawGameSaveMenu(
                    saveMenu,
                    menuUi,
                    config,
                    input,
                    assets,
                    font,
                    smallFont);
            if (action.type != GameSaveMenuActionType::None) {
                pendingSaveMenuAction = action;
            }
        } else if (graphicsSettingsOpen) {
            const GameGraphicsSettingsAction action = DrawGameGraphicsSettings(
                    menuUi, config, input, assets, font, smallFont,
                    graphicsSettingsDraft, menuStatus.c_str());
            if (action != GameGraphicsSettingsAction::None) {
                pendingSettingsAction = action;
            }
        } else {
            const bool regularGameplayMenu = flow.gameRunning
                    && flow.menuReturnScreen == ApplicationScreen::Game;
            const bool saveEnabled = regularGameplayMenu
                    && gameSession.CanSaveGame();
            const char* saveBlockedReason = !regularGameplayMenu
                    ? "Saving is only available during regular gameplay."
                    : gameSession.SaveGameBlockedReason().empty()
                    ? "Saving is currently unavailable."
                    : gameSession.SaveGameBlockedReason().c_str();
            pendingMenuAction = DrawGameMainMenu(
                    menuUi,
                    config,
                    input,
                    assets,
                    font,
                    smallFont,
                    flow.gameRunning,
                    saveEnabled,
                    saveBlockedReason,
                    menuStatus.c_str());
        }
    }
}

void GameApplication::Update(engine::EngineContext& context, float dt)
{
    if (!initialized) {
        return;
    }
    if (pendingGameOverMainMenu) {
        pendingGameOverMainMenu = false;
        EndGameToMainMenu(context);
        return;
    }
    if (pendingSaveMenuAction.has_value()) {
        const GameSaveMenuAction action = std::move(*pendingSaveMenuAction);
        pendingSaveMenuAction.reset();
        HandleSaveMenuAction(context, action);
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

    if (gameSession.IsLoading()) {
        gameSession.Update(context, gameScene, dt);
        const std::string loadFailure = gameSession.TakeFailureError();
        if (!loadFailure.empty()) {
            menuStatus = loadFailure;
            debugConsole.open = false;
            gameSession.SetConsoleInputCaptured(false);
            MarkApplicationGameStopped(flow);
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
                    if (saveMenu.mode != GameSaveMenuMode::Closed) {
                        if (saveMenu.confirmationOpen) {
                            saveMenu.confirmationOpen = false;
                        } else {
                            CloseGameSaveMenu(saveMenu, context.assets);
                        }
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
        if (gameSession.IsGameOver()) return;
        bool menuRequested = false;
        context.input.ForEachEvent(
                engine::InputEventType::KeyPressed,
                true,
                [this, &menuRequested](engine::InputEvent& event) {
                    if (event.key.key != KEY_ESCAPE) {
                        return;
                    }
                    if (gameSession.HandleEscape()) {
                        engine::ConsumeEvent(event);
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

    editor.SetGameSessionExists(gameSession.IsRunning());
    if (editor.ConsumeClearGameSessionRequest()) {
        ClearGameSession(context);
    }
    editor.Update(context, dt);
    if (editor.ConsumePlayerAudioSettingsChanged()) {
        RequestPlayerAudioAssets(
                context.assets,
                applicationSettings.playerSounds,
                playerAudio);
    }
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
    if (gameSession.IsLoadScreenOpaque()) {
        return ApplicationContentKind::Empty;
    }
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

float GameApplication::EditorUiTopInset() const
{
    return BackgroundScreen() == ApplicationScreen::Editor
            ? editor.VisibleMainMenuHeight()
            : 0.0f;
}

bool GameApplication::ShouldRefreshBackground() const
{
    if (gameSession.IsLoadScreenOpaque()) return false;
    if (gameSession.IsLoadScreenFading()) return true;
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
                applicationSettings.graphics.shadowQuality != FpsShadowQuality::Off,
                ShadowMapResolution(applicationSettings.graphics.shadowQuality),
                applicationSettings.graphics.maxDynamicLights,
                applicationSettings.graphics.maxShadowLightUpdatesPerFrame,
                applicationSettings.graphics.depthPrepass,
                applicationSettings.graphics.dynamicLightFadeInSeconds);
        gameScene.RenderShadowMaps(context);
    } else {
        editor.SetPreviewGraphicsQuality(
                applicationSettings.graphics.shadowQuality != FpsShadowQuality::Off,
                ShadowMapResolution(applicationSettings.graphics.shadowQuality),
                applicationSettings.graphics.maxDynamicLights,
                applicationSettings.graphics.maxShadowLightUpdatesPerFrame,
                applicationSettings.graphics.depthPrepass,
                applicationSettings.graphics.dynamicLightFadeInSeconds);
        editor.RenderPreview3DShadowMaps(context.assets);
    }
}

void GameApplication::Render3DScene(engine::EngineContext& context)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameScene.RenderScene(
                context,
                gameSession.Map(),
                true,
                gameSession.UseHighlight());
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

void GameApplication::Apply3DTransparentSurfaces(
        engine::RenderTarget& sceneTarget,
        engine::EngineContext& context,
        bool collectGpuDiagnostics)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameScene.ApplyTransparentSurfaces(
                sceneTarget, context, gameSession.Map(), collectGpuDiagnostics);
    } else {
        editor.ApplyPreview3DTransparentSurfaces(
                sceneTarget, context, collectGpuDiagnostics);
    }
}

bool GameApplication::Prepare3DOverlayPass(
        engine::RenderTarget& sceneTarget)
{
    if (flow.screen == ApplicationScreen::Game) {
        return gameScene.PreparePostBloomWorldOverlays(
                sceneTarget,
                gameSession.HasWorldDebugOverlays());
    }
    return BackgroundScreen() == ApplicationScreen::Editor;
}

void GameApplication::Render3DOverlays(const engine::World& world)
{
    if (flow.screen == ApplicationScreen::Game) {
        gameSession.RenderNavigationDebugWorld(gameScene);
        gameSession.RenderAiDebugWorld(world, gameScene);
    } else if (BackgroundScreen() == ApplicationScreen::Editor) {
        editor.RenderPreview3DOverlays();
    }
}

void GameApplication::Apply3DWorldAtmosphere(
        engine::RenderTarget& sceneTarget,
        bool collectGpuDiagnostics)
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameScene.ApplyWorldAtmosphere(
                sceneTarget,
                gameSession.Map(),
                collectGpuDiagnostics);
    } else {
        editor.ApplyPreview3DWorldAtmosphere(
                sceneTarget,
                collectGpuDiagnostics);
    }
}

const SectorAtmosphereDiagnostics& GameApplication::AtmosphereDiagnostics() const
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        return gameScene.Renderer().AtmosphereDiagnostics();
    }
    return editor.PreviewAtmosphereDiagnostics();
}

engine::ScenePresentationEffectParameters
GameApplication::ScenePresentationEffects() const
{
    engine::ScenePresentationEffectParameters result;
    if (BackgroundScreen() != ApplicationScreen::Game
            || !gameSession.IsRunning()
            || IsSectorBloomDiagnosticView(
                    gameScene.Renderer().BloomDebugView())) {
        return result;
    }

    const PlayerLowHealthVisualApplicationSettings& settings =
            applicationSettings.playerHealth.lowHealthVisual;
    const float strength = PlayerLowHealthVisualStrength(
            gameSession.PlayerHealth(),
            settings);
    const Vector4 vignetteColor = engine::SrgbColorBytesToLinearSceneRgba(
            settings.vignetteColor);
    result.desaturation = settings.maximumDesaturation * strength;
    result.vignetteOpacity = PlayerLowHealthVignetteOpacity(
            gameSession.PlayerHealth(), settings);
    result.vignetteColorLinear = {
            vignetteColor.x,
            vignetteColor.y,
            vignetteColor.z};
    result.vignetteInnerRadius = settings.vignetteInnerRadius;
    result.vignetteOuterRadius = settings.vignetteOuterRadius;
    return result;
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

float GameApplication::WorldFadeOpacity() const
{
    return BackgroundScreen() == ApplicationScreen::Game
            ? gameSession.WorldFadeOpacity() : 0.0f;
}

void GameApplication::Render3DHud(
        const engine::World& world,
        engine::AssetManager& assets,
        engine::FontHandle font,
        Rectangle playableViewport) const
{
    if (BackgroundScreen() == ApplicationScreen::Game) {
        gameSession.RenderHud(
                assets, font, usePromptFont, playableViewport);
        if (flow.screen == ApplicationScreen::Game) {
            gameSession.RenderAiDebugHud(
                    world, assets, font, playableViewport, gameScene);
        }
    } else {
        editor.RenderPreview3DHud(
                assets, usePromptFont, playableViewport);
    }
}

void GameApplication::RenderLoadingOverlay(
        Rectangle presentationViewport,
        int outputWidth,
        int outputHeight) const
{
    if (!gameSession.IsLoadOverlayVisible()) return;
    const float opacity = gameSession.LoadOverlayOpacity();
    if (opacity <= 0.0f) return;

    DrawRectangle(
            0,
            0,
            std::max(0, outputWidth),
            std::max(0, outputHeight),
            Fade(BLACK, opacity));
    const float barWidth = std::max(1.0f, presentationViewport.width * 0.40f);
    const float barHeight = std::max(
            1.0f,
            presentationViewport.height * (14.0f / 1080.0f));
    const Rectangle track{
            presentationViewport.x
                    + (presentationViewport.width - barWidth) * 0.5f,
            presentationViewport.y
                    + (presentationViewport.height - barHeight) * 0.5f,
            barWidth,
            barHeight};
    DrawRectangleRec(track, Fade(Color{20, 28, 40, 255}, opacity));
    Rectangle fill = track;
    fill.width *= std::clamp(gameSession.LoadProgress(), 0.0f, 1.0f);
    DrawRectangleRec(fill, Fade(Color{40, 120, 255, 255}, opacity));
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
            OpenGameSaveMenu(
                    saveMenu,
                    GameSaveMenuMode::Load,
                    flow.gameRunning,
                    saveRoot,
                    context.assets);
            menuStatus.clear();
            break;
        case MainMenuAction::SaveGame: {
            const bool regularGameplayMenu = flow.gameRunning
                    && flow.menuReturnScreen == ApplicationScreen::Game;
            if (!regularGameplayMenu || !gameSession.CanSaveGame()) {
                menuStatus = !regularGameplayMenu
                        ? "Saving is only available during regular gameplay."
                        : gameSession.SaveGameBlockedReason().empty()
                        ? "Saving is currently unavailable."
                        : gameSession.SaveGameBlockedReason();
                break;
            }
            OpenGameSaveMenu(
                    saveMenu,
                    GameSaveMenuMode::Save,
                    false,
                    saveRoot,
                    context.assets);
            menuStatus.clear();
            break;
        }
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

void GameApplication::HandleSaveMenuAction(
        engine::EngineContext& context,
        const GameSaveMenuAction& action)
{
    switch (action.type) {
        case GameSaveMenuActionType::None:
            return;
        case GameSaveMenuActionType::Back:
            CloseGameSaveMenu(saveMenu, context.assets);
            menuStatus.clear();
            return;
        case GameSaveMenuActionType::Save:
            if (flow.menuReturnScreen != ApplicationScreen::Game
                    || !gameSession.CanSaveGame()) {
                saveMenu.status = gameSession.SaveGameBlockedReason().empty()
                        ? "Saving is currently unavailable."
                        : gameSession.SaveGameBlockedReason();
                return;
            }
            if (action.slot < 1 || action.slot > GameSaveSlotCount
                    || !IsValidGameSaveName(action.name)) {
                saveMenu.status = "Select a slot and enter a valid save name.";
                return;
            }
            pendingGameSave = PendingGameSave{action.slot, action.name};
            CloseGameSaveMenu(saveMenu, context.assets);
            menuStatus = "Saving...";
            return;
        case GameSaveMenuActionType::Load:
            LoadGameFromSlot(context, action.slot);
            return;
    }
}

void GameApplication::LoadGameFromSlot(
        engine::EngineContext& context,
        int slot)
{
    GameSaveData candidate;
    std::string error;
    if (!LoadGameSaveSlot(saveRoot, slot, candidate, error)) {
        saveMenu.status = error.empty() ? "Could not read the save game" : error;
        return;
    }

    const std::string levelPath = ApplicationLevelAssetPath(
            candidate.currentLevelId);
    SectorTopologyMap preflightMap;
    if (levelPath.empty() || !LoadSectorRuntimeLevel(
                levelPath, materialRegistry, preflightMap, error)) {
        saveMenu.status = error.empty()
                ? "The saved level is unavailable" : error;
        return;
    }

    CloseGameSaveMenu(saveMenu, context.assets);
    context.audio.StopAll(context.assets);
    editor.SuspendRuntime(context);
    if (gameSession.IsRunning()) {
        gameSession.Shutdown(context, gameScene);
    } else if (gameScene.IsReady()) {
        gameScene.Shutdown(context);
    }

    itemCampaign = std::move(candidate.itemCampaign);
    persistentScripts = std::move(candidate.persistentScripts);
    levelSaveStates = std::move(candidate.levels);
    if (!gameSession.StartNew(
                context,
                gameScene,
                SectorLevelEntryRequest{candidate.currentLevelId, std::nullopt},
                materialRegistry,
                weaponRegistry,
                itemRegistry,
                itemModelAssets,
                itemCampaign,
                applicationSettings,
                playerAudio,
                persistentScripts,
                levelSaveStates,
                true,
                &candidate.player,
                true,
                error)) {
        menuStatus = error.empty() ? "Could not load the save game" : error;
        itemCampaign = ItemCampaignState{};
        persistentScripts = engine::PersistentScriptStore{};
        levelSaveStates.clear();
        editorAttachedToGame = false;
        editor.SetGameSessionExists(false);
        MarkApplicationGameStopped(flow);
        return;
    }
    editorAttachedToGame = false;
    editor.SetGameSessionExists(true);
    debugConsole.open = false;
    menuStatus.clear();
    MarkApplicationGameStarted(flow);
}

void GameApplication::ProcessPendingGameSave(
        engine::EngineContext& context,
        const Texture2D& scenePresentationTexture)
{
    if (!pendingGameSave.has_value()) return;
    const PendingGameSave request = std::move(*pendingGameSave);
    pendingGameSave.reset();

    if (flow.menuReturnScreen != ApplicationScreen::Game
            || !gameSession.CanSaveGame()) {
        menuStatus = gameSession.SaveGameBlockedReason().empty()
                ? "Saving is currently unavailable."
                : gameSession.SaveGameBlockedReason();
        return;
    }
    if (!gameSession.CaptureCurrentLevelSaveState(context, gameScene)) {
        menuStatus = "Could not capture the current level state.";
        return;
    }

    GameSaveData previous;
    std::string ignoredError;
    const bool hadPrevious = LoadGameSaveSlot(
            saveRoot, request.slot, previous, ignoredError);

    GameSaveData save;
    save.slot = request.slot;
    save.name = request.name;
    save.savedAtUtc = CurrentGameSaveTimestampUtc();
    save.currentLevelId = gameSession.LevelName();
    save.player = gameSession.CapturePlayerSaveState();
    save.itemCampaign = itemCampaign;
    save.persistentScripts = persistentScripts;
    save.levels = levelSaveStates;

    std::filesystem::path newThumbnail;
    if (IsTextureValid(scenePresentationTexture)) {
        std::error_code filesystemError;
        std::filesystem::create_directories(saveRoot, filesystemError);
        if (!filesystemError) {
            const long long generation = std::chrono::duration_cast<
                    std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            char filename[80]{};
            std::snprintf(filename, sizeof(filename),
                    "slot%02d_%lld.png", request.slot, generation);
            newThumbnail = saveRoot / filename;
            const std::filesystem::path temporary =
                    newThumbnail.string() + ".tmp.png";
            Image image = LoadImageFromTexture(scenePresentationTexture);
            if (IsImageValid(image)) {
                ImageFlipVertical(&image);
                ImageResize(&image, GameSaveThumbnailWidth,
                        GameSaveThumbnailHeight);
                if (ExportImage(image, temporary.string().c_str())) {
                    std::filesystem::rename(
                            temporary, newThumbnail, filesystemError);
                    if (!filesystemError) save.thumbnailFile = filename;
                }
                UnloadImage(image);
            }
            if (save.thumbnailFile.empty()) {
                std::filesystem::remove(temporary, filesystemError);
                newThumbnail.clear();
                TraceLog(LOG_WARNING,
                        "Could not write the save-game thumbnail");
            }
        }
    }

    std::string error;
    if (!WriteGameSaveSlot(saveRoot, save, error)) {
        if (!newThumbnail.empty()) {
            std::error_code filesystemError;
            std::filesystem::remove(newThumbnail, filesystemError);
        }
        menuStatus = error.empty() ? "Could not write the save game" : error;
        return;
    }

    if (hadPrevious && !previous.thumbnailFile.empty()
            && previous.thumbnailFile != save.thumbnailFile
            && previous.thumbnailFile.find('/') == std::string::npos
            && previous.thumbnailFile.find('\\') == std::string::npos) {
        std::error_code filesystemError;
        std::filesystem::remove(
                saveRoot / previous.thumbnailFile, filesystemError);
    }
    menuStatus = "Saved to slot " + std::to_string(request.slot) + ".";
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
    levelSaveStates.clear();
    InitializeItemCampaignState(
            itemCampaign,
            applicationSettings.playerInventory,
            &weaponRegistry);
    if (!gameSession.StartNew(
                context,
                gameScene,
                SectorLevelEntryRequest{applicationSettings.firstLevel, std::nullopt},
                materialRegistry,
                weaponRegistry,
                itemRegistry,
                itemModelAssets,
                itemCampaign,
                applicationSettings,
                playerAudio,
                persistentScripts,
                levelSaveStates,
                false,
                nullptr,
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
    if (ShouldRebuildGameFromEditorOnResume(flow, editorAttachedToGame)) {
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

void GameApplication::ClearGameSession(engine::EngineContext& context)
{
    if (!gameSession.IsRunning()) return;

    context.audio.StopAll(context.assets);
    gameSession.Shutdown(context, gameScene);
    itemCampaign = ItemCampaignState{};
    persistentScripts = engine::PersistentScriptStore{};
    levelSaveStates.clear();
    editorAttachedToGame = false;
    editor.SetGameSessionExists(false);
    debugConsole.open = false;
    menuStatus.clear();
    MarkApplicationGameStopped(flow);
    ShowApplicationEditor(flow);
}

void GameApplication::EndGameToMainMenu(engine::EngineContext& context)
{
    if (!gameSession.IsRunning()) return;
    context.audio.StopAll(context.assets);
    gameSession.Shutdown(context, gameScene);
    itemCampaign = ItemCampaignState{};
    persistentScripts = engine::PersistentScriptStore{};
    levelSaveStates.clear();
    editorAttachedToGame = false;
    editor.SetGameSessionExists(false);
    debugConsole.open = false;
    menuStatus.clear();
    MarkApplicationGameStopped(flow);
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
    if (gameSession.IsLoadOverlayVisible()) return false;
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
