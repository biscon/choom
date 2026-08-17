#include "sector_demo/renderer/SectorStaticModelRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "engine/components/AnimatedModel.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorStaticModelTransform.h"

#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace game {
namespace {

const char* SectorStaticModelVs = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexTangent;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec4 vertexColor;
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
uniform vec4 lightmapScaleBias;
uniform int useSkinning;
#define MAX_BONE_NUM 128
uniform mat4 boneMatrices[MAX_BONE_NUM];

out vec2 fragTexCoord;
out vec2 fragLightmapTexCoord;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec3 fragWorldTangent;
out float fragTangentSign;
out vec4 fragColor;

void main()
{
    vec4 localPosition = vec4(vertexPosition, 1.0);
    vec4 localNormal = vec4(vertexNormal, 0.0);
    vec4 localTangent = vec4(vertexTangent.xyz, 0.0);
    if (useSkinning != 0) {
        int bone0 = int(vertexBoneIndices.x);
        int bone1 = int(vertexBoneIndices.y);
        int bone2 = int(vertexBoneIndices.z);
        int bone3 = int(vertexBoneIndices.w);
        localPosition = vertexBoneWeights.x * (boneMatrices[bone0] * localPosition)
                + vertexBoneWeights.y * (boneMatrices[bone1] * localPosition)
                + vertexBoneWeights.z * (boneMatrices[bone2] * localPosition)
                + vertexBoneWeights.w * (boneMatrices[bone3] * localPosition);
        localNormal = vertexBoneWeights.x * (boneMatrices[bone0] * localNormal)
                + vertexBoneWeights.y * (boneMatrices[bone1] * localNormal)
                + vertexBoneWeights.z * (boneMatrices[bone2] * localNormal)
                + vertexBoneWeights.w * (boneMatrices[bone3] * localNormal);
        localTangent = vertexBoneWeights.x * (boneMatrices[bone0] * localTangent)
                + vertexBoneWeights.y * (boneMatrices[bone1] * localTangent)
                + vertexBoneWeights.z * (boneMatrices[bone2] * localTangent)
                + vertexBoneWeights.w * (boneMatrices[bone3] * localTangent);
    }
    fragTexCoord = vertexTexCoord;
    fragLightmapTexCoord =
            vertexTexCoord2 * lightmapScaleBias.xy + lightmapScaleBias.zw;
    fragWorldPosition = (matModel * localPosition).xyz;
    fragWorldNormal = normalize((matNormal * localNormal).xyz);
    fragWorldTangent = normalize((matModel * localTangent).xyz);
    fragTangentSign = vertexTangent.w;
    fragColor = vertexColor;
    gl_Position = mvp * localPosition;
}
)";

const char* SectorStaticModelFs = R"(
#version 330
in vec2 fragTexCoord;
in vec2 fragLightmapTexCoord;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec3 fragWorldTangent;
in float fragTangentSign;
in vec4 fragColor;

uniform sampler2D baseColorTexture;
uniform sampler2D metallicTexture;
uniform sampler2D normalTexture;
uniform sampler2D roughnessTexture;
uniform sampler2D occlusionTexture;
uniform sampler2D emissiveTexture;
uniform sampler2D lightmapTexture;
uniform samplerCube environmentTexture;
uniform vec4 baseColorFactor;
uniform vec3 emissiveFactor;
uniform float emissiveStrength;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform float normalScale;
uniform float occlusionStrength;
uniform float modelOpacity;
uniform int hasBaseColorTexture;
uniform int hasMetallicTexture;
uniform int hasNormalTexture;
uniform int hasRoughnessTexture;
uniform int hasOcclusionTexture;
uniform int hasEmissiveTexture;
uniform int baseColorTextureHardwareSrgb;
uniform int emissiveTextureHardwareSrgb;
uniform int hasEnvironment;
uniform vec3 cameraPosition;
uniform float environmentExposure;
uniform float outputBrightnessMultiplier;
uniform vec3 containingSectorAmbient;
uniform int hasStaticLightmap;
uniform int useBakedAmbientOcclusion;
uniform int useObjectProbeLighting;
uniform vec3 objectAmbientCube[6];
uniform vec3 objectAmbientCubeUpper[6];
uniform float objectAmbientCubeLowerHeight;
uniform float objectAmbientCubeUpperHeight;
uniform int useVerticalObjectProbeLighting;
uniform int pbrDiagnosticMode;
uniform float indirectDiffuseScale;
uniform float environmentSpecularScale;
uniform int useStaticSpecularLighting;

uniform int fogEnabled;
uniform vec3 fogColor;
uniform vec3 fogCameraPosition;
uniform float fogStartDistanceWorld;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeightWorld;
uniform float fogHeightFalloff;

