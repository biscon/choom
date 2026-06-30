#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorMeshBuilder.h"
#include "sector_demo/SectorPortalVisibility.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(float actual, float expected, float epsilon = 0.00001f)
{
    return std::fabs(actual - expected) <= epsilon;
}

bool Near(Vector2 actual, Vector2 expected, float epsilon = 0.00001f)
{
    return Near(actual.x, expected.x, epsilon) && Near(actual.y, expected.y, epsilon);
}

bool Near(Vector3 actual, Vector3 expected, float epsilon = 0.00001f)
{
    return Near(actual.x, expected.x, epsilon)
            && Near(actual.y, expected.y, epsilon)
            && Near(actual.z, expected.z, epsilon);
}

bool FiniteMatrix(Matrix matrix)
{
    return std::isfinite(matrix.m0) && std::isfinite(matrix.m1) && std::isfinite(matrix.m2) && std::isfinite(matrix.m3)
            && std::isfinite(matrix.m4) && std::isfinite(matrix.m5) && std::isfinite(matrix.m6) && std::isfinite(matrix.m7)
            && std::isfinite(matrix.m8) && std::isfinite(matrix.m9) && std::isfinite(matrix.m10) && std::isfinite(matrix.m11)
            && std::isfinite(matrix.m12) && std::isfinite(matrix.m13) && std::isfinite(matrix.m14) && std::isfinite(matrix.m15);
}

game::SectorReceiverBounds Bounds(int sectorId, Vector3 min, Vector3 max)
{
    game::SectorReceiverBounds bounds;
    bounds.sectorId = sectorId;
    bounds.min = min;
    bounds.max = max;
    return bounds;
}

game::SectorPreviewDynamicPointLightSource LightSource(
        int lightId,
        int ownerSectorId,
        Vector3 position,
        float radius,
        float intensity = 1.0f,
        Vector3 color = Vector3{1.0f, 1.0f, 1.0f})
{
    game::SectorPreviewDynamicPointLightSource source;
    source.lightId = lightId;
    source.ownerSectorId = ownerSectorId;
    source.light.lightId = lightId;
    source.light.position = position;
    source.light.color = color;
    source.light.radius = radius;
    source.light.intensity = intensity;
    return source;
}

game::SectorPreviewDynamicPointLightSource SpotLightSource(
        int lightId,
        int ownerSectorId,
        Vector3 position,
        float radius,
        float intensity = 1.0f,
        Vector3 color = Vector3{1.0f, 1.0f, 1.0f})
{
    game::SectorPreviewDynamicPointLightSource source = LightSource(
            lightId,
            ownerSectorId,
            position,
            radius,
            intensity,
            color);
    source.light.kind = game::SectorPreviewDynamicLightKind::Spot;
    source.light.direction = Vector3{1.0f, 0.0f, 0.0f};
    source.light.innerConeCos = std::cos(20.0f * 3.14159265358979323846f / 180.0f);
    source.light.outerConeCos = std::cos(35.0f * 3.14159265358979323846f / 180.0f);
    return source;
}

game::SectorPreviewDynamicPointLightSource ShadowSpotLightSource(
        int lightId,
        int ownerSectorId,
        Vector3 position,
        float radius,
        float intensity = 1.0f,
        int shadowPriority = game::DynamicSpotLightDefaultShadowPriority)
{
    game::SectorPreviewDynamicPointLightSource source = SpotLightSource(
            lightId,
            ownerSectorId,
            position,
            radius,
            intensity);
    source.light.castsShadow = true;
    source.light.shadowPriority = shadowPriority;
    return source;
}

bool HasCandidateLightId(
        const std::vector<game::SectorPreviewDynamicPointLightSource>& candidates,
        int lightId)
{
    for (const game::SectorPreviewDynamicPointLightSource& candidate : candidates) {
        if (candidate.lightId == lightId) {
            return true;
        }
    }
    return false;
}

bool HasShadowCasterLightId(
        const std::vector<game::SectorPreviewDynamicSpotLightShadowCaster>& shadowCasters,
        int lightId)
{
    for (const game::SectorPreviewDynamicSpotLightShadowCaster& shadowCaster : shadowCasters) {
        if (shadowCaster.lightId == lightId) {
            return true;
        }
    }
    return false;
}

const game::SectorPreviewDynamicPointLightSource* FindCandidateLightId(
        const std::vector<game::SectorPreviewDynamicPointLightSource>& candidates,
        int lightId)
{
    for (const game::SectorPreviewDynamicPointLightSource& candidate : candidates) {
        if (candidate.lightId == lightId) {
            return &candidate;
        }
    }
    return nullptr;
}

bool HasSelectedLightId(const std::vector<int>& selectedLightIds, int lightId)
{
    for (int selectedLightId : selectedLightIds) {
        if (selectedLightId == lightId) {
            return true;
        }
    }
    return false;
}

const game::SectorReceiverBounds* FindBounds(
        const std::vector<game::SectorReceiverBounds>& bounds,
        int sectorId)
{
    for (const game::SectorReceiverBounds& sectorBounds : bounds) {
        if (sectorBounds.sectorId == sectorId) {
            return &sectorBounds;
        }
    }
    return nullptr;
}

game::SectorTopologyWallPartSettings Part(const char* textureId)
{
    game::SectorTopologyWallPartSettings part;
    part.textureId = textureId;
    return part;
}

game::SectorTopologySector Sector(int id, float floorZ = 0.0f, float ceilingZ = 24.0f)
{
    game::SectorTopologySector sector;
    sector.id = id;
    sector.floorZ = floorZ;
    sector.ceilingZ = ceilingZ;
    sector.floorTextureId = "floor-" + std::to_string(id);
    sector.ceilingTextureId = "ceiling-" + std::to_string(id);
    sector.ambientColor = Color{200, 160, 120, 255};
    sector.ambientIntensity = 0.5f;
    return sector;
}

void AddSide(
        game::SectorTopologyMap& map,
        int sideId,
        int lineId,
        game::SectorTopologySideKind side,
        int sectorId,
        const char* prefix)
{
    game::SectorTopologySideDef sideDef;
    sideDef.id = sideId;
    sideDef.lineDefId = lineId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    sideDef.wall = Part((std::string(prefix) + "-wall").c_str());
    sideDef.lower = Part((std::string(prefix) + "-lower").c_str());
    sideDef.upper = Part((std::string(prefix) + "-upper").c_str());
    map.sideDefs.push_back(std::move(sideDef));
}

game::SectorTopologyMap MakeSquare()
{
    game::SectorTopologyMap map;
    map.vertices = {{1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64}};
    for (int i = 1; i <= 4; ++i) {
        const int end = i == 4 ? 1 : i + 1;
        map.lineDefs.push_back({i, i, end, i, -1});
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10, "square");
    }
    map.sectors.push_back(Sector(10));
    return map;
}

game::SectorTopologyMap MakeSquareWithHole()
{
    game::SectorTopologyMap map = MakeSquare();
    map.vertices.insert(map.vertices.end(), {
            {5, 16, 16}, {6, 16, 48}, {7, 48, 48}, {8, 48, 16}});
    for (int i = 5; i <= 8; ++i) {
        const int end = i == 8 ? 5 : i + 1;
        map.lineDefs.push_back({i, i, end, i, -1});
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10, "hole");
    }
    return map;
}

game::SectorTopologyMap MakeAdjacent(float leftFloor, float leftCeiling,
                                     float rightFloor, float rightCeiling)
{
    game::SectorTopologyMap map;
    map.vertices = {
            {1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64},
            {5, 128, 0}, {6, 128, 64}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, 8},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 3, 7, -1}};
    AddSide(map, 1, 1, game::SectorTopologySideKind::Front, 10, "left-1");
    AddSide(map, 2, 2, game::SectorTopologySideKind::Front, 10, "left-shared");
    AddSide(map, 3, 3, game::SectorTopologySideKind::Front, 10, "left-3");
    AddSide(map, 4, 4, game::SectorTopologySideKind::Front, 10, "left-4");
    AddSide(map, 5, 5, game::SectorTopologySideKind::Front, 20, "right-5");
    AddSide(map, 6, 6, game::SectorTopologySideKind::Front, 20, "right-6");
    AddSide(map, 7, 7, game::SectorTopologySideKind::Front, 20, "right-7");
    AddSide(map, 8, 2, game::SectorTopologySideKind::Back, 20, "right-shared");
    map.sectors.push_back(Sector(10, leftFloor, leftCeiling));
    map.sectors.push_back(Sector(20, rightFloor, rightCeiling));
    return map;
}

game::SectorTopologyMap MakeDiagonal()
{
    game::SectorTopologyMap map;
    map.vertices = {{1, 0, 0}, {2, 16, 16}, {3, 0, 32}};
    map.lineDefs = {{1, 1, 2, 1, -1}, {2, 2, 3, 2, -1}, {3, 3, 1, 3, -1}};
    for (int i = 1; i <= 3; ++i) {
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10, "diagonal");
    }
    map.sectors.push_back(Sector(10));
    return map;
}

bool HasBatchTexture(const game::SectorMeshBatchDataResult& result, const std::string& textureId)
{
    for (const game::SectorMeshBatchData& batch : result.batches) {
        if (batch.textureId == textureId) {
            return true;
        }
    }
    return false;
}

int CountSectorRecords(const game::SectorMeshBatchDataResult& result, int sectorId)
{
    int count = 0;
    for (const game::SectorMeshBatchData& batch : result.batches) {
        if (batch.sectorId == sectorId) {
            ++count;
        }
    }
    return count;
}

const game::SectorMeshBatchData* FindSectorRecord(
        const game::SectorMeshBatchDataResult& result,
        int sectorId,
        const std::string& textureId,
        const std::string& decalTextureId)
{
    for (const game::SectorMeshBatchData& batch : result.batches) {
        if (batch.sectorId == sectorId
                && batch.textureId == textureId
                && batch.decalTextureId == decalTextureId) {
            return &batch;
        }
    }
    return nullptr;
}

const game::SectorMeshBatchData* FindBatch(
        const game::SectorMeshBatchDataResult& result,
        const std::string& textureId,
        const std::string& decalTextureId)
{
    for (const game::SectorMeshBatchData& batch : result.batches) {
        if (batch.textureId == textureId && batch.decalTextureId == decalTextureId) {
            return &batch;
        }
    }
    return nullptr;
}

const game::SectorMeshBatchData* FindBatch(
        const game::SectorMeshBatchDataResult& result,
        const std::string& textureId,
        const std::string& decalTextureId,
        float decalOpacity,
        bool decalEmissive = false,
        Vector3 decalTint = {1.0f, 1.0f, 1.0f},
        float decalBloomIntensity = 1.0f)
{
    for (const game::SectorMeshBatchData& batch : result.batches) {
        if (batch.textureId == textureId
                && batch.decalTextureId == decalTextureId
                && Near(batch.decalOpacity, decalOpacity)
                && batch.decalEmissive == decalEmissive
                && Near(batch.decalTint, decalTint)
                && Near(batch.decalBloomIntensity, decalBloomIntensity)) {
            return &batch;
        }
    }
    return nullptr;
}

