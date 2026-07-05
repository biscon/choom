#include "sector_demo/SectorPreviewDoorRenderer.h"

#include "sector_demo/SectorRuntimeObjects.h"

#include <raylib.h>

#include <cstdio>
#include <limits>

namespace game {

namespace {

Mesh CreateDoorSlabMesh(const SectorDoorSlabMeshData& data)
{
    Mesh mesh = {};
    if (data.vertices.empty()
            || data.vertices.size() > static_cast<size_t>(std::numeric_limits<int>::max())
            || data.indices.empty()
            || data.indices.size() % 3u != 0u
            || data.indices.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return mesh;
    }

    mesh.vertexCount = static_cast<int>(data.vertices.size());
    mesh.triangleCount = static_cast<int>(data.indices.size() / 3u);
    mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(unsigned char))));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(static_cast<unsigned int>(data.indices.size() * sizeof(unsigned short))));

    if (mesh.vertices == nullptr
            || mesh.normals == nullptr
            || mesh.texcoords == nullptr
            || mesh.colors == nullptr
            || mesh.indices == nullptr) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to allocate door slab mesh data\n");
        UnloadMesh(mesh);
        return Mesh{};
    }

    for (int i = 0; i < mesh.vertexCount; ++i) {
        const SectorDoorSlabMeshVertex& vertex = data.vertices[static_cast<size_t>(i)];
        mesh.vertices[i * 3 + 0] = vertex.position.x;
        mesh.vertices[i * 3 + 1] = vertex.position.y;
        mesh.vertices[i * 3 + 2] = vertex.position.z;
        mesh.normals[i * 3 + 0] = vertex.normal.x;
        mesh.normals[i * 3 + 1] = vertex.normal.y;
        mesh.normals[i * 3 + 2] = vertex.normal.z;
        mesh.texcoords[i * 2 + 0] = vertex.uv.x;
        mesh.texcoords[i * 2 + 1] = vertex.uv.y;
        mesh.colors[i * 4 + 0] = vertex.color.r;
        mesh.colors[i * 4 + 1] = vertex.color.g;
        mesh.colors[i * 4 + 2] = vertex.color.b;
        mesh.colors[i * 4 + 3] = vertex.color.a;
    }

    for (size_t i = 0; i < data.indices.size(); ++i) {
        mesh.indices[i] = data.indices[i];
    }

    UploadMesh(&mesh, false);
    return mesh;
}

} // namespace

void SectorPreviewDoorRenderer::ReserveRuntimeDoorCapacity(size_t capacity)
{
    doorMeshCache.reserve(capacity);
    runtimeDoorShadowCasters.clear();
    runtimeDoorShadowCasters.reserve(capacity);
}

void SectorPreviewDoorRenderer::PrepareRuntimeDoorMeshes(engine::World& runtimeObjectWorld)
{
    for (auto& entry : doorMeshCache) {
        entry.second.seenThisFrame = false;
    }
    runtimeDoorShadowCasters.clear();

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender>(
            [this](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render) {
                if (!AppendSectorDoorShadowCaster(
                            entity,
                            transform,
                            object,
                            door,
                            anchor,
                            render,
                            runtimeDoorShadowCasters)) {
                    return;
                }

                DoorMeshCacheEntry& cacheEntry = doorMeshCache[door.placedObjectId];
                cacheEntry.seenThisFrame = true;
                const bool meshDirty = cacheEntry.mesh.vertexCount <= 0
                        || cacheEntry.width != render.width
                        || cacheEntry.height != render.height
                        || cacheEntry.thickness != render.thickness
                        || !SameSectorDoorFaceUvSet(cacheEntry.faceUvs, render.faceUvs);
                if (meshDirty) {
                    if (cacheEntry.mesh.vertexCount > 0) {
                        UnloadMesh(cacheEntry.mesh);
                    }
                    cacheEntry.meshData = BuildSectorDoorSlabMeshData(render);
                    cacheEntry.mesh = CreateDoorSlabMesh(cacheEntry.meshData);
                    cacheEntry.width = render.width;
                    cacheEntry.height = render.height;
                    cacheEntry.thickness = render.thickness;
                    cacheEntry.faceUvs = render.faceUvs;
                }
            });

    for (auto it = doorMeshCache.begin(); it != doorMeshCache.end();) {
        if (!it->second.seenThisFrame) {
            if (it->second.mesh.vertexCount > 0) {
                UnloadMesh(it->second.mesh);
            }
            it = doorMeshCache.erase(it);
        } else {
            ++it;
        }
    }
}

void SectorPreviewDoorRenderer::ClearPreparedShadowCasters()
{
    runtimeDoorShadowCasters.clear();
}

void SectorPreviewDoorRenderer::UnloadDoorMeshes()
{
    for (auto& entry : doorMeshCache) {
        if (entry.second.mesh.vertexCount > 0) {
            UnloadMesh(entry.second.mesh);
            entry.second.mesh = Mesh{};
        }
    }
    doorMeshCache.clear();
    runtimeDoorShadowCasters.clear();
}

SectorPreviewDoorRenderer::DoorMeshCacheEntry* SectorPreviewDoorRenderer::FindMutableDoorMesh(int placedObjectId)
{
    auto cacheIt = doorMeshCache.find(placedObjectId);
    if (cacheIt == doorMeshCache.end()) {
        return nullptr;
    }
    return &cacheIt->second;
}

const SectorPreviewDoorRenderer::DoorMeshCacheEntry* SectorPreviewDoorRenderer::FindDoorMesh(int placedObjectId) const
{
    const auto cacheIt = doorMeshCache.find(placedObjectId);
    if (cacheIt == doorMeshCache.end()) {
        return nullptr;
    }
    return &cacheIt->second;
}

const Mesh* SectorPreviewDoorRenderer::ResolveDoorShadowCasterMesh(
        const SectorDoorShadowCaster& caster,
        float& outWidth,
        float& outHeight) const
{
    const DoorMeshCacheEntry* cacheEntry = FindDoorMesh(caster.placedObjectId);
    if (cacheEntry == nullptr || cacheEntry->mesh.vertexCount <= 0) {
        return nullptr;
    }

    outWidth = cacheEntry->width;
    outHeight = cacheEntry->height;
    return &cacheEntry->mesh;
}

const Mesh* SectorPreviewDoorRenderer::ResolveDoorShadowCasterMesh(
        void* userData,
        const SectorDoorShadowCaster& caster,
        float& outWidth,
        float& outHeight)
{
    const SectorPreviewDoorRenderer* renderer = static_cast<const SectorPreviewDoorRenderer*>(userData);
    if (renderer == nullptr) {
        return nullptr;
    }
    return renderer->ResolveDoorShadowCasterMesh(caster, outWidth, outHeight);
}

} // namespace game
