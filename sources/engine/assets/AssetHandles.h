#pragma once

#include <cstdint>

namespace engine {

struct TextureHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

inline bool operator==(TextureHandle lhs, TextureHandle rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

inline bool operator!=(TextureHandle lhs, TextureHandle rhs)
{
    return !(lhs == rhs);
}

inline TextureHandle NullTextureHandle()
{
    return TextureHandle{};
}

inline bool IsNull(TextureHandle handle)
{
    return handle.index == UINT32_MAX;
}

struct SpriteAnimationHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

inline bool operator==(SpriteAnimationHandle lhs, SpriteAnimationHandle rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

inline bool operator!=(SpriteAnimationHandle lhs, SpriteAnimationHandle rhs)
{
    return !(lhs == rhs);
}

inline SpriteAnimationHandle NullSpriteAnimationHandle()
{
    return SpriteAnimationHandle{};
}

inline bool IsNull(SpriteAnimationHandle handle)
{
    return handle.index == UINT32_MAX;
}

struct FontHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

inline bool operator==(FontHandle lhs, FontHandle rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

inline bool operator!=(FontHandle lhs, FontHandle rhs)
{
    return !(lhs == rhs);
}

inline FontHandle NullFontHandle()
{
    return FontHandle{};
}

inline bool IsNull(FontHandle handle)
{
    return handle.index == UINT32_MAX;
}

struct AssetScopeHandle {
    uint32_t index = UINT32_MAX;
    uint32_t generation = 0;
};

inline bool operator==(AssetScopeHandle lhs, AssetScopeHandle rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

inline bool operator!=(AssetScopeHandle lhs, AssetScopeHandle rhs)
{
    return !(lhs == rhs);
}

inline AssetScopeHandle NullAssetScopeHandle()
{
    return AssetScopeHandle{};
}

inline bool IsNull(AssetScopeHandle handle)
{
    return handle.index == UINT32_MAX;
}

} // namespace engine
