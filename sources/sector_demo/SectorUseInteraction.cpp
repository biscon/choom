#include "sector_demo/SectorUseInteraction.h"
#include "sector_demo/SectorPropDragging.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/FontAssets.h"
#include "engine/assets/ModelAssets.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "game/npc/NpcRuntime.h"
#include "game/npc/NpcLineOfSight.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorLadderInteraction.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/SectorStructuralPrimitives.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace game {
namespace {

constexpr float UseFacingDotThreshold = 0.65f;
constexpr float UseOcclusionTolerance = 0.05f;
constexpr float UseHighlightPeriodSeconds = 2.4f;
constexpr float UseHighlightMaximumStrength = 0.14f;
constexpr float UseHighlightReleaseSeconds = 0.3f;

bool Finite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

BoundingBox TransformBounds(BoundingBox bounds, Matrix transform)
{
    BoundingBox result{
            Vector3{FLT_MAX, FLT_MAX, FLT_MAX},
            Vector3{-FLT_MAX, -FLT_MAX, -FLT_MAX}};
    for (float x : {bounds.min.x, bounds.max.x}) {
        for (float y : {bounds.min.y, bounds.max.y}) {
            for (float z : {bounds.min.z, bounds.max.z}) {
                const Vector3 point = Vector3Transform(Vector3{x, y, z}, transform);
                result.min.x = std::min(result.min.x, point.x);
                result.min.y = std::min(result.min.y, point.y);
                result.min.z = std::min(result.min.z, point.z);
                result.max.x = std::max(result.max.x, point.x);
                result.max.y = std::max(result.max.y, point.y);
                result.max.z = std::max(result.max.z, point.z);
            }
        }
    }
    return result;
}

Vector3 ClosestPoint(BoundingBox bounds, Vector3 point)
{
    return Vector3{
            std::clamp(point.x, bounds.min.x, bounds.max.x),
            std::clamp(point.y, bounds.min.y, bounds.max.y),
            std::clamp(point.z, bounds.min.z, bounds.max.z)};
}

Vector2 ClosestPointOnSegment(Vector2 point, Vector2 a, Vector2 b)
{
    const Vector2 segment = Vector2Subtract(b, a);
    const float lengthSq = Vector2LengthSqr(segment);
    if (lengthSq <= 0.000001f) return a;
    const float t = std::clamp(
            Vector2DotProduct(Vector2Subtract(point, a), segment) / lengthSq,
            0.0f,
            1.0f);
    return Vector2Add(a, Vector2Scale(segment, t));
}

bool IsVisible(
        const SectorCollisionWorld* collisionWorld,
        Vector3 eye,
        Vector3 target,
        float distance)
{
    if (collisionWorld == nullptr || distance <= UseOcclusionTolerance) return true;
    const Vector3 direction = Vector3Scale(
            Vector3Subtract(target, eye),
            1.0f / distance);
    const SectorCollisionRayHit hit = collisionWorld->Raycast(
            eye, direction, distance);
    return !hit.hit || hit.distance + UseOcclusionTolerance >= distance;
}

bool HiddenByDraggableProp(engine::World& world, engine::Entity entity, Vector3 eye, Vector3 targetPosition, float distance)
{
    bool hidden = false;
    world.ForEach<SectorPropDrag, SectorStaticModelCollider>([&](engine::Entity blocker,
            const SectorPropDrag&, const SectorStaticModelCollider& box) {
        if (blocker == entity || !box.resolved || distance <= UseOcclusionTolerance) return;
        const Vector2 relative{eye.x-box.center.x,eye.z-box.center.y};
        const Vector2 delta{targetPosition.x-eye.x,targetPosition.z-eye.z};
        const Ray ray{{Vector2DotProduct(relative,box.axisX),eye.y,Vector2DotProduct(relative,box.axisZ)},
                {Vector2DotProduct(delta,box.axisX)/distance,(targetPosition.y-eye.y)/distance,Vector2DotProduct(delta,box.axisZ)/distance}};
        const auto hit = GetRayCollisionBox(ray,{{-box.halfExtents.x,box.bottom,-box.halfExtents.y},
                {box.halfExtents.x,box.top,box.halfExtents.y}});
        if (hit.hit && hit.distance + UseOcclusionTolerance < distance) hidden = true;
    });
    return hidden;
}

void ConsiderTarget(
        engine::World& world,
        engine::Entity entity,
        SectorUseTargetKind kind,
        Vector3 targetPosition,
        float maximumDistance,
        Vector3 eye,
        Vector3 forward,
        const SectorCollisionWorld* collisionWorld,
        SectorUseTarget& best)
{
    const Vector3 offset = Vector3Subtract(targetPosition, eye);
    const float distance = Vector3Length(offset);
    if (!std::isfinite(distance) || distance > maximumDistance) return;
    const float facing = distance > 0.0001f
            ? Vector3DotProduct(forward, Vector3Scale(offset, 1.0f / distance))
            : 1.0f;
    if (facing < UseFacingDotThreshold
            || !IsVisible(collisionWorld, eye, targetPosition, distance)) {
        return;
    }
    if (HiddenByDraggableProp(world,entity,eye,targetPosition,distance)) return;
    const bool better = best.kind == SectorUseTargetKind::None
            || facing > best.facingDot + 0.0001f
            || (std::fabs(facing - best.facingDot) <= 0.0001f
                    && (distance < best.distance - 0.0001f
                            || (std::fabs(distance - best.distance) <= 0.0001f
                                    && entity.index < best.entity.index)));
    if (!better) return;
    best = SectorUseTarget{entity, kind, targetPosition, facing, distance};
    best.draggable = world.Has<SectorPropDrag>(entity);
}

bool CanTargetNpcWithObject(
        engine::World& world, engine::Entity entity, const NpcRuntimeInstance& npc)
{
    return !npc.instanceId.empty() && !npc.hostile && !npc.conversationHeld
            && !(world.Has<Health>(entity) && IsDepleted(world.Get<Health>(entity)))
            && !(world.Has<NpcCombatState>(entity)
                    && world.Get<NpcCombatState>(entity).dead);
}

void ConsiderObjectUseHit(
        SectorObjectUseTargetAccumulator& accumulator,
        engine::Entity entity,
        SectorUseTargetKind kind,
        const RayCollision& collision,
        bool selectable)
{
    if (kind != SectorUseTargetKind::StaticProp
            && kind != SectorUseTargetKind::DynamicProp
            && kind != SectorUseTargetKind::Door
            && kind != SectorUseTargetKind::Npc) {
        return;
    }
    if (!collision.hit || !std::isfinite(collision.distance)
            || collision.distance < 0.0f) {
        return;
    }
    const bool hasNearest = accumulator.nearest.kind
            != SectorUseTargetKind::None;
    const bool better = !hasNearest
            || collision.distance < accumulator.nearest.distance - 0.0001f
            || (std::fabs(collision.distance - accumulator.nearest.distance)
                            <= 0.0001f
                    && entity.index < accumulator.nearest.entity.index);
    if (!better) return;
    accumulator.nearest = SectorUseTarget{
            entity,
            kind,
            collision.point,
            1.0f,
            collision.distance};
    accumulator.nearestSelectable = selectable;
}

} // namespace

