#include "sector_demo/SectorMeshBuilder.h"

#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorPortalVisibility.h"

#include <raylib.h>

#include <algorithm>
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
    int sectorId = -1;
    std::string textureId;
    std::string decalTextureId;
    float decalOpacity = 1.0f;
    bool decalEmissive = false;
    Vector3 decalTint = {1.0f, 1.0f, 1.0f};
    float decalBloomIntensity = 1.0f;
    bool alphaTest = false;
    float alphaCutoff = 0.5f;
    bool receivesLightmap = true;

    bool operator==(const SectorMeshBatchKey& other) const
    {
        return sectorId == other.sectorId
                && textureId == other.textureId
                && decalTextureId == other.decalTextureId
                && decalOpacity == other.decalOpacity
                && decalEmissive == other.decalEmissive
                && decalTint.x == other.decalTint.x
                && decalTint.y == other.decalTint.y
                && decalTint.z == other.decalTint.z
                && decalBloomIntensity == other.decalBloomIntensity
                && alphaTest == other.alphaTest
                && alphaCutoff == other.alphaCutoff
                && receivesLightmap == other.receivesLightmap;
    }
};

struct SectorMeshBatchKeyHash {
    size_t operator()(const SectorMeshBatchKey& key) const
    {
        const size_t sectorHash = std::hash<int>{}(key.sectorId);
        const size_t textureHash = std::hash<std::string>{}(key.textureId);
        const size_t decalHash = std::hash<std::string>{}(key.decalTextureId);
        const size_t opacityHash = std::hash<float>{}(key.decalOpacity);
        const size_t emissiveHash = std::hash<bool>{}(key.decalEmissive);
        const size_t tintXHash = std::hash<float>{}(key.decalTint.x);
        const size_t tintYHash = std::hash<float>{}(key.decalTint.y);
        const size_t tintZHash = std::hash<float>{}(key.decalTint.z);
        const size_t bloomIntensityHash = std::hash<float>{}(key.decalBloomIntensity);
        const size_t alphaTestHash = std::hash<bool>{}(key.alphaTest);
        const size_t alphaCutoffHash = std::hash<float>{}(key.alphaCutoff);
        const size_t receivesLightmapHash = std::hash<bool>{}(key.receivesLightmap);
        size_t hash = sectorHash ^ (textureHash + 0x9e3779b9u + (sectorHash << 6u) + (sectorHash >> 2u));
        hash ^= decalHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= opacityHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= emissiveHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= tintXHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= tintYHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= tintZHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= bloomIntensityHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= alphaTestHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= alphaCutoffHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        hash ^= receivesLightmapHash + 0x9e3779b9u + (hash << 6u) + (hash >> 2u);
        return hash;
    }
};

float UnitOrDefault(float value, float fallback)
{
    if (!std::isfinite(value)) {
        return fallback;
    }
    return std::clamp(value, 0.0f, 1.0f);
}

Vector3 UnitRgbOrWhite(Vector3 value)
{
    return Vector3{
            UnitOrDefault(value.x, 1.0f),
            UnitOrDefault(value.y, 1.0f),
            UnitOrDefault(value.z, 1.0f)
    };
}

float DecalBloomIntensityOrDefault(float value)
{
    if (!std::isfinite(value)) {
        return 1.0f;
    }
    return std::clamp(value, 0.0f, 10.0f);
}

