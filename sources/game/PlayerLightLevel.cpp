#include "game/PlayerLightLevel.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorRectLight.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr float OcclusionEpsilon = 0.03f;

float Luminance(Vector3 color)
{
    if (!std::isfinite(color.x) || !std::isfinite(color.y)
            || !std::isfinite(color.z)) return 0.0f;
    return std::max(0.0f,
            color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f);
}

float BakedAmbientCubeLuminance(const BakedObjectLightingSample& sample)
{
    float total = 0.0f;
    for (const Vector3& face : sample.ambientCube) {
        total += Luminance(face);
    }
    return total / 6.0f;
}

bool SegmentIntersectsPrism(
        Vector3 origin,
        Vector3 direction,
        float maximumDistance,
        Vector2 center,
        Vector2 axisX,
        Vector2 axisZ,
        Vector2 halfExtents,
        float bottom,
        float top)
{
    const Vector2 relative{origin.x - center.x, origin.z - center.y};
    const float localOrigin[3] = {
            Vector2DotProduct(relative, axisX),
            origin.y,
            Vector2DotProduct(relative, axisZ)};
    const float localDirection[3] = {
            direction.x * axisX.x + direction.z * axisX.y,
            direction.y,
            direction.x * axisZ.x + direction.z * axisZ.y};
    const float minimum[3] = {-halfExtents.x, bottom, -halfExtents.y};
    const float maximum[3] = {halfExtents.x, top, halfExtents.y};
    float enter = 0.0f;
    float leave = maximumDistance;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(localDirection[axis]) <= 0.000001f) {
            if (localOrigin[axis] < minimum[axis]
                    || localOrigin[axis] > maximum[axis]) return false;
            continue;
        }
        float a = (minimum[axis] - localOrigin[axis]) / localDirection[axis];
        float b = (maximum[axis] - localOrigin[axis]) / localDirection[axis];
        if (a > b) std::swap(a, b);
        enter = std::max(enter, a);
        leave = std::min(leave, b);
        if (enter > leave) return false;
    }
    return enter >= 0.0f
            && enter < maximumDistance - OcclusionEpsilon;
}

bool IsOccluded(
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const std::vector<SectorStaticModelCollider>& dynamicColliders,
        Vector3 origin,
        Vector3 target)
{
    const Vector3 delta = Vector3Subtract(target, origin);
    const float distance = Vector3Length(delta);
    if (distance <= OcclusionEpsilon) return false;
    const Vector3 direction = Vector3Scale(delta, 1.0f / distance);
    const SectorCollisionRayHit sectorHit = collisionWorld.Raycast(
            origin, direction, distance);
    if (sectorHit.hit && sectorHit.distance < distance - OcclusionEpsilon) {
        return true;
    }
    for (const SectorDynamicDoorCollider& door : doorColliders) {
        if (SegmentIntersectsPrism(
                    origin, direction, distance,
                    door.center, door.tangent, door.normal,
                    door.halfExtents, door.bottom, door.top)) return true;
    }
    const auto intersectsModels = [&](
            const std::vector<SectorStaticModelCollider>& colliders) {
        for (const SectorStaticModelCollider& model : colliders) {
            if (!model.resolved || model.failed) continue;
            if (SegmentIntersectsPrism(
                        origin, direction, distance,
                        model.center, model.axisX, model.axisZ,
                        model.halfExtents, model.bottom, model.top)) {
                return true;
            }
        }
        return false;
    };
    return intersectsModels(staticColliders)
            || intersectsModels(dynamicColliders);
}

