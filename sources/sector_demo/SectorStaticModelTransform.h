#pragma once

#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <limits>

namespace game {

// Local bounds already include the imported model transform. Apply only the
// authored instance transform, consistently in rendering and bake preparation.
inline BoundingBox TransformSectorStaticModelBounds(BoundingBox localBounds, Matrix authored)
{
    const float infinity = std::numeric_limits<float>::infinity();
    BoundingBox result{{infinity, infinity, infinity}, {-infinity, -infinity, -infinity}};
    for (float x : {localBounds.min.x, localBounds.max.x}) {
        for (float y : {localBounds.min.y, localBounds.max.y}) {
            for (float z : {localBounds.min.z, localBounds.max.z}) {
                const Vector3 p = Vector3Transform({x, y, z}, authored);
                result.min = {std::min(result.min.x, p.x), std::min(result.min.y, p.y), std::min(result.min.z, p.z)};
                result.max = {std::max(result.max.x, p.x), std::max(result.max.y, p.y), std::max(result.max.z, p.z)};
            }
        }
    }
    return result;
}

inline Matrix BuildSectorStaticModelRotation(
        float rotationXRadians,
        float yawRadians,
        float rotationZRadians)
{
    return MatrixRotateXYZ(Vector3{
            rotationXRadians,
            yawRadians,
            rotationZRadians});
}

inline Matrix BuildSectorStaticModelAuthoredTransform(
        Vector3 worldPosition,
        float rotationXRadians,
        float yawRadians,
        float rotationZRadians,
        float scale)
{
    return MatrixMultiply(
            MatrixScale(scale, scale, scale),
            MatrixMultiply(
                    BuildSectorStaticModelRotation(
                            rotationXRadians,
                            yawRadians,
                            rotationZRadians),
                    MatrixTranslate(
                            worldPosition.x,
                            worldPosition.y,
                            worldPosition.z)));
}

inline Vector3 RotateSectorStaticModelDirection(
        Vector3 direction,
        float rotationXRadians,
        float yawRadians,
        float rotationZRadians)
{
    return Vector3Transform(
            direction,
            BuildSectorStaticModelRotation(
                    rotationXRadians,
                    yawRadians,
                    rotationZRadians));
}

} // namespace game
