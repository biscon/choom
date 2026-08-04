#include "engine/assets/ModelAssets.h"

#include <external/cgltf.h>
#include <rlgl.h>
#include <raymath.h>

#include <array>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <unordered_set>
#include <utility>

namespace engine {
namespace {

constexpr int RaylibMaterialMapCount = 12;
constexpr int PbrMaterialMapCount = 6;

struct ParsedModelMaterials {
    std::vector<ModelMaterialAsset> materials;
    std::vector<std::array<std::string, PbrMaterialMapCount>> textureSources;
    bool hasUnsupportedMaterial = false;
};

const cgltf_image* ImageForTexture(const cgltf_texture* texture)
{
    if (texture == nullptr) {
        return nullptr;
    }
    if (texture->image != nullptr) {
        return texture->image;
    }
    if (texture->has_basisu) {
        return texture->basisu_image;
    }
    if (texture->has_webp) {
        return texture->webp_image;
    }
    return nullptr;
}

std::string TextureSourceKey(
        const cgltf_data* data,
        const cgltf_texture_view& view,
        const std::filesystem::path& modelPath,
        const char* channelSuffix = nullptr)
{
    const cgltf_image* image = ImageForTexture(view.texture);
    if (data == nullptr || image == nullptr) {
        return {};
    }

    std::string key;
    if (image->uri != nullptr
            && image->uri[0] != '\0'
            && std::strncmp(image->uri, "data:", 5) != 0) {
        key = (modelPath.parent_path() / image->uri)
                      .lexically_normal()
                      .generic_string();
    } else {
        key = modelPath.lexically_normal().generic_string()
                + "#image:"
                + std::to_string(cgltf_image_index(data, image));
    }
    if (channelSuffix != nullptr) {
        key += channelSuffix;
    }
    return key;
}

ParsedModelMaterials ParseModelMaterials(
        const std::string& path,
        int raylibMaterialCount)
{
    ParsedModelMaterials parsed;
    parsed.materials.resize(static_cast<size_t>(std::max(0, raylibMaterialCount)));
    parsed.textureSources.resize(parsed.materials.size());
    if (!IsFileExtension(path.c_str(), ".gltf;.glb")) {
        return parsed;
    }

    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data)
            != cgltf_result_success
            || data == nullptr) {
        return parsed;
    }

    const std::filesystem::path modelPath(path);
    const size_t count = std::min(
            static_cast<size_t>(data->materials_count),
            parsed.materials.size() > 0 ? parsed.materials.size() - 1 : 0);
    for (size_t i = 0; i < count; ++i) {
        const cgltf_material& source = data->materials[i];
        ModelMaterialAsset& material = parsed.materials[i + 1];
        auto& textureSources = parsed.textureSources[i + 1];
        material.pbrMetallicRoughness = source.has_pbr_metallic_roughness;
        if (material.pbrMetallicRoughness) {
            const auto& pbr = source.pbr_metallic_roughness;
            material.baseColorFactor = Vector4{
                    pbr.base_color_factor[0],
                    pbr.base_color_factor[1],
                    pbr.base_color_factor[2],
                    pbr.base_color_factor[3]};
            material.metallicFactor = pbr.metallic_factor;
            material.roughnessFactor = pbr.roughness_factor;
            material.hasBaseColorTexture = pbr.base_color_texture.texture != nullptr;
            material.hasMetallicTexture = pbr.metallic_roughness_texture.texture != nullptr;
            material.hasRoughnessTexture = material.hasMetallicTexture;
            textureSources[MATERIAL_MAP_ALBEDO] = TextureSourceKey(
                    data, pbr.base_color_texture, modelPath);
            textureSources[MATERIAL_MAP_METALNESS] = TextureSourceKey(
                    data, pbr.metallic_roughness_texture, modelPath, "#metallic-b");
            textureSources[MATERIAL_MAP_ROUGHNESS] = TextureSourceKey(
                    data, pbr.metallic_roughness_texture, modelPath, "#roughness-g");
        }
        material.hasNormalTexture = source.normal_texture.texture != nullptr;
        material.normalScale = source.normal_texture.scale;
        textureSources[MATERIAL_MAP_NORMAL] = TextureSourceKey(
                data, source.normal_texture, modelPath);
        material.hasOcclusionTexture = source.occlusion_texture.texture != nullptr;
        material.occlusionStrength = source.occlusion_texture.scale;
        textureSources[MATERIAL_MAP_OCCLUSION] = TextureSourceKey(
                data, source.occlusion_texture, modelPath);
        material.hasEmissiveTexture = source.emissive_texture.texture != nullptr;
        material.emissiveFactor = Vector3{
                source.emissive_factor[0],
                source.emissive_factor[1],
                source.emissive_factor[2]};
        textureSources[MATERIAL_MAP_EMISSION] = TextureSourceKey(
                data, source.emissive_texture, modelPath);

        parsed.hasUnsupportedMaterial = parsed.hasUnsupportedMaterial
                || source.alpha_mode != cgltf_alpha_mode_opaque
                || source.has_pbr_specular_glossiness
                || source.has_clearcoat
                || source.has_transmission
                || source.has_volume
                || source.has_sheen
                || source.unlit
                || source.pbr_metallic_roughness.base_color_texture.texcoord != 0
                || source.pbr_metallic_roughness.metallic_roughness_texture.texcoord != 0
                || source.normal_texture.texcoord != 0
                || source.occlusion_texture.texcoord != 0
                || source.emissive_texture.texcoord != 0
                || source.pbr_metallic_roughness.base_color_texture.has_transform
                || source.pbr_metallic_roughness.metallic_roughness_texture.has_transform
                || source.normal_texture.has_transform
                || source.occlusion_texture.has_transform
                || source.emissive_texture.has_transform;
    }
    cgltf_free(data);
    return parsed;
}

