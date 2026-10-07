#include "sector_demo/SectorCollisionWorld.h"

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"
#include "sector_demo/SectorStaticModelTransform.h"
#include "sector_demo/renderer/SectorOpaqueDrawPolicy.h"

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

void TestBaseboardsDoNotChangeCollision()
{
    auto map = MakeSquare();
    game::SectorCollisionWorld before, after;
    std::string error;
    Check(before.BuildFromTopology(map, &error), "collision fixture builds without trim");
    for (auto& side : map.sideDefs) side.baseboard = {true, 8.0f, 2.0f, "trim"};
    Check(after.BuildFromTopology(map, &error), "collision fixture builds with thick trim");
    const auto* a = before.GetSectorEdges(10);
    const auto* b = after.GetSectorEdges(10);
    Check(a && b && a->size() == b->size(), "baseboards do not add collision edges");
    if (!a || !b || a->size() != b->size()) return;
    for (size_t i = 0; i < a->size(); ++i) {
        Check(Near((*a)[i].a, (*b)[i].a) && Near((*a)[i].b, (*b)[i].b)
                && (*a)[i].kind == (*b)[i].kind, "baseboard thickness leaves collision unchanged");
    }
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

void TestBoundsOverlap()
{
    auto map = MakeSquare(0.0f, 24.0f);
    game::SectorCollisionWorld world;
    Check(world.BuildFromTopology(map), "bounds query fixture builds");
    const float size = game::SectorCoordToWorldDistance(64);
    const float top = game::SectorAuthoringToWorldDistance(24);
    Check(world.BoundsOverlapSector(10, {{size / 4, 0, size / 4}, {size / 2, 1, size / 2}}),
          "bounds wholly inside sector overlap");
    Check(world.BoundsOverlapSector(10, {{-size, 0, -size}, {2 * size, 1, 2 * size}}),
          "bounds enclosing sector overlap without corners inside sector");
    Check(world.BoundsOverlapSector(10, {{size, 0, size / 4}, {size, 1, size / 2}}),
          "zero-thickness bounds touching boundary overlap");
    Check(world.BoundsOverlapSector(10, {{size + 0.0005f, 0, 0}, {2 * size, 1, size}}),
          "near-boundary bounds use collision tolerance");
    Check(!world.BoundsOverlapSector(10, {{size + 0.01f, 0, 0}, {2 * size, 1, size}}),
          "bounds clear of boundary are rejected");
    Check(!world.BoundsOverlapSector(10, {{0, -2, 0}, {size, -1, size}}),
          "bounds below floor are rejected");
    Check(!world.BoundsOverlapSector(10, {{0, top + 1, 0}, {size, top + 2, size}}),
          "bounds above solid ceiling are rejected");
    Check(!world.BoundsOverlapSector(99, {{0, 0, 0}, {size, 1, size}}),
          "unknown sector does not overlap");
    Check(!world.BoundsOverlapSector(10, {{NAN, 0, 0}, {size, 1, size}})
                  && !world.BoundsOverlapSector(10, {{size, 0, 0}, {0, 1, size}}),
          "invalid bounds do not overlap");
    map.sectors.front().ceilingSky = true;
    Check(world.BuildFromTopology(map), "sky bounds query fixture rebuilds");
    Check(world.BoundsOverlapSector(10, {{0, top + 1, 0}, {size, top + 2, size}}),
          "sky sector has no upper visibility bound");

    AddSectorLoop(map, 10, {{16, 16}, {16, 48}, {48, 48}, {48, 16}});
    Check(world.BuildFromTopology(map), "bounds query hole fixture builds");
    Check(!world.BoundsOverlapSector(10,
                  {{size * .4f, 0, size * .4f}, {size * .6f, 1, size * .6f}}),
          "bounds wholly inside hole are rejected");
    Check(world.BoundsOverlapSector(10,
                  {{size * .2f, 0, size * .4f}, {size * .4f, 1, size * .6f}}),
          "bounds crossing hole boundary overlap");

    SectorTopologyMap concave;
    concave.sectors.push_back(Sector(10));
    AddSectorLoop(concave, 10, {{0, 0}, {64, 0}, {64, 16}, {16, 16}, {16, 64}, {0, 64}});
    Check(world.BuildFromTopology(concave), "concave bounds query fixture builds");
    Check(!world.BoundsOverlapSector(10,
                  {{size * .4f, 0, size * .4f}, {size * .6f, 1, size * .6f}}),
          "sector bounding rectangle alone does not accept concave exterior");
    Check(world.BoundsOverlapSector(10,
                  {{size * .1f, 0, size * .4f}, {size * .4f, 1, size * .6f}}),
          "bounds crossing concave boundary overlap");
}

void TestLightingSectorBoundsResolution()
{
    SectorTopologyMap map;
    map.sectors = {Sector(20), Sector(10)};
    AddSectorLoop(map, 10, {{0, 0}, {512, 0}, {512, 512}, {0, 512}});
    AddSectorLoop(map, 20, {{1024, 0}, {1536, 0}, {1536, 512}, {1024, 512}});
    game::SectorCollisionWorld world;
    Check(world.BuildFromTopology(map), "lighting membership fixture builds");
    const BoundingBox rightRoom{{8, 0, 1}, {10, 2, 3}};
    Check(world.ResolveLightingSectorForBounds(10, rightRoom, true) == 10,
          "valid origin sector retains lighting precedence over bounds center");
    Check(world.ResolveLightingSectorForBounds(-1, rightRoom, true) == 20,
          "outside origin uses bounds center sector");
    Check(world.ResolveLightingSectorForBounds(-1, {{3, 0, 1}, {9, 2, 3}}, true) == 10,
          "equidistant overlapping sectors tie-break by stable ID");
    Check(world.ResolveLightingSectorForBounds(-1, {{3, 0, 1}, {10, 2, 3}}, true) == 20,
          "bounds centered in a gap choose nearest overlapping sector volume");
    Check(world.ResolveLightingSectorForBounds(-1, {{5, 0, 1}, {7, 2, 3}}, true) == 0,
          "nearby sectors without overlap are not used for lighting");
    Check(world.ResolveLightingSectorForBounds(-1, rightRoom, false) == 0
                  && world.ResolveLightingSectorForBounds(10, {}, false) == 10,
          "unavailable bounds preserve origin fallback");
    Check(world.ResolveLightingSectorForBounds(-1, {{NAN, 0, 0}, {1, 1, 1}}, true) == 0,
          "invalid bounds cannot assign a lighting sector");
    std::reverse(map.sectors.begin(), map.sectors.end());
    Check(world.BuildFromTopology(map)
                  && world.ResolveLightingSectorForBounds(-1, {{3, 0, 1}, {9, 2, 3}}, true) == 10,
          "sector storage ordering cannot change lighting membership");
    Check(world.ResolveLightingSectorForBounds(-1, {{1, 5, 1}, {2, 6, 2}}, true) == 0,
          "solid ceiling rejects vertically separated lighting receiver");
    map.sectors.front().ceilingSky = true;
    Check(world.BuildFromTopology(map)
                  && world.ResolveLightingSectorForBounds(-1, {{1, 5, 1}, {2, 6, 2}}, true) == 10,
          "sky ceiling permits elevated lighting receiver");
    AddSectorLoop(map, 10, {{128, 128}, {128, 384}, {384, 384}, {384, 128}});
    Check(world.BuildFromTopology(map)
                  && world.ResolveLightingSectorForBounds(-1, {{1.5f, 0, 1.5f}, {2.5f, 2, 2.5f}}, true) == 0
                  && world.ResolveLightingSectorForBounds(-1, {{0, 0, 0}, {4, 2, 4}}, true) == 10,
          "hole center requires actual bounds overlap with surrounding sector");
}

void TestStaticPropBoundsVisibility()
{
    SectorTopologyMap map;
    map.sectors.push_back(Sector(10));
    map.sectors.push_back(Sector(20));
    // Two disconnected rooms in world units: [0,4] and [8,12] along X.
    AddSectorLoop(map, 10, {{0, 0}, {512, 0}, {512, 512}, {0, 512}});
    AddSectorLoop(map, 20, {{1024, 0}, {1536, 0}, {1536, 512}, {1024, 512}});
    game::SectorCollisionWorld world;
    Check(world.BuildFromTopology(map), "static prop visibility fixture builds");
    game::RuntimePortalVisibilityResult visibility;
    visibility.validStartSector = true;
    visibility.visibleSectorIds = {10};
    const Camera3D camera{{2, 1, -8}, {2, 1, 2}, {0, 1, 0}, 90, CAMERA_PERSPECTIVE};
    const auto visible = [&](BoundingBox bounds, int originSector = -1, bool enabled = true,
                             bool hasBounds = true, const game::SectorCollisionWorld* lookup = nullptr) {
        return game::SectorStaticPropVisible(enabled, originSector, visibility,
                camera, 1, .1f, 100, bounds, hasBounds, lookup);
    };
    const BoundingBox fence{{-.2f, .08f, 1.96f}, {3.9f, 2.38f, 2.04f}};
    Check(visible(fence, -1, true, true, &world),
          "offset fence stays visible with origin outside all sectors");
    Check(visible({{3, 0, 2}, {9, 2, 2.1f}}, 20, true, true, &world),
          "prop extending from hidden sector into visible sector stays visible");
    const BoundingBox hidden{{8.1f, 0, 2}, {9, 2, 2.1f}};
    Check(!visible(hidden, 20, true, true, &world),
          "prop wholly in hidden sector is culled");
    visibility.boundarySurfaceSectorIds = {20};
    Check(!visible(hidden, 20, true, true, &world),
          "closed-portal boundary-only sector does not expose props");
    Check(!visible(fence, -1, false, true, &world), "hidden or disabled prop is culled");
    Check(!visible({{-1, 0, -12}, {1, 2, -11}}, 10, true, true, &world),
          "visible origin sector does not bypass frustum culling");
    Check(!visible(fence, -1, true, true), "missing lookup retains origin-sector policy");
    Check(!visible(fence, -1, true, false, &world)
                  && visible(fence, 10, true, false, &world),
          "unknown bounds retain origin-sector policy");
    Check(!visible({{NAN, 0, 0}, {1, 2, 1}}, -1, true, true, &world),
          "invalid bounds cannot expose an unknown sector prop");

    // A thin local fence segment is enough to isolate scale/yaw eligibility.
    // Transform its two endpoints with the same authored transform as rendering.
    const auto transformedFence = [](float scale, float yaw) {
        const Matrix transform = game::BuildSectorStaticModelAuthoredTransform(
                {-4, 1, 2}, 0, yaw, 0, scale);
        const Vector3 a = Vector3Transform({.05f, 0, 0}, transform);
        const Vector3 b = Vector3Transform({3.95f, 0, 0}, transform);
        return BoundingBox{{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)},
                           {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)}};
    };
    Check(!visible(transformedFence(1, 0), -1, true, true, &world),
          "unscaled offset fence is outside sector");
    Check(visible(transformedFence(1.05f, 0), -1, true, true, &world),
          "increased scale moves fence bounds into visible sector");
    Check(!visible(transformedFence(1.05f, PI / 2), -1, true, true, &world),
          "rotating fence away removes sector overlap immediately");
    visibility.fallbackDrawAll = true;
    Check(visible(hidden, -1, true, true, &world), "draw-all fallback remains available");
    visibility.fallbackDrawAll = false;
    visibility.validStartSector = false;
    Check(visible(hidden, -1, true, true, &world), "invalid start sector retains fallback");
}

