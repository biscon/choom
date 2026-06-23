#pragma once

#include "sector_demo/SectorUnits.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

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

struct SectorTopologyDecalLayer {
    std::string textureId;
    SectorTopologyUvSettings uv;
    float opacity = 1.0f;
    bool emissive = false;
    Vector3 tint = {1.0f, 1.0f, 1.0f};
    float bloomIntensity = 1.0f;
};

struct SectorTopologyWallPartSettings {
    std::string textureId;
    SectorTopologyUvSettings uv;
    SectorTopologyDecalLayer decal;
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
    SectorTopologyWallPartSettings middle;
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
    SectorTopologyDecalLayer floorDecal;
    SectorTopologyDecalLayer ceilingDecal;

    Color ambientColor = WHITE;
    float ambientIntensity = 1.0f;

    // These values initialize future sidedefs; existing sidedefs keep concrete values.
    SectorTopologyWallPartSettings defaultWall;
    SectorTopologyWallPartSettings defaultLower;
    SectorTopologyWallPartSettings defaultUpper;
};

struct SectorTopologyStaticPointLight {
    int id = -1;
    Vector3 position = {0.0f, SectorWorldToAuthoringDistance(1.8f), 0.0f};
    Color color = WHITE;
    float intensity = 1.0f;
    float radius = SectorWorldToAuthoringDistance(8.0f);
    float sourceRadius = 0.0f;
};

enum class SectorTopologyValidationSeverity {
    Warning,
    Error
};

enum class SectorTopologyObjectKind {
    Map,
    Vertex,
    LineDef,
    SideDef,
    Sector,
    StaticLight
};

struct SectorTopologyValidationIssue {
    SectorTopologyValidationSeverity severity = SectorTopologyValidationSeverity::Error;
    SectorTopologyObjectKind objectKind = SectorTopologyObjectKind::Map;
    int objectId = -1;
    std::string message;
};

struct SectorTopologyLoopEdge {
    int sideDefId = -1;
    int lineDefId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
    int startVertexId = -1;
    int endVertexId = -1;
};

// Positive signed area is counter-clockwise in topology coordinates, where
// positive X points right and positive Y points up. Outer loops are CCW and
// hole loops are clockwise.
struct SectorTopologyLoop {
    std::vector<int> vertexIds;
    std::vector<int> sideDefIds;
    std::vector<SectorTopologyLoopEdge> edges;
    int64_t signedAreaTwice = 0;
};

struct SectorTopologyLoopSet {
    SectorTopologyLoop outer;
    std::vector<SectorTopologyLoop> holes;
};

} // namespace game
