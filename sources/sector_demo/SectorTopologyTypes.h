#pragma once

#include "sector_demo/SectorTopologyUnits.h"

#include <raylib.h>

#include <string>

namespace game {

struct SectorTopologyVertex {
    int id = -1;
    SectorCoord x = 0;
    SectorCoord y = 0;
};

enum class SectorTopologySideKind {
    Front,
    Back
};

struct SectorTopologyUvSettings {
    Vector2 scale = {1.0f, 1.0f};
    Vector2 offset = {0.0f, 0.0f};
};

struct SectorTopologyWallPartSettings {
    std::string textureId;
    SectorTopologyUvSettings uv;
};

// A linedef is directed from its start vertex to its end vertex.
// The front side follows start -> end, and the back side follows end -> start.
// The sector owning either directed side lies to the left of that side.
struct SectorTopologyLineDef {
    int id = -1;
    int startVertexId = -1;
    int endVertexId = -1;
    int frontSideDefId = -1;
    int backSideDefId = -1;
};

struct SectorTopologySideDef {
    int id = -1;
    int lineDefId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
    int sectorId = -1;

    SectorTopologyWallPartSettings wall;
    SectorTopologyWallPartSettings lower;
    SectorTopologyWallPartSettings upper;
};

struct SectorTopologySector {
    int id = -1;
    std::string name;

    float floorZ = 0.0f;
    float ceilingZ = 24.0f;

    std::string floorTextureId;
    std::string ceilingTextureId;

    SectorTopologyUvSettings floorUv;
    SectorTopologyUvSettings ceilingUv;

    Color ambientColor = WHITE;
    float ambientIntensity = 1.0f;

    // These values initialize future sidedefs; existing sidedefs keep concrete values.
    SectorTopologyWallPartSettings defaultWall;
    SectorTopologyWallPartSettings defaultLower;
    SectorTopologyWallPartSettings defaultUpper;
};

} // namespace game
