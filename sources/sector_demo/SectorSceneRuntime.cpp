#include "sector_demo/SectorSceneRuntime.h"

#include "sector_demo/SectorAssetPaths.h"

#include <raylib.h>

#include <algorithm>
#include <set>

namespace game {

bool SectorSceneRuntime::Rebuild(
        engine::EngineContext& context,
        const SectorTopologyMap& map,
        const char* assetScopeName,
        const std::string& defaultFootstepSet,
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
    BeginLevelAudio(context, map, assetScopeName, defaultFootstepSet);
    BindRuntimeObjectAudio(context.world);
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
    BindRuntimeObjectAudio(context.world);
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
    UpdateSectorDoorAudioSystem(
            context.world,
            context.assets,
            context.audio);
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
        const std::string& defaultFootstepSet)
{
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
                    &map},
            map.fogSettings);
}

void SectorSceneRuntime::ApplyWorldAtmosphere(
        engine::RenderTarget& sceneTarget,
        const SectorTopologyMap& map)
{
    renderer.ApplyWorldAtmosphere(
            sceneTarget,
            map,
            runtimeObjects.objectLightProbes);
}

void SectorSceneRuntime::ApplyHdrBloom(
        engine::RenderTarget& sceneTarget,
        const engine::HdrBloomSettings& settings)
{
    renderer.ApplyHdrBloom(sceneTarget, settings);
}

bool SectorSceneRuntime::CompositeViewmodel(
        engine::RenderTarget& sceneTarget,
        const engine::RenderTarget& viewmodelTarget)
{
    return renderer.CompositeViewmodel(sceneTarget, viewmodelTarget);
}

} // namespace game
