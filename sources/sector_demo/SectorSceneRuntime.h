#pragma once

#include "engine/EngineContext.h"
#include "game/FootstepAudio.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcAudioSystem.h"
#include "game/npc/NpcNavigationSystem.h"
#include "game/npc/NpcPatrolSystem.h"
#include "game/npc/NpcCombatSystem.h"
#include "game/npc/NpcHeadLookSystem.h"
#include "game/npc/ai/NpcAiSystem.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorAudioOcclusion.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"
#include "sector_demo/renderer/SectorImpactParticleSystem.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

class SectorSceneRuntime {
public:
    void SetItemRuntimeAssets(
            const ItemRegistry* registry,
            const ItemModelAssetState* assets)
    {
        itemRegistry = registry;
        itemModelAssets = assets;
    }
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
    bool RefreshTopologyRuntimeData(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            std::string& error);
    bool SpawnItemRuntimeObject(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            const SectorPlacedRuntimeObject& object,
            engine::Entity* outEntity = nullptr);
    bool RebuildNavigationForMap(
            engine::EngineContext& context,
            const SectorTopologyMap& map);
    void Update(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            float dt,
            const Vector3* playerPosition,
            int playerSectorId,
            const SectorDoorPlayerObstacle* playerObstacle = nullptr,
            const NpcAiGameplayContext* npcGameplay = nullptr,
            int externalDoorHoldId = 0);
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
            uint64_t shotSequence,
            const FpsWeaponFiringDefinition& firing,
            FpsShotResult& outShot);
    void EmitPlayerSound(Vector3 positionWorld, float radiusWorld)
    {
        EmitNpcPlayerSound(npcAi, positionWorld, radiusWorld);
    }

    void RenderShadowMaps(engine::EngineContext& context);
    void RenderScene(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            bool useBakedAmbientOcclusion = true,
            SectorUseHighlight useHighlight = {});
    void ApplyWorldAtmosphere(
            engine::RenderTarget& sceneTarget,
            const SectorTopologyMap& map,
            bool collectGpuDiagnostics = false);
    void ApplyTransparentSurfaces(
            engine::RenderTarget& sceneTarget,
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            bool collectGpuDiagnostics = false);
    void ApplyHdrBloom(
            engine::RenderTarget& sceneTarget,
            const engine::HdrBloomSettings& settings,
            bool presentFromScratch = false);
    bool PreparePostBloomWorldOverlays(
            engine::RenderTarget& sceneTarget,
            bool overlayRequested);
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
    const NpcAiRuntime& NpcAi() const { return npcAi; }
    const NpcPatrolRuntime& NpcPatrol() const { return npcPatrol; }
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
    bool PlayLevelSound(
            engine::EngineContext& context,
            const std::string& id,
            float volume,
            float pitch,
            std::string& error);
    bool PlaySoundEmitter(
            engine::EngineContext& context,
            const std::string& id,
            const float* volumeOverride,
            float pitch,
            std::string& error);
    bool StopSoundEmitter(
            engine::EngineContext& context,
            const std::string& id,
            std::string& error);
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
    void UpdateLevelAudio(
            engine::EngineContext& context,
            const SectorTopologyMap& map,
            float dt,
            int playerSectorId);
    void BindRuntimeObjectAudio(engine::World& world);
    void PlayPendingNpcFootsteps(engine::EngineContext& context);

    SectorMeshRenderer renderer;
    SectorRuntimeObjectState runtimeObjects;
    SectorNavigationWorld navigation;
    NpcNavigationRuntime npcNavigation;
    NpcCombatRuntime npcCombat;
    NpcAiRuntime npcAi;
    SectorSoundPropagationWorld soundPropagation;
    NpcPatrolRuntime npcPatrol;
    NpcAudioRuntime npcAudio;
    SectorImpactParticleSystem impactParticles;
    engine::AssetScopeHandle audioScope = engine::NullAssetScopeHandle();
    std::unordered_map<std::string, engine::SoundHandle> levelSounds;
    std::unordered_map<std::string, engine::MusicHandle> levelMusicById;
    std::unordered_map<std::string, LoadedFootstepSet> footstepSets;
    std::unordered_map<int, std::string> footstepSetBySectorId;
    FootstepPlaybackState footstepPlayback;
    float footstepVolume = 1.0f;
    const ItemRegistry* itemRegistry = nullptr;
    const ItemModelAssetState* itemModelAssets = nullptr;

    struct RoomtonePlayback {
        std::string soundId;
        engine::MusicHandle music = engine::NullMusicHandle();
        float startVolume = 0.0f;
        float currentVolume = 0.0f;
        float targetVolume = 0.0f;
        bool failureReported = false;
    };
    std::vector<RoomtonePlayback> roomtonePlaybacks;
    int lastRoomtoneSectorId = -1;
    float roomtoneTransitionElapsedSeconds = 0.0f;
    float roomtoneTransitionDurationSeconds = 0.0f;

    struct SoundEmitterPlayback {
        std::string id;
        std::string soundId;
        engine::SoundHandle sound = engine::NullSoundHandle();
        engine::MusicHandle music = engine::NullMusicHandle();
        engine::SoundPlaybackHandle playback = engine::NullSoundPlaybackHandle();
        Vector3 positionWorld = {};
        float volume = 1.0f;
        float playbackVolume = 1.0f;
        float pitch = 1.0f;
        bool loop = false;
        bool loopRequested = false;
        bool autoStartPending = false;
        bool failureReported = false;
    };
    std::vector<SoundEmitterPlayback> soundEmitterPlaybacks;
};

} // namespace game