float SmoothStep(float edge0, float edge1, float value)
{
    if (edge0 == edge1) return value < edge1 ? 0.0f : 1.0f;
    const float t = std::clamp(
            (value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float DynamicLightContribution(
        const SectorPreviewDynamicPointLightUniform& light,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const std::vector<SectorStaticModelCollider>& dynamicColliders,
        Vector3 samplePosition,
        float runtimeSeconds)
{
    Vector3 target = light.position;
    float emitterAttenuation = 1.0f;
    if (light.kind == SectorPreviewDynamicLightKind::Rect) {
        Vector3 forward = Vector3LengthSqr(light.direction) > 0.00000001f
                ? Vector3Normalize(light.direction)
                : Vector3{0.0f, -1.0f, 0.0f};
        Vector3 right = Vector3LengthSqr(light.rectRight) > 0.00000001f
                ? Vector3Normalize(light.rectRight)
                : Vector3{1.0f, 0.0f, 0.0f};
        Vector3 up = Vector3CrossProduct(right, forward);
        up = Vector3LengthSqr(up) > 0.00000001f
                ? Vector3Normalize(up)
                : Vector3{0.0f, 0.0f, 1.0f};
        const Vector3 relative = Vector3Subtract(
                samplePosition, light.position);
        target = Vector3Add(
                light.position,
                Vector3Add(
                        Vector3Scale(right, std::clamp(
                                Vector3DotProduct(relative, right),
                                -light.innerConeCos, light.innerConeCos)),
                        Vector3Scale(up, std::clamp(
                                Vector3DotProduct(relative, up),
                                -light.outerConeCos, light.outerConeCos))));
        const Vector3 fromEmitter = Vector3Subtract(samplePosition, target);
        emitterAttenuation = Vector3LengthSqr(fromEmitter) > 0.00000001f
                ? std::max(0.0f, Vector3DotProduct(
                        forward, Vector3Normalize(fromEmitter)))
                : 1.0f;
    }

    const Vector3 delta = Vector3Subtract(target, samplePosition);
    const float distance = Vector3Length(delta);
    if (!(light.radius > 0.0f) || distance >= light.radius) return 0.0f;
    float attenuation = std::clamp(
            1.0f - distance / light.radius, 0.0f, 1.0f);
    attenuation *= attenuation;

    float coneAttenuation = emitterAttenuation;
    if (light.kind == SectorPreviewDynamicLightKind::Spot) {
        const Vector3 direction = Vector3LengthSqr(light.direction)
                        > 0.00000001f
                ? Vector3Normalize(light.direction)
                : Vector3{0.0f, -1.0f, 0.0f};
        const Vector3 fromLight = Vector3Subtract(
                samplePosition, light.position);
        const float coneDot = Vector3LengthSqr(fromLight) > 0.00000001f
                ? Vector3DotProduct(direction, Vector3Normalize(fromLight))
                : 1.0f;
        coneAttenuation = std::fabs(
                light.innerConeCos - light.outerConeCos) > 0.0001f
                ? SmoothStep(
                        light.outerConeCos, light.innerConeCos, coneDot)
                : (coneDot >= light.innerConeCos ? 1.0f : 0.0f);
    }
    if (coneAttenuation <= 0.0f) return 0.0f;

    if (light.castsShadow && light.shadowStrength > 0.0f
            && IsOccluded(
                    collisionWorld, doorColliders, staticColliders,
                    dynamicColliders, samplePosition, target)) {
        coneAttenuation *= 1.0f
                - std::clamp(light.shadowStrength, 0.0f, 1.0f);
    }
    const float intensity = DynamicLightEffectiveUploadIntensity(
            light, runtimeSeconds);
    const float value = Luminance(light.color) * intensity
            * attenuation * coneAttenuation;
    return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
}

float SampleDynamicLights(
        const SectorTopologyMap& map,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const std::vector<SectorStaticModelCollider>& dynamicColliders,
        const SectorPreviewDynamicPointLightSource* runtimePointLight,
        Vector3 position,
        float runtimeSeconds)
{
    float total = 0.0f;
    SectorPreviewDynamicPointLightUniform light;
    for (const SectorTopologyDynamicPointLight& source : map.dynamicPointLights) {
        if (MakeSectorPreviewDynamicPointLightUniform(source, light)) {
            total += DynamicLightContribution(
                    light, collisionWorld, doorColliders, staticColliders,
                    dynamicColliders, position, runtimeSeconds);
        }
    }
    for (const SectorTopologyDynamicSpotLight& source : map.dynamicSpotLights) {
        if (MakeSectorPreviewDynamicSpotLightUniform(source, light)) {
            total += DynamicLightContribution(
                    light, collisionWorld, doorColliders, staticColliders,
                    dynamicColliders, position, runtimeSeconds);
        }
    }
    for (const SectorTopologyDynamicRectLight& source : map.dynamicRectLights) {
        if (MakeSectorPreviewDynamicRectLightUniform(source, light)) {
            total += DynamicLightContribution(
                    light, collisionWorld, doorColliders, staticColliders,
                    dynamicColliders, position, runtimeSeconds);
        }
    }
    if (runtimePointLight != nullptr) {
        total += DynamicLightContribution(
                runtimePointLight->light,
                collisionWorld, doorColliders, staticColliders,
                dynamicColliders, position, runtimeSeconds);
    }
    return total;
}

} // namespace

PlayerLightLevelSample SamplePlayerLightLevel(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorTopologyMap& map,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const std::vector<SectorStaticModelCollider>& dynamicColliders,
        const SectorPreviewDynamicPointLightSource* runtimePointLight,
        Vector3 playerFeetPosition,
        float playerHeight,
        float playerRadius,
        int playerSectorId,
        float fullVisibilityLightLevel,
        float runtimeSeconds)
{
    PlayerLightLevelSample result;
    const float height = std::isfinite(playerHeight)
            ? std::max(0.001f, playerHeight) : 1.6f;
    const float radius = std::isfinite(playerRadius)
            ? std::clamp(playerRadius, 0.0f, height * 0.5f) : 0.25f;
    const float lowerY = playerFeetPosition.y + radius;
    const float upperY = playerFeetPosition.y + height - radius;
    const float sampleHeights[PlayerLightSamplePointCount] = {
            lowerY,
            playerFeetPosition.y + height * 0.5f,
            std::max(lowerY, upperY)};
    const float reference = std::isfinite(fullVisibilityLightLevel)
                    && fullVisibilityLightLevel > 0.0f
            ? fullVisibilityLightLevel : 1.0f;
    result.bakedProbeAvailable = !probes.probes.empty();
    for (size_t index = 0; index < result.points.size(); ++index) {
        PlayerLightPointSample& point = result.points[index];
        point.positionWorld = Vector3{
                playerFeetPosition.x,
                sampleHeights[index],
                playerFeetPosition.z};
        const BakedObjectLightingSample baked = SampleBakedObjectLighting(
                probes, point.positionWorld, playerSectorId, &map);
        point.bakedLight = BakedAmbientCubeLuminance(baked);
        point.dynamicLight = SampleDynamicLights(
                map, collisionWorld, doorColliders, staticColliders,
                dynamicColliders, runtimePointLight,
                point.positionWorld, runtimeSeconds);
        point.combinedLight = std::max(
                0.0f, point.bakedLight + point.dynamicLight);
        point.normalizedLight = std::clamp(
                point.combinedLight / reference, 0.0f, 1.0f);
        result.bakedLight += point.bakedLight;
        result.dynamicLight += point.dynamicLight;
        result.combinedLight += point.combinedLight;
    }
    const float inverseCount = 1.0f
            / static_cast<float>(result.points.size());
    result.bakedLight *= inverseCount;
    result.dynamicLight *= inverseCount;
    result.combinedLight *= inverseCount;
    result.normalizedLight = std::clamp(
            result.combinedLight / reference, 0.0f, 1.0f);
    return result;
}

} // namespace game
