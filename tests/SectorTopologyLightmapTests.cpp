#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

game::SectorTextureDefinition Texture(const char* id)
{
    game::SectorTextureDefinition texture;
    texture.id = id;
    texture.path = std::string("assets/textures/") + id + ".png";
    return texture;
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
    sector.name = "sector-" + std::to_string(id);
    sector.floorZ = floorZ;
    sector.ceilingZ = ceilingZ;
    sector.floorTextureId = "floor";
    sector.ceilingTextureId = "ceiling";
    sector.ambientColor = Color{200, 180, 160, 255};
    sector.ambientIntensity = 0.5f;
    sector.defaultWall = Part("wall");
    sector.defaultLower = Part("lower");
    sector.defaultUpper = Part("upper");
    return sector;
}

void AddTextureDefaults(game::SectorTopologyMap& map)
{
    for (const char* id : {"floor", "ceiling", "wall", "lower", "upper", "alt"}) {
        map.texturesById.emplace(id, Texture(id));
    }
}

void AddSide(
        game::SectorTopologyMap& map,
        int sideId,
        int lineId,
        game::SectorTopologySideKind side,
        int sectorId,
        const char* wallTexture = "wall")
{
    game::SectorTopologySideDef sideDef;
    sideDef.id = sideId;
    sideDef.lineDefId = lineId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    sideDef.wall = Part(wallTexture);
    sideDef.lower = Part("lower");
    sideDef.upper = Part("upper");
    map.sideDefs.push_back(sideDef);
}

game::SectorTopologyMap MakeSquare()
{
    game::SectorTopologyMap map;
    AddTextureDefaults(map);
    map.vertices = {{1, 0, 0}, {2, 64, 0}, {3, 64, 64}, {4, 0, 64}};
    for (int i = 1; i <= 4; ++i) {
        const int end = i == 4 ? 1 : i + 1;
        map.lineDefs.push_back({i, i, end, i, -1});
        AddSide(map, i, i, game::SectorTopologySideKind::Front, 10);
    }
    map.sectors.push_back(Sector(10));
    map.staticLights.push_back(game::SectorTopologyStaticPointLight{
            1,
            Vector3{16.0f, game::SectorWorldToAuthoringDistance(3.0f), 16.0f},
            WHITE,
            1.0f,
            game::SectorWorldToAuthoringDistance(8.0f),
            game::SectorWorldToAuthoringDistance(0.2f)
    });
    return map;
}

game::SectorTopologyMap MakeAdjacent(float leftFloor, float leftCeiling, float rightFloor, float rightCeiling)
{
    game::SectorTopologyMap map;
    AddTextureDefaults(map);
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
    AddSide(map, 1, 1, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, game::SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, game::SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 2, game::SectorTopologySideKind::Back, 20);
    map.sectors.push_back(Sector(10, leftFloor, leftCeiling));
    map.sectors.push_back(Sector(20, rightFloor, rightCeiling));
    return map;
}

game::SectorTopologyMap MakePlatform()
{
    game::SectorTopologyMap map = MakeSquare();
    map.vertices.insert(map.vertices.end(), {
            {5, 16, 16}, {6, 48, 16}, {7, 48, 48}, {8, 16, 48}});
    for (int i = 5; i <= 8; ++i) {
        const int end = i == 8 ? 5 : i + 1;
        const int frontSideId = i;
        const int backSideId = i + 4;
        map.lineDefs.push_back({i, i, end, frontSideId, backSideId});
        AddSide(map, frontSideId, i, game::SectorTopologySideKind::Front, 20);
        AddSide(map, backSideId, i, game::SectorTopologySideKind::Back, 10);
    }
    map.sectors.push_back(Sector(20, 8.0f, 24.0f));
    return map;
}

int CountWallChartsForLine(const game::SectorTopologyMap& map, int lineDefId)
{
    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "geometry builds for chart counting");
    game::SectorLightmapLayout layout;
    Check(game::BuildSectorLightmapLayout(map, layout, error), "layout builds for chart counting");

    int count = 0;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        const game::SectorGeneratedSurface& surface = geometry.surfaces[static_cast<size_t>(chart.surfaceIndex)];
        if (surface.ref.topologyLineDefId == lineDefId
                && (surface.ref.kind == game::SectorGeneratedSurfaceKind::Wall
                    || surface.ref.kind == game::SectorGeneratedSurfaceKind::LowerWall
                    || surface.ref.kind == game::SectorGeneratedSurfaceKind::UpperWall)) {
            ++count;
        }
    }
    return count;
}

