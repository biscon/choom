#include "engine/assets/ModelAssets.h"

#include <raymath.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    ++failures;
}

bool Near(float left, float right, float epsilon = 0.0001f)
{
    return std::fabs(left - right) <= epsilon;
}

bool Near(Vector3 left, Vector3 right, float epsilon = 0.0001f)
{
    return Near(left.x, right.x, epsilon)
            && Near(left.y, right.y, epsilon)
            && Near(left.z, right.z, epsilon);
}

void AppendFloat(std::vector<uint8_t>& bytes, float value)
{
    const size_t offset = bytes.size();
    bytes.resize(offset + sizeof(value));
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void WriteGeneratedRigidAnimationGltf(const std::filesystem::path& directory)
{
    std::vector<uint8_t> bytes;
    for (float value : {0.0f, 1.0f}) AppendFloat(bytes, value);
    for (float value : {0.0f, 0.0f, 0.0f, 0.0f, 2.0f, 0.0f}) {
        AppendFloat(bytes, value);
    }
    for (float value : {0.0f, 0.0f, 0.0f, 1.0f,
                        0.0f, 0.0f, 1.0f, 0.0f}) {
        AppendFloat(bytes, value);
    }
    for (float value : {
                0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 3.0f, 3.0f, 3.0f, 0.0f, 0.0f, 0.0f}) {
        AppendFloat(bytes, value);
    }

    std::ofstream binary(directory / "animation.bin", std::ios::binary);
    binary.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));

    std::ofstream gltf(directory / "animation.gltf");
    gltf << R"json({
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "animation.bin", "byteLength": 136}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 8},
    {"buffer": 0, "byteOffset": 8, "byteLength": 24},
    {"buffer": 0, "byteOffset": 32, "byteLength": 32},
    {"buffer": 0, "byteOffset": 64, "byteLength": 72}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 2, "type": "SCALAR"},
    {"bufferView": 1, "componentType": 5126, "count": 2, "type": "VEC3"},
    {"bufferView": 2, "componentType": 5126, "count": 2, "type": "VEC4"},
    {"bufferView": 3, "componentType": 5126, "count": 6, "type": "VEC3"}
  ],
  "meshes": [{"primitives": [{"attributes": {}}]}],
  "nodes": [
    {"translation": [1, 0, 0], "children": [1]},
    {"mesh": 0}
  ],
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "animations": [{
    "name": "lever",
    "samplers": [
      {"input": 0, "output": 1, "interpolation": "LINEAR"},
      {"input": 0, "output": 2, "interpolation": "STEP"},
      {"input": 0, "output": 3, "interpolation": "CUBICSPLINE"}
    ],
    "channels": [
      {"sampler": 0, "target": {"node": 1, "path": "translation"}},
      {"sampler": 1, "target": {"node": 1, "path": "rotation"}},
      {"sampler": 2, "target": {"node": 1, "path": "scale"}}
    ]
  }]
})json";
}

