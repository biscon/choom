#include "sector_demo/SectorFpsController.h"

#include <raymath.h>

#include <cmath>
#include <cstdio>

namespace {

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

bool Near(Vector3 a, Vector3 b, float epsilon = 0.0001f)
{
    return Near(a.x, b.x, epsilon)
            && Near(a.y, b.y, epsilon)
            && Near(a.z, b.z, epsilon);
}

void TestEyePositionUsesFeetAndEyeHeight()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{1.0f, 2.0f, 3.0f};
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.5f;
    Check(Near(game::SectorFpsControllerEyePosition(state, config), Vector3{1.0f, 3.5f, 3.0f}),
            "eye position adds eye height to feet position");
}

void TestPoseConversions()
{
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.2f;
    game::SectorMeshPreviewPose cameraPose{Vector3{10.0f, 2.2f, 3.0f}, 0.25f, -0.5f};
    const game::SectorFpsControllerState state =
            game::SectorFpsControllerStateFromCameraPose(cameraPose, config);
    Check(Near(state.feetPosition, Vector3{10.0f, 1.0f, 3.0f}),
            "camera pose to fps state subtracts eye height");
    Check(Near(state.yawRadians, 0.25f) && Near(state.pitchRadians, -0.5f),
            "camera pose to fps state preserves yaw and pitch");
    Check(Near(game::SectorFpsControllerPose(state, config).position, cameraPose.position),
            "fps state to camera pose adds eye height");
}

void TestVisualStepSmoothingCapturesSteppedUpContinuity()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{2.0f, 4.2f, 3.0f};
    state.yawRadians = 0.25f;
    state.pitchRadians = -0.1f;
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.2f;
    const float previousVisualEyeY = 5.2f;

    float offset = 0.0f;
    game::ApplySectorFpsVisualStepSmoothing(
            offset,
            game::SectorFpsVerticalTransition::SteppedUp,
            previousVisualEyeY,
            state,
            config,
            game::DefaultSectorFpsStepSmoothingRate(),
            0.016f);
    const game::SectorMeshPreviewPose visualPose =
            game::SectorFpsControllerVisualPose(state, config, offset);

    Check(Near(offset, -0.2f), "stepped-up smoothing captures negative continuity offset");
    Check(Near(visualPose.position.y, previousVisualEyeY),
          "stepped-up smoothing preserves initial visual eye height");
    Check(Near(state.feetPosition, Vector3{2.0f, 4.2f, 3.0f}),
          "stepped-up smoothing does not mutate physics feet");
}

void TestVisualStepSmoothingCapturesSnappedDownContinuity()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{2.0f, 3.8f, 3.0f};
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.2f;
    const float previousVisualEyeY = 5.2f;

    float offset = 0.0f;
    game::ApplySectorFpsVisualStepSmoothing(
            offset,
            game::SectorFpsVerticalTransition::SnappedDown,
            previousVisualEyeY,
            state,
            config,
            game::DefaultSectorFpsStepSmoothingRate(),
            0.016f);
    const game::SectorMeshPreviewPose visualPose =
            game::SectorFpsControllerVisualPose(state, config, offset);

    Check(Near(offset, 0.2f), "snapped-down smoothing captures positive continuity offset");
    Check(Near(visualPose.position.y, previousVisualEyeY),
          "snapped-down smoothing preserves initial visual eye height");
    Check(Near(state.feetPosition, Vector3{2.0f, 3.8f, 3.0f}),
          "snapped-down smoothing does not mutate physics feet");
}

void TestVisualStepSmoothingDecayAndClearTransitions()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 4.0f, 0.0f};
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.2f;

    float offset = 0.25f;
    game::ApplySectorFpsVisualStepSmoothing(
            offset,
            game::SectorFpsVerticalTransition::StayedGrounded,
            5.45f,
            state,
            config,
            game::DefaultSectorFpsStepSmoothingRate(),
            0.05f);
    Check(offset > 0.0f && offset < 0.25f, "visual step smoothing decays toward zero");

    game::ApplySectorFpsVisualStepSmoothing(
            offset,
            game::SectorFpsVerticalTransition::StayedGrounded,
            5.45f,
            state,
            config,
            game::DefaultSectorFpsStepSmoothingRate(),
            1.0f);
    Check(Near(offset, 0.0f), "visual step smoothing snaps tiny offsets to zero");

    const game::SectorFpsVerticalTransition clearTransitions[] = {
            game::SectorFpsVerticalTransition::StartedDrop,
            game::SectorFpsVerticalTransition::Landed,
            game::SectorFpsVerticalTransition::CeilingBonk,
            game::SectorFpsVerticalTransition::CannotFit
    };
    for (const game::SectorFpsVerticalTransition transition : clearTransitions) {
        offset = 0.25f;
        game::ApplySectorFpsVisualStepSmoothing(
                offset,
                transition,
                5.45f,
                state,
                config,
                game::DefaultSectorFpsStepSmoothingRate(),
                0.05f);
        Check(Near(offset, 0.0f), "visual step smoothing clears for non-step vertical transitions");
    }
}

