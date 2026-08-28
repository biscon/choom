#include "game/npc/ai/NpcAiSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/Health.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcNavigationSystem.h"
#include "game/npc/ai/NpcAiTypes.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorAudioOcclusion.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelCollision.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace game {
namespace {

constexpr float SoundEventLifetimeSeconds = 1.5f;
constexpr float ChaseRetargetSeconds = 0.25f;
constexpr float ChaseRetargetDistanceWorld = 0.35f;
constexpr float SearchTurnRadiansPerSecond = 1.4f;
constexpr float LineOfSightEpsilon = 0.03f;
constexpr float AttackSoundMinimumDistanceWorld = 1.0f;
constexpr float AttackSoundMaximumDistanceWorld = 25.0f;
constexpr float CommittedMeleeHitGraceWorld = 0.25f;
constexpr float PursuitSlotPaddingWorld = 0.10f;
constexpr float PursuitSlotAttackInsetWorld = 0.05f;
constexpr float PursuitSupportHeightToleranceWorld = 0.10f;
constexpr float PursuitOrbitRadiansPerSecond = 0.45f;
constexpr float PursuitMeleeToleranceWorld = 0.10f;
constexpr int MinimumPursuitSlotsPerRing = 6;

NpcNavigationRecord* FindNavigationRecord(
        NpcNavigationRuntime& runtime,
        engine::Entity entity)
{
    for (NpcNavigationRecord& record : runtime.records) {
        if (record.occupied && record.entity == entity) return &record;
    }
    return nullptr;
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
    return enter >= 0.0f && enter < maximumDistance - LineOfSightEpsilon;
}

bool HasLineOfSight(
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 origin,
        Vector3 target)
{
    const Vector3 delta = Vector3Subtract(target, origin);
    const float distance = Vector3Length(delta);
    if (distance <= LineOfSightEpsilon) return true;
    const Vector3 direction = Vector3Scale(delta, 1.0f / distance);
    const SectorCollisionRayHit sectorHit = collisionWorld.Raycast(
            origin, direction, distance);
    if (sectorHit.hit && sectorHit.distance < distance - LineOfSightEpsilon) {
        return false;
    }
    for (const SectorDynamicDoorCollider& door : doorColliders) {
        if (SegmentIntersectsPrism(
                    origin, direction, distance,
                    door.center, door.tangent, door.normal,
                    door.halfExtents, door.bottom, door.top)) return false;
    }
    for (const SectorStaticModelCollider& model : staticColliders) {
        if (!model.resolved || model.failed) continue;
        if (SegmentIntersectsPrism(
                    origin, direction, distance,
                    model.center, model.axisX, model.axisZ,
                    model.halfExtents, model.bottom, model.top)) return false;
    }
    return true;
}

struct NpcPlayerSightEvaluation {
    bool visible = false;
    float distanceWorld = 0.0f;
    NpcVisualDetectionReason failureReason =
            NpcVisualDetectionReason::NoPlayer;
};

NpcPlayerSightEvaluation EvaluatePlayerSight(
        const NpcAiState& ai,
        const SectorObjectTransform& transform,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 playerFeet,
        Vector3 playerEye)
{
    NpcPlayerSightEvaluation result;
    const Vector2 toPlayer{
            playerFeet.x - transform.position.x,
            playerFeet.z - transform.position.z};
    const float distance = Vector2Length(toPlayer);
    result.distanceWorld = distance;
    if (distance > ai.perception.visionRangeWorld) {
        result.failureReason = NpcVisualDetectionReason::OutsideRange;
        return result;
    }
    if (distance > 0.0001f) {
        const Vector2 forward{
                std::sin(transform.yawRadians),
                std::cos(transform.yawRadians)};
        const float cosine = Vector2DotProduct(
                forward, Vector2Scale(toPlayer, 1.0f / distance));
        const float minimumCosine = std::cos(
                ai.perception.visionAngleDegrees * 0.5f * DEG2RAD);
        if (cosine < minimumCosine) {
            result.failureReason = NpcVisualDetectionReason::OutsideCone;
            return result;
        }
    }
    const Vector3 npcEye{
            transform.position.x,
            transform.position.y + 1.4f,
            transform.position.z};
    result.visible = HasLineOfSight(
            collisionWorld, doorColliders, staticColliders,
            npcEye, playerEye);
    result.failureReason = result.visible
            ? NpcVisualDetectionReason::Building
            : NpcVisualDetectionReason::Occluded;
    return result;
}

void CancelAiMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& runtime,
        const NpcRuntimeInstance& npc)
{
    const NpcMoveStatus status = GetNpcMoveStatus(runtime, npc.instanceId);
    if (status.found && status.phase == NpcMovePhase::FollowingPath
            && status.authority == NpcMoveAuthority::Ai) {
        CancelNpcMove(world, navigation, runtime, npc.instanceId, status.requestId);
    }
}

void RequestAiMove(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        NpcNavigationRuntime& runtime,
        NpcRuntimeInstance& npc,
        Vector2 destination,
        NpcMoveGait gait)
{
    const NpcMoveStatus status = GetNpcMoveStatus(runtime, npc.instanceId);
    if (status.found && status.phase == NpcMovePhase::FollowingPath) {
        if (status.authority != NpcMoveAuthority::Ai
                && status.authority != NpcMoveAuthority::Patrol) return;
        if (status.authority == NpcMoveAuthority::Ai) {
            const Vector2 delta = Vector2Subtract(
                    status.requestedDestinationXZ, destination);
            if (Vector2Length(delta) <= ChaseRetargetDistanceWorld) return;
        }
    }
    RetargetNpcAiMove(
            world, navigation, collisionWorld, runtime,
            npc.instanceId, destination, gait);
}

