#include "engine/assets/FontAssets.h"

#include <raylib.h>

#include <cassert>
#include <cstdio>
#include <limits>
#include <utility>

namespace engine {

void FontAssets::OnScopeCreated(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);

    if (scope.index >= scopeData.size()) {
        scopeData.resize(static_cast<size_t>(scope.index) + 1);
    }
}

FontHandle FontAssets::RequestFont(
        AssetScopeHandle scope,
        const char* key,
        const char* path,
        int pixelSize,
        FontLoadFlags flags)
{
    if (key == nullptr || path == nullptr || pixelSize <= 0) {
        return NullFontHandle();
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return NullFontHandle();
    }

    const std::string requestKey = MakeRequestKey(key, path, pixelSize, flags);
    FontScopeData& data = scopeData[scope.index];
    const auto existing = data.fontByRequest.find(requestKey);
    if (existing != data.fontByRequest.end()) {
        return existing->second;
    }

    assert(fontSlots.size() < std::numeric_limits<uint32_t>::max());
    FontSlot slot;
    slot.state = FontState::Queued;
    slot.key = key;
    slot.path = path;
    slot.pixelSize = pixelSize;
    slot.flags = flags;
    slot.scope = scope;
    slot.asset.pixelSize = pixelSize;
    slot.asset.flags = flags;

    const uint32_t index = static_cast<uint32_t>(fontSlots.size());
    fontSlots.push_back(std::move(slot));

    FontHandle handle{index, fontSlots[index].generation};
    data.fonts.push_back(handle);
    data.fontByRequest.emplace(requestKey, handle);
    pendingLoads.push_back(handle);

    return handle;
}

bool FontAssets::IsReady(FontHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return IsValidFontNoLock(handle)
        && fontSlots[handle.index].state == FontState::Ready;
}

bool FontAssets::IsFinished(FontHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidFontNoLock(handle)
        || IsTerminal(fontSlots[handle.index].state);
}

bool FontAssets::HasFailed(FontHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidFontNoLock(handle)
        || fontSlots[handle.index].state == FontState::Failed;
}

const FontAsset* FontAssets::GetFont(FontHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidFontNoLock(handle)) {
        return nullptr;
    }

    const FontSlot& slot = fontSlots[handle.index];
    if (slot.state != FontState::Ready || !IsFontValid(slot.asset.font)) {
        return nullptr;
    }

    return &slot.asset;
}

bool FontAssets::IsScopeReady(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return false;
    }

    const FontScopeData& data = scopeData[scope.index];
    for (FontHandle handle : data.fonts) {
        if (!IsValidFontNoLock(handle)
                || fontSlots[handle.index].state != FontState::Ready) {
            return false;
        }
    }

    return true;
}

bool FontAssets::IsScopeFinished(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return false;
    }

    const FontScopeData& data = scopeData[scope.index];
    for (FontHandle handle : data.fonts) {
        if (IsValidFontNoLock(handle)
                && !IsTerminal(fontSlots[handle.index].state)) {
            return false;
        }
    }

    return true;
}

void FontAssets::GetScopeProgress(AssetScopeHandle scope, size_t& finished, size_t& total) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return;
    }

    const FontScopeData& data = scopeData[scope.index];
    total += data.fonts.size();
    for (FontHandle handle : data.fonts) {
        if (!IsValidFontNoLock(handle)
                || IsTerminal(fontSlots[handle.index].state)) {
            ++finished;
        }
    }
}

