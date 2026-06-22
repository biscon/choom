#pragma once

#include "sector_demo/SectorTopologyMap.h"

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
    int topologySectorId = -1;
    int topologyLineDefId = -1;
    int topologySideDefId = -1;
    SectorTopologySideKind topologySide = SectorTopologySideKind::Front;
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

struct SectorGeneratedSurfaceHit {
    bool hit = false;
    SectorGeneratedSurfaceRef ref;
    Vector3 worldPosition = {};
    float distance = 0.0f;
};

bool BuildSectorGeneratedGeometry(
        const SectorTopologyMap& map,
        SectorGeneratedGeometry& outGeometry,
        std::string* outError = nullptr);
SectorGeneratedSurfaceHit PickSectorGeneratedGeometry(
        const SectorGeneratedGeometry& geometry,
        Ray ray,
        float minDistance = 0.001f);
const char* SectorGeneratedSurfaceKindName(SectorGeneratedSurfaceKind kind);
std::string FormatSectorGeneratedSurfaceLabel(const SectorGeneratedSurfaceRef& ref);

} // namespace game
