#include "game/npc/ai/NpcAiDebugDraw.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/FontAssets.h"
#include "engine/ecs/World.h"
#include "engine/render/ColorTransfer.h"
#include "game/Health.h"
#include "game/npc/NpcRuntime.h"
#include "game/npc/ai/NpcAiDebugData.h"
#include "game/npc/ai/NpcAiSystem.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {
namespace {

constexpr float FloorOffsetWorld = 0.04f;
constexpr float PathOffsetWorld = 0.07f;
constexpr float LabelHeadClearanceWorld = 0.25f;
constexpr float SlotOffsetWorld = 0.10f;
constexpr float SlotMarkerRadiusWorld = 0.13f;

Color LinearOverlayColor(Color color)
{
    return engine::SrgbColorBytesToLinearSceneUnorm(color);
}

Vector3 RingPoint(Vector3 center, float radius, float radians)
{
    return Vector3{
            center.x + std::sin(radians) * radius,
            center.y,
            center.z + std::cos(radians) * radius};
}

void DrawGroundRing(
        Vector3 center,
        float radius,
        Color color,
        bool dashed)
{
    if (!std::isfinite(radius) || radius <= 0.0f) return;
    const int segments = std::clamp(
            static_cast<int>(std::ceil(radius * 8.0f)), 24, 96);
    Vector3 previous = RingPoint(center, radius, 0.0f);
    for (int segment = 1; segment <= segments; ++segment) {
        const float radians = 2.0f * PI
                * static_cast<float>(segment)
                / static_cast<float>(segments);
        const Vector3 current = RingPoint(center, radius, radians);
        if (!dashed || (segment & 1) == 0) {
            DrawLine3D(previous, current, color);
        }
        previous = current;
    }
}

void DrawVisionCone(
        Vector3 center,
        float yawRadians,
        float rangeWorld,
        float angleDegrees,
        Color color)
{
    if (!std::isfinite(rangeWorld) || rangeWorld <= 0.0f) return;
    const NpcAiDebugVisionGeometry cone = BuildNpcAiDebugVisionGeometry(
            center, yawRadians, rangeWorld, angleDegrees);
    DrawLine3D(cone.center, cone.leftBoundary, color);
    DrawLine3D(cone.center, cone.rightBoundary, color);
    const float arcRadians = std::max(0.0f,
            cone.endRadians - cone.startRadians);
    const int segments = std::clamp(
            static_cast<int>(std::ceil(arcRadians / (PI / 24.0f))),
            4,
            64);
    Vector3 previous = cone.leftBoundary;
    for (int segment = 1; segment <= segments; ++segment) {
        const float amount = static_cast<float>(segment)
                / static_cast<float>(segments);
        const Vector3 current = RingPoint(
                cone.center,
                rangeWorld,
                cone.startRadians + arcRadians * amount);
        DrawLine3D(previous, current, color);
        previous = current;
    }
}

void DrawLastKnownMarker(Vector3 position, Color color)
{
    position.y += PathOffsetWorld;
    constexpr float HalfSize = 0.18f;
    DrawLine3D(
            Vector3{position.x - HalfSize, position.y,
                    position.z - HalfSize},
            Vector3{position.x + HalfSize, position.y,
                    position.z + HalfSize},
            color);
    DrawLine3D(
            Vector3{position.x - HalfSize, position.y,
                    position.z + HalfSize},
            Vector3{position.x + HalfSize, position.y,
                    position.z - HalfSize},
            color);
    DrawLine3D(position,
            Vector3{position.x, position.y + 0.45f, position.z},
            color);
}

void DrawDashedLine(Vector3 start, Vector3 end, Color color)
{
    constexpr int Segments = 10;
    for (int segment = 0; segment < Segments; segment += 2) {
        const float begin = static_cast<float>(segment)
                / static_cast<float>(Segments);
        const float finish = static_cast<float>(segment + 1)
                / static_cast<float>(Segments);
        DrawLine3D(
                Vector3Lerp(start, end, begin),
                Vector3Lerp(start, end, finish),
                color);
    }
}

void DrawInvalidSlotMarker(Vector3 position, Color color)
{
    constexpr float HalfSize = 0.16f;
    DrawLine3D(
            {position.x - HalfSize, position.y, position.z - HalfSize},
            {position.x + HalfSize, position.y, position.z + HalfSize},
            color);
    DrawLine3D(
            {position.x - HalfSize, position.y, position.z + HalfSize},
            {position.x + HalfSize, position.y, position.z - HalfSize},
            color);
}

bool IsDeadNpc(const engine::World& world, engine::Entity entity)
{
    if (!world.Has<Health>(entity)) return false;
    if (IsDepleted(world.Get<Health>(entity))) return true;
    return world.Has<NpcCombatState>(entity)
            && world.Get<NpcCombatState>(entity).dead;
}

bool HasAiDebugComponents(
        const engine::World& world,
        const NpcNavigationRecord& record)
{
    return record.occupied
            && world.IsAlive(record.entity)
            && world.Has<NpcRuntimeInstance>(record.entity)
            && world.Has<NpcAiState>(record.entity)
            && world.Has<SectorObjectTransform>(record.entity)
            && world.Has<Health>(record.entity);
}

bool ProjectLabelAnchor(
        Vector3 anchor,
        const Camera3D& camera,
        Rectangle viewport,
        Vector2& projected)
{
    const Vector3 cameraForward = Vector3Subtract(
            camera.target, camera.position);
    const Vector3 toAnchor = Vector3Subtract(anchor, camera.position);
    if (Vector3DotProduct(cameraForward, toAnchor) <= 0.0f) return false;
    const int width = std::max(1,
            static_cast<int>(std::lround(viewport.width)));
    const int height = std::max(1,
            static_cast<int>(std::lround(viewport.height)));
    projected = GetWorldToScreenEx(anchor, camera, width, height);
    if (projected.x < 0.0f || projected.x > viewport.width
            || projected.y < 0.0f || projected.y > viewport.height) {
        return false;
    }
    projected.x += viewport.x;
    projected.y += viewport.y;
    return true;
}

} // namespace

