#include "game/PlayerHealthVisual.h"

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

float SmoothStep01(float value)
{
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float LowHealthRangeStrength(
        const Health& health,
        float startThresholdRatio,
        float fullEffectRatio)
{
    const float start = std::clamp(
            FiniteOr(startThresholdRatio, 0.50f), 0.0f, 1.0f);
    const float full = std::clamp(
            FiniteOr(fullEffectRatio, 0.10f), 0.0f, start);
    if (start <= full) return 0.0f;
    return SmoothStep01(
            (start - PlayerHealthRatio(health)) / (start - full));
}

bool Finite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

} // namespace

std::string PlayerHealthSettingsError(
        const PlayerHealthApplicationSettings& settings)
{
    const PlayerLowHealthVisualApplicationSettings& visual =
            settings.lowHealthVisual;
    const auto finite = [](float value) { return std::isfinite(value); };
    if (!finite(visual.thresholdRatio)
            || visual.thresholdRatio <= 0.0f
            || visual.thresholdRatio > 1.0f) {
        return "lowHealthVisual.thresholdRatio must be greater than 0 and at most 1";
    }
    if (!finite(visual.vignetteInnerRadius)
            || visual.vignetteInnerRadius < 0.0f
            || visual.vignetteInnerRadius > 2.0f) {
        return "lowHealthVisual.vignetteInnerRadius must be between 0 and 2";
    }
    if (!finite(visual.vignetteOuterRadius)
            || visual.vignetteOuterRadius < 0.0f
            || visual.vignetteOuterRadius > 2.0f) {
        return "lowHealthVisual.vignetteOuterRadius must be between 0 and 2";
    }
    if (visual.vignetteOuterRadius <= visual.vignetteInnerRadius) {
        return "lowHealthVisual.vignetteOuterRadius must be greater than vignetteInnerRadius";
    }
    if (!finite(visual.maximumVignetteOpacity)
            || visual.maximumVignetteOpacity < 0.0f
            || visual.maximumVignetteOpacity > 1.0f) {
        return "lowHealthVisual.maximumVignetteOpacity must be between 0 and 1";
    }
    if (!finite(visual.maximumDesaturation)
            || visual.maximumDesaturation < 0.0f
            || visual.maximumDesaturation > 1.0f) {
        return "lowHealthVisual.maximumDesaturation must be between 0 and 1";
    }

    const PlayerHeartbeatAudioApplicationSettings& heartbeat =
            settings.heartbeatAudio;
    if (!finite(heartbeat.startThresholdRatio)
            || heartbeat.startThresholdRatio <= 0.0f
            || heartbeat.startThresholdRatio > 1.0f) {
        return "heartbeatAudio.startThresholdRatio must be greater than 0 and at most 1";
    }
    if (!finite(heartbeat.fullEffectRatio)
            || heartbeat.fullEffectRatio < 0.0f
            || heartbeat.fullEffectRatio
                    >= heartbeat.startThresholdRatio) {
        return "heartbeatAudio.fullEffectRatio must be non-negative and less than startThresholdRatio";
    }
    if (!finite(heartbeat.maximumVolume)
            || heartbeat.maximumVolume < 0.0f
            || heartbeat.maximumVolume > 1.0f) {
        return "heartbeatAudio.maximumVolume must be between 0 and 1";
    }
    if (!finite(heartbeat.startPitch)
            || heartbeat.startPitch < 0.01f
            || heartbeat.startPitch > 4.0f
            || !finite(heartbeat.maximumPitch)
            || heartbeat.maximumPitch < heartbeat.startPitch
            || heartbeat.maximumPitch > 4.0f) {
        return "heartbeatAudio pitches must be between 0.01 and 4, with maximumPitch at least startPitch";
    }
    if (!finite(heartbeat.responseSeconds)
            || heartbeat.responseSeconds <= 0.0f
            || heartbeat.responseSeconds > 100.0f) {
        return "heartbeatAudio.responseSeconds must be greater than 0 and at most 100";
    }

    const PlayerLowHealthMovementApplicationSettings& movement =
            settings.lowHealthMovement;
    if (!finite(movement.startThresholdRatio)
            || movement.startThresholdRatio <= 0.0f
            || movement.startThresholdRatio > 1.0f) {
        return "lowHealthMovement.startThresholdRatio must be greater than 0 and at most 1";
    }
    if (!finite(movement.minimumSpeedScale)
            || movement.minimumSpeedScale < 0.0f
            || movement.minimumSpeedScale > 1.0f) {
        return "lowHealthMovement.minimumSpeedScale must be between 0 and 1";
    }

    const PlayerLowHealthCameraApplicationSettings& camera =
            settings.lowHealthCamera;
    if (!finite(camera.startThresholdRatio)
            || camera.startThresholdRatio <= 0.0f
            || camera.startThresholdRatio > 1.0f) {
        return "lowHealthCamera.startThresholdRatio must be greater than 0 and at most 1";
    }
    if (!finite(camera.fullEffectRatio)
            || camera.fullEffectRatio < 0.0f
            || camera.fullEffectRatio >= camera.startThresholdRatio) {
        return "lowHealthCamera.fullEffectRatio must be non-negative and less than startThresholdRatio";
    }
    if (!finite(camera.lateralAmplitudeWorld)
            || camera.lateralAmplitudeWorld < 0.0f
            || camera.lateralAmplitudeWorld > 1.0f
            || !finite(camera.verticalAmplitudeWorld)
            || camera.verticalAmplitudeWorld < 0.0f
            || camera.verticalAmplitudeWorld > 1.0f) {
        return "lowHealthCamera position amplitudes must be between 0 and 1";
    }
    if (!finite(camera.pitchAmplitudeDegrees)
            || camera.pitchAmplitudeDegrees < 0.0f
            || camera.pitchAmplitudeDegrees > 45.0f
            || !finite(camera.yawAmplitudeDegrees)
            || camera.yawAmplitudeDegrees < 0.0f
            || camera.yawAmplitudeDegrees > 45.0f
            || !finite(camera.rollAmplitudeDegrees)
            || camera.rollAmplitudeDegrees < 0.0f
            || camera.rollAmplitudeDegrees > 45.0f) {
        return "lowHealthCamera rotation amplitudes must be between 0 and 45 degrees";
    }
    if (!finite(camera.frequencyHz)
            || camera.frequencyHz < 0.0f
            || camera.frequencyHz > 20.0f) {
        return "lowHealthCamera.frequencyHz must be between 0 and 20";
    }
    if (!finite(camera.responseSeconds)
            || camera.responseSeconds <= 0.0f
            || camera.responseSeconds > 100.0f) {
        return "lowHealthCamera.responseSeconds must be greater than 0 and at most 100";
    }
    return {};
}