int CountBatches(
        const game::SectorMeshBatchDataResult& result,
        const std::string& textureId,
        const std::string& decalTextureId)
{
    int count = 0;
    for (const game::SectorMeshBatchData& batch : result.batches) {
        if (batch.textureId == textureId && batch.decalTextureId == decalTextureId) {
            ++count;
        }
    }
    return count;
}

int CountBatches(
        const game::SectorMeshBatchDataResult& result,
        const std::string& textureId,
        const std::string& decalTextureId,
        float decalOpacity,
        bool decalEmissive = false,
        Vector3 decalTint = {1.0f, 1.0f, 1.0f},
        float decalBloomIntensity = 1.0f)
{
    int count = 0;
    for (const game::SectorMeshBatchData& batch : result.batches) {
        if (batch.textureId == textureId
                && batch.decalTextureId == decalTextureId
                && Near(batch.decalOpacity, decalOpacity)
                && batch.decalEmissive == decalEmissive
                && Near(batch.decalTint, decalTint)
                && Near(batch.decalBloomIntensity, decalBloomIntensity)) {
            ++count;
        }
    }
    return count;
}

game::SectorGeneratedSurface MakeBatchTestSurface(
        const char* textureId,
        const char* decalTextureId,
        Vector2 decalUv,
        float decalOpacity,
        float xOffset,
        bool decalEmissive = false,
        Vector3 decalTint = {1.0f, 1.0f, 1.0f},
        float decalBloomIntensity = 1.0f)
{
    game::SectorGeneratedSurface surface;
    surface.textureId = textureId;
    surface.decalTextureId = decalTextureId;
    surface.decalOpacity = decalTextureId[0] == '\0' ? 1.0f : decalOpacity;
    surface.decalEmissive = decalTextureId[0] != '\0' && decalEmissive;
    surface.decalTint = decalTextureId[0] == '\0' ? Vector3{1.0f, 1.0f, 1.0f} : decalTint;
    surface.decalBloomIntensity = decalTextureId[0] == '\0' ? 1.0f : decalBloomIntensity;
    surface.normal = Vector3{0.0f, 1.0f, 0.0f};
    surface.vertices = {
            game::SectorGeneratedVertex{
                    Vector3{xOffset, 0.0f, 0.0f},
                    surface.normal,
                    Vector2{0.0f, 0.0f},
                    decalUv,
                    Vector2{0.0f, 0.0f},
                    WHITE},
            game::SectorGeneratedVertex{
                    Vector3{xOffset + 1.0f, 0.0f, 0.0f},
                    surface.normal,
                    Vector2{1.0f, 0.0f},
                    Vector2{decalUv.x + 1.0f, decalUv.y},
                    Vector2{1.0f, 0.0f},
                    WHITE},
            game::SectorGeneratedVertex{
                    Vector3{xOffset, 0.0f, 1.0f},
                    surface.normal,
                    Vector2{0.0f, 1.0f},
                    Vector2{decalUv.x, decalUv.y + 1.0f},
                    Vector2{0.0f, 1.0f},
                    WHITE}};
    return surface;
}

game::SectorMeshBatch MakeDrawRecord(
        int sectorId,
        const char* textureId,
        const char* decalTextureId = "",
        bool decalEmissive = false)
{
    game::SectorMeshBatch record;
    record.sectorId = sectorId;
    record.textureId = textureId;
    record.decalTextureId = decalTextureId;
    record.decalEmissive = decalEmissive;
    return record;
}

game::SectorGeneratedSurface MakeMiddleBatchTestSurface(const char* textureId, float xOffset)
{
    game::SectorGeneratedSurface surface =
            MakeBatchTestSurface(textureId, "", Vector2{0.0f, 0.0f}, 1.0f, xOffset);
    surface.ref.kind = game::SectorGeneratedSurfaceKind::Middle;
    surface.alphaTest = true;
    surface.alphaCutoff = 0.5f;
    surface.receivesLightmap = true;
    return surface;
}

int CountTrianglesByKind(
        const game::SectorGeneratedGeometry& geometry,
        game::SectorGeneratedSurfaceKind kind,
        int lineDefId = -1)
{
    int count = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.kind == kind
                && (lineDefId < 0 || surface.ref.topologyLineDefId == lineDefId)) {
            count += static_cast<int>(surface.vertices.size() / 3);
        }
    }
    return count;
}

int CountPortalWallTriangles(const game::SectorGeneratedGeometry& geometry, int lineDefId)
{
    return CountTrianglesByKind(geometry, game::SectorGeneratedSurfaceKind::Wall, lineDefId)
            + CountTrianglesByKind(geometry, game::SectorGeneratedSurfaceKind::LowerWall, lineDefId)
            + CountTrianglesByKind(geometry, game::SectorGeneratedSurfaceKind::UpperWall, lineDefId);
}

Vector3 Subtract(Vector3 a, Vector3 b)
{
    return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Cross(Vector3 a, Vector3 b)
{
    return Vector3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

float Dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float Length(Vector3 value)
{
    return std::sqrt(Dot(value, value));
}

bool TriangleWindingMatchesSurfaceNormal(const game::SectorGeneratedSurface& surface)
{
    constexpr float epsilon = 0.0001f;
    for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
        const Vector3 a = surface.vertices[i].position;
        const Vector3 b = surface.vertices[i + 1].position;
        const Vector3 c = surface.vertices[i + 2].position;
        const Vector3 geometricNormal = Cross(Subtract(b, a), Subtract(c, a));
        const float length = Length(geometricNormal);
        if (length <= epsilon) {
            return false;
        }
        if (Dot(Vector3{
                    geometricNormal.x / length,
                    geometricNormal.y / length,
                    geometricNormal.z / length},
                surface.normal) <= epsilon) {
            return false;
        }
    }
    return true;
}

Ray MakeRay(Vector3 position, Vector3 direction)
{
    return Ray{position, direction};
}

game::SectorGeneratedGeometry BuildGeometryOrFail(const game::SectorTopologyMap& map, const char* description)
{
    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), description);
    Check(error.empty(), "successful topology geometry build clears error");
    return geometry;
}

void TestSquareTopologyMeshBatchData()
{
    const game::SectorGeneratedGeometry geometry = BuildGeometryOrFail(MakeSquare(), "square topology geometry builds");
    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshBatchData(geometry);
    Check(!result.batches.empty(), "square topology batch data has batches");
    Check(result.vertexCount > 0, "square topology batch data has vertices");
    Check(result.triangleCount > 0, "square topology batch data has triangles");
    Check(HasBatchTexture(result, "floor-10"), "square topology batch data contains floor texture");
    Check(HasBatchTexture(result, "ceiling-10"), "square topology batch data contains ceiling texture");
    Check(HasBatchTexture(result, "square-wall"), "square topology batch data contains wall texture");
}

void TestOneSectorMeshDrawRecords()
{
    const game::SectorGeneratedGeometry geometry = BuildGeometryOrFail(MakeSquare(), "square topology geometry builds");
    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshDrawRecordData(geometry);
    Check(!result.batches.empty(), "one-sector topology draw records are built");
    Check(result.vertexCount > 0 && result.triangleCount > 0,
          "one-sector topology draw records include geometry");
    Check(CountSectorRecords(result, 10) == static_cast<int>(result.batches.size()),
          "one-sector topology draw records retain sector ownership");
}

void TestTwoSectorsWithSameMaterialKeepSeparateDrawRecords()
{
    game::SectorGeneratedGeometry geometry;
    game::SectorGeneratedSurface left = MakeBatchTestSurface("shared", "", Vector2{0.0f, 0.0f}, 1.0f, 0.0f);
    left.ref.topologySectorId = 10;
    game::SectorGeneratedSurface right = MakeBatchTestSurface("shared", "", Vector2{0.0f, 0.0f}, 1.0f, 2.0f);
    right.ref.topologySectorId = 20;
    geometry.surfaces.push_back(left);
    geometry.surfaces.push_back(right);

    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshDrawRecordData(geometry);
    Check(result.batches.size() == 2,
          "same material in two sectors produces separate sector draw records");
    Check(FindSectorRecord(result, 10, "shared", "") != nullptr,
          "left sector keeps its shared material draw record");
    Check(FindSectorRecord(result, 20, "shared", "") != nullptr,
          "right sector keeps its shared material draw record");
}