float Dot2(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

Vector2 ToColliderLocal(Vector2 point, const SectorStaticModelCollider& collider)
{
    const Vector2 relative{
            point.x - collider.center.x,
            point.y - collider.center.y};
    return {Dot2(relative, collider.axisX), Dot2(relative, collider.axisZ)};
}

bool PlayerSupportedByCollider(
        const NpcAiGameplayContext& gameplay,
        const SectorStaticModelCollider& collider)
{
    if (!gameplay.playerGrounded || !collider.resolved || collider.failed
            || std::fabs(gameplay.playerFeetPosition.y - collider.top)
                    > PursuitSupportHeightToleranceWorld) {
        return false;
    }
    const Vector2 local = ToColliderLocal(
            {gameplay.playerFeetPosition.x, gameplay.playerFeetPosition.z},
            collider);
    const Vector2 closest{
            std::clamp(local.x, -collider.halfExtents.x, collider.halfExtents.x),
            std::clamp(local.y, -collider.halfExtents.y, collider.halfExtents.y)};
    const Vector2 delta{local.x - closest.x, local.y - closest.y};
    return Dot2(delta, delta)
            <= gameplay.playerRadiusWorld * gameplay.playerRadiusWorld;
}

float RayExitDistanceFromExpandedCollider(
        Vector2 origin,
        Vector2 direction,
        const SectorStaticModelCollider& collider,
        float expansion)
{
    const Vector2 localOrigin = ToColliderLocal(origin, collider);
    const Vector2 localDirection{
            Dot2(direction, collider.axisX),
            Dot2(direction, collider.axisZ)};
    const Vector2 bounds{
            collider.halfExtents.x + expansion,
            collider.halfExtents.y + expansion};
    if (std::fabs(localOrigin.x) > bounds.x
            || std::fabs(localOrigin.y) > bounds.y) return 0.0f;
    float exit = std::numeric_limits<float>::infinity();
    if (std::fabs(localDirection.x) > 0.000001f) {
        const float face = localDirection.x > 0.0f ? bounds.x : -bounds.x;
        exit = std::min(exit, (face - localOrigin.x) / localDirection.x);
    }
    if (std::fabs(localDirection.y) > 0.000001f) {
        const float face = localDirection.y > 0.0f ? bounds.y : -bounds.y;
        exit = std::min(exit, (face - localOrigin.y) / localDirection.y);
    }
    return std::isfinite(exit) ? std::max(0.0f, exit) : 0.0f;
}

NpcPursuitSlot* FindPursuitSlot(
        NpcAiRuntime& runtime,
        engine::Entity owner)
{
    const auto found = std::find_if(
            runtime.pursuitSlots.begin(),
            runtime.pursuitSlots.end(),
            [owner](const NpcPursuitSlot& slot) {
                return slot.claimed && slot.owner == owner;
            });
    return found == runtime.pursuitSlots.end() ? nullptr : &*found;
}

void ResetPursuitAssignment(NpcAiState& ai)
{
    ai.pursuitSlotIndex = -1;
    ai.pursuitSlotRing = -1;
    ai.pursuitSlotKind = NpcPursuitSlotKind::None;
    ai.pursuitRetargetFailed = false;
}

void BuildPlayerPursuitSlots(
        engine::World& world,
        SectorNavigationWorld& navigation,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        NpcNavigationRuntime& npcNavigation,
        NpcAiRuntime& runtime,
        const NpcAiGameplayContext& gameplay,
        float dt)
{
    runtime.pursuitSlots.clear();
    runtime.pursuitParticipants.clear();
    runtime.pursuitOrbitPhaseRadians = std::fmod(
            runtime.pursuitOrbitPhaseRadians
                    + std::max(0.0f, dt) * PursuitOrbitRadiansPerSecond,
            2.0f * PI);

    for (const NpcNavigationRecord& record : npcNavigation.records) {
        if (!record.occupied || !world.IsAlive(record.entity)
                || !world.Has<NpcRuntimeInstance>(record.entity)
                || !world.Has<NpcAiState>(record.entity)
                || !world.Has<Health>(record.entity)
                || !world.Has<NpcCombatState>(record.entity)) {
            continue;
        }
        NpcRuntimeInstance& npc = world.Get<NpcRuntimeInstance>(record.entity);
        NpcAiState& ai = world.Get<NpcAiState>(record.entity);
        const NpcCombatState& combat = world.Get<NpcCombatState>(record.entity);
        const Health& health = world.Get<Health>(record.entity);
        const bool eligible = npc.hostile
                && !gameplay.playerInvisible
                && ai.aiType == kSeekAndDestroyNpcAiType
                && ai.awareness == NpcAwarenessState::Detected
                && !combat.dead && !IsDepleted(health);
        if (!eligible) {
            ResetPursuitAssignment(ai);
            continue;
        }
        if (runtime.pursuitParticipants.size()
                == runtime.pursuitParticipants.capacity()
                && !runtime.pursuitCapacityWarningPrinted) {
            runtime.pursuitCapacityWarningPrinted = true;
            std::fprintf(stderr,
                    "[NPC AI WARNING] pursuit-slot capacity exceeded; runtime allocation may occur.\n");
        }
        runtime.pursuitParticipants.push_back({
                record.entity,
                record.placedObjectId,
                ai.attack.rangeWorld});
    }
    std::sort(
            runtime.pursuitParticipants.begin(),
            runtime.pursuitParticipants.end(),
            [](const NpcPursuitParticipant& a,
                    const NpcPursuitParticipant& b) {
                return a.placedObjectId < b.placedObjectId;
            });

    const SectorNavigationSettings& nav = navigation.Settings();
    const float spacing = nav.agentRadius * 2.0f + PursuitSlotPaddingWorld;
    const Vector2 playerXZ{
            gameplay.playerFeetPosition.x,
            gameplay.playerFeetPosition.z};
    for (size_t participantIndex = 0;
            participantIndex < runtime.pursuitParticipants.size();
            ++participantIndex) {
        const NpcPursuitParticipant& participant =
                runtime.pursuitParticipants[participantIndex];
        NpcAiState& ai = world.Get<NpcAiState>(participant.entity);
        float radius = std::max(
                gameplay.playerRadiusWorld + nav.agentRadius
                        + PursuitSlotAttackInsetWorld,
                participant.attackRangeWorld - PursuitSlotAttackInsetWorld);
        size_t ordinal = participantIndex;
        int ring = 0;
        int capacity = MinimumPursuitSlotsPerRing;
        for (;;) {
            capacity = std::max(
                    MinimumPursuitSlotsPerRing,
                    static_cast<int>(std::floor(2.0f * PI * radius / spacing)));
            if (ordinal < static_cast<size_t>(capacity)) break;
            ordinal -= static_cast<size_t>(capacity);
            radius += spacing;
            ++ring;
        }
        float radians = 2.0f * PI * static_cast<float>(ordinal)
                / static_cast<float>(capacity);
        Vector2 direction{std::sin(radians), std::cos(radians)};
        float resolvedRadius = radius;
        for (const SectorStaticModelCollider& collider : staticColliders) {
            if (!PlayerSupportedByCollider(gameplay, collider)) continue;
            resolvedRadius = std::max(
                    resolvedRadius,
                    RayExitDistanceFromExpandedCollider(
                            playerXZ,
                            direction,
                            collider,
                            nav.agentRadius + nav.cellSize)
                            + nav.cellSize);
        }

        bool meleeCandidate = resolvedRadius
                <= participant.attackRangeWorld + PursuitMeleeToleranceWorld;
        if (!meleeCandidate) {
            const float orbitDirection = (ring & 1) == 0 ? 1.0f : -1.0f;
            radians += runtime.pursuitOrbitPhaseRadians * orbitDirection;
            direction = {std::sin(radians), std::cos(radians)};
            resolvedRadius = radius;
            for (const SectorStaticModelCollider& collider : staticColliders) {
                if (!PlayerSupportedByCollider(gameplay, collider)) continue;
                resolvedRadius = std::max(
                        resolvedRadius,
                        RayExitDistanceFromExpandedCollider(
                                playerXZ,
                                direction,
                                collider,
                                nav.agentRadius + nav.cellSize)
                                + nav.cellSize);
            }
        }

        NpcPursuitSlot slot;
        slot.owner = participant.entity;
        slot.index = static_cast<int>(participantIndex);
        slot.ring = ring;
        slot.claimed = true;
        slot.requestedPosition = {
                playerXZ.x + direction.x * resolvedRadius,
                gameplay.playerFeetPosition.y,
                playerXZ.y + direction.y * resolvedRadius};
        const int sectorId = collisionWorld.FindSectorContainingPoint(
                {slot.requestedPosition.x, slot.requestedPosition.z});
        SectorCollisionHeights heights;
        if (sectorId != 0
                && collisionWorld.GetSectorFloorCeiling(sectorId, &heights)) {
            const SectorNavigationNearestPointResult nearest =
                    navigation.FindNearestPoint({
                            slot.requestedPosition.x,
                            heights.floorZ,
                            slot.requestedPosition.z});
            if (nearest.status == SectorNavigationQueryStatus::Success) {
                slot.projected = true;
                slot.resolvedPosition = nearest.nearestPosition;
            }
        }
        if (!slot.projected) {
            slot.kind = NpcPursuitSlotKind::Invalid;
        } else {
            const Vector2 delta{
                    gameplay.playerFeetPosition.x - slot.resolvedPosition.x,
                    gameplay.playerFeetPosition.z - slot.resolvedPosition.z};
            const bool inRange = Vector2Length(delta)
                    <= participant.attackRangeWorld
                            + PursuitMeleeToleranceWorld;
            const bool lineOfSight = HasLineOfSight(
                    collisionWorld,
                    doorColliders,
                    staticColliders,
                    {slot.resolvedPosition.x,
                            slot.resolvedPosition.y + 1.4f,
                            slot.resolvedPosition.z},
                    gameplay.playerEyePosition);
            slot.kind = inRange && lineOfSight
                    ? NpcPursuitSlotKind::Melee
                    : NpcPursuitSlotKind::Orbit;
        }
        ai.pursuitSlotIndex = slot.index;
        ai.pursuitSlotRing = slot.ring;
        ai.pursuitSlotKind = slot.kind;
        runtime.pursuitSlots.push_back(slot);
    }
}

float AttackAnimationPhase(
        engine::World& world,
        engine::AssetManager& assets,
        engine::Entity entity)
{
    if (!world.Has<NpcAnimationState>(entity)
            || !world.Has<engine::AnimatedModelInstance>(entity)
            || !world.Has<engine::AnimatedModelAnimator>(entity)) return 0.0f;
    const NpcAnimationState& animation = world.Get<NpcAnimationState>(entity);
    const engine::AnimatedModelInstance& instance =
            world.Get<engine::AnimatedModelInstance>(entity);
    const engine::ModelAsset* asset = assets.GetModelAsset(instance.model);
    if (asset == nullptr) return 0.0f;
    const uint32_t attackIndex = animation.animationIndices[
            static_cast<size_t>(NpcAction::Attack)];
    if (attackIndex == engine::InvalidModelAnimationIndex) return 0.0f;
    const int frameCount = engine::ModelAnimationClipKeyframeCount(
            *asset, attackIndex);
    if (frameCount <= 1) return 1.0f;
    const engine::AnimatedModelAnimator& animator =
            world.Get<engine::AnimatedModelAnimator>(entity);
    const float frame = animator.targetAnimationIndex == attackIndex
            ? animator.targetFrame : animator.frame;
    return std::clamp(frame / static_cast<float>(frameCount - 1), 0.0f, 1.0f);
}

bool AttackAnimationFinished(
        engine::World& world,
        engine::Entity entity)
{
    if (!world.Has<NpcAnimationState>(entity)
            || !world.Has<engine::AnimatedModelAnimator>(entity)) {
        return true;
    }
    const NpcAnimationState& animation = world.Get<NpcAnimationState>(entity);
    const uint32_t attackIndex = animation.animationIndices[
            static_cast<size_t>(NpcAction::Attack)];
    if (attackIndex == engine::InvalidModelAnimationIndex) return true;
    return engine::IsAnimatedModelAnimationFinished(
            world.Get<engine::AnimatedModelAnimator>(entity),
            attackIndex);
}

bool StartAttack(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& npcNavigation,
        engine::Entity entity,
        NpcRuntimeInstance& npc,
        NpcAiState& ai,
        SectorObjectTransform& transform,
        Vector3 playerFeet)
{
    if (!world.Has<NpcAnimationState>(entity)
            || !world.Has<engine::AnimatedModelAnimator>(entity)) {
        return false;
    }
    NpcAnimationState& state = world.Get<NpcAnimationState>(entity);
    engine::AnimatedModelAnimator& animator =
            world.Get<engine::AnimatedModelAnimator>(entity);
    const size_t index = static_cast<size_t>(NpcAction::Attack);
    if (!state.resolved
            || state.animationIndices[index]
                    == engine::InvalidModelAnimationIndex) {
        return false;
    }
    CancelAiMove(world, navigation, npcNavigation, npc);
    const Vector2 direction{
            playerFeet.x - transform.position.x,
            playerFeet.z - transform.position.z};
    if (Vector2Length(direction) > 0.0001f) {
        transform.yawRadians = std::atan2(direction.x, direction.y);
    }
    npc.action = NpcAction::Attack;
    npc.actionLockedByAi = true;
    ai.attackCommitted = true;
    ai.attackHitResolved = false;
    ai.attackPhase = 0.0f;
    engine::SetAnimatedModelAnimation(
            animator, state.animationIndices[index],
            state.blendSeconds, true);
    animator.speed = state.animationSpeeds[index];
    engine::SetAnimatedModelAnimationLoop(
            animator, state.animationIndices[index], false);
    state.appliedAction = NpcAction::Attack;
    state.hasPendingAction = false;
    if (!engine::IsNull(ai.attackSound)) {
        engine::PositionalSoundSettings positional;
        positional.position = transform.position;
        positional.minimumDistanceWorld = AttackSoundMinimumDistanceWorld;
        positional.maximumDistanceWorld = AttackSoundMaximumDistanceWorld;
        audio.PlaySoundAt(assets, ai.attackSound, positional);
    }
    return true;
}

void FinishAttack(NpcRuntimeInstance& npc, NpcAiState& ai)
{
    ai.attackCommitted = false;
    ai.attackHitResolved = false;
    ai.attackPhase = 0.0f;
    npc.actionLockedByAi = false;
    npc.action = NpcAction::Idle;
}

} // namespace

