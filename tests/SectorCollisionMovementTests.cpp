#include "sector_demo/SectorCollisionWorld.h"
#include "game/items/ItemDropPlacement.h"

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
using game::SectorMaterialDefinition;
using game::SectorMaterialFilter;
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

SectorTopologyMap MakeAdjacentDestinationExtendsPastPortal(
        float leftFloor,
        float rightFloor)
{
    SectorTopologyMap map;
    map.vertices = {
            {1, Coord(0), Coord(0)},
            {2, Coord(64), Coord(0)},
            {3, Coord(64), Coord(64)},
            {4, Coord(0), Coord(64)},
            {5, Coord(128), Coord(0)},
            {6, Coord(128), Coord(128)},
            {7, Coord(64), Coord(128)}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, 9},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1},
            {6, 5, 6, 6, -1},
            {7, 6, 7, 7, -1},
            {8, 7, 3, 8, -1}};
    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
    AddSide(map, 5, 5, SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 8, SectorTopologySideKind::Front, 20);
    AddSide(map, 9, 2, SectorTopologySideKind::Back, 20);
    map.sectors.push_back(Sector(10, leftFloor, 24.0f));
    map.sectors.push_back(Sector(20, rightFloor, 24.0f));
    return map;
}

SectorTopologyMap MakeRoomLandingAndBlockedPortal(
        float landingWidth,
        float roomFloor,
        float landingFloor,
        float farFloor)
{
    const SectorCoord roomRight = Coord(64.0f);
    const SectorCoord landingRight = Coord(64.0f + landingWidth);
    const SectorCoord farRight = Coord(128.0f + landingWidth);

    SectorTopologyMap map;
    map.vertices = {
            {1, Coord(0), Coord(0)},
            {2, roomRight, Coord(0)},
            {3, roomRight, Coord(64)},
            {4, Coord(0), Coord(64)},
            {5, landingRight, Coord(0)},
            {6, landingRight, Coord(64)},
            {7, farRight, Coord(0)},
            {8, farRight, Coord(64)}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, 8},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1},
            {5, 2, 5, 5, -1},
            {6, 5, 6, 6, 12},
            {7, 6, 3, 7, -1},
            {8, 5, 7, 9, -1},
            {9, 7, 8, 10, -1},
            {10, 8, 6, 11, -1}};
    map.lineDefs[5].flags.blocksPlayer = true;

    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);

    AddSide(map, 5, 5, SectorTopologySideKind::Front, 20);
    AddSide(map, 6, 6, SectorTopologySideKind::Front, 20);
    AddSide(map, 7, 7, SectorTopologySideKind::Front, 20);
    AddSide(map, 8, 2, SectorTopologySideKind::Back, 20);

    AddSide(map, 9, 8, SectorTopologySideKind::Front, 30);
    AddSide(map, 10, 9, SectorTopologySideKind::Front, 30);
    AddSide(map, 11, 10, SectorTopologySideKind::Front, 30);
    AddSide(map, 12, 6, SectorTopologySideKind::Back, 30);

    map.sectors.push_back(Sector(10, roomFloor, 24.0f));
    map.sectors.push_back(Sector(20, landingFloor, 24.0f));
    map.sectors.push_back(Sector(30, farFloor, 24.0f));
    return map;
}

game::SectorCollisionWorld BuildWorld(const SectorTopologyMap& map)
{
    game::SectorCollisionWorld world;
    std::string error;
    Check(world.BuildFromTopology(map, &error), error.empty() ? "collision world builds" : error.c_str());
    return world;
}

bool AddStructuralPrimitive(
        SectorTopologyMap& map,
        const game::SectorAuthoringStructuralPrimitive& primitive)
{
    std::vector<game::SectorCompiledStructuralPrimitive> compiled;
    std::vector<game::SectorStructuralDiagnostic> diagnostics;
    if (!game::CompileSectorStructuralPrimitives(
                {primitive}, map, compiled, diagnostics)) {
        for (const game::SectorStructuralDiagnostic& diagnostic : diagnostics) {
            std::fprintf(stderr, "structural fixture diagnostic: %s\n",
                    diagnostic.message.c_str());
        }
        Check(false, "structural collision fixture compiles");
        return false;
    }
    map.compiledStructuralPrimitives = std::move(compiled);
    return true;
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
        float radius = 0.25f,
        bool constrainGroundedDropsToStepHeight = false)
{
    return world.ResolveMovement(
            game::SectorCollisionMoveState{position, feetY, sectorId, grounded},
            delta,
            game::SectorCollisionMoveConfig{
                    radius,
                    playerHeight,
                    stepHeight,
                    4,
                    constrainGroundedDropsToStepHeight});
}

Vector2 StructuralLocalPoint(
        const game::SectorAuthoringStructuralPrimitive& primitive,
        float localX,
        float localZ)
{
    const Vector2 center = game::SectorCoordToWorldPosition2(
            primitive.x, primitive.z);
    const float radians = primitive.yawDegrees
            * 3.14159265358979323846f / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    return Vector2{
            center.x + cosine * localX - sine * localZ,
            center.y + sine * localX + cosine * localZ};
}

