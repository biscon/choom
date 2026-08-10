#include "engine/audio/AudioSystem.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace engine {

namespace {

constexpr size_t InvalidSlot = std::numeric_limits<size_t>::max();

float FiniteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

SoundPlaybackSettings NormalizeSettings(SoundPlaybackSettings settings)
{
    settings.volume = std::clamp(FiniteOr(settings.volume, 1.0f), 0.0f, 1.0f);
    settings.pitch = std::clamp(FiniteOr(settings.pitch, 1.0f), 0.01f, 4.0f);
    settings.pan = std::clamp(FiniteOr(settings.pan, 0.0f), -1.0f, 1.0f);
    return settings;
}

PositionalSoundSettings NormalizePositional(PositionalSoundSettings settings)
{
    settings.minimumDistanceWorld = std::max(
            0.0f,
            FiniteOr(settings.minimumDistanceWorld, 1.0f));
    settings.maximumDistanceWorld = std::max(
            settings.minimumDistanceWorld + 0.001f,
            FiniteOr(settings.maximumDistanceWorld, 25.0f));
    if (!std::isfinite(settings.position.x)
            || !std::isfinite(settings.position.y)
            || !std::isfinite(settings.position.z)) {
        settings.position = {};
    }
    return settings;
}

MusicPlaybackSettings NormalizeSettings(MusicPlaybackSettings settings)
{
    settings.volume = std::clamp(FiniteOr(settings.volume, 1.0f), 0.0f, 1.0f);
    settings.pitch = std::clamp(FiniteOr(settings.pitch, 1.0f), 0.01f, 4.0f);
    settings.pan = std::clamp(FiniteOr(settings.pan, 0.0f), -1.0f, 1.0f);
    return settings;
}

float ToRaylibPan(float pan)
{
    return (std::clamp(pan, -1.0f, 1.0f) + 1.0f) * 0.5f;
}

} // namespace

AudioSpatialization ComputeAudioSpatialization(
        const AudioListener& listener,
        const PositionalSoundSettings& unnormalizedSource)
{
    const PositionalSoundSettings source = NormalizePositional(
            unnormalizedSource);
    const Vector3 offset = Vector3Subtract(source.position, listener.position);
    const float distance = Vector3Length(offset);

    AudioSpatialization result;
    if (distance <= source.minimumDistanceWorld) {
        result.volumeScale = 1.0f;
    } else if (distance >= source.maximumDistanceWorld) {
        result.volumeScale = 0.0f;
    } else {
        result.volumeScale = 1.0f
                - (distance - source.minimumDistanceWorld)
                / (source.maximumDistanceWorld
                        - source.minimumDistanceWorld);
    }

    Vector3 forward = listener.forward;
    Vector3 up = listener.up;
    if (Vector3LengthSqr(forward) <= 0.000001f) {
        forward = Vector3{0.0f, 0.0f, 1.0f};
    }
    if (Vector3LengthSqr(up) <= 0.000001f) {
        up = Vector3{0.0f, 1.0f, 0.0f};
    }
    forward = Vector3Normalize(forward);
    up = Vector3Normalize(up);
    Vector3 right = Vector3CrossProduct(forward, up);
    if (Vector3LengthSqr(right) <= 0.000001f) {
        right = Vector3{1.0f, 0.0f, 0.0f};
    } else {
        right = Vector3Normalize(right);
    }
    result.pan = distance <= 0.000001f
            ? 0.0f
            : std::clamp(
                    Vector3DotProduct(Vector3Scale(offset, 1.0f / distance), right),
                    -1.0f,
                    1.0f);
    return result;
}

AudioSystem::~AudioSystem()
{
    Shutdown();
}

bool AudioSystem::Initialize(
        size_t soundPlaybackCapacity,
        size_t musicPlaybackCapacity)
{
    Shutdown();
    soundPlaybacks.resize(std::max<size_t>(1, soundPlaybackCapacity));
    musicPlaybacks.resize(std::max<size_t>(1, musicPlaybackCapacity));
    InitAudioDevice();
    deviceReady = IsAudioDeviceReady();
    if (!deviceReady) {
        std::fprintf(stderr,
                "[AudioSystem WARNING] Audio device initialization failed; continuing silently\n");
    }
    return deviceReady;
}

