#include "sector_demo/SectorMeshBuilder.h"

#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"

#include <raylib.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

namespace {

struct SectorMeshBatchKey {
    std::string textureId;
    std::string decalTextureId;
    float decalOpacity = 1.0f;

    bool operator==(const SectorMeshBatchKey& other) const
    {
        return textureId == other.textureId
                && decalTextureId == other.decalTextureId
                && decalOpacity == other.decalOpacity;
    }
};

struct SectorMeshBatchKeyHash {
    size_t operator()(const SectorMeshBatchKey& key) const
    {
        const size_t textureHash = std::hash<std::string>{}(key.textureId);
        const size_t decalHash = std::hash<std::string>{}(key.decalTextureId);
        const size_t opacityHash = std::hash<float>{}(key.decalOpacity);
        size_t hash = textureHash ^ (decalHash + 0x9e3779b9u + (textureHash << 6u) + (textureHash >> 2u));
        hash ^= opacityHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        return hash;
    }
};

SectorMeshBatchData& BatchForKey(
        std::unordered_map<SectorMeshBatchKey, size_t, SectorMeshBatchKeyHash>& batchIndexByKey,
        std::vector<SectorMeshBatchData>& batches,
        const std::string& textureId,
        const std::string& decalTextureId,
        float decalOpacity)
{
    const SectorMeshBatchKey key{textureId, decalTextureId, decalTextureId.empty() ? 1.0f : decalOpacity};
    const auto existing = batchIndexByKey.find(key);
    if (existing != batchIndexByKey.end()) {
        return batches[existing->second];
    }

    SectorMeshBatchData batch;
    batch.textureId = textureId;
    batch.decalTextureId = decalTextureId;
    batch.decalOpacity = key.decalOpacity;
    batches.push_back(std::move(batch));
    const size_t index = batches.size() - 1;
    batchIndexByKey.emplace(key, index);
    return batches[index];
}

Vector2 ResolveLightmapUv(
        const SectorLightmapLayout* layout,
        size_t surfaceIndex,
        size_t vertexIndex)
{
    if (layout == nullptr
            || surfaceIndex >= layout->charts.size()
            || vertexIndex >= layout->charts[surfaceIndex].vertexUvs.size()) {
        return Vector2{0.0f, 0.0f};
    }

    return layout->charts[surfaceIndex].vertexUvs[vertexIndex];
}

Mesh CreateMeshFromBatch(const SectorMeshBatchData& batch)
{
    Mesh mesh = {};
    if (batch.vertices.empty() || batch.vertices.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return mesh;
    }

    mesh.vertexCount = static_cast<int>(batch.vertices.size());
    mesh.triangleCount = mesh.vertexCount / 3;
    mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
    mesh.texcoords2 = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
    mesh.tangents = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(float))));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(unsigned char))));

    if (mesh.vertices == nullptr
            || mesh.normals == nullptr
            || mesh.texcoords == nullptr
            || mesh.texcoords2 == nullptr
            || mesh.tangents == nullptr
            || mesh.colors == nullptr) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to allocate mesh data for texture '%s'\n", batch.textureId.c_str());
        UnloadMesh(mesh);
        return Mesh{};
    }

    for (int i = 0; i < mesh.vertexCount; ++i) {
        const SectorMeshBatchVertex& vertex = batch.vertices[static_cast<size_t>(i)];
        mesh.vertices[i * 3 + 0] = vertex.position.x;
        mesh.vertices[i * 3 + 1] = vertex.position.y;
        mesh.vertices[i * 3 + 2] = vertex.position.z;
        mesh.normals[i * 3 + 0] = vertex.normal.x;
        mesh.normals[i * 3 + 1] = vertex.normal.y;
        mesh.normals[i * 3 + 2] = vertex.normal.z;
        mesh.texcoords[i * 2 + 0] = vertex.uv.x;
        mesh.texcoords[i * 2 + 1] = vertex.uv.y;
        mesh.texcoords2[i * 2 + 0] = vertex.lightmapUv.x;
        mesh.texcoords2[i * 2 + 1] = vertex.lightmapUv.y;
        mesh.tangents[i * 4 + 0] = vertex.decalUv.x;
        mesh.tangents[i * 4 + 1] = vertex.decalUv.y;
        mesh.tangents[i * 4 + 2] = 0.0f;
        mesh.tangents[i * 4 + 3] = 1.0f;
        mesh.colors[i * 4 + 0] = vertex.color.r;
        mesh.colors[i * 4 + 1] = vertex.color.g;
        mesh.colors[i * 4 + 2] = vertex.color.b;
        mesh.colors[i * 4 + 3] = vertex.color.a;
    }

    UploadMesh(&mesh, false);
    return mesh;
}

} // namespace