void CheckContinuousStructuralTraversal(
        const game::SectorAuthoringStructuralPrimitive& primitive,
        const char* label)
{
    SectorTopologyMap map = MakeSquare();
    AddStructuralPrimitive(map, primitive);
    const game::SectorCollisionWorld world = BuildWorld(map);
    const float run = game::SectorCoordToWorldDistance(
            primitive.kind == game::SectorStructuralPrimitiveKind::Ramp
                    ? primitive.ramp.run
                    : primitive.stairs.run);
    const float low = game::SectorAuthoringToWorldDistance(
            primitive.kind == game::SectorStructuralPrimitiveKind::Ramp
                    ? primitive.ramp.low
                    : primitive.stairs.bottom);
    const float high = game::SectorAuthoringToWorldDistance(
            primitive.kind == game::SectorStructuralPrimitiveKind::Ramp
                    ? primitive.ramp.high
                    : primitive.stairs.bottom + primitive.stairs.rise);
    const float radians = primitive.yawDegrees
            * 3.14159265358979323846f / 180.0f;
    const Vector2 ascent{-std::sin(radians), std::cos(radians)};
    constexpr float movementPerFrame = 0.05f;
    const int frameCount = static_cast<int>(
            std::ceil((run + 0.1f) / movementPerFrame));
    Vector2 position = StructuralLocalPoint(
            primitive, 0.0f, -run * 0.5f - 0.2f);
    float feetY = low;
    bool traversedContinuously = false;
    bool movementBlocked = false;
    bool badTransition = false;

    const auto advance = [&](Vector2 delta) {
        const game::SectorCollisionMoveResult move = world.ResolveMovement(
                game::SectorCollisionMoveState{position, feetY, 10, true},
                delta,
                game::SectorCollisionMoveConfig{0.25f, 1.6f, 0.25f, 4});
        movementBlocked = movementBlocked || move.hitWall || move.blockedByStep;
        position = move.positionXZ;
        game::SectorCollisionHeights heights;
        if (!world.ResolveActorVerticalContext(
                    move.currentSectorId,
                    game::SectorCollisionVerticalQuery{
                            position, feetY, 0.25f, 1.6f, 0.25f, true},
                    &heights)) {
            movementBlocked = true;
            return;
        }
        game::SectorFpsControllerState fps;
        fps.feetPosition = {position.x, feetY, position.y};
        fps.currentSectorId = move.currentSectorId;
        fps.grounded = true;
        const game::SectorFpsVerticalResult vertical =
                game::UpdateSectorFpsVerticalPhysics(
                        fps,
                        game::SectorFpsControllerConfig{},
                        game::SectorFpsVerticalContext{
                                true,
                                heights.floorZ,
                                heights.ceilingZ,
                                heights.continuousFloor},
                        0.0f);
        if (heights.continuousFloor) {
            traversedContinuously = true;
            badTransition = badTransition
                    || vertical.transition
                            != game::SectorFpsVerticalTransition::StayedGrounded;
        }
        feetY = fps.feetPosition.y;
    };

    for (int frame = 0; frame < frameCount; ++frame) {
        advance({ascent.x * movementPerFrame, ascent.y * movementPerFrame});
    }
    const bool reachedHigh = feetY > high - 0.05f;
    for (int frame = 0; frame < frameCount - 4; ++frame) {
        advance({-ascent.x * movementPerFrame, -ascent.y * movementPerFrame});
    }
    const bool reachedLow = feetY < low + 0.05f;
    Check(!movementBlocked,
            (std::string(label) + " traverses without structural blocking").c_str());
    Check(traversedContinuously && !badTransition,
            (std::string(label) + " uses stable continuous-floor transitions").c_str());
    Check(reachedHigh && reachedLow,
            (std::string(label) + " follows support up and down").c_str());
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
    result = Move(world, start, Vector2{2.0f, 0.0f}, 10, true, 0.0f, 0.25f, 1.0f);
    Check(result.currentSectorId == 20 && !result.blockedByCeiling,
          "crouched-height collider passes through the same low portal");
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
    map.resolvedMaterialsById.emplace("bars", SectorMaterialDefinition{
            "bars", "textures/bars.png", SectorMaterialFilter::Point});
    map.sideDefs[1].middle.materialId = "bars";

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

void TestBlockedPortalBeyondShallowLandingDoesNotJitter()
{
    constexpr float landingWidth = 2.0f;
    const game::SectorCollisionWorld world =
            BuildWorld(MakeRoomLandingAndBlockedPortal(landingWidth, 0.0f, 2.0f, 2.0f));
    const float blockedPortalX =
            game::SectorCoordToWorldPosition2(Coord(64.0f + landingWidth), Coord(0)).x;
    const float centerZ = game::SectorCoordToWorldPosition2(Coord(0), Coord(32)).y;
    const float radius = 0.5f;
    const Vector2 frameDelta{9.0f / 120.0f, 0.0f};
    const game::SectorCollisionMoveConfig moveConfig{radius, 1.75f, 0.4f, 4};

    game::SectorFpsControllerConfig fpsConfig;
    fpsConfig.playerRadius = radius;
    fpsConfig.playerHeight = moveConfig.playerHeight;
    fpsConfig.stepHeight = moveConfig.stepHeight;
    game::SectorFpsControllerState fpsState;
    fpsState.feetPosition = Vector3{blockedPortalX - 1.0f, 0.0f, centerZ};
    fpsState.currentSectorId = 10;
    fpsState.grounded = true;

    bool reachedContact = false;
    Vector2 settledPosition{};
    int verticalStepTransitions = 0;
    for (int frame = 0; frame < 240; ++frame) {
        const Vector2 frameStart{fpsState.feetPosition.x, fpsState.feetPosition.z};
        const game::SectorCollisionMoveResult moveResult = world.ResolveMovement(
                game::SectorCollisionMoveState{
                        frameStart,
                        fpsState.feetPosition.y,
                        fpsState.currentSectorId,
                        fpsState.grounded},
                frameDelta,
                moveConfig);
        fpsState.feetPosition.x = moveResult.positionXZ.x;
        fpsState.feetPosition.z = moveResult.positionXZ.y;
        fpsState.currentSectorId = moveResult.currentSectorId;

        game::SectorCollisionHeights heights;
        Check(world.GetSectorFloorCeiling(fpsState.currentSectorId, &heights),
              "shallow landing movement sector heights are available");
        const game::SectorFpsVerticalResult verticalResult =
                game::UpdateSectorFpsVerticalPhysics(
                        fpsState,
                        fpsConfig,
                        game::SectorFpsVerticalContext{true, heights.floorZ, heights.ceilingZ},
                        1.0f / 120.0f);
        if (verticalResult.transition == game::SectorFpsVerticalTransition::SteppedUp
                || verticalResult.transition == game::SectorFpsVerticalTransition::SnappedDown) {
            ++verticalStepTransitions;
        }

        if (!moveResult.hitWall) {
            continue;
        }
        if (!reachedContact) {
            reachedContact = true;
            settledPosition = moveResult.positionXZ;
        } else {
            Check(Near(moveResult.positionXZ.x, settledPosition.x)
                          && Near(moveResult.positionXZ.y, settledPosition.y),
                  "held movement into shallow-landing blocked portal remains settled");
        }
        Check(moveResult.currentSectorId == 10,
              "shallow-landing blocked portal keeps the room as center sector");
        Check(Near(fpsState.feetPosition.y, 0.0f),
              "shallow-landing blocked portal keeps feet on the room floor");
    }

    Check(reachedContact, "shallow-landing blocked portal is reached");
    Check(Near(settledPosition.x, blockedPortalX - radius, 0.001f),
          "shallow-landing blocked portal settles at player-radius clearance");
    Check(verticalStepTransitions == 0,
          "shallow-landing blocked portal causes no step-up or snap-down oscillation");

    const Vector2 diagonalStart{blockedPortalX - 1.0f, centerZ - 1.0f};
    game::SectorCollisionMoveResult diagonalResult;
    Vector2 diagonalPosition = diagonalStart;
    int diagonalSectorId = 10;
    bool diagonalHit = false;
    for (int frame = 0; frame < 80; ++frame) {
        diagonalResult = world.ResolveMovement(
                game::SectorCollisionMoveState{diagonalPosition, 0.0f, diagonalSectorId, true},
                Vector2{frameDelta.x, 0.02f},
                moveConfig);
        diagonalPosition = diagonalResult.positionXZ;
        diagonalSectorId = diagonalResult.currentSectorId;
        diagonalHit = diagonalHit || diagonalResult.hitWall;
    }
    Check(diagonalHit, "diagonal shallow-landing approach reaches blocked portal");
    Check(diagonalPosition.x <= blockedPortalX - radius + 0.001f,
          "diagonal shallow-landing contact preserves normal clearance");
    Check(diagonalPosition.y > diagonalStart.y + 1.0f,
          "diagonal shallow-landing contact preserves tangential slide");
}

void TestBlockedPortalBeyondWideLandingPreservesStepUp()
{
    constexpr float landingWidth = 8.0f;
    const game::SectorCollisionWorld world =
            BuildWorld(MakeRoomLandingAndBlockedPortal(landingWidth, 0.0f, 2.0f, 2.0f));
    const float blockedPortalX =
            game::SectorCoordToWorldPosition2(Coord(64.0f + landingWidth), Coord(0)).x;
    const float centerZ = game::SectorCoordToWorldPosition2(Coord(0), Coord(32)).y;
    const float radius = 0.5f;
    const game::SectorCollisionMoveConfig moveConfig{radius, 1.75f, 0.4f, 4};

    game::SectorFpsControllerConfig fpsConfig;
    fpsConfig.playerRadius = radius;
    fpsConfig.playerHeight = moveConfig.playerHeight;
    fpsConfig.stepHeight = moveConfig.stepHeight;
    game::SectorFpsControllerState fpsState;
    fpsState.feetPosition = Vector3{blockedPortalX - 1.5f, 0.0f, centerZ};
    fpsState.currentSectorId = 10;
    fpsState.grounded = true;

    bool steppedUp = false;
    bool reachedContact = false;
    Vector2 settledPosition{};
    for (int frame = 0; frame < 240; ++frame) {
        const Vector2 frameStart{fpsState.feetPosition.x, fpsState.feetPosition.z};
        const game::SectorCollisionMoveResult moveResult = world.ResolveMovement(
                game::SectorCollisionMoveState{
                        frameStart,
                        fpsState.feetPosition.y,
                        fpsState.currentSectorId,
                        fpsState.grounded},
                Vector2{9.0f / 120.0f, 0.0f},
                moveConfig);
        fpsState.feetPosition.x = moveResult.positionXZ.x;
        fpsState.feetPosition.z = moveResult.positionXZ.y;
        fpsState.currentSectorId = moveResult.currentSectorId;

        game::SectorCollisionHeights heights;
        Check(world.GetSectorFloorCeiling(fpsState.currentSectorId, &heights),
              "wide landing movement sector heights are available");
        const game::SectorFpsVerticalResult verticalResult =
                game::UpdateSectorFpsVerticalPhysics(
                        fpsState,
                        fpsConfig,
                        game::SectorFpsVerticalContext{true, heights.floorZ, heights.ceilingZ},
                        1.0f / 120.0f);
        steppedUp = steppedUp
                || verticalResult.transition == game::SectorFpsVerticalTransition::SteppedUp;

        if (!moveResult.hitWall) {
            continue;
        }
        if (!reachedContact) {
            reachedContact = true;
            settledPosition = moveResult.positionXZ;
        } else {
            Check(Near(moveResult.positionXZ.x, settledPosition.x)
                          && Near(moveResult.positionXZ.y, settledPosition.y),
                  "held movement on wide landing remains settled at blocked portal");
        }
    }

    Check(steppedUp, "wide landing still performs normal automatic step-up");
    Check(reachedContact, "wide landing blocked portal is reached");
    Check(fpsState.currentSectorId == 20,
          "wide landing contact keeps player in raised landing sector");
    Check(Near(fpsState.feetPosition.y, game::SectorAuthoringToWorldDistance(2.0f)),
          "wide landing contact keeps raised floor height");
    Check(Near(settledPosition.x, blockedPortalX - radius, 0.001f),
          "wide landing blocked portal settles at player-radius clearance");
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

game::SectorFpsVerticalResult UpdateVerticalForMoveResult(
        const game::SectorCollisionWorld& world,
        const game::SectorCollisionMoveResult& moveResult,
        float feetY)
{
    game::SectorCollisionHeights heights;
    Check(world.GetSectorFloorCeiling(moveResult.currentSectorId, &heights),
          "move result sector heights are available");
    game::SectorFpsControllerState fpsState;
    fpsState.feetPosition = Vector3{moveResult.positionXZ.x, feetY, moveResult.positionXZ.y};
    fpsState.currentSectorId = moveResult.currentSectorId;
    fpsState.grounded = true;
    return game::UpdateSectorFpsVerticalPhysics(
            fpsState,
            game::SectorFpsControllerConfig{},
            game::SectorFpsVerticalContext{true, heights.floorZ, heights.ceilingZ},
            0.0f);
}

void TestDownwardPortalFootprintCommit()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).y;
    constexpr float startInset = 0.01f;
    const float feetY = game::SectorAuthoringToWorldDistance(4.0f);

    for (float radius : {0.5f, 1.5f}) {
        const Vector2 start{portalX - startInset, z};
        const Vector2 beforeClearanceDelta{radius * 0.75f, 0.0f};
        const Vector2 beforeClearanceExpected{
                start.x + beforeClearanceDelta.x,
                start.y + beforeClearanceDelta.y};
        game::SectorCollisionMoveResult moveResult = Move(
                world,
                start,
                beforeClearanceDelta,
                10,
                true,
                feetY,
                0.25f,
                1.6f,
                radius);

        Check(moveResult.currentSectorId == 10,
              "large downward portal waits for radius clearance before sector commit");
        Check(Near(moveResult.positionXZ.x, beforeClearanceExpected.x)
                      && Near(moveResult.positionXZ.y, beforeClearanceExpected.y),
              "large downward portal before clearance preserves requested horizontal movement");
        game::SectorFpsVerticalResult verticalResult =
                UpdateVerticalForMoveResult(world, moveResult, feetY);
        Check(verticalResult.transition == game::SectorFpsVerticalTransition::StayedGrounded,
              "large downward portal before clearance stays supported by upper sector");

        const Vector2 afterClearanceDelta{radius + 0.02f, 0.0f};
        const Vector2 afterClearanceExpected{
                start.x + afterClearanceDelta.x,
                start.y + afterClearanceDelta.y};
        moveResult = Move(
                world,
                start,
                afterClearanceDelta,
                10,
                true,
                feetY,
                0.25f,
                1.6f,
                radius);
        Check(moveResult.currentSectorId == 20,
              "large downward portal commits after radius clearance");
        Check(Near(moveResult.positionXZ.x, afterClearanceExpected.x)
                      && Near(moveResult.positionXZ.y, afterClearanceExpected.y),
              "large downward portal after clearance preserves requested horizontal movement");
        verticalResult = UpdateVerticalForMoveResult(world, moveResult, feetY);
        Check(verticalResult.transition == game::SectorFpsVerticalTransition::StartedDrop,
              "large downward portal after clearance starts falling");
    }
}

