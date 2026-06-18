#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace game {

struct SectorPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct SectorEdgePartUvOverride {
    Vector2 uvScale = {1.0f, 1.0f};
    Vector2 uvOffset = {0.0f, 0.0f};
    bool hasUvScale = false;
    bool hasUvOffset = false;
};

struct SectorSurfaceUvOverride {
    Vector2 uvScale = {1.0f, 1.0f};
    Vector2 uvOffset = {0.0f, 0.0f};
    bool hasUvScale = false;
    bool hasUvOffset = false;
};

struct SectorEdgeOverride {
    int edgeIndex = -1;

    std::string wallTextureId;
    std::string lowerWallTextureId;
    std::string upperWallTextureId;

    bool hasWallTexture = false;
    bool hasLowerWallTexture = false;
    bool hasUpperWallTexture = false;

    SectorEdgePartUvOverride wallUv;
    SectorEdgePartUvOverride lowerUv;
    SectorEdgePartUvOverride upperUv;
};

struct SectorDefinition {
    std::string id;
    std::vector<SectorPoint> points;
    float floorZ = 0.0f;
    float ceilingZ = 0.0f;
    std::string floorTextureId;
    std::string ceilingTextureId;
    std::string wallTextureId;
    std::string lowerWallTextureId;
    std::string upperWallTextureId;
    Color ambientColor = WHITE;
    float ambientIntensity = 1.0f;
    SectorSurfaceUvOverride floorUv;
    SectorSurfaceUvOverride ceilingUv;
    std::vector<SectorEdgeOverride> edgeOverrides;
};

enum class SectorTextureFilter {
    Point,
    Bilinear
};

struct SectorTextureDefinition {
    std::string id;
    std::string path;
    SectorTextureFilter filter = SectorTextureFilter::Bilinear;
};

struct SectorStaticPointLight {
    std::string id;
    Vector3 position = {0.0f, 1.8f, 0.0f};
    Color color = WHITE;
    float intensity = 1.0f;
    float radius = 8.0f;
    float sourceRadius = 0.0f;
};

struct SectorLightmapBakeSettings {
    float ambientOcclusionRadius = 1.25f;
    float ambientOcclusionStrength = 0.55f;
    float indirectBounceRadius = 4.0f;
    float indirectBounceStrength = 0.20f;
};

struct SectorLightmapMetadata {
    std::string path;
    int width = 0;
    int height = 0;
    std::string sourceHash;
};

struct SectorMap {
    std::unordered_map<std::string, SectorTextureDefinition> texturesById;
    std::vector<SectorDefinition> sectors;
    std::vector<SectorStaticPointLight> staticLights;
    SectorLightmapBakeSettings lightmapSettings;
    SectorLightmapMetadata bakedLightmap;
    Vector3 playerStartPosition = {0.0f, 1.6f, 0.0f};
    float playerStartYawRadians = 0.0f;
};

struct SectorMeshBatch {
    std::string textureId;
    Mesh mesh = {};
    int vertexCount = 0;
    int triangleCount = 0;
};

struct SectorMeshBuildResult {
    std::vector<SectorMeshBatch> batches;
    int vertexCount = 0;
    int triangleCount = 0;
};

struct SectorTextureBinding {
    std::string textureId;
    engine::TextureHandle handle = engine::NullTextureHandle();
};

struct EffectiveEdgePartSettings {
    std::string textureId;
    Vector2 uvScale = {1.0f, 1.0f};
    Vector2 uvOffset = {0.0f, 0.0f};
};

struct EffectiveEdgeSettings {
    EffectiveEdgePartSettings wall;
    EffectiveEdgePartSettings lower;
    EffectiveEdgePartSettings upper;
};

struct EdgeNeighborInfo {
    bool hasNeighbor = false;
    int sectorIndex = -1;
    int edgeIndex = -1;
};

} // namespace game
