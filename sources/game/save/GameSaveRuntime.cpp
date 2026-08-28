#include "game/save/GameSaveRuntime.h"

#include "engine/assets/ModelAssets.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/SectorScriptBindings.h"
#include "game/npc/NpcNavigationSystem.h"
#include "sector_demo/SectorBillboardRuntime.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorSceneRuntime.h"

#include <algorithm>
#include <cstdio>

namespace game {
namespace {

engine::Entity FindRuntimeEntity(
        const SectorRuntimeObjectState& state,
        int placedObjectId)
{
    const auto found = std::find_if(
            state.placedObjectEntities.begin(),
            state.placedObjectEntities.end(),
            [placedObjectId](const SectorPlacedRuntimeObjectEntity& value) {
                return value.placedObjectId == placedObjectId;
            });
    return found == state.placedObjectEntities.end()
            ? engine::NullEntity() : found->entity;
}

std::string AnimationName(
        const engine::ModelAsset* asset,
        std::uint32_t index)
{
    if (asset == nullptr || index == engine::InvalidModelAnimationIndex
            || index >= engine::ModelAnimationClipCount(*asset)) {
        return {};
    }
    const char* name = engine::ModelAnimationClipName(*asset, index);
    return name != nullptr ? std::string{name} : std::string{};
}

GameSaveAnimatorState CaptureAnimator(
        const engine::World& world,
        const engine::AssetManager& assets,
        engine::Entity entity)
{
    GameSaveAnimatorState result;
    const engine::AnimatedModelAnimator& animator =
            world.Get<engine::AnimatedModelAnimator>(entity);
    const engine::AnimatedModelInstance& instance =
            world.Get<engine::AnimatedModelInstance>(entity);
    const engine::ModelAsset* asset = assets.GetModelAsset(instance.model);
    const std::uint32_t active = animator.nodeAnimationIndex
                    != engine::InvalidModelAnimationIndex
            ? animator.nodeAnimationIndex : animator.animationIndex;
    result.animationName = AnimationName(asset, active);
    result.targetAnimationName = AnimationName(asset, animator.targetAnimationIndex);
    result.frame = animator.frame;
    result.targetFrame = animator.targetFrame;
    result.speed = animator.speed;
    result.playing = animator.playing;
    result.loop = animator.loop;
    result.reverse = animator.reverse;
    result.finished = animator.finished;
    result.paused = animator.paused;
    result.targetLoop = animator.targetLoop;
    result.targetFinished = animator.targetFinished;
    result.transitionDurationSeconds = animator.transitionDurationSeconds;
    result.transitionElapsedSeconds = animator.transitionElapsedSeconds;
    return result;
}

bool ApplyAnimator(
        engine::World& world,
        engine::AssetManager& assets,
        engine::Entity entity,
        const GameSaveAnimatorState& state)
{
    if (!world.Has<engine::AnimatedModelAnimator>(entity)
            || !world.Has<engine::AnimatedModelInstance>(entity)) return false;
    engine::AnimatedModelAnimator& animator =
            world.Get<engine::AnimatedModelAnimator>(entity);
    const engine::AnimatedModelInstance& instance =
            world.Get<engine::AnimatedModelInstance>(entity);
    const engine::ModelAsset* asset = assets.GetModelAsset(instance.model);
    if (asset == nullptr || state.animationName.empty()) return false;
    const int active = engine::FindModelAnimationClipIndex(*asset, state.animationName.c_str());
    if (active < 0 || !engine::SetAnimatedModelClip(
                animator, *asset, static_cast<std::uint32_t>(active), true)) {
        return false;
    }
    animator.frame = state.frame;
    animator.speed = state.speed;
    animator.playing = state.playing;
    animator.loop = state.loop;
    animator.reverse = state.reverse;
    animator.finished = state.finished;
    animator.paused = state.paused;
    animator.targetAnimationIndex = engine::InvalidModelAnimationIndex;
    if (!state.targetAnimationName.empty()) {
        const int target = engine::FindModelAnimationClipIndex(
                *asset, state.targetAnimationName.c_str());
        if (target >= 0) {
            animator.targetAnimationIndex = static_cast<std::uint32_t>(target);
            animator.targetFrame = state.targetFrame;
            animator.targetLoop = state.targetLoop;
            animator.targetFinished = state.targetFinished;
            animator.transitionDurationSeconds = state.transitionDurationSeconds;
            animator.transitionElapsedSeconds = state.transitionElapsedSeconds;
        }
    }
    animator.poseDirty = true;
    return true;
}

template <typename Light>
void CaptureLights(
        const std::vector<Light>& source,
        std::vector<GameSaveDynamicLightState>& destination)
{
    for (const Light& light : source) {
        if (light.instanceId.empty()) continue;
        destination.push_back(GameSaveDynamicLightState{
                light.instanceId, light.color, light.intensity, light.enabled});
    }
}

template <typename Light>
void ApplyLights(std::vector<Light>& lights, const GameSaveLevelState& state)
{
    for (Light& light : lights) {
        const auto found = std::find_if(
                state.dynamicLights.begin(), state.dynamicLights.end(),
                [&light](const GameSaveDynamicLightState& saved) {
                    return saved.instanceId == light.instanceId;
                });
        if (found == state.dynamicLights.end()) continue;
        light.color = found->color;
        light.intensity = found->intensity;
        light.enabled = found->enabled;
    }
}

void ResetNpcAwareness(engine::World& world, engine::Entity entity)
{
    if (!world.Has<NpcAiState>(entity)) return;
    NpcAiState& ai = world.Get<NpcAiState>(entity);
    ai.awareness = NpcAwarenessState::Unaware;
    ai.lastKnownPlayerPosition = {};
    ai.searchRemainingSeconds = 0.0f;
    ai.retargetRemainingSeconds = 0.0f;
    ai.attackPhase = 0.0f;
    ai.attackCommitted = false;
    ai.attackHitResolved = false;
    ai.scriptTakeoverPending = false;
    ai.directAlertPending = false;
    ai.pursuitSlotIndex = -1;
    ai.pursuitSlotRing = -1;
    ai.pursuitSlotKind = NpcPursuitSlotKind::None;
    ai.visualDetectionProgress = 0.0f;
    ai.playerInGeometricSight = false;
    ai.playerDetectionAudioPending = false;
}

} // namespace

const GameSaveLevelState* FindGameSaveLevelState(
        const std::vector<GameSaveLevelState>& levels,
        std::string_view levelId)
{
    const auto found = std::find_if(
            levels.begin(), levels.end(),
            [levelId](const GameSaveLevelState& state) {
                return state.levelId == levelId;
            });
    return found == levels.end() ? nullptr : &*found;
}

void UpsertGameSaveLevelState(
        std::vector<GameSaveLevelState>& levels,
        GameSaveLevelState state)
{
    const auto found = std::find_if(
            levels.begin(), levels.end(),
            [&state](const GameSaveLevelState& value) {
                return value.levelId == state.levelId;
            });
    if (found != levels.end()) {
        *found = std::move(state);
    } else {
        levels.push_back(std::move(state));
    }
}

GameSaveLevelState CaptureGameSaveLevelState(
        const engine::World& world,
        const engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const SectorRuntimeObjectState& runtimeObjects,
        const SectorScriptHost& scriptHost,
        std::string levelId)
{
    GameSaveLevelState result;
    result.levelId = std::move(levelId);
    CaptureLights(map.dynamicPointLights, result.dynamicLights);
    CaptureLights(map.dynamicSpotLights, result.dynamicLights);
    CaptureLights(map.dynamicRectLights, result.dynamicLights);
    for (const SectorScriptTriggerState& trigger : scriptHost.triggers) {
        if (trigger.triggerIndex >= map.triggers.size()) continue;
        result.triggers.push_back(GameSaveTriggerState{
                map.triggers[trigger.triggerIndex].id,
                trigger.enabled,
                trigger.inside,
                trigger.pending,
                trigger.consumed,
                trigger.remainingDelayMilliseconds});
    }

    for (const SectorPlacedRuntimeObjectEntity& tracked :
            runtimeObjects.placedObjectEntities) {
        const SectorPlacedRuntimeObject* authored =
                FindSectorPlacedRuntimeObject(map, tracked.placedObjectId);
        if (authored == nullptr) continue;
        const engine::Entity entity = tracked.entity;
        if (!world.IsAlive(entity)) {
            if (authored->kind == "npc") {
                GameSaveNpcState npc;
                npc.placedObjectId = authored->id;
                npc.instanceId = authored->npc.instanceId;
                npc.health = MakeHealth(1);
                npc.health.current = 0;
                npc.dead = true;
                npc.deathAnimationComplete = true;
                npc.despawned = true;
                result.npcs.push_back(std::move(npc));
            }
            continue;
        }
        if (world.Has<SectorDoor>(entity) && world.Has<SectorDoorMotion>(entity)) {
            const SectorDoor& door = world.Get<SectorDoor>(entity);
            const SectorDoorMotion& motion = world.Get<SectorDoorMotion>(entity);
            result.doors.push_back(GameSaveDoorState{door.placedObjectId,
                    door.instanceId, motion.openFraction,
                    motion.targetOpenFraction, door.enabled});
            continue;
        }
        if (world.Has<NpcRuntimeInstance>(entity)
                && world.Has<SectorObjectTransform>(entity)
                && world.Has<Health>(entity)
                && world.Has<NpcCombatState>(entity)) {
            const NpcRuntimeInstance& runtime = world.Get<NpcRuntimeInstance>(entity);
            const SectorObjectTransform& transform =
                    world.Get<SectorObjectTransform>(entity);
            const NpcCombatState& combat = world.Get<NpcCombatState>(entity);
            GameSaveNpcState npc;
            npc.placedObjectId = authored->id;
            npc.instanceId = runtime.instanceId;
            npc.position = transform.position;
            npc.yawRadians = transform.yawRadians;
            npc.health = world.Get<Health>(entity);
            npc.dead = combat.dead;
            npc.deathAnimationComplete = combat.deathAnimationComplete;
            npc.corpseElapsedSeconds = combat.corpseElapsedSeconds;
            if (world.Has<SectorDynamicModel>(entity)) {
                npc.opacity = world.Get<SectorDynamicModel>(entity).opacity;
            }
            if (world.Has<engine::AnimatedModelAnimator>(entity)
                    && world.Has<engine::AnimatedModelInstance>(entity)) {
                npc.hasAnimator = true;
                npc.animator = CaptureAnimator(world, assets, entity);
            }
            if (world.Has<NpcPatrolState>(entity)) {
                const NpcPatrolState& patrol = world.Get<NpcPatrolState>(entity);
                npc.hasPatrol = true;
                npc.patrolEditorId = patrol.patrolEditorId;
                npc.waypointIndex = patrol.waypointIndex;
                npc.direction = patrol.direction;
                npc.shuffleOrder = patrol.shuffleOrder;
                npc.shuffleCursor = patrol.shuffleCursor;
                npc.randomState = patrol.randomState;
                npc.phase = static_cast<int>(patrol.phase);
                npc.resumePhase = static_cast<int>(patrol.resumePhase);
                npc.waitRemainingSeconds = patrol.waitRemainingSeconds;
                npc.waypointBaseYawRadians = patrol.waypointBaseYawRadians;
                npc.lookOffsetRadians = patrol.lookOffsetRadians;
                npc.lookDirection = patrol.lookDirection;
                npc.retryRemainingSeconds = patrol.retryRemainingSeconds;
                npc.destinationXZ = patrol.destinationXZ;
                npc.stoppedByScript = patrol.stoppedByScript;
                npc.destinationInitialized = patrol.destinationInitialized;
            }
            result.npcs.push_back(std::move(npc));
            continue;
        }
        if (world.Has<SectorStaticModel>(entity)) {
            const SectorStaticModel& prop = world.Get<SectorStaticModel>(entity);
            result.props.push_back(GameSavePropState{
                    prop.placedObjectId, prop.instanceId, prop.emissiveScale});
            continue;
        }
        if (world.Has<SectorDynamicModel>(entity)) {
            const SectorDynamicModel& prop = world.Get<SectorDynamicModel>(entity);
            GameSavePropState saved;
            saved.placedObjectId = prop.placedObjectId;
            saved.instanceId = prop.instanceId;
            saved.emissiveScale = prop.emissiveScale;
            saved.opacity = prop.opacity;
            saved.useConsumed = prop.useConsumed;
            if (world.Has<engine::AnimatedModelAnimator>(entity)
                    && world.Has<engine::AnimatedModelInstance>(entity)) {
                saved.hasAnimator = true;
                saved.animator = CaptureAnimator(world, assets, entity);
            }
            result.props.push_back(std::move(saved));
            continue;
        }
        if (world.Has<SectorBillboardAnimator>(entity)) {
            const SectorBillboardAnimator& animator =
                    world.Get<SectorBillboardAnimator>(entity);
            result.billboards.push_back(GameSaveBillboardState{
                    authored->id, animator.timeSeconds, animator.speed,
                    animator.playing, animator.loop, animator.finished});
        }
    }
    return result;
}

void ApplyGameSaveLevelMapState(
        SectorTopologyMap& map,
        const GameSaveLevelState& state)
{
    ApplyLights(map.dynamicPointLights, state);
    ApplyLights(map.dynamicSpotLights, state);
    ApplyLights(map.dynamicRectLights, state);
}

void ApplyGameSaveLevelRuntimeState(
        engine::World& world,
        engine::AssetManager& assets,
        SectorSceneRuntime& scene,
        SectorScriptHost& scriptHost,
        const GameSaveLevelState& state)
{
    SectorRuntimeObjectState& runtimeObjects = scene.RuntimeObjects();
    for (const GameSaveDoorState& saved : state.doors) {
        const engine::Entity entity = FindRuntimeEntity(runtimeObjects, saved.placedObjectId);
        if (!world.IsAlive(entity) || !world.Has<SectorDoor>(entity)
                || !world.Has<SectorDoorMotion>(entity)) continue;
        SectorDoor& door = world.Get<SectorDoor>(entity);
        if (!saved.instanceId.empty() && door.instanceId != saved.instanceId) continue;
        door.enabled = saved.enabled;
        SectorDoorMotion& motion = world.Get<SectorDoorMotion>(entity);
        motion.openFraction = saved.openFraction;
        motion.targetOpenFraction = saved.targetOpenFraction;
    }
    for (const GameSavePropState& saved : state.props) {
        const engine::Entity entity = FindRuntimeEntity(runtimeObjects, saved.placedObjectId);
        if (!world.IsAlive(entity)) continue;
        if (world.Has<SectorStaticModel>(entity)) {
            SectorStaticModel& prop = world.Get<SectorStaticModel>(entity);
            if (!saved.instanceId.empty() && prop.instanceId != saved.instanceId) continue;
            prop.emissiveScale = saved.emissiveScale;
        } else if (world.Has<SectorDynamicModel>(entity)
                && !world.Has<NpcRuntimeInstance>(entity)) {
            SectorDynamicModel& prop = world.Get<SectorDynamicModel>(entity);
            if (!saved.instanceId.empty() && prop.instanceId != saved.instanceId) continue;
            prop.emissiveScale = saved.emissiveScale;
            prop.opacity = saved.opacity;
            prop.useConsumed = saved.useConsumed;
            if (saved.hasAnimator) ApplyAnimator(world, assets, entity, saved.animator);
        }
    }
    bool removedNpc = false;
    for (const GameSaveNpcState& saved : state.npcs) {
        const engine::Entity entity = FindRuntimeEntity(runtimeObjects, saved.placedObjectId);
        if (!world.IsAlive(entity) || !world.Has<NpcRuntimeInstance>(entity)) continue;
        const NpcRuntimeInstance& runtime = world.Get<NpcRuntimeInstance>(entity);
        if (runtime.instanceId != saved.instanceId) continue;
        if (saved.despawned) {
            DeactivateNpcNavigation(
                    world,
                    scene.Navigation(),
                    scene.NpcNavigation(),
                    entity);
            removedNpc = QueueRemoveSectorRuntimeObjectByEntity(
                    world, runtimeObjects, entity) || removedNpc;
            continue;
        }
        if (world.Has<SectorObjectTransform>(entity)) {
            SectorObjectTransform& transform = world.Get<SectorObjectTransform>(entity);
            transform.position = saved.position;
            transform.yawRadians = saved.yawRadians;
        }
        if (world.Has<Health>(entity)) world.Get<Health>(entity) = saved.health;
        bool animatorRestored = false;
        if (world.Has<NpcCombatState>(entity)) {
            NpcCombatState& combat = world.Get<NpcCombatState>(entity);
            combat.dead = saved.dead;
            combat.deathAnimationComplete = saved.deathAnimationComplete;
            combat.corpseElapsedSeconds = saved.corpseElapsedSeconds;
            combat.knockbackVelocity = {};
            combat.staggerRemainingSeconds = 0.0f;
            if (saved.dead) {
                combat.deathAnimationRequested = false;
                combat.hurtAnimationRequested = false;
                DeactivateNpcNavigation(
                        world,
                        scene.Navigation(),
                        scene.NpcNavigation(),
                        entity);
            }
        }
        if (world.Has<SectorDynamicModel>(entity)) {
            world.Get<SectorDynamicModel>(entity).opacity = saved.opacity;
        }
        if (saved.hasAnimator) {
            animatorRestored = ApplyAnimator(
                    world, assets, entity, saved.animator);
        }
        if (saved.dead && world.Has<NpcCombatState>(entity)) {
            NpcCombatState& combat = world.Get<NpcCombatState>(entity);
            combat.deathAnimationRequested = !animatorRestored;
        }
        if (saved.dead && animatorRestored
                && world.Has<NpcAnimationState>(entity)
                && world.Has<NpcCombatState>(entity)) {
            NpcAnimationState& animation = world.Get<NpcAnimationState>(entity);
            animation.appliedAction = NpcAction::Death;
            animation.pendingAction = NpcAction::Death;
            animation.hasPendingAction = false;
            if (saved.animator.finished) {
                world.Get<NpcCombatState>(entity).deathAnimationComplete = true;
            }
        }
        ResetNpcAwareness(world, entity);
        if (saved.hasPatrol && world.Has<NpcPatrolState>(entity)) {
            NpcPatrolState& patrol = world.Get<NpcPatrolState>(entity);
            if (patrol.patrolEditorId == saved.patrolEditorId) {
                patrol.waypointIndex = saved.waypointIndex;
                patrol.direction = saved.direction;
                patrol.shuffleOrder = saved.shuffleOrder;
                patrol.shuffleCursor = saved.shuffleCursor;
                patrol.randomState = saved.randomState;
                const int maximumPhase = static_cast<int>(NpcPatrolPhase::StoppedByScript);
                patrol.phase = static_cast<NpcPatrolPhase>(
                        std::clamp(saved.phase, 0, maximumPhase));
                patrol.resumePhase = static_cast<NpcPatrolPhase>(
                        std::clamp(saved.resumePhase, 0, maximumPhase));
                if (patrol.phase == NpcPatrolPhase::SuspendedAi
                        || patrol.phase == NpcPatrolPhase::SuspendedScript) {
                    patrol.phase = patrol.resumePhase;
                }
                patrol.waitRemainingSeconds = saved.waitRemainingSeconds;
                patrol.waypointBaseYawRadians = saved.waypointBaseYawRadians;
                patrol.lookOffsetRadians = saved.lookOffsetRadians;
                patrol.lookDirection = saved.lookDirection;
                patrol.retryRemainingSeconds = saved.retryRemainingSeconds;
                patrol.destinationXZ = saved.destinationXZ;
                patrol.stoppedByScript = saved.stoppedByScript;
                patrol.destinationInitialized = saved.destinationInitialized;
                patrol.requestId = 0;
                patrol.scriptOverrideActive = false;
            }
        }
    }
    if (removedNpc) world.FlushDestroyedEntities();
    for (const GameSaveBillboardState& saved : state.billboards) {
        const engine::Entity entity = FindRuntimeEntity(runtimeObjects, saved.placedObjectId);
        if (!world.IsAlive(entity) || !world.Has<SectorBillboardAnimator>(entity)) continue;
        SectorBillboardAnimator& animator = world.Get<SectorBillboardAnimator>(entity);
        animator.timeSeconds = saved.timeSeconds;
        animator.speed = saved.speed;
        animator.playing = saved.playing;
        animator.loop = saved.loop;
        animator.finished = saved.finished;
    }
    if (scriptHost.map != nullptr) {
        for (SectorScriptTriggerState& trigger : scriptHost.triggers) {
            if (trigger.triggerIndex >= scriptHost.map->triggers.size()) continue;
            const std::string& id = scriptHost.map->triggers[trigger.triggerIndex].id;
            const auto found = std::find_if(
                    state.triggers.begin(), state.triggers.end(),
                    [&id](const GameSaveTriggerState& value) { return value.id == id; });
            if (found == state.triggers.end()) continue;
            trigger.enabled = found->enabled;
            trigger.inside = found->inside;
            trigger.pending = found->pending;
            trigger.consumed = found->consumed;
            trigger.remainingDelayMilliseconds = found->remainingDelayMilliseconds;
        }
    }
    RefreshSectorDoorSpatialCaches(world, runtimeObjects);
}

} // namespace game
