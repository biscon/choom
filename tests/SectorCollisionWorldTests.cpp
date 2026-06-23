#include "sector_demo/SectorCollisionWorld.h"

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using game::SectorCoord;
using game::SectorTopologyLineDef;
using game::SectorTopologyMap;
using game::SectorTopologySector;
using game::SectorTopologySideDef;
using game::SectorTopologySideKind;
using game::SectorTopologyVertex;

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
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

void AddSide(
        SectorTopologyMap& map,
        int sideId,
        int lineId,
        SectorTopologySideKind side,
        int sectorId)
{
    SectorTopologySideDef sideDef;
    sideDef.id = sideId;
    sideDef.lineDefId = lineId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    map.sideDefs.push_back(sideDef);
}

SectorTopologySector Sector(int id, float floorZ = 0.0f, float ceilingZ = 24.0f)
{
    SectorTopologySector sector;
    sector.id = id;
    sector.floorZ = floorZ;
    sector.ceilingZ = ceilingZ;
    return sector;
}

void AddSectorLoop(
        SectorTopologyMap& map,
        int sectorId,
        const std::vector<std::pair<SectorCoord, SectorCoord>>& points)
{
    std::vector<int> vertexIds;
    for (const auto& point : points) {
        const int vertexId = game::AllocateSectorTopologyVertexId(map);
        map.vertices.push_back(SectorTopologyVertex{vertexId, point.first, point.second});
        vertexIds.push_back(vertexId);
    }

    for (size_t i = 0; i < vertexIds.size(); ++i) {
        const int lineId = game::AllocateSectorTopologyLineDefId(map);
        const int sideId = game::AllocateSectorTopologySideDefId(map);
        map.lineDefs.push_back(SectorTopologyLineDef{
                lineId,
                vertexIds[i],
                vertexIds[(i + 1) % vertexIds.size()],
                sideId,
                -1
        });
        AddSide(map, sideId, lineId, SectorTopologySideKind::Front, sectorId);
    }
}

SectorTopologyMap MakeSquare(float floorZ = 2.0f, float ceilingZ = 18.0f)
{
    SectorTopologyMap map;
    map.sectors.push_back(Sector(10, floorZ, ceilingZ));
    AddSectorLoop(map, 10, {{0, 0}, {64, 0}, {64, 64}, {0, 64}});
    return map;
}

SectorTopologyMap MakeAdjacent()
{
    SectorTopologyMap map;
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
    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 2, SectorTopologySideKind::Back, 20);
    map.sectors.push_back(Sector(20, 4.0f, 20.0f));
    map.sectors.push_back(Sector(10, 1.0f, 16.0f));
    return map;
}

const game::SectorCollisionEdge* FindEdge(
        const std::vector<game::SectorCollisionEdge>& edges,
        int lineDefId)
{
    for (const game::SectorCollisionEdge& edge : edges) {
        if (edge.lineDefId == lineDefId) {
            return &edge;
        }
    }
    return nullptr;
}

void TestBuildBasics()
{
    const SectorTopologyMap map = MakeSquare();
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "simple square collision world builds");
    Check(error.empty(), "successful collision build clears error");

    game::SectorCollisionHeights heights;
    Check(world.GetSectorFloorCeiling(10, &heights), "sector heights are available");
    Check(Near(heights.floorZ, game::SectorAuthoringToWorldDistance(2.0f))
                  && Near(heights.ceilingZ, game::SectorAuthoringToWorldDistance(18.0f)),
          "sector floor and ceiling heights are extracted in world units");

    const std::vector<game::SectorCollisionEdge>* edges = world.GetSectorEdges(10);
    Check(edges != nullptr && edges->size() == 4, "square has four collision edges");
    if (edges != nullptr) {
        const game::SectorCollisionEdge* edge = FindEdge(*edges, 1);
        Check(edge != nullptr, "collision edge keeps linedef ID");
        if (edge != nullptr) {
            Check(edge->kind == game::SectorCollisionEdgeKind::BlockingWall,
                  "one-sided edge is blocking");
            Check(!edge->blocksPlayer, "one-sided edge defaults blocksPlayer false");
            Check(edge->neighborSectorId == 0, "blocking edge has no neighbor");
            Check(Near(edge->a, game::SectorCoordToWorldPosition2(0, 0))
                          && Near(edge->b, game::SectorCoordToWorldPosition2(64, 0)),
                  "edge endpoints are stored in world coordinates");
        }
    }
}

