#pragma once

// Shared GLSL for opaque and alpha-tested surface receivers. Keeping the
// six-face mapping here prevents sector, door, model, and billboard shadows
// from disagreeing at cube-face boundaries.
#define SECTOR_DYNAMIC_SURFACE_SHADOW_GLSL R"glsl(
float SampleSpotShadowMap(vec2 atlasTile, float atlasScale, vec2 uv)
{
    return texture(shadowMap0,
            (atlasTile + clamp(uv, vec2(0.001), vec2(0.999)))
                    * atlasScale).r;
}

int PointShadowFace(vec3 ray)
{
    vec3 magnitude = abs(ray);
    if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z) {
        return ray.x >= 0.0 ? 0 : 1;
    }
    if (magnitude.y >= magnitude.z) return ray.y >= 0.0 ? 2 : 3;
    return ray.z >= 0.0 ? 4 : 5;
}

vec2 PointShadowFaceUv(int face, vec3 ray)
{
    vec3 magnitude = max(abs(ray), vec3(0.00001));
    vec2 projected;
    if (face == 0) projected = vec2(-ray.z, -ray.y) / magnitude.x;
    else if (face == 1) projected = vec2(ray.z, -ray.y) / magnitude.x;
    else if (face == 2) projected = vec2(ray.x, ray.z) / magnitude.y;
    else if (face == 3) projected = vec2(ray.x, -ray.z) / magnitude.y;
    else if (face == 4) projected = vec2(ray.x, -ray.y) / magnitude.z;
    else projected = vec2(-ray.x, -ray.y) / magnitude.z;
    return projected * 0.5 + 0.5;
}

vec3 PointShadowFaceRay(int face, vec2 uv)
{
    vec2 projected = uv * 2.0 - 1.0;
    if (face == 0) return vec3(1.0, -projected.y, -projected.x);
    if (face == 1) return vec3(-1.0, -projected.y, projected.x);
    if (face == 2) return vec3(projected.x, 1.0, projected.y);
    if (face == 3) return vec3(projected.x, -1.0, -projected.y);
    if (face == 4) return vec3(projected.x, -projected.y, 1.0);
    return vec3(-projected.x, -projected.y, -1.0);
}

float PointShadowPerspectiveDepth(float forwardDepth, float lightRadius)
{
    const float nearPlane = 0.05;
    float farCoefficient = lightRadius / max(lightRadius - nearPlane, 0.00001);
    return farCoefficient * (1.0 - nearPlane / max(forwardDepth, nearPlane));
}

float PointShadowSampleVisibility(
        int baseSlot,
        int sourceFace,
        vec2 sourceUv,
        ivec2 tileResolution,
        vec3 receiverPlaneNormal,
        float planeDistance,
        float lightRadius,
        float fallbackDepth,
        float effectiveBias)
{
    vec3 sourceRay = PointShadowFaceRay(sourceFace, sourceUv);
    int sampleFace = PointShadowFace(sourceRay);
    vec2 sampleUv = PointShadowFaceUv(sampleFace, sourceRay);
    ivec2 localTexel = clamp(
            ivec2(floor(sampleUv * vec2(tileResolution))),
            ivec2(0), tileResolution - ivec2(1));
    vec2 texelCenterUv = (vec2(localTexel) + vec2(0.5))
            / vec2(tileResolution);
    int sampleSlot = baseSlot + sampleFace;
    int atlasTiles = max(shadowAtlasTilesPerRow, 1);
    ivec2 atlasTile = ivec2(
            sampleSlot % atlasTiles, sampleSlot / atlasTiles);
    float blockerDepth = texelFetch(
            shadowMap0,
            atlasTile * tileResolution + localTexel,
            0).r;

    vec3 texelRay = PointShadowFaceRay(sampleFace, texelCenterUv);
    float planeDirection = dot(receiverPlaneNormal, texelRay);
    float receiverDepth = fallbackDepth;
    if (abs(planeDirection) > 0.000001) {
        float forwardDepth = planeDistance / planeDirection;
        if (forwardDepth > 0.05 && forwardDepth <= lightRadius) {
            receiverDepth = PointShadowPerspectiveDepth(
                    forwardDepth, lightRadius);
        }
    }
    return receiverDepth - effectiveBias <= blockerDepth ? 1.0 : 0.0;
}

