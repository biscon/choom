#include "sector_demo/SectorTopologyMap.h"

#include "sector_demo/SectorMath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace game {

namespace {

bool SameRgb(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

float FiniteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

} // namespace

SectorLightDustSettings NormalizeSectorLightDustSettings(SectorLightDustSettings settings)
{
    const SectorLightDustSettings defaults;
    settings.amount = std::clamp(settings.amount, 0, 128);
    settings.extentScale = std::clamp(FiniteOr(settings.extentScale, defaults.extentScale), 0.05f, 2.0f);
    settings.minimumSizeWorld = std::clamp(
            FiniteOr(settings.minimumSizeWorld, defaults.minimumSizeWorld),
            0.002f,
            0.25f);
    settings.maximumSizeWorld = std::clamp(
            FiniteOr(settings.maximumSizeWorld, defaults.maximumSizeWorld),
            settings.minimumSizeWorld,
            0.25f);
    settings.opacity = std::clamp(FiniteOr(settings.opacity, defaults.opacity), 0.0f, 1.0f);
    settings.driftSpeedWorld = std::clamp(
            FiniteOr(settings.driftSpeedWorld, defaults.driftSpeedWorld),
            0.0f,
            0.5f);
    settings.turbulenceWorld = std::clamp(
            FiniteOr(settings.turbulenceWorld, defaults.turbulenceWorld),
            0.0f,
            0.5f);
    settings.scatteringTint.a = 255;
    return settings;
}

SectorLightProxySettings NormalizeSectorLightProxySettings(SectorLightProxySettings settings)
{
    const SectorLightProxySettings defaults;
    settings.halo.radiusWorld = std::clamp(
            FiniteOr(settings.halo.radiusWorld, defaults.halo.radiusWorld), 0.01f, 64.0f);
    settings.halo.centerOffsetWorld.x = std::clamp(
            FiniteOr(settings.halo.centerOffsetWorld.x, defaults.halo.centerOffsetWorld.x),
            -100000.0f,
            100000.0f);
    settings.halo.centerOffsetWorld.y = std::clamp(
            FiniteOr(settings.halo.centerOffsetWorld.y, defaults.halo.centerOffsetWorld.y),
            -100000.0f,
            100000.0f);
    settings.halo.centerOffsetWorld.z = std::clamp(
            FiniteOr(settings.halo.centerOffsetWorld.z, defaults.halo.centerOffsetWorld.z),
            -100000.0f,
            100000.0f);
    settings.halo.brightness = std::clamp(
            FiniteOr(settings.halo.brightness, defaults.halo.brightness), 0.0f, 16.0f);
    settings.halo.maxExtinction = std::clamp(
            FiniteOr(settings.halo.maxExtinction, defaults.halo.maxExtinction), 0.0f, 1.0f);
    settings.halo.edgeSoftness = std::clamp(
            FiniteOr(settings.halo.edgeSoftness, defaults.halo.edgeSoftness), 0.01f, 1.0f);
    settings.halo.scatteringTint.a = 255;
    settings.shaft.originOffsetWorld.x = std::clamp(
            FiniteOr(settings.shaft.originOffsetWorld.x, defaults.shaft.originOffsetWorld.x),
            -100000.0f,
            100000.0f);
    settings.shaft.originOffsetWorld.y = std::clamp(
            FiniteOr(settings.shaft.originOffsetWorld.y, defaults.shaft.originOffsetWorld.y),
            -100000.0f,
            100000.0f);
    settings.shaft.originOffsetWorld.z = std::clamp(
            FiniteOr(settings.shaft.originOffsetWorld.z, defaults.shaft.originOffsetWorld.z),
            -100000.0f,
            100000.0f);
    settings.shaft.lengthScale = std::clamp(
            FiniteOr(settings.shaft.lengthScale, defaults.shaft.lengthScale), 0.01f, 2.0f);
    settings.shaft.widthScale = std::clamp(
            FiniteOr(settings.shaft.widthScale, defaults.shaft.widthScale), 0.01f, 2.0f);
    settings.shaft.brightness = std::clamp(
            FiniteOr(settings.shaft.brightness, defaults.shaft.brightness), 0.0f, 16.0f);
    settings.shaft.maxExtinction = std::clamp(
            FiniteOr(settings.shaft.maxExtinction, defaults.shaft.maxExtinction), 0.0f, 1.0f);
    settings.shaft.edgeSoftness = std::clamp(
            FiniteOr(settings.shaft.edgeSoftness, defaults.shaft.edgeSoftness), 0.01f, 1.0f);
    settings.shaft.scatteringTint.a = 255;
    return settings;
}

SectorLightAtmosphereSettings NormalizeSectorLightAtmosphereSettings(
        SectorLightAtmosphereSettings settings)
{
    settings.proxy = NormalizeSectorLightProxySettings(settings.proxy);
    settings.dust = NormalizeSectorLightDustSettings(settings.dust);
    return settings;
}

bool IsDefaultSectorLightProxySettings(const SectorLightProxySettings& settings)
{
    const SectorLightProxySettings value = NormalizeSectorLightProxySettings(settings);
    const SectorLightProxySettings defaults;
    return value.halo.enabled == defaults.halo.enabled
            && value.halo.radiusWorld == defaults.halo.radiusWorld
            && value.halo.centerOffsetWorld.x == defaults.halo.centerOffsetWorld.x
            && value.halo.centerOffsetWorld.y == defaults.halo.centerOffsetWorld.y
            && value.halo.centerOffsetWorld.z == defaults.halo.centerOffsetWorld.z
            && value.halo.brightness == defaults.halo.brightness
            && value.halo.maxExtinction == defaults.halo.maxExtinction
            && value.halo.edgeSoftness == defaults.halo.edgeSoftness
            && SameRgb(value.halo.scatteringTint, defaults.halo.scatteringTint)
            && value.shaft.enabled == defaults.shaft.enabled
            && value.shaft.originOffsetWorld.x == defaults.shaft.originOffsetWorld.x
            && value.shaft.originOffsetWorld.y == defaults.shaft.originOffsetWorld.y
            && value.shaft.originOffsetWorld.z == defaults.shaft.originOffsetWorld.z
            && value.shaft.lengthScale == defaults.shaft.lengthScale
            && value.shaft.widthScale == defaults.shaft.widthScale
            && value.shaft.brightness == defaults.shaft.brightness
            && value.shaft.maxExtinction == defaults.shaft.maxExtinction
            && value.shaft.edgeSoftness == defaults.shaft.edgeSoftness
            && SameRgb(value.shaft.scatteringTint, defaults.shaft.scatteringTint);
}

bool IsDefaultSectorLightDustSettings(const SectorLightDustSettings& settings)
{
    const SectorLightDustSettings value = NormalizeSectorLightDustSettings(settings);
    const SectorLightDustSettings defaults;
    return value.enabled == defaults.enabled
            && value.amount == defaults.amount
            && value.extentScale == defaults.extentScale
            && value.minimumSizeWorld == defaults.minimumSizeWorld
            && value.maximumSizeWorld == defaults.maximumSizeWorld
            && value.opacity == defaults.opacity
            && value.driftSpeedWorld == defaults.driftSpeedWorld
            && value.turbulenceWorld == defaults.turbulenceWorld
            && SameRgb(value.scatteringTint, defaults.scatteringTint);
}

bool IsDefaultSectorLightAtmosphereSettings(const SectorLightAtmosphereSettings& settings)
{
    return IsDefaultSectorLightProxySettings(settings.proxy)
            && IsDefaultSectorLightDustSettings(settings.dust);
}
namespace {

constexpr float PreviewWalkSpeedMin = 0.1f;
constexpr float PreviewWalkSpeedMax = 100.0f;
constexpr float PreviewRunSpeedMin = 0.1f;
constexpr float PreviewRunSpeedMax = 200.0f;
constexpr float PreviewMouseSensitivityMin = 0.01f;
constexpr float PreviewMouseSensitivityMax = 20.0f;
constexpr float PreviewEyeHeightMin = 0.1f;
constexpr float PreviewEyeHeightMax = 3.0f;
constexpr float PreviewGravityMin = 0.0f;
constexpr float PreviewGravityMax = 200.0f;
constexpr float PreviewPlayerRadiusMin = 0.05f;
constexpr float PreviewPlayerRadiusMax = 2.0f;
constexpr float PreviewPlayerHeightMin = 0.5f;
constexpr float PreviewPlayerHeightMax = 3.0f;
constexpr float PreviewStepHeightMin = 0.0f;
constexpr float PreviewStepHeightMax = 2.0f;
constexpr float PreviewJumpHeightMin = 0.0f;
constexpr float PreviewJumpHeightMax = 3.0f;
constexpr float PreviewHeadBobStrengthMin = 0.0f;
constexpr float PreviewHeadBobStrengthMax = 0.25f;
constexpr float PreviewHeadBobFrequencyMin = 0.0f;
constexpr float PreviewHeadBobFrequencyMax = 20.0f;
constexpr float PreviewObjectProbeDebugDrawMaxDistanceMin = 0.0f;
constexpr float PreviewObjectProbeDebugDrawMaxDistanceMax = 512.0f;
constexpr float SkyVerticalScaleMin = 0.01f;
constexpr float SkyVerticalScaleMax = 100.0f;
constexpr float DirectionalLightMinLengthSqr = 0.000001f;
constexpr float FogStartDistanceMin = 0.0f;
constexpr float FogStartDistanceMax = 512.0f;
constexpr float FogDensityMin = 0.0f;
constexpr float FogDensityMax = 1.0f;
constexpr float FogMaxOpacityMin = 0.0f;
constexpr float FogMaxOpacityMax = 1.0f;
constexpr float FogReferenceHeightMin = -512.0f;
constexpr float FogReferenceHeightMax = 512.0f;
constexpr float FogHeightFalloffMin = 0.0f;
constexpr float FogHeightFalloffMax = 16.0f;
constexpr float DoorAnchorSideProbeDistance = 0.001f;
constexpr float DoorAnchorSideEpsilon = 0.000001f;

float SectorCoordToWorldDistanceLocal(SectorCoord value)
{
    return static_cast<float>(value)
            / static_cast<float>(SectorCoordSubdivisions)
            * kSectorWorldUnitsPerAuthoringUnit;
}

Vector2 SectorCoordToWorldPosition2Local(SectorCoord x, SectorCoord y)
{
    return Vector2{SectorCoordToWorldDistanceLocal(x), SectorCoordToWorldDistanceLocal(y)};
}

float SignedDistanceFromDirectedLine(Vector2 origin, Vector2 tangent, Vector2 point)
{
    return tangent.x * (point.y - origin.y) - tangent.y * (point.x - origin.x);
}

bool ProbeIsInsideDirectedSide(Vector2 origin, Vector2 tangent, Vector2 point)
{
    return SignedDistanceFromDirectedLine(origin, tangent, point) > DoorAnchorSideEpsilon;
}

bool ProbeIsOutsideDirectedSide(Vector2 origin, Vector2 tangent, Vector2 point)
{
    return SignedDistanceFromDirectedLine(origin, tangent, point) < -DoorAnchorSideEpsilon;
}

template<typename T>
int AllocateNextId(const std::vector<T>& values)
{
    int maxId = 0;
    for (const T& value : values) {
        if (value.id > maxId) {
            maxId = value.id;
        }
    }

    if (maxId == std::numeric_limits<int>::max()) {
        return -1;
    }
    return maxId + 1;
}

template<typename T>
const T* FindById(const std::vector<T>& values, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return nullptr;
    }

