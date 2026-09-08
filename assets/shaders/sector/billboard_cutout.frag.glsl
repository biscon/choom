#version 330
in vec2 fragTexCoord;
in vec3 fragWorldPosition;
in vec4 fragColor;

uniform sampler2D texture0;
uniform float alphaCutoff;
uniform vec3 bakedBillboardLighting;

uniform int fogEnabled;
uniform vec3 fogColor;
uniform vec3 fogCameraPosition;
uniform float fogStartDistanceWorld;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeightWorld;
uniform float fogHeightFalloff;

#define MAX_DYNAMIC_LIGHTS 32
#define MAX_DYNAMIC_SHADOW_CASTERS 64
uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightTypes[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightDirections[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightSpotShadowRight[MAX_DYNAMIC_LIGHTS];
uniform vec2 dynamicLightSpotShadowProjection[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightProfiles[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightProfileParameters[MAX_DYNAMIC_LIGHTS];
uniform sampler2D flashlightCookie;
uniform int hasPointShadows;
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform int shadowAtlasTilesPerRow;
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

out vec4 finalColor;

const vec2 kPoissonDisk[12] = vec2[12](
    vec2(-0.326, -0.406),
    vec2(-0.840, -0.074),
    vec2(-0.696,  0.457),
    vec2(-0.203,  0.621),
    vec2( 0.962, -0.195),
    vec2( 0.473, -0.480),
    vec2( 0.519,  0.767),
    vec2( 0.185, -0.893),
    vec2( 0.507,  0.064),
    vec2( 0.896,  0.412),
    vec2(-0.322, -0.933),
    vec2(-0.792, -0.598)
);

#include "safe_normalize.glsl"


#include "flashlight_profile.glsl"

#include "finite_half_radiance.glsl"


#include "dynamic_shadow.glsl"

#include "surface_fog.glsl"

void main()
{
    vec3 receiverPlaneNormal = vec3(0.0, 1.0, 0.0);
    if (hasPointShadows != 0) {
        receiverPlaneNormal = SafeNormalize(
                cross(dFdx(fragWorldPosition), dFdy(fragWorldPosition)),
                vec3(0.0, 1.0, 0.0));
    }
    vec4 sampled = texture(texture0, fragTexCoord);
    if (sampled.a < alphaCutoff) {
        discard;
    }

    vec3 dynamicDirect = vec3(0.0);
    for (int i = 0; i < dynamicLightCount && i < MAX_DYNAMIC_LIGHTS; ++i) {
        float radius = dynamicLightRadii[i];
        vec3 toLight = dynamicLightPositions[i] - fragWorldPosition;
        float emitterAtten = 1.0;
        if (dynamicLightTypes[i] == 2) {
            vec3 emitterNormal = SafeNormalize(dynamicLightDirections[i], vec3(0.0, -1.0, 0.0));
            vec3 emitterRight = SafeNormalize(dynamicLightSpotShadowRight[i], vec3(1.0, 0.0, 0.0));
            vec3 emitterUp = SafeNormalize(cross(emitterRight, emitterNormal), vec3(0.0, 0.0, 1.0));
            vec3 relative = fragWorldPosition - dynamicLightPositions[i];
            vec3 nearest = dynamicLightPositions[i]
                    + emitterRight * clamp(dot(relative, emitterRight), -dynamicLightInnerConeCos[i], dynamicLightInnerConeCos[i])
                    + emitterUp * clamp(dot(relative, emitterUp), -dynamicLightOuterConeCos[i], dynamicLightOuterConeCos[i]);
            toLight = nearest - fragWorldPosition;
            emitterAtten = max(dot(emitterNormal, SafeNormalize(fragWorldPosition - nearest, emitterNormal)), 0.0);
        }
        float distanceSq = dot(toLight, toLight);
        if (radius > 0.0 && distanceSq < radius * radius) {
            float distanceToLight = sqrt(max(distanceSq, 0.0));
            vec3 lightDirection = distanceToLight > 0.0001
                    ? toLight / distanceToLight
                    : vec3(0.0, 1.0, 0.0);
            float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
            atten *= atten;
            if (atten <= 0.0 || dynamicLightIntensities[i] <= 0.0) continue;
            float coneAtten = emitterAtten;
            if (dynamicLightTypes[i] == 1) {
                vec3 spotDirection = SafeNormalize(dynamicLightDirections[i], vec3(0.0, -1.0, 0.0));
                vec3 fragmentDirectionFromLight = distanceToLight > 0.0001
                        ? -lightDirection
                        : spotDirection;
                float coneDot = dot(spotDirection, fragmentDirectionFromLight);
                float innerConeCos = dynamicLightInnerConeCos[i];
                float outerConeCos = dynamicLightOuterConeCos[i];
                if (dynamicLightProfiles[i] == 1) {
                    coneAtten = FlashlightProfileFactor(
                            i, fragmentDirectionFromLight);
                } else {
                    coneAtten = abs(innerConeCos - outerConeCos) > 0.0001
                            ? smoothstep(outerConeCos, innerConeCos, coneDot)
                            : step(innerConeCos, coneDot);
                }
            }
            if (coneAtten <= 0.0) continue;
            int shadowSlot = dynamicLightShadowSlots[i];
            if (shadowSlot >= 0 && shadowStrength[shadowSlot] > 0.0) {
                float visibility = DynamicLightShadowVisibility(
                        i, shadowSlot, fragWorldPosition, receiverPlaneNormal, lightDirection);
                coneAtten *= mix(1.0, visibility, clamp(shadowStrength[shadowSlot], 0.0, 1.0));
            }
            dynamicDirect += dynamicLightColors[i] * dynamicLightIntensities[i] * atten * coneAtten;
        }
    }

    vec3 surfaceRgb = sampled.rgb * fragColor.rgb;
    vec3 lighting = max(bakedBillboardLighting + dynamicDirect, vec3(0.0));
    finalColor = vec4(StoreFiniteHalfRadiance(ApplySectorFog(
            surfaceRgb * lighting,
            bakedBillboardLighting,
            fragWorldPosition)), 1.0);
}
