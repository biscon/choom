#pragma once

#include "engine/assets/ModelAssets.h"
#include "engine/components/AnimatedModel.h"

namespace engine {

class AssetManager;
class World;

uint32_t FindModelAnimationIndex(const ModelAsset& asset, const char* name);

// Selects one entry from ModelAnimationClipCount(). A glTF entry may drive
// both raylib skeletal data and project-owned rigid node channels.
bool SetAnimatedModelClip(
        AnimatedModelAnimator& animator,
        const ModelAsset& asset,
        uint32_t clipIndex,
        bool restart = true);

// A zero blend duration switches immediately. A positive duration keeps the
// current clip as the blend source and advances both source and target clips.
void SetAnimatedModelAnimation(
        AnimatedModelAnimator& animator,
        uint32_t animationIndex,
        float blendDurationSeconds = 0.0f,
        bool restart = true);

bool SetAnimatedModelAnimationByName(
        AnimatedModelAnimator& animator,
        const ModelAsset& asset,
        const char* name,
        float blendDurationSeconds = 0.0f,
        bool restart = true);

// Advances the active clip cursor. Returns true when the resulting pose must
// be applied, including the update that clamps a non-looping clip to its final
// frame and stops playback.
bool AdvanceAnimatedModelAnimator(
        AnimatedModelAnimator& animator,
        int keyframeCount,
        float dt);

// Called during explicit load/finalization, where allocating the per-instance
// pose buffers is allowed.
void PrepareAnimatedModelInstancesSystem(World& world, AssetManager& assets);

// Explicit load/finalization helper for presentation models that are not ECS entities.
bool PrepareAnimatedModelInstance(AnimatedModelInstance& instance, const ModelAsset& asset);

// Normal frame update. This does not allocate and never applies root motion to
// an entity transform.
void AnimatedModelSystem(World& world, AssetManager& assets, float dt);

// Returns a shallow view of the shared asset with instance-owned pose pointers.
Model BuildAnimatedModelPoseView(
        const ModelAsset& asset,
        AnimatedModelInstance& instance);

// Returns the palette used by one mesh. Dynamic props using GltfScene receive
// their mesh-relative glTF palette; other animated models retain raylib's
// shared skeletal palette.
const Matrix* AnimatedModelMeshBoneMatrices(
        const ModelAsset& asset,
        const AnimatedModelInstance& instance,
        int meshIndex,
        int& outBoneCount);

} // namespace engine
