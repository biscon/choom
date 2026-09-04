#pragma once

#include "sector_demo/SectorUnits.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace game {

enum class SectorRoomtoneMode {
    Inherit,
    Play,
    Silence
};

enum class SectorLiquidSurfaceReference {
    Floor,
    Ceiling
};

struct SectorLiquidParticulateSettings {
    int amount = 72;
    float sizeWorld = 0.010f;
    float opacity = 0.28f;
    float flowInfluence = 0.25f;
    float wakeInfluence = 0.60f;
};

// Liquid appearance is procedural. Distances and speeds use runtime/world units,
// while surfaceOffset uses the sector editor's authored height units.
struct SectorLiquidSettings {
    bool enabled = false;
    SectorLiquidSurfaceReference surfaceReference = SectorLiquidSurfaceReference::Floor;
    float surfaceOffset = 0.0f;
    Color shallowColor = Color{49, 126, 142, 255};
    Color deepColor = Color{8, 38, 54, 255};
    float visibilityDepthWorld = 4.0f;
    float roughness = 0.12f;
    float refractionStrength = 0.025f;
    float rippleScaleWorld = 0.9f;
    float rippleStrength = 0.22f;
    float rippleSpeed = 0.35f;
    float flowDirectionDegrees = 0.0f;
    float flowSpeedWorld = 0.0f;
    SectorLiquidParticulateSettings particulates;
};

constexpr float SectorLiquidMinVisibilityDepthWorld = 0.05f;
constexpr float SectorLiquidMaxVisibilityDepthWorld = 128.0f;
constexpr float SectorLiquidMinRippleScaleWorld = 0.05f;
constexpr float SectorLiquidMaxRippleScaleWorld = 64.0f;
constexpr float SectorLiquidMaxRefractionStrength = 0.25f;
constexpr float SectorLiquidMaxRippleStrength = 2.0f;
constexpr float SectorLiquidMaxRippleSpeed = 10.0f;
constexpr float SectorLiquidMaxFlowSpeedWorld = 32.0f;
constexpr int SectorLiquidMaxParticulateAmount = 192;
constexpr float SectorLiquidMinParticulateSizeWorld = 0.001f;
constexpr float SectorLiquidMaxParticulateSizeWorld = 0.1f;

inline bool AreSectorLiquidParticulateSettingsEqual(
        const SectorLiquidParticulateSettings& a,
        const SectorLiquidParticulateSettings& b)
{
    return a.amount == b.amount
            && a.sizeWorld == b.sizeWorld
            && a.opacity == b.opacity
            && a.flowInfluence == b.flowInfluence
            && a.wakeInfluence == b.wakeInfluence;
}

inline bool AreSectorLiquidSettingsEqual(
        const SectorLiquidSettings& a,
        const SectorLiquidSettings& b)
{
    const auto sameColor = [](Color left, Color right) {
        return left.r == right.r && left.g == right.g
                && left.b == right.b && left.a == right.a;
    };
    return a.enabled == b.enabled
            && a.surfaceReference == b.surfaceReference
            && a.surfaceOffset == b.surfaceOffset
            && sameColor(a.shallowColor, b.shallowColor)
            && sameColor(a.deepColor, b.deepColor)
            && a.visibilityDepthWorld == b.visibilityDepthWorld
            && a.roughness == b.roughness
            && a.refractionStrength == b.refractionStrength
            && a.rippleScaleWorld == b.rippleScaleWorld
            && a.rippleStrength == b.rippleStrength
            && a.rippleSpeed == b.rippleSpeed
            && a.flowDirectionDegrees == b.flowDirectionDegrees
            && a.flowSpeedWorld == b.flowSpeedWorld
            && AreSectorLiquidParticulateSettingsEqual(
                    a.particulates, b.particulates);
}

inline bool IsDefaultSectorLiquidSettings(const SectorLiquidSettings& settings)
{
    return AreSectorLiquidSettingsEqual(settings, SectorLiquidSettings{});
}

inline float ResolveSectorLiquidSurfaceHeight(
        const SectorLiquidSettings& liquid,
        float floorZ,
        float ceilingZ)
{
    return liquid.surfaceReference == SectorLiquidSurfaceReference::Ceiling
            ? ceilingZ - liquid.surfaceOffset
            : floorZ + liquid.surfaceOffset;
}

