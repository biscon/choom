#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorFreeflyController.h"
#include "game/PlayerStamina.h"
#include "game/PlayerHitCamera.h"

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

bool Near(Vector2 a, Vector2 b, float epsilon = 0.0001f)
{
    return Near(a.x, b.x, epsilon)
            && Near(a.y, b.y, epsilon);
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
    game::SectorViewPose cameraPose{Vector3{10.0f, 2.2f, 3.0f}, 0.25f, -0.5f};
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
    const game::SectorViewPose visualPose =
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
    const game::SectorViewPose visualPose =
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
    const float landingDipOffsetY = -0.06f;
    const game::SectorViewPose pose =
            game::SectorFpsControllerVisualPose(
                    state,
                    config,
                    visualStepOffsetY,
                    headBobOffset,
                    landingDipOffsetY);

    Check(Near(pose.position, Vector3{2.02f, 5.28f, 3.03f}),
          "headbob and landing dip layer on top of physics eye and visual step offset");
    Check(Near(state.feetPosition, originalFeet), "visual pose offsets do not mutate physics feet");
}

void TestCameraRecoilPoseComposition()
{
    const game::SectorViewPose base{
            Vector3{2.0f, 3.0f, 4.0f}, 0.25f, 0.1f, 0.0f};
    const Vector3 recoilDegrees{0.4f, -0.15f, 0.07f};
    const game::SectorViewPose effective =
            game::ApplySectorFpsViewRotationOffset(base, recoilDegrees);

    Check(Near(base.yawRadians, 0.25f)
                    && Near(base.pitchRadians, 0.1f)
                    && Near(base.rollRadians, 0.0f),
          "camera recoil composition leaves base mouse-look pose unchanged");
    Check(Near(effective.pitchRadians, base.pitchRadians + 0.4f * DEG2RAD),
          "positive pitch recoil composes as an upward effective-view kick");
    Check(Near(effective.yawRadians, base.yawRadians - 0.15f * DEG2RAD),
          "yaw recoil composes into effective aim");
    Check(Near(effective.rollRadians, 0.07f * DEG2RAD),
          "roll recoil composes into presentation");

    const game::SectorViewPose withoutRoll =
            game::ApplySectorFpsViewRotationOffset(
                    base, Vector3{0.4f, -0.15f, 0.0f});
    Check(Near(game::SectorViewForward(effective),
                    game::SectorViewForward(withoutRoll)),
          "roll recoil does not alter the centre-screen forward ray");
    Check(!Near(game::SectorViewUp(effective), game::SectorViewUp(withoutRoll)),
          "roll recoil rotates the rendered camera up vector");
    Check(Near(Vector3DotProduct(
                    game::SectorViewForward(effective),
                    game::SectorViewUp(effective)), 0.0f, 0.0002f),
          "rolled camera basis remains orthogonal");

    const Vector3 preImpulseDirection = game::SectorViewForward(effective);
    const game::SectorViewPose nextEffective =
            game::ApplySectorFpsViewRotationOffset(
                    base,
                    Vector3Add(recoilDegrees, Vector3{0.4f, 0.1f, 0.0f}));
    Check(Near(preImpulseDirection, game::SectorViewForward(effective)),
          "current shot direction remains the pre-impulse effective direction");
    Check(!Near(preImpulseDirection, game::SectorViewForward(nextEffective)),
          "a subsequent shot sees the accumulated recoil direction");

    game::SectorFpsControllerState lookState;
    lookState.yawRadians = base.yawRadians;
    lookState.pitchRadians = base.pitchRadians;
    game::SectorFpsControllerInput lookInput;
    lookInput.mouseLookEnabled = true;
    lookInput.mouseDelta = Vector2{5.0f, -3.0f};
    game::UpdateSectorFpsMouseLook(
            lookState, game::SectorFpsControllerConfig{}, lookInput);
    const game::SectorViewPose movedBase = game::SectorFpsControllerPose(
            lookState, game::SectorFpsControllerConfig{});
    const game::SectorViewPose movedEffective =
            game::ApplySectorFpsViewRotationOffset(movedBase, recoilDegrees);
    Check(!Near(movedEffective.yawRadians, effective.yawRadians)
                    && !Near(movedEffective.pitchRadians, effective.pitchRadians),
          "mouse look remains responsive while recoil is active");

    game::SectorViewPose pitchLimit = base;
    pitchLimit.pitchRadians = game::ClampSectorFpsPitch(1000.0f);
    const game::SectorViewPose limitedEffective =
            game::ApplySectorFpsViewRotationOffset(
                    pitchLimit, Vector3{45.0f, 0.0f, 0.0f});
    Check(Near(limitedEffective.pitchRadians, pitchLimit.pitchRadians),
          "effective recoil preserves the existing pitch limit");
    const game::SectorViewPose finiteEffective =
            game::ApplySectorFpsViewRotationOffset(
                    base,
                    Vector3{NAN, INFINITY, -INFINITY});
    Check(std::isfinite(finiteEffective.pitchRadians)
                    && std::isfinite(finiteEffective.yawRadians)
                    && std::isfinite(finiteEffective.rollRadians),
          "non-finite recoil offsets cannot corrupt the camera pose");
}

