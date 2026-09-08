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
uniform vec3 fogCenter;
uniform vec3 fogRadii;
uniform vec3 fogColor;
uniform vec4 fogParams; // start, end, exponent, maximum opacity
uniform int fogShape; // 0 ellipsoid, 1 yaw-oriented box
uniform int fogStyle; // 0 cloudy volume, 1 room fog
uniform float fogYaw;
uniform vec4 fogNoiseParams; // edge softness, scale, amount, runtime seconds
uniform vec2 fogFlow; // direction radians, speed world units/second
uniform vec2 fogEdgeParams; // edge width, maximum cloudy outward jitter
uniform vec3 fogLighting0;
uniform vec3 fogLighting1;
uniform vec3 fogLighting2;
uniform vec3 fogLighting3;

float hash31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 p) {
    vec3 cell = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash31(cell + vec3(0,0,0));
    float n100 = hash31(cell + vec3(1,0,0));
    float n010 = hash31(cell + vec3(0,1,0));
    float n110 = hash31(cell + vec3(1,1,0));
    float n001 = hash31(cell + vec3(0,0,1));
    float n101 = hash31(cell + vec3(1,0,1));
    float n011 = hash31(cell + vec3(0,1,1));
    float n111 = hash31(cell + vec3(1,1,1));
    return mix(mix(mix(n000,n100,f.x), mix(n010,n110,f.x), f.y),
               mix(mix(n001,n101,f.x), mix(n011,n111,f.x), f.y), f.z);
}

vec3 worldToBox(vec3 value) {
    float cosine = cos(fogYaw);
    float sine = sin(fogYaw);
    return vec3(
            cosine * value.x - sine * value.z,
            value.y,
            sine * value.x + cosine * value.z);
}

bool intersectEllipsoid(
        vec3 origin,
        vec3 direction,
        vec3 radii,
        out float enterT,
        out float exitT) {
    vec3 inverseRadii = 1.0 / max(radii, vec3(0.0001));
    vec3 o = (origin - fogCenter) * inverseRadii;
    vec3 d = direction * inverseRadii;
    float a = dot(d, d);
    float b = 2.0 * dot(o, d);
    float c = dot(o, o) - 1.0;
    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0 || a <= 0.0) return false;
    float root = sqrt(discriminant);
    enterT = (-b - root) / (2.0 * a);
    exitT = (-b + root) / (2.0 * a);
    return exitT > max(enterT, 0.0);
}

bool intersectBox(
        vec3 origin,
        vec3 direction,
        vec3 inputRadii,
        out float enterT,
        out float exitT) {
    vec3 radii = max(inputRadii, vec3(0.0001));
    vec3 o = worldToBox(origin - fogCenter);
    vec3 d = worldToBox(direction);
    const float epsilon = 0.000001;
    if ((abs(d.x) <= epsilon && abs(o.x) > radii.x)
            || (abs(d.y) <= epsilon && abs(o.y) > radii.y)
            || (abs(d.z) <= epsilon && abs(o.z) > radii.z)) return false;
    vec3 inverseDirection = vec3(
            abs(d.x) > epsilon ? 1.0 / d.x : 1e20,
            abs(d.y) > epsilon ? 1.0 / d.y : 1e20,
            abs(d.z) > epsilon ? 1.0 / d.z : 1e20);
    vec3 t0 = (-radii - o) * inverseDirection;
    vec3 t1 = (radii - o) * inverseDirection;
    vec3 minimumT = min(t0, t1);
    vec3 maximumT = max(t0, t1);
    enterT = max(max(minimumT.x, minimumT.y), minimumT.z);
    exitT = min(min(maximumT.x, maximumT.y), maximumT.z);
    return exitT > max(enterT, 0.0);
}

vec3 normalizedLocalPosition(vec3 worldPosition) {
    vec3 offset = worldPosition - fogCenter;
    if (fogShape == 1) offset = worldToBox(offset);
    return offset / max(fogRadii, vec3(0.0001));
}

