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
};

struct AudioSpatialization {
    float volumeScale = 1.0f;
    float pan = 0.0f;
};

using PositionalSoundOcclusionQuery = float (*)(
        void* context,
        Vector3 listenerPosition,
        Vector3 sourcePosition);

AudioSpatialization ComputeAudioSpatialization(
        const AudioListener& listener,
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
    void UpdatePositionalSoundOcclusion(
            float dt,
            void* queryContext,
            PositionalSoundOcclusionQuery query);
    void Update(AssetManager& assets);

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
    bool StopSound(AssetManager& assets, SoundPlaybackHandle playback);
    void StopSoundAsset(AssetManager& assets, SoundHandle sound);
    bool IsSoundPlaying(SoundPlaybackHandle playback) const;

    bool PlayMusic(
            AssetManager& assets,
            MusicHandle music,
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
        SoundHandle sound = NullSoundHandle();
        size_t voiceIndex = 0;
        uint64_t sequence = 0;
        SoundPlaybackSettings settings;
        PositionalSoundSettings positionalSettings;
        float occlusionVolumeScale = 1.0f;
        float occlusionTargetScale = 1.0f;
        float occlusionQueryRemainingSeconds = 0.0f;
        bool occlusionInitialized = false;
    };

    struct MusicPlaybackSlot {
        bool active = false;
        bool pausedBySystem = false;
        MusicHandle music = NullMusicHandle();
        MusicPlaybackSettings settings;
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

    bool deviceReady = false;
    bool suspended = false;
    uint64_t nextSequence = 1;
    AudioListener listener;
    std::vector<SoundPlaybackSlot> soundPlaybacks;
    std::vector<MusicPlaybackSlot> musicPlaybacks;
};

} // namespace engine
