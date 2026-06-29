#pragma once

#include "sector_demo/SectorMeshTypes.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace game {

struct SectorGeneratedGeometry;
struct SectorLightmapLayout;
struct SectorTopologyMap;
struct RuntimePortalVisibilityResult;

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
SectorMeshBatchDataResult BuildSectorMeshDrawRecordData(
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout* lightmapLayout = nullptr);
std::vector<SectorReceiverBounds> BuildSectorReceiverBounds(
        const SectorMeshBatchDataResult& drawRecordData);

SectorMeshBuildResult BuildSectorMeshes(
        const SectorTopologyMap& map,
        const SectorLightmapLayout* lightmapLayout = nullptr,
        std::string* outError = nullptr);
bool ShouldDrawSectorMeshRecordForVisibility(
        const SectorMeshBatch& record,
        const RuntimePortalVisibilityResult& visibility);
bool ShouldDrawEmissiveBloomSectorMeshRecordForVisibility(
        const SectorMeshBatch& record,
        const RuntimePortalVisibilityResult& visibility);
size_t CountSectorMeshDrawRecordsForVisibility(
        const std::vector<SectorMeshBatch>& records,
        const RuntimePortalVisibilityResult& visibility);
void UnloadSectorMeshes(SectorMeshBuildResult& buildResult);

} // namespace game
