#include "sector_demo/SectorMeshBuilder.h"

#include "sector_demo/SectorMap.h"
#include "util/earcut.h"

#include <raylib.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {

namespace {

struct PointKey {
    int x = 0;
    int y = 0;
};

struct EdgeRef {
    int sectorIndex = -1;
    int edgeIndex = -1;
};

struct SurfaceVertex {
    Vector3 position = {};
    Vector3 normal = {};
    Vector2 uv = {};
};

struct BatchBuilder {
    std::string textureId;
    std::vector<SurfaceVertex> vertices;
};

constexpr float TextureWorldSize = 2.0f;
constexpr float EdgeEpsilon = 0.001f;

using EarcutPoint = std::array<double, 2>;
using EarcutRing = std::vector<EarcutPoint>;
using EarcutPolygon = std::vector<EarcutRing>;

PointKey MakePointKey(SectorPoint point)
{
    return PointKey{
            static_cast<int>(std::lround(point.x * 1000.0f)),
            static_cast<int>(std::lround(point.y * 1000.0f))
    };
}

std::string MakePointKeyString(PointKey point)
{
    return std::to_string(point.x) + "," + std::to_string(point.y);
}

std::string MakeEdgeKey(SectorPoint a, SectorPoint b)
{
    return MakePointKeyString(MakePointKey(a)) + ">" + MakePointKeyString(MakePointKey(b));
}

Vector3 ToWorld(SectorPoint point, float height)
{
    return Vector3{point.x, height, point.y};
}

float EdgeLength(SectorPoint a, SectorPoint b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

Vector3 WallNormal(SectorPoint a, SectorPoint b)
{
    const float dx = b.x - a.x;
    const float dz = b.y - a.y;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (length <= EdgeEpsilon) {
        return Vector3{0.0f, 0.0f, -1.0f};
    }

    return Vector3{-dz / length, 0.0f, dx / length};
}

bool SamePoint(SectorPoint a, SectorPoint b)
{
    return std::fabs(a.x - b.x) <= EdgeEpsilon && std::fabs(a.y - b.y) <= EdgeEpsilon;
}

bool IsCollinear(SectorPoint a, SectorPoint b, SectorPoint c)
{
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float acx = c.x - a.x;
    const float acy = c.y - a.y;
    return std::fabs(abx * acy - aby * acx) <= EdgeEpsilon;
}

bool RangeOverlaps(float a0, float a1, float b0, float b1)
{
    const float minA = std::fmin(a0, a1);
    const float maxA = std::fmax(a0, a1);
    const float minB = std::fmin(b0, b1);
    const float maxB = std::fmax(b0, b1);
    return std::fmax(minA, minB) < std::fmin(maxA, maxB) - EdgeEpsilon;
}

bool EdgesPartiallyOverlap(SectorPoint a0, SectorPoint a1, SectorPoint b0, SectorPoint b1)
{
    if (!IsCollinear(a0, a1, b0) || !IsCollinear(a0, a1, b1)) {
        return false;
    }

    if (SamePoint(a0, b1) && SamePoint(a1, b0)) {
        return false;
    }

    if (SamePoint(a0, b0) && SamePoint(a1, b1)) {
        return false;
    }

    if (std::fabs(a0.x - a1.x) >= std::fabs(a0.y - a1.y)) {
        return RangeOverlaps(a0.x, a1.x, b0.x, b1.x);
    }

    return RangeOverlaps(a0.y, a1.y, b0.y, b1.y);
}

void AddTriangle(BatchBuilder& batch, SurfaceVertex a, SurfaceVertex b, SurfaceVertex c)
{
    batch.vertices.push_back(a);
    batch.vertices.push_back(b);
    batch.vertices.push_back(c);
}

float TriangleArea2(SectorPoint a, SectorPoint b, SectorPoint c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

EarcutPolygon BuildEarcutPolygon(const SectorDefinition& sector)
{
    EarcutPolygon polygon;
    polygon.emplace_back();
    polygon.back().reserve(sector.points.size());

    for (const SectorPoint point : sector.points) {
        polygon.back().push_back(EarcutPoint{
                static_cast<double>(point.x),
                static_cast<double>(point.y)
        });
    }

    return polygon;
}

Vector2 ApplyUvSettings(Vector2 baseUv, Vector2 uvScale, Vector2 uvOffset)
{
    return Vector2{
            baseUv.x * uvScale.x + uvOffset.x,
            baseUv.y * uvScale.y + uvOffset.y
    };
}

void EmitFlatSectorSurface(
        BatchBuilder& batch,
        const SectorDefinition& sector,
        float height,
        Vector3 normal,
        bool facePositiveY,
        Vector2 uvScale,
        Vector2 uvOffset)
{
    const EarcutPolygon polygon = BuildEarcutPolygon(sector);
    const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
    if (indices.size() % 3 != 0) {
        std::fprintf(
                stderr,
                "[SectorDemo WARNING] Earcut returned malformed indices for sector '%s'\n",
                sector.id.c_str()
        );
        return;
    }

    const size_t pointCount = sector.points.size();
    for (size_t i = 0; i < indices.size(); i += 3) {
        const uint32_t ia = indices[i + 0];
        const uint32_t ib = indices[i + 1];
        const uint32_t ic = indices[i + 2];
        if (ia >= pointCount || ib >= pointCount || ic >= pointCount) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Earcut returned out-of-range index for sector '%s'\n",
                    sector.id.c_str()
            );
            return;
        }

        const SectorPoint pa = sector.points[ia];
        SectorPoint pb = sector.points[ib];
        SectorPoint pc = sector.points[ic];
        const float area2 = TriangleArea2(pa, pb, pc);
        if (std::fabs(area2) <= EdgeEpsilon) {
            continue;
        }

        // In world X/Z space, a clockwise 2D triangle produces a +Y geometric normal.
        if ((facePositiveY && area2 > 0.0f) || (!facePositiveY && area2 < 0.0f)) {
            const SectorPoint swap = pb;
            pb = pc;
            pc = swap;
        }

        AddTriangle(
                batch,
                SurfaceVertex{ToWorld(pa, height), normal, ApplyUvSettings(Vector2{pa.x / TextureWorldSize, pa.y / TextureWorldSize}, uvScale, uvOffset)},
                SurfaceVertex{ToWorld(pb, height), normal, ApplyUvSettings(Vector2{pb.x / TextureWorldSize, pb.y / TextureWorldSize}, uvScale, uvOffset)},
                SurfaceVertex{ToWorld(pc, height), normal, ApplyUvSettings(Vector2{pc.x / TextureWorldSize, pc.y / TextureWorldSize}, uvScale, uvOffset)}
        );
    }
}

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

void AddFloor(BatchBuilder& batch, const SectorDefinition& sector)
{
    EmitFlatSectorSurface(
            batch,
            sector,
            sector.floorZ,
            Vector3{0.0f, 1.0f, 0.0f},
            true,
            sector.floorUv.hasUvScale ? sector.floorUv.uvScale : Vector2{1.0f, 1.0f},
            sector.floorUv.hasUvOffset ? sector.floorUv.uvOffset : Vector2{0.0f, 0.0f}
    );
}

void AddCeiling(BatchBuilder& batch, const SectorDefinition& sector)
{
    EmitFlatSectorSurface(
            batch,
            sector,
            sector.ceilingZ,
            Vector3{0.0f, -1.0f, 0.0f},
            false,
            sector.ceilingUv.hasUvScale ? sector.ceilingUv.uvScale : Vector2{1.0f, 1.0f},
            sector.ceilingUv.hasUvOffset ? sector.ceilingUv.uvOffset : Vector2{0.0f, 0.0f}
    );
}

void AddWallQuad(
        BatchBuilder& batch,
        SectorPoint a,
        SectorPoint b,
        float bottom,
        float top,
        Vector2 uvScale,
        Vector2 uvOffset)
{
    if (top <= bottom + EdgeEpsilon) {
        return;
    }

    const Vector3 normal = WallNormal(a, b);
    const float length = EdgeLength(a, b);
    const float u1 = length / TextureWorldSize;
    const float v0 = bottom / TextureWorldSize;
    const float v1 = top / TextureWorldSize;

    SurfaceVertex af{ToWorld(a, bottom), normal, ApplyUvSettings(Vector2{0.0f, v0}, uvScale, uvOffset)};
    SurfaceVertex ac{ToWorld(a, top), normal, ApplyUvSettings(Vector2{0.0f, v1}, uvScale, uvOffset)};
    SurfaceVertex bc{ToWorld(b, top), normal, ApplyUvSettings(Vector2{u1, v1}, uvScale, uvOffset)};
    SurfaceVertex bf{ToWorld(b, bottom), normal, ApplyUvSettings(Vector2{u1, v0}, uvScale, uvOffset)};

    AddTriangle(batch, af, bc, ac);
    AddTriangle(batch, af, bf, bc);
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

    if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.texcoords == nullptr) {
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
    }

