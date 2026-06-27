#include "sector_demo/SectorCollisionWorld.h"

#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

using game::SectorCoord;
using game::SectorTextureDefinition;
using game::SectorTextureFilter;
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
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(float a, float b, float epsilon = 0.0001f)
{
    return std::fabs(a - b) <= epsilon;
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

SectorCoord Coord(float authoringUnits)
{
    return static_cast<SectorCoord>(authoringUnits * game::SectorCoordSubdivisions);
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

SectorTopologyMap MakeSquare(float floorZ = 0.0f, float ceilingZ = 24.0f)
{
    SectorTopologyMap map;
    map.sectors.push_back(Sector(10, floorZ, ceilingZ));
    AddSectorLoop(map, 10, {{Coord(0), Coord(0)}, {Coord(64), Coord(0)}, {Coord(64), Coord(64)}, {Coord(0), Coord(64)}});
    return map;
}

SectorTopologyMap MakeAdjacent(float leftFloor, float rightFloor, float rightCeiling = 24.0f)
{
    SectorTopologyMap map;
    map.vertices = {
            {1, Coord(0), Coord(0)}, {2, Coord(64), Coord(0)}, {3, Coord(64), Coord(64)}, {4, Coord(0), Coord(64)},
            {5, Coord(128), Coord(0)}, {6, Coord(128), Coord(64)}};
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
    map.sectors.push_back(Sector(10, leftFloor, 24.0f));
    map.sectors.push_back(Sector(20, rightFloor, rightCeiling));
    return map;
}

game::SectorCollisionWorld BuildWorld(const SectorTopologyMap& map)
{
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), error.empty() ? "collision world builds" : error.c_str());
    return world;
}

game::SectorCollisionMoveResult Move(
        const game::SectorCollisionWorld& world,
        Vector2 position,
        Vector2 delta,
        int sectorId,
        bool grounded,
        float feetY = 0.0f,
        float stepHeight = 0.25f,
        float playerHeight = 1.6f,
        float radius = 0.25f)
{
    return world.ResolveMovement(
            game::SectorCollisionMoveState{position, feetY, sectorId, grounded},
            delta,
            game::SectorCollisionMoveConfig{radius, playerHeight, stepHeight, 4});
}

void TestBlockingWallStopsAndSlides()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeSquare());
    const Vector2 start = game::SectorCoordToWorldPosition2(Coord(60), Coord(32));
    const game::SectorCollisionMoveResult blocked =
            Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(blocked.hitWall, "solid wall reports hit");
    Check(blocked.currentSectorId == 10, "solid wall keeps sector");
    Check(blocked.positionXZ.x <= game::SectorCoordToWorldPosition2(Coord(64), Coord(0)).x - 0.249f,
          "solid wall prevents radius penetration");

    const game::SectorCollisionMoveResult slide =
            Move(world, start, Vector2{2.0f, 1.0f}, 10, true);
    Check(slide.hitWall, "diagonal wall contact reports hit");
    Check(slide.positionXZ.y > start.y + 0.5f, "diagonal wall contact preserves tangential slide");

    const Vector2 nearWall = game::SectorCoordToWorldPosition2(Coord(63), Coord(32));
    const game::SectorCollisionMoveResult pushed =
            Move(world, nearWall, Vector2{0.01f, 0.0f}, 10, true);
    Check(pushed.positionXZ.x <= game::SectorCoordToWorldPosition2(Coord(64), Coord(0)).x - 0.249f,
          "near-wall start is prevented from penetrating");
}

void TestPortalStepAndCeilingRules()
{
    const Vector2 start = game::SectorCoordToWorldPosition2(Coord(60), Coord(32));

    game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(0.0f, 0.0f));
    game::SectorCollisionMoveResult result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 20 && !result.hitWall, "same-floor portal is passable");

    world = BuildWorld(MakeAdjacent(0.0f, 2.0f));
    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 20, "small upward portal within step height is passable");

    world = BuildWorld(MakeAdjacent(0.0f, 4.0f));
    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 10 && result.blockedByStep,
          "upward portal above step height blocks");

    world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 20, "downward portal is passable in this phase");

    world = BuildWorld(MakeAdjacent(0.0f, 0.0f, 8.0f));
    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true, 0.0f, 0.25f, 1.6f);
    Check(result.currentSectorId == 10 && result.blockedByCeiling,
          "portal with insufficient ceiling clearance blocks");
}

