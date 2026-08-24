#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <array>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {

enum class ModelMaterialTextureRole : uint8_t {
    BaseColor,
    Metallic,
    Normal,
    Roughness,
    Occlusion,
    Emissive,
    Count
};

enum class ModelTextureTransfer : uint8_t {
    LinearData,
    ExplicitSrgbDecode
};

constexpr size_t ModelMaterialTextureRoleCount =
        static_cast<size_t>(ModelMaterialTextureRole::Count);

constexpr int ModelMaterialMapIndex(ModelMaterialTextureRole role)
{
    switch (role) {
        case ModelMaterialTextureRole::BaseColor: return MATERIAL_MAP_ALBEDO;
        case ModelMaterialTextureRole::Metallic: return MATERIAL_MAP_METALNESS;
        case ModelMaterialTextureRole::Normal: return MATERIAL_MAP_NORMAL;
        case ModelMaterialTextureRole::Roughness: return MATERIAL_MAP_ROUGHNESS;
        case ModelMaterialTextureRole::Occlusion: return MATERIAL_MAP_OCCLUSION;
        case ModelMaterialTextureRole::Emissive: return MATERIAL_MAP_EMISSION;
        case ModelMaterialTextureRole::Count: break;
    }
    return -1;
}

constexpr ModelTextureTransfer ModelMaterialTextureTransfer(
        ModelMaterialTextureRole role)
{
    return role == ModelMaterialTextureRole::BaseColor
                    || role == ModelMaterialTextureRole::Emissive
            ? ModelTextureTransfer::ExplicitSrgbDecode
            : ModelTextureTransfer::LinearData;
}

// glTF metallic-roughness images pack roughness in G and metallic in B.
// raylib extracts those channels into separate R8 textures before this
// renderer samples them.
constexpr int ModelMaterialPackedSourceChannel(ModelMaterialTextureRole role)
{
    return role == ModelMaterialTextureRole::Metallic
            ? 2
            : (role == ModelMaterialTextureRole::Roughness ? 1 : -1);
}

const char* ModelMaterialTextureRoleName(ModelMaterialTextureRole role);

struct ModelMaterialTextureInfo {
    bool declared = false;
    bool present = false;
    ModelTextureTransfer transfer = ModelTextureTransfer::LinearData;
    unsigned int internalFormat = 0;
    bool hardwareSrgbDecode = false;
};

enum ModelLoadFlags : uint32_t {
    ModelLoad_None = 0,
    ModelLoad_Animations = 1 << 0
};

struct ModelMaterialAsset {
    // raylib's glTF textures bypass TextureAssets. Base-color and emissive
    // textures contain scene-sRGB bytes and are explicitly decoded by the
    // active model shader. Normal, metallic/roughness, and occlusion channels
    // are linear data. Numeric factors retain glTF-defined conventions.
    Vector4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 emissiveFactor = {};
    float emissiveStrength = 1.0f;
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    float normalScale = 1.0f;
    float occlusionStrength = 1.0f;
    bool pbrMetallicRoughness = false;
    bool hasBaseColorTexture = false;
    bool hasMetallicTexture = false;
    bool hasNormalTexture = false;
    bool hasRoughnessTexture = false;
    bool hasOcclusionTexture = false;
    bool hasEmissiveTexture = false;
    std::array<ModelMaterialTextureInfo, ModelMaterialTextureRoleCount>
            textureInfo = {};
};

struct ModelOrientedBounds {
    Vector2 center = {};
    Vector2 axisX = {1.0f, 0.0f};
    Vector2 axisZ = {0.0f, 1.0f};
    Vector2 halfExtents = {};
    float bottom = 0.0f;
    float top = 0.0f;
};

enum class ModelNodeAnimationPath : uint8_t {
    Translation,
    Rotation,
    Scale
};

enum class ModelNodeAnimationInterpolation : uint8_t {
    Step,
    Linear,
    CubicSpline
};

struct ModelNodeAsset {
    int parentIndex = -1;
    ::Transform bindLocal = {
            Vector3{},
            Quaternion{0.0f, 0.0f, 0.0f, 1.0f},
            Vector3{1.0f, 1.0f, 1.0f}};
    Matrix bindLocalMatrix = {};
    Matrix bindWorldMatrix = {};
    bool usesMatrix = false;
};

struct ModelNodeAnimationChannel {
    uint32_t nodeIndex = 0;
    ModelNodeAnimationPath path = ModelNodeAnimationPath::Translation;
    ModelNodeAnimationInterpolation interpolation =
            ModelNodeAnimationInterpolation::Linear;
    std::vector<float> times;
    // Translation/scale use xyz and rotation uses xyzw. Cubic-spline tracks
    // retain glTF's in-tangent/value/out-tangent triplets.
    std::vector<Vector4> values;
};

struct ModelNodeAnimationClip {
    std::string name;
    float durationSeconds = 0.0f;
    int keyframeCount = 1;
    int skeletalAnimationIndex = -1;
    std::vector<ModelNodeAnimationChannel> channels;
};

struct ModelMeshNodeBinding {
    int nodeIndex = -1;
    Matrix inverseBindWorldMatrix = {};
    bool skinned = false;
};

