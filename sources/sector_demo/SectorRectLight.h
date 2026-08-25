#pragma once

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

struct SectorRectLightBasis {
    Vector3 forward = {0.0f, -1.0f, 0.0f};
    Vector3 right = {1.0f, 0.0f, 0.0f};
    Vector3 up = {0.0f, 0.0f, 1.0f};
};

inline SectorRectLightBasis BuildSectorRectLightBasis(
        Vector3 position,
        Vector3 target,
        float rollDegrees)
{
    Vector3 forward = Vector3Subtract(target, position);
    forward = Vector3LengthSqr(forward) > 0.00000001f
            ? Vector3Normalize(forward)
            : Vector3{0.0f, -1.0f, 0.0f};
    const Vector3 reference = std::fabs(Vector3DotProduct(
            forward, Vector3{0.0f, 1.0f, 0.0f})) > 0.98f
            ? Vector3{0.0f, 0.0f, 1.0f}
            : Vector3{0.0f, 1.0f, 0.0f};
    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, reference));
    Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float roll = std::isfinite(rollDegrees) ? rollDegrees * DEG2RAD : 0.0f;
    const float cosine = std::cos(roll);
    const float sine = std::sin(roll);
    const Vector3 rolledRight = Vector3Add(
            Vector3Scale(right, cosine), Vector3Scale(up, sine));
    const Vector3 rolledUp = Vector3Subtract(
            Vector3Scale(up, cosine), Vector3Scale(right, sine));
    return SectorRectLightBasis{forward, rolledRight, rolledUp};
}

inline Vector3 ClosestPointOnSectorRectLight(
        Vector3 point,
        Vector3 center,
        const SectorRectLightBasis& basis,
        float width,
        float height)
{
    const Vector3 offset = Vector3Subtract(point, center);
    const float halfWidth = std::max(width, 0.0f) * 0.5f;
    const float halfHeight = std::max(height, 0.0f) * 0.5f;
    const float x = std::clamp(
            Vector3DotProduct(offset, basis.right), -halfWidth, halfWidth);
    const float y = std::clamp(
            Vector3DotProduct(offset, basis.up), -halfHeight, halfHeight);
    return Vector3Add(
            center,
            Vector3Add(Vector3Scale(basis.right, x), Vector3Scale(basis.up, y)));
}

inline float SectorRectLightStartFeatherAttenuation(
        float forwardDistance,
        float startFeather)
{
    if (!(startFeather > 0.000001f) || !std::isfinite(startFeather)) {
        return forwardDistance >= 0.0f ? 1.0f : 0.0f;
    }
    if (!(forwardDistance > 0.0f)) return 0.0f;
    const float t = std::clamp(forwardDistance / startFeather, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace game
