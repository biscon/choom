#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/AudioAssets.h"
#include "engine/assets/FontAssets.h"
#include "engine/assets/FontLoadFlags.h"
#include "engine/assets/ModelAssets.h"
#include "engine/assets/SpriteAnimationAssets.h"
#include "engine/assets/TextureAssets.h"
#include "engine/assets/TextureColorUsage.h"
#include "engine/assets/TextureLoadFlags.h"

#include <raylib.h>

#include <atomic>
#include <cstddef>
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
            TextureColorUsage colorUsage,
            TextureLoadFlags flags = TextureLoad_PointFilter
    );

    TextureHandle CreateTextureFromImage(
            AssetScopeHandle scope,
            const char* key,
            const Image& image,
            TextureColorUsage colorUsage,
            TextureLoadFlags flags = TextureLoad_PointFilter
    );

    TextureHandle CreateCubemapFromImage(
            AssetScopeHandle scope,
            const char* key,
            const Image& image,
            TextureColorUsage colorUsage,
            int layout
    );

    bool IsReady(TextureHandle handle) const;
    bool IsFinished(TextureHandle handle) const;
    bool HasFailed(TextureHandle handle) const;
    const Texture2D* GetTexture(TextureHandle handle) const;
    const TextureCubemap* GetCubemap(TextureHandle handle) const;

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

    ModelHandle RequestModel(
            AssetScopeHandle scope,
            const char* key,
            const char* path,
            ModelLoadFlags flags = ModelLoad_None);

    bool IsReady(ModelHandle handle) const;
    bool IsFinished(ModelHandle handle) const;
    bool HasFailed(ModelHandle handle) const;
    const Model* GetModel(ModelHandle handle) const;
    const ModelAsset* GetModelAsset(ModelHandle handle) const;
    // Returns a borrowed ready asset from any live scope. The handle does not
    // acquire ownership; callers must consume it immediately on the main thread.
    ModelHandle FindReadyModelByPath(const char* path) const;

    SoundHandle RequestSound(AssetScopeHandle scope, const char* path);
    bool IsReady(SoundHandle handle) const;
    bool IsFinished(SoundHandle handle) const;
    bool HasFailed(SoundHandle handle) const;
    const SoundAsset* GetSound(SoundHandle handle) const;
    const Sound* GetSoundVoice(SoundHandle handle, size_t voiceIndex) const;

    MusicHandle RequestMusic(AssetScopeHandle scope, const char* path);
    bool IsReady(MusicHandle handle) const;
    bool IsFinished(MusicHandle handle) const;
    bool HasFailed(MusicHandle handle) const;
    const MusicAsset* GetMusic(MusicHandle handle) const;

    SpriteAnimationHandle RequestSpriteAnimation(
            AssetScopeHandle scope,
            const char* key,
            const char* jsonPath,
            TextureColorUsage atlasColorUsage,
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
    void GetScopeProgressCounts(
            AssetScopeHandle scope,
            size_t& finished,
            size_t& total) const;

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
        TextureColorUsage textureColorUsage = TextureColorUsage::Count;
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
    ModelAssets models;
    SpriteAnimationAssets spriteAnimations;
    SoundAssets sounds;
    MusicAssets music;

    std::thread worker;
    std::atomic<bool> shutdownRequested{false};
    std::mutex requestMutex;
    std::condition_variable requestCv;
    std::deque<AssetRequest> requests;
};

} // namespace engine
