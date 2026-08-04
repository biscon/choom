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

struct ModelMaterialAsset {
    Vector4 baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    Vector3 emissiveFactor = {};
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
};

struct ModelAsset {
    Model model = {};
    std::vector<ModelMaterialAsset> materials;
    BoundingBox localBounds = {};
    bool hasLocalBounds = false;
};

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
            const char* path);

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
    static std::string MakeRequestKey(const char* key, const char* path);
    static void DetachModelTextures(Model& model);
    static void UnloadGeometryModel(Model model);

    void QueueModelUnloadNoLock(ModelHandle handle);
    void UnloadReadyModels();

    mutable std::mutex stateMutex;
    std::vector<ModelSlot> modelSlots;
    std::vector<ModelScopeData> scopeData;
    std::deque<ModelHandle> pendingLoads;
    std::vector<Model> pendingUnloads;
    std::vector<Texture2D> pendingTextureUnloads;
};

} // namespace engine
