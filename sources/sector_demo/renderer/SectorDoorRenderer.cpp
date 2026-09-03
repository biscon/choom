#include "sector_demo/renderer/SectorDoorRenderer.h"

#include "sector_demo/renderer/SectorDynamicShadowSampling.h"
#include "sector_demo/renderer/SectorFlashlightProfileSampling.h"

#include "engine/assets/AssetManager.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorRuntimeObjects.h"

#include <raylib.h>
#include <rlgl.h>

#include <cstdio>
#include <limits>

namespace game {

namespace {

bool SameMatrixExact(const Matrix& a, const Matrix& b)
{
    return a.m0 == b.m0 && a.m1 == b.m1 && a.m2 == b.m2 && a.m3 == b.m3
            && a.m4 == b.m4 && a.m5 == b.m5 && a.m6 == b.m6 && a.m7 == b.m7
            && a.m8 == b.m8 && a.m9 == b.m9 && a.m10 == b.m10 && a.m11 == b.m11
            && a.m12 == b.m12 && a.m13 == b.m13 && a.m14 == b.m14 && a.m15 == b.m15;
}

const char* SectorDoorOpaqueVs = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec4 vertexTangent;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragWorldPosition = (matModel * vec4(vertexPosition, 1.0)).xyz;
    fragWorldNormal = normalize((matNormal * vec4(vertexNormal, 0.0)).xyz);
    fragColor = vec4(vertexTangent.xyz, 1.0);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* SectorDoorOpaqueFs = R"(
#version 330
in vec2 fragTexCoord;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D normalTexture;
uniform int hasNormalMap;
uniform float normalStrength;
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3 cameraPosition;
uniform samplerCube environmentTexture;
uniform int hasEnvironment;
uniform float environmentExposure;
uniform float indirectDiffuseScale;
uniform float environmentSpecularScale;
uniform int environmentBoxProjection;
uniform vec3 environmentCapturePosition;
uniform vec3 environmentInfluenceCenter;
uniform vec3 environmentHalfExtents;
uniform float environmentYaw;
uniform float environmentMaxLod;
uniform int pbrDiagnosticMode;
uniform int useObjectAmbientCube;
uniform vec3 objectAmbientCube[6];

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
uniform float staticSpecularLightStartFeathers[MAX_STATIC_SPECULAR_LIGHTS];
uniform int useStaticSpecularLighting;

#define MAX_DYNAMIC_LIGHTS 32
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

#define MAX_DYNAMIC_SHADOW_CASTERS 64
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform int shadowAtlasTilesPerRow;
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

uniform vec4 doorTint;

uniform int fogEnabled;
uniform vec3 fogColor;
uniform vec3 fogCameraPosition;
uniform float fogStartDistanceWorld;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeightWorld;
uniform float fogHeightFalloff;

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

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSq = dot(value, value);
    return lengthSq > 0.00000001 ? value * inversesqrt(lengthSq) : fallback;
}

)"
SECTOR_FLASHLIGHT_PROFILE_GLSL
R"(
vec3 StoreFiniteHalfRadiance(vec3 value)
{
    vec3 result;
    result.r = isnan(value.r) ? 0.0 : (isinf(value.r) ? (value.r > 0.0 ? 65504.0 : 0.0) : min(max(value.r, 0.0), 65504.0));
    result.g = isnan(value.g) ? 0.0 : (isinf(value.g) ? (value.g > 0.0 ? 65504.0 : 0.0) : min(max(value.g, 0.0), 65504.0));
    result.b = isnan(value.b) ? 0.0 : (isinf(value.b) ? (value.b > 0.0 ? 65504.0 : 0.0) : min(max(value.b, 0.0), 65504.0));
    return result;
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

float GeometrySmith(
        vec3 normal,
        vec3 viewDirection,
        vec3 lightDirection,
        float roughness)
{
    return GeometrySchlickGgx(
            max(dot(normal, viewDirection), 0.0), roughness)
            * GeometrySchlickGgx(
                    max(dot(normal, lightDirection), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0)
            * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec2 EnvironmentBrdfApprox(float roughness, float ndotv)
{
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

)"
SECTOR_DYNAMIC_SURFACE_SHADOW_GLSL
R"(
vec3 SurfaceNormal(vec3 geometricNormal, vec3 tangentNormalSample)
{
    if (hasNormalMap == 0) {
        return geometricNormal;
    }

    vec3 positionDx = dFdx(fragWorldPosition);
    vec3 positionDy = dFdy(fragWorldPosition);
    vec2 uvDx = dFdx(fragTexCoord);
    vec2 uvDy = dFdy(fragTexCoord);
    float uvDeterminant = uvDx.x * uvDy.y - uvDx.y * uvDy.x;
    float uvDerivativeScaleSq = dot(uvDx, uvDx) * dot(uvDy, uvDy);
    if (uvDeterminant * uvDeterminant
                    <= uvDerivativeScaleSq * 0.00000001) {
        return geometricNormal;
    }

    float inverseUvDeterminant = 1.0 / uvDeterminant;
    vec3 tangent = (positionDx * uvDy.y - positionDy * uvDx.y)
            * inverseUvDeterminant;
    vec3 sourceBitangent = (positionDy * uvDx.x - positionDx * uvDy.x)
            * inverseUvDeterminant;
    tangent -= geometricNormal * dot(tangent, geometricNormal);
    if (any(isnan(tangent)) || any(isinf(tangent))
            || any(isnan(sourceBitangent)) || any(isinf(sourceBitangent))
            || dot(tangent, tangent) <= 0.000000000001
            || dot(sourceBitangent, sourceBitangent) <= 0.000000000001) {
        return geometricNormal;
    }

    tangent = normalize(tangent);
    float handedness = dot(cross(geometricNormal, tangent), sourceBitangent) < 0.0
            ? -1.0
            : 1.0;
    vec3 bitangent = SafeNormalize(
            cross(geometricNormal, tangent),
            vec3(0.0, 0.0, 1.0)) * handedness;
    vec3 mappedNormal = tangentNormalSample * 2.0 - 1.0;
    mappedNormal.xy *= normalStrength;
    return SafeNormalize(
            mat3(tangent, bitangent, geometricNormal) * mappedNormal,
            geometricNormal);
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
    vec3 geometricNormal = SafeNormalize(
            fragWorldNormal, vec3(0.0, 1.0, 0.0));
    vec3 receiverPlaneNormal = geometricNormal;
    if (hasPointShadows != 0) {
        receiverPlaneNormal = SafeNormalize(
                cross(dFdx(fragWorldPosition), dFdy(fragWorldPosition)),
                geometricNormal);
        if (dot(receiverPlaneNormal, geometricNormal) < 0.0) {
            receiverPlaneNormal = -receiverPlaneNormal;
        }
    }
    vec4 sampled = texture(texture0, fragTexCoord);
    vec3 ambientWeights = geometricNormal * geometricNormal;
    vec3 objectProbeLighting =
            objectAmbientCube[geometricNormal.x >= 0.0 ? 0 : 1] * ambientWeights.x
            + objectAmbientCube[geometricNormal.y >= 0.0 ? 2 : 3] * ambientWeights.y
            + objectAmbientCube[geometricNormal.z >= 0.0 ? 4 : 5] * ambientWeights.z;
    vec3 staticProbeLighting = useObjectAmbientCube != 0
            ? max(objectProbeLighting, vec3(0.0))
            : max(fragColor.rgb, vec3(0.0));
    vec3 tint = clamp(doorTint.rgb, 0.0, 1.0);
    vec3 surfaceRgb = sampled.rgb * tint;
    vec3 tangentNormalSample = hasNormalMap != 0
            ? texture(normalTexture, fragTexCoord).xyz
            : vec3(0.5, 0.5, 1.0);
    vec3 worldNormal = SurfaceNormal(
            geometricNormal, tangentNormalSample);
    vec3 viewDirection = SafeNormalize(
            cameraPosition - fragWorldPosition, geometricNormal);
    float metallic = clamp(metallicFactor, 0.0, 1.0);
    float roughness = clamp(roughnessFactor, 0.045, 1.0);
    vec3 f0 = mix(vec3(0.04), surfaceRgb, metallic);
    vec3 indirectDiffuse = surfaceRgb
            * (1.0 - metallic)
            * staticProbeLighting
            * indirectDiffuseScale;
    vec3 dynamicDirectDiffuse = vec3(0.0);
    vec3 dynamicDirectSpecular = vec3(0.0);
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
            vec3 lightDirection = distanceToLight > 0.0001 ? toLight / distanceToLight : worldNormal;
            float ndotl = max(dot(worldNormal, lightDirection), 0.0);
            float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
            atten *= atten;
            if (ndotl <= 0.0 || atten <= 0.0
                    || dynamicLightIntensities[i] <= 0.0) continue;
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
            vec3 halfway = SafeNormalize(
                    viewDirection + lightDirection, worldNormal);
            float distribution = DistributionGgx(
                    worldNormal, halfway, roughness);
            float geometry = GeometrySmith(
                    worldNormal, viewDirection, lightDirection, roughness);
            vec3 fresnel = FresnelSchlick(
                    max(dot(halfway, viewDirection), 0.0), f0);
            vec3 specular = distribution * geometry * fresnel
                    / max(4.0
                            * max(dot(worldNormal, viewDirection), 0.0)
                            * ndotl,
                            0.001);
            vec3 diffuseWeight = (vec3(1.0) - fresnel)
                    * (1.0 - metallic);
            vec3 radiance = dynamicLightColors[i]
                    * dynamicLightIntensities[i]
                    * atten
                    * coneAtten;
            dynamicDirectDiffuse += diffuseWeight
                    * surfaceRgb
                    * radiance
                    * ndotl;
            dynamicDirectSpecular += specular * radiance * ndotl;
        }
    }

    vec3 staticDirectSpecular = vec3(0.0);
    if (useStaticSpecularLighting != 0) {
        for (int i = 0;
                i < staticSpecularLightCount
                        && i < MAX_STATIC_SPECULAR_LIGHTS;
                ++i) {
            float radius = staticSpecularLightRadii[i];
            vec3 toLight = staticSpecularLightPositions[i]
                    - fragWorldPosition;
            float distanceSq = dot(toLight, toLight);
            if (radius <= 0.0 || distanceSq >= radius * radius) continue;
            float distanceToLight = sqrt(max(distanceSq, 0.0));
            vec3 lightDirection = distanceToLight > 0.0001
                    ? toLight / distanceToLight
                    : worldNormal;
            float ndotl = max(dot(worldNormal, lightDirection), 0.0);
            if (ndotl <= 0.0) continue;
            float atten = clamp(
                    1.0 - distanceToLight / radius, 0.0, 1.0);
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
                        spotDirection, fragmentDirectionFromLight);
                float innerConeCos = staticSpecularLightInnerConeCos[i];
                float outerConeCos = staticSpecularLightOuterConeCos[i];
                coneAtten = abs(innerConeCos - outerConeCos) > 0.0001
                        ? smoothstep(outerConeCos, innerConeCos, coneDot)
                        : step(innerConeCos, coneDot);
            } else if (staticSpecularLightTypes[i] == 2) {
                vec3 rectDirection = SafeNormalize(
                        staticSpecularLightDirections[i],
                        vec3(0.0, -1.0, 0.0));
                float frontDistance = dot(
                        fragWorldPosition - staticSpecularLightPositions[i],
                        rectDirection);
                float startFeather = staticSpecularLightStartFeathers[i];
                coneAtten = startFeather > 0.000001
                        ? smoothstep(0.0, startFeather, frontDistance)
                        : step(0.0, frontDistance);
            }
            if (coneAtten <= 0.0) continue;
            vec3 halfway = SafeNormalize(
                    viewDirection + lightDirection, worldNormal);
            float distribution = DistributionGgx(
                    worldNormal, halfway, roughness);
            float geometry = GeometrySmith(
                    worldNormal, viewDirection, lightDirection, roughness);
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

    vec3 environmentSpecular = vec3(0.0);
    if (hasEnvironment != 0 && environmentSpecularScale > 0.0) {
        vec3 reflected = reflect(-viewDirection, worldNormal);
        if (environmentBoxProjection != 0) {
            float c = cos(-environmentYaw);
            float s = sin(-environmentYaw);
            vec3 origin = fragWorldPosition - environmentInfluenceCenter;
            vec3 localOrigin = vec3(origin.x*c-origin.z*s, origin.y, origin.x*s+origin.z*c);
            vec3 localDirection = vec3(reflected.x*c-reflected.z*s, reflected.y, reflected.x*s+reflected.z*c);
            vec3 safeDirection = mix(vec3(-1.0), vec3(1.0), step(vec3(0.0), localDirection))
                    * max(abs(localDirection), vec3(0.00001));
            vec3 exitPlane = mix(-environmentHalfExtents, environmentHalfExtents, step(vec3(0.0), localDirection));
            vec3 exitDistance = (exitPlane-localOrigin)/safeDirection;
            float distanceToBox = min(exitDistance.x, min(exitDistance.y, exitDistance.z));
            vec3 localHit = localOrigin + localDirection * max(distanceToBox, 0.0);
            vec3 captureOffset = environmentCapturePosition-environmentInfluenceCenter;
            vec3 localCapture = vec3(captureOffset.x*c-captureOffset.z*s, captureOffset.y, captureOffset.x*s+captureOffset.z*c);
            vec3 localLookup = localHit-localCapture;
            c = cos(environmentYaw); s = sin(environmentYaw);
            reflected = normalize(vec3(localLookup.x*c-localLookup.z*s, localLookup.y, localLookup.x*s+localLookup.z*c));
        }
        vec3 environment = textureLod(
                environmentTexture, reflected, roughness * max(environmentMaxLod, 0.0)).rgb;
        vec2 environmentBrdf = EnvironmentBrdfApprox(
                roughness,
                max(dot(worldNormal, viewDirection), 0.0));
        environmentSpecular = environment
                * (f0 * environmentBrdf.x + environmentBrdf.y)
                * environmentExposure
                * environmentSpecularScale;
    }

    vec3 outputRgb = indirectDiffuse
            + dynamicDirectDiffuse
            + dynamicDirectSpecular
            + staticDirectSpecular
            + environmentSpecular;
    if (pbrDiagnosticMode == 1) outputRgb = surfaceRgb;
    else if (pbrDiagnosticMode == 2) outputRgb = dynamicDirectDiffuse;
    else if (pbrDiagnosticMode == 3) {
        outputRgb = dynamicDirectSpecular + staticDirectSpecular;
    }
    else if (pbrDiagnosticMode == 4) outputRgb = indirectDiffuse;
    else if (pbrDiagnosticMode == 5) outputRgb = environmentSpecular;
    else if (pbrDiagnosticMode == 6) outputRgb = vec3(0.0);
    else if (pbrDiagnosticMode == 7) outputRgb = vec3(1.0);
    else if (pbrDiagnosticMode == 8) {
        outputRgb = vec3(metallic, roughness, 0.0);
    }
    else if (pbrDiagnosticMode == 9) {
        outputRgb = worldNormal * 0.5 + 0.5;
    }
    else if (pbrDiagnosticMode == 10) {
        outputRgb = hasNormalMap != 0
                ? tangentNormalSample
                : vec3(1.0, 0.0, 1.0);
    }
    if (pbrDiagnosticMode == 0) {
        outputRgb = ApplySectorFog(
                outputRgb,
                staticProbeLighting,
                fragWorldPosition);
    }
    finalColor = vec4(
            StoreFiniteHalfRadiance(outputRgb),
            clamp(sampled.a * doorTint.a, 0.0, 1.0));
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

int GetShaderLocationArrayElement(Shader shader, const char* name, std::size_t index)
{
    const std::string indexedName = std::string(name) + "[" + std::to_string(index) + "]";
    return GetShaderLocation(shader, indexedName.c_str());
}

Mesh CreateDoorSlabMesh(const SectorDoorSlabMeshData& data)
{
    Mesh mesh = {};
    if (data.vertices.empty()
            || data.vertices.size() > static_cast<size_t>(std::numeric_limits<int>::max())
            || data.indices.empty()
            || data.indices.size() % 3u != 0u
            || data.indices.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return mesh;
    }

    mesh.vertexCount = static_cast<int>(data.vertices.size());
    mesh.triangleCount = static_cast<int>(data.indices.size() / 3u);
    mesh.vertices = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.normals = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 3 * sizeof(float))));
    mesh.texcoords = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 2 * sizeof(float))));
    mesh.tangents = static_cast<float*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(float))));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(unsigned char))));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(static_cast<unsigned int>(data.indices.size() * sizeof(unsigned short))));

    if (mesh.vertices == nullptr
            || mesh.normals == nullptr
            || mesh.texcoords == nullptr
            || mesh.tangents == nullptr
            || mesh.colors == nullptr
            || mesh.indices == nullptr) {
        std::fprintf(stderr, "[SectorDemo ERROR] Failed to allocate door slab mesh data\n");
        UnloadMesh(mesh);
        return Mesh{};
    }

    for (int i = 0; i < mesh.vertexCount; ++i) {
        const SectorDoorSlabMeshVertex& vertex = data.vertices[static_cast<size_t>(i)];
        mesh.vertices[i * 3 + 0] = vertex.position.x;
        mesh.vertices[i * 3 + 1] = vertex.position.y;
        mesh.vertices[i * 3 + 2] = vertex.position.z;
        mesh.normals[i * 3 + 0] = vertex.normal.x;
        mesh.normals[i * 3 + 1] = vertex.normal.y;
        mesh.normals[i * 3 + 2] = vertex.normal.z;
        mesh.texcoords[i * 2 + 0] = vertex.uv.x;
        mesh.texcoords[i * 2 + 1] = vertex.uv.y;
        mesh.tangents[i * 4 + 0] = 1.0f;
        mesh.tangents[i * 4 + 1] = 1.0f;
        mesh.tangents[i * 4 + 2] = 1.0f;
        mesh.tangents[i * 4 + 3] = 1.0f;
        mesh.colors[i * 4 + 0] = vertex.color.r;
        mesh.colors[i * 4 + 1] = vertex.color.g;
        mesh.colors[i * 4 + 2] = vertex.color.b;
        mesh.colors[i * 4 + 3] = vertex.color.a;
    }

    for (size_t i = 0; i < data.indices.size(); ++i) {
        mesh.indices[i] = data.indices[i];
    }

    UploadMesh(&mesh, false);
    return mesh;
}

