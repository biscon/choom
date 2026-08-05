#pragma once

#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace game {

class SectorCollisionWorld;
struct SectorBillboardDynamicLightContext;
struct SectorReceiverBounds;

enum class SectorLightAtmosphereSourceKind {
    StaticPoint,
    StaticSpot,
    DynamicPoint,
    DynamicSpot
};

enum class SectorLightAtmosphereShape {
    Sphere,
    Cone
};

struct SectorLightAtmosphereSource {
    SectorLightAtmosphereSourceKind kind = SectorLightAtmosphereSourceKind::StaticPoint;
    SectorLightAtmosphereShape shape = SectorLightAtmosphereShape::Sphere;
    int lightId = 0;
    int ownerSectorId = 0;
    Vector3 positionWorld = {};
    Vector3 directionWorld = {0.0f, -1.0f, 0.0f};
    float rangeWorld = 0.0f;
    float outerConeCos = -1.0f;
    SectorLightAtmosphereSettings atmosphere;
};

struct SectorLightAtmosphereVolume {
    const SectorLightAtmosphereSource* source = nullptr;
    Vector3 originWorld = {};
    Vector3 directionWorld = {0.0f, -1.0f, 0.0f};
    Vector3 boundsCenterWorld = {};
    float boundsRadiusWorld = 0.0f;
    float extentWorld = 0.0f;
    float coneRadiusWorld = 0.0f;
};

constexpr float SectorLightAtmosphereMaximumConeHalfAngleDegrees = 85.0f;

void BuildSectorLightAtmosphereSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld,
        std::vector<SectorLightAtmosphereSource>& outSources);

bool MakeSectorLightAtmosphereVolume(
        const SectorLightAtmosphereSource& source,
        float extentScale,
        SectorLightAtmosphereVolume& outVolume);

bool IsSectorLightAtmosphereSourceDynamic(const SectorLightAtmosphereSource& source);
bool IsSectorLightAtmosphereSourceSelected(
        const SectorLightAtmosphereSource& source,
        const SectorBillboardDynamicLightContext& dynamicLights);

bool IsPointInsideSectorLightAtmosphereVolume(
        const SectorLightAtmosphereVolume& volume,
        Vector3 worldPosition);

bool IsSectorLightAtmosphereVolumeVisible(
        const SectorLightAtmosphereVolume& volume,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        const Camera3D& camera,
        float aspectRatio,
        float nearPlane,
        float farPlane);

} // namespace game