void FontAssets::UpdateMainThread(float)
{
    while (true) {
        FontHandle handle = NullFontHandle();
        std::string path;
        int pixelSize = 0;
        FontLoadFlags flags = FontLoad_BilinearFilter;

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (pendingLoads.empty()) {
                break;
            }

            handle = pendingLoads.front();
            pendingLoads.pop_front();

            if (!IsValidFontNoLock(handle)
                    || fontSlots[handle.index].state == FontState::QueuedForUnload) {
                continue;
            }

            const FontSlot& slot = fontSlots[handle.index];
            path = slot.path;
            pixelSize = slot.pixelSize;
            flags = slot.flags;
        }

        // LoadFontEx performs file IO and uploads the atlas texture as one blocking
        // main-thread operation, so this first font path does not split work by budget.
        Font loaded = LoadFontEx(path.c_str(), pixelSize, nullptr, 0);
        const bool loadedFont = IsFontValid(loaded);

        if (loadedFont) {
            SetTextureFilter(
                    loaded.texture,
                    HasFlag(flags, FontLoad_PointFilter)
                        ? TEXTURE_FILTER_POINT
                        : TEXTURE_FILTER_BILINEAR
            );
        } else {
            std::fprintf(stderr, "[AssetManager WARNING] Failed to load font: %s\n", path.c_str());
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (!IsValidFontNoLock(handle)) {
                if (loadedFont) {
                    pendingUnloads.push_back(loaded);
                }
                continue;
            }

            FontSlot& slot = fontSlots[handle.index];
            if (slot.state == FontState::QueuedForUnload) {
                if (loadedFont) {
                    pendingUnloads.push_back(loaded);
                }
                continue;
            }

            if (loadedFont) {
                slot.asset.font = loaded;
                slot.asset.pixelSize = pixelSize;
                slot.asset.flags = flags;
                slot.state = FontState::Ready;
                slot.error.clear();
            } else {
                slot.state = FontState::Failed;
                slot.error = "Failed to load font: " + path;
            }
        }
    }

    UnloadReadyFonts();
}

void FontAssets::UnloadScope(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return;
    }

    FontScopeData& data = scopeData[scope.index];
    for (FontHandle handle : data.fonts) {
        QueueFontUnloadNoLock(handle);
    }

    data.fonts.clear();
    data.fontByRequest.clear();
}

void FontAssets::ShutdownMainThread()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        pendingLoads.clear();
        for (FontSlot& slot : fontSlots) {
            if ((slot.state == FontState::Ready || slot.state == FontState::QueuedForUnload)
                    && IsFontValid(slot.asset.font)) {
                pendingUnloads.push_back(slot.asset.font);
                slot.asset.font = {};
            }
            slot.state = FontState::Unloaded;
        }
    }

    UnloadReadyFonts();

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        fontSlots.clear();
        scopeData.clear();
        pendingLoads.clear();
        pendingUnloads.clear();
    }
}

bool FontAssets::IsValidFontNoLock(FontHandle handle) const
{
    return handle.index < fontSlots.size()
        && fontSlots[handle.index].generation == handle.generation;
}

bool FontAssets::IsTerminal(FontState state)
{
    return state == FontState::Ready
        || state == FontState::Failed
        || state == FontState::Unloaded
        || state == FontState::QueuedForUnload;
}

bool FontAssets::IsFontValid(const Font& font)
{
    return font.texture.id != 0
        && font.glyphCount > 0
        && font.glyphs != nullptr
        && font.recs != nullptr;
}

std::string FontAssets::MakeRequestKey(
        const char* key,
        const char* path,
        int pixelSize,
        FontLoadFlags flags)
{
    return std::string(key)
        + "\n" + path
        + "\n" + std::to_string(pixelSize)
        + "\n" + std::to_string(static_cast<uint32_t>(flags));
}

void FontAssets::QueueFontUnloadNoLock(FontHandle handle)
{
    if (!IsValidFontNoLock(handle)) {
        return;
    }

    FontSlot& slot = fontSlots[handle.index];
    if (IsFontValid(slot.asset.font)) {
        pendingUnloads.push_back(slot.asset.font);
        slot.asset.font = {};
    }

    slot.state = FontState::QueuedForUnload;
    ++slot.generation;
}

void FontAssets::UnloadReadyFonts()
{
    std::vector<Font> unloads;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        unloads.swap(pendingUnloads);
    }

    for (Font font : unloads) {
        if (IsFontValid(font)) {
            UnloadFont(font);
        }
    }
}

} // namespace engine