void ApplyPbrTextureQuality(Texture2D& texture)
{
    if (texture.id == 0 || texture.id == rlGetTextureIdDefault()) {
        return;
    }
    if (texture.mipmaps <= 1) {
        GenTextureMipmaps(&texture);
    }
    SetTextureFilter(texture, TEXTURE_FILTER_TRILINEAR);
    SetTextureFilter(texture, TEXTURE_FILTER_ANISOTROPIC_8X);
}

void GenerateMissingTangents(Model& model)
{
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        Mesh& mesh = model.meshes[meshIndex];
        if (mesh.tangents == nullptr
                && mesh.vertices != nullptr
                && mesh.normals != nullptr
                && mesh.texcoords != nullptr) {
            GenMeshTangents(&mesh);
        }
    }
}

bool ComputeModelLocalBounds(const Model& model, BoundingBox& outBounds)
{
    bool foundVertex = false;
    Vector3 minimum{};
    Vector3 maximum{};
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const Mesh& mesh = model.meshes[meshIndex];
        if (mesh.vertices == nullptr || mesh.vertexCount <= 0) {
            continue;
        }
        for (int vertexIndex = 0; vertexIndex < mesh.vertexCount; ++vertexIndex) {
            const Vector3 source{
                    mesh.vertices[vertexIndex * 3],
                    mesh.vertices[vertexIndex * 3 + 1],
                    mesh.vertices[vertexIndex * 3 + 2]};
            const Vector3 transformed = Vector3Transform(source, model.transform);
            if (!std::isfinite(transformed.x)
                    || !std::isfinite(transformed.y)
                    || !std::isfinite(transformed.z)) {
                continue;
            }
            if (!foundVertex) {
                minimum = transformed;
                maximum = transformed;
                foundVertex = true;
                continue;
            }
            minimum.x = std::min(minimum.x, transformed.x);
            minimum.y = std::min(minimum.y, transformed.y);
            minimum.z = std::min(minimum.z, transformed.z);
            maximum.x = std::max(maximum.x, transformed.x);
            maximum.y = std::max(maximum.y, transformed.y);
            maximum.z = std::max(maximum.z, transformed.z);
        }
    }
    outBounds = BoundingBox{minimum, maximum};
    return foundVertex;
}

} // namespace

void ModelAssets::OnScopeCreated(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        scopeData.resize(static_cast<size_t>(scope.index) + 1);
    }
}

ModelHandle ModelAssets::RequestModel(
        AssetScopeHandle scope,
        const char* key,
        const char* path)
{
    if (key == nullptr || path == nullptr || key[0] == '\0' || path[0] == '\0') {
        return NullModelHandle();
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return NullModelHandle();
    }

    ModelScopeData& data = scopeData[scope.index];
    const std::string requestKey = MakeRequestKey(key, path);
    const auto existing = data.modelByRequest.find(requestKey);
    if (existing != data.modelByRequest.end()) {
        return existing->second;
    }

    assert(modelSlots.size() < std::numeric_limits<uint32_t>::max());
    ModelSlot slot;
    slot.state = ModelState::Queued;
    slot.key = key;
    slot.path = path;
    slot.scope = scope;

    const uint32_t index = static_cast<uint32_t>(modelSlots.size());
    modelSlots.push_back(std::move(slot));
    const ModelHandle handle{index, modelSlots[index].generation};
    data.models.push_back(handle);
    data.modelByRequest.emplace(requestKey, handle);
    pendingLoads.push_back(handle);
    return handle;
}

