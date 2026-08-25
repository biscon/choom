#include "engine/assets/ModelAssets.h"

#include <external/cgltf.h>
#include <external/glad.h>
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
#include <functional>
#include <limits>
#include <unordered_set>
#include <utility>

namespace engine {
namespace {

constexpr int RaylibMaterialMapCount = 12;
constexpr int PbrMaterialMapCount = 6;
constexpr float ModelGltfAnimationFramesPerSecond = 60.0f;
static_assert(PbrMaterialMapCount == static_cast<int>(ModelMaterialTextureRoleCount));

struct ParsedModelMaterials {
    std::vector<ModelMaterialAsset> materials;
    std::vector<std::array<std::string, PbrMaterialMapCount>> textureSources;
    bool hasUnsupportedMaterial = false;
};

struct ParsedModelNodeAnimations {
    std::vector<ModelNodeAsset> nodes;
    std::vector<uint32_t> evaluationOrder;
    std::vector<ModelMeshNodeBinding> meshBindings;
    ModelGltfSkinAsset skin;
    std::vector<ModelNodeAnimationClip> clips;
};

Matrix CgltfMatrix(const cgltf_float values[16])
{
    return Matrix{
            values[0], values[4], values[8], values[12],
            values[1], values[5], values[9], values[13],
            values[2], values[6], values[10], values[14],
            values[3], values[7], values[11], values[15]};
}

::Transform CgltfNodeBindTransform(const cgltf_node& node)
{
    return ::Transform{
            node.has_translation
                    ? Vector3{
                            node.translation[0],
                            node.translation[1],
                            node.translation[2]}
                    : Vector3{},
            node.has_rotation
                    ? Quaternion{
                            node.rotation[0],
                            node.rotation[1],
                            node.rotation[2],
                            node.rotation[3]}
                    : Quaternion{0.0f, 0.0f, 0.0f, 1.0f},
            node.has_scale
                    ? Vector3{
                            node.scale[0],
                            node.scale[1],
                            node.scale[2]}
                    : Vector3{1.0f, 1.0f, 1.0f}};
}

bool AppendNodeEvaluationOrder(
        uint32_t nodeIndex,
        const cgltf_data& data,
        std::vector<uint8_t>& visits,
        std::vector<uint32_t>& order)
{
    if (nodeIndex >= data.nodes_count) return false;
    uint8_t& visit = visits[nodeIndex];
    if (visit == 2) return true;
    if (visit == 1) return false;
    visit = 1;
    const cgltf_node& node = data.nodes[nodeIndex];
    if (node.parent != nullptr) {
        const cgltf_size parentIndex = cgltf_node_index(&data, node.parent);
        if (parentIndex >= data.nodes_count
                || !AppendNodeEvaluationOrder(
                        static_cast<uint32_t>(parentIndex),
                        data,
                        visits,
                        order)) {
            return false;
        }
    }
    visit = 2;
    order.push_back(nodeIndex);
    return true;
}

bool ReadAnimationChannel(
        const cgltf_data& data,
        const cgltf_animation_channel& source,
        ModelNodeAnimationChannel& outChannel)
{
    if (source.target_node == nullptr
            || source.sampler == nullptr
            || source.sampler->input == nullptr
            || source.sampler->output == nullptr
            || source.target_node->has_matrix) {
        return false;
    }
    const cgltf_size nodeIndex = cgltf_node_index(&data, source.target_node);
    if (nodeIndex >= data.nodes_count) return false;

    int componentCount = 0;
    switch (source.target_path) {
        case cgltf_animation_path_type_translation:
            outChannel.path = ModelNodeAnimationPath::Translation;
            componentCount = 3;
            break;
        case cgltf_animation_path_type_rotation:
            outChannel.path = ModelNodeAnimationPath::Rotation;
            componentCount = 4;
            break;
        case cgltf_animation_path_type_scale:
            outChannel.path = ModelNodeAnimationPath::Scale;
            componentCount = 3;
            break;
        default:
            return false;
    }
    switch (source.sampler->interpolation) {
        case cgltf_interpolation_type_step:
            outChannel.interpolation = ModelNodeAnimationInterpolation::Step;
            break;
        case cgltf_interpolation_type_linear:
            outChannel.interpolation = ModelNodeAnimationInterpolation::Linear;
            break;
        case cgltf_interpolation_type_cubic_spline:
            outChannel.interpolation =
                    ModelNodeAnimationInterpolation::CubicSpline;
            break;
        default:
            return false;
    }

    const cgltf_accessor& input = *source.sampler->input;
    const cgltf_accessor& output = *source.sampler->output;
    if (input.count == 0
            || input.type != cgltf_type_scalar
            || input.component_type != cgltf_component_type_r_32f
            || output.component_type != cgltf_component_type_r_32f
            || (componentCount == 3 && output.type != cgltf_type_vec3)
            || (componentCount == 4 && output.type != cgltf_type_vec4)) {
        return false;
    }
    const size_t valueMultiplier =
            outChannel.interpolation == ModelNodeAnimationInterpolation::CubicSpline
            ? 3u
            : 1u;
    if (output.count != input.count * valueMultiplier) return false;

    outChannel.nodeIndex = static_cast<uint32_t>(nodeIndex);
    outChannel.times.resize(input.count);
    outChannel.values.resize(output.count);
    float previous = -std::numeric_limits<float>::infinity();
    for (cgltf_size key = 0; key < input.count; ++key) {
        float time = 0.0f;
        if (!cgltf_accessor_read_float(&input, key, &time, 1)
                || !std::isfinite(time)
                || time < 0.0f
                || time <= previous) {
            return false;
        }
        outChannel.times[key] = time;
        previous = time;
    }
    for (cgltf_size valueIndex = 0; valueIndex < output.count; ++valueIndex) {
        float values[4] = {};
        if (!cgltf_accessor_read_float(
                    &output,
                    valueIndex,
                    values,
                    static_cast<cgltf_size>(componentCount))) {
            return false;
        }
        Vector4& value = outChannel.values[valueIndex];
        value = Vector4{values[0], values[1], values[2],
                componentCount == 4 ? values[3] : 0.0f};
        if (!std::isfinite(value.x)
                || !std::isfinite(value.y)
                || !std::isfinite(value.z)
                || !std::isfinite(value.w)) {
            return false;
        }
    }
    return true;
}

ParsedModelNodeAnimations ParseModelNodeAnimations(
        const std::string& path,
        const Model& model,
        int skeletalAnimationCount)
{
    ParsedModelNodeAnimations parsed;
    if (!IsFileExtension(path.c_str(), ".gltf;.glb")) return parsed;

    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data)
                    != cgltf_result_success
            || data == nullptr) {
        std::fprintf(
                stderr,
                "[AssetManager WARNING] Could not parse glTF node animations: %s\n",
                path.c_str());
        return parsed;
    }
    const cgltf_result bufferResult =
            cgltf_load_buffers(&options, data, path.c_str());
    if (bufferResult != cgltf_result_success) {
        std::fprintf(
                stderr,
                "[AssetManager WARNING] Could not load glTF node animation buffers: %s\n",
                path.c_str());
        cgltf_free(data);
        return parsed;
    }

