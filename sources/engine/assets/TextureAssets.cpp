#include "engine/assets/TextureAssets.h"

#include <external/glad.h>
#include <raylib.h>
#include <rlgl.h>

#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>
#include <utility>

namespace engine {
namespace {

struct TextureUploadBindings {
    int texture2D = 0;
    int cubemap = 0;
    int unpackAlignment = 4;
};

TextureUploadBindings CaptureTextureUploadBindings()
{
    TextureUploadBindings bindings;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bindings.texture2D);
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &bindings.cubemap);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &bindings.unpackAlignment);
    return bindings;
}

void RestoreTextureUploadBindings(const TextureUploadBindings& bindings)
{
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bindings.texture2D));
    glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(bindings.cubemap));
    glPixelStorei(GL_UNPACK_ALIGNMENT, bindings.unpackAlignment);
}

void ClearGlErrors()
{
    while (glGetError() != GL_NO_ERROR) {
    }
}

bool IsSrgbUploadPixelFormat(int format)
{
    return format == PIXELFORMAT_UNCOMPRESSED_R8G8B8
            || format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
}

Image PrepareSrgbUploadImage(const Image& source, bool& ownsImage)
{
    ownsImage = false;
    if (IsSrgbUploadPixelFormat(source.format)) {
        return source;
    }

    Image converted = ImageCopy(source);
    if (converted.data == nullptr) {
        return Image{};
    }
    ImageFormat(&converted, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ownsImage = true;
    return converted;
}

bool PromoteTextureToSrgb(Texture2D& texture, const Image& image)
{
    if (texture.id == 0 || image.data == nullptr
            || !IsSrgbUploadPixelFormat(image.format)) {
        return false;
    }

    const GLenum externalFormat = image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8
            ? GL_RGB
            : GL_RGBA;
    const GLenum internalFormat = image.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8
            ? GL_SRGB8
            : GL_SRGB8_ALPHA8;
    const TextureUploadBindings bindings = CaptureTextureUploadBindings();
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ClearGlErrors();

    const auto* pixels = static_cast<const unsigned char*>(image.data);
    std::size_t offset = 0;
    int width = image.width;
    int height = image.height;
    for (int mip = 0; mip < image.mipmaps; ++mip) {
        glTexImage2D(
                GL_TEXTURE_2D,
                mip,
                static_cast<GLint>(internalFormat),
                width,
                height,
                0,
                externalFormat,
                GL_UNSIGNED_BYTE,
                pixels + offset);
        offset += static_cast<std::size_t>(
                GetPixelDataSize(width, height, image.format));
        width = std::max(1, width / 2);
        height = std::max(1, height / 2);
    }

    int actualFormat = 0;
    glGetTexLevelParameteriv(
            GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &actualFormat);
    const bool valid = glGetError() == GL_NO_ERROR
            && actualFormat == static_cast<int>(internalFormat);
    RestoreTextureUploadBindings(bindings);
    return valid;
}

Texture2D UploadTextureFromImage(
        const Image& source,
        TextureColorUsage colorUsage)
{
    if (colorUsage != TextureColorUsage::SceneSrgb) {
        return LoadTextureFromImage(source);
    }

    bool ownsImage = false;
    Image uploadImage = PrepareSrgbUploadImage(source, ownsImage);
    Texture2D texture{};
    if (uploadImage.data != nullptr) {
        texture = LoadTextureFromImage(uploadImage);
        if (!PromoteTextureToSrgb(texture, uploadImage)) {
            if (texture.id != 0) {
                UnloadTexture(texture);
            }
            texture = {};
        }
    }
    if (ownsImage && uploadImage.data != nullptr) {
        UnloadImage(uploadImage);
    }
    return texture;
}

TextureCubemap UploadSrgbVerticalCubemap(const Image& source)
{
    TextureCubemap cubemap{};
    if (source.data == nullptr || source.width <= 0
            || source.height != source.width * 6
            || !IsSrgbUploadPixelFormat(source.format)) {
        return cubemap;
    }

    const GLenum externalFormat = source.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8
            ? GL_RGB
            : GL_RGBA;
    const GLenum internalFormat = source.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8
            ? GL_SRGB8
            : GL_SRGB8_ALPHA8;
    const TextureUploadBindings bindings = CaptureTextureUploadBindings();
    glGenTextures(1, &cubemap.id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ClearGlErrors();

    const auto* pixels = static_cast<const unsigned char*>(source.data);
    std::size_t offset = 0;
    int size = source.width;
    for (int mip = 0; mip < source.mipmaps; ++mip) {
        const std::size_t faceBytes = static_cast<std::size_t>(
                GetPixelDataSize(size, size, source.format));
        for (int face = 0; face < 6; ++face) {
            glTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                    mip,
                    static_cast<GLint>(internalFormat),
                    size,
                    size,
                    0,
                    externalFormat,
                    GL_UNSIGNED_BYTE,
                    pixels + offset + faceBytes * static_cast<std::size_t>(face));
        }
        offset += faceBytes * 6u;
        size = std::max(1, size / 2);
    }
    glTexParameteri(
            GL_TEXTURE_CUBE_MAP,
            GL_TEXTURE_MIN_FILTER,
            source.mipmaps > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    int actualFormat = 0;
    glGetTexLevelParameteriv(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X,
            0,
            GL_TEXTURE_INTERNAL_FORMAT,
            &actualFormat);
    const bool valid = glGetError() == GL_NO_ERROR
            && actualFormat == static_cast<int>(internalFormat);
    RestoreTextureUploadBindings(bindings);
    if (!valid) {
        if (cubemap.id != 0) {
            rlUnloadTexture(cubemap.id);
        }
        return TextureCubemap{};
    }

    cubemap.width = source.width;
    cubemap.height = source.width;
    cubemap.mipmaps = source.mipmaps;
    cubemap.format = source.format;
    return cubemap;
}

TextureCubemap UploadLinearHdrVerticalCubemap(const Image& source)
{
    TextureCubemap cubemap{};
    if (source.data == nullptr || source.width <= 0
            || source.height != source.width * 6
            || source.format != PIXELFORMAT_UNCOMPRESSED_R16G16B16A16) {
        return cubemap;
    }
    const TextureUploadBindings bindings = CaptureTextureUploadBindings();
    glGenTextures(1, &cubemap.id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    ClearGlErrors();
    const auto* pixels = static_cast<const unsigned char*>(source.data);
    std::size_t offset = 0;
    int size = source.width;
    for (int mip = 0; mip < source.mipmaps; ++mip) {
        const std::size_t faceBytes = static_cast<std::size_t>(size)
                * static_cast<std::size_t>(size) * 4u * sizeof(std::uint16_t);
        for (int face = 0; face < 6; ++face) {
            glTexImage2D(
                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + face,
                    mip,
                    GL_RGBA16F,
                    size,
                    size,
                    0,
                    GL_RGBA,
                    GL_HALF_FLOAT,
                    pixels + offset + faceBytes * static_cast<std::size_t>(face));
        }
        offset += faceBytes * 6u;
        size = std::max(1, size / 2);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
            source.mipmaps > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    int actualFormat = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0,
            GL_TEXTURE_INTERNAL_FORMAT, &actualFormat);
    const bool valid = glGetError() == GL_NO_ERROR
            && actualFormat == static_cast<int>(GL_RGBA16F);
    RestoreTextureUploadBindings(bindings);
    if (!valid) {
        if (cubemap.id != 0) rlUnloadTexture(cubemap.id);
        return {};
    }
    cubemap.width = source.width;
    cubemap.height = source.width;
    cubemap.mipmaps = source.mipmaps;
    cubemap.format = source.format;
    return cubemap;
}

} // namespace

unsigned int TextureInternalFormatForColorUsage(
        TextureColorUsage colorUsage,
        int pixelFormat)
{
    if (colorUsage == TextureColorUsage::SceneSrgb) {
        if (pixelFormat == PIXELFORMAT_UNCOMPRESSED_R8G8B8) {
            return GL_SRGB8;
        }
        if (pixelFormat == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
            return GL_SRGB8_ALPHA8;
        }
        return 0;
    }

    if (pixelFormat == PIXELFORMAT_UNCOMPRESSED_R16G16B16A16) {
        return GL_RGBA16F;
    }

    unsigned int internalFormat = 0;
    unsigned int externalFormat = 0;
    unsigned int type = 0;
    rlGetGlTextureFormats(
            pixelFormat, &internalFormat, &externalFormat, &type);
    return internalFormat;
}

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
        TextureColorUsage colorUsage,
        TextureLoadFlags flags)
{
    RequestResult result;
    if (key == nullptr || path == nullptr
            || !IsValidTextureRequestDescriptor(colorUsage, flags)) {
        return result;
    }

    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        return result;
    }

    const std::string requestKey = MakeTextureRequestKey(
            key, path, colorUsage, flags);
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
    slot.colorUsage = colorUsage;
    slot.flags = flags;
    slot.scope = scope;

    const uint32_t index = static_cast<uint32_t>(textureSlots.size());
    textureSlots.push_back(std::move(slot));

    result.handle = TextureHandle{index, textureSlots[index].generation};
    result.shouldQueue = true;
    result.path = path;
    result.colorUsage = colorUsage;
    result.flags = flags;

    data.textures.push_back(result.handle);
    data.textureByRequest.emplace(requestKey, result.handle);

    return result;
}

