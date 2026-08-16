#include "game/npc/NpcCombatSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "game/Health.h"
#include "game/npc/NpcNavigationSystem.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace game {
namespace {

constexpr float NpcRadius = 0.25f;
constexpr float NpcHeight = 1.6f;
constexpr float KnockbackDampingPerSecond = 12.0f;
constexpr float RayEpsilon = 0.00001f;

SectorFpsVerticalContext BuildCombatVerticalContext(
        const SectorCollisionWorld& collisionWorld,
        int sectorId)
{
    SectorFpsVerticalContext result;
    SectorCollisionHeights heights;
    if (sectorId != 0
            && collisionWorld.GetSectorFloorCeiling(sectorId, &heights)) {
        result.hasSector = true;
        result.floorZ = heights.floorZ;
        result.ceilingZ = heights.ceilingZ;
    }
    return result;
}

struct RayCandidate {
    bool hit = false;
    float distance = std::numeric_limits<float>::max();
    Vector3 position{};
    Vector3 normal{};
    FpsShotHitKind kind = FpsShotHitKind::None;
    FpsShotSurfaceKind surfaceKind = FpsShotSurfaceKind::None;
    int sectorId = 0;
    int lineDefId = 0;
    int sideDefId = 0;
    int neighborSectorId = 0;
    int placedObjectId = 0;
    engine::Entity entity = engine::NullEntity();
    engine::AnimatedModelSurfaceAnchor surfaceAnchor;
};

FpsShotSurfaceKind ToFpsSurfaceKind(SectorCollisionRaySurfaceKind kind)
{
    switch (kind) {
        case SectorCollisionRaySurfaceKind::Floor: return FpsShotSurfaceKind::Floor;
        case SectorCollisionRaySurfaceKind::Ceiling: return FpsShotSurfaceKind::Ceiling;
        case SectorCollisionRaySurfaceKind::Wall: return FpsShotSurfaceKind::Wall;
        case SectorCollisionRaySurfaceKind::LowerWall: return FpsShotSurfaceKind::LowerWall;
        case SectorCollisionRaySurfaceKind::UpperWall: return FpsShotSurfaceKind::UpperWall;
        case SectorCollisionRaySurfaceKind::None: break;
    }
    return FpsShotSurfaceKind::None;
}

bool RaySphere(
        Vector3 origin,
        Vector3 direction,
        Vector3 center,
        float radius,
        float maximumDistance,
        float& outDistance,
        Vector3& outNormal)
{
    const Vector3 offset = Vector3Subtract(origin, center);
    const float b = Vector3DotProduct(offset, direction);
    const float c = Vector3DotProduct(offset, offset) - radius * radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) return false;
    const float root = std::sqrt(discriminant);
    float distance = -b - root;
    if (distance < 0.0f) distance = -b + root;
    if (distance < 0.0f || distance > maximumDistance) return false;
    outDistance = distance;
    outNormal = Vector3Normalize(Vector3Subtract(
            Vector3Add(origin, Vector3Scale(direction, distance)), center));
    return true;
}

bool RayVerticalCapsule(
        Vector3 origin,
        Vector3 direction,
        Vector3 feet,
        float radius,
        float height,
        float maximumDistance,
        float& outDistance,
        Vector3& outNormal)
{
    outDistance = maximumDistance + 1.0f;
    bool hit = false;
    const float lowerY = feet.y + radius;
    const float upperY = feet.y + std::max(radius, height - radius);
    const float a = direction.x * direction.x + direction.z * direction.z;
    if (a > RayEpsilon) {
        const float ox = origin.x - feet.x;
        const float oz = origin.z - feet.z;
        const float b = 2.0f * (ox * direction.x + oz * direction.z);
        const float c = ox * ox + oz * oz - radius * radius;
        const float discriminant = b * b - 4.0f * a * c;
        if (discriminant >= 0.0f) {
            const float root = std::sqrt(discriminant);
            const float distances[2] = {
                    (-b - root) / (2.0f * a),
                    (-b + root) / (2.0f * a)};
            for (float distance : distances) {
                if (distance < 0.0f || distance > maximumDistance
                        || distance >= outDistance) continue;
                const float y = origin.y + direction.y * distance;
                if (y < lowerY || y > upperY) continue;
                const Vector3 position = Vector3Add(
                        origin, Vector3Scale(direction, distance));
                outDistance = distance;
                outNormal = Vector3Normalize(Vector3{
                        position.x - feet.x, 0.0f, position.z - feet.z});
                hit = true;
            }
        }
    }
    for (float centerY : {lowerY, upperY}) {
        float distance = 0.0f;
        Vector3 normal{};
        if (RaySphere(
                    origin,
                    direction,
                    Vector3{feet.x, centerY, feet.z},
                    radius,
                    std::min(maximumDistance, outDistance),
                    distance,
                    normal)) {
            outDistance = distance;
            outNormal = normal;
            hit = true;
        }
    }
    return hit;
}

