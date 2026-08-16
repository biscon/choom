#include "game/PlayerStamina.h"

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr float TwoPi = 6.28318530717958647692f;
constexpr float CameraStateEpsilon = 0.0001f;

float FiniteOr(float value, float fallback)
{
    return std::isfinite(value) ? value : fallback;
}

float ClampedRatio(float value)
{
    return std::clamp(FiniteOr(value, 1.0f), 0.0f, 1.0f);
}

} // namespace

PlayerStamina MakePlayerStamina(
        const PlayerStaminaApplicationSettings& settings)
{
    const float maximum = std::max(0.001f, FiniteOr(settings.maximum, 100.0f));
    return PlayerStamina{maximum, maximum, false};
}

float PlayerStaminaRatio(const PlayerStamina& stamina)
{
    if (!std::isfinite(stamina.maximum) || stamina.maximum <= 0.0f
            || !std::isfinite(stamina.current)) {
        return 0.0f;
    }
    return std::clamp(stamina.current / stamina.maximum, 0.0f, 1.0f);
}

bool CanPlayerStaminaSprint(const PlayerStamina& stamina)
{
    return !stamina.exhausted
            && std::isfinite(stamina.current)
            && stamina.current > 0.0f;
}

bool CanPlayerStaminaJump(
        const PlayerStamina& stamina,
        const PlayerStaminaApplicationSettings& settings)
{
    const float jumpCost = std::max(0.0f, FiniteOr(settings.jumpCost, 0.0f));
    return !stamina.exhausted
            && std::isfinite(stamina.current)
            && stamina.current >= jumpCost;
}

void UpdatePlayerStamina(
        PlayerStamina& stamina,
        const PlayerStaminaApplicationSettings& settings,
        bool sprinted,
        bool jumped,
        float dt)
{
    const float maximum = std::max(0.001f, FiniteOr(settings.maximum, 100.0f));
    stamina.maximum = maximum;
    stamina.current = std::clamp(
            FiniteOr(stamina.current, maximum),
            0.0f,
            maximum);
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return;
    }

    const float sprintCost = sprinted
            ? std::max(0.0f, FiniteOr(settings.sprintDrainPerSecond, 0.0f)) * dt
            : 0.0f;
    const float jumpCost = jumped
            ? std::max(0.0f, FiniteOr(settings.jumpCost, 0.0f))
            : 0.0f;
    const float consumed = sprintCost + jumpCost;
    if (consumed > 0.0f) {
        stamina.current = std::max(0.0f, stamina.current - consumed);
        if (stamina.current <= 0.0f) {
            stamina.current = 0.0f;
            stamina.exhausted = true;
        }
    } else {
        const float regeneration = std::max(
                0.0f,
                FiniteOr(settings.regenerationPerSecond, 0.0f));
        stamina.current = std::min(maximum, stamina.current + regeneration * dt);
    }

    const float recoveryRatio = std::clamp(
            FiniteOr(settings.exhaustedRecoveryRatio, 0.20f),
            0.0f,
            1.0f);
    if (stamina.exhausted
            && stamina.current >= maximum * recoveryRatio) {
        stamina.exhausted = false;
    }
}

void ClearPlayerWindedCamera(PlayerWindedCameraState& state)
{
    state = PlayerWindedCameraState{};
}

void UpdatePlayerWindedCamera(
        PlayerWindedCameraState& state,
        const PlayerWindedCameraApplicationSettings& settings,
        float staminaRatio,
        float dt)
{
    if (!std::isfinite(state.phase)
            || !std::isfinite(state.intensity)
            || !std::isfinite(state.verticalOffsetWorld)
            || !std::isfinite(state.pitchOffsetDegrees)) {
        ClearPlayerWindedCamera(state);
    }
    if (!settings.enabled) {
        ClearPlayerWindedCamera(state);
        return;
    }

    const float threshold = std::clamp(
            FiniteOr(settings.startThresholdRatio, 0.40f),
            0.0f,
            1.0f);
    const float ratio = ClampedRatio(staminaRatio);
    float target = threshold > 0.0f
            ? std::clamp((threshold - ratio) / threshold, 0.0f, 1.0f)
            : 0.0f;
    target = target * target * (3.0f - 2.0f * target);

    if (std::isfinite(dt) && dt > 0.0f) {
        const float responseSeconds = std::max(
                0.001f,
                FiniteOr(settings.responseSeconds, 0.35f));
        const float response = 1.0f - std::exp(-dt / responseSeconds);
        state.intensity += (target - state.intensity) * response;
        state.intensity = std::clamp(state.intensity, 0.0f, 1.0f);

        if (target > CameraStateEpsilon
                || state.intensity > CameraStateEpsilon) {
            const float frequency = std::max(
                    0.0f,
                    FiniteOr(settings.frequencyHz, 0.45f));
            state.phase += dt * frequency * TwoPi;
            if (std::fabs(state.phase) > 100000.0f) {
                state.phase = std::fmod(state.phase, TwoPi);
            }
        }
    }

    if (state.intensity <= CameraStateEpsilon) {
        state.intensity = 0.0f;
        state.verticalOffsetWorld = 0.0f;
        state.pitchOffsetDegrees = 0.0f;
        return;
    }

    const float wave = std::sin(state.phase);
    state.verticalOffsetWorld = wave
            * std::max(0.0f, FiniteOr(settings.verticalAmplitudeWorld, 0.015f))
            * state.intensity;
    state.pitchOffsetDegrees = -wave
            * std::max(0.0f, FiniteOr(settings.pitchAmplitudeDegrees, 0.60f))
            * state.intensity;
}

float AdvancePlayerBreathingAudioVolume(
        float currentVolume,
        const PlayerBreathingAudioApplicationSettings& settings,
        float staminaRatio,
        float dt)
{
    const float configuredVolume = std::clamp(
            FiniteOr(settings.volume, 0.75f),
            0.0f,
            1.0f);
    const float threshold = std::clamp(
            FiniteOr(settings.thresholdRatio, 0.20f),
            0.0f,
            1.0f);
    const float ratio = ClampedRatio(staminaRatio);
    if (ratio < threshold) {
        return configuredVolume;
    }

    currentVolume = std::clamp(FiniteOr(currentVolume, 0.0f), 0.0f, 1.0f);
    if (!std::isfinite(dt) || dt <= 0.0f || currentVolume <= 0.0f) {
        return currentVolume;
    }
    const float fadeOutSeconds = std::max(
            0.001f,
            FiniteOr(settings.fadeOutSeconds, 2.0f));
    const float fullScale = std::max(currentVolume, configuredVolume);
    return std::max(0.0f, currentVolume - fullScale * dt / fadeOutSeconds);
}

} // namespace game