bool ModelAssets::IsReady(ModelHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return IsValidModelNoLock(handle)
            && modelSlots[handle.index].state == ModelState::Ready;
}

bool ModelAssets::IsFinished(ModelHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidModelNoLock(handle)
            || IsTerminal(modelSlots[handle.index].state);
}

bool ModelAssets::HasFailed(ModelHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return !IsValidModelNoLock(handle)
            || modelSlots[handle.index].state == ModelState::Failed;
}

const Model* ModelAssets::GetModel(ModelHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidModelNoLock(handle)) {
        return nullptr;
    }
    const ModelSlot& slot = modelSlots[handle.index];
    return slot.state == ModelState::Ready && IsModelValid(slot.asset.model)
            ? &slot.asset.model
            : nullptr;
}

const ModelAsset* ModelAssets::GetModelAsset(ModelHandle handle) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!IsValidModelNoLock(handle)) {
        return nullptr;
    }
    const ModelSlot& slot = modelSlots[handle.index];
    return slot.state == ModelState::Ready && IsModelValid(slot.asset.model)
            ? &slot.asset
            : nullptr;
}

ModelHandle ModelAssets::FindReadyModelByPath(const char* path) const
{
    if (path == nullptr || path[0] == '\0') {
        return NullModelHandle();
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    for (uint32_t index = 0;
            index < static_cast<uint32_t>(modelSlots.size());
            ++index) {
        const ModelSlot& slot = modelSlots[index];
        if (slot.state == ModelState::Ready
                && slot.path == path
                && IsModelValid(slot.asset.model)) {
            return ModelHandle{index, slot.generation};
        }
    }
    return NullModelHandle();
}

bool ModelAssets::IsScopeReady(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return false;
    }
    for (ModelHandle handle : scopeData[scope.index].models) {
        if (!IsValidModelNoLock(handle)
                || modelSlots[handle.index].state != ModelState::Ready) {
            return false;
        }
    }
    return true;
}

bool ModelAssets::IsScopeFinished(AssetScopeHandle scope) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return false;
    }
    for (ModelHandle handle : scopeData[scope.index].models) {
        if (IsValidModelNoLock(handle)
                && !IsTerminal(modelSlots[handle.index].state)) {
            return false;
        }
    }
    return true;
}

void ModelAssets::GetScopeProgress(
        AssetScopeHandle scope,
        size_t& finished,
        size_t& total) const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return;
    }
    const ModelScopeData& data = scopeData[scope.index];
    total += data.models.size();
    for (ModelHandle handle : data.models) {
        if (!IsValidModelNoLock(handle)
                || IsTerminal(modelSlots[handle.index].state)) {
            ++finished;
        }
    }
}