void TestHeadBobUpdatesFromResolvedMovementOnly()
{
    game::SectorFpsControllerConfig config;
    config.headBobStrength = 0.04f;
    config.headBobFrequency = 8.0f;
    config.runSpeed = 12.0f;
    game::SectorFpsHeadBobState headBob;

    game::UpdateSectorFpsHeadBob(headBob, config, true, 6.0f, 0.0f, 0.016f);
    Check(headBob.phase > 0.0f, "grounded resolved movement advances headbob phase");
    Check(headBob.blend > 0.0f, "grounded resolved movement increases headbob blend");
    Check(Vector3LengthSqr(headBob.offset) > 0.0f, "grounded resolved movement produces headbob offset");

    const float phaseAfterMovement = headBob.phase;
    game::UpdateSectorFpsHeadBob(headBob, config, true, 0.0f, 0.0f, 0.016f);
    Check(Near(headBob.phase, phaseAfterMovement), "zero resolved movement does not advance headbob phase");
}

void TestHeadBobInactiveAndDisabledBehavior()
{
    game::SectorFpsControllerConfig config;
    config.headBobStrength = 0.04f;
    config.headBobFrequency = 8.0f;
    game::SectorFpsHeadBobState headBob;
    game::UpdateSectorFpsHeadBob(headBob, config, true, 6.0f, 0.25f, 0.016f);
    const float phaseAfterMovement = headBob.phase;
    const Vector3 offsetAfterMovement = headBob.offset;

    game::UpdateSectorFpsHeadBob(headBob, config, true, 6.0f, 0.25f, 0.0f);
    Check(Near(headBob.phase, phaseAfterMovement), "zero dt does not advance headbob phase");
    Check(Near(headBob.offset, offsetAfterMovement), "zero dt does not create a new headbob impulse");

    game::UpdateSectorFpsHeadBob(headBob, config, false, 6.0f, 0.25f, 0.25f);
    Check(Near(headBob.phase, phaseAfterMovement), "inactive headbob does not advance phase");
    Check(headBob.blend < 1.0f, "inactive headbob decays blend toward zero");

    config.headBobStrength = 0.0f;
    game::UpdateSectorFpsHeadBob(headBob, config, true, 6.0f, 0.25f, 0.016f);
    Check(Near(headBob.blend, 0.0f) && Near(headBob.offset, Vector3{}),
          "zero headbob strength clears visible bob");

    config.headBobStrength = 0.04f;
    config.headBobFrequency = 0.0f;
    headBob.phase = 1.0f;
    headBob.blend = 1.0f;
    headBob.offset = Vector3{1.0f, 1.0f, 1.0f};
    game::UpdateSectorFpsHeadBob(headBob, config, true, 6.0f, 0.25f, 0.016f);
    Check(Near(headBob.phase, 1.0f), "zero headbob frequency does not advance phase");
    Check(Near(headBob.blend, 0.0f) && Near(headBob.offset, Vector3{}),
          "zero headbob frequency clears visible bob");
}

void TestHeadBobVisualOnlyPoseLayer()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{2.0f, 4.0f, 3.0f};
    state.yawRadians = 0.25f;
    state.pitchRadians = -0.1f;
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.2f;

    const Vector3 originalFeet = state.feetPosition;
    const float visualStepOffsetY = 0.15f;
    const Vector3 headBobOffset{0.02f, -0.01f, 0.03f};
    const game::SectorMeshPreviewPose pose =
            game::SectorFpsControllerVisualPose(state, config, visualStepOffsetY, headBobOffset);

    Check(Near(pose.position, Vector3{2.02f, 5.34f, 3.03f}),
          "headbob layers on top of physics eye and visual step offset");
    Check(Near(state.feetPosition, originalFeet), "headbob pose does not mutate physics feet");
}

