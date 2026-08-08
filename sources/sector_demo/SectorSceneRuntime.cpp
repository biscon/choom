#include "sector_demo/SectorSceneRuntime.h"

#include "sector_demo/SectorAssetPaths.h"

#include <raylib.h>

namespace game {

bool SectorSceneRuntime::Rebuild(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        const char* assetScopeName,
        std::string& error)
{
    StopLevelAudio(context);
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
    BeginLevelAudio(context, map, assetScopeName);
    return true;
}

void SectorSceneRuntime::Shutdown(engine::EngineContext& context)
{
    StopLevelAudio(context);
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
    UpdateLevelAudio(context);
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

void SectorSceneRuntime::StopLevelAudio(engine::EngineContext& context)
{
    for (const auto& entry : levelSounds) {
        context.audio.StopSoundAsset(context.assets, entry.second);
    }
    if (!engine::IsNull(levelMusic)) {
        context.audio.StopMusic(context.assets, levelMusic);
    }
    if (!engine::IsNull(audioScope)) {
        context.assets.UnloadScope(audioScope);
    }
    audioScope = engine::NullAssetScopeHandle();
    levelSounds.clear();
    levelMusic = engine::NullMusicHandle();
    levelMusicStartPending = false;
    levelMusicFailureReported = false;
}

engine::SoundHandle SectorSceneRuntime::FindLevelSound(
        const std::string& id) const
{
    const auto found = levelSounds.find(id);
    return found == levelSounds.end()
            ? engine::NullSoundHandle()
            : found->second;
}

void SectorSceneRuntime::BeginLevelAudio(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        const char* scopeName)
{
    if (map.audioSettings.musicPath.empty()
            && map.audioSettings.soundsById.empty()) {
        return;
    }
    const std::string name = std::string{
            scopeName != nullptr ? scopeName : "sector_level"} + "_audio";
    audioScope = context.assets.CreateScope(name.c_str());
    if (engine::IsNull(audioScope)) {
        TraceLog(LOG_WARNING, "Could not create level audio asset scope");
        return;
    }
    levelSounds.reserve(map.audioSettings.soundsById.size());
    for (const auto& entry : map.audioSettings.soundsById) {
        const std::string path = ResolveSectorAudioAssetPath(entry.second);
        const engine::SoundHandle handle = context.assets.RequestSound(
                audioScope,
                path.c_str());
        if (!engine::IsNull(handle)) levelSounds.emplace(entry.first, handle);
    }
    if (!map.audioSettings.musicPath.empty()) {
        const std::string path = ResolveSectorAudioAssetPath(
                map.audioSettings.musicPath);
        levelMusic = context.assets.RequestMusic(audioScope, path.c_str());
        levelMusicStartPending = !engine::IsNull(levelMusic);
    }
}

void SectorSceneRuntime::UpdateLevelAudio(engine::EngineContext& context)
{
    if (!levelMusicStartPending || engine::IsNull(levelMusic)) return;
    if (context.assets.IsReady(levelMusic)) {
        if (context.audio.PlayMusic(
                    context.assets,
                    levelMusic,
                    engine::MusicPlaybackSettings{})) {
            levelMusicStartPending = false;
        }
        return;
    }
    if (context.assets.HasFailed(levelMusic)) {
        if (!levelMusicFailureReported) {
            TraceLog(LOG_WARNING, "Configured level music failed to load");
            levelMusicFailureReported = true;
        }
        levelMusicStartPending = false;
    }
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