void InitializeNpcAiRuntime(
        NpcAiRuntime& runtime,
        size_t soundCapacity,
        size_t pursuitCapacity)
{
    runtime.playerSounds.clear();
    runtime.playerSounds.reserve(soundCapacity);
    runtime.pursuitSlots.clear();
    runtime.pursuitSlots.reserve(pursuitCapacity);
    runtime.pursuitParticipants.clear();
    runtime.pursuitParticipants.reserve(pursuitCapacity);
    runtime.nextSoundSequence = 1;
    runtime.pursuitOrbitPhaseRadians = 0.0f;
    runtime.capacityWarningPrinted = false;
    runtime.pursuitCapacityWarningPrinted = false;
    runtime.playerLightLevel = {};
    runtime.playerLightDetectionFactor = 1.0f;
    runtime.playerCrouchBlend = 0.0f;
    runtime.playerMovementNoiseMultiplier = 1.0f;
    runtime.playerSneaking = false;
}

void ClearNpcAiRuntime(NpcAiRuntime& runtime)
{
    runtime.playerSounds.clear();
    runtime.pursuitSlots.clear();
    runtime.pursuitParticipants.clear();
    runtime.nextSoundSequence = 1;
    runtime.pursuitOrbitPhaseRadians = 0.0f;
    runtime.capacityWarningPrinted = false;
    runtime.pursuitCapacityWarningPrinted = false;
    runtime.playerLightLevel = {};
    runtime.playerLightDetectionFactor = 1.0f;
    runtime.playerCrouchBlend = 0.0f;
    runtime.playerMovementNoiseMultiplier = 1.0f;
    runtime.playerSneaking = false;
}

