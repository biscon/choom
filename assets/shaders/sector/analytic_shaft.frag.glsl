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
uniform vec3 coneApex;
uniform vec3 coneDirection;
uniform float coneLength;
uniform float coneBaseRadius;
uniform int shaftShape; // 0 cone, 1 rectangular frustum
uniform vec3 rectRight;
uniform vec3 rectUp;
uniform vec2 rectNearHalfSize;
uniform vec2 rectFarHalfSize;
uniform vec3 shaftRadiance;
uniform vec2 shaftParams; // edge softness, maximum extinction
uniform vec4 fogParamsA; // mode, start, end/density, maximum opacity
uniform vec4 fogParamsB; // exponent, reference height, height falloff, unused

#include "safe_normalize_lower.glsl"

void includeConeHit(float t, inout float enterT, inout float exitT, inout int hitCount) {
    if (isnan(t) || isinf(t)) return;
    enterT = min(enterT, t);
    exitT = max(exitT, t);
    hitCount++;
}

bool intersectFiniteCone(vec3 rayOrigin, vec3 rayDirection,
        out float enterT, out float exitT) {
    vec3 axis = safeNormalize(coneDirection, vec3(0.0, -1.0, 0.0));
    float height = max(coneLength, 0.0001);
    float radius = max(coneBaseRadius, 0.0001);
    float slopeSquared = radius * radius / (height * height);
    vec3 originOffset = rayOrigin - coneApex;
    float originAxial = dot(originOffset, axis);
    float directionAxial = dot(rayDirection, axis);
    vec3 originRadial = originOffset - axis * originAxial;
    vec3 directionRadial = rayDirection - axis * directionAxial;
    float a = dot(directionRadial, directionRadial)
            - slopeSquared * directionAxial * directionAxial;
    float b = 2.0 * (dot(originRadial, directionRadial)
            - slopeSquared * originAxial * directionAxial);
    float c = dot(originRadial, originRadial) - slopeSquared * originAxial * originAxial;
    enterT = 1e30;
    exitT = -1e30;
    int hitCount = 0;
    const float epsilon = 0.000001;
    if (abs(a) <= epsilon) {
        if (abs(b) > epsilon) {
            float t = -c / b;
            float axial = originAxial + t * directionAxial;
            if (axial >= -epsilon && axial <= height + epsilon) {
                includeConeHit(t, enterT, exitT, hitCount);
            }
        }
    } else {
        float discriminant = b * b - 4.0 * a * c;
        if (discriminant >= 0.0) {
            float root = sqrt(max(discriminant, 0.0));
            float inverse = 0.5 / a;
            float t0 = (-b - root) * inverse;
            float axial0 = originAxial + t0 * directionAxial;
            if (axial0 >= -epsilon && axial0 <= height + epsilon) {
                includeConeHit(t0, enterT, exitT, hitCount);
            }
            float t1 = (-b + root) * inverse;
            float axial1 = originAxial + t1 * directionAxial;
            if (axial1 >= -epsilon && axial1 <= height + epsilon) {
                includeConeHit(t1, enterT, exitT, hitCount);
            }
        }
    }
    if (abs(directionAxial) > epsilon) {
        float baseT = (height - originAxial) / directionAxial;
        vec3 baseOffset = originOffset + rayDirection * baseT - axis * height;
        if (dot(baseOffset, baseOffset) <= radius * radius + epsilon) {
            includeConeHit(baseT, enterT, exitT, hitCount);
        }
    }
    return hitCount >= 2 && exitT - enterT > epsilon;
}

bool clipRectFrustumPlane(
        vec3 localOrigin,
        vec3 localDirection,
        vec3 planeNormal,
        float planeLimit,
        inout float enterT,
        inout float exitT) {
    float originDistance = dot(planeNormal, localOrigin) - planeLimit;
    float directionDistance = dot(planeNormal, localDirection);
    const float epsilon = 0.000001;
    if (abs(directionDistance) <= epsilon) return originDistance <= epsilon;
    float t = -originDistance / directionDistance;
    if (directionDistance < 0.0) enterT = max(enterT, t);
    else exitT = min(exitT, t);
    return enterT <= exitT + epsilon;
}

