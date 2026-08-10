#pragma once

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace game {

inline float ClampUnitFloat(float value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

inline unsigned char ClampColorByte(float value)
{
    return static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(value)), 0, 255));
}

inline Color UnitRgbToColor(Vector3 rgb, unsigned char alpha = 255)
{
    return Color{
            ClampColorByte(ClampUnitFloat(rgb.x) * 255.0f),
            ClampColorByte(ClampUnitFloat(rgb.y) * 255.0f),
            ClampColorByte(ClampUnitFloat(rgb.z) * 255.0f),
            alpha};
}

} // namespace game
