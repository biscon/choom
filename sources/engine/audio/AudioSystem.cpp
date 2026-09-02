#include "engine/audio/AudioSystem.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace engine {

namespace {

constexpr size_t InvalidSlot = std::numeric_limits<size_t>::max();
constexpr float PositionalFadeStart = 0.8f;
constexpr float PositionalPropagationQueryIntervalSeconds = 0.1f;
constexpr float PositionalPropagationBlendSeconds = 0.5f;
constexpr float MinimumLowPassCutoffHz = 20.0f;

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

float SmoothStep01(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float MoveTowards(float current, float target, float maximumDelta)
{
    if (current < target) return std::min(current + maximumDelta, target);
    return std::max(current - maximumDelta, target);
}

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

PositionalSoundPropagation NormalizePropagation(
        PositionalSoundPropagation propagation,
        const AudioListener& listener,
        const PositionalSoundSettings& source)
{
    if (!IsFinite(propagation.apparentPosition)) {
        propagation.apparentPosition = source.position;
    }
    if (!std::isfinite(propagation.distanceWorld)
            || propagation.distanceWorld < 0.0f) {
        propagation.distanceWorld = Vector3Distance(
                listener.position, source.position);
    }
    propagation.volumeScale = std::clamp(
            FiniteOr(propagation.volumeScale, 1.0f), 0.0f, 1.0f);
    propagation.lowPassCutoffHz = std::clamp(
            FiniteOr(
                    propagation.lowPassCutoffHz,
                    AudioUnfilteredLowPassCutoffHz),
            MinimumLowPassCutoffHz,
            AudioUnfilteredLowPassCutoffHz);
    return propagation;
}

void AdvancePropagation(
        PositionalSoundPropagation& current,
        const PositionalSoundPropagation& target,
        float dt)
{
    const float blend = PositionalPropagationBlendSeconds > 0.0f
            ? std::clamp(dt / PositionalPropagationBlendSeconds, 0.0f, 1.0f)
            : 1.0f;
    current.volumeScale = MoveTowards(
            current.volumeScale,
            target.volumeScale,
            blend);
    current.distanceWorld +=
            (target.distanceWorld - current.distanceWorld) * blend;
    current.apparentPosition = Vector3Lerp(
            current.apparentPosition,
            target.apparentPosition,
            blend);
    const float currentLog = std::log(std::max(
            current.lowPassCutoffHz, MinimumLowPassCutoffHz));
    const float targetLog = std::log(std::max(
            target.lowPassCutoffHz, MinimumLowPassCutoffHz));
    current.lowPassCutoffHz = std::exp(
            currentLog + (targetLog - currentLog) * blend);
}

template<typename Playback>
void UpdatePlaybackPropagation(
        Playback& playback,
        const AudioListener& listener,
        float dt,
        void* queryContext,
        PositionalSoundPropagationQuery query)
{
    playback.propagationQueryRemainingSeconds -= dt;
    if (!playback.propagationInitialized
            || playback.propagationQueryRemainingSeconds <= 0.0f) {
        PositionalSoundPropagation queried;
        queried.apparentPosition = playback.positionalSettings.position;
        queried.distanceWorld = Vector3Distance(
                listener.position,
                playback.positionalSettings.position);
        if (query != nullptr) {
            queried = query(
                    queryContext,
                    listener.position,
                    playback.positionalSettings);
        }
        playback.propagationTarget = NormalizePropagation(
                queried, listener, playback.positionalSettings);
        playback.propagationQueryRemainingSeconds =
                PositionalPropagationQueryIntervalSeconds;
        if (!playback.propagationInitialized) {
            playback.propagation = playback.propagationTarget;
            playback.propagationInitialized = true;
        }
    }
    AdvancePropagation(
            playback.propagation,
            playback.propagationTarget,
            dt);
}

} // namespace

float AudioListenerLowPassCutoffHz(float normalizedStrength)
{
    const float strength = std::clamp(
            FiniteOr(normalizedStrength, 0.0f), 0.0f, 1.0f);
    if (strength <= 0.0f) return AudioUnfilteredLowPassCutoffHz;
    if (strength >= 1.0f) return AudioMinimumListenerLowPassCutoffHz;
    const float dryLog = std::log(AudioUnfilteredLowPassCutoffHz);
    const float wetLog = std::log(AudioMinimumListenerLowPassCutoffHz);
    return std::exp(dryLog + (wetLog - dryLog) * strength);
}

float CombineAudioLowPassCutoffs(float firstHz, float secondHz)
{
    return std::clamp(
            std::min(
                    FiniteOr(firstHz, AudioUnfilteredLowPassCutoffHz),
                    FiniteOr(secondHz, AudioUnfilteredLowPassCutoffHz)),
            MinimumLowPassCutoffHz,
            AudioUnfilteredLowPassCutoffHz);
}

float ComputeAudioDistanceAttenuation(
        float rawDistanceWorld,
        const PositionalSoundSettings& unnormalizedSource)
{
    const PositionalSoundSettings source = NormalizePositional(
            unnormalizedSource);
    const float distance = std::max(
            0.0f, FiniteOr(rawDistanceWorld, 0.0f));
    if (distance <= source.minimumDistanceWorld) return 1.0f;
    if (distance >= source.maximumDistanceWorld) return 0.0f;
    const float distancePastMinimum =
            distance - source.minimumDistanceWorld;
    const float referenceDistance = std::max(
            1.0f, source.minimumDistanceWorld);
    const float inverseDistanceGain = referenceDistance
            / (referenceDistance + distancePastMinimum);
    const float rangeT = distancePastMinimum
            / (source.maximumDistanceWorld
                    - source.minimumDistanceWorld);
    const float fadeT = (rangeT - PositionalFadeStart)
            / (1.0f - PositionalFadeStart);
    const float cutoffGain = 1.0f - SmoothStep01(fadeT);
    return inverseDistanceGain * cutoffGain;
}

AudioSpatialization ComputeAudioSpatialization(
        const AudioListener& listener,
        const PositionalSoundSettings& unnormalizedSource)
{
    const PositionalSoundSettings source = NormalizePositional(
            unnormalizedSource);
    const Vector3 offset = Vector3Subtract(source.position, listener.position);
    const float distance = Vector3Length(offset);

    AudioSpatialization result;
    result.volumeScale = ComputeAudioDistanceAttenuation(distance, source);

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
    listenerLowPassStrength = 0.0f;
    listenerLowPassTargetStrength = 0.0f;
    if (deviceReady || IsAudioDeviceReady()) {
        CloseAudioDevice();
    }
    deviceReady = false;
}

void AudioSystem::SetListener(const AudioListener& value)
{
    listener = value;
}

void AudioSystem::SetListenerLowPassStrength(float normalizedStrength)
{
    listenerLowPassTargetStrength = std::clamp(
            FiniteOr(normalizedStrength, 0.0f), 0.0f, 1.0f);
}

void AudioSystem::UpdatePositionalSoundPropagation(
        float rawDt,
        void* queryContext,
        PositionalSoundPropagationQuery query)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    for (SoundPlaybackSlot& playback : soundPlaybacks) {
        if (!playback.active || !playback.positional) continue;
        UpdatePlaybackPropagation(
                playback, listener, dt, queryContext, query);
    }
    for (MusicPlaybackSlot& playback : musicPlaybacks) {
        if (!playback.active || !playback.positional) continue;
        UpdatePlaybackPropagation(
                playback, listener, dt, queryContext, query);
    }
}

