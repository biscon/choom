#pragma once

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

inline bool IntersectSectorEditorLightProxyPlacementPlane(
        Ray ray,
        Vector3 planePoint,
        Vector3 planeNormal,
        Vector3& outIntersection)
{
    const float denominator = Vector3DotProduct(ray.direction, planeNormal);
    if (!std::isfinite(denominator) || std::fabs(denominator) <= 0.000001f) {
        return false;
    }
    const float distance = Vector3DotProduct(
            Vector3Subtract(planePoint, ray.position), planeNormal) / denominator;
    if (!std::isfinite(distance) || distance < 0.0f) return false;
    outIntersection = Vector3Add(ray.position, Vector3Scale(ray.direction, distance));
    return std::isfinite(outIntersection.x)
            && std::isfinite(outIntersection.y)
            && std::isfinite(outIntersection.z);
}

inline Vector3 ApplySectorEditorLightProxyPlacementDrag(
        Vector3 dragStartCenter,
        Vector3 dragStartIntersection,
        Vector3 currentIntersection,
        bool precision)
{
    const float scale = precision ? 0.1f : 1.0f;
    return Vector3Add(
            dragStartCenter,
            Vector3Scale(
                    Vector3Subtract(currentIntersection, dragStartIntersection),
                    scale));
}

inline Vector3 ApplySectorEditorLightProxyPlacementDepth(
        Vector3 center,
        Vector3 cameraPosition,
        Vector3 cameraForward,
        float wheelDelta,
        bool precision)
{
    const float distance = Vector3Distance(center, cameraPosition);
    float stepWorld = std::max(0.01f, distance * 0.02f);
    if (precision) stepWorld *= 0.1f;
    return Vector3Add(
            center,
            Vector3Scale(cameraForward, -wheelDelta * stepWorld));
}

} // namespace game