void DrawNpcAiDebugWorld(
        const engine::World& world,
        const NpcNavigationRuntime& navigation,
        const NpcAiRuntime& aiRuntime,
        const SectorMeshRenderer& renderer)
{
    if (!renderer.IsRendererReady()) return;
    const Color hearingColor = LinearOverlayColor(
            Color{70, 206, 255, 220});
    const Color visionRangeColor = LinearOverlayColor(
            Color{92, 242, 132, 100});
    const Color visionConeColor = LinearOverlayColor(
            Color{92, 242, 132, 245});
    const Color attackRangeColor = LinearOverlayColor(
            Color{255, 92, 92, 235});
    const Color pathColor = LinearOverlayColor(
            Color{255, 102, 214, 245});
    const Color lastKnownColor = LinearOverlayColor(
            Color{255, 214, 78, 245});
    const Color soundColor = LinearOverlayColor(
            Color{255, 170, 62, 205});
    const Color meleeSlotColor = LinearOverlayColor(
            Color{78, 244, 116, 245});
    const Color orbitSlotColor = LinearOverlayColor(
            Color{255, 178, 52, 245});
    const Color invalidSlotColor = LinearOverlayColor(
            Color{255, 64, 64, 245});

    BeginMode3D(renderer.RenderCamera());
    const auto sampleColor = [](float normalized) {
        const float value = std::clamp(normalized, 0.0f, 1.0f);
        return LinearOverlayColor(Color{
                static_cast<unsigned char>(35.0f + value * 220.0f),
                static_cast<unsigned char>(70.0f + value * 180.0f),
                static_cast<unsigned char>(160.0f - value * 80.0f),
                245});
    };
    for (size_t index = 0;
            index < aiRuntime.playerLightLevel.points.size(); ++index) {
        const PlayerLightPointSample& point =
                aiRuntime.playerLightLevel.points[index];
        const Color color = sampleColor(point.normalizedLight);
        DrawSphere(point.positionWorld, 0.055f, color);
        if (index > 0) {
            DrawLine3D(
                    aiRuntime.playerLightLevel.points[index - 1].positionWorld,
                    point.positionWorld,
                    color);
        }
    }
    for (const NpcSoundEvent& sound : aiRuntime.playerSounds) {
        Vector3 center = sound.positionWorld;
        center.y += FloorOffsetWorld;
        const float fade = std::clamp(
                sound.remainingSeconds / 1.5f, 0.2f, 1.0f);
        Color faded = soundColor;
        faded.a = static_cast<unsigned char>(
                static_cast<float>(faded.a) * fade);
        DrawGroundRing(center, sound.radiusWorld, faded, true);
        DrawLine3D(center,
                Vector3{center.x, center.y + 0.25f, center.z},
                faded);
    }

    for (const NpcPursuitSlot& slot : aiRuntime.pursuitSlots) {
        const Color color = slot.kind == NpcPursuitSlotKind::Melee
                ? meleeSlotColor
                : slot.kind == NpcPursuitSlotKind::Orbit
                ? orbitSlotColor
                : invalidSlotColor;
        Vector3 requested = slot.requestedPosition;
        requested.y += SlotOffsetWorld;
        if (!slot.projected) {
            DrawInvalidSlotMarker(requested, color);
            continue;
        }
        Vector3 resolved = slot.resolvedPosition;
        resolved.y += SlotOffsetWorld;
        DrawGroundRing(resolved, SlotMarkerRadiusWorld, color, false);
        if (slot.claimed) DrawSphere(resolved, 0.055f, color);
        const Vector2 projectionDelta{
                resolved.x - requested.x,
                resolved.z - requested.z};
        if (Vector2Length(projectionDelta) > 0.05f) {
            DrawDashedLine(requested, resolved, color);
        }
        if (slot.claimed && world.IsAlive(slot.owner)
                && world.Has<SectorObjectTransform>(slot.owner)) {
            Vector3 owner = world.Get<SectorObjectTransform>(slot.owner).position;
            owner.y += SlotOffsetWorld;
            DrawLine3D(owner, resolved, color);
        }
    }

    for (const NpcNavigationRecord& record : navigation.records) {
        if (!HasAiDebugComponents(world, record)) continue;
        const NpcAiState& ai = world.Get<NpcAiState>(record.entity);
        const SectorObjectTransform& transform =
                world.Get<SectorObjectTransform>(record.entity);
        if (IsDeadNpc(world, record.entity)) continue;
        Vector3 center = transform.position;
        center.y += FloorOffsetWorld;
        DrawGroundRing(
                center, ai.perception.hearingRangeWorld,
                hearingColor, false);
        DrawGroundRing(
                center, ai.perception.visionRangeWorld,
                visionRangeColor, true);
        DrawVisionCone(
                center,
                transform.yawRadians,
                ai.perception.visionRangeWorld,
                ai.perception.visionAngleDegrees,
                visionConeColor);
        DrawGroundRing(
                center, ai.attack.rangeWorld,
                attackRangeColor, false);

        const Vector3 npcEye{
                transform.position.x,
                transform.position.y + 1.4f,
                transform.position.z};
        const Vector3 playerTarget =
                aiRuntime.playerLightLevel.points.back().positionWorld;
        const Color sightColor = LinearOverlayColor(
                ai.visualDetectionReason == NpcVisualDetectionReason::Detected
                        ? Color{255, 70, 70, 245}
                        : ai.visualDetectionReason
                                        == NpcVisualDetectionReason::Building
                        ? Color{255, 205, 60, 235}
                        : Color{90, 130, 180, 170});
        DrawLine3D(npcEye, playerTarget, sightColor);

        if (ai.awareness != NpcAwarenessState::Unaware) {
            DrawLastKnownMarker(ai.lastKnownPlayerPosition, lastKnownColor);
        }
        if (record.phase == NpcMovePhase::FollowingPath
                && NpcAiDebugRemainingCornerCount(record) > 0) {
            Vector3 previous = record.physicalPosition;
            previous.y += PathOffsetWorld;
            for (size_t cornerIndex = record.nextCorner;
                    cornerIndex < record.cornerCount;
                    ++cornerIndex) {
                Vector3 corner = record.corners[cornerIndex];
                corner.y += PathOffsetWorld;
                DrawLine3D(previous, corner, pathColor);
                DrawSphere(corner, 0.05f, pathColor);
                previous = corner;
            }
        }
    }
    EndMode3D();
}