    for (const T& value : values) {
        if (value.id == id) {
            return &value;
        }
    }
    return nullptr;
}

template<typename T>
T* FindById(std::vector<T>& values, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return nullptr;
    }

    for (T& value : values) {
        if (value.id == id) {
            return &value;
        }
    }
    return nullptr;
}

} // namespace

SectorPreviewSettings DefaultSectorPreviewSettings()
{
    return SectorPreviewSettings{};
}

SectorPreviewSettings NormalizeSectorPreviewSettings(SectorPreviewSettings settings)
{
    const SectorPreviewSettings defaults = DefaultSectorPreviewSettings();
    settings.walkSpeed = ClampFinite(
            settings.walkSpeed,
            PreviewWalkSpeedMin,
            PreviewWalkSpeedMax,
            defaults.walkSpeed);
    settings.runSpeed = ClampFinite(
            settings.runSpeed,
            PreviewRunSpeedMin,
            PreviewRunSpeedMax,
            defaults.runSpeed);
    settings.mouseSensitivity = ClampFinite(
            settings.mouseSensitivity,
            PreviewMouseSensitivityMin,
            PreviewMouseSensitivityMax,
            defaults.mouseSensitivity);
    settings.eyeHeight = ClampFinite(
            settings.eyeHeight,
            PreviewEyeHeightMin,
            PreviewEyeHeightMax,
            defaults.eyeHeight);
    settings.gravity = ClampFinite(
            settings.gravity,
            PreviewGravityMin,
            PreviewGravityMax,
            defaults.gravity);
    settings.playerRadius = ClampFinite(
            settings.playerRadius,
            PreviewPlayerRadiusMin,
            PreviewPlayerRadiusMax,
            defaults.playerRadius);
    settings.playerHeight = ClampFinite(
            settings.playerHeight,
            PreviewPlayerHeightMin,
            PreviewPlayerHeightMax,
            defaults.playerHeight);
    settings.playerHeight = std::max(settings.playerHeight, settings.eyeHeight);
    settings.stepHeight = ClampFinite(
            settings.stepHeight,
            PreviewStepHeightMin,
            PreviewStepHeightMax,
            defaults.stepHeight);
    settings.jumpHeight = ClampFinite(
            settings.jumpHeight,
            PreviewJumpHeightMin,
            PreviewJumpHeightMax,
            defaults.jumpHeight);
    settings.headBobStrength = ClampFinite(
            settings.headBobStrength,
            PreviewHeadBobStrengthMin,
            PreviewHeadBobStrengthMax,
            defaults.headBobStrength);
    settings.headBobFrequency = ClampFinite(
            settings.headBobFrequency,
            PreviewHeadBobFrequencyMin,
            PreviewHeadBobFrequencyMax,
            defaults.headBobFrequency);
    settings.objectProbeDebugDrawMaxDistanceWorld = ClampFinite(
            settings.objectProbeDebugDrawMaxDistanceWorld,
            PreviewObjectProbeDebugDrawMaxDistanceMin,
            PreviewObjectProbeDebugDrawMaxDistanceMax,
            defaults.objectProbeDebugDrawMaxDistanceWorld);
    return settings;
}