void TestStepDownPortalFootprintCommit()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(2.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).y;
    const float feetY = game::SectorAuthoringToWorldDistance(2.0f);
    const float radius = 0.5f;
    const Vector2 start{portalX - 0.01f, z};

    game::SectorCollisionMoveResult moveResult = Move(
            world,
            start,
            Vector2{radius * 0.75f, 0.0f},
            10,
            true,
            feetY,
            0.25f,
            1.6f,
            radius);
    Check(moveResult.currentSectorId == 10,
          "ordinary step-down retains upper support while the footprint overlaps it");
    game::SectorFpsVerticalResult verticalResult =
            UpdateVerticalForMoveResult(world, moveResult, feetY);
    Check(verticalResult.transition == game::SectorFpsVerticalTransition::StayedGrounded,
          "ordinary step-down does not snap while the upper tread still supports the footprint");

    moveResult = Move(
            world,
            start,
            Vector2{radius + 0.02f, 0.0f},
            10,
            true,
            feetY,
            0.25f,
            1.6f,
            radius);
    Check(moveResult.currentSectorId == 20,
          "ordinary step-down commits after the footprint clears the upper tread");
    verticalResult = UpdateVerticalForMoveResult(world, moveResult, feetY);
    Check(verticalResult.transition == game::SectorFpsVerticalTransition::SnappedDown,
          "ordinary step-down snaps to the lower tread after support clearance");
}

void TestStepDownFootprintSupportAtPortalCorner()
{
    const game::SectorCollisionWorld world = BuildWorld(
            MakeAdjacentDestinationExtendsPastPortal(2.0f, 0.0f));
    const Vector2 portalEnd =
            game::SectorCoordToWorldPosition2(Coord(64), Coord(64));
    const float feetY = game::SectorAuthoringToWorldDistance(2.0f);
    const float radius = 0.5f;
    const game::SectorCollisionMoveConfig config{radius, 1.6f, 0.25f, 4};

    int sectorId = world.FindSectorForPlayerFootprint(
            Vector2{portalEnd.x + 0.2f, portalEnd.y + 0.2f},
            10,
            feetY,
            true,
            config);
    Check(sectorId == 10,
          "diagonal step-down retains support while the footprint overlaps the tread corner");

    sectorId = world.FindSectorForPlayerFootprint(
            Vector2{portalEnd.x + 0.4f, portalEnd.y + 0.4f},
            10,
            feetY,
            true,
            config);
    Check(sectorId == 20,
          "diagonal step-down releases support after clearing the tread corner");
}