void TestDecalMeshBatchData()
{
    game::SectorGeneratedGeometry geometry;
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-a", Vector2{0.25f, 0.5f}, 0.6f, 0.0f));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-b", Vector2{1.25f, 1.5f}, 0.7f, 2.0f));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-a", Vector2{2.25f, 2.5f}, 0.6f, 4.0f));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-a", Vector2{3.25f, 3.5f}, 0.8f, 6.0f));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "", Vector2{4.25f, 4.5f}, 1.0f, 8.0f));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "", Vector2{5.25f, 5.5f}, 0.2f, 10.0f));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-a", Vector2{6.25f, 6.5f}, 0.6f, 12.0f, true));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-a", Vector2{7.25f, 7.5f}, 0.6f, 14.0f, true));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-a", Vector2{8.25f, 8.5f}, 0.6f, 16.0f, false, Vector3{1.0f, 0.25f, 0.25f}));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-a", Vector2{9.25f, 9.5f}, 0.6f, 18.0f, true, Vector3{1.0f, 1.0f, 1.0f}, 3.0f));
    geometry.surfaces.push_back(MakeBatchTestSurface("stone", "poster-a", Vector2{10.25f, 10.5f}, 0.6f, 20.0f, false, Vector3{1.0f, 1.0f, 1.0f}, std::numeric_limits<float>::infinity()));

    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshBatchData(geometry);
    Check(result.batches.size() == 7, "decal batch key separates base texture by decal settings");
    Check(CountBatches(result, "stone", "poster-a") == 5, "same decal texture with different settings splits batches");
    Check(CountBatches(result, "stone", "poster-a", 0.6f) == 1, "same base decal opacity and bloom intensity share one batch");
    Check(CountBatches(result, "stone", "poster-a", 0.8f) == 1, "different decal opacity creates a separate batch");
    Check(CountBatches(result, "stone", "poster-a", 0.6f, true) == 1, "different decal emissive flag creates a separate batch");
    Check(CountBatches(result, "stone", "poster-a", 0.6f, false, Vector3{1.0f, 0.25f, 0.25f}) == 1,
          "different decal tint creates a separate batch");
    Check(CountBatches(result, "stone", "poster-a", 0.6f, true, Vector3{1.0f, 1.0f, 1.0f}, 3.0f) == 1,
          "different decal bloom intensity creates a separate batch");
    Check(CountBatches(result, "stone", "poster-b") == 1, "different decal texture creates separate batch");
    Check(CountBatches(result, "stone", "") == 1, "missing decals batch with empty decal key");

    const game::SectorMeshBatchData* posterA = FindBatch(result, "stone", "poster-a", 0.6f);
    const game::SectorMeshBatchData* posterAHighOpacity = FindBatch(result, "stone", "poster-a", 0.8f);
    const game::SectorMeshBatchData* posterAEmissive = FindBatch(result, "stone", "poster-a", 0.6f, true);
    const game::SectorMeshBatchData* posterATinted = FindBatch(result, "stone", "poster-a", 0.6f, false, Vector3{1.0f, 0.25f, 0.25f});
    const game::SectorMeshBatchData* posterABrightBloom = FindBatch(result, "stone", "poster-a", 0.6f, true, Vector3{1.0f, 1.0f, 1.0f}, 3.0f);
    const game::SectorMeshBatchData* posterB = FindBatch(result, "stone", "poster-b");
    const game::SectorMeshBatchData* noDecal = FindBatch(result, "stone", "");
    Check(posterA != nullptr && posterA->vertices.size() == 9,
          "same decal batch contains matching poster-a surfaces with default bloom intensity");
    Check(posterAHighOpacity != nullptr && posterAHighOpacity->vertices.size() == 3,
          "different opacity poster-a surface is isolated for uniform opacity");
    Check(posterAEmissive != nullptr && posterAEmissive->vertices.size() == 6,
          "same emissive decal batch contains matching surfaces");
    Check(posterATinted != nullptr && posterATinted->vertices.size() == 3,
          "different tint poster-a surface is isolated");
    Check(posterB != nullptr && posterB->vertices.size() == 3,
          "different decal batch contains poster-b surface");
    Check(noDecal != nullptr && noDecal->vertices.size() == 6,
          "empty decal batch contains no-decal surfaces");

    if (posterA != nullptr && !posterA->vertices.empty()) {
        Check(Near(posterA->decalOpacity, 0.6f),
              "mesh batch stores uniform decal opacity");
        Check(!posterA->decalEmissive, "mesh batch stores default decal emissive flag");
        Check(Near(posterA->decalTint, Vector3{1.0f, 1.0f, 1.0f}),
              "mesh batch stores default decal tint");
        Check(Near(posterA->decalBloomIntensity, 1.0f),
              "mesh batch canonicalizes default decal bloom intensity");
        Check(Near(posterA->vertices.front().decalUv, Vector2{0.25f, 0.5f}),
              "mesh batch preserves decal UV");
        Check(Near(posterA->vertices.front().decalOpacity, 0.6f),
              "mesh batch preserves decal opacity");
        Check(Near(posterA->vertices.front().decalBloomIntensity, 1.0f),
              "mesh batch vertex stores canonicalized decal bloom intensity");
    }
    if (posterAEmissive != nullptr && !posterAEmissive->vertices.empty()) {
        Check(posterAEmissive->decalEmissive, "mesh batch preserves emissive decal flag");
    }
    if (posterATinted != nullptr && !posterATinted->vertices.empty()) {
        Check(Near(posterATinted->decalTint, Vector3{1.0f, 0.25f, 0.25f}),
              "mesh batch preserves decal tint");
    }
    if (posterABrightBloom != nullptr && !posterABrightBloom->vertices.empty()) {
        Check(Near(posterABrightBloom->decalBloomIntensity, 3.0f),
              "mesh batch preserves decal bloom intensity");
    }
    if (noDecal != nullptr && !noDecal->vertices.empty()) {
        Check(noDecal->decalTextureId.empty(), "no-decal batch stores empty decal texture ID");
        Check(Near(noDecal->decalOpacity, 1.0f),
              "no-decal batch stores default uniform opacity");
        Check(!noDecal->decalEmissive, "no-decal batch stores default emissive flag");
        Check(Near(noDecal->decalTint, Vector3{1.0f, 1.0f, 1.0f}),
              "no-decal batch stores default tint");
        Check(Near(noDecal->decalBloomIntensity, 1.0f),
              "no-decal batch stores default bloom intensity");
        Check(Near(noDecal->vertices.front().decalOpacity, 1.0f),
              "no-decal batch stores default opacity");
    }
}

void TestSectorDrawRecordMaterialGrouping()
{
    game::SectorGeneratedGeometry geometry;
    game::SectorGeneratedSurface baseA = MakeBatchTestSurface("stone", "", Vector2{0.0f, 0.0f}, 1.0f, 0.0f);
    baseA.ref.topologySectorId = 10;
    game::SectorGeneratedSurface baseB = MakeBatchTestSurface("stone", "", Vector2{0.0f, 0.0f}, 1.0f, 2.0f);
    baseB.ref.topologySectorId = 10;
    game::SectorGeneratedSurface decal = MakeBatchTestSurface("stone", "poster", Vector2{0.25f, 0.5f}, 0.6f, 4.0f);
    decal.ref.topologySectorId = 10;
    geometry.surfaces.push_back(baseA);
    geometry.surfaces.push_back(baseB);
    geometry.surfaces.push_back(decal);

    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshDrawRecordData(geometry);
    const game::SectorMeshBatchData* baseRecord = FindSectorRecord(result, 10, "stone", "");
    const game::SectorMeshBatchData* decalRecord = FindSectorRecord(result, 10, "stone", "poster");
    Check(result.batches.size() == 2,
          "sector draw record grouping still uses material and decal key");
    Check(baseRecord != nullptr && baseRecord->vertices.size() == 6,
          "matching sector/material surfaces share one sector draw record");
    Check(decalRecord != nullptr && decalRecord->vertices.size() == 3,
          "different decal key creates a separate sector draw record");
    Check(decalRecord != nullptr && Near(decalRecord->decalOpacity, 0.6f),
          "sector draw record preserves decal opacity");
}

void TestSectorDrawRecordLightmapUvsUseOriginalSurfaceIndex()
{
    game::SectorGeneratedGeometry geometry;
    game::SectorGeneratedSurface first = MakeBatchTestSurface("stone", "", Vector2{0.0f, 0.0f}, 1.0f, 0.0f);
    first.ref.topologySectorId = 10;
    game::SectorGeneratedSurface second = MakeBatchTestSurface("stone", "", Vector2{0.0f, 0.0f}, 1.0f, 2.0f);
    second.ref.topologySectorId = 20;
    geometry.surfaces.push_back(first);
    geometry.surfaces.push_back(second);

    game::SectorLightmapLayout layout;
    layout.charts.resize(2);
    layout.charts[0].vertexUvs = {Vector2{0.1f, 0.2f}, Vector2{0.2f, 0.2f}, Vector2{0.1f, 0.3f}};
    layout.charts[1].vertexUvs = {Vector2{0.7f, 0.8f}, Vector2{0.8f, 0.8f}, Vector2{0.7f, 0.9f}};

    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshDrawRecordData(geometry, &layout);
    const game::SectorMeshBatchData* firstRecord = FindSectorRecord(result, 10, "stone", "");
    const game::SectorMeshBatchData* secondRecord = FindSectorRecord(result, 20, "stone", "");
    Check(firstRecord != nullptr && Near(firstRecord->vertices.front().lightmapUv, Vector2{0.1f, 0.2f}),
          "first sector draw record copies lightmap UVs from original surface index");
    Check(secondRecord != nullptr && Near(secondRecord->vertices.front().lightmapUv, Vector2{0.7f, 0.8f}),
          "second sector draw record copies lightmap UVs from original surface index");
}

void TestMiddleTextureBatchState()
{
    game::SectorGeneratedGeometry geometry;
    game::SectorGeneratedSurface opaque = MakeBatchTestSurface("shared", "", Vector2{0.0f, 0.0f}, 1.0f, 0.0f);
    opaque.ref.kind = game::SectorGeneratedSurfaceKind::Wall;
    geometry.surfaces.push_back(opaque);
    geometry.surfaces.push_back(MakeMiddleBatchTestSurface("shared", 2.0f));
    geometry.surfaces.push_back(MakeMiddleBatchTestSurface("shared", 4.0f));

    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshBatchData(geometry);
    Check(result.batches.size() == 2,
          "alpha test splits middle textures from opaque walls");

    const game::SectorMeshBatchData* opaqueBatch = nullptr;
    const game::SectorMeshBatchData* middleBatch = nullptr;
    for (const game::SectorMeshBatchData& batch : result.batches) {
        if (batch.textureId != "shared") {
            continue;
        }
        if (batch.alphaTest) {
            middleBatch = &batch;
        } else {
            opaqueBatch = &batch;
        }
    }

    Check(opaqueBatch != nullptr && !opaqueBatch->alphaTest && opaqueBatch->receivesLightmap,
          "ordinary wall batch remains opaque and lightmapped");
    Check(middleBatch != nullptr && middleBatch->alphaTest && middleBatch->receivesLightmap,
          "middle texture batch stores alpha-test and lightmapped state");
    Check(middleBatch != nullptr && Near(middleBatch->alphaCutoff, 0.5f),
          "middle texture batch stores alpha cutoff");
    Check(middleBatch != nullptr && middleBatch->vertices.size() == 6,
          "matching middle texture surfaces share the alpha-test batch");
}

void TestSectorDrawRecordMiddleTextureAlphaTest()
{
    game::SectorGeneratedGeometry geometry;
    game::SectorGeneratedSurface middle = MakeMiddleBatchTestSurface("bars", 0.0f);
    middle.ref.topologySectorId = 10;
    geometry.surfaces.push_back(middle);

    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshDrawRecordData(geometry);
    const game::SectorMeshBatchData* record = FindSectorRecord(result, 10, "bars", "");
    Check(record != nullptr && record->alphaTest,
          "middle texture sector draw record preserves alpha-test state");
    Check(record != nullptr && Near(record->alphaCutoff, 0.5f),
          "middle texture sector draw record preserves alpha cutoff");
    Check(record != nullptr && record->receivesLightmap,
          "middle texture sector draw record preserves lightmap receiver state");
}

void TestSectorDrawRecordEmissiveDecalMetadata()
{
    game::SectorGeneratedGeometry geometry;
    game::SectorGeneratedSurface emissive =
            MakeBatchTestSurface("stone", "sign", Vector2{0.25f, 0.5f}, 0.75f, 0.0f, true, Vector3{0.5f, 1.0f, 0.25f}, 4.0f);
    emissive.ref.topologySectorId = 10;
    geometry.surfaces.push_back(emissive);

    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshDrawRecordData(geometry);
    const game::SectorMeshBatchData* record = FindSectorRecord(result, 10, "stone", "sign");
    Check(record != nullptr && record->decalEmissive,
          "emissive decal sector draw record preserves emissive flag");
    Check(record != nullptr && Near(record->decalTint, Vector3{0.5f, 1.0f, 0.25f}),
          "emissive decal sector draw record preserves tint");
    Check(record != nullptr && Near(record->decalBloomIntensity, 4.0f),
          "emissive decal sector draw record preserves bloom intensity");
}

void TestLightmapParticipationSplitsBatchKey()
{
    game::SectorGeneratedGeometry geometry;
    game::SectorGeneratedSurface lightmapped = MakeBatchTestSurface("stone", "", Vector2{0.0f, 0.0f}, 1.0f, 0.0f);
    game::SectorGeneratedSurface unlightmapped = MakeBatchTestSurface("stone", "", Vector2{0.0f, 0.0f}, 1.0f, 2.0f);
    unlightmapped.receivesLightmap = false;
    geometry.surfaces.push_back(lightmapped);
    geometry.surfaces.push_back(unlightmapped);

    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshBatchData(geometry);
    Check(result.batches.size() == 2,
          "receivesLightmap participates in the batch key even without alpha test");
}