SectorTopologyFogSettings DefaultSectorTopologyFogSettings()
{
    return SectorTopologyFogSettings{};
}

SectorTopologyFogSettings NormalizeSectorTopologyFogSettings(SectorTopologyFogSettings settings)
{
    const SectorTopologyFogSettings defaults = DefaultSectorTopologyFogSettings();
    settings.color.a = 255;
    settings.startDistanceWorld = ClampFinite(
            settings.startDistanceWorld,
            FogStartDistanceMin,
            FogStartDistanceMax,
            defaults.startDistanceWorld);
    settings.endDistanceWorld = ClampFinite(
            settings.endDistanceWorld,
            settings.startDistanceWorld + 0.01f,
            4096.0f,
            std::max(defaults.endDistanceWorld, settings.startDistanceWorld + 0.01f));
    settings.falloffExponent = ClampFinite(
            settings.falloffExponent, 0.05f, 8.0f, defaults.falloffExponent);
    settings.brightness = ClampFinite(
            settings.brightness, 0.0f, 16.0f, defaults.brightness);
    settings.density = ClampFinite(
            settings.density,
            FogDensityMin,
            FogDensityMax,
            defaults.density);
    settings.maxOpacity = ClampFinite(
            settings.maxOpacity,
            FogMaxOpacityMin,
            FogMaxOpacityMax,
            defaults.maxOpacity);
    settings.referenceHeightWorld = ClampFinite(
            settings.referenceHeightWorld,
            FogReferenceHeightMin,
            FogReferenceHeightMax,
            defaults.referenceHeightWorld);
    settings.heightFalloff = ClampFinite(
            settings.heightFalloff,
            FogHeightFalloffMin,
            FogHeightFalloffMax,
            defaults.heightFalloff);
    return settings;
}