void WriteGeneratedSkinnedAnimationGltf(
        const std::filesystem::path& directory)
{
    std::vector<uint8_t> bytes;
    for (float value : {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                -1.0f, 0.0f, 0.0f, 1.0f}) {
        AppendFloat(bytes, value);
    }
    for (float value : {0.0f, 1.0f}) AppendFloat(bytes, value);
    for (float value : {
                0.0f, 0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 0.70710678f, 0.70710678f}) {
        AppendFloat(bytes, value);
    }

    std::ofstream binary(directory / "skin.bin", std::ios::binary);
    binary.write(
            reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));

    std::ofstream gltf(directory / "skin.gltf");
    gltf << R"json({
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "skin.bin", "byteLength": 104}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 64},
    {"buffer": 0, "byteOffset": 64, "byteLength": 8},
    {"buffer": 0, "byteOffset": 72, "byteLength": 32}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 1, "type": "MAT4"},
    {"bufferView": 1, "componentType": 5126, "count": 2, "type": "SCALAR"},
    {"bufferView": 2, "componentType": 5126, "count": 2, "type": "VEC4"}
  ],
  "meshes": [
    {"primitives": [{"attributes": {}}]},
    {"primitives": [{"attributes": {}}]}
  ],
  "skins": [{
    "inverseBindMatrices": 0,
    "skeleton": 1,
    "joints": [1]
  }],
  "nodes": [
    {
      "rotation": [0.70710678, 0, 0, 0.70710678],
      "scale": [2, 2, 2],
      "children": [1, 2, 3]
    },
    {"translation": [1, 0, 0]},
    {"mesh": 0, "skin": 0},
    {"mesh": 1, "skin": 0, "translation": [0, 3, 0]}
  ],
  "scenes": [{"nodes": [0]}],
  "scene": 0,
  "animations": [{
    "name": "fan",
    "samplers": [
      {"input": 1, "output": 2, "interpolation": "LINEAR"}
    ],
    "channels": [
      {"sampler": 0, "target": {"node": 1, "path": "rotation"}}
    ]
  }]
})json";
}

void TestGeneratedRigidAnimationImportAndSampling()
{
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / "engine_model_node_animation_tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    WriteGeneratedRigidAnimationGltf(directory);

    engine::ModelAsset asset;
    asset.model.meshCount = 1;
    Check(
            engine::LoadModelNodeAnimationsFromGltf(
                    (directory / "animation.gltf").string().c_str(),
                    asset),
            "generated glTF node data should import");
    Check(asset.nodes.size() == 2, "node hierarchy should import");
    Check(asset.nodeEvaluationOrder.size() == 2,
            "node evaluation order should include every node");
    Check(asset.meshNodeBindings.size() == 1
                    && asset.meshNodeBindings[0].nodeIndex == 1,
            "raylib mesh order should map to the source glTF node");
    Check(engine::ModelAnimationClipCount(asset) == 1,
            "rigid glTF animation should appear in the shared clip catalog");
    Check(std::string{engine::ModelAnimationClipName(asset, 0)} == "lever",
            "rigid clip name should be preserved");
    Check(asset.nodeAnimationClips[0].channels.size() == 3,
            "translation, rotation, and scale channels should import");
    Check(asset.nodeAnimationClips[0].channels[0].interpolation
                    == engine::ModelNodeAnimationInterpolation::Linear,
            "LINEAR interpolation should import");
    Check(asset.nodeAnimationClips[0].channels[1].interpolation
                    == engine::ModelNodeAnimationInterpolation::Step,
            "STEP interpolation should import");
    Check(asset.nodeAnimationClips[0].channels[2].interpolation
                    == engine::ModelNodeAnimationInterpolation::CubicSpline,
            "CUBICSPLINE interpolation should import");

    std::vector<Transform> local(asset.nodes.size());
    std::vector<Matrix> localMatrices(asset.nodes.size());
    std::vector<Matrix> worldMatrices(asset.nodes.size());
    std::vector<Matrix> meshMatrices(1);
    Check(engine::SampleModelNodeAnimation(
                    asset,
                    0,
                    0.5f,
                    local,
                    localMatrices,
                    worldMatrices,
                    meshMatrices),
            "imported rigid clip should sample with pre-sized pose storage");
    Check(Near(local[1].translation.y, 1.0f),
            "LINEAR translation should interpolate at half time");
    Check(Near(local[1].rotation.w, 1.0f),
            "STEP rotation should retain the preceding key");
    Check(Near(local[1].scale.x, 2.0f)
                    && Near(local[1].scale.y, 2.0f)
                    && Near(local[1].scale.z, 2.0f),
            "CUBICSPLINE scale should evaluate its Hermite curve");
    const Vector3 movedBindPoint = Vector3Transform(
            Vector3{1.0f, 0.0f, 0.0f},
            meshMatrices[0]);
    Check(Near(movedBindPoint.x, 1.0f) && Near(movedBindPoint.y, 1.0f),
            "mesh transform should be bind-relative and preserve parent bind placement");

    std::filesystem::remove_all(directory);
}