void TestPlayerHitCameraDirectionalImpulseAndRecovery()
{
    game::NpcAttackCameraImpactDefinition definition;
    definition.pitchKickDegrees = 3.0f;
    definition.rollKickDegrees = 4.0f;
    definition.maxPitchDegrees = 5.0f;
    definition.maxRollDegrees = 6.0f;
    definition.springFrequencyHz = 4.0f;
    definition.springDampingRatio = 0.75f;

    game::PlayerHitCameraState camera;
    game::ApplyPlayerHitCameraImpulse(
            camera, definition, Vector2{-1.0f, 0.0f}, 0.0f);
    Check(Near(camera.rotationDegrees, Vector3{3.0f, 0.0f, 0.0f}),
          "an attacker in front produces a positive pitch kick");

    game::ClearPlayerHitCamera(camera);
    game::ApplyPlayerHitCameraImpulse(
            camera, definition, Vector2{1.0f, 0.0f}, 0.0f);
    Check(Near(camera.rotationDegrees, Vector3{-3.0f, 0.0f, 0.0f}),
          "an attacker behind produces the opposite pitch kick");

    game::ClearPlayerHitCamera(camera);
    game::ApplyPlayerHitCameraImpulse(
            camera, definition, Vector2{0.0f, -1.0f}, 0.0f);
    Check(Near(camera.rotationDegrees, Vector3{0.0f, 0.0f, 4.0f}),
          "an attacker on the right produces a positive roll kick");
    game::ClearPlayerHitCamera(camera);
    game::ApplyPlayerHitCameraImpulse(
            camera, definition, Vector2{0.0f, 1.0f}, 0.0f);
    Check(Near(camera.rotationDegrees, Vector3{0.0f, 0.0f, -4.0f}),
          "an attacker on the left produces the opposite roll kick");

    game::ClearPlayerHitCamera(camera);
    game::ApplyPlayerHitCameraImpulse(
            camera, definition, Vector2{0.0f, -2.0f}, PI * 0.5f);
    Check(Near(camera.rotationDegrees.x, 3.0f)
                  && Near(camera.rotationDegrees.z, 0.0f),
          "directional hit camera mapping follows player yaw");

    game::ClearPlayerHitCamera(camera);
    game::ApplyPlayerHitCameraImpulse(
            camera, definition, Vector2{}, 0.0f);
    Check(Near(camera.rotationDegrees.x, 3.0f),
          "an overlapping attacker falls back to a visible pitch kick");
    game::ApplyPlayerHitCameraImpulse(
            camera, definition, Vector2{-1.0f, 0.0f}, 0.0f);
    Check(Near(camera.rotationDegrees.x, 5.0f),
          "repeated hit camera impulses clamp at the authored limit");
    game::ApplyPlayerHitCameraImpulse(
            camera, definition, Vector2{1.0f, 0.0f}, 0.0f);
    Check(Near(camera.rotationDegrees.x, 2.0f),
          "opposite hit directions cancel accumulated camera rotation");

    game::NpcAttackCameraImpactDefinition disabled = definition;
    disabled.enabled = false;
    const Vector3 beforeDisabled = camera.rotationDegrees;
    game::ApplyPlayerHitCameraImpulse(
            camera, disabled, Vector2{-1.0f, 0.0f}, 0.0f);
    Check(Near(camera.rotationDegrees, beforeDisabled),
          "a disabled attack adds no camera impulse without clearing recovery");

    const Vector3 beforeZeroDt = camera.rotationDegrees;
    game::UpdatePlayerHitCamera(camera, 0.0f);
    Check(Near(camera.rotationDegrees, beforeZeroDt),
          "zero dt does not advance the hit camera spring");
    bool rebounded = false;
    for (int step = 0; step < 240; ++step) {
        game::UpdatePlayerHitCamera(camera, 1.0f / 240.0f);
        rebounded = rebounded || camera.rotationDegrees.x < 0.0f;
    }
    Check(rebounded,
          "the default underdamped hit camera response includes a small rebound");
    Check(std::fabs(camera.rotationDegrees.x) < 0.001f
                  && std::fabs(camera.rotationDegrees.z) < 0.001f,
          "the hit camera spring recovers to its neutral pose");

    camera.rotationDegrees.x = NAN;
    game::UpdatePlayerHitCamera(camera, 1.0f / 60.0f);
    Check(Near(camera.rotationDegrees, Vector3{})
                  && Near(camera.rotationVelocityDegrees, Vector3{}),
          "non-finite hit camera state resets safely");
}