float sampleFogNoiseAtScale(
        vec3 worldPosition,
        vec2 flowWorld,
        float noiseScale) {
    vec3 noisePosition = worldPosition;
    noisePosition.xz -= flowWorld;
    return smoothstep(
            0.15,
            0.85,
            valueNoise(noisePosition / max(noiseScale, 0.05)));
}

float sampleFogNoise(vec3 worldPosition, vec2 flowWorld) {
    return sampleFogNoiseAtScale(
            worldPosition,
            flowWorld,
            fogNoiseParams.y);
}

float roundedBoxDistance(vec3 localPosition, vec3 halfExtents, float radius) {
    vec3 q = abs(localPosition) - max(halfExtents - vec3(radius), vec3(0.0001));
    return length(max(q, vec3(0.0)))
            + min(max(q.x, max(q.y, q.z)), 0.0)
            - radius;
}

float boxDistance(vec3 localPosition, vec3 halfExtents) {
    vec3 q = abs(localPosition) - halfExtents;
    return length(max(q, vec3(0.0)))
            + min(max(q.x, max(q.y, q.z)), 0.0);
}

float ellipsoidDistance(vec3 localPosition) {
    float minimumRadius = min(fogRadii.x, min(fogRadii.y, fogRadii.z));
    float normalizedRadius = length(
            localPosition / max(fogRadii, vec3(0.0001)));
    return (normalizedRadius - 1.0) * minimumRadius;
}

float analyticShapeDistance(vec3 localPosition, bool roundedCloudyBox) {
    if (fogShape == 0) return ellipsoidDistance(localPosition);
    if (!roundedCloudyBox) {
        return boxDistance(localPosition, max(fogRadii, vec3(0.0001)));
    }
    float minimumRadius = min(fogRadii.x, min(fogRadii.y, fogRadii.z));
    return roundedBoxDistance(
            localPosition,
            max(fogRadii, vec3(0.0001)),
            minimumRadius * 0.15);
}

float cloudyBoundary(vec3 localPosition, float silhouetteNoise) {
    float edgeJitter = (silhouetteNoise - 0.5) * 2.0 * fogEdgeParams.y;
    float distance = analyticShapeDistance(localPosition, true);
    return smoothstep(
            0.0,
            max(fogEdgeParams.x, 0.0001),
            -distance + edgeJitter);
}

float roomBoundary(vec3 localPosition) {
    float distance = analyticShapeDistance(localPosition, false);
    return smoothstep(
            0.0,
            max(fogEdgeParams.x, 0.0001),
            -distance);
}

vec3 interpolateFogLighting(vec2 normalizedLocalXZ) {
    const float footprint = 0.5;
    vec2 uv = clamp(
            normalizedLocalXZ / (2.0 * footprint) + 0.5,
            vec2(0.0),
            vec2(1.0));
    vec3 negativeZ = mix(fogLighting0, fogLighting1, uv.x);
    vec3 positiveZ = mix(fogLighting2, fogLighting3, uv.x);
    return mix(negativeZ, positiveZ, uv.y);
}

