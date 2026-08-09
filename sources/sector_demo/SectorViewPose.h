#pragma once

#include <raylib.h>

#include <cmath>

namespace game {

struct SectorViewPose {
    Vector3 position = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
    float rollRadians = 0.0f;
};

inline Vector3 SectorViewForward(const SectorViewPose& pose)
{
    const float cosPitch = std::cos(pose.pitchRadians);
    return Vector3{
            std::cos(pose.yawRadians) * cosPitch,
            std::sin(pose.pitchRadians),
            std::sin(pose.yawRadians) * cosPitch};
}

inline Vector3 SectorViewUp(const SectorViewPose& pose)
{
    const float cosYaw = std::cos(pose.yawRadians);
    const float sinYaw = std::sin(pose.yawRadians);
    const float cosPitch = std::cos(pose.pitchRadians);
    const float sinPitch = std::sin(pose.pitchRadians);
    const float cosRoll = std::cos(pose.rollRadians);
    const float sinRoll = std::sin(pose.rollRadians);
    const Vector3 baseUp{-cosYaw * sinPitch, cosPitch, -sinYaw * sinPitch};
    const Vector3 right{-sinYaw, 0.0f, cosYaw};
    return Vector3{
            baseUp.x * cosRoll + right.x * sinRoll,
            baseUp.y * cosRoll + right.y * sinRoll,
            baseUp.z * cosRoll + right.z * sinRoll};
}

} // namespace game