SectorTopologySkySettings DefaultSectorTopologySkySettings()
{
    return SectorTopologySkySettings{};
}

SectorTopologySkySettings NormalizeSectorTopologySkySettings(SectorTopologySkySettings settings)
{
    const SectorTopologySkySettings defaults = DefaultSectorTopologySkySettings();
    if (!std::isfinite(settings.yawOffsetDegrees)) {
        settings.yawOffsetDegrees = defaults.yawOffsetDegrees;
    }
    if (!std::isfinite(settings.verticalOffset)) {
        settings.verticalOffset = defaults.verticalOffset;
    }
    settings.verticalScale = ClampFinite(
            settings.verticalScale,
            SkyVerticalScaleMin,
            SkyVerticalScaleMax,
            defaults.verticalScale);
    settings.topColor.a = 255;
    return settings;
}

SectorTopologyDirectionalLightSettings DefaultSectorTopologyDirectionalLightSettings()
{
    SectorTopologyDirectionalLightSettings settings;
    settings.directionToLight = NormalizeSectorTopologyDirectionalLightSettings(settings).directionToLight;
    return settings;
}

SectorTopologyDirectionalLightSettings NormalizeSectorTopologyDirectionalLightSettings(
        SectorTopologyDirectionalLightSettings settings)
{
    const Vector3 fallback{-0.35f, 0.80f, -0.25f};
    const float fallbackLength = std::sqrt(
            fallback.x * fallback.x + fallback.y * fallback.y + fallback.z * fallback.z);
    const Vector3 defaultDirection{
            fallback.x / fallbackLength,
            fallback.y / fallbackLength,
            fallback.z / fallbackLength
    };

    const Vector3 direction = settings.directionToLight;
    const float lengthSqr = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (!std::isfinite(direction.x)
            || !std::isfinite(direction.y)
            || !std::isfinite(direction.z)
            || lengthSqr <= DirectionalLightMinLengthSqr) {
        settings.directionToLight = defaultDirection;
    } else {
        const float length = std::sqrt(lengthSqr);
        settings.directionToLight = Vector3{
                direction.x / length,
                direction.y / length,
                direction.z / length
        };
    }
    settings.color.a = 255;
    if (!std::isfinite(settings.intensity) || settings.intensity < 0.0f) {
        settings.intensity = 0.0f;
    }
    return settings;
}

