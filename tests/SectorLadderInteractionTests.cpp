#include "sector_demo/SectorLadderInteraction.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorLiquidInteraction.h"
#include "sector_demo/SectorStructuralPrimitives.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

void Require(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

bool Near(float a, float b, float epsilon = 0.0001f)
{
    return std::fabs(a - b) <= epsilon;
}

game::SectorCoord Coord(float authoringUnits)
{
    return static_cast<game::SectorCoord>(
            authoringUnits * game::SectorCoordSubdivisions);
}

void AddSide(
        game::SectorTopologyMap& map,
        int sideId,
        int lineId,
        game::SectorTopologySideKind side,
        int sectorId)
{
    game::SectorTopologySideDef sideDef;
    sideDef.id = sideId;
    sideDef.lineDefId = lineId;
    sideDef.side = side;
    sideDef.sectorId = sectorId;
    map.sideDefs.push_back(sideDef);
}

game::SectorTopologySector Sector(
        int id,
        float floorZ,
        float ceilingZ = 40.0f)
{
    game::SectorTopologySector sector;
    sector.id = id;
    sector.floorZ = floorZ;
    sector.ceilingZ = ceilingZ;
    return sector;
}

game::SectorTopologyMap MakeSquare(float floorZ = 0.0f)
{
    using namespace game;
    SectorTopologyMap map;
    map.vertices = {
            {1, Coord(0), Coord(0)},
            {2, Coord(64), Coord(0)},
            {3, Coord(64), Coord(64)},
            {4, Coord(0), Coord(64)}};
    map.lineDefs = {
            {1, 1, 2, 1, -1},
            {2, 2, 3, 2, -1},
            {3, 3, 4, 3, -1},
            {4, 4, 1, 4, -1}};
    AddSide(map, 1, 1, SectorTopologySideKind::Front, 10);
    AddSide(map, 2, 2, SectorTopologySideKind::Front, 10);
    AddSide(map, 3, 3, SectorTopologySideKind::Front, 10);
    AddSide(map, 4, 4, SectorTopologySideKind::Front, 10);
    map.sectors.push_back(Sector(10, floorZ));
    return map;
}

game::SectorTopologyMap MakeAdjacent(float leftFloor, float rightFloor)
{
    using namespace game;
    SectorTopologyMap map;
    map.vertices = {
            {1, Coord(0), Coord(0)},
            {2, Coord(64), Coord(0)},
            {3, Coord(64), Coord(64)},
            {4, Coord(0), Coord(64)},
            {5, Coord(128), Coord(0)},
            {6, Coord(128), Coord(64)}};
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
    map.sectors.push_back(Sector(10, leftFloor));
    map.sectors.push_back(Sector(20, rightFloor));
    return map;
}

void CompileStructures(
        game::SectorTopologyMap& map,
        const std::vector<game::SectorAuthoringStructuralPrimitive>& structures)
{
    std::vector<game::SectorStructuralDiagnostic> diagnostics;
    Require(game::CompileSectorStructuralPrimitives(
                    structures,
                    map,
                    map.compiledStructuralPrimitives,
                    diagnostics),
            "generated structural fixture compiles");
}

void CompileLadder(
        game::SectorTopologyMap& map,
        const game::SectorAuthoringStructuralPrimitive& ladder)
{
    CompileStructures(map, {ladder});
}

game::SectorCollisionWorld BuildWorld(const game::SectorTopologyMap& map)
{
    game::SectorCollisionWorld world;
    std::string error;
    Require(world.BuildFromTopology(map, &error),
            "generated ladder collision world builds");
    return world;
}

void TestLiquidStateAndDetach()
{
    using namespace game;
    SectorTopologyMap liquidMap;
    SectorTopologySector liquidSector = Sector(7, -16.0f);
    liquidSector.liquid.enabled = true;
    liquidSector.liquid.surfaceOffset = 16.0f;
    liquidMap.sectors.push_back(liquidSector);

    SectorFpsControllerConfig config = DefaultSectorFpsControllerConfig();
    SectorFpsControllerState controller;
    controller.currentSectorId = 7;
    controller.feetPosition.y = -1.30f;
    SectorLiquidMovementState liquid;
    liquid.exitingWater = true;
    liquid.impactEntryActive = true;
    UpdateSectorLadderLiquidState(
            liquid, controller, config, liquidMap, false);
    Require(liquid.swimming && liquid.cameraSubmerged,
            "ladder motion refreshes swimming and submerged state below the waterline");
    Require(!liquid.exitingWater && !liquid.impactEntryActive,
            "ladder traversal cancels incompatible liquid transitions");

    controller.feetPosition.y = -0.50f;
    UpdateSectorLadderLiquidState(
            liquid, controller, config, liquidMap, false);
    Require(!liquid.swimming && !liquid.cameraSubmerged,
            "ladder motion clears liquid state while climbing above the waterline");

    SectorLadderTraversalState traversal;
    traversal.phase = SectorLadderTraversalPhase::Climbing;
    controller.grounded = true;
    controller.verticalVelocity = 3.0f;
    Require(!TryDetachSectorLadderTraversal(
                    traversal, controller, false)
                    && IsSectorLadderTraversalActive(traversal),
            "ladder detach is rejected while the camera is above water");
    Require(TryDetachSectorLadderTraversal(
                    traversal, controller, true)
                    && !IsSectorLadderTraversalActive(traversal)
                    && !controller.grounded
                    && Near(controller.verticalVelocity, 0.0f),
            "ladder detach releases an underwater player without adding momentum");
}

game::SectorAuthoringStructuralPrimitive MakePortalLadder()
{
    game::SectorAuthoringStructuralPrimitive ladder =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Ladder);
    ladder.id = 42;
    ladder.x = Coord(64.0f) + 40;
    ladder.z = Coord(32.0f);
    ladder.yawDegrees = 270.0f;
    ladder.ladder.bottom = -27.2f;
    ladder.ladder.height = 30.0f;
    return ladder;
}

