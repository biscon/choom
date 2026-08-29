#include "game/npc/NpcBoneImpactSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/ModelAssets.h"
#include "engine/components/AnimatedModel.h"
#include "engine/ecs/World.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "game/npc/NpcRuntime.h"

#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace game {
namespace {

constexpr float InfluenceEpsilon = 0.00001f;
constexpr float RotationEpsilonRadians = 0.000001f;
constexpr float VelocityEpsilonRadiansPerSecond = 0.00001f;

bool Finite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

Vector3 TransformDirection(Vector3 value, Matrix transform)
{
    return Vector3{
            transform.m0 * value.x + transform.m4 * value.y
                    + transform.m8 * value.z,
            transform.m1 * value.x + transform.m5 * value.y
                    + transform.m9 * value.z,
            transform.m2 * value.x + transform.m6 * value.y
                    + transform.m10 * value.z};
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

void ClearMotion(NpcBoneImpactState& state)
{
    state.angularOffsets.fill({});
    state.angularVelocities.fill({});
    state.activeBones.fill(0);
    state.hasActiveMotion = false;
}

bool ResolveSkeleton(
        NpcBoneImpactState& state,
        const engine::ModelAsset& asset,
        engine::ModelHandle model)
{
    if (state.resolvedModel != model) {
        ClearMotion(state);
        state.resolvedModel = model;
        state.skeletonResolutionAttempted = false;
        state.skeletonValid = false;
        state.warningPrinted = false;
        state.classificationWarningPrinted = false;
        state.resolvedBoneCount = 0;
    }
    if (state.skeletonResolutionAttempted) return state.skeletonValid;
    state.skeletonResolutionAttempted = true;

    const int boneCount = asset.model.skeleton.boneCount;
    const BoneInfo* bones = asset.model.skeleton.bones;
    if (bones == nullptr || asset.model.skeleton.bindPose == nullptr
            || boneCount <= 0
            || boneCount > engine::MaxAnimatedModelBones) {
        return false;
    }

    std::array<int, engine::MaxAnimatedModelBones> depths{};
    for (int boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
        int current = boneIndex;
        int depth = 0;
        while (current >= 0 && depth < boneCount) {
            if (current >= boneCount) return false;
            current = bones[current].parent;
            ++depth;
        }
        if (current >= 0) return false;
        depths[static_cast<size_t>(boneIndex)] = depth;

        int insertAt = boneIndex;
        while (insertAt > 0) {
            const int previous = state.boneOrder[
                    static_cast<size_t>(insertAt - 1)];
            if (depths[static_cast<size_t>(previous)] <= depth) break;
            state.boneOrder[static_cast<size_t>(insertAt)] =
                    static_cast<uint8_t>(previous);
            --insertAt;
        }
        state.boneOrder[static_cast<size_t>(insertAt)] =
                static_cast<uint8_t>(boneIndex);
    }
    state.resolvedBoneCount = boneCount;
    state.skeletonValid = true;
    return true;
}

void AdvanceSpringAxis(
        float& offset,
        float& velocity,
        float frequencyHz,
        float dampingRatio,
        float deltaSeconds)
{
    const float angularFrequency = 2.0f * PI * frequencyHz;
    float remaining = deltaSeconds;
    while (remaining > 0.0f) {
        const float step = std::min(remaining, 1.0f / 240.0f);
        const float acceleration = -angularFrequency * angularFrequency * offset
                - 2.0f * dampingRatio * angularFrequency * velocity;
        velocity += acceleration * step;
        offset += velocity * step;
        remaining -= step;
    }
}

bool HasUsableAnimatedPose(
        const engine::ModelAsset& asset,
        const engine::AnimatedModelInstance& instance,
        const engine::AnimatedModelAnimator& animator,
        int boneCount)
{
    return instance.poseSource
                    == engine::AnimatedModelPoseSource::RaylibSkeletal
            && instance.currentPose.size() == static_cast<size_t>(boneCount)
            && instance.boneMatrices.size() == static_cast<size_t>(boneCount)
            && animator.animationIndex != engine::InvalidModelAnimationIndex
            && asset.animations != nullptr
            && animator.animationIndex
                    < static_cast<uint32_t>(std::max(0, asset.animationCount))
            && IsModelAnimationValid(
                    asset.model,
                    asset.animations[animator.animationIndex]);
}

} // namespace

int FindNpcBoneImpactDominantBone(
        const engine::ModelAsset& asset,
        const engine::AnimatedModelSurfaceAnchor& anchor,
        float* outNormalizedInfluence)
{
    if (outNormalizedInfluence != nullptr) *outNormalizedInfluence = 0.0f;
    const int boneCount = asset.model.skeleton.boneCount;
    if (!anchor.valid
            || asset.model.skeleton.bones == nullptr
            || boneCount <= 0
            || boneCount > engine::MaxAnimatedModelBones
            || anchor.meshIndex >= static_cast<uint32_t>(asset.model.meshCount)
            || asset.model.meshes == nullptr) {
        return -1;
    }
    const Mesh& mesh = asset.model.meshes[anchor.meshIndex];
    if (mesh.boneIndices == nullptr || mesh.boneWeights == nullptr) return -1;

    const float barycentric[3] = {
            anchor.barycentric.x,
            anchor.barycentric.y,
            anchor.barycentric.z};
    std::array<float, engine::MaxAnimatedModelBones> influences{};
    float totalInfluence = 0.0f;
    for (size_t corner = 0; corner < anchor.vertexIndices.size(); ++corner) {
        const uint32_t vertexIndex = anchor.vertexIndices[corner];
        if (vertexIndex >= static_cast<uint32_t>(mesh.vertexCount)
                || !std::isfinite(barycentric[corner])
                || barycentric[corner] < 0.0f) {
            return -1;
        }
        for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex) {
            const size_t sourceIndex = static_cast<size_t>(vertexIndex) * 4u
                    + static_cast<size_t>(influenceIndex);
            const float weight = mesh.boneWeights[sourceIndex];
            if (!std::isfinite(weight) || weight < 0.0f) return -1;
            if (weight <= 0.0f || barycentric[corner] <= 0.0f) continue;
            const uint32_t boneIndex = mesh.boneIndices[sourceIndex];
            if (boneIndex >= static_cast<uint32_t>(boneCount)) return -1;
            const float contribution = barycentric[corner] * weight;
            influences[boneIndex] += contribution;
            totalInfluence += contribution;
        }
    }
    if (!std::isfinite(totalInfluence) || totalInfluence <= InfluenceEpsilon) {
        return -1;
    }