void AppendDoorRenderDebugText(std::string& renderDebugText, const std::string& doorText)
{
    const size_t existing = renderDebugText.find(" | doors:");
    if (existing != std::string::npos) {
        renderDebugText.erase(existing);
    }
    if (!doorText.empty() && !renderDebugText.empty()) {
        renderDebugText += " | " + doorText;
    }
}

} // namespace

void SectorDoorRenderer::ReserveRuntimeDoorCapacity(size_t capacity)
{
    doorMeshCache.reserve(capacity);
    runtimeDoorShadowCasters.clear();
    runtimeDoorShadowCasters.reserve(capacity);
    runtimeDoorModelShadowCasters.clear();
    runtimeDoorModelShadowCasters.reserve(capacity * 2);
}

void SectorDoorRenderer::ResetOpaqueShaderLocations()
{
    opaqueShaderLocations = SectorDoorOpaqueShaderLocations{};
}

bool SectorDoorRenderer::LoadOpaqueResources()
{
    opaqueShader = LoadShaderFromMemory(SectorDoorOpaqueVs, SectorDoorOpaqueFs);
    if (opaqueShader.id == 0) {
        opaqueShader = Shader{};
        ResetOpaqueShaderLocations();
        opaqueShaderLoaded = false;
        return false;
    }

    opaqueShader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(opaqueShader, "vertexPosition");
    opaqueShader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocationAttrib(opaqueShader, "vertexNormal");
    opaqueShader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(opaqueShader, "vertexTexCoord");
    opaqueShader.locs[SHADER_LOC_VERTEX_COLOR] = GetShaderLocationAttrib(opaqueShader, "vertexColor");
    opaqueShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(opaqueShader, "mvp");
    opaqueShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(opaqueShader, "matModel");
    opaqueShader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(opaqueShader, "matNormal");
    opaqueShader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(opaqueShader, "texture0");
    opaqueShader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(opaqueShader, "normalTexture");
    opaqueShader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(opaqueShader, "shadowMap0");
    opaqueShader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(opaqueShader, "shadowMap1");
    opaqueShader.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(
            opaqueShader, "environmentTexture");
    opaqueShaderLocations.texture = opaqueShader.locs[SHADER_LOC_MAP_DIFFUSE];
    opaqueShaderLocations.normalTexture = opaqueShader.locs[SHADER_LOC_MAP_NORMAL];
    opaqueShaderLocations.hasNormalMap = GetShaderLocation(opaqueShader, "hasNormalMap");
    opaqueShaderLocations.normalStrength = GetShaderLocation(opaqueShader, "normalStrength");
    opaqueShaderLocations.metallicFactor = GetShaderLocation(opaqueShader, "metallicFactor");
    opaqueShaderLocations.roughnessFactor = GetShaderLocation(opaqueShader, "roughnessFactor");
    opaqueShaderLocations.cameraPosition = GetShaderLocation(opaqueShader, "cameraPosition");
    opaqueShaderLocations.hasEnvironment = GetShaderLocation(opaqueShader, "hasEnvironment");
    opaqueShaderLocations.environmentExposure = GetShaderLocation(opaqueShader, "environmentExposure");
    opaqueShaderLocations.indirectDiffuseScale = GetShaderLocation(opaqueShader, "indirectDiffuseScale");
    opaqueShaderLocations.environmentSpecularScale = GetShaderLocation(opaqueShader, "environmentSpecularScale");
    opaqueShaderLocations.environmentBoxProjection = GetShaderLocation(opaqueShader, "environmentBoxProjection");
    opaqueShaderLocations.environmentCapturePosition = GetShaderLocation(opaqueShader, "environmentCapturePosition");
    opaqueShaderLocations.environmentInfluenceCenter = GetShaderLocation(opaqueShader, "environmentInfluenceCenter");
    opaqueShaderLocations.environmentHalfExtents = GetShaderLocation(opaqueShader, "environmentHalfExtents");
    opaqueShaderLocations.environmentYaw = GetShaderLocation(opaqueShader, "environmentYaw");
    opaqueShaderLocations.environmentMaxLod = GetShaderLocation(opaqueShader, "environmentMaxLod");
    opaqueShaderLocations.pbrDiagnosticMode = GetShaderLocation(opaqueShader, "pbrDiagnosticMode");
    opaqueShaderLocations.useObjectAmbientCube = GetShaderLocation(
            opaqueShader, "useObjectAmbientCube");
    opaqueShaderLocations.objectAmbientCube = GetShaderLocationArrayBase(
            opaqueShader, "objectAmbientCube");
    opaqueShaderLocations.useStaticSpecularLighting = GetShaderLocation(
            opaqueShader, "useStaticSpecularLighting");
    opaqueShaderLocations.dynamicLightCount = GetShaderLocation(opaqueShader, "dynamicLightCount");
    opaqueShaderLocations.dynamicLightPositions = GetShaderLocationArrayBase(opaqueShader, "dynamicLightPositions");
    opaqueShaderLocations.dynamicLightColors = GetShaderLocationArrayBase(opaqueShader, "dynamicLightColors");
    opaqueShaderLocations.dynamicLightRadii = GetShaderLocationArrayBase(opaqueShader, "dynamicLightRadii");
    opaqueShaderLocations.dynamicLightIntensities = GetShaderLocationArrayBase(opaqueShader, "dynamicLightIntensities");
    opaqueShaderLocations.dynamicLightTypes = GetShaderLocationArrayBase(opaqueShader, "dynamicLightTypes");
    opaqueShaderLocations.dynamicLightDirections = GetShaderLocationArrayBase(opaqueShader, "dynamicLightDirections");
    opaqueShaderLocations.dynamicLightInnerConeCos = GetShaderLocationArrayBase(opaqueShader, "dynamicLightInnerConeCos");
    opaqueShaderLocations.dynamicLightOuterConeCos = GetShaderLocationArrayBase(opaqueShader, "dynamicLightOuterConeCos");
    opaqueShaderLocations.dynamicLightSpotShadowRight = GetShaderLocationArrayBase(
            opaqueShader, "dynamicLightSpotShadowRight");
    opaqueShaderLocations.dynamicLightSpotShadowProjection = GetShaderLocationArrayBase(
            opaqueShader, "dynamicLightSpotShadowProjection");
    opaqueShaderLocations.dynamicLightProfiles = GetShaderLocationArrayBase(
            opaqueShader, "dynamicLightProfiles");
    opaqueShaderLocations.dynamicLightProfileParameters = GetShaderLocationArrayBase(
            opaqueShader, "dynamicLightProfileParameters");
    opaqueShaderLocations.flashlightCookie = GetShaderLocation(
            opaqueShader, "flashlightCookie");
    opaqueShaderLocations.hasPointShadows = GetShaderLocation(
            opaqueShader, "hasPointShadows");
    opaqueShaderLocations.dynamicLightShadowSlots = GetShaderLocationArrayBase(opaqueShader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        opaqueShaderLocations.shadowLightMatrices[i] =
                GetShaderLocationArrayElement(opaqueShader, "shadowLightMatrices", i);
    }
    opaqueShaderLocations.shadowBias = GetShaderLocationArrayBase(opaqueShader, "shadowBias");
    opaqueShaderLocations.shadowStrength = GetShaderLocationArrayBase(opaqueShader, "shadowStrength");
    opaqueShaderLocations.shadowSoftness = GetShaderLocationArrayBase(opaqueShader, "shadowSoftness");
    opaqueShaderLocations.shadowAtlasTilesPerRow = GetShaderLocation(opaqueShader, "shadowAtlasTilesPerRow");
    opaqueShaderLocations.tint = GetShaderLocation(opaqueShader, "doorTint");
    opaqueShaderLocations.staticSpecular = GetSectorStaticSpecularShaderLocations(
            opaqueShader);
    opaqueShaderLocations.fog = GetSectorFogShaderLocations(opaqueShader);
    opaqueShaderLoaded = true;

    opaqueMaterial = LoadMaterialDefault();
    opaqueDefaultMaterialTexture = opaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    opaqueMaterial.shader = opaqueShader;
    opaqueMaterialLoaded = true;
    return true;
}