void ModelAssets::UpdateMainThread(float maxMilliseconds)
{
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        ModelHandle handle;
        std::string path;
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (pendingLoads.empty()) {
                break;
            }
            handle = pendingLoads.front();
            pendingLoads.pop_front();
            if (!IsValidModelNoLock(handle)
                    || modelSlots[handle.index].state != ModelState::Queued) {
                continue;
            }
            path = modelSlots[handle.index].path;
        }

        // raylib glTF loading performs GPU mesh and texture creation. Keep the
        // entire operation on the main thread.
        Model loaded = LoadModel(path.c_str());
        const bool loadedModel = IsModelValid(loaded);
        if (!loadedModel) {
            std::fprintf(stderr, "[AssetManager WARNING] Failed to load model: %s\n", path.c_str());
            if (loaded.meshes != nullptr
                    || loaded.materials != nullptr
                    || loaded.meshMaterial != nullptr) {
                // Failed raylib loads retain normal ownership because they are
                // never entered into the scope shared-texture registry.
                std::unordered_set<unsigned int> unloaded;
                for (int materialIndex = 0;
                        materialIndex < loaded.materialCount;
                        ++materialIndex) {
                    Material& material = loaded.materials[materialIndex];
                    if (material.maps == nullptr) {
                        continue;
                    }
                    for (int mapIndex = 0;
                            mapIndex < RaylibMaterialMapCount;
                            ++mapIndex) {
                        Texture2D& texture = material.maps[mapIndex].texture;
                        if (texture.id != 0
                                && texture.id != rlGetTextureIdDefault()
                                && unloaded.insert(texture.id).second) {
                            UnloadTexture(texture);
                        }
                        texture = {};
                    }
                }
                UnloadModel(loaded);
                loaded = {};
            }
        }

        ParsedModelMaterials parsed;
        BoundingBox localBounds{};
        bool hasLocalBounds = false;
        if (loadedModel) {
            parsed = ParseModelMaterials(path, loaded.materialCount);
            GenerateMissingTangents(loaded);
            hasLocalBounds = ComputeModelLocalBounds(loaded, localBounds);
        }

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (!IsValidModelNoLock(handle)
                    || modelSlots[handle.index].state == ModelState::QueuedForUnload) {
                if (loadedModel) {
                    // The model was cancelled before its textures were adopted.
                    // Keep the old owned-model cleanup path local to this case.
                    std::unordered_set<unsigned int> unloaded;
                    for (int materialIndex = 0;
                            materialIndex < loaded.materialCount;
                            ++materialIndex) {
                        Material& material = loaded.materials[materialIndex];
                        if (material.maps == nullptr) {
                            continue;
                        }
                        for (int mapIndex = 0;
                                mapIndex < RaylibMaterialMapCount;
                                ++mapIndex) {
                            Texture2D& texture = material.maps[mapIndex].texture;
                            if (texture.id != 0
                                    && texture.id != rlGetTextureIdDefault()
                                    && unloaded.insert(texture.id).second) {
                                pendingTextureUnloads.push_back(texture);
                            }
                            texture = {};
                        }
                    }
                    pendingUnloads.push_back(loaded);
                }
            } else {
                ModelSlot& slot = modelSlots[handle.index];
                if (loadedModel) {
                    ModelScopeData& scope = scopeData[slot.scope.index];
                    const unsigned int defaultTextureId = rlGetTextureIdDefault();
                    for (int materialIndex = 0;
                            materialIndex < loaded.materialCount;
                            ++materialIndex) {
                        Material& material = loaded.materials[materialIndex];
                        if (material.maps == nullptr) {
                            continue;
                        }
                        for (int mapIndex = 0;
                                mapIndex < RaylibMaterialMapCount;
                                ++mapIndex) {
                            Texture2D& texture = material.maps[mapIndex].texture;
                            if (texture.id == 0 || texture.id == defaultTextureId) {
                                continue;
                            }
                            std::string sourceKey;
                            if (materialIndex < static_cast<int>(parsed.textureSources.size())
                                    && mapIndex < PbrMaterialMapCount) {
                                sourceKey = parsed.textureSources[
                                        static_cast<size_t>(materialIndex)][
                                        static_cast<size_t>(mapIndex)];
                            }
                            if (sourceKey.empty()) {
                                sourceKey = path
                                        + "#material:"
                                        + std::to_string(materialIndex)
                                        + "#map:"
                                        + std::to_string(mapIndex);
                            }

                            const auto existing = scope.sharedTextureBySource.find(sourceKey);
                            if (existing != scope.sharedTextureBySource.end()) {
                                if (existing->second.id != texture.id) {
                                    pendingTextureUnloads.push_back(texture);
                                }
                                texture = existing->second;
                                continue;
                            }

                            const auto sameId = std::find_if(
                                    scope.sharedTextures.begin(),
                                    scope.sharedTextures.end(),
                                    [&texture](const Texture2D& candidate) {
                                        return candidate.id == texture.id;
                                    });
                            if (sameId != scope.sharedTextures.end()) {
                                texture = *sameId;
                                scope.sharedTextureBySource.emplace(sourceKey, texture);
                                continue;
                            }

                            ApplyPbrTextureQuality(texture);
                            scope.sharedTextures.push_back(texture);
                            scope.sharedTextureBySource.emplace(sourceKey, texture);
                        }
                    }
                    slot.asset.model = loaded;
                    slot.asset.materials = std::move(parsed.materials);
                    slot.asset.localBounds = localBounds;
                    slot.asset.hasLocalBounds = hasLocalBounds;
                    slot.state = ModelState::Ready;
                    slot.error.clear();
                    if (parsed.hasUnsupportedMaterial) {
                        std::fprintf(
                                stderr,
                                "[AssetManager WARNING] Model uses glTF material features outside core opaque metallic/roughness support: %s\n",
                                path.c_str());
                    }
                } else {
                    slot.state = ModelState::Failed;
                    slot.error = "Failed to load model: " + path;
                }
            }
        }

        if (maxMilliseconds > 0.0f) {
            const float elapsed = std::chrono::duration<float, std::milli>(
                    std::chrono::steady_clock::now() - start).count();
            if (elapsed >= maxMilliseconds) {
                break;
            }
        }
    }

    UnloadReadyModels();
}