    int bestBone = -1;
    float bestInfluence = -1.0f;
    for (int boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
        const float influence = influences[static_cast<size_t>(boneIndex)];
        if (influence > bestInfluence + InfluenceEpsilon) {
            bestBone = boneIndex;
            bestInfluence = influence;
        }
    }
    if (bestBone >= 0 && outNormalizedInfluence != nullptr) {
        *outNormalizedInfluence = bestInfluence / totalInfluence;
    }
    return bestBone;
}

bool AddNpcBoneImpactImpulse(
        NpcBoneImpactState& state,
        const engine::ModelAsset& asset,
        const engine::AnimatedModelInstance& instance,
        Matrix authoredTransform,
        int boneIndex,
        Vector3 impactPositionWorld,
        Vector3 bulletDirectionWorld)
{
    if (!ResolveSkeleton(state, asset, instance.model)
            || instance.poseSource
                    != engine::AnimatedModelPoseSource::RaylibSkeletal
            || instance.currentPose.size()
                    != static_cast<size_t>(state.resolvedBoneCount)
            || boneIndex < 0 || boneIndex >= state.resolvedBoneCount
            || !Finite(impactPositionWorld)
            || !Finite(bulletDirectionWorld)) {
        return false;
    }
    const float directionLength = Vector3Length(bulletDirectionWorld);
    if (directionLength <= InfluenceEpsilon) return false;
    const Vector3 direction = Vector3Scale(
            bulletDirectionWorld, 1.0f / directionLength);
    const Matrix modelWorld = MatrixMultiply(
            asset.model.transform, authoredTransform);
    const Vector3 pivotWorld = Vector3Transform(
            instance.currentPose[static_cast<size_t>(boneIndex)].translation,
            modelWorld);
    const Vector3 lever = Vector3Subtract(impactPositionWorld, pivotWorld);
    Vector3 axisWorld = Vector3CrossProduct(lever, direction);
    if (Vector3Length(axisWorld) <= InfluenceEpsilon) {
        axisWorld = Vector3CrossProduct(Vector3{0.0f, 1.0f, 0.0f}, direction);
    }
    if (Vector3Length(axisWorld) <= InfluenceEpsilon) {
        axisWorld = Vector3CrossProduct(Vector3{1.0f, 0.0f, 0.0f}, direction);
    }
    const float axisLength = Vector3Length(axisWorld);
    if (axisLength <= InfluenceEpsilon) return false;
    axisWorld = Vector3Scale(axisWorld, 1.0f / axisLength);

    Vector3 axisModel = TransformDirection(
            axisWorld, MatrixInvert(modelWorld));
    const float axisModelLength = Vector3Length(axisModel);
    if (!Finite(axisModel) || axisModelLength <= InfluenceEpsilon) return false;
    axisModel = Vector3Scale(axisModel, 1.0f / axisModelLength);

    const float impulse = std::clamp(
            state.impulseDegreesPerSecond,
            0.0f,
            kMaximumNpcBoneImpactImpulseDegreesPerSecond) * DEG2RAD;
    if (impulse <= 0.0f) return false;
    Vector3& velocity = state.angularVelocities[
            static_cast<size_t>(boneIndex)];
    velocity = Vector3Add(velocity, Vector3Scale(axisModel, impulse));
    const float maximumVelocity =
            kMaximumNpcBoneImpactImpulseDegreesPerSecond * DEG2RAD;
    const float velocityLength = Vector3Length(velocity);
    if (velocityLength > maximumVelocity) {
        velocity = Vector3Scale(velocity, maximumVelocity / velocityLength);
    }
    state.activeBones[static_cast<size_t>(boneIndex)] = 1;
    state.hasActiveMotion = true;
    return true;
}

