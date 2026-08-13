#pragma once

#include "engine/EngineContext.h"
#include "game/SectorLevelLoader.h"
#include "game/FpsPlayerRuntime.h"
#include "game/PlayerAudio.h"
#include "game/SectorScriptBindings.h"
#include "engine/scripting/ScriptSystem.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_demo/SectorSceneRuntime.h"

#include <string>

namespace game {

class SectorGameSession {
public:
    bool StartNew(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            const SectorLevelEntryRequest& entry,
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
    void Update(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            float dt);
    void RenderViewmodel(
            engine::AssetManager& assets,
            SectorSceneRuntime& scene);
    void RenderHud(Rectangle playableViewport) const;

    bool RebuildFromMap(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            const SectorTopologyMap& map,
            std::string& error);

    bool IsRunning() const { return running; }
    std::string TakeFailureError();
    const SectorTopologyMap& Map() const { return topologyMap; }
    const std::string& LevelName() const { return levelName; }
    const std::string& LevelPath() const { return levelPath; }
    int CurrentSectorId() const
    {
        return controller.fpsControllerState.currentSectorId;
    }

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

    SectorTopologyMap topologyMap;
    SectorEditorPreviewControllerState controller;
    SectorEditorPreviewCollisionState collision;
    std::string levelName;
    std::string levelPath;
    bool running = false;
    bool paused = false;
    FpsPlayerRuntime fpsPlayer;
    const FpsWeaponRegistry* weaponRegistry = nullptr;
    const FpsApplicationSettings* applicationSettings = nullptr;
    PlayerAudioRuntime* playerAudio = nullptr;
    engine::PersistentScriptStore* persistentScripts = nullptr;
    engine::ScriptRuntime scripts;
    SectorScriptHost scriptHost;
    std::string failureError;
};

} // namespace game