float DynamicLightShadowVisibility(
        int lightIndex,
        int shadowSlot,
        vec3 worldPosition,
        vec3 worldNormal,
        vec3 surfaceToLightDirection)
{
    if (shadowSlot < 0 || shadowSlot >= MAX_DYNAMIC_SHADOW_CASTERS) {
        return 1.0;
    }

    vec3 fromLight = worldPosition - dynamicLightPositions[lightIndex];
    bool pointProjection = dynamicLightTypes[lightIndex] == 0;
    float normalLightDot = max(dot(
            SafeNormalize(worldNormal, vec3(0.0, 1.0, 0.0)),
            SafeNormalize(surfaceToLightDirection, vec3(0.0, 1.0, 0.0))), 0.0);
    float effectiveBias = min(
            shadowBias[shadowSlot]
                    * (1.0 + (1.0 - normalLightDot) * 2.0),
            0.02);
    float softness = clamp(shadowSoftness[shadowSlot], 0.0, 8.0);
    int atlasTiles = max(shadowAtlasTilesPerRow, 1);
    ivec2 tileResolution = textureSize(shadowMap0, 0) / atlasTiles;

    if (pointProjection) {
        float lightRadius = max(dynamicLightRadii[lightIndex], 0.00001);
        vec3 magnitude = abs(fromLight);
        float forwardDepth = max(magnitude.x, max(magnitude.y, magnitude.z));
        if (forwardDepth <= 0.05 || forwardDepth > lightRadius) return 1.0;
        int face = PointShadowFace(fromLight);
        vec2 uv = PointShadowFaceUv(face, fromLight);
        float receiverDepth = PointShadowPerspectiveDepth(
                forwardDepth, lightRadius);
        float planeDistance = dot(worldNormal, fromLight);

        if (softness <= 0.0) {
            vec2 texelPosition = uv * vec2(tileResolution) - vec2(0.5);
            ivec2 baseTexel = ivec2(floor(texelPosition));
            vec2 blend = fract(texelPosition);
            float visible = 0.0;
            for (int y = 0; y < 2; ++y) {
                for (int x = 0; x < 2; ++x) {
                    vec2 sourceUv = (vec2(baseTexel + ivec2(x, y))
                            + vec2(0.5)) / vec2(tileResolution);
                    float weight = (x == 0 ? 1.0 - blend.x : blend.x)
                            * (y == 0 ? 1.0 - blend.y : blend.y);
                    visible += weight * PointShadowSampleVisibility(
                            shadowSlot, face, sourceUv, tileResolution,
                            worldNormal, planeDistance, lightRadius,
                            receiverDepth, effectiveBias);
                }
            }
            return visible;
        }

        vec2 radius = max(0.25, softness) / vec2(tileResolution);
        float visible = 0.0;
        for (int i = 0; i < 12; ++i) {
            visible += PointShadowSampleVisibility(
                    shadowSlot,
                    face,
                    uv + kPoissonDisk[i] * radius,
                    tileResolution,
                    worldNormal,
                    planeDistance,
                    lightRadius,
                    receiverDepth,
                    effectiveBias);
        }
        return visible / 12.0;
    }

    vec3 forward = dynamicLightDirections[lightIndex];
    vec3 right = dynamicLightSpotShadowRight[lightIndex];
    vec3 up = cross(right, forward);
    float spotForwardDepth = dot(fromLight, forward);
    if (spotForwardDepth <= 0.05) return 1.0;
    vec2 projection = dynamicLightSpotShadowProjection[lightIndex];
    vec3 shadowCoord = vec3(
            vec2(dot(fromLight, right), dot(fromLight, up))
                    * projection.x / max(2.0 * spotForwardDepth, 0.00001)
                    + 0.5,
            projection.y * (1.0 - 0.05 / spotForwardDepth));
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0
            || shadowCoord.y < 0.0 || shadowCoord.y > 1.0
            || shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        return 1.0;
    }

    ivec2 atlasTile = ivec2(
            shadowSlot % atlasTiles, shadowSlot / atlasTiles);
    float atlasScale = 1.0 / float(atlasTiles);
    if (softness <= 0.0) {
        float blockerDepth = SampleSpotShadowMap(
                vec2(atlasTile), atlasScale, shadowCoord.xy);
        return shadowCoord.z - effectiveBias <= blockerDepth ? 1.0 : 0.0;
    }

    vec2 radius = max(0.25, softness) / vec2(tileResolution);
    float visible = 0.0;
    for (int i = 0; i < 12; ++i) {
        float blockerDepth = SampleSpotShadowMap(
                vec2(atlasTile),
                atlasScale,
                shadowCoord.xy + kPoissonDisk[i] * radius);
        visible += shadowCoord.z - effectiveBias <= blockerDepth ? 1.0 : 0.0;
    }
    return visible / 12.0;
}
)glsl"