bool intersectRectFrustum(vec3 rayOrigin, vec3 rayDirection,
        out float enterT, out float exitT) {
    vec3 axis = safeNormalize(coneDirection, vec3(0.0, -1.0, 0.0));
    vec3 offset = rayOrigin - coneApex;
    vec3 localOrigin = vec3(dot(offset, rectRight), dot(offset, rectUp), dot(offset, axis));
    vec3 localDirection = vec3(dot(rayDirection, rectRight), dot(rayDirection, rectUp), dot(rayDirection, axis));
    float length = max(coneLength, 0.0001);
    vec2 nearHalfSize = max(rectNearHalfSize, vec2(0.0001));
    vec2 farHalfSize = max(rectFarHalfSize, nearHalfSize);
    vec2 sideSlope = (farHalfSize - nearHalfSize) / length;
    enterT = -1e30;
    exitT = 1e30;
    return clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(0.0, 0.0, -1.0), 0.0, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(0.0, 0.0, 1.0), length, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(1.0, 0.0, -sideSlope.x), nearHalfSize.x, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(-1.0, 0.0, -sideSlope.x), nearHalfSize.x, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(0.0, 1.0, -sideSlope.y), nearHalfSize.y, enterT, exitT)
            && clipRectFrustumPlane(localOrigin, localDirection,
                    vec3(0.0, -1.0, -sideSlope.y), nearHalfSize.y, enterT, exitT)
            && exitT - enterT > 0.000001;
}

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

float shaftOpticalProfileAt(
        float sampleT,
        vec3 rayDirection,
        float visibleChord,
        vec3 axis,
        float lateralExponent,
        float startFadeWidth,
        float endFadeWidth) {
    vec3 samplePosition = cameraPosition + rayDirection * sampleT;
    float axial01 = clamp(
            dot(samplePosition - coneApex, axis) / max(coneLength, 0.0001),
            0.0,
            1.0);
    float localDiameter = max(
            2.0 * coneBaseRadius * max(axial01, 0.01),
            0.0001);
    // The old hard clamp exposed a contour where chord/localDiameter crossed
    // one. Smooth that transition before applying the authored edge profile.
    float rawCoverage = max(visibleChord / localDiameter, 0.0);
    float coverage = smoothstep(0.0, 1.0, rawCoverage);
    float lateral = pow(coverage, lateralExponent);
    float longitudinal = smoothstep(0.0, startFadeWidth, axial01)
            * (1.0 - smoothstep(1.0 - endFadeWidth, 1.0, axial01));
    return (1.0 - exp(-2.0 * coverage)) * lateral * longitudinal;
}

float rectShaftDensityAt(
        float sampleT,
        vec3 rayDirection,
        vec3 axis,
        float edgeFeather,
        float cornerExponent,
        float startFadeWidth,
        float endFadeWidth) {
    vec3 samplePosition = cameraPosition + rayDirection * sampleT;
    vec3 sampleOffset = samplePosition - coneApex;
    float axial01 = clamp(
            dot(sampleOffset, axis) / max(coneLength, 0.0001),
            0.0,
            1.0);
    vec2 localHalfSize = max(
            mix(rectNearHalfSize, rectFarHalfSize, axial01),
            vec2(0.0001));
    vec2 normalizedLateral = abs(vec2(
            dot(sampleOffset, rectRight),
            dot(sampleOffset, rectUp))) / localHalfSize;
    float roundedRectDistance = pow(
            pow(normalizedLateral.x, cornerExponent)
                    + pow(normalizedLateral.y, cornerExponent),
            1.0 / cornerExponent);
    float lateral = 1.0 - smoothstep(
            1.0 - edgeFeather,
            1.0,
            roundedRectDistance);
    float longitudinal = smoothstep(0.0, startFadeWidth, axial01)
            * (1.0 - smoothstep(1.0 - endFadeWidth, 1.0, axial01));
    return lateral * longitudinal;
}

