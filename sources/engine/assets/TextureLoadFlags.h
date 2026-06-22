#pragma once

#include <cstdint>

namespace engine {

enum TextureLoadFlags : uint32_t {
    TextureLoad_None = 0,
    TextureLoad_PremultiplyAlpha = 1 << 0,
    TextureLoad_PointFilter = 1 << 1,
    TextureLoad_BilinearFilter = 1 << 2,
    TextureLoad_Mipmaps = 1 << 3,
    TextureLoad_TrilinearFilter = 1 << 4,
    TextureLoad_Anisotropic8x = 1 << 5
};

inline TextureLoadFlags operator|(TextureLoadFlags lhs, TextureLoadFlags rhs)
{
    return static_cast<TextureLoadFlags>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)
    );
}

inline bool HasFlag(TextureLoadFlags flags, TextureLoadFlags flag)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

} // namespace engine