void AudioSystem::Shutdown()
{
    soundPlaybacks.clear();
    musicPlaybacks.clear();
    suspended = false;
    nextSequence = 1;
    if (deviceReady || IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    deviceReady = false;
}

void AudioSystem::SetListener(const AudioListener& value)
{
    listener = value;
}

void AudioSystem::Update(AssetManager& assets)
{
    if (!deviceReady || suspended) return;

    for (size_t i = 0; i < soundPlaybacks.size(); ++i) {
        SoundPlaybackSlot& playback = soundPlaybacks[i];
        if (!playback.active) continue;
        const Sound* voice = assets.GetSoundVoice(
                playback.sound,
                playback.voiceIndex);
        if (voice == nullptr || !::IsSoundPlaying(*voice)) {
            DeactivateSoundSlot(assets, i, false);
            continue;
        }
        ApplySoundMix(*voice, playback);
    }

    for (MusicPlaybackSlot& playback : musicPlaybacks) {
        if (!playback.active) continue;
        const MusicAsset* asset = assets.GetMusic(playback.music);
        if (asset == nullptr) {
            playback = {};
            continue;
        }
        Music stream = asset->stream;
        stream.looping = playback.settings.looping;
        UpdateMusicStream(stream);
        if (!::IsMusicStreamPlaying(stream)) playback = {};
    }
}

SoundPlaybackHandle AudioSystem::PlaySound(
        AssetManager& assets,
        SoundHandle sound,
        const SoundPlaybackSettings& settings)
{
    return PlaySoundInternal(assets, sound, settings, nullptr);
}

SoundPlaybackHandle AudioSystem::PlaySoundAt(
        AssetManager& assets,
        SoundHandle sound,
        const PositionalSoundSettings& positional,
        const SoundPlaybackSettings& settings)
{
    return PlaySoundInternal(assets, sound, settings, &positional);
}

bool AudioSystem::SetSoundPosition(
        SoundPlaybackHandle playback,
        Vector3 position)
{
    if (!IsValidPlayback(playback)
            || !soundPlaybacks[playback.index].positional) {
        return false;
    }
    soundPlaybacks[playback.index].positionalSettings.position = position;
    return true;
}

bool AudioSystem::StopSound(
        AssetManager& assets,
        SoundPlaybackHandle playback)
{
    if (!IsValidPlayback(playback)) return false;
    DeactivateSoundSlot(assets, playback.index, true);
    return true;
}

void AudioSystem::StopSoundAsset(AssetManager& assets, SoundHandle sound)
{
    for (size_t i = 0; i < soundPlaybacks.size(); ++i) {
        if (soundPlaybacks[i].active
                && soundPlaybacks[i].sound == sound) {
            DeactivateSoundSlot(assets, i, true);
        }
    }
}

bool AudioSystem::IsSoundPlaying(SoundPlaybackHandle playback) const
{
    return IsValidPlayback(playback);
}

bool AudioSystem::PlayMusic(
        AssetManager& assets,
        MusicHandle music,
        const MusicPlaybackSettings& unnormalizedSettings)
{
    if (!deviceReady || suspended || IsNull(music)) return false;
    const MusicAsset* asset = assets.GetMusic(music);
    if (asset == nullptr) return false;
    const MusicPlaybackSettings settings = NormalizeSettings(
            unnormalizedSettings);

    MusicPlaybackSlot* freeSlot = nullptr;
    for (MusicPlaybackSlot& playback : musicPlaybacks) {
        if (playback.active && playback.music == music) {
            playback.settings = settings;
            Music stream = asset->stream;
            SetMusicVolume(stream, settings.volume);
            SetMusicPitch(stream, settings.pitch);
            SetMusicPan(stream, ToRaylibPan(settings.pan));
            return true;
        }
        if (!playback.active && freeSlot == nullptr) freeSlot = &playback;
    }
    if (freeSlot == nullptr) {
        std::fprintf(stderr,
                "[AudioSystem WARNING] Music playback capacity exceeded\n");
        return false;
    }
    Music stream = asset->stream;
    stream.looping = settings.looping;
    SetMusicVolume(stream, settings.volume);
    SetMusicPitch(stream, settings.pitch);
    SetMusicPan(stream, ToRaylibPan(settings.pan));
    ::PlayMusicStream(stream);
    freeSlot->active = true;
    freeSlot->music = music;
    freeSlot->settings = settings;
    return true;
}

bool AudioSystem::StopMusic(AssetManager& assets, MusicHandle music)
{
    for (MusicPlaybackSlot& playback : musicPlaybacks) {
        if (!playback.active || playback.music != music) continue;
        const MusicAsset* asset = assets.GetMusic(music);
        if (asset != nullptr) {
            Music stream = asset->stream;
            if (playback.pausedBySystem) ::ResumeMusicStream(stream);
            ::StopMusicStream(stream);
        }
        playback = {};
        return true;
    }
    return false;
}

bool AudioSystem::IsMusicPlaying(MusicHandle music) const
{
    for (const MusicPlaybackSlot& playback : musicPlaybacks) {
        if (playback.active && playback.music == music) return true;
    }
    return false;
}

void AudioSystem::PauseAll(AssetManager& assets)
{
    if (!deviceReady || suspended) return;
    for (SoundPlaybackSlot& playback : soundPlaybacks) {
        if (!playback.active) continue;
        const Sound* voice = assets.GetSoundVoice(
                playback.sound,
                playback.voiceIndex);
        if (voice != nullptr && ::IsSoundPlaying(*voice)) {
            ::PauseSound(*voice);
            playback.pausedBySystem = true;
        }
    }
    for (MusicPlaybackSlot& playback : musicPlaybacks) {
        if (!playback.active) continue;
        const MusicAsset* asset = assets.GetMusic(playback.music);
        if (asset != nullptr && ::IsMusicStreamPlaying(asset->stream)) {
            ::PauseMusicStream(asset->stream);
            playback.pausedBySystem = true;
        }
    }
    suspended = true;
}

void AudioSystem::ResumeAll(AssetManager& assets)
{
    if (!deviceReady || !suspended) return;
    for (SoundPlaybackSlot& playback : soundPlaybacks) {
        if (!playback.active || !playback.pausedBySystem) continue;
        const Sound* voice = assets.GetSoundVoice(
                playback.sound,
                playback.voiceIndex);
        if (voice != nullptr) ::ResumeSound(*voice);
        playback.pausedBySystem = false;
    }
    for (MusicPlaybackSlot& playback : musicPlaybacks) {
        if (!playback.active || !playback.pausedBySystem) continue;
        const MusicAsset* asset = assets.GetMusic(playback.music);
        if (asset != nullptr) ::ResumeMusicStream(asset->stream);
        playback.pausedBySystem = false;
    }
    suspended = false;
}

void AudioSystem::StopAll(AssetManager& assets)
{
    for (size_t i = 0; i < soundPlaybacks.size(); ++i) {
        if (soundPlaybacks[i].active) DeactivateSoundSlot(assets, i, true);
    }
    for (MusicPlaybackSlot& playback : musicPlaybacks) {
        if (!playback.active) continue;
        const MusicAsset* asset = assets.GetMusic(playback.music);
        if (asset != nullptr) {
            if (playback.pausedBySystem) ::ResumeMusicStream(asset->stream);
            ::StopMusicStream(asset->stream);
        }
        playback = {};
    }
    suspended = false;
}

SoundPlaybackHandle AudioSystem::PlaySoundInternal(
        AssetManager& assets,
        SoundHandle sound,
        const SoundPlaybackSettings& unnormalizedSettings,
        const PositionalSoundSettings* positional)
{
    if (!deviceReady || suspended || IsNull(sound)) {
        return NullSoundPlaybackHandle();
    }
    const SoundAsset* asset = assets.GetSound(sound);
    if (asset == nullptr || asset->voiceCount == 0) {
        return NullSoundPlaybackHandle();
    }

    const size_t voiceIndex = FindVoiceIndex(assets, sound);
    if (voiceIndex == InvalidSlot) return NullSoundPlaybackHandle();
    const size_t playbackIndex = FindSoundPlaybackSlot(assets, sound);
    if (playbackIndex == InvalidSlot) return NullSoundPlaybackHandle();

    SoundPlaybackSlot& playback = soundPlaybacks[playbackIndex];
    playback.active = true;
    playback.positional = positional != nullptr;
    playback.pausedBySystem = false;
    playback.sound = sound;
    playback.voiceIndex = voiceIndex;
    playback.sequence = nextSequence++;
    playback.settings = NormalizeSettings(unnormalizedSettings);
    if (positional != nullptr) {
        playback.positionalSettings = NormalizePositional(*positional);
    }

    const Sound* voice = assets.GetSoundVoice(sound, voiceIndex);
    if (voice == nullptr) {
        playback.active = false;
        return NullSoundPlaybackHandle();
    }
    ApplySoundMix(*voice, playback);
    ::PlaySound(*voice);
    return SoundPlaybackHandle{
            static_cast<uint32_t>(playbackIndex),
            playback.generation};
}

size_t AudioSystem::FindSoundPlaybackSlot(
        AssetManager& assets,
        SoundHandle sound)
{
    size_t freeSlot = InvalidSlot;
    size_t oldestSameSound = InvalidSlot;
    size_t oldestGlobal = InvalidSlot;
    uint64_t oldestSameSequence = std::numeric_limits<uint64_t>::max();
    uint64_t oldestGlobalSequence = std::numeric_limits<uint64_t>::max();
    for (size_t i = 0; i < soundPlaybacks.size(); ++i) {
        SoundPlaybackSlot& playback = soundPlaybacks[i];
        if (playback.active && !suspended && !playback.pausedBySystem) {
            const Sound* voice = assets.GetSoundVoice(
                    playback.sound,
                    playback.voiceIndex);
            if (voice == nullptr || !::IsSoundPlaying(*voice)) {
                DeactivateSoundSlot(assets, i, false);
            }
        }
        if (!playback.active) {
            if (freeSlot == InvalidSlot) freeSlot = i;
            continue;
        }
        if (playback.sequence < oldestGlobalSequence) {
            oldestGlobalSequence = playback.sequence;
            oldestGlobal = i;
        }
        if (playback.sound == sound
                && playback.sequence < oldestSameSequence) {
            oldestSameSequence = playback.sequence;
            oldestSameSound = i;
        }
    }
    if (freeSlot != InvalidSlot) return freeSlot;
    const size_t stolen = oldestSameSound != InvalidSlot
            ? oldestSameSound : oldestGlobal;
    if (stolen != InvalidSlot) DeactivateSoundSlot(assets, stolen, true);
    return stolen;
}

size_t AudioSystem::FindVoiceIndex(AssetManager& assets, SoundHandle sound)
{
    const SoundAsset* asset = assets.GetSound(sound);
    if (asset == nullptr) return InvalidSlot;
    for (size_t voiceIndex = 0; voiceIndex < asset->voiceCount; ++voiceIndex) {
        bool used = false;
        for (const SoundPlaybackSlot& playback : soundPlaybacks) {
            if (playback.active && playback.sound == sound
                    && playback.voiceIndex == voiceIndex) {
                used = true;
                break;
            }
        }
        if (!used) return voiceIndex;
    }

    size_t oldest = InvalidSlot;
    uint64_t oldestSequence = std::numeric_limits<uint64_t>::max();
    for (size_t i = 0; i < soundPlaybacks.size(); ++i) {
        const SoundPlaybackSlot& playback = soundPlaybacks[i];
        if (playback.active && playback.sound == sound
                && playback.sequence < oldestSequence) {
            oldest = i;
            oldestSequence = playback.sequence;
        }
    }
    if (oldest == InvalidSlot) return InvalidSlot;
    const size_t voiceIndex = soundPlaybacks[oldest].voiceIndex;
    DeactivateSoundSlot(assets, oldest, true);
    return voiceIndex;
}

void AudioSystem::ApplySoundMix(
        const Sound& voice,
        const SoundPlaybackSlot& playback) const
{
    float volume = playback.settings.volume;
    float pan = playback.settings.pan;
    if (playback.positional) {
        const AudioSpatialization spatial = ComputeAudioSpatialization(
                listener,
                playback.positionalSettings);
        volume *= spatial.volumeScale;
        pan = spatial.pan;
    }
    ::SetSoundVolume(voice, volume);
    ::SetSoundPitch(voice, playback.settings.pitch);
    ::SetSoundPan(voice, ToRaylibPan(pan));
}

void AudioSystem::DeactivateSoundSlot(
        AssetManager& assets,
        size_t slotIndex,
        bool stopVoice)
{
    if (slotIndex >= soundPlaybacks.size()) return;
    SoundPlaybackSlot& playback = soundPlaybacks[slotIndex];
    if (playback.active && stopVoice) {
        const Sound* voice = assets.GetSoundVoice(
                playback.sound,
                playback.voiceIndex);
        if (voice != nullptr) {
            if (playback.pausedBySystem) ::ResumeSound(*voice);
            ::StopSound(*voice);
        }
    }
    const uint32_t nextGeneration = playback.generation + 1;
    playback = {};
    playback.generation = nextGeneration == 0 ? 1 : nextGeneration;
}

bool AudioSystem::IsValidPlayback(SoundPlaybackHandle handle) const
{
    return handle.index < soundPlaybacks.size()
            && soundPlaybacks[handle.index].active
            && soundPlaybacks[handle.index].generation == handle.generation;
}

} // namespace engine
