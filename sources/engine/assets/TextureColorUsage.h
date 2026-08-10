#pragma once

#include "engine/assets/TextureLoadFlags.h"

namespace engine {

enum class TextureColorUsage {
    SceneSrgb,
    LinearData,
    DisplaySrgb,
    Count
};

inline const char* TextureColorUsageName(TextureColorUsage usage)
{
    switch (usage) {
        case TextureColorUsage::SceneSrgb: return "scene-sRGB";
        case TextureColorUsage::LinearData: return "linear-data";
        case TextureColorUsage::DisplaySrgb: return "display-sRGB";
        case TextureColorUsage::Count: break;
    }
    return "invalid";
}

inline bool IsValidTextureColorUsage(TextureColorUsage usage)
{
    return usage == TextureColorUsage::SceneSrgb
            || usage == TextureColorUsage::LinearData
            || usage == TextureColorUsage::DisplaySrgb;
}

inline bool IsValidTextureRequestDescriptor(
        TextureColorUsage usage,
        TextureLoadFlags flags)
{
    if (!IsValidTextureColorUsage(usage)) {
        return false;
    }

    constexpr uint32_t KnownFlags = static_cast<uint32_t>(TextureLoad_PremultiplyAlpha)
            | static_cast<uint32_t>(TextureLoad_PointFilter)
            | static_cast<uint32_t>(TextureLoad_BilinearFilter)
            | static_cast<uint32_t>(TextureLoad_Mipmaps)
            | static_cast<uint32_t>(TextureLoad_TrilinearFilter)
            | static_cast<uint32_t>(TextureLoad_Anisotropic8x);
    const uint32_t flagBits = static_cast<uint32_t>(flags);
    if ((flagBits & ~KnownFlags) != 0) {
        return false;
    }

    constexpr uint32_t FilterFlags = static_cast<uint32_t>(TextureLoad_PointFilter)
            | static_cast<uint32_t>(TextureLoad_BilinearFilter)
            | static_cast<uint32_t>(TextureLoad_TrilinearFilter)
            | static_cast<uint32_t>(TextureLoad_Anisotropic8x);
    const uint32_t filterBits = flagBits & FilterFlags;
    return filterBits == 0 || (filterBits & (filterBits - 1)) == 0;
}

} // namespace engine
