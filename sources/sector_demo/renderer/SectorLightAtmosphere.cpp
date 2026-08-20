#include "sector_demo/renderer/SectorLightAtmosphere.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorRectLight.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorUnits.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

Vector3 NormalizeDirection(Vector3 value)
{
    return Vector3LengthSqr(value) > 0.00000001f
            ? Vector3Normalize(value)
            : Vector3{0.0f, -1.0f, 0.0f};
}

int OwnerSector(const SectorCollisionWorld* world, Vector3 position)
{
    return world != nullptr
            ? world->FindSectorContainingPoint(Vector2{position.x, position.z})
            : 0;
}

bool ContainsSector(const RuntimePortalVisibilityResult& visibility, int sectorId)
{
    return std::find(visibility.visibleSectorIds.begin(), visibility.visibleSectorIds.end(), sectorId)
            != visibility.visibleSectorIds.end();
}

float DistanceSquaredToBounds(Vector3 point, const SectorReceiverBounds& bounds)
{
    const float x = std::clamp(point.x, bounds.min.x, bounds.max.x);
    const float y = std::clamp(point.y, bounds.min.y, bounds.max.y);
    const float z = std::clamp(point.z, bounds.min.z, bounds.max.z);
    return Vector3LengthSqr(Vector3Subtract(point, Vector3{x, y, z}));
}

bool SphereIntersectsVisibleReceivers(
        Vector3 center,
        float radius,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& bounds)
{
    const float radiusSquared = radius * radius;
    for (const SectorReceiverBounds& receiver : bounds) {
        if (receiver.sectorId <= 0
                || (!visibility.fallbackDrawAll
                    && visibility.validStartSector
                    && !ContainsSector(visibility, receiver.sectorId))) {
            continue;
        }
        if (DistanceSquaredToBounds(center, receiver) <= radiusSquared) return true;
    }
    return false;
}

bool SphereIntersectsCameraFrustum(
        Vector3 center,
        float radius,
        const Camera3D& camera,
        float aspect,
        float nearPlane,
        float farPlane)
{
    const Vector3 forward = NormalizeDirection(Vector3Subtract(camera.target, camera.position));
    Vector3 right = Vector3CrossProduct(forward, camera.up);
    right = Vector3LengthSqr(right) > 0.00000001f
            ? Vector3Normalize(right)
            : Vector3{1.0f, 0.0f, 0.0f};
    const Vector3 up = NormalizeDirection(Vector3CrossProduct(right, forward));
    const Vector3 relative = Vector3Subtract(center, camera.position);
    const float depth = Vector3DotProduct(relative, forward);
    if (depth + radius < nearPlane || depth - radius > farPlane) return false;
    const float tanVertical = std::tan(camera.fovy * DEG2RAD * 0.5f);
    const float halfHeight = std::max(depth, 0.0f) * tanVertical;
    const float halfWidth = halfHeight * std::max(aspect, 0.001f);
    return std::fabs(Vector3DotProduct(relative, right)) <= halfWidth + radius
            && std::fabs(Vector3DotProduct(relative, up)) <= halfHeight + radius;
}

template<typename Light>
SectorLightAtmosphereSource MakePointSource(
        const Light& light,
        SectorLightAtmosphereSourceKind kind,
        const SectorCollisionWorld* sectorLookupWorld)
{
    SectorLightAtmosphereSource source;
    source.kind = kind;
    source.shape = SectorLightAtmosphereShape::Sphere;
    source.lightId = light.id;
    source.positionWorld = SectorAuthoringToWorldPosition(light.position);
    source.rangeWorld = SectorAuthoringToWorldDistance(light.radius);
    source.color = light.color;
    source.intensity = light.intensity;
    source.ownerSectorId = OwnerSector(sectorLookupWorld, source.positionWorld);
    source.atmosphere = NormalizeSectorLightAtmosphereSettings(light.atmosphere);
    return source;
}

