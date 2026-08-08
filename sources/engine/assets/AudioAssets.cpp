#include "engine/assets/AudioAssets.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <utility>

namespace engine {

namespace {

template<typename Handle, typename Slot>
bool IsHandleValid(const std::vector<Slot>& slots, Handle handle)
{
    return handle.index < slots.size()
            && slots[handle.index].generation == handle.generation;
}

std::string NormalizeAudioPath(const char* path)
{
    if (path == nullptr || path[0] == '\0') {
        return {};
    }
    return std::filesystem::path(path).lexically_normal().generic_string();
}

} // namespace

void SoundAssets::OnScopeCreated(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        scopeData.resize(static_cast<size_t>(scope.index) + 1);
    }
}

SoundHandle SoundAssets::RequestSound(AssetScopeHandle scope, const char* path)
{
    const std::string normalizedPath = NormalizePath(path);
    if (normalizedPath.empty()) {
        return NullSoundHandle();
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return NullSoundHandle();
    }

    ScopeData& owner = scopeData[scope.index];
    const auto owned = owner.soundByPath.find(normalizedPath);
    if (owned != owner.soundByPath.end()) {
        return owned->second;
    }

    SoundHandle handle = NullSoundHandle();
    const auto shared = soundByPath.find(normalizedPath);
    if (shared != soundByPath.end() && IsValidNoLock(shared->second)) {
        handle = shared->second;
        ++slots[handle.index].ownerCount;
    } else {
        assert(slots.size() < std::numeric_limits<uint32_t>::max());
        Slot slot;
        slot.ownerCount = 1;
        slot.path = normalizedPath;
        const uint32_t index = static_cast<uint32_t>(slots.size());
        slots.push_back(std::move(slot));
        handle = SoundHandle{index, slots[index].generation};
        soundByPath[normalizedPath] = handle;
        pendingLoads.push_back(handle);
    }

    owner.sounds.push_back(handle);
    owner.soundByPath.emplace(normalizedPath, handle);
    return handle;
}

bool SoundAssets::IsReady(SoundHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return IsValidNoLock(handle) && slots[handle.index].state == State::Ready;
}

bool SoundAssets::IsFinished(SoundHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidNoLock(handle) || IsTerminal(slots[handle.index].state);
}

bool SoundAssets::HasFailed(SoundHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidNoLock(handle) || slots[handle.index].state == State::Failed;
}

const SoundAsset* SoundAssets::Get(SoundHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidNoLock(handle) || slots[handle.index].state != State::Ready) {
        return nullptr;
    }
    return &slots[handle.index].asset;
}

const Sound* SoundAssets::GetVoice(SoundHandle handle, size_t voiceIndex) const
{
    const SoundAsset* asset = Get(handle);
    if (asset == nullptr || voiceIndex >= asset->voiceCount) {
        return nullptr;
    }
    return voiceIndex == 0 ? &asset->source : &asset->aliases[voiceIndex - 1];
}

bool SoundAssets::IsScopeReady(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return false;
    for (SoundHandle handle : scopeData[scope.index].sounds) {
        if (!IsValidNoLock(handle) || slots[handle.index].state != State::Ready) {
            return false;
        }
    }
    return true;
}

bool SoundAssets::IsScopeFinished(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return false;
    for (SoundHandle handle : scopeData[scope.index].sounds) {
        if (IsValidNoLock(handle) && !IsTerminal(slots[handle.index].state)) {
            return false;
        }
    }
    return true;
}

void SoundAssets::GetScopeProgress(
        AssetScopeHandle scope,
        size_t& finished,
        size_t& total) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return;
    total += scopeData[scope.index].sounds.size();
    for (SoundHandle handle : scopeData[scope.index].sounds) {
        if (!IsValidNoLock(handle) || IsTerminal(slots[handle.index].state)) {
            ++finished;
        }
    }
}