void ConsiderSectorObjectUseBounds(
        SectorObjectUseTargetAccumulator& accumulator,
        Ray ray,
        engine::Entity entity,
        SectorUseTargetKind kind,
        BoundingBox bounds,
        bool selectable)
{
    if (!Finite(ray.position) || !Finite(ray.direction)
            || Vector3LengthSqr(ray.direction) <= 0.000001f
            || !Finite(bounds.min) || !Finite(bounds.max)) return;
    ray.direction = Vector3Normalize(ray.direction);
    ConsiderObjectUseHit(accumulator, entity, kind,
            GetRayCollisionBox(ray, bounds), selectable);
}

void ConsiderSectorObjectUseTransformedBounds(
        SectorObjectUseTargetAccumulator& accumulator,
        Ray ray,
        engine::Entity entity,
        SectorUseTargetKind kind,
        BoundingBox localBounds,
        Matrix transform,
        bool selectable)
{
    const float determinant = MatrixDeterminant(transform);
    if (!Finite(ray.position) || !Finite(ray.direction)
            || Vector3LengthSqr(ray.direction) <= 0.000001f
            || !Finite(localBounds.min) || !Finite(localBounds.max)
            || !std::isfinite(determinant) || std::fabs(determinant) < 1e-12f) return;
    const Matrix inverse = MatrixInvert(transform);
    Matrix inverseDirection = inverse;
    inverseDirection.m12 = inverseDirection.m13 = inverseDirection.m14 = 0.0f;
    const Ray localRay{Vector3Transform(ray.position, inverse),
            Vector3Normalize(Vector3Transform(ray.direction, inverseDirection))};
    if (!Finite(localRay.position) || !Finite(localRay.direction)) return;
    RayCollision hit = GetRayCollisionBox(localRay, localBounds);
    if (!hit.hit) return;
    hit.point = Vector3Transform(hit.point, transform);
    hit.distance = Vector3Distance(ray.position, hit.point);
    ConsiderObjectUseHit(accumulator, entity, kind, hit, selectable);
}

