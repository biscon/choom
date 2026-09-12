#pragma once
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
namespace game
{
// Continuous separating-axis sweep of a translating oriented box against a
// convex polygon (a two-point polygon is a wall). No frame-time allocation.
inline float SweepSectorBoxPolygon(Vector2 center, Vector2 axisX, Vector2 axisZ, Vector2 half,
                                   Vector2 delta, const Vector2 *points, size_t count)
{
    if (count < 2)
        return 1.0f;
    float enter = -std::numeric_limits<float>::infinity();
    float leave = std::numeric_limits<float>::infinity();
    const auto axis = [&](Vector2 n)
    {
        const float len = Vector2Length(n);
        if (len < 0.000001f)
            return true;
        n = Vector2Scale(n, 1.0f / len);
        float low = Vector2DotProduct(points[0], n), high = low;
        for (size_t i = 1; i < count; ++i)
        {
            const float d = Vector2DotProduct(points[i], n);
            low = std::min(low, d);
            high = std::max(high, d);
        }
        const float radius = half.x * std::abs(Vector2DotProduct(axisX, n)) +
                             half.y * std::abs(Vector2DotProduct(axisZ, n));
        const float origin = Vector2DotProduct(center, n);
        low -= radius;
        high += radius;
        const float velocity = Vector2DotProduct(delta, n);
        if (std::abs(velocity) < 0.0000001f)
            return origin > low + 0.00001f && origin < high - 0.00001f;
        float a = (low - origin) / velocity, b = (high - origin) / velocity;
        if (a > b)
            std::swap(a, b);
        enter = std::max(enter, a);
        leave = std::min(leave, b);
        return enter < leave;
    };
    if (!axis(axisX) || !axis(axisZ))
        return 1.0f;
    for (size_t i = 0; i < count; ++i)
    {
        const Vector2 edge = Vector2Subtract(points[(i + 1) % count], points[i]);
        if (!axis({-edge.y, edge.x}))
            return 1.0f;
    }
    if (leave <= 0.000001f || enter >= 1.0f)
        return 1.0f;
    const float skinFraction = 0.0001f / std::max(Vector2Length(delta), 0.0001f);
    return std::clamp(enter - skinFraction, 0.0f, 1.0f);
}
inline void SectorBoxCorners(Vector2 center, Vector2 x, Vector2 z, Vector2 half, Vector2 (&out)[4])
{
    x = Vector2Scale(x, half.x);
    z = Vector2Scale(z, half.y);
    out[0] = Vector2Subtract(Vector2Subtract(center, x), z);
    out[1] = Vector2Add(Vector2Subtract(center, z), x);
    out[2] = Vector2Add(Vector2Add(center, x), z);
    out[3] = Vector2Add(Vector2Subtract(center, x), z);
}
// Circle against polygon edges and rounded endpoints. Shared by constrained
// player movement so the player keeps the normal circular footprint.
inline float SweepSectorCirclePolygon(Vector2 center, float radius, Vector2 delta,
                                      const Vector2 *points, size_t count)
{
    float earliest = 1.0f;
    const float speed2 = Vector2LengthSqr(delta);
    if (count < 2 || speed2 < 0.00000001f)
        return earliest;
    const auto record = [&](float t, Vector2 normal)
    {
        if (t >= -0.000001f && t < earliest && Vector2DotProduct(delta, normal) < -0.000001f)
            earliest = std::max(0.0f, t);
    };
    for (size_t i = 0; i < count; ++i)
    {
        const Vector2 a = points[i], b = points[(i + 1) % count];
        const Vector2 edge = Vector2Subtract(b, a), relative = Vector2Subtract(center, a);
        const float length = Vector2Length(edge);
        if (length < 0.000001f)
            continue;
        const Vector2 u = Vector2Scale(edge, 1.0f / length), normal{-u.y, u.x};
        const float velocity = Vector2DotProduct(delta, normal),
                    distance = Vector2DotProduct(relative, normal);
        if (std::abs(velocity) > 0.0000001f)
            for (float sign : {-1.0f, 1.0f})
            {
                const float t = (sign * radius - distance) / velocity;
                const float along =
                    Vector2DotProduct(Vector2Add(relative, Vector2Scale(delta, t)), u);
                if (along >= 0 && along <= length)
                    record(t, Vector2Scale(normal, sign));
            }
        const float projected = Vector2DotProduct(relative, delta);
        const float discriminant =
            projected * projected - speed2 * (Vector2LengthSqr(relative) - radius * radius);
        if (discriminant >= 0)
        {
            const float t = (-projected - std::sqrt(discriminant)) / speed2;
            record(t, Vector2Add(relative, Vector2Scale(delta, t)));
        }
    }
    return earliest < 1 ? std::max(0.0f, earliest - 0.0001f / std::sqrt(speed2)) : 1.0f;
}

} // namespace game
