#pragma once

#include "engine/EngineContext.h"
#include "game/GameLevelLoading.h"
#include "game/Health.h"
#include "game/SectorLevelLoader.h"
#include "game/FpsPlayerRuntime.h"
#include "game/items/ItemAssets.h"
#include "game/items/ItemInventory.h"
#include "game/items/ItemInventoryUI.h"
#include "game/PlayerAudio.h"
#include "game/PlayerHitCamera.h"
#include "game/SectorScriptBindings.h"
#include "game/SectorGameNavigationDebug.h"
#include "engine/scripting/ScriptSystem.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_demo/SectorSceneRuntime.h"
#include "sector_demo/SectorUseInteraction.h"

#include <string>
#include <array>
#include <vector>

namespace game {

class SectorGameSession {
public:
    bool StartNew(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            const SectorLevelEntryRequest& entry,
            const SectorMaterialRegistry& materialRegistry,
            const FpsWeaponRegistry& weaponRegistry,
            const ItemRegistry& itemRegistry,
            const ItemModelAssetState& itemModelAssets,
            ItemCampaignState& itemCampaign,
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
    void RenderAiDebugWorld(
            const engine::World& world,
            const SectorSceneRuntime& scene) const;
    void RenderAiDebugHud(
            const engine::World& world,
            engine::AssetManager& assets,
            engine::FontHandle font,
            Rectangle playableViewport,
            const SectorSceneRuntime& scene) const;
    void RenderNavigationDebugPanel(
            const engine::UIConfig& config,
            engine::AssetManager& assets,
            engine::FontHandle smallFont,
            const SectorSceneRuntime& scene) const;
    void RenderInventoryUI(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont,
            engine::FontHandle usePromptFont);
    bool HandleEscape();
    bool IsInventoryOpen() const { return inventoryUi.open; }

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
    SectorUseHighlight UseHighlight() const
    {
        return useHighlightState.highlight;
    }
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
    void SetGodMode(bool enabled);
    void SetAiFrozen(bool frozen) { aiFrozen = frozen; }
    void SetAiDebugVisible(bool visible) { aiDebugVisible = visible; }
    bool GodMode() const { return godMode; }
    bool AiFrozen() const { return aiFrozen; }
    bool AiDebugVisible() const { return aiDebugVisible; }
    bool HasWorldDebugOverlays() const {
        return IsActive() && (navigationDebug.visible || aiDebugVisible);
    }
    bool IsGameOver() const { return gameOver; }

private:
    struct PendingItemTake {
        engine::ScriptTaskHandle task{};
        engine::Entity entity = engine::NullEntity();
        int placedObjectId = 0;
        std::string instanceId;
        bool active = false;
    };

    struct HeldObjectUseState {
        ItemHeldUsePhase phase = ItemHeldUsePhase::Inactive;
        std::uint64_t runtimeId = 0;
        engine::ScriptTaskHandle task{};
    };

    bool RequestItemTake(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            engine::Entity entity);
    bool CommitItemTake(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            engine::Entity entity,
            int placedObjectId,
            const std::string& instanceId);
    void UpdatePendingItemTake(
            engine::EngineContext& context,
            SectorSceneRuntime& scene);
    void ShowCarryRefusal();
    void ShowDropRefusal();
    void ShowOutOfAmmo();
    void RefreshMouseLookCapture();
    void SetInventoryOpen(bool open);
    void ClearHeldObjectUse();
    bool BeginHeldObjectUse(std::uint64_t runtimeId);
    void InvokeHeldObjectUse(engine::EngineContext& context);
    void UpdatePendingHeldObjectUse();
    bool ConsumeHeldObjectEntry(std::uint64_t runtimeId);
    void ProcessInventoryAction(
            engine::EngineContext& context,
            SectorSceneRuntime& scene);
    bool DropInventoryEntry(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            std::uint64_t runtimeId,
            std::size_t& affectedIndex);
    void UpdateItemPresentations(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            float dt);
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
    const ItemRegistry* itemRegistry = nullptr;
    const ItemModelAssetState* itemModelAssets = nullptr;
    ItemCampaignState* itemCampaign = nullptr;
    const SectorMaterialRegistry* materialRegistry = nullptr;
    const FpsApplicationSettings* applicationSettings = nullptr;
    PlayerAudioRuntime* playerAudio = nullptr;
    engine::PersistentScriptStore* persistentScripts = nullptr;
    engine::ScriptRuntime scripts;
    SectorScriptHost scriptHost;
    SectorGameNavigationDebugState navigationDebug;
    Health playerHealth = MakeHealth(100);
    Vector2 playerKnockbackVelocity{};
    float playerStunRemainingSeconds = 0.0f;
    bool godMode = false;
    bool aiFrozen = false;
    bool aiDebugVisible = false;
    bool gameOver = false;
    PlayerStamina playerStamina;
    PlayerWindedCameraState windedCamera;
    PlayerLowHealthCameraState lowHealthCamera;
    PlayerHitCameraState hitCamera;
    PlayerBreathingAudioRuntime breathingAudio;
    PlayerHeartbeatAudioRuntime heartbeatAudio;
    std::string failureError;
    SectorUseTarget useTarget;
    SectorUseHighlightState useHighlightState;
    std::array<char, 128> usePromptTitle{};
    std::array<char, 128> itemMessage{};
    float itemMessageElapsedSeconds = 0.0f;
    PendingItemTake pendingItemTake;
    ItemInventoryUIState inventoryUi;
    ItemInventoryUIAction pendingInventoryAction;
    HeldObjectUseState heldObjectUse;
    std::vector<engine::Entity> completedItemPresentations;
    Rectangle logicalViewport = {0.0f, 0.0f, 1920.0f, 1080.0f};
};

} // namespace game