void SoundAssets::UpdateMainThread(float maxMilliseconds)
{
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        SoundHandle handle;
        std::string path;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (pendingLoads.empty()) break;
            handle = pendingLoads.front();
            pendingLoads.pop_front();
            if (!IsValidNoLock(handle)
                    || slots[handle.index].state != State::Queued) {
                continue;
            }
            path = slots[handle.index].path;
        }

        SoundAsset loaded;
        loaded.path = path;
        loaded.source = LoadSound(path.c_str());
        if (IsSoundValid(loaded.source)) {
            loaded.voiceCount = 1;
            for (size_t i = 0; i < loaded.aliases.size(); ++i) {
                Sound alias = LoadSoundAlias(loaded.source);
                if (!IsSoundValid(alias)) break;
                loaded.aliases[i] = alias;
                ++loaded.voiceCount;
            }
        }
        const bool success = IsSoundValid(loaded.source)
                && loaded.voiceCount > 0;
        if (!success) {
            std::fprintf(stderr,
                    "[AssetManager WARNING] Failed to load sound: %s\n",
                    path.c_str());
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (IsValidNoLock(handle)
                    && slots[handle.index].state == State::Queued) {
                Slot& slot = slots[handle.index];
                if (success) {
                    slot.asset = std::move(loaded);
                    slot.state = State::Ready;
                } else {
                    slot.state = State::Failed;
                }
            } else if (success) {
                UnloadAsset(loaded);
            }
        }

        if (maxMilliseconds > 0.0f) {
            const float elapsed = std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
            if (elapsed >= maxMilliseconds) break;
        }
    }
}

void SoundAssets::UnloadScope(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return;
    ScopeData& owner = scopeData[scope.index];
    for (SoundHandle handle : owner.sounds) ReleaseNoLock(handle);
    owner.sounds.clear();
    owner.soundByPath.clear();
}

void SoundAssets::ShutdownMainThread()
{
    std::lock_guard<std::mutex> lock(stateMutex);
    pendingLoads.clear();
    for (Slot& slot : slots) {
        if (slot.state == State::Ready) UnloadAsset(slot.asset);
        slot.state = State::Unloaded;
        ++slot.generation;
    }
    slots.clear();
    scopeData.clear();
    soundByPath.clear();
}

bool SoundAssets::IsValidNoLock(SoundHandle handle) const
{
    return IsHandleValid(slots, handle);
}

bool SoundAssets::IsTerminal(State state)
{
    return state == State::Ready || state == State::Failed
            || state == State::Unloaded;
}

std::string SoundAssets::NormalizePath(const char* path)
{
    return NormalizeAudioPath(path);
}

void SoundAssets::UnloadAsset(SoundAsset& asset)
{
    for (size_t voiceIndex = asset.voiceCount; voiceIndex > 1; --voiceIndex) {
        Sound& alias = asset.aliases[voiceIndex - 2];
        if (IsSoundValid(alias)) UnloadSoundAlias(alias);
        alias = {};
    }
    if (IsSoundValid(asset.source)) UnloadSound(asset.source);
    asset = {};
}

void SoundAssets::ReleaseNoLock(SoundHandle handle)
{
    if (!IsValidNoLock(handle)) return;
    Slot& slot = slots[handle.index];
    if (slot.ownerCount > 0) --slot.ownerCount;
    if (slot.ownerCount != 0) return;
    soundByPath.erase(slot.path);
    if (slot.state == State::Ready) UnloadAsset(slot.asset);
    slot.state = State::Unloaded;
    ++slot.generation;
}

void MusicAssets::OnScopeCreated(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        scopeData.resize(static_cast<size_t>(scope.index) + 1);
    }
}

MusicHandle MusicAssets::RequestMusic(AssetScopeHandle scope, const char* path)
{
    const std::string normalizedPath = NormalizePath(path);
    if (normalizedPath.empty()) return NullMusicHandle();

    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return NullMusicHandle();
    ScopeData& owner = scopeData[scope.index];
    const auto owned = owner.musicByPath.find(normalizedPath);
    if (owned != owner.musicByPath.end()) return owned->second;

    MusicHandle handle = NullMusicHandle();
    const auto shared = musicByPath.find(normalizedPath);
    if (shared != musicByPath.end() && IsValidNoLock(shared->second)) {
        handle = shared->second;
        ++slots[handle.index].ownerCount;
    } else {
        assert(slots.size() < std::numeric_limits<uint32_t>::max());
        Slot slot;
        slot.ownerCount = 1;
        slot.path = normalizedPath;
        const uint32_t index = static_cast<uint32_t>(slots.size());
        slots.push_back(std::move(slot));
        handle = MusicHandle{index, slots[index].generation};
        musicByPath[normalizedPath] = handle;
        pendingLoads.push_back(handle);
    }
    owner.music.push_back(handle);
    owner.musicByPath.emplace(normalizedPath, handle);
    return handle;
}

