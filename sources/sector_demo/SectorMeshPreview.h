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

    SectorMeshBuildResult meshes;
    SectorGeneratedGeometry generatedGeometry;
    std::unordered_map<std::string, engine::TextureHandle> textureHandlesById;
    engine::TextureHandle lightmapTexture = engine::NullTextureHandle();
    engine::AssetScopeHandle assetScope = engine::NullAssetScopeHandle();
    Material material = {};
    Texture2D defaultMaterialTexture = {};
    bool materialLoaded = false;
    int useLightmapLoc = -1;
    int useBakedAmbientOcclusionLoc = -1;
    int hasDecalLoc = -1;
    int decalOpacityLoc = -1;
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
