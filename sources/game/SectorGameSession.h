#pragma once

#include "engine/EngineContext.h"
#include "game/GameLevelLoading.h"
#include "game/Health.h"
#include "game/SectorLevelLoader.h"
#include "game/FpsPlayerRuntime.h"
#include "game/PlayerAudio.h"
#include "game/SectorScriptBindings.h"
#include "game/SectorGameNavigationDebug.h"
#include "engine/scripting/ScriptSystem.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_demo/SectorSceneRuntime.h"
#include "sector_demo/SectorUseInteraction.h"

#include <string>
#include <array>

namespace game {

class SectorGameSession {
public:
    bool StartNew(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            const SectorLevelEntryRequest& entry,
            const SectorMaterialRegistry& materialRegistry,
            const FpsWeaponRegistry& weaponRegistry,
            const FpsApplicationSettings& applicationSettings,
            PlayerAudioRuntime& playerAudioRuntime,
            engine::PersistentScriptStore& persistentScripts,
            bool loadingSave,
            std::string& error);
    void Shutdown(engine::EngineContext& context, SectorSceneRuntime& scene);
    void SuspendForEditor(engine::EngineContext& context);

    void Pause();
    void Resume(SectorSceneRuntime& scene);
    void SetConsoleInputCaptured(bool captured);
    void Update(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            float dt);
    void RenderViewmodel(
            engine::AssetManager& assets,
            SectorSceneRuntime& scene);
    void RenderHud(
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle usePromptFont,
            Rectangle playableViewport) const;
    void RenderNavigationDebugWorld(const SectorSceneRuntime& scene) const;
    void RenderNavigationDebugPanel(
            const engine::UIConfig& config,
            engine::AssetManager& assets,
            engine::FontHandle smallFont,
            const SectorSceneRuntime& scene) const;

    bool RebuildFromMap(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            const SectorTopologyMap& map,
            std::string& error);
    bool ReloadCurrentMap(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            bool remainPaused,
            std::string& error);

    bool IsRunning() const { return running; }
    bool IsActive() const {
        return loading.phase == GameLevelLoadPhase::Active;
    }
    bool IsLoading() const { return IsGameLevelLoading(loading); }
    bool IsLoadOverlayVisible() const {
        return IsGameLevelLoadOverlayVisible(loading);
    }
    bool IsLoadScreenOpaque() const {
        return loading.phase == GameLevelLoadPhase::Loading;
    }
    bool IsLoadScreenFading() const {
        return loading.phase == GameLevelLoadPhase::Fading;
    }
    float LoadProgress() const { return loading.displayedProgress; }
    float LoadOverlayOpacity() const {
        return GameLevelLoadOverlayOpacity(loading);
    }
    std::string TakeFailureError();
    const SectorTopologyMap& Map() const { return topologyMap; }
    const std::string& LevelName() const { return levelName; }
    const std::string& LevelPath() const { return levelPath; }
    engine::ScriptRuntime* ConsoleScriptRuntime()
    {
        return IsActive() ? &scripts : nullptr;
    }
    int CurrentSectorId() const
    {
        return controller.fpsControllerState.currentSectorId;
    }
    const Health& PlayerHealth() const { return playerHealth; }
    const PlayerStamina& PlayerStaminaState() const { return playerStamina; }

private:
    bool BuildCollisionAndPlayer(
            SectorSceneRuntime& scene,
            bool initializePlayer,
            const SectorCompiledLevelMarker* entryMarker,
            std::string* error = nullptr);
    void ApplyPlayerPose(SectorSceneRuntime& scene);
    void ConsumeScriptTransitionRequest(
            engine::EngineContext& context,
            SectorSceneRuntime& scene);
    void UpdateLoading(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            float dt);
    bool ActivateLoadedMap(
            engine::EngineContext& context,
            std::string& error);

    SectorTopologyMap topologyMap;
    SectorEditorPreviewControllerState controller;
    SectorEditorPreviewCollisionState collision;
    std::string levelName;
    std::string levelPath;
    bool running = false;
    bool paused = false;
    bool consoleInputCaptured = false;
    bool pendingLoadingSave = false;
    GameLevelLoadingState loading;
    FpsPlayerRuntime fpsPlayer;
    const FpsWeaponRegistry* weaponRegistry = nullptr;
    const SectorMaterialRegistry* materialRegistry = nullptr;
    const FpsApplicationSettings* applicationSettings = nullptr;
    PlayerAudioRuntime* playerAudio = nullptr;
    engine::PersistentScriptStore* persistentScripts = nullptr;
    engine::ScriptRuntime scripts;
    SectorScriptHost scriptHost;
    SectorGameNavigationDebugState navigationDebug;
    Health playerHealth = MakeHealth(100);
    PlayerStamina playerStamina;
    PlayerWindedCameraState windedCamera;
    PlayerBreathingAudioRuntime breathingAudio;
    std::string failureError;
    SectorUseTarget useTarget;
    std::array<char, 128> usePromptTitle{};
};

} // namespace game
