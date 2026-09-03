#include "sector_demo/renderer/SectorDuctCoverRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorRuntimeObjects.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace game {
namespace {

struct CoverMeshVertex {
    Vector3 position = {};
    Vector3 normal = {};
    Vector2 uv = {};
};

struct CoverMeshData {
    std::vector<CoverMeshVertex> vertices;
    std::vector<unsigned short> indices;
};

Vector3 RotateAroundLocalX(Vector3 value, float radians)
{
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return Vector3{
            value.x,
            value.y * cosine - value.z * sine,
            value.y * sine + value.z * cosine};
}

void AppendQuad(
        CoverMeshData& data,
        Vector3 a,
        Vector3 b,
        Vector3 c,
        Vector3 d,
        Vector3 normal,
        float uvWidth,
        float uvHeight)
{
    if (data.vertices.size() + 4u
            > static_cast<size_t>(std::numeric_limits<unsigned short>::max())) {
        return;
    }
    const unsigned short base = static_cast<unsigned short>(data.vertices.size());
    data.vertices.push_back(CoverMeshVertex{a, normal, Vector2{0.0f, uvHeight}});
    data.vertices.push_back(CoverMeshVertex{b, normal, Vector2{uvWidth, uvHeight}});
    data.vertices.push_back(CoverMeshVertex{c, normal, Vector2{uvWidth, 0.0f}});
    data.vertices.push_back(CoverMeshVertex{d, normal, Vector2{0.0f, 0.0f}});
    data.indices.insert(data.indices.end(), {
            static_cast<unsigned short>(base + 0),
            static_cast<unsigned short>(base + 1),
            static_cast<unsigned short>(base + 2),
            static_cast<unsigned short>(base + 0),
            static_cast<unsigned short>(base + 2),
            static_cast<unsigned short>(base + 3)});
}

void AppendBox(
        CoverMeshData& data,
        Vector3 center,
        float width,
        float height,
        float depth,
        float pitchRadians)
{
    if (width <= 0.0f || height <= 0.0f || depth <= 0.0f) return;
    const float halfWidth = width * 0.5f;
    const float halfHeight = height * 0.5f;
    const float halfDepth = depth * 0.5f;
    const auto point = [center, pitchRadians](float x, float y, float z) {
        return Vector3Add(center, RotateAroundLocalX(Vector3{x, y, z}, pitchRadians));
    };
    const auto normal = [pitchRadians](float x, float y, float z) {
        return RotateAroundLocalX(Vector3{x, y, z}, pitchRadians);
    };

    const Vector3 bottomFrontLeft = point(-halfWidth, -halfHeight, halfDepth);
    const Vector3 bottomFrontRight = point(halfWidth, -halfHeight, halfDepth);
    const Vector3 bottomBackRight = point(halfWidth, -halfHeight, -halfDepth);
    const Vector3 bottomBackLeft = point(-halfWidth, -halfHeight, -halfDepth);
    const Vector3 topFrontLeft = point(-halfWidth, halfHeight, halfDepth);
    const Vector3 topFrontRight = point(halfWidth, halfHeight, halfDepth);
    const Vector3 topBackRight = point(halfWidth, halfHeight, -halfDepth);
    const Vector3 topBackLeft = point(-halfWidth, halfHeight, -halfDepth);

    // World-sized spans keep the procedural texture density stable as the
    // portal changes size, matching procedural doors and ladders.
    AppendQuad(data, bottomFrontLeft, bottomFrontRight, topFrontRight,
            topFrontLeft, normal(0.0f, 0.0f, 1.0f), width, height);
    AppendQuad(data, bottomBackRight, bottomBackLeft, topBackLeft,
            topBackRight, normal(0.0f, 0.0f, -1.0f), width, height);
    AppendQuad(data, bottomFrontRight, bottomBackRight, topBackRight,
            topFrontRight, normal(1.0f, 0.0f, 0.0f), depth, height);
    AppendQuad(data, bottomBackLeft, bottomFrontLeft, topFrontLeft,
            topBackLeft, normal(-1.0f, 0.0f, 0.0f), depth, height);
    AppendQuad(data, topFrontLeft, topFrontRight, topBackRight,
            topBackLeft, normal(0.0f, 1.0f, 0.0f), width, depth);
    AppendQuad(data, bottomBackLeft, bottomBackRight, bottomFrontRight,
            bottomFrontLeft, normal(0.0f, -1.0f, 0.0f), width, depth);
}

Mesh CreateCoverMesh(const CoverMeshData& data)
{
    Mesh mesh = {};
    if (data.vertices.empty() || data.indices.empty()
            || data.vertices.size()
                    > static_cast<size_t>(std::numeric_limits<int>::max())
            || data.indices.size()
                    > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return mesh;
    }
    mesh.vertexCount = static_cast<int>(data.vertices.size());
    mesh.triangleCount = static_cast<int>(data.indices.size() / 3u);
    mesh.vertices = static_cast<float*>(MemAlloc(
            static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(
            static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.texcoords = static_cast<float*>(MemAlloc(
            static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
    mesh.tangents = static_cast<float*>(MemAlloc(
            static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(float))));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(
            static_cast<unsigned int>(data.indices.size()
                    * sizeof(unsigned short))));
    if (mesh.vertices == nullptr || mesh.normals == nullptr
            || mesh.texcoords == nullptr || mesh.tangents == nullptr
            || mesh.indices == nullptr) {
        std::fprintf(stderr,
                "[SectorDemo ERROR] Failed to allocate duct cover mesh data\n");
        UnloadMesh(mesh);
        return Mesh{};
    }
    for (int index = 0; index < mesh.vertexCount; ++index) {
        const CoverMeshVertex& vertex = data.vertices[static_cast<size_t>(index)];
        mesh.vertices[index * 3 + 0] = vertex.position.x;
        mesh.vertices[index * 3 + 1] = vertex.position.y;
        mesh.vertices[index * 3 + 2] = vertex.position.z;
        mesh.normals[index * 3 + 0] = vertex.normal.x;
        mesh.normals[index * 3 + 1] = vertex.normal.y;
        mesh.normals[index * 3 + 2] = vertex.normal.z;
        mesh.texcoords[index * 2 + 0] = vertex.uv.x;
        mesh.texcoords[index * 2 + 1] = vertex.uv.y;
        mesh.tangents[index * 4 + 0] = 1.0f;
        mesh.tangents[index * 4 + 1] = 1.0f;
        mesh.tangents[index * 4 + 2] = 1.0f;
        mesh.tangents[index * 4 + 3] = 1.0f;
    }
    std::copy(data.indices.begin(), data.indices.end(), mesh.indices);
    UploadMesh(&mesh, false);
    return mesh;
}

void BuildCoverMeshes(
        const SectorDuctAccess& access,
        Mesh& outFrame,
        Mesh& outLouvers)
{
    const float border = std::min(
            access.cover.frameBorderWidthWorld,
            std::max(0.001f, std::min(access.width, access.height) * 0.49f));
    const float innerWidth = std::max(0.001f, access.width - border * 2.0f);
    const float innerHeight = std::max(0.001f, access.height - border * 2.0f);
    const float thickness = access.cover.thickness;

    CoverMeshData frame;
    frame.vertices.reserve(4u * 24u);
    frame.indices.reserve(4u * 36u);
    const float horizontalCenter = (access.width - border) * 0.5f;
    AppendBox(frame, Vector3{-horizontalCenter, 0.0f, 0.0f},
            border, access.height, thickness, 0.0f);
    AppendBox(frame, Vector3{horizontalCenter, 0.0f, 0.0f},
            border, access.height, thickness, 0.0f);
    const float verticalCenter = (access.height - border) * 0.5f;
    AppendBox(frame, Vector3{0.0f, -verticalCenter, 0.0f},
            innerWidth, border, thickness, 0.0f);
    AppendBox(frame, Vector3{0.0f, verticalCenter, 0.0f},
            innerWidth, border, thickness, 0.0f);

    CoverMeshData louvers;
    const int count = std::clamp(access.cover.louverCount, 1, 64);
    louvers.vertices.reserve(static_cast<size_t>(count) * 24u);
    louvers.indices.reserve(static_cast<size_t>(count) * 36u);
    const float spacing = innerHeight / static_cast<float>(count);
    const float slatHeight = std::max(
            0.005f, std::min(border * 0.35f, spacing * 0.45f));
    const float pitch = access.cover.louverAngleDegrees * DEG2RAD;
    for (int index = 0; index < count; ++index) {
        const float localY = -innerHeight * 0.5f
                + spacing * (static_cast<float>(index) + 0.5f);
        AppendBox(louvers, Vector3{0.0f, localY, 0.0f},
                innerWidth, slatHeight, thickness, pitch);
    }
    outFrame = CreateCoverMesh(frame);
    outLouvers = CreateCoverMesh(louvers);
}

SectorDoorResolvedMaterial ResolveMaterial(
        const SectorDoorDrawContext& context,
        const std::string& materialId,
        const Texture2D& rendererDefault)
{
    SectorDoorResolvedMaterial resolved;
    if (!materialId.empty() && context.materialResolver.resolve != nullptr) {
        resolved = context.materialResolver.resolve(
                context.materialResolver.userData,
                *context.assets,
                materialId);
    }
    if (resolved.albedo == nullptr) {
        resolved.albedo = context.defaultMaterialTexture != nullptr
                ? context.defaultMaterialTexture : &rendererDefault;
    }
    resolved.normalStrength = std::isfinite(resolved.normalStrength)
            ? std::clamp(resolved.normalStrength, 0.0f, 1.0f) : 1.0f;
    resolved.metallicFactor = std::isfinite(resolved.metallicFactor)
            ? std::clamp(resolved.metallicFactor, 0.0f, 1.0f) : 0.0f;
    resolved.roughnessFactor = std::isfinite(resolved.roughnessFactor)
            ? std::clamp(resolved.roughnessFactor, 0.045f, 1.0f) : 0.8f;
    return resolved;
}

void ApplyMaterial(
        Material& material,
        const SectorDoorOpaqueShaderLocations& locations,
        const SectorDoorResolvedMaterial& resolved)
{
    const bool hasNormal = resolved.normal != nullptr
            && resolved.normal->id != 0;
    const int hasNormalValue = hasNormal ? 1 : 0;
    if (locations.hasNormalMap >= 0) SetShaderValue(material.shader,
            locations.hasNormalMap, &hasNormalValue, SHADER_UNIFORM_INT);
    if (locations.normalStrength >= 0) SetShaderValue(material.shader,
            locations.normalStrength, &resolved.normalStrength,
            SHADER_UNIFORM_FLOAT);
    if (locations.metallicFactor >= 0) SetShaderValue(material.shader,
            locations.metallicFactor, &resolved.metallicFactor,
            SHADER_UNIFORM_FLOAT);
    if (locations.roughnessFactor >= 0) SetShaderValue(material.shader,
            locations.roughnessFactor, &resolved.roughnessFactor,
            SHADER_UNIFORM_FLOAT);
    material.maps[MATERIAL_MAP_DIFFUSE].texture = *resolved.albedo;
    material.maps[MATERIAL_MAP_NORMAL].texture = hasNormal
            ? *resolved.normal : Texture2D{};
    material.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
}

} // namespace

bool SectorDuctCoverRenderer::Initialize()
{
    if (loaded) return true;
    meshCache.reserve(kSectorRuntimeObjectInitialCapacity);
    loaded = true;
    return true;
}

void SectorDuctCoverRenderer::Shutdown()
{
    for (auto& entry : meshCache) {
        if (entry.second.frame.vertexCount > 0) UnloadMesh(entry.second.frame);
        if (entry.second.louvers.vertexCount > 0) UnloadMesh(entry.second.louvers);
    }
    meshCache.clear();
    loaded = false;
}

void SectorDuctCoverRenderer::Draw(
        const SectorDoorDrawContext& context,
        SectorDoorRenderer& opaqueRenderer)
{
    if (!loaded || !opaqueRenderer.IsOpaqueReady()
            || context.runtimeObjectWorld == nullptr
            || context.assets == nullptr) {
        return;
    }

    for (auto& entry : meshCache) entry.second.seenThisFrame = false;
    Material& material = opaqueRenderer.OpaqueMaterial();
    const Texture2D& rendererDefault =
            opaqueRenderer.OpaqueDefaultMaterialTexture();
    const SectorDoorOpaqueShaderLocations& locations =
            opaqueRenderer.OpaqueShaderLocations();
    const RuntimePortalVisibilityResult emptyVisibility;
    const RuntimePortalVisibilityResult& visibility = context.visibility != nullptr
            ? *context.visibility : emptyVisibility;
    const SectorStaticSpecularLightState emptyStaticSpecularLights;
    const SectorStaticSpecularLightState& staticSpecularLights =
            context.staticSpecularLights != nullptr
            ? *context.staticSpecularLights : emptyStaticSpecularLights;

    const bool environmentActive = context.environment != nullptr
            && context.environment->id != 0
            && NormalizeSectorPbrContributionSettings(context.pbr)
                    .worldEnvironmentSpecularScale > 0.0f;
    material.maps[MATERIAL_MAP_ROUGHNESS].texture =
            context.dynamicLighting.shadowMaps.shadowMap0 != nullptr
            ? *context.dynamicLighting.shadowMaps.shadowMap0 : Texture2D{};
    material.maps[MATERIAL_MAP_OCCLUSION].texture =
            context.dynamicLighting.shadowMaps.shadowMap1 != nullptr
            ? *context.dynamicLighting.shadowMaps.shadowMap1 : Texture2D{};
    material.maps[MATERIAL_MAP_CUBEMAP].texture = environmentActive
            ? *context.environment : Texture2D{};

    rlDisableColorBlend();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
    const int useObjectAmbientCube = 1;
    if (locations.useObjectAmbientCube >= 0) SetShaderValue(material.shader,
            locations.useObjectAmbientCube, &useObjectAmbientCube,
            SHADER_UNIFORM_INT);
    const Vector4 tint = engine::SrgbColorBytesToLinearSceneRgba(WHITE);
    if (locations.tint >= 0) SetShaderValue(material.shader,
            locations.tint, &tint, SHADER_UNIFORM_VEC4);

    context.runtimeObjectWorld->ForEach<
            SectorDuctAccess,
            SectorObjectTransform,
            SectorObject,
            SectorObjectLighting>(
            [&](engine::Entity,
                    SectorDuctAccess& access,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorObjectLighting& lighting) {
                if (!access.cover.enabled || !object.visible
                        || access.width <= 0.0f || access.height <= 0.0f
                        || access.cover.thickness <= 0.0f) {
                    return;
                }
                MeshCacheEntry& cache = meshCache[access.placedObjectId];
                cache.seenThisFrame = true;
                const float border = std::min(
                        access.cover.frameBorderWidthWorld,
                        std::max(0.001f,
                                std::min(access.width, access.height) * 0.49f));
                const bool meshDirty = cache.frame.vertexCount <= 0
                        || cache.louvers.vertexCount <= 0
                        || cache.width != access.width
                        || cache.height != access.height
                        || cache.thickness != access.cover.thickness
                        || cache.border != border
                        || cache.angleDegrees
                                != access.cover.louverAngleDegrees
                        || cache.louverCount != access.cover.louverCount;
                if (meshDirty) {
                    if (cache.frame.vertexCount > 0) UnloadMesh(cache.frame);
                    if (cache.louvers.vertexCount > 0) UnloadMesh(cache.louvers);
                    cache.frame = {};
                    cache.louvers = {};
                    BuildCoverMeshes(access, cache.frame, cache.louvers);
                    cache.width = access.width;
                    cache.height = access.height;
                    cache.thickness = access.cover.thickness;
                    cache.border = border;
                    cache.angleDegrees = access.cover.louverAngleDegrees;
                    cache.louverCount = access.cover.louverCount;
                }
                if (cache.frame.vertexCount <= 0
                        || cache.louvers.vertexCount <= 0) {
                    return;
                }

                const Vector3 position = Vector3Add(
                        transform.position, access.coverOffset);
                BakedObjectLightingSample baked =
                        ResolveBakedObjectLightingVerticalSample(
                                lighting.vertical, position.y);
                if (!baked.valid) baked = lighting.baked;
                if (!baked.valid && context.lighting.mapForFallback != nullptr) {
                    const Vector3 fallback = ComputeSectorModelAmbient(
                            *context.lighting.mapForFallback,
                            object.currentSectorId);
                    for (Vector3& face : baked.ambientCube) face = fallback;
                    baked.valid = true;
                }
                if (!baked.valid) {
                    for (Vector3& face : baked.ambientCube) {
                        face = Vector3{0.15f, 0.15f, 0.15f};
                    }
                }
                if (locations.objectAmbientCube >= 0) {
                    SetShaderValueV(material.shader,
                            locations.objectAmbientCube,
                            baked.ambientCube,
                            SHADER_UNIFORM_VEC3,
                            6);
                }

                const float boundsRadius = std::max(
                        access.width, access.cover.thickness) * 0.5f;
                SectorReceiverBounds receiverBounds{
                        object.currentSectorId,
                        Vector3{position.x - boundsRadius,
                                position.y - access.height * 0.5f,
                                position.z - boundsRadius},
                        Vector3{position.x + boundsRadius,
                                position.y + access.height * 0.5f,
                                position.z + boundsRadius}};
                const SectorStaticSpecularLightContext specularContext =
                        SelectSectorStaticSpecularLights(
                                staticSpecularLights,
                                receiverBounds,
                                object.currentSectorId,
                                visibility,
                                context.staticSpecularEligible);
                UploadSectorStaticSpecularLights(
                        material.shader, locations.staticSpecular,
                        specularContext);
                const int useStaticSpecular =
                        specularContext.lightCount > 0 ? 1 : 0;
                if (locations.useStaticSpecularLighting >= 0) {
                    SetShaderValue(material.shader,
                            locations.useStaticSpecularLighting,
                            &useStaticSpecular,
                            SHADER_UNIFORM_INT);
                }

                const Matrix model = BuildSectorDuctCoverModelMatrix(
                        access, position);
                const SectorDoorResolvedMaterial frameMaterial =
                        ResolveMaterial(context,
                                access.cover.frameMaterialId,
                                rendererDefault);
                if (frameMaterial.albedo != nullptr
                        && frameMaterial.albedo->id != 0) {
                    ApplyMaterial(material, locations, frameMaterial);
                    DrawMesh(cache.frame, material, model);
                }
                const SectorDoorResolvedMaterial louverMaterial =
                        ResolveMaterial(context,
                                access.cover.louverMaterialId,
                                rendererDefault);
                if (louverMaterial.albedo != nullptr
                        && louverMaterial.albedo->id != 0) {
                    ApplyMaterial(material, locations, louverMaterial);
                    DrawMesh(cache.louvers, material, model);
                }
            });

    for (auto iterator = meshCache.begin(); iterator != meshCache.end();) {
        if (!iterator->second.seenThisFrame) {
            if (iterator->second.frame.vertexCount > 0) {
                UnloadMesh(iterator->second.frame);
            }
            if (iterator->second.louvers.vertexCount > 0) {
                UnloadMesh(iterator->second.louvers);
            }
            iterator = meshCache.erase(iterator);
        } else {
            ++iterator;
        }
    }

    const int disableObjectAmbientCube = 0;
    if (locations.useObjectAmbientCube >= 0) SetShaderValue(material.shader,
            locations.useObjectAmbientCube, &disableObjectAmbientCube,
            SHADER_UNIFORM_INT);
    material.maps[MATERIAL_MAP_DIFFUSE].texture = rendererDefault;
    material.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
    material.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
    material.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
    material.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableBackfaceCulling();
}

} // namespace game