    parsed.nodes.resize(data->nodes_count);
    parsed.evaluationOrder.reserve(data->nodes_count);
    std::vector<uint8_t> visits(data->nodes_count, 0);
    bool hierarchyValid = true;
    for (cgltf_size nodeIndex = 0;
            nodeIndex < data->nodes_count;
            ++nodeIndex) {
        const cgltf_node& source = data->nodes[nodeIndex];
        ModelNodeAsset& target = parsed.nodes[nodeIndex];
        target.parentIndex = source.parent != nullptr
                ? static_cast<int>(cgltf_node_index(data, source.parent))
                : -1;
        target.bindLocal = CgltfNodeBindTransform(source);
        cgltf_float localValues[16] = {};
        cgltf_float worldValues[16] = {};
        cgltf_node_transform_local(&source, localValues);
        cgltf_node_transform_world(&source, worldValues);
        target.bindLocalMatrix = CgltfMatrix(localValues);
        target.bindWorldMatrix = CgltfMatrix(worldValues);
        target.usesMatrix = source.has_matrix;
        hierarchyValid = hierarchyValid
                && AppendNodeEvaluationOrder(
                        static_cast<uint32_t>(nodeIndex),
                        *data,
                        visits,
                        parsed.evaluationOrder);
    }
    if (!hierarchyValid) {
        std::fprintf(
                stderr,
                "[AssetManager WARNING] glTF node hierarchy is cyclic or invalid: %s\n",
                path.c_str());
        parsed = {};
        cgltf_free(data);
        return parsed;
    }

    if (data->skins_count > 0) {
        const cgltf_skin& skin = data->skins[0];
        const bool jointCountMatchesRaylib =
                model.skeleton.boneCount > 0
                && skin.joints_count
                        == static_cast<cgltf_size>(
                                model.skeleton.boneCount);
        const bool inverseBindAccessorValid =
                skin.inverse_bind_matrices == nullptr
                || (skin.inverse_bind_matrices->count == skin.joints_count
                        && skin.inverse_bind_matrices->type
                                == cgltf_type_mat4
                        && skin.inverse_bind_matrices->component_type
                                == cgltf_component_type_r_32f);
        bool skinValid = jointCountMatchesRaylib
                && inverseBindAccessorValid;
        if (skinValid) {
            parsed.skin.jointNodeIndices.resize(skin.joints_count);
            parsed.skin.inverseBindMatrices.assign(
                    skin.joints_count,
                    MatrixIdentity());
            for (cgltf_size jointIndex = 0;
                    jointIndex < skin.joints_count;
                    ++jointIndex) {
                if (skin.joints[jointIndex] == nullptr) {
                    skinValid = false;
                    break;
                }
                const cgltf_size nodeIndex = cgltf_node_index(
                        data,
                        skin.joints[jointIndex]);
                if (nodeIndex >= data->nodes_count) {
                    skinValid = false;
                    break;
                }
                parsed.skin.jointNodeIndices[jointIndex] =
                        static_cast<uint32_t>(nodeIndex);
                if (skin.inverse_bind_matrices != nullptr) {
                    cgltf_float values[16] = {};
                    if (!cgltf_accessor_read_float(
                                skin.inverse_bind_matrices,
                                jointIndex,
                                values,
                                16)) {
                        skinValid = false;
                        break;
                    }
                    for (float value : values) {
                        if (!std::isfinite(value)) {
                            skinValid = false;
                            break;
                        }
                    }
                    if (!skinValid) break;
                    parsed.skin.inverseBindMatrices[jointIndex] =
                            CgltfMatrix(values);
                }
            }
        }
        if (!skinValid) {
            parsed.skin = {};
            std::fprintf(
                    stderr,
                    "[AssetManager WARNING] glTF skin metadata does not match raylib's loaded skeleton; exact dynamic-prop skinning disabled: %s\n",
                    path.c_str());
        }
        if (data->skins_count > 1) {
            std::fprintf(
                    stderr,
                    "[AssetManager WARNING] glTF has multiple skins; exact dynamic-prop skinning supports the first skin only: %s\n",
                    path.c_str());
        }
    }

    parsed.meshBindings.reserve(static_cast<size_t>(std::max(0, model.meshCount)));
    for (cgltf_size nodeIndex = 0;
            nodeIndex < data->nodes_count;
            ++nodeIndex) {
        const cgltf_node& node = data->nodes[nodeIndex];
        if (node.mesh == nullptr) continue;
        for (cgltf_size primitiveIndex = 0;
                primitiveIndex < node.mesh->primitives_count;
                ++primitiveIndex) {
            const cgltf_primitive& primitive =
                    node.mesh->primitives[primitiveIndex];
            if (primitive.type != cgltf_primitive_type_triangles) continue;
            ModelMeshNodeBinding binding;
            binding.nodeIndex = static_cast<int>(nodeIndex);
            binding.skinned = node.skin != nullptr;
            binding.skinIndex = node.skin == nullptr
                    ? -1
                    : (node.skin == &data->skins[0] ? 0 : -2);
            const Matrix bindWorld =
                    parsed.nodes[nodeIndex].bindWorldMatrix;
            const float determinant = MatrixDeterminant(bindWorld);
            if (!std::isfinite(determinant)
                    || std::fabs(determinant) <= 0.0000001f) {
                binding.nodeIndex = -1;
                std::fprintf(
                        stderr,
                        "[AssetManager WARNING] glTF mesh node has a non-invertible bind transform; rigid animation disabled for that mesh: %s\n",
                        path.c_str());
            } else {
                binding.inverseBindWorldMatrix = MatrixInvert(bindWorld);
            }
            parsed.meshBindings.push_back(binding);
        }
    }
    if (parsed.meshBindings.size()
            != static_cast<size_t>(std::max(0, model.meshCount))) {
        std::fprintf(
                stderr,
                "[AssetManager WARNING] glTF mesh/node mapping did not match raylib mesh order; rigid animation disabled: %s\n",
                path.c_str());
        parsed.meshBindings.assign(
                static_cast<size_t>(std::max(0, model.meshCount)),
                ModelMeshNodeBinding{});
    }

    parsed.clips.reserve(data->animations_count);
    bool warnedUnsupportedPath = false;
    for (cgltf_size animationIndex = 0;
            animationIndex < data->animations_count;
            ++animationIndex) {
        const cgltf_animation& source = data->animations[animationIndex];
        ModelNodeAnimationClip clip;
        if (source.name != nullptr) clip.name = source.name;
        clip.skeletalAnimationIndex =
                animationIndex < static_cast<cgltf_size>(
                        std::max(0, skeletalAnimationCount))
                ? static_cast<int>(animationIndex)
                : -1;
        clip.channels.reserve(source.channels_count);
        for (cgltf_size channelIndex = 0;
                channelIndex < source.channels_count;
                ++channelIndex) {
            ModelNodeAnimationChannel channel;
            if (!ReadAnimationChannel(
                        *data,
                        source.channels[channelIndex],
                        channel)) {
                if (source.channels[channelIndex].target_path
                                == cgltf_animation_path_type_weights
                        && !warnedUnsupportedPath) {
                    std::fprintf(
                            stderr,
                            "[AssetManager WARNING] glTF morph-weight animations are unsupported and will be ignored: %s\n",
                            path.c_str());
                    warnedUnsupportedPath = true;
                }
                continue;
            }
            clip.durationSeconds = std::max(
                    clip.durationSeconds,
                    channel.times.back());
            clip.channels.push_back(std::move(channel));
        }
        clip.keyframeCount = std::max(
                1,
                static_cast<int>(
                        std::floor(
                                clip.durationSeconds
                                * ModelGltfAnimationFramesPerSecond))
                        + 1);
        if (clip.skeletalAnimationIndex >= 0 || !clip.channels.empty()) {
            parsed.clips.push_back(std::move(clip));
        }
    }
    cgltf_free(data);
    return parsed;
}