int CountValidCharts(const game::SectorLightmapLayout& layout)
{
    int count = 0;
    for (const game::SectorLightmapChart& chart : layout.charts) {
        if (chart.surfaceIndex >= 0) {
            ++count;
        }
    }
    return count;
}

int CountGeneratedSurfaces(const game::SectorGeneratedGeometry& geometry, game::SectorGeneratedSurfaceKind kind)
{
    int count = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.kind == kind) {
            ++count;
        }
    }
    return count;
}

int CountGeneratedTrianglesExceptMiddle(const game::SectorGeneratedGeometry& geometry)
{
    int count = 0;
    for (const game::SectorGeneratedSurface& surface : geometry.surfaces) {
        if (surface.ref.kind != game::SectorGeneratedSurfaceKind::Middle) {
            count += static_cast<int>(surface.vertices.size() / 3);
        }
    }
    return count;
}

void TestSourceHashChanges()
{
    const game::SectorTopologyMap base = MakeSquare();
    const std::string hash = game::ComputeSectorLightmapSourceHash(base);

    game::SectorTopologyMap movedVertex = base;
    movedVertex.vertices[0].x += 1;
    Check(game::ComputeSectorLightmapSourceHash(movedVertex) != hash, "hash changes when vertex coordinate changes");

    game::SectorTopologyMap changedSector = base;
    changedSector.sectors[0].floorZ += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedSector) != hash, "hash changes when sector floor changes");
    changedSector = base;
    changedSector.sectors[0].ceilingTextureId = "alt";
    Check(game::ComputeSectorLightmapSourceHash(changedSector) != hash, "hash changes when sector texture changes");

    game::SectorTopologyMap changedSideDef = base;
    changedSideDef.sideDefs[0].wall.textureId = "alt";
    Check(game::ComputeSectorLightmapSourceHash(changedSideDef) != hash, "hash changes when sidedef wall texture changes");

    game::SectorTopologyMap changedLight = base;
    changedLight.staticLights[0].position.x += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light position changes");
    changedLight = base;
    changedLight.staticLights[0].radius += 1.0f;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light radius changes");
    changedLight = base;
    changedLight.staticLights[0].intensity += 0.25f;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light intensity changes");
    changedLight = base;
    changedLight.staticLights[0].color.r = 64;
    Check(game::ComputeSectorLightmapSourceHash(changedLight) != hash, "hash changes when light color changes");
}

void TestSourceHashIgnoresMiddleTextureData()
{
    const game::SectorTopologyMap base = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    const std::string hash = game::ComputeSectorLightmapSourceHash(base);

    game::SectorTopologyMap withMiddle = base;
    withMiddle.texturesById.emplace("bars", Texture("bars"));
    game::FindSectorTopologySideDef(withMiddle, 2)->middle = Part("bars");
    Check(game::ComputeSectorLightmapSourceHash(withMiddle) == hash,
          "hash ignores middle texture data and middle-only texture table entries");
}

void TestSourceHashStableWhenVectorsReordered()
{
    game::SectorTopologyMap reordered = MakeAdjacent(0.0f, 24.0f, 8.0f, 24.0f);
    const std::string hash = game::ComputeSectorLightmapSourceHash(reordered);
    std::reverse(reordered.vertices.begin(), reordered.vertices.end());
    std::reverse(reordered.lineDefs.begin(), reordered.lineDefs.end());
    std::reverse(reordered.sideDefs.begin(), reordered.sideDefs.end());
    std::reverse(reordered.sectors.begin(), reordered.sectors.end());
    Check(game::ComputeSectorLightmapSourceHash(reordered) == hash, "hash is stable when topology vectors are reordered");
}

