#include "game/npc/NpcHeadLookSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/npc/NpcLineOfSight.h"
#include "game/npc/NpcRuntime.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace game {
namespace {

constexpr float AngleEpsilonRadians = 0.00001f;

float WrapRadians(float radians)
{
    radians = std::fmod(radians + PI, 2.0f * PI);
    if (radians < 0.0f) radians += 2.0f * PI;
    return radians - PI;
}

Matrix PoseTransformMatrix(const Transform& transform)
{
    return MatrixMultiply(
            MatrixMultiply(
                    MatrixScale(
                            transform.scale.x,
                            transform.scale.y,
                            transform.scale.z),
                    QuaternionToMatrix(transform.rotation)),
            MatrixTranslate(
                    transform.translation.x,
                    transform.translation.y,
                    transform.translation.z));
}

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

bool ResolveHeadBone(
        NpcHeadLookState& state,
        const engine::ModelAsset& asset)
{
    state.boneResolutionAttempted = true;
    state.boneIndex = -1;
    state.affectedBones.fill(0);
    const int boneCount = asset.model.skeleton.boneCount;
    const BoneInfo* bones = asset.model.skeleton.bones;
    if (bones == nullptr || boneCount <= 0
            || boneCount > engine::MaxAnimatedModelBones) {
        return false;
    }
    for (int index = 0; index < boneCount; ++index) {
        if (std::strncmp(
                    bones[index].name,
                    state.boneName.c_str(),
                    sizeof(bones[index].name)) == 0) {
            state.boneIndex = index;
            break;
        }
    }
    if (state.boneIndex < 0) return false;
    for (int index = 0; index < boneCount; ++index) {
        state.affectedBones[static_cast<size_t>(index)] =
                IsBoneDescendantOf(
                        bones, boneCount, index, state.boneIndex)
                ? 1u : 0u;
    }
    return true;
}

bool HasValidSkeletalPose(
        const engine::ModelAsset& asset,
        const engine::AnimatedModelInstance& instance,
        const engine::AnimatedModelAnimator& animator)
{
    const int boneCount = asset.model.skeleton.boneCount;
    return instance.poseSource == engine::AnimatedModelPoseSource::RaylibSkeletal
            && instance.poseReady
            && !instance.poseFailed
            && boneCount > 0
            && boneCount <= engine::MaxAnimatedModelBones
            && instance.currentPose.size() == static_cast<size_t>(boneCount)
            && instance.boneMatrices.size() == static_cast<size_t>(boneCount)
            && asset.model.skeleton.bindPose != nullptr
            && asset.model.skeleton.bones != nullptr
            && animator.animationIndex != engine::InvalidModelAnimationIndex
            && asset.animations != nullptr
            && animator.animationIndex
                    < static_cast<uint32_t>(std::max(0, asset.animationCount))
            && IsModelAnimationValid(
                    asset.model,
                    asset.animations[animator.animationIndex]);
}

void ApplyHeadRotation(
        const engine::ModelAsset& asset,
        engine::AnimatedModelInstance& instance,
        const NpcHeadLookState& state)
{
    if (state.boneIndex < 0
            || (std::fabs(state.currentYawRadians) <= AngleEpsilonRadians
                    && std::fabs(state.currentPitchRadians)
                            <= AngleEpsilonRadians)) {
        return;
    }
    const int boneCount = asset.model.skeleton.boneCount;
    const Vector3 pivot = instance.currentPose[
            static_cast<size_t>(state.boneIndex)].translation;
    const Quaternion delta = QuaternionNormalize(QuaternionFromEuler(
            -state.currentPitchRadians,
            state.currentYawRadians,
            0.0f));
    for (int index = 0; index < boneCount; ++index) {
        if (state.affectedBones[static_cast<size_t>(index)] == 0) continue;
        Transform& pose = instance.currentPose[static_cast<size_t>(index)];
        pose.translation = Vector3Add(
                pivot,
                Vector3RotateByQuaternion(
                        Vector3Subtract(pose.translation, pivot), delta));
        pose.rotation = QuaternionNormalize(
                QuaternionMultiply(delta, pose.rotation));
        instance.boneMatrices[static_cast<size_t>(index)] = MatrixMultiply(
                MatrixInvert(PoseTransformMatrix(
                        asset.model.skeleton.bindPose[index])),
                PoseTransformMatrix(pose));
    }
}

NpcHeadLookTargetAngles EvaluateTargetAngles(
        Vector3 npcPosition,
        float npcYawRadians,
        Vector3 headWorldPosition,
        Vector3 playerEyePosition,
        bool enabled,
        float rangeWorld,
        float maxYawDegrees,
        float maxPitchDegrees,
        bool hasLineOfSight)
{
    NpcHeadLookTargetAngles result;
    if (!enabled || !hasLineOfSight) return result;
    const Vector3 fromNpc = Vector3Subtract(playerEyePosition, npcPosition);
    const float horizontalDistance = std::sqrt(
            fromNpc.x * fromNpc.x + fromNpc.z * fromNpc.z);
    if (horizontalDistance > rangeWorld) return result;

    const Vector3 fromHead = Vector3Subtract(
            playerEyePosition, headWorldPosition);
    const float headHorizontalDistance = std::sqrt(
            fromHead.x * fromHead.x + fromHead.z * fromHead.z);
    const float targetYaw = WrapRadians(
            std::atan2(fromHead.x, fromHead.z) - npcYawRadians);
    const float targetPitch = std::atan2(
            fromHead.y, std::max(headHorizontalDistance, 0.000001f));
    const float maxYaw = maxYawDegrees * DEG2RAD;
    const float maxPitch = maxPitchDegrees * DEG2RAD;
    if (std::fabs(targetYaw) > maxYaw
            || std::fabs(targetPitch) > maxPitch) {
        return result;
    }
    result.yawRadians = std::clamp(targetYaw, -maxYaw, maxYaw);
    result.pitchRadians = std::clamp(targetPitch, -maxPitch, maxPitch);
    result.active = true;
    return result;
}

} // namespace

