#pragma once

#include "game/navigation/SectorNavigationTypes.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorTopologyMap.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct SectorNavigationBuildTriangle {
    Vector3 a = {};
    Vector3 b = {};
    Vector3 c = {};
    uint8_t area = 0;
    int sourceId = 0;
};

struct SectorNavigationBuildDoorLink {
    int placedObjectId = 0;
    Vector3 frontStage = {};
    Vector3 backStage = {};
    float radius = 0.25f;
};

struct SectorNavigationBuildInput {
    std::vector<SectorNavigationBuildTriangle> triangles;
    std::vector<SectorNavigationDebugObstacle> staticObstacles;
    std::vector<SectorNavigationDebugDoorPlaceholder> doorPlaceholders;
    std::vector<SectorNavigationBuildDoorLink> doorLinks;
    BoundingBox bounds = {};
    uint64_t sourceHash = 0;
};

// Builds deterministic, CPU-only rasterization input from topology and resolved
// collision data. It deliberately does not use generated render meshes.
bool BuildSectorNavigationBuildInput(
        const SectorTopologyMap& map,
        const std::vector<SectorStaticModelCollider>& resolvedStaticColliders,
        const SectorNavigationSettings& settings,
        SectorNavigationBuildInput& outInput,
        std::vector<std::string>& outWarnings,
        std::string& outError);

uint64_t ComputeSectorNavigationSourceHash(
        const SectorTopologyMap& map,
        const std::vector<SectorStaticModelCollider>& resolvedStaticColliders,
        const SectorNavigationSettings& settings);

std::string FormatSectorNavigationSourceHash(uint64_t hash);

} // namespace game
