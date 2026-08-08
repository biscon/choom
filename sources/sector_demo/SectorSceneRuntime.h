#pragma once

#include "engine/EngineContext.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"

#include <raylib.h>

#include <string>
#include <unordered_map>

namespace game {

class SectorSceneRuntime {
public:
    bool Rebuild(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            const char* assetScopeName,
            std::string& error);
    void Shutdown(engine::EngineContext& context);
    void StopLevelAudio(engine::EngineContext& context);

    void RefreshMapRuntimeObjects(
            engine::EngineContext& context,
            const SectorTopologyMap& map);
    void Update(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            float dt,
            const Vector3* playerPosition);

    void RenderShadowMaps(engine::EngineContext& context);
    void RenderScene(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            bool useBakedAmbientOcclusion = true);
    void ApplyPostProcessing(
            engine::AssetManager& assets,
            RenderTexture2D& sceneTarget,
            const SectorTopologyMap& map);

    SectorMeshRenderer& Renderer() { return renderer; }
    const SectorMeshRenderer& Renderer() const { return renderer; }
    SectorRuntimeObjectState& RuntimeObjects() { return runtimeObjects; }
    const SectorRuntimeObjectState& RuntimeObjects() const { return runtimeObjects; }
    bool IsReady() const { return renderer.IsRendererReady(); }
    engine::SoundHandle FindLevelSound(const std::string& id) const;

private:
    void BeginLevelAudio(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            const char* scopeName);
    void UpdateLevelAudio(engine::EngineContext& context);

    SectorMeshRenderer renderer;
    SectorRuntimeObjectState runtimeObjects;
    engine::AssetScopeHandle audioScope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::SoundHandle> levelSounds;
    engine::MusicHandle levelMusic = engine::NullMusicHandle();
    bool levelMusicStartPending = false;
    bool levelMusicFailureReported = false;
};

} // namespace game