void SectorDoorRenderer::ShutdownOpaqueResources()
{
    if (opaqueMaterialLoaded) {
        opaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = opaqueDefaultMaterialTexture;
        opaqueMaterial.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
        opaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
        opaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
        opaqueMaterial.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
        UnloadMaterial(opaqueMaterial);
        opaqueMaterial = Material{};
        opaqueDefaultMaterialTexture = Texture2D{};
        opaqueShader = Shader{};
        ResetOpaqueShaderLocations();
        opaqueMaterialLoaded = false;
        opaqueShaderLoaded = false;
    }
}

void SectorDoorRenderer::PrepareRuntimeDoorMeshes(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld)
{
    for (auto& entry : doorMeshCache) {
        entry.second.seenThisFrame = false;
    }
    runtimeDoorShadowCasters.clear();
    runtimeDoorModelShadowCasters.clear();

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender>(
            [this, &assets, &runtimeObjectWorld](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render) {
                if (runtimeObjectWorld.Has<SectorDoorModelRender>(entity)) {
                    const SectorDoorModelRender& model =
                            runtimeObjectWorld.Get<SectorDoorModelRender>(entity);
                    const SectorDoorModelDrawPolicy policy =
                            ResolveSectorDoorModelDrawPolicy(
                                    model,
                                    assets.GetModelAsset(model.leafModel) != nullptr,
                                    assets.GetModelAsset(model.frameModel) != nullptr);
                    if (!policy.drawProcedural) {
                        return;
                    }
                }
                if (!AppendSectorDoorShadowCaster(
                            entity,
                            transform,
                            object,
                            door,
                            anchor,
                            render,
                            runtimeDoorShadowCasters)) {
                    return;
                }

                DoorMeshCacheEntry& cacheEntry = doorMeshCache[door.placedObjectId];
                cacheEntry.seenThisFrame = true;
                const bool meshDirty = cacheEntry.mesh.vertexCount <= 0
                        || cacheEntry.width != render.width
                        || cacheEntry.height != render.height
                        || cacheEntry.thickness != render.thickness
                        || !SameSectorDoorFaceUvSet(cacheEntry.faceUvs, render.faceUvs);
                if (meshDirty) {
                    if (cacheEntry.mesh.vertexCount > 0) {
                        UnloadMesh(cacheEntry.mesh);
                    }
                    cacheEntry.meshData = BuildSectorDoorSlabMeshData(render);
                    cacheEntry.mesh = CreateDoorSlabMesh(cacheEntry.meshData);
                    cacheEntry.width = render.width;
                    cacheEntry.height = render.height;
                    cacheEntry.thickness = render.thickness;
                    cacheEntry.faceUvs = render.faceUvs;
                    cacheEntry.staticLightingValid = false;
                }
            });

    CollectSectorDoorModelShadowCasters(
            runtimeObjectWorld, assets, runtimeDoorModelShadowCasters);

    RefreshSectorDoorShadowCasterRevision(
            shadowCasterRevisionState,
            runtimeDoorShadowCasters,
            runtimeDoorModelShadowCasters);

    for (auto it = doorMeshCache.begin(); it != doorMeshCache.end();) {
        if (!it->second.seenThisFrame) {
            if (it->second.mesh.vertexCount > 0) {
                UnloadMesh(it->second.mesh);
            }
            it = doorMeshCache.erase(it);
        } else {
            ++it;
        }
    }
}

