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

struct SurfaceVertex {
    Vector3 position = {};
    Vector3 normal = {};
    Vector2 uv = {};
    Vector2 lightmapUv = {};
    Color color = WHITE;
};

struct BatchBuilder {
    std::string textureId;
    std::vector<SurfaceVertex> vertices;
};

BatchBuilder& BatchForTexture(
        std::unordered_map<std::string, size_t>& batchIndexByTexture,
        std::vector<BatchBuilder>& batches,
        const std::string& textureId)
{
    const auto existing = batchIndexByTexture.find(textureId);
    if (existing != batchIndexByTexture.end()) {
        return batches[existing->second];
    }

    BatchBuilder batch;
    batch.textureId = textureId;
    batches.push_back(std::move(batch));
    const size_t index = batches.size() - 1;
    batchIndexByTexture.emplace(textureId, index);
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

Mesh CreateMeshFromBatch(const BatchBuilder& batch)
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
    mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(unsigned char))));

    if (mesh.vertices == nullptr
            || mesh.normals == nullptr
            || mesh.texcoords == nullptr
            || mesh.texcoords2 == nullptr
            || mesh.colors == nullptr) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to allocate mesh data for texture '%s'\n", batch.textureId.c_str());
        UnloadMesh(mesh);
        return Mesh{};
    }

    for (int i = 0; i < mesh.vertexCount; ++i) {
        const SurfaceVertex& vertex = batch.vertices[static_cast<size_t>(i)];
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
        mesh.colors[i * 4 + 0] = vertex.color.r;
        mesh.colors[i * 4 + 1] = vertex.color.g;
        mesh.colors[i * 4 + 2] = vertex.color.b;
        mesh.colors[i * 4 + 3] = vertex.color.a;
    }

    UploadMesh(&mesh, false);
    return mesh;
}

} // namespace

SectorMeshBuildResult BuildSectorMeshes(const SectorMap& map, const SectorLightmapLayout* lightmapLayout)
{
    SectorMeshBuildResult result;
    SectorGeneratedGeometry geometry;
    if (!BuildSectorGeneratedGeometry(map, geometry)) {
        return result;
    }

    std::vector<BatchBuilder> batchBuilders;
    std::unordered_map<std::string, size_t> batchIndexByTexture;

    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        BatchBuilder& batch = BatchForTexture(batchIndexByTexture, batchBuilders, surface.textureId);
        batch.vertices.reserve(batch.vertices.size() + surface.vertices.size());
        for (size_t vertexIndex = 0; vertexIndex < surface.vertices.size(); ++vertexIndex) {
            const SectorGeneratedVertex& vertex = surface.vertices[vertexIndex];
            batch.vertices.push_back(SurfaceVertex{
                    vertex.position,
                    vertex.normal,
                    vertex.uv,
                    ResolveLightmapUv(lightmapLayout, surfaceIndex, vertexIndex),
                    vertex.color
            });
        }
    }

    for (const BatchBuilder& builder : batchBuilders) {
        Mesh mesh = CreateMeshFromBatch(builder);
        if (mesh.vertexCount <= 0) {
            continue;
        }

        SectorMeshBatch batch;
        batch.textureId = builder.textureId;
        batch.mesh = mesh;
        batch.vertexCount = mesh.vertexCount;
        batch.triangleCount = mesh.triangleCount;
        result.vertexCount += batch.vertexCount;
        result.triangleCount += batch.triangleCount;
        result.batches.push_back(batch);
    }

    std::fprintf(
            stdout,
            "[SectorDemo] Generated %d vertices, %d triangles, %zu mesh batches\n",
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