void TestFootstepCadenceUsesResolvedTravel()
{
    game::SectorFpsControllerConfig config;
    config.walkSpeed = 6.0f;
    config.runSpeed = 12.0f;
    game::SectorFpsFootstepCadenceState state;

    Check(Near(game::SectorFpsFootstepStrideDistance(config, 6.0f), 1.5f),
          "walking footstep stride uses walking spacing");
    Check(Near(game::SectorFpsFootstepStrideDistance(config, 12.0f), 2.4f),
          "running footstep stride uses running spacing");
    Check(!game::UpdateSectorFpsFootstepCadence(state, config, true, 1.0f, 6.0f),
          "partial resolved stride does not trigger a footstep");
    Check(game::UpdateSectorFpsFootstepCadence(state, config, true, 0.5f, 6.0f),
          "completed resolved stride triggers one footstep");
    Check(!game::UpdateSectorFpsFootstepCadence(state, config, true, 0.0f, 6.0f),
          "stationary frame does not trigger a footstep");
    Check(game::UpdateSectorFpsFootstepCadence(state, config, true, 10.0f, 6.0f),
          "large resolved movement emits at most one footstep event");
    Check(state.accumulatedDistanceWorld < 1.5f,
          "large resolved movement discards burst-producing complete strides");
    Check(!game::UpdateSectorFpsFootstepCadence(state, config, false, 2.0f, 6.0f),
          "inactive or airborne cadence does not trigger");
    Check(Near(state.accumulatedDistanceWorld, 0.0f),
          "inactive or airborne cadence resets accumulated travel");
}

void TestFrameEventsReportSuccessfulJumpAndLanding()
{
    game::SectorFpsVerticalResult vertical;
    game::SectorFpsFrameEvents events = game::BuildSectorFpsFrameEvents(
            true,
            vertical);
    Check(events.jumped, "successful jump is exposed as a frame event");
    Check(!events.landed, "jump frame does not report a landing");

    vertical.transition = game::SectorFpsVerticalTransition::Landed;
    vertical.landingImpactSpeed = 7.5f;
    events = game::BuildSectorFpsFrameEvents(false, vertical);
    Check(!events.jumped, "non-jump frame clears jump event");
    Check(events.landed, "landed vertical transition is exposed as a frame event");
    Check(Near(events.landingImpactSpeed, 7.5f),
          "landing event preserves impact speed");

    vertical.transition = game::SectorFpsVerticalTransition::SnappedDown;
    events = game::BuildSectorFpsFrameEvents(false, vertical);
    Check(!events.landed, "snapped-down floor transition is not an airborne landing");
    Check(Near(events.landingImpactSpeed, 0.0f),
          "non-landing transition has no impact speed");
}

void TestLandingDipAmountCurve()
{
    constexpr float MinImpact = 0.5f;
    constexpr float FullImpact = 12.0f;
    constexpr float MaxDip = 0.45f;
    constexpr float CurvePower = 2.25f;

    Check(Near(game::ComputeSectorFpsLandingDipAmount(
            0.25f, MinImpact, FullImpact, MaxDip, CurvePower), 0.0f),
          "landing dip curve ignores impact below minimum");
    Check(Near(game::ComputeSectorFpsLandingDipAmount(
            MinImpact, MinImpact, FullImpact, MaxDip, CurvePower), 0.0f),
          "landing dip curve is zero at minimum impact");

    const float lowJumpDip = game::ComputeSectorFpsLandingDipAmount(
            5.5f, MinImpact, FullImpact, MaxDip, CurvePower);
    const float oneMeterDip = game::ComputeSectorFpsLandingDipAmount(
            7.1f, MinImpact, FullImpact, MaxDip, CurvePower);
    const float highFallDip = game::ComputeSectorFpsLandingDipAmount(
            10.0f, MinImpact, FullImpact, MaxDip, CurvePower);
    Check(Near(lowJumpDip, 0.0691f, 0.0002f),
          "landing dip curve gives subtle partial dip for default jump speed");
    Check(Near(oneMeterDip, 0.1290f, 0.0002f),
          "landing dip curve gives moderate dip for one-meter impact speed");
    Check(lowJumpDip > 0.0f && lowJumpDip < oneMeterDip && oneMeterDip < highFallDip,
          "landing dip curve is monotonic across low, mid, and high impact speeds");

    Check(Near(game::ComputeSectorFpsLandingDipAmount(
            FullImpact, MinImpact, FullImpact, MaxDip, CurvePower), MaxDip),
          "landing dip curve reaches max at full impact speed");
    Check(Near(game::ComputeSectorFpsLandingDipAmount(
            15.8f, MinImpact, FullImpact, MaxDip, CurvePower), MaxDip),
          "landing dip curve clamps above full impact speed");
}