SectorMeshBatchDataResult BuildSectorMeshBatchData(
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout* lightmapLayout)
{
    SectorMeshBatchDataResult result;

    std::unordered_map<SectorMeshBatchKey, size_t, SectorMeshBatchKeyHash> batchIndexByKey;

    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        SectorMeshBatchData& batch = BatchForKey(
                batchIndexByKey,
                result.batches,
                surface.textureId,
                surface.decalTextureId,
                surface.decalOpacity);
        batch.vertices.reserve(batch.vertices.size() + surface.vertices.size());
        for (size_t vertexIndex = 0; vertexIndex < surface.vertices.size(); ++vertexIndex) {
            const SectorGeneratedVertex& vertex = surface.vertices[vertexIndex];
            batch.vertices.push_back(SectorMeshBatchVertex{
                    vertex.position,
                    vertex.normal,
                    vertex.uv,
                    vertex.decalUv,
                    ResolveLightmapUv(lightmapLayout, surfaceIndex, vertexIndex),
                    surface.decalOpacity,
                    vertex.color
            });
        }
    }

    for (SectorMeshBatchData& batch : result.batches) {
        batch.vertexCount = static_cast<int>(batch.vertices.size());
        batch.triangleCount = batch.vertexCount / 3;
        result.vertexCount += batch.vertexCount;
        result.triangleCount += batch.triangleCount;
    }

    return result;
}

SectorMeshBuildResult BuildSectorMeshes(
        const SectorTopologyMap& map,
        const SectorLightmapLayout* lightmapLayout,
        std::string* outError)
{
    if (outError != nullptr) {
        outError->clear();
    }

    SectorMeshBuildResult result;
    SectorGeneratedGeometry geometry;
    if (!BuildSectorGeneratedGeometry(map, geometry, outError)) {
        return result;
    }

    const SectorMeshBatchDataResult batchData = BuildSectorMeshBatchData(geometry, lightmapLayout);

    for (const SectorMeshBatchData& builder : batchData.batches) {
        Mesh mesh = CreateMeshFromBatch(builder);
        if (mesh.vertexCount <= 0) {
            continue;
        }

        SectorMeshBatch batch;
        batch.textureId = builder.textureId;
        batch.decalTextureId = builder.decalTextureId;
        batch.decalOpacity = builder.decalOpacity;
        batch.mesh = mesh;
        batch.vertexCount = mesh.vertexCount;
        batch.triangleCount = mesh.triangleCount;
        result.vertexCount += batch.vertexCount;
        result.triangleCount += batch.triangleCount;
        result.batches.push_back(batch);
    }

    std::fprintf(
            stdout,
            "[SectorDemo] Generated %d vertices, %d triangles, %zu topology mesh batches\n",
            result.vertexCount,
            result.triangleCount,
            result.batches.size()
    );

    return result;
}

void UnloadSectorMeshes(SectorMeshBuildResult& buildResult)
{
    for (SectorMeshBatch& batch : buildResult.batches) {
        if (batch.mesh.vertexCount > 0) {
            UnloadMesh(batch.mesh);
            batch.mesh = Mesh{};
        }
    }

    buildResult = SectorMeshBuildResult{};
}

} // namespace game
