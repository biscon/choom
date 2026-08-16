#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

#include <array>
#include <cstdint>

namespace engine {

struct AnimatedModelInstance;
struct ModelAsset;

struct AnimatedModelSurfaceAnchor {
    ModelHandle model = NullModelHandle();
    std::array<uint32_t, 3> vertexIndices{};
    Vector3 barycentric = {1.0f, 0.0f, 0.0f};
    uint32_t meshIndex = 0;
    bool valid = false;
};

enum class AnimatedModelRaycastStatus : uint8_t {
    Unavailable,
    Miss,
    Hit
};

struct AnimatedModelRaycastResult {
    float distance = 0.0f;
    Vector3 position{};
    Vector3 normal{};
    AnimatedModelSurfaceAnchor anchor;
};

// authoredTransform is the same world position/rotation/scale transform used
// by the dynamic-model renderer, excluding Model::transform.
AnimatedModelRaycastStatus RaycastAnimatedModel(
        const ModelAsset& asset,
        const AnimatedModelInstance& instance,
        Matrix authoredTransform,
        Ray ray,
        float maximumDistance,
        AnimatedModelRaycastResult& outResult);

bool ResolveAnimatedModelSurfaceAnchor(
        const ModelAsset& asset,
        const AnimatedModelInstance& instance,
        const AnimatedModelSurfaceAnchor& anchor,
        Matrix authoredTransform,
        Vector3& outPosition);

} // namespace engine