#define MAX_DYNAMIC_LIGHTS 8
#define MAX_DYNAMIC_SHADOW_CASTERS 2
uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightTypes[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightDirections[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];
uniform mat4 shadowLightMatrices[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

#define MAX_STATIC_SPECULAR_LIGHTS 4
uniform int staticSpecularLightCount;
uniform vec3 staticSpecularLightPositions[MAX_STATIC_SPECULAR_LIGHTS];
uniform vec3 staticSpecularLightColors[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightRadii[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightIntensities[MAX_STATIC_SPECULAR_LIGHTS];
uniform int staticSpecularLightTypes[MAX_STATIC_SPECULAR_LIGHTS];
uniform vec3 staticSpecularLightDirections[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightInnerConeCos[MAX_STATIC_SPECULAR_LIGHTS];
uniform float staticSpecularLightOuterConeCos[MAX_STATIC_SPECULAR_LIGHTS];
out vec4 finalColor;

const vec2 kPoissonDisk[12] = vec2[12](
    vec2(-0.326, -0.406), vec2(-0.840, -0.074),
    vec2(-0.696,  0.457), vec2(-0.203,  0.621),
    vec2( 0.962, -0.195), vec2( 0.473, -0.480),
    vec2( 0.519,  0.767), vec2( 0.185, -0.893),
    vec2( 0.507,  0.064), vec2( 0.896,  0.412),
    vec2(-0.322, -0.933), vec2(-0.792, -0.598)
);

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSq = dot(value, value);
    return lengthSq > 0.00000001 ? value * inversesqrt(lengthSq) : fallback;
}

vec3 SrgbToLinear(vec3 value)
{
    bvec3 cutoff = lessThanEqual(value, vec3(0.04045));
    vec3 low = value / 12.92;
    vec3 high = pow((value + 0.055) / 1.055, vec3(2.4));
    return mix(high, low, cutoff);
}

vec3 DecodeColorTexture(vec3 value, int hardwareSrgb)
{
    return hardwareSrgb != 0 ? value : SrgbToLinear(value);
}

float DistributionGgx(vec3 normal, vec3 halfway, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(normal, halfway), 0.0);
    float denominator = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denominator * denominator, 0.000001);
}

float GeometrySchlickGgx(float ndotv, float roughness)
{
    float r = roughness + 1.0;
    float k = r * r / 8.0;
    return ndotv / max(ndotv * (1.0 - k) + k, 0.000001);
}

float GeometrySmith(vec3 normal, vec3 viewDirection, vec3 lightDirection, float roughness)
{
    return GeometrySchlickGgx(max(dot(normal, viewDirection), 0.0), roughness)
            * GeometrySchlickGgx(max(dot(normal, lightDirection), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 EvaluateObjectAmbientCube(vec3 normal)
{
    vec3 weights = normal * normal;
    vec3 xLighting = objectAmbientCube[normal.x >= 0.0 ? 0 : 1];
    vec3 yLighting = objectAmbientCube[normal.y >= 0.0 ? 2 : 3];
    vec3 zLighting = objectAmbientCube[normal.z >= 0.0 ? 4 : 5];
    vec3 lowerLighting = xLighting * weights.x
            + yLighting * weights.y
            + zLighting * weights.z;
    if (useVerticalObjectProbeLighting == 0) return lowerLighting;

    vec3 upperXLighting = objectAmbientCubeUpper[normal.x >= 0.0 ? 0 : 1];
    vec3 upperYLighting = objectAmbientCubeUpper[normal.y >= 0.0 ? 2 : 3];
    vec3 upperZLighting = objectAmbientCubeUpper[normal.z >= 0.0 ? 4 : 5];
    vec3 upperLighting = upperXLighting * weights.x
            + upperYLighting * weights.y
            + upperZLighting * weights.z;
    float heightRange = objectAmbientCubeUpperHeight - objectAmbientCubeLowerHeight;
    float blend = heightRange > 0.0001
            ? clamp((fragWorldPosition.y - objectAmbientCubeLowerHeight) / heightRange, 0.0, 1.0)
            : 0.0;
    return mix(lowerLighting, upperLighting, blend);
}

vec3 EvaluateFogObjectProbeLighting()
{
    vec3 lowerLighting = (
            objectAmbientCube[0]
            + objectAmbientCube[1]
            + objectAmbientCube[2]
            + objectAmbientCube[4]
            + objectAmbientCube[5]) / 5.0;
    if (useVerticalObjectProbeLighting == 0) return lowerLighting;

    vec3 upperLighting = (
            objectAmbientCubeUpper[0]
            + objectAmbientCubeUpper[1]
            + objectAmbientCubeUpper[2]
            + objectAmbientCubeUpper[4]
            + objectAmbientCubeUpper[5]) / 5.0;
    float heightRange = objectAmbientCubeUpperHeight - objectAmbientCubeLowerHeight;
    float blend = heightRange > 0.0001
            ? clamp((fragWorldPosition.y - objectAmbientCubeLowerHeight) / heightRange, 0.0, 1.0)
            : 0.0;
    return mix(lowerLighting, upperLighting, blend);
}

vec2 EnvironmentBrdfApprox(float roughness, float ndotv)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

float SampleShadowMap(int shadowSlot, vec2 uv)
{
    return shadowSlot == 0 ? texture(shadowMap0, uv).r : texture(shadowMap1, uv).r;
}

float DynamicSpotLightShadowVisibility(
        int shadowSlot,
        vec3 worldPosition,
        vec3 worldNormal,
        vec3 surfaceToLightDirection)
{
    if (shadowSlot < 0 || shadowSlot >= MAX_DYNAMIC_SHADOW_CASTERS) return 1.0;
    vec4 lightClip = shadowLightMatrices[shadowSlot] * vec4(worldPosition, 1.0);
    if (lightClip.w <= 0.0) return 1.0;
    vec3 shadowCoord = lightClip.xyz / lightClip.w * 0.5 + 0.5;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
            shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
            shadowCoord.z < 0.0 || shadowCoord.z > 1.0) return 1.0;

    float normalLightDot = max(dot(
            SafeNormalize(worldNormal, vec3(0.0, 1.0, 0.0)),
            SafeNormalize(surfaceToLightDirection, vec3(0.0, 1.0, 0.0))), 0.0);
    float effectiveBias = min(
            shadowBias[shadowSlot] * (1.0 + (1.0 - normalLightDot) * 2.0),
            0.02);
    float compareDepth = shadowCoord.z - effectiveBias;
    float softness = clamp(shadowSoftness[shadowSlot], 0.0, 8.0);
    if (softness <= 0.0) {
        return compareDepth <= SampleShadowMap(shadowSlot, shadowCoord.xy) ? 1.0 : 0.0;
    }

    vec2 radius = max(0.25, softness) / vec2(textureSize(shadowMap0, 0));
    float visible = 0.0;
    for (int i = 0; i < 12; ++i) {
        vec2 uv = clamp(shadowCoord.xy + kPoissonDisk[i] * radius, vec2(0.0), vec2(1.0));
        visible += compareDepth <= SampleShadowMap(shadowSlot, uv) ? 1.0 : 0.0;
    }
    return visible / 12.0;
}

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

void main()
{
    vec3 geometricNormal = SafeNormalize(fragWorldNormal, vec3(0.0, 1.0, 0.0));
    vec3 tangent = SafeNormalize(
            fragWorldTangent - geometricNormal * dot(fragWorldTangent, geometricNormal),
            SafeNormalize(cross(abs(geometricNormal.y) < 0.999
                    ? vec3(0.0, 1.0, 0.0)
                    : vec3(1.0, 0.0, 0.0), geometricNormal), vec3(1.0, 0.0, 0.0)));
    vec3 bitangent = SafeNormalize(cross(geometricNormal, tangent), vec3(0.0, 0.0, 1.0))
            * (fragTangentSign < 0.0 ? -1.0 : 1.0);
    vec3 worldNormal = geometricNormal;
    if (hasNormalTexture != 0) {
        vec3 mappedNormal = texture(normalTexture, fragTexCoord).xyz * 2.0 - 1.0;
        mappedNormal.xy *= normalScale;
        worldNormal = SafeNormalize(
                mat3(tangent, bitangent, geometricNormal) * mappedNormal,
                geometricNormal);
    }

    vec4 sampledBase = hasBaseColorTexture != 0
            ? texture(baseColorTexture, fragTexCoord)
            : vec4(1.0);
    vec3 albedo = DecodeColorTexture(
            sampledBase.rgb,
            baseColorTextureHardwareSrgb)
            * baseColorFactor.rgb
            * fragColor.rgb;
    float metallic = clamp(metallicFactor
            * (hasMetallicTexture != 0 ? texture(metallicTexture, fragTexCoord).r : 1.0),
            0.0, 1.0);
    float roughness = clamp(roughnessFactor
            * (hasRoughnessTexture != 0 ? texture(roughnessTexture, fragTexCoord).r : 1.0),
            0.045, 1.0);
    float materialAo = hasOcclusionTexture != 0
            ? mix(1.0, texture(occlusionTexture, fragTexCoord).r, occlusionStrength)
            : 1.0;
    vec3 viewDirection = SafeNormalize(cameraPosition - fragWorldPosition, geometricNormal);
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 directDiffuse = vec3(0.0);
    vec3 dynamicDirectSpecular = vec3(0.0);
    for (int i = 0; i < dynamicLightCount && i < MAX_DYNAMIC_LIGHTS; ++i) {
        float radius = dynamicLightRadii[i];
        vec3 toLight = dynamicLightPositions[i] - fragWorldPosition;
        float distanceSq = dot(toLight, toLight);
        if (radius > 0.0 && distanceSq < radius * radius) {
            float distanceToLight = sqrt(max(distanceSq, 0.0));
            vec3 lightDirection = distanceToLight > 0.0001
                    ? toLight / distanceToLight
                    : worldNormal;
            float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
            atten *= atten;
            float coneAtten = 1.0;
            if (dynamicLightTypes[i] == 1) {
                vec3 spotDirection = SafeNormalize(
                        dynamicLightDirections[i],
                        vec3(0.0, -1.0, 0.0));
                vec3 fragmentDirectionFromLight = distanceToLight > 0.0001
                        ? -lightDirection
                        : spotDirection;
                float coneDot = dot(spotDirection, fragmentDirectionFromLight);
                coneAtten = abs(dynamicLightInnerConeCos[i] - dynamicLightOuterConeCos[i]) > 0.0001
                        ? smoothstep(
                                dynamicLightOuterConeCos[i],
                                dynamicLightInnerConeCos[i],
                                coneDot)
                        : step(dynamicLightInnerConeCos[i], coneDot);
                int shadowSlot = dynamicLightShadowSlots[i];
                if (shadowSlot >= 0) {
                    float visibility = DynamicSpotLightShadowVisibility(
                            shadowSlot,
                            fragWorldPosition,
                            worldNormal,
                            lightDirection);
                    coneAtten *= mix(
                            1.0,
                            visibility,
                            clamp(shadowStrength[shadowSlot], 0.0, 1.0));
                }
            }
            float ndotl = max(dot(worldNormal, lightDirection), 0.0);
            if (ndotl > 0.0) {
                vec3 halfway = SafeNormalize(viewDirection + lightDirection, worldNormal);
                float distribution = DistributionGgx(worldNormal, halfway, roughness);
                float geometry = GeometrySmith(worldNormal, viewDirection, lightDirection, roughness);
                vec3 fresnel = FresnelSchlick(max(dot(halfway, viewDirection), 0.0), f0);
                vec3 specular = distribution * geometry * fresnel
                        / max(4.0 * max(dot(worldNormal, viewDirection), 0.0) * ndotl, 0.001);
                vec3 diffuseWeight = (vec3(1.0) - fresnel) * (1.0 - metallic);
                vec3 radiance = dynamicLightColors[i]
                        * dynamicLightIntensities[i]
                        * atten
                        * coneAtten;
                directDiffuse += diffuseWeight * albedo * radiance * ndotl;
                dynamicDirectSpecular += specular * radiance * ndotl;
            }
        }
    }

    vec3 staticDirectSpecular = vec3(0.0);
    if (useStaticSpecularLighting != 0) {
        for (int i = 0;
                i < staticSpecularLightCount
                        && i < MAX_STATIC_SPECULAR_LIGHTS;
                ++i) {
            float radius = staticSpecularLightRadii[i];
            vec3 toLight = staticSpecularLightPositions[i] - fragWorldPosition;
            float distanceSq = dot(toLight, toLight);
            if (radius > 0.0 && distanceSq < radius * radius) {
                float distanceToLight = sqrt(max(distanceSq, 0.0));
                vec3 lightDirection = distanceToLight > 0.0001
                        ? toLight / distanceToLight
                        : worldNormal;
                float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
                atten *= atten;
                float coneAtten = 1.0;
                if (staticSpecularLightTypes[i] == 1) {
                    vec3 spotDirection = SafeNormalize(
                            staticSpecularLightDirections[i],
                            vec3(0.0, -1.0, 0.0));
                    vec3 fragmentDirectionFromLight = distanceToLight > 0.0001
                            ? -lightDirection
                            : spotDirection;
                    float coneDot = dot(
                            spotDirection,
                            fragmentDirectionFromLight);
                    coneAtten = abs(
                            staticSpecularLightInnerConeCos[i]
                                    - staticSpecularLightOuterConeCos[i]) > 0.0001
                            ? smoothstep(
                                    staticSpecularLightOuterConeCos[i],
                                    staticSpecularLightInnerConeCos[i],
                                    coneDot)
                            : step(
                                    staticSpecularLightInnerConeCos[i],
                                    coneDot);
                }
                float ndotl = max(dot(worldNormal, lightDirection), 0.0);
                if (ndotl > 0.0 && coneAtten > 0.0) {
                    vec3 halfway = SafeNormalize(
                            viewDirection + lightDirection,
                            worldNormal);
                    float distribution = DistributionGgx(
                            worldNormal, halfway, roughness);
                    float geometry = GeometrySmith(
                            worldNormal,
                            viewDirection,
                            lightDirection,
                            roughness);
                    vec3 fresnel = FresnelSchlick(
                            max(dot(halfway, viewDirection), 0.0), f0);
                    vec3 specular = distribution * geometry * fresnel
                            / max(4.0
                                    * max(dot(worldNormal, viewDirection), 0.0)
                                    * ndotl,
                                    0.001);
                    vec3 radiance = staticSpecularLightColors[i]
                            * staticSpecularLightIntensities[i]
                            * atten
                            * coneAtten;
                    staticDirectSpecular += specular * radiance * ndotl;
                }
            }
        }
    }

    vec4 bakedStaticSample = hasStaticLightmap != 0
            ? texture(lightmapTexture, fragLightmapTexCoord)
            : vec4(0.0, 0.0, 0.0, 1.0);
    vec3 staticLighting = containingSectorAmbient;
    vec3 staticAtmosphericLighting = containingSectorAmbient;
    if (useObjectProbeLighting != 0) {
        staticLighting = EvaluateObjectAmbientCube(worldNormal);
        staticAtmosphericLighting = EvaluateFogObjectProbeLighting();
    } else if (hasStaticLightmap != 0) {
        float ao = useBakedAmbientOcclusion != 0 ? bakedStaticSample.a : 1.0;
        staticLighting = containingSectorAmbient * ao + bakedStaticSample.rgb;
        staticAtmosphericLighting = containingSectorAmbient + bakedStaticSample.rgb;
    }
    // Probe, fallback ambient, and lightmap RGB are incoming diffuse
    // illumination. They never enter final radiance without the material's
    // diffuse base-color, non-metal, AO, and renderer scale.
    vec3 indirectDiffuse = albedo
            * (1.0 - metallic)
            * staticLighting
            * materialAo
            * indirectDiffuseScale;
    vec3 environmentSpecular = vec3(0.0);
    if (hasEnvironment != 0) {
        vec3 reflected = reflect(-viewDirection, worldNormal);
        vec3 environment = textureLod(
                environmentTexture,
                reflected,
                roughness * 8.0).rgb;
        vec2 environmentBrdf = EnvironmentBrdfApprox(
                roughness,
                max(dot(worldNormal, viewDirection), 0.0));
        environmentSpecular = environment
                * (f0 * environmentBrdf.x + environmentBrdf.y)
                * environmentExposure
                * environmentSpecularScale
                * materialAo;
    }
    vec3 emissive = emissiveFactor;
    if (hasEmissiveTexture != 0) {
        emissive *= DecodeColorTexture(
                texture(emissiveTexture, fragTexCoord).rgb,
                emissiveTextureHardwareSrgb);
    }
    emissive *= max(emissiveStrength, 0.0);
    vec3 linearColor = indirectDiffuse
            + directDiffuse
            + dynamicDirectSpecular
            + staticDirectSpecular
            + environmentSpecular
            + emissive;
    if (pbrDiagnosticMode == 1) linearColor = albedo;
    else if (pbrDiagnosticMode == 2) linearColor = directDiffuse;
    else if (pbrDiagnosticMode == 3) {
        linearColor = dynamicDirectSpecular + staticDirectSpecular;
    }
    else if (pbrDiagnosticMode == 4) linearColor = indirectDiffuse;
    else if (pbrDiagnosticMode == 5) linearColor = environmentSpecular;
    else if (pbrDiagnosticMode == 6) linearColor = emissive;
    else if (pbrDiagnosticMode == 7) linearColor = vec3(materialAo);
    else if (pbrDiagnosticMode == 8) linearColor = vec3(metallic, roughness, 0.0);
    else if (pbrDiagnosticMode == 9) linearColor = worldNormal * 0.5 + 0.5;

    // Artistic/display ceilings are forbidden. The final write below applies
    // only the unavoidable finite RGBA16F storage boundary.
    linearColor = max(linearColor, vec3(0.0));
    if (pbrDiagnosticMode == 0) {
        linearColor *= outputBrightnessMultiplier;
        linearColor = ApplySectorFog(
                linearColor,
                staticAtmosphericLighting,
                fragWorldPosition);
    }
    linearColor.r = isnan(linearColor.r) ? 0.0 : (isinf(linearColor.r) ? (linearColor.r > 0.0 ? 65504.0 : 0.0) : min(max(linearColor.r, 0.0), 65504.0));
    linearColor.g = isnan(linearColor.g) ? 0.0 : (isinf(linearColor.g) ? (linearColor.g > 0.0 ? 65504.0 : 0.0) : min(max(linearColor.g, 0.0), 65504.0));
    linearColor.b = isnan(linearColor.b) ? 0.0 : (isinf(linearColor.b) ? (linearColor.b > 0.0 ? 65504.0 : 0.0) : min(max(linearColor.b, 0.0), 65504.0));
    finalColor = vec4(
            linearColor,
            clamp(modelOpacity, 0.0, 1.0));
}
)";

int GetShaderLocationArrayBase(Shader shader, const char* name)
{
    const int location = GetShaderLocation(shader, name);
    if (location >= 0) {
        return location;
    }
    const std::string indexedName = std::string(name) + "[0]";
    return GetShaderLocation(shader, indexedName.c_str());
}

int GetShaderLocationArrayElement(Shader shader, const char* name, size_t index)
{
    const std::string indexedName = std::string(name)
            + "[" + std::to_string(index) + "]";
    return GetShaderLocation(shader, indexedName.c_str());
}

void SetShaderSamplerUnit(Shader shader, int location, int textureUnit)
{
    if (location >= 0) {
        SetShaderValue(
                shader,
                location,
                &textureUnit,
                SHADER_UNIFORM_INT);
    }
}

void InitializeSectorPbrSamplerUnits(
        Shader shader,
        int lightmapTextureLocation,
        int environmentTextureLocation,
        int shadowMap0Location,
        int shadowMap1Location)
{
    constexpr std::array<int, 6> materialTextureUnits{
            MATERIAL_MAP_ALBEDO,
            MATERIAL_MAP_METALNESS,
            MATERIAL_MAP_NORMAL,
            MATERIAL_MAP_ROUGHNESS,
            MATERIAL_MAP_OCCLUSION,
            MATERIAL_MAP_EMISSION};
    for (int textureUnit : materialTextureUnits) {
        SetShaderSamplerUnit(
                shader,
                shader.locs[SHADER_LOC_MAP_DIFFUSE + textureUnit],
                textureUnit);
    }
    SetShaderSamplerUnit(
            shader,
            lightmapTextureLocation,
            SectorStaticModelLightmapMaterialMap);
    // A missing environment leaves this sampler active in the linked shader.
    // Keep its cubemap unit distinct from the default sampler2D unit even when
    // DrawMesh() has no cubemap texture to bind for the current map.
    SetShaderSamplerUnit(
            shader,
            environmentTextureLocation,
            SectorStaticModelEnvironmentMaterialMap);
    SetShaderSamplerUnit(
            shader,
            shadowMap0Location,
            SectorStaticModelShadowMap0MaterialMap);
    SetShaderSamplerUnit(
            shader,
            shadowMap1Location,
            SectorStaticModelShadowMap1MaterialMap);
    rlDisableShader();
}

void AppendStaticModelDebugText(
        std::string& renderDebugText,
        size_t drawn,
        size_t considered,
        size_t portalCulled,
        size_t skipped)
{
    const size_t existing = renderDebugText.find(" | props:");
    if (existing != std::string::npos) {
        renderDebugText.erase(existing);
    }
    if (!renderDebugText.empty()) {
        renderDebugText += " | props: "
                + std::to_string(drawn)
                + " drawn / "
                + std::to_string(considered)
                + " considered, "
                + std::to_string(portalCulled)
                + " portal culled, "
                + std::to_string(skipped)
                + " skipped";
    }
}

Vector4 ColorToNormalizedVector4(Color color)
{
    constexpr float scale = 1.0f / 255.0f;
    return Vector4{
            static_cast<float>(color.r) * scale,
            static_cast<float>(color.g) * scale,
            static_cast<float>(color.b) * scale,
            static_cast<float>(color.a) * scale};
}

const SectorStaticModelLightmapObject* FindLightmapObject(
        const SectorStaticModelLightmapData& data,
        int objectId)
{
    const auto found = std::lower_bound(
            data.objects.begin(),
            data.objects.end(),
            objectId,
            [](const SectorStaticModelLightmapObject& object, int id) {
                return object.objectId < id;
            });
    return found != data.objects.end() && found->objectId == objectId
            ? &*found
            : nullptr;
}

bool BuildRemappedMesh(
        const Mesh& source,
        const SectorStaticModelLightmapMesh& remap,
        Mesh& outMesh)
{
    outMesh = {};
    if (source.vertexCount != remap.originalVertexCount
            || source.vertices == nullptr
            || remap.indices.empty()
            || remap.indices.size()
                    > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    outMesh.vertexCount = static_cast<int>(remap.indices.size());
    outMesh.triangleCount = outMesh.vertexCount / 3;
    outMesh.vertices = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 3 * sizeof(float))));
    outMesh.normals = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 3 * sizeof(float))));
    outMesh.texcoords = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 2 * sizeof(float))));
    outMesh.texcoords2 = static_cast<float*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 2 * sizeof(float))));
    outMesh.colors = static_cast<unsigned char*>(
            MemAlloc(static_cast<unsigned int>(
                    static_cast<size_t>(outMesh.vertexCount)
                    * 4 * sizeof(unsigned char))));
    if (source.tangents != nullptr) {
        outMesh.tangents = static_cast<float*>(
                MemAlloc(static_cast<unsigned int>(
                        static_cast<size_t>(outMesh.vertexCount)
                        * 4 * sizeof(float))));
    }
    if (outMesh.vertices == nullptr
            || outMesh.normals == nullptr
            || outMesh.texcoords == nullptr
            || outMesh.texcoords2 == nullptr
            || outMesh.colors == nullptr
            || (source.tangents != nullptr && outMesh.tangents == nullptr)) {
        UnloadMesh(outMesh);
        outMesh = {};
        return false;
    }

    for (int vertexIndex = 0;
            vertexIndex < outMesh.vertexCount;
            ++vertexIndex) {
        const uint32_t remapIndex =
                remap.indices[static_cast<size_t>(vertexIndex)];
        if (remapIndex >= remap.sourceVertexIndices.size()
                || remapIndex >= remap.localLightmapUvs.size()) {
            UnloadMesh(outMesh);
            outMesh = {};
            return false;
        }
        const uint32_t sourceIndex =
                remap.sourceVertexIndices[remapIndex];
        if (sourceIndex >= static_cast<uint32_t>(source.vertexCount)) {
            UnloadMesh(outMesh);
            outMesh = {};
            return false;
        }
        std::copy_n(
                source.vertices + sourceIndex * 3,
                3,
                outMesh.vertices + vertexIndex * 3);
        if (source.normals != nullptr) {
            std::copy_n(
                    source.normals + sourceIndex * 3,
                    3,
                    outMesh.normals + vertexIndex * 3);
        } else {
            std::fill_n(outMesh.normals + vertexIndex * 3, 3, 0.0f);
        }
        if (source.texcoords != nullptr) {
            std::copy_n(
                    source.texcoords + sourceIndex * 2,
                    2,
                    outMesh.texcoords + vertexIndex * 2);
        } else {
            std::fill_n(outMesh.texcoords + vertexIndex * 2, 2, 0.0f);
        }
        const Vector2 lightmapUv =
                remap.localLightmapUvs[remapIndex];
        outMesh.texcoords2[vertexIndex * 2] = lightmapUv.x;
        outMesh.texcoords2[vertexIndex * 2 + 1] = lightmapUv.y;
        if (source.colors != nullptr) {
            std::copy_n(
                    source.colors + sourceIndex * 4,
                    4,
                    outMesh.colors + vertexIndex * 4);
        } else {
            std::fill_n(
                    outMesh.colors + vertexIndex * 4,
                    4,
                    static_cast<unsigned char>(255));
        }
        if (source.tangents != nullptr) {
            std::copy_n(
                    source.tangents + sourceIndex * 4,
                    4,
                    outMesh.tangents + vertexIndex * 4);
        }
    }
    for (int triangle = 0;
            triangle < outMesh.triangleCount;
            ++triangle) {
        float* normal0 = outMesh.normals + (triangle * 3) * 3;
        float* normal1 = outMesh.normals + (triangle * 3 + 1) * 3;
        float* normal2 = outMesh.normals + (triangle * 3 + 2) * 3;
        const Vector3 n0{normal0[0], normal0[1], normal0[2]};
        const Vector3 n1{normal1[0], normal1[1], normal1[2]};
        const Vector3 n2{normal2[0], normal2[1], normal2[2]};
        if (Vector3LengthSqr(n0) > 0.0000001f
                && Vector3LengthSqr(n1) > 0.0000001f
                && Vector3LengthSqr(n2) > 0.0000001f) {
            continue;
        }
        const float* vertex0 =
                outMesh.vertices + (triangle * 3) * 3;
        const float* vertex1 =
                outMesh.vertices + (triangle * 3 + 1) * 3;
        const float* vertex2 =
                outMesh.vertices + (triangle * 3 + 2) * 3;
        const Vector3 faceNormal = Vector3Normalize(Vector3CrossProduct(
                Vector3Subtract(
                        Vector3{vertex1[0], vertex1[1], vertex1[2]},
                        Vector3{vertex0[0], vertex0[1], vertex0[2]}),
                Vector3Subtract(
                        Vector3{vertex2[0], vertex2[1], vertex2[2]},
                        Vector3{vertex0[0], vertex0[1], vertex0[2]})));
        for (int corner = 0; corner < 3; ++corner) {
            float* normal =
                    outMesh.normals + (triangle * 3 + corner) * 3;
            normal[0] = faceNormal.x;
            normal[1] = faceNormal.y;
            normal[2] = faceNormal.z;
        }
    }
    UploadMesh(&outMesh, false);
    return outMesh.vaoId != 0;
}

} // namespace

