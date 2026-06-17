#include "engine/assets/SpriteAnimationAssets.h"

#include "util/json.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <utility>

namespace engine {

static constexpr uint32_t InvalidClipIndex = UINT32_MAX;

static float JsonFloat(const nlohmann::ordered_json& json, const char* key, float fallback = 0.0f)
{
    if (!json.contains(key) || !json[key].is_number()) {
        return fallback;
    }

    return json[key].get<float>();
}

static uint32_t JsonUint(const nlohmann::ordered_json& json, const char* key, uint32_t fallback = 0)
{
    if (!json.contains(key) || !json[key].is_number_unsigned()) {
        if (json.contains(key) && json[key].is_number_integer()) {
            const int value = json[key].get<int>();
            return value >= 0 ? static_cast<uint32_t>(value) : fallback;
        }
        return fallback;
    }

    return json[key].get<uint32_t>();
}

static uint32_t ParseRepeat(const nlohmann::ordered_json& json)
{
    if (!json.contains("repeat")) {
        return 0;
    }

    const nlohmann::ordered_json& repeat = json["repeat"];
    if (repeat.is_number_unsigned()) {
        return repeat.get<uint32_t>();
    }
    if (repeat.is_number_integer()) {
        const int value = repeat.get<int>();
        return value >= 0 ? static_cast<uint32_t>(value) : 0;
    }
    if (repeat.is_string()) {
        try {
            return static_cast<uint32_t>(std::stoul(repeat.get<std::string>()));
        } catch (...) {
            return 0;
        }
    }

    return 0;
}

static SpritePlaybackMode ParsePlaybackMode(const std::string& direction)
{
    if (direction == "reverse") {
        return SpritePlaybackMode::Reverse;
    }
    if (direction == "pingpong") {
        return SpritePlaybackMode::PingPong;
    }
    return SpritePlaybackMode::Loop;
}

static Rectangle ParseRectangle(const nlohmann::ordered_json& json)
{
    return Rectangle{
            JsonFloat(json, "x"),
            JsonFloat(json, "y"),
            JsonFloat(json, "w"),
            JsonFloat(json, "h")
    };
}

static bool ParseSpriteAnimationJson(
        const std::string& jsonPath,
        SpriteAnimationAsset& asset,
        std::string& atlasPath,
        std::string& error)
{
    std::ifstream input(jsonPath);
    if (!input.is_open()) {
        error = "Failed to open sprite animation JSON: " + jsonPath;
        return false;
    }

    nlohmann::ordered_json document = nlohmann::ordered_json::parse(input, nullptr, false);
    if (document.is_discarded()) {
        error = "Failed to parse sprite animation JSON: " + jsonPath;
        return false;
    }

    if (!document.contains("frames") || !document["frames"].is_object()) {
        error = "Sprite animation JSON missing frames object: " + jsonPath;
        return false;
    }

    if (!document.contains("meta")
            || !document["meta"].is_object()
            || !document["meta"].contains("image")
            || !document["meta"]["image"].is_string()
            || document["meta"]["image"].get<std::string>().empty()) {
        error = "Sprite animation JSON missing meta.image: " + jsonPath;
        return false;
    }

    const std::filesystem::path jsonFilePath(jsonPath);
    std::filesystem::path imagePath(document["meta"]["image"].get<std::string>());
    if (imagePath.is_relative()) {
        imagePath = jsonFilePath.parent_path() / imagePath;
    }
    atlasPath = imagePath.lexically_normal().string();

    SpriteAnimationAsset parsed;

    std::vector<std::string> frameNames;
    const nlohmann::ordered_json& framesJson = document["frames"];
    parsed.frames.reserve(framesJson.size());
    frameNames.reserve(framesJson.size());

    for (auto it = framesJson.begin(); it != framesJson.end(); ++it) {
        const std::string frameName = it.key();
        const nlohmann::ordered_json& frameJson = it.value();
        if (!frameJson.is_object() || !frameJson.contains("frame")) {
            std::fprintf(stderr, "[AssetManager WARNING] Skipping malformed sprite frame: %s\n", frameName.c_str());
            continue;
        }

        if (frameJson.value("rotated", false)) {
            std::fprintf(stderr, "[AssetManager WARNING] Rotated Aseprite frame parsed best-effort: %s\n", frameName.c_str());
        }

        SpriteFrame frame;
        frame.source = ParseRectangle(frameJson["frame"]);
        if (frameJson.contains("sourceSize") && frameJson["sourceSize"].is_object()) {
            const nlohmann::ordered_json& sourceSize = frameJson["sourceSize"];
            frame.sourceSize = Vector2{
                    JsonFloat(sourceSize, "w", frame.source.width),
                    JsonFloat(sourceSize, "h", frame.source.height)
            };
        } else {
            frame.sourceSize = Vector2{frame.source.width, frame.source.height};
        }

        if (frameJson.contains("spriteSourceSize") && frameJson["spriteSourceSize"].is_object()) {
            const nlohmann::ordered_json& spriteSourceSize = frameJson["spriteSourceSize"];
            frame.spriteSourcePosition = Vector2{
                    JsonFloat(spriteSourceSize, "x"),
                    JsonFloat(spriteSourceSize, "y")
            };
            frame.spriteSourceSize = Vector2{
                    JsonFloat(spriteSourceSize, "w", frame.source.width),
                    JsonFloat(spriteSourceSize, "h", frame.source.height)
            };
        } else {
            frame.spriteSourcePosition = Vector2{};
            frame.spriteSourceSize = Vector2{frame.source.width, frame.source.height};
        }

        frame.durationSeconds = JsonFloat(frameJson, "duration", 100.0f) / 1000.0f;
        if (frame.durationSeconds <= 0.0f) {
            frame.durationSeconds = 0.1f;
        }

        parsed.frames.push_back(frame);
        frameNames.push_back(frameName);
    }

    if (parsed.frames.empty()) {
        error = "Sprite animation JSON has no valid frames: " + jsonPath;
        return false;
    }

    if (document.contains("meta")
            && document["meta"].is_object()
            && document["meta"].contains("frameTags")
            && document["meta"]["frameTags"].is_array()) {
        const nlohmann::ordered_json& tags = document["meta"]["frameTags"];
        parsed.clips.reserve(tags.size());

        for (const nlohmann::ordered_json& tag : tags) {
            if (!tag.is_object()) {
                continue;
            }

            const uint32_t from = JsonUint(tag, "from", InvalidClipIndex);
            const uint32_t to = JsonUint(tag, "to", InvalidClipIndex);
            if (from == InvalidClipIndex || to == InvalidClipIndex || to < from || from >= parsed.frames.size()) {
                std::fprintf(stderr, "[AssetManager WARNING] Skipping invalid sprite frame tag in %s\n", jsonPath.c_str());
                continue;
            }

            const uint32_t clampedTo = std::min<uint32_t>(to, static_cast<uint32_t>(parsed.frames.size() - 1));
            SpriteClip clip;
            clip.name = tag.value("name", "");
            clip.firstFrame = from;
            clip.frameCount = clampedTo - from + 1;
            clip.playback = ParsePlaybackMode(tag.value("direction", "forward"));
            clip.repeat = ParseRepeat(tag);
            parsed.clips.push_back(std::move(clip));
        }
    }

    if (parsed.clips.empty()) {
        std::string currentClipName;
        uint32_t currentFirstFrame = 0;

        for (uint32_t i = 0; i < frameNames.size(); ++i) {
            const std::string clipName = ExtractClipNameFromFrameName(frameNames[i]);
            if (clipName.empty()) {
                continue;
            }

            if (currentClipName.empty()) {
                currentClipName = clipName;
                currentFirstFrame = i;
            } else if (clipName != currentClipName) {
                parsed.clips.push_back(SpriteClip{
                        currentClipName,
                        currentFirstFrame,
                        i - currentFirstFrame,
                        SpritePlaybackMode::Loop,
                        0
                });
                currentClipName = clipName;
                currentFirstFrame = i;
            }
        }

        if (!currentClipName.empty()) {
            parsed.clips.push_back(SpriteClip{
                    currentClipName,
                    currentFirstFrame,
                    static_cast<uint32_t>(frameNames.size()) - currentFirstFrame,
                    SpritePlaybackMode::Loop,
                    0
            });
        }
    }

    if (parsed.clips.empty()) {
        parsed.clips.push_back(SpriteClip{
                "Default",
                0,
                static_cast<uint32_t>(parsed.frames.size()),
                SpritePlaybackMode::Loop,
                0
        });
    }

    asset = std::move(parsed);
    return true;
}

std::string ExtractClipNameFromFrameName(const std::string& frameName)
{
    const size_t marker = frameName.find('#');
    if (marker == std::string::npos) {
        return "";
    }

    const size_t begin = marker + 1;
    size_t end = begin;
    while (end < frameName.size()
            && !std::isspace(static_cast<unsigned char>(frameName[end]))) {
        ++end;
    }

    return frameName.substr(begin, end - begin);
}

void SpriteAnimationAssets::OnScopeCreated(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        scopeData.resize(static_cast<size_t>(scope.index) + 1);
    }
}