bool AdvanceNpcBoneImpactSpring(
        NpcBoneImpactState& state,
        float deltaSeconds)
{
    if (!state.hasActiveMotion) return false;
    const float dt = std::isfinite(deltaSeconds)
            ? std::clamp(deltaSeconds, 0.0f, 0.25f)
            : 0.0f;
    const float frequency = std::clamp(
            state.springFrequencyHz,
            kMinimumNpcBoneImpactSpringFrequencyHz,
            kMaximumNpcBoneImpactSpringFrequencyHz);
    const float damping = std::clamp(
            state.springDampingRatio,
            kMinimumNpcBoneImpactSpringDampingRatio,
            kMaximumNpcBoneImpactSpringDampingRatio);
    const float maximumAngle = std::clamp(
            state.maxAngleDegrees,
            0.0f,
            kMaximumNpcBoneImpactAngleDegrees) * DEG2RAD;
    bool active = false;
    for (size_t boneIndex = 0; boneIndex < state.activeBones.size(); ++boneIndex) {
        if (state.activeBones[boneIndex] == 0) continue;
        Vector3& offset = state.angularOffsets[boneIndex];
        Vector3& velocity = state.angularVelocities[boneIndex];
        if (!Finite(offset) || !Finite(velocity)) {
            offset = {};
            velocity = {};
            state.activeBones[boneIndex] = 0;
            continue;
        }
        AdvanceSpringAxis(offset.x, velocity.x, frequency, damping, dt);
        AdvanceSpringAxis(offset.y, velocity.y, frequency, damping, dt);
        AdvanceSpringAxis(offset.z, velocity.z, frequency, damping, dt);

        const float offsetLength = Vector3Length(offset);
        if (maximumAngle <= 0.0f) {
            offset = {};
            velocity = {};
        } else if (offsetLength > maximumAngle) {
            const Vector3 outward = Vector3Scale(offset, 1.0f / offsetLength);
            offset = Vector3Scale(outward, maximumAngle);
            const float outwardVelocity = Vector3DotProduct(velocity, outward);
            if (outwardVelocity > 0.0f) {
                velocity = Vector3Subtract(
                        velocity,
                        Vector3Scale(outward, outwardVelocity));
            }
        }
        if (Vector3Length(offset) < RotationEpsilonRadians
                && Vector3Length(velocity)
                        < VelocityEpsilonRadiansPerSecond) {
            offset = {};
            velocity = {};
            state.activeBones[boneIndex] = 0;
            continue;
        }
        active = true;
    }
    state.hasActiveMotion = active;
    return active;
}