void TestBlocksPlayerPortalMovement()
{
    const Vector2 start = game::SectorCoordToWorldPosition2(Coord(60), Coord(32));

    SectorTopologyMap map = MakeAdjacent(0.0f, 0.0f);
    game::SectorCollisionWorld world = BuildWorld(map);
    game::SectorCollisionMoveResult result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 20 && !result.hitWall,
          "same-height portal without blocksPlayer is passable");

    map.lineDefs[1].flags.blocksPlayer = true;
    world = BuildWorld(map);
    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 10 && result.hitWall,
          "same-height portal with blocksPlayer blocks movement");
    Check(!result.blockedByStep && !result.blockedByCeiling,
          "blocksPlayer portal reports wall contact instead of step or ceiling block");

    map.lineDefs[1].flags.blocksPlayer = false;
    world = BuildWorld(map);
    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 20 && !result.hitWall,
          "disabling blocksPlayer restores portal passability");
}

void TestMiddleTexturePortalMovement()
{
    const Vector2 start = game::SectorCoordToWorldPosition2(Coord(60), Coord(32));
    SectorTopologyMap map = MakeAdjacent(0.0f, 0.0f);
    map.texturesById.emplace("bars", SectorTextureDefinition{
            "bars", "textures/bars.png", SectorTextureFilter::Point});
    map.sideDefs[1].middle.textureId = "bars";

    game::SectorCollisionWorld world = BuildWorld(map);
    game::SectorCollisionMoveResult result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 20 && !result.hitWall,
          "middle-texture portal without blocksPlayer remains passable");

    map.lineDefs[1].flags.blocksPlayer = true;
    world = BuildWorld(map);
    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 10 && result.hitWall,
          "middle-texture portal with blocksPlayer blocks movement");

    const game::SectorCollisionMoveResult slide =
            Move(world, start, Vector2{2.0f, 1.0f}, 10, true);
    Check(slide.currentSectorId == 10 && slide.hitWall,
          "diagonal movement into blocked middle-texture portal reports wall contact");
    Check(slide.positionXZ.y > start.y + 0.5f,
          "diagonal blocked middle-texture portal preserves tangential slide");
}

void TestDownwardPortalVerticalTransitions()
{
    const Vector2 start = game::SectorCoordToWorldPosition2(Coord(60), Coord(32));

    game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 2.0f));
    game::SectorCollisionMoveResult moveResult = Move(
            world,
            start,
            Vector2{2.0f, 0.0f},
            10,
            true,
            game::SectorAuthoringToWorldDistance(4.0f));
    Check(moveResult.currentSectorId == 20, "small downward portal is horizontally passable");

    game::SectorCollisionHeights heights;
    Check(world.GetSectorFloorCeiling(moveResult.currentSectorId, &heights),
          "small downward portal destination heights are available");
    game::SectorFpsControllerState fpsState;
    fpsState.feetPosition = Vector3{
            moveResult.positionXZ.x,
            game::SectorAuthoringToWorldDistance(4.0f),
            moveResult.positionXZ.y};
    fpsState.currentSectorId = moveResult.currentSectorId;
    fpsState.grounded = true;
    fpsState.verticalVelocity = -5.0f;
    game::SectorFpsControllerConfig fpsConfig;
    game::SectorFpsVerticalResult verticalResult =
            game::UpdateSectorFpsVerticalPhysics(
                    fpsState,
                    fpsConfig,
                    game::SectorFpsVerticalContext{true, heights.floorZ, heights.ceilingZ},
                    0.0f);
    Check(verticalResult.transition == game::SectorFpsVerticalTransition::SnappedDown,
          "small downward portal snaps down after movement");
    Check(Near(fpsState.feetPosition.y, game::SectorAuthoringToWorldDistance(2.0f)),
          "small downward portal places feet on lower floor");
    Check(fpsState.grounded, "small downward portal remains grounded");

    world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    moveResult = Move(
            world,
            start,
            Vector2{2.0f, 0.0f},
            10,
            true,
            game::SectorAuthoringToWorldDistance(4.0f));
    Check(moveResult.currentSectorId == 20, "large downward portal is horizontally passable");
    Check(world.GetSectorFloorCeiling(moveResult.currentSectorId, &heights),
          "large downward portal destination heights are available");
    fpsState.feetPosition = Vector3{
            moveResult.positionXZ.x,
            game::SectorAuthoringToWorldDistance(4.0f),
            moveResult.positionXZ.y};
    fpsState.currentSectorId = moveResult.currentSectorId;
    fpsState.grounded = true;
    fpsState.verticalVelocity = -5.0f;
    verticalResult = game::UpdateSectorFpsVerticalPhysics(
            fpsState,
            fpsConfig,
            game::SectorFpsVerticalContext{true, heights.floorZ, heights.ceilingZ},
            0.0f);
    Check(verticalResult.transition == game::SectorFpsVerticalTransition::StartedDrop,
          "large downward portal starts drop after movement");
    Check(Near(fpsState.feetPosition.y, game::SectorAuthoringToWorldDistance(4.0f)),
          "large downward portal preserves feet height initially");
    Check(!fpsState.grounded, "large downward portal starts falling");
    Check(Near(fpsState.verticalVelocity, 0.0f),
          "large downward portal starts falling with deterministic zero velocity");

    world = BuildWorld(MakeAdjacent(0.0f, 0.0f));
    moveResult = Move(world, start, Vector2{2.0f, 0.0f}, 10, true, 0.0f);
    Check(moveResult.currentSectorId == 20, "same-height portal is horizontally passable");
    Check(world.GetSectorFloorCeiling(moveResult.currentSectorId, &heights),
          "same-height portal destination heights are available");
    fpsState.feetPosition = Vector3{moveResult.positionXZ.x, 0.0f, moveResult.positionXZ.y};
    fpsState.currentSectorId = moveResult.currentSectorId;
    fpsState.grounded = true;
    verticalResult = game::UpdateSectorFpsVerticalPhysics(
            fpsState,
            fpsConfig,
            game::SectorFpsVerticalContext{true, heights.floorZ, heights.ceilingZ},
            0.0f);
    Check(verticalResult.transition == game::SectorFpsVerticalTransition::StayedGrounded,
          "same-height portal stays grounded without drop");
    Check(fpsState.grounded, "same-height portal remains grounded");
}

