#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/TextureLoadFlags.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

class TextureAssets {
public:
    struct RequestResult {
        TextureHandle handle;
        bool shouldQueue = false;
        std::string path;
        TextureLoadFlags flags = TextureLoad_PointFilter;
    };

    void OnScopeCreated(AssetScopeHandle scope);
    void UnloadScope(AssetScopeHandle scope);

    RequestResult RequestTexture(
            AssetScopeHandle scope,
            const char* key,
            const char* path,
            TextureLoadFlags flags
    );

    TextureHandle CreateTextureFromImage(
            AssetScopeHandle scope,
            const char* key,
            const Image& image,
            TextureLoadFlags flags
    );

    bool IsReady(TextureHandle handle) const;
    bool IsFinished(TextureHandle handle) const;
    bool HasFailed(TextureHandle handle) const;
    const Texture2D* GetTexture(TextureHandle handle) const;

    bool IsScopeReady(AssetScopeHandle scope) const;
    bool IsScopeFinished(AssetScopeHandle scope) const;
    float GetScopeProgress(AssetScopeHandle scope) const;
    void GetScopeProgressCounts(AssetScopeHandle scope, size_t& finished, size_t& total) const;

    void ProcessTextureRequestOnWorkerThread(
            TextureHandle handle,
            const std::string& path,
            TextureLoadFlags flags
    );

    void UpdateMainThread(float maxMilliseconds);
    void ShutdownMainThread();

private:
    enum class TextureState {
        Unloaded,
        Queued,
        Loading,
        WaitingForUpload,
        Ready,
        Failed,
        QueuedForUnload
    };

    struct TextureSlot {
        uint32_t generation = 1;
        TextureState state = TextureState::Unloaded;
        std::string key;
        std::string path;
        TextureLoadFlags flags = TextureLoad_PointFilter;
        AssetScopeHandle scope;
        Texture2D texture = {};
        std::string error;
    };

    struct TextureScopeData {
        std::vector<TextureHandle> textures;
        std::unordered_map<std::string, TextureHandle> textureByRequest;
    };

    struct CompletedTexture {
        TextureHandle handle;
        Image image = {};
        bool success = false;
        std::string error;
    };

    bool IsValidTextureNoLock(TextureHandle handle) const;
    static bool IsTerminal(TextureState state);
    static std::string MakeTextureRequestKey(const char* key, const char* path, TextureLoadFlags flags);
    static std::string MakeGeneratedTextureKey(const char* key, TextureLoadFlags flags);
    static void ApplyTextureLoadFlags(Texture2D& texture, TextureLoadFlags flags);

    void QueueTextureUnloadNoLock(TextureHandle handle);
    void UnloadReadyTextures();

    mutable std::mutex stateMutex;
    std::vector<TextureSlot> textureSlots;
    std::vector<TextureScopeData> scopeData;
    std::vector<Texture2D> pendingUnloads;

    std::mutex completedMutex;
    std::deque<CompletedTexture> completed;
};

} // namespace engine