void TestForwardMovementIgnoresPitchAndPreservesY()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 3.0f, 0.0f};
    state.yawRadians = 0.0f;
    state.pitchRadians = 1.0f;
    game::SectorFpsControllerConfig config;
    config.walkSpeed = 6.0f;
    game::SectorFpsControllerInput input;
    input.moveForward = true;
    UpdateSectorFpsController(state, config, input, 0.5f);
    Check(Near(state.feetPosition, Vector3{3.0f, 3.0f, 0.0f}),
            "forward movement uses yaw and preserves feet Y");
}

void TestRunAndWalkSpeeds()
{
    game::SectorFpsControllerConfig config;
    config.walkSpeed = 6.0f;
    config.runSpeed = 12.0f;

    game::SectorFpsControllerState walking;
    game::SectorFpsControllerInput walkInput;
    walkInput.moveForward = true;
    UpdateSectorFpsController(walking, config, walkInput, 1.0f);
    Check(Near(walking.feetPosition.x, 6.0f), "walk speed is used without run");

    game::SectorFpsControllerState running;
    game::SectorFpsControllerInput runInput;
    runInput.moveForward = true;
    runInput.run = true;
    UpdateSectorFpsController(running, config, runInput, 1.0f);
    Check(Near(running.feetPosition.x, 12.0f), "run speed is used with run input");
}

void TestMouseLookRawDeltaAndPitchClamp()
{
    game::SectorFpsControllerState state;
    game::SectorFpsControllerConfig config;
    config.mouseSensitivity = 2.0f;
    game::SectorFpsControllerInput input;
    input.mouseLookEnabled = true;
    input.mouseDelta = Vector2{10.0f, -10000.0f};
    UpdateSectorFpsController(state, config, input, 123.0f);
    Check(Near(state.yawRadians, 0.06f), "mouse look uses raw delta times sensitivity without dt");
    Check(state.pitchRadians <= 1.5534f && state.pitchRadians >= 1.5532f,
            "pitch clamps to about positive 89 degrees");
}

void TestConfigNormalization()
{
    game::SectorFpsControllerConfig config;
    config.walkSpeed = -1.0f;
    config.runSpeed = 999.0f;
    config.mouseSensitivity = INFINITY;
    config.eyeHeight = NAN;
    config.gravity = 500.0f;
    config.playerRadius = -1.0f;
    config.playerHeight = NAN;
    config.stepHeight = 99.0f;
    config.jumpHeight = 99.0f;
    config.headBobStrength = 99.0f;
    config.headBobFrequency = 99.0f;
    config = game::NormalizeSectorFpsControllerConfig(config);
    Check(Near(config.walkSpeed, 0.1f), "walk speed clamps low");
    Check(Near(config.runSpeed, 200.0f), "run speed clamps high");
    Check(Near(config.mouseSensitivity, 1.0f), "non-finite mouse sensitivity uses default");
    Check(Near(config.eyeHeight, 1.2f), "non-finite eye height uses default");
    Check(Near(config.gravity, 200.0f), "gravity clamps high");
    Check(Near(config.playerRadius, 0.05f), "player radius clamps low");
    Check(Near(config.playerHeight, 1.6f), "non-finite player height uses default");
    Check(Near(config.stepHeight, 2.0f), "step height clamps high");
    Check(Near(config.jumpHeight, 3.0f), "jump height clamps high");
    Check(Near(config.headBobStrength, 0.25f), "headbob strength clamps high");
    Check(Near(config.headBobFrequency, 20.0f), "headbob frequency clamps high");

    config.gravity = -5.0f;
    config.jumpHeight = -5.0f;
    config.headBobStrength = -5.0f;
    config.headBobFrequency = -5.0f;
    config = game::NormalizeSectorFpsControllerConfig(config);
    Check(Near(config.gravity, 0.0f), "gravity clamps low");
    Check(Near(config.jumpHeight, 0.0f), "jump height clamps low");
    Check(Near(config.headBobStrength, 0.0f), "headbob strength clamps low");
    Check(Near(config.headBobFrequency, 0.0f), "headbob frequency clamps low");

    config.gravity = INFINITY;
    config.jumpHeight = INFINITY;
    config.headBobStrength = INFINITY;
    config.headBobFrequency = INFINITY;
    config.eyeHeight = 2.2f;
    config.playerHeight = 1.0f;
    config = game::NormalizeSectorFpsControllerConfig(config);
    Check(Near(config.gravity, 25.0f), "non-finite gravity uses default");
    Check(Near(config.jumpHeight, 0.6f), "non-finite jump height uses default");
    Check(Near(config.headBobStrength, 0.020f), "non-finite headbob strength uses default");
    Check(Near(config.headBobFrequency, 2.0f), "non-finite headbob frequency uses default");
    Check(Near(config.playerHeight, 2.2f), "player height is at least eye height");
    Check(Near(game::DefaultSectorFpsControllerConfig().gravity, 25.0f),
          "default gravity is 25");
    Check(Near(game::DefaultSectorFpsControllerConfig().eyeHeight, 1.2f),
          "default eye height is 1.2 world units");
    Check(Near(game::DefaultSectorFpsControllerConfig().playerHeight, 1.6f),
          "default player height is 1.6 world units");
    Check(Near(game::DefaultSectorFpsControllerConfig().stepHeight, 0.25f),
          "default step height is 0.25 world units");
    Check(Near(game::DefaultSectorFpsControllerConfig().jumpHeight, 0.6f),
          "default jump height is 0.6 world units");
    Check(Near(game::DefaultSectorFpsControllerConfig().headBobStrength, 0.020f),
          "default headbob strength is 0.020 world units");
    Check(Near(game::DefaultSectorFpsControllerConfig().headBobFrequency, 2.0f),
          "default headbob frequency is 2");
}

