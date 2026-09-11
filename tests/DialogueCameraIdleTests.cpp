#include "game/dialogue/DialogueCameraIdle.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace {
using game::DialogueCameraIdlePhase;
using game::DialogueCameraIdleSettings;
using game::DialogueCameraIdleState;

bool AtRest(const DialogueCameraIdleState& state)
{
    for (size_t i = 0; i < state.offsets.size(); ++i)
        if (state.offsets[i] != 0.0f || state.velocities[i] != 0.0f) return false;
    return state.positionOffsetLocal.x == 0 && state.positionOffsetLocal.y == 0
            && state.positionOffsetLocal.z == 0 && state.rotationDegrees.x == 0
            && state.rotationDegrees.y == 0 && state.rotationDegrees.z == 0;
}

void Step(DialogueCameraIdleState& state, const DialogueCameraIdleSettings& settings,
        float seconds, bool visible = true)
{
    while (seconds > 0) {
        const float dt = std::min(seconds, 1.0f / 120.0f);
        game::UpdateDialogueCameraIdle(state, settings, visible, false, dt);
        seconds -= dt;
    }
}

void TimingVariationAndBounds()
{
    const DialogueCameraIdleSettings settings;
    DialogueCameraIdleState state;
    state.randomState = 12345;
    Step(state, settings, 20, false);
    assert(AtRest(state) && state.phase == DialogueCameraIdlePhase::Inactive);
    game::UpdateDialogueCameraIdle(state, settings, true, false, 0);
    assert(state.phase == DialogueCameraIdlePhase::Waiting && AtRest(state));
    assert(state.duration >= 1 && state.duration <= 3);
    const float initialWait = state.duration;
    Step(state, settings, initialWait - 0.01f);
    assert(AtRest(state) && state.phase == DialogueCameraIdlePhase::Waiting);
    Step(state, settings, 0.02f);
    assert(state.phase == DialogueCameraIdlePhase::Moving);
    const auto firstAmplitudes = state.amplitudes;
    const auto firstPhases = state.phases;
    const auto firstFrequencies = state.frequencies;
    const float firstDuration = state.duration;
    assert(firstDuration >= 2 && firstDuration <= 3);
    const std::array<float, 6> bounds{0.006f, 0.004f, 0.002f, 0.75f, 0.5f, 0.25f};
    int movements = 1;
    int waitingFrames = 0;
    auto previousPhase = state.phase;
    for (int frame = 0; frame < 120 * 60; ++frame) {
        game::UpdateDialogueCameraIdle(state, settings, true, false, 1.0f / 120.0f);
        for (size_t i = 0; i < bounds.size(); ++i)
            assert(std::isfinite(state.offsets[i]) && std::fabs(state.offsets[i]) <= bounds[i]);
        if (state.phase == DialogueCameraIdlePhase::Waiting) {
            assert(AtRest(state));
            assert(state.duration >= 3 && state.duration <= 5);
            ++waitingFrames;
        }
        if (state.phase == DialogueCameraIdlePhase::Moving && previousPhase != state.phase) {
            ++movements;
            assert(state.duration >= 2 && state.duration <= 3);
            assert(state.amplitudes != firstAmplitudes && state.phases != firstPhases);
            assert(state.frequencies != firstFrequencies && state.duration != firstDuration);
        }
        previousPhase = state.phase;
    }
    assert(movements >= 7 && movements <= 12);
    assert(waitingFrames > 120 * 25);
    DialogueCameraIdleState replay;
    replay.randomState = 12345;
    game::UpdateDialogueCameraIdle(replay, settings, true, false, 0);
    assert(replay.duration == initialWait);
    Step(replay, settings, initialWait + 0.01f);
    assert(replay.amplitudes == firstAmplitudes && replay.duration == firstDuration);
}

