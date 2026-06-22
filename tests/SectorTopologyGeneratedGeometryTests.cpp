#include "sector_demo/SectorGeneratedGeometry.h"

#include <algorithm>
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

Vector2 ApplyTestUv(Vector2 baseUv, Vector2 scale, Vector2 offset)
{
    return Vector2{baseUv.x * scale.x + offset.x, baseUv.y * scale.y + offset.y};
}

game::SectorTopologyWallPartSettings Part(const char* textureId)
{
    game::SectorTopologyWallPartSettings part;
    part.textureId = textureId;
    return part;
}

game::SectorTopologyDecalLayer Decal(
        const char* textureId,
        Vector2 scale,
        Vector2 offset,
        float opacity)
{
    game::SectorTopologyDecalLayer decal;
    decal.textureId = textureId;
    decal.uv.scale = scale;
    decal.uv.offset = offset;
    decal.opacity = opacity;
    return decal;
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

const game::SectorGeneratedSurface* FindSurface(
        const game::SectorGeneratedGeometry& geometry,
        game::SectorGeneratedSurfaceKind kind,
        int sectorId,
        int lineDefId = -2)
{
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.kind == kind
            && surface.ref.topologySectorId == sectorId
            && (lineDefId == -2 || surface.ref.topologyLineDefId == lineDefId)) {
            return &surface;
        }
    }
    return nullptr;
}

int CountWallKindsForLine(const game::SectorGeneratedGeometry& geometry, int lineDefId)
{
    int count = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.topologyLineDefId == lineDefId
            && (surface.ref.kind == game::SectorGeneratedSurfaceKind::Wall
                || surface.ref.kind == game::SectorGeneratedSurfaceKind::LowerWall
                || surface.ref.kind == game::SectorGeneratedSurfaceKind::UpperWall)) {
            ++count;
        }
    }
    return count;
}

double TriangleAreaXZ(const game::SectorGeneratedSurface& surface)
{
    double area = 0.0;
    for (size_t i = 0; i + 2 < surface.vertices.size(); i += 3) {
        const Vector3 a = surface.vertices[i].position;
        const Vector3 b = surface.vertices[i + 1].position;
        const Vector3 c = surface.vertices[i + 2].position;
        area += std::fabs(
                static_cast<double>(b.x - a.x) * static_cast<double>(c.z - a.z)
                - static_cast<double>(b.z - a.z) * static_cast<double>(c.x - a.x)) * 0.5;
    }
    return area;
}

void TestSquare()
{
    const game::SectorTopologyMap map = MakeSquare();
    game::SectorGeneratedGeometry geometry;
    std::string error = "stale";
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "square builds");
    Check(error.empty(), "successful build clears error");
    Check(geometry.surfaces.size() == 6, "square has two flats and four walls");
    const auto* floor = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Floor, 10);
    const auto* ceiling = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Ceiling, 10);
    Check(floor != nullptr && floor->vertices.size() == 6, "square has one triangulated floor surface");
    Check(ceiling != nullptr && ceiling->vertices.size() == 6, "square has one triangulated ceiling surface");
    Check(floor != nullptr && floor->normal.y == 1.0f, "floor normal points up");
    Check(ceiling != nullptr && ceiling->normal.y == -1.0f, "ceiling normal points down");
    for (int lineId = 1; lineId <= 4; ++lineId) {
        const auto* wall = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Wall, 10, lineId);
        Check(wall != nullptr, "square wall exists");
        Check(wall != nullptr && wall->ref.topologySideDefId == lineId,
              "wall ref contains sidedef ID");
        Check(wall != nullptr && wall->ref.topologySide == game::SectorTopologySideKind::Front,
              "wall ref contains side kind");
    }
    Check(floor != nullptr
          && game::FormatSectorGeneratedSurfaceLabel(floor->ref).find("sector=10") != std::string::npos,
          "topology surface label includes stable sector ID");
}

