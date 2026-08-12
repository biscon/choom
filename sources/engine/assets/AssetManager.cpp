#include "engine/assets/AssetManager.h"

#include <cassert>
#include <limits>
#include <utility>

namespace engine {

AssetManager::AssetManager() = default;

AssetManager::~AssetManager()
{
    Shutdown();
}

bool AssetManager::Initialize()
{
    if (initialized) {
        return true;
    }

    shutdownRequested.store(false);
    globalScope = CreateScope("global");
    initialized = !IsNull(globalScope);

    if (!initialized) {
        return false;
    }

    worker = std::thread(&AssetManager::WorkerLoop, this);
    return true;
}

void AssetManager::Shutdown()
{
    if (!initialized && !worker.joinable()) {
        return;
    }

    shutdownRequested.store(true);
    requestCv.notify_all();

    if (worker.joinable()) {
        worker.join();
    }

    {
        std::lock_guard<std::mutex> lock(requestMutex);
        requests.clear();
    }

    spriteAnimations.Shutdown();
    models.ShutdownMainThread();
    fonts.ShutdownMainThread();
    textures.ShutdownMainThread();
    music.ShutdownMainThread();
    sounds.ShutdownMainThread();

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        scopes.clear();
        globalScope = NullAssetScopeHandle();
        initialized = false;
    }
}

AssetScopeHandle AssetManager::CreateScope(const char* name)
{
    AssetScopeHandle handle;
    {
        std::lock_guard<std::mutex> lock(stateMutex);

        assert(scopes.size() < std::numeric_limits<uint32_t>::max());
        ScopeSlot slot;
        slot.alive = true;
        slot.name = (name != nullptr) ? name : "";

        const uint32_t index = static_cast<uint32_t>(scopes.size());
        scopes.push_back(std::move(slot));
        handle = AssetScopeHandle{index, scopes[index].generation};
    }

    textures.OnScopeCreated(handle);
    fonts.OnScopeCreated(handle);
    models.OnScopeCreated(handle);
    spriteAnimations.OnScopeCreated(handle);
    sounds.OnScopeCreated(handle);
    music.OnScopeCreated(handle);
    return handle;
}

AssetScopeHandle AssetManager::GlobalScope() const
{
    return globalScope;
}

void AssetManager::UnloadScope(AssetScopeHandle scope)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);

        if (!IsValidScopeNoLock(scope)) {
            return;
        }

        ScopeSlot& scopeSlot = scopes[scope.index];
        scopeSlot.alive = false;
        ++scopeSlot.generation;
    }

    textures.UnloadScope(scope);
    fonts.UnloadScope(scope);
    models.UnloadScope(scope);
    spriteAnimations.UnloadScope(scope);
    sounds.UnloadScope(scope);
    music.UnloadScope(scope);
}

TextureHandle AssetManager::RequestTexture(
        AssetScopeHandle scope,
        const char* key,
        const char* path,
        TextureColorUsage colorUsage,
        TextureLoadFlags flags)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidScopeNoLock(scope)) {
            return NullTextureHandle();
        }
    }

    TextureAssets::RequestResult result = textures.RequestTexture(
            scope, key, path, colorUsage, flags);
    if (result.shouldQueue) {
        AssetRequest request;
        request.type = AssetRequestType::Texture;
        request.texture = result.handle;
        request.path = result.path;
        request.textureColorUsage = result.colorUsage;
        request.textureFlags = result.flags;
        EnqueueRequest(std::move(request));
    }

    return result.handle;
}

TextureHandle AssetManager::CreateTextureFromImage(
        AssetScopeHandle scope,
        const char* key,
        const Image& image,
        TextureColorUsage colorUsage,
        TextureLoadFlags flags)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidScopeNoLock(scope)) {
            return NullTextureHandle();
        }
    }

    return textures.CreateTextureFromImage(
            scope, key, image, colorUsage, flags);
}

TextureHandle AssetManager::CreateCubemapFromImage(
        AssetScopeHandle scope,
        const char* key,
        const Image& image,
        TextureColorUsage colorUsage,
        int layout)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidScopeNoLock(scope)) {
            return NullTextureHandle();
        }
    }
    return textures.CreateCubemapFromImage(
            scope, key, image, colorUsage, layout);
}

bool AssetManager::IsReady(TextureHandle handle) const
{
    return textures.IsReady(handle);
}

bool AssetManager::IsFinished(TextureHandle handle) const
{
    return textures.IsFinished(handle);
}

