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

out vec4 finalColor;
)glsl")
            + ToneMappingGlsl
            + ColorTransferGlsl
            + R"glsl(
void main()
{
    vec4 scene = texture(texture0, fragTexCoord);
    vec3 exposed = max(scene.rgb, vec3(0.0))
            * exp2(clamp(presentationExposureEv, -8.0, 8.0));
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
