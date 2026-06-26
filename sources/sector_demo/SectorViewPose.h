#pragma once

#include <raylib.h>

namespace game {

struct SectorViewPose {
    Vector3 position = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

} // namespace game
