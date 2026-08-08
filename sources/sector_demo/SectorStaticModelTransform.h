#pragma once

#include <raylib.h>
#include <raymath.h>

namespace game {

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
