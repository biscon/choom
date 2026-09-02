#include "game/PlayerAudio.h"

#include "sector_demo/SectorAssetPaths.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace game {

namespace {

constexpr float MinimumPlayerSoundPitch = 0.98f;
constexpr float MaximumPlayerSoundPitch = 1.02f;
constexpr float BreathingVolumeEpsilon = 0.0001f;
constexpr float HeartbeatVolumeEpsilon = 0.0001f;

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
        const PlayerLiquidAudioApplicationSettings& liquidSettings,
        PlayerAudioRuntime& runtime)
{
    runtime = PlayerAudioRuntime{};
    const std::string breathingPath = ResolveSectorAudioAssetPath(
            "player/Heavy_Breathing_01.ogg");
    runtime.heavyBreathing = assets.RequestMusic(
            assets.GlobalScope(),
            breathingPath.c_str());
    const std::string heartbeatPath = ResolveSectorAudioAssetPath(
            "player/heartbeat_loop.wav");
    runtime.heartbeat = assets.RequestSound(
            assets.GlobalScope(),
            heartbeatPath.c_str());
    if (!liquidSettings.splashSoundPath.empty()) {
        const std::string splashPath = ResolveSectorAudioAssetPath(
                liquidSettings.splashSoundPath);
        runtime.liquidSplash = assets.RequestSound(
                assets.GlobalScope(), splashPath.c_str());
    }
    if (!liquidSettings.exitSoundPath.empty()) {
        const std::string exitPath = ResolveSectorAudioAssetPath(
                liquidSettings.exitSoundPath);
        runtime.liquidExit = assets.RequestSound(
                assets.GlobalScope(), exitPath.c_str());
    }
    if (!liquidSettings.swimLoopSoundPath.empty()) {
        const std::string loopPath = ResolveSectorAudioAssetPath(
                liquidSettings.swimLoopSoundPath);
        runtime.liquidSwimLoop = assets.RequestSound(
                assets.GlobalScope(), loopPath.c_str());
    }
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

void UpdatePlayerLiquidAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const PlayerAudioRuntime& playerAudio,
        PlayerLiquidAudioPlaybackState& state,
        bool swimming,
        bool exitingWater,
        bool swimControlHeld)
{
    const PlayerLiquidAudioFrameDecision decision =
            AdvancePlayerLiquidAudioFrame(
                    state.frame,
                    swimming,
                    exitingWater,
                    swimControlHeld);
    if (decision.playEntrySplash
            && !engine::IsNull(playerAudio.liquidSplash)) {
        audio.PlaySound(assets, playerAudio.liquidSplash);
    }
    if (decision.playExitSound && !engine::IsNull(playerAudio.liquidExit)) {
        audio.PlaySound(assets, playerAudio.liquidExit);
    }

    if (!decision.loopShouldExist) {
        if (!engine::IsNull(state.swimLoopPlayback)) {
            audio.StopSound(assets, state.swimLoopPlayback);
            state.swimLoopPlayback = engine::NullSoundPlaybackHandle();
        }
        return;
    }

    if (decision.loopShouldPlay) {
        if (!audio.IsSoundPlaying(state.swimLoopPlayback)) {
            engine::SoundPlaybackSettings settings;
            settings.looping = true;
            state.swimLoopPlayback = audio.PlaySound(
                    assets, playerAudio.liquidSwimLoop, settings);
        } else {
            audio.SetSoundPlaybackPaused(
                    assets, state.swimLoopPlayback, false);
        }
    } else if (audio.IsSoundPlaying(state.swimLoopPlayback)) {
        audio.SetSoundPlaybackPaused(
                assets, state.swimLoopPlayback, true);
    }
}

void StopPlayerLiquidAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerLiquidAudioPlaybackState& state)
{
    if (!engine::IsNull(state.swimLoopPlayback)) {
        audio.StopSound(assets, state.swimLoopPlayback);
    }
    state = PlayerLiquidAudioPlaybackState{};
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

void UpdatePlayerHeartbeatAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const PlayerAudioRuntime& playerAudio,
        PlayerHeartbeatAudioRuntime& runtime,
        const PlayerHeartbeatAudioApplicationSettings& settings,
        const Health& health,
        float rawDt)
{
    if (!settings.enabled || IsDepleted(health)) {
        StopPlayerHeartbeatAudio(assets, audio, runtime);
        return;
    }
    if (!std::isfinite(runtime.volume) || !std::isfinite(runtime.pitch)) {
        StopPlayerHeartbeatAudio(assets, audio, runtime);
    }

    const float targetVolume = PlayerHeartbeatVolume(health, settings);
    const float targetPitch = PlayerHeartbeatPitch(health, settings);
    const float dt = std::isfinite(rawDt)
            ? std::clamp(rawDt, 0.0f, 0.25f)
            : 0.0f;
    if (dt > 0.0f) {
        const float responseSeconds = std::max(
                0.001f, settings.responseSeconds);
        const float response = 1.0f - std::exp(-dt / responseSeconds);
        runtime.volume += (targetVolume - runtime.volume) * response;
        runtime.pitch += (targetPitch - runtime.pitch) * response;
    }
    runtime.volume = std::clamp(runtime.volume, 0.0f, 1.0f);
    runtime.pitch = std::clamp(runtime.pitch, 0.01f, 4.0f);

    if (runtime.volume <= HeartbeatVolumeEpsilon
            && targetVolume <= HeartbeatVolumeEpsilon) {
        StopPlayerHeartbeatAudio(assets, audio, runtime);
        return;
    }

    const engine::SoundPlaybackSettings playbackSettings{
            runtime.volume,
            runtime.pitch,
            0.0f,
            true};
    if (!audio.IsSoundPlaying(runtime.playback)) {
        runtime.playback = audio.PlaySound(
                assets,
                playerAudio.heartbeat,
                playbackSettings);
        return;
    }
    if (!audio.SetSoundPlaybackSettings(
                assets,
                runtime.playback,
                playbackSettings)) {
        runtime.playback = engine::NullSoundPlaybackHandle();
    }
}

void StopPlayerHeartbeatAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerHeartbeatAudioRuntime& runtime)
{
    if (!engine::IsNull(runtime.playback)) {
        audio.StopSound(assets, runtime.playback);
    }
    runtime = PlayerHeartbeatAudioRuntime{};
}

} // namespace game
