#include "sector_demo/SectorUseInteraction.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/FontAssets.h"
#include "engine/assets/ModelAssets.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <limits>

namespace game {
namespace {

constexpr float UseFacingDotThreshold = 0.65f;
constexpr float UseOcclusionTolerance = 0.05f;
constexpr float UseHighlightPeriodSeconds = 2.4f;
constexpr float UseHighlightAttackSeconds = 0.18f;
constexpr float UseHighlightMinimumStrengthRatio = 0.4f;
constexpr float UseHighlightMaximumStrength = 0.14f;

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

SectorUseTarget FindSectorUseTarget(
        engine::World& world,
        const engine::AssetManager* assets,
        Vector3 eyePosition,
        Vector3 forward,
        const SectorCollisionWorld* collisionWorld,
        bool includeDynamicProps)
{
    SectorUseTarget best;
    if (!Finite(eyePosition) || !Finite(forward)
            || Vector3LengthSqr(forward) <= 0.000001f) {
        return best;
    }
    forward = Vector3Normalize(forward);

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
    return best;
}

std::string_view SectorUseTargetTitle(
        engine::World& world,
        const SectorUseTarget& target)
{
    if (!world.IsAlive(target.entity)) return {};
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

SectorUseHighlight BuildSectorUseHighlight(
        const SectorUseTarget& target,
        float targetElapsedSeconds)
{
    if (target.kind != SectorUseTargetKind::DynamicProp
            || engine::IsNull(target.entity)
            || !std::isfinite(targetElapsedSeconds)
            || targetElapsedSeconds < 0.0f) {
        return {};
    }
    constexpr float Tau = 6.28318530717958647692f;
    const float phase = std::fmod(
            targetElapsedSeconds,
            UseHighlightPeriodSeconds) / UseHighlightPeriodSeconds;
    const float pulse = 0.5f - 0.5f * std::cos(Tau * phase);
    const float attack = std::clamp(
            targetElapsedSeconds / UseHighlightAttackSeconds,
            0.0f,
            1.0f);
    return SectorUseHighlight{
            target.entity,
            attack * UseHighlightMaximumStrength
                    * (UseHighlightMinimumStrengthRatio
                            + (1.0f - UseHighlightMinimumStrengthRatio)
                                    * pulse)};
}

void DrawSectorUsePrompt(
        Rectangle viewport,
        const engine::FontAsset* font,
        std::string_view title)
{
    if (font == nullptr || title.empty()) return;
    constexpr const char* prefix = "Use ";
    const float size = static_cast<float>(font->pixelSize);
    const float spacing = 1.0f;
    const Vector2 prefixSize = MeasureTextEx(font->font, prefix, size, spacing);
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
            viewport.x + (viewport.width - prefixSize.x - titleSize.x) * 0.5f,
            viewport.y + viewport.height - size - 48.0f};
    const Vector2 shadow = Vector2Add(origin, Vector2{3.0f, 3.0f});
    DrawTextEx(font->font, prefix, shadow, size, spacing, Color{0, 0, 0, 220});
    DrawTextEx(
            font->font,
            titleText.data(),
            Vector2Add(shadow, Vector2{prefixSize.x, 0.0f}),
            size,
            spacing,
            Color{0, 0, 0, 220});
    DrawTextEx(font->font, prefix, origin, size, spacing, RAYWHITE);
    DrawTextEx(
            font->font,
            titleText.data(),
            Vector2Add(origin, Vector2{prefixSize.x, 0.0f}),
            size,
            spacing,
            RAYWHITE);
}

} // namespace game