void TestJumpStart()
{
    game::SectorFpsControllerState state;
    state.grounded = true;
    state.verticalVelocity = -3.0f;
    game::SectorFpsControllerConfig config;
    config.gravity = 25.0f;
    config.jumpHeight = 0.6f;

    Check(game::TryStartSectorFpsJump(state, config), "grounded jump starts");
    Check(!state.grounded, "jump start clears grounded");
    Check(Near(state.verticalVelocity, std::sqrt(2.0f * 25.0f * 0.6f)),
          "jump start computes velocity from gravity and jump height");

    const float jumpVelocity = state.verticalVelocity;
    Check(!game::TryStartSectorFpsJump(state, config), "airborne jump press does nothing");
    Check(Near(state.verticalVelocity, jumpVelocity), "airborne jump preserves velocity");

    state.grounded = true;
    state.verticalVelocity = -3.0f;
    config.gravity = 0.0f;
    Check(!game::TryStartSectorFpsJump(state, config), "zero gravity jump does not start");
    Check(state.grounded, "zero gravity jump preserves grounded");
    Check(Near(state.verticalVelocity, -3.0f), "zero gravity jump preserves velocity");

    config.gravity = 25.0f;
    config.jumpHeight = 0.0f;
    Check(!game::TryStartSectorFpsJump(state, config), "zero jump height jump does not start");
    Check(state.grounded, "zero jump height preserves grounded");
}

void TestJumpInputUsesEdgePress()
{
    game::SectorFpsControllerState state;
    state.grounded = true;
    game::SectorFpsControllerConfig config;
    config.gravity = 25.0f;
    config.jumpHeight = 0.6f;
    game::SectorFpsControllerInput input;
    input.jumpPressed = true;

    game::UpdateSectorFpsController(state, config, input, 0.0f);
    const float firstVelocity = state.verticalVelocity;
    Check(!state.grounded && firstVelocity > 0.0f, "jump input starts one jump");

    game::UpdateSectorFpsController(state, config, input, 0.0f);
    Check(Near(state.verticalVelocity, firstVelocity),
          "held jump input does not restart while airborne");
}

