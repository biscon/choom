#pragma once

#include "engine/EngineContext.h"
#include "game/FootstepAudio.h"
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
            const std::string& defaultFootstepSet,
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
    engine::MusicHandle FindLevelMusic(const std::string& id) const;
    engine::SoundPlaybackHandle PlayFootstepForSector(
            engine::EngineContext& context,
            int sectorId,
            float volume);
    engine::SoundPlaybackHandle PlayFootstepForSectorAt(
            engine::EngineContext& context,
            int sectorId,
            float volume,
            const engine::PositionalSoundSettings& positional);

private:
    void BeginLevelAudio(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            const char* scopeName,
            const std::string& defaultFootstepSet);
    void UpdateLevelAudio(engine::EngineContext& context);
    void BindRuntimeObjectAudio(engine::World& world);

    SectorMeshRenderer renderer;
    SectorRuntimeObjectState runtimeObjects;
    engine::AssetScopeHandle audioScope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::SoundHandle> levelSounds;
    std::unordered_map<std::string, engine::MusicHandle> levelMusicById;
    std::unordered_map<std::string, LoadedFootstepSet> footstepSets;
    std::unordered_map<int, std::string> footstepSetBySectorId;
    FootstepPlaybackState footstepPlayback;
    engine::MusicHandle backgroundMusic = engine::NullMusicHandle();
    float levelMusicVolume = SectorLevelAudioSettings::DefaultMusicVolume;
    bool levelMusicStartPending = false;
    bool levelMusicFailureReported = false;
};

} // namespace game
