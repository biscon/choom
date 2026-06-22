#pragma once

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct SectorMeshBatch {
    std::string textureId;
    std::string decalTextureId;
    float decalOpacity = 1.0f;
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