bool RayOrientedPrism(
        Vector3 origin,
        Vector3 direction,
        Vector2 center,
        Vector2 axisX,
        Vector2 axisZ,
        Vector2 halfExtents,
        float bottom,
        float top,
        float maximumDistance,
        float& outDistance,
        Vector3& outNormal)
{
    const Vector2 offset{origin.x - center.x, origin.z - center.y};
    const float localOrigin[3] = {
            Vector2DotProduct(offset, axisX),
            origin.y - (bottom + top) * 0.5f,
            Vector2DotProduct(offset, axisZ)};
    const Vector2 directionXZ{direction.x, direction.z};
    const float localDirection[3] = {
            Vector2DotProduct(directionXZ, axisX),
            direction.y,
            Vector2DotProduct(directionXZ, axisZ)};
    const float extents[3] = {
            halfExtents.x,
            std::max(0.0f, (top - bottom) * 0.5f),
            halfExtents.y};
    float nearDistance = 0.0f;
    float farDistance = maximumDistance;
    int nearAxis = -1;
    float nearSign = 0.0f;
    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(localDirection[axis]) <= RayEpsilon) {
            if (localOrigin[axis] < -extents[axis]
                    || localOrigin[axis] > extents[axis]) return false;
            continue;
        }
        float first = (-extents[axis] - localOrigin[axis])
                / localDirection[axis];
        float second = (extents[axis] - localOrigin[axis])
                / localDirection[axis];
        float sign = -1.0f;
        if (first > second) {
            std::swap(first, second);
            sign = 1.0f;
        }
        if (first > nearDistance) {
            nearDistance = first;
            nearAxis = axis;
            nearSign = sign;
        }
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance) return false;
    }
    if (nearDistance < 0.0f || nearDistance > maximumDistance) return false;
    outDistance = nearDistance;
    if (nearAxis == 0) {
        outNormal = {axisX.x * nearSign, 0.0f, axisX.y * nearSign};
    } else if (nearAxis == 1) {
        outNormal = {0.0f, nearSign, 0.0f};
    } else {
        outNormal = {axisZ.x * nearSign, 0.0f, axisZ.y * nearSign};
    }
    return true;
}

Vector3 NpcRenderPosition(
        engine::World& world,
        engine::Entity entity,
        const SectorObjectTransform& transform)
{
    return world.Has<SectorObjectVisualOffset>(entity)
            ? Vector3Add(
                    transform.position,
                    world.Get<SectorObjectVisualOffset>(entity).position)
            : transform.position;
}

Matrix NpcAuthoredTransform(
        engine::World& world,
        engine::Entity entity,
        const SectorObjectTransform& transform,
        const SectorDynamicModel& model)
{
    return BuildSectorStaticModelAuthoredTransform(
            NpcRenderPosition(world, entity, transform),
            transform.rotationXRadians,
            transform.yawRadians,
            transform.rotationZRadians,
            model.scale);
}

Vector3 ToNpcLocalPoint(Vector3 worldPoint, Matrix authoredTransform)
{
    return Vector3Transform(worldPoint, MatrixInvert(authoredTransform));
}

} // namespace

void InitializeNpcCombatRuntime(NpcCombatRuntime& runtime, size_t npcCapacity)
{
    runtime.deferredDestroy.clear();
    runtime.deferredDestroy.reserve(npcCapacity);
}

