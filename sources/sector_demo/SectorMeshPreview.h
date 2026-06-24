#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorMeshTypes.h"

#include <raylib.h>

#include <string>
#include <unordered_map>

namespace game {

struct SectorTopologyMap;

struct SectorMeshPreviewPose {
    Vector3 position = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

class SectorMeshPreview {
public:
    bool Rebuild(
            engine::AssetManager& assets,
            const SectorTopologyMap& map,
            const char* scopeName,
            std::string& error);
    void Shutdown(engine::AssetManager& assets);

    void Enter();
    void Leave();

    void Update(engine::Input& input, float dt);
    void Render(engine::AssetManager& assets, bool useBakedAmbientOcclusion = true);
    void ApplyEmissiveDecalBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget);

    bool IsReady() const { return initialized; }
    bool IsMouseLookEnabled() const { return mouseLookEnabled; }
    Vector3 Position() const { return position; }
    const Camera3D& Camera() const { return camera; }
    SectorMeshPreviewPose Pose() const;
    void ApplyPose(const SectorMeshPreviewPose& pose);
    void SetMouseLookEnabled(bool enabled);
    size_t SectorCount() const { return sectorCount; }
    size_t BatchCount() const { return meshes.batches.size(); }
    int TriangleCount() const { return meshes.triangleCount; }
    const SectorGeneratedGeometry& GeneratedGeometry() const { return generatedGeometry; }
    float AssetProgress(engine::AssetManager& assets) const;
    const char* LightmapStatusText() const;

private:
    static std::string ResolveAssetPath(const std::string& path);
    engine::TextureHandle TextureForId(const std::string& textureId) const;
    void UpdateCamera();
    bool EnsureBloomResources(int sceneWidth, int sceneHeight);
    void UnloadBloomResources();
    void RenderBloomSource(engine::AssetManager& assets);
    void UnloadSkyCylinderMesh();
    void DrawSkyCylinder(const Texture2D& texture);

    SectorMeshBuildResult meshes;
    SectorGeneratedGeometry generatedGeometry;
    std::unordered_map<std::string, engine::TextureHandle> textureHandlesById;
    engine::TextureHandle lightmapTexture = engine::NullTextureHandle();
    engine::TextureHandle skyTextureHandle = engine::NullTextureHandle();
    engine::AssetScopeHandle assetScope = engine::NullAssetScopeHandle();
    Material material = {};
    Texture2D defaultMaterialTexture = {};
    bool materialLoaded = false;
    Mesh skyCylinderMesh = {};
    Mesh skyTopCapMesh = {};
    Material skyMaterial = {};
    Texture2D skyDefaultMaterialTexture = {};
    bool skyMaterialLoaded = false;
    int useLightmapLoc = -1;
    int useBakedAmbientOcclusionLoc = -1;
    int hasLightmapLoc = -1;
    int alphaTestLoc = -1;
    int alphaCutoffLoc = -1;
    int hasDecalLoc = -1;
    int decalOpacityLoc = -1;
    int decalEmissiveLoc = -1;
    int decalTintLoc = -1;
    Material bloomSourceMaterial = {};
    Texture2D bloomDefaultMaterialTexture = {};
    bool bloomSourceMaterialLoaded = false;
    int bloomHasDecalLoc = -1;
    int bloomDecalOpacityLoc = -1;
    int bloomDecalEmissiveLoc = -1;
    int bloomDecalTintLoc = -1;
    int bloomDecalIntensityLoc = -1;
    Shader blurShader = {};
    Shader compositeShader = {};
    int blurTexelSizeLoc = -1;
    int blurDirectionLoc = -1;
    int compositeStrengthLoc = -1;
    int compositeBloomTextureLoc = -1;
    RenderTexture2D bloomSceneCopy = {};
    RenderTexture2D bloomSource = {};
    RenderTexture2D bloomBlurA = {};
    RenderTexture2D bloomBlurB = {};
    int bloomSceneWidth = 0;
    int bloomSceneHeight = 0;
    int bloomTargetWidth = 0;
    int bloomTargetHeight = 0;
    int lightmapStatus = 0;
    bool initialized = false;
    bool mouseLookEnabled = true;
    int mouseLookWarmupFrames = 0;
    size_t sectorCount = 0;

    Camera3D camera = {};
    Vector3 position = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

} // namespace game