void main() {
    vec2 uv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float enterT = 0.0;
    float exitT = 0.0;
    bool hit = shaftShape == 1
            ? intersectRectFrustum(cameraPosition, rayDirection, enterT, exitT)
            : intersectFiniteCone(cameraPosition, rayDirection, enterT, exitT);
    if (!hit) discard;
    float depth = texture(sceneDepth, uv).r;
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float sceneDistance = depth >= 0.999999 ? farPlane
            : forwardDistance / max(dot(rayDirection, cameraForward), 0.0001);
    enterT = max(enterT, 0.0);
    exitT = min(exitT, sceneDistance);
    float chord = max(exitT - enterT, 0.0);
    if (chord <= 0.00001) discard;
    float midpointT = (enterT + exitT) * 0.5;
    vec3 midpoint = cameraPosition + rayDirection * midpointT;
    vec3 axis = safeNormalize(coneDirection, vec3(0.0, -1.0, 0.0));
    // The authored midpoint now matches the old maximum softness. The upper
    // half adds a gentler tail without expanding the finite cone.
    float mappedSoftness = clamp(shaftParams.x * 2.0, 0.02, 2.0);
    float baseSoftness = min(mappedSoftness, 1.0);
    float extraSoftness = max(mappedSoftness - 1.0, 0.0);
    float lateralExponent = mix(0.2, 2.5, baseSoftness)
            + 2.0 * extraSoftness;
    float startFadeWidth = min(mix(0.02, 0.18, baseSoftness)
            + 0.12 * extraSoftness, 0.30);
    float endFadeWidth = min(mix(0.04, 0.35, baseSoftness)
            + 0.20 * extraSoftness, 0.55);
    float opticalThickness = 0.0;
    if (shaftShape == 1) {
        float rectSoftness = clamp((shaftParams.x - 0.01) / 0.99, 0.0, 1.0);
        float edgeFeather = mix(0.02, 0.45, rectSoftness);
        float cornerExponent = mix(24.0, 4.0, rectSoftness);
        vec3 midpointOffset = midpoint - coneApex;
        float midpointAxial01 = clamp(
                dot(midpointOffset, axis) / max(coneLength, 0.0001),
                0.0,
                1.0);
        vec2 midpointHalfSize = mix(
                rectNearHalfSize,
                rectFarHalfSize,
                midpointAxial01);
        float localDiameter = max(
                2.0 * max(midpointHalfSize.x, midpointHalfSize.y),
                0.0001);
        // Saturating within half a local diameter prevents changes in the
        // intersected frustum face from drawing corner rays through the fog.
        float pathCoverage = smoothstep(
                0.0,
                0.5,
                max(chord / localDiameter, 0.0));
        // Fixed five-point Gauss-Legendre integration keeps the rounded
        // density stable as the view crosses the frustum's planar edges.
        float integratedDensity =
                0.1184634430 * rectShaftDensityAt(
                        enterT + chord * 0.0469100770,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth)
                + 0.2393143352 * rectShaftDensityAt(
                        enterT + chord * 0.2307653449,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth)
                + 0.2844444444 * rectShaftDensityAt(
                        enterT + chord * 0.5,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth)
                + 0.2393143352 * rectShaftDensityAt(
                        enterT + chord * 0.7692346551,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth)
                + 0.1184634430 * rectShaftDensityAt(
                        enterT + chord * 0.9530899230,
                        rayDirection, axis, edgeFeather, cornerExponent,
                        startFadeWidth, endFadeWidth);
        opticalThickness = 0.8646647168 * pathCoverage * integratedDensity;
    } else {
        // Fixed, unrolled samples avoid the single-midpoint profile trough that
        // becomes visible when an authored shaft origin is displaced.
        opticalThickness = (
                shaftOpticalProfileAt(
                        enterT + chord * (1.0 / 6.0),
                        rayDirection,
                        chord,
                        axis,
                        lateralExponent,
                        startFadeWidth,
                        endFadeWidth)
                + shaftOpticalProfileAt(
                        enterT + chord * 0.5,
                        rayDirection,
                        chord,
                        axis,
                        lateralExponent,
                        startFadeWidth,
                        endFadeWidth)
                + shaftOpticalProfileAt(
                        enterT + chord * (5.0 / 6.0),
                        rayDirection,
                        chord,
                        axis,
                        lateralExponent,
                        startFadeWidth,
                        endFadeWidth)) / 3.0;
    }
    float phaseFacing = clamp(dot(axis, -rayDirection) * 0.5 + 0.5, 0.0, 1.0);
    float phase = mix(0.20, 1.0, pow(phaseFacing, 3.0));
    float scatterWeight = opticalThickness * phase * fogTransmittance(midpoint);
    float extinction = clamp(shaftParams.y, 0.0, 1.0) * opticalThickness;
    if (scatterWeight <= 0.00001 && extinction <= 0.00001) discard;
    vec3 inScattering = min(max(shaftRadiance * scatterWeight, vec3(0.0)), vec3(65504.0));
    if (any(isnan(inScattering)) || any(isinf(inScattering))
            || isnan(extinction) || isinf(extinction)) discard;
    finalColor = vec4(inScattering, extinction);
}
