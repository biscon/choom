#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/FontLoadFlags.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

enum class FontState {
    Unloaded,
    Queued,
    Ready,
    Failed,
    QueuedForUnload
};

struct FontAsset {
    Font font = {};
    int pixelSize = 0;
    FontLoadFlags flags = FontLoad_BilinearFilter;
};

class FontAssets {
public:
    void OnScopeCreated(AssetScopeHandle scope);

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

    bool IsScopeReady(AssetScopeHandle scope) const;
    bool IsScopeFinished(AssetScopeHandle scope) const;
    void GetScopeProgress(AssetScopeHandle scope, size_t& finished, size_t& total) const;

    void UpdateMainThread(float maxMilliseconds);
    void UnloadScope(AssetScopeHandle scope);
    void ShutdownMainThread();

private:
    struct FontSlot {
        uint32_t generation = 1;
        FontState state = FontState::Unloaded;
        std::string key;
        std::string path;
        int pixelSize = 0;
        FontLoadFlags flags = FontLoad_BilinearFilter;
        AssetScopeHandle scope;
        FontAsset asset;
        std::string error;
    };

    struct FontScopeData {
        std::vector<FontHandle> fonts;
        std::unordered_map<std::string, FontHandle> fontByRequest;
    };

    bool IsValidFontNoLock(FontHandle handle) const;
    static bool IsTerminal(FontState state);
    static bool IsFontValid(const Font& font);
    static std::string MakeRequestKey(
            const char* key,
            const char* path,
            int pixelSize,
            FontLoadFlags flags
    );

    void QueueFontUnloadNoLock(FontHandle handle);
    void UnloadReadyFonts();

    mutable std::mutex stateMutex;
    std::vector<FontSlot> fontSlots;
    std::vector<FontScopeData> scopeData;
    std::deque<FontHandle> pendingLoads;
    std::vector<Font> pendingUnloads;
};

} // namespace engine
