#include "sector_demo/SectorSceneRuntime.h"

#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorAudioOcclusion.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/npc/NpcBoneImpactSystem.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <set>

namespace game {

namespace {

bool ScopeFinishedOrEmpty(
        const engine::AssetManager& assets,
        engine::AssetScopeHandle scope)
{
    return engine::IsNull(scope) || assets.IsScopeFinished(scope);
}

void AccumulateScopeProgress(
        const engine::AssetManager& assets,
        engine::AssetScopeHandle scope,
        size_t& finished,
        size_t& total)
{
    if (engine::IsNull(scope)) return;
    size_t scopeFinished = 0;
    size_t scopeTotal = 0;
    assets.GetScopeProgressCounts(scope, scopeFinished, scopeTotal);
    finished += scopeFinished;
    total += scopeTotal;
}

} // namespace

bool SectorSceneRuntime::Rebuild(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        const char* assetScopeName,
        const std::string& defaultFootstepSet,
        float newFootstepVolume,
        std::string& error)
{
    ShutdownNpcAudioRuntime(context.assets, context.audio, npcAudio);
    ShutdownNpcNavigationRuntime(context.world, navigation, npcNavigation);
    ClearNpcPatrolRuntime(npcPatrol);
    navigation.Shutdown();
    StopLevelAudio(context);
    soundPropagation.Clear();
    if (!renderer.RebuildRendererResources(
                context.assets,
                map,
                assetScopeName,
                error)) {
        return false;
    }
    std::string soundPropagationError;
    if (!soundPropagation.Build(map, &soundPropagationError)) {
        TraceLog(
                LOG_WARNING,
                "Sound propagation graph build failed: %s",
                soundPropagationError.c_str());
    }
    if (!navigation.Initialize(BuildSectorNavigationSettingsForMap(map))) {
        TraceLog(LOG_WARNING, "Navigation service initialization failed");
    }
    ResetSectorRuntimeObjectsForMap(
            context.world,
            context.assets,
            runtimeObjects,
            map,
            itemRegistry,
            itemModelAssets);
    InitializeNpcAudioRuntime(
            context.world,
            context.assets,
            runtimeObjects.runtimeObjectAssetScope,
            runtimeObjects.npcDefinitionCatalog,
            npcAudio,
            map.runtimeObjects.size());
    if (navigation.State() != SectorNavigationState::Uninitialized) {
        navigation.RequestRebuild();
        InitializeNpcNavigationRuntime(context.world, navigation, npcNavigation);
    }
    InitializeNpcCombatRuntime(npcCombat, map.runtimeObjects.size());
    InitializeNpcAiRuntime(
            npcAi, 64, navigation.Capacities().agentCapacity);
    InitializeNpcPatrolRuntime(
            npcPatrol, navigation.Capacities().agentCapacity);
    impactParticles.Clear();
    BeginLevelAudio(
            context,
            map,
            assetScopeName,
            defaultFootstepSet,
            newFootstepVolume);
    BindRuntimeObjectAudio(context.world);
    return true;
}

bool SectorSceneRuntime::RebuildNavigationForMap(
        engine::EngineContext& context,
        const SectorTopologyMap& map)
{
    ShutdownNpcNavigationRuntime(context.world, navigation, npcNavigation);
    ClearNpcPatrolRuntime(npcPatrol);
    navigation.Shutdown();
    if (!navigation.Initialize(BuildSectorNavigationSettingsForMap(map))) {
        TraceLog(LOG_WARNING, "Navigation service initialization failed");
        return false;
    }
    navigation.RequestRebuild();
    InitializeNpcNavigationRuntime(context.world, navigation, npcNavigation);
    InitializeNpcCombatRuntime(npcCombat, map.runtimeObjects.size());
    InitializeNpcAiRuntime(
            npcAi, 64, navigation.Capacities().agentCapacity);
    InitializeNpcPatrolRuntime(
            npcPatrol, navigation.Capacities().agentCapacity);
    impactParticles.Clear();
    return true;
}

void SectorSceneRuntime::Shutdown(engine::EngineContext& context)
{
    ShutdownNpcAudioRuntime(context.assets, context.audio, npcAudio);
    ShutdownNpcNavigationRuntime(context.world, navigation, npcNavigation);
    ClearNpcPatrolRuntime(npcPatrol);
    ClearNpcCombatRuntime(npcCombat);
    ClearNpcAiRuntime(npcAi);
    soundPropagation.Clear();
    impactParticles.Clear();
    navigation.Shutdown();
    StopLevelAudio(context);
    ClearSectorRuntimeObjects(context.world, context.assets, runtimeObjects);
    renderer.ShutdownRendererResources(context.assets);
}

void SectorSceneRuntime::RefreshMapRuntimeObjects(
        engine::EngineContext& context,
        const SectorTopologyMap& map)
{
    ShutdownNpcAudioRuntime(context.assets, context.audio, npcAudio);
    ShutdownNpcNavigationRuntime(context.world, navigation, npcNavigation);
    ClearNpcPatrolRuntime(npcPatrol);
    navigation.ResetForRebuild();
    ResetSectorRuntimeObjectsForMap(
            context.world,
            context.assets,
            runtimeObjects,
            map,
            itemRegistry,
            itemModelAssets);
    InitializeNpcAudioRuntime(
            context.world,
            context.assets,
            runtimeObjects.runtimeObjectAssetScope,
            runtimeObjects.npcDefinitionCatalog,
            npcAudio,
            map.runtimeObjects.size());
    if (navigation.State() != SectorNavigationState::Uninitialized) {
        navigation.RequestRebuild();
        InitializeNpcNavigationRuntime(context.world, navigation, npcNavigation);
    }
    InitializeNpcCombatRuntime(npcCombat, map.runtimeObjects.size());
    InitializeNpcAiRuntime(
            npcAi, 64, navigation.Capacities().agentCapacity);
    InitializeNpcPatrolRuntime(
            npcPatrol, navigation.Capacities().agentCapacity);
    impactParticles.Clear();
    BindRuntimeObjectAudio(context.world);
}

bool SectorSceneRuntime::RefreshTopologyRuntimeData(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        std::string& error)
{
    error.clear();
    soundPropagation.Clear();
    std::string soundPropagationError;
    const bool soundPropagationReady =
            soundPropagation.Build(map, &soundPropagationError);
    if (!soundPropagationReady) {
        TraceLog(
                LOG_WARNING,
                "Sound propagation graph refresh failed: %s",
                soundPropagationError.c_str());
    }

    ShutdownNpcAudioRuntime(context.assets, context.audio, npcAudio);
    ShutdownNpcNavigationRuntime(context.world, navigation, npcNavigation);
    ClearNpcPatrolRuntime(npcPatrol);
    ResetSectorRuntimeObjectsForMap(
            context.world,
            context.assets,
            runtimeObjects,
            map,
            itemRegistry,
            itemModelAssets);
    InitializeNpcAudioRuntime(
            context.world,
            context.assets,
            runtimeObjects.runtimeObjectAssetScope,
            runtimeObjects.npcDefinitionCatalog,
            npcAudio,
            map.runtimeObjects.size());
    const bool navigationReady = RebuildNavigationForMap(context, map);
    BindRuntimeObjectAudio(context.world);
    if (!soundPropagationReady || !navigationReady) {
        error = !soundPropagationReady
                ? (soundPropagationError.empty()
                        ? "Sound propagation refresh failed"
                        : soundPropagationError)
                : "Navigation refresh failed";
        return false;
    }
    return true;
}

bool SectorSceneRuntime::SpawnItemRuntimeObject(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        const SectorPlacedRuntimeObject& object,
        engine::Entity* outEntity)
{
    if (itemRegistry == nullptr || itemModelAssets == nullptr) return false;
    return SpawnSectorItemRuntimeObject(
            context.world,
            context.assets,
            runtimeObjects,
            map,
            object,
            *itemRegistry,
            *itemModelAssets,
            outEntity);
}

void SectorSceneRuntime::Update(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        float dt,
        const Vector3* playerPosition,
        int playerSectorId,
        const SectorDoorPlayerObstacle* playerObstacle,
        const NpcAiGameplayContext* npcGameplay,
        int externalDoorHoldId)
{
    UpdateLevelAudio(context, map, dt, playerSectorId);
    PrepareNpcDoorTraversalAndHoldsSystem(
            context.world,
            navigation,
            npcNavigation,
            runtimeObjects.dynamicDoorColliders,
            dt,
            npcGameplay != nullptr && npcGameplay->frozen,
            externalDoorHoldId);
    CollectNpcDoorObstacles(
            context.world,
            npcNavigation,
            runtimeObjects.doorObstacles,
            playerObstacle);
    UpdateSectorRuntimeObjects(
            context.world,
            context.assets,
            runtimeObjects,
            map,
            dt,
            playerPosition,
            playerObstacle,
            &runtimeObjects.doorObstacles);
    UpdateSectorDoorAudioSystem(
            context.world,
            context.assets,
            context.audio);
    renderer.FinalizeRuntimeObjectResources(context.assets, context.world);
    if (runtimeObjects.doorSpatialStateChanged
            || !runtimeObjects.doorCollisionCacheInitialized) {
        runtimeObjects.dynamicDoorColliders.clear();
        CollectSectorDoorDynamicColliders(
                context.world,
                runtimeObjects.dynamicDoorColliders);
        runtimeObjects.dynamicPortalBlockers.clear();
        CollectSectorDoorDynamicPortalBlockers(
                context.world,
                runtimeObjects.dynamicPortalBlockers);
        runtimeObjects.doorCollisionCacheInitialized = true;
    }
    if (runtimeObjects.objectSectorLookupWorldValid) {
        const bool npcKnockbackMoved = UpdateNpcCombatSystem(
                context.world,
                runtimeObjects.objectSectorLookupWorld,
                runtimeObjects.dynamicDoorColliders,
                runtimeObjects.physicalModelColliders,
                &map,
                npcCombat,
                dt);
        if (npcKnockbackMoved) {
            UpdateSectorObjectBakedLightingSystem(
                    context.world,
                    runtimeObjects.objectLightProbes,
                    &map);
        }
    }
    navigation.UpdateBuild(
            map,
            runtimeObjects.physicalModelColliders,
            runtimeObjects.staticModelPendingCount);
    navigation.UpdateDynamicObstacles(
            runtimeObjects.dynamicModelColliders,
            dt);
    SynchronizeSectorNavigationDoorLinksSystem(
            context.world,
            navigation,
            runtimeObjects.dynamicDoorColliders);
    if (npcGameplay != nullptr
            && runtimeObjects.objectSectorLookupWorldValid) {
        UpdateNpcAiSystem(
                context.world,
                context.assets,
                context.audio,
                navigation,
                npcNavigation,
                runtimeObjects.objectSectorLookupWorld,
                runtimeObjects.dynamicDoorColliders,
                runtimeObjects.staticModelColliders,
                npcAi,
                *npcGameplay,
                dt,
                &soundPropagation,
                &runtimeObjects.dynamicPortalBlockers);
        UpdateNpcPatrolSystem(
                context.world,
                navigation,
                runtimeObjects.objectSectorLookupWorld,
                npcNavigation,
                npcPatrol,
                map,
                dt,
                npcGameplay->frozen);
    }
    if (runtimeObjects.objectSectorLookupWorldValid) {
        UpdateNpcNavigationAndLocomotionSystem(
                context.world,
                context.assets,
                navigation,
                npcNavigation,
                runtimeObjects.npcDefinitionCatalog,
                runtimeObjects.objectSectorLookupWorld,
                runtimeObjects.dynamicDoorColliders,
                runtimeObjects.physicalModelColliders,
                runtimeObjects.objectLightProbes,
                map,
                dt,
                playerObstacle,
                npcGameplay != nullptr && npcGameplay->frozen);
    }
    engine::AnimatedModelSystem(context.world, context.assets, dt);
    if (runtimeObjects.objectSectorLookupWorldValid) {
        const Vector3* headLookTarget = npcGameplay != nullptr
                        && !npcGameplay->playerInvisible
                ? &npcGameplay->playerEyePosition
                : nullptr;
        UpdateNpcHeadLookSystem(
                context.world,
                context.assets,
                runtimeObjects.objectSectorLookupWorld,
                runtimeObjects.dynamicDoorColliders,
                runtimeObjects.staticModelColliders,
                headLookTarget,
                dt);
    }
    UpdateNpcBoneImpactSystem(context.world, context.assets, dt);
    if (runtimeObjects.objectSectorLookupWorldValid) {
        UpdateNpcFootstepEventsSystem(
                context.world,
                context.assets,
                npcNavigation,
                runtimeObjects.npcDefinitionCatalog,
                dt);
        PlayPendingNpcFootsteps(context);
    }
    UpdateNpcAudioSystem(
            context.world,
            context.assets,
            context.audio,
            npcAudio,
            dt);
    SectorAudioOcclusionContext audioOcclusion;
    if (runtimeObjects.objectSectorLookupWorldValid) {
        audioOcclusion.collisionWorld =
                &runtimeObjects.objectSectorLookupWorld;
    }
    audioOcclusion.doorColliders = &runtimeObjects.dynamicDoorColliders;
    audioOcclusion.portalBlockers = &runtimeObjects.dynamicPortalBlockers;
    audioOcclusion.propagationWorld = &soundPropagation;
    context.audio.UpdatePositionalSoundPropagation(
            dt,
            &audioOcclusion,
            QuerySectorSoundPropagation);
    impactParticles.Update(context.world, &context.assets, dt);
    renderer.AdvanceRuntime(dt);
}

bool SectorSceneRuntime::ResolvePlayerWeaponShot(
        engine::EngineContext& context,
        const SectorCollisionWorld* collisionWorld,
        Vector3 rayOrigin,
        Vector3 rayDirection,
        uint64_t shotSequence,
        const FpsWeaponFiringDefinition& firing,
        FpsShotResult& outShot)
{
    WeaponPelletVolleyResult volley;
    const bool hit = game::ResolvePlayerWeaponPelletVolley(
            context.world,
            &context.assets,
            navigation,
            npcNavigation,
            collisionWorld,
            runtimeObjects.dynamicDoorColliders,
            runtimeObjects.staticModelColliders,
            rayOrigin,
            rayDirection,
            shotSequence,
            firing,
            volley,
            &npcAudio,
            &npcAi,
            &runtimeObjects.windowColliders);
    outShot = volley.shots[0];
    for (int pelletIndex = 0;
            pelletIndex < volley.pelletCount;
            ++pelletIndex) {
        impactParticles.Spawn(
                volley.impacts[static_cast<size_t>(pelletIndex)]);
    }
    return hit;
}

void SectorSceneRuntime::UpdateLoadPreparation(
        engine::EngineContext& context,
        const SectorTopologyMap& map)
{
    UpdateSectorRuntimeObjects(
            context.world,
            context.assets,
            runtimeObjects,
            map,
            0.0f,
            nullptr,
            nullptr,
            nullptr);
    renderer.FinalizeRuntimeObjectResources(context.assets, context.world);
    if (runtimeObjects.doorSpatialStateChanged
            || !runtimeObjects.doorCollisionCacheInitialized) {
        runtimeObjects.dynamicDoorColliders.clear();
        CollectSectorDoorDynamicColliders(
                context.world,
                runtimeObjects.dynamicDoorColliders);
        runtimeObjects.dynamicPortalBlockers.clear();
        CollectSectorDoorDynamicPortalBlockers(
                context.world,
                runtimeObjects.dynamicPortalBlockers);
        runtimeObjects.doorCollisionCacheInitialized = true;
    }
    navigation.UpdateBuild(
            map,
            runtimeObjects.physicalModelColliders,
            runtimeObjects.staticModelPendingCount);
    navigation.UpdateDynamicObstacles(
            runtimeObjects.dynamicModelColliders,
            0.0f);
    SynchronizeSectorNavigationDoorLinksSystem(
            context.world,
            navigation,
            runtimeObjects.dynamicDoorColliders);
    if (runtimeObjects.objectSectorLookupWorldValid) {
        UpdateNpcNavigationAndLocomotionSystem(
                context.world,
                context.assets,
                navigation,
                npcNavigation,
                runtimeObjects.npcDefinitionCatalog,
                runtimeObjects.objectSectorLookupWorld,
                runtimeObjects.dynamicDoorColliders,
                runtimeObjects.physicalModelColliders,
                runtimeObjects.objectLightProbes,
                map,
                0.0f,
                nullptr);
    }
    engine::AnimatedModelSystem(context.world, context.assets, 0.0f);
    renderer.AdvanceRuntime(0.0f);
}

bool SectorSceneRuntime::AreLoadAssetScopesFinished(
        const engine::AssetManager& assets) const
{
    return ScopeFinishedOrEmpty(assets, renderer.RendererAssetScope())
            && ScopeFinishedOrEmpty(
                    assets, runtimeObjects.runtimeObjectAssetScope)
            && ScopeFinishedOrEmpty(assets, audioScope);
}

void SectorSceneRuntime::AccumulateLoadAssetProgress(
        const engine::AssetManager& assets,
        size_t& finished,
        size_t& total) const
{
    AccumulateScopeProgress(
            assets, renderer.RendererAssetScope(), finished, total);
    AccumulateScopeProgress(
            assets, runtimeObjects.runtimeObjectAssetScope, finished, total);
    AccumulateScopeProgress(assets, audioScope, finished, total);
}

NpcMoveRequestResult SectorSceneRuntime::RequestNpcMove(
        engine::EngineContext& context,
        std::string_view instanceId,
        Vector2 destinationXZ,
        NpcMoveGait gait,
        NpcMoveAuthority authority)
{
    if (!runtimeObjects.objectSectorLookupWorldValid) {
        return {false,
                SectorNavigationQueryStatus::NavigationUnavailable,
                0,
                "sector collision is unavailable"};
    }
    return game::RequestNpcMove(
            context.world,
            navigation,
            runtimeObjects.objectSectorLookupWorld,
            npcNavigation,
            instanceId,
            destinationXZ,
            gait,
            authority);
}

bool SectorSceneRuntime::CancelNpcMove(
        engine::EngineContext& context,
        std::string_view instanceId,
        uint64_t expectedRequestId)
{
    return game::CancelNpcMove(
            context.world, navigation, npcNavigation, instanceId,
            expectedRequestId);
}

NpcMoveStatus SectorSceneRuntime::GetNpcMoveStatus(
        std::string_view instanceId) const
{
    return game::GetNpcMoveStatus(npcNavigation, instanceId);
}

void SectorSceneRuntime::StopLevelAudio(engine::EngineContext& context)
{
    for (const auto& entry : levelSounds) {
        context.audio.StopSoundAsset(context.assets, entry.second);
    }
    for (const auto& entry : footstepSets) {
        for (const engine::SoundHandle sound : entry.second.sounds) {
            context.audio.StopSoundAsset(context.assets, sound);
        }
    }
    for (const auto& entry : levelMusicById) {
        context.audio.StopMusic(context.assets, entry.second);
    }
    for (const RoomtonePlayback& playback : roomtonePlaybacks) {
        context.audio.StopMusic(context.assets, playback.music);
    }
    for (SoundEmitterPlayback& emitter : soundEmitterPlaybacks) {
        if (!engine::IsNull(emitter.playback)) {
            context.audio.StopSound(context.assets, emitter.playback);
        }
        if (!engine::IsNull(emitter.music)) {
            context.audio.StopMusic(context.assets, emitter.music);
        }
    }
    if (!engine::IsNull(audioScope)) {
        context.assets.UnloadScope(audioScope);
    }
    audioScope = engine::NullAssetScopeHandle();
    levelSounds.clear();
    levelMusicById.clear();
    footstepSets.clear();
    footstepSetBySectorId.clear();
    footstepPlayback = FootstepPlaybackState{};
    footstepVolume = 1.0f;
    roomtonePlaybacks.clear();
    lastRoomtoneSectorId = -1;
    roomtoneTransitionElapsedSeconds = 0.0f;
    roomtoneTransitionDurationSeconds = 0.0f;
    soundEmitterPlaybacks.clear();
}

engine::SoundPlaybackHandle SectorSceneRuntime::PlayFootstepForSector(
        engine::EngineContext& context,
        int sectorId,
        float volume)
{
    const auto assigned = footstepSetBySectorId.find(sectorId);
    if (assigned == footstepSetBySectorId.end()) {
        return engine::NullSoundPlaybackHandle();
    }
    const auto set = footstepSets.find(assigned->second);
    if (set == footstepSets.end()) return engine::NullSoundPlaybackHandle();
    return PlayFootstep(
            context.assets,
            context.audio,
            set->second,
            footstepPlayback,
            volume);
}

engine::SoundPlaybackHandle SectorSceneRuntime::PlayFootstepForSectorAt(
        engine::EngineContext& context,
        int sectorId,
        float volume,
        const engine::PositionalSoundSettings& positional)
{
    const auto assigned = footstepSetBySectorId.find(sectorId);
    if (assigned == footstepSetBySectorId.end()) {
        return engine::NullSoundPlaybackHandle();
    }
    const auto set = footstepSets.find(assigned->second);
    if (set == footstepSets.end()) return engine::NullSoundPlaybackHandle();
    return PlayFootstepAt(
            context.assets,
            context.audio,
            set->second,
            footstepPlayback,
            volume,
            positional);
}

engine::SoundHandle SectorSceneRuntime::FindLevelSound(
        const std::string& id) const
{
    const auto found = levelSounds.find(id);
    return found == levelSounds.end()
            ? engine::NullSoundHandle()
            : found->second;
}

engine::MusicHandle SectorSceneRuntime::FindLevelMusic(
        const std::string& id) const
{
    const auto found = levelMusicById.find(id);
    return found == levelMusicById.end()
            ? engine::NullMusicHandle()
            : found->second;
}

bool SectorSceneRuntime::PlayLevelSound(
        engine::EngineContext& context,
        const std::string& id,
        float volume,
        float pitch,
        std::string& error)
{
    const engine::SoundHandle sound = FindLevelSound(id);
    if (engine::IsNull(sound)) {
        error = "map Sound ID was not found: " + id;
        return false;
    }
    if (!context.assets.IsReady(sound)) {
        error = context.assets.HasFailed(sound)
                ? "map Sound failed to load: " + id
                : "map Sound is not ready: " + id;
        return false;
    }
    engine::SoundPlaybackSettings settings;
    settings.volume = volume;
    settings.pitch = pitch;
    if (engine::IsNull(context.audio.PlaySound(context.assets, sound, settings))) {
        error = "map Sound could not start: " + id;
        return false;
    }
    error.clear();
    return true;
}

bool SectorSceneRuntime::PlaySoundEmitter(
        engine::EngineContext& context,
        const std::string& id,
        const float* volumeOverride,
        float pitch,
        std::string& error)
{
    auto found = std::find_if(
            soundEmitterPlaybacks.begin(), soundEmitterPlaybacks.end(),
            [&id](const SoundEmitterPlayback& emitter) { return emitter.id == id; });
    if (found == soundEmitterPlaybacks.end()) {
        error = "sound emitter was not found: " + id;
        return false;
    }
    SoundEmitterPlayback& emitter = *found;
    const bool streaming = !engine::IsNull(emitter.music);
    if (engine::IsNull(emitter.sound) && !streaming) {
        error = "sound emitter has no valid Sound/Music: " + id;
        return false;
    }
    const bool ready = streaming
            ? context.assets.IsReady(emitter.music)
            : context.assets.IsReady(emitter.sound);
    if (!ready) {
        const bool failed = streaming
                ? context.assets.HasFailed(emitter.music)
                : context.assets.HasFailed(emitter.sound);
        error = failed ? "sound emitter asset failed to load: " + id
                       : "sound emitter asset is not ready: " + id;
        return false;
    }
    emitter.pitch = pitch;
    const float playbackVolume = volumeOverride != nullptr
            ? *volumeOverride : emitter.volume;
    emitter.playbackVolume = playbackVolume;
    engine::PositionalSoundSettings positional;
    positional.position = emitter.positionWorld;
    if (streaming) {
        engine::MusicPlaybackSettings settings;
        settings.volume = playbackVolume;
        settings.pitch = pitch;
        settings.looping = emitter.loop;
        if (context.audio.IsMusicPlaying(emitter.music)) {
            if (emitter.loop) {
                if (!context.audio.PlayMusicAt(
                            context.assets, emitter.music, positional, settings)) {
                    error = "sound emitter settings could not be updated: " + id;
                    return false;
                }
                emitter.loopRequested = true;
                emitter.autoStartPending = false;
                error.clear();
                return true;
            }
            context.audio.StopMusic(context.assets, emitter.music);
        }
        if (!context.audio.PlayMusicAt(
                    context.assets, emitter.music, positional, settings)) {
            error = "sound emitter could not start: " + id;
            return false;
        }
    } else {
        engine::SoundPlaybackSettings settings;
        settings.volume = playbackVolume;
        settings.pitch = pitch;
        settings.looping = emitter.loop;
        if (emitter.loop && context.audio.IsSoundPlaying(emitter.playback)) {
            if (!context.audio.SetSoundPlaybackSettings(
                        context.assets, emitter.playback, settings)) {
                error = "sound emitter settings could not be updated: " + id;
                return false;
            }
            emitter.loopRequested = true;
            emitter.autoStartPending = false;
            error.clear();
            return true;
        }
        if (context.audio.IsSoundPlaying(emitter.playback)) {
            context.audio.StopSound(context.assets, emitter.playback);
        }
        emitter.playback = context.audio.PlaySoundAt(
                context.assets, emitter.sound, positional, settings);
        if (engine::IsNull(emitter.playback)) {
            error = "sound emitter could not start: " + id;
            return false;
        }
    }
    emitter.loopRequested = emitter.loop;
    emitter.autoStartPending = false;
    error.clear();
    return true;
}

bool SectorSceneRuntime::StopSoundEmitter(
        engine::EngineContext& context,
        const std::string& id,
        std::string& error)
{
    auto found = std::find_if(
            soundEmitterPlaybacks.begin(), soundEmitterPlaybacks.end(),
            [&id](const SoundEmitterPlayback& emitter) { return emitter.id == id; });
    if (found == soundEmitterPlaybacks.end()) {
        error = "sound emitter was not found: " + id;
        return false;
    }
    if (context.audio.IsSoundPlaying(found->playback)) {
        context.audio.StopSound(context.assets, found->playback);
    }
    if (context.audio.IsMusicPlaying(found->music)) {
        context.audio.StopMusic(context.assets, found->music);
    }
    found->playback = engine::NullSoundPlaybackHandle();
    found->loopRequested = false;
    found->autoStartPending = false;
    error.clear();
    return true;
}

void SectorSceneRuntime::BeginLevelAudio(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        const char* scopeName,
        const std::string& defaultFootstepSet,
        float newFootstepVolume)
{
    footstepVolume = std::clamp(newFootstepVolume, 0.0f, 1.0f);
    const FootstepCatalog catalog = DiscoverFootstepCatalog(
            ASSETS_PATH "audio/footsteps");
    if (!catalog.warning.empty()) {
        TraceLog(LOG_WARNING, "%s", catalog.warning.c_str());
    }
    const FootstepCatalogSet* defaultSet = FindFootstepCatalogSet(
            catalog,
            defaultFootstepSet);
    std::set<std::string> usedFootstepSets;
    bool defaultSetNeeded = false;
    footstepSetBySectorId.reserve(map.sectors.size());
    for (const SectorTopologySector& sector : map.sectors) {
        const std::string& requested = sector.footstepSet.empty()
                ? defaultFootstepSet
                : sector.footstepSet;
        if (sector.footstepSet.empty()) defaultSetNeeded = true;
        const FootstepCatalogSet* resolved = FindFootstepCatalogSet(catalog, requested);
        if (resolved == nullptr && !sector.footstepSet.empty()) {
            TraceLog(
                    LOG_WARNING,
                    "Sector %d footstep set '%s' was not found; using '%s'",
                    sector.id,
                    requested.c_str(),
                    defaultFootstepSet.c_str());
            resolved = defaultSet;
            defaultSetNeeded = true;
        }
        if (resolved != nullptr) {
            footstepSetBySectorId.emplace(sector.id, resolved->id);
            usedFootstepSets.insert(resolved->id);
        }
    }
    if (defaultSet == nullptr && defaultSetNeeded) {
        TraceLog(
                LOG_WARNING,
                "Default footstep set '%s' was not found; affected sectors will be silent",
                defaultFootstepSet.c_str());
    }

    if (map.audioSettings.soundsById.empty()
            && map.soundEmitters.empty()
            && usedFootstepSets.empty()) {
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
    levelMusicById.reserve(map.audioSettings.soundsById.size());
    for (const auto& entry : map.audioSettings.soundsById) {
        const SectorSoundDefinition& definition = entry.second;
        const std::string path = ResolveSectorAudioAssetPath(definition.path);
        if (definition.type == SectorSoundType::Music) {
            const engine::MusicHandle handle = context.assets.RequestMusic(
                    audioScope,
                    path.c_str());
            if (!engine::IsNull(handle)) levelMusicById.emplace(entry.first, handle);
        } else {
            const engine::SoundHandle handle = context.assets.RequestSound(
                    audioScope,
                    path.c_str());
            if (!engine::IsNull(handle)) levelSounds.emplace(entry.first, handle);
        }
    }
    size_t maximumVariationCount = 0;
    size_t maximumSetIdLength = 0;
    footstepSets.reserve(usedFootstepSets.size());
    for (const std::string& id : usedFootstepSets) {
        const FootstepCatalogSet* source = FindFootstepCatalogSet(catalog, id);
        if (source == nullptr) continue;
        LoadedFootstepSet loaded;
        loaded.id = source->id;
        loaded.sounds.reserve(source->relativePaths.size());
        for (const std::string& relativePath : source->relativePaths) {
            const std::string path = ResolveSectorAudioAssetPath(relativePath);
            const engine::SoundHandle handle = context.assets.RequestSound(
                    audioScope,
                    path.c_str());
            if (!engine::IsNull(handle)) loaded.sounds.push_back(handle);
        }
        maximumVariationCount = std::max(maximumVariationCount, loaded.sounds.size());
        maximumSetIdLength = std::max(maximumSetIdLength, loaded.id.size());
        if (!loaded.sounds.empty()) footstepSets.emplace(loaded.id, std::move(loaded));
    }
    ReserveFootstepPlaybackState(
            footstepPlayback,
            maximumVariationCount,
            maximumSetIdLength);
    roomtonePlaybacks.reserve(levelMusicById.size());
    soundEmitterPlaybacks.reserve(map.soundEmitters.size());
    for (const SectorCompiledSoundEmitter& source : map.soundEmitters) {
        SoundEmitterPlayback emitter;
        emitter.id = source.id;
        emitter.soundId = source.soundId;
        emitter.positionWorld = source.positionWorld;
        emitter.volume = source.volume;
        emitter.playbackVolume = source.volume;
        emitter.loop = source.loop;
        emitter.loopRequested = source.loop;
        emitter.autoStartPending = source.loop;
        const auto definition = map.audioSettings.soundsById.find(source.soundId);
        if (definition != map.audioSettings.soundsById.end()
                && definition->second.type == SectorSoundType::Music) {
            const std::string path = ResolveSectorAudioAssetPath(
                    definition->second.path);
            const std::string instanceKey = "sound_emitter:" + source.id;
            emitter.music = context.assets.RequestMusicInstance(
                    audioScope, instanceKey.c_str(), path.c_str());
        } else {
            emitter.sound = FindLevelSound(source.soundId);
        }
        soundEmitterPlaybacks.push_back(std::move(emitter));
    }
}

void SectorSceneRuntime::BindRuntimeObjectAudio(engine::World& world)
{
    world.ForEach<SectorDoor, SectorDoorAudio>(
            [this](engine::Entity, SectorDoor&, SectorDoorAudio& audio) {
                audio.openSound = FindLevelSound(audio.openSoundId);
                audio.closeSound = FindLevelSound(audio.closeSoundId);
                audio.pendingEvent = SectorDoorAudioEvent::None;
            });
}

void SectorSceneRuntime::PlayPendingNpcFootsteps(
        engine::EngineContext& context)
{
    for (NpcNavigationRecord& record : npcNavigation.records) {
        if (!record.footstepEvent) continue;
        record.footstepEvent = false;
        if (!record.occupied || !context.world.IsAlive(record.entity)
                || !context.world.Has<SectorObjectTransform>(record.entity)
                || !context.world.Has<SectorObject>(record.entity)) {
            continue;
        }
        const SectorObjectTransform& transform =
                context.world.Get<SectorObjectTransform>(record.entity);
        const SectorObject& object =
                context.world.Get<SectorObject>(record.entity);
        PlayFootstepForSectorAt(
                context,
                object.currentSectorId,
                footstepVolume,
                MakeNpcFootstepPositionalSettings(transform.position));
    }
}

void SectorSceneRuntime::UpdateLevelAudio(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        float rawDt,
        int playerSectorId)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;

    if (playerSectorId != lastRoomtoneSectorId) {
        lastRoomtoneSectorId = playerSectorId;
        const SectorTopologySector* sector = FindSectorTopologySector(map, playerSectorId);
        if (sector != nullptr && sector->roomtone.mode != SectorRoomtoneMode::Inherit) {
            const int fadeMilliseconds = sector->roomtone.fadeMilliseconds
                            == SectorRoomtoneSettings::UseMapFadeMilliseconds
                    ? map.audioSettings.roomtoneFadeMilliseconds
                    : sector->roomtone.fadeMilliseconds;
            roomtoneTransitionElapsedSeconds = 0.0f;
            roomtoneTransitionDurationSeconds =
                    static_cast<float>(fadeMilliseconds) / 1000.0f;
            for (RoomtonePlayback& playback : roomtonePlaybacks) {
                playback.startVolume = playback.currentVolume;
                playback.targetVolume = 0.0f;
            }

            if (sector->roomtone.mode == SectorRoomtoneMode::Play) {
                const engine::MusicHandle requested = FindLevelMusic(
                        sector->roomtone.soundId);
                if (engine::IsNull(requested)) {
                    TraceLog(
                            LOG_WARNING,
                            "Sector %d roomtone '%s' is missing or is not streaming Music",
                            sector->id,
                            sector->roomtone.soundId.c_str());
                    roomtoneTransitionDurationSeconds = 0.0f;
                    for (RoomtonePlayback& playback : roomtonePlaybacks) {
                        playback.targetVolume = playback.startVolume;
                    }
                } else {
                    auto existing = std::find_if(
                            roomtonePlaybacks.begin(), roomtonePlaybacks.end(),
                            [requested](const RoomtonePlayback& playback) {
                                return playback.music == requested;
                            });
                    if (existing == roomtonePlaybacks.end()) {
                        RoomtonePlayback incoming;
                        incoming.soundId = sector->roomtone.soundId;
                        incoming.music = requested;
                        incoming.targetVolume = sector->roomtone.volume;
                        roomtonePlaybacks.push_back(std::move(incoming));
                    } else {
                        existing->targetVolume = sector->roomtone.volume;
                    }
                }
            }
        }
    }

    const float transitionT = roomtoneTransitionDurationSeconds <= 0.0f
            ? 1.0f
            : std::clamp(
                    (roomtoneTransitionElapsedSeconds + dt)
                            / roomtoneTransitionDurationSeconds,
                    0.0f,
                    1.0f);
    constexpr float HalfPi = 1.57079632679489661923f;
    for (RoomtonePlayback& playback : roomtonePlaybacks) {
        if (playback.targetVolume >= playback.startVolume) {
            playback.currentVolume = playback.startVolume
                    + (playback.targetVolume - playback.startVolume)
                            * std::sin(transitionT * HalfPi);
        } else {
            playback.currentVolume = playback.targetVolume
                    + (playback.startVolume - playback.targetVolume)
                            * std::cos(transitionT * HalfPi);
        }
        engine::MusicPlaybackSettings settings;
        settings.volume = playback.currentVolume;
        if (context.assets.IsReady(playback.music)) {
            if (context.audio.PlayMusic(context.assets, playback.music, settings)) {
            }
        } else if (context.assets.HasFailed(playback.music)
                && !playback.failureReported) {
            TraceLog(LOG_WARNING, "Roomtone '%s' failed to load",
                    playback.soundId.c_str());
            playback.failureReported = true;
            playback.targetVolume = 0.0f;
        }
    }
    roomtoneTransitionElapsedSeconds += dt;
    if (transitionT >= 1.0f) {
        for (size_t index = roomtonePlaybacks.size(); index > 0; --index) {
            RoomtonePlayback& playback = roomtonePlaybacks[index - 1];
            playback.currentVolume = playback.targetVolume;
            playback.startVolume = playback.targetVolume;
            if (playback.targetVolume <= 0.0f) {
                context.audio.StopMusic(context.assets, playback.music);
                roomtonePlaybacks.erase(roomtonePlaybacks.begin()
                        + static_cast<std::ptrdiff_t>(index - 1));
            }
        }
    }

    for (SoundEmitterPlayback& emitter : soundEmitterPlaybacks) {
        if (!emitter.loop || !emitter.loopRequested
                || (!emitter.autoStartPending
                    && (context.audio.IsSoundPlaying(emitter.playback)
                        || context.audio.IsMusicPlaying(emitter.music)))) {
            continue;
        }
        const bool streaming = !engine::IsNull(emitter.music);
        if (engine::IsNull(emitter.sound) && !streaming) {
            if (!emitter.failureReported && !emitter.soundId.empty()) {
                TraceLog(LOG_WARNING,
                        "Sound emitter '%s' references missing map Sound/Music '%s'",
                        emitter.id.c_str(), emitter.soundId.c_str());
                emitter.failureReported = true;
            }
            emitter.autoStartPending = false;
            continue;
        }
        const bool ready = streaming
                ? context.assets.IsReady(emitter.music)
                : context.assets.IsReady(emitter.sound);
        if (!ready) {
            const bool failed = streaming
                    ? context.assets.HasFailed(emitter.music)
                    : context.assets.HasFailed(emitter.sound);
            if (failed) {
                emitter.autoStartPending = false;
            }
            continue;
        }
        engine::PositionalSoundSettings positional;
        positional.position = emitter.positionWorld;
        if (streaming) {
            engine::MusicPlaybackSettings settings;
            settings.volume = emitter.playbackVolume;
            settings.pitch = emitter.pitch;
            settings.looping = true;
            context.audio.PlayMusicAt(
                    context.assets, emitter.music, positional, settings);
        } else {
            engine::SoundPlaybackSettings settings;
            settings.volume = emitter.playbackVolume;
            settings.pitch = emitter.pitch;
            settings.looping = true;
            emitter.playback = context.audio.PlaySoundAt(
                    context.assets, emitter.sound, positional, settings);
        }
        emitter.autoStartPending = false;
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
        bool useBakedAmbientOcclusion,
        SectorUseHighlight useHighlight)
{
    renderer.DrawScene(
            context.assets,
            useBakedAmbientOcclusion,
            &context.world,
            SectorRuntimeDoorLightingContext{
                    &runtimeObjects.objectLightProbes,
                    &map,
                    runtimeObjects.staticLightingRevision},
            map.fogSettings,
            false,
            useHighlight);
    BeginMode3D(renderer.RenderCamera());
    impactParticles.Draw(
            renderer.RenderCamera(),
            renderer.VisibilityResult());
    EndMode3D();
}

void SectorSceneRuntime::ApplyWorldAtmosphere(
        engine::RenderTarget& sceneTarget,
        const SectorTopologyMap& map,
        bool collectGpuDiagnostics)
{
    renderer.ApplyWorldAtmosphere(
            sceneTarget,
            map,
            runtimeObjects.objectLightProbes,
            collectGpuDiagnostics);
}

void SectorSceneRuntime::ApplyGlass(
        engine::RenderTarget& sceneTarget,
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        bool collectGpuDiagnostics)
{
    renderer.ApplyGlass(
            sceneTarget,
            context.assets,
            &context.world,
            SectorRuntimeDoorLightingContext{
                    &runtimeObjects.objectLightProbes,
                    &map,
                    runtimeObjects.staticLightingRevision},
            map.fogSettings,
            collectGpuDiagnostics);
}

void SectorSceneRuntime::ApplyHdrBloom(
        engine::RenderTarget& sceneTarget,
        const engine::HdrBloomSettings& settings,
        bool presentFromScratch)
{
    renderer.ApplyHdrBloom(sceneTarget, settings, presentFromScratch);
}

bool SectorSceneRuntime::PreparePostBloomWorldOverlays(
        engine::RenderTarget& sceneTarget,
        bool overlayRequested)
{
    return renderer.PreparePostBloomWorldOverlays(
            sceneTarget, overlayRequested);
}

bool SectorSceneRuntime::CompositeViewmodel(
        engine::RenderTarget& sceneTarget,
        const engine::RenderTarget& viewmodelTarget)
{
    return renderer.CompositeViewmodel(sceneTarget, viewmodelTarget);
}

} // namespace game