void TestLandingDipAmountInvalidInputs()
{
    constexpr float MinImpact = 0.5f;
    constexpr float FullImpact = 12.0f;
    constexpr float MaxDip = 0.45f;
    constexpr float CurvePower = 2.25f;

    const float invalidResults[] = {
            game::ComputeSectorFpsLandingDipAmount(INFINITY, MinImpact, FullImpact, MaxDip, CurvePower),
            game::ComputeSectorFpsLandingDipAmount(NAN, MinImpact, FullImpact, MaxDip, CurvePower),
            game::ComputeSectorFpsLandingDipAmount(7.1f, INFINITY, FullImpact, MaxDip, CurvePower),
            game::ComputeSectorFpsLandingDipAmount(7.1f, MinImpact, INFINITY, MaxDip, CurvePower),
            game::ComputeSectorFpsLandingDipAmount(7.1f, MinImpact, MinImpact, MaxDip, CurvePower),
            game::ComputeSectorFpsLandingDipAmount(7.1f, FullImpact, MinImpact, MaxDip, CurvePower),
            game::ComputeSectorFpsLandingDipAmount(7.1f, MinImpact, FullImpact, INFINITY, CurvePower),
            game::ComputeSectorFpsLandingDipAmount(7.1f, MinImpact, FullImpact, -1.0f, CurvePower),
            game::ComputeSectorFpsLandingDipAmount(7.1f, MinImpact, FullImpact, MaxDip, INFINITY),
            game::ComputeSectorFpsLandingDipAmount(7.1f, MinImpact, FullImpact, MaxDip, 0.0f),
    };

    for (const float result : invalidResults) {
        Check(std::isfinite(result) && Near(result, 0.0f),
              "landing dip curve returns finite zero for invalid inputs");
    }
}

void TestLandingDipTriggerDecayAndRobustness()
{
    game::SectorFpsLandingDipState landingDip;
    game::SectorFpsVerticalResult result;
    result.hasSector = true;
    result.transition = game::SectorFpsVerticalTransition::Landed;
    result.landingImpactSpeed = 5.0f;

    game::UpdateSectorFpsLandingDip(landingDip, result, 0.0f);
    Check(Near(landingDip.offsetY, 0.0f), "zero dt does not create landing dip");

    landingDip.offsetY = -0.05f;
    result.transition = game::SectorFpsVerticalTransition::Landed;
    result.landingImpactSpeed = 12.0f;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.0f);
    Check(Near(landingDip.offsetY, -0.05f), "zero dt does not replace existing landing dip");

    result.transition = game::SectorFpsVerticalTransition::Landed;
    result.landingImpactSpeed = 7.1f;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.016f);
    Check(landingDip.offsetY < 0.0f, "landing dip creates negative camera offset");
    Check(Near(landingDip.offsetY, -0.1290f, 0.0002f), "landing dip follows impact range curve");

    result.landingImpactSpeed = 12.0f;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.016f);
    Check(Near(landingDip.offsetY, -0.45f), "landing dip reaches max at full impact speed");

    result.landingImpactSpeed = 15.8f;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.016f);
    Check(Near(landingDip.offsetY, -0.45f), "landing dip clamps above full impact speed");

    const float offsetAfterLanding = landingDip.offsetY;
    result.transition = game::SectorFpsVerticalTransition::StayedGrounded;
    result.landingImpactSpeed = 0.0f;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.016f);
    Check(landingDip.offsetY > offsetAfterLanding && landingDip.offsetY < 0.0f,
          "landing dip decays toward zero");

    landingDip.offsetY = -0.05f;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.0f);
    Check(Near(landingDip.offsetY, -0.05f), "zero dt does not decay landing dip");

    landingDip.offsetY = -0.00005f;
    game::UpdateSectorFpsLandingDip(landingDip, result, 1.0f);
    Check(Near(landingDip.offsetY, 0.0f), "tiny landing dip snaps to zero");

    result.transition = game::SectorFpsVerticalTransition::Landed;
    result.landingImpactSpeed = 0.25f;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.016f);
    Check(Near(landingDip.offsetY, 0.0f), "tiny landing impact does not create dip");

    landingDip.offsetY = -0.03f;
    result.landingImpactSpeed = INFINITY;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.016f);
    Check(Near(landingDip.offsetY, 0.0f), "non-finite landing impact clears and skips dip");

    landingDip.offsetY = INFINITY;
    result.transition = game::SectorFpsVerticalTransition::StayedGrounded;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.0f);
    Check(Near(landingDip.offsetY, 0.0f), "non-finite stored landing dip clears");
}

