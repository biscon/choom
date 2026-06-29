#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct RuntimePortalVisibilityResult;

constexpr float kSectorGeneratedTextureWorldSize = 2.0f;

enum class SectorGeneratedSurfaceKind {
    Floor,
    Ceiling,
    Wall,
    LowerWall,
    UpperWall,
    Middle
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
    Vector2 decalUv = {};
    Vector2 chartUv = {};
    Color color = WHITE;
};

struct SectorGeneratedSurface {
    SectorGeneratedSurfaceRef ref;
    std::string textureId;
    std::string decalTextureId;
    float decalOpacity = 1.0f;
    bool decalEmissive = false;
    Vector3 decalTint = {1.0f, 1.0f, 1.0f};
    float decalBloomIntensity = 1.0f;
    Vector3 normal = {};
    bool alphaTest = false;
    float alphaCutoff = 0.5f;
    bool receivesLightmap = true;
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
SectorGeneratedSurfaceHit PickSectorGeneratedGeometry(
        const SectorGeneratedGeometry& geometry,
        Ray ray,
        const RuntimePortalVisibilityResult& visibility,
        float minDistance = 0.001f);
bool ShouldIncludeSectorGeneratedSurfaceForVisibility(
        const SectorGeneratedSurface& surface,
        const RuntimePortalVisibilityResult& visibility);
const char* SectorGeneratedSurfaceKindName(SectorGeneratedSurfaceKind kind);
std::string FormatSectorGeneratedSurfaceLabel(const SectorGeneratedSurfaceRef& ref);

} // namespace game