SpriteAnimationAssets::RequestResult SpriteAnimationAssets::RequestSpriteAnimation(
        AssetScopeHandle scope,
        const char* key,
        const char* jsonPath,
        TextureLoadFlags textureFlags)
{
    RequestResult result;
    if (key == nullptr || jsonPath == nullptr) {
        return result;
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return result;
    }

    const std::string requestKey = MakeRequestKey(key, jsonPath, textureFlags);
    SpriteAnimationScopeData& data = scopeData[scope.index];
    const auto existing = data.animationByRequest.find(requestKey);
    if (existing != data.animationByRequest.end()) {
        result.handle = existing->second;
        return result;
    }

    assert(animationSlots.size() < std::numeric_limits<uint32_t>::max());
    SpriteAnimationSlot slot;
    slot.state = SpriteAnimationState::Queued;
    slot.key = key;
    slot.jsonPath = jsonPath;
    slot.textureFlags = textureFlags;
    slot.scope = scope;

    const uint32_t index = static_cast<uint32_t>(animationSlots.size());
    animationSlots.push_back(std::move(slot));

    result.handle = SpriteAnimationHandle{index, animationSlots[index].generation};
    result.shouldQueueJson = true;
    result.jsonPath = jsonPath;

    data.animations.push_back(result.handle);
    data.animationByRequest.emplace(requestKey, result.handle);

    return result;
}

