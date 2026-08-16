#include "game/PlayerAudio.h"

#include "sector_demo/SectorAssetPaths.h"

#include <raylib.h>

#include <algorithm>

namespace game {

namespace {

constexpr float MinimumPlayerSoundPitch = 0.98f;
constexpr float MaximumPlayerSoundPitch = 1.02f;
constexpr float BreathingVolumeEpsilon = 0.0001f;

PlayerSoundRuntimeEvent* FindPlayerSoundEvent(
        PlayerAudioRuntime& runtime,
        std::string_view eventId)
{
    const auto found = std::find_if(
            runtime.events.begin(),
            runtime.events.end(),
            [eventId](const PlayerSoundRuntimeEvent& event) {
                return event.id == eventId;
            });
    return found == runtime.events.end() ? nullptr : &*found;
}

engine::SoundPlaybackHandle PlayPlayerSoundInternal(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerAudioRuntime& runtime,
        std::string_view eventId,
        const engine::PositionalSoundSettings* positional)
{
    PlayerSoundRuntimeEvent* event = FindPlayerSoundEvent(runtime, eventId);
    if (event == nullptr || event->setIndex >= runtime.sets.size()) {
        return engine::NullSoundPlaybackHandle();
    }
    LoadedSoundSet& set = runtime.sets[event->setIndex].soundSet;
    return positional == nullptr
            ? PlaySoundSet(
                    assets,
                    audio,
                    set,
                    event->playback,
                    event->volume,
                    MinimumPlayerSoundPitch,
                    MaximumPlayerSoundPitch)
            : PlaySoundSetAt(
                    assets,
                    audio,
                    set,
                    event->playback,
                    event->volume,
                    MinimumPlayerSoundPitch,
                    MaximumPlayerSoundPitch,
                    *positional);
}

} // namespace

void RequestPlayerAudioAssets(
        engine::AssetManager& assets,
        const PlayerSoundApplicationSettings& settings,
        PlayerAudioRuntime& runtime)
{
    runtime = PlayerAudioRuntime{};
    const std::string breathingPath = ResolveSectorAudioAssetPath(
            "player/Heavy_Breathing_01.ogg");
    runtime.heavyBreathing = assets.RequestMusic(
            assets.GlobalScope(),
            breathingPath.c_str());
    const SoundSetCatalog catalog = DiscoverSoundSetCatalog(
            ASSETS_PATH "audio/player",
            "player");
    if (!catalog.warning.empty()) TraceLog(LOG_WARNING, "%s", catalog.warning.c_str());

    runtime.sets.reserve(settings.events.size());
    runtime.events.reserve(settings.events.size());
    for (const PlayerSoundEventSettings& configured : settings.events) {
        const SoundSetCatalogSet* source = FindSoundSetCatalogSet(
                catalog,
                configured.set);
        if (source == nullptr) {
            TraceLog(
                    LOG_WARNING,
                    "Player sound event '%s' references missing set '%s'",
                    configured.id.c_str(),
                    configured.set.c_str());
            continue;
        }

        size_t setIndex = runtime.sets.size();
        for (size_t i = 0; i < runtime.sets.size(); ++i) {
            if (runtime.sets[i].soundSet.id == source->id) {
                setIndex = i;
                break;
            }
        }
        if (setIndex == runtime.sets.size()) {
            PlayerSoundRuntimeSet runtimeSet;
            runtimeSet.soundSet.id = source->id;
            runtimeSet.soundSet.sounds.reserve(source->relativePaths.size());
            for (const std::string& relativePath : source->relativePaths) {
                const std::string path = ResolveSectorAudioAssetPath(relativePath);
                const engine::SoundHandle handle = assets.RequestSound(
                        assets.GlobalScope(),
                        path.c_str());
                if (!engine::IsNull(handle)) {
                    runtimeSet.soundSet.sounds.push_back(handle);
                }
            }
            if (runtimeSet.soundSet.sounds.empty()) {
                TraceLog(
                        LOG_WARNING,
                        "Player sound set '%s' has no requestable variations",
                        source->id.c_str());
                continue;
            }
            runtime.sets.push_back(std::move(runtimeSet));
        }

        PlayerSoundRuntimeEvent event;
        event.id = configured.id;
        event.setIndex = setIndex;
        event.volume = configured.volume;
        const LoadedSoundSet& loaded = runtime.sets[setIndex].soundSet;
        ReserveSoundSetPlaybackState(
                event.playback,
                loaded.sounds.size(),
                loaded.id.size());
        runtime.events.push_back(std::move(event));
    }
}

engine::SoundPlaybackHandle PlayPlayerSound(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerAudioRuntime& runtime,
        std::string_view eventId)
{
    return PlayPlayerSoundInternal(
            assets, audio, runtime, eventId, nullptr);
}

engine::SoundPlaybackHandle PlayPlayerSoundAt(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerAudioRuntime& runtime,
        std::string_view eventId,
        const engine::PositionalSoundSettings& positional)
{
    return PlayPlayerSoundInternal(
            assets, audio, runtime, eventId, &positional);
}

void UpdatePlayerBreathingAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const PlayerAudioRuntime& playerAudio,
        PlayerBreathingAudioRuntime& runtime,
        const PlayerBreathingAudioApplicationSettings& settings,
        float staminaRatio,
        float dt)
{
    runtime.volume = AdvancePlayerBreathingAudioVolume(
            runtime.volume,
            settings,
            staminaRatio,
            dt);
    if (runtime.volume > BreathingVolumeEpsilon) {
        runtime.playing = audio.PlayMusic(
                assets,
                playerAudio.heavyBreathing,
                engine::MusicPlaybackSettings{
                        runtime.volume,
                        1.0f,
                        0.0f,
                        true});
        return;
    }

    runtime.volume = 0.0f;
    if (runtime.playing
            || audio.IsMusicPlaying(playerAudio.heavyBreathing)) {
        audio.StopMusic(assets, playerAudio.heavyBreathing);
    }
    runtime.playing = false;
}

void StopPlayerBreathingAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const PlayerAudioRuntime& playerAudio,
        PlayerBreathingAudioRuntime& runtime)
{
    if (runtime.playing
            || audio.IsMusicPlaying(playerAudio.heavyBreathing)) {
        audio.StopMusic(assets, playerAudio.heavyBreathing);
    }
    runtime = PlayerBreathingAudioRuntime{};
}

} // namespace game
