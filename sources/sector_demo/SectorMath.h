#pragma once

#include <raylib.h>

#include <algorithm>
#include <cmath>

namespace game {

inline bool IsFiniteFloat(float value)
{
    return std::isfinite(value);
}

inline bool IsFiniteVector2(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

inline bool IsFiniteVector3(Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline bool IsFiniteMatrix(Matrix matrix)
{
    return std::isfinite(matrix.m0) && std::isfinite(matrix.m1) && std::isfinite(matrix.m2) && std::isfinite(matrix.m3)
            && std::isfinite(matrix.m4) && std::isfinite(matrix.m5) && std::isfinite(matrix.m6) && std::isfinite(matrix.m7)
            && std::isfinite(matrix.m8) && std::isfinite(matrix.m9) && std::isfinite(matrix.m10) && std::isfinite(matrix.m11)
            && std::isfinite(matrix.m12) && std::isfinite(matrix.m13) && std::isfinite(matrix.m14) && std::isfinite(matrix.m15);
}

inline float ClampFinite(float value, float minValue, float maxValue, float fallback)
{
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

inline Vector2 NormalizeVector2OrFallback(Vector2 value, Vector2 fallback, float minLengthSquared = 0.000001f)
{
    const float lengthSquared = value.x * value.x + value.y * value.y;
    if (lengthSquared <= minLengthSquared || !std::isfinite(lengthSquared)) {
        return fallback;
    }

    const float invLength = 1.0f / std::sqrt(lengthSquared);
    return Vector2{value.x * invLength, value.y * invLength};
}

inline Vector3 NormalizeVector3OrFallback(Vector3 value, Vector3 fallback, float minLengthSquared = 0.000001f)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (lengthSquared <= minLengthSquared || !std::isfinite(lengthSquared)) {
        return fallback;
    }

    const float invLength = 1.0f / std::sqrt(lengthSquared);
    return Vector3{value.x * invLength, value.y * invLength, value.z * invLength};
}

inline float SmoothStep01(float t)
{
    t = std::isfinite(t) ? std::clamp(t, 0.0f, 1.0f) : 0.0f;
    return t * t * (3.0f - 2.0f * t);
}

inline float SmootherStep01(float t)
{
    t = std::isfinite(t) ? std::clamp(t, 0.0f, 1.0f) : 0.0f;
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

} // namespace game
