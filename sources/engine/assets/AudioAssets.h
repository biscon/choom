#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

constexpr size_t SoundAssetVoiceCount = 8;

struct SoundAsset {
    Sound source{};
    std::array<Sound, SoundAssetVoiceCount - 1> aliases{};
    size_t voiceCount = 0;
    std::string path;
};

struct MusicAsset {
    Music stream{};
    std::string path;
};

class SoundAssets {
public:
    void OnScopeCreated(AssetScopeHandle scope);
    SoundHandle RequestSound(AssetScopeHandle scope, const char* path);

    bool IsReady(SoundHandle handle) const;
    bool IsFinished(SoundHandle handle) const;
    bool HasFailed(SoundHandle handle) const;
    const SoundAsset* Get(SoundHandle handle) const;
    const Sound* GetVoice(SoundHandle handle, size_t voiceIndex) const;

    bool IsScopeReady(AssetScopeHandle scope) const;
    bool IsScopeFinished(AssetScopeHandle scope) const;
    void GetScopeProgress(AssetScopeHandle scope, size_t& finished, size_t& total) const;

    void UpdateMainThread(float maxMilliseconds);
    void UnloadScope(AssetScopeHandle scope);
    void ShutdownMainThread();

private:
    enum class State { Queued, Ready, Failed, Unloaded };

    struct Slot {
        uint32_t generation = 1;
        State state = State::Queued;
        uint32_t ownerCount = 0;
        std::string path;
        SoundAsset asset;
    };

    struct ScopeData {
        std::vector<SoundHandle> sounds;
        std::unordered_map<std::string, SoundHandle> soundByPath;
    };

    bool IsValidNoLock(SoundHandle handle) const;
    static bool IsTerminal(State state);
    static std::string NormalizePath(const char* path);
    static void UnloadAsset(SoundAsset& asset);
    void ReleaseNoLock(SoundHandle handle);

    mutable std::mutex stateMutex;
    std::vector<Slot> slots;
    std::vector<ScopeData> scopeData;
    std::unordered_map<std::string, SoundHandle> soundByPath;
    std::deque<SoundHandle> pendingLoads;
};

class MusicAssets {
public:
    void OnScopeCreated(AssetScopeHandle scope);
    MusicHandle RequestMusic(AssetScopeHandle scope, const char* path);
    MusicHandle RequestMusicInstance(
            AssetScopeHandle scope,
            const char* instanceKey,
            const char* path);

    bool IsReady(MusicHandle handle) const;
    bool IsFinished(MusicHandle handle) const;
    bool HasFailed(MusicHandle handle) const;
    const MusicAsset* Get(MusicHandle handle) const;

    bool IsScopeReady(AssetScopeHandle scope) const;
    bool IsScopeFinished(AssetScopeHandle scope) const;
    void GetScopeProgress(AssetScopeHandle scope, size_t& finished, size_t& total) const;

    void UpdateMainThread(float maxMilliseconds);
    void UnloadScope(AssetScopeHandle scope);
    void ShutdownMainThread();

private:
    enum class State { Queued, Ready, Failed, Unloaded };

    struct Slot {
        uint32_t generation = 1;
        State state = State::Queued;
        uint32_t ownerCount = 0;
        bool sharedByPath = false;
        std::string path;
        MusicAsset asset;
    };

    struct ScopeData {
        std::vector<MusicHandle> music;
        std::unordered_map<std::string, MusicHandle> musicByPath;
        std::unordered_map<std::string, MusicHandle> musicInstancesByKey;
    };

    bool IsValidNoLock(MusicHandle handle) const;
    static bool IsTerminal(State state);
    static std::string NormalizePath(const char* path);
    static void UnloadAsset(MusicAsset& asset);
    void ReleaseNoLock(MusicHandle handle);

    mutable std::mutex stateMutex;
    std::vector<Slot> slots;
    std::vector<ScopeData> scopeData;
    std::unordered_map<std::string, MusicHandle> musicByPath;
    std::deque<MusicHandle> pendingLoads;
};

} // namespace engine