// Computes a tight XZ footprint after applying Model::transform. Model loading
// calls this once so runtime collision never scans model vertices.
bool ComputeModelOrientedBounds(
        const Model& model,
        ModelOrientedBounds& outBounds);

struct ModelAsset {
    Model model = {};
    ModelAnimation* animations = nullptr;
    int animationCount = 0;
    // Project-owned glTF scene/node animation data. raylib keeps ownership of
    // mesh/material/skeletal resources but flattens rigid node transforms into
    // vertices, so unskinned mesh instances use animated bind-relative deltas.
    std::vector<ModelNodeAsset> nodes;
    std::vector<uint32_t> nodeEvaluationOrder;
    std::vector<ModelMeshNodeBinding> meshNodeBindings;
    std::vector<ModelNodeAnimationClip> nodeAnimationClips;
    std::vector<ModelMaterialAsset> materials;
    BoundingBox localBounds = {};
    // Conservative bounds covering loaded skeletal animation poses. These are
    // generated once during model finalization and are used as a cheap raycast
    // broad phase; exact hits still test the current skinned triangles.
    BoundingBox animatedLocalBounds = {};
    ModelOrientedBounds localCollisionBounds = {};
    bool hasLocalBounds = false;
    bool hasAnimatedLocalBounds = false;
    bool hasLocalCollisionBounds = false;
};

// CPU-side glTF scene/animation import used while finalizing a loaded raylib
// model. Exposed separately so importer behavior can be tested without a GPU
// context; model.meshCount must match the raylib model that owns the meshes.
bool LoadModelNodeAnimationsFromGltf(
        const char* path,
        ModelAsset& asset);

size_t ModelAnimationClipCount(const ModelAsset& asset);
const char* ModelAnimationClipName(const ModelAsset& asset, size_t clipIndex);
int FindModelAnimationClipIndex(const ModelAsset& asset, const char* name);
int ModelAnimationClipKeyframeCount(const ModelAsset& asset, size_t clipIndex);

// All output vectors must already match their corresponding asset sizes. This
// function performs no allocation and samples at an arbitrary glTF time.
bool SampleModelNodeAnimation(
        const ModelAsset& asset,
        size_t clipIndex,
        float timeSeconds,
        std::vector<::Transform>& nodeLocalTransforms,
        std::vector<Matrix>& nodeLocalMatrices,
        std::vector<Matrix>& nodeWorldMatrices,
        std::vector<Matrix>& meshNodeMatrices);

// Returns the full DrawMesh transform for one mesh. authoredTransform excludes
// Model::transform, matching the existing animated-model rendering contract.
Matrix AnimatedModelMeshTransform(
        const ModelAsset& asset,
        const std::vector<Matrix>& meshNodeMatrices,
        int meshIndex,
        Matrix authoredTransform);

enum class ModelState {
    Unloaded,
    Queued,
    Ready,
    Failed,
    QueuedForUnload
};

class ModelAssets {
public:
    void OnScopeCreated(AssetScopeHandle scope);

    ModelHandle RequestModel(
            AssetScopeHandle scope,
            const char* key,
            const char* path,
            ModelLoadFlags flags = ModelLoad_None);

    bool IsReady(ModelHandle handle) const;
    bool IsFinished(ModelHandle handle) const;
    bool HasFailed(ModelHandle handle) const;
    const Model* GetModel(ModelHandle handle) const;
    const ModelAsset* GetModelAsset(ModelHandle handle) const;
    ModelHandle FindReadyModelByPath(const char* path) const;

    bool IsScopeReady(AssetScopeHandle scope) const;
    bool IsScopeFinished(AssetScopeHandle scope) const;
    void GetScopeProgress(AssetScopeHandle scope, size_t& finished, size_t& total) const;

    void UpdateMainThread(float maxMilliseconds);
    void UnloadScope(AssetScopeHandle scope);
    void ShutdownMainThread();

private:
    struct ModelSlot {
        uint32_t generation = 1;
        ModelState state = ModelState::Unloaded;
        std::string key;
        std::string path;
        ModelLoadFlags flags = ModelLoad_None;
        AssetScopeHandle scope;
        ModelAsset asset;
        std::string error;
    };

    struct ModelScopeData {
        std::vector<ModelHandle> models;
        std::unordered_map<std::string, ModelHandle> modelByRequest;
        std::unordered_map<std::string, Texture2D> sharedTextureBySource;
        std::vector<Texture2D> sharedTextures;
    };

    bool IsValidModelNoLock(ModelHandle handle) const;
    static bool IsTerminal(ModelState state);
    static bool IsModelValid(const Model& model);
    static std::string MakeRequestKey(const char* key, const char* path, ModelLoadFlags flags);
    static void DetachModelTextures(Model& model);
    static void UnloadGeometryModel(Model model);

    void QueueModelUnloadNoLock(ModelHandle handle);
    void UnloadReadyModels();

    mutable std::mutex stateMutex;
    std::vector<ModelSlot> modelSlots;
    std::vector<ModelScopeData> scopeData;
    std::deque<ModelHandle> pendingLoads;
    struct PendingModelUnload {
        Model model = {};
        ModelAnimation* animations = nullptr;
        int animationCount = 0;
    };

    std::vector<PendingModelUnload> pendingUnloads;
    std::vector<Texture2D> pendingTextureUnloads;
};

} // namespace engine
