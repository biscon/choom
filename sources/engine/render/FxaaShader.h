#pragma once

namespace engine {

inline constexpr const char* FxaaFragmentShader = R"(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texelSize;

out vec4 finalColor;

void main()
{
    vec3 rgbNW = texture(texture0, fragTexCoord + vec2(-1.0, -1.0) * texelSize).rgb;
    vec3 rgbNE = texture(texture0, fragTexCoord + vec2( 1.0, -1.0) * texelSize).rgb;
    vec3 rgbSW = texture(texture0, fragTexCoord + vec2(-1.0,  1.0) * texelSize).rgb;
    vec3 rgbSE = texture(texture0, fragTexCoord + vec2( 1.0,  1.0) * texelSize).rgb;
    vec3 rgbM  = texture(texture0, fragTexCoord).rgb;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;

    const float edgeThresholdMin = 0.0312;
    const float edgeThresholdMax = 0.125;
    if (lumaRange < max(edgeThresholdMin, lumaMax * edgeThresholdMax)) {
        finalColor = vec4(rgbM, texture(texture0, fragTexCoord).a) * fragColor;
        return;
    }

    vec2 direction;
    direction.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    direction.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float directionReduce = max(
            (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * 0.0078125),
            0.0009765625);
    float inverseDirectionAdjustment = 1.0 / (min(abs(direction.x), abs(direction.y)) + directionReduce);

    direction = clamp(direction * inverseDirectionAdjustment, vec2(-8.0), vec2(8.0)) * texelSize;

    vec3 rgbA = 0.5 * (
            texture(texture0, fragTexCoord + direction * (1.0 / 3.0 - 0.5)).rgb +
            texture(texture0, fragTexCoord + direction * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
            texture(texture0, fragTexCoord + direction * -0.5).rgb +
            texture(texture0, fragTexCoord + direction *  0.5).rgb);

    float lumaB = dot(rgbB, luma);
    vec3 rgb = (lumaB < lumaMin || lumaB > lumaMax) ? rgbA : rgbB;

    finalColor = vec4(rgb, texture(texture0, fragTexCoord).a) * fragColor;
}
)";

} // namespace engine