void TestGroundedFloorTransitions()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{1.0f, 4.0f, 2.0f};
    state.grounded = true;
    state.verticalVelocity = -3.0f;
    game::SectorFpsControllerConfig config;
    game::SectorFpsVerticalContext context{true, 4.0f, 20.0f};

    game::SectorFpsVerticalResult result =
            game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(result.transition == game::SectorFpsVerticalTransition::StayedGrounded,
          "same-floor grounded transition reports stayed grounded");
    Check(Near(state.feetPosition.y, 4.0f), "same-floor transition keeps feet on floor");
    Check(state.grounded, "same-floor transition stays grounded");
    Check(Near(state.verticalVelocity, 0.0f), "same-floor transition clears vertical velocity");

    state.feetPosition.y = 4.0f;
    state.grounded = true;
    state.verticalVelocity = -2.0f;
    context.floorZ = 4.2f;
    result = game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(result.transition == game::SectorFpsVerticalTransition::SteppedUp,
          "small upward floor transition reports stepped up");
    Check(Near(state.feetPosition.y, 4.2f), "small upward floor transition snaps up");
    Check(state.grounded, "small upward floor transition stays grounded");
    Check(Near(state.verticalVelocity, 0.0f), "small upward floor transition clears vertical velocity");

    state.feetPosition.y = 4.0f;
    state.grounded = true;
    state.verticalVelocity = -2.0f;
    context.floorZ = 4.5f;
    result = game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(result.transition == game::SectorFpsVerticalTransition::BlockedStep,
          "large upward floor transition reports blocked step");
    Check(Near(state.feetPosition.y, 4.0f), "large upward floor transition does not snap up");
    Check(state.grounded, "large upward floor transition preserves grounded state for caller recovery");
    Check(Near(state.verticalVelocity, 0.0f), "large upward floor transition clears vertical velocity");

    state.feetPosition.y = 4.0f;
    state.grounded = true;
    state.verticalVelocity = -2.0f;
    context.floorZ = 3.8f;
    result = game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(result.transition == game::SectorFpsVerticalTransition::SnappedDown,
          "small downward floor transition reports snapped down");
    Check(Near(state.feetPosition.y, 3.8f), "small downward floor transition snaps down");
    Check(state.grounded, "small downward floor transition stays grounded");
    Check(Near(state.verticalVelocity, 0.0f), "small downward floor transition clears vertical velocity");

    state.feetPosition.y = 4.0f;
    state.grounded = true;
    state.verticalVelocity = -2.0f;
    context.floorZ = 3.0f;
    result = game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(result.transition == game::SectorFpsVerticalTransition::StartedDrop,
          "large downward floor transition reports started drop");
    Check(Near(state.feetPosition.y, 4.0f), "large downward floor transition preserves feet height initially");
    Check(!state.grounded, "large downward floor transition starts falling");
    Check(Near(state.verticalVelocity, 0.0f), "large downward floor transition starts with deterministic zero velocity");

    result = game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.1f);
    Check(state.verticalVelocity < 0.0f, "gravity begins after started drop on the next update");
    Check(!state.grounded, "falling after started drop remains airborne while above floor");

    result = game::UpdateSectorFpsVerticalPhysics(state, config, context, 1.0f);
    Check(result.transition == game::SectorFpsVerticalTransition::Landed,
          "falling after started drop reports landed");
    Check(Near(state.feetPosition.y, 3.0f), "falling after started drop lands on lower floor");
    Check(state.grounded, "landing after started drop sets grounded");
    Check(Near(state.verticalVelocity, 0.0f), "landing after started drop clears vertical velocity");
}

void TestFallingAndLanding()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 10.0f, 0.0f};
    state.grounded = false;
    game::SectorFpsControllerConfig config;
    config.gravity = 10.0f;
    game::SectorFpsVerticalContext context{true, 0.0f, 20.0f};

    game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(Near(state.verticalVelocity, -5.0f), "falling applies gravity to velocity");
    Check(Near(state.feetPosition.y, 7.5f), "falling integrates feet height");
    Check(!state.grounded, "airborne player remains falling above floor");

    const game::SectorFpsVerticalResult result =
            game::UpdateSectorFpsVerticalPhysics(state, config, context, 2.0f);
    Check(result.transition == game::SectorFpsVerticalTransition::Landed,
          "falling player landing reports landed");
    Check(Near(state.feetPosition.y, 0.0f), "falling player lands on floor");
    Check(state.grounded, "landing sets grounded true");
    Check(Near(state.verticalVelocity, 0.0f), "landing clears vertical velocity");
}

void TestJumpVerticalMotionAndLanding()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 0.0f, 0.0f};
    state.grounded = true;
    game::SectorFpsControllerConfig config;
    config.gravity = 10.0f;
    config.jumpHeight = 1.0f;
    game::SectorFpsVerticalContext context{true, 0.0f, 20.0f};

    Check(game::TryStartSectorFpsJump(state, config), "jump starts before vertical update");
    const float jumpVelocity = state.verticalVelocity;
    game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.1f);
    Check(state.feetPosition.y > 0.0f, "jumping player moves upward on first update");
    Check(state.verticalVelocity < jumpVelocity, "gravity reduces upward jump velocity");
    Check(!state.grounded, "jumping player remains airborne above floor");

    const game::SectorFpsVerticalResult result =
            game::UpdateSectorFpsVerticalPhysics(state, config, context, 2.0f);
    Check(result.transition == game::SectorFpsVerticalTransition::Landed,
          "jumping player eventually lands");
    Check(Near(state.feetPosition.y, 0.0f), "jumping player lands on floor");
    Check(state.grounded, "jump landing sets grounded");
    Check(Near(state.verticalVelocity, 0.0f), "jump landing clears velocity");
}