SectorUseTarget FinishSectorObjectUseTarget(
        const SectorObjectUseTargetAccumulator& accumulator,
        float topologyHitDistance)
{
    if (accumulator.nearest.kind == SectorUseTargetKind::None
            || !accumulator.nearestSelectable) {
        return {};
    }
    if (std::isfinite(topologyHitDistance)
            && topologyHitDistance >= 0.0f
            && topologyHitDistance + UseOcclusionTolerance
                    < accumulator.nearest.distance) {
        return {};
    }
    return accumulator.nearest;
}

SectorUseTarget FindSectorObjectUseTarget(
        engine::World& world,
        const engine::AssetManager& assets,
        Ray ray,
        const SectorCollisionWorld* collisionWorld)
{
    SectorObjectUseTargetAccumulator accumulator;
    world.ForEach<SectorObjectTransform, SectorObject, SectorStaticModel>(
            [&assets, ray, &accumulator](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorStaticModel& model) {
                if (!object.visible) return;
                const engine::ModelAsset* asset = assets.GetModelAsset(model.model);
                if (asset == nullptr || !asset->hasLocalBounds) return;
                const Matrix authored = BuildSectorStaticModelAuthoredTransform(
                        transform.position,
                        transform.rotationXRadians,
                        transform.yawRadians,
                        transform.rotationZRadians,
                        model.scale);
                ConsiderSectorObjectUseBounds(
                        accumulator,
                        ray,
                        entity,
                        SectorUseTargetKind::StaticProp,
                        TransformBounds(asset->localBounds, authored),
                        object.itemDropTarget && !model.instanceId.empty());
            });
    world.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDynamicModel,
            engine::AnimatedModelInstance>(
            [&world, &assets, ray, &accumulator](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDynamicModel& model,
                    engine::AnimatedModelInstance& instance) {
                if (!object.visible || model.opacity <= 0.0f
                        || !instance.poseReady || instance.poseFailed) {
                    return;
                }
                const engine::ModelAsset* asset = assets.GetModelAsset(instance.model);
                if (asset == nullptr
                        || (!asset->hasAnimatedLocalBounds
                                && !asset->hasLocalBounds)) {
                    return;
                }
                Vector3 renderPosition = transform.position;
                if (world.Has<SectorObjectVisualOffset>(entity)) {
                    renderPosition = Vector3Add(
                            renderPosition,
                            world.Get<SectorObjectVisualOffset>(entity).position);
                }
                const Matrix authored = BuildSectorStaticModelAuthoredTransform(
                        renderPosition,
                        transform.rotationXRadians,
                        transform.yawRadians,
                        transform.rotationZRadians,
                        model.scale);
                const BoundingBox localBounds = asset->hasAnimatedLocalBounds
                        ? asset->animatedLocalBounds : asset->localBounds;
                const bool isNpc = world.Has<NpcRuntimeInstance>(entity);
                const bool selectable = isNpc
                        ? CanTargetNpcWithObject(world, entity,
                                world.Get<NpcRuntimeInstance>(entity))
                        : !model.instanceId.empty();
                ConsiderSectorObjectUseBounds(
                        accumulator,
                        ray,
                        entity,
                        isNpc ? SectorUseTargetKind::Npc : SectorUseTargetKind::DynamicProp,
                        TransformBounds(localBounds, authored),
                        object.itemDropTarget && selectable);
            });
    world.ForEach<SectorObjectTransform, SectorObject, SectorDoor,
            SectorDoorResolvedAnchor, SectorDoorRender>(
            [&world, &assets, ray, &accumulator](engine::Entity entity,
                    SectorObjectTransform& transform, SectorObject& object,
                    SectorDoor& door, SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render) {
                if (!object.visible || !door.enabled || !render.visible) return;
                if (world.Has<SectorDoorModelRender>(entity)) {
                    const auto& model = world.Get<SectorDoorModelRender>(entity);
                    const auto* leaf = assets.GetModelAsset(model.leafModel);
                    const auto policy = ResolveSectorDoorModelDrawPolicy(
                            model, leaf != nullptr,
                            assets.GetModelAsset(model.frameModel) != nullptr);
                    if (policy.drawLeaf) {
                        if (leaf->hasLocalBounds) {
                            ConsiderSectorObjectUseTransformedBounds(accumulator,
                                    ray, entity, SectorUseTargetKind::Door,
                                    leaf->localBounds, model.leafMatrix,
                                    object.itemDropTarget && !door.instanceId.empty());
                        }
                        return;
                    }
                }
                if (!std::isfinite(render.width) || render.width <= 0.0f
                        || !std::isfinite(render.height) || render.height <= 0.0f
                        || !std::isfinite(render.thickness) || render.thickness <= 0.0f) return;
                const BoundingBox bounds{
                        {-render.width * 0.5f, -render.height * 0.5f, -render.thickness * 0.5f},
                        {render.width * 0.5f, render.height * 0.5f, render.thickness * 0.5f}};
                ConsiderSectorObjectUseTransformedBounds(accumulator,
                        ray, entity, SectorUseTargetKind::Door, bounds,
                        BuildSectorDoorSlabModelMatrix(transform, anchor, render),
                        object.itemDropTarget && !door.instanceId.empty());
            });
    float topologyDistance = -1.0f;
    if (collisionWorld != nullptr
            && accumulator.nearest.kind != SectorUseTargetKind::None
            && accumulator.nearest.distance > 0.0f) {
        const SectorCollisionRayHit hit = collisionWorld->Raycast(
                ray.position,
                ray.direction,
                accumulator.nearest.distance);
        if (hit.hit) topologyDistance = hit.distance;
    }
    return FinishSectorObjectUseTarget(accumulator, topologyDistance);
}

