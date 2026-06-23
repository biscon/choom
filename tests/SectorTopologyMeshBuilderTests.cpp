#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorMeshBuilder.h"

#include <cmath>
#include <cstdio>
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

} // namespace

int main()
{
    TestSquareTopologyMeshBatchData();
    TestDecalMeshBatchData();
    TestHoleTopologyMeshBatchData();
    TestEqualHeightPortal();
    TestDifferentHeightPortal();
    TestTriangleWindingAgainstNormals();
    TestInvalidTopologyMeshBuilder();
    TestGeneratedGeometryPickingKeepsTopologyRefs();
    if (failures == 0) {
        std::puts("Sector topology mesh builder tests passed");
    }
    return failures == 0 ? 0 : 1;
}
