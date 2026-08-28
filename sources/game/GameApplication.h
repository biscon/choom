#pragma once

#include "engine/EngineContext.h"
#include "engine/debug/DebugConsoleData.h"
#include "engine/ui/UI.h"
#include "engine/scripting/ScriptData.h"
#include "engine/render/ScenePresentationShader.h"
#include "game/ApplicationFlow.h"
#include "game/FpsWeaponRegistry.h"
#include "game/items/ItemAssets.h"
#include "game/items/ItemDefinitions.h"
#include "game/items/ItemInventory.h"
#include "game/GameMainMenu.h"
#include "game/PlayerAudio.h"
#include "game/SectorGameSession.h"
#include "game/save/GameSaveData.h"
#include "game/save/GameSaveMenu.h"
#include "sector_editor/SectorEditor.h"
#include "sector_demo/SectorMaterialRegistry.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace game {

enum class ApplicationContentKind {
    Empty,
    Editor2D,
    Sector3D
};

class GameApplication {
public:
    GameApplication()
        : editor(
                applicationSettings,
                materialRegistry,
                itemRegistry,
                itemModelAssets) {}

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
    void UpdateMainThreadPreparation(engine::EngineContext& context);
    bool IsGlobalPreparationFinished() const;
    void UpdateDebugConsole(engine::EngineContext& context, float dt);
    void ProcessDeferredDebugActions(engine::EngineContext& context);
    void RenderDebugConsole(
            engine::AssetManager& assets,
            int logicalWidth,
            int logicalHeight);

    ApplicationContentKind BackgroundContentKind() const;
    float EditorUiTopInset() const;
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
    bool Prepare3DOverlayPass(engine::RenderTarget& sceneTarget);
    void Render3DOverlays(const engine::World& world);
    void Apply3DWorldAtmosphere(
            engine::RenderTarget& sceneTarget,
            bool collectGpuDiagnostics = false);
    void Apply3DHdrBloom(engine::RenderTarget& sceneTarget);
    bool Composite3DViewmodel(
            engine::RenderTarget& sceneTarget,
            const engine::RenderTarget& viewmodelTarget);
    const engine::RenderTarget* HdrDebugPresentationSource() const;
    const SectorAtmosphereDiagnostics& AtmosphereDiagnostics() const;
    engine::ScenePresentationEffectParameters ScenePresentationEffects() const;
    float WorldFadeOpacity() const;
    void Render3DHud(
            const engine::World& world,
            engine::AssetManager& assets,
            engine::FontHandle font,
            Rectangle playableViewport) const;
    void RenderLoadingOverlay(
            Rectangle presentationViewport,
            int outputWidth,
            int outputHeight) const;
    void ProcessPendingGameSave(
            engine::EngineContext& context,
            const Texture2D& scenePresentationTexture);

private:
    void HandleMenuAction(
            engine::EngineContext& context,
            MainMenuAction action);
    void HandleSaveMenuAction(
            engine::EngineContext& context,
            const GameSaveMenuAction& action);
    void LoadGameFromSlot(engine::EngineContext& context, int slot);
    void StartNewGame(engine::EngineContext& context);
    void ResumeGame(engine::EngineContext& context);
    void ClearGameSession(engine::EngineContext& context);
    void EndGameToMainMenu(engine::EngineContext& context);
    void OpenEditor(engine::EngineContext& context);
    ApplicationScreen BackgroundScreen() const;
    bool DebugConsoleAvailable() const;
    void ApplyPerspectiveFov();

    ApplicationFlowState flow;
    FpsApplicationSettings applicationSettings;
    SectorMaterialRegistry materialRegistry;
    ItemRegistry itemRegistry;
    ItemModelAssetState itemModelAssets;
    ItemCampaignState itemCampaign;
    SectorEditor editor;
    SectorGameSession gameSession;
    SectorSceneRuntime gameScene;
    FpsWeaponRegistry weaponRegistry;
    PlayerAudioRuntime playerAudio;
    engine::PersistentScriptStore persistentScripts;
    std::vector<GameSaveLevelState> levelSaveStates;
    std::filesystem::path saveRoot;
    GameSaveMenuState saveMenu;
    std::optional<GameSaveMenuAction> pendingSaveMenuAction;
    struct PendingGameSave {
        int slot = 0;
        std::string name;
    };
    std::optional<PendingGameSave> pendingGameSave;
    engine::DebugConsoleData debugConsole;
    std::string menuStatus;
    std::optional<MainMenuAction> pendingMenuAction;
    std::optional<GameGraphicsSettingsAction> pendingSettingsAction;
    std::optional<FpsApplicationSettings> pendingGraphicsSettings;
    FpsApplicationSettings graphicsSettingsDraft;
    bool graphicsSettingsOpen = false;
    bool pendingGameOverMainMenu = false;
    bool editorAttachedToGame = false;
    engine::FontHandle usePromptFont = engine::NullFontHandle();
    bool initialized = false;
    bool itemIconDiagnosticReported = false;
};

} // namespace game