void TestLandingDipClearTransitions()
{
    game::SectorFpsLandingDipState landingDip;
    game::SectorFpsVerticalResult result;
    result.hasSector = true;

    const game::SectorFpsVerticalTransition clearTransitions[] = {
            game::SectorFpsVerticalTransition::StartedDrop,
            game::SectorFpsVerticalTransition::CannotFit
    };
    for (const game::SectorFpsVerticalTransition transition : clearTransitions) {
        landingDip.offsetY = -0.05f;
        result.transition = transition;
        game::UpdateSectorFpsLandingDip(landingDip, result, 0.016f);
        Check(Near(landingDip.offsetY, 0.0f), "landing dip clears on vertical reset transitions");
    }

    landingDip.offsetY = -0.05f;
    result.hasSector = false;
    result.transition = game::SectorFpsVerticalTransition::None;
    game::UpdateSectorFpsLandingDip(landingDip, result, 0.016f);
    Check(Near(landingDip.offsetY, 0.0f), "landing dip clears with no sector");

    landingDip.offsetY = -0.05f;
    game::ClearSectorFpsLandingDip(landingDip);
    Check(Near(landingDip.offsetY, 0.0f), "landing dip clears on explicit jump or mode reset");
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

    game::SectorFpsControllerInput slowedInput;
    slowedInput.moveForward = true;
    slowedInput.run = true;
    slowedInput.movementSpeedScale = 0.5f;
    slowedInput.externalHorizontalMovementDelta = {0.0f, 1.25f};
    const Vector2 slowedDelta = game::ComputeSectorFpsHorizontalMovementDelta(
            game::SectorFpsControllerState{}, config, slowedInput, 1.0f);
    Check(Near(slowedDelta, Vector2{6.0f, 1.25f}),
          "movement speed scaling leaves external collision movement unscaled");

    game::SectorFpsControllerInput strafeInput;
    strafeInput.strafeRight = true;
    strafeInput.run = true;
    const Vector2 strafeDelta = game::ComputeSectorFpsHorizontalMovementDelta(
            game::SectorFpsControllerState{},
            config,
            strafeInput,
            1.0f);
    Check(Near(Vector2Length(strafeDelta), 12.0f),
          "pure strafing may use run speed");

    game::SectorFpsControllerInput backwardInput;
    backwardInput.moveBackward = true;
    backwardInput.run = true;
    const Vector2 backwardDelta = game::ComputeSectorFpsHorizontalMovementDelta(
            game::SectorFpsControllerState{},
            config,
            backwardInput,
            1.0f);
    Check(Near(backwardDelta, Vector2{-6.0f, 0.0f}),
          "backpedalling is capped at walk speed while run is held");
    Check(!game::SectorFpsInputUsesRunSpeed(backwardInput),
          "backward input is not classified as sprinting");

    backwardInput.strafeRight = true;
    const Vector2 backwardDiagonal =
            game::ComputeSectorFpsHorizontalMovementDelta(
                    game::SectorFpsControllerState{},
                    config,
                    backwardInput,
                    1.0f);
    Check(Near(Vector2Length(backwardDiagonal), 6.0f),
          "backward diagonal movement is capped at walk speed");

    game::SectorFpsControllerInput opposingInput;
    opposingInput.moveForward = true;
    opposingInput.moveBackward = true;
    opposingInput.strafeRight = true;
    opposingInput.run = true;
    Check(game::SectorFpsInputUsesRunSpeed(opposingInput),
          "opposing forward inputs with net sideways motion may sprint");
}