void TestHole()
{
    game::SectorTopologyMap map = MakeSquare();
    map.vertices.insert(map.vertices.end(), {
            {5, 16, 16}, {6, 16, 48}, {7, 48, 48}, {8, 48, 16}});
    for (int i = 5; i <= 8; ++i) {
        const int end = i == 8 ? 5 : i + 1;
        map.lineDefs.push_back({i, i, end, i, -1});
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10, "hole");
    }

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "sector with hole builds");
    const auto* floor = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Floor, 10);
    Check(floor != nullptr && floor->vertices.size() > 6, "hole floor is one multi-triangle surface");
    const double outerWorldSide = game::SectorCoordToWorldDistance(64);
    const double holeWorldSide = game::SectorCoordToWorldDistance(32);
    const double expectedArea = outerWorldSide * outerWorldSide - holeWorldSide * holeWorldSide;
    Check(floor != nullptr && std::fabs(TriangleAreaXZ(*floor) - expectedArea) < 0.000001,
          "floor triangulation excludes hole area");
}

void TestEqualHeightPortal()
{
    const game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "equal-height adjacent sectors build");
    Check(CountWallKindsForLine(geometry, 2) == 0, "equal-height portal emits no wall mesh");
    Check(CountWallKindsForLine(geometry, 1) == 1 && CountWallKindsForLine(geometry, 7) == 1,
          "outer one-sided walls remain");
}

void TestDifferentFloorPortal()
{
    const game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 8.0f, 24.0f);
    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "different-floor sectors build");
    const auto* lower = FindSurface(geometry, game::SectorGeneratedSurfaceKind::LowerWall, 10, 2);
    Check(lower != nullptr, "lower wall is emitted on lower sector side");
    Check(lower != nullptr && lower->textureId == "left-shared-lower",
          "lower wall uses current sidedef lower texture");
    Check(FindSurface(geometry, game::SectorGeneratedSurfaceKind::LowerWall, 20, 2) == nullptr,
          "higher sector side emits no duplicate lower wall");
}

void TestDifferentCeilingPortal()
{
    const game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 16.0f);
    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "different-ceiling sectors build");
    const auto* upper = FindSurface(geometry, game::SectorGeneratedSurfaceKind::UpperWall, 10, 2);
    Check(upper != nullptr, "upper wall is emitted on taller sector side");
    Check(upper != nullptr && upper->textureId == "left-shared-upper",
          "upper wall uses current sidedef upper texture");
    Check(FindSurface(geometry, game::SectorGeneratedSurfaceKind::UpperWall, 20, 2) == nullptr,
          "shorter sector side emits no duplicate upper wall");
}

void TestDiagonalLength()
{
    game::SectorTopologyMap map;
    map.vertices = {{1, 0, 0}, {2, 16, 16}, {3, 0, 32}};
    map.lineDefs = {{1, 1, 2, 1, -1}, {2, 2, 3, 2, -1}, {3, 3, 1, 3, -1}};
    for (int i = 1; i <= 3; ++i) {
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10, "diagonal");
    }
    map.sectors.push_back(Sector(10));

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "diagonal sector builds");
    const auto* wall = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Wall, 10, 1);
    const float expected = game::SectorCoordDistanceToWorldDistance(std::sqrt(512.0));
    Check(wall != nullptr && Near(wall->chartWidth, expected),
          "diagonal wall chart width preserves Euclidean length");
}

