#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/render/RenderTarget.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/renderer/SectorFog.h"

#include <raylib.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace engine {
class AssetManager;
}

namespace game {

struct RuntimePortalVisibilityResult;

class SectorBloomRenderer {
public:
    void Shutdown();
    void ApplyEmissiveDecalBloomToScene(
            engine::AssetManager& assets,
            bool previewInitialized,
            const Camera3D& camera,
            const std::vector<SectorMeshBatch>& sectorDrawRecords,
            const RuntimePortalVisibilityResult& visibilityResult,
            const std::unordered_map<std::string, engine::TextureHandle>& textureHandlesById,
            RenderTexture2D& sceneTarget,
            const SectorFogRenderContext& fogContext);

    bool IsLoaded() const;

private:
    bool EnsureResources(int sceneWidth, int sceneHeight);
    void RenderBloomSource(
            engine::AssetManager& assets,
            const Camera3D& camera,
            const std::vector<SectorMeshBatch>& sectorDrawRecords,
            const RuntimePortalVisibilityResult& visibilityResult,
            const std::unordered_map<std::string, engine::TextureHandle>& textureHandlesById,
            const SectorFogRenderContext& fogContext);
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
    SectorFogShaderLocations fogShaderLocations;
    Shader blurShader = {};
    Shader compositeShader = {};
    int blurTexelSizeLoc = -1;
    int blurDirectionLoc = -1;
    int compositeStrengthLoc = -1;
    int compositeBloomTextureLoc = -1;
    engine::RenderTarget sceneCopy;
    engine::RenderTarget source;
    engine::RenderTarget blurA;
    engine::RenderTarget blurB;
    int sceneWidth = 0;
    int sceneHeight = 0;
    int targetWidth = 0;
    int targetHeight = 0;
};

} // namespace game