void TestHoleTopologyMeshBatchData()
{
    const game::SectorGeneratedGeometry geometry = BuildGeometryOrFail(MakeSquareWithHole(), "hole topology geometry builds");
    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshBatchData(geometry);
    Check(!result.batches.empty(), "hole topology batch data has batches");
    Check(result.vertexCount > 0 && result.triangleCount > 0, "hole topology batch data has geometry");
    Check(CountTrianglesByKind(geometry, game::SectorGeneratedSurfaceKind::Floor) > 0,
          "hole topology generated floor triangles");
    Check(CountTrianglesByKind(geometry, game::SectorGeneratedSurfaceKind::Ceiling) > 0,
          "hole topology generated ceiling triangles");
}

void TestEqualHeightPortal()
{
    const game::SectorGeneratedGeometry geometry =
            BuildGeometryOrFail(MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f), "equal-height portal geometry builds");
    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshBatchData(geometry);
    Check(result.triangleCount > 0, "equal-height portal batch data has triangles");
    Check(CountPortalWallTriangles(geometry, 2) == 0, "equal-height shared portal emits no wall triangles");
    Check(CountPortalWallTriangles(geometry, 1) > 0 && CountPortalWallTriangles(geometry, 7) > 0,
          "equal-height portal external walls still emit triangles");
}

void TestDifferentHeightPortal()
{
    const game::SectorGeneratedGeometry geometry =
            BuildGeometryOrFail(MakeAdjacent(0.0f, 24.0f, 8.0f, 16.0f), "different-height portal geometry builds");
    const game::SectorMeshBatchDataResult result = game::BuildSectorMeshBatchData(geometry);
    Check(result.triangleCount > 0, "different-height portal batch data has triangles");
    Check(CountTrianglesByKind(geometry, game::SectorGeneratedSurfaceKind::LowerWall, 2) > 0,
          "different-height portal emits lower wall triangles");
    Check(CountTrianglesByKind(geometry, game::SectorGeneratedSurfaceKind::UpperWall, 2) > 0,
          "different-height portal emits upper wall triangles");
}

void TestTriangleWindingAgainstNormals()
{
    const std::vector<game::SectorTopologyMap> maps = {
            MakeSquare(),
            MakeAdjacent(0.0f, 24.0f, 8.0f, 16.0f),
            MakeDiagonal()};
    int floorCount = 0;
    int ceilingCount = 0;
    int wallCount = 0;
    int lowerCount = 0;
    int upperCount = 0;
    int diagonalWallCount = 0;

    for (size_t mapIndex = 0; mapIndex < maps.size(); ++mapIndex) {
        const game::SectorGeneratedGeometry geometry = BuildGeometryOrFail(maps[mapIndex], "winding topology geometry builds");
        for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
            Check(TriangleWindingMatchesSurfaceNormal(surface), "triangle winding agrees with surface normal");
            switch (surface.ref.kind) {
                case game::SectorGeneratedSurfaceKind::Floor:
                    ++floorCount;
                    break;
                case game::SectorGeneratedSurfaceKind::Ceiling:
                    ++ceilingCount;
                    break;
                case game::SectorGeneratedSurfaceKind::Wall:
                    ++wallCount;
                    if (surface.ref.topologyLineDefId == 1 && mapIndex == 2) {
                        ++diagonalWallCount;
                    }
                    break;
                case game::SectorGeneratedSurfaceKind::LowerWall:
                    ++lowerCount;
                    break;
                case game::SectorGeneratedSurfaceKind::UpperWall:
                    ++upperCount;
                    break;
                case game::SectorGeneratedSurfaceKind::Middle:
                    break;
            }
        }
    }

    Check(floorCount > 0, "winding test covered floors");
    Check(ceilingCount > 0, "winding test covered ceilings");
    Check(wallCount > 0, "winding test covered one-sided walls");
    Check(lowerCount > 0, "winding test covered lower walls");
    Check(upperCount > 0, "winding test covered upper walls");
    Check(diagonalWallCount > 0, "winding test covered diagonal walls");
}

void TestInvalidTopologyMeshBuilder()
{
    game::SectorTopologyMap invalid = MakeSquare();
    invalid.lineDefs.front().startVertexId = 999;
    std::string error = "stale";
    const game::SectorMeshBuildResult result = game::BuildSectorMeshes(invalid, nullptr, &error);
    Check(result.batches.empty(), "invalid topology mesh builder returns no batches");
    Check(result.vertexCount == 0 && result.triangleCount == 0, "invalid topology mesh builder returns empty counts");
    Check(error.find("validation") != std::string::npos && error.find("999") != std::string::npos,
          "invalid topology mesh builder reports validation error");
}

void TestGeneratedGeometryPickingKeepsTopologyRefs()
{
    const game::SectorGeneratedGeometry square = BuildGeometryOrFail(MakeSquare(), "pick square geometry builds");
    const game::SectorGeneratedSurfaceHit floorHit = game::PickSectorGeneratedGeometry(
            square,
            MakeRay(Vector3{0.25f, -1.0f, 0.25f}, Vector3{0.0f, 1.0f, 0.0f}));
    Check(floorHit.hit, "floor ray hits generated topology geometry");
    Check(floorHit.ref.kind == game::SectorGeneratedSurfaceKind::Floor, "floor ray returns floor kind");
    Check(floorHit.ref.topologySectorId == 10, "floor ray preserves topology sector id");

    const game::SectorGeneratedSurfaceHit wallHit = game::PickSectorGeneratedGeometry(
            square,
            MakeRay(Vector3{0.25f, 1.5f, -1.0f}, Vector3{0.0f, 0.0f, 1.0f}));
    Check(wallHit.hit, "wall ray hits generated topology geometry");
    Check(wallHit.ref.kind == game::SectorGeneratedSurfaceKind::Wall, "wall ray returns wall kind");
    Check(wallHit.ref.topologyLineDefId == 1, "wall ray preserves topology linedef id");
    Check(wallHit.ref.topologySideDefId == 1, "wall ray preserves topology sidedef id");
    Check(wallHit.ref.topologySide == game::SectorTopologySideKind::Front, "wall ray preserves topology side");

    const game::SectorGeneratedGeometry stepped =
            BuildGeometryOrFail(MakeAdjacent(0.0f, 24.0f, 8.0f, 16.0f), "pick stepped geometry builds");
    const game::SectorGeneratedSurfaceHit lowerHit = game::PickSectorGeneratedGeometry(
            stepped,
            MakeRay(Vector3{1.0f, 0.5f, 0.25f}, Vector3{-1.0f, 0.0f, 0.0f}));
    Check(lowerHit.hit, "lower wall ray hits generated topology geometry");
    Check(lowerHit.ref.kind == game::SectorGeneratedSurfaceKind::LowerWall, "lower wall ray returns lower kind");
    Check(lowerHit.ref.topologyLineDefId == 2, "lower wall ray preserves topology linedef id");
    Check(lowerHit.ref.topologySideDefId == 2, "lower wall ray preserves topology sidedef id");

    const game::SectorGeneratedSurfaceHit upperHit = game::PickSectorGeneratedGeometry(
            stepped,
            MakeRay(Vector3{1.0f, 2.5f, 0.25f}, Vector3{-1.0f, 0.0f, 0.0f}));
    Check(upperHit.hit, "upper wall ray hits generated topology geometry");
    Check(upperHit.ref.kind == game::SectorGeneratedSurfaceKind::UpperWall, "upper wall ray returns upper kind");
    Check(upperHit.ref.topologyLineDefId == 2, "upper wall ray preserves topology linedef id");
    Check(upperHit.ref.topologySideDefId == 2, "upper wall ray preserves topology sidedef id");
}

void TestGeneratedGeometryPickingResolvesMiddleFacingRefs()
{
    game::SectorTopologyMap bothAssigned = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::FindSectorTopologySideDef(bothAssigned, 2)->middle = Part("front-bars");
    game::FindSectorTopologySideDef(bothAssigned, 8)->middle = Part("back-bars");
    const game::SectorGeneratedGeometry bothGeometry =
            BuildGeometryOrFail(bothAssigned, "pick both-assigned middle geometry builds");

    const game::SectorGeneratedSurfaceHit bothFrontHit = game::PickSectorGeneratedGeometry(
            bothGeometry,
            MakeRay(Vector3{0.25f, 1.0f, 0.25f}, Vector3{1.0f, 0.0f, 0.0f}));
    Check(bothFrontHit.hit, "front ray hits both-assigned middle texture");
    Check(bothFrontHit.ref.kind == game::SectorGeneratedSurfaceKind::Middle,
          "front ray returns middle kind");
    Check(bothFrontHit.ref.topologySideDefId == 2,
          "front ray picks front middle sidedef");

    const game::SectorGeneratedSurfaceHit bothBackHit = game::PickSectorGeneratedGeometry(
            bothGeometry,
            MakeRay(Vector3{1.0f, 1.0f, 0.25f}, Vector3{-1.0f, 0.0f, 0.0f}));
    Check(bothBackHit.hit, "back ray hits both-assigned middle texture");
    Check(bothBackHit.ref.kind == game::SectorGeneratedSurfaceKind::Middle,
          "back ray returns middle kind");
    Check(bothBackHit.ref.topologySideDefId == 8,
          "back ray picks back middle sidedef");

    game::SectorTopologyMap frontAssigned = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::FindSectorTopologySideDef(frontAssigned, 2)->middle = Part("bars");
    const game::SectorGeneratedGeometry frontGeometry =
            BuildGeometryOrFail(frontAssigned, "pick front-assigned middle geometry builds");
    const game::SectorGeneratedSurfaceHit frontOwnerHit = game::PickSectorGeneratedGeometry(
            frontGeometry,
            MakeRay(Vector3{0.25f, 1.0f, 0.25f}, Vector3{1.0f, 0.0f, 0.0f}));
    const game::SectorGeneratedSurfaceHit frontOwnerBackHit = game::PickSectorGeneratedGeometry(
            frontGeometry,
            MakeRay(Vector3{1.0f, 1.0f, 0.25f}, Vector3{-1.0f, 0.0f, 0.0f}));
    Check(frontOwnerHit.hit && frontOwnerHit.ref.topologySideDefId == 2,
          "front-side ray picks front owner for single-assigned middle texture");
    Check(frontOwnerBackHit.hit && frontOwnerBackHit.ref.topologySideDefId == 2,
          "back-side ray picks front owner for single-assigned middle texture");

    game::SectorTopologyMap backAssigned = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::FindSectorTopologySideDef(backAssigned, 8)->middle = Part("bars");
    const game::SectorGeneratedGeometry backGeometry =
            BuildGeometryOrFail(backAssigned, "pick back-assigned middle geometry builds");
    const game::SectorGeneratedSurfaceHit backOwnerFrontHit = game::PickSectorGeneratedGeometry(
            backGeometry,
            MakeRay(Vector3{0.25f, 1.0f, 0.25f}, Vector3{1.0f, 0.0f, 0.0f}));
    const game::SectorGeneratedSurfaceHit backOwnerHit = game::PickSectorGeneratedGeometry(
            backGeometry,
            MakeRay(Vector3{1.0f, 1.0f, 0.25f}, Vector3{-1.0f, 0.0f, 0.0f}));
    Check(backOwnerFrontHit.hit && backOwnerFrontHit.ref.topologySideDefId == 8,
          "front-side ray picks back owner for back-assigned middle texture");
    Check(backOwnerHit.hit && backOwnerHit.ref.topologySideDefId == 8,
          "back-side ray picks back owner for back-assigned middle texture");
}

