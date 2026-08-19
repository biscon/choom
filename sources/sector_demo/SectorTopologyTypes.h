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
constexpr int DynamicSpotLightMinShadowPriority = -1000;
constexpr int DynamicSpotLightMaxShadowPriority = 1000;
constexpr int DynamicSpotLightDefaultShadowPriority = 0;
constexpr float DynamicSpotLightMinShadowBias = 0.0f;
constexpr float DynamicSpotLightMaxShadowBias = 0.1f;
constexpr float DynamicSpotLightDefaultShadowBias = 0.00015f;
constexpr float DynamicSpotLightMinShadowStrength = 0.0f;
constexpr float DynamicSpotLightMaxShadowStrength = 1.0f;
constexpr float DynamicSpotLightDefaultShadowStrength = 1.0f;
constexpr float DynamicSpotLightMinShadowSoftness = 0.0f;
constexpr float DynamicSpotLightMaxShadowSoftness = 8.0f;
constexpr float DynamicSpotLightDefaultShadowSoftness = 1.0f;
constexpr float TopologyUvScaleMin = 0.001f;
constexpr float TopologyUvScaleMax = 64.0f;

inline float ClampDynamicLightFlickerSpeed(float value)
{
    return std::clamp(value, DynamicLightFlickerMinSpeed, DynamicLightFlickerMaxSpeed);
}

inline float ClampDynamicLightFlickerAmount(float value)
{
    return std::clamp(value, DynamicLightFlickerMinAmount, DynamicLightFlickerMaxAmount);
}

inline int ClampDynamicSpotLightShadowPriority(int value)
{
    return std::clamp(value, DynamicSpotLightMinShadowPriority, DynamicSpotLightMaxShadowPriority);
}

inline float ClampDynamicSpotLightShadowBias(float value)
{
    return std::clamp(value, DynamicSpotLightMinShadowBias, DynamicSpotLightMaxShadowBias);
}

inline float ClampDynamicSpotLightShadowStrength(float value)
{
    return std::clamp(value, DynamicSpotLightMinShadowStrength, DynamicSpotLightMaxShadowStrength);
}

