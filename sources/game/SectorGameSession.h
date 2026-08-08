#pragma once

#include "engine/EngineContext.h"
#include "game/SectorLevelLoader.h"
#include "game/FpsPlayerRuntime.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_demo/SectorSceneRuntime.h"

#include <string>

namespace game {

class SectorGameSession {
public:
    bool StartNew(
            engine::EngineContext& context,
            SectorSceneRuntime& scene,
            const std::string& levelName,
            const FpsWeaponRegistry& weaponRegistry,
            const FpsApplicationSettings& applicationSettings,
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
    bool BuildCollisionAndPlayer(SectorSceneRuntime& scene, bool initializePlayer);
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
};

} // namespace game