void SectorDoorRenderer::Draw(const SectorDoorDrawContext& context)
{
    if (!IsOpaqueReady()) {
        renderStats = {};
        if (context.renderDebugText != nullptr) {
            AppendDoorRenderDebugText(*context.renderDebugText, "doors: shader unavailable");
        }
        return;
    }
    if (context.assets == nullptr || context.runtimeObjectWorld == nullptr) {
        renderStats = {};
        return;
    }

    Material& doorOpaqueMaterial = OpaqueMaterial();
    const Texture2D& doorOpaqueDefaultMaterialTexture = OpaqueDefaultMaterialTexture();
    const SectorDoorOpaqueShaderLocations& doorOpaqueLocations = OpaqueShaderLocations();
    PrepareRuntimeDoorMeshes(*context.assets, *context.runtimeObjectWorld);

    size_t consideredCount = 0;
    size_t drawnCount = 0;
    size_t skippedCount = 0;
    const SectorBakedObjectLightProbeRuntimeData emptyObjectLightProbes;
    const SectorBakedObjectLightProbeRuntimeData& objectLightProbes =
            context.lighting.objectLightProbes != nullptr
            ? *context.lighting.objectLightProbes
            : emptyObjectLightProbes;
    const SectorStaticSpecularLightState emptyStaticSpecularLights;
    const SectorStaticSpecularLightState& staticSpecularLights =
            context.staticSpecularLights != nullptr
            ? *context.staticSpecularLights
            : emptyStaticSpecularLights;
    const RuntimePortalVisibilityResult emptyVisibility;
    const RuntimePortalVisibilityResult& visibility =
            context.visibility != nullptr
            ? *context.visibility
            : emptyVisibility;
    const SectorPbrContributionSettings pbr =
            NormalizeSectorPbrContributionSettings(context.pbr);
    const bool environmentActive = context.environment != nullptr
            && context.environment->id != 0
            && pbr.worldEnvironmentSpecularScale > 0.0f;

    rlDisableColorBlend();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
    SectorDynamicLightShaderLocations dynamicLightLocations;
    dynamicLightLocations.dynamicLightCount = doorOpaqueLocations.dynamicLightCount;
    dynamicLightLocations.dynamicLightPositions = doorOpaqueLocations.dynamicLightPositions;
    dynamicLightLocations.dynamicLightColors = doorOpaqueLocations.dynamicLightColors;
    dynamicLightLocations.dynamicLightRadii = doorOpaqueLocations.dynamicLightRadii;
    dynamicLightLocations.dynamicLightIntensities = doorOpaqueLocations.dynamicLightIntensities;
    dynamicLightLocations.dynamicLightTypes = doorOpaqueLocations.dynamicLightTypes;
    dynamicLightLocations.dynamicLightDirections = doorOpaqueLocations.dynamicLightDirections;
    dynamicLightLocations.dynamicLightInnerConeCos = doorOpaqueLocations.dynamicLightInnerConeCos;
    dynamicLightLocations.dynamicLightOuterConeCos = doorOpaqueLocations.dynamicLightOuterConeCos;
    dynamicLightLocations.dynamicLightSpotShadowRight =
            doorOpaqueLocations.dynamicLightSpotShadowRight;
    dynamicLightLocations.dynamicLightSpotShadowProjection =
            doorOpaqueLocations.dynamicLightSpotShadowProjection;
    dynamicLightLocations.dynamicLightProfiles =
            doorOpaqueLocations.dynamicLightProfiles;
    dynamicLightLocations.dynamicLightProfileParameters =
            doorOpaqueLocations.dynamicLightProfileParameters;
    dynamicLightLocations.flashlightCookie =
            doorOpaqueLocations.flashlightCookie;
    dynamicLightLocations.hasPointShadows = doorOpaqueLocations.hasPointShadows;
    const std::vector<SectorPreviewDynamicPointLightUniform> emptyDynamicLights;
    const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights =
            context.dynamicLighting.selectedLights != nullptr
            ? *context.dynamicLighting.selectedLights
            : emptyDynamicLights;
    UploadSectorRendererDynamicPointLights(
            doorOpaqueMaterial.shader,
            dynamicLightLocations,
            context.dynamicLighting.enabled,
            context.dynamicLighting.runtimeSeconds,
            selectedDynamicLights);
    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = doorOpaqueLocations.dynamicLightShadowSlots;
    shadowLocations.shadowLightMatrices = doorOpaqueLocations.shadowLightMatrices;
    shadowLocations.shadowBias = doorOpaqueLocations.shadowBias;
    shadowLocations.shadowStrength = doorOpaqueLocations.shadowStrength;
    shadowLocations.shadowSoftness = doorOpaqueLocations.shadowSoftness;
    shadowLocations.shadowAtlasTilesPerRow = doorOpaqueLocations.shadowAtlasTilesPerRow;
    UploadSectorRendererDynamicSpotLightShadowUniforms(
            doorOpaqueMaterial.shader,
            shadowLocations,
            context.dynamicLighting.shadowUniforms);
    UploadSectorFogShaderValues(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.fog,
            context.fog);
    const Texture2D* shadowMap0 = context.dynamicLighting.shadowMaps.shadowMap0;
    const Texture2D* shadowMap1 = context.dynamicLighting.shadowMaps.shadowMap1;
    doorOpaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = shadowMap0 != nullptr ? *shadowMap0 : Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = shadowMap1 != nullptr ? *shadowMap1 : Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_CUBEMAP].texture = environmentActive
            ? *context.environment
            : Texture2D{};
    if (doorOpaqueLocations.texture >= 0) {
        const int diffuseTextureUnit = 0;
        SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueLocations.texture, &diffuseTextureUnit, SHADER_UNIFORM_INT);
    }
    if (doorOpaqueLocations.normalTexture >= 0) {
        const int normalTextureUnit = MATERIAL_MAP_NORMAL;
        SetShaderValue(
                doorOpaqueMaterial.shader,
                doorOpaqueLocations.normalTexture,
                &normalTextureUnit,
                SHADER_UNIFORM_INT);
    }
    if (doorOpaqueLocations.cameraPosition >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.cameraPosition,
            &context.camera.position,
            SHADER_UNIFORM_VEC3);
    const int hasEnvironment = environmentActive ? 1 : 0;
    if (doorOpaqueLocations.hasEnvironment >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.hasEnvironment,
            &hasEnvironment,
            SHADER_UNIFORM_INT);
    const float environmentExposure = SanitizeSectorPbrNonnegative(
            context.environmentExposure);
    if (doorOpaqueLocations.environmentExposure >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.environmentExposure,
            &environmentExposure,
            SHADER_UNIFORM_FLOAT);
    if (doorOpaqueLocations.indirectDiffuseScale >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.indirectDiffuseScale,
            &pbr.worldIndirectDiffuseScale,
            SHADER_UNIFORM_FLOAT);
    if (doorOpaqueLocations.environmentSpecularScale >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.environmentSpecularScale,
            &pbr.worldEnvironmentSpecularScale,
            SHADER_UNIFORM_FLOAT);
    const int boxProjection = context.environmentBoxProjection ? 1 : 0;
    if (doorOpaqueLocations.environmentBoxProjection >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentBoxProjection,
            &boxProjection, SHADER_UNIFORM_INT);
    if (doorOpaqueLocations.environmentCapturePosition >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentCapturePosition,
            &context.environmentCapturePosition, SHADER_UNIFORM_VEC3);
    if (doorOpaqueLocations.environmentInfluenceCenter >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentInfluenceCenter,
            &context.environmentInfluenceCenter, SHADER_UNIFORM_VEC3);
    if (doorOpaqueLocations.environmentHalfExtents >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentHalfExtents,
            &context.environmentHalfExtents, SHADER_UNIFORM_VEC3);
    if (doorOpaqueLocations.environmentYaw >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentYaw,
            &context.environmentYaw, SHADER_UNIFORM_FLOAT);
    if (doorOpaqueLocations.environmentMaxLod >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentMaxLod,
            &context.environmentMaxLod, SHADER_UNIFORM_FLOAT);
    const int pbrDiagnosticMode = static_cast<int>(pbr.diagnosticMode);
    if (doorOpaqueLocations.pbrDiagnosticMode >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.pbrDiagnosticMode,
            &pbrDiagnosticMode,
            SHADER_UNIFORM_INT);
    const int useObjectAmbientCube = 0;
    if (doorOpaqueLocations.useObjectAmbientCube >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.useObjectAmbientCube,
            &useObjectAmbientCube,
            SHADER_UNIFORM_INT);

    context.runtimeObjectWorld->ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender>(
            [this,
             &context,
             &consideredCount,
             &drawnCount,
             &skippedCount,
             &objectLightProbes,
             &staticSpecularLights,
             &visibility,
             &doorOpaqueMaterial,
             &doorOpaqueLocations](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render) {
                ++consideredCount;
                if (!object.visible || !door.enabled || !render.visible) {
                    ++skippedCount;
                    return;
                }
                if (context.runtimeObjectWorld->Has<SectorDoorModelRender>(entity)) {
                    const SectorDoorModelRender& model =
                            context.runtimeObjectWorld->Get<SectorDoorModelRender>(entity);
                    const SectorDoorModelDrawPolicy policy =
                            ResolveSectorDoorModelDrawPolicy(
                                    model,
                                    context.assets->GetModelAsset(model.leafModel) != nullptr,
                                    context.assets->GetModelAsset(model.frameModel) != nullptr);
                    if (!policy.drawProcedural) {
                        ++skippedCount;
                        return;
                    }
                }
                if (render.width <= 0.0f || render.height <= 0.0f || render.thickness <= 0.0f) {
                    ++skippedCount;
                    return;
                }

                SectorDoorResolvedMaterial resolvedMaterial;
                if (!render.materialId.empty()
                        && context.materialResolver.resolve != nullptr) {
                    resolvedMaterial = context.materialResolver.resolve(
                            context.materialResolver.userData,
                            *context.assets,
                            render.materialId);
                }
                if (resolvedMaterial.albedo == nullptr) {
                    resolvedMaterial.albedo = context.defaultMaterialTexture != nullptr
                            ? context.defaultMaterialTexture
                            : &opaqueDefaultMaterialTexture;
                }
                if (resolvedMaterial.albedo == nullptr
                        || resolvedMaterial.albedo->id == 0) {
                    ++skippedCount;
                    return;
                }
                const bool hasNormalMap = resolvedMaterial.normal != nullptr
                        && resolvedMaterial.normal->id != 0;
                resolvedMaterial.normalStrength = std::isfinite(
                            resolvedMaterial.normalStrength)
                        ? std::clamp(resolvedMaterial.normalStrength, 0.0f, 1.0f)
                        : 1.0f;
                resolvedMaterial.metallicFactor = std::isfinite(
                            resolvedMaterial.metallicFactor)
                        ? std::clamp(resolvedMaterial.metallicFactor, 0.0f, 1.0f)
                        : 0.0f;
                resolvedMaterial.roughnessFactor = std::isfinite(
                            resolvedMaterial.roughnessFactor)
                        ? std::clamp(resolvedMaterial.roughnessFactor, 0.0f, 1.0f)
                        : 0.8f;

                DoorMeshCacheEntry* cacheEntry = FindMutableDoorMesh(door.placedObjectId);
                if (cacheEntry == nullptr || cacheEntry->mesh.vertexCount <= 0) {
                    ++skippedCount;
                    return;
                }

                const Matrix doorModel = BuildSectorDoorSlabModelMatrix(
                        transform,
                        anchor,
                        render);
                const bool staticLightingDirty = !cacheEntry->staticLightingValid
                        || cacheEntry->staticLightingSectorId != object.currentSectorId
                        || cacheEntry->staticLightingRevision != context.lighting.revision
                        || !SameMatrixExact(cacheEntry->staticLightingModel, doorModel);
                if (staticLightingDirty && !BuildSectorDoorStaticLightingColors(
                            cacheEntry->meshData,
                            transform,
                            object,
                            anchor,
                            render,
                            objectLightProbes,
                            context.lighting.mapForFallback,
                            cacheEntry->staticLightingValues)) {
                    cacheEntry->staticLightingValues.assign(
                            static_cast<size_t>(cacheEntry->mesh.vertexCount),
                            Vector3{1.0f, 1.0f, 1.0f});
                }
                if (staticLightingDirty
                        && cacheEntry->mesh.tangents != nullptr
                        && cacheEntry->staticLightingValues.size() == static_cast<size_t>(cacheEntry->mesh.vertexCount)) {
                    for (int i = 0; i < cacheEntry->mesh.vertexCount; ++i) {
                        const Vector3 lighting = cacheEntry->staticLightingValues[static_cast<size_t>(i)];
                        cacheEntry->mesh.tangents[i * 4 + 0] = lighting.x;
                        cacheEntry->mesh.tangents[i * 4 + 1] = lighting.y;
                        cacheEntry->mesh.tangents[i * 4 + 2] = lighting.z;
                        cacheEntry->mesh.tangents[i * 4 + 3] = 1.0f;
                    }
                    UpdateMeshBuffer(
                            cacheEntry->mesh,
                            RL_DEFAULT_SHADER_ATTRIB_LOCATION_TANGENT,
                            cacheEntry->mesh.tangents,
                            cacheEntry->mesh.vertexCount * 4 * static_cast<int>(sizeof(float)),
                            0);
                }
                if (staticLightingDirty) {
                    cacheEntry->staticLightingModel = doorModel;
                    cacheEntry->staticLightingSectorId = object.currentSectorId;
                    cacheEntry->staticLightingRevision = context.lighting.revision;
                    cacheEntry->staticLightingValid = true;
                }

                if (doorOpaqueLocations.tint >= 0) {
                    const Vector4 tint = engine::SrgbColorBytesToLinearSceneRgba(
                            render.tint);
                    SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueLocations.tint, &tint, SHADER_UNIFORM_VEC4);
                }

                const int hasNormalMapValue = hasNormalMap ? 1 : 0;
                if (doorOpaqueLocations.hasNormalMap >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.hasNormalMap,
                        &hasNormalMapValue,
                        SHADER_UNIFORM_INT);
                if (doorOpaqueLocations.normalStrength >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.normalStrength,
                        &resolvedMaterial.normalStrength,
                        SHADER_UNIFORM_FLOAT);
                if (doorOpaqueLocations.metallicFactor >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.metallicFactor,
                        &resolvedMaterial.metallicFactor,
                        SHADER_UNIFORM_FLOAT);
                if (doorOpaqueLocations.roughnessFactor >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.roughnessFactor,
                        &resolvedMaterial.roughnessFactor,
                        SHADER_UNIFORM_FLOAT);

                const int receiverSectorId = object.currentSectorId > 0
                        ? object.currentSectorId
                        : (anchor.frontSectorId > 0
                                ? anchor.frontSectorId
                                : anchor.backSectorId);
                SectorReceiverBounds receiverBounds{
                        receiverSectorId,
                        transform.position,
                        transform.position};
                BuildSectorDoorReceiverBounds(
                        transform,
                        object,
                        door,
                        anchor,
                        render,
                        receiverSectorId,
                        receiverBounds);
                const SectorStaticSpecularLightContext staticSpecularContext =
                        SelectSectorStaticSpecularLights(
                                staticSpecularLights,
                                receiverBounds,
                                receiverSectorId,
                                visibility,
                                context.staticSpecularEligible);
                UploadSectorStaticSpecularLights(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.staticSpecular,
                        staticSpecularContext);
                const int useStaticSpecularLighting =
                        staticSpecularContext.lightCount > 0 ? 1 : 0;
                if (doorOpaqueLocations.useStaticSpecularLighting >= 0) {
                    SetShaderValue(
                            doorOpaqueMaterial.shader,
                            doorOpaqueLocations.useStaticSpecularLighting,
                            &useStaticSpecularLighting,
                            SHADER_UNIFORM_INT);
                }

                doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
                        *resolvedMaterial.albedo;
                doorOpaqueMaterial.maps[MATERIAL_MAP_NORMAL].texture = hasNormalMap
                        ? *resolvedMaterial.normal
                        : Texture2D{};
                doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                DrawMesh(
                        cacheEntry->mesh,
                        doorOpaqueMaterial,
                        doorModel);
                ++drawnCount;
            });

    doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = doorOpaqueDefaultMaterialTexture;
    doorOpaqueMaterial.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    renderStats.considered = consideredCount;
    renderStats.drawn = drawnCount;
    renderStats.skipped = skippedCount;
    if (context.renderDebugText != nullptr) {
        AppendDoorRenderDebugText(
                *context.renderDebugText,
                "doors: "
                        + std::to_string(drawnCount)
                        + " drawn / "
                        + std::to_string(consideredCount)
                        + " considered, "
                        + std::to_string(skippedCount)
                        + " skipped");
    }
}

