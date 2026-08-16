#pragma once

#include "game/FpsWeaponRegistry.h"
#include "game/SoundSetAudio.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace game {

struct PlayerSoundRuntimeSet {
    LoadedSoundSet soundSet;
};

struct PlayerSoundRuntimeEvent {
    std::string id;
    size_t setIndex = 0;
    float volume = 1.0f;
    SoundSetPlaybackState playback;
};

struct PlayerAudioRuntime {
    std::vector<PlayerSoundRuntimeSet> sets;
    std::vector<PlayerSoundRuntimeEvent> events;
    engine::MusicHandle heavyBreathing = engine::NullMusicHandle();
};

struct PlayerBreathingAudioRuntime {
    float volume = 0.0f;
    bool playing = false;
};

void RequestPlayerAudioAssets(
        engine::AssetManager& assets,
        const PlayerSoundApplicationSettings& settings,
        PlayerAudioRuntime& runtime);

engine::SoundPlaybackHandle PlayPlayerSound(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerAudioRuntime& runtime,
        std::string_view eventId);
engine::SoundPlaybackHandle PlayPlayerSoundAt(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerAudioRuntime& runtime,
        std::string_view eventId,
        const engine::PositionalSoundSettings& positional);
void UpdatePlayerBreathingAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const PlayerAudioRuntime& playerAudio,
        PlayerBreathingAudioRuntime& runtime,
        const PlayerBreathingAudioApplicationSettings& settings,
        float staminaRatio,
        float dt);
void StopPlayerBreathingAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const PlayerAudioRuntime& playerAudio,
        PlayerBreathingAudioRuntime& runtime);

} // namespace game
