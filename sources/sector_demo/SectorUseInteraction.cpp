#include "sector_demo/SectorUseInteraction.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/FontAssets.h"
#include "engine/assets/ModelAssets.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "game/npc/NpcRuntime.h"
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

void ConsiderTarget(
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
    const bool better = best.kind == SectorUseTargetKind::None
            || facing > best.facingDot + 0.0001f
            || (std::fabs(facing - best.facingDot) <= 0.0001f
                    && (distance < best.distance - 0.0001f
                            || (std::fabs(distance - best.distance) <= 0.0001f
                                    && entity.index < best.entity.index)));
    if (!better) return;
    best = SectorUseTarget{entity, kind, targetPosition, facing, distance};
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
    if (kind != SectorUseTargetKind::StaticProp
            && kind != SectorUseTargetKind::DynamicProp) {
        return;
    }
    if (!Finite(ray.position) || !Finite(ray.direction)
            || Vector3LengthSqr(ray.direction) <= 0.000001f
            || !Finite(bounds.min) || !Finite(bounds.max)) {
        return;
    }
    ray.direction = Vector3Normalize(ray.direction);
    const RayCollision collision = GetRayCollisionBox(ray, bounds);
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
                        !model.instanceId.empty());
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
                if (!object.visible || world.Has<NpcRuntimeInstance>(entity)
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
                ConsiderSectorObjectUseBounds(
                        accumulator,
                        ray,
                        entity,
                        SectorUseTargetKind::DynamicProp,
                        TransformBounds(localBounds, authored),
                        !model.instanceId.empty());
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
    if (!world.IsAlive(target.entity)) return {};
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
        const SectorTopologyMap* topologyMap)
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
                        entity,
                        SectorUseTargetKind::Item,
                        point,
                        item.takeDistance,
                        eyePosition,
                        forward,
                        collisionWorld,
                        best);
            });

    if (includeDynamicProps) {
        world.ForEach<SectorDynamicModel, SectorObjectTransform, engine::AnimatedModelInstance>(
                [&](engine::Entity entity,
                        SectorDynamicModel& prop,
                        SectorObjectTransform& transform,
                        engine::AnimatedModelInstance& instance) {
                    if (prop.onUseScript.empty() || prop.useConsumed
                            || !std::isfinite(prop.useDistance)
                            || prop.useDistance <= 0.0f) {
                        return;
                    }
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
                        entity,
                        SectorUseTargetKind::Door,
                        point,
                        interaction.interactionDistance,
                        eyePosition,
                        forward,
                        collisionWorld,
                        best);
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
    if (font == nullptr || title.empty() || action.empty()) return;
    std::array<char, 24> prefix{};
    std::snprintf(
            prefix.data(), prefix.size(), "%.*s ",
            static_cast<int>(action.size()), action.data());
    const float size = static_cast<float>(font->pixelSize);
    const float spacing = 1.0f;
    const Vector2 prefixSize = MeasureTextEx(
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