template<typename Light>
SectorLightAtmosphereSource MakeSpotSource(
        const Light& light,
        SectorLightAtmosphereSourceKind kind,
        const SectorCollisionWorld* sectorLookupWorld)
{
    SectorLightAtmosphereSource source;
    source.kind = kind;
    source.shape = SectorLightAtmosphereShape::Cone;
    source.lightId = light.id;
    source.positionWorld = SectorAuthoringToWorldPosition(light.position);
    const Vector3 targetWorld = SectorAuthoringToWorldPosition(light.target);
    source.directionWorld = NormalizeDirection(Vector3Subtract(targetWorld, source.positionWorld));
    source.rangeWorld = SectorAuthoringToWorldDistance(light.range);
    source.outerConeCos = std::cos(std::clamp(light.outerConeDegrees, 0.0f, 179.0f) * DEG2RAD);
    source.color = light.color;
    source.intensity = light.intensity;
    source.ownerSectorId = OwnerSector(sectorLookupWorld, source.positionWorld);
    source.atmosphere = NormalizeSectorLightAtmosphereSettings(light.atmosphere);
    return source;
}

template<typename Light>
SectorLightAtmosphereSource MakeRectSource(
        const Light& light,
        SectorLightAtmosphereSourceKind kind,
        const SectorCollisionWorld* sectorLookupWorld)
{
    SectorLightAtmosphereSource source;
    source.kind = kind;
    source.shape = SectorLightAtmosphereShape::RectPrism;
    source.lightId = light.id;
    source.positionWorld = SectorAuthoringToWorldPosition(light.position);
    const Vector3 target = SectorAuthoringToWorldPosition(light.target);
    const SectorRectLightBasis basis = BuildSectorRectLightBasis(
            source.positionWorld, target, light.rollDegrees);
    source.directionWorld = basis.forward;
    source.rightWorld = basis.right;
    source.widthWorld = SectorAuthoringToWorldDistance(light.width);
    source.heightWorld = SectorAuthoringToWorldDistance(light.height);
    source.rangeWorld = SectorAuthoringToWorldDistance(light.range);
    source.color = light.color;
    source.intensity = light.intensity;
    source.ownerSectorId = OwnerSector(sectorLookupWorld, source.positionWorld);
    source.atmosphere = NormalizeSectorLightAtmosphereSettings(light.atmosphere);
    return source;
}

} // namespace

void BuildSectorLightAtmosphereSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld,
        std::vector<SectorLightAtmosphereSource>& outSources)
{
    outSources.clear();
    outSources.reserve(
            map.staticLights.size()
            + map.staticSpotLights.size()
            + map.staticRectLights.size()
            + map.dynamicPointLights.size()
            + map.dynamicSpotLights.size()
            + map.dynamicRectLights.size());
    for (const SectorTopologyStaticPointLight& light : map.staticLights) {
        if (light.atmosphere.proxy.halo.enabled
                || light.atmosphere.dust.enabled) {
            outSources.push_back(MakePointSource(light, SectorLightAtmosphereSourceKind::StaticPoint, sectorLookupWorld));
        }
    }
    for (const SectorTopologyStaticSpotLight& light : map.staticSpotLights) {
        if (light.atmosphere.proxy.halo.enabled
                || light.atmosphere.proxy.shaft.enabled || light.atmosphere.dust.enabled) {
            outSources.push_back(MakeSpotSource(light, SectorLightAtmosphereSourceKind::StaticSpot, sectorLookupWorld));
        }
    }
    for (const SectorTopologyStaticRectLight& light : map.staticRectLights) {
        if (light.atmosphere.proxy.halo.enabled
                || light.atmosphere.proxy.shaft.enabled || light.atmosphere.dust.enabled) {
            outSources.push_back(MakeRectSource(
                    light, SectorLightAtmosphereSourceKind::StaticRect, sectorLookupWorld));
        }
    }
    for (const SectorTopologyDynamicPointLight& light : map.dynamicPointLights) {
        if (light.enabled && (light.atmosphere.proxy.halo.enabled
                || light.atmosphere.dust.enabled)) {
            outSources.push_back(MakePointSource(light, SectorLightAtmosphereSourceKind::DynamicPoint, sectorLookupWorld));
        }
    }
    for (const SectorTopologyDynamicSpotLight& light : map.dynamicSpotLights) {
        if (light.enabled && (light.atmosphere.proxy.halo.enabled
                || light.atmosphere.proxy.shaft.enabled || light.atmosphere.dust.enabled)) {
            outSources.push_back(MakeSpotSource(light, SectorLightAtmosphereSourceKind::DynamicSpot, sectorLookupWorld));
        }
    }
    for (const SectorTopologyDynamicRectLight& light : map.dynamicRectLights) {
        if (light.enabled && (light.atmosphere.proxy.halo.enabled
                || light.atmosphere.proxy.shaft.enabled || light.atmosphere.dust.enabled)) {
            outSources.push_back(MakeRectSource(
                    light, SectorLightAtmosphereSourceKind::DynamicRect, sectorLookupWorld));
        }
    }
}