void DrawNpcAiDebugLabels(
        const engine::World& world,
        const NpcNavigationRuntime& navigation,
        const NpcAiRuntime& aiRuntime,
        const SectorMeshRenderer& renderer,
        float agentHeight,
        Vector3 playerFeetPosition,
        bool aiFrozen,
        engine::AssetManager& assets,
        engine::FontHandle font,
        Rectangle playableViewport)
{
    if (!renderer.IsRendererReady()
            || playableViewport.width <= 0.0f
            || playableViewport.height <= 0.0f) return;
    const engine::FontAsset* fontAsset = assets.GetFont(font);
    const Font nativeFont = fontAsset != nullptr
            ? fontAsset->font : GetFontDefault();
    const float fontSize = fontAsset != nullptr
            ? static_cast<float>(fontAsset->pixelSize) : 20.0f;
    constexpr float Spacing = 1.0f;
    constexpr float PaddingX = 7.0f;
    constexpr float PaddingY = 5.0f;
    const float lineHeight = fontSize + 2.0f;
    const Camera3D& camera = renderer.RenderCamera();

    char playerText[320];
    std::snprintf(
            playerText,
            sizeof(playerText),
            "PLAYER VISIBILITY %.3f | response x%.2f | baked %.3f + dynamic %.3f = %.3f | crouch %.0f%% | move noise x%.2f%s",
            aiRuntime.playerLightLevel.normalizedLight,
            aiRuntime.playerLightDetectionFactor,
            aiRuntime.playerLightLevel.bakedLight,
            aiRuntime.playerLightLevel.dynamicLight,
            aiRuntime.playerLightLevel.combinedLight,
            aiRuntime.playerCrouchBlend * 100.0f,
            aiRuntime.playerMovementNoiseMultiplier,
            aiRuntime.playerLightLevel.bakedProbeAvailable
                    ? "" : " | AMBIENT FALLBACK");
    const Vector2 playerTextSize = MeasureTextEx(
            nativeFont, playerText, fontSize, Spacing);
    const Rectangle playerBox{
            playableViewport.x + 8.0f,
            playableViewport.y + 8.0f,
            std::min(playerTextSize.x + PaddingX * 2.0f,
                    playableViewport.width - 16.0f),
            lineHeight + PaddingY * 2.0f};
    DrawRectangleRec(playerBox, Color{8, 10, 14, 220});
    DrawRectangleLinesEx(playerBox, 1.0f, Color{100, 180, 255, 255});
    DrawTextEx(
            nativeFont,
            playerText,
            Vector2{playerBox.x + PaddingX, playerBox.y + PaddingY},
            fontSize,
            Spacing,
            WHITE);

    for (const NpcNavigationRecord& record : navigation.records) {
        if (!HasAiDebugComponents(world, record)) continue;
        const NpcRuntimeInstance& npc =
                world.Get<NpcRuntimeInstance>(record.entity);
        const NpcAiState& ai = world.Get<NpcAiState>(record.entity);
        const Health& health = world.Get<Health>(record.entity);
        const bool dead = IsDeadNpc(world, record.entity);
        Vector3 anchor = record.visualPosition;
        anchor.y += std::max(0.1f, agentHeight)
                + LabelHeadClearanceWorld;
        Vector2 screen{};
        if (!ProjectLabelAnchor(
                    anchor, camera, playableViewport, screen)) continue;

        const NpcAiDebugLabelData label = BuildNpcAiDebugLabelData(
                npc, ai, health, record, playerFeetPosition,
                aiFrozen, dead);
        float textWidth = 0.0f;
        for (const auto& line : label.lines) {
            textWidth = std::max(textWidth,
                    MeasureTextEx(
                            nativeFont, line.data(), fontSize, Spacing).x);
        }
        const float boxWidth = textWidth + PaddingX * 2.0f;
        const float boxHeight = lineHeight
                * static_cast<float>(label.lines.size())
                + PaddingY * 2.0f;
        if (boxWidth > playableViewport.width
                || boxHeight > playableViewport.height) continue;
        float boxX = screen.x - boxWidth * 0.5f;
        float boxY = screen.y - boxHeight - 8.0f;
        boxX = std::clamp(
                boxX,
                playableViewport.x,
                playableViewport.x + playableViewport.width - boxWidth);
        boxY = std::clamp(
                boxY,
                playableViewport.y,
                playableViewport.y + playableViewport.height - boxHeight);
        const Rectangle box{boxX, boxY, boxWidth, boxHeight};
        DrawRectangleRec(box, Color{8, 10, 14, 220});
        DrawRectangleLinesEx(box, 1.0f, label.headingColor);
        for (size_t lineIndex = 0;
                lineIndex < label.lines.size();
                ++lineIndex) {
            const Color color = lineIndex == 0
                    ? label.headingColor
                    : lineIndex == label.lines.size() - 1
                    ? Color{190, 200, 214, 255}
                    : WHITE;
            DrawTextEx(
                    nativeFont,
                    label.lines[lineIndex].data(),
                    Vector2{
                            box.x + PaddingX,
                            box.y + PaddingY
                                    + lineHeight
                                            * static_cast<float>(lineIndex)},
                    fontSize,
                    Spacing,
                    color);
        }
    }
}

} // namespace game