void TestDownwardPortalOffAxisFootprintCommit()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(28), Coord(28)).y;
    const float feetY = game::SectorAuthoringToWorldDistance(4.0f);
    const float radius = 0.5f;
    const Vector2 start{portalX - 0.01f, z};

    Vector2 delta{radius * 0.75f, 0.35f};
    Vector2 expected{start.x + delta.x, start.y + delta.y};
    game::SectorCollisionMoveResult moveResult = Move(
            world,
            start,
            delta,
            10,
            true,
            feetY,
            0.25f,
            1.6f,
            radius);
    Check(moveResult.currentSectorId == 10,
          "off-axis downward portal waits for radius clearance before sector commit");
    Check(Near(moveResult.positionXZ.x, expected.x)
                  && Near(moveResult.positionXZ.y, expected.y),
          "off-axis downward portal before clearance has no destination snap");

    delta = Vector2{radius + 0.02f, 0.35f};
    expected = Vector2{start.x + delta.x, start.y + delta.y};
    moveResult = Move(
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
          "off-axis downward portal commits after radius clearance");
    Check(Near(moveResult.positionXZ.x, expected.x)
                  && Near(moveResult.positionXZ.y, expected.y),
          "off-axis downward portal after clearance has no destination snap");
    const game::SectorFpsVerticalResult verticalResult =
            UpdateVerticalForMoveResult(world, moveResult, feetY);
    Check(verticalResult.transition == game::SectorFpsVerticalTransition::StartedDrop,
          "off-axis downward portal after clearance starts falling");
}

void TestDownwardPortalFootprintLookupWaitsAfterBarelyCrossing()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).y;
    const float feetY = game::SectorAuthoringToWorldDistance(4.0f);
    const float radius = 1.5f;
    const game::SectorCollisionMoveConfig config{radius, 1.6f, 0.25f, 4};

    int sectorId = world.FindSectorForPlayerFootprint(
            Vector2{portalX + 0.01f, z},
            10,
            feetY,
            true,
            config);
    Check(sectorId == 10,
          "footprint lookup keeps upper sector after barely crossed downward portal");

    sectorId = world.FindSectorForPlayerFootprint(
            Vector2{portalX + radius + 0.01f, z},
            10,
            feetY,
            true,
            config);
    Check(sectorId == 20,
          "footprint lookup commits lower sector after radius clearance");
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

void TestFeetHeightControlsReverseStepBlocking()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).y;
    const float radius = 1.5f;
    const Vector2 start{portalX + radius + 0.1f, z};
    game::SectorCollisionMoveResult result = Move(
            world,
            start,
            Vector2{-0.75f, 0.0f},
            20,
            true,
            0.0f,
            0.25f,
            1.6f,
            radius);

    Check(result.currentSectorId == 20 && result.blockedByStep,
          "movement from the lower floor into a too-high portal remains blocked");
    Check(result.positionXZ.x >= portalX + radius - 0.001f,
          "too-high portal keeps radius clearance for feet on the lower floor");

    result = Move(
            world,
            start,
            Vector2{-0.75f, 0.0f},
            20,
            true,
            game::SectorAuthoringToWorldDistance(4.0f),
            0.25f,
            1.6f,
            radius);
    Check(result.currentSectorId == 20 && !result.blockedByStep,
          "feet retained at the upper stair height can approach that stair from the lower sector");
    Check(result.positionXZ.x < start.x - 0.7f,
          "retained stair-height feet are not pushed away by the lower sector id");
}

void TestGroundedDropConstraint()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeAdjacent(4.0f, 0.0f));
    const float portalX = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).x;
    const float z = game::SectorCoordToWorldPosition2(Coord(64), Coord(32)).y;
    const float radius = 0.5f;
    const Vector2 start{portalX - radius - 0.1f, z};
    const game::SectorCollisionMoveResult result = Move(
            world,
            start,
            Vector2{0.75f, 0.0f},
            10,
            true,
            game::SectorAuthoringToWorldDistance(4.0f),
            0.25f,
            1.6f,
            radius,
            true);

    Check(result.currentSectorId == 10 && result.blockedByDrop,
          "navigation-style grounded movement blocks drops above step height");
    Check(!result.blockedByStep && result.positionXZ.x <= portalX - radius + 0.001f,
          "blocked drop reports its own reason and preserves ledge clearance");
}