bool MakeSectorLightAtmosphereVolume(
        const SectorLightAtmosphereSource& source,
        float extentScale,
        float heightOffsetWorld,
        SectorLightAtmosphereVolume& outVolume)
{
    return MakeSectorLightAtmosphereVolume(
            source,
            extentScale,
            Vector3{0.0f, heightOffsetWorld, 0.0f},
            outVolume);
}

bool MakeSectorLightAtmosphereVolume(
        const SectorLightAtmosphereSource& source,
        float extentScale,
        Vector3 originOffsetWorld,
        SectorLightAtmosphereVolume& outVolume)
{
    const float extent = source.rangeWorld * std::clamp(extentScale, 0.05f, 2.0f);
    if (!std::isfinite(extent) || extent <= 0.0f
            || !std::isfinite(originOffsetWorld.x)
            || !std::isfinite(originOffsetWorld.y)
            || !std::isfinite(originOffsetWorld.z)) return false;
    const Vector3 originWorld = Vector3Add(source.positionWorld, originOffsetWorld);
    outVolume = SectorLightAtmosphereVolume{};
    outVolume.source = &source;
    outVolume.originWorld = originWorld;
    outVolume.directionWorld = NormalizeDirection(source.directionWorld);
    outVolume.extentWorld = extent;
    if (source.shape == SectorLightAtmosphereShape::Sphere) {
        outVolume.boundsCenterWorld = originWorld;
        outVolume.boundsRadiusWorld = extent;
        return true;
    }
    if (source.shape == SectorLightAtmosphereShape::RectPrism) {
        outVolume.rightWorld = NormalizeDirection(source.rightWorld);
        outVolume.upWorld = NormalizeDirection(Vector3CrossProduct(
                outVolume.rightWorld, outVolume.directionWorld));
        outVolume.halfWidthWorld = source.widthWorld * 0.5f;
        outVolume.halfHeightWorld = source.heightWorld * 0.5f;
        outVolume.boundsCenterWorld = Vector3Add(originWorld,
                Vector3Scale(outVolume.directionWorld, extent * 0.5f));
        outVolume.boundsRadiusWorld = std::sqrt(extent * extent * 0.25f
                + outVolume.halfWidthWorld * outVolume.halfWidthWorld
                + outVolume.halfHeightWorld * outVolume.halfHeightWorld);
        return std::isfinite(outVolume.boundsRadiusWorld) && outVolume.boundsRadiusWorld > 0.0f;
    }
    const float authoredAngle = std::acos(std::clamp(source.outerConeCos, -1.0f, 1.0f)) * RAD2DEG;
    const float proxyAngle = std::min(authoredAngle, SectorLightAtmosphereMaximumConeHalfAngleDegrees);
    outVolume.coneRadiusWorld = std::tan(proxyAngle * DEG2RAD) * extent;
    outVolume.boundsCenterWorld = Vector3Add(
            originWorld,
            Vector3Scale(outVolume.directionWorld, extent * 0.5f));
    outVolume.boundsRadiusWorld = std::sqrt(
            extent * extent * 0.25f + outVolume.coneRadiusWorld * outVolume.coneRadiusWorld);
    return std::isfinite(outVolume.boundsRadiusWorld) && outVolume.boundsRadiusWorld > 0.0f;
}