SectorTopologyIndexes BuildSectorTopologyIndexes(const SectorTopologyMap& map)
{
    SectorTopologyIndexes indexes;

    for (size_t i = 0; i < map.vertices.size(); ++i) {
        indexes.vertexIndicesById[map.vertices[i].id].push_back(i);
    }
    for (size_t i = 0; i < map.lineDefs.size(); ++i) {
        indexes.lineDefIndicesById[map.lineDefs[i].id].push_back(i);
    }
    for (size_t i = 0; i < map.sideDefs.size(); ++i) {
        const SectorTopologySideDef& sideDef = map.sideDefs[i];
        indexes.sideDefIndicesById[sideDef.id].push_back(i);
        indexes.sideDefIndicesBySectorId[sideDef.sectorId].push_back(i);
        if (sideDef.side == SectorTopologySideKind::Front) {
            indexes.frontSideDefIndicesByLineDefId[sideDef.lineDefId].push_back(i);
        } else if (sideDef.side == SectorTopologySideKind::Back) {
            indexes.backSideDefIndicesByLineDefId[sideDef.lineDefId].push_back(i);
        }
    }
    for (size_t i = 0; i < map.sectors.size(); ++i) {
        indexes.sectorIndicesById[map.sectors[i].id].push_back(i);
    }

    return indexes;
}

bool IsValidSectorTopologyId(int id)
{
    return id > 0;
}

const char* SectorTopologySideKindName(SectorTopologySideKind side)
{
    switch (side) {
        case SectorTopologySideKind::Front:
            return "Front";
        case SectorTopologySideKind::Back:
            return "Back";
    }
    return "Unknown";
}

SectorTopologySideKind OppositeSectorTopologySideKind(SectorTopologySideKind side)
{
    return side == SectorTopologySideKind::Front
           ? SectorTopologySideKind::Back
           : SectorTopologySideKind::Front;
}

int AllocateSectorTopologyVertexId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.vertices);
}

int AllocateSectorTopologyLineDefId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.lineDefs);
}