void TestHeightsUseRenderedWorldUnits()
{
    const SectorTopologyMap map = MakeSquare(8.0f, 40.0f);
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "world-unit height test builds");

    game::SectorCollisionHeights heights;
    Check(world.GetSectorFloorCeiling(10, &heights), "world-unit sector heights are available");
    Check(Near(heights.floorZ, game::SectorAuthoringToWorldDistance(8.0f))
                  && Near(heights.ceilingZ, game::SectorAuthoringToWorldDistance(40.0f)),
          "collision heights match generated geometry world-space Y units");
    Check(!Near(heights.floorZ, 8.0f) && !Near(heights.ceilingZ, 40.0f),
          "collision heights are not raw authored sector heights");
}

void TestPortalExtraction()
{
    const SectorTopologyMap map = MakeAdjacent();
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "adjacent sectors build");

    const std::vector<game::SectorCollisionEdge>* leftEdges = world.GetSectorEdges(10);
    const std::vector<game::SectorCollisionEdge>* rightEdges = world.GetSectorEdges(20);
    Check(leftEdges != nullptr && rightEdges != nullptr, "portal sector edges are available");
    if (leftEdges != nullptr && rightEdges != nullptr) {
        const game::SectorCollisionEdge* leftPortal = FindEdge(*leftEdges, 2);
        const game::SectorCollisionEdge* rightPortal = FindEdge(*rightEdges, 2);
        Check(leftPortal != nullptr && rightPortal != nullptr, "both sectors expose shared portal");
        if (leftPortal != nullptr && rightPortal != nullptr) {
            Check(leftPortal->kind == game::SectorCollisionEdgeKind::Portal
                          && leftPortal->neighborSectorId == 20
                          && leftPortal->sideDefId == 2
                          && !leftPortal->blocksPlayer,
                  "left portal references right sector and preserves sidedef ID");
            Check(rightPortal->kind == game::SectorCollisionEdgeKind::Portal
                          && rightPortal->neighborSectorId == 10
                          && rightPortal->sideDefId == 8
                          && !rightPortal->blocksPlayer,
                  "right portal references left sector and preserves sidedef ID");
        }
    }

    const std::vector<int>* leftNeighbors = world.GetPortalNeighbors(10);
    Check(leftNeighbors != nullptr && leftNeighbors->size() == 1 && leftNeighbors->front() == 20,
          "portal neighbor list is exposed");
}

void TestBlocksPlayerFlagExtraction()
{
    SectorTopologyMap map = MakeAdjacent();
    map.lineDefs[1].flags.blocksPlayer = true;
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "blocksPlayer portal world builds");

    const std::vector<game::SectorCollisionEdge>* leftEdges = world.GetSectorEdges(10);
    const std::vector<game::SectorCollisionEdge>* rightEdges = world.GetSectorEdges(20);
    Check(leftEdges != nullptr && rightEdges != nullptr, "flagged portal sector edges are available");
    if (leftEdges != nullptr && rightEdges != nullptr) {
        const game::SectorCollisionEdge* leftPortal = FindEdge(*leftEdges, 2);
        const game::SectorCollisionEdge* rightPortal = FindEdge(*rightEdges, 2);
        Check(leftPortal != nullptr
                      && leftPortal->kind == game::SectorCollisionEdgeKind::Portal
                      && leftPortal->blocksPlayer,
              "flagged left portal carries blocksPlayer");
        Check(rightPortal != nullptr
                      && rightPortal->kind == game::SectorCollisionEdgeKind::Portal
                      && rightPortal->blocksPlayer,
              "flagged right portal carries blocksPlayer");
    }

    map = MakeSquare();
    map.lineDefs[0].flags.blocksPlayer = true;
    Check(world.BuildFromTopology(map, &error), "flagged one-sided wall world builds");
    const std::vector<game::SectorCollisionEdge>* edges = world.GetSectorEdges(10);
    Check(edges != nullptr, "flagged one-sided wall edges are available");
    if (edges != nullptr) {
        const game::SectorCollisionEdge* edge = FindEdge(*edges, 1);
        Check(edge != nullptr
                      && edge->kind == game::SectorCollisionEdgeKind::BlockingWall
                      && edge->blocksPlayer,
              "one-sided wall remains blocking and carries authored flag");
    }
}

