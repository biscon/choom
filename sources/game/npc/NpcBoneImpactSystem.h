#pragma once

#include "engine/systems/AnimatedModelRaycast.h"

#include <raylib.h>

namespace engine {
class AssetManager;
class World;
struct AnimatedModelInstance;
struct ModelAsset;
}

namespace game {

struct NpcBoneImpactState;

int FindNpcBoneImpactDominantBone(
        const engine::ModelAsset& asset,
        const engine::AnimatedModelSurfaceAnchor& anchor,
        float* outNormalizedInfluence = nullptr);

bool AddNpcBoneImpactImpulse(
        NpcBoneImpactState& state,
        const engine::ModelAsset& asset,
        const engine::AnimatedModelInstance& instance,
        Matrix authoredTransform,
        int boneIndex,
        Vector3 impactPositionWorld,
        Vector3 bulletDirectionWorld);

bool AdvanceNpcBoneImpactSpring(
        NpcBoneImpactState& state,
        float deltaSeconds);

bool ApplyNpcBoneImpactPose(
        const engine::ModelAsset& asset,
        engine::AnimatedModelInstance& instance,
        NpcBoneImpactState& state);

void UpdateNpcBoneImpactSystem(
        engine::World& world,
        engine::AssetManager& assets,
        float deltaSeconds);

} // namespace game
