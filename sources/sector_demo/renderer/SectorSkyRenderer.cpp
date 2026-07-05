#include "sector_demo/renderer/SectorSkyRenderer.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorSkyCylinder.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <cstdio>
#include <limits>

namespace game {

namespace {

Mesh CreateSkyCylinderMesh(const SectorSkyCylinderMeshData& data)
{
    Mesh mesh = {};
    if (data.positions.empty()
            || data.positions.size() != data.normals.size()
            || data.positions.size() != data.uvs.size()
            || data.positions.size() > static_cast<size_t>(std::numeric_limits<int>::max())
            || data.indices.empty()
            || data.indices.size() % 3u != 0u
            || data.indices.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return mesh;
    }

    mesh.vertexCount = static_cast<int>(data.positions.size());
    mesh.triangleCount = static_cast<int>(data.indices.size() / 3u);
    mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(static_cast<unsigned int>(data.indices.size() * sizeof(unsigned short))));

    if (mesh.vertices == nullptr
            || mesh.normals == nullptr
            || mesh.texcoords == nullptr
            || mesh.indices == nullptr) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to allocate sky cylinder mesh data\n");
        UnloadMesh(mesh);
        return Mesh{};
    }

    for (int i = 0; i < mesh.vertexCount; ++i) {
        const size_t index = static_cast<size_t>(i);
        mesh.vertices[i * 3 + 0] = data.positions[index].x;
        mesh.vertices[i * 3 + 1] = data.positions[index].y;
        mesh.vertices[i * 3 + 2] = data.positions[index].z;
        mesh.normals[i * 3 + 0] = data.normals[index].x;
        mesh.normals[i * 3 + 1] = data.normals[index].y;
        mesh.normals[i * 3 + 2] = data.normals[index].z;
        mesh.texcoords[i * 2 + 0] = data.uvs[index].x;
        mesh.texcoords[i * 2 + 1] = data.uvs[index].y;
    }

    for (size_t i = 0; i < data.indices.size(); ++i) {
        mesh.indices[i] = data.indices[i];
    }

    UploadMesh(&mesh, false);
    return mesh;
}

} // namespace

void SectorSkyRenderer::Rebuild(const SectorTopologyMap& map, engine::TextureHandle textureHandle)
{
    Shutdown();

    if (!ShouldRenderSkyCylinder(map)) {
        return;
    }

    const SectorTopologySkySettings skySettings = NormalizeSectorTopologySkySettings(map.skySettings);
    skyTextureHandle = textureHandle;
    skyYawOffsetDegrees = skySettings.yawOffsetDegrees;
    skyTopCapColor = skySettings.topColor;
    const SectorSkyCylinderMeshData skyData = BuildSkyCylinderMeshData(
            kDefaultSkyCylinderSegments,
            kDefaultSkyCylinderRadius,
            kDefaultSkyCylinderHeight,
            skySettings.verticalOffset,
            skySettings.verticalScale);
    const SectorSkyCylinderMeshData skyTopCapData = BuildSkyCylinderTopCapMeshData();
    skyCylinderMesh = CreateSkyCylinderMesh(skyData);
    skyTopCapMesh = CreateSkyCylinderMesh(skyTopCapData);
    if (skyCylinderMesh.vertexCount > 0 && skyTopCapMesh.vertexCount > 0) {
        skyMaterial = LoadMaterialDefault();
        skyDefaultMaterialTexture = skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
        skyMaterialLoaded = true;
    } else {
        Shutdown();
        skyTextureHandle = engine::NullTextureHandle();
    }
}

void SectorSkyRenderer::Shutdown()
{
    if (skyCylinderMesh.vertexCount > 0) {
        UnloadMesh(skyCylinderMesh);
        skyCylinderMesh = Mesh{};
    }
    if (skyTopCapMesh.vertexCount > 0) {
        UnloadMesh(skyTopCapMesh);
        skyTopCapMesh = Mesh{};
    }

    if (skyMaterialLoaded) {
        skyMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
        skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = skyDefaultMaterialTexture;
        UnloadMaterial(skyMaterial);
        skyMaterial = Material{};
        skyDefaultMaterialTexture = Texture2D{};
        skyMaterialLoaded = false;
    }

    skyTextureHandle = engine::NullTextureHandle();
    skyYawOffsetDegrees = 0.0f;
    skyTopCapColor = DefaultSectorTopologySkySettings().topColor;
}

void SectorSkyRenderer::Draw(engine::AssetManager& assets, const Camera3D& camera)
{
    const Texture2D* skyTexture = assets.GetTexture(skyTextureHandle);
    if (skyTexture != nullptr
            && skyCylinderMesh.vertexCount > 0
            && skyTopCapMesh.vertexCount > 0
            && skyMaterialLoaded) {
        DrawSkyCylinder(*skyTexture, camera);
    }
}

bool SectorSkyRenderer::IsLoaded() const
{
    return skyCylinderMesh.vertexCount > 0
            || skyTopCapMesh.vertexCount > 0
            || skyMaterialLoaded;
}

void SectorSkyRenderer::DrawSkyCylinder(const Texture2D& texture, const Camera3D& camera)
{
    const Matrix transform = MatrixMultiply(
            MatrixRotateY(skyYawOffsetDegrees * DEG2RAD),
            MatrixTranslate(camera.position.x, camera.position.y, camera.position.z));
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    rlDisableDepthMask();
    DrawMesh(skyCylinderMesh, skyMaterial, transform);
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = skyDefaultMaterialTexture;
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].color = skyTopCapColor;
    DrawMesh(skyTopCapMesh, skyMaterial, transform);
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
    skyMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = texture;
    rlEnableDepthMask();
}

} // namespace game
