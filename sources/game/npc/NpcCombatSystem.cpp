#include "game/npc/NpcCombatSystem.h"

#include "game/npc/NpcAudioSystem.h"
#include "game/npc/ai/NpcAiSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/ModelAssets.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "game/Health.h"
#include "game/npc/NpcNavigationSystem.h"
#include "game/npc/NpcBoneImpactSystem.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace game {
namespace {

constexpr float NpcRadius = 0.25f;
constexpr float NpcHeight = 1.6f;
constexpr float KnockbackDampingPerSecond = 12.0f;
constexpr float RayEpsilon = 0.00001f;
constexpr float BodyPartInfluenceThreshold = 0.5f;
constexpr int MaximumWeaponDamage = 1000000;

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
    int boneImpactBoneIndex = -1;
    float bodyPartDamageMultiplier = 1.0f;
    bool bodyPartDamageMatched = false;
};

bool IsBoneDescendantOf(
        const BoneInfo* bones,
        int boneCount,
        int candidate,
        int ancestor)
{
    int current = candidate;
    for (int depth = 0; depth < boneCount && current >= 0; ++depth) {
        if (current == ancestor) return true;
        if (current >= boneCount) return false;
        current = bones[current].parent;
    }
    return false;
}

bool ResolveBodyPartDamageRow(
        const engine::ModelAsset& asset,
        NpcBodyPartDamageRuntimeRow& row)
{
    row.boneResolutionAttempted = true;
    row.boneIndex = -1;
    row.affectedBones.fill(0);
    const int boneCount = asset.model.skeleton.boneCount;
    const BoneInfo* bones = asset.model.skeleton.bones;
    if (bones == nullptr || boneCount <= 0
            || boneCount > engine::MaxAnimatedModelBones) {
        return false;
    }
    for (int index = 0; index < boneCount; ++index) {
        if (std::strncmp(
                    bones[index].name,
                    row.boneName.c_str(),
                    sizeof(bones[index].name)) == 0) {
            row.boneIndex = index;
            break;
        }
    }
    if (row.boneIndex < 0) return false;
    int current = row.boneIndex;
    int depth = 0;
    for (; depth < boneCount && current >= 0; ++depth) {
        if (current >= boneCount) return false;
        current = bones[current].parent;
    }
    if (current >= 0) return false;
    for (int index = 0; index < boneCount; ++index) {
        row.affectedBones[static_cast<size_t>(index)] =
                IsBoneDescendantOf(
                        bones, boneCount, index, row.boneIndex)
                ? 1u : 0u;
    }
    return true;
}

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