void ModelAssets::UnloadScope(AssetScopeHandle scope)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return;
    }
    ModelScopeData& data = scopeData[scope.index];
    for (ModelHandle handle : data.models) {
        QueueModelUnloadNoLock(handle);
    }
    pendingTextureUnloads.insert(
            pendingTextureUnloads.end(),
            data.sharedTextures.begin(),
            data.sharedTextures.end());
    data.models.clear();
    data.modelByRequest.clear();
    data.sharedTextureBySource.clear();
    data.sharedTextures.clear();
}

void ModelAssets::ShutdownMainThread()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        pendingLoads.clear();
        for (ModelSlot& slot : modelSlots) {
            if ((slot.state == ModelState::Ready
                    || slot.state == ModelState::QueuedForUnload)
                    && IsModelValid(slot.asset.model)) {
                DetachModelTextures(slot.asset.model);
                pendingUnloads.push_back(slot.asset.model);
                slot.asset = {};
            }
            slot.state = ModelState::Unloaded;
        }
        for (ModelScopeData& scope : scopeData) {
            pendingTextureUnloads.insert(
                    pendingTextureUnloads.end(),
                    scope.sharedTextures.begin(),
                    scope.sharedTextures.end());
            scope.sharedTextureBySource.clear();
            scope.sharedTextures.clear();
        }
    }
    UnloadReadyModels();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        modelSlots.clear();
        scopeData.clear();
        pendingLoads.clear();
        pendingUnloads.clear();
        pendingTextureUnloads.clear();
    }
}

bool ModelAssets::IsValidModelNoLock(ModelHandle handle) const
{
    return handle.index < modelSlots.size()
            && modelSlots[handle.index].generation == handle.generation;
}

bool ModelAssets::IsTerminal(ModelState state)
{
    return state == ModelState::Ready
            || state == ModelState::Failed
            || state == ModelState::Unloaded
            || state == ModelState::QueuedForUnload;
}

bool ModelAssets::IsModelValid(const Model& model)
{
    return ::IsModelValid(model);
}

std::string ModelAssets::MakeRequestKey(const char* key, const char* path)
{
    (void)key;
    return path;
}

void ModelAssets::DetachModelTextures(Model& model)
{
    for (int materialIndex = 0; materialIndex < model.materialCount; ++materialIndex) {
        Material& material = model.materials[materialIndex];
        if (material.maps == nullptr) {
            continue;
        }
        for (int mapIndex = 0; mapIndex < RaylibMaterialMapCount; ++mapIndex) {
            material.maps[mapIndex].texture = {};
        }
    }
}

void ModelAssets::UnloadGeometryModel(Model model)
{
    UnloadModel(model);
}

void ModelAssets::QueueModelUnloadNoLock(ModelHandle handle)
{
    if (!IsValidModelNoLock(handle)) {
        return;
    }
    ModelSlot& slot = modelSlots[handle.index];
    if (IsModelValid(slot.asset.model)) {
        DetachModelTextures(slot.asset.model);
        pendingUnloads.push_back(slot.asset.model);
        slot.asset = {};
    }
    slot.state = ModelState::QueuedForUnload;
    ++slot.generation;
}

void ModelAssets::UnloadReadyModels()
{
    std::vector<Model> unloads;
    std::vector<Texture2D> textureUnloads;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        unloads.swap(pendingUnloads);
        textureUnloads.swap(pendingTextureUnloads);
    }
    for (Model model : unloads) {
        if (IsModelValid(model)) {
            UnloadGeometryModel(model);
        }
    }
    std::unordered_set<unsigned int> unloadedTextureIds;
    for (Texture2D texture : textureUnloads) {
        if (texture.id != 0
                && texture.id != rlGetTextureIdDefault()
                && unloadedTextureIds.insert(texture.id).second) {
            UnloadTexture(texture);
        }
    }
}

} // namespace engine