const char* SectorPbrDiagnosticModeName(SectorPbrDiagnosticMode mode)
{
    switch (mode) {
        case SectorPbrDiagnosticMode::Full: return "Full PBR";
        case SectorPbrDiagnosticMode::BaseColor: return "Base Color";
        case SectorPbrDiagnosticMode::DirectDiffuse: return "Direct Diffuse";
        case SectorPbrDiagnosticMode::DirectSpecular: return "Direct Specular";
        case SectorPbrDiagnosticMode::IndirectDiffuse: return "Probe / Indirect Diffuse";
        case SectorPbrDiagnosticMode::EnvironmentSpecular: return "Environment Specular";
        case SectorPbrDiagnosticMode::Emissive: return "Emissive";
        case SectorPbrDiagnosticMode::MaterialOcclusion: return "Material AO";
        case SectorPbrDiagnosticMode::MetallicRoughness: return "Metallic / Roughness";
        case SectorPbrDiagnosticMode::ShadingNormal: return "Shading Normal";
        case SectorPbrDiagnosticMode::Count: break;
    }
    return "Full PBR";
}

const char* SectorPbrLightingPathName(SectorPbrLightingPath path)
{
    switch (path) {
        case SectorPbrLightingPath::WorldStatic: return "world static model";
        case SectorPbrLightingPath::WorldDynamic: return "world dynamic model";
        case SectorPbrLightingPath::Viewmodel: return "viewmodel arms";
        case SectorPbrLightingPath::ViewmodelAttachment: return "viewmodel attachment";
    }
    return "unknown";
}

const char* SectorPbrIndirectSourceName(SectorPbrIndirectSource source)
{
    switch (source) {
        case SectorPbrIndirectSource::SectorAmbient: return "sector ambient fallback";
        case SectorPbrIndirectSource::ObjectProbe: return "object probe";
        case SectorPbrIndirectSource::StaticLightmap: return "static model lightmap";
    }
    return "unknown";
}