void TestGeneratedSurfaceVisibilitySelection()
{
    const game::SectorGeneratedGeometry geometry =
            BuildGeometryOrFail(MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f),
                                "visible surface filter geometry builds");

    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};

    int included = 0;
    int hiddenSectorIncluded = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (!game::ShouldIncludeSectorGeneratedSurfaceForVisibility(surface, visible)) {
            continue;
        }
        ++included;
        if (surface.ref.topologySectorId == 20) {
            ++hiddenSectorIncluded;
        }
    }
    Check(included > 0, "visible-sector surface filter includes visible sector surfaces");
    Check(hiddenSectorIncluded == 0, "visible-sector surface filter excludes hidden sector surfaces");

    game::RuntimePortalVisibilityResult fallback;
    fallback.fallbackDrawAll = true;
    int fallbackIncluded = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (game::ShouldIncludeSectorGeneratedSurfaceForVisibility(surface, fallback)) {
            ++fallbackIncluded;
        }
    }
    Check(fallbackIncluded == static_cast<int>(geometry.surfaces.size()),
          "fallback draw-all surface filter includes all surfaces");
}

void TestGeneratedGeometryPickingUsesVisibilityFilter()
{
    const game::SectorGeneratedGeometry geometry =
            BuildGeometryOrFail(MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f),
                                "visibility-aware pick geometry builds");
    const Ray rightSectorFloorRay =
            MakeRay(Vector3{0.75f, -1.0f, 0.25f}, Vector3{0.0f, 1.0f, 0.0f});

    game::RuntimePortalVisibilityResult visibleLeft;
    visibleLeft.validStartSector = true;
    visibleLeft.visibleSectorIds = {10};
    const game::SectorGeneratedSurfaceHit hiddenHit =
            game::PickSectorGeneratedGeometry(geometry, rightSectorFloorRay, visibleLeft);
    Check(!hiddenHit.hit, "visibility-aware picking ignores hidden-sector surfaces");

    game::RuntimePortalVisibilityResult visibleRight;
    visibleRight.validStartSector = true;
    visibleRight.visibleSectorIds = {20};
    const game::SectorGeneratedSurfaceHit visibleHit =
            game::PickSectorGeneratedGeometry(geometry, rightSectorFloorRay, visibleRight);
    Check(visibleHit.hit, "visibility-aware picking still hits visible-sector surfaces");
    Check(visibleHit.ref.topologySectorId == 20,
          "visibility-aware picking preserves topology refs for visible picked surfaces");

    game::RuntimePortalVisibilityResult fallback;
    fallback.fallbackDrawAll = true;
    const game::SectorGeneratedSurfaceHit fallbackHit =
            game::PickSectorGeneratedGeometry(geometry, rightSectorFloorRay, fallback);
    Check(fallbackHit.hit && fallbackHit.ref.topologySectorId == 20,
          "visibility-aware picking falls back to all surfaces");
}

void TestGeneratedSurfaceHighlightVisibilitySelection()
{
    game::SectorGeneratedSurface visibleSurface = MakeBatchTestSurface("visible", "", {}, 1.0f, 0.0f);
    visibleSurface.ref.topologySectorId = 10;
    game::SectorGeneratedSurface hiddenSurface = MakeBatchTestSurface("hidden", "", {}, 1.0f, 2.0f);
    hiddenSurface.ref.topologySectorId = 20;

    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};
    Check(game::ShouldIncludeSectorGeneratedSurfaceForVisibility(visibleSurface, visible),
          "highlight helper includes selected visible sector surface");
    Check(!game::ShouldIncludeSectorGeneratedSurfaceForVisibility(hiddenSurface, visible),
          "highlight helper suppresses selected hidden sector surface");
}

void TestDrawRecordVisibilitySelection()
{
    const std::vector<game::SectorMeshBatch> records = {
            MakeDrawRecord(10, "a"),
            MakeDrawRecord(20, "b"),
            MakeDrawRecord(30, "c")};

    game::RuntimePortalVisibilityResult fallback;
    fallback.validStartSector = false;
    fallback.fallbackDrawAll = true;
    Check(game::CountSectorMeshDrawRecordsForVisibility(records, fallback) == 3,
          "draw selection returns all records for fallback draw-all");
    Check(game::ShouldDrawSectorMeshRecordForVisibility(records[1], fallback),
          "invalid start sector draws all records");

    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10, 30};
    Check(game::CountSectorMeshDrawRecordsForVisibility(records, visible) == 2,
          "draw selection returns only visible sector records");
    Check(game::ShouldDrawSectorMeshRecordForVisibility(records[0], visible),
          "visible sector record is selected");
    Check(!game::ShouldDrawSectorMeshRecordForVisibility(records[1], visible),
          "disconnected hidden sector record is not selected");
    Check(game::ShouldDrawSectorMeshRecordForVisibility(records[2], visible),
          "second visible sector record is selected");

    game::RuntimePortalVisibilityResult emptyVisible;
    emptyVisible.validStartSector = true;
    Check(game::CountSectorMeshDrawRecordsForVisibility(records, emptyVisible) == 0,
          "draw selection handles empty visible set safely");
}

void TestSectorReceiverBoundsFromDrawRecords()
{
    const game::SectorGeneratedGeometry geometry =
            BuildGeometryOrFail(MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f),
                                "receiver bounds geometry builds");
    const game::SectorMeshBatchDataResult drawRecords = game::BuildSectorMeshDrawRecordData(geometry);
    const std::vector<game::SectorReceiverBounds> receiverBounds =
            game::BuildSectorReceiverBounds(drawRecords);

    const game::SectorReceiverBounds* left = FindBounds(receiverBounds, 10);
    const game::SectorReceiverBounds* right = FindBounds(receiverBounds, 20);
    Check(left != nullptr, "receiver bounds include left sector");
    Check(right != nullptr, "receiver bounds include right sector");
    Check(left != nullptr && Near(left->min, Vector3{0.0f, 0.0f, 0.0f}),
          "left receiver bounds min comes from generated draw geometry");
    Check(left != nullptr && Near(left->max, Vector3{0.5f, 3.0f, 0.5f}),
          "left receiver bounds max comes from generated draw geometry");
    Check(right != nullptr && Near(right->min, Vector3{0.5f, 0.0f, 0.0f}),
          "right receiver bounds min comes from generated draw geometry");
    Check(right != nullptr && Near(right->max, Vector3{1.0f, 3.0f, 0.5f}),
          "right receiver bounds max comes from generated draw geometry");
}

void TestBloomDrawRecordVisibilitySelection()
{
    const std::vector<game::SectorMeshBatch> records = {
            MakeDrawRecord(10, "wall", "poster", true),
            MakeDrawRecord(20, "wall", "hidden-poster", true),
            MakeDrawRecord(30, "wall", "plain-decal", false),
            MakeDrawRecord(40, "wall", "", true)};

    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10, 30, 40};
    Check(game::ShouldDrawEmissiveBloomSectorMeshRecordForVisibility(records[0], visible),
          "bloom selection includes visible emissive decal record");
    Check(!game::ShouldDrawEmissiveBloomSectorMeshRecordForVisibility(records[1], visible),
          "bloom selection uses the same hidden-sector filtering");
    Check(!game::ShouldDrawEmissiveBloomSectorMeshRecordForVisibility(records[2], visible),
          "bloom selection skips non-emissive decal records");
    Check(!game::ShouldDrawEmissiveBloomSectorMeshRecordForVisibility(records[3], visible),
          "bloom selection skips records without decal textures");

    game::RuntimePortalVisibilityResult fallback;
    fallback.fallbackDrawAll = true;
    Check(game::ShouldDrawEmissiveBloomSectorMeshRecordForVisibility(records[1], fallback),
          "bloom selection falls back to draw-all visibility");
}

void TestDynamicPointLightVisibilityCandidateSelection()
{
    game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::SectorCollisionWorld lookupWorld;
    std::string error;
    Check(lookupWorld.BuildFromTopology(map, &error), "dynamic light sector lookup world builds");

    map.dynamicPointLights = {
            game::SectorTopologyDynamicPointLight{
                    1,
                    Vector3{2.0f, 1.0f, 2.0f},
                    WHITE,
                    1.0f,
                    8.0f,
                    true},
            game::SectorTopologyDynamicPointLight{
                    2,
                    Vector3{6.0f, 1.0f, 2.0f},
                    WHITE,
                    1.0f,
                    8.0f,
                    true},
            game::SectorTopologyDynamicPointLight{
                    3,
                    Vector3{2.0f, 1.0f, 2.0f},
                    WHITE,
                    0.0f,
                    8.0f,
                    true},
            game::SectorTopologyDynamicPointLight{
                    4,
                    Vector3{18.0f, 1.0f, 2.0f},
                    WHITE,
                    1.0f,
                    8.0f,
                    true},
            game::SectorTopologyDynamicPointLight{
                    5,
                    Vector3{2.0f, 1.0f, 2.0f},
                    WHITE,
                    1.0f,
                    8.0f,
                    false},
            game::SectorTopologyDynamicPointLight{
                    6,
                    Vector3{2.0f, 1.0f, 2.0f},
                    WHITE,
                    1.0f,
                    -1.0f,
                    true},
            game::SectorTopologyDynamicPointLight{
                    7,
                    Vector3{2.0f, 1.0f, 2.0f},
                    WHITE,
                    -1.0f,
                    8.0f,
                    true},
            game::SectorTopologyDynamicPointLight{
                    8,
                    Vector3{std::numeric_limits<float>::quiet_NaN(), 1.0f, 2.0f},
                    WHITE,
                    1.0f,
                    8.0f,
                    true}};

    std::vector<game::SectorPreviewDynamicPointLightSource> sources;
    game::BuildSectorPreviewDynamicPointLightSources(map, &lookupWorld, sources);
    Check(sources.size() == 3, "dynamic light source collection drops invalid lights");
    Check(sources[0].lightId == 1 && sources[0].ownerSectorId == 10,
          "dynamic light source collection assigns left-sector ownership");
    Check(sources[1].lightId == 2 && sources[1].ownerSectorId == 20,
          "dynamic light source collection assigns right-sector ownership");
    Check(sources[2].lightId == 4 && sources[2].ownerSectorId == 0,
          "dynamic light source collection leaves outside lights unassigned");

    game::RuntimePortalVisibilityResult visibleLeft;
    visibleLeft.validStartSector = true;
    visibleLeft.visibleSectorIds = {10};
    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{0.5f, 3.0f, 0.5f}),
            Bounds(20, Vector3{0.5f, 0.0f, 0.0f}, Vector3{1.0f, 3.0f, 0.5f})};
    std::vector<game::SectorPreviewDynamicPointLightSource> candidates;
    game::CollectSectorPreviewDynamicPointLightCandidates(sources, visibleLeft, receiverBounds, candidates);
    Check(candidates.size() == 2,
          "dynamic light candidates include owner-visible and visible-receiver-overlap lights");
    Check(HasCandidateLightId(candidates, 1), "dynamic light candidates include visible owner sector");
    Check(HasCandidateLightId(candidates, 2), "dynamic light candidates include hidden owner touching visible receivers");
    Check(!HasCandidateLightId(candidates, 4), "dynamic light candidates exclude irrelevant hidden outside light");
    Check(!candidates.empty() && Near(candidates[0].light.position, Vector3{0.25f, 0.125f, 0.25f}),
          "dynamic light candidates preserve world-space uniform data");

    game::RuntimePortalVisibilityResult fallback;
    fallback.fallbackDrawAll = true;
    game::CollectSectorPreviewDynamicPointLightCandidates(sources, fallback, receiverBounds, candidates);
    Check(candidates.size() == 3,
          "dynamic light candidates include all valid lights during fallback draw-all");
}

