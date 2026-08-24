#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

#include <cstdint>
#include <vector>

namespace engine {

static constexpr uint32_t InvalidModelAnimationIndex = UINT32_MAX;
static constexpr int MaxAnimatedModelBones = 128;
static constexpr float GltfAnimationFramesPerSecond = 60.0f;

enum class AnimatedModelPoseSource : uint8_t {
    RaylibSkeletal,
    GltfScene
};

// Per-instance mutable pose storage. The ModelAsset retains ownership of the
// shared meshes, materials, skeleton, and animation clips.
struct AnimatedModelInstance {
    ModelHandle model;
    std::vector<::Transform> currentPose;
    std::vector<Matrix> boneMatrices;
    std::vector<::Transform> nodeLocalTransforms;
    std::vector<Matrix> nodeLocalMatrices;
    std::vector<Matrix> nodeWorldMatrices;
    std::vector<Matrix> meshNodeMatrices;
    std::vector<Matrix> meshBoneMatrices;
    AnimatedModelPoseSource poseSource =
            AnimatedModelPoseSource::RaylibSkeletal;
    bool poseReady = false;
    bool poseFailed = false;
};

struct AnimatedModelAnimator {
    uint32_t animationIndex = InvalidModelAnimationIndex;
    uint32_t nodeAnimationIndex = InvalidModelAnimationIndex;
    float frame = 0.0f;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;
    bool reverse = false;
    bool finished = false;
    bool poseDirty = true;

    uint32_t targetAnimationIndex = InvalidModelAnimationIndex;
    float targetFrame = 0.0f;
    bool targetLoop = true;
    bool targetFinished = false;
    float transitionDurationSeconds = 0.0f;
    float transitionElapsedSeconds = 0.0f;
};

inline bool IsAnimatedModelTransitioning(const AnimatedModelAnimator& animator)
{
    return animator.targetAnimationIndex != InvalidModelAnimationIndex;
}

} // namespace engine