void TestPlayerStaminaConsumptionRecoveryAndLockout()
{
    game::PlayerStaminaApplicationSettings settings;
    game::PlayerStamina stamina = game::MakePlayerStamina(settings);
    Check(Near(stamina.current, 100.0f)
                  && Near(game::PlayerStaminaRatio(stamina), 1.0f),
          "new player stamina starts full");

    game::UpdatePlayerStamina(stamina, settings, true, false, 1.0f);
    Check(Near(stamina.current, 80.0f),
          "sprinting drains configured stamina per second");
    game::UpdatePlayerStamina(stamina, settings, false, true, 1.0f / 60.0f);
    Check(Near(stamina.current, 60.0f),
          "a successful jump deducts its fixed stamina cost");
    game::UpdatePlayerStamina(stamina, settings, false, false, 2.0f);
    Check(Near(stamina.current, 85.0f),
          "a frame without consuming actions regenerates stamina");

    stamina.current = 25.0f;
    game::UpdatePlayerStamina(stamina, settings, true, true, 0.5f);
    Check(Near(stamina.current, 0.0f) && stamina.exhausted,
          "combined sprint and jump costs clamp at zero and exhaust the player");
    Check(!game::CanPlayerStaminaSprint(stamina)
                  && !game::CanPlayerStaminaJump(stamina, settings),
          "exhaustion locks sprint and jump");

    game::UpdatePlayerStamina(stamina, settings, false, false, 1.0f);
    Check(Near(stamina.current, 12.5f) && stamina.exhausted,
          "recovery below the hysteresis threshold stays locked");
    game::UpdatePlayerStamina(stamina, settings, false, false, 0.6f);
    Check(Near(stamina.current, 20.0f) && !stamina.exhausted,
          "reaching the recovery threshold unlocks stamina actions");
    Check(game::CanPlayerStaminaJump(stamina, settings),
          "default recovery threshold restores enough stamina for one jump");

    stamina.current = 10.0f;
    stamina.exhausted = false;
    Check(!game::CanPlayerStaminaJump(stamina, settings),
          "jump is rejected when its cost is unaffordable before exhaustion");
    game::UpdatePlayerStamina(stamina, settings, false, false, 1.0f);
    Check(Near(stamina.current, 22.5f),
          "a rejected consuming action leaves the frame free to regenerate");
}

void TestPlayerWindedCameraAndBreathingEnvelope()
{
    game::PlayerWindedCameraApplicationSettings cameraSettings;
    cameraSettings.responseSeconds = 0.01f;
    game::PlayerWindedCameraState camera;
    game::UpdatePlayerWindedCamera(camera, cameraSettings, 0.0f, 0.25f);
    Check(camera.intensity > 0.99f,
          "empty stamina drives the winded camera toward full intensity");
    Check(std::isfinite(camera.verticalOffsetWorld)
                  && std::isfinite(camera.pitchOffsetDegrees)
                  && std::fabs(camera.verticalOffsetWorld) > 0.0f,
          "winded camera produces finite procedural offsets");

    game::UpdatePlayerWindedCamera(camera, cameraSettings, 1.0f, 0.25f);
    Check(camera.intensity < 0.01f,
          "recovered stamina smoothly removes the winded camera effect");
    cameraSettings.enabled = false;
    game::UpdatePlayerWindedCamera(camera, cameraSettings, 0.0f, 0.25f);
    Check(Near(camera.intensity, 0.0f)
                  && Near(camera.verticalOffsetWorld, 0.0f)
                  && Near(camera.pitchOffsetDegrees, 0.0f),
          "disabled winded camera clears all visual offsets");

    game::PlayerBreathingAudioApplicationSettings audioSettings;
    float volume = game::AdvancePlayerBreathingAudioVolume(
            0.0f,
            audioSettings,
            0.19f,
            0.0f);
    Check(Near(volume, 0.75f),
          "breathing audio starts at configured volume below threshold");
    volume = game::AdvancePlayerBreathingAudioVolume(
            volume,
            audioSettings,
            0.20f,
            1.0f);
    Check(Near(volume, 0.375f),
          "breathing audio fades linearly after reaching threshold");
    volume = game::AdvancePlayerBreathingAudioVolume(
            volume,
            audioSettings,
            0.19f,
            0.1f);
    Check(Near(volume, 0.75f),
          "dropping below threshold during fade restores full loop volume");
    volume = game::AdvancePlayerBreathingAudioVolume(
            volume,
            audioSettings,
            0.50f,
            2.0f);
    Check(Near(volume, 0.0f),
          "breathing audio reaches silence after the configured fade duration");
}