void TestDownwardPortalDoesNotApplyRadiusNudge()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).y;
    constexpr float startInset = 0.01f;
    const float feetY = game::SectorAuthoringToWorldDistance(4.0f);

    for (float radius : {0.25f, 0.5f, 1.5f}) {
        const Vector2 start{portalX - startInset, z};
        const Vector2 delta{radius * 0.75f, 0.0f};
        const Vector2 expected{start.x + delta.x, start.y + delta.y};
        const game::SectorCollisionMoveResult moveResult = Move(
                world,
                start,
                delta,
                10,
                true,
                feetY,
                0.25f,
                1.6f,
                radius);

        Check(moveResult.currentSectorId == 20,
              "large downward portal crossing reaches lower sector");
        Check(Near(moveResult.positionXZ.x, expected.x)
                      && Near(moveResult.positionXZ.y, expected.y),
              "large downward portal crossing preserves requested horizontal movement");

        game::SectorCollisionHeights heights;
        Check(world.GetSectorFloorCeiling(moveResult.currentSectorId, &heights),
              "large downward portal regression destination heights are available");
        game::SectorFpsControllerState fpsState;
        fpsState.feetPosition = Vector3{moveResult.positionXZ.x, feetY, moveResult.positionXZ.y};
        fpsState.currentSectorId = moveResult.currentSectorId;
        fpsState.grounded = true;
        const game::SectorFpsVerticalResult verticalResult =
                game::UpdateSectorFpsVerticalPhysics(
                        fpsState,
                        game::SectorFpsControllerConfig{},
                        game::SectorFpsVerticalContext{true, heights.floorZ, heights.ceilingZ},
                        0.0f);
        Check(verticalResult.transition == game::SectorFpsVerticalTransition::StartedDrop,
              "large downward portal regression starts falling immediately");
        Check(!fpsState.grounded, "large downward portal regression clears grounded");
    }
}

void TestLowerSectorNearReversePortalDoesNotApplyRadiusNudge()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).y;
    const float feetY = game::SectorAuthoringToWorldDistance(4.0f);
    const float radius = 1.5f;
    const Vector2 start{portalX + 0.01f, z};

    game::SectorCollisionMoveResult result = Move(
            world,
            start,
            Vector2{},
            20,
            true,
            feetY,
            0.25f,
            1.6f,
            radius);
    Check(result.currentSectorId == 20, "zero movement near reverse step portal keeps lower sector");
    Check(Near(result.positionXZ.x, start.x) && Near(result.positionXZ.y, start.y),
          "zero movement near reverse step portal does not apply radius nudge");

    const Vector2 tinyAway{0.0002f, 0.0f};
    result = Move(
            world,
            start,
            tinyAway,
            20,
            true,
            feetY,
            0.25f,
            1.6f,
            radius);
    Check(result.currentSectorId == 20, "tiny inward movement near reverse step portal keeps lower sector");
    Check(Near(result.positionXZ.x, start.x + tinyAway.x)
                  && Near(result.positionXZ.y, start.y + tinyAway.y),
          "tiny inward movement near reverse step portal does not apply radius nudge");
}