void TestGeneratedSkinnedAnimationImportAndSampling()
{
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / "engine_model_skin_animation_tests";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    WriteGeneratedSkinnedAnimationGltf(directory);

    engine::ModelAsset asset;
    asset.model.meshCount = 2;
    asset.model.skeleton.boneCount = 1;
    Check(
            engine::LoadModelNodeAnimationsFromGltf(
                    (directory / "skin.gltf").string().c_str(),
                    asset),
            "generated skinned glTF data should import");
    Check(asset.gltfSkin.jointNodeIndices.size() == 1
                    && asset.gltfSkin.jointNodeIndices[0] == 1,
            "skin joint should map to its source glTF node");
    Check(asset.gltfSkin.inverseBindMatrices.size() == 1,
            "inverse-bind matrix should import");
    Check(asset.meshNodeBindings.size() == 2
                    && asset.meshNodeBindings[0].skinIndex == 0
                    && asset.meshNodeBindings[1].skinIndex == 0,
            "each skinned mesh should retain its skin association");

    std::vector<Transform> local(asset.nodes.size());
    std::vector<Matrix> localMatrices(asset.nodes.size());
    std::vector<Matrix> worldMatrices(asset.nodes.size());
    std::vector<Matrix> meshMatrices(2);
    std::vector<Matrix> meshBoneMatrices(2);
    Check(engine::SampleModelNodeAnimation(
                    asset,
                    0,
                    0.0f,
                    local,
                    localMatrices,
                    worldMatrices,
                    meshMatrices)
                    && engine::BuildModelMeshSkinMatrices(
                            asset,
                            worldMatrices,
                            meshBoneMatrices),
            "bind-time glTF skin palette should evaluate");

    const Vector3 firstBakedVertex{4.0f, 0.0f, 0.0f};
    const Vector3 secondBakedVertex{4.0f, 0.0f, 6.0f};
    Check(Near(
                    Vector3Transform(
                            firstBakedVertex,
                            meshBoneMatrices[0]),
                    Vector3{4.0f, 0.0f, 0.0f}),
            "bind palette should preserve a normally bound mesh");
    Check(Near(
                    Vector3Transform(
                            secondBakedVertex,
                            meshBoneMatrices[1]),
                    Vector3{4.0f, 0.0f, 0.0f}),
            "per-mesh palette should remove each baked mesh-node transform");

    Check(engine::SampleModelNodeAnimation(
                    asset,
                    0,
                    1.0f,
                    local,
                    localMatrices,
                    worldMatrices,
                    meshMatrices)
                    && engine::BuildModelMeshSkinMatrices(
                            asset,
                            worldMatrices,
                            meshBoneMatrices),
            "animated glTF skin palette should evaluate");
    const Vector3 expectedRotatedVertex{2.0f, 0.0f, 2.0f};
    Check(Near(
                    Vector3Transform(
                            firstBakedVertex,
                            meshBoneMatrices[0]),
                    expectedRotatedVertex),
            "joint should rotate around its translated pivot on the ancestor-transformed axis");
    Check(Near(
                    Vector3Transform(
                            secondBakedVertex,
                            meshBoneMatrices[1]),
                    expectedRotatedVertex),
            "separate mesh-node bind transforms should produce the same skinned pose");

    std::filesystem::remove_all(directory);
}

} // namespace

int main()
{
    TestGeneratedRigidAnimationImportAndSampling();
    TestGeneratedSkinnedAnimationImportAndSampling();
    if (failures != 0) {
        std::cerr << failures << " model node animation test(s) failed\n";
        return 1;
    }
    std::cout << "Model node animation tests passed\n";
    return 0;
}