bool SectorStaticModelRenderer::Load()
{
    shader = LoadShaderFromMemory(SectorStaticModelVs, SectorStaticModelFs);
    if (shader.id == 0) {
        shader = {};
        shaderLoaded = false;
        return false;
    }

    shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] =
            GetShaderLocationAttrib(shader, "vertexNormal");
    shader.locs[SHADER_LOC_VERTEX_TANGENT] =
            GetShaderLocationAttrib(shader, "vertexTangent");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD02] =
            GetShaderLocationAttrib(shader, "vertexTexCoord2");
    shader.locs[SHADER_LOC_VERTEX_COLOR] =
            GetShaderLocationAttrib(shader, "vertexColor");
    shader.locs[SHADER_LOC_VERTEX_BONEIDS] =
            GetShaderLocationAttrib(shader, "vertexBoneIndices");
    shader.locs[SHADER_LOC_VERTEX_BONEWEIGHTS] =
            GetShaderLocationAttrib(shader, "vertexBoneWeights");
    shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] =
            GetShaderLocation(shader, "boneMatrices");
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "baseColorTexture");
    shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(shader, "metallicTexture");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "normalTexture");
    shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(shader, "roughnessTexture");
    shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(shader, "occlusionTexture");
    shader.locs[SHADER_LOC_MAP_EMISSION] = GetShaderLocation(shader, "emissiveTexture");

    baseColorFactorLoc = GetShaderLocation(shader, "baseColorFactor");
    emissiveFactorLoc = GetShaderLocation(shader, "emissiveFactor");
    emissiveStrengthLoc = GetShaderLocation(shader, "emissiveStrength");
    metallicFactorLoc = GetShaderLocation(shader, "metallicFactor");
    roughnessFactorLoc = GetShaderLocation(shader, "roughnessFactor");
    normalScaleLoc = GetShaderLocation(shader, "normalScale");
    occlusionStrengthLoc = GetShaderLocation(shader, "occlusionStrength");
    modelOpacityLoc = GetShaderLocation(shader, "modelOpacity");
    hasBaseColorTextureLoc = GetShaderLocation(shader, "hasBaseColorTexture");
    hasMetallicTextureLoc = GetShaderLocation(shader, "hasMetallicTexture");
    hasNormalTextureLoc = GetShaderLocation(shader, "hasNormalTexture");
    hasRoughnessTextureLoc = GetShaderLocation(shader, "hasRoughnessTexture");
    hasOcclusionTextureLoc = GetShaderLocation(shader, "hasOcclusionTexture");
    hasEmissiveTextureLoc = GetShaderLocation(shader, "hasEmissiveTexture");
    baseColorHardwareSrgbLoc = GetShaderLocation(
            shader, "baseColorTextureHardwareSrgb");
    emissiveHardwareSrgbLoc = GetShaderLocation(
            shader, "emissiveTextureHardwareSrgb");
    diagnosticModeLoc = GetShaderLocation(shader, "pbrDiagnosticMode");
    indirectDiffuseScaleLoc = GetShaderLocation(
            shader, "indirectDiffuseScale");
    environmentSpecularScaleLoc = GetShaderLocation(
            shader, "environmentSpecularScale");
    useStaticSpecularLightingLoc = GetShaderLocation(
            shader, "useStaticSpecularLighting");
    cameraPositionLoc = GetShaderLocation(shader, "cameraPosition");
    environmentExposureLoc = GetShaderLocation(shader, "environmentExposure");
    outputBrightnessMultiplierLoc =
            GetShaderLocation(shader, "outputBrightnessMultiplier");
    hasEnvironmentLoc = GetShaderLocation(shader, "hasEnvironment");
    environmentTextureLoc = GetShaderLocation(shader, "environmentTexture");
    lightmapScaleBiasLoc =
            GetShaderLocation(shader, "lightmapScaleBias");
    hasStaticLightmapLoc =
            GetShaderLocation(shader, "hasStaticLightmap");
    useBakedAmbientOcclusionLoc =
            GetShaderLocation(shader, "useBakedAmbientOcclusion");
    containingSectorAmbientLoc =
            GetShaderLocation(shader, "containingSectorAmbient");
    useObjectProbeLightingLoc =
            GetShaderLocation(shader, "useObjectProbeLighting");
    for (size_t i = 0; i < objectAmbientCubeLocs.size(); ++i) {
        objectAmbientCubeLocs[i] = GetShaderLocationArrayElement(
                shader, "objectAmbientCube", i);
        objectAmbientCubeUpperLocs[i] = GetShaderLocationArrayElement(
                shader, "objectAmbientCubeUpper", i);
    }
    objectAmbientCubeLowerHeightLoc =
            GetShaderLocation(shader, "objectAmbientCubeLowerHeight");
    objectAmbientCubeUpperHeightLoc =
            GetShaderLocation(shader, "objectAmbientCubeUpperHeight");
    useVerticalObjectProbeLightingLoc =
            GetShaderLocation(shader, "useVerticalObjectProbeLighting");
    useSkinningLoc = GetShaderLocation(shader, "useSkinning");
    lightmapTextureLoc =
            GetShaderLocation(shader, "lightmapTexture");
    shader.locs[
            SHADER_LOC_MAP_DIFFUSE
                    + SectorStaticModelLightmapMaterialMap] =
            lightmapTextureLoc;
    shader.locs[
            SHADER_LOC_MAP_DIFFUSE
                    + SectorStaticModelEnvironmentMaterialMap] =
            environmentTextureLoc;
    dynamicLightCountLoc = GetShaderLocation(shader, "dynamicLightCount");
    dynamicLightPositionsLoc = GetShaderLocationArrayBase(shader, "dynamicLightPositions");
    dynamicLightColorsLoc = GetShaderLocationArrayBase(shader, "dynamicLightColors");
    dynamicLightRadiiLoc = GetShaderLocationArrayBase(shader, "dynamicLightRadii");
    dynamicLightIntensitiesLoc = GetShaderLocationArrayBase(shader, "dynamicLightIntensities");
    dynamicLightTypesLoc = GetShaderLocationArrayBase(shader, "dynamicLightTypes");
    dynamicLightDirectionsLoc = GetShaderLocationArrayBase(shader, "dynamicLightDirections");
    dynamicLightInnerConeCosLoc = GetShaderLocationArrayBase(shader, "dynamicLightInnerConeCos");
    dynamicLightOuterConeCosLoc = GetShaderLocationArrayBase(shader, "dynamicLightOuterConeCos");
    staticSpecularLocations = GetSectorStaticSpecularShaderLocations(shader);
    dynamicLightShadowSlotsLoc = GetShaderLocationArrayBase(shader, "dynamicLightShadowSlots");
    for (size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        shadowLightMatrixLocs[i] =
                GetShaderLocationArrayElement(shader, "shadowLightMatrices", i);
    }
    shadowBiasLoc = GetShaderLocationArrayBase(shader, "shadowBias");
    shadowStrengthLoc = GetShaderLocationArrayBase(shader, "shadowStrength");
    shadowSoftnessLoc = GetShaderLocationArrayBase(shader, "shadowSoftness");
    shadowMap0Loc = GetShaderLocation(shader, "shadowMap0");
    shadowMap1Loc = GetShaderLocation(shader, "shadowMap1");
    shader.locs[
            SHADER_LOC_MAP_DIFFUSE
                    + SectorStaticModelShadowMap0MaterialMap] =
            shadowMap0Loc;
    shader.locs[
            SHADER_LOC_MAP_DIFFUSE
                    + SectorStaticModelShadowMap1MaterialMap] =
            shadowMap1Loc;
    InitializeSectorPbrSamplerUnits(
            shader,
            lightmapTextureLoc,
            environmentTextureLoc,
            shadowMap0Loc,
            shadowMap1Loc);
    fogShaderLocations = GetSectorFogShaderLocations(shader);
    shaderLoaded = true;
    return true;
}

void SectorStaticModelRenderer::Shutdown()
{
    ClearCachedModels();
    shadowCasterCollection = {};
    lightmapData = {};
    if (shaderLoaded) {
        UnloadShader(shader);
    }
    shader = {};
    baseColorFactorLoc = -1;
    emissiveFactorLoc = -1;
    emissiveStrengthLoc = -1;
    metallicFactorLoc = -1;
    roughnessFactorLoc = -1;
    normalScaleLoc = -1;
    occlusionStrengthLoc = -1;
    modelOpacityLoc = -1;
    hasBaseColorTextureLoc = -1;
    hasMetallicTextureLoc = -1;
    hasNormalTextureLoc = -1;
    hasRoughnessTextureLoc = -1;
    hasOcclusionTextureLoc = -1;
    hasEmissiveTextureLoc = -1;
    baseColorHardwareSrgbLoc = -1;
    emissiveHardwareSrgbLoc = -1;
    diagnosticModeLoc = -1;
    indirectDiffuseScaleLoc = -1;
    environmentSpecularScaleLoc = -1;
    useStaticSpecularLightingLoc = -1;
    cameraPositionLoc = -1;
    environmentExposureLoc = -1;
    outputBrightnessMultiplierLoc = -1;
    hasEnvironmentLoc = -1;
    environmentTextureLoc = -1;
    lightmapScaleBiasLoc = -1;
    hasStaticLightmapLoc = -1;
    useBakedAmbientOcclusionLoc = -1;
    containingSectorAmbientLoc = -1;
    useObjectProbeLightingLoc = -1;
    objectAmbientCubeLocs.fill(-1);
    objectAmbientCubeUpperLocs.fill(-1);
    objectAmbientCubeLowerHeightLoc = -1;
    objectAmbientCubeUpperHeightLoc = -1;
    useVerticalObjectProbeLightingLoc = -1;
    useSkinningLoc = -1;
    lightmapTextureLoc = -1;
    dynamicLightCountLoc = -1;
    dynamicLightPositionsLoc = -1;
    dynamicLightColorsLoc = -1;
    dynamicLightRadiiLoc = -1;
    dynamicLightIntensitiesLoc = -1;
    dynamicLightTypesLoc = -1;
    dynamicLightDirectionsLoc = -1;
    dynamicLightInnerConeCosLoc = -1;
    dynamicLightOuterConeCosLoc = -1;
    staticSpecularLocations = SectorStaticSpecularShaderLocations{};
    dynamicLightShadowSlotsLoc = -1;
    shadowLightMatrixLocs.fill(-1);
    shadowBiasLoc = -1;
    shadowStrengthLoc = -1;
    shadowSoftnessLoc = -1;
    shadowMap0Loc = -1;
    shadowMap1Loc = -1;
    fogShaderLocations = SectorFogShaderLocations{};
    shaderLoaded = false;
    warningPrinted = false;
}

void SectorStaticModelRenderer::ResetDebugState()
{
    warningPrinted = false;
    worldDiagnostics = {};
    viewmodelDiagnostics = {};
}

void SectorStaticModelRenderer::UploadPbrDrawState(
        const SectorPbrDrawState& state)
{
    const int diagnosticMode = static_cast<int>(state.diagnosticMode);
    const int useObjectProbe = state.useObjectProbe ? 1 : 0;
    const int useVerticalProbe = state.useVerticalObjectProbe ? 1 : 0;
    const int hasEnvironment = state.environmentActive ? 1 : 0;
    const int useStaticSpecular = state.staticSpecularEligible ? 1 : 0;
    if (diagnosticModeLoc >= 0) SetShaderValue(shader, diagnosticModeLoc, &diagnosticMode, SHADER_UNIFORM_INT);
    if (indirectDiffuseScaleLoc >= 0) SetShaderValue(shader, indirectDiffuseScaleLoc, &state.indirectDiffuseScale, SHADER_UNIFORM_FLOAT);
    if (environmentSpecularScaleLoc >= 0) SetShaderValue(shader, environmentSpecularScaleLoc, &state.environmentSpecularScale, SHADER_UNIFORM_FLOAT);
    if (useStaticSpecularLightingLoc >= 0) SetShaderValue(shader, useStaticSpecularLightingLoc, &useStaticSpecular, SHADER_UNIFORM_INT);
    if (environmentExposureLoc >= 0) SetShaderValue(shader, environmentExposureLoc, &state.environmentExposure, SHADER_UNIFORM_FLOAT);
    if (outputBrightnessMultiplierLoc >= 0) SetShaderValue(shader, outputBrightnessMultiplierLoc, &state.outputBrightnessMultiplier, SHADER_UNIFORM_FLOAT);
    if (hasEnvironmentLoc >= 0) SetShaderValue(shader, hasEnvironmentLoc, &hasEnvironment, SHADER_UNIFORM_INT);
    if (useObjectProbeLightingLoc >= 0) SetShaderValue(shader, useObjectProbeLightingLoc, &useObjectProbe, SHADER_UNIFORM_INT);
    if (useVerticalObjectProbeLightingLoc >= 0) SetShaderValue(shader, useVerticalObjectProbeLightingLoc, &useVerticalProbe, SHADER_UNIFORM_INT);
}