NpcBodyPartDamageMatch ClassifyNpcBodyPartDamage(
        const engine::ModelAsset& asset,
        const engine::AnimatedModelSurfaceAnchor& anchor,
        NpcBodyPartDamageState& state)
{
    NpcBodyPartDamageMatch result;
    if (!anchor.valid || state.rows.empty()
            || state.rows.size() > kMaximumNpcBodyPartDamageRows) {
        return result;
    }
    const int boneCount = asset.model.skeleton.boneCount;
    const BoneInfo* bones = asset.model.skeleton.bones;
    if (bones == nullptr || boneCount <= 0
            || boneCount > engine::MaxAnimatedModelBones) {
        return result;
    }
    for (NpcBodyPartDamageRuntimeRow& row : state.rows) {
        if (!row.boneResolutionAttempted
                && !ResolveBodyPartDamageRow(asset, row)
                && !row.warningPrinted) {
            std::fprintf(
                    stderr,
                    "[NPC Body-Part Damage WARNING] Bone '%s' was not found or has an invalid hierarchy.\n",
                    row.boneName.c_str());
            row.warningPrinted = true;
        }
    }
    if (anchor.meshIndex >= static_cast<uint32_t>(asset.model.meshCount)
            || asset.model.meshes == nullptr) {
        return result;
    }
    const Mesh& mesh = asset.model.meshes[anchor.meshIndex];
    if (mesh.boneIndices == nullptr || mesh.boneWeights == nullptr) {
        if (!state.classificationWarningPrinted) {
            std::fprintf(
                    stderr,
                    "[NPC Body-Part Damage WARNING] Exact hit mesh has no skin weights; base damage will be used.\n");
            state.classificationWarningPrinted = true;
        }
        return result;
    }
    const float barycentric[3] = {
            anchor.barycentric.x,
            anchor.barycentric.y,
            anchor.barycentric.z};
    std::array<float, kMaximumNpcBodyPartDamageRows> influences{};
    float totalInfluence = 0.0f;
    for (size_t corner = 0; corner < anchor.vertexIndices.size(); ++corner) {
        const uint32_t vertexIndex = anchor.vertexIndices[corner];
        const float barycentricWeight = barycentric[corner];
        if (vertexIndex >= static_cast<uint32_t>(mesh.vertexCount)
                || !std::isfinite(barycentricWeight)
                || barycentricWeight < 0.0f) {
            return result;
        }
        for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex) {
            const size_t sourceIndex = static_cast<size_t>(vertexIndex) * 4u
                    + static_cast<size_t>(influenceIndex);
            const float weight = mesh.boneWeights[sourceIndex];
            if (!std::isfinite(weight) || weight < 0.0f) {
                return result;
            }
            if (weight <= 0.0f || barycentricWeight <= 0.0f) continue;
            const uint32_t boneIndex = mesh.boneIndices[sourceIndex];
            if (boneIndex >= static_cast<uint32_t>(boneCount)) return result;
            const float weightedInfluence = barycentricWeight * weight;
            totalInfluence += weightedInfluence;
            for (size_t rowIndex = 0; rowIndex < state.rows.size(); ++rowIndex) {
                const NpcBodyPartDamageRuntimeRow& row = state.rows[rowIndex];
                if (row.boneIndex >= 0
                        && row.affectedBones[static_cast<size_t>(boneIndex)] != 0) {
                    influences[rowIndex] += weightedInfluence;
                }
            }
        }
    }
    if (!std::isfinite(totalInfluence) || totalInfluence <= RayEpsilon) {
        return result;
    }

    int bestRow = -1;
    float bestInfluence = 0.0f;
    for (size_t rowIndex = 0; rowIndex < state.rows.size(); ++rowIndex) {
        const NpcBodyPartDamageRuntimeRow& row = state.rows[rowIndex];
        if (row.boneIndex < 0
                || !std::isfinite(row.damageMultiplier)
                || row.damageMultiplier < 0.0f
                || row.damageMultiplier > kMaximumNpcBodyPartDamageMultiplier) {
            continue;
        }
        const float normalizedInfluence = influences[rowIndex] / totalInfluence;
        if (normalizedInfluence < BodyPartInfluenceThreshold) continue;
        bool replace = bestRow < 0;
        if (bestRow >= 0) {
            const NpcBodyPartDamageRuntimeRow& best =
                    state.rows[static_cast<size_t>(bestRow)];
            const bool rowIsDescendant = row.boneIndex != best.boneIndex
                    && IsBoneDescendantOf(
                            bones, boneCount, row.boneIndex, best.boneIndex);
            const bool bestIsDescendant = row.boneIndex != best.boneIndex
                    && IsBoneDescendantOf(
                            bones, boneCount, best.boneIndex, row.boneIndex);
            if (rowIsDescendant) {
                replace = true;
            } else if (!bestIsDescendant
                    && normalizedInfluence > bestInfluence + RayEpsilon) {
                replace = true;
            }
        }
        if (replace) {
            bestRow = static_cast<int>(rowIndex);
            bestInfluence = normalizedInfluence;
        }
    }
    if (bestRow >= 0) {
        result.multiplier = state.rows[static_cast<size_t>(bestRow)]
                .damageMultiplier;
        result.rowIndex = bestRow;
        result.matched = true;
    }
    return result;
}

int ScaleNpcBodyPartDamage(int baseDamage, float multiplier)
{
    const int normalizedBase = std::clamp(
            baseDamage, 0, MaximumWeaponDamage);
    if (!std::isfinite(multiplier) || multiplier < 0.0f) {
        return normalizedBase;
    }
    const double scaled = std::clamp(
            static_cast<double>(normalizedBase)
                    * static_cast<double>(multiplier),
            0.0,
            static_cast<double>(MaximumWeaponDamage));
    return static_cast<int>(std::floor(scaled + 0.5));
}

