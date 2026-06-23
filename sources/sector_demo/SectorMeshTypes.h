#pragma once

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct SectorMeshBatch {
    std::string textureId;
    std::string decalTextureId;
    float decalOpacity = 1.0f;
    bool decalEmissive = false;
    Vector3 decalTint = {1.0f, 1.0f, 1.0f};
    float decalBloomIntensity = 1.0f;
    bool alphaTest = false;
    float alphaCutoff = 0.5f;
    bool receivesLightmap = true;
    Mesh mesh = {};
    int vertexCount = 0;
    int triangleCount = 0;
};

struct SectorMeshBuildResult {
    std::vector<SectorMeshBatch> batches;
    int vertexCount = 0;
    int triangleCount = 0;
};

} // namespace game