void SectorStaticModelRenderer::UploadPbrMaterialTransferState(
        const engine::ModelMaterialAsset& material)
{
    const int baseHardwareSrgb = material.textureInfo[static_cast<size_t>(
            engine::ModelMaterialTextureRole::BaseColor)].hardwareSrgbDecode ? 1 : 0;
    const int emissiveHardwareSrgb = material.textureInfo[static_cast<size_t>(
            engine::ModelMaterialTextureRole::Emissive)].hardwareSrgbDecode ? 1 : 0;
    if (baseColorHardwareSrgbLoc >= 0) SetShaderValue(shader, baseColorHardwareSrgbLoc, &baseHardwareSrgb, SHADER_UNIFORM_INT);
    if (emissiveHardwareSrgbLoc >= 0) SetShaderValue(shader, emissiveHardwareSrgbLoc, &emissiveHardwareSrgb, SHADER_UNIFORM_INT);
}

void SectorStaticModelRenderer::RecordPbrDiagnostics(
        SectorPbrDrawDiagnostics& diagnostics,
        int placedObjectId,
        engine::ModelHandle model,
        int materialIndex,
        const SectorPbrDrawState& state,
        const engine::ModelMaterialAsset& material,
        const SectorStaticSpecularLightContext& staticSpecularLights)
{
    diagnostics.valid = true;
    diagnostics.placedObjectId = placedObjectId;
    diagnostics.model = model;
    diagnostics.materialIndex = materialIndex;
    diagnostics.state = state;
    diagnostics.material = material;
    diagnostics.staticSpecularLights = staticSpecularLights;
}

void SectorStaticModelRenderer::SetLightmapData(
        SectorStaticModelLightmapData data)
{
    ClearCachedModels();
    lightmapData = std::move(data);
    std::sort(
            lightmapData.objects.begin(),
            lightmapData.objects.end(),
            [](const auto& left, const auto& right) {
                return left.objectId < right.objectId;
            });
    cachedModels.reserve(lightmapData.models.size());
}

void SectorStaticModelRenderer::ClearCachedModels()
{
    for (CachedModel& cached : cachedModels) {
        for (Mesh& mesh : cached.meshes) {
            if (mesh.vaoId != 0
                    || mesh.vertices != nullptr
                    || mesh.vboId != nullptr) {
                UnloadMesh(mesh);
            }
            mesh = {};
        }
    }
    cachedModels.clear();
}

const SectorStaticModelRenderer::CachedModel*
SectorStaticModelRenderer::FindCachedModel(
        engine::ModelHandle handle,
        int lightmapModelIndex) const
{
    const auto found = std::find_if(
            cachedModels.begin(),
            cachedModels.end(),
            [handle, lightmapModelIndex](const CachedModel& cached) {
                return cached.handle == handle
                        && cached.lightmapModelIndex == lightmapModelIndex;
            });
    return found == cachedModels.end() ? nullptr : &*found;
}

void SectorStaticModelRenderer::FinalizeResources(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld)
{
    engine::PrepareAnimatedModelInstancesSystem(runtimeObjectWorld, assets);
    if (lightmapData.objects.empty()) {
        return;
    }
    runtimeObjectWorld.ForEach<
            SectorObject,
            SectorStaticModel>(
            [this, &assets](
                    engine::Entity,
                    SectorObject&,
                    SectorStaticModel& staticModel) {
                const SectorStaticModelLightmapObject* object =
                        FindLightmapObject(
                                lightmapData,
                                staticModel.placedObjectId);
                if (object == nullptr
                        || object->modelIndex < 0
                        || object->modelIndex
                                >= static_cast<int>(lightmapData.models.size())
                        || FindCachedModel(
                                   staticModel.model,
                                   object->modelIndex)
                                != nullptr) {
                    return;
                }
                const Model* source = assets.GetModel(staticModel.model);
                if (source == nullptr) {
                    return;
                }
                const auto& lightmapModel =
                        lightmapData.models[
                                static_cast<size_t>(object->modelIndex)];
                CachedModel cached;
                cached.handle = staticModel.model;
                cached.lightmapModelIndex = object->modelIndex;
                cached.meshes.resize(lightmapModel.meshes.size());
                bool valid = source->meshCount
                        == static_cast<int>(lightmapModel.meshes.size());
                for (size_t meshIndex = 0;
                        valid && meshIndex < lightmapModel.meshes.size();
                        ++meshIndex) {
                    valid = BuildRemappedMesh(
                            source->meshes[meshIndex],
                            lightmapModel.meshes[meshIndex],
                            cached.meshes[meshIndex]);
                }
                if (!valid) {
                    for (Mesh& mesh : cached.meshes) {
                        if (mesh.vaoId != 0
                                || mesh.vertices != nullptr
                                || mesh.vboId != nullptr) {
                            UnloadMesh(mesh);
                        }
                        mesh = {};
                    }
                    cached.meshes.clear();
                    if (!warningPrinted) {
                        std::fprintf(
                                stderr,
                                "[SectorMeshRenderer WARNING] Static prop lightmap mesh remap could not be finalized; using sector ambient fallback\n");
                        warningPrinted = true;
                    }
                }
                cachedModels.push_back(std::move(cached));
            });
}

void SectorStaticModelRenderer::ReserveShadowCasterCapacity(size_t capacity)
{
    ReserveSectorStaticModelShadowCasters(
            shadowCasterCollection,
            capacity);
}

void SectorStaticModelRenderer::PrepareShadowRenderContext(
        SectorDynamicSpotLightShadowRenderContext& context,
        engine::World* runtimeObjectWorld)
{
    UpdateSectorStaticModelShadowCasters(
            shadowCasterCollection,
            runtimeObjectWorld);
    context.staticModelShadowCasters = &shadowCasterCollection.casters;
    context.staticModelShadowCasterRevision =
            shadowCasterCollection.revision;
}

void SectorStaticModelRenderer::ClearPreparedShadowCasters()
{
    ClearSectorStaticModelShadowCasters(shadowCasterCollection);
}

bool SectorStaticModelRenderer::DrawWorldDynamicModel(
        const engine::ModelAsset& modelAsset,
        const Model& model,
        engine::ModelHandle modelHandle,
        Matrix modelTransform,
        int placedObjectId,
        int receiverSectorId,
        const SectorReceiverBounds& receiverBounds,
        Vector3 containingSectorAmbient,
        float environmentExposure,
        const BakedObjectLightingVerticalSample& lighting,
        const SectorBillboardDynamicLightContext& dynamicLightContext,
        const SectorStaticSpecularLightState& staticSpecularLights,
        const RuntimePortalVisibilityResult& visibility,
        bool objectProbeBakeCurrent,
        const TextureCubemap* environment,
        bool allowSkinning,
        float opacity)
{
    const bool canSkin = allowSkinning
            && model.skeleton.boneCount > 0
            && model.skeleton.boneCount <= engine::MaxAnimatedModelBones
            && model.boneMatrices != nullptr
            && shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] >= 0;
    const int useSkinning = canSkin ? 1 : 0;
    const int noStaticLightmap = 0;
    const int noBakedAo = 0;
    opacity = std::isfinite(opacity) ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
    if (modelOpacityLoc >= 0) {
        SetShaderValue(shader, modelOpacityLoc, &opacity, SHADER_UNIFORM_FLOAT);
    }
    if (useSkinningLoc >= 0) SetShaderValue(shader, useSkinningLoc, &useSkinning, SHADER_UNIFORM_INT);
    if (hasStaticLightmapLoc >= 0) SetShaderValue(shader, hasStaticLightmapLoc, &noStaticLightmap, SHADER_UNIFORM_INT);
    if (useBakedAmbientOcclusionLoc >= 0) SetShaderValue(shader, useBakedAmbientOcclusionLoc, &noBakedAo, SHADER_UNIFORM_INT);
    if (containingSectorAmbientLoc >= 0) {
        const Vector3 ambient = SanitizeSectorPbrNonnegative(containingSectorAmbient);
        SetShaderValue(shader, containingSectorAmbientLoc, &ambient, SHADER_UNIFORM_VEC3);
    }
    for (size_t face = 0; face < objectAmbientCubeLocs.size(); ++face) {
        if (objectAmbientCubeLocs[face] >= 0) {
            const Vector3 lowerAmbient = SanitizeSectorPbrNonnegative(
                    lighting.lower.ambientCube[face]);
            SetShaderValue(shader, objectAmbientCubeLocs[face], &lowerAmbient, SHADER_UNIFORM_VEC3);
        }
        if (objectAmbientCubeUpperLocs[face] >= 0) {
            const Vector3 upperAmbient = SanitizeSectorPbrNonnegative(
                    lighting.upper.ambientCube[face]);
            SetShaderValue(shader, objectAmbientCubeUpperLocs[face], &upperAmbient, SHADER_UNIFORM_VEC3);
        }
    }
    const float lowerProbeHeight = std::isfinite(lighting.lowerHeightWorld)
            ? lighting.lowerHeightWorld : 0.0f;
    const float upperProbeHeight = std::isfinite(lighting.upperHeightWorld)
            ? lighting.upperHeightWorld : lowerProbeHeight;
    if (objectAmbientCubeLowerHeightLoc >= 0) SetShaderValue(shader, objectAmbientCubeLowerHeightLoc, &lowerProbeHeight, SHADER_UNIFORM_FLOAT);
    if (objectAmbientCubeUpperHeightLoc >= 0) SetShaderValue(shader, objectAmbientCubeUpperHeightLoc, &upperProbeHeight, SHADER_UNIFORM_FLOAT);
    if (canSkin) {
        rlEnableShader(shader.id);
        rlSetUniformMatrices(
                shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
                model.boneMatrices,
                model.skeleton.boneCount);
    }

    const bool validProbe = lighting.lower.valid || lighting.upper.valid;
    const SectorStaticSpecularLightContext staticSpecularContext =
            SelectSectorStaticSpecularLights(
                    staticSpecularLights,
                    receiverBounds,
                    receiverSectorId,
                    visibility,
                    objectProbeBakeCurrent && validProbe);
    UploadSectorStaticSpecularLights(
            shader, staticSpecularLocations, staticSpecularContext);

    const bool environmentActive = environment != nullptr && environment->id != 0;
    bool drewMesh = false;
    for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
        if (model.meshMaterial == nullptr) continue;
        const int materialIndex = model.meshMaterial[meshIndex];
        if (materialIndex < 0 || materialIndex >= model.materialCount) continue;
        const Material& source = model.materials[materialIndex];
        if (source.maps == nullptr) continue;

        std::array<MaterialMap, SectorStaticModelMaterialMapCount> maps{};
        std::copy_n(source.maps, SectorStaticModelMaterialMapCount, maps.begin());
        ConfigureSectorStaticModelAuxiliaryMaterialMaps(
                maps,
                nullptr,
                false,
                environment,
                dynamicLightContext.shadowMaps.shadowMap0,
                dynamicLightContext.shadowMaps.shadowMap1);
        Material material = source;
        material.shader = shader;
        material.maps = maps.data();
        engine::ModelMaterialAsset pbrMaterial;
        if (materialIndex < static_cast<int>(modelAsset.materials.size())
                && modelAsset.materials[static_cast<size_t>(materialIndex)]
                           .pbrMetallicRoughness) {
            pbrMaterial = modelAsset.materials[static_cast<size_t>(materialIndex)];
        } else {
            pbrMaterial.baseColorFactor = ColorToNormalizedVector4(
                    maps[MATERIAL_MAP_DIFFUSE].color);
            pbrMaterial.roughnessFactor = 1.0f;
            pbrMaterial.hasBaseColorTexture =
                    maps[MATERIAL_MAP_DIFFUSE].texture.id != 0;
        }
        pbrMaterial = NormalizeSectorPbrMaterial(pbrMaterial);
        if (baseColorFactorLoc >= 0) SetShaderValue(shader, baseColorFactorLoc, &pbrMaterial.baseColorFactor, SHADER_UNIFORM_VEC4);
        if (emissiveFactorLoc >= 0) SetShaderValue(shader, emissiveFactorLoc, &pbrMaterial.emissiveFactor, SHADER_UNIFORM_VEC3);
        if (emissiveStrengthLoc >= 0) SetShaderValue(shader, emissiveStrengthLoc, &pbrMaterial.emissiveStrength, SHADER_UNIFORM_FLOAT);
        if (metallicFactorLoc >= 0) SetShaderValue(shader, metallicFactorLoc, &pbrMaterial.metallicFactor, SHADER_UNIFORM_FLOAT);
        if (roughnessFactorLoc >= 0) SetShaderValue(shader, roughnessFactorLoc, &pbrMaterial.roughnessFactor, SHADER_UNIFORM_FLOAT);
        if (normalScaleLoc >= 0) SetShaderValue(shader, normalScaleLoc, &pbrMaterial.normalScale, SHADER_UNIFORM_FLOAT);
        if (occlusionStrengthLoc >= 0) SetShaderValue(shader, occlusionStrengthLoc, &pbrMaterial.occlusionStrength, SHADER_UNIFORM_FLOAT);
        const int hasBase = pbrMaterial.hasBaseColorTexture ? 1 : 0;
        const int hasMetal = pbrMaterial.hasMetallicTexture ? 1 : 0;
        const int hasNormal = pbrMaterial.hasNormalTexture ? 1 : 0;
        const int hasRoughness = pbrMaterial.hasRoughnessTexture ? 1 : 0;
        const int hasOcclusion = pbrMaterial.hasOcclusionTexture ? 1 : 0;
        const int hasEmissive = pbrMaterial.hasEmissiveTexture ? 1 : 0;
        if (hasBaseColorTextureLoc >= 0) SetShaderValue(shader, hasBaseColorTextureLoc, &hasBase, SHADER_UNIFORM_INT);
        if (hasMetallicTextureLoc >= 0) SetShaderValue(shader, hasMetallicTextureLoc, &hasMetal, SHADER_UNIFORM_INT);
        if (hasNormalTextureLoc >= 0) SetShaderValue(shader, hasNormalTextureLoc, &hasNormal, SHADER_UNIFORM_INT);
        if (hasRoughnessTextureLoc >= 0) SetShaderValue(shader, hasRoughnessTextureLoc, &hasRoughness, SHADER_UNIFORM_INT);
        if (hasOcclusionTextureLoc >= 0) SetShaderValue(shader, hasOcclusionTextureLoc, &hasOcclusion, SHADER_UNIFORM_INT);
        if (hasEmissiveTextureLoc >= 0) SetShaderValue(shader, hasEmissiveTextureLoc, &hasEmissive, SHADER_UNIFORM_INT);
        const SectorPbrDrawState drawState = BuildSectorPbrDrawState(
                SectorPbrLightingPath::WorldDynamic,
                validProbe,
                false,
                objectProbeBakeCurrent,
                environmentActive,
                environmentExposure,
                1.0f,
                false,
                contributionSettings);
        UploadPbrDrawState(drawState);
        UploadPbrMaterialTransferState(pbrMaterial);
        if (!worldDiagnostics.valid
                && (placedObjectId == diagnosticSelectedObjectId
                        || diagnosticSelectedObjectId < 0)) {
            RecordPbrDiagnostics(
                    worldDiagnostics,
                    placedObjectId,
                    modelHandle,
                    materialIndex,
                    drawState,
                    pbrMaterial,
                    staticSpecularContext);
        }
        DrawMesh(model.meshes[meshIndex], material, modelTransform);
        drewMesh = true;
    }
    return drewMesh;
}