void TestPointLookup()
{
    const SectorTopologyMap map = MakeAdjacent();
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "lookup world builds");

    const Vector2 leftInside = game::SectorCoordToWorldPosition2(32, 32);
    const Vector2 rightInside = game::SectorCoordToWorldPosition2(96, 32);
    const Vector2 outside = game::SectorCoordToWorldPosition2(160, 32);
    const Vector2 sharedBoundary = game::SectorCoordToWorldPosition2(64, 32);
    const Vector2 nearBoundary{sharedBoundary.x + 0.0005f, sharedBoundary.y};

    Check(world.FindSectorContainingPoint(leftInside) == 10,
          "point inside left sector resolves left sector");
    Check(world.FindSectorContainingPoint(rightInside) == 20,
          "point inside right sector resolves right sector");
    Check(world.FindSectorContainingPoint(outside) == 0,
          "point outside all sectors returns invalid");
    Check(world.FindSectorContainingPoint(sharedBoundary) == 10,
          "global boundary lookup is deterministic by sector ID");
    Check(world.FindSectorContainingPointPreferCurrent(sharedBoundary, 20) == 20,
          "current-sector-first keeps current sector on boundary");
    Check(world.FindSectorContainingPointPreferCurrent(nearBoundary, 10) == 10,
          "near-boundary lookup treats edge epsilon as contained for current sector");
    Check(world.FindSectorContainingPointPreferCurrent(rightInside, 10) == 20,
          "current-sector-first checks portal neighbors before global fallback");
}

void TestHoles()
{
    SectorTopologyMap map = MakeSquare();
    AddSectorLoop(map, 10, {{16, 16}, {16, 48}, {48, 48}, {48, 16}});

    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "sector with hole builds");
    Check(world.FindSectorContainingPoint(game::SectorCoordToWorldPosition2(8, 8)) == 10,
          "point inside outer loop and outside hole resolves sector");
    Check(world.FindSectorContainingPoint(game::SectorCoordToWorldPosition2(32, 32)) == 0,
          "point inside hole is not contained by sector");
    Check(world.FindSectorContainingPoint(game::SectorCoordToWorldPosition2(16, 32)) == 0,
          "point on hole boundary is not contained by sector");
}

void ExpectBuildFailure(SectorTopologyMap map, const char* description)
{
    game::SectorCollisionWorld world;
    std::string error;
    Check(!world.BuildFromTopology(map, &error), description);
    Check(!error.empty(), "failed build reports an error");
}

void TestRobustness()
{
    SectorTopologyMap missingSector = MakeSquare();
    missingSector.sideDefs.front().sectorId = 999;
    ExpectBuildFailure(missingSector, "missing referenced sector fails build");

    SectorTopologyMap missingLine = MakeSquare();
    missingLine.sideDefs.front().lineDefId = 999;
    ExpectBuildFailure(missingLine, "missing referenced linedef fails build");

    SectorTopologyMap missingVertex = MakeSquare();
    missingVertex.lineDefs.front().startVertexId = 999;
    ExpectBuildFailure(missingVertex, "missing referenced vertex fails build");

    SectorTopologyMap zeroLength = MakeSquare();
    zeroLength.vertices[1].x = zeroLength.vertices[0].x;
    zeroLength.vertices[1].y = zeroLength.vertices[0].y;
    ExpectBuildFailure(zeroLength, "zero-length edge fails build");

    SectorTopologyMap nonFiniteHeight = MakeSquare();
    nonFiniteHeight.sectors.front().floorZ = std::nanf("");
    ExpectBuildFailure(nonFiniteHeight, "non-finite floor height fails build");

    SectorTopologyMap invalidHeight = MakeSquare();
    invalidHeight.sectors.front().ceilingZ = invalidHeight.sectors.front().floorZ;
    ExpectBuildFailure(invalidHeight, "invalid height span fails build");

    SectorTopologyMap missingPortalOpposite = MakeAdjacent();
    missingPortalOpposite.sideDefs.pop_back();
    ExpectBuildFailure(missingPortalOpposite, "portal with missing opposite sidedef fails build");
}

} // namespace

int main()
{
    TestBuildBasics();
    TestHeightsUseRenderedWorldUnits();
    TestPortalExtraction();
    TestBlocksPlayerFlagExtraction();
    TestPointLookup();
    TestHoles();
    TestRobustness();

    if (failures != 0) {
        std::cerr << failures << " sector collision world test(s) failed\n";
        return 1;
    }
    std::cout << "Sector collision world tests passed\n";
    return 0;
}
