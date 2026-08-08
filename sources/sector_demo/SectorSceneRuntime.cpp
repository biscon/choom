#include "sector_demo/SectorSceneRuntime.h"

namespace game {

bool SectorSceneRuntime::Rebuild(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        const char* assetScopeName,
        std::string& error)
{
    if (!renderer.RebuildRendererResources(
                context.assets,
                map,
                assetScopeName,
                error)) {
        return false;
    }
    ResetSectorRuntimeObjectsForMap(
            context.world,
            context.assets,
            runtimeObjects,
            map);
    return true;
}

void SectorSceneRuntime::Shutdown(engine::EngineContext& context)
{
    ClearSectorRuntimeObjects(context.world, context.assets, runtimeObjects);
    renderer.ShutdownRendererResources(context.assets);
}

void SectorSceneRuntime::RefreshMapRuntimeObjects(
        engine::EngineContext& context,
        const SectorTopologyMap& map)
{
    ResetSectorRuntimeObjectsForMap(
            context.world,
            context.assets,
            runtimeObjects,
            map);
}

void SectorSceneRuntime::Update(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        float dt,
        const Vector3* playerPosition)
{
    UpdateSectorRuntimeObjects(
            context.world,
            context.assets,
            runtimeObjects,
            map,
            dt,
            playerPosition);
    renderer.FinalizeRuntimeObjectResources(context.assets, context.world);
    runtimeObjects.dynamicDoorColliders.clear();
    CollectSectorDoorDynamicColliders(
            context.world,
            runtimeObjects.dynamicDoorColliders);
    runtimeObjects.dynamicPortalBlockers.clear();
    CollectSectorDoorDynamicPortalBlockers(
            context.world,
            runtimeObjects.dynamicPortalBlockers);
    renderer.AdvanceRuntime(dt);
}

void SectorSceneRuntime::RenderShadowMaps(engine::EngineContext& context)
{
    renderer.RenderDynamicSpotLightShadowMaps(
            context.assets,
            &context.world);
}

void SectorSceneRuntime::RenderScene(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        bool useBakedAmbientOcclusion)
{
    renderer.DrawScene(
            context.assets,
            useBakedAmbientOcclusion,
            &context.world,
            SectorRuntimeDoorLightingContext{
                    &runtimeObjects.objectLightProbes,
                    &map},
            map.fogSettings);
}

void SectorSceneRuntime::ApplyPostProcessing(
        engine::AssetManager& assets,
        RenderTexture2D& sceneTarget,
        const SectorTopologyMap& map)
{
    renderer.ApplyEmissiveDecalBloomToScene(
            assets,
            sceneTarget,
            map.fogSettings);
    renderer.ApplyLocalFogToScene(
            sceneTarget,
            map,
            runtimeObjects.objectLightProbes);
}

} // namespace game
