#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/FontAssets.h"
#include "engine/assets/FontLoadFlags.h"
#include "engine/assets/SpriteAnimationAssets.h"
#include "engine/assets/TextureAssets.h"
#include "engine/assets/TextureLoadFlags.h"

#include <raylib.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace engine {

class AssetManager {
public:
    AssetManager();
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    bool Initialize();
    void Shutdown();

    AssetScopeHandle CreateScope(const char* name);
    AssetScopeHandle GlobalScope() const;

    void UnloadScope(AssetScopeHandle scope);

    TextureHandle RequestTexture(
            AssetScopeHandle scope,
            const char* key,
            const char* path,
            TextureLoadFlags flags = TextureLoad_PointFilter
    );

    TextureHandle CreateTextureFromImage(
            AssetScopeHandle scope,
            const char* key,
            const Image& image,
            TextureLoadFlags flags = TextureLoad_PointFilter
    );

    bool IsReady(TextureHandle handle) const;
    bool IsFinished(TextureHandle handle) const;
    bool HasFailed(TextureHandle handle) const;
    const Texture2D* GetTexture(TextureHandle handle) const;

    FontHandle RequestFont(
            AssetScopeHandle scope,
            const char* key,
            const char* path,
            int pixelSize,
            FontLoadFlags flags = FontLoad_BilinearFilter
    );

    bool IsReady(FontHandle handle) const;
    bool IsFinished(FontHandle handle) const;
    bool HasFailed(FontHandle handle) const;
    const FontAsset* GetFont(FontHandle handle) const;

    SpriteAnimationHandle RequestSpriteAnimation(
            AssetScopeHandle scope,
            const char* key,
            const char* jsonPath,
            TextureLoadFlags textureFlags = TextureLoad_PointFilter
    );

    bool IsReady(SpriteAnimationHandle handle) const;
    bool IsFinished(SpriteAnimationHandle handle) const;
    bool HasFailed(SpriteAnimationHandle handle) const;
    const SpriteAnimationAsset* GetSpriteAnimation(SpriteAnimationHandle handle) const;

    uint32_t FindSpriteClipIndex(SpriteAnimationHandle handle, const char* clipName) const;
    const SpriteClip* GetSpriteClip(SpriteAnimationHandle handle, uint32_t clipIndex) const;

    bool IsScopeReady(AssetScopeHandle scope) const;
    bool IsScopeFinished(AssetScopeHandle scope) const;
    float GetScopeProgress(AssetScopeHandle scope) const;

    void UpdateMainThread(float maxMilliseconds);

private:
    enum class AssetRequestType {
        Texture,
        SpriteAnimation
    };

    struct AssetRequest {
        AssetRequestType type = AssetRequestType::Texture;
        TextureHandle texture;
        SpriteAnimationHandle spriteAnimation;
        std::string path;
        TextureLoadFlags textureFlags = TextureLoad_PointFilter;
    };

    struct ScopeSlot {
        uint32_t generation = 1;
        bool alive = false;
        std::string name;
    };

    bool IsValidScopeNoLock(AssetScopeHandle scope) const;
    void WorkerLoop();
    void EnqueueRequest(AssetRequest request);

    mutable std::mutex stateMutex;
    std::vector<ScopeSlot> scopes;
    AssetScopeHandle globalScope;
    bool initialized = false;

    TextureAssets textures;
    FontAssets fonts;
    SpriteAnimationAssets spriteAnimations;

    std::thread worker;
    std::atomic<bool> shutdownRequested{false};
    std::mutex requestMutex;
    std::condition_variable requestCv;
    std::deque<AssetRequest> requests;
};

} // namespace engine