TextureHandle TextureAssets::CreateTextureFromImage(
        AssetScopeHandle scope,
        const char* key,
        const Image& image,
        TextureColorUsage colorUsage,
        TextureLoadFlags flags)
{
    if (key == nullptr || image.data == nullptr || image.width <= 0 || image.height <= 0
            || !IsValidTextureRequestDescriptor(colorUsage, flags)) {
        return NullTextureHandle();
    }

    Texture2D uploaded = UploadTextureFromImage(image, colorUsage);
    if (uploaded.id == 0) {
        std::fprintf(stderr, "[AssetManager WARNING] Texture upload failed for generated texture: %s\n", key);
        return NullTextureHandle();
    }

    ApplyTextureLoadFlags(uploaded, flags);

    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        pendingUnloads.push_back(uploaded);
        return NullTextureHandle();
    }

    const std::string requestKey = MakeGeneratedTextureKey(
            key, colorUsage, flags);
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
    slot.colorUsage = colorUsage;
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

TextureHandle TextureAssets::CreateCubemapFromImage(
        AssetScopeHandle scope,
        const char* key,
        const Image& image,
        TextureColorUsage colorUsage,
        int layout)
{
    if (key == nullptr || image.data == nullptr || image.width <= 0 || image.height <= 0
            || !IsValidTextureColorUsage(colorUsage)) {
        return NullTextureHandle();
    }

    const std::string requestKey = MakeGeneratedCubemapKey(key, colorUsage);
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (scope.index >= scopeData.size()) {
            return NullTextureHandle();
        }
        const auto existing = scopeData[scope.index].textureByRequest.find(requestKey);
        if (existing != scopeData[scope.index].textureByRequest.end()) {
            return existing->second;
        }
    }

    TextureCubemap uploaded = colorUsage == TextureColorUsage::SceneSrgb
            && layout == CUBEMAP_LAYOUT_LINE_VERTICAL
            ? UploadSrgbVerticalCubemap(image)
            : colorUsage == TextureColorUsage::LinearData
                            && layout == CUBEMAP_LAYOUT_LINE_VERTICAL
                            && image.format == PIXELFORMAT_UNCOMPRESSED_R16G16B16A16
                    ? UploadLinearHdrVerticalCubemap(image)
            : colorUsage == TextureColorUsage::SceneSrgb
                    ? TextureCubemap{}
                    : LoadTextureCubemap(image, layout);
    if (uploaded.id == 0) {
        std::fprintf(stderr, "[AssetManager WARNING] Cubemap upload failed for generated texture: %s\n", key);
        return NullTextureHandle();
    }
    if (uploaded.mipmaps > 1) {
        rlCubemapParameters(
                uploaded.id,
                RL_TEXTURE_MIN_FILTER,
                RL_TEXTURE_FILTER_MIP_LINEAR);
    }
    rlCubemapParameters(
            uploaded.id,
            RL_TEXTURE_MAG_FILTER,
            RL_TEXTURE_FILTER_LINEAR);

    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        pendingUnloads.push_back(uploaded);
        return NullTextureHandle();
    }
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
    slot.path = "<generated-cubemap>";
    slot.colorUsage = colorUsage;
    slot.scope = scope;
    slot.texture = uploaded;
    slot.cubemap = true;
    const uint32_t index = static_cast<uint32_t>(textureSlots.size());
    textureSlots.push_back(std::move(slot));
    const TextureHandle handle{index, textureSlots[index].generation};
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
    if (slot.state != TextureState::Ready || slot.texture.id == 0 || slot.cubemap) {
        return nullptr;
    }

    return &slot.texture;
}

