#pragma once

#include <cstdint>

namespace engine {

enum FontLoadFlags : uint32_t {
    FontLoad_None = 0,
    FontLoad_PointFilter = 1 << 0,
    FontLoad_BilinearFilter = 1 << 1
};

inline FontLoadFlags operator|(FontLoadFlags lhs, FontLoadFlags rhs)
{
    return static_cast<FontLoadFlags>(
            static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs)
    );
}

inline bool HasFlag(FontLoadFlags flags, FontLoadFlags flag)
{
    return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
}

} // namespace engine