void InitializeNpcCombatRuntime(NpcCombatRuntime& runtime, size_t npcCapacity)
{
    runtime.deferredDestroy.clear();
    runtime.deferredDestroy.reserve(npcCapacity);
}

void ClearNpcCombatRuntime(NpcCombatRuntime& runtime)
{
    runtime.deferredDestroy.clear();
}

static bool TracePlayerWeaponShot(
        engine::World& world,
        const engine::AssetManager* assets,
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 rayOrigin,
        Vector3 rayDirection,
        float maximumDistance,
        FpsShotResult& outShot,
        RayCandidate& outCandidate)
{
    outShot = {};
    outShot.accepted = true;
    outShot.rayOrigin = rayOrigin;
    outShot.rayDirection = Vector3Normalize(rayDirection);
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
                    NpcRuntimeInstance& npc,
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
                            if (world.Has<NpcBoneImpactState>(entity)) {
                                NpcBoneImpactState& boneImpact =
                                        world.Get<NpcBoneImpactState>(entity);
                                best.boneImpactBoneIndex =
                                        FindNpcBoneImpactDominantBone(
                                                *modelAsset,
                                                modelHit.anchor);
                                if (best.boneImpactBoneIndex < 0
                                        && !boneImpact
                                                    .classificationWarningPrinted) {
                                    std::fprintf(
                                            stderr,
                                            "[NPC Bone Impact WARNING] Exact hit on NPC '%s' has no usable skin weights.\n",
                                            npc.definitionId.c_str());
                                    boneImpact.classificationWarningPrinted = true;
                                }
                            }
                            if (world.Has<NpcBodyPartDamageState>(entity)) {
                                const NpcBodyPartDamageMatch bodyPart =
                                        ClassifyNpcBodyPartDamage(
                                                *modelAsset,
                                                modelHit.anchor,
                                                world.Get<NpcBodyPartDamageState>(
                                                        entity));
                                best.bodyPartDamageMultiplier =
                                        bodyPart.multiplier;
                                best.bodyPartDamageMatched = bodyPart.matched;
                            }
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
    outShot.bodyPartDamageMultiplier = best.bodyPartDamageMultiplier;
    outShot.bodyPartDamageMatched = best.bodyPartDamageMatched;

    outCandidate = best;
    return true;
}