bool MusicAssets::IsReady(MusicHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return IsValidNoLock(handle) && slots[handle.index].state == State::Ready;
}

bool MusicAssets::IsFinished(MusicHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidNoLock(handle) || IsTerminal(slots[handle.index].state);
}

bool MusicAssets::HasFailed(MusicHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidNoLock(handle) || slots[handle.index].state == State::Failed;
}

const MusicAsset* MusicAssets::Get(MusicHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidNoLock(handle) || slots[handle.index].state != State::Ready) {
        return nullptr;
    }
    return &slots[handle.index].asset;
}

bool MusicAssets::IsScopeReady(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return false;
    for (MusicHandle handle : scopeData[scope.index].music) {
        if (!IsValidNoLock(handle) || slots[handle.index].state != State::Ready) {
            return false;
        }
    }
    return true;
}

bool MusicAssets::IsScopeFinished(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return false;
    for (MusicHandle handle : scopeData[scope.index].music) {
        if (IsValidNoLock(handle) && !IsTerminal(slots[handle.index].state)) {
            return false;
        }
    }
    return true;
}

void MusicAssets::GetScopeProgress(
        AssetScopeHandle scope,
        size_t& finished,
        size_t& total) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return;
    total += scopeData[scope.index].music.size();
    for (MusicHandle handle : scopeData[scope.index].music) {
        if (!IsValidNoLock(handle) || IsTerminal(slots[handle.index].state)) {
            ++finished;
        }
    }
}

void MusicAssets::UpdateMainThread(float maxMilliseconds)
{
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        MusicHandle handle;
        std::string path;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (pendingLoads.empty()) break;
            handle = pendingLoads.front();
            pendingLoads.pop_front();
            if (!IsValidNoLock(handle)
                    || slots[handle.index].state != State::Queued) {
                continue;
            }
            path = slots[handle.index].path;
        }

        MusicAsset loaded;
        loaded.path = path;
        loaded.stream = LoadMusicStream(path.c_str());
        const bool success = IsMusicValid(loaded.stream);
        if (!success) {
            std::fprintf(stderr,
                    "[AssetManager WARNING] Failed to load music: %s\n",
                    path.c_str());
        }
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (IsValidNoLock(handle)
                    && slots[handle.index].state == State::Queued) {
                Slot& slot = slots[handle.index];
                if (success) {
                    slot.asset = std::move(loaded);
                    slot.state = State::Ready;
                } else {
                    slot.state = State::Failed;
                }
            } else if (success) {
                UnloadAsset(loaded);
            }
        }

        if (maxMilliseconds > 0.0f) {
            const float elapsed = std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
            if (elapsed >= maxMilliseconds) break;
        }
    }
}

void MusicAssets::UnloadScope(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) return;
    ScopeData& owner = scopeData[scope.index];
    for (MusicHandle handle : owner.music) ReleaseNoLock(handle);
    owner.music.clear();
    owner.musicByPath.clear();
}

void MusicAssets::ShutdownMainThread()
{
    std::lock_guard<std::mutex> lock(stateMutex);
    pendingLoads.clear();
    for (Slot& slot : slots) {
        if (slot.state == State::Ready) UnloadAsset(slot.asset);
        slot.state = State::Unloaded;
        ++slot.generation;
    }
    slots.clear();
    scopeData.clear();
    musicByPath.clear();
}

bool MusicAssets::IsValidNoLock(MusicHandle handle) const
{
    return IsHandleValid(slots, handle);
}

bool MusicAssets::IsTerminal(State state)
{
    return state == State::Ready || state == State::Failed
            || state == State::Unloaded;
}

std::string MusicAssets::NormalizePath(const char* path)
{
    return NormalizeAudioPath(path);
}

void MusicAssets::UnloadAsset(MusicAsset& asset)
{
    if (IsMusicValid(asset.stream)) UnloadMusicStream(asset.stream);
    asset = {};
}

void MusicAssets::ReleaseNoLock(MusicHandle handle)
{
    if (!IsValidNoLock(handle)) return;
    Slot& slot = slots[handle.index];
    if (slot.ownerCount > 0) --slot.ownerCount;
    if (slot.ownerCount != 0) return;
    musicByPath.erase(slot.path);
    if (slot.state == State::Ready) UnloadAsset(slot.asset);
    slot.state = State::Unloaded;
    ++slot.generation;
}

} // namespace engine
