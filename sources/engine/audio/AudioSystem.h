#pragma once

#include "engine/assets/AssetManager.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace engine {

struct AudioListener {
    Vector3 position{};
    Vector3 forward{0.0f, 0.0f, 1.0f};
    Vector3 up{0.0f, 1.0f, 0.0f};
};

struct SoundPlaybackSettings {
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    bool looping = false;
    bool affectedByListenerEffects = true;
};

struct PositionalSoundSettings {
    Vector3 position{};
    float minimumDistanceWorld = 1.0f;
    float maximumDistanceWorld = 25.0f;
};

struct MusicPlaybackSettings {
    float volume = 1.0f;
    float pitch = 1.0f;
    float pan = 0.0f;
    bool looping = true;
    bool affectedByListenerEffects = true;
};

struct AudioSpatialization {
    float volumeScale = 1.0f;
    float pan = 0.0f;
};

inline constexpr float AudioUnfilteredLowPassCutoffHz = 20000.0f;
inline constexpr float AudioMinimumListenerLowPassCutoffHz = 250.0f;
inline constexpr float AudioListenerEffectTransitionSeconds = 0.20f;

float AudioListenerLowPassCutoffHz(float normalizedStrength);
float CombineAudioLowPassCutoffs(float firstHz, float secondHz);

struct PositionalSoundPropagation {
    Vector3 apparentPosition{};
    float distanceWorld = 0.0f;
    float volumeScale = 1.0f;
    float lowPassCutoffHz = AudioUnfilteredLowPassCutoffHz;
};

using PositionalSoundPropagationQuery = PositionalSoundPropagation (*)(
        void* context,
        Vector3 listenerPosition,
        const PositionalSoundSettings& source);

AudioSpatialization ComputeAudioSpatialization(
        const AudioListener& listener,
        const PositionalSoundSettings& source);
float ComputeAudioDistanceAttenuation(
        float distanceWorld,
        const PositionalSoundSettings& source);

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    bool Initialize(
            size_t soundPlaybackCapacity = 256,
            size_t musicPlaybackCapacity = 16);
    void Shutdown();
    bool IsReady() const { return deviceReady; }

    void SetListener(const AudioListener& value);
    const AudioListener& Listener() const { return listener; }
    void SetListenerLowPassStrength(float normalizedStrength);
    float ListenerLowPassStrength() const { return listenerLowPassStrength; }
    void UpdatePositionalSoundPropagation(
            float dt,
            void* queryContext,
            PositionalSoundPropagationQuery query);
    void Update(AssetManager& assets, float dt);

    SoundPlaybackHandle PlaySound(
            AssetManager& assets,
            SoundHandle sound,
            const SoundPlaybackSettings& settings = {});
    SoundPlaybackHandle PlaySoundAt(
            AssetManager& assets,
            SoundHandle sound,
            const PositionalSoundSettings& positional,
            const SoundPlaybackSettings& settings = {});
    bool SetSoundPosition(
            SoundPlaybackHandle playback,
            Vector3 position);
    bool SetSoundPlaybackSettings(
            AssetManager& assets,
            SoundPlaybackHandle playback,
            const SoundPlaybackSettings& settings);
    bool SetSoundPlaybackPaused(
            AssetManager& assets,
            SoundPlaybackHandle playback,
            bool paused);
    bool StopSound(AssetManager& assets, SoundPlaybackHandle playback);
    void StopSoundAsset(AssetManager& assets, SoundHandle sound);
    bool IsSoundPlaying(SoundPlaybackHandle playback) const;

    bool PlayMusic(
            AssetManager& assets,
            MusicHandle music,
            const MusicPlaybackSettings& settings = {});
    bool PlayMusicAt(
            AssetManager& assets,
            MusicHandle music,
            const PositionalSoundSettings& positional,
            const MusicPlaybackSettings& settings = {});
    bool StopMusic(AssetManager& assets, MusicHandle music);
    bool IsMusicPlaying(MusicHandle music) const;

    void PauseAll(AssetManager& assets);
    void ResumeAll(AssetManager& assets);
    void StopAll(AssetManager& assets);
    bool IsPaused() const { return suspended; }

private:
    struct SoundPlaybackSlot {
        uint32_t generation = 1;
        bool active = false;
        bool positional = false;
        bool pausedBySystem = false;
        bool pausedByCaller = false;
        SoundHandle sound = NullSoundHandle();
        size_t voiceIndex = 0;
        uint64_t sequence = 0;
        SoundPlaybackSettings settings;
        PositionalSoundSettings positionalSettings;
        PositionalSoundPropagation propagation;
        PositionalSoundPropagation propagationTarget;
        float propagationQueryRemainingSeconds = 0.0f;
        bool propagationInitialized = false;
    };

    struct MusicPlaybackSlot {
        bool active = false;
        bool positional = false;
        bool pausedBySystem = false;
        MusicHandle music = NullMusicHandle();
        MusicPlaybackSettings settings;
        PositionalSoundSettings positionalSettings;
        PositionalSoundPropagation propagation;
        PositionalSoundPropagation propagationTarget;
        float propagationQueryRemainingSeconds = 0.0f;
        bool propagationInitialized = false;
    };

    SoundPlaybackHandle PlaySoundInternal(
            AssetManager& assets,
            SoundHandle sound,
            const SoundPlaybackSettings& settings,
            const PositionalSoundSettings* positional);
    size_t FindSoundPlaybackSlot(AssetManager& assets, SoundHandle sound);
    size_t FindVoiceIndex(AssetManager& assets, SoundHandle sound);
    void ApplySoundMix(
            const Sound& voice,
            const SoundPlaybackSlot& playback) const;
    void DeactivateSoundSlot(
            AssetManager& assets,
            size_t slotIndex,
            bool stopVoice);
    bool IsValidPlayback(SoundPlaybackHandle handle) const;
    bool PlayMusicInternal(
            AssetManager& assets,
            MusicHandle music,
            const MusicPlaybackSettings& settings,
            const PositionalSoundSettings* positional);
    void ApplyMusicMix(
            Music& stream,
            const MusicPlaybackSlot& playback) const;

    bool deviceReady = false;
    bool suspended = false;
    uint64_t nextSequence = 1;
    AudioListener listener;
    float listenerLowPassStrength = 0.0f;
    float listenerLowPassTargetStrength = 0.0f;
    std::vector<SoundPlaybackSlot> soundPlaybacks;
    std::vector<MusicPlaybackSlot> musicPlaybacks;
};

} // namespace engine