void TestRaycastSurfacesAndRange()
{
    const SectorTopologyMap map = MakeSquare(0.0f, 24.0f);
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "raycast square builds");
    const Vector2 center = game::SectorCoordToWorldPosition2(32, 32);
    const float ceiling = game::SectorAuthoringToWorldDistance(24.0f);

    const game::SectorCollisionRayHit floorHit = world.Raycast(
            Vector3{center.x, 1.0f, center.y},
            Vector3{0.0f, -1.0f, 0.0f},
            100.0f);
    Check(floorHit.hit
                  && floorHit.surfaceKind
                          == game::SectorCollisionRaySurfaceKind::Floor
                  && Near(floorHit.position.y, 0.0f)
                  && Near(floorHit.normal.y, 1.0f),
          "raycast hits floor with world position and opposing normal");

    const game::SectorCollisionRayHit ceilingHit = world.Raycast(
            Vector3{center.x, 1.0f, center.y},
            Vector3{0.0f, 1.0f, 0.0f},
            100.0f);
    Check(ceilingHit.hit
                  && ceilingHit.surfaceKind
                          == game::SectorCollisionRaySurfaceKind::Ceiling
                  && Near(ceilingHit.position.y, ceiling)
                  && Near(ceilingHit.normal.y, -1.0f),
          "raycast hits ceiling with world position and opposing normal");

    const game::SectorCollisionRayHit wallHit = world.Raycast(
            Vector3{center.x, 1.0f, center.y},
            Vector3{1.0f, 0.0f, 0.0f},
            100.0f);
    Check(wallHit.hit
                  && wallHit.surfaceKind
                          == game::SectorCollisionRaySurfaceKind::Wall
                  && wallHit.lineDefId > 0
                  && wallHit.sideDefId > 0
                  && wallHit.sectorId == 10,
          "raycast preserves nearest wall topology identity");
    Check(!world.Raycast(
                    Vector3{center.x, 1.0f, center.y},
                    Vector3{1.0f, 0.0f, 0.0f},
                    wallHit.distance * 0.5f).hit,
          "raycast maximum range produces a normal miss");
}