bool ModelMaterialHasTextureRole(
        const ModelMaterialAsset& material,
        ModelMaterialTextureRole role)
{
    switch (role) {
        case ModelMaterialTextureRole::BaseColor: return material.hasBaseColorTexture;
        case ModelMaterialTextureRole::Metallic: return material.hasMetallicTexture;
        case ModelMaterialTextureRole::Normal: return material.hasNormalTexture;
        case ModelMaterialTextureRole::Roughness: return material.hasRoughnessTexture;
        case ModelMaterialTextureRole::Occlusion: return material.hasOcclusionTexture;
        case ModelMaterialTextureRole::Emissive: return material.hasEmissiveTexture;
        case ModelMaterialTextureRole::Count: break;
    }
    return false;
}

void SetModelMaterialHasTextureRole(
        ModelMaterialAsset& material,
        ModelMaterialTextureRole role,
        bool present)
{
    switch (role) {
        case ModelMaterialTextureRole::BaseColor: material.hasBaseColorTexture = present; return;
        case ModelMaterialTextureRole::Metallic: material.hasMetallicTexture = present; return;
        case ModelMaterialTextureRole::Normal: material.hasNormalTexture = present; return;
        case ModelMaterialTextureRole::Roughness: material.hasRoughnessTexture = present; return;
        case ModelMaterialTextureRole::Occlusion: material.hasOcclusionTexture = present; return;
        case ModelMaterialTextureRole::Emissive: material.hasEmissiveTexture = present; return;
        case ModelMaterialTextureRole::Count: return;
    }
}

bool IsHardwareSrgbInternalFormat(unsigned int format)
{
    return format == GL_SRGB8 || format == GL_SRGB8_ALPHA8;
}

unsigned int QueryTextureInternalFormat(Texture2D texture)
{
    if (texture.id == 0 || texture.id == rlGetTextureIdDefault()) {
        return 0;
    }
    int previousBinding = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBinding);
    while (glGetError() != GL_NO_ERROR) {
    }
    glBindTexture(GL_TEXTURE_2D, texture.id);
    int actualFormat = 0;
    glGetTexLevelParameteriv(
            GL_TEXTURE_2D,
            0,
            GL_TEXTURE_INTERNAL_FORMAT,
            &actualFormat);
    const bool valid = glGetError() == GL_NO_ERROR;
    glBindTexture(GL_TEXTURE_2D, static_cast<unsigned int>(previousBinding));
    return valid && actualFormat > 0
            ? static_cast<unsigned int>(actualFormat)
            : 0;
}

void PopulateModelMaterialTextureInfo(
        const std::string& path,
        const Model& model,
        std::vector<ModelMaterialAsset>& materials)
{
    const int count = std::min(
            model.materialCount,
            static_cast<int>(materials.size()));
    for (int materialIndex = 0; materialIndex < count; ++materialIndex) {
        if (model.materials == nullptr
                || model.materials[materialIndex].maps == nullptr) {
            continue;
        }
        ModelMaterialAsset& material = materials[static_cast<size_t>(materialIndex)];
        for (size_t roleIndex = 0;
                roleIndex < ModelMaterialTextureRoleCount;
                ++roleIndex) {
            const auto role = static_cast<ModelMaterialTextureRole>(roleIndex);
            ModelMaterialTextureInfo& info = material.textureInfo[roleIndex];
            info.declared = ModelMaterialHasTextureRole(material, role);
            info.transfer = ModelMaterialTextureTransfer(role);
            if (!info.declared) {
                continue;
            }
            const int mapIndex = ModelMaterialMapIndex(role);
            const Texture2D texture = model.materials[materialIndex]
                                              .maps[mapIndex]
                                              .texture;
            info.present = texture.id != 0
                    && texture.id != rlGetTextureIdDefault();
            SetModelMaterialHasTextureRole(material, role, info.present);
            if (!info.present) {
                std::fprintf(
                        stderr,
                        "[AssetManager WARNING] glTF %s texture was declared but is not bound (material %d): %s\n",
                        ModelMaterialTextureRoleName(role),
                        materialIndex,
                        path.c_str());
                continue;
            }
            info.internalFormat = QueryTextureInternalFormat(texture);
            info.hardwareSrgbDecode = IsHardwareSrgbInternalFormat(
                    info.internalFormat);
            if (info.transfer == ModelTextureTransfer::LinearData
                    && info.hardwareSrgbDecode) {
                std::fprintf(
                        stderr,
                        "[AssetManager WARNING] glTF %s texture unexpectedly uses hardware-sRGB storage (material %d): %s\n",
                        ModelMaterialTextureRoleName(role),
                        materialIndex,
                        path.c_str());
            }
        }
    }
}

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
        material.emissiveStrength = source.has_emissive_strength
                ? source.emissive_strength.emissive_strength
                : 1.0f;
        textureSources[MATERIAL_MAP_EMISSION] = TextureSourceKey(
                data, source.emissive_texture, modelPath);

        parsed.hasUnsupportedMaterial = parsed.hasUnsupportedMaterial
                || source.alpha_mode != cgltf_alpha_mode_opaque
                || source.has_pbr_specular_glossiness
                || source.has_clearcoat
                || source.has_specular
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

float Cross(Vector2 origin, Vector2 a, Vector2 b)
{
    return (a.x - origin.x) * (b.y - origin.y)
            - (a.y - origin.y) * (b.x - origin.x);
}

bool SetAxisAlignedOrientedBounds(
        Vector2 minimum,
        Vector2 maximum,
        float bottom,
        float top,
        ModelOrientedBounds& outBounds)
{
    constexpr float epsilon = 0.000001f;
    const Vector2 half{
            (maximum.x - minimum.x) * 0.5f,
            (maximum.y - minimum.y) * 0.5f};
    if (!(half.x > epsilon)
            || !(half.y > epsilon)
            || !(top > bottom + epsilon)) {
        outBounds = {};
        return false;
    }
    outBounds = ModelOrientedBounds{
            Vector2{
                    (minimum.x + maximum.x) * 0.5f,
                    (minimum.y + maximum.y) * 0.5f},
            Vector2{1.0f, 0.0f},
            Vector2{0.0f, 1.0f},
            half,
            bottom,
            top};
    return true;
}

struct ModelBoundsAccumulator {
    Vector3 minimum{};
    Vector3 maximum{};
    bool valid = false;
};

void IncludeModelBoundsPoint(ModelBoundsAccumulator& bounds, Vector3 point)
{
    if (!std::isfinite(point.x)
            || !std::isfinite(point.y)
            || !std::isfinite(point.z)) {
        return;
    }
    if (!bounds.valid) {
        bounds.minimum = point;
        bounds.maximum = point;
        bounds.valid = true;
        return;
    }
    bounds.minimum.x = std::min(bounds.minimum.x, point.x);
    bounds.minimum.y = std::min(bounds.minimum.y, point.y);
    bounds.minimum.z = std::min(bounds.minimum.z, point.z);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x);
    bounds.maximum.y = std::max(bounds.maximum.y, point.y);
    bounds.maximum.z = std::max(bounds.maximum.z, point.z);
}