game::SectorAuthoringStructuralPrimitive MakeRampLandingLadder()
{
    game::SectorAuthoringStructuralPrimitive ladder =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Ladder);
    ladder.id = 42;
    ladder.x = Coord(32.0f);
    ladder.z = Coord(32.0f);
    ladder.yawDegrees = 270.0f;
    ladder.ladder.height = 5.8f;
    return ladder;
}

game::SectorAuthoringStructuralPrimitive MakeRampLanding()
{
    game::SectorAuthoringStructuralPrimitive ramp =
            game::DefaultSectorAuthoringStructuralPrimitive(
                    game::SectorStructuralPrimitiveKind::Ramp);
    ramp.id = 43;
    ramp.x = Coord(24.0f);
    ramp.z = Coord(32.0f);
    ramp.yawDegrees = 90.0f;
    ramp.ramp.width = Coord(64.0f);
    ramp.ramp.run = Coord(16.0f);
    ramp.ramp.low = 2.0f;
    ramp.ramp.high = 10.0f;
    return ramp;
}

void ClimbFromBottomToTop(
        game::SectorLadderTraversalState& traversal,
        game::SectorFpsControllerState& controller,
        const game::SectorFpsControllerConfig& config,
        const game::SectorTopologyMap& map,
        const game::SectorCollisionWorld& world)
{
    using namespace game;
    Require(BeginSectorLadderTraversal(
                    traversal,
                    controller,
                    config,
                    map,
                    &world,
                    42,
                    SectorLadderEndpoint::Bottom),
            "generated ladder begins traversal from the bottom");
    SectorFpsControllerInput input;
    UpdateSectorLadderTraversal(
            traversal,
            controller,
            config,
            input,
            map,
            &world,
            SectorLadderTransitionSeconds);
    input.moveForward = true;
    UpdateSectorLadderTraversal(
            traversal, controller, config, input, map, &world, 10.0f);
}