inline float ClampDynamicSpotLightShadowSoftness(float value)
{
    return std::clamp(value, DynamicSpotLightMinShadowSoftness, DynamicSpotLightMaxShadowSoftness);
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

struct SectorDoorFaceUv {
    Vector2 scale = {1.0f, 1.0f};
    Vector2 offset = {0.0f, 0.0f};
};

enum class SectorDoorFace {
    // Front is the local +normal slab face. For anchored doors this is the side facing
    // from anchor.frontSectorId toward anchor.backSectorId; Back is the opposite face.
    Front = 0,
    Back,
    Left,
    Right,
    Top,
    Bottom,
    Count
};

inline constexpr int SectorDoorFaceCount = static_cast<int>(SectorDoorFace::Count);

struct SectorDoorFaceUvSet {
    SectorDoorFaceUv faces[SectorDoorFaceCount];
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
    // Empty uses the application-wide default footstep set.
    std::string footstepSet;
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

struct SectorLightHazeSettings {
    bool enabled = false;
    float extentScale = 0.40f;
    float heightOffsetWorld = 0.0f;
    float density = 0.04f;
    Color scatteringTint = WHITE;
    float edgeSoftness = 0.35f;
    float noiseAmount = 0.65f;
    float noiseScaleWorld = 0.5f;
    float flowDirectionDegrees = 0.0f;
    float flowSpeedWorld = 0.20f;
};

struct SectorLightDustSettings {
    bool enabled = false;
    int amount = 24;
    float extentScale = 0.55f;
    float minimumSizeWorld = 0.008f;
    float maximumSizeWorld = 0.022f;
    float opacity = 0.22f;
    float driftSpeedWorld = 0.025f;
    float turbulenceWorld = 0.015f;
    Color scatteringTint = WHITE;
};

struct SectorLightProxyHaloSettings {
    bool enabled = false;
    float radiusWorld = 0.5f;
    float brightness = 0.12f;
    float maxExtinction = 0.03f;
    float edgeSoftness = 0.5f;
    Color scatteringTint = WHITE;
};

struct SectorLightProxyShaftSettings {
    bool enabled = false;
    float lengthScale = 0.65f;
    float widthScale = 0.75f;
    float brightness = 0.05f;
    float maxExtinction = 0.08f;
    float edgeSoftness = 0.5f;
    Color scatteringTint = WHITE;
};

struct SectorLightProxySettings {
    SectorLightProxyHaloSettings halo;
    SectorLightProxyShaftSettings shaft;
};

struct SectorLightAtmosphereSettings {
    SectorLightHazeSettings haze;
    SectorLightProxySettings proxy;
    SectorLightDustSettings dust;
};

SectorLightHazeSettings NormalizeSectorLightHazeSettings(SectorLightHazeSettings settings);
SectorLightDustSettings NormalizeSectorLightDustSettings(SectorLightDustSettings settings);
SectorLightProxySettings NormalizeSectorLightProxySettings(SectorLightProxySettings settings);
SectorLightAtmosphereSettings NormalizeSectorLightAtmosphereSettings(
        SectorLightAtmosphereSettings settings);
bool IsDefaultSectorLightHazeSettings(const SectorLightHazeSettings& settings);
bool IsDefaultSectorLightDustSettings(const SectorLightDustSettings& settings);
bool IsDefaultSectorLightProxySettings(const SectorLightProxySettings& settings);
bool IsDefaultSectorLightAtmosphereSettings(const SectorLightAtmosphereSettings& settings);

struct SectorTopologyStaticPointLight {
    int id = -1;
    Vector3 position = {0.0f, SectorWorldToAuthoringDistance(1.8f), 0.0f};
    Color color = WHITE;
    float intensity = 1.0f;
    float radius = SectorWorldToAuthoringDistance(8.0f);
    float sourceRadius = 0.0f;
    SectorLightAtmosphereSettings atmosphere;
    bool castsShadow = true;
};

struct SectorTopologyStaticSpotLight {
    int id = -1;
    Vector3 position = {0.0f, SectorWorldToAuthoringDistance(1.8f), 0.0f};
    Vector3 target = {SectorWorldToAuthoringDistance(4.0f), SectorWorldToAuthoringDistance(1.0f), 0.0f};
    Color color = WHITE;
    float intensity = 1.0f;
    float range = SectorWorldToAuthoringDistance(8.0f);
    float innerConeDegrees = 20.0f;
    float outerConeDegrees = 35.0f;
    float sourceRadius = 0.0f;
    SectorLightAtmosphereSettings atmosphere;
    bool castsShadow = true;
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
    SectorLightAtmosphereSettings atmosphere;
    bool castsShadow = false;
    int shadowPriority = DynamicSpotLightDefaultShadowPriority;
    float shadowBias = DynamicSpotLightDefaultShadowBias;
    float shadowStrength = DynamicSpotLightDefaultShadowStrength;
    float shadowSoftness = DynamicSpotLightDefaultShadowSoftness;
};

struct SectorTopologyDynamicSpotLight {
    int id = -1;
    Vector3 position = {0.0f, SectorWorldToAuthoringDistance(1.8f), 0.0f};
    Vector3 target = {SectorWorldToAuthoringDistance(4.0f), SectorWorldToAuthoringDistance(1.0f), 0.0f};
    Color color = WHITE;
    float intensity = 1.0f;
    float range = SectorWorldToAuthoringDistance(8.0f);
    float innerConeDegrees = 20.0f;
    float outerConeDegrees = 35.0f;
    bool enabled = true;
    bool flicker = false;
    float flickerSpeed = DynamicLightFlickerDefaultSpeed;
    float flickerAmount = DynamicLightFlickerDefaultAmount;
    bool castsShadow = false;
    int shadowPriority = DynamicSpotLightDefaultShadowPriority;
    float shadowBias = DynamicSpotLightDefaultShadowBias;
    float shadowStrength = DynamicSpotLightDefaultShadowStrength;
    float shadowSoftness = DynamicSpotLightDefaultShadowSoftness;
    SectorLightAtmosphereSettings atmosphere;
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
    DynamicLight,
    LevelMarker
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
