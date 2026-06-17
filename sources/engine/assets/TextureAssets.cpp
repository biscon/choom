#include "engine/assets/TextureAssets.h"

#include <raylib.h>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>
#include <utility>

namespace engine {

void TextureAssets::OnScopeCreated(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        scopeData.resize(static_cast<size_t>(scope.index) + 1);
    }
}

void TextureAssets::UnloadScope(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        return;
    }

    TextureScopeData& data = scopeData[scope.index];
    for (TextureHandle handle : data.textures) {
        QueueTextureUnloadNoLock(handle);
    }

    data.textures.clear();
    data.textureByRequest.clear();
}

TextureAssets::RequestResult TextureAssets::RequestTexture(
        AssetScopeHandle scope,
        const char* key,
        const char* path,
        TextureLoadFlags flags)
{
    RequestResult result;
    if (key == nullptr || path == nullptr) {
        return result;
    }

    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        return result;
    }

    const std::string requestKey = MakeTextureRequestKey(key, path, flags);
    TextureScopeData& data = scopeData[scope.index];
    const auto existing = data.textureByRequest.find(requestKey);
    if (existing != data.textureByRequest.end()) {
        result.handle = existing->second;
        return result;
    }

    assert(textureSlots.size() < std::numeric_limits<uint32_t>::max());
    TextureSlot slot;
    slot.state = TextureState::Queued;
    slot.key = key;
    slot.path = path;
    slot.flags = flags;
    slot.scope = scope;

    const uint32_t index = static_cast<uint32_t>(textureSlots.size());
    textureSlots.push_back(std::move(slot));

    result.handle = TextureHandle{index, textureSlots[index].generation};
    result.shouldQueue = true;
    result.path = path;
    result.flags = flags;

    data.textures.push_back(result.handle);
    data.textureByRequest.emplace(requestKey, result.handle);

    return result;
}

TextureHandle TextureAssets::CreateTextureFromImage(
        AssetScopeHandle scope,
        const char* key,
        const Image& image,
        TextureLoadFlags flags)
{
    if (key == nullptr || image.data == nullptr || image.width <= 0 || image.height <= 0) {
        return NullTextureHandle();
    }

    Texture2D uploaded = LoadTextureFromImage(image);
    if (uploaded.id == 0) {
        std::fprintf(stderr, "[AssetManager WARNING] Texture upload failed for generated texture: %s\n", key);
        return NullTextureHandle();
    }

    if (HasFlag(flags, TextureLoad_BilinearFilter)) {
        SetTextureFilter(uploaded, TEXTURE_FILTER_BILINEAR);
    } else {
        SetTextureFilter(uploaded, TEXTURE_FILTER_POINT);
    }

    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        pendingUnloads.push_back(uploaded);
        return NullTextureHandle();
    }

    const std::string requestKey = MakeGeneratedTextureKey(key, flags);
    TextureScopeData& data = scopeData[scope.index];
    const auto existing = data.textureByRequest.find(requestKey);
    if (existing != data.textureByRequest.end()) {
        pendingUnloads.push_back(uploaded);
        return existing->second;
    }

    assert(textureSlots.size() < std::numeric_limits<uint32_t>::max());
    TextureSlot slot;
    slot.state = TextureState::Ready;
    slot.key = key;
    slot.path = "<generated>";
    slot.flags = flags;
    slot.scope = scope;
    slot.texture = uploaded;

    const uint32_t index = static_cast<uint32_t>(textureSlots.size());
    textureSlots.push_back(std::move(slot));

    TextureHandle handle{index, textureSlots[index].generation};
    data.textures.push_back(handle);
    data.textureByRequest.emplace(requestKey, handle);
    return handle;
}

bool TextureAssets::IsReady(TextureHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return IsValidTextureNoLock(handle)
        && textureSlots[handle.index].state == TextureState::Ready;
}

bool TextureAssets::IsFinished(TextureHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidTextureNoLock(handle)
        || IsTerminal(textureSlots[handle.index].state);
}

bool TextureAssets::HasFailed(TextureHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidTextureNoLock(handle)
        || textureSlots[handle.index].state == TextureState::Failed;
}

const Texture2D* TextureAssets::GetTexture(TextureHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidTextureNoLock(handle)) {
        return nullptr;
    }

    const TextureSlot& slot = textureSlots[handle.index];
    if (slot.state != TextureState::Ready || slot.texture.id == 0) {
        return nullptr;
    }

    return &slot.texture;
}

bool TextureAssets::IsScopeReady(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        return false;
    }

    const TextureScopeData& data = scopeData[scope.index];
    for (TextureHandle handle : data.textures) {
        if (!IsValidTextureNoLock(handle)
                || textureSlots[handle.index].state != TextureState::Ready) {
            return false;
        }
    }

    return true;
}

bool TextureAssets::IsScopeFinished(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        return false;
    }

    const TextureScopeData& data = scopeData[scope.index];
    for (TextureHandle handle : data.textures) {
        if (IsValidTextureNoLock(handle)
                && !IsTerminal(textureSlots[handle.index].state)) {
            return false;
        }
    }

    return true;
}

float TextureAssets::GetScopeProgress(AssetScopeHandle scope) const
{
    size_t finished = 0;
    size_t total = 0;
    GetScopeProgressCounts(scope, finished, total);
    if (total == 0) {
        return 1.0f;
    }

    return static_cast<float>(finished) / static_cast<float>(total);
}

void TextureAssets::GetScopeProgressCounts(AssetScopeHandle scope, size_t& finished, size_t& total) const
{
    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        return;
    }

    const TextureScopeData& data = scopeData[scope.index];
    total += data.textures.size();
    for (TextureHandle handle : data.textures) {
        if (!IsValidTextureNoLock(handle)
                || IsTerminal(textureSlots[handle.index].state)) {
            ++finished;
        }
    }
}