void IncludeTransformedModelBounds(
        ModelBoundsAccumulator& destination,
        const ModelBoundsAccumulator& source,
        Matrix transform)
{
    if (!source.valid) return;
    for (float x : {source.minimum.x, source.maximum.x}) {
        for (float y : {source.minimum.y, source.maximum.y}) {
            for (float z : {source.minimum.z, source.maximum.z}) {
                IncludeModelBoundsPoint(
                        destination,
                        Vector3Transform(Vector3{x, y, z}, transform));
            }
        }
    }
}

Matrix ModelPoseTransformMatrix(const Transform& transform)
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

bool ComputeAnimatedModelLocalBounds(
        const Model& model,
        const ModelAnimation* animations,
        int animationCount,
        BoundingBox localBounds,
        bool hasLocalBounds,
        BoundingBox& outBounds)
{
    const int boneCount = model.skeleton.boneCount;
    if (!hasLocalBounds
            || boneCount <= 0
            || model.skeleton.bindPose == nullptr
            || animations == nullptr
            || animationCount <= 0) {
        return false;
    }

    std::vector<ModelBoundsAccumulator> boneInfluenceBounds(
            static_cast<size_t>(boneCount));
    bool foundSkinningData = false;
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const Mesh& mesh = model.meshes[meshIndex];
        if (mesh.vertices == nullptr
                || mesh.vertexCount <= 0
                || mesh.boneIndices == nullptr
                || mesh.boneWeights == nullptr) {
            continue;
        }
        for (int vertexIndex = 0; vertexIndex < mesh.vertexCount; ++vertexIndex) {
            const Vector3 vertex{
                    mesh.vertices[vertexIndex * 3],
                    mesh.vertices[vertexIndex * 3 + 1],
                    mesh.vertices[vertexIndex * 3 + 2]};
            for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex) {
                const int offset = vertexIndex * 4 + influenceIndex;
                const float weight = mesh.boneWeights[offset];
                const int boneIndex = mesh.boneIndices[offset];
                if (!std::isfinite(weight)
                        || weight <= 0.0f
                        || boneIndex < 0
                        || boneIndex >= boneCount) {
                    continue;
                }
                IncludeModelBoundsPoint(
                        boneInfluenceBounds[static_cast<size_t>(boneIndex)],
                        vertex);
                foundSkinningData = true;
            }
        }
    }
    if (!foundSkinningData) return false;

    ModelBoundsAccumulator animatedBounds;
    IncludeModelBoundsPoint(animatedBounds, localBounds.min);
    IncludeModelBoundsPoint(animatedBounds, localBounds.max);
    // A weighted skinning result is a convex combination of bone-transformed
    // points. Including the model origin keeps the envelope conservative for
    // assets whose quantized weights sum to slightly less than one.
    IncludeModelBoundsPoint(
            animatedBounds,
            Vector3Transform(Vector3{}, model.transform));

    for (int animationIndex = 0;
            animationIndex < animationCount;
            ++animationIndex) {
        const ModelAnimation& animation = animations[animationIndex];
        if (animation.keyframePoses == nullptr
                || animation.keyframeCount <= 0
                || animation.boneCount <= 0) {
            continue;
        }
        const int animationBoneCount = std::min(
                boneCount, animation.boneCount);
        for (int frameIndex = 0;
                frameIndex < animation.keyframeCount;
                ++frameIndex) {
            const Transform* pose = animation.keyframePoses[frameIndex];
            if (pose == nullptr) continue;
            for (int boneIndex = 0;
                    boneIndex < animationBoneCount;
                    ++boneIndex) {
                const ModelBoundsAccumulator& influenceBounds =
                        boneInfluenceBounds[static_cast<size_t>(boneIndex)];
                if (!influenceBounds.valid) continue;
                const Matrix bindPose = ModelPoseTransformMatrix(
                        model.skeleton.bindPose[boneIndex]);
                const Matrix currentPose = ModelPoseTransformMatrix(
                        pose[boneIndex]);
                const Matrix boneMatrix = MatrixMultiply(
                        MatrixInvert(bindPose), currentPose);
                IncludeTransformedModelBounds(
                        animatedBounds,
                        influenceBounds,
                        MatrixMultiply(boneMatrix, model.transform));
            }
        }
    }
    if (!animatedBounds.valid) return false;

    const Vector3 size = Vector3Subtract(
            animatedBounds.maximum, animatedBounds.minimum);
    const float padding = std::max(
            0.025f,
            Vector3Length(size) * 0.025f);
    outBounds = {
            Vector3Subtract(
                    animatedBounds.minimum,
                    Vector3{padding, padding, padding}),
            Vector3Add(
                    animatedBounds.maximum,
                    Vector3{padding, padding, padding})};
    return true;
}

