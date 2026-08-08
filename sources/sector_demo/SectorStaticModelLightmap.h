#pragma once

#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
}

namespace game {

enum class SectorLightmapStatus;

constexpr int kSectorStaticModelLightmapSidecarVersion = 2;
constexpr const char* kSectorStaticModelLightmapSidecarFormat =
        "staticModelUvRemapF32LE";

struct SectorStaticModelLightmapMesh {
    int originalVertexCount = 0;
    bool preservesAuthoredUv2 = false;
    int usableWidth = 0;
    int usableHeight = 0;
    std::vector<uint32_t> sourceVertexIndices;
    std::vector<Vector3> importedPositions;
    std::vector<Vector3> importedNormals;
    std::vector<Vector2> localLightmapUvs;
    std::vector<uint32_t> indices;
};

struct SectorStaticModelLightmapModel {
    std::string modelPath;
    std::string geometryFingerprint;
    std::vector<SectorStaticModelLightmapMesh> meshes;
};

struct SectorStaticModelLightmapMeshPlacement {
    int atlasIndex = -1;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int usableX = 0;
    int usableY = 0;
    int usableWidth = 0;
    int usableHeight = 0;
    Vector2 atlasScale = {};
    Vector2 atlasBias = {};
};

struct SectorStaticModelLightmapObject {
    int objectId = 0;
    int modelIndex = -1;
    int containingSectorId = -1;
    Vector3 worldPosition = {};
    float yawRadians = 0.0f;
    float rotationXRadians = 0.0f;
    float rotationZRadians = 0.0f;
    float scale = 1.0f;
    std::vector<SectorStaticModelLightmapMeshPlacement> meshPlacements;
};

struct SectorStaticModelLightmapData {
    std::string sourceHash;
    std::vector<SectorStaticModelLightmapModel> models;
    std::vector<SectorStaticModelLightmapObject> objects;
};

struct SectorStaticModelLightmapPackCursor {
    int atlasIndex = 0;
    int shelfX = 0;
    int shelfY = 0;
    int shelfHeight = 0;
};

bool HasAssignedSectorStaticModels(const SectorTopologyMap& map);

bool RefreshSectorStaticModelGeometryFingerprints(
        SectorTopologyMap& map,
        std::string& outError);

bool CopySectorStaticModelForLightmap(
        const std::string& modelPath,
        const std::string& geometryFingerprint,
        const Model& model,
        SectorStaticModelLightmapModel& outModel,
        std::string& outError);

using SectorStaticModelReadyModelLookup =
        std::function<const Model*(const std::string& resolvedPath)>;

// Ready models are borrowed from existing asset scopes when possible. The
// temporary bake scope is created only for paths that are not already loaded.
bool PrepareSectorStaticModelsForLightmapBake(
        SectorTopologyMap& map,
        engine::AssetManager& assets,
        const std::function<bool()>& isCancellationRequested,
        SectorStaticModelLightmapData& outData,
        std::string& outError,
        const SectorStaticModelReadyModelLookup& readyModelLookup = {});

bool PackSectorStaticModelLightmapCharts(
        SectorStaticModelLightmapData& data,
        int atlasWidth,
        int atlasHeight,
        int gutter,
        SectorStaticModelLightmapPackCursor cursor,
        std::string& outError);

bool WriteSectorStaticModelLightmapSidecar(
        const std::string& path,
        const SectorStaticModelLightmapData& data,
        std::string& outError);

bool ReadSectorStaticModelLightmapSidecar(
        const std::string& path,
        const SectorBakedStaticModelLightmapMetadata* expectedMetadata,
        SectorStaticModelLightmapData& outData,
        std::string& outError);

bool AreSectorStaticModelLightmapAtlasIndicesValid(
        const SectorStaticModelLightmapData& data,
        int atlasCount);

SectorLightmapStatus GetSectorStaticModelLightmapStatus(
        const SectorTopologyMap& map);

std::string MakeSectorStaticModelSidecarPathForLightmapPath(
        const std::string& lightmapPath);

} // namespace game