void TestDynamicPointLightReceiverBoundCandidateSelection()
{
    const std::vector<game::SectorPreviewDynamicPointLightSource> sources = {
            LightSource(1, 20, Vector3{1.0f, 0.5f, 1.0f}, 0.75f),
            LightSource(2, 20, Vector3{1.99f, 0.5f, 0.5f}, 0.0f),
            LightSource(3, 20, Vector3{3.0f, 0.5f, 0.5f}, 0.9f)};
    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f})};

    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};

    std::vector<game::SectorPreviewDynamicPointLightSource> candidates;
    game::CollectSectorPreviewDynamicPointLightCandidates(sources, visible, receiverBounds, candidates);
    Check(HasCandidateLightId(candidates, 1),
          "hidden-owner light overlapping visible receiver bounds is included");
    Check(!HasCandidateLightId(candidates, 3),
          "hidden-owner light outside visible receiver bounds is excluded");

    const std::vector<game::SectorPreviewDynamicPointLightSource> boundarySources = {
            LightSource(4, 20, Vector3{1.052f, 0.5f, 0.5f}, 0.005f)};
    game::CollectSectorPreviewDynamicPointLightCandidates(boundarySources, visible, receiverBounds, candidates);
    Check(HasCandidateLightId(candidates, 4),
          "dynamic light sphere overlap uses padded receiver bounds at boundaries");
}

void TestDynamicSpotLightRuntimeSourcePacking()
{
    game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::SectorCollisionWorld lookupWorld;
    std::string error;
    Check(lookupWorld.BuildFromTopology(map, &error), "dynamic spotlight sector lookup world builds");

    map.dynamicSpotLights = {
            game::SectorTopologyDynamicSpotLight{
                    30,
                    Vector3{2.0f, 8.0f, 2.0f},
                    Vector3{10.0f, 8.0f, 2.0f},
                    WHITE,
                    2.0f,
                    16.0f,
                    20.0f,
                    35.0f,
                    true,
                    false,
                    game::DynamicLightFlickerDefaultSpeed,
                    game::DynamicLightFlickerDefaultAmount,
                    true,
                    7,
                    0.01f,
                    0.75f},
            game::SectorTopologyDynamicSpotLight{
                    31,
                    Vector3{6.0f, 8.0f, 2.0f},
                    Vector3{6.0f, 4.0f, 2.0f},
                    RED,
                    1.0f,
                    8.0f,
                    15.0f,
                    40.0f,
                    true,
                    true,
                    2.0f,
                    0.5f},
            game::SectorTopologyDynamicSpotLight{
                    32,
                    Vector3{2.0f, 8.0f, 2.0f},
                    Vector3{2.0f, 8.0f, 2.0f},
                    WHITE,
                    1.0f,
                    -1.0f,
                    20.0f,
                    35.0f,
                    true},
            game::SectorTopologyDynamicSpotLight{
                    33,
                    Vector3{4.0f, 8.0f, 4.0f},
                    Vector3{4.0f, 8.0f, 4.0f},
                    BLUE,
                    1.5f,
                    12.0f,
                    80.0f,
                    20.0f,
                    true}};

    std::vector<game::SectorPreviewDynamicPointLightSource> sources;
    game::BuildSectorPreviewDynamicPointLightSources(map, &lookupWorld, sources);
    Check(sources.size() == 3, "dynamic spotlight source collection drops invalid lights");

    const game::SectorPreviewDynamicPointLightSource* spot = FindCandidateLightId(sources, 30);
    Check(spot != nullptr && spot->ownerSectorId == 10,
          "dynamic spotlight source collection assigns origin sector ownership");
    Check(spot != nullptr && spot->light.kind == game::SectorPreviewDynamicLightKind::Spot,
          "dynamic spotlight source packing marks spot kind");
    Check(spot != nullptr && Near(spot->light.position, Vector3{0.25f, 1.0f, 0.25f}),
          "dynamic spotlight source packing converts position to world units");
    Check(spot != nullptr && Near(spot->light.direction, Vector3{1.0f, 0.0f, 0.0f}),
          "dynamic spotlight source packing stores normalized world direction");
    Check(spot != nullptr && Near(spot->light.radius, 2.0f),
          "dynamic spotlight source packing converts range to world units");
    Check(spot != nullptr
                  && Near(spot->light.innerConeCos, std::cos(20.0f * 3.14159265358979323846f / 180.0f))
                  && Near(spot->light.outerConeCos, std::cos(35.0f * 3.14159265358979323846f / 180.0f)),
          "dynamic spotlight source packing stores cone cosines");
    Check(spot != nullptr
                  && spot->light.castsShadow
                  && spot->light.shadowPriority == 7
                  && Near(spot->light.shadowBias, 0.01f)
                  && Near(spot->light.shadowStrength, 0.75f),
          "dynamic spotlight source packing preserves shadow selection settings");

    const game::SectorPreviewDynamicPointLightSource* flickerSpot = FindCandidateLightId(sources, 31);
    Check(flickerSpot != nullptr
                  && flickerSpot->light.flicker
                  && Near(flickerSpot->light.flickerSpeed, 2.0f)
                  && Near(flickerSpot->light.flickerAmount, 0.5f),
          "dynamic spotlight source packing preserves flicker upload settings");

    const game::SectorPreviewDynamicPointLightSource* degenerateSpot = FindCandidateLightId(sources, 33);
    Check(degenerateSpot != nullptr && Near(degenerateSpot->light.direction, Vector3{0.0f, -1.0f, 0.0f}),
          "dynamic spotlight source packing uses a safe fallback direction for coincident target");
    Check(degenerateSpot != nullptr
                  && Near(degenerateSpot->light.innerConeCos, std::cos(80.0f * 3.14159265358979323846f / 180.0f))
                  && Near(degenerateSpot->light.outerConeCos, degenerateSpot->light.innerConeCos),
          "dynamic spotlight source packing clamps outer cone to at least inner cone");
}

void TestDynamicSpotLightCandidateSelection()
{
    const std::vector<game::SectorPreviewDynamicPointLightSource> sources = {
            LightSource(1, 10, Vector3{0.5f, 0.5f, 0.5f}, 0.5f),
            SpotLightSource(20, 20, Vector3{1.0f, 0.5f, 0.5f}, 0.75f),
            SpotLightSource(21, 20, Vector3{3.0f, 0.5f, 0.5f}, 0.75f)};
    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f})};

    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};

    std::vector<game::SectorPreviewDynamicPointLightSource> candidates;
    game::CollectSectorPreviewDynamicPointLightCandidates(sources, visible, receiverBounds, candidates);
    Check(HasCandidateLightId(candidates, 1), "dynamic light candidates keep point lights");
    Check(HasCandidateLightId(candidates, 20),
          "dynamic spotlight candidates include range sphere overlap with visible receivers");
    Check(!HasCandidateLightId(candidates, 21),
          "dynamic spotlight candidates exclude hidden irrelevant lights in normal visibility");

    game::RuntimePortalVisibilityResult fallback;
    fallback.validStartSector = true;
    fallback.fallbackDrawAll = true;
    fallback.visibleSectorIds = {10};
    game::CollectSectorPreviewDynamicPointLightCandidates(sources, fallback, receiverBounds, candidates);
    Check(candidates.size() == 3,
          "fallback draw-all dynamic light candidates include points and spotlights");
}

void TestDynamicPointLightRankingAndPacking()
{
    std::vector<game::SectorPreviewDynamicPointLightSource> candidates = {
            LightSource(1, 10, Vector3{20.0f, 0.0f, 0.0f}, 10.0f, 100.0f),
            LightSource(2, 20, Vector3{4.0f, 0.5f, 0.5f}, 10.0f, 1.0f),
            LightSource(3, 20, Vector3{0.5f, 0.5f, 0.5f}, 10.0f, 1.0f),
            LightSource(4, 20, Vector3{0.5f, 0.5f, 0.5f}, 10.0f, 4.0f)};
    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f})};
    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};

    std::vector<game::SectorPreviewDynamicPointLightUniform> selected;
    game::SelectRankedSectorPreviewDynamicPointLights(candidates, visible, receiverBounds, 2, selected);
    Check(selected.size() == 2, "dynamic light ranking applies max-light cap");
    Check(!selected.empty() && Near(selected[0].position, Vector3{0.5f, 0.5f, 0.5f}),
          "dynamic light ranking lets receiver-relevant stronger lights win");
    Check(selected.size() > 1 && Near(selected[1].position, Vector3{0.5f, 0.5f, 0.5f}),
          "dynamic light ranking prioritizes receiver overlap over owner-sector visibility alone");

    game::SelectRankedSectorPreviewDynamicPointLights(candidates, visible, receiverBounds, 8, selected);
    Check(selected.size() == 4,
          "dynamic light ranking does not drop candidates solely because they do not affect receiver bounds");

    std::vector<int> selectedIds = {99};
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            receiverBounds,
            0,
            selected,
            &selectedIds);
    Check(selected.empty() && selectedIds.empty(),
          "dynamic light packing zero-count clears selected lights and IDs");

    std::vector<game::SectorPreviewDynamicPointLightSource> manyCandidates;
    for (int i = 0; i < 12; ++i) {
        manyCandidates.push_back(LightSource(
                100 + i,
                10,
                Vector3{0.5f, 0.5f, 0.5f},
                10.0f,
                static_cast<float>(12 - i)));
    }
    game::SelectRankedSectorPreviewDynamicPointLights(
            manyCandidates,
            visible,
            receiverBounds,
            8,
            selected,
            &selectedIds);
    Check(selected.size() == 8 && selectedIds.size() == 8,
          "dynamic light packing applies shader max-count cap");
    Check(selectedIds.front() == 100 && selectedIds.back() == 107,
          "dynamic light packing keeps deterministic strongest-light order at max count");

    std::vector<game::SectorPreviewDynamicPointLightSource> tiedCandidates = {
            LightSource(8, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f),
            LightSource(7, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f)};
    game::SelectRankedSectorPreviewDynamicPointLights(
            tiedCandidates,
            visible,
            receiverBounds,
            1,
            selected,
            &selectedIds);
    Check(selected.size() == 1 && selectedIds.size() == 1 && selectedIds[0] == 7,
          "dynamic light ranking uses deterministic light-id tie breaking");
}