void AudioSystem::Update(AssetManager& assets, float rawDt)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    const float maximumChange = AudioListenerEffectTransitionSeconds > 0.0f
            ? dt / AudioListenerEffectTransitionSeconds : 1.0f;
    listenerLowPassStrength = MoveTowards(
            listenerLowPassStrength,
            listenerLowPassTargetStrength,
            maximumChange);
    if (!deviceReady || suspended) return;

    for (size_t i = 0; i < soundPlaybacks.size(); ++i) {
        SoundPlaybackSlot& playback = soundPlaybacks[i];
        if (!playback.active) continue;
        const Sound* voice = assets.GetSoundVoice(
                playback.sound,
                playback.voiceIndex);
        if (playback.pausedByCaller) continue;
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
        ApplyMusicMix(stream, playback);
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

bool AudioSystem::SetSoundPlaybackSettings(
        AssetManager& assets,
        SoundPlaybackHandle playback,
        const SoundPlaybackSettings& settings)
{
    if (!IsValidPlayback(playback)) return false;
    SoundPlaybackSlot& slot = soundPlaybacks[playback.index];
    slot.settings = NormalizeSettings(settings);
    const Sound* voice = assets.GetSoundVoice(slot.sound, slot.voiceIndex);
    if (voice == nullptr) return false;
    ApplySoundMix(*voice, slot);
    return true;
}

bool AudioSystem::SetSoundPlaybackPaused(
        AssetManager& assets,
        SoundPlaybackHandle playback,
        bool paused)
{
    if (!IsValidPlayback(playback)) return false;
    SoundPlaybackSlot& slot = soundPlaybacks[playback.index];
    const Sound* voice = assets.GetSoundVoice(slot.sound, slot.voiceIndex);
    if (voice == nullptr) return false;
    if (slot.pausedByCaller == paused) return true;
    slot.pausedByCaller = paused;
    if (paused) {
        if (!slot.pausedBySystem && ::IsSoundPlaying(*voice)) {
            ::PauseSound(*voice);
        }
    } else if (!slot.pausedBySystem && !suspended) {
        ApplySoundMix(*voice, slot);
        ::ResumeSound(*voice);
    }
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
    return PlayMusicInternal(assets, music, unnormalizedSettings, nullptr);
}

bool AudioSystem::PlayMusicAt(
        AssetManager& assets,
        MusicHandle music,
        const PositionalSoundSettings& positional,
        const MusicPlaybackSettings& unnormalizedSettings)
{
    return PlayMusicInternal(assets, music, unnormalizedSettings, &positional);
}

bool AudioSystem::PlayMusicInternal(
        AssetManager& assets,
        MusicHandle music,
        const MusicPlaybackSettings& unnormalizedSettings,
        const PositionalSoundSettings* positional)
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
            const bool wasPositional = playback.positional;
            playback.positional = positional != nullptr;
            if (positional != nullptr) {
                playback.positionalSettings = NormalizePositional(*positional);
                if (!wasPositional) {
                    playback.propagation = {};
                    playback.propagationTarget = {};
                    playback.propagationQueryRemainingSeconds = 0.0f;
                    playback.propagationInitialized = false;
                }
            }
            Music stream = asset->stream;
            ApplyMusicMix(stream, playback);
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
    freeSlot->active = true;
    freeSlot->positional = positional != nullptr;
    freeSlot->music = music;
    freeSlot->settings = settings;
    if (positional != nullptr) {
        freeSlot->positionalSettings = NormalizePositional(*positional);
    }
    ApplyMusicMix(stream, *freeSlot);
    ::PlayMusicStream(stream);
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
        if (playback.pausedByCaller) continue;
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
    playback.pausedByCaller = false;
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
        if (playback.active && !suspended && !playback.pausedBySystem
                && !playback.pausedByCaller) {
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
        PositionalSoundSettings apparent = playback.positionalSettings;
        apparent.position = playback.propagationInitialized
                ? playback.propagation.apparentPosition
                : playback.positionalSettings.position;
        const AudioSpatialization spatial = ComputeAudioSpatialization(
                listener, apparent);
        const float distance = playback.propagationInitialized
                ? playback.propagation.distanceWorld
                : Vector3Distance(
                        listener.position,
                        playback.positionalSettings.position);
        const float propagationScale = playback.propagationInitialized
                ? playback.propagation.volumeScale : 1.0f;
        volume *= ComputeAudioDistanceAttenuation(
                distance, playback.positionalSettings) * propagationScale;
        pan = spatial.pan;
    }
    ::SetSoundVolume(voice, volume);
    ::SetSoundPitch(voice, playback.settings.pitch);
    ::SetSoundPan(voice, ToRaylibPan(pan));
    ::SetSoundLooping(voice, playback.settings.looping);
    const float propagationCutoff = playback.positional
                    && playback.propagationInitialized
            ? playback.propagation.lowPassCutoffHz
            : AudioUnfilteredLowPassCutoffHz;
    const float listenerCutoff = playback.settings.affectedByListenerEffects
            ? AudioListenerLowPassCutoffHz(listenerLowPassStrength)
            : AudioUnfilteredLowPassCutoffHz;
    ::SetAudioStreamLowPassFilter(
            voice.stream,
            CombineAudioLowPassCutoffs(propagationCutoff, listenerCutoff));
}

