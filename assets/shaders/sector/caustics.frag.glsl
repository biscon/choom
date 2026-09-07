#version 330
in vec2 fragUv;
out vec4 finalColor;

uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D causticsLookup;
uniform vec2 viewportSize;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform float nearPlane;
uniform float farPlane;
uniform float liquidSurfaceY;
uniform float liquidVisibilityDepth;
// scale world, strength, speed, wave amount
uniform vec4 liquidRipple;
// direction radians, flow speed
uniform vec2 liquidFlow;
// caustics strength, scale multiplier, speed multiplier
uniform vec3 causticsVisual;
uniform float runtimeSeconds;

vec3 ReconstructWorldPosition(vec2 uv, float depth, out float sceneDistance)
{
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane
                    - zNdc * (farPlane - nearPlane), 0.00001);
    sceneDistance = forwardDistance
            / max(dot(rayDirection, cameraForward), 0.0001);
    return cameraPosition + rayDirection * sceneDistance;
}

mat2 Rotation(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, -s, s, c);
}

float SampleCausticLayer(
        vec2 projectedWorld,
        vec2 projectedFlowDirection,
        float rotationOffset)
{
    float worldScale = max(liquidRipple.x * causticsVisual.y, 0.05);
    float phase = runtimeSeconds * liquidRipple.z * causticsVisual.z;
    vec2 flowWorld = projectedFlowDirection * runtimeSeconds
            * min(max(liquidFlow.y, 0.0), 4.0) * 0.12;
    vec2 primaryUv = Rotation(0.21 + rotationOffset)
            * ((projectedWorld - flowWorld) / worldScale)
            + vec2(phase * 0.018, -phase * 0.013);
    vec2 secondaryUv = Rotation(-0.63 + rotationOffset * 0.37)
            * ((projectedWorld + flowWorld * 0.35) / (worldScale * 0.71))
            + vec2(-phase * 0.011, phase * 0.016)
            + vec2(0.37, 0.19);
    float primary = texture(causticsLookup, primaryUv).r;
    float secondary = texture(causticsLookup, secondaryUv).r;
    float softened = primary * (0.67 + secondary * 0.33);
    return smoothstep(0.10, 0.82, softened);
}

void main()
{
    vec2 screenUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec4 scene = texture(sceneColor, screenUv);
    float depth = texture(sceneDepth, screenUv).r;
    if (depth >= 0.999999 || causticsVisual.x <= 0.0) {
        finalColor = scene;
        return;
    }

    float sceneDistance = 0.0;
    vec3 world = ReconstructWorldPosition(screenUv, depth, sceneDistance);
    float belowSurface = liquidSurfaceY - world.y;
    if (belowSurface < 0.0) {
        finalColor = scene;
        return;
    }

    vec3 dx = dFdx(world);
    vec3 dy = dFdy(world);
    float expectedPixelWorld = max(
            2.0 * sceneDistance * tanHalfFov
                    / max(viewportSize.y, 1.0),
            0.0005);
    float largestDerivative = max(length(dx), length(dy));
    float continuity = 1.0 - smoothstep(
            expectedPixelWorld * 6.0,
            expectedPixelWorld * 18.0,
            largestDerivative);
    vec3 normalCross = cross(dx, dy);
    float normalLength = length(normalCross);
    if (continuity <= 0.0001 || normalLength <= 0.0000001) {
        finalColor = scene;
        return;
    }
    vec3 normal = normalCross / normalLength;
    vec3 absoluteNormal = abs(normal);
    vec3 projectionWeights = pow(absoluteNormal, vec3(4.0));
    projectionWeights /= max(
            projectionWeights.x + projectionWeights.y + projectionWeights.z,
            0.000001);
    vec2 horizontalFlowDirection = vec2(
            cos(liquidFlow.x), sin(liquidFlow.x));
    float pattern = 0.0;
    if (projectionWeights.x > 0.001) {
        pattern += SampleCausticLayer(
                        world.zy,
                        vec2(horizontalFlowDirection.y, 0.0),
                        1.17)
                * projectionWeights.x;
    }
    if (projectionWeights.y > 0.001) {
        pattern += SampleCausticLayer(
                        world.xz,
                        horizontalFlowDirection,
                        0.0)
                * projectionWeights.y;
    }
    if (projectionWeights.z > 0.001) {
        pattern += SampleCausticLayer(
                        world.xy,
                        vec2(horizontalFlowDirection.x, 0.0),
                        -0.91)
                * projectionWeights.z;
    }

    float depthFade = exp(-belowSurface / max(liquidVisibilityDepth, 0.05));
    float receiver = mix(0.35, 1.0, absoluteNormal.y);
    float surfaceMotion = mix(
            0.55,
            1.0,
            clamp(liquidRipple.w / 0.5, 0.0, 1.0));
    float effectStrength = clamp(causticsVisual.x, 0.0, 1.0)
            * depthFade * receiver * continuity * surfaceMotion;
    float signedModulation = (pattern - 0.38) * effectStrength * 0.65;
    float luminance = dot(max(scene.rgb, vec3(0.0)),
            vec3(0.2126, 0.7152, 0.0722));
    if (signedModulation > 0.0) {
        signedModulation *= 1.0 - smoothstep(0.80, 1.25, luminance);
    }
    float modulation = clamp(1.0 + signedModulation, 0.75, 1.35);
    float darkSurfaceFactor = 1.0 - smoothstep(0.08, 0.30, luminance);
    float positivePattern = max(pattern - 0.38, 0.0);
    float darkSurfaceLift = min(
            positivePattern * effectStrength * 0.10 * darkSurfaceFactor,
            0.04);
    vec3 rgb = max(scene.rgb, vec3(0.0)) * modulation
            + vec3(darkSurfaceLift);
    finalColor = vec4(min(rgb, vec3(65504.0)), scene.a);
}
