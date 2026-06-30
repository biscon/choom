#pragma once

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct SectorMeshBatch {
    int sectorId = -1;
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

struct SectorReceiverBounds {
    int sectorId = -1;
    Vector3 min = {};
    Vector3 max = {};
};

struct SectorMeshBuildResult {
    std::vector<SectorMeshBatch> batches;
    std::vector<SectorMeshBatch> sectorDrawRecords;
    std::vector<SectorReceiverBounds> sectorReceiverBounds;
    int vertexCount = 0;
    int triangleCount = 0;
};

} // namespace game