const TextureCubemap* TextureAssets::GetCubemap(TextureHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidTextureNoLock(handle)) {
        return nullptr;
    }
    const TextureSlot& slot = textureSlots[handle.index];
    if (slot.state != TextureState::Ready || slot.texture.id == 0 || !slot.cubemap) {
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
        TextureColorUsage colorUsage,
        TextureLoadFlags flags)
{
    if (!IsValidTextureRequestDescriptor(colorUsage, flags)) {
        return;
    }
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
        TextureColorUsage colorUsage = TextureColorUsage::Count;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            shouldUpload = IsValidTextureNoLock(payload.handle)
                && textureSlots[payload.handle.index].state != TextureState::QueuedForUnload;
            if (shouldUpload) {
                colorUsage = textureSlots[payload.handle.index].colorUsage;
            }
        }

        Texture2D uploaded = {};
        bool uploadedTexture = false;
        if (shouldUpload && payload.success && payload.image.data != nullptr) {
            uploaded = UploadTextureFromImage(payload.image, colorUsage);
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
                    ApplyTextureLoadFlags(slot.texture, slot.flags);
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

std::string TextureAssets::MakeTextureRequestKey(
        const char* key,
        const char* path,
        TextureColorUsage colorUsage,
        TextureLoadFlags flags)
{
    return std::string(key) + "\n" + path
            + "\n" + std::to_string(static_cast<int>(colorUsage))
            + "\n" + std::to_string(static_cast<uint32_t>(flags));
}

std::string TextureAssets::MakeGeneratedTextureKey(
        const char* key,
        TextureColorUsage colorUsage,
        TextureLoadFlags flags)
{
    return std::string(key) + "\n<generated>"
            + "\n" + std::to_string(static_cast<int>(colorUsage))
            + "\n" + std::to_string(static_cast<uint32_t>(flags));
}

std::string TextureAssets::MakeGeneratedCubemapKey(
        const char* key,
        TextureColorUsage colorUsage)
{
    return std::string(key) + "\n<generated-cubemap>"
            + "\n" + std::to_string(static_cast<int>(colorUsage));
}

void TextureAssets::ApplyTextureLoadFlags(Texture2D& texture, TextureLoadFlags flags)
{
    if (texture.id == 0) {
        return;
    }

    const bool needsMipmaps = HasFlag(flags, TextureLoad_Mipmaps)
            || HasFlag(flags, TextureLoad_TrilinearFilter)
            || HasFlag(flags, TextureLoad_Anisotropic8x);
    if (needsMipmaps && texture.mipmaps <= 1) {
        GenTextureMipmaps(&texture);
    }

    if (HasFlag(flags, TextureLoad_Anisotropic8x)) {
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
        SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);
    } else if (HasFlag(flags, TextureLoad_TrilinearFilter)) {
        SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
    } else if (HasFlag(flags, TextureLoad_BilinearFilter)) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    } else {
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    }
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