int AllocateSectorTopologySideDefId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.sideDefs);
}

int AllocateSectorTopologySectorId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.sectors);
}

int AllocateSectorTopologyStaticLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.staticLights);
}

int AllocateSectorTopologyStaticSpotLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.staticSpotLights);
}

int AllocateSectorTopologyDynamicLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.dynamicPointLights);
}

int AllocateSectorTopologyDynamicSpotLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.dynamicSpotLights);
}

int AllocateSectorPlacedRuntimeObjectId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.runtimeObjects);
}

const SectorTopologyVertex* FindSectorTopologyVertex(const SectorTopologyMap& map, int id)
{
    return FindById(map.vertices, id);
}

SectorTopologyVertex* FindSectorTopologyVertex(SectorTopologyMap& map, int id)
{
    return FindById(map.vertices, id);
}

const SectorTopologyLineDef* FindSectorTopologyLineDef(const SectorTopologyMap& map, int id)
{
    return FindById(map.lineDefs, id);
}

SectorTopologyLineDef* FindSectorTopologyLineDef(SectorTopologyMap& map, int id)
{
    return FindById(map.lineDefs, id);
}

const SectorTopologySideDef* FindSectorTopologySideDef(const SectorTopologyMap& map, int id)
{
    return FindById(map.sideDefs, id);
}

SectorTopologySideDef* FindSectorTopologySideDef(SectorTopologyMap& map, int id)
{
    return FindById(map.sideDefs, id);
}

const SectorTopologySector* FindSectorTopologySector(const SectorTopologyMap& map, int id)
{
    return FindById(map.sectors, id);
}

SectorTopologySector* FindSectorTopologySector(SectorTopologyMap& map, int id)
{
    return FindById(map.sectors, id);
}

const SectorTopologyStaticPointLight* FindSectorTopologyStaticLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.staticLights, id);
}

SectorTopologyStaticPointLight* FindSectorTopologyStaticLight(SectorTopologyMap& map, int id)
{
    return FindById(map.staticLights, id);
}

bool RemoveSectorTopologyStaticLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.staticLights.begin(),
            map.staticLights.end(),
            [id](const SectorTopologyStaticPointLight& light) { return light.id == id; });
    if (found == map.staticLights.end()) {
        return false;
    }

    map.staticLights.erase(found);
    return true;
}

const SectorTopologyStaticSpotLight* FindSectorTopologyStaticSpotLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.staticSpotLights, id);
}

SectorTopologyStaticSpotLight* FindSectorTopologyStaticSpotLight(SectorTopologyMap& map, int id)
{
    return FindById(map.staticSpotLights, id);
}

bool RemoveSectorTopologyStaticSpotLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.staticSpotLights.begin(),
            map.staticSpotLights.end(),
            [id](const SectorTopologyStaticSpotLight& light) { return light.id == id; });
    if (found == map.staticSpotLights.end()) {
        return false;
    }

    map.staticSpotLights.erase(found);
    return true;
}

const SectorTopologyDynamicPointLight* FindSectorTopologyDynamicLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.dynamicPointLights, id);
}

SectorTopologyDynamicPointLight* FindSectorTopologyDynamicLight(SectorTopologyMap& map, int id)
{
    return FindById(map.dynamicPointLights, id);
}

bool RemoveSectorTopologyDynamicLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.dynamicPointLights.begin(),
            map.dynamicPointLights.end(),
            [id](const SectorTopologyDynamicPointLight& light) { return light.id == id; });
    if (found == map.dynamicPointLights.end()) {
        return false;
    }

    map.dynamicPointLights.erase(found);
    return true;
}

const SectorTopologyDynamicSpotLight* FindSectorTopologyDynamicSpotLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.dynamicSpotLights, id);
}

SectorTopologyDynamicSpotLight* FindSectorTopologyDynamicSpotLight(SectorTopologyMap& map, int id)
{
    return FindById(map.dynamicSpotLights, id);
}

bool RemoveSectorTopologyDynamicSpotLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.dynamicSpotLights.begin(),
            map.dynamicSpotLights.end(),
            [id](const SectorTopologyDynamicSpotLight& light) { return light.id == id; });
    if (found == map.dynamicSpotLights.end()) {
        return false;
    }

    map.dynamicSpotLights.erase(found);
    return true;
}