void TextureAssets::ProcessTextureRequestOnWorkerThread(
        TextureHandle handle,
        const std::string& path,
        TextureLoadFlags flags)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidTextureNoLock(handle)
                || textureSlots[handle.index].state == TextureState::QueuedForUnload) {
            return;
        }
        textureSlots[handle.index].state = TextureState::Loading;
    }

    CompletedTexture payload;
    payload.handle = handle;
    payload.image = LoadImage(path.c_str());
    payload.success = payload.image.data != nullptr;

    if (payload.success) {
        if (HasFlag(flags, TextureLoad_PremultiplyAlpha)) {
            ImageAlphaPremultiply(&payload.image);
        }
    } else {
        payload.error = "Failed to load image: " + path;
        std::fprintf(stderr, "[AssetManager WARNING] %s\n", payload.error.c_str());
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (IsValidTextureNoLock(handle)
                && textureSlots[handle.index].state != TextureState::QueuedForUnload) {
            textureSlots[handle.index].state = payload.success
                ? TextureState::WaitingForUpload
                : TextureState::Loading;
        }
    }

    {
        std::lock_guard<std::mutex> lock(completedMutex);
        completed.push_back(std::move(payload));
    }
}

void TextureAssets::UpdateMainThread(float maxMilliseconds)
{
    const auto start = std::chrono::steady_clock::now();

    while (true) {
        CompletedTexture payload;
        {
            std::lock_guard<std::mutex> lock(completedMutex);
            if (completed.empty()) {
                break;
            }
            payload = std::move(completed.front());
            completed.pop_front();
        }

        bool shouldUpload = false;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            shouldUpload = IsValidTextureNoLock(payload.handle)
                && textureSlots[payload.handle.index].state != TextureState::QueuedForUnload;
        }

        Texture2D uploaded = {};
        bool uploadedTexture = false;
        if (shouldUpload && payload.success && payload.image.data != nullptr) {
            uploaded = LoadTextureFromImage(payload.image);
            uploadedTexture = uploaded.id != 0;
        }

        if (payload.image.data != nullptr) {
            UnloadImage(payload.image);
            payload.image = {};
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (IsValidTextureNoLock(payload.handle)) {
                TextureSlot& slot = textureSlots[payload.handle.index];
                if (slot.state == TextureState::QueuedForUnload) {
                    if (uploadedTexture) {
                        pendingUnloads.push_back(uploaded);
                    }
                } else if (uploadedTexture) {
                    slot.texture = uploaded;
                    if (HasFlag(slot.flags, TextureLoad_BilinearFilter)) {
                        SetTextureFilter(slot.texture, TEXTURE_FILTER_BILINEAR);
                    } else {
                        SetTextureFilter(slot.texture, TEXTURE_FILTER_POINT);
                    }
                    slot.state = TextureState::Ready;
                    slot.error.clear();
                } else {
                    slot.state = TextureState::Failed;
                    slot.error = payload.error.empty()
                        ? "Texture upload failed"
                        : payload.error;
                }
            } else if (uploadedTexture) {
                pendingUnloads.push_back(uploaded);
            }
        }

        if (maxMilliseconds > 0.0f) {
            const auto now = std::chrono::steady_clock::now();
            const float elapsed = std::chrono::duration<float, std::milli>(now - start).count();
            if (elapsed >= maxMilliseconds) {
                break;
            }
        }
    }

    UnloadReadyTextures();
}

void TextureAssets::ShutdownMainThread()
{
    {
        std::lock_guard<std::mutex> lock(completedMutex);
        for (CompletedTexture& payload : completed) {
            if (payload.image.data != nullptr) {
                UnloadImage(payload.image);
            }
        }
        completed.clear();
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        for (TextureSlot& slot : textureSlots) {
            if ((slot.state == TextureState::Ready || slot.state == TextureState::QueuedForUnload)
                    && slot.texture.id != 0) {
                pendingUnloads.push_back(slot.texture);
                slot.texture = {};
            }
            slot.state = TextureState::Unloaded;
        }
    }

    UnloadReadyTextures();

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        textureSlots.clear();
        scopeData.clear();
        pendingUnloads.clear();
    }
}

bool TextureAssets::IsValidTextureNoLock(TextureHandle handle) const
{
    return handle.index < textureSlots.size()
        && textureSlots[handle.index].generation == handle.generation;
}

bool TextureAssets::IsTerminal(TextureState state)
{
    return state == TextureState::Ready
        || state == TextureState::Failed
        || state == TextureState::Unloaded;
}

std::string TextureAssets::MakeTextureRequestKey(const char* key, const char* path, TextureLoadFlags flags)
{
    return std::string(key) + "\n" + path + "\n" + std::to_string(static_cast<uint32_t>(flags));
}

std::string TextureAssets::MakeGeneratedTextureKey(const char* key, TextureLoadFlags flags)
{
    return std::string(key) + "\n<generated>\n" + std::to_string(static_cast<uint32_t>(flags));
}

void TextureAssets::QueueTextureUnloadNoLock(TextureHandle handle)
{
    if (!IsValidTextureNoLock(handle)) {
        return;
    }

    TextureSlot& slot = textureSlots[handle.index];
    if (slot.texture.id != 0) {
        pendingUnloads.push_back(slot.texture);
        slot.texture = {};
    }

    slot.state = TextureState::QueuedForUnload;
    ++slot.generation;
}

void TextureAssets::UnloadReadyTextures()
{
    std::vector<Texture2D> unloads;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        unloads.swap(pendingUnloads);
    }

    for (Texture2D texture : unloads) {
        if (texture.id != 0) {
            UnloadTexture(texture);
        }
    }
}

} // namespace engine
