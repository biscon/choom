#version 330
out vec4 finalColor;
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform float nearPlane;
uniform float farPlane;
uniform vec3 sphereCenter;
uniform float sphereRadius;
uniform vec3 haloRadiance;
uniform vec2 haloParams; // edge softness, maximum extinction
uniform vec4 fogParamsA; // mode, start, end/density, maximum opacity
uniform vec4 fogParamsB; // exponent, reference height, height falloff, unused

bool intersectSphere(vec3 rayOrigin, vec3 rayDirection,
        out float enterT, out float exitT) {
    vec3 offset = rayOrigin - sphereCenter;
    float b = dot(offset, rayDirection);
    float c = dot(offset, offset) - sphereRadius * sphereRadius;
    float discriminant = b * b - c;
    if (discriminant < 0.0) return false;
    float root = sqrt(max(discriminant, 0.0));
    enterT = -b - root;
    exitT = -b + root;
    return exitT > max(enterT, 0.0);
}

#include "safe_normalize_lower.glsl"

float fogTransmittance(vec3 position) {
    if (fogParamsA.x < 0.5) return 1.0;
    float amount = 0.0;
    if (fogParamsA.x < 1.5) {
        float distance = max(length(position - cameraPosition) - fogParamsA.y, 0.0);
        float midpointHeight = (cameraPosition.y + position.y) * 0.5;
        float height = max(midpointHeight - fogParamsB.y, 0.0);
        amount = min(1.0 - exp(-fogParamsA.z * distance
                * exp(-height * fogParamsB.z)), fogParamsA.w);
    } else {
        amount = pow(clamp((dot(position - cameraPosition, cameraForward) - fogParamsA.y)
                / max(fogParamsA.z - fogParamsA.y, 0.0001), 0.0, 1.0),
                max(fogParamsB.x, 0.0001)) * fogParamsA.w;
    }
    return 1.0 - clamp(amount, 0.0, 1.0);
}

void main() {
    vec2 uv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float enterT = 0.0;
    float exitT = 0.0;
    if (!intersectSphere(cameraPosition, rayDirection, enterT, exitT)) discard;

    float unclippedEnterT = max(enterT, 0.0);
    float depth = texture(sceneDepth, uv).r;
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float sceneDistance = depth >= 0.999999 ? farPlane
            : forwardDistance / max(dot(rayDirection, cameraForward), 0.0001);
    enterT = unclippedEnterT;
    exitT = min(exitT, sceneDistance);
    float visibleChord = max(exitT - enterT, 0.0);
    if (visibleChord <= 0.00001) discard;

    float closestT = max(dot(sphereCenter - cameraPosition, rayDirection), 0.0);
    vec3 closestPoint = cameraPosition + rayDirection * closestT;
    float radial = clamp(length(closestPoint - sphereCenter)
            / max(sphereRadius, 0.0001), 0.0, 1.0);
    float mappedSoftness = clamp(haloParams.x * 2.0, 0.02, 2.0);
    float baseSoftness = min(mappedSoftness, 1.0);
    float extraSoftness = max(mappedSoftness - 1.0, 0.0);
    float broad = 1.0 - smoothstep(1.0 - baseSoftness, 1.0, radial);
    float core = pow(max(1.0 - radial, 0.0), mix(8.0, 3.0, baseSoftness));
    float profile = clamp(0.35 * broad + 0.65 * core, 0.0, 1.0);
    profile = pow(profile, 1.0 + 1.5 * extraSoftness);

    float midpointT = (enterT + exitT) * 0.5;
    vec3 midpoint = cameraPosition + rayDirection * midpointT;
    float normalizedChord = clamp(visibleChord / max(2.0 * sphereRadius, 0.0001), 0.0, 1.0);
    float opticalThickness = 1.0 - exp(-2.5 * normalizedChord * profile);
    float phaseT = mix(enterT, exitT, 0.35);
    vec3 phasePosition = cameraPosition + rayDirection * phaseT;
    vec3 lightTravel = safeNormalize(phasePosition - sphereCenter, -rayDirection);
    float phaseFacing = clamp(dot(lightTravel, -rayDirection) * 0.5 + 0.5, 0.0, 1.0);
    float phase = mix(0.35, 1.0, pow(phaseFacing, 3.0));
    float scatterWeight = opticalThickness * phase * fogTransmittance(midpoint);
    float extinction = clamp(haloParams.y, 0.0, 1.0) * opticalThickness;
    if (scatterWeight <= 0.00001 && extinction <= 0.00001) discard;
    vec3 inScattering = min(max(haloRadiance * scatterWeight, vec3(0.0)), vec3(65504.0));
    if (any(isnan(inScattering)) || any(isinf(inScattering))
            || isnan(extinction) || isinf(extinction)) discard;
    finalColor = vec4(inScattering, extinction);
}