float PlayerHealthRatio(const Health& health)
{
    if (health.maximum <= 0) return 0.0f;
    return std::clamp(
            static_cast<float>(health.current)
                    / static_cast<float>(health.maximum),
            0.0f,
            1.0f);
}

float PlayerLowHealthVisualStrength(
        const Health& health,
        const PlayerLowHealthVisualApplicationSettings& settings)
{
    if (!settings.enabled
            || !std::isfinite(settings.thresholdRatio)
            || settings.thresholdRatio <= 0.0f) {
        return 0.0f;
    }
    const float threshold = std::clamp(settings.thresholdRatio, 0.0f, 1.0f);
    const float deficit = std::clamp(
            (threshold - PlayerHealthRatio(health)) / threshold,
            0.0f,
            1.0f);
    return deficit * deficit * (3.0f - 2.0f * deficit);
}

float PlayerLowHealthVignetteOpacity(
        const Health& health,
        const PlayerLowHealthVisualApplicationSettings& settings)
{
    const float authoredOpacity = std::clamp(
            settings.maximumVignetteOpacity
                    * PlayerLowHealthVisualStrength(health, settings),
            0.0f,
            1.0f);
    // Treat the authored value as an artist-facing intensity. A quadratic
    // ease-out makes injury readable earlier and lets high values feel
    // substantially less transparent without changing the vignette shape.
    return authoredOpacity * (2.0f - authoredOpacity);
}

float PlayerHeartbeatStrength(
        const Health& health,
        const PlayerHeartbeatAudioApplicationSettings& settings)
{
    if (!settings.enabled) return 0.0f;
    return LowHealthRangeStrength(
            health,
            settings.startThresholdRatio,
            settings.fullEffectRatio);
}

float PlayerHeartbeatVolume(
        const Health& health,
        const PlayerHeartbeatAudioApplicationSettings& settings)
{
    return std::clamp(
            FiniteOr(settings.maximumVolume, 1.0f)
                    * PlayerHeartbeatStrength(health, settings),
            0.0f,
            1.0f);
}

