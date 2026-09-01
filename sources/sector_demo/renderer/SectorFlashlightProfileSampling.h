#pragma once

// Shared GLSL for the projected flashlight beam. The generic spotlight cone
// only clips the profile; this function owns the single visible edge feather.
#define SECTOR_FLASHLIGHT_PROFILE_GLSL R"glsl(
float FlashlightProfileFactor(int lightIndex, vec3 directionFromLight)
{
    vec3 spotDirection = SafeNormalize(
            dynamicLightDirections[lightIndex], vec3(0.0, -1.0, 0.0));
    vec3 right = SafeNormalize(
            dynamicLightSpotShadowRight[lightIndex], vec3(1.0, 0.0, 0.0));
    vec3 up = SafeNormalize(
            cross(right, spotDirection), vec3(0.0, 0.0, 1.0));
    float axial = dot(directionFromLight, spotDirection);
    if (axial <= 0.0001) return 0.0;

    vec2 projected = vec2(
            dot(directionFromLight, right),
            dot(directionFromLight, up))
            * dynamicLightSpotShadowProjection[lightIndex].x / axial;
    float radial = length(projected);
    if (radial >= 1.0) return 0.0;

    vec3 parameters = dynamicLightProfileParameters[lightIndex];
    float hotspotRadius = clamp(parameters.x, 0.001, 0.999);
    float hotspot = 1.0 - smoothstep(
            hotspotRadius * 0.55, hotspotRadius, radial);
    float edgeStart = clamp(1.0 - parameters.z, 0.001, 0.999);
    float edge = 1.0 - smoothstep(edgeStart, 1.0, radial);
    float cookie = texture(
            flashlightCookie,
            projected * vec2(0.5, -0.5) + 0.5).r;
    float cookieVariation = mix(0.90, 1.08, cookie);
    return mix(parameters.y, 1.0, hotspot) * edge * cookieVariation;
}
)glsl"
