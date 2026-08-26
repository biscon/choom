#include "game/npc/ai/NpcAiSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/Health.h"
#include "game/npc/NpcNavigationSystem.h"
#include "game/npc/ai/NpcAiTypes.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelCollision.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

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

bool CanSeePlayer(
        const NpcAiState& ai,
        const SectorObjectTransform& transform,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        Vector3 playerFeet,
        Vector3 playerEye)
{
    const Vector2 toPlayer{
            playerFeet.x - transform.position.x,
            playerFeet.z - transform.position.z};
    const float distance = Vector2Length(toPlayer);
    if (distance > ai.perception.visionRangeWorld) return false;
    if (distance > 0.0001f) {
        const Vector2 forward{
                std::sin(transform.yawRadians),
                std::cos(transform.yawRadians)};
        const float cosine = Vector2DotProduct(
                forward, Vector2Scale(toPlayer, 1.0f / distance));
        const float minimumCosine = std::cos(
                ai.perception.visionAngleDegrees * 0.5f * DEG2RAD);
        if (cosine < minimumCosine) return false;
    }
    const Vector3 npcEye{
            transform.position.x,
            transform.position.y + 1.4f,
            transform.position.z};
    return HasLineOfSight(
            collisionWorld, doorColliders, staticColliders,
            npcEye, playerEye);
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
        if (status.authority != NpcMoveAuthority::Ai) return;
        const Vector2 delta = Vector2Subtract(
                status.requestedDestinationXZ, destination);
        if (Vector2Length(delta) <= ChaseRetargetDistanceWorld) return;
        CancelNpcMove(world, navigation, runtime, npc.instanceId, status.requestId);
    }
    RequestNpcMove(
            world, navigation, collisionWorld, runtime,
            npc.instanceId, destination, gait, NpcMoveAuthority::Ai);
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
    npc.actionLockedByAi = false;
    npc.action = NpcAction::Idle;
}

} // namespace

void InitializeNpcAiRuntime(NpcAiRuntime& runtime, size_t soundCapacity)
{
    runtime.playerSounds.clear();
    runtime.playerSounds.reserve(soundCapacity);
    runtime.nextSoundSequence = 1;
    runtime.capacityWarningPrinted = false;
}

void ClearNpcAiRuntime(NpcAiRuntime& runtime)
{
    runtime.playerSounds.clear();
    runtime.nextSoundSequence = 1;
    runtime.capacityWarningPrinted = false;
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
    ai.awareness = NpcAwarenessState::Detected;
    ai.lastKnownPlayerPosition = playerPositionWorld;
    ai.scriptTakeoverPending = true;
    ai.directAlertPending = true;
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
        float dt)
{
    const float safeDt = std::max(0.0f, dt);
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
        if (gameplay.frozen) return;

        const NpcAwarenessState previousAwareness = ai.awareness;
        const bool directAlert = ai.directAlertPending;
        ai.directAlertPending = false;
        const bool playerAlive = gameplay.playerHealth != nullptr
                && !IsDepleted(*gameplay.playerHealth);
        const bool seesPlayer = playerAlive && CanSeePlayer(
                ai, transform, collisionWorld, doorColliders,
                staticColliders, gameplay.playerFeetPosition,
                gameplay.playerEyePosition);
        if (seesPlayer) {
            ai.awareness = NpcAwarenessState::Detected;
            ai.lastKnownPlayerPosition = gameplay.playerFeetPosition;
            ai.searchRemainingSeconds = 0.0f;
        } else if (ai.awareness == NpcAwarenessState::Detected
                && !directAlert
                && !ai.attackCommitted) {
            ai.awareness = NpcAwarenessState::InvestigatingTravel;
            ai.searchRemainingSeconds =
                    ai.perception.investigationDurationMilliseconds / 1000.0f;
        }

        if (ai.awareness != NpcAwarenessState::Detected) {
            for (auto sound = runtime.playerSounds.rbegin();
                    sound != runtime.playerSounds.rend(); ++sound) {
                if (sound->sequence <= ai.lastHeardSoundSequence) break;
                const Vector2 delta{
                        sound->positionWorld.x - transform.position.x,
                        sound->positionWorld.z - transform.position.z};
                const float distance = Vector2Length(delta);
                if (distance <= sound->radiusWorld
                        && distance <= ai.perception.hearingRangeWorld) {
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
            RequestAiMove(
                    world, navigation, collisionWorld, npcNavigation, npc,
                    {ai.lastKnownPlayerPosition.x,
                            ai.lastKnownPlayerPosition.z},
                    NpcMoveGait::Run);
            const NpcMoveStatus move = GetNpcMoveStatus(
                    npcNavigation, npc.instanceId);
            if (!move.found || move.phase == NpcMovePhase::Arrived
                    || move.phase == NpcMovePhase::Failed) {
                ai.awareness = NpcAwarenessState::InvestigatingSearch;
                CancelAiMove(world, navigation, npcNavigation, npc);
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
                RequestAiMove(
                        world, navigation, collisionWorld, npcNavigation, npc,
                        {pursuitPosition.x, pursuitPosition.z},
                        NpcMoveGait::Run);
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
        if (!ai.attackHitResolved && phase >= ai.attack.hitPhase) {
            ai.attackHitResolved = true;
            const Vector2 actualToPlayer{
                    gameplay.playerFeetPosition.x - transform.position.x,
                    gameplay.playerFeetPosition.z - transform.position.z};
            const float actualPlayerDistance = Vector2Length(actualToPlayer);
            const bool connected = IsNpcAiCommittedMeleeHitInRange(
                            actualPlayerDistance, ai.attack.rangeWorld)
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
