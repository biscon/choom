#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace game {

constexpr std::string_view kDefaultSkyTextureId = "sky_cylinder";
constexpr int kDefaultSkyCylinderSegments = 64;
constexpr float kDefaultSkyCylinderRadius = 1200.0f;
constexpr float kDefaultSkyCylinderHeight = 2400.0f;

struct SectorSkyCylinderMeshData {
    std::vector<Vector3> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> uvs;
    std::vector<uint16_t> indices;
};

bool HasSkyCeilingSector(const SectorTopologyMap& map);
const SectorTextureDefinition* FindDefaultSkyTexture(const SectorTopologyMap& map);
bool ShouldRenderDefaultSkyCylinder(const SectorTopologyMap& map);
SectorSkyCylinderMeshData BuildSkyCylinderMeshData(
        int segments = kDefaultSkyCylinderSegments,
        float radius = kDefaultSkyCylinderRadius,
        float height = kDefaultSkyCylinderHeight);

} // namespace game
