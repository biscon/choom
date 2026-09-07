#ifndef SECTOR_SURFACE_FOG_GLSL
#define SECTOR_SURFACE_FOG_GLSL

vec3 ApplySectorFog(
        vec3 surfaceRgb,
        vec3 staticAtmosphericLighting,
        vec3 worldPosition)
{
    if (fogEnabled == 0 || fogDensity <= 0.0 || fogMaxOpacity <= 0.0) {
        return surfaceRgb;
    }

    float fogDistance = max(length(worldPosition - fogCameraPosition) - fogStartDistanceWorld, 0.0);
    float midpointHeight = (fogCameraPosition.y + worldPosition.y) * 0.5;
    float heightAboveReference = max(midpointHeight - fogReferenceHeightWorld, 0.0);
    float heightMultiplier = exp(-heightAboveReference * fogHeightFalloff);
    float fogAmount = min(
            1.0 - exp(-fogDensity * fogDistance * heightMultiplier),
            fogMaxOpacity);
    vec3 fogLighting = max(staticAtmosphericLighting, vec3(0.0));
    float fogLightPeak = max(max(fogLighting.r, fogLighting.g), fogLighting.b);
    float fogLightVisibility = smoothstep(0.0, 0.04, fogLightPeak);
    vec3 fogLightTint = fogLightPeak > 0.00001
            ? clamp(fogLighting / fogLightPeak, vec3(0.0), vec3(1.0))
            : vec3(0.0);
    vec3 fogScattering = fogColor * fogLightTint * fogLightVisibility;
    return surfaceRgb * (1.0 - fogAmount) + fogScattering * fogAmount;
}

#endif