void TestDynamicSpotLightRankingAndPacking()
{
    std::vector<game::SectorPreviewDynamicPointLightSource> candidates;
    for (int i = 0; i < 7; ++i) {
        candidates.push_back(LightSource(
                100 + i,
                10,
                Vector3{0.5f, 0.5f, 0.5f},
                10.0f,
                static_cast<float>(7 - i)));
    }
    candidates.push_back(SpotLightSource(200, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f, 20.0f));
    candidates.push_back(SpotLightSource(201, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f, 19.0f));

    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f})};
    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};

    std::vector<game::SectorPreviewDynamicPointLightUniform> selected;
    std::vector<int> selectedIds;
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            receiverBounds,
            8,
            selected,
            &selectedIds);
    Check(selected.size() == 8 && selectedIds.size() == 8,
          "dynamic points and spotlights share the total runtime light cap");
    Check(selectedIds.size() >= 2 && selectedIds[0] == 200 && selectedIds[1] == 201,
          "dynamic spotlight ranking uses the same receiver score as points");
    Check(selected.size() >= 2
                  && selected[0].kind == game::SectorPreviewDynamicLightKind::Spot
                  && selected[1].kind == game::SectorPreviewDynamicLightKind::Spot,
          "dynamic light packing preserves selected spotlight kind");

    std::vector<game::SectorPreviewDynamicPointLightSource> tiedCandidates = {
            SpotLightSource(8, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f),
            LightSource(7, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f)};
    game::SelectRankedSectorPreviewDynamicPointLights(
            tiedCandidates,
            visible,
            receiverBounds,
            1,
            selected,
            &selectedIds);
    Check(selected.size() == 1 && selectedIds.size() == 1 && selectedIds[0] == 7,
          "dynamic point and spotlight tie ranking remains deterministic by light id");
}

void TestDynamicSpotLightShadowCasterSelection()
{
    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f})};
    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};

    std::vector<game::SectorPreviewDynamicPointLightSource> candidates = {
            ShadowSpotLightSource(10, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 4.0f),
            ShadowSpotLightSource(11, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 3.0f),
            ShadowSpotLightSource(12, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 2.0f)};
    std::vector<game::SectorPreviewDynamicPointLightUniform> selected;
    std::vector<int> selectedIds;
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            receiverBounds,
            2,
            selected,
            &selectedIds);

    std::vector<game::SectorPreviewDynamicSpotLightShadowCaster> shadowCasters;
    game::SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selected,
            visible,
            receiverBounds,
            game::MaxDynamicSpotLightShadowCasters,
            shadowCasters);
    Check(shadowCasters.size() == 2
                  && HasShadowCasterLightId(shadowCasters, 10)
                  && HasShadowCasterLightId(shadowCasters, 11)
                  && !HasShadowCasterLightId(shadowCasters, 12),
          "dynamic spotlight shadow selection only assigns slots to already-selected dynamic lights");
    Check(shadowCasters.size() == game::MaxDynamicSpotLightShadowCasters,
          "dynamic spotlight shadow selection applies the default shadow caster cap");
    Check(shadowCasters.size() >= 2
                  && shadowCasters[0].dynamicLightIndex == 0
                  && shadowCasters[0].shadowSlot == 0
                  && shadowCasters[1].dynamicLightIndex == 1
                  && shadowCasters[1].shadowSlot == 1,
          "dynamic spotlight shadow selection records dynamic light index and sequential shadow slot");

    selected = {
            LightSource(20, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 10.0f).light,
            SpotLightSource(21, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 9.0f).light,
            ShadowSpotLightSource(22, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 1.0f).light};
    selected[0].castsShadow = true;
    game::SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selected,
            visible,
            receiverBounds,
            game::MaxDynamicSpotLightShadowCasters,
            shadowCasters);
    Check(shadowCasters.size() == 1 && shadowCasters[0].lightId == 22,
          "dynamic spotlight shadow selection ignores dynamic points and castsShadow=false spotlights");

    selected = {
            ShadowSpotLightSource(30, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 10.0f, 0).light,
            ShadowSpotLightSource(31, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 1.0f, 1).light,
            ShadowSpotLightSource(32, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 9.0f, 0).light,
            ShadowSpotLightSource(33, 10, Vector3{0.5f, 0.5f, 0.5f}, 8.0f, 9.0f, 0).light};
    game::SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selected,
            visible,
            receiverBounds,
            4,
            shadowCasters);
    Check(shadowCasters.size() == 4
                  && shadowCasters[0].lightId == 31
                  && shadowCasters[1].lightId == 30
                  && shadowCasters[2].lightId == 32
                  && shadowCasters[3].lightId == 33,
          "dynamic spotlight shadow selection orders by priority then score then light id");

    game::SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selected,
            visible,
            receiverBounds,
            0,
            shadowCasters);
    Check(shadowCasters.empty(),
          "dynamic spotlight shadow selection clears output for zero shadow caster budget");
}

void TestDynamicSpotLightShadowMatrices()
{
    std::vector<game::SectorPreviewDynamicPointLightUniform> selected = {
            ShadowSpotLightSource(40, 10, Vector3{0.5f, 1.5f, 0.5f}, 8.0f, 4.0f).light,
            ShadowSpotLightSource(41, 10, Vector3{0.5f, 1.5f, 0.5f}, 8.0f, 3.0f).light,
            ShadowSpotLightSource(42, 10, Vector3{0.5f, 1.5f, 0.5f}, 8.0f, 2.0f).light};
    selected[0].lightId = 40;
    selected[0].direction = Vector3{1.0f, 0.0f, 0.0f};
    selected[1].lightId = 41;
    selected[1].direction = Vector3{0.0f, -1.0f, 0.0f};
    selected[2].lightId = 42;
    selected[2].direction = Vector3{0.0f, 0.0f, 0.0f};

    game::SectorPreviewDynamicSpotLightShadowMatrix matrix;
    Check(game::MakeSectorPreviewDynamicSpotLightShadowMatrix(selected[0], 0, 0, matrix),
          "dynamic spotlight shadow matrix builds for a valid spotlight");
    Check(matrix.lightId == 40
                  && matrix.dynamicLightIndex == 0
                  && matrix.shadowSlot == 0
                  && FiniteMatrix(matrix.view)
                  && FiniteMatrix(matrix.projection)
                  && FiniteMatrix(matrix.lightViewProjection),
          "dynamic spotlight shadow matrix output is finite and preserves slot metadata");

    Check(game::MakeSectorPreviewDynamicSpotLightShadowMatrix(selected[1], 1, 1, matrix)
                  && FiniteMatrix(matrix.lightViewProjection),
          "dynamic spotlight shadow matrix handles near-vertical spotlight directions");
    Check(game::MakeSectorPreviewDynamicSpotLightShadowMatrix(selected[2], 2, 0, matrix)
                  && FiniteMatrix(matrix.lightViewProjection),
          "dynamic spotlight shadow matrix uses a finite fallback for degenerate directions");

    selected[0].kind = game::SectorPreviewDynamicLightKind::Point;
    Check(!game::MakeSectorPreviewDynamicSpotLightShadowMatrix(selected[0], 0, 0, matrix),
          "dynamic spotlight shadow matrix rejects point lights");
    selected[0].kind = game::SectorPreviewDynamicLightKind::Spot;
    selected[0].radius = 0.01f;
    Check(!game::MakeSectorPreviewDynamicSpotLightShadowMatrix(selected[0], 0, 0, matrix),
          "dynamic spotlight shadow matrix rejects invalid shadow ranges");
    selected[0].radius = 8.0f;

    std::vector<game::SectorPreviewDynamicSpotLightShadowCaster> shadowCasters = {
            game::SectorPreviewDynamicSpotLightShadowCaster{40, 0, 0, 0, 1.0f, 0.002f, 1.0f},
            game::SectorPreviewDynamicSpotLightShadowCaster{41, 1, 1, 0, 1.0f, 0.002f, 1.0f},
            game::SectorPreviewDynamicSpotLightShadowCaster{99, 99, 2, 0, 1.0f, 0.002f, 1.0f}};
    std::vector<game::SectorPreviewDynamicSpotLightShadowMatrix> matrices;
    game::BuildSectorPreviewDynamicSpotLightShadowMatrices(selected, shadowCasters, matrices);
    Check(matrices.size() == game::MaxDynamicSpotLightShadowCasters
                  && matrices[0].shadowSlot == 0
                  && matrices[1].shadowSlot == 1,
          "dynamic spotlight shadow matrix build respects selected caster slots and skips invalid indices");
    Check(game::DynamicSpotLightShadowMapResolution == 1024,
          "dynamic spotlight shadow map default resolution is 1024");
}

void TestDynamicPointLightFlickerHelper()
{
    const float a = game::EvaluateDynamicLightFlickerMultiplier(42, 1.25f, 1.0f, 0.5f);
    const float b = game::EvaluateDynamicLightFlickerMultiplier(42, 1.25f, 1.0f, 0.5f);
    Check(Near(a, b), "dynamic light flicker is deterministic for identical inputs");
    Check(std::isfinite(a) && a >= 0.0f && a <= 1.0f,
          "dynamic light flicker multiplier is finite and clamped");
    Check(Near(game::EvaluateDynamicLightFlickerMultiplier(42, 1.25f, 3.0f, 0.0f), 1.0f),
          "dynamic light flicker amount zero returns full brightness");
    Check(Near(game::EvaluateDynamicLightFlickerMultiplier(42, std::numeric_limits<float>::infinity(), 1.0f, 0.5f), 1.0f),
          "dynamic light flicker non-finite time returns full brightness");

    bool foundTimeVariation = false;
    float previous = game::EvaluateDynamicLightFlickerMultiplier(42, 0.0f, 1.0f, 0.8f);
    for (int i = 1; i <= 240; ++i) {
        const float current = game::EvaluateDynamicLightFlickerMultiplier(
                42,
                static_cast<float>(i) / 120.0f,
                1.0f,
                0.8f);
        if (std::fabs(current - previous) > 0.0001f) {
            foundTimeVariation = true;
            break;
        }
        previous = current;
    }
    Check(foundTimeVariation, "dynamic light flicker can vary across absolute time samples");

    bool foundLightVariation = false;
    const float firstLight = game::EvaluateDynamicLightFlickerMultiplier(10, 0.375f, 1.0f, 1.0f);
    for (int lightId = 11; lightId < 40; ++lightId) {
        const float current = game::EvaluateDynamicLightFlickerMultiplier(lightId, 0.375f, 1.0f, 1.0f);
        if (std::fabs(current - firstLight) > 0.0001f) {
            foundLightVariation = true;
            break;
        }
    }
    Check(foundLightVariation, "dynamic light flicker varies across light IDs without requiring an exact pattern");

    const float sameTimestampFrom60Hz = game::EvaluateDynamicLightFlickerMultiplier(7, 0.5f, 1.25f, 0.65f);
    const float sameTimestampFrom144Hz = game::EvaluateDynamicLightFlickerMultiplier(7, 0.5f, 1.25f, 0.65f);
    Check(Near(sameTimestampFrom60Hz, sameTimestampFrom144Hz),
          "dynamic light flicker depends on absolute timestamp rather than sample cadence");

    int slowChanges = 0;
    int fastChanges = 0;
    float slowPrevious = game::EvaluateDynamicLightFlickerMultiplier(30, 0.0f, 0.1f, 0.8f);
    float fastPrevious = game::EvaluateDynamicLightFlickerMultiplier(30, 0.0f, 4.0f, 0.8f);
    for (int i = 1; i <= 500; ++i) {
        const float t = static_cast<float>(i) / 250.0f;
        const float slowCurrent = game::EvaluateDynamicLightFlickerMultiplier(30, t, 0.1f, 0.8f);
        const float fastCurrent = game::EvaluateDynamicLightFlickerMultiplier(30, t, 4.0f, 0.8f);
        if (std::fabs(slowCurrent - slowPrevious) > 0.0001f) {
            ++slowChanges;
        }
        if (std::fabs(fastCurrent - fastPrevious) > 0.0001f) {
            ++fastChanges;
        }
        slowPrevious = slowCurrent;
        fastPrevious = fastCurrent;
    }
    Check(fastChanges > slowChanges, "higher dynamic light flicker speed changes sampled multipliers more often");

    const int slowSegmentCrossings = static_cast<int>(
            std::floor(2.0f * game::DynamicLightFlickerBaseRateHz * game::ClampDynamicLightFlickerSpeed(0.1f)));
    const int fastSegmentCrossings = static_cast<int>(
            std::floor(2.0f * game::DynamicLightFlickerBaseRateHz * game::ClampDynamicLightFlickerSpeed(4.0f)));
    Check(fastSegmentCrossings > slowSegmentCrossings,
          "higher dynamic light flicker speed crosses more time segments over the same range");
}

