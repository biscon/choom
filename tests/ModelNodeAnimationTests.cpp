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

} // namespace

int main()
{
    TestGeneratedRigidAnimationImportAndSampling();
    if (failures != 0) {
        std::cerr << failures << " model node animation test(s) failed\n";
        return 1;
    }
    std::cout << "Model node animation tests passed\n";
    return 0;
}