void SettlingPauseResetAndInvalidTime()
{
    DialogueCameraIdleSettings settings;
    settings.initialWaitSeconds = {1, 1};
    settings.movementSeconds = {2, 2};
    DialogueCameraIdleState state;
    Step(state, settings, 1.8f);
    assert(state.phase == DialogueCameraIdlePhase::Moving && !AtRest(state));
    const auto moving = state;
    for (const float dt : {0.0f, -1.0f, std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()}) {
        game::UpdateDialogueCameraIdle(state, settings, true, false, dt);
        assert(state.elapsed == moving.elapsed && state.offsets == moving.offsets);
    }
    game::UpdateDialogueCameraIdle(state, settings, true, true, 60);
    assert(state.elapsed == moving.elapsed && state.offsets == moving.offsets);
    game::UpdateDialogueCameraIdle(state, settings, false, false, 0);
    assert(state.phase == DialogueCameraIdlePhase::Settling);
    assert(state.offsets == moving.offsets && state.velocities == moving.velocities);
    // Finite-difference slope matches the pre-interruption analytic velocity.
    game::UpdateDialogueCameraIdle(state, settings, false, false, 0.0001f);
    for (size_t i = 0; i < state.offsets.size(); ++i) {
        const float derivative = (state.offsets[i] - moving.offsets[i]) / 0.0001f;
        assert(std::fabs(derivative - moving.velocities[i]) < 0.005f);
    }
    Step(state, settings, 0.5f, false);
    assert(AtRest(state) && state.phase == DialogueCameraIdlePhase::Inactive);
    Step(state, settings, 10, false); // speech outside the menu never restarts it
    assert(AtRest(state));

    Step(state, settings, 1.8f);
    game::UpdateDialogueCameraIdle(state, settings, false, false, 0.1f);
    const auto settling = state;
    game::UpdateDialogueCameraIdle(state, settings, true, false, 0);
    assert(state.phase == DialogueCameraIdlePhase::Settling && state.offsets == settling.offsets);
    Step(state, settings, 0.31f);
    assert(state.phase == DialogueCameraIdlePhase::Waiting && AtRest(state));
    Step(state, settings, 1.5f);
    assert(state.phase == DialogueCameraIdlePhase::Moving);
    const auto seed = state.randomState;
    game::ClearDialogueCameraIdle(state);
    assert(AtRest(state) && state.randomState == seed && !state.menuVisible);
    assert(state.phase == DialogueCameraIdlePhase::Inactive);

    auto normal = moving;
    auto stalled = moving;
    game::UpdateDialogueCameraIdle(normal, settings, true, false, 0.25f);
    game::UpdateDialogueCameraIdle(stalled, settings, true, false, 1000);
    assert(normal.elapsed == stalled.elapsed && normal.offsets == stalled.offsets);
}

void GestureEndpointsAndFrameRate()
{
    DialogueCameraIdleSettings settings;
    settings.initialWaitSeconds = {1, 1};
    settings.movementSeconds = {2, 2};
    DialogueCameraIdleState state;
    for (int i = 0; i < 4; ++i)
        game::UpdateDialogueCameraIdle(state, settings, true, false, 0.25f);
    assert(state.phase == DialogueCameraIdlePhase::Moving && AtRest(state));
    for (int i = 0; i < 7; ++i)
        game::UpdateDialogueCameraIdle(state, settings, true, false, 0.25f);
    game::UpdateDialogueCameraIdle(state, settings, true, false, 0.2499f);
    for (size_t i = 0; i < state.offsets.size(); ++i) {
        assert(std::fabs(state.offsets[i]) < 0.000001f);
        assert(std::fabs(state.velocities[i]) < 0.001f);
    }
    game::UpdateDialogueCameraIdle(state, settings, true, false, 0.001f);
    assert(state.phase == DialogueCameraIdlePhase::Waiting && AtRest(state));

    DialogueCameraIdleState slow, fast;
    for (int frame = 0; frame < 30 * 20; ++frame)
        game::UpdateDialogueCameraIdle(slow, settings, true, false, 1.0f / 30);
    for (int frame = 0; frame < 120 * 20; ++frame)
        game::UpdateDialogueCameraIdle(fast, settings, true, false, 1.0f / 120);
    assert(slow.phase == fast.phase && slow.randomState == fast.randomState);
    for (size_t i = 0; i < slow.offsets.size(); ++i)
        assert(std::fabs(slow.offsets[i] - fast.offsets[i]) < 0.0001f);
}
}

void RunDialogueCameraIdleTests()
{
    TimingVariationAndBounds();
    SettlingPauseResetAndInvalidTime();
    GestureEndpointsAndFrameRate();
}