std::string_view SectorObjectUseTargetInstanceId(
        engine::World& world,
        const SectorUseTarget& target)
{
    if (!world.IsAlive(target.entity) || !world.Has<SectorObject>(target.entity)
            || !world.Get<SectorObject>(target.entity).itemDropTarget) return {};
    if (target.kind == SectorUseTargetKind::Door
            && world.Has<SectorDoor>(target.entity)) {
        const auto& door = world.Get<SectorDoor>(target.entity);
        return door.enabled ? std::string_view{door.instanceId} : std::string_view{};
    }
    if (target.kind == SectorUseTargetKind::Npc
            && world.Has<NpcRuntimeInstance>(target.entity)) {
        const auto& npc = world.Get<NpcRuntimeInstance>(target.entity);
        return CanTargetNpcWithObject(world, target.entity, npc)
                ? std::string_view{npc.instanceId} : std::string_view{};
    }
    if (target.kind == SectorUseTargetKind::StaticProp
            && world.Has<SectorStaticModel>(target.entity)) {
        return world.Get<SectorStaticModel>(target.entity).instanceId;
    }
    if (target.kind == SectorUseTargetKind::DynamicProp
            && world.Has<SectorDynamicModel>(target.entity)
            && !world.Has<NpcRuntimeInstance>(target.entity)) {
        return world.Get<SectorDynamicModel>(target.entity).instanceId;
    }
    return {};
}

