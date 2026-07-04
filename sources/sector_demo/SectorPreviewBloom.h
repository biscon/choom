#pragma once

#include "engine/assets/AssetHandles.h"
#include "sector_demo/SectorMeshTypes.h"

#include <raylib.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace engine {
class AssetManager;
}

namespace game {

struct RuntimePortalVisibilityResult;

class SectorPreviewBloom {
public:
    void Shutdown();
    void ApplyEmissiveDecalBloomToScene(
            engine::AssetManager& assets,
            bool previewInitialized,
            const Camera3D& camera,
            const std::vector<SectorMeshBatch>& sectorDrawRecords,
            const RuntimePortalVisibilityResult& visibilityResult,
            const std::unordered_map<std::string, engine::TextureHandle>& textureHandlesById,
            RenderTexture2D& sceneTarget);

    bool IsLoaded() const;

private:
    bool EnsureResources(int sceneWidth, int sceneHeight);
    void RenderBloomSource(
            engine::AssetManager& assets,
            const Camera3D& camera,
            const std::vector<SectorMeshBatch>& sectorDrawRecords,
            const RuntimePortalVisibilityResult& visibilityResult,
            const std::unordered_map<std::string, engine::TextureHandle>& textureHandlesById);
    engine::TextureHandle TextureForId(
            const std::unordered_map<std::string, engine::TextureHandle>& textureHandlesById,
            const std::string& textureId) const;

    Material sourceMaterial = {};
    Texture2D defaultMaterialTexture = {};
    bool sourceMaterialLoaded = false;
    int hasDecalLoc = -1;
    int decalOpacityLoc = -1;
    int decalEmissiveLoc = -1;
    int decalTintLoc = -1;
    int decalIntensityLoc = -1;
    Shader blurShader = {};
    Shader compositeShader = {};
    int blurTexelSizeLoc = -1;
    int blurDirectionLoc = -1;
    int compositeStrengthLoc = -1;
    int compositeBloomTextureLoc = -1;
    RenderTexture2D sceneCopy = {};
    RenderTexture2D source = {};
    RenderTexture2D blurA = {};
    RenderTexture2D blurB = {};
    int sceneWidth = 0;
    int sceneHeight = 0;
    int targetWidth = 0;
    int targetHeight = 0;
};

} // namespace game
