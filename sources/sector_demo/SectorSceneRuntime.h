#pragma once

#include "engine/EngineContext.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"

#include <raylib.h>

#include <string>

namespace game {

class SectorSceneRuntime {
public:
    bool Rebuild(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            const char* assetScopeName,
            std::string& error);
    void Shutdown(engine::EngineContext& context);

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

private:
    SectorMeshRenderer renderer;
    SectorRuntimeObjectState runtimeObjects;
};

} // namespace game