void TestDynamicPointLightFlickerEffectiveIntensity()
{
    game::SectorPreviewDynamicPointLightUniform light;
    light.lightId = 12;
    light.intensity = 2.25f;
    light.flicker = false;
    light.flickerSpeed = 4.0f;
    light.flickerAmount = 1.0f;
    Check(Near(game::DynamicLightEffectiveUploadIntensity(light, 0.75f), 2.25f),
          "disabled dynamic light flicker leaves upload intensity unchanged");

    light.flicker = true;
    light.flickerAmount = 0.0f;
    Check(Near(game::DynamicLightEffectiveUploadIntensity(light, 0.75f), 2.25f),
          "zero dynamic light flicker amount leaves upload intensity unchanged");

    light.flickerAmount = 1.0f;
    const float effective = game::DynamicLightEffectiveUploadIntensity(light, 0.75f);
    Check(std::isfinite(effective) && effective >= 0.0f && effective <= light.intensity,
          "enabled dynamic light flicker upload intensity remains within base intensity bounds");
}

void TestDynamicPointLightFlickerDoesNotAffectSelection()
{
    std::vector<game::SectorPreviewDynamicPointLightSource> candidates = {
            LightSource(1, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f, 5.0f),
            LightSource(2, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f, 4.0f)};
    candidates[0].light.flicker = true;
    candidates[0].light.flickerSpeed = 10.0f;
    candidates[0].light.flickerAmount = 1.0f;

    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f})};
    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};

    std::vector<game::SectorPreviewDynamicPointLightUniform> selected;
    std::vector<int> selectedIds;
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            receiverBounds,
            1,
            selected,
            &selectedIds);
    Check(selectedIds.size() == 1 && selectedIds[0] == 1 && selected.size() == 1 && Near(selected[0].intensity, 5.0f),
          "dynamic light selection uses base intensity even when flicker is enabled");
}

void TestDynamicSpotLightFlickerDoesNotAffectSelection()
{
    std::vector<game::SectorPreviewDynamicPointLightSource> candidates = {
            SpotLightSource(30, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f, 5.0f),
            LightSource(31, 10, Vector3{0.5f, 0.5f, 0.5f}, 10.0f, 4.0f)};
    candidates[0].light.flicker = true;
    candidates[0].light.flickerSpeed = 10.0f;
    candidates[0].light.flickerAmount = 1.0f;

    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f})};
    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};

    std::vector<game::SectorPreviewDynamicPointLightUniform> selected;
    std::vector<int> selectedIds;
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            receiverBounds,
            1,
            selected,
            &selectedIds);
    Check(selectedIds.size() == 1
                  && selectedIds[0] == 30
                  && selected.size() == 1
                  && selected[0].kind == game::SectorPreviewDynamicLightKind::Spot
                  && Near(selected[0].intensity, 5.0f),
          "dynamic spotlight selection uses base intensity even when flicker is enabled");
}

void TestDynamicPointLightFallbackUsesAllReceiverBounds()
{
    std::vector<game::SectorPreviewDynamicPointLightSource> candidates = {
            LightSource(1, 10, Vector3{0.5f, 0.5f, 0.5f}, 5.0f, 1.0f),
            LightSource(2, 20, Vector3{20.5f, 0.5f, 0.5f}, 5.0f, 4.0f)};
    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f}),
            Bounds(20, Vector3{20.0f, 0.0f, 0.0f}, Vector3{21.0f, 1.0f, 1.0f})};

    game::RuntimePortalVisibilityResult fallback;
    fallback.validStartSector = true;
    fallback.fallbackDrawAll = true;
    fallback.visibleSectorIds = {10};

    std::vector<game::SectorPreviewDynamicPointLightSource> collected;
    game::CollectSectorPreviewDynamicPointLightCandidates(candidates, fallback, receiverBounds, collected);
    Check(collected.size() == 2,
          "fallback draw-all candidate collection includes all valid lights");

    std::vector<game::SectorPreviewDynamicPointLightUniform> selected;
    game::SelectRankedSectorPreviewDynamicPointLights(collected, fallback, receiverBounds, 1, selected);
    Check(selected.size() == 1 && Near(selected[0].position, Vector3{20.5f, 0.5f, 0.5f}),
          "fallback draw-all ranking treats all receiver bounds as visible");

    const std::vector<game::SectorReceiverBounds> noBounds;
    game::CollectSectorPreviewDynamicPointLightCandidates(candidates, fallback, noBounds, collected);
    game::SelectRankedSectorPreviewDynamicPointLights(collected, fallback, noBounds, 2, selected);
    Check(selected.size() == 2,
          "fallback draw-all without receiver bounds still selects valid lights conservatively");
}

void TestDynamicPointLightSelectionHysteresis()
{
    game::RuntimePortalVisibilityResult visible;
    visible.validStartSector = true;
    visible.visibleSectorIds = {10};
    const std::vector<game::SectorReceiverBounds> noReceiverBounds;

    std::vector<game::SectorPreviewDynamicPointLightSource> candidates = {
            LightSource(1, 10, Vector3{0.0f, 0.0f, 0.0f}, 5.0f, 1.0f),
            LightSource(2, 10, Vector3{1.0f, 0.0f, 0.0f}, 5.0f, 1.1f)};
    std::vector<int> selectedIds = {1};
    std::vector<game::SectorPreviewDynamicPointLightUniform> selected;
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            noReceiverBounds,
            1,
            selected,
            &selectedIds,
            &selectedIds);
    Check(selectedIds.size() == 1 && selectedIds[0] == 1,
          "dynamic light hysteresis retains old selected light against small score difference");

    candidates = {
            LightSource(1, 10, Vector3{0.0f, 0.0f, 0.0f}, 5.0f, 1.0f),
            LightSource(2, 10, Vector3{1.0f, 0.0f, 0.0f}, 5.0f, 1.25f)};
    selectedIds = {1};
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            noReceiverBounds,
            1,
            selected,
            &selectedIds,
            &selectedIds);
    Check(selectedIds.size() == 1 && selectedIds[0] == 2,
          "dynamic light hysteresis replaces old selected light with clearly better candidate");

    candidates = {
            LightSource(2, 10, Vector3{1.0f, 0.0f, 0.0f}, 5.0f, 1.0f)};
    selectedIds = {1};
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            noReceiverBounds,
            1,
            selected,
            &selectedIds,
            &selectedIds);
    Check(selectedIds.size() == 1 && selectedIds[0] == 2,
          "dynamic light hysteresis removes deleted or disabled old selected light immediately");

    candidates = {
            LightSource(1, 10, Vector3{0.0f, 0.0f, 0.0f}, 5.0f, 1.0f),
            LightSource(2, 10, Vector3{1.0f, 0.0f, 0.0f}, 5.0f, 1.0f),
            LightSource(3, 10, Vector3{2.0f, 0.0f, 0.0f}, 5.0f, 1.0f)};
    selectedIds = {1, 2, 3};
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            noReceiverBounds,
            2,
            selected,
            &selectedIds,
            &selectedIds);
    Check(selectedIds.size() == 2,
          "dynamic light hysteresis keeps selected count capped");

    const std::vector<game::SectorReceiverBounds> receiverBounds = {
            Bounds(10, Vector3{0.0f, 0.0f, 0.0f}, Vector3{1.0f, 1.0f, 1.0f})};
    candidates = {
            LightSource(1, 10, Vector3{20.0f, 0.0f, 0.0f}, 5.0f, 1.0f),
            LightSource(2, 10, Vector3{0.5f, 0.5f, 0.5f}, 5.0f, 1.0f)};
    selectedIds = {1};
    game::SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visible,
            receiverBounds,
            1,
            selected,
            &selectedIds,
            &selectedIds);
    Check(selectedIds.size() == 1 && selectedIds[0] == 2 && !HasSelectedLightId(selectedIds, 1),
          "dynamic light hysteresis does not retain outside-radius old selected light");
}

} // namespace

int main()
{
    TestSquareTopologyMeshBatchData();
    TestOneSectorMeshDrawRecords();
    TestTwoSectorsWithSameMaterialKeepSeparateDrawRecords();
    TestDecalMeshBatchData();
    TestSectorDrawRecordMaterialGrouping();
    TestSectorDrawRecordLightmapUvsUseOriginalSurfaceIndex();
    TestMiddleTextureBatchState();
    TestSectorDrawRecordMiddleTextureAlphaTest();
    TestSectorDrawRecordEmissiveDecalMetadata();
    TestLightmapParticipationSplitsBatchKey();
    TestHoleTopologyMeshBatchData();
    TestEqualHeightPortal();
    TestDifferentHeightPortal();
    TestTriangleWindingAgainstNormals();
    TestInvalidTopologyMeshBuilder();
    TestGeneratedGeometryPickingKeepsTopologyRefs();
    TestGeneratedGeometryPickingResolvesMiddleFacingRefs();
    TestGeneratedSurfaceVisibilitySelection();
    TestGeneratedGeometryPickingUsesVisibilityFilter();
    TestGeneratedSurfaceHighlightVisibilitySelection();
    TestDrawRecordVisibilitySelection();
    TestSectorReceiverBoundsFromDrawRecords();
    TestBloomDrawRecordVisibilitySelection();
    TestDynamicPointLightVisibilityCandidateSelection();
    TestDynamicPointLightReceiverBoundCandidateSelection();
    TestDynamicSpotLightRuntimeSourcePacking();
    TestDynamicSpotLightCandidateSelection();
    TestDynamicPointLightRankingAndPacking();
    TestDynamicSpotLightRankingAndPacking();
    TestDynamicSpotLightShadowCasterSelection();
    TestDynamicSpotLightShadowMatrices();
    TestDynamicPointLightFlickerHelper();
    TestDynamicPointLightFlickerEffectiveIntensity();
    TestDynamicPointLightFlickerDoesNotAffectSelection();
    TestDynamicSpotLightFlickerDoesNotAffectSelection();
    TestDynamicPointLightFallbackUsesAllReceiverBounds();
    TestDynamicPointLightSelectionHysteresis();
    if (failures == 0) {
        std::puts("Sector topology mesh builder tests passed");
    }
    return failures == 0 ? 0 : 1;
}