void TestAirbornePortalRules()
{
    const Vector2 start = game::SectorCoordToWorldPosition2(Coord(60), Coord(32));
    game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(0.0f, 2.0f));
    game::SectorCollisionMoveResult result =
            Move(world, start, Vector2{2.0f, 0.0f}, 10, false, 0.0f);
    Check(result.currentSectorId == 10 && result.blockedByStep,
          "airborne player cannot auto-step up through higher portal");

    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, false, 0.25f);
    Check(result.currentSectorId == 20, "airborne player can pass when vertical span fits");
}

void TestJumpingPlayerCannotAutoStepThroughPortal()
{
    const Vector2 start = game::SectorCoordToWorldPosition2(Coord(60), Coord(32));
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(0.0f, 2.0f));

    game::SectorFpsControllerState fpsState;
    fpsState.feetPosition = Vector3{start.x, 0.0f, start.y};
    fpsState.currentSectorId = 10;
    fpsState.grounded = true;
    game::SectorFpsControllerConfig fpsConfig;
    fpsConfig.gravity = 25.0f;
    fpsConfig.jumpHeight = 0.6f;

    Check(game::TryStartSectorFpsJump(fpsState, fpsConfig), "jump starts before portal movement");
    game::SectorCollisionMoveResult result = Move(
            world,
            start,
            Vector2{2.0f, 0.0f},
            fpsState.currentSectorId,
            fpsState.grounded,
            fpsState.feetPosition.y);
    Check(result.currentSectorId == 10 && result.blockedByStep,
          "jumping player cannot auto-step through higher-floor portal");

    fpsState.feetPosition.y = 0.25f;
    result = Move(
            world,
            start,
            Vector2{2.0f, 0.0f},
            fpsState.currentSectorId,
            fpsState.grounded,
            fpsState.feetPosition.y);
    Check(result.currentSectorId == 20,
          "jumping player can pass higher-floor portal after current cylinder fits");

    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true, 0.0f);
    Check(result.currentSectorId == 20,
          "grounded player can still step through higher-floor portal within step height");
}

void TestLowerSectorMovementIntoTooHighPortalStillBlocks()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).y;
    const float feetY = game::SectorAuthoringToWorldDistance(4.0f);
    const float radius = 1.5f;
    const Vector2 start{portalX + radius + 0.1f, z};
    const game::SectorCollisionMoveResult result = Move(
            world,
            start,
            Vector2{-0.75f, 0.0f},
            20,
            true,
            feetY,
            0.25f,
            1.6f,
            radius);

    Check(result.currentSectorId == 20 && result.blockedByStep,
          "movement from lower sector into too-high portal is still blocked by step");
    Check(result.positionXZ.x >= portalX + radius - 0.001f,
          "too-high reverse portal still applies radius clearance when moving into it");
}

void TestSectorFallbackAndBoundary()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeSquare());
    const Vector2 inside = game::SectorCoordToWorldPosition2(Coord(32), Coord(32));
    game::SectorCollisionMoveResult result = Move(world, inside, Vector2{0.1f, 0.0f}, 0, true);
    Check(result.currentSectorId == 10, "movement resolves sector from feet position when current sector is invalid");

    const Vector2 nearLeftWall = game::SectorCoordToWorldPosition2(Coord(2), Coord(32));
    result = Move(world, nearLeftWall, Vector2{-2.0f, 0.0f}, 10, true);
    Check(result.currentSectorId == 10 && result.positionXZ.x >= 0.249f,
          "movement cannot leave all sectors through solid boundary");
}

} // namespace

int main()
{
    TestBlockingWallStopsAndSlides();
    TestPortalStepAndCeilingRules();
    TestBlocksPlayerPortalMovement();
    TestMiddleTexturePortalMovement();
    TestDownwardPortalVerticalTransitions();
    TestDownwardPortalDoesNotApplyRadiusNudge();
    TestLowerSectorNearReversePortalDoesNotApplyRadiusNudge();
    TestAirbornePortalRules();
    TestJumpingPlayerCannotAutoStepThroughPortal();
    TestLowerSectorMovementIntoTooHighPortalStillBlocks();
    TestSectorFallbackAndBoundary();
    if (failures == 0) {
        std::puts("Sector collision movement tests passed");
    }
    return failures == 0 ? 0 : 1;
}
