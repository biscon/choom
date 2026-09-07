#pragma once

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct SectorCompiledReflectionProbe {
    int sourceAuthoringProbeId = -1;
    int topologySectorId = -1;
    bool enabled = true;
    Vector3 capturePositionWorld = {};
    Vector3 influenceCenterWorld = {};
    Vector3 halfExtentsWorld = {2.0f, 1.5f, 2.0f};
    float yawRadians = 0.0f;
    int priority = 0;
    float intensity = 1.0f;
    int resolution = 128;
    float blendDistanceWorld = 0.5f;
};

} // namespace game