void TestStructuralPrimitiveCollision()
{
    game::SectorAuthoringStructuralPrimitive box =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Box);
    box.id = 1;
    box.x = Coord(32.0f);
    box.z = Coord(32.0f);
    box.box.width = Coord(16.0f);
    box.box.depth = Coord(16.0f);
    box.box.bottom = 0.0f;
    box.box.top = 8.0f;

    SectorTopologyMap boxMap = MakeSquare();
    AddStructuralPrimitive(boxMap, box);
    game::SectorCollisionWorld boxWorld = BuildWorld(boxMap);
    game::SectorCollisionMoveResult blocked = Move(
            boxWorld, {1.5f, 4.0f}, {4.0f, 0.0f}, 10, true);
    Check(blocked.hitWall && blocked.blockedByStep
                  && blocked.positionXZ.x <= 2.751f,
          "colliding box blocks and preserves the player radius");

    game::SectorAuthoringStructuralPrimitive ladder =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Ladder);
    ladder.id = 2;
    ladder.x = Coord(32.0f);
    ladder.z = Coord(32.0f);
    SectorTopologyMap ladderMap = MakeSquare();
    AddStructuralPrimitive(ladderMap, ladder);
    const game::SectorCollisionMoveResult ladderBlocked = Move(
            BuildWorld(ladderMap), {4.0f, 3.0f}, {0.0f, 2.0f}, 10, true);
    Check(ladderBlocked.hitWall && ladderBlocked.blockedByStep
                  && ladderBlocked.positionXZ.y < 3.8f,
          "ladder collision uses a smooth slab that blocks passage through rung gaps");

    ladder.collision = false;
    SectorTopologyMap disabledLadderMap = MakeSquare();
    AddStructuralPrimitive(disabledLadderMap, ladder);
    const game::SectorCollisionMoveResult ladderPassThrough = Move(
            BuildWorld(disabledLadderMap),
            {4.0f, 3.0f}, {0.0f, 2.0f}, 10, true);
    Check(ladderPassThrough.positionXZ.y > 4.5f
                  && !ladderPassThrough.hitWall,
          "collision-disabled ladders do not block movement");

    box.collision = false;
    SectorTopologyMap disabledBoxMap = MakeSquare();
    AddStructuralPrimitive(disabledBoxMap, box);
    const game::SectorCollisionMoveResult passThrough = Move(
            BuildWorld(disabledBoxMap),
            {1.5f, 4.0f}, {4.0f, 0.0f}, 10, true);
    Check(passThrough.positionXZ.x > 5.0f && !passThrough.hitWall,
          "collision-disabled box does not enter movement queries");

    box.collision = true;
    box.box.top = 2.0f;
    SectorTopologyMap slabMap = MakeSquare();
    AddStructuralPrimitive(slabMap, box);
    const game::SectorCollisionWorld slabWorld = BuildWorld(slabMap);
    const game::SectorCollisionMoveResult stepped = Move(
            slabWorld, {2.5f, 4.0f}, {1.0f, 0.0f}, 10, true);
    game::SectorCollisionHeights slabHeights;
    Check(stepped.positionXZ.x > 3.0f
                  && slabWorld.ResolveActorVerticalContext(
                          10,
                          game::SectorCollisionVerticalQuery{
                                  stepped.positionXZ,
                                  0.0f,
                                  0.25f,
                                  1.6f,
                                  0.25f,
                                  true},
                          &slabHeights)
                  && Near(slabHeights.floorZ, 0.25f),
          "reachable box top becomes structural support");

    box.box.bottom = 8.0f;
    box.box.top = 16.0f;
    SectorTopologyMap bridgeMap = MakeSquare();
    AddStructuralPrimitive(bridgeMap, box);
    const game::SectorCollisionMoveResult beneath = Move(
            BuildWorld(bridgeMap),
            {1.5f, 4.0f}, {4.0f, 0.0f}, 10, true,
            0.0f, 0.25f, 0.8f);
    Check(beneath.positionXZ.x > 5.0f && !beneath.hitWall,
          "actor with enough headroom can move beneath a raised box");

    game::SectorAuthoringStructuralPrimitive edgeBox =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Box);
    edgeBox.id = 10;
    edgeBox.x = Coord(32.0f);
    edgeBox.z = Coord(32.0f);
    edgeBox.yawDegrees = 332.1540832519531f;
    edgeBox.box.width = Coord(32.0f);
    edgeBox.box.depth = Coord(24.0f);
    edgeBox.box.bottom = 8.0f;
    edgeBox.box.top = 10.0f;
    SectorTopologyMap edgeBoxMap = MakeSquare(0.0f, 44.0f);
    AddStructuralPrimitive(edgeBoxMap, edgeBox);
    const game::SectorCollisionWorld edgeBoxWorld = BuildWorld(edgeBoxMap);
    const float edgeBoxTop = game::SectorAuthoringToWorldDistance(
            edgeBox.box.top);
    const float edgeBoxHalfDepth = game::SectorCoordToWorldDistance(
            edgeBox.box.depth) * 0.5f;
    game::SectorCollisionHeights retainedBoxHeights;
    const Vector2 retainedBoxPosition = StructuralLocalPoint(
            edgeBox, 0.0f, -edgeBoxHalfDepth - 0.05f);
    Check(edgeBoxWorld.ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          retainedBoxPosition,
                          edgeBoxTop,
                          0.25f,
                          1.8f,
                          0.25f,
                          true},
                  &retainedBoxHeights)
                  && Near(retainedBoxHeights.floorZ, edgeBoxTop)
                  && Near(retainedBoxHeights.ceilingZ, 5.5f),
          "raised box retains top support while the player radius overlaps its edge");
    game::SectorFpsControllerState retainedBoxState;
    retainedBoxState.feetPosition = {
            retainedBoxPosition.x, edgeBoxTop, retainedBoxPosition.y};
    retainedBoxState.currentSectorId = 10;
    retainedBoxState.grounded = true;
    game::SectorFpsControllerConfig tallPlayerConfig;
    tallPlayerConfig.playerHeight = 1.8f;
    const game::SectorFpsVerticalResult retainedBoxVertical =
            game::UpdateSectorFpsVerticalPhysics(
                    retainedBoxState,
                    tallPlayerConfig,
                    game::SectorFpsVerticalContext{
                            true,
                            retainedBoxHeights.floorZ,
                            retainedBoxHeights.ceilingZ,
                            retainedBoxHeights.continuousFloor},
                    0.0f);
    Check(!retainedBoxVertical.cannotFit
                  && retainedBoxVertical.transition
                          == game::SectorFpsVerticalTransition::StayedGrounded,
          "retained box support does not enter the underside cannot-fit interval");
    const Vector2 clearedBoxPosition = StructuralLocalPoint(
            edgeBox, 0.0f, -edgeBoxHalfDepth - 0.251f);
    game::SectorCollisionHeights clearedBoxHeights;
    Check(edgeBoxWorld.ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          clearedBoxPosition,
                          edgeBoxTop,
                          0.25f,
                          1.8f,
                          0.25f,
                          true},
                  &clearedBoxHeights)
                  && Near(clearedBoxHeights.floorZ, 0.0f),
          "raised box support ends after the full player radius clears");
    retainedBoxState.feetPosition = {
            clearedBoxPosition.x, edgeBoxTop, clearedBoxPosition.y};
    const game::SectorFpsVerticalResult clearedBoxVertical =
            game::UpdateSectorFpsVerticalPhysics(
                    retainedBoxState,
                    tallPlayerConfig,
                    game::SectorFpsVerticalContext{
                            true,
                            clearedBoxHeights.floorZ,
                            clearedBoxHeights.ceilingZ,
                            clearedBoxHeights.continuousFloor},
                    0.0f);
    Check(!clearedBoxVertical.cannotFit
                  && clearedBoxVertical.transition
                          == game::SectorFpsVerticalTransition::StartedDrop,
          "clearing a raised box edge starts a normal unobstructed drop");
    Vector2 edgeWalkPosition = StructuralLocalPoint(
            edgeBox, 0.0f, -edgeBoxHalfDepth + 0.02f);
    const Vector2 edgeWalkStepStart = StructuralLocalPoint(
            edgeBox, 0.0f, 0.0f);
    const Vector2 edgeWalkStepEnd = StructuralLocalPoint(
            edgeBox, 0.0f, -0.05f);
    const Vector2 edgeWalkDelta{
            edgeWalkStepEnd.x - edgeWalkStepStart.x,
            edgeWalkStepEnd.y - edgeWalkStepStart.y};
    float edgeWalkFeetY = edgeBoxTop;
    bool edgeWalkGrounded = true;
    bool retainedPastCenter = false;
    bool droppedAfterClearance = false;
    bool edgeWalkFailed = false;
    for (int frame = 0; frame < 7; ++frame) {
        const game::SectorCollisionMoveResult move = edgeBoxWorld.ResolveMovement(
                game::SectorCollisionMoveState{
                        edgeWalkPosition, edgeWalkFeetY, 10, edgeWalkGrounded},
                edgeWalkDelta,
                game::SectorCollisionMoveConfig{0.25f, 1.8f, 0.25f, 4});
        edgeWalkFailed = edgeWalkFailed || move.hitWall || move.blockedByStep;
        edgeWalkPosition = move.positionXZ;
        game::SectorCollisionHeights heights;
        if (!edgeBoxWorld.ResolveActorVerticalContext(
                    10,
                    game::SectorCollisionVerticalQuery{
                            edgeWalkPosition,
                            edgeWalkFeetY,
                            0.25f,
                            1.8f,
                            0.25f,
                            edgeWalkGrounded},
                    &heights)) {
            edgeWalkFailed = true;
            break;
        }
        game::SectorFpsControllerState fps;
        fps.feetPosition = {
                edgeWalkPosition.x, edgeWalkFeetY, edgeWalkPosition.y};
        fps.currentSectorId = 10;
        fps.grounded = edgeWalkGrounded;
        const game::SectorFpsVerticalResult vertical =
                game::UpdateSectorFpsVerticalPhysics(
                        fps,
                        tallPlayerConfig,
                        game::SectorFpsVerticalContext{
                                true,
                                heights.floorZ,
                                heights.ceilingZ,
                                heights.continuousFloor},
                        0.0f);
        edgeWalkFailed = edgeWalkFailed || vertical.cannotFit;
        if (frame > 0 && frame < 5) {
            retainedPastCenter = retainedPastCenter
                    || vertical.transition
                            == game::SectorFpsVerticalTransition::StayedGrounded;
        }
        if (vertical.transition == game::SectorFpsVerticalTransition::StartedDrop) {
            droppedAfterClearance = frame >= 5;
            break;
        }
        edgeWalkFeetY = fps.feetPosition.y;
        edgeWalkGrounded = fps.grounded;
    }
    Check(!edgeWalkFailed && retainedPastCenter && droppedAfterClearance,
          "slow box-edge movement retains support until clear, then drops safely");

    game::SectorAuthoringStructuralPrimitive ramp =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Ramp);
    ramp.id = 2;
    ramp.x = Coord(32.0f);
    ramp.z = Coord(32.0f);
    ramp.ramp.width = Coord(16.0f);
    ramp.ramp.run = Coord(32.0f);
    ramp.ramp.solidBottom = 0.0f;
    ramp.ramp.low = 0.0f;
    ramp.ramp.high = 8.0f;
    SectorTopologyMap rampMap = MakeSquare();
    AddStructuralPrimitive(rampMap, ramp);
    const game::SectorCollisionWorld rampWorld = BuildWorld(rampMap);
    const game::SectorCollisionMoveResult lowEntry = Move(
            rampWorld, {4.0f, 1.5f}, {0.0f, 0.6f}, 10, true);
    Check(lowEntry.positionXZ.y > 2.0f && !lowEntry.hitWall,
          "ramp low edge permits reachable entry");
    const game::SectorCollisionMoveResult sideBlocked = Move(
            rampWorld, {2.5f, 2.1f}, {0.6f, 0.0f}, 10, true);
    Check(sideBlocked.hitWall && sideBlocked.positionXZ.x <= 2.751f,
          "ramp side remains solid at its low end");
    const game::SectorCollisionMoveResult highBlocked = Move(
            rampWorld, {4.0f, 6.8f}, {0.0f, -1.0f}, 10, true);
    Check(highBlocked.hitWall && highBlocked.blockedByStep
                  && highBlocked.positionXZ.y >= 6.249f,
          "ramp high edge blocks an unreachable approach");
    game::SectorCollisionHeights rampHeights;
    Check(rampWorld.ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          {4.0f, 4.0f},
                          0.25f,
                          0.25f,
                          1.6f,
                          0.25f,
                          true},
                  &rampHeights)
                  && Near(rampHeights.floorZ, 0.5f)
                  && rampHeights.continuousFloor,
          "ramp exposes its inclined support height");
    game::SectorCollisionHeights rampSideHeights;
    Check(rampWorld.ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          {3.249f, 4.0f},
                          0.0f,
                          0.25f,
                          1.6f,
                          0.25f,
                          true},
                  &rampSideHeights)
                  && Near(rampSideHeights.floorZ, 0.0f)
                  && !rampSideHeights.continuousFloor,
          "ramp side radius overlap does not become floor support");
    game::SectorAuthoringStructuralPrimitive yawedRamp = ramp;
    yawedRamp.id = 20;
    yawedRamp.yawDegrees = 27.0f;
    CheckContinuousStructuralTraversal(yawedRamp, "yawed ramp");

    game::SectorAuthoringStructuralPrimitive stairs =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Stairs);
    stairs.id = 3;
    stairs.x = Coord(32.0f);
    stairs.z = Coord(32.0f);
    stairs.stairs.width = Coord(16.0f);
    stairs.stairs.run = Coord(32.0f);
    stairs.stairs.bottom = 0.0f;
    stairs.stairs.rise = 8.0f;
    SectorTopologyMap stairMap = MakeSquare();
    AddStructuralPrimitive(stairMap, stairs);
    const game::SectorCollisionWorld stairWorld = BuildWorld(stairMap);
    game::SectorCollisionHeights stairHeights;
    Check(stairWorld.ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          {4.0f, 4.0f},
                          0.25f,
                          0.25f,
                          1.6f,
                          0.25f,
                          true},
                  &stairHeights)
                  && Near(stairHeights.floorZ, 0.5f)
                  && stairHeights.continuousFloor,
          "stairs expose the smooth support ramp rather than tread contacts");
    const game::SectorCollisionMoveResult stairSide = Move(
            stairWorld, {2.5f, 3.0f}, {0.6f, 0.0f}, 10, true,
            0.25f);
    Check(stairSide.hitWall && stairSide.positionXZ.x <= 2.751f,
          "stair side is a continuous solid boundary");
    game::SectorAuthoringStructuralPrimitive yawedStairs = stairs;
    yawedStairs.id = 21;
    yawedStairs.yawDegrees = 317.0f;
    CheckContinuousStructuralTraversal(yawedStairs, "yawed stairs");
    SectorTopologyMap stairEdgeMap = MakeSquare();
    AddStructuralPrimitive(stairEdgeMap, yawedStairs);
    const game::SectorCollisionWorld stairEdgeWorld = BuildWorld(stairEdgeMap);
    const float stairHalfRun = game::SectorCoordToWorldDistance(
            yawedStairs.stairs.run) * 0.5f;
    const float stairTop = game::SectorAuthoringToWorldDistance(
            yawedStairs.stairs.bottom + yawedStairs.stairs.rise);
    game::SectorCollisionHeights retainedStairHeights;
    const Vector2 retainedStairPosition = StructuralLocalPoint(
            yawedStairs, 0.0f, stairHalfRun + 0.014f);
    Check(stairEdgeWorld.ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          retainedStairPosition,
                          stairTop,
                          0.25f,
                          1.6f,
                          0.25f,
                          true},
                  &retainedStairHeights)
                  && Near(retainedStairHeights.floorZ, stairTop)
                  && retainedStairHeights.continuousFloor,
          "staircase high edge retains the last-step support within player radius");
    const Vector2 stairExitStart = StructuralLocalPoint(
            yawedStairs, 0.0f, stairHalfRun - 0.02f);
    const float stairRiseWorld = game::SectorAuthoringToWorldDistance(
            yawedStairs.stairs.rise);
    const float stairRunWorld = game::SectorCoordToWorldDistance(
            yawedStairs.stairs.run);
    const float stairExitFeet = stairTop
            - stairRiseWorld * 0.02f / stairRunWorld;
    const Vector2 stairExitStepStart = StructuralLocalPoint(
            yawedStairs, 0.0f, 0.0f);
    const Vector2 stairExitStepEnd = StructuralLocalPoint(
            yawedStairs, 0.0f, 0.05f);
    const game::SectorCollisionMoveResult stairExitMove =
            stairEdgeWorld.ResolveMovement(
                    game::SectorCollisionMoveState{
                            stairExitStart, stairExitFeet, 10, true},
                    {stairExitStepEnd.x - stairExitStepStart.x,
                            stairExitStepEnd.y - stairExitStepStart.y},
                    game::SectorCollisionMoveConfig{
                            0.25f, 1.6f, 0.25f, 4});
    game::SectorCollisionHeights movedStairEdgeHeights;
    Check(!stairExitMove.hitWall
                  && stairEdgeWorld.ResolveActorVerticalContext(
                          10,
                          game::SectorCollisionVerticalQuery{
                                  stairExitMove.positionXZ,
                                  stairExitFeet,
                                  0.25f,
                                  1.6f,
                                  0.25f,
                                  true},
                          &movedStairEdgeHeights)
                  && Near(movedStairEdgeHeights.floorZ, stairTop)
                  && movedStairEdgeHeights.continuousFloor,
          "staircase edge handoff raises retained support to the final-step height");
    game::SectorFpsControllerState stairExitState;
    stairExitState.feetPosition = {
            stairExitMove.positionXZ.x, stairExitFeet,
            stairExitMove.positionXZ.y};
    stairExitState.currentSectorId = 10;
    stairExitState.grounded = true;
    const game::SectorFpsVerticalResult stairExitVertical =
            game::UpdateSectorFpsVerticalPhysics(
                    stairExitState,
                    game::SectorFpsControllerConfig{},
                    game::SectorFpsVerticalContext{
                            true,
                            movedStairEdgeHeights.floorZ,
                            movedStairEdgeHeights.ceilingZ,
                            movedStairEdgeHeights.continuousFloor},
                    0.0f);
    Check(stairExitVertical.transition
                          == game::SectorFpsVerticalTransition::StayedGrounded
                  && Near(stairExitState.feetPosition.y, stairTop),
          "staircase final-step edge handoff remains grounded without clipping");
    game::SectorCollisionHeights clearedStairHeights;
    const Vector2 clearedStairPosition = StructuralLocalPoint(
            yawedStairs, 0.0f, stairHalfRun + 0.251f);
    Check(stairEdgeWorld.ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          clearedStairPosition,
                          stairTop,
                          0.25f,
                          1.6f,
                          0.25f,
                          true},
                  &clearedStairHeights)
                  && Near(clearedStairHeights.floorZ, 0.0f)
                  && !clearedStairHeights.continuousFloor,
          "staircase high-edge support ends after the full radius clears");

    game::SectorAuthoringStructuralPrimitive cylinder =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Cylinder);
    cylinder.id = 4;
    cylinder.x = Coord(32.0f);
    cylinder.z = Coord(32.0f);
    cylinder.cylinder.radius = Coord(8.0f);
    cylinder.cylinder.bottom = 0.0f;
    cylinder.cylinder.top = 8.0f;
    SectorTopologyMap cylinderMap = MakeSquare();
    AddStructuralPrimitive(cylinderMap, cylinder);
    const game::SectorCollisionMoveResult cylinderBlocked = Move(
            BuildWorld(cylinderMap),
            {1.5f, 4.0f}, {3.0f, 0.0f}, 10, true);
    Check(cylinderBlocked.hitWall && cylinderBlocked.positionXZ.x <= 2.751f,
          "cylinder uses its circular collision footprint");

    game::SectorAuthoringStructuralPrimitive ceilingPipe = cylinder;
    ceilingPipe.id = 7;
    ceilingPipe.pitchDegrees = 90.0f;
    ceilingPipe.cylinder.radius = Coord(4.0f);
    ceilingPipe.cylinder.bottom = 16.0f;
    ceilingPipe.cylinder.top = 32.0f;
    SectorTopologyMap ceilingPipeMap = MakeSquare();
    AddStructuralPrimitive(ceilingPipeMap, ceilingPipe);
    const game::SectorCollisionMoveResult underPipe = Move(
            BuildWorld(ceilingPipeMap),
            {1.5f, 4.0f}, {3.0f, 0.0f}, 10, true,
            0.0f, 0.25f, 1.6f);
    Check(underPipe.positionXZ.x > 4.0f && !underPipe.hitWall,
          "actor with enough headroom can pass beneath a horizontal cylinder");

    game::SectorAuthoringStructuralPrimitive lowPipe = ceilingPipe;
    lowPipe.id = 8;
    lowPipe.cylinder.bottom = 4.0f;
    lowPipe.cylinder.top = 20.0f;
    SectorTopologyMap lowPipeMap = MakeSquare();
    AddStructuralPrimitive(lowPipeMap, lowPipe);
    const game::SectorCollisionMoveResult lowPipeBlocked = Move(
            BuildWorld(lowPipeMap),
            {1.5f, 4.0f}, {3.0f, 0.0f}, 10, true,
            0.0f, 0.25f, 1.6f);
    Check(lowPipeBlocked.hitWall && lowPipeBlocked.blockedByCeiling,
          "low horizontal cylinder uses conservative tilted clearance collision");

    game::SectorAuthoringStructuralPrimitive sphere =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Sphere);
    sphere.id = 5;
    sphere.x = Coord(32.0f);
    sphere.z = Coord(32.0f);
    sphere.sphere.radius = Coord(4.0f);
    sphere.sphere.centerHeight = 4.0f;
    sphere.collision = true;
    SectorTopologyMap sphereMap = MakeSquare();
    AddStructuralPrimitive(sphereMap, sphere);
    const game::SectorCollisionMoveResult sphereBlocked = Move(
            BuildWorld(sphereMap),
            {1.5f, 4.0f}, {3.0f, 0.0f}, 10, true);
    Check(sphereBlocked.hitWall && sphereBlocked.positionXZ.x <= 3.251f,
          "collision-enabled sphere blocks with a spherical approximation");
    const game::SectorCollisionWorld sphereWorld = BuildWorld(sphereMap);
    game::SectorCollisionHeights sphereHeights;
    Check(sphereWorld.ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          {4.0f, 4.0f},
                          1.1f,
                          0.25f,
                          1.6f,
                          0.25f,
                          false},
                  &sphereHeights)
                  && Near(sphereHeights.floorZ, 1.0f)
                  && !sphereHeights.continuousFloor,
          "airborne actor sees the upper sphere as landing support");
    game::SectorFpsControllerState fallingState;
    fallingState.feetPosition = {4.0f, 1.1f, 4.0f};
    fallingState.currentSectorId = 10;
    fallingState.grounded = false;
    fallingState.verticalVelocity = -1.0f;
    const game::SectorFpsVerticalResult sphereLanding =
            game::UpdateSectorFpsVerticalPhysics(
                    fallingState,
                    game::SectorFpsControllerConfig{},
                    game::SectorFpsVerticalContext{
                            true,
                            sphereHeights.floorZ,
                            sphereHeights.ceilingZ,
                            sphereHeights.continuousFloor},
                    0.1f);
    Check(sphereLanding.transition == game::SectorFpsVerticalTransition::Landed
                  && fallingState.grounded
                  && Near(fallingState.feetPosition.y, 1.0f),
          "falling actor lands on a collision-enabled sphere");

    game::SectorAuthoringStructuralPrimitive raisedSphere = sphere;
    raisedSphere.id = 6;
    raisedSphere.sphere.centerHeight = 12.0f;
    SectorTopologyMap raisedSphereMap = MakeSquare();
    AddStructuralPrimitive(raisedSphereMap, raisedSphere);
    game::SectorCollisionHeights undersideHeights;
    Check(BuildWorld(raisedSphereMap).ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          {4.0f, 4.0f},
                          0.0f,
                          0.25f,
                          0.8f,
                          0.25f,
                          true},
                  &undersideHeights)
                  && Near(undersideHeights.ceilingZ, 1.0f),
          "raised sphere lower hemisphere limits headroom");

    sphere.collision = false;
    SectorTopologyMap disabledSphereMap = MakeSquare();
    AddStructuralPrimitive(disabledSphereMap, sphere);
    game::SectorCollisionHeights disabledSphereHeights;
    Check(BuildWorld(disabledSphereMap).ResolveActorVerticalContext(
                  10,
                  game::SectorCollisionVerticalQuery{
                          {4.0f, 4.0f},
                          1.1f,
                          0.25f,
                          1.6f,
                          0.25f,
                          false},
                  &disabledSphereHeights)
                  && Near(disabledSphereHeights.floorZ, 0.0f),
          "collision-disabled sphere provides no landing support");
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