bool SpriteAnimationAssets::IsReady(SpriteAnimationHandle handle, const TextureAssets& textureAssets) const
{
    TextureHandle atlasTexture = NullTextureHandle();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidAnimationNoLock(handle)
                || animationSlots[handle.index].state != SpriteAnimationState::Ready) {
            return false;
        }
        atlasTexture = animationSlots[handle.index].asset.atlasTexture;
    }

    return textureAssets.IsReady(atlasTexture);
}

bool SpriteAnimationAssets::IsFinished(SpriteAnimationHandle handle, const TextureAssets& textureAssets) const
{
    TextureHandle atlasTexture = NullTextureHandle();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidAnimationNoLock(handle)) {
            return true;
        }

        const SpriteAnimationSlot& slot = animationSlots[handle.index];
        if (!IsTerminal(slot.state)) {
            return false;
        }
        atlasTexture = slot.asset.atlasTexture;
    }

    return textureAssets.IsFinished(atlasTexture);
}

bool SpriteAnimationAssets::HasFailed(SpriteAnimationHandle handle, const TextureAssets& textureAssets) const
{
    TextureHandle atlasTexture = NullTextureHandle();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidAnimationNoLock(handle)) {
            return true;
        }

        const SpriteAnimationSlot& slot = animationSlots[handle.index];
        if (slot.state == SpriteAnimationState::Failed) {
            return true;
        }
        atlasTexture = slot.asset.atlasTexture;
    }

    return textureAssets.HasFailed(atlasTexture);
}

