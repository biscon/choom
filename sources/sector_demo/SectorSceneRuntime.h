#pragma once

#include "engine/EngineContext.h"
#include "game/FootstepAudio.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcNavigationSystem.h"
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
            const Vector3* playerPosition,
            const SectorDoorPlayerObstacle* playerObstacle = nullptr);

    void RenderShadowMaps(engine::EngineContext& context);
    void RenderScene(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            bool useBakedAmbientOcclusion = true);
    void ApplyWorldAtmosphere(
            engine::RenderTarget& sceneTarget,
            const SectorTopologyMap& map);
    void ApplyHdrBloom(
            engine::RenderTarget& sceneTarget,
            const engine::HdrBloomSettings& settings,
            bool presentFromScratch = false);
    bool CompositeViewmodel(
            engine::RenderTarget& sceneTarget,
            const engine::RenderTarget& viewmodelTarget);
    const engine::RenderTarget* HdrDebugPresentationSource() const
    {
        return renderer.HdrDebugPresentationSource();
    }

    SectorMeshRenderer& Renderer() { return renderer; }
    const SectorMeshRenderer& Renderer() const { return renderer; }
    SectorRuntimeObjectState& RuntimeObjects() { return runtimeObjects; }
    const SectorRuntimeObjectState& RuntimeObjects() const { return runtimeObjects; }
    SectorNavigationWorld& Navigation() { return navigation; }
    const SectorNavigationWorld& Navigation() const { return navigation; }
    NpcNavigationRuntime& NpcNavigation() { return npcNavigation; }
    const NpcNavigationRuntime& NpcNavigation() const { return npcNavigation; }
    NpcMoveRequestResult RequestNpcMove(
            engine::EngineContext& context,
            std::string_view instanceId,
            Vector2 destinationXZ,
            NpcMoveGait gait = NpcMoveGait::Walk,
            NpcMoveAuthority authority = NpcMoveAuthority::Programmatic);
    bool CancelNpcMove(
            engine::EngineContext& context,
            std::string_view instanceId,
            uint64_t expectedRequestId = 0);
    NpcMoveStatus GetNpcMoveStatus(std::string_view instanceId) const;
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
    SectorNavigationWorld navigation;
    NpcNavigationRuntime npcNavigation;
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