void TestTopExitResolution()
{
    using namespace game;
    const SectorFpsControllerConfig config = DefaultSectorFpsControllerConfig();

    SectorTopologyMap landingMap = MakeAdjacent(10.0f, -50.0f);
    const SectorAuthoringStructuralPrimitive landingLadder = MakePortalLadder();
    CompileLadder(landingMap, landingLadder);
    const SectorCollisionWorld landingWorld = BuildWorld(landingMap);
    SectorFpsControllerState controller;
    controller.currentSectorId = 20;
    controller.feetPosition = {
            SectorCoordToWorldDistance(landingLadder.x) + 1.0f,
            SectorAuthoringToWorldDistance(landingLadder.ladder.bottom),
            SectorCoordToWorldDistance(landingLadder.z)};
    SectorLadderTraversalState traversal;
    ClimbFromBottomToTop(
            traversal, controller, config, landingMap, landingWorld);
    Require(traversal.phase == SectorLadderTraversalPhase::Dismounting
                    && Near(traversal.transitionTargetFeet.y, 1.25f),
            "top exit pulls the player onto a reachable adjacent landing");
    SectorFpsControllerInput input;
    UpdateSectorLadderTraversal(
            traversal,
            controller,
            config,
            input,
            landingMap,
            &landingWorld,
            SectorLadderTransitionSeconds);
    Require(!IsSectorLadderTraversalActive(traversal)
                    && Near(controller.feetPosition.y, 1.25f)
                    && controller.grounded,
            "reachable landing dismount completes grounded at the resolved floor height");

    SectorTopologyMap rampMap = MakeSquare();
    const SectorAuthoringStructuralPrimitive rampLadder =
            MakeRampLandingLadder();
    const SectorAuthoringStructuralPrimitive ramp = MakeRampLanding();
    CompileStructures(rampMap, {ramp, rampLadder});
    const SectorCollisionWorld rampWorld = BuildWorld(rampMap);
    controller = {};
    controller.currentSectorId = 10;
    controller.feetPosition = {
            SectorCoordToWorldDistance(rampLadder.x) + 1.0f,
            SectorAuthoringToWorldDistance(rampLadder.ladder.bottom),
            SectorCoordToWorldDistance(rampLadder.z)};
    traversal = {};
    SectorFpsControllerConfig rampConfig = config;
    rampConfig.stepHeight = 0.4f;
    ClimbFromBottomToTop(
            traversal, controller, rampConfig, rampMap, rampWorld);
    Require(traversal.phase == SectorLadderTraversalPhase::Dismounting
                    && traversal.transitionTargetFeet.y < traversal.topY
                    && traversal.topY - traversal.transitionTargetFeet.y
                            <= rampConfig.stepHeight + 0.0001f,
            "a nearby ramp supports a top exit below the ladder top");
    const Vector3 rampExit = traversal.transitionTargetFeet;
    UpdateSectorLadderTraversal(
            traversal,
            controller,
            rampConfig,
            input,
            rampMap,
            &rampWorld,
            SectorLadderTransitionSeconds);
    Require(!IsSectorLadderTraversalActive(traversal)
                    && controller.grounded
                    && Near(controller.feetPosition.y, rampExit.y),
            "ramp dismount preserves grounded support for the movement handoff");
    const SectorCollisionMoveResult rampMove = rampWorld.ResolveMovement(
            SectorCollisionMoveState{
                    {controller.feetPosition.x, controller.feetPosition.z},
                    controller.feetPosition.y,
                    controller.currentSectorId,
                    controller.grounded},
            Vector2{-0.10f, 0.0f},
            SectorCollisionMoveConfig{
                    rampConfig.playerRadius,
                    rampConfig.playerHeight,
                    rampConfig.stepHeight,
                    4});
    Require(rampMove.positionXZ.x
                    < controller.feetPosition.x - 0.05f,
            "first grounded movement frame advances farther onto the ramp");

    SectorTopologyMap blockedRampMap = MakeSquare();
    SectorAuthoringStructuralPrimitive blocker =
            DefaultSectorAuthoringStructuralPrimitive(
                    SectorStructuralPrimitiveKind::Box);
    blocker.id = 44;
    blocker.x = Coord(28.0f);
    blocker.z = Coord(32.0f);
    blocker.box.width = Coord(4.0f);
    blocker.box.depth = Coord(4.0f);
    blocker.box.bottom = 8.0f;
    blocker.box.top = 20.0f;
    CompileStructures(blockedRampMap, {ramp, blocker, rampLadder});
    const SectorCollisionWorld blockedRampWorld = BuildWorld(blockedRampMap);
    controller = {};
    controller.currentSectorId = 10;
    controller.feetPosition = {
            SectorCoordToWorldDistance(rampLadder.x) + 1.0f,
            SectorAuthoringToWorldDistance(rampLadder.ladder.bottom),
            SectorCoordToWorldDistance(rampLadder.z)};
    traversal = {};
    ClimbFromBottomToTop(
            traversal,
            controller,
            rampConfig,
            blockedRampMap,
            blockedRampWorld);
    Require(traversal.phase == SectorLadderTraversalPhase::Climbing,
            "an obstacle above a structural landing still blocks the top exit");

    SectorTopologyMap highMap = MakeAdjacent(16.0f, -50.0f);
    const SectorAuthoringStructuralPrimitive highLadder = MakePortalLadder();
    CompileLadder(highMap, highLadder);
    const SectorCollisionWorld highWorld = BuildWorld(highMap);
    controller = {};
    controller.currentSectorId = 20;
    controller.feetPosition = {
            SectorCoordToWorldDistance(highLadder.x) + 1.0f,
            SectorAuthoringToWorldDistance(highLadder.ladder.bottom),
            SectorCoordToWorldDistance(highLadder.z)};
    traversal = {};
    ClimbFromBottomToTop(
            traversal, controller, config, highMap, highWorld);
    Require(traversal.phase == SectorLadderTraversalPhase::Climbing,
            "a landing above ladder exit reach keeps the player attached");

    SectorTopologyMap unsupportedMap = MakeSquare();
    unsupportedMap.sectors.front().ceilingZ = 54.0f;
    SectorAuthoringStructuralPrimitive unsupportedLadder =
            DefaultSectorAuthoringStructuralPrimitive(
                    SectorStructuralPrimitiveKind::Ladder);
    unsupportedLadder.id = 42;
    unsupportedLadder.x = Coord(32.0f);
    unsupportedLadder.z = Coord(32.0f);
    unsupportedLadder.ladder.height = 40.0f;
    CompileLadder(unsupportedMap, unsupportedLadder);
    const SectorCollisionWorld unsupportedWorld = BuildWorld(unsupportedMap);
    controller = {};
    controller.currentSectorId = 10;
    controller.feetPosition = {
            SectorCoordToWorldDistance(unsupportedLadder.x),
            0.0f,
            SectorCoordToWorldDistance(unsupportedLadder.z) + 1.0f};
    traversal = {};
    SectorFpsControllerConfig unsupportedConfig = config;
    unsupportedConfig.eyeHeight = 1.75f;
    unsupportedConfig.playerHeight = 1.8f;
    ClimbFromBottomToTop(
            traversal,
            controller,
            unsupportedConfig,
            unsupportedMap,
            unsupportedWorld);
    Require(!IsSectorLadderTraversalActive(traversal)
                    && !controller.grounded
                    && Near(
                            controller.feetPosition.y,
                            SectorAuthoringToWorldDistance(
                                    unsupportedLadder.ladder.height)),
            "an unsupported ladder top detaches immediately into falling physics");
}

} // namespace