void EmitNpcPlayerSound(
        NpcAiRuntime& runtime,
        Vector3 positionWorld,
        float radiusWorld,
        float lifetimeSeconds)
{
    if (!std::isfinite(radiusWorld) || radiusWorld <= 0.0f) return;
    if (runtime.playerSounds.size() == runtime.playerSounds.capacity()
            && !runtime.capacityWarningPrinted) {
        std::fprintf(stderr,
                "[NPC AI WARNING] sound-event capacity exceeded; runtime allocation may occur.\n");
        runtime.capacityWarningPrinted = true;
    }
    runtime.playerSounds.push_back(NpcSoundEvent{
            runtime.nextSoundSequence++, positionWorld, radiusWorld,
            std::max(0.01f, std::isfinite(lifetimeSeconds)
                    ? lifetimeSeconds : SoundEventLifetimeSeconds)});
}

void AlertNpcToPlayerPosition(
        engine::World& world,
        engine::Entity entity,
        Vector3 playerPositionWorld)
{
    if (!world.IsAlive(entity) || !world.Has<NpcAiState>(entity)) return;
    NpcAiState& ai = world.Get<NpcAiState>(entity);
    const bool newlyDetected = ai.awareness != NpcAwarenessState::Detected;
    ai.awareness = NpcAwarenessState::Detected;
    ai.visualDetectionProgress = 1.0f;
    ai.visualDetectionReason = NpcVisualDetectionReason::Detected;
    ai.lastKnownPlayerPosition = playerPositionWorld;
    ai.scriptTakeoverPending = true;
    ai.directAlertPending = true;
    if (newlyDetected
            && world.Has<NpcRuntimeInstance>(entity)
            && world.Get<NpcRuntimeInstance>(entity).hostile) {
        ai.playerDetectionAudioPending = true;
    }
}