void TestLogicalSelfComparison()
{
    game::SectorGeneratedSurfaceRef floorA;
    floorA.kind = game::SectorGeneratedSurfaceKind::Floor;
    floorA.topologySectorId = 10;
    game::SectorGeneratedSurfaceRef floorB = floorA;
    Check(game::IsSameLogicalSectorLightmapSurface(floorA, floorB), "same floor sector is same logical surface");
    floorB.topologySectorId = 20;
    Check(!game::IsSameLogicalSectorLightmapSurface(floorA, floorB), "different floor sector is different");

    game::SectorGeneratedSurfaceRef wallA;
    wallA.kind = game::SectorGeneratedSurfaceKind::Wall;
    wallA.topologySectorId = 10;
    wallA.topologyLineDefId = 2;
    wallA.topologySideDefId = 8;
    wallA.topologySide = game::SectorTopologySideKind::Back;
    game::SectorGeneratedSurfaceRef wallB = wallA;
    Check(game::IsSameLogicalSectorLightmapSurface(wallA, wallB), "same wall topology refs are same logical surface");
    wallB.topologySideDefId = 9;
    Check(!game::IsSameLogicalSectorLightmapSurface(wallA, wallB), "different sidedef is different wall surface");
    wallB = wallA;
    wallB.kind = game::SectorGeneratedSurfaceKind::LowerWall;
    Check(!game::IsSameLogicalSectorLightmapSurface(wallA, wallB), "different wall kind is different surface");
}

void TestLayoutSmoke()
{
    std::string error;
    game::SectorLightmapLayout layout;
    Check(game::BuildSectorLightmapLayout(MakeSquare(), layout, error) && !layout.charts.empty(),
          "simple sector topology layout succeeds");

    const game::SectorTopologyMap equalPortal = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    Check(game::BuildSectorLightmapLayout(equalPortal, layout, error) && !layout.charts.empty(),
          "adjacent equal-height topology layout succeeds");
    Check(CountWallChartsForLine(equalPortal, 2) == 0, "equal-height portal produces no wall chart");

    const game::SectorTopologyMap raisedPlatform = MakePlatform();
    Check(game::BuildSectorLightmapLayout(raisedPlatform, layout, error) && !layout.charts.empty(),
          "inserted platform topology layout succeeds");
    Check(CountWallChartsForLine(raisedPlatform, 5) > 0, "platform riser receives a chart");
}

void TestMiddleSurfacesSkippedByLightmap()
{
    game::SectorTopologyMap map = MakeAdjacent(0.0f, 24.0f, 0.0f, 24.0f);
    map.texturesById.emplace("bars", Texture("bars"));
    game::FindSectorTopologySideDef(map, 2)->middle = Part("bars");

    game::SectorGeneratedGeometry geometry;
    std::string error;
    Check(game::BuildSectorGeneratedGeometry(map, geometry, &error), "middle lightmap test geometry builds");
    const int middleSurfaceCount = CountGeneratedSurfaces(geometry, game::SectorGeneratedSurfaceKind::Middle);
    Check(middleSurfaceCount == 2, "middle lightmap test generated middle surfaces");

    game::SectorLightmapLayout layout;
    Check(game::BuildSectorLightmapLayout(map, layout, error), "middle lightmap layout builds");
    Check(static_cast<int>(layout.charts.size()) == static_cast<int>(geometry.surfaces.size()),
          "layout keeps surface-index slots for generated geometry");
    Check(CountValidCharts(layout) == static_cast<int>(geometry.surfaces.size()) - middleSurfaceCount,
          "middle surfaces do not allocate valid lightmap charts");

    game::SectorLightmapBakeResult result;
    Check(game::BakeSectorLightmap(map, layout, "/tmp/sector_middle_lightmap_test.png", result, error),
          "middle lightmap bake succeeds without using middle surfaces");
    Check(result.staticGeometryTriangles == CountGeneratedTrianglesExceptMiddle(geometry),
          "bake triangle/BVH input ignores middle surfaces");
}

} // namespace

int main()
{
    TestSourceHashChanges();
    TestSourceHashIgnoresMiddleTextureData();
    TestSourceHashStableWhenVectorsReordered();
    TestLogicalSelfComparison();
    TestLayoutSmoke();
    TestMiddleSurfacesSkippedByLightmap();

    if (failures != 0) {
        std::fprintf(stderr, "%d sector topology lightmap test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
