#pragma once

#include "game/FpsWeaponRegistry.h"
#include "game/SoundSetAudio.h"

#include <algorithm>
#include <cmath>
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
    engine::SoundHandle heartbeat = engine::NullSoundHandle();
    engine::SoundHandle liquidSplash = engine::NullSoundHandle();
    engine::SoundHandle liquidExit = engine::NullSoundHandle();
    engine::SoundHandle liquidSwimLoop = engine::NullSoundHandle();
};

struct PlayerLiquidAudioFrameState {
    bool initialized = false;
    bool wasSwimming = false;
};

struct PlayerLiquidAudioFrameDecision {
    bool playEntrySplash = false;
    bool playExitSound = false;
    bool loopShouldExist = false;
    bool loopShouldPlay = false;
};

struct PlayerLiquidAudioPlaybackState {
    PlayerLiquidAudioFrameState frame;
    engine::SoundPlaybackHandle swimLoopPlayback =
            engine::NullSoundPlaybackHandle();
};

inline float AdvancePlayerLiquidRoomtoneGain(
        float currentGain,
        bool cameraSubmerged,
        const PlayerLiquidAudioApplicationSettings& settings,
        float rawDt)
{
    const float target = cameraSubmerged ? 0.0f : 1.0f;
    const float current = std::clamp(
            std::isfinite(currentGain) ? currentGain : 1.0f,
            0.0f,
            1.0f);
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    const float duration = cameraSubmerged
            ? settings.roomtoneSubmergeFadeSeconds
            : settings.roomtoneResurfaceFadeSeconds;
    if (!std::isfinite(duration) || duration <= 0.0f) return target;
    const float maximumChange = dt / duration;
    return current < target
            ? std::min(target, current + maximumChange)
            : std::max(target, current - maximumChange);
}

inline PlayerLiquidAudioFrameDecision AdvancePlayerLiquidAudioFrame(
        PlayerLiquidAudioFrameState& state,
        bool swimming,
        bool exitingWater,
        bool swimControlHeld)
{
    PlayerLiquidAudioFrameDecision result;
    if (state.initialized && swimming != state.wasSwimming) {
        result.playEntrySplash = swimming;
        result.playExitSound = !swimming;
    }
    state.initialized = true;
    state.wasSwimming = swimming;
    result.loopShouldExist = swimming && !exitingWater;
    result.loopShouldPlay = result.loopShouldExist && swimControlHeld;
    return result;
}

struct PlayerBreathingAudioRuntime {
    float volume = 0.0f;
    bool playing = false;
};

struct PlayerHeartbeatAudioRuntime {
    engine::SoundPlaybackHandle playback =
            engine::NullSoundPlaybackHandle();
    float volume = 0.0f;
    float pitch = 1.0f;
};

void RequestPlayerAudioAssets(
        engine::AssetManager& assets,
        const PlayerSoundApplicationSettings& settings,
        const PlayerLiquidAudioApplicationSettings& liquidSettings,
        PlayerAudioRuntime& runtime);

void UpdatePlayerLiquidAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const PlayerAudioRuntime& playerAudio,
        PlayerLiquidAudioPlaybackState& state,
        bool swimming,
        bool exitingWater,
        bool swimControlHeld);
void StopPlayerLiquidAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerLiquidAudioPlaybackState& state);

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
void UpdatePlayerHeartbeatAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        const PlayerAudioRuntime& playerAudio,
        PlayerHeartbeatAudioRuntime& runtime,
        const PlayerHeartbeatAudioApplicationSettings& settings,
        const Health& health,
        float dt);
void StopPlayerHeartbeatAudio(
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        PlayerHeartbeatAudioRuntime& runtime);

} // namespace game