    UploadMesh(&mesh, false);
    return mesh;
}

void WarnAboutPartialEdges(const SectorMap& map)
{
    for (size_t sectorA = 0; sectorA < map.sectors.size(); ++sectorA) {
        const SectorDefinition& a = map.sectors[sectorA];
        for (size_t edgeA = 0; edgeA < a.points.size(); ++edgeA) {
            const SectorPoint a0 = a.points[edgeA];
            const SectorPoint a1 = a.points[(edgeA + 1) % a.points.size()];

            for (size_t sectorB = sectorA + 1; sectorB < map.sectors.size(); ++sectorB) {
                const SectorDefinition& b = map.sectors[sectorB];
                for (size_t edgeB = 0; edgeB < b.points.size(); ++edgeB) {
                    const SectorPoint b0 = b.points[edgeB];
                    const SectorPoint b1 = b.points[(edgeB + 1) % b.points.size()];
                    if (EdgesPartiallyOverlap(a0, a1, b0, b1)) {
                        std::fprintf(
                                stderr,
                                "[SectorDemo WARNING] Sectors '%s' and '%s' have partially matching edges; MVP requires exact reverse endpoints\n",
                                a.id.c_str(),
                                b.id.c_str()
                        );
                    }
                }
            }
        }
    }
}

} // namespace

