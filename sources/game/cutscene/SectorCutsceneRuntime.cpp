#include "game/cutscene/SectorCutsceneRuntime.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/FontAssets.h"
#include "engine/assets/ModelAssets.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/scripting/ScriptSystem.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace game {
namespace {

constexpr float ArrivalTolerance = 0.10f;
constexpr float MovementEpsilon = 0.0001f;
constexpr float ReplanAfterSeconds = 1.50f;
constexpr uint32_t MaximumReplans = 3;
constexpr double SayCodepointsPerSecond = 40.0;
constexpr double AutomaticHoldSecondsPerCodepoint = 0.045;
constexpr double MinimumAutomaticHoldSeconds = 1.5;
constexpr double MaximumAutomaticHoldSeconds = 8.0;
constexpr double TextFadeInSeconds = 0.25;
constexpr double CaptionFadeOutSeconds = 0.35;
constexpr size_t MaximumWrappedLines = 64;

double SafeDelta(float dt)
{
    return std::isfinite(dt) ? std::max(0.0, static_cast<double>(dt)) : 0.0;
}

float Smoothstep(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

float Smootherstep(float value)
{
    value = std::clamp(value, 0.0f, 1.0f);
    return value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
}

float ShortestAngleDelta(float from, float to)
{
    return std::remainder(to - from, 2.0f * PI);
}

// Follow the route visually without altering the collision movement delta.
Vector2 PlayerRouteLookAhead(
        const SectorCutscenePlayerMoveState& move, Vector3 position)
{
    float remaining = std::clamp(move.movementSpeed * 0.35f, 0.25f, 1.0f);
    Vector2 point{position.x, position.z};
    for (size_t i = move.nextCorner; i < move.cornerCount; ++i) {
        const Vector3 corner = i == move.nextCorner
                && move.doorPhase == NpcDoorTraversalPhase::Crossing
                ? move.doorLanding : move.corners[i];
        const Vector2 end{corner.x, corner.z};
        const float distance = Vector2Distance(point, end);
        if (distance > remaining) {
            return Vector2Lerp(point, end, remaining / distance);
        }
        point = end;
        remaining -= distance;
        if (move.cornerDoorIds[i] > 0) break;
    }
    return point;
}

float PlayerRouteRemainingDistance(
        const SectorCutscenePlayerMoveState& move, Vector3 position)
{
    float remaining = 0.0f;
    Vector2 point{position.x, position.z};
    for (size_t i = move.nextCorner; i < move.cornerCount; ++i) {
        const Vector3 corner = i == move.nextCorner
                && move.doorPhase == NpcDoorTraversalPhase::Crossing
                ? move.doorLanding : move.corners[i];
        const Vector2 end{corner.x, corner.z};
        remaining += Vector2Distance(point, end);
        point = end;
        // Off-mesh door links include the approach and the landing.
        if (move.cornerDoorIds[i] > 0) {
            const Vector2 landing{
                    move.cornerDoorLandings[i].x, move.cornerDoorLandings[i].z};
            remaining += Vector2Distance(point, landing);
            point = landing;
        }
    }
    return remaining;
}

float SmoothPlayerFacing(float yaw, float target, float& velocity, float dt)
{
    if (dt <= 0.0f) return yaw;
    constexpr float omega = 2.0f / 0.2f;
    constexpr float maximumSpeed = 2.0f * PI;
    const float delta = ShortestAngleDelta(yaw, target);
    // Limit the spring displacement, then integrate the critically damped
    // spring analytically. This avoids Euler instability on long frames.
    const float change = -std::clamp(delta, -maximumSpeed * 0.2f,
            maximumSpeed * 0.2f);
    const float decay = std::exp(-omega * dt);
    const float nextVelocity = (velocity * (1.0f - omega * dt)
            - omega * omega * change * dt) * decay;
    const float step = -change + (change * (1.0f + omega * dt)
            + velocity * dt) * decay;
    velocity = std::clamp(nextVelocity, -maximumSpeed, maximumSpeed);
    if (step * delta >= 0.0f && std::fabs(step) >= std::fabs(delta)) {
        velocity = 0.0f;
        return yaw + delta;
    }
    return yaw + std::clamp(step, -maximumSpeed * dt, maximumSpeed * dt);
}

bool ApplyLookPoint(
        SectorCutsceneLookState& look,
        SectorFpsControllerState& player,
        Vector3 eye, Vector3 target, float dt)
{
    const Vector3 delta = Vector3Subtract(target, eye);
    const float horizontal = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    if (!std::isfinite(horizontal) || !std::isfinite(delta.y)
            || (horizontal <= MovementEpsilon
                && std::fabs(delta.y) <= MovementEpsilon)) return false;
    look.elapsedSeconds += SafeDelta(dt);
    const float progress = look.durationSeconds <= 0.0 ? 1.0f
            : static_cast<float>(look.elapsedSeconds / look.durationSeconds);
    const float eased = Smootherstep(progress);
    // Unwrap against the previous target so a moving target crossing +/-PI
    // cannot flip the interpolation to the other side of the camera.
    look.targetYawRadians += ShortestAngleDelta(
            look.targetYawRadians, std::atan2(delta.z, delta.x));
    const float targetPitch = ClampSectorFpsPitch(
            std::atan2(delta.y, std::max(horizontal, MovementEpsilon)));
    player.yawRadians = look.startYawRadians
            + (look.targetYawRadians - look.startYawRadians) * eased;
    player.pitchRadians = ClampSectorFpsPitch(look.startPitchRadians
            + (targetPitch - look.startPitchRadians) * eased);
    return true;
}

void ReleasePlayerPath(
        SectorCutscenePlayerMoveState& move,
        SectorNavigationWorld* navigation)
{
    if (navigation != nullptr && !IsNull(move.pathHandle)) {
        navigation->ReleasePathRecord(move.pathHandle);
    }
    move.pathHandle = {};
    move.cornerCount = 0;
    move.nextCorner = 0;
    move.corridorTileCount = 0;
    move.pathTileRevision = 0;
    move.doorPhase = NpcDoorTraversalPhase::None;
    move.doorId = 0;
    move.doorDirection = SectorNavigationDoorDirection::None;
    move.doorLanding = {};
    move.doorWaitSeconds = 0.0f;
    move.holdsDoor = false;
    move.doorReplanRequested = false;
}

bool CopyPlayerPath(
        SectorCutscenePlayerMoveState& move,
        SectorNavigationWorld& navigation,
        const SectorNavigationPathResult& path)
{
    ReleasePlayerPath(move, &navigation);
    move.pathHandle = navigation.AllocatePathRecord();
    if (IsNull(move.pathHandle)) return false;
    move.cornerCount = std::min(path.cornerCount, move.corners.size());
    std::copy_n(path.corners.begin(), move.cornerCount, move.corners.begin());
    std::copy_n(path.cornerDoorIds.begin(), move.cornerCount,
            move.cornerDoorIds.begin());
    std::copy_n(path.cornerDoorDirections.begin(), move.cornerCount,
            move.cornerDoorDirections.begin());
    std::copy_n(path.cornerDoorLandings.begin(), move.cornerCount,
            move.cornerDoorLandings.begin());
    move.corridorTileCount = std::min(
            path.corridorTileCount, move.corridorTiles.size());
    std::copy_n(path.corridorTiles.begin(), move.corridorTileCount,
            move.corridorTiles.begin());
    move.pathTileRevision = path.tileRevision;
    move.projectedDestination = path.projectedDestination;
    move.nextCorner = 0;
    while (move.nextCorner < move.cornerCount) {
        if (move.cornerDoorIds[move.nextCorner] > 0) break;
        const Vector3& corner = move.corners[move.nextCorner];
        const float dx = corner.x - path.projectedStart.x;
        const float dz = corner.z - path.projectedStart.z;
        if (std::sqrt(dx * dx + dz * dz) > ArrivalTolerance) break;
        ++move.nextCorner;
    }
    return true;
}

bool BuildPlayerPath(
        SectorCutscenePlayerMoveState& move,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        const SectorFpsControllerState& player,
        std::string& error)
{
    if (navigation.State() != SectorNavigationState::Ready) {
        error = "navigation is not ready";
        return false;
    }
    const int sectorId = collisionWorld.FindSectorContainingPointPreferCurrent(
            move.requestedDestinationXZ, player.currentSectorId);
    SectorCollisionHeights heights;
    if (sectorId == 0
            || !collisionWorld.GetSectorFloorCeiling(sectorId, &heights)) {
        error = "destination is outside sector collision";
        return false;
    }
    const SectorNavigationPathResult path = navigation.FindPath(
            player.feetPosition,
            Vector3{move.requestedDestinationXZ.x, heights.floorZ,
                    move.requestedDestinationXZ.y},
            {true});
    if (path.status != SectorNavigationQueryStatus::Success) {
        error = SectorNavigationQueryStatusName(path.status);
        return false;
    }
    if (!CopyPlayerPath(move, navigation, path)) {
        error = "navigation path record capacity was exceeded";
        return false;
    }
    error.clear();
    return true;
}

void FailPlayerMove(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        engine::ScriptRuntime& scripts,
        const char* reason)
{
    SectorCutscenePlayerMoveState& move = runtime.playerMove;
    const engine::ScriptOperationHandle operation = move.operation;
    ReleasePlayerPath(move, &navigation);
    move.active = false;
    move.arrivalLook.active = false;
    if (engine::IsValid(operation)) {
        engine::ScriptSystemFailOperation(
                scripts, operation, reason != nullptr ? reason : "movement failed");
    }
}

void CompletePlayerMove(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        engine::ScriptRuntime& scripts)
{
    SectorCutscenePlayerMoveState& move = runtime.playerMove;
    const engine::ScriptOperationHandle operation = move.operation;
    ReleasePlayerPath(move, &navigation);
    move.active = false;
    move.arrivalLook.active = false;
    if (engine::IsValid(operation)) {
        engine::ScriptSystemCompleteOperation(scripts, operation);
    }
}

size_t CountCodepoints(std::string_view text, bool& valid)
{
    valid = true;
    size_t count = 0;
    for (size_t cursor = 0; cursor < text.size();) {
        int bytes = 0;
        const int codepoint = GetCodepointNext(text.data() + cursor, &bytes);
        if (bytes <= 0 || cursor + static_cast<size_t>(bytes) > text.size()
                || (codepoint == 0x3f
                    && static_cast<unsigned char>(text[cursor]) >= 0x80)) {
            valid = false;
            return count;
        }
        cursor += static_cast<size_t>(bytes);
        ++count;
    }
    return count;
}

size_t BytePrefixForCodepoints(std::string_view text, size_t count)
{
    size_t cursor = 0;
    size_t seen = 0;
    while (cursor < text.size() && seen < count) {
        int bytes = 0;
        GetCodepointNext(text.data() + cursor, &bytes);
        cursor += static_cast<size_t>(std::max(1, bytes));
        ++seen;
    }
    return std::min(cursor, text.size());
}

bool ResolveLookTargetPoint(
        engine::World& world,
        engine::AssetManager& assets,
        const SectorCutsceneLookState& look,
        Vector3& point)
{
    if (!world.IsAlive(look.entity)
            || !world.Has<SectorObjectTransform>(look.entity)) return false;
    const SectorObjectTransform& transform =
            world.Get<SectorObjectTransform>(look.entity);
    Vector3 renderPosition = transform.position;
    if (world.Has<SectorObjectVisualOffset>(look.entity)) {
        renderPosition = Vector3Add(
                renderPosition,
                world.Get<SectorObjectVisualOffset>(look.entity).position);
    }

    engine::ModelHandle modelHandle = engine::NullModelHandle();
    float scale = 1.0f;
    bool animated = false;
    if (look.targetKind == SectorCutsceneLookTargetKind::Npc) {
        if (!world.Has<NpcRuntimeInstance>(look.entity)
                || !world.Has<SectorDynamicModel>(look.entity)
                || !world.Has<engine::AnimatedModelInstance>(look.entity)) {
            return false;
        }
        scale = world.Get<SectorDynamicModel>(look.entity).scale;
        modelHandle = world.Get<engine::AnimatedModelInstance>(look.entity).model;
        animated = true;
    } else if (world.Has<SectorStaticModel>(look.entity)) {
        const SectorStaticModel& model = world.Get<SectorStaticModel>(look.entity);
        modelHandle = model.model;
        scale = model.scale;
    } else if (world.Has<SectorDynamicModel>(look.entity)
            && world.Has<engine::AnimatedModelInstance>(look.entity)
            && !world.Has<NpcRuntimeInstance>(look.entity)) {
        scale = world.Get<SectorDynamicModel>(look.entity).scale;
        modelHandle = world.Get<engine::AnimatedModelInstance>(look.entity).model;
        animated = true;
    } else {
        return false;
    }

    const engine::ModelAsset* asset = assets.GetModelAsset(modelHandle);
    if (asset == nullptr || !asset->hasLocalBounds) return false;
    const BoundingBox bounds = animated && asset->hasAnimatedLocalBounds
            ? asset->animatedLocalBounds : asset->localBounds;
    const Vector3 localPoint{
            (bounds.min.x + bounds.max.x) * 0.5f,
            bounds.min.y + (bounds.max.y - bounds.min.y) * look.targetHeight,
            (bounds.min.z + bounds.max.z) * 0.5f};
    const Matrix authored = BuildSectorStaticModelAuthoredTransform(
            renderPosition,
            transform.rotationXRadians,
            transform.yawRadians,
            transform.rotationZRadians,
            scale);
    point = Vector3Transform(localPoint, authored);
    return std::isfinite(point.x) && std::isfinite(point.y)
            && std::isfinite(point.z);
}

struct WrappedLine {
    size_t start = 0;
    size_t end = 0;
};

float GlyphAdvance(const Font& font, int codepoint, float spacing)
{
    const int index = GetGlyphIndex(font, codepoint);
    if (index < 0 || index >= font.glyphCount) return 0.0f;
    const GlyphInfo& glyph = font.glyphs[index];
    const float advance = static_cast<float>(glyph.advanceX > 0
            ? glyph.advanceX
            : font.recs[index].width + glyph.offsetX);
    return advance + spacing;
}

size_t BuildWrappedLines(
        std::string_view text,
        const Font& font,
        float maximumWidth,
        std::array<WrappedLine, MaximumWrappedLines>& lines)
{
    size_t lineCount = 0;
    size_t start = 0;
    size_t cursor = 0;
    size_t lastBreak = 0;
    float width = 0.0f;
    while (cursor < text.size() && lineCount < lines.size()) {
        const size_t codepointStart = cursor;
        int bytes = 0;
        const int codepoint = GetCodepointNext(text.data() + cursor, &bytes);
        cursor += static_cast<size_t>(std::max(1, bytes));
        if (codepoint == '\n') {
            lines[lineCount++] = {start, codepointStart};
            start = cursor;
            lastBreak = start;
            width = 0.0f;
            continue;
        }
        if (codepoint == ' ' || codepoint == '\t') lastBreak = cursor;
        width += GlyphAdvance(font, codepoint, 1.0f);
        if (width <= maximumWidth || codepointStart == start) continue;
        const size_t end = lastBreak > start ? lastBreak : codepointStart;
        lines[lineCount++] = {start, end};
        start = end;
        while (start < text.size()
                && (text[start] == ' ' || text[start] == '\t')) ++start;
        cursor = start;
        lastBreak = start;
        width = 0.0f;
    }
    if (lineCount < lines.size() && start <= text.size()) {
        lines[lineCount++] = {start, text.size()};
    }
    return lineCount;
}

} // namespace

void InitializeSectorCutsceneRuntime(SectorCutsceneRuntime& runtime)
{
    runtime = SectorCutsceneRuntime{};
    runtime.caption.text.reserve(kSectorCutsceneMaximumCaptionBytes);
}

void ResetSectorCutsceneRuntime(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld* navigation)
{
    ReleasePlayerPath(runtime.playerMove, navigation);
    const size_t captionCapacity = std::max(
            runtime.caption.text.capacity(),
            kSectorCutsceneMaximumCaptionBytes);
    runtime = SectorCutsceneRuntime{};
    runtime.caption.text.reserve(captionCapacity);
}

bool BeginSectorCutscenePlayerMove(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        const SectorFpsControllerState& player,
        Vector2 destinationXZ,
        NpcMoveGait gait,
        float movementSpeed,
        uint64_t& outToken,
        std::string& error)
{
    if (runtime.playerMove.active) {
        error = "player already has an active scripted move";
        return false;
    }
    if (!std::isfinite(destinationXZ.x) || !std::isfinite(destinationXZ.y)) {
        error = "player destination coordinates must be finite";
        return false;
    }
    if (!std::isfinite(movementSpeed) || movementSpeed <= 0.0f) {
        error = "player movement speed is invalid";
        return false;
    }
    SectorCutscenePlayerMoveState move;
    move.token = runtime.nextToken++;
    move.requestedDestinationXZ = destinationXZ;
    move.gait = gait;
    move.movementSpeed = movementSpeed;
    move.active = true;
    runtime.playerMove = move;
    if (!BuildPlayerPath(
            runtime.playerMove, navigation, collisionWorld, player, error)) {
        runtime.playerMove.active = false;
        return false;
    }
    outToken = runtime.playerMove.token;
    return true;
}

void BindSectorCutscenePlayerMoveOperation(
        SectorCutsceneRuntime& runtime,
        uint64_t token,
        engine::ScriptOperationHandle operation)
{
    if (runtime.playerMove.active && runtime.playerMove.token == token) {
        runtime.playerMove.operation = operation;
    }
}

void CancelSectorCutscenePlayerMove(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld* navigation,
        uint64_t token)
{
    if (!runtime.playerMove.active || runtime.playerMove.token != token) return;
    ReleasePlayerPath(runtime.playerMove, navigation);
    runtime.playerMove.active = false;
    runtime.playerMove.arrivalLook.active = false;
}

void PrepareSectorCutscenePlayerDoorTraversal(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        Vector3 playerFeetPosition,
        float rawDt)
{
    SectorCutscenePlayerMoveState& move = runtime.playerMove;
    if (!move.active || move.arrived || move.nextCorner >= move.cornerCount) return;
    if (move.doorPhase == NpcDoorTraversalPhase::None
            && move.cornerDoorIds[move.nextCorner] > 0) {
        move.doorPhase = NpcDoorTraversalPhase::Approaching;
        move.doorId = move.cornerDoorIds[move.nextCorner];
        move.doorDirection = move.cornerDoorDirections[move.nextCorner];
        move.doorLanding = move.cornerDoorLandings[move.nextCorner];
        move.doorWaitSeconds = 0.0f;
    }
    if (move.doorPhase == NpcDoorTraversalPhase::None) return;
    const Vector3 stage = move.corners[move.nextCorner];
    const float dx = stage.x - playerFeetPosition.x;
    const float dz = stage.z - playerFeetPosition.z;
    if (move.doorPhase == NpcDoorTraversalPhase::Approaching
            && std::sqrt(dx * dx + dz * dz) <= ArrivalTolerance) {
        move.doorPhase = NpcDoorTraversalPhase::WaitingForClearance;
        move.holdsDoor = true;
    }
    if (move.doorPhase != NpcDoorTraversalPhase::WaitingForClearance) return;
    move.doorWaitSeconds += static_cast<float>(SafeDelta(rawDt));
    move.holdsDoor = true;
    SectorNavigationDoorLinkState linkState;
    if (!navigation.GetDoorLinkRuntimeState(move.doorId, linkState)
            || linkState == SectorNavigationDoorLinkState::Disabled) {
        move.holdsDoor = false;
        move.doorReplanRequested = true;
        return;
    }
    if (SectorDoorTraversalIsClear(
                move.doorId,
                stage,
                move.doorLanding,
                navigation.Settings().agentRadius,
                navigation.Settings().agentHeight,
                doorColliders)) {
        move.doorPhase = NpcDoorTraversalPhase::Crossing;
    }
}

int SectorCutscenePlayerDoorHoldId(const SectorCutsceneRuntime& runtime)
{
    return runtime.playerMove.active && runtime.playerMove.holdsDoor
            ? runtime.playerMove.doorId : 0;
}

Vector2 BuildSectorCutscenePlayerMoveDelta(
        SectorCutsceneRuntime& runtime,
        const SectorFpsControllerState& player,
        float rawDt,
        float* outFacingYawRadians)
{
    if (outFacingYawRadians != nullptr) *outFacingYawRadians = player.yawRadians;
    SectorCutscenePlayerMoveState& move = runtime.playerMove;
    const float dt = static_cast<float>(SafeDelta(rawDt));
    if (!move.active || move.arrived || dt <= 0.0f || move.nextCorner >= move.cornerCount
            || move.doorPhase == NpcDoorTraversalPhase::WaitingForClearance) {
        return {};
    }
    const Vector3 target = move.doorPhase == NpcDoorTraversalPhase::Crossing
            ? move.doorLanding : move.corners[move.nextCorner];
    const Vector2 delta{
            target.x - player.feetPosition.x,
            target.z - player.feetPosition.z};
    const float distance = Vector2Length(delta);
    if (distance <= MovementEpsilon) return {};
    const Vector2 direction = Vector2Scale(delta, 1.0f / distance);
    if (outFacingYawRadians != nullptr) {
        const Vector2 lookPoint = PlayerRouteLookAhead(move, player.feetPosition);
        const Vector2 lookDelta = Vector2Subtract(lookPoint,
                Vector2{player.feetPosition.x, player.feetPosition.z});
        if (Vector2Length(lookDelta) > MovementEpsilon) {
            *outFacingYawRadians = std::atan2(lookDelta.y, lookDelta.x);
        }
    }
    return Vector2Scale(direction, std::min(distance, move.movementSpeed * dt));
}

bool AdvanceSectorCutscenePlayerCamera(
        SectorCutsceneRuntime& runtime,
        SectorFpsControllerState& player,
        Vector3 eyePosition,
        const Vector3* arrivalTarget,
        float rawDt)
{
    SectorCutscenePlayerMoveState& move = runtime.playerMove;
    if (!move.active) return true;
    const float dt = static_cast<float>(SafeDelta(rawDt));
    if (move.arrivalLook.active) {
        if (arrivalTarget == nullptr) return false;
        if (!move.arrivalLookStarted
                && move.doorPhase != NpcDoorTraversalPhase::WaitingForClearance
                && (move.arrived || PlayerRouteRemainingDistance(move,
                        player.feetPosition) <= move.movementSpeed
                                * move.arrivalLook.durationSeconds + ArrivalTolerance)) {
            move.arrivalLookStarted = true;
            move.arrivalLook.startYawRadians = player.yawRadians;
            move.arrivalLook.targetYawRadians = player.yawRadians;
            move.arrivalLook.startPitchRadians = player.pitchRadians;
            move.facingVelocity = 0.0f;
        }
        if (move.arrivalLookStarted) {
            return ApplyLookPoint(move.arrivalLook, player, eyePosition,
                    *arrivalTarget, dt);
        }
    }
    if (runtime.look.active || move.arrived) {
        move.facingVelocity = 0.0f;
        return true;
    }
    float targetYaw = player.yawRadians;
    BuildSectorCutscenePlayerMoveDelta(runtime, player, dt, &targetYaw);
    player.yawRadians = SmoothPlayerFacing(
            player.yawRadians, targetYaw, move.facingVelocity, dt);
    return true;
}

void UpdateSectorCutscenePlayerCamera(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        engine::World& world,
        engine::AssetManager& assets,
        SectorFpsControllerState& player,
        const SectorFpsControllerConfig& playerConfig,
        engine::ScriptRuntime& scripts,
        float dt)
{
    if (!runtime.playerMove.active) return;
    Vector3 target{};
    const bool hasTarget = runtime.playerMove.arrivalLook.active;
    if (hasTarget && !ResolveLookTargetPoint(
            world, assets, runtime.playerMove.arrivalLook, target)) {
        FailPlayerMove(runtime, navigation, scripts,
                "arrival look target was removed or is not ready");
        return;
    }
    if (!AdvanceSectorCutscenePlayerCamera(runtime, player,
            SectorFpsControllerEyePosition(player, playerConfig),
            hasTarget ? &target : nullptr, dt)) {
        FailPlayerMove(runtime, navigation, scripts,
                "arrival look target coincides with the camera");
    }
}

void FinishSectorCutscenePlayerMoveFrame(
        SectorCutsceneRuntime& runtime,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        const SectorFpsControllerState& player,
        Vector2 previousPositionXZ,
        engine::ScriptRuntime& scripts,
        float rawDt)
{
    SectorCutscenePlayerMoveState& move = runtime.playerMove;
    if (!move.active) return;
    if (move.arrived) {
        if (move.arrivalLook.elapsedSeconds >= move.arrivalLook.durationSeconds) {
            CompletePlayerMove(runtime, navigation, scripts);
        }
        return;
    }
    if (navigation.State() != SectorNavigationState::Ready
            || !navigation.IsPathRecordValid(move.pathHandle)) {
        FailPlayerMove(runtime, navigation, scripts,
                "navigation was rebuilt during movement");
        return;
    }
    if (move.doorReplanRequested) {
        std::string error;
        if (move.replanCount >= MaximumReplans
                || !BuildPlayerPath(
                        move, navigation, collisionWorld, player, error)) {
            FailPlayerMove(runtime, navigation, scripts,
                    "door link became unavailable during player movement");
            return;
        }
        ++move.replanCount;
    }
    if (navigation.CorridorTouchesChangedTile(
                move.corridorTiles.data(), move.corridorTileCount,
                move.pathTileRevision)) {
        std::string error;
        if (move.replanCount >= MaximumReplans
                || !BuildPlayerPath(
                        move, navigation, collisionWorld, player, error)) {
            FailPlayerMove(runtime, navigation, scripts,
                    "dynamic obstacle invalidated the player route");
            return;
        }
        ++move.replanCount;
    }

    while (move.nextCorner < move.cornerCount) {
        const Vector3 target = move.doorPhase == NpcDoorTraversalPhase::Crossing
                ? move.doorLanding : move.corners[move.nextCorner];
        const float dx = target.x - player.feetPosition.x;
        const float dz = target.z - player.feetPosition.z;
        if (std::sqrt(dx * dx + dz * dz) > ArrivalTolerance) break;
        if (move.doorPhase == NpcDoorTraversalPhase::Approaching
                || move.doorPhase
                        == NpcDoorTraversalPhase::WaitingForClearance) {
            break;
        }
        ++move.nextCorner;
        if (move.doorPhase == NpcDoorTraversalPhase::Crossing) {
            move.doorPhase = NpcDoorTraversalPhase::None;
            move.doorId = 0;
            move.doorDirection = SectorNavigationDoorDirection::None;
            move.doorLanding = {};
            move.doorWaitSeconds = 0.0f;
            move.holdsDoor = false;
        }
    }
    const float destinationDx = move.projectedDestination.x - player.feetPosition.x;
    const float destinationDz = move.projectedDestination.z - player.feetPosition.z;
    if (move.nextCorner >= move.cornerCount
            && std::sqrt(destinationDx * destinationDx
                    + destinationDz * destinationDz) <= ArrivalTolerance) {
        move.arrived = true;
        ReleasePlayerPath(move, &navigation);
        if (!move.arrivalLook.active || (move.arrivalLookStarted
                && move.arrivalLook.elapsedSeconds >= move.arrivalLook.durationSeconds)) {
            CompletePlayerMove(runtime, navigation, scripts);
        }
        return;
    }
    const Vector2 current{player.feetPosition.x, player.feetPosition.z};
    if (Vector2Distance(current, previousPositionXZ) <= MovementEpsilon
            && move.doorPhase != NpcDoorTraversalPhase::WaitingForClearance) {
        move.stallSeconds += static_cast<float>(SafeDelta(rawDt));
    } else {
        move.stallSeconds = 0.0f;
    }
    if (move.stallSeconds >= ReplanAfterSeconds) {
        std::string error;
        if (move.replanCount >= MaximumReplans
                || !BuildPlayerPath(move, navigation, collisionWorld, player, error)) {
            FailPlayerMove(runtime, navigation, scripts,
                    "player movement stalled after bounded replans");
            return;
        }
        ++move.replanCount;
        move.stallSeconds = 0.0f;
    }
}

bool BeginSectorCutsceneLook(
        SectorCutsceneRuntime& runtime,
        engine::Entity entity,
        SectorCutsceneLookTargetKind kind,
        float targetHeight,
        double durationSeconds,
        const SectorFpsControllerState& player,
        uint64_t& outToken,
        std::string& error)
{
    if (runtime.look.active || (runtime.playerMove.active
            && runtime.playerMove.arrivalLook.active)) {
        error = "camera already has an active scripted look";
        return false;
    }
    if (!std::isfinite(targetHeight) || targetHeight < 0.0f
            || targetHeight > 1.0f || !std::isfinite(durationSeconds)
            || durationSeconds < 0.0) {
        error = "scripted look arguments are invalid";
        return false;
    }
    SectorCutsceneLookState look;
    look.token = runtime.nextToken++;
    look.entity = entity;
    look.targetKind = kind;
    look.targetHeight = targetHeight;
    look.startYawRadians = player.yawRadians;
    look.startPitchRadians = player.pitchRadians;
    look.durationSeconds = durationSeconds;
    look.active = true;
    runtime.look = look;
    outToken = look.token;
    error.clear();
    return true;
}

void BindSectorCutsceneLookOperation(
        SectorCutsceneRuntime& runtime,
        uint64_t token,
        engine::ScriptOperationHandle operation)
{
    if (runtime.look.active && runtime.look.token == token) {
        runtime.look.operation = operation;
    }
}

void CancelSectorCutsceneLook(SectorCutsceneRuntime& runtime, uint64_t token)
{
    if (runtime.look.active && runtime.look.token == token) {
        runtime.look.active = false;
    }
}

void UpdateSectorCutsceneLook(
        SectorCutsceneRuntime& runtime,
        engine::World& world,
        engine::AssetManager& assets,
        SectorFpsControllerState& player,
        const SectorFpsControllerConfig& playerConfig,
        engine::ScriptRuntime& scripts,
        float dt)
{
    SectorCutsceneLookState& look = runtime.look;
    if (!look.active) return;
    Vector3 target;
    if (!ResolveLookTargetPoint(world, assets, look, target)) {
        const engine::ScriptOperationHandle operation = look.operation;
        look.active = false;
        engine::ScriptSystemFailOperation(
                scripts, operation, "look target was removed or is not ready");
        return;
    }
    const Vector3 eye = SectorFpsControllerEyePosition(
            player, playerConfig);
    const Vector3 delta = Vector3Subtract(target, eye);
    const float horizontal = std::sqrt(delta.x * delta.x + delta.z * delta.z);
    if (horizontal <= MovementEpsilon && std::fabs(delta.y) <= MovementEpsilon) {
        const engine::ScriptOperationHandle operation = look.operation;
        look.active = false;
        engine::ScriptSystemFailOperation(scripts, operation,
                "look target coincides with the camera");
        return;
    }
    look.elapsedSeconds += SafeDelta(dt);
    const float progress = look.durationSeconds <= 0.0
            ? 1.0f
            : static_cast<float>(look.elapsedSeconds / look.durationSeconds);
    const float eased = Smootherstep(progress);
    const float targetYaw = std::atan2(delta.z, delta.x);
    const float targetPitch = ClampSectorFpsPitch(
            std::atan2(delta.y, std::max(horizontal, MovementEpsilon)));
    player.yawRadians = look.startYawRadians
            + ShortestAngleDelta(look.startYawRadians, targetYaw) * eased;
    player.pitchRadians = ClampSectorFpsPitch(
            look.startPitchRadians
            + (targetPitch - look.startPitchRadians) * eased);
    if (progress >= 1.0f) {
        const engine::ScriptOperationHandle operation = look.operation;
        look.active = false;
        engine::ScriptSystemCompleteOperation(scripts, operation);
    }
}

bool BeginSectorCutsceneCaption(
        SectorCutsceneRuntime& runtime,
        SectorCutsceneCaptionKind kind,
        SectorCutsceneTextPosition position,
        std::string_view text,
        const double* holdSeconds,
        uint64_t& outToken,
        std::string& error)
{
    if (text.empty()) {
        error = "caption text must not be empty";
        return false;
    }
    if (holdSeconds != nullptr
            && (!std::isfinite(*holdSeconds) || *holdSeconds < 0.0)) {
        error = "caption hold duration is invalid";
        return false;
    }
    if (text.size() > kSectorCutsceneMaximumCaptionBytes) {
        error = "caption text exceeds 8192 UTF-8 bytes";
        return false;
    }
    bool validUtf8 = false;
    const size_t codepoints = CountCodepoints(text, validUtf8);
    if (!validUtf8 || codepoints > kSectorCutsceneMaximumCaptionCodepoints) {
        error = !validUtf8 ? "caption text must be valid UTF-8"
                : "caption text exceeds 2048 codepoints";
        return false;
    }
    SectorCutsceneCaptionState& caption = runtime.caption;
    caption.token = runtime.nextToken++;
    caption.operation = {};
    caption.kind = kind;
    caption.position = position;
    caption.text.assign(text.data(), text.size());
    caption.codepointCount = codepoints;
    caption.visibleByteCount = kind == SectorCutsceneCaptionKind::Text
            ? text.size() : 0;
    caption.revealSeconds = kind == SectorCutsceneCaptionKind::Say
            ? static_cast<double>(codepoints) / SayCodepointsPerSecond : 0.0;
    caption.fadeInSeconds = kind == SectorCutsceneCaptionKind::Text
            ? TextFadeInSeconds : 0.0;
    caption.holdSeconds = holdSeconds != nullptr
            ? *holdSeconds
            : std::clamp(
                    static_cast<double>(codepoints)
                            * AutomaticHoldSecondsPerCodepoint,
                    MinimumAutomaticHoldSeconds,
                    MaximumAutomaticHoldSeconds);
    caption.fadeOutSeconds = CaptionFadeOutSeconds;
    caption.elapsedSeconds = 0.0;
    caption.opacity = kind == SectorCutsceneCaptionKind::Say ? 1.0f : 0.0f;
    caption.active = true;
    outToken = caption.token;
    error.clear();
    return true;
}

void BindSectorCutsceneCaptionOperation(
        SectorCutsceneRuntime& runtime,
        uint64_t token,
        engine::ScriptOperationHandle operation)
{
    if (runtime.caption.active && runtime.caption.token == token) {
        runtime.caption.operation = operation;
    }
}

void CancelSectorCutsceneCaption(SectorCutsceneRuntime& runtime, uint64_t token)
{
    if (runtime.caption.active && runtime.caption.token == token) {
        runtime.caption.active = false;
    }
}

bool BeginSectorCutsceneFade(
        SectorCutsceneRuntime& runtime,
        float targetOpacity,
        double durationSeconds,
        uint64_t& outToken,
        std::string& error)
{
    if (!std::isfinite(targetOpacity) || !std::isfinite(durationSeconds)
            || durationSeconds < 0.0) {
        error = "fade arguments are invalid";
        return false;
    }
    SectorCutsceneFadeState& fade = runtime.fade;
    fade.token = runtime.nextToken++;
    fade.operation = {};
    fade.startOpacity = fade.opacity;
    fade.targetOpacity = std::clamp(targetOpacity, 0.0f, 1.0f);
    fade.durationSeconds = durationSeconds;
    fade.elapsedSeconds = 0.0;
    fade.active = true;
    outToken = fade.token;
    error.clear();
    return true;
}

void BindSectorCutsceneFadeOperation(
        SectorCutsceneRuntime& runtime,
        uint64_t token,
        engine::ScriptOperationHandle operation)
{
    if (runtime.fade.active && runtime.fade.token == token) {
        runtime.fade.operation = operation;
    }
}

void CancelSectorCutsceneFade(SectorCutsceneRuntime& runtime, uint64_t token)
{
    if (runtime.fade.active && runtime.fade.token == token) {
        runtime.fade.active = false;
    }
}

void UpdateSectorCutsceneTimelines(
        SectorCutsceneRuntime& runtime,
        engine::ScriptRuntime& scripts,
        float dt)
{
    const double delta = SafeDelta(dt);
    SectorCutscenePresentationState& presentation = runtime.presentation;
    presentation.progress = std::clamp(presentation.progress
            + (presentation.active ? delta : -delta) / 0.35, 0.0, 1.0);
    SectorCutsceneCaptionState& caption = runtime.caption;
    if (caption.active) {
        caption.elapsedSeconds += delta;
        const double revealStart = caption.fadeInSeconds;
        const double revealEnd = revealStart + caption.revealSeconds;
        const double holdEnd = revealEnd + caption.holdSeconds;
        const double end = holdEnd + caption.fadeOutSeconds;
        if (caption.fadeInSeconds > 0.0
                && caption.elapsedSeconds < caption.fadeInSeconds) {
            caption.opacity = Smoothstep(static_cast<float>(
                    caption.elapsedSeconds / caption.fadeInSeconds));
        } else if (caption.elapsedSeconds < holdEnd) {
            caption.opacity = 1.0f;
        } else {
            caption.opacity = 1.0f - Smoothstep(static_cast<float>(
                    (caption.elapsedSeconds - holdEnd)
                    / caption.fadeOutSeconds));
        }
        if (caption.kind == SectorCutsceneCaptionKind::Say) {
            const double revealElapsed = std::clamp(
                    caption.elapsedSeconds - revealStart,
                    0.0,
                    caption.revealSeconds);
            const size_t visibleCodepoints = caption.revealSeconds <= 0.0
                    ? caption.codepointCount
                    : std::min(caption.codepointCount,
                            static_cast<size_t>(std::floor(
                                    revealElapsed * SayCodepointsPerSecond)));
            caption.visibleByteCount = BytePrefixForCodepoints(
                    caption.text, visibleCodepoints);
        }
        if (caption.elapsedSeconds >= end) {
            const engine::ScriptOperationHandle operation = caption.operation;
            caption.opacity = 0.0f;
            caption.active = false;
            engine::ScriptSystemCompleteOperation(scripts, operation);
        }
    }

    SectorCutsceneFadeState& fade = runtime.fade;
    if (fade.active) {
        fade.elapsedSeconds += delta;
        const float progress = fade.durationSeconds <= 0.0
                ? 1.0f
                : static_cast<float>(fade.elapsedSeconds / fade.durationSeconds);
        const float eased = Smoothstep(progress);
        fade.opacity = fade.startOpacity
                + (fade.targetOpacity - fade.startOpacity) * eased;
        if (progress >= 1.0f) {
            const engine::ScriptOperationHandle operation = fade.operation;
            fade.opacity = fade.targetOpacity;
            fade.active = false;
            engine::ScriptSystemCompleteOperation(scripts, operation);
        }
    }
}

SectorCutscenePresentationLayout BuildSectorCutscenePresentationLayout(
        const SectorCutscenePresentationState& presentation,
        Rectangle viewport,
        SectorCutsceneTextPosition captionPosition,
        float captionBlockHeight)
{
    SectorCutscenePresentationLayout layout;
    if (viewport.width <= 0.0f || viewport.height <= 0.0f) return layout;
    const float progress = static_cast<float>(std::clamp(
            presentation.progress, 0.0, 1.0));
    const float blend = progress * progress * (3.0f - 2.0f * progress);
    const float barHeight = viewport.height * 0.15f * blend;
    layout.topBar = {viewport.x, viewport.y, viewport.width, barHeight};
    layout.bottomBar = {viewport.x, viewport.y + viewport.height - barHeight,
            viewport.width, barHeight};
    layout.captionY = viewport.y + viewport.height * 0.12f;
    if (captionPosition == SectorCutsceneTextPosition::Center) {
        layout.captionY = viewport.y + (viewport.height - captionBlockHeight) * 0.5f;
    } else if (captionPosition == SectorCutsceneTextPosition::Bottom) {
        const float normalY = viewport.y + viewport.height * 0.88f - captionBlockHeight;
        const float padding = viewport.height * 0.015f;
        const float cinematicY = std::max(viewport.y, std::min(
                viewport.y + viewport.height * 0.85f + padding,
                viewport.y + viewport.height - padding - captionBlockHeight));
        layout.captionY = normalY + (cinematicY - normalY) * blend;
    }
    return layout;
}

void DrawSectorCutsceneLetterbox(
        const SectorCutsceneRuntime& runtime,
        Rectangle viewport)
{
    const SectorCutscenePresentationLayout layout = BuildSectorCutscenePresentationLayout(
            runtime.presentation, viewport, SectorCutsceneTextPosition::Bottom, 0.0f);
    if (layout.topBar.height <= 0.0f) return;
    DrawRectangleRec(layout.topBar, BLACK);
    DrawRectangleRec(layout.bottomBar, BLACK);
}

void DrawSectorCutsceneCaption(
        const SectorCutsceneRuntime& runtime,
        engine::AssetManager& assets,
        engine::FontHandle fontHandle,
        Rectangle viewport)
{
    const SectorCutsceneCaptionState& caption = runtime.caption;
    const engine::FontAsset* asset = assets.GetFont(fontHandle);
    if (!caption.active || caption.opacity <= 0.0f || asset == nullptr
            || viewport.width <= 0.0f || viewport.height <= 0.0f) return;
    const Font& font = asset->font;
    const float fontSize = static_cast<float>(asset->pixelSize);
    const float lineAdvance = fontSize + 8.0f;
    const float maximumWidth = viewport.width * 0.80f;
    std::array<WrappedLine, MaximumWrappedLines> lines{};
    const size_t lineCount = BuildWrappedLines(
            caption.text, font, maximumWidth, lines);
    if (lineCount == 0) return;
    const float blockHeight = static_cast<float>(lineCount) * lineAdvance - 8.0f;
    const float y = BuildSectorCutscenePresentationLayout(
            runtime.presentation, viewport, caption.position, blockHeight).captionY;
    const unsigned char alpha = static_cast<unsigned char>(std::lround(
            std::clamp(caption.opacity, 0.0f, 1.0f) * 255.0f));
    std::array<char, kSectorCutsceneMaximumCaptionBytes + 1> lineBuffer{};
    for (size_t index = 0; index < lineCount; ++index) {
        size_t start = lines[index].start;
        size_t end = std::min(lines[index].end, caption.visibleByteCount);
        while (start < end && (caption.text[start] == ' '
                || caption.text[start] == '\t')) ++start;
        while (end > start && (caption.text[end - 1] == ' '
                || caption.text[end - 1] == '\t')) --end;
        if (end > start) {
            const size_t length = std::min(end - start, lineBuffer.size() - 1);
            std::memcpy(lineBuffer.data(), caption.text.data() + start, length);
            lineBuffer[length] = '\0';
            const Vector2 measured = MeasureTextEx(
                    font, lineBuffer.data(), fontSize, 1.0f);
            const Vector2 position{
                    viewport.x + (viewport.width - measured.x) * 0.5f,
                    y + static_cast<float>(index) * lineAdvance};
            DrawTextEx(font, lineBuffer.data(),
                    Vector2{position.x + 2.0f, position.y + 2.0f},
                    fontSize, 1.0f, Color{0, 0, 0, alpha});
            DrawTextEx(font, lineBuffer.data(), position,
                    fontSize, 1.0f, Color{245, 245, 240, alpha});
        }
    }
}

} // namespace game