int ApplyNpcAiPlayerDamage(
        const NpcAiGameplayContext& gameplay,
        int damage)
{
    if (gameplay.godMode || gameplay.playerHealth == nullptr) return 0;
    const int appliedDamage = ApplyDamage(*gameplay.playerHealth, damage);
    if (appliedDamage > 0 && gameplay.playerDamaged != nullptr) {
        gameplay.playerDamaged(
                gameplay.playerDamageUserData,
                appliedDamage);
    }
    return appliedDamage;
}

int ApplyNpcAiPlayerAttackEffects(
        const NpcAiGameplayContext& gameplay,
        const NpcActionDefinition& attack,
        Vector2 directionFromAttackerToPlayer)
{
    const int appliedDamage = ApplyNpcAiPlayerDamage(
            gameplay, attack.damage);
    if (appliedDamage <= 0) return 0;
    if (gameplay.playerAttackHit != nullptr) {
        gameplay.playerAttackHit(
                gameplay.playerAttackHitUserData,
                appliedDamage,
                attack.cameraImpact,
                directionFromAttackerToPlayer);
    }
    if (gameplay.playerStunRemainingSeconds != nullptr) {
        *gameplay.playerStunRemainingSeconds = std::max(
                *gameplay.playerStunRemainingSeconds,
                attack.stunMilliseconds / 1000.0f);
    }
    const float directionLength = Vector2Length(
            directionFromAttackerToPlayer);
    if (gameplay.playerKnockbackVelocity != nullptr
            && attack.knockbackImpulseWorldPerSecond > 0.0f
            && directionLength > 0.0001f) {
        *gameplay.playerKnockbackVelocity = Vector2Add(
                *gameplay.playerKnockbackVelocity,
                Vector2Scale(
                        directionFromAttackerToPlayer,
                        attack.knockbackImpulseWorldPerSecond
                                / directionLength));
    }
    return appliedDamage;
}

bool IsNpcAiCommittedMeleeHitInRange(
        float playerDistanceWorld,
        float attackRangeWorld)
{
    return std::isfinite(playerDistanceWorld)
            && std::isfinite(attackRangeWorld)
            && playerDistanceWorld >= 0.0f
            && attackRangeWorld >= 0.0f
            && playerDistanceWorld
                    <= attackRangeWorld + CommittedMeleeHitGraceWorld;
}

bool IsNpcAiCommittedMeleeHitWithinArc(
        Vector2 directionFromAttackerToPlayer,
        float attackerYawRadians,
        float hitArcDegrees)
{
    if (!std::isfinite(directionFromAttackerToPlayer.x)
            || !std::isfinite(directionFromAttackerToPlayer.y)
            || !std::isfinite(attackerYawRadians)
            || !std::isfinite(hitArcDegrees)
            || hitArcDegrees <= 0.0f || hitArcDegrees > 360.0f) {
        return false;
    }
    if (hitArcDegrees >= 360.0f) return true;
    const float distance = Vector2Length(directionFromAttackerToPlayer);
    if (distance <= 0.0001f) return true;
    const Vector2 direction = Vector2Scale(
            directionFromAttackerToPlayer, 1.0f / distance);
    const Vector2 forward{
            std::sin(attackerYawRadians),
            std::cos(attackerYawRadians)};
    const float halfArcRadians = hitArcDegrees * DEG2RAD * 0.5f;
    return Vector2DotProduct(forward, direction)
            >= std::cos(halfArcRadians);
}

float NpcAiAttackAdvanceSpeedFactor(
        float attackPhase,
        float hitPhase,
        float advanceSpeedMultiplier)
{
    if (!std::isfinite(attackPhase) || !std::isfinite(hitPhase)
            || !std::isfinite(advanceSpeedMultiplier)
            || hitPhase <= 0.0f || advanceSpeedMultiplier <= 0.0f
            || attackPhase >= hitPhase) {
        return 0.0f;
    }
    const float progress = std::clamp(attackPhase / hitPhase, 0.0f, 1.0f);
    const float smoothProgress = progress * progress
            * (3.0f - 2.0f * progress);
    return advanceSpeedMultiplier * (1.0f - smoothProgress);
}