int main()
{
    using namespace game;

    SectorTopologyMap map;
    SectorCompiledStructuralPrimitive compiled;
    compiled.sourceAuthoringPrimitiveId = 42;
    compiled.authored = DefaultSectorAuthoringStructuralPrimitive(
            SectorStructuralPrimitiveKind::Ladder);
    compiled.authored.id = 42;
    map.compiledStructuralPrimitives.push_back(compiled);

    SectorFpsControllerConfig config = DefaultSectorFpsControllerConfig();
    SectorFpsControllerState controller;
    controller.feetPosition = {0.0f, 0.0f, 1.0f};
    controller.grounded = true;
    SectorLadderTraversalState traversal;
    Require(BeginSectorLadderTraversal(
                    traversal, controller, config, map, nullptr,
                    42, SectorLadderEndpoint::Bottom),
            "bottom ladder endpoint begins mounting");
    Require(traversal.phase == SectorLadderTraversalPhase::Mounting,
            "mount begins with an eased transition");

    SectorFpsControllerInput input;
    Require(UpdateSectorLadderTraversal(
                    traversal, controller, config, input,
                    map, nullptr, SectorLadderTransitionSeconds),
            "mount transition consumes controller movement");
    Require(traversal.phase == SectorLadderTraversalPhase::Climbing
                    && Near(controller.feetPosition.y, traversal.bottomY),
            "bottom mount ends on the constrained rail");

    input.moveForward = true;
    controller.yawRadians = traversal.facingYawRadians + PI;
    UpdateSectorLadderTraversal(
            traversal, controller, config, input, map, nullptr, 0.5f);
    Require(Near(controller.feetPosition.y, 0.75f),
            "forward input climbs at the shared ladder speed");
    Require(Near(std::remainder(
                         controller.yawRadians - traversal.facingYawRadians,
                         2.0f * PI),
                         SectorLadderLookYawArcRadians),
            "ladder yaw is clamped to the configured look arc");

    UpdateSectorLadderTraversal(
            traversal, controller, config, input, map, nullptr, 10.0f);
    Require(traversal.phase == SectorLadderTraversalPhase::Dismounting,
            "reaching the top begins an automatic dismount");
    UpdateSectorLadderTraversal(
            traversal, controller, config, input,
            map, nullptr, SectorLadderTransitionSeconds);
    Require(!IsSectorLadderTraversalActive(traversal)
                    && !controller.grounded,
            "completed top dismount restores ordinary airborne physics");

    controller.feetPosition = {0.0f, 2.5f, -1.0f};
    Require(BeginSectorLadderTraversal(
                    traversal, controller, config, map, nullptr,
                    42, SectorLadderEndpoint::Top),
            "top ladder endpoint begins a descent mount");
    Require(Near(traversal.transitionTargetFeet.y, 2.5f),
            "top mount targets the authored ladder top");
    ResetSectorLadderTraversal(traversal);
    Require(!BeginSectorLadderTraversal(
                    traversal, controller, config, map, nullptr,
                    999, SectorLadderEndpoint::Bottom),
            "missing stable ladder IDs cannot begin traversal");

    TestLiquidStateAndDetach();
    TestTopExitResolution();

    std::cout << "Sector ladder interaction tests passed\n";
    return 0;
}