const SectorPlacedRuntimeObject* FindSectorPlacedRuntimeObject(const SectorTopologyMap& map, int id)
{
    return FindById(map.runtimeObjects, id);
}

SectorPlacedRuntimeObject* FindSectorPlacedRuntimeObject(SectorTopologyMap& map, int id)
{
    return FindById(map.runtimeObjects, id);
}

bool RemoveSectorPlacedRuntimeObject(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.runtimeObjects.begin(),
            map.runtimeObjects.end(),
            [id](const SectorPlacedRuntimeObject& object) { return object.id == id; });
    if (found == map.runtimeObjects.end()) {
        return false;
    }

    map.runtimeObjects.erase(found);
    return true;
}

const SectorCompiledLevelMarker* FindSectorCompiledLevelMarker(
        const SectorTopologyMap& map,
        const std::string& id)
{
    for (const SectorCompiledLevelMarker& marker : map.levelMarkers) {
        if (marker.id == id) {
            return &marker;
        }
    }
    return nullptr;
}

SectorResolvedDoorAnchor ResolveSectorDoorAnchor(
        const SectorTopologyMap& map,
        const SectorPlacedDoor& door)
{
    SectorResolvedDoorAnchor resolved;

    const SectorDoorAnchor& anchor = door.anchor;
    resolved.lineDefId = anchor.lineDefId;
    resolved.frontSectorId = anchor.frontSectorId;
    resolved.backSectorId = anchor.backSectorId;
    resolved.frontSideDefId = anchor.frontSideDefId;
    resolved.backSideDefId = anchor.backSideDefId;
    resolved.width = door.width;
    resolved.height = door.height;

    const auto fail = [&resolved](std::string diagnostic) {
        resolved.valid = false;
        resolved.diagnostic = std::move(diagnostic);
        return resolved;
    };

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, anchor.lineDefId);
    if (lineDef == nullptr) {
        return fail("door anchor linedef is missing");
    }
    if (!IsValidSectorTopologyId(lineDef->frontSideDefId)
            || !IsValidSectorTopologyId(lineDef->backSideDefId)) {
        return fail("door anchor linedef is not a two-sided portal");
    }
    if (lineDef->frontSideDefId != anchor.frontSideDefId
            || lineDef->backSideDefId != anchor.backSideDefId) {
        return fail("door anchor sidedef IDs no longer match the linedef");
    }

    const SectorTopologySideDef* frontSide = FindSectorTopologySideDef(map, lineDef->frontSideDefId);
    const SectorTopologySideDef* backSide = FindSectorTopologySideDef(map, lineDef->backSideDefId);
    if (frontSide == nullptr || backSide == nullptr) {
        return fail("door anchor sidedef is missing");
    }
    if (frontSide->lineDefId != lineDef->id
            || frontSide->side != SectorTopologySideKind::Front
            || backSide->lineDefId != lineDef->id
            || backSide->side != SectorTopologySideKind::Back) {
        return fail("door anchor sidedefs are not the linedef front/back pair");
    }
    if (frontSide->sectorId != anchor.frontSectorId
            || backSide->sectorId != anchor.backSectorId) {
        return fail("door anchor sector pair no longer matches the portal");
    }

    const SectorTopologySector* frontSector = FindSectorTopologySector(map, frontSide->sectorId);
    const SectorTopologySector* backSector = FindSectorTopologySector(map, backSide->sectorId);
    if (frontSector == nullptr || backSector == nullptr) {
        return fail("door anchor sector is missing");
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(map, *lineDef, start, end)) {
        return fail("door anchor linedef vertex is missing");
    }

    resolved.endpointA = SectorCoordToWorldPosition2Local(start->x, start->y);
    resolved.endpointB = SectorCoordToWorldPosition2Local(end->x, end->y);
    const Vector2 delta{
            resolved.endpointB.x - resolved.endpointA.x,
            resolved.endpointB.y - resolved.endpointA.y
    };
    const float lengthSqr = delta.x * delta.x + delta.y * delta.y;
    if (!std::isfinite(lengthSqr) || lengthSqr <= 0.0f) {
        return fail("door anchor portal has zero length");
    }

    resolved.portalWidth = std::sqrt(lengthSqr);
    resolved.tangent = Vector2{
            delta.x / resolved.portalWidth,
            delta.y / resolved.portalWidth
    };
    resolved.midpoint = Vector2{
            (resolved.endpointA.x + resolved.endpointB.x) * 0.5f,
            (resolved.endpointA.y + resolved.endpointB.y) * 0.5f
    };

    // Front sidedefs use the linedef start->end direction; back sidedefs use end->start.
    // A sidedef's sector lies on the positive side of that directed edge. Resolve and
    // verify the door normal so positive normalOffset always moves front sector -> back sector.
    Vector2 candidateNormal{resolved.tangent.y, -resolved.tangent.x};
    const Vector2 backTangent{-resolved.tangent.x, -resolved.tangent.y};
    const auto pointsTowardBack = [&](Vector2 normal) {
        const Vector2 probe{
                resolved.midpoint.x + normal.x * DoorAnchorSideProbeDistance,
                resolved.midpoint.y + normal.y * DoorAnchorSideProbeDistance};
        return ProbeIsOutsideDirectedSide(resolved.endpointA, resolved.tangent, probe)
                && ProbeIsInsideDirectedSide(resolved.endpointB, backTangent, probe);
    };
    if (!pointsTowardBack(candidateNormal)) {
        candidateNormal = Vector2{-candidateNormal.x, -candidateNormal.y};
        if (!pointsTowardBack(candidateNormal)) {
            return fail("door anchor normal could not be verified against portal front/back sides");
        }
    }
    resolved.normal = candidateNormal;

    resolved.openBottom = SectorAuthoringToWorldDistance(std::max(frontSector->floorZ, backSector->floorZ));
    resolved.openTop = SectorAuthoringToWorldDistance(std::min(frontSector->ceilingZ, backSector->ceilingZ));
    if (!std::isfinite(resolved.openBottom)
            || !std::isfinite(resolved.openTop)
            || resolved.openBottom >= resolved.openTop) {
        return fail("door anchor portal has no positive vertical opening");
    }

    resolved.portalHeight = resolved.openTop - resolved.openBottom;
    if (resolved.width == 0.0f) {
        resolved.width = resolved.portalWidth;
    }
    if (resolved.height == 0.0f) {
        resolved.height = resolved.portalHeight;
    }
    resolved.valid = true;
    resolved.diagnostic.clear();
    return resolved;
}