static void ApplyPlayerWeaponImpact(
        engine::World& world,
        const engine::AssetManager* assets,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& npcNavigation,
        const FpsWeaponImpactDefinition& impact,
        const RayCandidate& best,
        const FpsShotResult& outShot,
        WeaponImpactEvent& outImpact,
        NpcAudioRuntime* npcAudio,
        NpcAiRuntime* npcAi)
{
    outImpact = {};
    if (best.kind == FpsShotHitKind::Npc
            && world.IsAlive(best.entity)
            && world.Has<Health>(best.entity)
            && world.Has<NpcCombatState>(best.entity)
            && world.Has<SectorObjectTransform>(best.entity)) {
        Health& health = world.Get<Health>(best.entity);
        NpcCombatState& combat = world.Get<NpcCombatState>(best.entity);
        const int damage = best.bodyPartDamageMatched
                ? ScaleNpcBodyPartDamage(
                        impact.damage,
                        best.bodyPartDamageMultiplier)
                : impact.damage;
        const int appliedDamage = ApplyDamage(health, damage);
        if (appliedDamage > 0) {
            if (assets != nullptr
                    && best.boneImpactBoneIndex >= 0
                    && world.Has<NpcBoneImpactState>(best.entity)
                    && world.Has<engine::AnimatedModelInstance>(best.entity)
                    && world.Has<SectorDynamicModel>(best.entity)) {
                engine::AnimatedModelInstance& instance =
                        world.Get<engine::AnimatedModelInstance>(best.entity);
                const engine::ModelAsset* asset =
                        assets->GetModelAsset(instance.model);
                if (asset != nullptr) {
                    AddNpcBoneImpactImpulse(
                            world.Get<NpcBoneImpactState>(best.entity),
                            *asset,
                            instance,
                            NpcAuthoredTransform(
                                    world,
                                    best.entity,
                                    world.Get<SectorObjectTransform>(best.entity),
                                    world.Get<SectorDynamicModel>(best.entity)),
                            best.boneImpactBoneIndex,
                            best.position,
                            outShot.rayDirection);
                }
            }
            if (npcAi != nullptr) {
                AlertNpcToPlayerPosition(world, best.entity, outShot.rayOrigin);
            }
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
                if (npcAudio != nullptr) {
                    QueueNpcVocalEvent(
                            *npcAudio, best.entity, NpcVocalEvent::Death);
                }
            } else {
                combat.staggerRemainingSeconds = impact.staggerSeconds;
                const bool committedUnstaggeredAttack =
                        impact.staggerSeconds <= 0.0f
                        && world.Has<NpcRuntimeInstance>(best.entity)
                        && world.Get<NpcRuntimeInstance>(best.entity)
                                .actionLockedByAi;
                combat.hurtAnimationRequested = !committedUnstaggeredAttack;
                if (npcAudio != nullptr) {
                    QueueNpcVocalEvent(
                            *npcAudio, best.entity, NpcVocalEvent::Hurt);
                }
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
        WeaponImpactEvent& outImpact,
        NpcAudioRuntime* npcAudio,
        NpcAiRuntime* npcAi)
{
    RayCandidate candidate;
    const bool hit = TracePlayerWeaponShot(
            world,
            assets,
            collisionWorld,
            doorColliders,
            staticColliders,
            rayOrigin,
            rayDirection,
            maximumDistance,
            outShot,
            candidate);
    outImpact = {};
    if (hit) {
        ApplyPlayerWeaponImpact(
                world,
                assets,
                navigation,
                npcNavigation,
                impact,
                candidate,
                outShot,
                outImpact,
                npcAudio,
                npcAi);
    }
    return hit;
}

bool ResolvePlayerWeaponPelletVolley(
        engine::World& world,
        const engine::AssetManager* assets,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& npcNavigation,
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 rayOrigin,
        Vector3 aimDirection,
        uint64_t shotSequence,
        const FpsWeaponFiringDefinition& rawFiring,
        WeaponPelletVolleyResult& outVolley,
        NpcAudioRuntime* npcAudio,
        NpcAiRuntime* npcAi)
{
    outVolley = {};
    const int configuredPelletCount = std::clamp(
            rawFiring.pellets.count, 1, MaxFpsWeaponPellets);
    outVolley.pelletCount = rawFiring.pellets.enabled
            ? configuredPelletCount
            : 1;
    const float maximumRange = std::isfinite(rawFiring.maximumRangeWorld)
            ? std::clamp(rawFiring.maximumRangeWorld, 1.0f, 10000.0f)
            : FpsWeaponFiringDefinition{}.maximumRangeWorld;
    std::array<RayCandidate, MaxFpsWeaponPellets> candidates{};

    for (int pelletIndex = 0;
            pelletIndex < outVolley.pelletCount;
            ++pelletIndex) {
        const Vector3 pelletDirection = FpsWeaponPelletDirection(
                aimDirection,
                rawFiring.pellets,
                pelletIndex,
                shotSequence);
        if (TracePlayerWeaponShot(
                    world,
                    assets,
                    collisionWorld,
                    doorColliders,
                    staticColliders,
                    rayOrigin,
                    pelletDirection,
                    maximumRange,
                    outVolley.shots[static_cast<size_t>(pelletIndex)],
                    candidates[static_cast<size_t>(pelletIndex)])) {
            ++outVolley.hitCount;
        }
    }

    for (int pelletIndex = 0;
            pelletIndex < outVolley.pelletCount;
            ++pelletIndex) {
        const size_t index = static_cast<size_t>(pelletIndex);
        if (!outVolley.shots[index].hit) continue;
        ApplyPlayerWeaponImpact(
                world,
                assets,
                navigation,
                npcNavigation,
                rawFiring.impact,
                candidates[index],
                outVolley.shots[index],
                outVolley.impacts[index],
                npcAudio,
                npcAi);
    }
    return outVolley.hitCount > 0;
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
