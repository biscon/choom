#include "engine/systems/AnimatedModelSystem.h"

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace engine {
namespace {

bool ValidAnimationIndex(const ModelAsset& asset, uint32_t index)
{
    return asset.animations != nullptr
            && index < static_cast<uint32_t>(std::max(0, asset.animationCount));
}

bool ValidNodeAnimationIndex(const ModelAsset& asset, uint32_t index)
{
    return index < asset.nodeAnimationClips.size()
            && !asset.nodeAnimationClips[index].channels.empty();
}

bool AdvanceFrame(
        float& frame,
        bool& playing,
        bool& finished,
        bool loop,
        float speed,
        int keyframeCount,
        float dt)
{
    if (!playing || finished || speed <= 0.0f || dt <= 0.0f || keyframeCount <= 0) {
        return false;
    }

    frame += dt * GltfAnimationFramesPerSecond * speed;
    const float frameCount = static_cast<float>(keyframeCount);
    if (loop) {
        frame = std::fmod(frame, frameCount);
        if (frame < 0.0f) {
            frame += frameCount;
        }
    } else if (frame >= frameCount - 1.0f) {
        frame = std::max(0.0f, frameCount - 1.0f);
        finished = true;
        playing = false;
    }
    return true;
}

} // namespace

uint32_t FindModelAnimationIndex(const ModelAsset& asset, const char* name)
{
    if (name == nullptr || name[0] == '\0' || asset.animations == nullptr) {
        return InvalidModelAnimationIndex;
    }
    for (int i = 0; i < asset.animationCount; ++i) {
        if (std::strncmp(asset.animations[i].name, name, sizeof(asset.animations[i].name)) == 0) {
            return static_cast<uint32_t>(i);
        }
    }
    return InvalidModelAnimationIndex;
}

void SetAnimatedModelAnimation(
        AnimatedModelAnimator& animator,
        uint32_t animationIndex,
        float blendDurationSeconds,
        bool restart)
{
    animator.nodeAnimationIndex = InvalidModelAnimationIndex;
    if (!restart
            && animator.animationIndex == animationIndex
            && !IsAnimatedModelTransitioning(animator)) {
        return;
    }

    if (blendDurationSeconds > 0.0f
            && animator.animationIndex != InvalidModelAnimationIndex
            && animator.animationIndex != animationIndex) {
        animator.targetAnimationIndex = animationIndex;
        animator.targetFrame = 0.0f;
        animator.targetLoop = animator.loop;
        animator.targetFinished = false;
        animator.transitionDurationSeconds = blendDurationSeconds;
        animator.transitionElapsedSeconds = 0.0f;
    } else {
        const bool changed = animator.animationIndex != animationIndex;
        animator.animationIndex = animationIndex;
        if (restart || changed) {
            animator.frame = 0.0f;
        }
        animator.targetAnimationIndex = InvalidModelAnimationIndex;
        animator.targetFrame = 0.0f;
        animator.targetLoop = true;
        animator.targetFinished = false;
        animator.transitionDurationSeconds = 0.0f;
        animator.transitionElapsedSeconds = 0.0f;
    }
    animator.finished = false;
    animator.playing = true;
    animator.poseDirty = true;
}

bool SetAnimatedModelClip(
        AnimatedModelAnimator& animator,
        const ModelAsset& asset,
        uint32_t clipIndex,
        bool restart)
{
    if (clipIndex >= ModelAnimationClipCount(asset)) return false;
    uint32_t skeletalIndex = InvalidModelAnimationIndex;
    uint32_t nodeIndex = InvalidModelAnimationIndex;
    if (!asset.nodeAnimationClips.empty()) {
        const ModelNodeAnimationClip& clip =
                asset.nodeAnimationClips[clipIndex];
        if (clip.skeletalAnimationIndex >= 0
                && clip.skeletalAnimationIndex < asset.animationCount) {
            skeletalIndex = static_cast<uint32_t>(
                    clip.skeletalAnimationIndex);
        }
        if (!clip.channels.empty()) nodeIndex = clipIndex;
    } else if (clipIndex
            < static_cast<uint32_t>(
                    std::max(0, asset.animationCount))) {
        skeletalIndex = clipIndex;
    }
    if (skeletalIndex == InvalidModelAnimationIndex
            && nodeIndex == InvalidModelAnimationIndex) {
        return false;
    }
    SetAnimatedModelAnimation(
            animator,
            skeletalIndex,
            0.0f,
            restart);
    animator.nodeAnimationIndex = nodeIndex;
    animator.poseDirty = true;
    return true;
}

bool SetAnimatedModelAnimationByName(
        AnimatedModelAnimator& animator,
        const ModelAsset& asset,
        const char* name,
        float blendDurationSeconds,
        bool restart)
{
    const uint32_t index = FindModelAnimationIndex(asset, name);
    if (index == InvalidModelAnimationIndex) {
        return false;
    }
    SetAnimatedModelAnimation(animator, index, blendDurationSeconds, restart);
    return true;
}

bool AdvanceAnimatedModelAnimator(
        AnimatedModelAnimator& animator,
        int keyframeCount,
        float dt)
{
    const bool poseChanged = AdvanceFrame(
            animator.frame,
            animator.playing,
            animator.finished,
            animator.loop,
            animator.speed,
            keyframeCount,
            dt);
    if (poseChanged) {
        animator.poseDirty = true;
    }
    return poseChanged;
}

void PrepareAnimatedModelInstancesSystem(World& world, AssetManager& assets)
{
    world.ForEach<AnimatedModelInstance>(
            [&assets](Entity, AnimatedModelInstance& instance) {
                if (instance.poseReady || instance.poseFailed) {
                    return;
                }
                const ModelAsset* asset = assets.GetModelAsset(instance.model);
                if (asset == nullptr) {
                    if (assets.HasFailed(instance.model)) {
                        instance.poseFailed = true;
                    }
                    return;
                }

                PrepareAnimatedModelInstance(instance, *asset);
            });
}

bool PrepareAnimatedModelInstance(AnimatedModelInstance& instance, const ModelAsset& asset)
{
    const int boneCount = asset.model.skeleton.boneCount;
    if (boneCount > MaxAnimatedModelBones
            || (boneCount > 0
                    && asset.model.skeleton.bindPose == nullptr)) {
        std::fprintf(stderr, "[AnimatedModel WARNING] Model has %d bones; GPU skinning supports at most %d.\n",
                boneCount, MaxAnimatedModelBones);
        instance.poseFailed = true;
        return false;
    }
    if (boneCount > 0) {
        instance.currentPose.assign(
                asset.model.skeleton.bindPose,
                asset.model.skeleton.bindPose + boneCount);
        instance.boneMatrices.assign(
                static_cast<size_t>(boneCount),
                MatrixIdentity());
    }
    instance.nodeLocalTransforms.resize(asset.nodes.size());
    instance.nodeLocalMatrices.resize(asset.nodes.size());
    instance.nodeWorldMatrices.resize(asset.nodes.size());
    instance.meshNodeMatrices.assign(
            static_cast<size_t>(std::max(0, asset.model.meshCount)),
            MatrixIdentity());
    instance.poseReady = true;
    return true;
}

Model BuildAnimatedModelPoseView(
        const ModelAsset& asset,
        AnimatedModelInstance& instance)
{
    Model model = asset.model;
    if (!instance.currentPose.empty()) {
        model.currentPose = instance.currentPose.data();
    }
    if (!instance.boneMatrices.empty()) {
        model.boneMatrices = instance.boneMatrices.data();
    }
    return model;
}

void AnimatedModelSystem(World& world, AssetManager& assets, float dt)
{
    world.ForEach<AnimatedModelInstance, AnimatedModelAnimator>(
            [&assets, dt](
                    Entity,
                    AnimatedModelInstance& instance,
                    AnimatedModelAnimator& animator) {
                if (!instance.poseReady || instance.poseFailed) {
                    return;
                }
                const ModelAsset* asset = assets.GetModelAsset(instance.model);
                if (asset == nullptr) {
                    return;
                }
                const bool skeletalValid =
                        ValidAnimationIndex(*asset, animator.animationIndex)
                        && IsModelAnimationValid(
                                asset->model,
                                asset->animations[animator.animationIndex]);
                const bool nodeValid = ValidNodeAnimationIndex(
                        *asset,
                        animator.nodeAnimationIndex);
                if (!skeletalValid && !nodeValid) return;

                const int keyframeCount = nodeValid
                        ? asset->nodeAnimationClips[
                                  animator.nodeAnimationIndex]
                                  .keyframeCount
                        : asset->animations[animator.animationIndex]
                                  .keyframeCount;
                AdvanceAnimatedModelAnimator(
                        animator, keyframeCount, dt);
                const bool applyPose = animator.playing
                        || animator.poseDirty;

                Model poseModel = BuildAnimatedModelPoseView(*asset, instance);
                if (skeletalValid
                        && ValidAnimationIndex(
                                *asset,
                                animator.targetAnimationIndex)) {
                    const ModelAnimation& source =
                            asset->animations[animator.animationIndex];
                    const ModelAnimation& target = asset->animations[animator.targetAnimationIndex];
                    if (IsModelAnimationValid(asset->model, target)) {
                        bool targetPlaying = !animator.targetFinished;
                        AdvanceFrame(
                                animator.targetFrame,
                                targetPlaying,
                                animator.targetFinished,
                                animator.targetLoop,
                                animator.speed,
                                target.keyframeCount,
                                dt);
                        animator.transitionElapsedSeconds += std::max(0.0f, dt);
                        const float blend = animator.transitionDurationSeconds > 0.0f
                                ? std::clamp(
                                        animator.transitionElapsedSeconds
                                                / animator.transitionDurationSeconds,
                                        0.0f,
                                        1.0f)
                                : 1.0f;
                        UpdateModelAnimationEx(
                                poseModel,
                                source,
                                animator.frame,
                                target,
                                animator.targetFrame,
                                blend);
                        if (blend >= 1.0f) {
                            animator.animationIndex = animator.targetAnimationIndex;
                            animator.frame = animator.targetFrame;
                            animator.loop = animator.targetLoop;
                            animator.finished = animator.targetFinished;
                            animator.playing = !animator.targetFinished;
                            animator.targetAnimationIndex = InvalidModelAnimationIndex;
                            animator.targetLoop = true;
                            animator.targetFinished = false;
                            animator.transitionDurationSeconds = 0.0f;
                            animator.transitionElapsedSeconds = 0.0f;
                        }
                        animator.poseDirty = false;
                        return;
                    }
                    animator.targetAnimationIndex = InvalidModelAnimationIndex;
                    animator.targetLoop = true;
                    animator.targetFinished = false;
                }

                if (applyPose && skeletalValid) {
                    const ModelAnimation& source =
                            asset->animations[animator.animationIndex];
                    UpdateModelAnimationEx(
                            poseModel,
                            source,
                            animator.frame,
                            source,
                            animator.frame,
                            0.0f);
                }
                if (applyPose && nodeValid) {
                    if (!SampleModelNodeAnimation(
                                *asset,
                                animator.nodeAnimationIndex,
                                animator.frame
                                        / GltfAnimationFramesPerSecond,
                                instance.nodeLocalTransforms,
                                instance.nodeLocalMatrices,
                                instance.nodeWorldMatrices,
                                instance.meshNodeMatrices)) {
                        std::fprintf(
                                stderr,
                                "[AnimatedModel WARNING] Rigid node animation pose storage is invalid; disabling the rigid clip.\n");
                        animator.nodeAnimationIndex =
                                InvalidModelAnimationIndex;
                        std::fill(
                                instance.meshNodeMatrices.begin(),
                                instance.meshNodeMatrices.end(),
                                MatrixIdentity());
                    }
                }
                animator.poseDirty = false;
            });
}

} // namespace engine
