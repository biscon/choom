#pragma once

#include "sector_demo/SectorTypes.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

enum class SectorGeneratedSurfaceKind {
    Floor,
    Ceiling,
    Wall,
    LowerWall,
    UpperWall
};

struct SectorGeneratedSurfaceRef {
    SectorGeneratedSurfaceKind kind = SectorGeneratedSurfaceKind::Floor;
    int sectorIndex = -1;
    SectorBoundaryRingKind ringKind = SectorBoundaryRingKind::Outer;
    int holeIndex = -1;
    int edgeIndex = -1;
};

struct SectorGeneratedVertex {
    Vector3 position = {};
    Vector3 normal = {};
    Vector2 uv = {};
    Vector2 chartUv = {};
    Color color = WHITE;
};

struct SectorGeneratedSurface {
    SectorGeneratedSurfaceRef ref;
    std::string textureId;
    Vector3 normal = {};
    std::vector<SectorGeneratedVertex> vertices;
    float chartWidth = 0.0f;
    float chartHeight = 0.0f;
};

struct SectorGeneratedGeometry {
    std::vector<SectorGeneratedSurface> surfaces;
};

bool BuildSectorGeneratedGeometry(const SectorMap& map, SectorGeneratedGeometry& outGeometry);
const char* SectorGeneratedSurfaceKindName(SectorGeneratedSurfaceKind kind);

} // namespace game