void TestHeightEditsAffectTopologyGeometry()
{
    game::SectorTopologyMap map = MakeSquare();
    map.sectors.front().floorZ = 4.0f;
    map.sectors.front().ceilingZ = 18.0f;

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error),
          "height-edited topology sector builds");

    const auto* floor = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Floor, 10);
    const auto* ceiling = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Ceiling, 10);
    const auto* wall = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Wall, 10, 1);
    const float expectedFloorY = game::SectorAuthoringToWorldDistance(4.0f);
    const float expectedCeilingY = game::SectorAuthoringToWorldDistance(18.0f);
    Check(floor != nullptr && !floor->vertices.empty(), "height-edited floor exists");
    Check(ceiling != nullptr && !ceiling->vertices.empty(), "height-edited ceiling exists");
    Check(wall != nullptr && !wall->vertices.empty(), "height-edited wall exists");

    if (floor != nullptr && !floor->vertices.empty()) {
        Check(Near(floor->vertices.front().position.y, expectedFloorY),
              "floor vertices use edited floorZ");
    }
    if (ceiling != nullptr && !ceiling->vertices.empty()) {
        Check(Near(ceiling->vertices.front().position.y, expectedCeilingY),
              "ceiling vertices use edited ceilingZ");
    }
    if (wall != nullptr && wall->vertices.size() >= 4) {
        float minY = wall->vertices.front().position.y;
        float maxY = wall->vertices.front().position.y;
        for (const game::SectorGeneratedVertex& vertex : wall->vertices) {
            minY = std::min(minY, vertex.position.y);
            maxY = std::max(maxY, vertex.position.y);
        }
        Check(Near(minY, expectedFloorY) && Near(maxY, expectedCeilingY),
              "wall span uses edited floorZ and ceilingZ");
    }
}

void TestDecalsPropagateWithoutChangingBaseUvs()
{
    game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 8.0f, 16.0f);
    game::SectorTopologyMap baseline = map;

    map.sectors[0].floorDecal = Decal("floor-mark", Vector2{2.0f, 3.0f}, Vector2{0.25f, 0.5f}, 0.6f);
    map.sectors[0].ceilingDecal = Decal("ceiling-grime", Vector2{4.0f, 5.0f}, Vector2{0.75f, 1.25f}, 0.7f);
    game::SectorTopologySideDef* outerSide = game::FindSectorTopologySideDef(map, 1);
    game::SectorTopologySideDef* sharedSide = game::FindSectorTopologySideDef(map, 2);
    Check(outerSide != nullptr && sharedSide != nullptr, "decal test topology sidedefs exist");
    if (outerSide != nullptr) {
        outerSide->wall.decal = Decal("wall-poster", Vector2{1.5f, 2.0f}, Vector2{0.1f, 0.2f}, 0.8f);
    }
    if (sharedSide != nullptr) {
        sharedSide->lower.decal = Decal("lower-sign", Vector2{2.5f, 3.5f}, Vector2{0.3f, 0.4f}, 0.55f);
        sharedSide->upper.decal = Decal("upper-text", Vector2{3.5f, 4.5f}, Vector2{0.5f, 0.6f}, 0.45f);
    }

    game::SectorGeneratedGeometry geometry;
    game::SectorGeneratedGeometry baselineGeometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "decal topology geometry builds");
    Check(game::BuildSectorGeneratedGeometry(baseline, baselineGeometry, &error), "baseline decal topology geometry builds");

    const auto* floor = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Floor, 10);
    const auto* baselineFloor = FindSurface(baselineGeometry, game::SectorGeneratedSurfaceKind::Floor, 10);
    const auto* ceiling = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Ceiling, 10);
    const auto* wall = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Wall, 10, 1);
    const auto* lower = FindSurface(geometry, game::SectorGeneratedSurfaceKind::LowerWall, 10, 2);
    const auto* upper = FindSurface(geometry, game::SectorGeneratedSurfaceKind::UpperWall, 10, 2);
    const auto* missing = FindSurface(geometry, game::SectorGeneratedSurfaceKind::Wall, 10, 3);

    Check(floor != nullptr && floor->decalTextureId == "floor-mark" && Near(floor->decalOpacity, 0.6f),
          "floor decal texture and opacity propagate");
    Check(ceiling != nullptr && ceiling->decalTextureId == "ceiling-grime" && Near(ceiling->decalOpacity, 0.7f),
          "ceiling decal texture and opacity propagate");
    Check(wall != nullptr && wall->decalTextureId == "wall-poster" && Near(wall->decalOpacity, 0.8f),
          "wall decal texture and opacity propagate");
    Check(lower != nullptr && lower->decalTextureId == "lower-sign" && Near(lower->decalOpacity, 0.55f),
          "lower wall decal texture and opacity propagate");
    Check(upper != nullptr && upper->decalTextureId == "upper-text" && Near(upper->decalOpacity, 0.45f),
          "upper wall decal texture and opacity propagate");
    Check(missing != nullptr && missing->decalTextureId.empty() && Near(missing->decalOpacity, 1.0f),
          "missing decal keeps empty texture and default opacity");

    if (floor != nullptr && baselineFloor != nullptr) {
        Check(floor->vertices.size() == baselineFloor->vertices.size(), "decal floor vertex count matches baseline");
        for (size_t i = 0; i < floor->vertices.size() && i < baselineFloor->vertices.size(); ++i) {
            Check(Near(floor->vertices[i].uv, baselineFloor->vertices[i].uv),
                  "floor base UVs are unchanged by decals");
        }
    }

    if (floor != nullptr && !floor->vertices.empty()) {
        const game::SectorGeneratedVertex& vertex = floor->vertices.front();
        const Vector2 rawUv{
                vertex.position.x / game::kSectorGeneratedTextureWorldSize,
                vertex.position.z / game::kSectorGeneratedTextureWorldSize};
        Check(Near(vertex.decalUv, ApplyTestUv(rawUv, Vector2{2.0f, 3.0f}, Vector2{0.25f, 0.5f})),
              "floor decal UV uses raw surface UV with decal settings");
    }
    if (wall != nullptr && !wall->vertices.empty()) {
        const game::SectorGeneratedVertex& vertex = wall->vertices.front();
        Check(Near(vertex.decalUv, ApplyTestUv(vertex.uv, Vector2{1.5f, 2.0f}, Vector2{0.1f, 0.2f})),
              "wall decal UV uses wall surface UV with decal settings");
    }
    if (lower != nullptr && !lower->vertices.empty()) {
        const game::SectorGeneratedVertex& vertex = lower->vertices.front();
        Check(Near(vertex.decalUv, ApplyTestUv(vertex.uv, Vector2{2.5f, 3.5f}, Vector2{0.3f, 0.4f})),
              "lower decal UV uses lower wall surface UV with decal settings");
    }
    if (upper != nullptr && !upper->vertices.empty()) {
        const game::SectorGeneratedVertex& vertex = upper->vertices.front();
        Check(Near(vertex.decalUv, ApplyTestUv(vertex.uv, Vector2{3.5f, 4.5f}, Vector2{0.5f, 0.6f})),
              "upper decal UV uses upper wall surface UV with decal settings");
    }
}