void ClearNpcCombatRuntime(NpcCombatRuntime& runtime)
{
    runtime.deferredDestroy.clear();
}

bool ResolvePlayerWeaponShot(
        engine::World& world,
        const engine::AssetManager* assets,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& npcNavigation,
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 rayOrigin,
        Vector3 rayDirection,
        float maximumDistance,
        const FpsWeaponImpactDefinition& impact,
        FpsShotResult& outShot,
        WeaponImpactEvent& outImpact)
{
    outShot = {};
    outShot.accepted = true;
    outShot.rayOrigin = rayOrigin;
    outShot.rayDirection = Vector3Normalize(rayDirection);
    outImpact = {};
    RayCandidate best;

    if (collisionWorld != nullptr) {
        const SectorCollisionRayHit hit = collisionWorld->Raycast(
                rayOrigin, outShot.rayDirection, maximumDistance);
        if (hit.hit) {
            best.hit = true;
            best.distance = hit.distance;
            best.position = hit.position;
            best.normal = hit.normal;
            best.kind = FpsShotHitKind::SectorSurface;
            best.surfaceKind = ToFpsSurfaceKind(hit.surfaceKind);
            best.sectorId = hit.sectorId;
            best.lineDefId = hit.lineDefId;
            best.sideDefId = hit.sideDefId;
            best.neighborSectorId = hit.neighborSectorId;
        }
    }

    for (const SectorDynamicDoorCollider& door : doorColliders) {
        float distance = 0.0f;
        Vector3 normal{};
        if (RayOrientedPrism(
                    rayOrigin, outShot.rayDirection,
                    door.center, door.tangent, door.normal,
                    door.halfExtents, door.bottom, door.top,
                    std::min(maximumDistance, best.distance),
                    distance, normal)) {
            best = {true, distance,
                    Vector3Add(rayOrigin, Vector3Scale(outShot.rayDirection, distance)),
                    normal, FpsShotHitKind::Door, FpsShotSurfaceKind::None,
                    0, 0, 0, 0, door.placedObjectId, door.entity};
        }
    }
    for (const SectorStaticModelCollider& collider : staticColliders) {
        if (!collider.resolved || collider.failed) continue;
        float distance = 0.0f;
        Vector3 normal{};
        if (RayOrientedPrism(
                    rayOrigin, outShot.rayDirection,
                    collider.center, collider.axisX, collider.axisZ,
                    collider.halfExtents, collider.bottom, collider.top,
                    std::min(maximumDistance, best.distance),
                    distance, normal)) {
            best = {true, distance,
                    Vector3Add(rayOrigin, Vector3Scale(outShot.rayDirection, distance)),
                    normal, FpsShotHitKind::SolidProp, FpsShotSurfaceKind::None,
                    0, 0, 0, 0, collider.placedObjectId,
                    engine::NullEntity()};
        }
    }

    world.ForEach<NpcRuntimeInstance, Health, NpcCombatState,
            SectorObjectTransform, SectorObject>(
            [&](engine::Entity entity,
                    NpcRuntimeInstance&,
                    Health& health,
                    NpcCombatState& combat,
                    SectorObjectTransform& transform,
                    SectorObject& object) {
                if (combat.dead || IsDepleted(health)) return;
                float distance = 0.0f;
                Vector3 normal{};
                engine::AnimatedModelRaycastStatus modelStatus =
                        engine::AnimatedModelRaycastStatus::Unavailable;
                if (assets != nullptr
                        && world.Has<SectorDynamicModel>(entity)
                        && world.Has<engine::AnimatedModelInstance>(entity)) {
                    const SectorDynamicModel& dynamicModel =
                            world.Get<SectorDynamicModel>(entity);
                    const engine::AnimatedModelInstance& modelInstance =
                            world.Get<engine::AnimatedModelInstance>(entity);
                    const engine::ModelAsset* modelAsset =
                            assets->GetModelAsset(modelInstance.model);
                    if (modelAsset != nullptr) {
                        engine::AnimatedModelRaycastResult modelHit;
                        modelStatus = engine::RaycastAnimatedModel(
                                *modelAsset,
                                modelInstance,
                                NpcAuthoredTransform(
                                        world,
                                        entity,
                                        transform,
                                        dynamicModel),
                                Ray{rayOrigin, outShot.rayDirection},
                                std::min(maximumDistance, best.distance),
                                modelHit);
                        if (modelStatus
                                == engine::AnimatedModelRaycastStatus::Hit) {
                            best = {
                                    true,
                                    modelHit.distance,
                                    modelHit.position,
                                    modelHit.normal,
                                    FpsShotHitKind::Npc,
                                    FpsShotSurfaceKind::None,
                                    object.currentSectorId,
                                    0,
                                    0,
                                    0,
                                    0,
                                    entity,
                                    modelHit.anchor};
                            return;
                        }
                        if (modelStatus
                                == engine::AnimatedModelRaycastStatus::Miss) {
                            return;
                        }
                    }
                }
                if (!RayVerticalCapsule(
                            rayOrigin, outShot.rayDirection, transform.position,
                            NpcRadius, NpcHeight,
                            std::min(maximumDistance, best.distance),
                            distance, normal)) return;
                best = {true, distance,
                        Vector3Add(rayOrigin, Vector3Scale(outShot.rayDirection, distance)),
                        normal, FpsShotHitKind::Npc, FpsShotSurfaceKind::None,
                        object.currentSectorId, 0, 0, 0, 0, entity};
            });

    if (!best.hit) {
        return false;
    }
    if (best.sectorId == 0 && collisionWorld != nullptr) {
        best.sectorId = collisionWorld->FindSectorContainingPoint(
                Vector2{best.position.x, best.position.z});
    }
    outShot.hit = true;
    outShot.position = best.position;
    outShot.normal = best.normal;
    outShot.distance = best.distance;
    outShot.hitKind = best.kind;
    outShot.surfaceKind = best.surfaceKind;
    outShot.sectorId = best.sectorId;
    outShot.lineDefId = best.lineDefId;
    outShot.sideDefId = best.sideDefId;
    outShot.neighborSectorId = best.neighborSectorId;
    outShot.placedObjectId = best.placedObjectId;
    outShot.targetEntity = best.entity;

    if (best.kind == FpsShotHitKind::Npc
            && world.IsAlive(best.entity)
            && world.Has<Health>(best.entity)
            && world.Has<NpcCombatState>(best.entity)
            && world.Has<SectorObjectTransform>(best.entity)) {
        Health& health = world.Get<Health>(best.entity);
        NpcCombatState& combat = world.Get<NpcCombatState>(best.entity);
        const int appliedDamage = ApplyDamage(health, impact.damage);
        if (appliedDamage > 0) {
            const Vector2 horizontal{outShot.rayDirection.x, outShot.rayDirection.z};
            const float length = Vector2Length(horizontal);
            if (length > RayEpsilon) {
                combat.knockbackVelocity = Vector2Add(
                        combat.knockbackVelocity,
                        Vector2Scale(
                                horizontal,
                                impact.knockbackImpulseWorldPerSecond / length));
            }
            if (IsDepleted(health)) {
                combat.dead = true;
                combat.hurtAnimationRequested = false;
                combat.hurtAnimationPlaying = false;
                combat.deathAnimationRequested = true;
                combat.deathAnimationComplete = false;
                DeactivateNpcNavigation(
                        world, navigation, npcNavigation, best.entity);
            } else {
                combat.staggerRemainingSeconds = impact.staggerSeconds;
                combat.hurtAnimationRequested = true;
            }
            if (impact.blood.enabled) {
                outImpact.kind = WeaponImpactKind::Blood;
                outImpact.position = best.position;
                outImpact.normal = best.normal;
                outImpact.sectorId = best.sectorId;
                outImpact.attachedEntity = best.entity;
                outImpact.surfaceAnchor = best.surfaceAnchor;
                const SectorObjectTransform& transform =
                        world.Get<SectorObjectTransform>(best.entity);
                if (world.Has<SectorDynamicModel>(best.entity)) {
                    outImpact.localPosition = ToNpcLocalPoint(
                            best.position,
                            NpcAuthoredTransform(
                                    world,
                                    best.entity,
                                    transform,
                                    world.Get<SectorDynamicModel>(best.entity)));
                } else {
                    outImpact.localPosition = Vector3Subtract(
                            best.position, transform.position);
                }
                outImpact.particles = impact.blood;
            }
        }
    } else if (impact.surfaceDebris.enabled) {
        outImpact.kind = WeaponImpactKind::SurfaceDebris;
        outImpact.position = best.position;
        outImpact.normal = best.normal;
        outImpact.sectorId = best.sectorId;
        outImpact.particles = impact.surfaceDebris;
    }
    return true;
}