bool ComputeGltfAnimatedModelLocalBounds(
        const ModelAsset& asset,
        BoundingBox localBounds,
        bool hasLocalBounds,
        BoundingBox& outBounds)
{
    if (!hasLocalBounds
            || asset.nodeAnimationClips.empty()
            || asset.nodes.empty()
            || asset.meshNodeBindings.size()
                    != static_cast<size_t>(
                            std::max(0, asset.model.meshCount))) {
        return false;
    }
    bool hasRigidChannels = false;
    for (const ModelNodeAnimationClip& clip :
            asset.nodeAnimationClips) {
        hasRigidChannels = hasRigidChannels || !clip.channels.empty();
    }
    if (!hasRigidChannels) return false;

    std::vector<ModelBoundsAccumulator> meshBounds(
            static_cast<size_t>(
                    std::max(0, asset.model.meshCount)));
    const int boneCount = std::max(
            0,
            asset.model.skeleton.boneCount);
    const bool hasExactSkin = boneCount > 0
            && asset.gltfSkin.jointNodeIndices.size()
                    == static_cast<size_t>(boneCount)
            && asset.gltfSkin.inverseBindMatrices.size()
                    == static_cast<size_t>(boneCount);
    std::vector<ModelBoundsAccumulator> meshBoneBounds(
            static_cast<size_t>(
                    std::max(0, asset.model.meshCount))
                    * static_cast<size_t>(boneCount));
    for (int meshIndex = 0;
            meshIndex < asset.model.meshCount;
            ++meshIndex) {
        const Mesh& mesh = asset.model.meshes[meshIndex];
        if (mesh.vertices == nullptr || mesh.vertexCount <= 0) continue;
        for (int vertexIndex = 0;
                vertexIndex < mesh.vertexCount;
                ++vertexIndex) {
            const Vector3 vertex{
                    mesh.vertices[vertexIndex * 3],
                    mesh.vertices[vertexIndex * 3 + 1],
                    mesh.vertices[vertexIndex * 3 + 2]};
            IncludeModelBoundsPoint(
                    meshBounds[static_cast<size_t>(meshIndex)],
                    vertex);
            if (!hasExactSkin
                    || asset.meshNodeBindings[
                               static_cast<size_t>(meshIndex)]
                               .skinIndex != 0
                    || mesh.boneIndices == nullptr
                    || mesh.boneWeights == nullptr) {
                continue;
            }
            for (int influenceIndex = 0;
                    influenceIndex < 4;
                    ++influenceIndex) {
                const int influenceOffset =
                        vertexIndex * 4 + influenceIndex;
                const float weight =
                        mesh.boneWeights[influenceOffset];
                const int influencedBone =
                        mesh.boneIndices[influenceOffset];
                if (!std::isfinite(weight)
                        || weight <= 0.0f
                        || influencedBone < 0
                        || influencedBone >= boneCount) {
                    continue;
                }
                IncludeModelBoundsPoint(
                        meshBoneBounds[
                                static_cast<size_t>(meshIndex)
                                        * static_cast<size_t>(boneCount)
                                + static_cast<size_t>(influencedBone)],
                        vertex);
            }
        }
    }

    std::vector<::Transform> localTransforms(asset.nodes.size());
    std::vector<Matrix> localMatrices(asset.nodes.size());
    std::vector<Matrix> worldMatrices(asset.nodes.size());
    std::vector<Matrix> meshMatrices(
            static_cast<size_t>(
                    std::max(0, asset.model.meshCount)));
    std::vector<Matrix> meshBoneMatrices(
            static_cast<size_t>(
                    std::max(0, asset.model.meshCount))
                    * static_cast<size_t>(boneCount),
            MatrixIdentity());
    ModelBoundsAccumulator animatedBounds;
    IncludeModelBoundsPoint(animatedBounds, localBounds.min);
    IncludeModelBoundsPoint(animatedBounds, localBounds.max);
    IncludeModelBoundsPoint(
            animatedBounds,
            Vector3Transform(Vector3{}, asset.model.transform));
    for (size_t clipIndex = 0;
            clipIndex < asset.nodeAnimationClips.size();
            ++clipIndex) {
        const ModelNodeAnimationClip& clip =
                asset.nodeAnimationClips[clipIndex];
        if (clip.channels.empty()) continue;
        for (int frameIndex = 0;
                frameIndex < clip.keyframeCount;
                ++frameIndex) {
            const float timeSeconds = std::min(
                    clip.durationSeconds,
                    static_cast<float>(frameIndex)
                            / ModelGltfAnimationFramesPerSecond);
            if (!SampleModelNodeAnimation(
                        asset,
                        clipIndex,
                        timeSeconds,
                        localTransforms,
                        localMatrices,
                        worldMatrices,
                        meshMatrices)) {
                continue;
            }
            const bool skinPoseValid = hasExactSkin
                    && BuildModelMeshSkinMatrices(
                            asset,
                            worldMatrices,
                            meshBoneMatrices);
            for (int meshIndex = 0;
                    meshIndex < asset.model.meshCount;
                    ++meshIndex) {
                const ModelMeshNodeBinding& binding =
                        asset.meshNodeBindings[
                                static_cast<size_t>(meshIndex)];
                if (binding.skinIndex == 0
                        && binding.nodeIndex >= 0
                        && skinPoseValid) {
                    for (int boneIndex = 0;
                            boneIndex < boneCount;
                            ++boneIndex) {
                        IncludeTransformedModelBounds(
                                animatedBounds,
                                meshBoneBounds[
                                        static_cast<size_t>(meshIndex)
                                                * static_cast<size_t>(boneCount)
                                        + static_cast<size_t>(boneIndex)],
                                MatrixMultiply(
                                        meshBoneMatrices[
                                                static_cast<size_t>(meshIndex)
                                                        * static_cast<size_t>(boneCount)
                                                + static_cast<size_t>(boneIndex)],
                                        asset.model.transform));
                    }
                    continue;
                }
                if (binding.skinned) continue;
                IncludeTransformedModelBounds(
                        animatedBounds,
                        meshBounds[static_cast<size_t>(meshIndex)],
                        MatrixMultiply(
                                meshMatrices[
                                        static_cast<size_t>(meshIndex)],
                                asset.model.transform));
            }
        }
    }
    if (!animatedBounds.valid) return false;
    const Vector3 size = Vector3Subtract(
            animatedBounds.maximum,
            animatedBounds.minimum);
    const float padding = std::max(
            0.025f,
            Vector3Length(size) * 0.025f);
    outBounds = {
            Vector3Subtract(
                    animatedBounds.minimum,
                    Vector3{padding, padding, padding}),
            Vector3Add(
                    animatedBounds.maximum,
                    Vector3{padding, padding, padding})};
    return true;
}

} // namespace

bool LoadModelNodeAnimationsFromGltf(
        const char* path,
        ModelAsset& asset)
{
    if (path == nullptr || path[0] == '\0') return false;
    ParsedModelNodeAnimations parsed = ParseModelNodeAnimations(
            path,
            asset.model,
            asset.animationCount);
    asset.nodes = std::move(parsed.nodes);
    asset.nodeEvaluationOrder = std::move(parsed.evaluationOrder);
    asset.meshNodeBindings = std::move(parsed.meshBindings);
    asset.gltfSkin = std::move(parsed.skin);
    asset.nodeAnimationClips = std::move(parsed.clips);
    return !asset.nodes.empty();
}

size_t ModelAnimationClipCount(const ModelAsset& asset)
{
    return !asset.nodeAnimationClips.empty()
            ? asset.nodeAnimationClips.size()
            : static_cast<size_t>(std::max(0, asset.animationCount));
}

const char* ModelAnimationClipName(const ModelAsset& asset, size_t clipIndex)
{
    if (!asset.nodeAnimationClips.empty()) {
        if (clipIndex >= asset.nodeAnimationClips.size()) return nullptr;
        const ModelNodeAnimationClip& clip =
                asset.nodeAnimationClips[clipIndex];
        if (!clip.name.empty()) return clip.name.c_str();
        if (clip.skeletalAnimationIndex >= 0
                && clip.skeletalAnimationIndex < asset.animationCount
                && asset.animations != nullptr) {
            return asset.animations[clip.skeletalAnimationIndex].name;
        }
        return "";
    }
    return asset.animations != nullptr
                    && clipIndex
                            < static_cast<size_t>(
                                    std::max(0, asset.animationCount))
            ? asset.animations[clipIndex].name
            : nullptr;
}

