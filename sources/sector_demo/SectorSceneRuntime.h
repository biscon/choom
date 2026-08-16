#pragma once

#include "engine/EngineContext.h"
#include "game/FootstepAudio.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcAudioSystem.h"
#include "game/npc/NpcNavigationSystem.h"
#include "game/npc/NpcCombatSystem.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"
#include "sector_demo/renderer/SectorImpactParticleSystem.h"

#include <raylib.h>

#include <cstddef>
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
            float footstepVolume,
            std::string& error);
    void Shutdown(engine::EngineContext& context);
    void StopLevelAudio(engine::EngineContext& context);

    void RefreshMapRuntimeObjects(
            engine::EngineContext& context,
            const SectorTopologyMap& map);
    bool RebuildNavigationForMap(
            engine::EngineContext& context,
            const SectorTopologyMap& map);
    void Update(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            float dt,
            const Vector3* playerPosition,
            const SectorDoorPlayerObstacle* playerObstacle = nullptr);
    void UpdateLoadPreparation(
            engine::EngineContext& context,
            const SectorTopologyMap& map);
    bool AreLoadAssetScopesFinished(
            const engine::AssetManager& assets) const;
    void AccumulateLoadAssetProgress(
            const engine::AssetManager& assets,
            size_t& finished,
            size_t& total) const;
    bool ResolvePlayerWeaponShot(
            engine::EngineContext& context,
            const SectorCollisionWorld* collisionWorld,
            Vector3 rayOrigin,
            Vector3 rayDirection,
            float maximumDistance,
            const FpsWeaponImpactDefinition& impact,
            FpsShotResult& outShot);

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
            const std::string& defaultFootstepSet,
            float footstepVolume);
    void UpdateLevelAudio(engine::EngineContext& context);
    void BindRuntimeObjectAudio(engine::World& world);
    void PlayPendingNpcFootsteps(engine::EngineContext& context);

    SectorMeshRenderer renderer;
    SectorRuntimeObjectState runtimeObjects;
    SectorNavigationWorld navigation;
    NpcNavigationRuntime npcNavigation;
    NpcCombatRuntime npcCombat;
    NpcAudioRuntime npcAudio;
    SectorImpactParticleSystem impactParticles;
    engine::AssetScopeHandle audioScope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::SoundHandle> levelSounds;
    std::unordered_map<std::string, engine::MusicHandle> levelMusicById;
    std::unordered_map<std::string, LoadedFootstepSet> footstepSets;
    std::unordered_map<int, std::string> footstepSetBySectorId;
    FootstepPlaybackState footstepPlayback;
    float footstepVolume = 1.0f;
    engine::MusicHandle backgroundMusic = engine::NullMusicHandle();
    float levelMusicVolume = SectorLevelAudioSettings::DefaultMusicVolume;
    bool levelMusicStartPending = false;
    bool levelMusicFailureReported = false;
};

} // namespace game