inline SectorLiquidSettings NormalizeSectorLiquidSettingsForSpan(
        SectorLiquidSettings liquid,
        float floorZ,
        float ceilingZ)
{
    const float span = std::max(0.0f, ceilingZ - floorZ);
    liquid.surfaceOffset = std::clamp(
            std::isfinite(liquid.surfaceOffset) ? liquid.surfaceOffset : 0.0f,
            0.0f,
            span);
    liquid.flowDirectionDegrees = std::isfinite(liquid.flowDirectionDegrees)
            ? std::fmod(liquid.flowDirectionDegrees, 360.0f)
            : 0.0f;
    if (liquid.flowDirectionDegrees < 0.0f) liquid.flowDirectionDegrees += 360.0f;
    return liquid;
}

struct SectorRoomtoneSettings {
    static constexpr float DefaultVolume = 0.6f;
    static constexpr int UseMapFadeMilliseconds = -1;

    SectorRoomtoneMode mode = SectorRoomtoneMode::Inherit;
    std::string soundId;
    float volume = DefaultVolume;
    int fadeMilliseconds = UseMapFadeMilliseconds;
};

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

enum class SectorDuctCoverRemovalSide {
    Outside,
    Crawlspace
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
    std::string materialId;
    SectorTopologyUvSettings uv;
    float opacity = 1.0f;
    bool emissive = false;
    Vector3 tint = {1.0f, 1.0f, 1.0f};
    float bloomIntensity = 1.0f;
};

struct SectorTopologyWallPartSettings {
    std::string materialId;
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

    std::string floorMaterialId;
    std::string ceilingMaterialId;
    // Empty uses the application-wide default footstep set.
    std::string footstepSet;
    bool ceilingSky = false;
    // Gameplay-only marker for sectors that use the prone duct traversal proxy.
    bool crawlspace = false;
    SectorRoomtoneSettings roomtone;
    SectorLiquidSettings liquid;

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
    Vector3 centerOffsetWorld = {};
    float brightness = 0.12f;
    float maxExtinction = 0.03f;
    float edgeSoftness = 0.5f;
    Color scatteringTint = WHITE;
};

struct SectorLightProxyShaftSettings {
    bool enabled = false;
    Vector3 originOffsetWorld = {};
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
    SectorLightProxySettings proxy;
    SectorLightDustSettings dust;
};

SectorLightDustSettings NormalizeSectorLightDustSettings(SectorLightDustSettings settings);
SectorLightProxySettings NormalizeSectorLightProxySettings(SectorLightProxySettings settings);
SectorLightAtmosphereSettings NormalizeSectorLightAtmosphereSettings(
        SectorLightAtmosphereSettings settings);
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

// A one-sided rectangular emitter. The target defines the emitting normal;
// roll rotates the width/height axes around that normal.
struct SectorTopologyStaticRectLight {
    int id = -1;
    Vector3 position = {0.0f, SectorWorldToAuthoringDistance(1.8f), 0.0f};
    Vector3 target = {0.0f, SectorWorldToAuthoringDistance(1.0f), 0.0f};
    float rollDegrees = 0.0f;
    float width = SectorWorldToAuthoringDistance(1.0f);
    float height = SectorWorldToAuthoringDistance(0.25f);
    Color color = WHITE;
    float intensity = 1.0f;
    float range = SectorWorldToAuthoringDistance(8.0f);
    // Front-side distance over which emission ramps up from the light plane.
    float startFeather = 0.0f;
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
    std::string instanceId;
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
    std::string instanceId;
};

struct SectorTopologyDynamicRectLight {
    int id = -1;
    Vector3 position = {0.0f, SectorWorldToAuthoringDistance(1.8f), 0.0f};
    Vector3 target = {0.0f, SectorWorldToAuthoringDistance(1.0f), 0.0f};
    float rollDegrees = 0.0f;
    float width = SectorWorldToAuthoringDistance(1.0f);
    float height = SectorWorldToAuthoringDistance(0.25f);
    Color color = WHITE;
    float intensity = 1.0f;
    float range = SectorWorldToAuthoringDistance(8.0f);
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
    std::string instanceId;
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
    LevelMarker,
    SoundEmitter
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