SectorUseTarget FindSectorUseTarget(
        engine::World& world,
        const engine::AssetManager* assets,
        Vector3 eyePosition,
        Vector3 forward,
        const SectorCollisionWorld* collisionWorld,
        bool includeDynamicProps,
        const SectorTopologyMap* topologyMap,
        float ductInteractionDistanceWorld,
        int viewerSectorId,
        const SectorRuntimeObjectState* runtimeObjects)
{
    SectorUseTarget best;
    if (!Finite(eyePosition) || !Finite(forward)
            || Vector3LengthSqr(forward) <= 0.000001f) {
        return best;
    }
    forward = Vector3Normalize(forward);

    world.ForEach<SectorItem, SectorObjectTransform>(
            [&](engine::Entity entity,
                    SectorItem& item,
                    SectorObjectTransform& transform) {
                if (item.takePending || !IsItemSettled(item.presentation)
                        || item.title.empty()
                        || !std::isfinite(item.takeDistance)
                        || item.takeDistance <= 0.0f) {
                    return;
                }
                Vector3 point = transform.position;
                if (assets != nullptr) {
                    const engine::ModelAsset* asset =
                            assets->GetModelAsset(item.model);
                    if (asset != nullptr && asset->hasLocalBounds) {
                        const Matrix authored = BuildSectorStaticModelAuthoredTransform(
                                transform.position,
                                transform.rotationXRadians,
                                transform.yawRadians,
                                transform.rotationZRadians,
                                item.scale);
                        point = ClosestPoint(
                                TransformBounds(asset->localBounds, authored),
                                eyePosition);
                    }
                }
                ConsiderTarget(
                        world,
                        entity,
                        SectorUseTargetKind::Item,
                        point,
                        item.takeDistance,
                        eyePosition,
                        forward,
                        collisionWorld,
                        best);
            });

    {
        world.ForEach<SectorDynamicModel, SectorObjectTransform, engine::AnimatedModelInstance>(
                [&](engine::Entity entity,
                        SectorDynamicModel& prop,
                        SectorObjectTransform& transform,
                        engine::AnimatedModelInstance& instance) {
                    if ((!world.Has<SectorPropDrag>(entity) && (!includeDynamicProps || prop.onUseScript.empty() || prop.useConsumed))
                            || !std::isfinite(prop.useDistance)
                            || prop.useDistance <= 0.0f) {
                        return;
                    }
                    if (world.Has<SectorPropDrag>(entity)
                            && (!world.Has<SectorStaticModelCollider>(entity)
                                || !world.Get<SectorStaticModelCollider>(entity).resolved
                                || world.Get<SectorStaticModelCollider>(entity).failed
                                || (topologyMap && !FindSectorPath(topologyMap->paths,world.Get<SectorPropDrag>(entity).settings.pathEditorId)))) return;
                    Vector3 point = transform.position;
                    if (assets != nullptr) {
                        const engine::ModelAsset* asset = assets->GetModelAsset(instance.model);
                        if (asset != nullptr && (asset->hasAnimatedLocalBounds || asset->hasLocalBounds)) {
                            const BoundingBox localBounds = asset->hasAnimatedLocalBounds
                                    ? asset->animatedLocalBounds : asset->localBounds;
                            const Matrix authored = BuildSectorStaticModelAuthoredTransform(
                                    transform.position,
                                    transform.rotationXRadians,
                                    transform.yawRadians,
                                    transform.rotationZRadians,
                                    prop.scale);
                            point = ClosestPoint(TransformBounds(localBounds, authored), eyePosition);
                        }
                    }
                    ConsiderTarget(
                            world,
                            entity,
                            SectorUseTargetKind::DynamicProp,
                            point,
                            prop.useDistance,
                            eyePosition,
                            forward,
                            collisionWorld,
                            best);
                });
    }

    world.ForEach<NpcRuntimeInstance, SectorObject, SectorObjectTransform, SectorDynamicModel>(
            [&](engine::Entity entity, NpcRuntimeInstance& npc, SectorObject& object,
                    SectorObjectTransform& transform, SectorDynamicModel& model) {
                if (npc.hostile || npc.conversationHeld || npc.onUseScript.empty()
                        || !object.visible || model.opacity <= 0.0f
                        || (world.Has<Health>(entity) && IsDepleted(world.Get<Health>(entity)))
                        || (world.Has<NpcCombatState>(entity) && world.Get<NpcCombatState>(entity).dead)) return;
                BoundingBox bounds{{-0.3f, 0, -0.3f}, {0.3f, 1.8f, 0.3f}};
                if (assets && world.Has<engine::AnimatedModelInstance>(entity)) {
                    const auto* asset = assets->GetModelAsset(world.Get<engine::AnimatedModelInstance>(entity).model);
                    if (!asset || !asset->hasLocalBounds) return;
                    bounds = asset->hasAnimatedLocalBounds ? asset->animatedLocalBounds : asset->localBounds;
                }
                const Matrix authored = BuildSectorStaticModelAuthoredTransform(transform.position,
                        transform.rotationXRadians, transform.yawRadians, transform.rotationZRadians, model.scale);
                bounds = TransformBounds(bounds, authored);
                // At close range the player's gaze may meet any part of the body,
                // rather than a single center point outside the facing cone.
                const Ray ray{eyePosition, forward};
                const RayCollision hit = GetRayCollisionBox(ray, bounds);
                const Vector3 point = hit.hit ? hit.point : ClosestPoint(bounds, eyePosition);
                if (runtimeObjects && collisionWorld
                        && !HasNpcLineOfSight(*collisionWorld, runtimeObjects->dynamicDoorColliders,
                                runtimeObjects->physicalModelColliders, eyePosition, point)) return;
                ConsiderTarget(world, entity, SectorUseTargetKind::Npc, point, npc.useDistance,
                        eyePosition, forward, collisionWorld, best);
            });

    world.ForEach<SectorDoor, SectorDoorResolvedAnchor, SectorDoorInteraction>(
            [&](engine::Entity entity,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorInteraction& interaction) {
                if (!door.enabled || interaction.autoOpen
                        || !std::isfinite(interaction.interactionDistance)
                        || interaction.interactionDistance <= 0.0f) {
                    return;
                }
                const Vector2 pointXZ = ClosestPointOnSegment(
                        Vector2{eyePosition.x, eyePosition.z},
                        anchor.endpointA,
                        anchor.endpointB);
                const float minimumY = std::min(anchor.openBottom, anchor.openTop);
                const float maximumY = std::max(anchor.openBottom, anchor.openTop);
                const Vector3 point{
                        pointXZ.x,
                        std::clamp(eyePosition.y, minimumY, maximumY),
                        pointXZ.y};
                ConsiderTarget(
                        world,
                        entity,
                        SectorUseTargetKind::Door,
                        point,
                        interaction.interactionDistance,
                        eyePosition,
                        forward,
                        collisionWorld,
                        best);
            });
    world.ForEach<SectorDuctAccess>(
            [&](engine::Entity entity, SectorDuctAccess& access) {
                if (!std::isfinite(ductInteractionDistanceWorld)
                        || ductInteractionDistanceWorld <= 0.0f) return;
                if (viewerSectorId == access.crawlspaceSectorId
                        && !IsSectorDuctCoverBlocking(access)) return;
                if (access.cover.enabled
                        && access.coverPhase != SectorDuctCoverPhase::Attached
                        && access.coverPhase != SectorDuctCoverPhase::Settled) return;
                const Vector2 eyeXZ{eyePosition.x, eyePosition.z};
                const Vector2 delta{eyeXZ.x - access.centerXZ.x,
                        eyeXZ.y - access.centerXZ.y};
                const float tangentDistance = std::clamp(
                        Vector2DotProduct(delta, access.tangent),
                        -access.width * 0.5f, access.width * 0.5f);
                const Vector2 closestXZ{
                        access.centerXZ.x + access.tangent.x * tangentDistance,
                        access.centerXZ.y + access.tangent.y * tangentDistance};
                const Vector3 point{
                        closestXZ.x,
                        std::clamp(eyePosition.y,
                                access.openingBottom, access.openingTop),
                        closestXZ.y};
                ConsiderTarget(world, entity, SectorUseTargetKind::DuctAccess,
                        point, ductInteractionDistanceWorld, eyePosition,
                        forward, collisionWorld, best);
            });
    if (topologyMap != nullptr) {
        for (const SectorCompiledStructuralPrimitive& compiled
                : topologyMap->compiledStructuralPrimitives) {
            const SectorAuthoringStructuralPrimitive& ladder = compiled.authored;
            if (!ladder.enabled
                    || ladder.kind != SectorStructuralPrimitiveKind::Ladder) {
                continue;
            }
            const Vector2 center = SectorCoordToWorldPosition2(ladder.x, ladder.z);
            const Vector3 front3 = RotateSectorStructuralPrimitiveVector(
                    ladder, Vector3{0.0f, 0.0f, 1.0f});
            const float length = std::hypot(front3.x, front3.z);
            if (!(length > 0.0001f)) continue;
            const Vector2 front{front3.x / length, front3.z / length};
            const float depth = SectorStructuralLadderFrameThicknessWorld
                    * ladder.ladder.thicknessScale;
            constexpr float InteractionOffset = 0.55f;
            constexpr float InteractionHeight = 1.0f;
            const float bottomY = SectorAuthoringToWorldDistance(ladder.ladder.bottom);
            const float topY = SectorAuthoringToWorldDistance(
                    ladder.ladder.bottom + ladder.ladder.height);
            const std::array<std::pair<SectorLadderEndpoint, Vector3>, 2> anchors{{
                    {SectorLadderEndpoint::Bottom,
                            Vector3{
                                    center.x + front.x
                                            * (depth * 0.5f + InteractionOffset),
                                    bottomY + InteractionHeight,
                                    center.y + front.y
                                            * (depth * 0.5f + InteractionOffset)}},
                    {SectorLadderEndpoint::Top,
                            Vector3{
                                    center.x - front.x
                                            * (depth * 0.5f + InteractionOffset),
                                    topY + InteractionHeight,
                                    center.y - front.y
                                            * (depth * 0.5f + InteractionOffset)}}}};
            for (const auto& anchor : anchors) {
                const Vector3 offset = Vector3Subtract(anchor.second, eyePosition);
                const float distance = Vector3Length(offset);
                if (!std::isfinite(distance) || distance > 2.0f) continue;
                const float side = anchor.first == SectorLadderEndpoint::Bottom
                        ? 1.0f : -1.0f;
                const Vector3 target{
                        center.x + front.x * side * depth * 0.5f,
                        eyePosition.y,
                        center.y + front.y * side * depth * 0.5f};
                const Vector3 targetOffset = Vector3Subtract(target, eyePosition);
                const float targetDistance = Vector3Length(targetOffset);
                const float facing = targetDistance > 0.0001f
                        ? Vector3DotProduct(
                                forward,
                                Vector3Scale(targetOffset, 1.0f / targetDistance))
                        : 1.0f;
                if (facing < UseFacingDotThreshold
                        || !IsVisible(
                                collisionWorld,
                                eyePosition,
                                target,
                                targetDistance)) {
                    continue;
                }
                if (HiddenByDraggableProp(world,engine::NullEntity(),eyePosition,target,targetDistance)) continue;
                    const bool better = best.kind == SectorUseTargetKind::None
                        || facing > best.facingDot + 0.0001f
                        || (std::fabs(facing - best.facingDot) <= 0.0001f
                                && distance < best.distance - 0.0001f)
                        || (best.kind == SectorUseTargetKind::Ladder
                                && std::fabs(facing - best.facingDot) <= 0.0001f
                                && std::fabs(distance - best.distance) <= 0.0001f
                                && ladder.id < best.ladderPrimitiveId);
                if (!better) continue;
                best = {};
                best.kind = SectorUseTargetKind::Ladder;
                best.targetPosition = target;
                best.facingDot = facing;
                best.distance = distance;
                best.ladderPrimitiveId = ladder.id;
                best.ladderEndpoint = anchor.first;
            }
        }
    }
    return best;
}