void TestRaycastPortalOpeningAndWallStrips()
{
    const SectorTopologyMap map = MakeAdjacent();
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "portal raycast world builds");
    const Vector2 left = game::SectorCoordToWorldPosition2(32, 32);

    const game::SectorCollisionRayHit through = world.Raycast(
            Vector3{left.x, 1.0f, left.y},
            Vector3{1.0f, 0.0f, 0.0f},
            100.0f);
    Check(through.hit && through.lineDefId == 6 && through.sectorId == 20,
          "ray passes through portal opening and hits farther solid wall");

    const game::SectorCollisionRayHit lower = world.Raycast(
            Vector3{left.x, 0.2f, left.y},
            Vector3{1.0f, 0.0f, 0.0f},
            100.0f);
    Check(lower.hit && lower.lineDefId == 2
                  && lower.surfaceKind
                          == game::SectorCollisionRaySurfaceKind::LowerWall,
          "ray hits portal lower wall strip below opening");
}

void TestRaycastSkipsSkyCeiling()
{
    SectorTopologyMap map = MakeSquare(0.0f, 24.0f);
    map.sectors.front().ceilingSky = true;
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), "sky raycast world builds");
    const Vector2 center = game::SectorCoordToWorldPosition2(32, 32);
    Check(!world.Raycast(
                    Vector3{center.x, 1.0f, center.y},
                    Vector3{0.0f, 1.0f, 0.0f},
                    100.0f).hit,
          "raycast does not hit an open-sky ceiling");
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
    TestBaseboardsDoNotChangeCollision();
    TestBuildBasics();
    TestHeightsUseRenderedWorldUnits();
    TestPortalExtraction();
    TestBlocksPlayerFlagExtraction();
    TestPointLookup();
    TestHoles();
    TestBoundsOverlap();
    TestStaticPropBoundsVisibility();
    TestLightingSectorBoundsResolution();
    TestRaycastSurfacesAndRange();
    TestRaycastPortalOpeningAndWallStrips();
    TestRaycastSkipsSkyCeiling();
    TestRobustness();

    if (failures != 0) {
        std::cerr << failures << " sector collision world test(s) failed\n";
        return 1;
    }
    std::cout << "Sector collision world tests passed\n";
    return 0;
}