void UpdateNpcAiSystem(
        engine::World& world,
        engine::AssetManager& assets,
        engine::AudioSystem& audio,
        SectorNavigationWorld& navigation,
        NpcNavigationRuntime& npcNavigation,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        NpcAiRuntime& runtime,
        const NpcAiGameplayContext& gameplay,
        float dt,
        const SectorSoundPropagationWorld* soundPropagation,
        const std::vector<RuntimePortalDynamicBlocker>* portalBlockers)
{
    const float safeDt = std::max(0.0f, dt);
    const bool useSneakDetection = gameplay.playerSneaking
            && gameplay.playerSneakSettings != nullptr;
    runtime.playerLightLevel = gameplay.playerLightLevel != nullptr
            ? *gameplay.playerLightLevel
            : PlayerLightLevelSample{};
    runtime.playerLightDetectionFactor =
            useSneakDetection
            ? PlayerSneakLightDetectionFactor(
                    gameplay.playerNormalizedLightLevel,
                    gameplay.playerSneakSettings->darknessCutoffNormalized,
                    gameplay.playerSneakSettings
                            ->lightHalfResponseRangeNormalized)
            : 1.0f;
    runtime.playerCrouchBlend = std::isfinite(gameplay.playerCrouchBlend)
            ? std::clamp(gameplay.playerCrouchBlend, 0.0f, 1.0f)
            : 0.0f;
    runtime.playerMovementNoiseMultiplier = gameplay.playerSneaking
            && std::isfinite(gameplay.playerMovementNoiseMultiplier)
            ? std::max(0.0f, gameplay.playerMovementNoiseMultiplier)
            : 1.0f;
    runtime.playerSneaking = gameplay.playerSneaking;
    for (NpcSoundEvent& sound : runtime.playerSounds) {
        sound.remainingSeconds -= safeDt;
    }
    runtime.playerSounds.erase(
            std::remove_if(
                    runtime.playerSounds.begin(), runtime.playerSounds.end(),
                    [](const NpcSoundEvent& sound) {
                        return sound.remainingSeconds <= 0.0f;
                    }),
            runtime.playerSounds.end());

    BuildPlayerPursuitSlots(
            world,
            navigation,
            collisionWorld,
            doorColliders,
            staticColliders,
            npcNavigation,
            runtime,
            gameplay,
            safeDt);

    world.ForEach<NpcRuntimeInstance, NpcAiState, SectorObjectTransform,
            Health, NpcCombatState>(
            [&](engine::Entity entity,
                    NpcRuntimeInstance& npc,
                    NpcAiState& ai,
                    SectorObjectTransform& transform,
                    Health& health,
                    NpcCombatState& combat) {
        engine::AnimatedModelAnimator* animator =
                world.Has<engine::AnimatedModelAnimator>(entity)
                ? &world.Get<engine::AnimatedModelAnimator>(entity) : nullptr;
        const bool aiActive = ai.awareness != NpcAwarenessState::Unaware
                || ai.attackCommitted
                || (FindNavigationRecord(npcNavigation, entity) != nullptr
                    && FindNavigationRecord(npcNavigation, entity)->authority
                            == NpcMoveAuthority::Ai);
        if (animator != nullptr) {
            animator->paused = gameplay.frozen && aiActive
                    && !combat.dead
                    && !combat.hurtAnimationRequested
                    && !combat.hurtAnimationPlaying
                    && combat.staggerRemainingSeconds <= 0.0f;
        }
        if (combat.dead || IsDepleted(health)) {
            ai.previousIntent = NpcAiIntent::Idle;
            FinishAttack(npc, ai);
            return;
        }
        if (combat.staggerRemainingSeconds > 0.0f
                || combat.hurtAnimationRequested
                || combat.hurtAnimationPlaying) {
            ai.previousIntent = NpcAiIntent::Idle;
            if (ai.attackCommitted) FinishAttack(npc, ai);
            return;
        }
        if (npc.hostile && gameplay.playerInvisible) {
            ai.awareness = NpcAwarenessState::Unaware;
            ai.previousIntent = NpcAiIntent::Idle;
            ai.directAlertPending = false;
            ai.scriptTakeoverPending = false;
            ai.playerInGeometricSight = false;
            ai.playerDetectionAudioPending = false;
            ai.visualDetectionProgress = 0.0f;
            ai.visualLightDetectionFactor = 0.0f;
            ai.visualProximityDetectionFactor = 0.0f;
            ai.visualDetectionRateFactor = 0.0f;
            ai.visualDetectionReason = NpcVisualDetectionReason::NoPlayer;
            CancelAiMove(world, navigation, npcNavigation, npc);
            FinishAttack(npc, ai);
            ResetPursuitAssignment(ai);
            return;
        }
        if (gameplay.frozen) return;

        const NpcAwarenessState previousAwareness = ai.awareness;
        const bool directAlert = ai.directAlertPending;
        ai.directAlertPending = false;
        const bool playerAlive = gameplay.playerHealth != nullptr
                && !IsDepleted(*gameplay.playerHealth);
        NpcPlayerSightEvaluation sight;
        if (playerAlive) {
            sight = EvaluatePlayerSight(
                    ai, transform, collisionWorld, doorColliders,
                    staticColliders, gameplay.playerFeetPosition,
                    gameplay.playerEyePosition);
        }
        ai.playerInGeometricSight = playerAlive && sight.visible;
        if (!playerAlive) {
            ai.visualLightDetectionFactor = 0.0f;
            ai.visualProximityDetectionFactor = 0.0f;
        } else if (useSneakDetection) {
            ai.visualLightDetectionFactor = PlayerSneakLightDetectionFactor(
                    gameplay.playerNormalizedLightLevel,
                    gameplay.playerSneakSettings->darknessCutoffNormalized,
                    gameplay.playerSneakSettings
                            ->lightHalfResponseRangeNormalized);
            ai.visualProximityDetectionFactor =
                    PlayerSneakProximityDetectionFactor(
                            sight.distanceWorld,
                            gameplay.playerSneakSettings
                                    ->darknessProximityRangeWorld);
        } else {
            ai.visualLightDetectionFactor = 1.0f;
            ai.visualProximityDetectionFactor = 0.0f;
        }
        bool seesPlayer = false;
        if (ai.awareness == NpcAwarenessState::Detected) {
            seesPlayer = ai.playerInGeometricSight;
            ai.visualDetectionProgress = 1.0f;
            ai.visualDetectionRateFactor = 1.0f;
            ai.visualDetectionReason = directAlert || seesPlayer
                    ? NpcVisualDetectionReason::Detected
                    : sight.failureReason;
        } else if (!useSneakDetection) {
            seesPlayer = ai.playerInGeometricSight;
            ai.visualDetectionProgress = seesPlayer ? 1.0f : 0.0f;
            ai.visualDetectionRateFactor = seesPlayer ? 1.0f : 0.0f;
            ai.visualDetectionReason = seesPlayer
                    ? NpcVisualDetectionReason::Detected
                    : sight.failureReason;
        } else {
            const PlayerVisualDetectionStep detection =
                    AdvancePlayerVisualDetection(
                            ai.visualDetectionProgress,
                            ai.playerInGeometricSight,
                            sight.distanceWorld,
                            gameplay.playerNormalizedLightLevel,
                            gameplay.playerCrouchBlend,
                            *gameplay.playerSneakSettings,
                            safeDt);
            ai.visualDetectionProgress = detection.progress;
            ai.visualLightDetectionFactor = detection.lightFactor;
            ai.visualProximityDetectionFactor = detection.proximityFactor;
            ai.visualDetectionRateFactor = detection.rateFactor;
            seesPlayer = detection.detected;
            if (!playerAlive) {
                ai.visualDetectionReason =
                        NpcVisualDetectionReason::NoPlayer;
            } else if (!ai.playerInGeometricSight) {
                ai.visualDetectionReason = sight.failureReason;
            } else if (!detection.building) {
                ai.visualDetectionReason =
                        NpcVisualDetectionReason::Darkness;
            } else {
                ai.visualDetectionReason = detection.detected
                        ? NpcVisualDetectionReason::Detected
                        : NpcVisualDetectionReason::Building;
            }
        }
        if (seesPlayer) {
            ai.awareness = NpcAwarenessState::Detected;
            ai.visualDetectionProgress = 1.0f;
            ai.visualDetectionReason = NpcVisualDetectionReason::Detected;
            ai.lastKnownPlayerPosition = gameplay.playerFeetPosition;
            ai.searchRemainingSeconds = 0.0f;
        } else if (ai.awareness == NpcAwarenessState::Detected
                && !directAlert
                && !ai.attackCommitted) {
            ai.awareness = NpcAwarenessState::InvestigatingTravel;
            ai.visualDetectionProgress = 0.0f;
            ai.searchRemainingSeconds =
                    ai.perception.investigationDurationMilliseconds / 1000.0f;
        }
        if (npc.hostile
                && previousAwareness != NpcAwarenessState::Detected
                && ai.awareness == NpcAwarenessState::Detected) {
            ai.playerDetectionAudioPending = true;
        }

        if (ai.awareness != NpcAwarenessState::Detected) {
            for (auto sound = runtime.playerSounds.rbegin();
                    sound != runtime.playerSounds.rend(); ++sound) {
                if (sound->sequence <= ai.lastHeardSoundSequence) break;
                const Vector2 delta{
                        sound->positionWorld.x - transform.position.x,
                        sound->positionWorld.z - transform.position.z};
                const float distance = Vector2Length(delta);
                bool audible = distance <= sound->radiusWorld
                        && distance <= ai.perception.hearingRangeWorld;
                if (soundPropagation != nullptr) {
                    static const std::vector<RuntimePortalDynamicBlocker>
                            noPortalBlockers;
                    const Vector3 npcEar{
                            transform.position.x,
                            transform.position.y + 1.4f,
                            transform.position.z};
                    const SectorSoundPropagationResult propagation =
                            soundPropagation->Evaluate(
                                    &collisionWorld,
                                    doorColliders,
                                    portalBlockers != nullptr
                                            ? *portalBlockers
                                            : noPortalBlockers,
                                    npcEar,
                                    sound->positionWorld);
                    const auto pathAudible = [&](
                            const SectorSoundPropagationPath& path) {
                        return path.valid
                                && path.distanceWorld
                                        <= ai.perception.hearingRangeWorld
                                && path.distanceWorld
                                        <= sound->radiusWorld
                                                * path.volumeScale;
                    };
                    audible = pathAudible(propagation.transmission)
                            || pathAudible(propagation.portal);
                }
                if (audible) {
                    ai.lastHeardSoundSequence = sound->sequence;
                    ai.lastKnownPlayerPosition = sound->positionWorld;
                    ai.awareness = NpcAwarenessState::InvestigatingTravel;
                    ai.searchRemainingSeconds =
                            ai.perception.investigationDurationMilliseconds
                            / 1000.0f;
                    break;
                }
            }
        }

        if ((ai.scriptTakeoverPending
                    || (previousAwareness == NpcAwarenessState::Unaware
                        && ai.awareness != NpcAwarenessState::Unaware))
                && gameplay.interruptScriptMovement != nullptr) {
            gameplay.interruptScriptMovement(
                    gameplay.scriptUserData, entity, npc.instanceId.c_str());
            ai.scriptTakeoverPending = false;
        }

        if (ai.awareness == NpcAwarenessState::InvestigatingTravel) {
            ai.previousIntent = NpcAiIntent::Idle;
            npc.actionLockedByAi = false;
            const Vector2 investigationDestination{
                    ai.lastKnownPlayerPosition.x,
                    ai.lastKnownPlayerPosition.z};
            const auto beginSearch = [&]() {
                ai.awareness = NpcAwarenessState::InvestigatingSearch;
                CancelAiMove(world, navigation, npcNavigation, npc);
            };
            NpcMoveStatus move = GetNpcMoveStatus(
                    npcNavigation, npc.instanceId);
            const bool terminalInvestigationMove = move.found
                    && move.authority == NpcMoveAuthority::Ai
                    && (move.phase == NpcMovePhase::Arrived
                        || move.phase == NpcMovePhase::Failed)
                    && Vector2Distance(
                            move.requestedDestinationXZ,
                            investigationDestination)
                            <= ChaseRetargetDistanceWorld;
            if (terminalInvestigationMove) {
                beginSearch();
                return;
            }
            RequestAiMove(
                    world, navigation, collisionWorld, npcNavigation, npc,
                    investigationDestination,
                    NpcMoveGait::Run);
            move = GetNpcMoveStatus(
                    npcNavigation, npc.instanceId);
            if (!move.found
                    || move.phase != NpcMovePhase::FollowingPath
                    || move.authority != NpcMoveAuthority::Ai) {
                beginSearch();
            }
            return;
        }
        if (ai.awareness == NpcAwarenessState::InvestigatingSearch) {
            ai.previousIntent = NpcAiIntent::Idle;
            CancelAiMove(world, navigation, npcNavigation, npc);
            npc.actionLockedByAi = false;
            npc.action = NpcAction::Idle;
            ai.searchRemainingSeconds -= safeDt;
            transform.yawRadians += ai.searchTurnDirection
                    * SearchTurnRadiansPerSecond * safeDt;
            if (ai.searchRemainingSeconds <= 0.0f) {
                ai.awareness = NpcAwarenessState::Unaware;
            }
            return;
        }
        if (ai.awareness != NpcAwarenessState::Detected) {
            ai.previousIntent = NpcAiIntent::Idle;
            FinishAttack(npc, ai);
            return;
        }

        const NpcAiTypeDescriptor* plugin = FindNpcAiType(ai.aiType);
        if (plugin == nullptr || plugin->update == nullptr) {
            ai.previousIntent = NpcAiIntent::Idle;
            return;
        }
        const Vector3 pursuitPosition = seesPlayer
                ? gameplay.playerFeetPosition : ai.lastKnownPlayerPosition;
        const Vector2 toPlayer{
                pursuitPosition.x - transform.position.x,
                pursuitPosition.z - transform.position.z};
        const float playerDistance = Vector2Length(toPlayer);
        const NpcAiIntent intent = plugin->update(NpcAiPluginInput{
                playerDistance, ai.attack.rangeWorld,
                ai.attackCommitted, playerAlive, ai.previousIntent});
        ai.previousIntent = intent;
        if (intent == NpcAiIntent::Idle) {
            CancelAiMove(world, navigation, npcNavigation, npc);
            FinishAttack(npc, ai);
            return;
        }
        if (intent == NpcAiIntent::ChasePlayer) {
            FinishAttack(npc, ai);
            ai.retargetRemainingSeconds -= safeDt;
            if (ai.retargetRemainingSeconds <= 0.0f) {
                const NpcPursuitSlot* slot = FindPursuitSlot(runtime, entity);
                const Vector2 destination = slot != nullptr && slot->projected
                        ? Vector2{slot->resolvedPosition.x,
                                slot->resolvedPosition.z}
                        : Vector2{pursuitPosition.x, pursuitPosition.z};
                const NpcMoveStatus before = GetNpcMoveStatus(
                        npcNavigation, npc.instanceId);
                RequestAiMove(
                        world, navigation, collisionWorld, npcNavigation, npc,
                        destination,
                        NpcMoveGait::Run);
                const NpcMoveStatus after = GetNpcMoveStatus(
                        npcNavigation, npc.instanceId);
                ai.pursuitRetargetFailed = slot != nullptr
                        && (!after.found
                            || after.requestId == before.requestId)
                        && Vector2Distance(
                                before.requestedDestinationXZ,
                                destination) > ChaseRetargetDistanceWorld;
                ai.retargetRemainingSeconds = ChaseRetargetSeconds;
            }
            return;
        }

        if (!ai.attackCommitted) {
            StartAttack(
                    world, assets, audio, navigation, npcNavigation, entity,
                    npc, ai, transform, pursuitPosition);
            return;
        }
        const float phase = AttackAnimationPhase(world, assets, entity);
        ai.attackPhase = phase;
        if (phase <= ai.attack.aimTrackingEndPhase) {
            const Vector2 trackingDirection{
                    gameplay.playerFeetPosition.x - transform.position.x,
                    gameplay.playerFeetPosition.z - transform.position.z};
            if (Vector2Length(trackingDirection) > 0.0001f) {
                transform.yawRadians = std::atan2(
                        trackingDirection.x, trackingDirection.y);
            }
        }
        if (!ai.attackHitResolved && phase >= ai.attack.hitPhase) {
            ai.attackHitResolved = true;
            const Vector2 actualToPlayer{
                    gameplay.playerFeetPosition.x - transform.position.x,
                    gameplay.playerFeetPosition.z - transform.position.z};
            const float actualPlayerDistance = Vector2Length(actualToPlayer);
            const bool connected = IsNpcAiCommittedMeleeHitInRange(
                            actualPlayerDistance, ai.attack.rangeWorld)
                    && IsNpcAiCommittedMeleeHitWithinArc(
                            actualToPlayer,
                            transform.yawRadians,
                            ai.attack.hitArcDegrees)
                    && HasLineOfSight(
                            collisionWorld, doorColliders, staticColliders,
                            {transform.position.x,
                                    transform.position.y + 1.4f,
                                    transform.position.z},
                            gameplay.playerEyePosition);
            if (connected) {
                if (!engine::IsNull(ai.attackImpactSound)) {
                    engine::PositionalSoundSettings positional;
                    positional.position = transform.position;
                    positional.minimumDistanceWorld =
                            AttackSoundMinimumDistanceWorld;
                    positional.maximumDistanceWorld =
                            AttackSoundMaximumDistanceWorld;
                    audio.PlaySoundAt(
                            assets, ai.attackImpactSound, positional);
                }
                ApplyNpcAiPlayerAttackEffects(
                        gameplay, ai.attack, actualToPlayer);
            }
        }
        if (animator == nullptr || AttackAnimationFinished(world, entity)) {
            FinishAttack(npc, ai);
        }
    });
}

} // namespace game