float PlayerHeartbeatPitch(
        const Health& health,
        const PlayerHeartbeatAudioApplicationSettings& settings)
{
    const float start = std::clamp(
            FiniteOr(settings.startPitch, 1.0f), 0.01f, 4.0f);
    const float maximum = std::clamp(
            FiniteOr(settings.maximumPitch, 1.5f), start, 4.0f);
    return start + (maximum - start)
            * PlayerHeartbeatStrength(health, settings);
}

float PlayerLowHealthMovementSpeedScale(
        const Health& health,
        const PlayerLowHealthMovementApplicationSettings& settings)
{
    if (!settings.enabled) return 1.0f;
    const float threshold = std::clamp(
            FiniteOr(settings.startThresholdRatio, 0.50f), 0.0f, 1.0f);
    if (threshold <= 0.0f) return 1.0f;
    const float minimum = std::clamp(
            FiniteOr(settings.minimumSpeedScale, 0.20f), 0.0f, 1.0f);
    const float healthyFraction = std::clamp(
            PlayerHealthRatio(health) / threshold, 0.0f, 1.0f);
    return minimum + (1.0f - minimum) * healthyFraction;
}

float PlayerLowHealthCameraStrength(
        const Health& health,
        const PlayerLowHealthCameraApplicationSettings& settings)
{
    if (!settings.enabled) return 0.0f;
    const float strength = LowHealthRangeStrength(
            health,
            settings.startThresholdRatio,
            settings.fullEffectRatio);
    return strength * strength;
}

void ClearPlayerLowHealthCamera(PlayerLowHealthCameraState& state)
{
    state = PlayerLowHealthCameraState{};
}

void UpdatePlayerLowHealthCamera(
        PlayerLowHealthCameraState& state,
        const PlayerLowHealthCameraApplicationSettings& settings,
        const Health& health,
        float rawDt)
{
    if (!std::isfinite(state.phase)
            || !std::isfinite(state.intensity)
            || !Finite(state.positionOffsetLocal)
            || !Finite(state.rotationDegrees)) {
        ClearPlayerLowHealthCamera(state);
    }
    if (!settings.enabled) {
        ClearPlayerLowHealthCamera(state);
        return;
    }

    const float target = PlayerLowHealthCameraStrength(health, settings);
    const float dt = std::isfinite(rawDt)
            ? std::clamp(rawDt, 0.0f, 0.25f)
            : 0.0f;
    if (dt > 0.0f) {
        const float responseSeconds = std::max(
                0.001f,
                FiniteOr(settings.responseSeconds, 0.35f));
        const float response = 1.0f - std::exp(-dt / responseSeconds);
        state.intensity += (target - state.intensity) * response;
        state.intensity = std::clamp(state.intensity, 0.0f, 1.0f);
        if (target > CameraStateEpsilon
                || state.intensity > CameraStateEpsilon) {
            const float frequency = std::max(
                    0.0f, FiniteOr(settings.frequencyHz, 0.55f));
            state.phase += dt * frequency * TwoPi;
            if (std::fabs(state.phase) > 100000.0f) {
                state.phase = std::fmod(state.phase, TwoPi);
            }
        }
    }

    if (state.intensity <= CameraStateEpsilon) {
        state.intensity = 0.0f;
        state.positionOffsetLocal = {};
        state.rotationDegrees = {};
        return;
    }

    const float lateral = std::max(
            0.0f, FiniteOr(settings.lateralAmplitudeWorld, 0.020f));
    const float vertical = std::max(
            0.0f, FiniteOr(settings.verticalAmplitudeWorld, 0.012f));
    const float phase = state.phase;
    state.positionOffsetLocal = Vector3{
            std::sin(phase) * lateral * state.intensity,
            std::sin(phase * 0.73f + 1.10f)
                    * vertical * state.intensity,
            std::sin(phase * 1.17f + 2.30f)
                    * lateral * 0.35f * state.intensity};
    state.rotationDegrees = Vector3{
            std::sin(phase * 0.83f + 0.40f)
                    * std::max(0.0f, FiniteOr(
                            settings.pitchAmplitudeDegrees, 0.85f))
                    * state.intensity,
            std::sin(phase * 0.61f + 2.00f)
                    * std::max(0.0f, FiniteOr(
                            settings.yawAmplitudeDegrees, 0.65f))
                    * state.intensity,
            std::sin(phase * 1.07f + 1.30f)
                    * std::max(0.0f, FiniteOr(
                            settings.rollAmplitudeDegrees, 1.50f))
                    * state.intensity};
}

} // namespace game
