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
    config.eyeHeight = 5.0f;
    Check(Near(game::SectorFpsControllerEyePosition(state, config), Vector3{1.0f, 7.0f, 3.0f}),
            "eye position adds eye height to feet position");
}

void TestPoseConversions()
{
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 4.5f;
    game::SectorMeshPreviewPose cameraPose{Vector3{10.0f, 9.0f, 3.0f}, 0.25f, -0.5f};
    const game::SectorFpsControllerState state =
            game::SectorFpsControllerStateFromCameraPose(cameraPose, config);
    Check(Near(state.feetPosition, Vector3{10.0f, 4.5f, 3.0f}),
            "camera pose to fps state subtracts eye height");
    Check(Near(state.yawRadians, 0.25f) && Near(state.pitchRadians, -0.5f),
            "camera pose to fps state preserves yaw and pitch");
    Check(Near(game::SectorFpsControllerPose(state, config).position, cameraPose.position),
            "fps state to camera pose adds eye height");
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
    config = game::NormalizeSectorFpsControllerConfig(config);
    Check(Near(config.walkSpeed, 0.1f), "walk speed clamps low");
    Check(Near(config.runSpeed, 200.0f), "run speed clamps high");
    Check(Near(config.mouseSensitivity, 1.0f), "non-finite mouse sensitivity uses default");
    Check(Near(config.eyeHeight, 5.0f), "non-finite eye height uses default");
    Check(Near(config.gravity, 200.0f), "gravity clamps high");

    config.gravity = -5.0f;
    config = game::NormalizeSectorFpsControllerConfig(config);
    Check(Near(config.gravity, 0.0f), "gravity clamps low");

    config.gravity = INFINITY;
    config = game::NormalizeSectorFpsControllerConfig(config);
    Check(Near(config.gravity, 25.0f), "non-finite gravity uses default");
    Check(Near(game::DefaultSectorFpsControllerConfig().gravity, 25.0f),
          "default gravity is 25");
}

void TestGroundedPlayerSnapsToFloor()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{1.0f, 9.0f, 2.0f};
    state.grounded = true;
    state.verticalVelocity = -3.0f;
    game::SectorFpsControllerConfig config;
    game::SectorFpsVerticalContext context{true, 4.0f, 20.0f};

    game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(Near(state.feetPosition.y, 4.0f), "grounded player snaps to current floor");
    Check(state.grounded, "grounded player stays grounded");
    Check(Near(state.verticalVelocity, 0.0f), "grounded snap clears vertical velocity");

    context.floorZ = 8.0f;
    game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(Near(state.feetPosition.y, 8.0f), "grounded player follows changed sector floor");
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

    game::UpdateSectorFpsVerticalPhysics(state, config, context, 2.0f);
    Check(Near(state.feetPosition.y, 0.0f), "falling player lands on floor");
    Check(state.grounded, "landing sets grounded true");
    Check(Near(state.verticalVelocity, 0.0f), "landing clears vertical velocity");
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
    config.eyeHeight = 5.0f;
    config.gravity = 0.0f;
    game::SectorFpsVerticalContext context{true, 0.0f, 20.0f};

    game::UpdateSectorFpsVerticalPhysics(state, config, context, 0.5f);
    Check(Near(state.feetPosition.y, 15.0f), "ceiling clamp moves feet to maximum allowed height");
    Check(Near(state.verticalVelocity, 0.0f), "ceiling clamp clears upward velocity");
    Check(!state.grounded, "ceiling clamp does not mark airborne player grounded");
}

void TestCannotFitClampsToFloor()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{0.0f, 12.0f, 0.0f};
    state.grounded = false;
    state.verticalVelocity = 9.0f;
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 6.0f;
    game::SectorFpsVerticalContext context{true, 10.0f, 14.0f};

    const game::SectorFpsVerticalResult result =
            game::UpdateSectorFpsVerticalPhysics(state, config, context, 1.0f);
    Check(result.cannotFit, "cannot-fit vertical result is reported");
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
    TestForwardMovementIgnoresPitchAndPreservesY();
    TestRunAndWalkSpeeds();
    TestMouseLookRawDeltaAndPitchClamp();
    TestConfigNormalization();
    TestGroundedPlayerSnapsToFloor();
    TestFallingAndLanding();
    TestZeroGravityDoesNotMoveVertically();
    TestCeilingClamp();
    TestCannotFitClampsToFloor();
    TestNoSectorPreservesVerticalState();
    if (failures == 0) {
        std::puts("Sector FPS controller tests passed");
    }
    return failures == 0 ? 0 : 1;
}