void AudioSystem::ApplyMusicMix(
        Music& stream,
        const MusicPlaybackSlot& playback) const
{
    float volume = playback.settings.volume;
    float pan = playback.settings.pan;
    if (playback.positional) {
        PositionalSoundSettings apparent = playback.positionalSettings;
        apparent.position = playback.propagationInitialized
                ? playback.propagation.apparentPosition
                : playback.positionalSettings.position;
        const AudioSpatialization spatial = ComputeAudioSpatialization(
                listener, apparent);
        const float distance = playback.propagationInitialized
                ? playback.propagation.distanceWorld
                : Vector3Distance(
                        listener.position,
                        playback.positionalSettings.position);
        const float propagationScale = playback.propagationInitialized
                ? playback.propagation.volumeScale : 1.0f;
        volume *= ComputeAudioDistanceAttenuation(
                distance, playback.positionalSettings) * propagationScale;
        pan = spatial.pan;
    }
    stream.looping = playback.settings.looping;
    ::SetMusicVolume(stream, volume);
    ::SetMusicPitch(stream, playback.settings.pitch);
    ::SetMusicPan(stream, ToRaylibPan(pan));
    const float propagationCutoff = playback.positional
                    && playback.propagationInitialized
            ? playback.propagation.lowPassCutoffHz
            : AudioUnfilteredLowPassCutoffHz;
    const float listenerCutoff = playback.settings.affectedByListenerEffects
            ? AudioListenerLowPassCutoffHz(listenerLowPassStrength)
            : AudioUnfilteredLowPassCutoffHz;
    ::SetAudioStreamLowPassFilter(
            stream.stream,
            CombineAudioLowPassCutoffs(propagationCutoff, listenerCutoff));
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
            ::SetSoundLooping(*voice, false);
            if (playback.pausedBySystem || playback.pausedByCaller) {
                ::ResumeSound(*voice);
            }
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