std::string_view SectorUseTargetTitle(
        engine::World& world,
        const SectorUseTarget& target)
{
    if (target.kind == SectorUseTargetKind::Ladder) return "Ladder";
    if (!world.IsAlive(target.entity)) return {};
    if (target.kind == SectorUseTargetKind::Npc && world.Has<NpcRuntimeInstance>(target.entity))
        return world.Get<NpcRuntimeInstance>(target.entity).displayName;
    if (target.kind == SectorUseTargetKind::Item
            && world.Has<SectorItem>(target.entity)) {
        return world.Get<SectorItem>(target.entity).title;
    }
    if (target.kind == SectorUseTargetKind::DynamicProp
            && world.Has<SectorDynamicModel>(target.entity)) {
        return world.Get<SectorDynamicModel>(target.entity).useTitle;
    }
    if (target.kind == SectorUseTargetKind::Door
            && world.Has<SectorDoorInteraction>(target.entity)) {
        return world.Get<SectorDoorInteraction>(target.entity).useTitle;
    }
    if (target.kind == SectorUseTargetKind::DuctAccess
            && world.Has<SectorDuctAccess>(target.entity)) {
        return IsSectorDuctCoverBlocking(
                world.Get<SectorDuctAccess>(target.entity))
                ? "Vent Cover" : "Duct Access";
    }
    return {};
}