NpcHeadLookTargetAngles EvaluateNpcHeadLookTargetAngles(
        Vector3 npcPosition,
        float npcYawRadians,
        Vector3 headWorldPosition,
        Vector3 playerEyePosition,
        const NpcHeadLookDefinition& definition,
        bool hasLineOfSight)
{
    return EvaluateTargetAngles(
            npcPosition,
            npcYawRadians,
            headWorldPosition,
            playerEyePosition,
            definition.enabled,
            definition.rangeWorld,
            definition.maxYawDegrees,
            definition.maxPitchDegrees,
            hasLineOfSight);
}

float MoveNpcHeadLookAngleToward(
        float currentRadians,
        float targetRadians,
        float deltaSeconds)
{
    const float maximumStep = kNpcHeadLookTurnSpeedDegreesPerSecond
            * DEG2RAD * std::max(0.0f, deltaSeconds);
    const float delta = targetRadians - currentRadians;
    if (std::fabs(delta) <= maximumStep) return targetRadians;
    return currentRadians + std::copysign(maximumStep, delta);
}

void UpdateNpcHeadLookSystem(
        engine::World& world,
        engine::AssetManager& assets,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const Vector3* playerEyePosition,
        float deltaSeconds)
{
    world.ForEach<NpcRuntimeInstance, NpcHeadLookState, NpcCombatState,
            SectorObjectTransform, SectorDynamicModel,
            engine::AnimatedModelInstance, engine::AnimatedModelAnimator>(
            [&world, &assets, &collisionWorld, &doorColliders, &staticColliders,
             playerEyePosition, deltaSeconds](
                    engine::Entity entity,
                    NpcRuntimeInstance& npc,
                    NpcHeadLookState& state,
                    NpcCombatState& combat,
                    SectorObjectTransform& transform,
                    SectorDynamicModel& dynamicModel,
                    engine::AnimatedModelInstance& instance,
                    engine::AnimatedModelAnimator& animator) {
                const engine::ModelAsset* asset =
                        assets.GetModelAsset(instance.model);
                if (asset == nullptr
                        || !HasValidSkeletalPose(*asset, instance, animator)) {
                    return;
                }
                if (!state.boneResolutionAttempted
                        && !ResolveHeadBone(state, *asset)
                        && !state.warningPrinted) {
                    std::fprintf(
                            stderr,
                            "[NPC Head Look WARNING] Bone '%s' was not found for NPC '%s'.\n",
                            state.boneName.c_str(), npc.definitionId.c_str());
                    state.warningPrinted = true;
                }
                if (state.boneIndex < 0) return;

                Vector3 renderPosition = transform.position;
                if (world.Has<SectorObjectVisualOffset>(entity)) {
                    renderPosition = Vector3Add(
                            renderPosition,
                            world.Get<SectorObjectVisualOffset>(entity).position);
                }
                const Matrix authoredTransform =
                        BuildSectorStaticModelAuthoredTransform(
                                renderPosition,
                                transform.rotationXRadians,
                                transform.yawRadians,
                                transform.rotationZRadians,
                                dynamicModel.scale);
                const Matrix modelWorld = MatrixMultiply(
                        asset->model.transform, authoredTransform);
                const Vector3 headWorldPosition = Vector3Transform(
                        instance.currentPose[
                                static_cast<size_t>(state.boneIndex)]
                                .translation,
                        modelWorld);

                NpcHeadLookTargetAngles target;
                if (!npc.hostile && !combat.dead
                        && playerEyePosition != nullptr) {
                    target = EvaluateTargetAngles(
                            transform.position,
                            transform.yawRadians,
                            headWorldPosition,
                            *playerEyePosition,
                            true,
                            state.rangeWorld,
                            state.maxYawDegrees,
                            state.maxPitchDegrees,
                            true);
                    if (target.active
                            && !HasNpcLineOfSight(
                                    collisionWorld,
                                    doorColliders,
                                    staticColliders,
                                    headWorldPosition,
                                    *playerEyePosition)) {
                        target = {};
                    }
                }
                state.currentYawRadians = MoveNpcHeadLookAngleToward(
                        state.currentYawRadians,
                        target.active ? target.yawRadians : 0.0f,
                        deltaSeconds);
                state.currentPitchRadians = MoveNpcHeadLookAngleToward(
                        state.currentPitchRadians,
                        target.active ? target.pitchRadians : 0.0f,
                        deltaSeconds);
                ApplyHeadRotation(*asset, instance, state);
                if (std::fabs(state.currentYawRadians)
                                > AngleEpsilonRadians
                        || std::fabs(state.currentPitchRadians)
                                > AngleEpsilonRadians) {
                    animator.poseDirty = true;
                }
            });
}

} // namespace game