void SectorDoorRenderer::PrepareShadowRenderContext(
        SectorDynamicSpotLightShadowRenderContext& context,
        engine::World* runtimeObjectWorld)
{
    if (runtimeObjectWorld != nullptr) {
        if (context.assets != nullptr) {
            PrepareRuntimeDoorMeshes(*context.assets, *runtimeObjectWorld);
        } else {
            ClearPreparedShadowCasters();
        }
    } else {
        ClearPreparedShadowCasters();
    }

    context.doorShadowCasters = &ShadowCasters();
    context.doorModelShadowCasters = &runtimeDoorModelShadowCasters;
    context.doorShadowCasterRevision = shadowCasterRevisionState.revision;
    context.doorMeshResolverUserData = this;
    context.doorMeshResolver = &SectorDoorRenderer::ResolveDoorShadowCasterMesh;
}

void SectorDoorRenderer::ClearPreparedShadowCasters()
{
    runtimeDoorShadowCasters.clear();
    runtimeDoorModelShadowCasters.clear();
    RefreshSectorDoorShadowCasterRevision(
            shadowCasterRevisionState,
            runtimeDoorShadowCasters,
            runtimeDoorModelShadowCasters);
}

void SectorDoorRenderer::UnloadDoorMeshes()
{
    for (auto& entry : doorMeshCache) {
        if (entry.second.mesh.vertexCount > 0) {
            UnloadMesh(entry.second.mesh);
            entry.second.mesh = Mesh{};
        }
    }
    doorMeshCache.clear();
    runtimeDoorShadowCasters.clear();
    runtimeDoorModelShadowCasters.clear();
}