int FindModelAnimationClipIndex(const ModelAsset& asset, const char* name)
{
    if (name == nullptr || name[0] == '\0') return -1;
    const size_t count = ModelAnimationClipCount(asset);
    for (size_t index = 0; index < count; ++index) {
        const char* candidate = ModelAnimationClipName(asset, index);
        if (candidate != nullptr && std::strcmp(candidate, name) == 0) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

int ModelAnimationClipKeyframeCount(
        const ModelAsset& asset,
        size_t clipIndex)
{
    if (!asset.nodeAnimationClips.empty()) {
        if (clipIndex >= asset.nodeAnimationClips.size()) return 0;
        return asset.nodeAnimationClips[clipIndex].keyframeCount;
    }
    if (asset.animations == nullptr
            || clipIndex
                    >= static_cast<size_t>(
                            std::max(0, asset.animationCount))) {
        return 0;
    }
    return asset.animations[clipIndex].keyframeCount;
}

namespace {

Vector4 SampleModelNodeAnimationChannel(
        const ModelNodeAnimationChannel& channel,
        float timeSeconds)
{
    if (channel.times.empty() || channel.values.empty()) return {};
    const auto upper = std::upper_bound(
            channel.times.begin(),
            channel.times.end(),
            timeSeconds);
    size_t first = upper == channel.times.begin()
            ? 0
            : static_cast<size_t>(
                    std::distance(channel.times.begin(), upper) - 1);
    if (first + 1 >= channel.times.size()) {
        const size_t valueIndex =
                channel.interpolation
                                == ModelNodeAnimationInterpolation::CubicSpline
                ? first * 3 + 1
                : first;
        return channel.values[valueIndex];
    }
    if (channel.interpolation == ModelNodeAnimationInterpolation::Step) {
        return channel.values[first];
    }

    const float firstTime = channel.times[first];
    const float secondTime = channel.times[first + 1];
    const float duration = secondTime - firstTime;
    const float amount = duration > 0.0f
            ? std::clamp(
                    (timeSeconds - firstTime) / duration,
                    0.0f,
                    1.0f)
            : 0.0f;
    if (channel.interpolation == ModelNodeAnimationInterpolation::Linear) {
        if (channel.path == ModelNodeAnimationPath::Rotation) {
            return QuaternionSlerp(
                    channel.values[first],
                    channel.values[first + 1],
                    amount);
        }
        return Vector4Lerp(
                channel.values[first],
                channel.values[first + 1],
                amount);
    }

    const Vector4 firstValue = channel.values[first * 3 + 1];
    Vector4 firstTangent = Vector4Scale(
            channel.values[first * 3 + 2],
            duration);
    const Vector4 secondValue =
            channel.values[(first + 1) * 3 + 1];
    Vector4 secondTangent = Vector4Scale(
            channel.values[(first + 1) * 3],
            duration);
    if (channel.path == ModelNodeAnimationPath::Rotation) {
        return QuaternionNormalize(QuaternionCubicHermiteSpline(
                QuaternionNormalize(firstValue),
                firstTangent,
                QuaternionNormalize(secondValue),
                secondTangent,
                amount));
    }
    const Vector3 value = Vector3CubicHermite(
            Vector3{firstValue.x, firstValue.y, firstValue.z},
            Vector3{firstTangent.x, firstTangent.y, firstTangent.z},
            Vector3{secondValue.x, secondValue.y, secondValue.z},
            Vector3{secondTangent.x, secondTangent.y, secondTangent.z},
            amount);
    return Vector4{value.x, value.y, value.z, 0.0f};
}

} // namespace

bool SampleModelNodeAnimation(
        const ModelAsset& asset,
        size_t clipIndex,
        float timeSeconds,
        std::vector<::Transform>& nodeLocalTransforms,
        std::vector<Matrix>& nodeLocalMatrices,
        std::vector<Matrix>& nodeWorldMatrices,
        std::vector<Matrix>& meshNodeMatrices)
{
    if (clipIndex >= asset.nodeAnimationClips.size()
            || nodeLocalTransforms.size() != asset.nodes.size()
            || nodeLocalMatrices.size() != asset.nodes.size()
            || nodeWorldMatrices.size() != asset.nodes.size()
            || meshNodeMatrices.size()
                    != static_cast<size_t>(
                            std::max(0, asset.model.meshCount))
            || asset.meshNodeBindings.size()
                    != static_cast<size_t>(
                            std::max(0, asset.model.meshCount))) {
        return false;
    }
    const ModelNodeAnimationClip& clip =
            asset.nodeAnimationClips[clipIndex];
    timeSeconds = std::isfinite(timeSeconds)
            ? std::clamp(timeSeconds, 0.0f, clip.durationSeconds)
            : 0.0f;
    for (size_t nodeIndex = 0;
            nodeIndex < asset.nodes.size();
            ++nodeIndex) {
        nodeLocalTransforms[nodeIndex] =
                asset.nodes[nodeIndex].bindLocal;
    }
    for (const ModelNodeAnimationChannel& channel : clip.channels) {
        if (channel.nodeIndex >= nodeLocalTransforms.size()) continue;
        const Vector4 value =
                SampleModelNodeAnimationChannel(channel, timeSeconds);
        ::Transform& transform =
                nodeLocalTransforms[channel.nodeIndex];
        switch (channel.path) {
            case ModelNodeAnimationPath::Translation:
                transform.translation =
                        Vector3{value.x, value.y, value.z};
                break;
            case ModelNodeAnimationPath::Rotation:
                transform.rotation = QuaternionNormalize(value);
                break;
            case ModelNodeAnimationPath::Scale:
                transform.scale = Vector3{value.x, value.y, value.z};
                break;
        }
    }
    for (size_t nodeIndex = 0;
            nodeIndex < asset.nodes.size();
            ++nodeIndex) {
        nodeLocalMatrices[nodeIndex] =
                asset.nodes[nodeIndex].usesMatrix
                ? asset.nodes[nodeIndex].bindLocalMatrix
                : ModelPoseTransformMatrix(
                        nodeLocalTransforms[nodeIndex]);
    }
    for (uint32_t nodeIndex : asset.nodeEvaluationOrder) {
        if (nodeIndex >= asset.nodes.size()) return false;
        const int parentIndex = asset.nodes[nodeIndex].parentIndex;
        nodeWorldMatrices[nodeIndex] = parentIndex >= 0
                ? MatrixMultiply(
                        nodeLocalMatrices[nodeIndex],
                        nodeWorldMatrices[
                                static_cast<size_t>(parentIndex)])
                : nodeLocalMatrices[nodeIndex];
    }
    for (size_t meshIndex = 0;
            meshIndex < meshNodeMatrices.size();
            ++meshIndex) {
        const ModelMeshNodeBinding& binding =
                asset.meshNodeBindings[meshIndex];
        meshNodeMatrices[meshIndex] = binding.nodeIndex >= 0
                        && !binding.skinned
                        && static_cast<size_t>(binding.nodeIndex)
                                < nodeWorldMatrices.size()
                ? MatrixMultiply(
                        binding.inverseBindWorldMatrix,
                        nodeWorldMatrices[
                                static_cast<size_t>(
                                        binding.nodeIndex)])
                : MatrixIdentity();
    }
    return true;
}

bool BuildModelMeshSkinMatrices(
        const ModelAsset& asset,
        const std::vector<Matrix>& nodeWorldMatrices,
        std::vector<Matrix>& meshBoneMatrices)
{
    const int meshCount = std::max(0, asset.model.meshCount);
    const int boneCount = std::max(0, asset.model.skeleton.boneCount);
    const size_t expectedMatrixCount =
            static_cast<size_t>(meshCount)
            * static_cast<size_t>(boneCount);
    if (boneCount <= 0
            || nodeWorldMatrices.size() != asset.nodes.size()
            || asset.meshNodeBindings.size()
                    != static_cast<size_t>(meshCount)
            || asset.gltfSkin.jointNodeIndices.size()
                    != static_cast<size_t>(boneCount)
            || asset.gltfSkin.inverseBindMatrices.size()
                    != static_cast<size_t>(boneCount)
            || meshBoneMatrices.size() != expectedMatrixCount) {
        return false;
    }

    std::fill(
            meshBoneMatrices.begin(),
            meshBoneMatrices.end(),
            MatrixIdentity());
    for (int meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
        const ModelMeshNodeBinding& binding =
                asset.meshNodeBindings[static_cast<size_t>(meshIndex)];
        if (binding.skinIndex != 0 || binding.nodeIndex < 0) continue;
        Matrix* palette = meshBoneMatrices.data()
                + static_cast<size_t>(meshIndex)
                        * static_cast<size_t>(boneCount);
        for (int boneIndex = 0; boneIndex < boneCount; ++boneIndex) {
            const uint32_t jointNodeIndex =
                    asset.gltfSkin.jointNodeIndices[
                            static_cast<size_t>(boneIndex)];
            if (jointNodeIndex >= nodeWorldMatrices.size()) return false;
            const Matrix jointSkinMatrix = MatrixMultiply(
                    asset.gltfSkin.inverseBindMatrices[
                            static_cast<size_t>(boneIndex)],
                    nodeWorldMatrices[jointNodeIndex]);
            palette[boneIndex] = MatrixMultiply(
                    binding.inverseBindWorldMatrix,
                    jointSkinMatrix);
        }
    }
    return true;
}

Matrix AnimatedModelMeshTransform(
        const ModelAsset& asset,
        const std::vector<Matrix>& meshNodeMatrices,
        int meshIndex,
        Matrix authoredTransform)
{
    Matrix transform = MatrixMultiply(
            asset.model.transform,
            authoredTransform);
    if (meshIndex >= 0
            && static_cast<size_t>(meshIndex)
                    < meshNodeMatrices.size()) {
        transform = MatrixMultiply(
                meshNodeMatrices[static_cast<size_t>(meshIndex)],
                transform);
    }
    return transform;
}

bool ComputeModelOrientedBounds(
        const Model& model,
        ModelOrientedBounds& outBounds)
{
    constexpr float epsilon = 0.000001f;
    outBounds = {};

    size_t vertexCapacity = 0;
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const Mesh& mesh = model.meshes[meshIndex];
        if (mesh.vertices != nullptr && mesh.vertexCount > 0) {
            vertexCapacity += static_cast<size_t>(mesh.vertexCount);
        }
    }
    std::vector<Vector2> points;
    points.reserve(vertexCapacity);
    Vector2 minimum{};
    Vector2 maximum{};
    float bottom = 0.0f;
    float top = 0.0f;
    bool foundVertex = false;
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        const Mesh& mesh = model.meshes[meshIndex];
        if (mesh.vertices == nullptr || mesh.vertexCount <= 0) continue;
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
            const Vector2 point{transformed.x, transformed.z};
            points.push_back(point);
            if (!foundVertex) {
                minimum = point;
                maximum = point;
                bottom = transformed.y;
                top = transformed.y;
                foundVertex = true;
            } else {
                minimum.x = std::min(minimum.x, point.x);
                minimum.y = std::min(minimum.y, point.y);
                maximum.x = std::max(maximum.x, point.x);
                maximum.y = std::max(maximum.y, point.y);
                bottom = std::min(bottom, transformed.y);
                top = std::max(top, transformed.y);
            }
        }
    }
    if (!foundVertex || points.size() < 3) return false;

    std::sort(
            points.begin(),
            points.end(),
            [](Vector2 a, Vector2 b) {
                return a.x < b.x || (a.x == b.x && a.y < b.y);
            });
    points.erase(
            std::unique(
                    points.begin(),
                    points.end(),
                    [](Vector2 a, Vector2 b) {
                        return a.x == b.x && a.y == b.y;
                    }),
            points.end());
    if (points.size() < 3) {
        return SetAxisAlignedOrientedBounds(
                minimum, maximum, bottom, top, outBounds);
    }

    std::vector<Vector2> hull(points.size() * 2);
    size_t hullCount = 0;
    for (Vector2 point : points) {
        while (hullCount >= 2
                && Cross(hull[hullCount - 2], hull[hullCount - 1], point)
                        <= epsilon) {
            --hullCount;
        }
        hull[hullCount++] = point;
    }
    const size_t lowerCount = hullCount;
    for (size_t pointIndex = points.size() - 1; pointIndex > 0; --pointIndex) {
        const Vector2 point = points[pointIndex - 1];
        while (hullCount > lowerCount
                && Cross(hull[hullCount - 2], hull[hullCount - 1], point)
                        <= epsilon) {
            --hullCount;
        }
        hull[hullCount++] = point;
    }
    if (hullCount > 1) --hullCount;
    hull.resize(hullCount);
    if (hull.size() < 3) {
        return SetAxisAlignedOrientedBounds(
                minimum, maximum, bottom, top, outBounds);
    }

    float bestArea = std::numeric_limits<float>::infinity();
    float bestAxisAlignment = -1.0f;
    Vector2 bestAxisX{1.0f, 0.0f};
    Vector2 bestAxisZ{0.0f, 1.0f};
    float bestMinimumX = 0.0f;
    float bestMaximumX = 0.0f;
    float bestMinimumZ = 0.0f;
    float bestMaximumZ = 0.0f;
    for (size_t edgeIndex = 0; edgeIndex < hull.size(); ++edgeIndex) {
        const Vector2 a = hull[edgeIndex];
        const Vector2 b = hull[(edgeIndex + 1) % hull.size()];
        Vector2 axisX{b.x - a.x, b.y - a.y};
        const float length = std::sqrt(
                axisX.x * axisX.x + axisX.y * axisX.y);
        if (!(length > epsilon) || !std::isfinite(length)) continue;
        axisX.x /= length;
        axisX.y /= length;
        if (axisX.x < 0.0f
                || (std::fabs(axisX.x) <= epsilon && axisX.y < 0.0f)) {
            axisX.x = -axisX.x;
            axisX.y = -axisX.y;
        }
        const Vector2 axisZ{-axisX.y, axisX.x};
        float minimumX = std::numeric_limits<float>::infinity();
        float maximumX = -std::numeric_limits<float>::infinity();
        float minimumZ = std::numeric_limits<float>::infinity();
        float maximumZ = -std::numeric_limits<float>::infinity();
        for (Vector2 point : hull) {
            const float projectedX = point.x * axisX.x + point.y * axisX.y;
            const float projectedZ = point.x * axisZ.x + point.y * axisZ.y;
            minimumX = std::min(minimumX, projectedX);
            maximumX = std::max(maximumX, projectedX);
            minimumZ = std::min(minimumZ, projectedZ);
            maximumZ = std::max(maximumZ, projectedZ);
        }
        const float area = (maximumX - minimumX) * (maximumZ - minimumZ);
        const float alignment = std::fabs(axisX.x);
        const float tieTolerance = std::isfinite(bestArea)
                ? epsilon * std::max(1.0f, std::fabs(bestArea))
                : 0.0f;
        if (area < bestArea - tieTolerance
                || (std::fabs(area - bestArea) <= tieTolerance
                        && alignment > bestAxisAlignment + epsilon)) {
            bestArea = area;
            bestAxisAlignment = alignment;
            bestAxisX = axisX;
            bestAxisZ = axisZ;
            bestMinimumX = minimumX;
            bestMaximumX = maximumX;
            bestMinimumZ = minimumZ;
            bestMaximumZ = maximumZ;
        }
    }
    if (!std::isfinite(bestArea)) {
        return SetAxisAlignedOrientedBounds(
                minimum, maximum, bottom, top, outBounds);
    }

    const Vector2 half{
            (bestMaximumX - bestMinimumX) * 0.5f,
            (bestMaximumZ - bestMinimumZ) * 0.5f};
    if (!(half.x > epsilon)
            || !(half.y > epsilon)
            || !(top > bottom + epsilon)) {
        return SetAxisAlignedOrientedBounds(
                minimum, maximum, bottom, top, outBounds);
    }
    const float centerX = (bestMinimumX + bestMaximumX) * 0.5f;
    const float centerZ = (bestMinimumZ + bestMaximumZ) * 0.5f;
    outBounds = ModelOrientedBounds{
            Vector2{
                    bestAxisX.x * centerX + bestAxisZ.x * centerZ,
                    bestAxisX.y * centerX + bestAxisZ.y * centerZ},
            bestAxisX,
            bestAxisZ,
            half,
            bottom,
            top};
    return true;
}

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
        const char* path,
        ModelLoadFlags flags)
{
    if (key == nullptr || path == nullptr || key[0] == '\0' || path[0] == '\0') {
        return NullModelHandle();
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.index >= scopeData.size()) {
        return NullModelHandle();
    }

    ModelScopeData& data = scopeData[scope.index];
    const std::string requestKey = MakeRequestKey(key, path, flags);
    const auto existing = data.modelByRequest.find(requestKey);
    if (existing != data.modelByRequest.end()) {
        return existing->second;
    }

    assert(modelSlots.size() < std::numeric_limits<uint32_t>::max());
    ModelSlot slot;
    slot.state = ModelState::Queued;
    slot.key = key;
    slot.path = path;
    slot.flags = flags;
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
        ModelLoadFlags flags = ModelLoad_None;
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
            flags = modelSlots[handle.index].flags;
        }

        // raylib glTF loading performs GPU mesh and texture creation. Keep the
        // entire operation on the main thread.
        Model loaded = LoadModel(path.c_str());
        const bool loadedModel = IsModelValid(loaded);
        ModelAnimation* loadedAnimations = nullptr;
        int loadedAnimationCount = 0;
        if (loadedModel && (flags & ModelLoad_Animations) != 0) {
            loadedAnimations = LoadModelAnimations(path.c_str(), &loadedAnimationCount);
        }
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
        ModelAsset nodeAnimationAsset;
        BoundingBox localBounds{};
        BoundingBox animatedLocalBounds{};
        ModelOrientedBounds localCollisionBounds{};
        bool hasLocalBounds = false;
        bool hasAnimatedLocalBounds = false;
        bool hasLocalCollisionBounds = false;
        if (loadedModel) {
            parsed = ParseModelMaterials(path, loaded.materialCount);
            if ((flags & ModelLoad_Animations) != 0) {
                nodeAnimationAsset.model = loaded;
                nodeAnimationAsset.animationCount = loadedAnimationCount;
                LoadModelNodeAnimationsFromGltf(
                        path.c_str(),
                        nodeAnimationAsset);
            }
            GenerateMissingTangents(loaded);
            hasLocalBounds = ComputeModelLocalBounds(loaded, localBounds);
            hasLocalCollisionBounds = ComputeModelOrientedBounds(
                    loaded,
                    localCollisionBounds);
            hasAnimatedLocalBounds = ComputeAnimatedModelLocalBounds(
                    loaded,
                    loadedAnimations,
                    loadedAnimationCount,
                    localBounds,
                    hasLocalBounds,
                    animatedLocalBounds);
            BoundingBox gltfAnimatedBounds{};
            const bool hasGltfAnimatedBounds =
                    ComputeGltfAnimatedModelLocalBounds(
                            nodeAnimationAsset,
                            localBounds,
                            hasLocalBounds,
                            gltfAnimatedBounds);
            if (hasGltfAnimatedBounds) {
                if (hasAnimatedLocalBounds) {
                    animatedLocalBounds.min.x = std::min(
                            animatedLocalBounds.min.x,
                            gltfAnimatedBounds.min.x);
                    animatedLocalBounds.min.y = std::min(
                            animatedLocalBounds.min.y,
                            gltfAnimatedBounds.min.y);
                    animatedLocalBounds.min.z = std::min(
                            animatedLocalBounds.min.z,
                            gltfAnimatedBounds.min.z);
                    animatedLocalBounds.max.x = std::max(
                            animatedLocalBounds.max.x,
                            gltfAnimatedBounds.max.x);
                    animatedLocalBounds.max.y = std::max(
                            animatedLocalBounds.max.y,
                            gltfAnimatedBounds.max.y);
                    animatedLocalBounds.max.z = std::max(
                            animatedLocalBounds.max.z,
                            gltfAnimatedBounds.max.z);
                } else {
                    animatedLocalBounds = gltfAnimatedBounds;
                    hasAnimatedLocalBounds = true;
                }
            }
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
                    pendingUnloads.push_back(PendingModelUnload{
                            loaded, loadedAnimations, loadedAnimationCount});
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
                    PopulateModelMaterialTextureInfo(
                            path,
                            loaded,
                            parsed.materials);
                    slot.asset.model = loaded;
                    slot.asset.animations = loadedAnimations;
                    slot.asset.animationCount = loadedAnimationCount;
                    slot.asset.nodes = std::move(
                            nodeAnimationAsset.nodes);
                    slot.asset.nodeEvaluationOrder = std::move(
                            nodeAnimationAsset.nodeEvaluationOrder);
                    slot.asset.meshNodeBindings = std::move(
                            nodeAnimationAsset.meshNodeBindings);
                    slot.asset.gltfSkin = std::move(
                            nodeAnimationAsset.gltfSkin);
                    slot.asset.nodeAnimationClips = std::move(
                            nodeAnimationAsset.nodeAnimationClips);
                    slot.asset.materials = std::move(parsed.materials);
                    slot.asset.localBounds = localBounds;
                    slot.asset.animatedLocalBounds = animatedLocalBounds;
                    slot.asset.localCollisionBounds = localCollisionBounds;
                    slot.asset.hasLocalBounds = hasLocalBounds;
                    slot.asset.hasAnimatedLocalBounds = hasAnimatedLocalBounds;
                    slot.asset.hasLocalCollisionBounds =
                            hasLocalCollisionBounds;
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

const char* ModelMaterialTextureRoleName(ModelMaterialTextureRole role)
{
    switch (role) {
        case ModelMaterialTextureRole::BaseColor: return "base color";
        case ModelMaterialTextureRole::Metallic: return "metallic (B->R)";
        case ModelMaterialTextureRole::Normal: return "normal";
        case ModelMaterialTextureRole::Roughness: return "roughness (G->R)";
        case ModelMaterialTextureRole::Occlusion: return "occlusion";
        case ModelMaterialTextureRole::Emissive: return "emissive";
        case ModelMaterialTextureRole::Count: break;
    }
    return "unknown";
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
                pendingUnloads.push_back(PendingModelUnload{
                        slot.asset.model,
                        slot.asset.animations,
                        slot.asset.animationCount});
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

std::string ModelAssets::MakeRequestKey(
        const char* key,
        const char* path,
        ModelLoadFlags flags)
{
    (void)key;
    return std::string(path) + "#flags:" + std::to_string(static_cast<uint32_t>(flags));
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
        pendingUnloads.push_back(PendingModelUnload{
                slot.asset.model,
                slot.asset.animations,
                slot.asset.animationCount});
        slot.asset = {};
    }
    slot.state = ModelState::QueuedForUnload;
    ++slot.generation;
}

void ModelAssets::UnloadReadyModels()
{
    std::vector<PendingModelUnload> unloads;
    std::vector<Texture2D> textureUnloads;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        unloads.swap(pendingUnloads);
        textureUnloads.swap(pendingTextureUnloads);
    }
    for (PendingModelUnload& unload : unloads) {
        if (unload.animations != nullptr && unload.animationCount > 0) {
            UnloadModelAnimations(unload.animations, unload.animationCount);
        }
        if (IsModelValid(unload.model)) {
            UnloadGeometryModel(unload.model);
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