bool AssetManager::HasFailed(TextureHandle handle) const
{
    return textures.HasFailed(handle);
}

const Texture2D* AssetManager::GetTexture(TextureHandle handle) const
{
    return textures.GetTexture(handle);
}

const TextureCubemap* AssetManager::GetCubemap(TextureHandle handle) const
{
    return textures.GetCubemap(handle);
}

FontHandle AssetManager::RequestFont(
        AssetScopeHandle scope,
        const char* key,
        const char* path,
        int pixelSize,
        FontLoadFlags flags)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidScopeNoLock(scope)) {
            return NullFontHandle();
        }
    }

    return fonts.RequestFont(scope, key, path, pixelSize, flags);
}

bool AssetManager::IsReady(FontHandle handle) const
{
    return fonts.IsReady(handle);
}

bool AssetManager::IsFinished(FontHandle handle) const
{
    return fonts.IsFinished(handle);
}

bool AssetManager::HasFailed(FontHandle handle) const
{
    return fonts.HasFailed(handle);
}

const FontAsset* AssetManager::GetFont(FontHandle handle) const
{
    return fonts.GetFont(handle);
}

ModelHandle AssetManager::RequestModel(
        AssetScopeHandle scope,
        const char* key,
        const char* path,
        ModelLoadFlags flags)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidScopeNoLock(scope)) {
            return NullModelHandle();
        }
    }
    return models.RequestModel(scope, key, path, flags);
}

bool AssetManager::IsReady(ModelHandle handle) const
{
    return models.IsReady(handle);
}

bool AssetManager::IsFinished(ModelHandle handle) const
{
    return models.IsFinished(handle);
}

bool AssetManager::HasFailed(ModelHandle handle) const
{
    return models.HasFailed(handle);
}

const Model* AssetManager::GetModel(ModelHandle handle) const
{
    return models.GetModel(handle);
}

const ModelAsset* AssetManager::GetModelAsset(ModelHandle handle) const
{
    return models.GetModelAsset(handle);
}

ModelHandle AssetManager::FindReadyModelByPath(const char* path) const
{
    return models.FindReadyModelByPath(path);
}

SoundHandle AssetManager::RequestSound(AssetScopeHandle scope, const char* path)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidScopeNoLock(scope)) return NullSoundHandle();
    }
    return sounds.RequestSound(scope, path);
}

bool AssetManager::IsReady(SoundHandle handle) const
{
    return sounds.IsReady(handle);
}

bool AssetManager::IsFinished(SoundHandle handle) const
{
    return sounds.IsFinished(handle);
}

bool AssetManager::HasFailed(SoundHandle handle) const
{
    return sounds.HasFailed(handle);
}

const SoundAsset* AssetManager::GetSound(SoundHandle handle) const
{
    return sounds.Get(handle);
}

const Sound* AssetManager::GetSoundVoice(
        SoundHandle handle,
        size_t voiceIndex) const
{
    return sounds.GetVoice(handle, voiceIndex);
}

MusicHandle AssetManager::RequestMusic(AssetScopeHandle scope, const char* path)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidScopeNoLock(scope)) return NullMusicHandle();
    }
    return music.RequestMusic(scope, path);
}

bool AssetManager::IsReady(MusicHandle handle) const
{
    return music.IsReady(handle);
}

bool AssetManager::IsFinished(MusicHandle handle) const
{
    return music.IsFinished(handle);
}

bool AssetManager::HasFailed(MusicHandle handle) const
{
    return music.HasFailed(handle);
}

const MusicAsset* AssetManager::GetMusic(MusicHandle handle) const
{
    return music.Get(handle);
}

SpriteAnimationHandle AssetManager::RequestSpriteAnimation(
        AssetScopeHandle scope,
        const char* key,
        const char* jsonPath,
        TextureColorUsage atlasColorUsage,
        TextureLoadFlags textureFlags)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidScopeNoLock(scope)) {
            return NullSpriteAnimationHandle();
        }
    }

    SpriteAnimationAssets::RequestResult result = spriteAnimations.RequestSpriteAnimation(
            scope,
            key,
            jsonPath,
            atlasColorUsage,
            textureFlags
    );

    if (result.shouldQueueJson) {
        AssetRequest request;
        request.type = AssetRequestType::SpriteAnimation;
        request.spriteAnimation = result.handle;
        request.path = result.jsonPath;
        request.textureColorUsage = atlasColorUsage;
        request.textureFlags = textureFlags;
        EnqueueRequest(std::move(request));
    }

    return result.handle;
}

