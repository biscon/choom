#pragma once

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

namespace game {

constexpr int SectorReflectionProbeBakeVersion = 1;

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
};

struct SectorBakedReflectionProbeMetadata {
    std::string path;
    int version = 0;
    int count = 0;
    std::string format;
};

struct SectorBakedReflectionProbeRecord {
    int probeId = -1;
    int resolution = 0;
    int mipCount = 0;
    std::string sourceHash;
    // Face-major RGBA16F mip data: +X, -X, +Y, -Y, +Z, -Z for every mip.
    std::vector<std::uint16_t> rgba16;
};

struct SectorBakedReflectionProbeArtifact {
    int version = 0;
    std::vector<SectorBakedReflectionProbeRecord> probes;
};

} // namespace game