void SectorStaticModelRenderer::Draw(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLightContext,
        const SectorStaticSpecularLightState& staticSpecularLights,
        bool surfaceLightmapBakeCurrent,
        bool objectProbeBakeCurrent,
        const SectorFogRenderContext& fogContext,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<engine::TextureHandle>& lightmapTextures,
        const TextureCubemap* environment,
        bool useBakedAmbientOcclusion,
        std::string& renderDebugText)
{
    if (!shaderLoaded || shader.id == 0) {
        AppendStaticModelDebugText(renderDebugText, 0, 0, 0, 0);
        return;
    }
    const float opaque = 1.0f;
    if (modelOpacityLoc >= 0) {
        SetShaderValue(shader, modelOpacityLoc, &opaque, SHADER_UNIFORM_FLOAT);
    }

    SectorDynamicLightShaderLocations dynamicLocations;
    dynamicLocations.dynamicLightCount = dynamicLightCountLoc;
    dynamicLocations.dynamicLightPositions = dynamicLightPositionsLoc;
    dynamicLocations.dynamicLightColors = dynamicLightColorsLoc;
    dynamicLocations.dynamicLightRadii = dynamicLightRadiiLoc;
    dynamicLocations.dynamicLightIntensities = dynamicLightIntensitiesLoc;
    dynamicLocations.dynamicLightTypes = dynamicLightTypesLoc;
    dynamicLocations.dynamicLightDirections = dynamicLightDirectionsLoc;
    dynamicLocations.dynamicLightInnerConeCos = dynamicLightInnerConeCosLoc;
    dynamicLocations.dynamicLightOuterConeCos = dynamicLightOuterConeCosLoc;
    UploadSectorRendererDynamicPointLights(
            shader,
            dynamicLocations,
            dynamicLightContext);

    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    UploadSectorRendererDynamicSpotLightShadowUniforms(
            shader,
            shadowLocations,
            dynamicLightContext.shadowUniforms);
    UploadSectorFogShaderValues(shader, fogShaderLocations, fogContext);
    if (cameraPositionLoc >= 0) {
        SetShaderValue(
                shader,
                cameraPositionLoc,
                &camera.position,
                SHADER_UNIFORM_VEC3);
    }
    const bool environmentActive = environment != nullptr && environment->id != 0;
    worldDiagnostics = {};

    size_t considered = 0;
    size_t drawn = 0;
    size_t portalCulled = 0;
    size_t skipped = 0;
    rlDisableColorBlend();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorStaticModel>(
            [this,
             &assets,
             &dynamicLightContext,
             &staticSpecularLights,
             &visibility,
             &lightmapTextures,
             environment,
             environmentActive,
             surfaceLightmapBakeCurrent,
             useBakedAmbientOcclusion,
             &considered,
             &drawn,
             &portalCulled,
             &skipped](
                    engine::Entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorStaticModel& staticModel) {
                ++considered;
                if (!ShouldDrawRuntimeSectorForVisibility(
                            object.currentSectorId,
                            visibility)) {
                    ++portalCulled;
                    return;
                }
                if (!object.visible) {
                    ++skipped;
                    return;
                }
                const engine::ModelAsset* modelAsset =
                        assets.GetModelAsset(staticModel.model);
                if (modelAsset == nullptr) {
                    ++skipped;
                    if (!warningPrinted
                            && !engine::IsNull(staticModel.model)
                            && assets.HasFailed(staticModel.model)) {
                        std::fprintf(
                                stderr,
                                "[SectorMeshRenderer WARNING] Skipping static prop with failed model asset\n");
                        warningPrinted = true;
                    }
                    return;
                }
                const Model* model = &modelAsset->model;

                const int useSkinning = 0;
                if (useSkinningLoc >= 0) {
                    SetShaderValue(shader, useSkinningLoc, &useSkinning, SHADER_UNIFORM_INT);
                }

                if (containingSectorAmbientLoc >= 0) {
                    const Vector3 ambient = SanitizeSectorPbrNonnegative(
                            staticModel.containingSectorAmbient);
                    SetShaderValue(
                            shader,
                            containingSectorAmbientLoc,
                            &ambient,
                            SHADER_UNIFORM_VEC3);
                }
                const int useAo = useBakedAmbientOcclusion ? 1 : 0;
                if (useBakedAmbientOcclusionLoc >= 0) {
                    SetShaderValue(
                            shader,
                            useBakedAmbientOcclusionLoc,
                            &useAo,
                            SHADER_UNIFORM_INT);
                }
                const Matrix authoredTransform =
                        BuildSectorStaticModelAuthoredTransform(
                                transform.position,
                                transform.rotationXRadians,
                                transform.yawRadians,
                                transform.rotationZRadians,
                                staticModel.scale);
                const Matrix modelTransform = MatrixMultiply(
                        model->transform,
                        authoredTransform);

                const SectorStaticModelLightmapObject* lightmapObject =
                        FindLightmapObject(
                                lightmapData,
                                staticModel.placedObjectId);
                const CachedModel* cached =
                        lightmapObject == nullptr
                        ? nullptr
                        : FindCachedModel(
                                staticModel.model,
                                lightmapObject->modelIndex);
                const bool hasRemapData = lightmapObject != nullptr
                        && cached != nullptr
                        && cached->meshes.size()
                                == static_cast<size_t>(model->meshCount)
                        && lightmapObject->meshPlacements.size()
                                == static_cast<size_t>(model->meshCount);
                const SectorReceiverBounds receiverBounds =
                        modelAsset->hasLocalBounds
                        ? TransformSectorStaticSpecularReceiverBounds(
                                modelAsset->localBounds,
                                authoredTransform,
                                object.currentSectorId,
                                transform.position)
                        : SectorReceiverBounds{
                                object.currentSectorId,
                                transform.position,
                                transform.position};
                const SectorStaticSpecularLightContext staticSpecularContext =
                        SelectSectorStaticSpecularLights(
                                staticSpecularLights,
                                receiverBounds,
                                object.currentSectorId,
                                visibility,
                                surfaceLightmapBakeCurrent && hasRemapData);
                UploadSectorStaticSpecularLights(
                        shader,
                        staticSpecularLocations,
                        staticSpecularContext);

                bool drewMesh = false;
                for (int meshIndex = 0; meshIndex < model->meshCount; ++meshIndex) {
                    if (model->meshMaterial == nullptr) {
                        continue;
                    }
                    const int materialIndex = model->meshMaterial[meshIndex];
                    if (materialIndex < 0 || materialIndex >= model->materialCount) {
                        continue;
                    }
                    const Material& source = model->materials[materialIndex];
                    if (source.maps == nullptr) {
                        continue;
                    }

                    const SectorStaticModelLightmapMeshPlacement* placement =
                            hasRemapData
                            ? &lightmapObject->meshPlacements[
                                    static_cast<size_t>(meshIndex)]
                            : nullptr;
                    const Texture2D* lightmap = placement != nullptr
                            && placement->atlasIndex >= 0
                            && placement->atlasIndex
                                    < static_cast<int>(lightmapTextures.size())
                            ? assets.GetTexture(lightmapTextures[
                                    static_cast<size_t>(placement->atlasIndex)])
                            : nullptr;
                    const bool hasRemappedMesh = placement != nullptr
                            && lightmap != nullptr
                            && lightmap->id != 0;

                    std::array<
                            MaterialMap,
                            SectorStaticModelMaterialMapCount> maps{};
                    std::copy_n(
                            source.maps,
                            SectorStaticModelMaterialMapCount,
                            maps.begin());
                    ConfigureSectorStaticModelAuxiliaryMaterialMaps(
                            maps,
                            lightmap,
                            hasRemappedMesh,
                            environment,
                            dynamicLightContext.shadowMaps.shadowMap0,
                            dynamicLightContext.shadowMaps.shadowMap1);
                    Material material = source;
                    material.shader = shader;
                    material.maps = maps.data();
                    engine::ModelMaterialAsset pbrMaterial;
                    if (materialIndex >= 0
                            && materialIndex < static_cast<int>(modelAsset->materials.size())
                            && modelAsset->materials[
                                       static_cast<size_t>(materialIndex)]
                                       .pbrMetallicRoughness) {
                        pbrMaterial = modelAsset->materials[
                                static_cast<size_t>(materialIndex)];
                    } else {
                        pbrMaterial.baseColorFactor = ColorToNormalizedVector4(
                                maps[MATERIAL_MAP_DIFFUSE].color);
                        pbrMaterial.metallicFactor = 0.0f;
                        pbrMaterial.roughnessFactor = 1.0f;
                        pbrMaterial.hasBaseColorTexture =
                                maps[MATERIAL_MAP_DIFFUSE].texture.id != 0;
                    }
                    pbrMaterial = NormalizeSectorPbrMaterial(pbrMaterial);
                    if (baseColorFactorLoc >= 0) {
                        SetShaderValue(
                                shader,
                                baseColorFactorLoc,
                                &pbrMaterial.baseColorFactor,
                                SHADER_UNIFORM_VEC4);
                    }
                    if (emissiveFactorLoc >= 0) SetShaderValue(shader, emissiveFactorLoc, &pbrMaterial.emissiveFactor, SHADER_UNIFORM_VEC3);
                    if (emissiveStrengthLoc >= 0) SetShaderValue(shader, emissiveStrengthLoc, &pbrMaterial.emissiveStrength, SHADER_UNIFORM_FLOAT);
                    if (metallicFactorLoc >= 0) SetShaderValue(shader, metallicFactorLoc, &pbrMaterial.metallicFactor, SHADER_UNIFORM_FLOAT);
                    if (roughnessFactorLoc >= 0) SetShaderValue(shader, roughnessFactorLoc, &pbrMaterial.roughnessFactor, SHADER_UNIFORM_FLOAT);
                    if (normalScaleLoc >= 0) SetShaderValue(shader, normalScaleLoc, &pbrMaterial.normalScale, SHADER_UNIFORM_FLOAT);
                    if (occlusionStrengthLoc >= 0) SetShaderValue(shader, occlusionStrengthLoc, &pbrMaterial.occlusionStrength, SHADER_UNIFORM_FLOAT);
                    const int hasBase = pbrMaterial.hasBaseColorTexture ? 1 : 0;
                    const int hasMetal = pbrMaterial.hasMetallicTexture ? 1 : 0;
                    const int hasNormal = pbrMaterial.hasNormalTexture ? 1 : 0;
                    const int hasRoughness = pbrMaterial.hasRoughnessTexture ? 1 : 0;
                    const int hasOcclusion = pbrMaterial.hasOcclusionTexture ? 1 : 0;
                    const int hasEmissive = pbrMaterial.hasEmissiveTexture ? 1 : 0;
                    if (hasBaseColorTextureLoc >= 0) SetShaderValue(shader, hasBaseColorTextureLoc, &hasBase, SHADER_UNIFORM_INT);
                    if (hasMetallicTextureLoc >= 0) SetShaderValue(shader, hasMetallicTextureLoc, &hasMetal, SHADER_UNIFORM_INT);
                    if (hasNormalTextureLoc >= 0) SetShaderValue(shader, hasNormalTextureLoc, &hasNormal, SHADER_UNIFORM_INT);
                    if (hasRoughnessTextureLoc >= 0) SetShaderValue(shader, hasRoughnessTextureLoc, &hasRoughness, SHADER_UNIFORM_INT);
                    if (hasOcclusionTextureLoc >= 0) SetShaderValue(shader, hasOcclusionTextureLoc, &hasOcclusion, SHADER_UNIFORM_INT);
                    if (hasEmissiveTextureLoc >= 0) SetShaderValue(shader, hasEmissiveTextureLoc, &hasEmissive, SHADER_UNIFORM_INT);
                    const int hasStaticLightmap =
                            hasRemappedMesh ? 1 : 0;
                    if (hasStaticLightmapLoc >= 0) {
                        SetShaderValue(
                                shader,
                                hasStaticLightmapLoc,
                                &hasStaticLightmap,
                                SHADER_UNIFORM_INT);
                    }
                    Vector4 scaleBias{};
                    if (hasRemappedMesh) {
                        scaleBias = Vector4{
                                placement->atlasScale.x,
                                placement->atlasScale.y,
                                placement->atlasBias.x,
                                placement->atlasBias.y};
                    }
                    if (lightmapScaleBiasLoc >= 0) {
                        SetShaderValue(
                                shader,
                                lightmapScaleBiasLoc,
                                &scaleBias,
                                SHADER_UNIFORM_VEC4);
                    }
                    const SectorPbrDrawState drawState = BuildSectorPbrDrawState(
                            SectorPbrLightingPath::WorldStatic,
                            false,
                            hasRemappedMesh,
                            surfaceLightmapBakeCurrent,
                            environmentActive,
                            staticModel.environmentExposure,
                            1.0f,
                            false,
                            contributionSettings);
                    UploadPbrDrawState(drawState);
                    UploadPbrMaterialTransferState(pbrMaterial);
                    if (!worldDiagnostics.valid
                            && (staticModel.placedObjectId
                                            == diagnosticSelectedObjectId
                                    || diagnosticSelectedObjectId < 0)) {
                        RecordPbrDiagnostics(
                                worldDiagnostics,
                                staticModel.placedObjectId,
                                staticModel.model,
                                materialIndex,
                                drawState,
                                pbrMaterial,
                                staticSpecularContext);
                    }
                    const Mesh& mesh = hasRemappedMesh
                            ? cached->meshes[static_cast<size_t>(meshIndex)]
                            : model->meshes[meshIndex];
                    DrawMesh(mesh, material, modelTransform);
                    drewMesh = true;
                }
                if (drewMesh) {
                    ++drawn;
                } else {
                    ++skipped;
                }
            });

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorObjectLighting,
            SectorDynamicModel,
            engine::AnimatedModelInstance>(
            [this,
             &assets,
             &dynamicLightContext,
             &staticSpecularLights,
             &visibility,
             environment,
             objectProbeBakeCurrent,
             &considered,
             &drawn,
             &portalCulled,
             &skipped,
             &runtimeObjectWorld](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorObjectLighting& lighting,
                    SectorDynamicModel& dynamicModel,
                    engine::AnimatedModelInstance& instance) {
                ++considered;
                if (!ShouldDrawRuntimeSectorForVisibility(object.currentSectorId, visibility)) {
                    ++portalCulled;
                    return;
                }
                if (!object.visible || !instance.poseReady || instance.poseFailed) {
                    ++skipped;
                    return;
                }
                const engine::ModelAsset* modelAsset = assets.GetModelAsset(instance.model);
                if (modelAsset == nullptr) {
                    ++skipped;
                    return;
                }
                Model model = engine::BuildAnimatedModelPoseView(*modelAsset, instance);
                Vector3 renderPosition = transform.position;
                if (runtimeObjectWorld.Has<SectorObjectVisualOffset>(entity)) {
                    renderPosition = Vector3Add(
                            renderPosition,
                            runtimeObjectWorld.Get<SectorObjectVisualOffset>(entity).position);
                }
                const Matrix authoredTransform = BuildSectorStaticModelAuthoredTransform(
                        renderPosition,
                        transform.rotationXRadians,
                        transform.yawRadians,
                        transform.rotationZRadians,
                        dynamicModel.scale);
                const Matrix modelTransform = MatrixMultiply(model.transform, authoredTransform);
                const SectorReceiverBounds receiverBounds =
                        modelAsset->hasLocalBounds
                        ? TransformSectorStaticSpecularReceiverBounds(
                                modelAsset->localBounds,
                                authoredTransform,
                                object.currentSectorId,
                                renderPosition)
                        : SectorReceiverBounds{
                                object.currentSectorId,
                                renderPosition,
                                renderPosition};
                const bool fading = dynamicModel.opacity < 0.999f;
                if (fading) {
                    rlEnableColorBlend();
                    rlSetBlendMode(BLEND_ALPHA);
                    rlDisableDepthMask();
                }
                const bool drewMesh = DrawWorldDynamicModel(
                        *modelAsset,
                        model,
                        instance.model,
                        modelTransform,
                        dynamicModel.placedObjectId,
                        object.currentSectorId,
                        receiverBounds,
                        dynamicModel.containingSectorAmbient,
                        dynamicModel.environmentExposure,
                        lighting.vertical,
                        dynamicLightContext,
                        staticSpecularLights,
                        visibility,
                        objectProbeBakeCurrent,
                        environment,
                        true,
                        dynamicModel.opacity);
                if (fading) {
                    rlDisableColorBlend();
                    rlEnableDepthMask();
                }
                if (drewMesh) ++drawn;
                else ++skipped;
            });

    runtimeObjectWorld.ForEach<
            SectorObject,
            SectorObjectLighting,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender,
            SectorDoorModelRender>(
            [this,
             &assets,
             &dynamicLightContext,
             &staticSpecularLights,
             &visibility,
             environment,
             objectProbeBakeCurrent,
             &considered,
             &drawn,
             &portalCulled,
             &skipped](
                    engine::Entity,
                    SectorObject& object,
                    SectorObjectLighting& lighting,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render,
                    SectorDoorModelRender& modelRender) {
                if (!modelRender.modelVisualRequested) {
                    return;
                }
                ++considered;
                if (!ShouldDrawSectorDoorForVisibility(anchor, visibility)) {
                    ++portalCulled;
                    return;
                }
                if (!object.visible || !door.enabled || !render.visible) {
                    ++skipped;
                    return;
                }
                const engine::ModelAsset* leafAsset =
                        assets.GetModelAsset(modelRender.leafModel);
                const engine::ModelAsset* frameAsset =
                        assets.GetModelAsset(modelRender.frameModel);
                const SectorDoorModelDrawPolicy policy =
                        ResolveSectorDoorModelDrawPolicy(
                                modelRender,
                                leafAsset != nullptr,
                                frameAsset != nullptr);
                if (!policy.drawLeaf && !policy.drawFrame) {
                    ++skipped;
                    return;
                }

                const SectorReceiverBounds receiverBounds{
                        object.currentSectorId,
                        modelRender.receiverBounds.min,
                        modelRender.receiverBounds.max};
                bool drewDoorMesh = false;
                if (policy.drawLeaf) {
                    const Model& leaf = leafAsset->model;
                    drewDoorMesh = DrawWorldDynamicModel(
                            *leafAsset,
                            leaf,
                            modelRender.leafModel,
                            MatrixMultiply(leaf.transform, modelRender.leafMatrix),
                            door.placedObjectId,
                            object.currentSectorId,
                            receiverBounds,
                            modelRender.containingSectorAmbient,
                            modelRender.environmentExposure,
                            lighting.vertical,
                            dynamicLightContext,
                            staticSpecularLights,
                            visibility,
                            objectProbeBakeCurrent,
                            environment,
                            false) || drewDoorMesh;
                }
                if (policy.drawFrame) {
                    const Model& frame = frameAsset->model;
                    drewDoorMesh = DrawWorldDynamicModel(
                            *frameAsset,
                            frame,
                            modelRender.frameModel,
                            MatrixMultiply(frame.transform, modelRender.frameMatrix),
                            door.placedObjectId,
                            object.currentSectorId,
                            receiverBounds,
                            modelRender.containingSectorAmbient,
                            modelRender.environmentExposure,
                            lighting.vertical,
                            dynamicLightContext,
                            staticSpecularLights,
                            visibility,
                            objectProbeBakeCurrent,
                            environment,
                            false) || drewDoorMesh;
                }
                if (drewDoorMesh) ++drawn;
                else ++skipped;
            });

    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
    AppendStaticModelDebugText(
            renderDebugText,
            drawn,
            considered,
            portalCulled,
            skipped);
}