void main() {
    vec2 uv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float enterT = 0.0;
    float exitT = 0.0;
    vec3 intersectionRadii = fogRadii
            + vec3(fogStyle == 0 ? fogEdgeParams.y : 0.0);
    bool intersects = fogShape == 1
            ? intersectBox(
                    cameraPosition,
                    rayDirection,
                    intersectionRadii,
                    enterT,
                    exitT)
            : intersectEllipsoid(
                    cameraPosition,
                    rayDirection,
                    intersectionRadii,
                    enterT,
                    exitT);
    if (!intersects) discard;
    enterT = max(enterT, 0.0);
    if (exitT <= enterT) discard;
    vec2 flowWorld = vec2(cos(fogFlow.x), sin(fogFlow.x))
            * fogFlow.y * fogNoiseParams.w;
    float depth = texture(sceneDepth, uv).r;
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float sceneDistance = depth >= 0.999999 ? farPlane
            : forwardDistance / max(dot(rayDirection, cameraForward), 0.0001);
    exitT = min(exitT, sceneDistance);
    float chord = max(exitT - enterT, 0.0);
    if (chord <= 0.0) discard;
    float midpointT = (enterT + exitT) * 0.5;
    vec3 midpoint = cameraPosition + rayDirection * midpointT;
    float pixelsPerWorld = viewportSize.y / max(
            2.0 * tanHalfFov * max(midpointT, nearPlane),
            0.0001);
    float projectedNoisePixels = max(fogNoiseParams.y, 0.05) * pixelsPerWorld;
    float noiseDetail = smoothstep(4.0, 16.0, projectedNoisePixels);
    float minimumRadius = min(fogRadii.x, min(fogRadii.y, fogRadii.z));
    float projectedMinimumDiameterPixels = 2.0 * minimumRadius * pixelsPerWorld;
    float peakAttenuation = mix(
            0.65,
            1.0,
            smoothstep(48.0, 192.0, projectedMinimumDiameterPixels));
    float smallVolumeVisibility = smoothstep(
            12.0,
            24.0,
            projectedMinimumDiameterPixels);
    float noiseModulation = 1.0;
    if (fogNoiseParams.z > 0.0001) {
        if (fogStyle == 0) {
            float nearNoise = sampleFogNoise(
                    cameraPosition + rayDirection * mix(enterT, exitT, 0.20),
                    flowWorld);
            float middleNoise = sampleFogNoise(midpoint, flowWorld);
            float farNoise = sampleFogNoise(
                    cameraPosition + rayDirection * mix(enterT, exitT, 0.80),
                    flowWorld);
            nearNoise = mix(0.5, nearNoise, noiseDetail);
            middleNoise = mix(0.5, middleNoise, noiseDetail);
            farNoise = mix(0.5, farNoise, noiseDetail);
            float integratedNoise = nearNoise * 0.25
                    + middleNoise * 0.50
                    + farNoise * 0.25;
            noiseModulation = mix(
                    1.0,
                    mix(0.30, 1.70, integratedNoise),
                    fogNoiseParams.z);
        } else {
            float filteredNoise = mix(
                    0.5,
                    sampleFogNoise(midpoint, flowWorld),
                    noiseDetail);
            noiseModulation = mix(
                    1.0,
                    mix(0.80, 1.20, filteredNoise),
                    fogNoiseParams.z);
        }
    }
    vec3 midpointLocal = midpoint - fogCenter;
    if (fogShape == 1) midpointLocal = worldToBox(midpointLocal);
    float boundary = 1.0;
    if (fogStyle == 0) {
        float silhouetteScale = max(fogNoiseParams.y, minimumRadius * 0.75);
        float silhouetteNoise = sampleFogNoiseAtScale(
                midpoint,
                flowWorld,
                silhouetteScale);
        boundary = cloudyBoundary(midpointLocal, silhouetteNoise);
    } else {
        boundary = roomBoundary(midpointLocal);
    }
    if (boundary <= 0.00001) discard;
    float shapedPath = chord * boundary * noiseModulation;
    float range = max(fogParams.y - fogParams.x, 0.0001);
    float pathProfile = clamp((shapedPath - fogParams.x) / range, 0.0, 1.0);
    float opacity = clamp(fogParams.w, 0.0, 1.0)
            * pow(pathProfile, max(fogParams.z, 0.0001))
            * peakAttenuation
            * smallVolumeVisibility;
    if (opacity <= 0.00001) discard;
    vec3 midpointNormalized = normalizedLocalPosition(midpoint);
    vec3 staticLighting = interpolateFogLighting(midpointNormalized.xz);
    vec3 resultColor = min(
            max(fogColor * staticLighting, vec3(0.0)),
            vec3(65504.0));
    if (any(isnan(resultColor)) || any(isinf(resultColor))
            || isnan(opacity) || isinf(opacity)) discard;
    finalColor = vec4(resultColor, opacity);
}