bool ApplyNpcBoneImpactPose(
        const engine::ModelAsset& asset,
        engine::AnimatedModelInstance& instance,
        NpcBoneImpactState& state)
{
    if (!ResolveSkeleton(state, asset, instance.model)
            || instance.poseSource
                    != engine::AnimatedModelPoseSource::RaylibSkeletal
            || instance.currentPose.size()
                    != static_cast<size_t>(state.resolvedBoneCount)
            || instance.boneMatrices.size()
                    != static_cast<size_t>(state.resolvedBoneCount)) {
        return false;
    }
    const BoneInfo* bones = asset.model.skeleton.bones;
    bool applied = false;
    for (int orderIndex = 0;
            orderIndex < state.resolvedBoneCount;
            ++orderIndex) {
        const int impactBone = state.boneOrder[
                static_cast<size_t>(orderIndex)];
        if (state.activeBones[static_cast<size_t>(impactBone)] == 0) continue;
        const Vector3 offset = state.angularOffsets[
                static_cast<size_t>(impactBone)];
        const float angle = Vector3Length(offset);
        if (!Finite(offset) || angle <= RotationEpsilonRadians) continue;
        const Quaternion delta = QuaternionNormalize(QuaternionFromAxisAngle(
                Vector3Scale(offset, 1.0f / angle), angle));
        const Vector3 pivot = instance.currentPose[
                static_cast<size_t>(impactBone)].translation;
        for (int boneIndex = 0;
                boneIndex < state.resolvedBoneCount;
                ++boneIndex) {
            if (!IsBoneDescendantOf(
                        bones,
                        state.resolvedBoneCount,
                        boneIndex,
                        impactBone)) {
                continue;
            }
            Transform& pose = instance.currentPose[
                    static_cast<size_t>(boneIndex)];
            pose.translation = Vector3Add(
                    pivot,
                    Vector3RotateByQuaternion(
                            Vector3Subtract(pose.translation, pivot), delta));
            pose.rotation = QuaternionNormalize(
                    QuaternionMultiply(delta, pose.rotation));
        }
        applied = true;
    }
    if (!applied) return false;
    for (int boneIndex = 0;
            boneIndex < state.resolvedBoneCount;
            ++boneIndex) {
        instance.boneMatrices[static_cast<size_t>(boneIndex)] = MatrixMultiply(
                MatrixInvert(PoseTransformMatrix(
                        asset.model.skeleton.bindPose[boneIndex])),
                PoseTransformMatrix(instance.currentPose[
                        static_cast<size_t>(boneIndex)]));
    }
    return true;
}

void UpdateNpcBoneImpactSystem(
        engine::World& world,
        engine::AssetManager& assets,
        float deltaSeconds)
{
    world.ForEach<NpcRuntimeInstance, NpcBoneImpactState,
            engine::AnimatedModelInstance, engine::AnimatedModelAnimator>(
            [&assets, deltaSeconds](
                    engine::Entity,
                    NpcRuntimeInstance& npc,
                    NpcBoneImpactState& state,
                    engine::AnimatedModelInstance& instance,
                    engine::AnimatedModelAnimator& animator) {
                const engine::ModelAsset* asset =
                        assets.GetModelAsset(instance.model);
                if (asset == nullptr || !instance.poseReady
                        || instance.poseFailed) {
                    return;
                }
                if (!ResolveSkeleton(state, *asset, instance.model)
                        || !HasUsableAnimatedPose(
                                *asset,
                                instance,
                                animator,
                                state.resolvedBoneCount)) {
                    if (!state.warningPrinted) {
                        std::fprintf(
                                stderr,
                                "[NPC Bone Impact WARNING] NPC '%s' has no usable animated raylib skeletal pose.\n",
                                npc.definitionId.c_str());
                        state.warningPrinted = true;
                    }
                    ClearMotion(state);
                    return;
                }
                if (!AdvanceNpcBoneImpactSpring(state, deltaSeconds)) return;
                if (ApplyNpcBoneImpactPose(*asset, instance, state)) {
                    animator.poseDirty = true;
                }
            });
}

} // namespace game