const SpriteAnimationAsset* SpriteAnimationAssets::Get(
        SpriteAnimationHandle handle,
        const TextureAssets& textureAssets) const
{
    TextureHandle atlasTexture = NullTextureHandle();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidAnimationNoLock(handle)
                || animationSlots[handle.index].state != SpriteAnimationState::Ready) {
            return nullptr;
        }

        atlasTexture = animationSlots[handle.index].asset.atlasTexture;
    }

    if (!textureAssets.IsReady(atlasTexture)) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidAnimationNoLock(handle)
            || animationSlots[handle.index].state != SpriteAnimationState::Ready) {
        return nullptr;
    }

    return &animationSlots[handle.index].asset;
}

uint32_t SpriteAnimationAssets::FindClipIndex(SpriteAnimationHandle handle, const char* clipName) const
{
    if (clipName == nullptr) {
        return InvalidClipIndex;
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidAnimationNoLock(handle)
            || animationSlots[handle.index].state != SpriteAnimationState::Ready) {
        return InvalidClipIndex;
    }

    const std::vector<SpriteClip>& clips = animationSlots[handle.index].asset.clips;
    for (uint32_t i = 0; i < clips.size(); ++i) {
        if (clips[i].name == clipName) {
            return i;
        }
    }

    return InvalidClipIndex;
}

const SpriteClip* SpriteAnimationAssets::GetClip(SpriteAnimationHandle handle, uint32_t clipIndex) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidAnimationNoLock(handle)
            || animationSlots[handle.index].state != SpriteAnimationState::Ready) {
        return nullptr;
    }

    const std::vector<SpriteClip>& clips = animationSlots[handle.index].asset.clips;
    if (clipIndex >= clips.size()) {
        return nullptr;
    }

    return &clips[clipIndex];
}

bool SpriteAnimationAssets::IsScopeReady(AssetScopeHandle scope, const TextureAssets& textureAssets) const
{
    std::vector<SpriteAnimationHandle> handles;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (scope.index >= scopeData.size()) {
            return false;
        }
        handles = scopeData[scope.index].animations;
    }

    for (SpriteAnimationHandle handle : handles) {
        if (!IsReady(handle, textureAssets)) {
            return false;
        }
    }

    return true;
}

bool SpriteAnimationAssets::IsScopeFinished(AssetScopeHandle scope, const TextureAssets& textureAssets) const
{
    std::vector<SpriteAnimationHandle> handles;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (scope.index >= scopeData.size()) {
            return false;
        }
        handles = scopeData[scope.index].animations;
    }

    for (SpriteAnimationHandle handle : handles) {
        if (!IsFinished(handle, textureAssets)) {
            return false;
        }
    }

    return true;
}

void SpriteAnimationAssets::GetScopeProgress(
        AssetScopeHandle scope,
        const TextureAssets& textureAssets,
        size_t& finished,
        size_t& total) const
{
    std::vector<SpriteAnimationHandle> handles;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (scope.index >= scopeData.size()) {
            return;
        }
        handles = scopeData[scope.index].animations;
    }

    total += handles.size();
    for (SpriteAnimationHandle handle : handles) {
        if (IsFinished(handle, textureAssets)) {
            ++finished;
        }
    }
}

