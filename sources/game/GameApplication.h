#pragma once

#include "engine/EngineContext.h"
#include "engine/debug/DebugConsoleData.h"
#include "engine/ui/UI.h"
#include "engine/scripting/ScriptData.h"
#include "game/ApplicationFlow.h"
#include "game/FpsWeaponRegistry.h"
#include "game/GameMainMenu.h"
#include "game/PlayerAudio.h"
#include "game/SectorGameSession.h"
#include "sector_editor/SectorEditor.h"

#include <optional>
#include <string>

namespace game {

enum class ApplicationContentKind {
    Empty,
    Editor2D,
    Sector3D
};

class GameApplication {
public:
    GameApplication() : editor(applicationSettings) {}

    bool Init(
            engine::EngineContext& context,
            FpsApplicationSettings initialSettings,
            std::string settingsLoadError);
    void Shutdown(engine::EngineContext& context);

    void RenderInteractiveUI(
            engine::UIContext& contentUi,
            engine::UIContext& menuUi,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void Update(engine::EngineContext& context, float dt);
    void UpdateDebugConsole(engine::EngineContext& context, float dt);
    void ProcessDeferredDebugActions(engine::EngineContext& context);
    void RenderDebugConsole(
            engine::AssetManager& assets,
            int logicalWidth,
            int logicalHeight);

    ApplicationContentKind BackgroundContentKind() const;
    bool ShouldRefreshBackground() const;
    bool IsMenuOpen() const;
    bool QuitRequested() const { return flow.quitRequested; }
    const FpsApplicationSettings& ApplicationSettings() const {
        return applicationSettings;
    }
    const FpsApplicationSettings* PendingGraphicsSettings() const;
    bool CommitPendingGraphicsSettings(std::string& error);
    void RejectPendingGraphicsSettings(const std::string& error);
    void TogglePerformanceOverlay();

    void Render2D(engine::AssetManager& assets);
    void Render3DShadowMaps(engine::EngineContext& context);
    void Render3DScene(engine::EngineContext& context);
    void Render3DViewmodel(engine::AssetManager& assets);
    void Render3DOverlays();
    void Apply3DWorldAtmosphere(engine::RenderTarget& sceneTarget);
    void Apply3DHdrBloom(engine::RenderTarget& sceneTarget);
    bool Composite3DViewmodel(
            engine::RenderTarget& sceneTarget,
            const engine::RenderTarget& viewmodelTarget);
    const engine::RenderTarget* HdrDebugPresentationSource() const;
    void Render3DHud(Rectangle playableViewport) const;
    void RenderLoadingOverlay(
            Rectangle presentationViewport,
            int outputWidth,
            int outputHeight) const;

private:
    void HandleMenuAction(
            engine::EngineContext& context,
            MainMenuAction action);
    void StartNewGame(engine::EngineContext& context);
    void ResumeGame(engine::EngineContext& context);
    void OpenEditor(engine::EngineContext& context);
    ApplicationScreen BackgroundScreen() const;
    bool DebugConsoleAvailable() const;
    void ApplyPerspectiveFov();

    ApplicationFlowState flow;
    FpsApplicationSettings applicationSettings;
    SectorEditor editor;
    SectorGameSession gameSession;
    SectorSceneRuntime gameScene;
    FpsWeaponRegistry weaponRegistry;
    PlayerAudioRuntime playerAudio;
    engine::PersistentScriptStore persistentScripts;
    engine::DebugConsoleData debugConsole;
    std::string menuStatus;
    std::optional<MainMenuAction> pendingMenuAction;
    std::optional<GameGraphicsSettingsAction> pendingSettingsAction;
    std::optional<FpsApplicationSettings> pendingGraphicsSettings;
    FpsApplicationSettings graphicsSettingsDraft;
    bool graphicsSettingsOpen = false;
    bool editorAttachedToGame = false;
    bool initialized = false;
};

} // namespace game