SectorMeshBuildResult BuildSectorMeshes(const SectorMap& map)
{
    SectorMeshBuildResult result;
    std::vector<BatchBuilder> batchBuilders;
    std::unordered_map<std::string, size_t> batchIndexByTexture;
    std::unordered_map<std::string, EdgeRef> edgeRefs;

    WarnAboutPartialEdges(map);

    for (size_t sectorIndex = 0; sectorIndex < map.sectors.size(); ++sectorIndex) {
        const SectorDefinition& sector = map.sectors[sectorIndex];
        for (size_t edgeIndex = 0; edgeIndex < sector.points.size(); ++edgeIndex) {
            const SectorPoint a = sector.points[edgeIndex];
            const SectorPoint b = sector.points[(edgeIndex + 1) % sector.points.size()];
            const std::string edgeKey = MakeEdgeKey(a, b);
            if (edgeRefs.find(edgeKey) != edgeRefs.end()) {
                std::fprintf(
                        stderr,
                        "[SectorDemo WARNING] Duplicate same-direction edge found in sector '%s'\n",
                        sector.id.c_str()
                );
            }
            edgeRefs[edgeKey] = EdgeRef{static_cast<int>(sectorIndex), static_cast<int>(edgeIndex)};
        }
    }

    for (size_t sectorIndex = 0; sectorIndex < map.sectors.size(); ++sectorIndex) {
        const SectorDefinition& sector = map.sectors[sectorIndex];
        AddFloor(BatchForTexture(batchIndexByTexture, batchBuilders, sector.floorTextureId), sector);
        AddCeiling(BatchForTexture(batchIndexByTexture, batchBuilders, sector.ceilingTextureId), sector);

        for (size_t edgeIndex = 0; edgeIndex < sector.points.size(); ++edgeIndex) {
            const SectorPoint a = sector.points[edgeIndex];
            const SectorPoint b = sector.points[(edgeIndex + 1) % sector.points.size()];
            const EffectiveEdgeSettings edgeSettings = GetEffectiveEdgeSettings(sector, static_cast<int>(edgeIndex));
            const auto reverse = edgeRefs.find(MakeEdgeKey(b, a));
            if (reverse == edgeRefs.end() || reverse->second.sectorIndex == static_cast<int>(sectorIndex)) {
                AddWallQuad(
                        BatchForTexture(batchIndexByTexture, batchBuilders, edgeSettings.wall.textureId),
                        a,
                        b,
                        sector.floorZ,
                        sector.ceilingZ,
                        edgeSettings.wall.uvScale,
                        edgeSettings.wall.uvOffset
                );
                continue;
            }

            const SectorDefinition& neighbor = map.sectors[static_cast<size_t>(reverse->second.sectorIndex)];
            if (neighbor.floorZ > sector.floorZ) {
                AddWallQuad(
                        BatchForTexture(batchIndexByTexture, batchBuilders, edgeSettings.lower.textureId),
                        a,
                        b,
                        sector.floorZ,
                        neighbor.floorZ,
                        edgeSettings.lower.uvScale,
                        edgeSettings.lower.uvOffset
                );
            }

            if (neighbor.ceilingZ < sector.ceilingZ) {
                AddWallQuad(
                        BatchForTexture(batchIndexByTexture, batchBuilders, edgeSettings.upper.textureId),
                        a,
                        b,
                        neighbor.ceilingZ,
                        sector.ceilingZ,
                        edgeSettings.upper.uvScale,
                        edgeSettings.upper.uvOffset
                );
            }
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
