#include "sector_demo/SectorSceneRuntime.h"

#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorAudioOcclusion.h"
#include "engine/systems/AnimatedModelSystem.h"

#include <raylib.h>

#include <algorithm>
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
    navigation.Shutdown();
    StopLevelAudio(context);
    if (!renderer.RebuildRendererResources(
                context.assets,
                map,
                assetScopeName,
                error)) {
        return false;
    }
    if (!navigation.Initialize(BuildSectorNavigationSettingsForMap(map))) {
        TraceLog(LOG_WARNING, "Navigation service initialization failed");
    }
    ResetSectorRuntimeObjectsForMap(
            context.world,
            context.assets,
            runtimeObjects,
            map);
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
    navigation.Shutdown();
    if (!navigation.Initialize(BuildSectorNavigationSettingsForMap(map))) {
        TraceLog(LOG_WARNING, "Navigation service initialization failed");
        return false;
    }
    navigation.RequestRebuild();
    InitializeNpcNavigationRuntime(context.world, navigation, npcNavigation);
    InitializeNpcCombatRuntime(npcCombat, map.runtimeObjects.size());
    impactParticles.Clear();
    return true;
}

void SectorSceneRuntime::Shutdown(engine::EngineContext& context)
{
    ShutdownNpcAudioRuntime(context.assets, context.audio, npcAudio);
    ShutdownNpcNavigationRuntime(context.world, navigation, npcNavigation);
    ClearNpcCombatRuntime(npcCombat);
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
    navigation.ResetForRebuild();
    ResetSectorRuntimeObjectsForMap(
            context.world,
            context.assets,
            runtimeObjects,
            map);
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
    impactParticles.Clear();
    BindRuntimeObjectAudio(context.world);
}

void SectorSceneRuntime::Update(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        float dt,
        const Vector3* playerPosition,
        const SectorDoorPlayerObstacle* playerObstacle)
{
    UpdateLevelAudio(context);
    PrepareNpcDoorTraversalAndHoldsSystem(
            context.world,
            navigation,
            npcNavigation,
            runtimeObjects.dynamicDoorColliders,
            dt);
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
                runtimeObjects.staticModelColliders,
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
            runtimeObjects.staticModelColliders,
            runtimeObjects.staticModelPendingCount);
    navigation.UpdateDynamicObstacles(
            runtimeObjects.dynamicModelColliders,
            dt);
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
                runtimeObjects.staticModelColliders,
                runtimeObjects.objectLightProbes,
                map,
                dt,
                playerObstacle);
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
    context.audio.UpdatePositionalSoundOcclusion(
            dt,
            &audioOcclusion,
            QuerySectorSoundOcclusion);
    engine::AnimatedModelSystem(context.world, context.assets, dt);
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
            &npcAudio);
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
            runtimeObjects.staticModelColliders,
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
                runtimeObjects.staticModelColliders,
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
    if (!engine::IsNull(backgroundMusic)) {
        context.audio.StopMusic(context.assets, backgroundMusic);
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
    backgroundMusic = engine::NullMusicHandle();
    levelMusicVolume = SectorLevelAudioSettings::DefaultMusicVolume;
    levelMusicStartPending = false;
    levelMusicFailureReported = false;
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

    if (map.audioSettings.musicPath.empty()
            && map.audioSettings.soundsById.empty()
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
    if (!map.audioSettings.musicPath.empty()) {
        const std::string path = ResolveSectorAudioAssetPath(
                map.audioSettings.musicPath);
        backgroundMusic = context.assets.RequestMusic(audioScope, path.c_str());
        levelMusicVolume = map.audioSettings.musicVolume;
        levelMusicStartPending = !engine::IsNull(backgroundMusic);
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

void SectorSceneRuntime::UpdateLevelAudio(engine::EngineContext& context)
{
    if (!levelMusicStartPending || engine::IsNull(backgroundMusic)) return;
    if (context.assets.IsReady(backgroundMusic)) {
        engine::MusicPlaybackSettings settings;
        settings.volume = levelMusicVolume;
        if (context.audio.PlayMusic(
                    context.assets,
                    backgroundMusic,
                    settings)) {
            levelMusicStartPending = false;
        }
        return;
    }
    if (context.assets.HasFailed(backgroundMusic)) {
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
                    &map,
                    runtimeObjects.staticLightingRevision},
            map.fogSettings);
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

void SectorSceneRuntime::ApplyHdrBloom(
        engine::RenderTarget& sceneTarget,
        const engine::HdrBloomSettings& settings,
        bool presentFromScratch)
{
    renderer.ApplyHdrBloom(sceneTarget, settings, presentFromScratch);
}

bool SectorSceneRuntime::CompositeViewmodel(
        engine::RenderTarget& sceneTarget,
        const engine::RenderTarget& viewmodelTarget)
{
    return renderer.CompositeViewmodel(sceneTarget, viewmodelTarget);
}

} // namespace game
