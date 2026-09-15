#include "engine/render/ScreenShake.h"
#include "sector_demo/SectorFpsController.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace {

bool Near(Vector3 a, Vector3 b, float epsilon = 0.00001f)
{
    return std::fabs(a.x - b.x) < epsilon && std::fabs(a.y - b.y) < epsilon
            && std::fabs(a.z - b.z) < epsilon;
}

float Magnitude(Vector3 value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

void TimingNoiseAndEnvelopes()
{
    for (auto type : {engine::ScreenShakeType::Rumble, engine::ScreenShakeType::Impact}) {
        engine::ScreenShakeState single, split;
        const auto handle = engine::StartScreenShake(single, 1.0f, 2.0, type);
        engine::StartScreenShake(split, 1.0f, 2.0, type);
        assert(handle.token != 0 && Near(single.rotationDegrees, {}));
        engine::UpdateScreenShake(single, 0.5);
        for (int i = 0; i < 60; ++i) engine::UpdateScreenShake(split, 1.0 / 120.0);
        assert(Near(single.rotationDegrees, split.rotationDegrees));
        assert(Magnitude(single.rotationDegrees) > 0.01f);
        const Vector3 frozen = single.rotationDegrees;
        for (double dt : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                    std::numeric_limits<double>::quiet_NaN()})
            engine::UpdateScreenShake(single, dt);
        assert(Near(frozen, single.rotationDegrees));
        assert(single.instances[0].elapsedSeconds == 0.5);
        engine::UpdateScreenShake(single, 1.49999);
        assert(Magnitude(single.rotationDegrees) < 0.0001f);
        engine::UpdateScreenShake(single, 0.01);
        assert(!engine::IsScreenShakeActive(single, handle));
        assert(Near(single.rotationDegrees, {}));

        // Compare envelopes using exactly the same phase/seed within each style.
        engine::ScreenShakeState shortShake, longShake;
        engine::StartScreenShake(shortShake, 1, 2, type);
        engine::StartScreenShake(longShake, 1, 4, type);
        engine::UpdateScreenShake(shortShake, 1.0);
        engine::UpdateScreenShake(longShake, 1.0);
        if (type == engine::ScreenShakeType::Rumble)
            assert(Near(shortShake.rotationDegrees, longShake.rotationDegrees));
        else
            assert(Magnitude(shortShake.rotationDegrees) < Magnitude(longShake.rotationDegrees));

        engine::ScreenShakeState continuous;
        engine::StartScreenShake(continuous, 1, 4, type);
        engine::UpdateScreenShake(continuous, 0.000001);
        assert(Magnitude(continuous.rotationDegrees) < 0.0001f);
        engine::UpdateScreenShake(continuous, 0.337);
        const Vector3 before = continuous.rotationDegrees;
        engine::UpdateScreenShake(continuous, 0.000001);
        assert(Near(before, continuous.rotationDegrees, 0.001f));
    }
}

void OverlapBoundsCancellationAndReset()
{
    engine::ScreenShakeState state;
    const auto first = engine::StartScreenShake(state, 1, 1);
    const auto second = engine::StartScreenShake(state, 0.5f, 4, engine::ScreenShakeType::Impact);
    engine::UpdateScreenShake(state, 0.2);
    auto firstOnly = state;
    auto secondOnly = state;
    engine::CancelScreenShake(firstOnly, second);
    engine::CancelScreenShake(secondOnly, first);
    const Vector3 a = firstOnly.rotationDegrees, b = secondOnly.rotationDegrees;
    assert(Near(state.rotationDegrees, {a.x + b.x, a.y + b.y, a.z + b.z}));
    assert(engine::CancelScreenShake(state, first));
    assert(Near(state.rotationDegrees, b));
    assert(engine::IsScreenShakeActive(state, second));
    const auto replacement = engine::StartScreenShake(state, 1, 4);
    assert(replacement.token != first.token);
    assert(!engine::CancelScreenShake(state, first));
    engine::ResetScreenShake(state);
    assert(Near(state.rotationDegrees, {}));
    const auto afterReset = engine::StartScreenShake(state, 1, 4);
    assert(afterReset.token != replacement.token);
    assert(!engine::CancelScreenShake(state, replacement));
    for (size_t i = 1; i < engine::kScreenShakeCapacity; ++i)
        assert(engine::StartScreenShake(state, 1, 4).token != 0);
    const auto nextToken = state.nextToken;
    const char* error = nullptr;
    assert(engine::StartScreenShake(state, 1, 4, engine::ScreenShakeType::Rumble, &error).token == 0);
    assert(error != nullptr && state.nextToken == nextToken);
    for (int frame = 0; frame < 480; ++frame) {
        engine::UpdateScreenShake(state, 1.0 / 120.0);
        const auto r = state.rotationDegrees;
        assert(std::fabs(r.x) <= 4 && std::fabs(r.y) <= 4 && std::fabs(r.z) <= 1.5f);
    }
    engine::UpdateScreenShake(state, 0.01);
    assert(Near(state.rotationDegrees, {}));
}

void ValidationZeroStrengthAndPresentation()
{
    engine::ScreenShakeState state;
    assert(engine::StartScreenShake(state, -1, 1).token == 0);
    assert(engine::StartScreenShake(state, 1.01f, 1).token == 0);
    assert(engine::StartScreenShake(state, NAN, 1).token == 0);
    assert(engine::StartScreenShake(state, 1, INFINITY).token == 0);
    assert(engine::StartScreenShake(state, 1, -1).token == 0);
    assert(engine::StartScreenShake(state, 1, 1, static_cast<engine::ScreenShakeType>(99)).token == 0);
    assert(state.nextToken == 1);
    const auto instant = engine::StartScreenShake(state, 1, 0);
    assert(instant.token != 0 && !engine::IsScreenShakeActive(state, instant));
    const auto silent = engine::StartScreenShake(state, 0, 1);
    engine::UpdateScreenShake(state, 0.5);
    assert(engine::IsScreenShakeActive(state, silent) && Near(state.rotationDegrees, {}));
    engine::UpdateScreenShake(state, 0.5);
    assert(!engine::IsScreenShakeActive(state, silent));
    engine::StartScreenShake(state, 1, 2);
    engine::UpdateScreenShake(state, 0.3);
    game::SectorFpsControllerState player;
    player.feetPosition = {3, 4, 5};
    player.currentSectorId = 17;
    player.yawRadians = 0.2f;
    player.pitchRadians = 0.1f;
    const auto base = game::SectorFpsControllerPose(player, {});
    const auto shaken = game::ApplySectorFpsViewRotationOffset(base, state.rotationDegrees);
    assert(Near(base.position, shaken.position));
    assert(player.currentSectorId == 17 && Near(player.feetPosition, {3, 4, 5}));
    assert(player.yawRadians == 0.2f && player.pitchRadians == 0.1f);
    assert(shaken.yawRadians != base.yawRadians || shaken.pitchRadians != base.pitchRadians);
    const auto again = game::ApplySectorFpsViewRotationOffset(base, state.rotationDegrees);
    assert(shaken.yawRadians == again.yawRadians && shaken.rollRadians == again.rollRadians);
}

} // namespace

void RunScreenShakeTests()
{
    TimingNoiseAndEnvelopes();
    OverlapBoundsCancellationAndReset();
    ValidationZeroStrengthAndPresentation();
}
