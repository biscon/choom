#pragma once

#include "sector_demo/SectorUnits.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace game {

constexpr float DynamicLightFlickerBaseRateHz = 8.0f;
constexpr float DynamicLightFlickerTransitionFraction = 0.18f;
constexpr float DynamicLightFlickerDefaultSpeed = 1.0f;
constexpr float DynamicLightFlickerDefaultAmount = 0.35f;
constexpr float DynamicLightFlickerMinSpeed = 0.05f;
constexpr float DynamicLightFlickerMaxSpeed = 10.0f;
constexpr float DynamicLightFlickerMinAmount = 0.0f;
constexpr float DynamicLightFlickerMaxAmount = 1.0f;

inline float ClampDynamicLightFlickerSpeed(float value)
{
    return std::clamp(value, DynamicLightFlickerMinSpeed, DynamicLightFlickerMaxSpeed);
}

inline float ClampDynamicLightFlickerAmount(float value)
{
    return std::clamp(value, DynamicLightFlickerMinAmount, DynamicLightFlickerMaxAmount);
}

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

struct SectorTopologyLineDefFlags {
    bool blocksPlayer = false;
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
    SectorTopologyLineDefFlags flags;
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
    bool ceilingSky = false;

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

struct SectorTopologyDynamicPointLight {
    int id = -1;
    Vector3 position = {0.0f, SectorWorldToAuthoringDistance(1.8f), 0.0f};
    Color color = WHITE;
    float intensity = 1.0f;
    float radius = SectorWorldToAuthoringDistance(8.0f);
    bool enabled = true;
    bool flicker = false;
    // 0.2-0.4 is subtle, 0.6-0.8 is a strong failing-light dip, near 1.0 can drop nearly off.
    float flickerSpeed = DynamicLightFlickerDefaultSpeed;
    float flickerAmount = DynamicLightFlickerDefaultAmount;
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
    StaticLight,
    DynamicLight
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