void TestItemDropClearanceQueries()
{
    const game::SectorCollisionWorld world = BuildWorld(MakeSquare());
    const game::ItemDropCandidate clear = game::BuildItemDropCandidate(
            world,
            10,
            Vector3{4.0f, 0.0f, 4.0f},
            game::kItemDropFallbackLocalBounds);
    Check(clear.valid && Near(clear.worldBounds.min.y, 0.0f),
          "drop candidate places its lower bound on the sector floor");
    Check(game::ItemDropFitsTopology(world, clear),
          "drop candidate fits clear topology");
    const float eyeLift = game::ItemDropLiftToCenterAtHeight(clear, 1.2f);
    const game::ItemDropCandidate swept =
            game::BuildLiftedItemDropSweep(clear, eyeLift);
    Check(Near(eyeLift, 0.7f)
                  && Near(swept.worldBounds.min.y, clear.worldBounds.min.y)
                  && Near(swept.worldBounds.max.y, 1.7f),
          "drop presentation centers at eye height and builds a floor-to-eye sweep");
    Check(game::ItemDropFitsTopology(world, swept),
          "drop eye-to-floor sweep fits clear topology");

    const game::ItemDropCandidate wall = game::BuildItemDropCandidate(
            world,
            10,
            Vector3{0.4f, 0.0f, 4.0f},
            game::kItemDropFallbackLocalBounds);
    Check(wall.valid && !game::ItemDropFitsTopology(world, wall),
          "drop candidate overlapping a wall is refused");

    const game::SectorCollisionWorld lowWorld = BuildWorld(MakeSquare(0.0f, 4.0f));
    const game::ItemDropCandidate lowCeiling = game::BuildItemDropCandidate(
            lowWorld,
            10,
            Vector3{4.0f, 0.0f, 4.0f},
            game::kItemDropFallbackLocalBounds);
    Check(lowCeiling.valid && !game::ItemDropFitsTopology(lowWorld, lowCeiling),
          "drop candidate exceeding the ceiling is refused");
    const game::ItemDropCandidate lowSweep =
            game::BuildLiftedItemDropSweep(lowCeiling, 1.0f);
    Check(lowSweep.valid && !game::ItemDropFitsTopology(lowWorld, lowSweep),
          "drop eye-to-floor sweep respects sector ceiling clearance");

    game::SectorStaticModelCollider prop;
    prop.center = Vector2{4.0f, 4.0f};
    prop.axisX = Vector2{1.0f, 0.0f};
    prop.axisZ = Vector2{0.0f, 1.0f};
    prop.halfExtents = Vector2{0.25f, 0.25f};
    prop.bottom = 0.0f;
    prop.top = 1.0f;
    prop.resolved = true;
    Check(game::ItemDropBoundsOverlap(clear.worldBounds, prop),
          "drop bounds detect static or dynamic prop overlap");

    std::vector<game::SectorStaticModelCollider> propColliders;
    Check(!game::ItemDropBoundsOverlapAnyPropCollider(
                  clear.worldBounds, propColliders),
          "drop clearance ignores props without collision components");
    propColliders.push_back(prop);
    Check(game::ItemDropBoundsOverlapAnyPropCollider(
                  clear.worldBounds, propColliders),
          "drop clearance rejects resolved collision-enabled props");

    game::SectorDynamicDoorCollider door;
    door.center = Vector2{4.0f, 4.0f};
    door.tangent = Vector2{1.0f, 0.0f};
    door.normal = Vector2{0.0f, 1.0f};
    door.halfExtents = Vector2{0.5f, 0.1f};
    door.bottom = 0.0f;
    door.top = 2.0f;
    Check(game::ItemDropBoundsOverlap(clear.worldBounds, door),
          "drop bounds detect door overlap");
    Check(game::ItemDropBoundsOverlapPlayer(
                  clear.worldBounds,
                  Vector3{4.0f, 0.0f, 4.0f},
                  0.25f,
                  1.6f),
          "drop bounds detect player overlap");
    Check(!game::ItemDropBoundsOverlapPlayer(
                  clear.worldBounds,
                  Vector3{2.0f, 0.0f, 2.0f},
                  0.25f,
                  1.6f),
          "drop bounds allow a separated player");

    const auto slots = game::BuildItemDropSlotOrigins(
            Vector3{4.0f, 0.0f, 4.0f},
            Vector3{1.0f, 0.0f, 0.0f},
            0.25f,
            game::kItemDropFallbackLocalBounds);
    bool allSlotsInFront = true;
    bool allSlotsDistinct = true;
    for (std::size_t first = 0; first < slots.size(); ++first) {
        allSlotsInFront = allSlotsInFront && slots[first].x > 4.0f;
        for (std::size_t second = first + 1; second < slots.size(); ++second) {
            const float dx = slots[first].x - slots[second].x;
            const float dz = slots[first].z - slots[second].z;
            allSlotsDistinct = allSlotsDistinct
                    && dx * dx + dz * dz > 0.01f;
        }
    }
    Check(allSlotsInFront && allSlotsDistinct,
          "drop placement builds six distinct fan slots in front of the player");

    const float firstYaw = game::BuildItemDropRandomYawRadians(12, 31);
    const float secondYaw = game::BuildItemDropRandomYawRadians(12, 32);
    Check(firstYaw >= 0.0f && firstYaw < 2.0f * PI
                  && secondYaw >= 0.0f && secondYaw < 2.0f * PI
                  && !Near(firstYaw, secondYaw),
          "drop placement produces varied normalized yaw values");

    const BoundingBox rectangularBounds{
            Vector3{-1.0f, 0.0f, -0.25f},
            Vector3{1.0f, 0.5f, 0.25f}};
    const game::ItemDropCandidate rotated = game::BuildItemDropCandidate(
            world,
            10,
            Vector3{4.0f, 0.0f, 4.0f},
            rectangularBounds,
            PI * 0.5f);
    Check(rotated.valid
                  && Near(rotated.yawRadians, PI * 0.5f)
                  && Near(rotated.worldBounds.max.x - rotated.worldBounds.min.x, 0.5f)
                  && Near(rotated.worldBounds.max.z - rotated.worldBounds.min.z, 2.0f),
          "drop clearance rotates rectangular item bounds with the chosen yaw");
}

} // namespace

int main()
{
    TestBlockingWallStopsAndSlides();
    TestPortalStepAndCeilingRules();
    TestBlocksPlayerPortalMovement();
    TestMiddleTexturePortalMovement();
    TestBlockedPortalBeyondShallowLandingDoesNotJitter();
    TestBlockedPortalBeyondWideLandingPreservesStepUp();
    TestDownwardPortalVerticalTransitions();
    TestDownwardPortalFootprintCommit();
    TestStepDownPortalFootprintCommit();
    TestStepDownFootprintSupportAtPortalCorner();
    TestDownwardPortalOffAxisFootprintCommit();
    TestDownwardPortalFootprintLookupWaitsAfterBarelyCrossing();
    TestLowerSectorNearReversePortalDoesNotApplyRadiusNudge();
    TestAirbornePortalRules();
    TestJumpingPlayerCannotAutoStepThroughPortal();
    TestFeetHeightControlsReverseStepBlocking();
    TestGroundedDropConstraint();
    TestStructuralPrimitiveCollision();
    TestSectorFallbackAndBoundary();
    TestItemDropClearanceQueries();
    if (failures == 0) {
        std::puts("Sector collision movement tests passed");
    }
    return failures == 0 ? 0 : 1;
}
