#pragma once

#include <raylib.h>
#include <array>
#include <cstddef>
#include <cstdint>

namespace engine {

inline constexpr size_t kScreenShakeCapacity = 16;
inline constexpr Vector3 kScreenShakeAmplitudeDegrees{2.0f, 2.0f, 0.75f};
inline constexpr Vector3 kScreenShakeLimitDegrees{4.0f, 4.0f, 1.5f};

enum class ScreenShakeType { Rumble = 1, Impact = 2 };

// Tokens are never reused, including across resets of the same state.
struct ScreenShakeHandle { uint64_t token = 0; };

struct ScreenShakeInstance {
    ScreenShakeHandle handle;
    ScreenShakeType type = ScreenShakeType::Rumble;
    float strength = 0.0f;
    double durationSeconds = 0.0;
    double elapsedSeconds = 0.0;
    uint32_t seed = 0;
    bool active = false;
};

struct ScreenShakeState {
    std::array<ScreenShakeInstance, kScreenShakeCapacity> instances{};
    uint64_t nextToken = 1;
    Vector3 rotationDegrees{};
};

// Main-thread, allocation-free backend. Duration is in seconds, strength in [0,1].
// A zero token indicates failure; error, when supplied, receives a static string.
ScreenShakeHandle StartScreenShake(ScreenShakeState& state, float strength,
        double durationSeconds, ScreenShakeType type = ScreenShakeType::Rumble,
        const char** error = nullptr);
bool IsScreenShakeActive(const ScreenShakeState& state, ScreenShakeHandle handle);
bool CancelScreenShake(ScreenShakeState& state, ScreenShakeHandle handle);
void UpdateScreenShake(ScreenShakeState& state, double deltaSeconds);
void ResetScreenShake(ScreenShakeState& state);

} // namespace engine