namespace {

float EvaluateUseHighlightPulse(float elapsedSeconds)
{
    constexpr float Tau = 6.28318530717958647692f;
    const float phase = std::fmod(
            elapsedSeconds,
            UseHighlightPeriodSeconds) / UseHighlightPeriodSeconds;
    const float pulse = 0.5f - 0.5f * std::cos(Tau * phase);
    return pulse * UseHighlightMaximumStrength;
}

} // namespace

void ResetSectorUseHighlight(SectorUseHighlightState& state)
{
    state = {};
}

void UpdateSectorUseHighlight(
        SectorUseHighlightState& state,
        const SectorUseTarget& target,
        float dt)
{
    dt = std::isfinite(dt) && dt > 0.0f ? dt : 0.0f;
    const bool hasDynamicPropTarget =
            (target.kind == SectorUseTargetKind::StaticProp
                    || target.kind == SectorUseTargetKind::DynamicProp
                    || target.kind == SectorUseTargetKind::Item)
            && !engine::IsNull(target.entity);
    if (hasDynamicPropTarget) {
        if (state.highlight.entity != target.entity || state.releasing) {
            state.highlight = SectorUseHighlight{target.entity, 0.0f};
            state.pulseElapsedSeconds = 0.0f;
        } else {
            state.pulseElapsedSeconds += dt;
        }
        state.releaseElapsedSeconds = 0.0f;
        state.releaseStartStrength = 0.0f;
        state.releasing = false;
        state.highlight.strength = EvaluateUseHighlightPulse(
                state.pulseElapsedSeconds);
        return;
    }

    if (engine::IsNull(state.highlight.entity)) return;
    if (!state.releasing) {
        state.releaseElapsedSeconds = 0.0f;
        state.releaseStartStrength = state.highlight.strength;
        state.releasing = true;
    }
    state.releaseElapsedSeconds += dt;
    const float release = std::clamp(
            state.releaseElapsedSeconds / UseHighlightReleaseSeconds,
            0.0f,
            1.0f);
    const float easedRelease = release * release * (3.0f - 2.0f * release);
    state.highlight.strength = state.releaseStartStrength
            * (1.0f - easedRelease);
    if (release >= 1.0f || state.highlight.strength <= 0.000001f) {
        ResetSectorUseHighlight(state);
    }
}