TextureAssets::RequestResult SpriteAnimationAssets::ProcessSpriteAnimationRequestOnWorkerThread(
        SpriteAnimationHandle handle,
        const std::string& jsonPath,
        TextureAssets& textureAssets)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidAnimationNoLock(handle)
                || animationSlots[handle.index].state == SpriteAnimationState::QueuedForUnload) {
            return TextureAssets::RequestResult{};
        }

        SpriteAnimationSlot& slot = animationSlots[handle.index];
        slot.state = SpriteAnimationState::Loading;
    }

    SpriteAnimationAsset parsed;
    std::string atlasPath;
    std::string error;
    const bool success = ParseSpriteAnimationJson(jsonPath, parsed, atlasPath, error);
    if (!success) {
        std::fprintf(stderr, "[AssetManager WARNING] %s\n", error.c_str());
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidAnimationNoLock(handle)) {
            return TextureAssets::RequestResult{};
        }

        SpriteAnimationSlot& slot = animationSlots[handle.index];
        if (slot.state == SpriteAnimationState::QueuedForUnload) {
            return TextureAssets::RequestResult{};
        }

        slot.state = SpriteAnimationState::Failed;
        slot.error = error;
        return TextureAssets::RequestResult{};
    }

    std::string textureKey;
    TextureLoadFlags textureFlags = TextureLoad_PointFilter;
    AssetScopeHandle scope = NullAssetScopeHandle();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidAnimationNoLock(handle)
                || animationSlots[handle.index].state == SpriteAnimationState::QueuedForUnload) {
            return TextureAssets::RequestResult{};
        }

        const SpriteAnimationSlot& slot = animationSlots[handle.index];
        textureKey = slot.key;
        textureFlags = slot.textureFlags;
        scope = slot.scope;
    }

    TextureAssets::RequestResult textureRequest = textureAssets.RequestTexture(
            scope,
            textureKey.c_str(),
            atlasPath.c_str(),
            textureFlags
    );

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (!IsValidAnimationNoLock(handle)) {
            return TextureAssets::RequestResult{};
        }

        SpriteAnimationSlot& slot = animationSlots[handle.index];
        if (slot.state == SpriteAnimationState::QueuedForUnload) {
            return TextureAssets::RequestResult{};
        }

        if (IsNull(textureRequest.handle)) {
            slot.state = SpriteAnimationState::Failed;
            slot.error = "Failed to request sprite animation atlas texture: " + atlasPath;
            return TextureAssets::RequestResult{};
        }

        parsed.atlasTexture = textureRequest.handle;
        slot.asset = std::move(parsed);
        slot.state = SpriteAnimationState::Ready;
        slot.error.clear();
    }

    return textureRequest;
}

void SpriteAnimationAssets::UnloadScope(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return;
    }

    SpriteAnimationScopeData& data = scopeData[scope.index];
    for (SpriteAnimationHandle handle : data.animations) {
        if (!IsValidAnimationNoLock(handle)) {
            continue;
        }

        SpriteAnimationSlot& slot = animationSlots[handle.index];
        slot.state = SpriteAnimationState::QueuedForUnload;
        slot.asset = SpriteAnimationAsset{};
        ++slot.generation;
    }

    data.animations.clear();
    data.animationByRequest.clear();
}

void SpriteAnimationAssets::Shutdown()
{
    std::lock_guard<std::mutex> lock(stateMutex);
    animationSlots.clear();
    scopeData.clear();
}

bool SpriteAnimationAssets::IsValidAnimationNoLock(SpriteAnimationHandle handle) const
{
    return handle.index < animationSlots.size()
        && animationSlots[handle.index].generation == handle.generation;
}

bool SpriteAnimationAssets::IsTerminal(SpriteAnimationState state)
{
    return state == SpriteAnimationState::Ready
        || state == SpriteAnimationState::Failed
        || state == SpriteAnimationState::Unloaded
        || state == SpriteAnimationState::QueuedForUnload;
}

std::string SpriteAnimationAssets::MakeRequestKey(
        const char* key,
        const char* jsonPath,
        TextureLoadFlags textureFlags)
{
    return std::string(key)
        + "\n" + jsonPath
        + "\n" + std::to_string(static_cast<uint32_t>(textureFlags));
}

} // namespace engine