void TestInvalidAndEmptyMaps()
{
    game::SectorTopologyMap invalid = MakeSquare();
    invalid.lineDefs.front().startVertexId = 999;
    game::SectorGeneratedGeometry geometry;
    geometry.surfaces.push_back({});
    std::string error;
    Check(!game::BuildSectorGeneratedGeometry(invalid, geometry, &error), "invalid topology is rejected");
    Check(geometry.surfaces.empty(), "invalid topology clears output geometry");
    Check(error.find("validation") != std::string::npos && error.find("999") != std::string::npos,
          "invalid topology returns a readable error");

    game::SectorTopologyMap empty;
    error.clear();
    Check(!game::BuildSectorGeneratedGeometry(empty, geometry, &error), "empty topology map is rejected");
    Check(geometry.surfaces.empty() && error.find("no sectors") != std::string::npos,
          "empty topology map returns explicit error and no geometry");
}

} // namespace

int main()
{
    TestSquare();
    TestHole();
    TestEqualHeightPortal();
    TestDifferentFloorPortal();
    TestDifferentCeilingPortal();
    TestDiagonalLength();
    TestHeightEditsAffectTopologyGeometry();
    TestDecalsPropagateWithoutChangingBaseUvs();
    TestInvalidAndEmptyMaps();
    if (failures == 0) {
        std::puts("Sector topology generated geometry tests passed");
    }
    return failures == 0 ? 0 : 1;
}