bool AssetManager::IsReady(SpriteAnimationHandle handle) const
{
    return spriteAnimations.IsReady(handle, textures);
}

bool AssetManager::IsFinished(SpriteAnimationHandle handle) const
{
    return spriteAnimations.IsFinished(handle, textures);
}

bool AssetManager::HasFailed(SpriteAnimationHandle handle) const
{
    return spriteAnimations.HasFailed(handle, textures);
}

const SpriteAnimationAsset* AssetManager::GetSpriteAnimation(SpriteAnimationHandle handle) const
{
    return spriteAnimations.Get(handle, textures);
}

uint32_t AssetManager::FindSpriteClipIndex(SpriteAnimationHandle handle, const char* clipName) const
{
    return spriteAnimations.FindClipIndex(handle, clipName);
}

const SpriteClip* AssetManager::GetSpriteClip(SpriteAnimationHandle handle, uint32_t clipIndex) const
{
    return spriteAnimations.GetClip(handle, clipIndex);
}

bool AssetManager::IsScopeReady(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return IsValidScopeNoLock(scope)
        && textures.IsScopeReady(scope)
        && fonts.IsScopeReady(scope)
        && models.IsScopeReady(scope)
        && spriteAnimations.IsScopeReady(scope, textures)
        && sounds.IsScopeReady(scope)
        && music.IsScopeReady(scope);
}

bool AssetManager::IsScopeFinished(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return IsValidScopeNoLock(scope)
        && textures.IsScopeFinished(scope)
        && fonts.IsScopeFinished(scope)
        && models.IsScopeFinished(scope)
        && spriteAnimations.IsScopeFinished(scope, textures)
        && sounds.IsScopeFinished(scope)
        && music.IsScopeFinished(scope);
}

float AssetManager::GetScopeProgress(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidScopeNoLock(scope)) {
        return 1.0f;
    }

    size_t finished = 0;
    size_t total = 0;
    textures.GetScopeProgressCounts(scope, finished, total);
    fonts.GetScopeProgress(scope, finished, total);
    models.GetScopeProgress(scope, finished, total);
    spriteAnimations.GetScopeProgress(scope, textures, finished, total);
    sounds.GetScopeProgress(scope, finished, total);
    music.GetScopeProgress(scope, finished, total);

    if (total == 0) {
        return 1.0f;
    }

    return static_cast<float>(finished) / static_cast<float>(total);
}

void AssetManager::UpdateMainThread(float maxMilliseconds)
{
    textures.UpdateMainThread(maxMilliseconds);
    fonts.UpdateMainThread(maxMilliseconds);
    models.UpdateMainThread(maxMilliseconds);
    sounds.UpdateMainThread(maxMilliseconds);
    music.UpdateMainThread(maxMilliseconds);
}

bool AssetManager::IsValidScopeNoLock(AssetScopeHandle scope) const
{
    return scope.index < scopes.size()
        && scopes[scope.index].alive
        && scopes[scope.index].generation == scope.generation;
}

void AssetManager::WorkerLoop()
{
    while (true) {
        AssetRequest request;
        {
            std::unique_lock<std::mutex> lock(requestMutex);
            requestCv.wait(lock, [this]() {
                return shutdownRequested.load() || !requests.empty();
            });

            if (shutdownRequested.load()) {
                return;
            }

            request = std::move(requests.front());
            requests.pop_front();
        }

        switch (request.type) {
            case AssetRequestType::Texture:
                textures.ProcessTextureRequestOnWorkerThread(
                        request.texture,
                        request.path,
                        request.textureColorUsage,
                        request.textureFlags
                );
                break;
            case AssetRequestType::SpriteAnimation:
                {
                    TextureAssets::RequestResult result =
                            spriteAnimations.ProcessSpriteAnimationRequestOnWorkerThread(
                        request.spriteAnimation,
                        request.path,
                        textures
                    );
                    if (result.shouldQueue) {
                        AssetRequest textureRequest;
                        textureRequest.type = AssetRequestType::Texture;
                        textureRequest.texture = result.handle;
                        textureRequest.path = result.path;
                        textureRequest.textureColorUsage = result.colorUsage;
                        textureRequest.textureFlags = result.flags;
                        EnqueueRequest(std::move(textureRequest));
                    }
                }
                break;
        }
    }
}

void AssetManager::EnqueueRequest(AssetRequest request)
{
    {
        std::lock_guard<std::mutex> lock(requestMutex);
        requests.push_back(std::move(request));
    }
    requestCv.notify_one();
}

} // namespace engine