SectorMeshBatchData& BatchForKey(
        std::unordered_map<SectorMeshBatchKey, size_t, SectorMeshBatchKeyHash>& batchIndexByKey,
        std::vector<SectorMeshBatchData>& batches,
        int sectorId,
        const std::string& textureId,
        const std::string& decalTextureId,
        float decalOpacity,
        bool decalEmissive,
        Vector3 decalTint,
        float decalBloomIntensity,
        bool alphaTest,
        float alphaCutoff,
        bool receivesLightmap)
{
    const bool hasDecal = !decalTextureId.empty();
    const SectorMeshBatchKey key{
            sectorId,
            textureId,
            decalTextureId,
            hasDecal ? UnitOrDefault(decalOpacity, 1.0f) : 1.0f,
            hasDecal && decalEmissive,
            hasDecal ? UnitRgbOrWhite(decalTint) : Vector3{1.0f, 1.0f, 1.0f},
            hasDecal ? DecalBloomIntensityOrDefault(decalBloomIntensity) : 1.0f,
            alphaTest,
            alphaTest ? UnitOrDefault(alphaCutoff, 0.5f) : 0.5f,
            receivesLightmap};
    const auto existing = batchIndexByKey.find(key);
    if (existing != batchIndexByKey.end()) {
        return batches[existing->second];
    }

    SectorMeshBatchData batch;
    batch.sectorId = key.sectorId;
    batch.textureId = textureId;
    batch.decalTextureId = decalTextureId;
    batch.decalOpacity = key.decalOpacity;
    batch.decalEmissive = key.decalEmissive;
    batch.decalTint = key.decalTint;
    batch.decalBloomIntensity = key.decalBloomIntensity;
    batch.alphaTest = key.alphaTest;
    batch.alphaCutoff = key.alphaCutoff;
    batch.receivesLightmap = key.receivesLightmap;
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

SectorMeshBatchDataResult BuildSectorMeshBatchDataInternal(
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout* lightmapLayout,
        bool groupBySector)
{
    SectorMeshBatchDataResult result;

    std::unordered_map<SectorMeshBatchKey, size_t, SectorMeshBatchKeyHash> batchIndexByKey;

    for (size_t surfaceIndex = 0; surfaceIndex < geometry.surfaces.size(); ++surfaceIndex) {
        const SectorGeneratedSurface& surface = geometry.surfaces[surfaceIndex];
        const int sectorId = groupBySector ? surface.ref.topologySectorId : -1;
        SectorMeshBatchData& batch = BatchForKey(
                batchIndexByKey,
                result.batches,
                sectorId,
                surface.textureId,
                surface.decalTextureId,
                surface.decalOpacity,
                surface.decalEmissive,
                surface.decalTint,
                surface.decalBloomIntensity,
                surface.alphaTest,
                surface.alphaCutoff,
                surface.receivesLightmap);
        batch.vertices.reserve(batch.vertices.size() + surface.vertices.size());
        for (size_t vertexIndex = 0; vertexIndex < surface.vertices.size(); ++vertexIndex) {
            const SectorGeneratedVertex& vertex = surface.vertices[vertexIndex];
            batch.vertices.push_back(SectorMeshBatchVertex{
                    vertex.position,
                    vertex.normal,
                    vertex.uv,
                    vertex.decalUv,
                    ResolveLightmapUv(lightmapLayout, surfaceIndex, vertexIndex),
                    batch.decalOpacity,
                    batch.decalEmissive,
                    batch.decalTint,
                    batch.decalBloomIntensity,
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

SectorMeshBatch MakeUploadedBatch(const SectorMeshBatchData& builder, Mesh mesh)
{
    SectorMeshBatch batch;
    batch.sectorId = builder.sectorId;
    batch.textureId = builder.textureId;
    batch.decalTextureId = builder.decalTextureId;
    batch.decalOpacity = builder.decalOpacity;
    batch.decalEmissive = builder.decalEmissive;
    batch.decalTint = builder.decalTint;
    batch.decalBloomIntensity = builder.decalBloomIntensity;
    batch.alphaTest = builder.alphaTest;
    batch.alphaCutoff = builder.alphaCutoff;
    batch.receivesLightmap = builder.receivesLightmap;
    batch.mesh = mesh;
    batch.vertexCount = mesh.vertexCount;
    batch.triangleCount = mesh.triangleCount;
    return batch;
}

} // namespace

SectorMeshBatchDataResult BuildSectorMeshBatchData(
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout* lightmapLayout)
{
    return BuildSectorMeshBatchDataInternal(geometry, lightmapLayout, false);
}

SectorMeshBatchDataResult BuildSectorMeshDrawRecordData(
        const SectorGeneratedGeometry& geometry,
        const SectorLightmapLayout* lightmapLayout)
{
    return BuildSectorMeshBatchDataInternal(geometry, lightmapLayout, true);
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
    result.batches.reserve(batchData.batches.size());
    for (const SectorMeshBatchData& builder : batchData.batches) {
        SectorMeshBatch batch;
        batch.sectorId = builder.sectorId;
        batch.textureId = builder.textureId;
        batch.decalTextureId = builder.decalTextureId;
        batch.decalOpacity = builder.decalOpacity;
        batch.decalEmissive = builder.decalEmissive;
        batch.decalTint = builder.decalTint;
        batch.decalBloomIntensity = builder.decalBloomIntensity;
        batch.alphaTest = builder.alphaTest;
        batch.alphaCutoff = builder.alphaCutoff;
        batch.receivesLightmap = builder.receivesLightmap;
        batch.vertexCount = builder.vertexCount;
        batch.triangleCount = builder.triangleCount;
        result.batches.push_back(batch);
    }

    const SectorMeshBatchDataResult drawRecordData = BuildSectorMeshDrawRecordData(geometry, lightmapLayout);
    result.sectorDrawRecords.reserve(drawRecordData.batches.size());
    for (const SectorMeshBatchData& builder : drawRecordData.batches) {
        Mesh mesh = CreateMeshFromBatch(builder);
        if (mesh.vertexCount <= 0) {
            continue;
        }

        SectorMeshBatch batch = MakeUploadedBatch(builder, mesh);
        result.vertexCount += batch.vertexCount;
        result.triangleCount += batch.triangleCount;
        result.sectorDrawRecords.push_back(batch);
    }

    std::fprintf(
            stdout,
            "[SectorDemo] Generated %d vertices, %d triangles, %zu sector mesh draw records\n",
            result.vertexCount,
            result.triangleCount,
            result.sectorDrawRecords.size()
    );

    return result;
}

bool ShouldDrawSectorMeshRecordForVisibility(
        const SectorMeshBatch& record,
        const RuntimePortalVisibilityResult& visibility)
{
    if (!visibility.validStartSector || visibility.fallbackDrawAll) {
        return true;
    }

    return std::find(
            visibility.visibleSectorIds.begin(),
            visibility.visibleSectorIds.end(),
            record.sectorId) != visibility.visibleSectorIds.end();
}

bool ShouldDrawEmissiveBloomSectorMeshRecordForVisibility(
        const SectorMeshBatch& record,
        const RuntimePortalVisibilityResult& visibility)
{
    return record.decalEmissive
            && !record.decalTextureId.empty()
            && ShouldDrawSectorMeshRecordForVisibility(record, visibility);
}

size_t CountSectorMeshDrawRecordsForVisibility(
        const std::vector<SectorMeshBatch>& records,
        const RuntimePortalVisibilityResult& visibility)
{
    size_t count = 0;
    for (const SectorMeshBatch& record : records) {
        if (ShouldDrawSectorMeshRecordForVisibility(record, visibility)) {
            ++count;
        }
    }
    return count;
}

void UnloadSectorMeshes(SectorMeshBuildResult& buildResult)
{
    for (SectorMeshBatch& batch : buildResult.sectorDrawRecords) {
        if (batch.mesh.vertexCount > 0) {
            UnloadMesh(batch.mesh);
            batch.mesh = Mesh{};
        }
    }

    buildResult = SectorMeshBuildResult{};
}

} // namespace game