const SectorTopologySideDef* FindOppositeSectorTopologySideDef(
        const SectorTopologyMap& map,
        int sideDefId)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef->lineDefId);
    if (lineDef == nullptr) {
        return nullptr;
    }

    int oppositeSideDefId = -1;
    SectorTopologySideKind expectedOppositeKind = SectorTopologySideKind::Front;
    if (sideDef->side == SectorTopologySideKind::Front) {
        if (lineDef->frontSideDefId != sideDef->id) {
            return nullptr;
        }
        oppositeSideDefId = lineDef->backSideDefId;
        expectedOppositeKind = SectorTopologySideKind::Back;
    } else if (sideDef->side == SectorTopologySideKind::Back) {
        if (lineDef->backSideDefId != sideDef->id) {
            return nullptr;
        }
        oppositeSideDefId = lineDef->frontSideDefId;
        expectedOppositeKind = SectorTopologySideKind::Front;
    } else {
        return nullptr;
    }

    const SectorTopologySideDef* opposite = FindSectorTopologySideDef(map, oppositeSideDefId);
    if (opposite == nullptr
        || opposite->lineDefId != lineDef->id
        || opposite->side != expectedOppositeKind) {
        return nullptr;
    }
    return opposite;
}

bool GetSectorTopologyLineVertices(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& line,
        const SectorTopologyVertex*& outStart,
        const SectorTopologyVertex*& outEnd)
{
    outStart = nullptr;
    outEnd = nullptr;

    const SectorTopologyVertex* start = FindSectorTopologyVertex(map, line.startVertexId);
    const SectorTopologyVertex* end = FindSectorTopologyVertex(map, line.endVertexId);
    if (start == nullptr || end == nullptr) {
        return false;
    }

    outStart = start;
    outEnd = end;
    return true;
}

} // namespace game
