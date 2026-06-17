#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/TextureAssets.h"
#include "engine/assets/TextureLoadFlags.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

enum class SpritePlaybackMode {
    Once,
    Loop,
    PingPong,
    Reverse
};

struct SpriteFrame {
    Rectangle source = {};
    Vector2 sourceSize = {};
    Vector2 spriteSourcePosition = {};
    Vector2 spriteSourceSize = {};
    float durationSeconds = 0.1f;
};

struct SpriteClip {
    std::string name;
    uint32_t firstFrame = 0;
    uint32_t frameCount = 0;
    SpritePlaybackMode playback = SpritePlaybackMode::Loop;
    uint32_t repeat = 0;
};

struct SpriteAnimationAsset {
    TextureHandle atlasTexture;
    std::vector<SpriteFrame> frames;
    std::vector<SpriteClip> clips;
};

std::string ExtractClipNameFromFrameName(const std::string& frameName);

class SpriteAnimationAssets {
public:
    struct RequestResult {
        SpriteAnimationHandle handle;
        bool shouldQueueJson = false;
        std::string jsonPath;
    };

    void OnScopeCreated(AssetScopeHandle scope);

    RequestResult RequestSpriteAnimation(
            AssetScopeHandle scope,
            const char* key,
            const char* jsonPath,
            TextureLoadFlags textureFlags
    );

    bool IsReady(SpriteAnimationHandle handle, const TextureAssets& textureAssets) const;
    bool IsFinished(SpriteAnimationHandle handle, const TextureAssets& textureAssets) const;
    bool HasFailed(SpriteAnimationHandle handle, const TextureAssets& textureAssets) const;

    const SpriteAnimationAsset* Get(SpriteAnimationHandle handle, const TextureAssets& textureAssets) const;

    uint32_t FindClipIndex(SpriteAnimationHandle handle, const char* clipName) const;
    const SpriteClip* GetClip(SpriteAnimationHandle handle, uint32_t clipIndex) const;

    bool IsScopeReady(AssetScopeHandle scope, const TextureAssets& textureAssets) const;
    bool IsScopeFinished(AssetScopeHandle scope, const TextureAssets& textureAssets) const;
    void GetScopeProgress(
            AssetScopeHandle scope,
            const TextureAssets& textureAssets,
            size_t& finished,
            size_t& total
    ) const;

    TextureAssets::RequestResult ProcessSpriteAnimationRequestOnWorkerThread(
            SpriteAnimationHandle handle,
            const std::string& jsonPath,
            TextureAssets& textureAssets
    );

    void UnloadScope(AssetScopeHandle scope);
    void Shutdown();

private:
    enum class SpriteAnimationState {
        Unloaded,
        Queued,
        Loading,
        Ready,
        Failed,
        QueuedForUnload
    };

    struct SpriteAnimationSlot {
        uint32_t generation = 1;
        SpriteAnimationState state = SpriteAnimationState::Unloaded;
        std::string key;
        std::string jsonPath;
        TextureLoadFlags textureFlags = TextureLoad_PointFilter;
        AssetScopeHandle scope;
        SpriteAnimationAsset asset;
        std::string error;
    };

    struct SpriteAnimationScopeData {
        std::vector<SpriteAnimationHandle> animations;
        std::unordered_map<std::string, SpriteAnimationHandle> animationByRequest;
    };

    bool IsValidAnimationNoLock(SpriteAnimationHandle handle) const;
    static bool IsTerminal(SpriteAnimationState state);
    static std::string MakeRequestKey(
            const char* key,
            const char* jsonPath,
            TextureLoadFlags textureFlags
    );

    mutable std::mutex stateMutex;
    std::vector<SpriteAnimationSlot> animationSlots;
    std::vector<SpriteAnimationScopeData> scopeData;
};

} // namespace engine