void TestZeroGravityDoesNotMoveVertically()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 10.0f, 0.0f};
    state.grounded = false;
    state.verticalVelocity = -4.0f;
    game::SectorFpsControllerConfig config;
    config.gravity = 0.0f;
    game::SectorFpsVerticalContext context{true, 0.0f, 20.0f};

    game::UpdateSectorFpsVerticalPhysics(state, config, context, 1.0f);
    Check(Near(state.feetPosition.y, 10.0f), "zero gravity does not move falling player");
    Check(Near(state.verticalVelocity, -4.0f), "zero gravity preserves existing velocity");
    Check(!state.grounded, "zero gravity does not force grounded state above floor");
}

void TestCeilingClamp()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 18.0f, 0.0f};
    state.grounded = false;
    state.verticalVelocity = 6.0f;
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.2f;
    config.playerHeight = 5.0f;
    config.gravity = 0.0f;
    game::SectorFpsVerticalContext context{true, 0.0f, 20.0f};

    const game::SectorFpsVerticalResult result =
            game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(Near(state.feetPosition.y, 17.0f), "ceiling clamp uses player height for maximum allowed feet height");
    Check(Near(state.verticalVelocity, 0.0f), "ceiling clamp clears upward velocity");
    Check(!state.grounded, "ceiling clamp does not mark airborne player grounded");
    Check(result.transition == game::SectorFpsVerticalTransition::CeilingBonk,
          "ceiling clamp reports ceiling bonk transition");
}

void TestCannotFitClampsToFloor()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 12.0f, 0.0f};
    state.grounded = false;
    state.verticalVelocity = 9.0f;
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.2f;
    config.playerHeight = 3.0f;
    game::SectorFpsVerticalContext context{true, 10.0f, 12.0f};

    const game::SectorFpsVerticalResult result =
            game::UpdateSectorFpsVerticalPhysics(state, config, context, 1.0f);
    Check(result.cannotFit, "cannot-fit vertical result is reported");
    Check(result.transition == game::SectorFpsVerticalTransition::CannotFit,
          "cannot-fit vertical result reports cannot fit transition");
    Check(Near(state.feetPosition.y, 10.0f), "cannot-fit case leaves feet on floor");
    Check(state.feetPosition.y >= context.floorZ, "cannot-fit case never places feet below floor");
    Check(state.grounded, "cannot-fit case sets grounded true");
    Check(Near(state.verticalVelocity, 0.0f), "cannot-fit case clears vertical velocity");
}

void TestNoSectorPreservesVerticalState()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 12.0f, 0.0f};
    state.grounded = true;
    state.verticalVelocity = -5.0f;
    game::SectorFpsControllerConfig config;
    config.gravity = 25.0f;
    game::SectorFpsVerticalContext context{false, 0.0f, 0.0f};

    game::UpdateSectorFpsVerticalPhysics(state, config, context, 1.0f);
    Check(Near(state.feetPosition.y, 12.0f), "no-sector context preserves feet Y");
    Check(Near(state.verticalVelocity, -5.0f), "no-sector context preserves vertical velocity");
    Check(!state.grounded, "no-sector context clears grounded state");
}

} // namespace

int main()
{
    TestEyePositionUsesFeetAndEyeHeight();
    TestPoseConversions();
    TestVisualStepSmoothingCapturesSteppedUpContinuity();
    TestVisualStepSmoothingCapturesSnappedDownContinuity();
    TestVisualStepSmoothingDecayAndClearTransitions();
    TestHeadBobUpdatesFromResolvedMovementOnly();
    TestHeadBobInactiveAndDisabledBehavior();
    TestHeadBobVisualOnlyPoseLayer();
    TestForwardMovementIgnoresPitchAndPreservesY();
    TestRunAndWalkSpeeds();
    TestMouseLookRawDeltaAndPitchClamp();
    TestConfigNormalization();
    TestJumpStart();
    TestJumpInputUsesEdgePress();
    TestGroundedFloorTransitions();
    TestFallingAndLanding();
    TestJumpVerticalMotionAndLanding();
    TestZeroGravityDoesNotMoveVertically();
    TestCeilingClamp();
    TestCannotFitClampsToFloor();
    TestNoSectorPreservesVerticalState();
    if (failures == 0) {
        std::puts("Sector FPS controller tests passed");
    }
    return failures == 0 ? 0 : 1;
}
