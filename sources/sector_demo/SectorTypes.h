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

struct SectorEdgeOverride {
    int edgeIndex = -1;

    std::string wallTextureId;
    std::string lowerWallTextureId;
    std::string upperWallTextureId;

    bool hasWallTexture = false;
    bool hasLowerWallTexture = false;
    bool hasUpperWallTexture = false;

    Vector2 uvScale = {1.0f, 1.0f};
    Vector2 uvOffset = {0.0f, 0.0f};
    bool hasUvScale = false;
    bool hasUvOffset = false;
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
    std::vector<SectorEdgeOverride> edgeOverrides;
};

struct SectorMap {
    std::unordered_map<std::string, std::string> texturePathsById;
    std::vector<SectorDefinition> sectors;
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

struct EffectiveEdgeSettings {
    std::string wallTextureId;
    std::string lowerWallTextureId;
    std::string upperWallTextureId;
    Vector2 uvScale = {1.0f, 1.0f};
    Vector2 uvOffset = {0.0f, 0.0f};
};

struct EdgeNeighborInfo {
    bool hasNeighbor = false;
    int sectorIndex = -1;
    int edgeIndex = -1;
};

} // namespace game
