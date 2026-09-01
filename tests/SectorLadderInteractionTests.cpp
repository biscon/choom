#include "sector_demo/SectorLadderInteraction.h"
#include "sector_demo/SectorStructuralPrimitives.h"
#include "sector_demo/SectorTopologyMap.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

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

    std::cout << "Sector ladder interaction tests passed\n";
    return 0;
}
