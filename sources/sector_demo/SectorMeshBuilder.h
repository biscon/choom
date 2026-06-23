#pragma once

#include "sector_demo/SectorMeshTypes.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct SectorGeneratedGeometry;
struct SectorLightmapLayout;
struct SectorTopologyMap;

struct SectorMeshBatchVertex {
    Vector3 position = {};
    Vector3 normal = {};
    Vector2 uv = {};
    Vector2 decalUv = {};
    Vector2 lightmapUv = {};
    float decalOpacity = 1.0f;
    bool decalEmissive = false;
    Vector3 decalTint = {1.0f, 1.0f, 1.0f};
    float decalBloomIntensity = 1.0f;
    Color color = WHITE;
};

struct SectorMeshBatchData {
    std::string textureId;
    std::string decalTextureId;
    float decalOpacity = 1.0f;
    bool decalEmissive = false;
    Vector3 decalTint = {1.0f, 1.0f, 1.0f};
    float decalBloomIntensity = 1.0f;
    bool alphaTest = false;
    float alphaCutoff = 0.5f;
    bool receivesLightmap = true;
    std::vector<SectorMeshBatchVertex> vertices;
    int vertexCount = 0;
    int triangleCount = 0;
};

struct SectorMeshBatchDataResult {
    std::vector<SectorMeshBatchData> batches;
    int vertexCount = 0;
    int triangleCount = 0;
};

SectorMeshBatchDataResult BuildSectorMeshBatchData(
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout* lightmapLayout = nullptr);

SectorMeshBuildResult BuildSectorMeshes(
        const SectorTopologyMap& map,
        const SectorLightmapLayout* lightmapLayout = nullptr,
        std::string* outError = nullptr);
void UnloadSectorMeshes(SectorMeshBuildResult& buildResult);

} // namespace game