void TestCrouchToggleTransitionAndEffectiveDimensions()
{
    game::SectorFpsControllerState state;
    state.feetPosition = Vector3{2.0f, 3.0f, 4.0f};
    state.grounded = true;
    game::SectorFpsControllerConfig config;
    config.eyeHeight = 1.2f;
    config.playerHeight = 1.6f;

    Check(game::TryToggleSectorFpsCrouch(state, true),
          "grounded crouch toggle starts crouching");
    Check(state.crouchTargeted, "crouch toggle records crouched target");
    game::UpdateSectorFpsCrouch(
            state,
            true,
            game::DefaultSectorFpsCrouchTransitionDuration() * 0.5f);
    Check(Near(state.crouchAmount, 0.5f), "crouch transition reaches midpoint by half duration");
    Check(Near(game::SectorFpsCrouchBlend(state), 0.5f),
          "crouch transition smoothstep is centered at half duration");
    game::SectorFpsControllerConfig effective =
            game::EffectiveSectorFpsControllerConfig(state, config);
    Check(Near(effective.eyeHeight, 0.975f)
                  && Near(effective.playerHeight, 1.3f),
          "mid-crouch smoothly interpolates eye and collider height");
    Check(Near(state.feetPosition, Vector3{2.0f, 3.0f, 4.0f}),
          "crouch transition does not move physical feet");

    game::UpdateSectorFpsCrouch(
            state,
            true,
            game::DefaultSectorFpsCrouchTransitionDuration() * 0.5f);
    effective = game::EffectiveSectorFpsControllerConfig(state, config);
    Check(Near(state.crouchAmount, 1.0f)
                  && Near(effective.eyeHeight, 0.75f)
                  && Near(effective.playerHeight, 1.0f),
          "full crouch uses derived 62.5 percent dimensions");

    Check(!game::TryToggleSectorFpsCrouch(state, false),
          "blocked standing request is rejected");
    Check(state.crouchTargeted && Near(state.crouchAmount, 1.0f),
          "rejected standing request remains fully crouched");
    Check(game::TryToggleSectorFpsCrouch(state, true),
          "clear standing request starts standing transition");
    game::UpdateSectorFpsCrouch(
            state,
            true,
            game::DefaultSectorFpsCrouchTransitionDuration());
    Check(!state.crouchTargeted && Near(state.crouchAmount, 0.0f),
          "standing transition reaches standing endpoint");
}

void TestCrouchGroundingReversalAndResetRules()
{
    game::SectorFpsControllerState state;
    Check(!game::TryToggleSectorFpsCrouch(state, true),
          "airborne crouch toggle is ignored");
    Check(!state.crouchTargeted && Near(state.crouchAmount, 0.0f),
          "ignored airborne crouch toggle preserves standing state");

    state.grounded = true;
    state.crouchTargeted = false;
    state.crouchAmount = 0.5f;
    game::UpdateSectorFpsCrouch(state, false, 0.01f);
    Check(state.crouchTargeted && state.crouchAmount > 0.5f,
          "lost standing clearance reverses transition toward crouched");

    state.crouchAmount = NAN;
    game::UpdateSectorFpsCrouch(state, true, 0.0f);
    Check(Near(state.crouchAmount, 1.0f),
          "non-finite crouch state recovers to its target endpoint");
    game::ResetSectorFpsCrouch(state);
    Check(!state.crouchTargeted && Near(state.crouchAmount, 0.0f),
          "crouch reset restores standing state");
}