void SectorStaticModelRenderer::DrawViewmodel(
        const engine::ModelAsset& asset,
        engine::AnimatedModelInstance& instance,
        const Camera3D& camera,
        Matrix transform,
        const engine::ModelAsset* attachmentAsset,
        Matrix attachmentTransform,
        const SectorBillboardDynamicLightContext& dynamicLightContext,
        const SectorStaticSpecularLightContext& staticSpecularLights,
        bool objectProbeBakeCurrent,
        const TextureCubemap* environment,
        const BakedObjectLightingVerticalSample& ambientLighting,
        const SectorViewmodelLightingContext& lighting,
        const SectorViewmodelLightingContext& attachmentLighting)
{
    if (!shaderLoaded || !instance.poseReady || instance.poseFailed) return;
    const float opaque = 1.0f;
    if (modelOpacityLoc >= 0) {
        SetShaderValue(shader, modelOpacityLoc, &opaque, SHADER_UNIFORM_FLOAT);
    }

    SectorDynamicLightShaderLocations dynamicLocations{
            dynamicLightCountLoc, dynamicLightPositionsLoc, dynamicLightColorsLoc,
            dynamicLightRadiiLoc, dynamicLightIntensitiesLoc, dynamicLightTypesLoc,
            dynamicLightDirectionsLoc, dynamicLightInnerConeCosLoc,
            dynamicLightOuterConeCosLoc};
    UploadSectorRendererDynamicPointLights(shader, dynamicLocations, dynamicLightContext);
    UploadSectorStaticSpecularLights(
            shader,
            staticSpecularLocations,
            staticSpecularLights);
    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    UploadSectorRendererDynamicSpotLightShadowUniforms(shader, shadowLocations, dynamicLightContext.shadowUniforms);
    UploadSectorFogShaderValues(shader, fogShaderLocations, SectorFogRenderContext{});
    if (cameraPositionLoc >= 0) SetShaderValue(shader, cameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
    const bool environmentActive = environment && environment->id != 0;
    viewmodelDiagnostics = {};

    const int disabled = 0;
    if (hasStaticLightmapLoc >= 0) SetShaderValue(shader, hasStaticLightmapLoc, &disabled, SHADER_UNIFORM_INT);
    if (useBakedAmbientOcclusionLoc >= 0) SetShaderValue(shader, useBakedAmbientOcclusionLoc, &disabled, SHADER_UNIFORM_INT);
    const Vector3 fallbackAmbient = SanitizeSectorPbrNonnegative(
            ambientLighting.lower.ambientCube[2]);
    if (containingSectorAmbientLoc >= 0) SetShaderValue(shader, containingSectorAmbientLoc, &fallbackAmbient, SHADER_UNIFORM_VEC3);
    for (size_t face = 0; face < objectAmbientCubeLocs.size(); ++face) {
        const Vector3 lowerAmbient = SanitizeSectorPbrNonnegative(
                ambientLighting.lower.ambientCube[face]);
        const Vector3 upperAmbient = SanitizeSectorPbrNonnegative(
                ambientLighting.upper.ambientCube[face]);
        if (objectAmbientCubeLocs[face] >= 0) SetShaderValue(shader, objectAmbientCubeLocs[face], &lowerAmbient, SHADER_UNIFORM_VEC3);
        if (objectAmbientCubeUpperLocs[face] >= 0) SetShaderValue(shader, objectAmbientCubeUpperLocs[face], &upperAmbient, SHADER_UNIFORM_VEC3);
    }
    const float lowerProbeHeight = std::isfinite(ambientLighting.lowerHeightWorld)
            ? ambientLighting.lowerHeightWorld : 0.0f;
    const float upperProbeHeight = std::isfinite(ambientLighting.upperHeightWorld)
            ? ambientLighting.upperHeightWorld : lowerProbeHeight;
    if (objectAmbientCubeLowerHeightLoc >= 0) SetShaderValue(shader, objectAmbientCubeLowerHeightLoc, &lowerProbeHeight, SHADER_UNIFORM_FLOAT);
    if (objectAmbientCubeUpperHeightLoc >= 0) SetShaderValue(shader, objectAmbientCubeUpperHeightLoc, &upperProbeHeight, SHADER_UNIFORM_FLOAT);
    rlDisableColorBlend();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
    const bool validProbe = ambientLighting.lower.valid
            || ambientLighting.upper.valid;
    const auto drawModel = [this,
                            environment,
                            environmentActive,
                            validProbe,
                            objectProbeBakeCurrent,
                            &staticSpecularLights,
                            &dynamicLightContext](
            const engine::ModelAsset& modelAsset,
            Model model,
            Matrix itemTransform,
            const SectorViewmodelLightingContext& itemLighting,
            SectorPbrLightingPath path) {
        const int skinningEnabled = 1;
        const int skinningDisabled = 0;
        const bool canSkin = model.skeleton.boneCount > 0
                && model.skeleton.boneCount <= engine::MaxAnimatedModelBones
                && model.boneMatrices != nullptr
                && shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS] >= 0;
        if (useSkinningLoc >= 0) {
            SetShaderValue(
                    shader,
                    useSkinningLoc,
                    canSkin ? &skinningEnabled : &skinningDisabled,
                    SHADER_UNIFORM_INT);
        }
        if (canSkin) {
            rlEnableShader(shader.id);
            rlSetUniformMatrices(
                    shader.locs[SHADER_LOC_MATRIX_BONETRANSFORMS],
                    model.boneMatrices,
                    model.skeleton.boneCount);
        }

        const Matrix modelTransform = MatrixMultiply(
                model.transform, itemTransform);
        for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
            if (!model.meshMaterial) continue;
            const int materialIndex = model.meshMaterial[meshIndex];
            if (materialIndex < 0 || materialIndex >= model.materialCount) continue;
            const Material& source = model.materials[materialIndex];
            if (!source.maps) continue;
            std::array<MaterialMap, SectorStaticModelMaterialMapCount> maps{};
            std::copy_n(
                    source.maps,
                    SectorStaticModelMaterialMapCount,
                    maps.begin());
            ConfigureSectorStaticModelAuxiliaryMaterialMaps(
                    maps,
                    nullptr,
                    false,
                    environment,
                    dynamicLightContext.shadowMaps.shadowMap0,
                    dynamicLightContext.shadowMaps.shadowMap1);
            Material material = source;
            material.shader = shader;
            material.maps = maps.data();
            engine::ModelMaterialAsset pbr;
            if (materialIndex < static_cast<int>(modelAsset.materials.size())
                    && modelAsset.materials[
                            static_cast<size_t>(materialIndex)].pbrMetallicRoughness) {
                pbr = modelAsset.materials[static_cast<size_t>(materialIndex)];
            } else {
                pbr.baseColorFactor = ColorToNormalizedVector4(
                        maps[MATERIAL_MAP_DIFFUSE].color);
                pbr.roughnessFactor = 1.0f;
                pbr.hasBaseColorTexture =
                        maps[MATERIAL_MAP_DIFFUSE].texture.id != 0;
            }
            ApplySectorViewmodelMaterialOverride(
                    itemLighting,
                    pbr.metallicFactor,
                    pbr.roughnessFactor,
                    pbr.hasMetallicTexture,
                    pbr.hasRoughnessTexture);
            pbr = NormalizeSectorPbrMaterial(pbr);
            if (baseColorFactorLoc >= 0) SetShaderValue(shader, baseColorFactorLoc, &pbr.baseColorFactor, SHADER_UNIFORM_VEC4);
            if (emissiveFactorLoc >= 0) SetShaderValue(shader, emissiveFactorLoc, &pbr.emissiveFactor, SHADER_UNIFORM_VEC3);
            if (emissiveStrengthLoc >= 0) SetShaderValue(shader, emissiveStrengthLoc, &pbr.emissiveStrength, SHADER_UNIFORM_FLOAT);
            if (metallicFactorLoc >= 0) SetShaderValue(shader, metallicFactorLoc, &pbr.metallicFactor, SHADER_UNIFORM_FLOAT);
            if (roughnessFactorLoc >= 0) SetShaderValue(shader, roughnessFactorLoc, &pbr.roughnessFactor, SHADER_UNIFORM_FLOAT);
            if (normalScaleLoc >= 0) SetShaderValue(shader, normalScaleLoc, &pbr.normalScale, SHADER_UNIFORM_FLOAT);
            if (occlusionStrengthLoc >= 0) SetShaderValue(shader, occlusionStrengthLoc, &pbr.occlusionStrength, SHADER_UNIFORM_FLOAT);
            const int hasBase = pbr.hasBaseColorTexture;
            const int hasMetal = pbr.hasMetallicTexture;
            const int hasNormal = pbr.hasNormalTexture;
            const int hasRough = pbr.hasRoughnessTexture;
            const int hasOcclusion = pbr.hasOcclusionTexture;
            const int hasEmissive = pbr.hasEmissiveTexture;
            if (hasBaseColorTextureLoc >= 0) SetShaderValue(shader, hasBaseColorTextureLoc, &hasBase, SHADER_UNIFORM_INT);
            if (hasMetallicTextureLoc >= 0) SetShaderValue(shader, hasMetallicTextureLoc, &hasMetal, SHADER_UNIFORM_INT);
            if (hasNormalTextureLoc >= 0) SetShaderValue(shader, hasNormalTextureLoc, &hasNormal, SHADER_UNIFORM_INT);
            if (hasRoughnessTextureLoc >= 0) SetShaderValue(shader, hasRoughnessTextureLoc, &hasRough, SHADER_UNIFORM_INT);
            if (hasOcclusionTextureLoc >= 0) SetShaderValue(shader, hasOcclusionTextureLoc, &hasOcclusion, SHADER_UNIFORM_INT);
            if (hasEmissiveTextureLoc >= 0) SetShaderValue(shader, hasEmissiveTextureLoc, &hasEmissive, SHADER_UNIFORM_INT);
            const SectorPbrDrawState drawState = BuildSectorPbrDrawState(
                    path,
                    validProbe,
                    false,
                    objectProbeBakeCurrent,
                    environmentActive,
                    itemLighting.environmentExposure,
                    itemLighting.brightnessMultiplier,
                    itemLighting.materialOverrideEnabled,
                    contributionSettings);
            UploadPbrDrawState(drawState);
            UploadPbrMaterialTransferState(pbr);
            RecordPbrDiagnostics(
                    viewmodelDiagnostics,
                    -1,
                    engine::NullModelHandle(),
                    materialIndex,
                    drawState,
                    pbr,
                    staticSpecularLights);
            DrawMesh(model.meshes[meshIndex], material, modelTransform);
        }
    };

    if (attachmentAsset != nullptr) {
        drawModel(
                *attachmentAsset,
                attachmentAsset->model,
                attachmentTransform,
                attachmentLighting,
                SectorPbrLightingPath::ViewmodelAttachment);
    }
    drawModel(
            asset,
            engine::BuildAnimatedModelPoseView(asset, instance),
            transform,
            lighting,
            SectorPbrLightingPath::Viewmodel);
    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

} // namespace game
