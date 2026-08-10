#pragma once

#include "engine/EngineContext.h"
#include "game/SectorLevelLoader.h"
#include "game/FpsPlayerRuntime.h"
#include "game/PlayerAudio.h"
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
            std::string& error);
    void Shutdown(engine::EngineContext& context, SectorSceneRuntime& scene);

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
};

} // namespace game