SectorDoorRenderer::DoorMeshCacheEntry* SectorDoorRenderer::FindMutableDoorMesh(int placedObjectId)
{
    auto cacheIt = doorMeshCache.find(placedObjectId);
    if (cacheIt == doorMeshCache.end()) {
        return nullptr;
    }
    return &cacheIt->second;
}

const SectorDoorRenderer::DoorMeshCacheEntry* SectorDoorRenderer::FindDoorMesh(int placedObjectId) const
{
    const auto cacheIt = doorMeshCache.find(placedObjectId);
    if (cacheIt == doorMeshCache.end()) {
        return nullptr;
    }
    return &cacheIt->second;
}

const Mesh* SectorDoorRenderer::ResolveDoorShadowCasterMesh(
        const SectorDoorShadowCaster& caster,
        float& outWidth,
        float& outHeight) const
{
    const DoorMeshCacheEntry* cacheEntry = FindDoorMesh(caster.placedObjectId);
    if (cacheEntry == nullptr || cacheEntry->mesh.vertexCount <= 0) {
        return nullptr;
    }

    outWidth = cacheEntry->width;
    outHeight = cacheEntry->height;
    return &cacheEntry->mesh;
}

const Mesh* SectorDoorRenderer::ResolveDoorShadowCasterMesh(
        void* userData,
        const SectorDoorShadowCaster& caster,
        float& outWidth,
        float& outHeight)
{
    const SectorDoorRenderer* renderer = static_cast<const SectorDoorRenderer*>(userData);
    if (renderer == nullptr) {
        return nullptr;
    }
    return renderer->ResolveDoorShadowCasterMesh(caster, outWidth, outHeight);
}

} // namespace game
