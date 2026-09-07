#pragma once

#include <raylib.h>

namespace engine {

struct ScenePresentationEffectParameters {
    float desaturation = 0.0f;
    float vignetteOpacity = 0.0f;
    Vector3 vignetteColorLinear{};
    float vignetteInnerRadius = 0.50f;
    float vignetteOuterRadius = 1.05f;
    float underwaterAmount = 0.0f;
    Vector3 underwaterShallowColorLinear{};
    Vector3 underwaterDeepColorLinear{};
    float underwaterVisibilityDepthWorld = 4.0f;
    float underwaterRippleScaleWorld = 0.9f;
    float underwaterRippleStrength = 0.22f;
    float underwaterRippleSpeed = 0.35f;
    float underwaterDistortionStrength = 1.35f;
    float underwaterFlowDirectionRadians = 0.0f;
    float underwaterFlowSpeedWorld = 0.0f;
    float runtimeSeconds = 0.0f;
};

} // namespace engine