void DrawSectorUsePrompt(
        Rectangle viewport,
        const engine::FontAsset* font,
        std::string_view title,
        std::string_view action)
{
    if (font == nullptr || title.empty()) return;
    std::array<char, 24> prefix{};
    if (!action.empty()) std::snprintf(
            prefix.data(), prefix.size(), "%.*s ",
            static_cast<int>(action.size()), action.data());
    const float size = static_cast<float>(font->pixelSize);
    const float spacing = 1.0f;
    const Vector2 prefixSize = action.empty() ? Vector2{} : MeasureTextEx(
            font->font, prefix.data(), size, spacing);
    std::array<char, 128> titleText{};
    std::snprintf(
            titleText.data(),
            titleText.size(),
            "%.*s",
            static_cast<int>(title.size()),
            title.data());
    const Vector2 titleSize = MeasureTextEx(
            font->font, titleText.data(), size, spacing);
    const Vector2 origin{
            std::round(viewport.x
                    + (viewport.width - prefixSize.x - titleSize.x) * 0.5f),
            std::round(viewport.y + viewport.height - size - 48.0f)};
    const float titleX = std::round(origin.x + prefixSize.x);
    const Vector2 shadow = Vector2{
            std::round(origin.x + 3.0f), std::round(origin.y + 3.0f)};
    DrawTextEx(font->font, prefix.data(), shadow, size, spacing, Color{0, 0, 0, 220});
    DrawTextEx(
            font->font,
            titleText.data(),
            Vector2{std::round(titleX + 3.0f), shadow.y},
            size,
            spacing,
            Color{0, 0, 0, 220});
    DrawTextEx(font->font, prefix.data(), origin, size, spacing, RAYWHITE);
    DrawTextEx(
            font->font,
            titleText.data(),
            Vector2{titleX, origin.y},
            size,
            spacing,
            RAYWHITE);
}

void DrawSectorUseMessage(
        Rectangle viewport,
        const engine::FontAsset* font,
        std::string_view message,
        float elapsedSeconds)
{
    if (font == nullptr || message.empty()) return;
    const float fade = elapsedSeconds <= 1.5f
            ? 1.0f
            : std::clamp(1.0f - (elapsedSeconds - 1.5f) / 0.75f, 0.0f, 1.0f);
    if (fade <= 0.0f) return;
    std::array<char, 160> text{};
    std::snprintf(
            text.data(), text.size(), "%.*s",
            static_cast<int>(message.size()), message.data());
    const float size = static_cast<float>(font->pixelSize);
    const float spacing = 1.0f;
    const Vector2 measured = MeasureTextEx(
            font->font, text.data(), size, spacing);
    const Vector2 origin{
            std::round(viewport.x + (viewport.width - measured.x) * 0.5f),
            std::round(viewport.y + viewport.height - size - 48.0f)};
    DrawTextEx(
            font->font,
            text.data(),
            Vector2Add(origin, Vector2{3.0f, 3.0f}),
            size,
            spacing,
            Fade(BLACK, 0.86f * fade));
    DrawTextEx(
            font->font,
            text.data(),
            origin,
            size,
            spacing,
            Fade(RAYWHITE, fade));
}

} // namespace game
