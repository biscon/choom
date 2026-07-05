#pragma once

#include "engine/assets/AssetHandles.h"

#include <raylib.h>

namespace engine {
class AssetManager;
}

namespace game {

struct SectorTopologyMap;

class SectorSkyRenderer {
public:
    void Rebuild(const SectorTopologyMap& map, engine::TextureHandle textureHandle);
    void Shutdown();
    void Draw(engine::AssetManager& assets, const Camera3D& camera);

    bool IsLoaded() const;

private:
    void DrawSkyCylinder(const Texture2D& texture, const Camera3D& camera);

    engine::TextureHandle skyTextureHandle = engine::NullTextureHandle();
    Mesh skyCylinderMesh = {};
    Mesh skyTopCapMesh = {};
    Material skyMaterial = {};
    Texture2D skyDefaultMaterialTexture = {};
    float skyYawOffsetDegrees = 0.0f;
    Color skyTopCapColor = Color{95, 165, 235, 255};
    bool skyMaterialLoaded = false;
};

} // namespace game