void TestCrouchedMovementSpeedAndVerticalFit()
{
    game::SectorFpsControllerConfig config;
    config.walkSpeed = 6.0f;
    config.runSpeed = 12.0f;
    config.eyeHeight = 1.2f;
    config.playerHeight = 1.6f;
    game::SectorFpsControllerInput input;
    input.moveForward = true;
    input.run = true;

    game::SectorFpsControllerState crouched;
    crouched.crouchTargeted = true;
    crouched.crouchAmount = 1.0f;
    const Vector2 crouchedDelta = game::ComputeSectorFpsHorizontalMovementDelta(
            crouched,
            config,
            input,
            1.0f);
    Check(Near(crouchedDelta, Vector2{3.0f, 0.0f}),
          "fully crouched run input moves at half walking speed");

    game::SectorFpsControllerState midpoint = crouched;
    midpoint.crouchAmount = 0.5f;
    const Vector2 midpointDelta = game::ComputeSectorFpsHorizontalMovementDelta(
            midpoint,
            config,
            input,
            1.0f);
    Check(Near(midpointDelta, Vector2{7.5f, 0.0f}),
          "crouch transition smoothly blends running toward crouched speed");

    crouched.grounded = true;
    const game::SectorFpsVerticalResult crouchedResult =
            game::UpdateSectorFpsVerticalPhysics(
                    crouched,
                    config,
                    game::SectorFpsVerticalContext{true, 0.0f, 1.2f},
                    0.0f);
    Check(!crouchedResult.cannotFit,
          "crouched effective collider fits beneath a low ceiling");

    game::SectorFpsControllerState standing;
    standing.grounded = true;
    const game::SectorFpsVerticalResult standingResult =
            game::UpdateSectorFpsVerticalPhysics(
                    standing,
                    config,
                    game::SectorFpsVerticalContext{true, 0.0f, 1.2f},
                    0.0f);
    Check(standingResult.cannotFit,
          "standing collider still rejects the same low ceiling");
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

void TestMouseLookCaptureWarmup()
{
    game::SectorFreeflyControllerState capture;
    capture.mouseLookEnabled = true;
    capture.mouseLookWarmupFrames = 2;
    game::SectorFpsControllerState view;
    game::SectorFpsControllerInput input;
    input.mouseDelta = Vector2{1000.0f, -1000.0f};

    input.mouseLookEnabled =
            game::AdvanceSectorFreeflyMouseLookCapture(capture);
    game::UpdateSectorFpsMouseLook(
            view, game::SectorFpsControllerConfig{}, input);
    Check(Near(view.yawRadians, 0.0f) && Near(view.pitchRadians, 0.0f),
          "first mouse recapture frame discards its delta");
    input.mouseLookEnabled =
            game::AdvanceSectorFreeflyMouseLookCapture(capture);
    game::UpdateSectorFpsMouseLook(
            view, game::SectorFpsControllerConfig{}, input);
    Check(Near(view.yawRadians, 0.0f) && Near(view.pitchRadians, 0.0f),
          "second mouse recapture frame discards its delta");

    input.mouseDelta = Vector2{10.0f, -5.0f};
    input.mouseLookEnabled =
            game::AdvanceSectorFreeflyMouseLookCapture(capture);
    game::UpdateSectorFpsMouseLook(
            view, game::SectorFpsControllerConfig{}, input);
    Check(input.mouseLookEnabled
                  && Near(view.yawRadians, 0.03f)
                  && Near(view.pitchRadians, 0.015f),
          "mouse look resumes normally after recapture warmup");

    capture.mouseLookEnabled = false;
    capture.mouseLookWarmupFrames = 2;
    Check(!game::AdvanceSectorFreeflyMouseLookCapture(capture)
                  && capture.mouseLookWarmupFrames == 2,
          "disabled mouse look does not consume recapture warmup");
    capture.mouseLookEnabled = true;
    Check(!game::AdvanceSectorFreeflyMouseLookCapture(capture)
                  && capture.mouseLookWarmupFrames == 1,
          "a later recapture starts a fresh warmup");
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
    Check(Near(result.landingImpactSpeed, 25.0f), "falling landing reports impact speed before velocity clears");
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
    Check(result.landingImpactSpeed > 0.0f, "jump landing reports downward impact speed");
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
    TestCameraRecoilPoseComposition();
    TestPlayerHitCameraDirectionalImpulseAndRecovery();
    TestFootstepCadenceUsesResolvedTravel();
    TestFrameEventsReportSuccessfulJumpAndLanding();
    TestLandingDipAmountCurve();
    TestLandingDipAmountInvalidInputs();
    TestLandingDipTriggerDecayAndRobustness();
    TestLandingDipClearTransitions();
    TestForwardMovementIgnoresPitchAndPreservesY();
    TestRunAndWalkSpeeds();
    TestPlayerStaminaConsumptionRecoveryAndLockout();
    TestPlayerWindedCameraAndBreathingEnvelope();
    TestCrouchToggleTransitionAndEffectiveDimensions();
    TestCrouchGroundingReversalAndResetRules();
    TestCrouchedMovementSpeedAndVerticalFit();
    TestMouseLookRawDeltaAndPitchClamp();
    TestMouseLookCaptureWarmup();
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
