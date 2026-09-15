#include "engine/render/ScreenShake.h"

#include <algorithm>
#include <cmath>

namespace engine {
namespace {

double Smooth(double t)
{
    t = std::clamp(t, 0.0, 1.0);
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

uint32_t Hash(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    return value ^ (value >> 16);
}

// One-dimensional gradient Perlin noise, normalized to [-1,1]. Wrapping before
// conversion also keeps arbitrarily long finite durations safe to sample.
float Noise(double seconds, double frequency, uint32_t seed)
{
    const double x = std::fmod(seconds, 65536.0 / frequency) * frequency
            + static_cast<double>(seed & 0xffffu) / 65536.0;
    const auto cell = static_cast<uint32_t>(std::floor(x));
    const double t = x - std::floor(x);
    const double a = (Hash((cell & 0xffffu) ^ seed) & 1u) ? t : -t;
    const double b = (Hash(((cell + 1u) & 0xffffu) ^ seed) & 1u) ? t - 1.0 : 1.0 - t;
    return static_cast<float>(2.0 * (a + (b - a) * Smooth(t)));
}

double Envelope(const ScreenShakeInstance& shake)
{
    const double t = shake.elapsedSeconds;
    const double duration = shake.durationSeconds;
    if (duration <= 0.0 || t >= duration) return 0.0;
    const bool impact = shake.type == ScreenShakeType::Impact;
    const double attack = std::min(impact ? 0.01 : 0.1, duration * 0.1);
    if (t < attack) return Smooth(t / attack);
    if (impact) {
        const double remaining = (duration - t) / (duration - attack);
        return remaining * remaining;
    }
    const double release = std::min(0.35, duration * 0.25);
    return Smooth((duration - t) / release);
}

void Evaluate(ScreenShakeState& state)
{
    Vector3 rotation{};
    for (const auto& shake : state.instances) {
        if (!shake.active) continue;
        const float amplitude = shake.strength * static_cast<float>(Envelope(shake));
        const double frequency = shake.type == ScreenShakeType::Impact ? 14.0 : 8.0;
        rotation.x += kScreenShakeAmplitudeDegrees.x * amplitude
                * Noise(shake.elapsedSeconds, frequency, Hash(shake.seed));
        rotation.y += kScreenShakeAmplitudeDegrees.y * amplitude
                * Noise(shake.elapsedSeconds, frequency, Hash(shake.seed + 1u));
        rotation.z += kScreenShakeAmplitudeDegrees.z * amplitude
                * Noise(shake.elapsedSeconds, frequency, Hash(shake.seed + 2u));
    }
    state.rotationDegrees = {
        std::clamp(rotation.x, -kScreenShakeLimitDegrees.x, kScreenShakeLimitDegrees.x),
        std::clamp(rotation.y, -kScreenShakeLimitDegrees.y, kScreenShakeLimitDegrees.y),
        std::clamp(rotation.z, -kScreenShakeLimitDegrees.z, kScreenShakeLimitDegrees.z)};
}

} // namespace

ScreenShakeHandle StartScreenShake(ScreenShakeState& state, float strength,
        double durationSeconds, ScreenShakeType type, const char** error)
{
    if (error != nullptr) *error = nullptr;
    const char* failure = nullptr;
    if (!std::isfinite(strength) || strength < 0.0f || strength > 1.0f)
        failure = "shake strength must be finite and between 0 and 1";
    else if (!std::isfinite(durationSeconds) || durationSeconds < 0.0)
        failure = "shake duration must be finite and non-negative";
    else if (type != ScreenShakeType::Rumble && type != ScreenShakeType::Impact)
        failure = "shake type must be SHAKE_RUMBLE or SHAKE_IMPACT";
    else if (state.nextToken == 0)
        failure = "screen shake tokens exhausted";
    if (failure != nullptr) {
        if (error != nullptr) *error = failure;
        return {};
    }
    // Instant no-ops do not consume capacity.
    if (durationSeconds == 0.0) return {state.nextToken++};
    for (auto& shake : state.instances) {
        if (shake.active) continue;
        const ScreenShakeHandle handle{state.nextToken++};
        shake = {handle, type, strength, durationSeconds, 0.0,
                Hash(static_cast<uint32_t>(handle.token)
                        ^ static_cast<uint32_t>(handle.token >> 32)), true};
        return handle;
    }
    TraceLog(LOG_WARNING, "Screen shake capacity exceeded (%zu)", kScreenShakeCapacity);
    if (error != nullptr) *error = "screen shake capacity exceeded";
    return {};
}

bool IsScreenShakeActive(const ScreenShakeState& state, ScreenShakeHandle handle)
{
    for (const auto& shake : state.instances)
        if (shake.active && shake.handle.token == handle.token) return true;
    return false;
}

bool CancelScreenShake(ScreenShakeState& state, ScreenShakeHandle handle)
{
    for (auto& shake : state.instances) {
        if (!shake.active || shake.handle.token != handle.token) continue;
        shake.active = false;
        Evaluate(state);
        return true;
    }
    return false;
}

void UpdateScreenShake(ScreenShakeState& state, double deltaSeconds)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0) return;
    for (auto& shake : state.instances) {
        if (!shake.active) continue;
        const double remaining = shake.durationSeconds - shake.elapsedSeconds;
        if (deltaSeconds >= remaining) {
            shake.elapsedSeconds = shake.durationSeconds;
            shake.active = false;
        } else {
            shake.elapsedSeconds += deltaSeconds;
        }
    }
    Evaluate(state);
}

void ResetScreenShake(ScreenShakeState& state)
{
    state.instances = {};
    state.rotationDegrees = {};
}

} // namespace engine
