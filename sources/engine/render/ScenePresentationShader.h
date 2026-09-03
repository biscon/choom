#pragma once

#include "engine/render/ColorTransferGlsl.h"
#include "engine/render/ToneMappingGlsl.h"

#include <raylib.h>

#include <string>

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

inline std::string BuildScenePresentationFragmentShader()
{
    return std::string(R"glsl(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform int presentationToneMapper;
uniform float presentationExposureEv;
uniform float presentationDesaturation;
uniform float presentationVignetteOpacity;
uniform vec3 presentationVignetteColorLinear;
uniform float presentationVignetteInnerRadius;
uniform float presentationVignetteOuterRadius;
uniform float presentationUnderwaterAmount;
uniform vec3 presentationUnderwaterShallowColorLinear;
uniform vec3 presentationUnderwaterDeepColorLinear;
uniform float presentationUnderwaterVisibilityDepthWorld;
uniform vec4 presentationUnderwaterRipple;
uniform vec2 presentationUnderwaterFlow;
uniform float presentationRuntimeSeconds;

out vec4 finalColor;
)glsl")
            + ToneMappingGlsl
            + ColorTransferGlsl
            + R"glsl(
void main()
{
    float underwater = clamp(presentationUnderwaterAmount, 0.0, 1.0);
    float rippleScale = max(presentationUnderwaterRipple.x, 0.05);
    float rippleStrength = max(presentationUnderwaterRipple.y, 0.0);
    float phase = presentationRuntimeSeconds * presentationUnderwaterRipple.z;
    vec2 flowDirection = vec2(cos(presentationUnderwaterFlow.x),
            sin(presentationUnderwaterFlow.x));
    vec2 p = fragTexCoord * vec2(1.0, 0.72) * (8.0 / rippleScale)
            - flowDirection * presentationRuntimeSeconds
                    * presentationUnderwaterFlow.y * 0.08;
    vec2 distortion = vec2(
            sin(p.y * 5.7 + phase * 1.13) + sin(p.x * 8.1 - phase * 1.71),
            cos(p.x * 6.3 + phase * 1.37) + cos(p.y * 9.2 - phase * 1.29));
    distortion *= underwater * min(rippleStrength, 2.0) * 0.0014;
    distortion *= clamp(presentationUnderwaterRipple.w, 0.0, 4.0);
    vec2 sceneUv = clamp(fragTexCoord + distortion, vec2(0.001), vec2(0.999));
    vec4 scene = texture(texture0, sceneUv);
    vec3 exposed = max(scene.rgb, vec3(0.0))
            * exp2(clamp(presentationExposureEv, -8.0, 8.0));
    float inverseVisibility = 1.0 / max(
            presentationUnderwaterVisibilityDepthWorld, 0.05);
    float attenuation = clamp(0.22 + inverseVisibility * 0.32, 0.22, 0.78)
            * underwater;
    vec3 waterTint = mix(
            presentationUnderwaterShallowColorLinear,
            presentationUnderwaterDeepColorLinear,
            clamp(0.35 + inverseVisibility * 0.12, 0.35, 0.72));
    exposed = mix(exposed, exposed * mix(vec3(1.0), waterTint, 0.58), underwater);
    exposed = mix(exposed, waterTint, attenuation * 0.46);
    vec3 mapped = ApplyToneMapping(exposed, presentationToneMapper);
    float luminance = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
    mapped = mix(
            mapped,
            vec3(luminance),
            clamp(presentationDesaturation, 0.0, 1.0));

    vec2 centered = abs(fragTexCoord * 2.0 - 1.0);
    float vignetteDistance = pow(
            pow(centered.x, 4.0) + pow(centered.y, 4.0),
            0.25);
    float vignetteMask = smoothstep(
            presentationVignetteInnerRadius,
            presentationVignetteOuterRadius,
            vignetteDistance);
    mapped = mix(
            mapped,
            clamp(presentationVignetteColorLinear, 0.0, 1.0),
            vignetteMask * clamp(presentationVignetteOpacity, 0.0, 1.0));
    finalColor = vec4(
            LinearSceneToDisplaySrgb(mapped),
            clamp(scene.a, 0.0, 1.0)) * fragColor;
}
)glsl";
}

} // namespace engine