bool IsSectorLightAtmosphereSourceDynamic(const SectorLightAtmosphereSource& source)
{
    return source.kind == SectorLightAtmosphereSourceKind::DynamicPoint
            || source.kind == SectorLightAtmosphereSourceKind::DynamicSpot
            || source.kind == SectorLightAtmosphereSourceKind::DynamicRect;
}

bool IsSectorLightAtmosphereSourceSelected(
        const SectorLightAtmosphereSource& source,
        const SectorBillboardDynamicLightContext& dynamicLights)
{
    if (!IsSectorLightAtmosphereSourceDynamic(source)) return true;
    const int expectedType = source.kind == SectorLightAtmosphereSourceKind::DynamicSpot ? 1
            : source.kind == SectorLightAtmosphereSourceKind::DynamicRect ? 2 : 0;
    for (int index = 0; index < dynamicLights.dynamicLightCount; ++index) {
        if (dynamicLights.dynamicLightIds[static_cast<std::size_t>(index)] == source.lightId
                && dynamicLights.dynamicLightTypes[static_cast<std::size_t>(index)] == expectedType) {
            return true;
        }
    }
    return false;
}

bool IsPointInsideSectorLightAtmosphereVolume(
        const SectorLightAtmosphereVolume& volume,
        Vector3 worldPosition)
{
    if (volume.source == nullptr) return false;
    const Vector3 offset = Vector3Subtract(worldPosition, volume.originWorld);
    if (volume.source->shape == SectorLightAtmosphereShape::Sphere) {
        return Vector3LengthSqr(offset) <= volume.extentWorld * volume.extentWorld;
    }
    if (volume.source->shape == SectorLightAtmosphereShape::RectPrism) {
        const float axial = Vector3DotProduct(offset, volume.directionWorld);
        return axial >= 0.0f && axial <= volume.extentWorld
                && std::fabs(Vector3DotProduct(offset, volume.rightWorld)) <= volume.halfWidthWorld
                && std::fabs(Vector3DotProduct(offset, volume.upWorld)) <= volume.halfHeightWorld;
    }
    const float axial = Vector3DotProduct(offset, volume.directionWorld);
    if (axial < 0.0f || axial > volume.extentWorld) return false;
    const Vector3 radial = Vector3Subtract(offset, Vector3Scale(volume.directionWorld, axial));
    const float allowedRadius = volume.coneRadiusWorld * axial / std::max(volume.extentWorld, 0.0001f);
    return Vector3LengthSqr(radial) <= allowedRadius * allowedRadius;
}

bool IsSectorLightAtmosphereVolumeVisible(
        const SectorLightAtmosphereVolume& volume,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        const Camera3D& camera,
        float aspectRatio,
        float nearPlane,
        float farPlane)
{
    if (volume.source == nullptr
            || !SphereIntersectsCameraFrustum(
                    volume.boundsCenterWorld,
                    volume.boundsRadiusWorld,
                    camera,
                    aspectRatio,
                    nearPlane,
                    farPlane)) {
        return false;
    }
    if (!visibility.validStartSector || visibility.fallbackDrawAll) return true;
    if (volume.source->ownerSectorId > 0
            && ContainsSector(visibility, volume.source->ownerSectorId)) {
        return true;
    }
    return SphereIntersectsVisibleReceivers(
            volume.boundsCenterWorld,
            volume.boundsRadiusWorld,
            visibility,
            receiverBounds);
}

} // namespace game