bool UpdateNpcCombatSystem(
        engine::World& world,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const SectorTopologyMap* map,
        NpcCombatRuntime& runtime,
        float rawDt)
{
    const float dt = std::isfinite(rawDt) ? std::max(0.0f, rawDt) : 0.0f;
    runtime.deferredDestroy.clear();
    bool capacityWarned = false;
    bool movedNpc = false;
    world.ForEach<NpcCombatState, SectorObjectTransform, SectorObject,
            SectorDynamicModel>(
            [&](engine::Entity entity,
                    NpcCombatState& combat,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDynamicModel& model) {
                combat.staggerRemainingSeconds = std::max(
                        0.0f, combat.staggerRemainingSeconds - dt);
                const float speed = Vector2Length(combat.knockbackVelocity);
                if (speed > 0.001f && dt > 0.0f) {
                    const SectorCollisionMoveConfig config{
                            NpcRadius, NpcHeight, 0.25f, 4};
                    const SectorCollisionMoveState moveState{
                            {transform.position.x, transform.position.z},
                            transform.position.y,
                            object.currentSectorId,
                            true};
                    SectorCollisionMoveResult result = collisionWorld.ResolveMovement(
                            moveState,
                            Vector2Scale(combat.knockbackVelocity, dt),
                            config);
                    result = ResolveSectorDoorDynamicCollidersForPlayerMovement(
                            moveState, result, config, doorColliders);
                    const SectorFpsVerticalContext vertical =
                            BuildCombatVerticalContext(
                                    collisionWorld,
                                    result.currentSectorId);
                    result = ResolveSectorStaticModelCollidersForPlayerMovement(
                            moveState, result, config, vertical, staticColliders);
                    transform.position.x = result.positionXZ.x;
                    transform.position.z = result.positionXZ.y;
                    object.currentSectorId = result.currentSectorId;
                    movedNpc = true;
                    if (map != nullptr) {
                        model.containingSectorAmbient = ComputeSectorModelAmbient(
                                *map, object.currentSectorId);
                        model.environmentExposure =
                                ComputeSectorModelEnvironmentExposure(
                                        *map, object.currentSectorId);
                    }
                    combat.knockbackVelocity = Vector2Scale(
                            combat.knockbackVelocity,
                            std::exp(-KnockbackDampingPerSecond * dt));
                } else {
                    combat.knockbackVelocity = {};
                }

                if (!combat.dead || !combat.deathAnimationComplete
                        || !combat.despawnOnDeath) return;
                combat.corpseElapsedSeconds += dt;
                if (combat.corpseElapsedSeconds
                        <= combat.corpseDespawnDelaySeconds) return;
                const float fadeAge = combat.corpseElapsedSeconds
                        - combat.corpseDespawnDelaySeconds;
                model.opacity = std::clamp(
                        1.0f - fadeAge / combat.corpseFadeDurationSeconds,
                        0.0f,
                        1.0f);
                model.shadowMode = SectorDynamicModelShadowMode::None;
                if (model.opacity > 0.0f) return;
                if (runtime.deferredDestroy.size()
                        == runtime.deferredDestroy.capacity()) {
                    if (!capacityWarned) {
                        std::fprintf(stderr,
                                "[NPC Combat WARNING] Deferred corpse capacity exceeded; allocation may occur.\n");
                        capacityWarned = true;
                    }
                }
                runtime.deferredDestroy.push_back(entity);
            });
    for (engine::Entity entity : runtime.deferredDestroy) {
        if (world.IsAlive(entity)) world.DestroyLater(entity);
    }
    if (!runtime.deferredDestroy.empty()) world.FlushDestroyedEntities();
    return movedNpc;
}

} // namespace game
