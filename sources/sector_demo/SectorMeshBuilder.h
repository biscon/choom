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
    Vector2 lightmapUv = {};
    Color color = WHITE;
};

struct SectorMeshBatchData {
    std::string textureId;
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
