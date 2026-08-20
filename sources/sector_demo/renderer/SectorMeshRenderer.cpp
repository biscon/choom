#include "sector_demo/renderer/SectorMeshRenderer.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorAssetPaths.h"
#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorMeshBuilder.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorSkyCylinder.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <external/glad.h>
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace game {

namespace {

constexpr float DefaultVisibilityDebugAspect = 16.0f / 9.0f;
constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;

const char* HdrCompositeVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragUv;
uniform mat4 mvp;
void main() { fragUv=vertexTexCoord; gl_Position=mvp*vec4(vertexPosition,1.0); }
)";

const char* HdrCompositeFs = R"(
#version 330
in vec2 fragUv;
out vec4 finalColor;
uniform sampler2D sceneColor;
uniform sampler2D sourceColor;
uniform int compositeMode;
float safeRadianceChannel(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? 65504.0 : 0.0;
    return min(max(value,0.0),65504.0);
}
vec3 safeRadiance(vec3 value) {
    return vec3(safeRadianceChannel(value.r),safeRadianceChannel(value.g),
            safeRadianceChannel(value.b));
}
float safeAlpha(float value) {
    return (isnan(value)||isinf(value))?1.0:clamp(value,0.0,1.0);
}
void main() {
    vec4 source=texture(sourceColor,fragUv);
    if (compositeMode == 1) {
        vec4 scene=texture(sceneColor,fragUv);
        float coverage=isnan(source.a)||isinf(source.a)?0.0:clamp(source.a,0.0,1.0);
        finalColor=vec4(safeRadiance(safeRadiance(scene.rgb)*(1.0-coverage)
                +safeRadiance(source.rgb)),safeAlpha(scene.a));
        return;
    }
    finalColor=vec4(clamp(safeRadiance(source.rgb),vec3(0.0),vec3(65504.0)),safeAlpha(source.a));
}
)";

void AppendBillboardRenderDebugText(std::string& renderDebugText, const std::string& billboardText)
{
    const size_t existing = renderDebugText.find(" | billboards:");
    if (existing != std::string::npos) {
        renderDebugText.erase(existing);
    }
    if (!billboardText.empty() && !renderDebugText.empty()) {
        renderDebugText += " | " + billboardText;
    }
}

Vector2 PreviewYawForwardXZ(float yawRadians)
{
    return Vector2{std::cos(yawRadians), std::sin(yawRadians)};
}

float VisibilityDebugHorizontalFovRadians(const Camera3D& camera, float pitchRadians)
{
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float aspect = screenWidth > 0 && screenHeight > 0
            ? static_cast<float>(screenWidth) / static_cast<float>(screenHeight)
            : DefaultVisibilityDebugAspect;
    const float verticalFovRadians = camera.fovy * DegreesToRadians;
    return ComputeRuntimePortalVisibilityHorizontalFovRadians(
            verticalFovRadians,
            aspect,
            pitchRadians);
}

const char* SectorLightmapVs = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec2 vertexTexCoord2;
in vec4 vertexTangent;
in vec4 vertexColor;

uniform mat4 mvp;

out vec2 fragTexCoord;
out vec2 fragTexCoord2;
out vec2 fragDecalUv;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec4 fragColor;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragTexCoord2 = vertexTexCoord2;
    fragDecalUv = vertexTangent.xy;
    fragWorldPosition = vertexPosition;
    fragWorldNormal = normalize(vertexNormal);
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* SectorLightmapFs = R"(
#version 330
in vec2 fragTexCoord;
in vec2 fragTexCoord2;
in vec2 fragDecalUv;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D decalTexture;
uniform sampler2D normalTexture;
uniform float useLightmap;
uniform float useBakedAmbientOcclusion;
uniform int hasLightmap;
uniform int hasNormalMap;
uniform int alphaTest;
uniform float alphaCutoff;
uniform int hasDecal;
uniform float decalOpacity;
uniform int decalEmissive;
uniform float decalEmissiveStrength;
uniform vec3 decalTint;

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

vec3 SafeNormalize(vec3 value, vec3 fallback)
{
    float lengthSq = dot(value, value);
    return lengthSq > 0.00000001 ? value * inversesqrt(lengthSq) : fallback;
}

vec3 StoreFiniteHalfRadiance(vec3 value)
{
    vec3 result;
    result.r = isnan(value.r) ? 0.0 : (isinf(value.r) ? (value.r > 0.0 ? 65504.0 : 0.0) : min(max(value.r, 0.0), 65504.0));
    result.g = isnan(value.g) ? 0.0 : (isinf(value.g) ? (value.g > 0.0 ? 65504.0 : 0.0) : min(max(value.g, 0.0), 65504.0));
    result.b = isnan(value.b) ? 0.0 : (isinf(value.b) ? (value.b > 0.0 ? 65504.0 : 0.0) : min(max(value.b, 0.0), 65504.0));
    return result;
}

float SampleSpotShadowMap(vec2 atlasTile, float atlasScale, vec2 uv)
{
    return texture(shadowMap0,
            (atlasTile + clamp(uv, vec2(0.001), vec2(0.999)))
                    * atlasScale).r;
}

float SamplePointShadowMap(
        ivec2 atlasTile,
        ivec2 tileResolution,
        vec2 uv,
        out vec2 sampledUv)
{
    ivec2 localTexel = clamp(
            ivec2(floor(clamp(uv, vec2(0.0), vec2(0.999999))
                    * vec2(tileResolution))),
            ivec2(0),
            tileResolution - ivec2(1));
    sampledUv = (vec2(localTexel) + vec2(0.5)) / vec2(tileResolution);
    return texelFetch(shadowMap0,
            atlasTile * tileResolution + localTexel, 0).r;
}

float PointReceiverPlaneDepth(
        int hemisphere,
        vec2 sampleUv,
        vec3 receiverPlaneNormal,
        float planeDistance,
        float lightRadius,
        float fallbackDepth)
{
    vec2 projected = sampleUv * 2.0 - 1.0;
    float projectedRadiusSquared = dot(projected, projected);
    if (projectedRadiusSquared > 1.0) return fallbackDepth;
    float inverseDenominator = 1.0 / (1.0 + projectedRadiusSquared);
    vec3 rayDirection = vec3(
            projected * (2.0 * inverseDenominator),
            (1.0 - projectedRadiusSquared) * inverseDenominator * float(hemisphere));
    float planeDirection = dot(receiverPlaneNormal, rayDirection);
    if (abs(planeDirection) <= 0.000001) return fallbackDepth;
    float radialDepth = planeDistance / planeDirection;
    if (radialDepth <= 0.00001 || radialDepth > lightRadius) return fallbackDepth;
    return radialDepth / lightRadius;
}

vec3 SurfaceNormal(vec3 geometricNormal)
{
    if (hasNormalMap == 0) {
        return geometricNormal;
    }

    vec3 positionDx = dFdx(fragWorldPosition);
    vec3 positionDy = dFdy(fragWorldPosition);
    vec2 uvDx = dFdx(fragTexCoord);
    vec2 uvDy = dFdy(fragTexCoord);
    vec3 positionDyPerpendicular = cross(positionDy, geometricNormal);
    vec3 positionDxPerpendicular = cross(geometricNormal, positionDx);
    vec3 tangent = positionDyPerpendicular * uvDx.x
            + positionDxPerpendicular * uvDy.x;
    vec3 bitangent = positionDyPerpendicular * uvDx.y
            + positionDxPerpendicular * uvDy.y;
    float basisLengthSq = max(dot(tangent, tangent), dot(bitangent, bitangent));
    if (basisLengthSq <= 0.00000001) {
        return geometricNormal;
    }

    float inverseBasisLength = inversesqrt(basisLengthSq);
    vec3 mappedNormal = texture(normalTexture, fragTexCoord).xyz * 2.0 - 1.0;
    return SafeNormalize(
            mat3(
                    tangent * inverseBasisLength,
                    bitangent * inverseBasisLength,
                    geometricNormal) * mappedNormal,
            geometricNormal);
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

    bool pointProjection = dynamicLightTypes[lightIndex] == 0;
    int pointHemisphere = 0;
    vec3 shadowCoord;
    if (pointProjection) {
        vec3 fromLight = worldPosition - dynamicLightPositions[lightIndex];
        float radialDepth = length(fromLight);
        if (radialDepth <= 0.00001) return 1.0;
        pointHemisphere = fromLight.z >= 0.0 ? 1 : -1;
        shadowSlot += pointHemisphere > 0 ? 0 : 1;
        float denominator = radialDepth + abs(fromLight.z);
        shadowCoord = vec3(fromLight.xy / max(denominator, 0.00001) * 0.5 + 0.5,
                radialDepth / max(dynamicLightRadii[lightIndex], 0.00001));
    } else {
        vec3 fromLight = worldPosition - dynamicLightPositions[lightIndex];
        vec3 forward = dynamicLightDirections[lightIndex];
        vec3 right = dynamicLightSpotShadowRight[lightIndex];
        vec3 up = cross(right, forward);
        float forwardDepth = dot(fromLight, forward);
        if (forwardDepth <= 0.05) return 1.0;
        vec2 projection = dynamicLightSpotShadowProjection[lightIndex];
        float depth = projection.y * (1.0 - 0.05 / forwardDepth);
        shadowCoord = vec3(vec2(dot(fromLight,right),dot(fromLight,up))
                * projection.x / max(2.0*forwardDepth,0.00001)+0.5, depth);
    }
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
            shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
            shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        return 1.0;
    }

    int atlasTiles = max(shadowAtlasTilesPerRow, 1);
    ivec2 atlasTile = ivec2(shadowSlot % atlasTiles, shadowSlot / atlasTiles);
    ivec2 tileResolution = textureSize(shadowMap0, 0) / atlasTiles;
    float atlasScale = 1.0 / float(atlasTiles);
    vec2 texelSize = 1.0 / vec2(tileResolution);
    float pointPlaneDistance = pointProjection
            ? dot(worldNormal, worldPosition - dynamicLightPositions[lightIndex])
            : 0.0;
    float pointLightRadius = max(dynamicLightRadii[lightIndex], 0.00001);
    float normalLightDot = max(dot(
            SafeNormalize(worldNormal, vec3(0.0, 1.0, 0.0)),
            SafeNormalize(surfaceToLightDirection, vec3(0.0, 1.0, 0.0))), 0.0);
    float effectiveBias = min(shadowBias[shadowSlot] * (1.0 + (1.0 - normalLightDot) * 2.0), 0.02);
    float softness = clamp(shadowSoftness[shadowSlot], 0.0, 8.0);
    if (softness <= 0.0) {
        vec2 sampledUv;
        float shadowDepth = pointProjection
                ? SamplePointShadowMap(
                        atlasTile, tileResolution, shadowCoord.xy, sampledUv)
                : SampleSpotShadowMap(
                        vec2(atlasTile), atlasScale, shadowCoord.xy);
        if (!pointProjection) sampledUv = shadowCoord.xy;
        float receiverDepth = pointProjection
                ? PointReceiverPlaneDepth(
                        pointHemisphere, sampledUv, worldNormal,
                        pointPlaneDistance, pointLightRadius, shadowCoord.z)
                : shadowCoord.z;
        return receiverDepth - effectiveBias <= shadowDepth ? 1.0 : 0.0;
    }

    vec2 radius = max(0.25, softness) * texelSize;
    float visible = 0.0;
    for (int i = 0; i < 12; ++i) {
        vec2 sampleUv = clamp(shadowCoord.xy + kPoissonDisk[i] * radius, vec2(0.0), vec2(1.0));
        vec2 sampledUv;
        float shadowDepth = pointProjection
                ? SamplePointShadowMap(
                        atlasTile, tileResolution, sampleUv, sampledUv)
                : SampleSpotShadowMap(
                        vec2(atlasTile), atlasScale, sampleUv);
        if (!pointProjection) sampledUv = sampleUv;
        float receiverDepth = pointProjection
                ? PointReceiverPlaneDepth(
                        pointHemisphere, sampledUv, worldNormal,
                        pointPlaneDistance, pointLightRadius, shadowCoord.z)
                : shadowCoord.z;
        visible += receiverDepth - effectiveBias <= shadowDepth ? 1.0 : 0.0;
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
    vec3 receiverPlaneNormal = geometricNormal;
    if (hasPointShadows != 0) {
        receiverPlaneNormal = SafeNormalize(
                cross(dFdx(fragWorldPosition), dFdy(fragWorldPosition)),
                geometricNormal);
        if (dot(receiverPlaneNormal, geometricNormal) < 0.0) {
            receiverPlaneNormal = -receiverPlaneNormal;
        }
    }
    vec4 baseColor = texture(texture0, fragTexCoord);
    if (alphaTest != 0 && baseColor.a < alphaCutoff) {
        discard;
    }
    vec3 surfaceRgb = baseColor.rgb;
    vec3 emissiveDecalRgb = vec3(0.0);
    float emissiveDecalAlpha = 0.0;
    if (hasDecal != 0) {
        float decalMask =
            fragDecalUv.x >= 0.0 && fragDecalUv.x <= 1.0 &&
            fragDecalUv.y >= 0.0 && fragDecalUv.y <= 1.0
                ? 1.0
                : 0.0;
        vec4 decalColor = texture(decalTexture, fragDecalUv);
        float decalAlpha = decalColor.a * decalOpacity * decalMask;
        vec3 decalRgb = decalColor.rgb * decalTint;
        if (decalEmissive != 0) {
            emissiveDecalRgb = decalRgb;
            emissiveDecalAlpha = decalAlpha;
        } else {
            surfaceRgb = mix(baseColor.rgb, decalRgb, decalAlpha);
        }
    }
    vec4 bakedSample = (useLightmap > 0.5 && hasLightmap != 0) ? texture(texture1, fragTexCoord2) : vec4(0.0, 0.0, 0.0, 1.0);
    float aoFactor = (useBakedAmbientOcclusion > 0.5 && hasLightmap != 0) ? bakedSample.a : 1.0;
    vec3 worldNormal = SurfaceNormal(geometricNormal);
    vec3 ambient = fragColor.rgb * aoFactor;
    vec3 bakedDirect = bakedSample.rgb;
    vec3 dynamicDirect = vec3(0.0);
    for (int i = 0; i < dynamicLightCount && i < MAX_DYNAMIC_LIGHTS; ++i) {
        float radius = dynamicLightRadii[i];
        vec3 toLight = dynamicLightPositions[i] - fragWorldPosition;
        float distanceSq = dot(toLight, toLight);
        if (radius > 0.0 && distanceSq < radius * radius) {
            float distanceToLight = sqrt(max(distanceSq, 0.0));
            vec3 lightDirection = distanceToLight > 0.0001 ? toLight / distanceToLight : worldNormal;
            float ndotl = max(dot(worldNormal, lightDirection), 0.0);
            float atten = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
            atten *= atten;
            if (ndotl <= 0.0 || atten <= 0.0
                    || dynamicLightIntensities[i] <= 0.0) continue;
            float coneAtten = 1.0;
            if (dynamicLightTypes[i] == 1) {
                vec3 spotDirection = SafeNormalize(dynamicLightDirections[i], vec3(0.0, -1.0, 0.0));
                vec3 fragmentDirectionFromLight = distanceToLight > 0.0001
                        ? -lightDirection
                        : spotDirection;
                float coneDot = dot(spotDirection, fragmentDirectionFromLight);
                float innerConeCos = dynamicLightInnerConeCos[i];
                float outerConeCos = dynamicLightOuterConeCos[i];
                coneAtten = abs(innerConeCos - outerConeCos) > 0.0001
                        ? smoothstep(outerConeCos, innerConeCos, coneDot)
                        : step(innerConeCos, coneDot);
            }
            if (coneAtten <= 0.0) continue;
            int shadowSlot = dynamicLightShadowSlots[i];
            if (shadowSlot >= 0 && shadowStrength[shadowSlot] > 0.0) {
                float visibility = DynamicLightShadowVisibility(
                        i, shadowSlot, fragWorldPosition, receiverPlaneNormal, lightDirection);
                coneAtten *= mix(1.0, visibility,
                        clamp(shadowStrength[shadowSlot], 0.0, 1.0));
            }
            dynamicDirect += dynamicLightColors[i] * dynamicLightIntensities[i] * atten * ndotl * coneAtten;
        }
    }
    vec3 bakedLighting = max(ambient + bakedDirect, vec3(0.0));
    vec3 lighting = max(bakedLighting + dynamicDirect, vec3(0.0));
    vec3 staticAtmosphericLighting = max(fragColor.rgb + bakedDirect, vec3(0.0));
    vec3 litRgb = surfaceRgb * lighting;
    vec3 emissiveRadiance = emissiveDecalRgb * max(decalEmissiveStrength, 0.0);
    vec3 surfaceOutput = litRgb * (1.0 - emissiveDecalAlpha)
            + emissiveRadiance * emissiveDecalAlpha;
    finalColor = vec4(StoreFiniteHalfRadiance(ApplySectorFog(
            surfaceOutput,
            staticAtmosphericLighting,
            fragWorldPosition)),
            clamp(baseColor.a * fragColor.a, 0.0, 1.0));
}
)";

const char* SectorDepthPrepassVs = R"(
#version 330
in vec3 vertexPosition;
uniform mat4 mvp;
void main() { gl_Position = mvp * vec4(vertexPosition, 1.0); }
)";

const char* SectorDepthPrepassFs = R"(
#version 330
void main() { }
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

std::string FormatDynamicLightDebugText(
        bool dynamicLightingEnabled,
        size_t selectedCount,
        size_t candidateCount,
        size_t totalCount,
        const std::vector<SectorPreviewDynamicLightKey>& selectedKeys)
{
    std::ostringstream out;
    out << "dynamic lights: selected "
            << selectedCount
            << " | portal eligible "
            << candidateCount
            << " | sources "
            << totalCount
            << " ("
            << (dynamicLightingEnabled ? "on" : "off")
            << ")";
    if (!selectedKeys.empty()) {
        out << " | selected: ";
        for (size_t i = 0; i < selectedKeys.size(); ++i) {
            if (i > 0) {
                out << ",";
            }
            out << (selectedKeys[i].kind == SectorPreviewDynamicLightKind::Spot
                            ? "spot:"
                            : "point:")
                    << selectedKeys[i].lightId;
        }
    }
    return out.str();
}

size_t CountDynamicSpotLightShadowCandidates(const std::vector<SectorPreviewDynamicPointLightUniform>& selectedLights)
{
    size_t count = 0;
    for (const SectorPreviewDynamicPointLightUniform& light : selectedLights) {
        if (light.castsShadow) {
            ++count;
        }
    }
    return count;
}

std::string FormatDynamicSpotLightShadowDebugText(
        size_t casterCount,
        size_t candidateCount,
        size_t maxCasterCount,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& casters,
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedLights)
{
    std::ostringstream out;
    out << "shadow casters: "
            << casterCount
            << " / "
            << candidateCount
            << " candidates / max "
            << maxCasterCount;
    if (!casters.empty()) {
        out << " | shadow ids: ";
        for (size_t i = 0; i < casters.size(); ++i) {
            if (i > 0) {
                out << ",";
            }
            const SectorPreviewDynamicSpotLightShadowCaster& caster = casters[i];
            const SectorPreviewDynamicLightKind kind = caster.dynamicLightIndex >= 0
                            && static_cast<std::size_t>(caster.dynamicLightIndex)
                                    < selectedLights.size()
                    ? selectedLights[static_cast<std::size_t>(
                            caster.dynamicLightIndex)].kind
                    : SectorPreviewDynamicLightKind::Spot;
            out << (kind == SectorPreviewDynamicLightKind::Spot
                            ? "spot:"
                            : "point:")
                    << caster.lightId;
        }
    }
    return out.str();
}

std::vector<std::string> SortedTopologyTextureIds(const SectorTopologyMap& map)
{
    std::vector<std::string> ids;
    ids.reserve(map.texturesById.size());
    for (const auto& texture : map.texturesById) {
        ids.push_back(texture.first);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool LoadPreviewMaterial(
        Material& material,
        Texture2D& defaultMaterialTexture,
        bool& materialLoaded,
        int& useLightmapLoc,
        int& useBakedAmbientOcclusionLoc,
        int& hasLightmapLoc,
        int& hasNormalMapLoc,
        int& alphaTestLoc,
        int& alphaCutoffLoc,
        int& hasDecalLoc,
        int& decalOpacityLoc,
        int& decalEmissiveLoc,
        int& decalEmissiveStrengthLoc,
        int& decalTintLoc,
        int& dynamicLightCountLoc,
        int& dynamicLightPositionsLoc,
        int& dynamicLightColorsLoc,
        int& dynamicLightRadiiLoc,
        int& dynamicLightIntensitiesLoc,
        int& dynamicLightTypesLoc,
        int& dynamicLightDirectionsLoc,
        int& dynamicLightInnerConeCosLoc,
        int& dynamicLightOuterConeCosLoc,
        int& dynamicLightSpotShadowRightLoc,
        int& dynamicLightSpotShadowProjectionLoc,
        int& hasPointShadowsLoc,
        int& dynamicLightShadowSlotsLoc,
        std::array<int, MaxDynamicSpotLightShadowCasters>& shadowLightMatrixLocs,
        int& shadowBiasLoc,
        int& shadowStrengthLoc,
        int& shadowSoftnessLoc,
        int& shadowAtlasTilesPerRowLoc,
        SectorFogShaderLocations& fogShaderLocations,
        std::string& error)
{
    material = LoadMaterialDefault();
    Shader shader = LoadShaderFromMemory(SectorLightmapVs, SectorLightmapFs);
    if (shader.id == 0) {
        UnloadMaterial(material);
        material = Material{};
        error = "Preview failed: could not load sector lightmap shader";
        return false;
    }
    material.shader = shader;
    material.shader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocationAttrib(material.shader, "vertexNormal");
    material.shader.locs[SHADER_LOC_VERTEX_TANGENT] = GetShaderLocationAttrib(material.shader, "vertexTangent");
    material.shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(material.shader, "texture0");
    material.shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(material.shader, "texture1");
    material.shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(material.shader, "decalTexture");
    material.shader.locs[SHADER_LOC_MAP_HEIGHT] = GetShaderLocation(material.shader, "normalTexture");
    material.shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(material.shader, "shadowMap0");
    material.shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(material.shader, "shadowMap1");
    useLightmapLoc = GetShaderLocation(material.shader, "useLightmap");
    useBakedAmbientOcclusionLoc = GetShaderLocation(material.shader, "useBakedAmbientOcclusion");
    hasLightmapLoc = GetShaderLocation(material.shader, "hasLightmap");
    hasNormalMapLoc = GetShaderLocation(material.shader, "hasNormalMap");
    alphaTestLoc = GetShaderLocation(material.shader, "alphaTest");
    alphaCutoffLoc = GetShaderLocation(material.shader, "alphaCutoff");
    hasDecalLoc = GetShaderLocation(material.shader, "hasDecal");
    decalOpacityLoc = GetShaderLocation(material.shader, "decalOpacity");
    decalEmissiveLoc = GetShaderLocation(material.shader, "decalEmissive");
    decalEmissiveStrengthLoc = GetShaderLocation(material.shader, "decalEmissiveStrength");
    decalTintLoc = GetShaderLocation(material.shader, "decalTint");
    dynamicLightCountLoc = GetShaderLocation(material.shader, "dynamicLightCount");
    dynamicLightPositionsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightPositions");
    dynamicLightColorsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightColors");
    dynamicLightRadiiLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightRadii");
    dynamicLightIntensitiesLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightIntensities");
    dynamicLightTypesLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightTypes");
    dynamicLightDirectionsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightDirections");
    dynamicLightInnerConeCosLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightInnerConeCos");
    dynamicLightOuterConeCosLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightOuterConeCos");
    dynamicLightSpotShadowRightLoc = GetShaderLocationArrayBase(
            material.shader, "dynamicLightSpotShadowRight");
    dynamicLightSpotShadowProjectionLoc = GetShaderLocationArrayBase(
            material.shader, "dynamicLightSpotShadowProjection");
    hasPointShadowsLoc = GetShaderLocation(material.shader, "hasPointShadows");
    dynamicLightShadowSlotsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        shadowLightMatrixLocs[i] = GetShaderLocationArrayElement(material.shader, "shadowLightMatrices", i);
    }
    shadowBiasLoc = GetShaderLocationArrayBase(material.shader, "shadowBias");
    shadowStrengthLoc = GetShaderLocationArrayBase(material.shader, "shadowStrength");
    shadowSoftnessLoc = GetShaderLocationArrayBase(material.shader, "shadowSoftness");
    shadowAtlasTilesPerRowLoc = GetShaderLocation(material.shader, "shadowAtlasTilesPerRow");
    fogShaderLocations = GetSectorFogShaderLocations(material.shader);
    defaultMaterialTexture = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    materialLoaded = true;
    return true;
}

bool ComputeGeometryBounds(const SectorGeneratedGeometry& geometry, Vector3& outMin, Vector3& outMax)
{
    SectorAabb3 bounds = EmptySectorAabb3();
    bool found = false;
    for (const SectorGeneratedSurface& surface : geometry.surfaces) {
        for (const SectorGeneratedVertex& vertex : surface.vertices) {
            ExpandSectorAabb3(bounds, vertex.position);
            found = true;
        }
    }
    outMin = bounds.min;
    outMax = bounds.max;
    return found;
}

} // namespace

bool SectorMeshRenderer::Rebuild(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const char* scopeName,
        std::string& error)
{
    return RebuildRendererResources(assets, map, scopeName, error);
}

bool SectorMeshRenderer::RebuildRendererResources(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const char* scopeName,
        std::string& error)
{
    Shutdown(assets);
    error.clear();

    if (map.sectors.empty()) {
        error = "Preview failed: topology map has no sectors";
        return false;
    }

    if (map.texturesById.empty()) {
        error = "Preview failed: missing topology texture table";
        return false;
    }

    if (!BuildSectorGeneratedGeometry(map, generatedGeometry, &error)) {
        error = error.empty()
                ? "Preview failed: topology generated no geometry"
                : "Preview failed: " + error;
        generatedGeometry = {};
        return false;
    }
    billboardRenderer.ResetDebugState();
    staticModelRenderer.ResetDebugState();

    std::string visibilityError;
    visibilityGraphValid = BuildRuntimeSectorVisibilityGraph(map, visibilityGraph, &visibilityError);
    if (!visibilityGraphValid) {
        std::fprintf(stderr, "[SectorDemo WARNING] Visibility graph build failed: %s\n", visibilityError.c_str());
        visibilityGraph = {};
    }
    visibilityLookupWorldValid = visibilityLookupWorld.BuildFromTopology(map, &visibilityError);
    if (!visibilityLookupWorldValid) {
        std::fprintf(stderr, "[SectorDemo WARNING] Visibility sector lookup build failed: %s\n", visibilityError.c_str());
    }

    assetScope = assets.CreateScope(scopeName == nullptr ? "sector_mesh_preview" : scopeName);
    if (engine::IsNull(assetScope)) {
        generatedGeometry = {};
        error = "Preview failed: could not create asset scope";
        return false;
    }

    for (const std::string& textureId : SortedTopologyTextureIds(map)) {
        const auto it = map.texturesById.find(textureId);
        if (it == map.texturesById.end()) {
            continue;
        }

        const SectorTextureDefinition& texture = it->second;
        const std::string resolvedPath = ResolveSectorAssetPath(texture.path);
        engine::TextureHandle handle = assets.RequestTexture(
                assetScope,
                texture.id.c_str(),
                resolvedPath.c_str(),
                engine::TextureColorUsage::SceneSrgb,
                SectorTextureLoadFlags(texture.filter));
        textureHandlesById.emplace(texture.id, handle);

        const std::string normalMapPath = SectorTextureNormalMapPath(texture.path);
        const std::string resolvedNormalMapPath = ResolveSectorAssetPath(normalMapPath);
        std::error_code normalMapError;
        if (!normalMapPath.empty()
                && std::filesystem::is_regular_file(resolvedNormalMapPath, normalMapError)
                && !normalMapError) {
            const std::string normalMapKey = texture.id + "_sector_normal";
            normalTextureHandlesById.emplace(
                    texture.id,
                    assets.RequestTexture(
                            assetScope,
                            normalMapKey.c_str(),
                            resolvedNormalMapPath.c_str(),
                            engine::TextureColorUsage::LinearData,
                            SectorTextureLoadFlags(texture.filter)));
        }
    }

    if (ShouldRenderSkyCylinder(map)) {
        const SectorTextureDefinition* skyTexture = FindSkyTexture(map);
        const engine::TextureHandle skyTextureHandle = skyTexture == nullptr
                ? engine::NullTextureHandle()
                : TextureForId(skyTexture->id);
        skyRenderer.Rebuild(map, skyTextureHandle);
    }
    BuildSectorPbrEnvironment(
            assets,
            assetScope,
            map,
            pbrEnvironment);

    SectorLightmapLayout lightmapLayout;
    const std::vector<SectorLightmapAtlasMetadata> lightmapAtlases =
            GetSectorLightmapAtlases(map.bakedLightmap);
    const SectorLightmapStatus status = GetSectorLightmapStatus(map);
    lightmapStatus = static_cast<int>(status);
    objectProbeBakeCurrent =
            GetSectorBakedObjectLightProbeStatus(map)
                    == SectorLightmapStatus::Valid;
    bool useLightmapLayout = status == SectorLightmapStatus::Valid
            && BuildSectorLightmapLayout(map, lightmapLayout, error);
    if (useLightmapLayout
            && lightmapLayout.atlasCount
                    > static_cast<int>(lightmapAtlases.size())) {
        error = "Baked lightmap metadata does not contain every topology atlas";
        useLightmapLayout = false;
        lightmapStatus = static_cast<int>(SectorLightmapStatus::Stale);
    }
    if (status == SectorLightmapStatus::Valid && !useLightmapLayout) {
        std::fprintf(stderr, "[SectorDemo WARNING] %s\n", error.c_str());
        error.clear();
    }

    SectorStaticModelLightmapData staticModelLightmapData;
    if (useLightmapLayout && HasAssignedSectorStaticModels(map)) {
        std::string staticModelError;
        if (!ReadSectorStaticModelLightmapSidecar(
                    ResolveSectorAssetPath(
                            map.bakedLightmap.staticModels.path),
                    &map.bakedLightmap.staticModels,
                    staticModelLightmapData,
                    staticModelError)) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Static model lightmap disabled: %s\n",
                    staticModelError.c_str());
            staticModelLightmapData = {};
        } else if (!AreSectorStaticModelLightmapAtlasIndicesValid(
                           staticModelLightmapData,
                           static_cast<int>(lightmapAtlases.size()))) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Static model lightmap disabled: atlas index is outside installed metadata\n");
            staticModelLightmapData = {};
        }
    }
    staticModelRenderer.SetLightmapData(
            std::move(staticModelLightmapData));

    if (useLightmapLayout) {
        lightmapTextures.reserve(lightmapAtlases.size());
        for (size_t atlasIndex = 0; atlasIndex < lightmapAtlases.size(); ++atlasIndex) {
            const std::string resolvedPath = ResolveSectorAssetPath(
                    lightmapAtlases[atlasIndex].path);
            const std::string key = "sector_lightmap_atlas_"
                    + std::to_string(atlasIndex);
            SectorLightmapArtifactData artifact;
            std::string artifactError;
            if (!ReadSectorLightmapArtifact(
                        resolvedPath,
                        &map.bakedLightmap,
                        artifact,
                        artifactError)) {
                std::fprintf(stderr,
                        "[SectorDemo WARNING] HDR lightmap disabled: %s\n",
                        artifactError.c_str());
                lightmapTextures.clear();
                useLightmapLayout = false;
                lightmapStatus = static_cast<int>(SectorLightmapStatus::Invalid);
                break;
            }
            Image image{};
            image.data = artifact.rgba16.data();
            image.width = artifact.width;
            image.height = artifact.height;
            image.mipmaps = 1;
            image.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
            const engine::TextureHandle texture = assets.CreateTextureFromImage(
                    assetScope,
                    key.c_str(),
                    image,
                    engine::TextureColorUsage::LinearData,
                    engine::TextureLoad_BilinearFilter);
            if (engine::IsNull(texture)) {
                std::fprintf(stderr,
                        "[SectorDemo WARNING] HDR lightmap GPU upload failed for '%s'\n",
                        resolvedPath.c_str());
                lightmapTextures.clear();
                useLightmapLayout = false;
                lightmapStatus = static_cast<int>(SectorLightmapStatus::Invalid);
                break;
            }
            lightmapTextures.push_back(texture);
        }
    }

    std::string meshError;
    meshes = BuildSectorMeshes(map, useLightmapLayout ? &lightmapLayout : nullptr, &meshError);
    if (meshes.sectorDrawRecords.empty()) {
        Shutdown(assets);
        error = meshError.empty()
                ? "Preview failed: topology mesh builder produced no sector draw records"
                : "Preview failed: " + meshError;
        return false;
    }

    surfaceLightmapBakeCurrent = useLightmapLayout
            && static_cast<SectorLightmapStatus>(lightmapStatus)
                    == SectorLightmapStatus::Valid;
    RebuildSectorStaticSpecularLights(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            meshes.sectorReceiverBounds,
            staticSpecularLightState);

    dynamicLightState.RebuildSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
    const size_t runtimeObjectCapacity = std::max(
            kSectorRuntimeObjectInitialCapacity,
            map.runtimeObjects.size());
    staticModelRenderer.ReserveShadowCasterCapacity(runtimeObjectCapacity);
    dynamicModelShadowRenderer.ReserveShadowCasterCapacity(
            runtimeObjectCapacity);
    dynamicLightState.ReserveReceiverBoundsCapacity(
            meshes.sectorReceiverBounds.size(),
            runtimeObjectCapacity);
    BuildSectorLightAtmosphereSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            lightAtmosphereSources);
    doorRenderer.ReserveRuntimeDoorCapacity(runtimeObjectCapacity);
    runtimeSeconds = 0.0f;
    distanceFogRenderer.Shutdown();
    analyticFogRenderer.Shutdown();
    analyticLightShaftRenderer.Shutdown();
    lightProxyRenderer.Shutdown();
    lightDustRenderer.Shutdown();
    analyticFogRenderer.Reserve(map.compiledLocalFogVolumes.size());
    analyticLightShaftRenderer.Reserve(lightAtmosphereSources.size());
    lightProxyRenderer.Reserve(lightAtmosphereSources.size());
    UnloadHdrSceneColorView();

    if (!dynamicLightState.EnsureShadowMapResources()) {
        Shutdown(assets);
        error = "Preview failed: could not create dynamic spotlight shadow maps";
        return false;
    }

    if (!dynamicLightState.LoadShadowMaterial()) {
        Shutdown(assets);
        error = "Preview failed: could not load dynamic spotlight shadow shader";
        return false;
    }

    if (!dynamicModelShadowRenderer.Load()) {
        Shutdown(assets);
        error = "Preview failed: could not load dynamic model shadow renderer";
        return false;
    }
    if (!billboardRenderer.Load()) {
        Shutdown(assets);
        error = "Preview failed: could not load billboard cutout shader";
        return false;
    }

    if (!staticModelRenderer.Load()) {
        Shutdown(assets);
        error = "Preview failed: could not load static model shader";
        return false;
    }

    if (!doorRenderer.LoadOpaqueResources()) {
        Shutdown(assets);
        error = "Preview failed: could not load door opaque shader";
        return false;
    }

    if (!LoadPreviewMaterial(
                material,
                defaultMaterialTexture,
                materialLoaded,
                useLightmapLoc,
                useBakedAmbientOcclusionLoc,
                hasLightmapLoc,
                hasNormalMapLoc,
                alphaTestLoc,
                alphaCutoffLoc,
                hasDecalLoc,
                decalOpacityLoc,
                decalEmissiveLoc,
                decalEmissiveStrengthLoc,
                decalTintLoc,
                dynamicLightCountLoc,
                dynamicLightPositionsLoc,
                dynamicLightColorsLoc,
                dynamicLightRadiiLoc,
                dynamicLightIntensitiesLoc,
                dynamicLightTypesLoc,
                dynamicLightDirectionsLoc,
                dynamicLightInnerConeCosLoc,
                dynamicLightOuterConeCosLoc,
                dynamicLightSpotShadowRightLoc,
                dynamicLightSpotShadowProjectionLoc,
                hasPointShadowsLoc,
                dynamicLightShadowSlotsLoc,
                shadowLightMatrixLocs,
                shadowBiasLoc,
                shadowStrengthLoc,
                shadowSoftnessLoc,
                shadowAtlasTilesPerRowLoc,
                fogShaderLocations,
                error)) {
        Shutdown(assets);
        return false;
    }

    depthPrepassMaterial = LoadMaterialDefault();
    Shader depthShader = LoadShaderFromMemory(SectorDepthPrepassVs, SectorDepthPrepassFs);
    if (depthShader.id == 0) {
        Shutdown(assets);
        error = "Preview failed: could not load depth pre-pass shader";
        return false;
    }
    depthPrepassMaterial.shader = depthShader;
    depthPrepassMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(depthShader, "vertexPosition");
    depthPrepassMaterial.shader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(depthShader, "mvp");
    depthPrepassMaterialLoaded = true;

    sectorCount = map.sectors.size();

    Vector3 boundsMin{};
    Vector3 boundsMax{};
    if (ComputeGeometryBounds(generatedGeometry, boundsMin, boundsMax)) {
        const Vector3 center = Vector3Scale(Vector3Add(boundsMin, boundsMax), 0.5f);
        const float height = std::max(1.6f, (boundsMax.y - boundsMin.y) * 0.5f);
        position = Vector3{center.x, boundsMin.y + height, center.z};
    } else {
        position = Vector3{0.0f, 1.6f, 0.0f};
    }
    yawRadians = 0.0f;
    pitchRadians = 0.0f;
    camera.fovy = verticalFovDegrees;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateCamera();
    UpdateVisibilityDebug();

    initialized = true;
    return true;
}

void SectorMeshRenderer::Shutdown(engine::AssetManager& assets)
{
    ShutdownRendererResources(assets);
}

void SectorMeshRenderer::ShutdownRendererResources(engine::AssetManager& assets)
{
    ShutdownAtmosphereGpuQueries();
    atmosphereDiagnostics = SectorAtmosphereDiagnostics{};
    generatedGeometry = {};
    visibilityGraph = {};
    visibilityResult = {};
    portalVisibilityDebugText.clear();
    visibilityDebugText.clear();
    renderDebugText.clear();
    billboardRenderer.ResetDebugState();
    staticModelRenderer.ResetDebugState();
    visibilityLookupWorld = SectorCollisionWorld{};
    visibilityGraphValid = false;
    visibilityLookupWorldValid = false;
    dynamicLightState.Reset();
    ResetSectorStaticSpecularLights(staticSpecularLightState);
    surfaceLightmapBakeCurrent = false;
    objectProbeBakeCurrent = false;
    lightAtmosphereSources.clear();
    doorRenderer.ClearPreparedShadowCasters();
    dynamicModelShadowRenderer.ClearPreparedShadowCasters();
    runtimeSeconds = 0.0f;
    distanceFogRenderer.Shutdown();
    analyticFogRenderer.Shutdown();
    analyticLightShaftRenderer.Shutdown();
    lightProxyRenderer.Shutdown();
    lightDustRenderer.Shutdown();
    UnloadHdrSceneColorView();
    if (!initialized
            && engine::IsNull(assetScope)
            && meshes.batches.empty()
            && meshes.sectorDrawRecords.empty()
            && !materialLoaded
            && !bloomRenderer.IsLoaded()
            && hdrCompositeShader.id == 0
            && !engine::IsRenderTargetReady(hdrSceneScratch)
            && !billboardRenderer.IsLoaded()
            && !staticModelRenderer.IsLoaded()
            && !doorRenderer.HasOpaqueResources()
            && !doorRenderer.HasCachedDoorMeshes()
            && !dynamicLightState.HasShadowMapResources()
            && !dynamicLightState.HasShadowMaterial()
            && !dynamicModelShadowRenderer.IsLoaded()
            && !skyRenderer.IsLoaded()) {
        return;
    }

    bloomRenderer.Shutdown();
    if (hdrCompositeShader.id != 0) UnloadShader(hdrCompositeShader);
    hdrCompositeShader = {};
    hdrCompositeSceneLoc = -1;
    hdrCompositeSourceLoc = -1;
    hdrCompositeModeLoc = -1;
    hdrCompositeShaderFailed = false;
    engine::UnloadRenderTarget(hdrSceneScratch);
    hdrSceneScratchError.clear();
    hdrSceneScratchDiagnostic = "not allocated";
    hdrSceneScratchFailedWidth = 0;
    hdrSceneScratchFailedHeight = 0;
    dynamicLightState.UnloadShadowMaterial();
    dynamicLightState.UnloadShadowMapResources();
    dynamicModelShadowRenderer.Shutdown();
    skyRenderer.Shutdown();
    pbrEnvironment = {};
    doorRenderer.UnloadDoorMeshes();
    UnloadSectorMeshes(meshes);
    textureHandlesById.clear();
    normalTextureHandlesById.clear();
    lightmapTextures.clear();
    sectorCount = 0;

    if (materialLoaded) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = defaultMaterialTexture;
        material.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
        material.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
        material.maps[MATERIAL_MAP_HEIGHT].texture = Texture2D{};
        UnloadMaterial(material);
        material = Material{};
        defaultMaterialTexture = Texture2D{};
        materialLoaded = false;
        dynamicLightShadowSlotsLoc = -1;
        shadowLightMatrixLocs.fill(-1);
        shadowBiasLoc = -1;
        shadowStrengthLoc = -1;
        shadowSoftnessLoc = -1;
    }
    if (depthPrepassMaterialLoaded) {
        UnloadMaterial(depthPrepassMaterial);
        depthPrepassMaterial = Material{};
        depthPrepassMaterialLoaded = false;
    }

    billboardRenderer.Shutdown();
    staticModelRenderer.Shutdown();
    doorRenderer.ShutdownOpaqueResources();

    if (!engine::IsNull(assetScope)) {
        assets.UnloadScope(assetScope);
        assetScope = engine::NullAssetScopeHandle();
    }

    initialized = false;
}

void SectorMeshRenderer::AdvanceRuntime(float dt)
{
    if (std::isfinite(dt) && dt > 0.0f) {
        runtimeSeconds += dt;
    }
}

void SectorMeshRenderer::FinalizeRuntimeObjectResources(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld)
{
    staticModelRenderer.FinalizeResources(
            assets,
            runtimeObjectWorld);
}

void SectorMeshRenderer::Render(
        engine::AssetManager& assets,
        bool useBakedAmbientOcclusion,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting,
        const SectorTopologyFogSettings& fogSettings)
{
    RenderDynamicSpotLightShadowMaps(assets, runtimeObjectWorld);
    DrawScene(assets, useBakedAmbientOcclusion, runtimeObjectWorld, doorLighting, fogSettings);
}

void SectorMeshRenderer::DrawScene(
        engine::AssetManager& assets,
        bool useBakedAmbientOcclusion,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting,
        const SectorTopologyFogSettings& fogSettings)
{
    if (!initialized) {
        return;
    }

    BeginMode3D(camera);
    skyRenderer.Draw(assets, camera);
    if (depthPrepassEnabled && depthPrepassMaterialLoaded) {
        rlDrawRenderBatchActive();
        rlColorMask(false, false, false, false);
        rlEnableDepthTest();
        rlEnableDepthMask();
        DrawDepthPrepass(assets, runtimeObjectWorld);
        rlDrawRenderBatchActive();
        rlColorMask(true, true, true, true);
    }

    SectorTopologyFogSettings materialFogSettings = fogSettings;
    if (NormalizeSectorTopologyFogSettings(fogSettings).mode
            == SectorTopologyFogMode::Distance) {
        materialFogSettings.enabled = false;
    }
    const SectorFogRenderContext fogContext =
            BuildSectorFogRenderContext(materialFogSettings, camera.position);
    UploadSectorFogShaderValues(material.shader, fogShaderLocations, fogContext);

    float useAo = useBakedAmbientOcclusion ? 1.0f : 0.0f;
    const bool dynamicShadowsEnabled =
            shadowMapsEnabled && dynamicLightingEnabled;
    const Texture2D* shadowMap0 = dynamicShadowsEnabled
            ? dynamicLightState.ShadowMapDepthTexture(0) : nullptr;
    const Texture2D* shadowMap1 = dynamicShadowsEnabled
            ? dynamicLightState.ShadowMapDepthTexture(1) : nullptr;
    material.maps[MATERIAL_MAP_ROUGHNESS].texture = shadowMap0 != nullptr ? *shadowMap0 : Texture2D{};
    material.maps[MATERIAL_MAP_OCCLUSION].texture = shadowMap1 != nullptr ? *shadowMap1 : Texture2D{};
    if (useBakedAmbientOcclusionLoc >= 0) {
        SetShaderValue(material.shader, useBakedAmbientOcclusionLoc, &useAo, SHADER_UNIFORM_FLOAT);
    }
    SectorDynamicLightShaderLocations dynamicLightLocations;
    dynamicLightLocations.dynamicLightCount = dynamicLightCountLoc;
    dynamicLightLocations.dynamicLightPositions = dynamicLightPositionsLoc;
    dynamicLightLocations.dynamicLightColors = dynamicLightColorsLoc;
    dynamicLightLocations.dynamicLightRadii = dynamicLightRadiiLoc;
    dynamicLightLocations.dynamicLightIntensities = dynamicLightIntensitiesLoc;
    dynamicLightLocations.dynamicLightTypes = dynamicLightTypesLoc;
    dynamicLightLocations.dynamicLightDirections = dynamicLightDirectionsLoc;
    dynamicLightLocations.dynamicLightInnerConeCos = dynamicLightInnerConeCosLoc;
    dynamicLightLocations.dynamicLightOuterConeCos = dynamicLightOuterConeCosLoc;
    dynamicLightLocations.dynamicLightSpotShadowRight = dynamicLightSpotShadowRightLoc;
    dynamicLightLocations.dynamicLightSpotShadowProjection =
            dynamicLightSpotShadowProjectionLoc;
    dynamicLightLocations.hasPointShadows = hasPointShadowsLoc;
    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    shadowLocations.shadowAtlasTilesPerRow = shadowAtlasTilesPerRowLoc;
    dynamicLightState.BuildSectorLightContexts(
            meshes.sectorReceiverBounds,
            dynamicLightingEnabled,
            dynamicShadowsEnabled,
            runtimeSeconds);
    const SectorBillboardDynamicLightContext fallbackLightContext =
            dynamicLightState.BuildLightContext(
                    nullptr,
                    dynamicLightingEnabled,
                    dynamicShadowsEnabled,
                    runtimeSeconds);
    UploadSectorRendererDynamicSpotLightShadowUniforms(
            material.shader,
            shadowLocations,
            fallbackLightContext.shadowUniforms);
    int uploadedLightSectorId = std::numeric_limits<int>::min();
    for (const SectorMeshBatch& batch : meshes.sectorDrawRecords) {
        if (!ShouldDrawSectorMeshRecordForVisibility(batch, visibilityResult)) {
            continue;
        }

        if (batch.sectorId != uploadedLightSectorId) {
            const SectorBillboardDynamicLightContext* lightContext =
                    dynamicLightState.FindSectorLightContext(batch.sectorId);
            if (lightContext == nullptr) lightContext = &fallbackLightContext;
            UploadSectorRendererDynamicPointLights(
                    material.shader, dynamicLightLocations, *lightContext);
            UploadSectorRendererDynamicShadowSlots(
                    material.shader,
                    shadowLocations.dynamicLightShadowSlots,
                    lightContext->shadowUniforms);
            uploadedLightSectorId = batch.sectorId;
        }

        const engine::TextureHandle textureHandle = TextureForId(batch.textureId);
        const Texture2D* texture = assets.GetTexture(textureHandle);
        material.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                ? *texture
                : defaultMaterialTexture;

        const Texture2D* normalTexture = assets.GetTexture(
                NormalTextureForId(batch.textureId));

        const Texture2D* decalTexture = nullptr;
        if (!batch.decalTextureId.empty()) {
            decalTexture = assets.GetTexture(TextureForId(batch.decalTextureId));
        }

        const Texture2D* lightmap = batch.lightmapAtlasIndex >= 0
                && batch.lightmapAtlasIndex
                        < static_cast<int>(lightmapTextures.size())
                ? assets.GetTexture(lightmapTextures[
                        static_cast<size_t>(batch.lightmapAtlasIndex)])
                : nullptr;
        const float useLightmap = lightmap != nullptr ? 1.0f : 0.0f;
        material.maps[MATERIAL_MAP_SPECULAR].texture = lightmap != nullptr
                ? *lightmap
                : Texture2D{};
        const int hasDecal = decalTexture != nullptr ? 1 : 0;
        const int hasLightmap = batch.receivesLightmap
                && lightmap != nullptr ? 1 : 0;
        const int hasNormalMap = normalTexture != nullptr ? 1 : 0;
        const int alphaTest = batch.alphaTest ? 1 : 0;
        const float alphaCutoff = batch.alphaCutoff;
        const float decalOpacity = batch.decalOpacity;
        const int decalEmissive = hasDecal != 0 && batch.decalEmissive ? 1 : 0;
        const Vector3 decalTint = hasDecal != 0
                ? engine::SrgbNormalizedRgbToLinearScene(batch.decalTint)
                : Vector3{1.0f, 1.0f, 1.0f};
        material.maps[MATERIAL_MAP_NORMAL].texture = (decalTexture != nullptr)
                ? *decalTexture
                : Texture2D{};
        material.maps[MATERIAL_MAP_HEIGHT].texture = (normalTexture != nullptr)
                ? *normalTexture
                : Texture2D{};
        if (useLightmapLoc >= 0) {
            SetShaderValue(
                    material.shader,
                    useLightmapLoc,
                    &useLightmap,
                    SHADER_UNIFORM_FLOAT);
        }
        if (hasLightmapLoc >= 0) {
            SetShaderValue(material.shader, hasLightmapLoc, &hasLightmap, SHADER_UNIFORM_INT);
        }
        if (hasNormalMapLoc >= 0) {
            SetShaderValue(material.shader, hasNormalMapLoc, &hasNormalMap, SHADER_UNIFORM_INT);
        }
        if (alphaTestLoc >= 0) {
            SetShaderValue(material.shader, alphaTestLoc, &alphaTest, SHADER_UNIFORM_INT);
        }
        if (alphaCutoffLoc >= 0) {
            SetShaderValue(material.shader, alphaCutoffLoc, &alphaCutoff, SHADER_UNIFORM_FLOAT);
        }
        if (hasDecalLoc >= 0) {
            SetShaderValue(material.shader, hasDecalLoc, &hasDecal, SHADER_UNIFORM_INT);
        }
        if (decalOpacityLoc >= 0) {
            SetShaderValue(material.shader, decalOpacityLoc, &decalOpacity, SHADER_UNIFORM_FLOAT);
        }
        if (decalEmissiveLoc >= 0) {
            SetShaderValue(material.shader, decalEmissiveLoc, &decalEmissive, SHADER_UNIFORM_INT);
        }
        if (decalEmissiveStrengthLoc >= 0) {
            const float emissiveStrength = batch.decalEmissiveStrength;
            SetShaderValue(material.shader, decalEmissiveStrengthLoc, &emissiveStrength, SHADER_UNIFORM_FLOAT);
        }
        if (decalTintLoc >= 0) {
            SetShaderValue(material.shader, decalTintLoc, &decalTint, SHADER_UNIFORM_VEC3);
        }
        DrawMesh(batch.mesh, material, MatrixIdentity());
    }
    if (runtimeObjectWorld != nullptr) {
        SectorDoorDrawContext doorDrawContext;
        doorDrawContext.assets = &assets;
        doorDrawContext.runtimeObjectWorld = runtimeObjectWorld;
        doorDrawContext.lighting = doorLighting;
        doorDrawContext.dynamicLighting.enabled = dynamicLightingEnabled;
        doorDrawContext.dynamicLighting.runtimeSeconds = runtimeSeconds;
        doorDrawContext.dynamicLighting.selectedLights = &dynamicLightState.SelectedLights();
        doorDrawContext.dynamicLighting.shadowUniforms =
                dynamicLightState.PackShadowUniforms(shadowMapsEnabled);
        doorDrawContext.dynamicLighting.shadowMaps = dynamicLightState.BuildShadowMapTextures();
        doorDrawContext.fog = fogContext;
        doorDrawContext.textureResolver.userData = this;
        doorDrawContext.textureResolver.resolve = &SectorMeshRenderer::ResolveShadowCasterTexture;
        doorDrawContext.defaultMaterialTexture = &defaultMaterialTexture;
        doorDrawContext.renderDebugText = &renderDebugText;
        doorRenderer.Draw(doorDrawContext);

        const SectorBillboardDynamicLightContext billboardLightContext = BuildBillboardDynamicLightContext();
        const TextureCubemap* pbrEnvironmentTexture = assets.GetCubemap(
                pbrEnvironment.cubemap);
        if (!IsSectorPbrEnvironmentActive(
                    pbrEnvironment,
                    pbrEnvironmentTexture)) {
            pbrEnvironmentTexture = nullptr;
        }
        staticModelRenderer.Draw(
                assets,
                *runtimeObjectWorld,
                camera,
                billboardLightContext,
                staticSpecularLightState,
                surfaceLightmapBakeCurrent,
                objectProbeBakeCurrent
                        && doorLighting.objectLightProbes != nullptr
                        && !doorLighting.objectLightProbes->probes.empty(),
                fogContext,
                visibilityResult,
                lightmapTextures,
                pbrEnvironmentTexture,
                useBakedAmbientOcclusion,
                renderDebugText);
        SectorDynamicModelShadowDrawContext modelShadowContext;
        modelShadowContext.assets = &assets;
        modelShadowContext.world = runtimeObjectWorld;
        modelShadowContext.collisionWorld = visibilityLookupWorldValid
                ? &visibilityLookupWorld
                : nullptr;
        modelShadowContext.visibility = &visibilityResult;
        dynamicModelShadowRenderer.Draw(modelShadowContext);
        billboardRenderer.Draw(
                assets,
                *runtimeObjectWorld,
                camera,
                billboardLightContext,
                fogContext,
                renderDebugText);
    }
    EndMode3D();
}

void SectorMeshRenderer::DrawDepthPrepass(
        engine::AssetManager& assets,
        engine::World* runtimeObjectWorld)
{
    for (const SectorMeshBatch& batch : meshes.sectorDrawRecords) {
        if (batch.alphaTest
                || !ShouldDrawSectorMeshRecordForVisibility(batch, visibilityResult)) {
            continue;
        }
        DrawMesh(batch.mesh, depthPrepassMaterial, MatrixIdentity());
    }
    if (runtimeObjectWorld == nullptr) return;

    SectorDynamicSpotLightShadowRenderContext context;
    context.assets = &assets;
    context.sectorDrawRecords = &meshes.sectorDrawRecords;
    doorRenderer.PrepareShadowRenderContext(context, runtimeObjectWorld);
    staticModelRenderer.PrepareShadowRenderContext(context, runtimeObjectWorld);

    if (context.doorShadowCasters != nullptr && context.doorMeshResolver != nullptr) {
        for (const SectorDoorShadowCaster& caster : *context.doorShadowCasters) {
            float width = 0.0f;
            float height = 0.0f;
            const Mesh* mesh = context.doorMeshResolver(
                    context.doorMeshResolverUserData, caster, width, height);
            if (mesh == nullptr || mesh->vertexCount <= 0) continue;
            DrawMesh(*mesh, depthPrepassMaterial,
                    BuildSectorDoorShadowCasterModelMatrix(caster, width, height));
        }
    }
    if (context.doorModelShadowCasters != nullptr) {
        for (const SectorDoorModelShadowCaster& caster : *context.doorModelShadowCasters) {
            const engine::ModelAsset* asset = assets.GetModelAsset(caster.model);
            if (asset == nullptr) continue;
            const Matrix transform = MatrixMultiply(asset->model.transform, caster.transform);
            for (int i = 0; i < asset->model.meshCount; ++i) {
                if (asset->model.meshes[i].vertexCount > 0) {
                    DrawMesh(asset->model.meshes[i], depthPrepassMaterial, transform);
                }
            }
        }
    }
    if (context.staticModelShadowCasters != nullptr) {
        rlDisableBackfaceCulling();
        for (const SectorStaticModelShadowCaster& caster : *context.staticModelShadowCasters) {
            const engine::ModelAsset* asset = assets.GetModelAsset(caster.model);
            if (asset == nullptr) continue;
            const Matrix transform = MatrixMultiply(asset->model.transform, caster.transform);
            for (int i = 0; i < asset->model.meshCount; ++i) {
                if (asset->model.meshes[i].vertexCount > 0) {
                    DrawMesh(asset->model.meshes[i], depthPrepassMaterial, transform);
                }
            }
        }
        rlEnableBackfaceCulling();
    }
}

SectorBillboardDynamicLightContext SectorMeshRenderer::BuildBillboardDynamicLightContext() const
{
    return dynamicLightState.BuildLightContext(
            nullptr,
            dynamicLightingEnabled,
            shadowMapsEnabled && dynamicLightingEnabled,
            runtimeSeconds);
}

void SectorMeshRenderer::DrawViewmodel(
        engine::AssetManager& assets,
        const engine::ModelAsset& asset,
        engine::AnimatedModelInstance& instance,
        const Camera3D& viewmodelCamera,
        Matrix transform,
        const engine::ModelAsset* attachmentAsset,
        Matrix attachmentTransform,
        int receiverSectorId,
        bool objectProbeRuntimeAvailable,
        const BakedObjectLightingVerticalSample& ambientLighting,
        const SectorViewmodelLightingContext& lighting,
        const SectorViewmodelLightingContext& attachmentLighting)
{
    BeginMode3D(viewmodelCamera);
    const TextureCubemap* pbrEnvironmentTexture = assets.GetCubemap(
            pbrEnvironment.cubemap);
    if (!IsSectorPbrEnvironmentActive(
                pbrEnvironment,
                pbrEnvironmentTexture)) {
        pbrEnvironmentTexture = nullptr;
    }
    const SectorReceiverBounds receiverBounds{
            receiverSectorId,
            viewmodelCamera.position,
            viewmodelCamera.position};
    const bool validProbe = ambientLighting.lower.valid
            || ambientLighting.upper.valid;
    const bool currentProbeForDraw = objectProbeBakeCurrent
            && objectProbeRuntimeAvailable;
    const SectorStaticSpecularLightContext staticSpecularContext =
            SelectSectorStaticSpecularLights(
                    staticSpecularLightState,
                    receiverBounds,
                    receiverSectorId,
                    visibilityResult,
                    currentProbeForDraw && validProbe);
    staticModelRenderer.DrawViewmodel(
            asset, instance, viewmodelCamera, transform,
            attachmentAsset, attachmentTransform,
            BuildBillboardDynamicLightContext(),
            staticSpecularContext,
            currentProbeForDraw,
            pbrEnvironmentTexture,
            ambientLighting,
            lighting,
            attachmentLighting);
    EndMode3D();
}

void SectorMeshRenderer::RenderDynamicSpotLightShadowMaps(
        engine::AssetManager& assets,
        engine::World* runtimeObjectWorld)
{
    dynamicLightState.BeginShadowFrame(
            shadowMapsEnabled && dynamicLightingEnabled);
    if (!shadowMapsEnabled) {
        return;
    }
    if (dynamicLightingEnabled && dynamicLightState.IsShadowRenderReady()) {
        SectorDynamicSpotLightShadowRenderContext context;
        context.assets = &assets;
        context.sectorDrawRecords = &meshes.sectorDrawRecords;
        context.sectorReceiverBounds = &meshes.sectorReceiverBounds;
        context.userData = this;
        context.textureResolver = &SectorMeshRenderer::ResolveShadowCasterTexture;
        doorRenderer.PrepareShadowRenderContext(context, runtimeObjectWorld);
        staticModelRenderer.PrepareShadowRenderContext(
                context,
                runtimeObjectWorld);
        dynamicModelShadowRenderer.PrepareShadowRenderContext(
                context,
                runtimeObjectWorld);
        dynamicLightState.RenderShadowMaps(context);
    }
}

bool SectorMeshRenderer::EnsureHdrSceneScratch(
        const engine::RenderTarget& sceneTarget)
{
    const int width = sceneTarget.native.texture.width;
    const int height = sceneTarget.native.texture.height;
    if (engine::IsRenderTargetReady(hdrSceneScratch)
            && hdrSceneScratch.native.texture.width == width
            && hdrSceneScratch.native.texture.height == height) {
        return true;
    }
    if (hdrSceneScratchFailedWidth == width
            && hdrSceneScratchFailedHeight == height
            && !hdrSceneScratchError.empty()) {
        return false;
    }
    engine::UnloadRenderTarget(hdrSceneScratch);
    std::string error;
    if (!engine::LoadRenderTarget(
                engine::RenderTargetDescriptor{
                        "hdr-effect-scene-scratch",
                        width,
                        height,
                        engine::RenderTargetColorFormat::Rgba32Float,
                        engine::RenderTargetFilter::Point,
                        engine::RenderTargetWrap::Clamp,
                        engine::RenderTargetDepthKind::None,
                        1},
                hdrSceneScratch,
                &error)) {
        hdrSceneScratchError = error;
        hdrSceneScratchDiagnostic = "disabled: " + error;
        hdrSceneScratchFailedWidth = width;
        hdrSceneScratchFailedHeight = height;
        TraceLog(LOG_WARNING, "HDR EFFECTS: shared RGBA32F scratch unavailable: %s",
                error.c_str());
        return false;
    }
    hdrSceneScratchError.clear();
    hdrSceneScratchDiagnostic = engine::FormatRenderTargetDiagnostic(hdrSceneScratch);
    hdrSceneScratchFailedWidth = 0;
    hdrSceneScratchFailedHeight = 0;
    return true;
}

bool SectorMeshRenderer::EnsureHdrSceneColorView(
        const engine::RenderTarget& sceneTarget)
{
    if (hdrSceneColorView.id != 0
            && hdrSceneColorView.texture.id == sceneTarget.native.texture.id) {
        return true;
    }
    UnloadHdrSceneColorView();
    hdrSceneColorView.id = rlLoadFramebuffer();
    hdrSceneColorView.texture = sceneTarget.native.texture;
    if (hdrSceneColorView.id == 0) {
        hdrSceneColorView = {};
        return false;
    }
    rlEnableFramebuffer(hdrSceneColorView.id);
    rlFramebufferAttach(
            hdrSceneColorView.id,
            sceneTarget.native.texture.id,
            RL_ATTACHMENT_COLOR_CHANNEL0,
            RL_ATTACHMENT_TEXTURE2D,
            0);
    const bool complete = rlFramebufferComplete(hdrSceneColorView.id);
    rlDisableFramebuffer();
    if (!complete) {
        UnloadHdrSceneColorView();
    }
    return complete;
}

void SectorMeshRenderer::UnloadHdrSceneColorView()
{
    if (hdrSceneColorView.id != 0) {
        rlUnloadFramebuffer(hdrSceneColorView.id);
    }
    hdrSceneColorView = {};
}

bool SectorMeshRenderer::EnsureHdrCompositeShader()
{
    if (hdrCompositeShader.id != 0) return true;
    if (hdrCompositeShaderFailed) return false;
    hdrCompositeShader = LoadShaderFromMemory(HdrCompositeVs, HdrCompositeFs);
    if (hdrCompositeShader.id == 0) { hdrCompositeShaderFailed=true; return false; }
    hdrCompositeSceneLoc = GetShaderLocation(hdrCompositeShader, "sceneColor");
    hdrCompositeSourceLoc = GetShaderLocation(hdrCompositeShader, "sourceColor");
    hdrCompositeModeLoc = GetShaderLocation(hdrCompositeShader, "compositeMode");
    return hdrCompositeSceneLoc >= 0 && hdrCompositeSourceLoc >= 0
            && hdrCompositeModeLoc >= 0;
}

bool SectorMeshRenderer::CommitHdrScratch(engine::RenderTarget& sceneTarget)
{
    if (!EnsureHdrCompositeShader()) return false;
    const int mode = 0;
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneTarget.native);
    BeginShaderMode(hdrCompositeShader);
    SetShaderValueTexture(hdrCompositeShader, hdrCompositeSourceLoc, hdrSceneScratch.native.texture);
    SetShaderValue(hdrCompositeShader, hdrCompositeModeLoc, &mode, SHADER_UNIFORM_INT);
    rlDisableColorBlend();
    DrawTexturePro(
            hdrSceneScratch.native.texture,
            Rectangle{0, 0, static_cast<float>(hdrSceneScratch.native.texture.width),
                    -static_cast<float>(hdrSceneScratch.native.texture.height)},
            Rectangle{0, 0, static_cast<float>(sceneTarget.native.texture.width),
                    static_cast<float>(sceneTarget.native.texture.height)},
            Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndShaderMode();
    EndTextureMode();
    return true;
}

bool SectorMeshRenderer::CompositeViewmodel(
        engine::RenderTarget& sceneTarget,
        const engine::RenderTarget& viewmodelTarget)
{
    if (!initialized || !EnsureHdrSceneScratch(sceneTarget) || !EnsureHdrCompositeShader()
            || viewmodelTarget.descriptor.colorFormat
                    != engine::RenderTargetColorFormat::Rgba32Float) {
        return false;
    }
    const int mode = 1;
    rlDrawRenderBatchActive();
    BeginTextureMode(hdrSceneScratch.native);
    ClearBackground(BLANK);
    BeginShaderMode(hdrCompositeShader);
    SetShaderValueTexture(hdrCompositeShader, hdrCompositeSceneLoc, sceneTarget.native.texture);
    SetShaderValueTexture(hdrCompositeShader, hdrCompositeSourceLoc, viewmodelTarget.native.texture);
    SetShaderValue(hdrCompositeShader, hdrCompositeModeLoc, &mode, SHADER_UNIFORM_INT);
    rlDisableColorBlend();
    DrawTexturePro(
            sceneTarget.native.texture,
            Rectangle{0, 0, static_cast<float>(sceneTarget.native.texture.width),
                    -static_cast<float>(sceneTarget.native.texture.height)},
            Rectangle{0, 0, static_cast<float>(hdrSceneScratch.native.texture.width),
                    static_cast<float>(hdrSceneScratch.native.texture.height)},
            Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndShaderMode();
    EndTextureMode();
    return CommitHdrScratch(sceneTarget);
}

unsigned int SectorMeshRenderer::AtmosphereGpuQuery(
        std::size_t pass,
        std::size_t querySlot,
        bool end) const
{
    const std::size_t queryIndex =
            (querySlot * AtmosphereGpuPassCount + pass) * 2
            + (end ? 1u : 0u);
    return atmosphereGpuQueries[queryIndex];
}

bool SectorMeshRenderer::EnsureAtmosphereGpuQueries()
{
    if (atmosphereGpuQueriesInitialized) return true;
    if (glQueryCounter == nullptr) return false;
    glGenQueries(
            static_cast<GLsizei>(atmosphereGpuQueries.size()),
            atmosphereGpuQueries.data());
    atmosphereGpuQueriesInitialized = atmosphereGpuQueries[0] != 0;
    return atmosphereGpuQueriesInitialized;
}

void SectorMeshRenderer::ShutdownAtmosphereGpuQueries()
{
    if (atmosphereGpuQueriesInitialized) {
        glDeleteQueries(
                static_cast<GLsizei>(atmosphereGpuQueries.size()),
                atmosphereGpuQueries.data());
    }
    atmosphereGpuQueries.fill(0);
    atmosphereGpuIssuedMasks.fill(0);
    atmosphereGpuFrameIndex = 0;
    atmosphereGpuSlot = 0;
    atmosphereGpuQueriesInitialized = false;
    atmosphereGpuActive = false;
}

void SectorMeshRenderer::BeginAtmosphereGpuFrame(bool enabled)
{
    atmosphereGpuActive = false;
    atmosphereGpuSlot = atmosphereGpuFrameIndex % AtmosphereGpuQueryLatency;
    const std::uint8_t issuedMask =
            atmosphereGpuIssuedMasks[atmosphereGpuSlot];
    if (issuedMask != 0 && atmosphereGpuQueriesInitialized) {
        bool ready = true;
        for (std::size_t pass = 0; pass < AtmosphereGpuPassCount; ++pass) {
            if ((issuedMask & (1u << pass)) == 0) continue;
            GLint available = GL_FALSE;
            glGetQueryObjectiv(
                    AtmosphereGpuQuery(pass, atmosphereGpuSlot, true),
                    GL_QUERY_RESULT_AVAILABLE,
                    &available);
            ready = ready && available == GL_TRUE;
        }
        if (ready) {
            double* values[AtmosphereGpuPassCount] = {
                    &atmosphereDiagnostics.distanceFogGpuMilliseconds,
                    &atmosphereDiagnostics.analyticFogGpuMilliseconds,
                    &atmosphereDiagnostics.analyticShaftGpuMilliseconds,
                    &atmosphereDiagnostics.lightHaloGpuMilliseconds,
                    &atmosphereDiagnostics.dustGpuMilliseconds};
            for (std::size_t pass = 0; pass < AtmosphereGpuPassCount; ++pass) {
                if ((issuedMask & (1u << pass)) == 0) continue;
                GLuint64 startNanoseconds = 0;
                GLuint64 endNanoseconds = 0;
                glGetQueryObjectui64v(
                        AtmosphereGpuQuery(pass, atmosphereGpuSlot, false),
                        GL_QUERY_RESULT,
                        &startNanoseconds);
                glGetQueryObjectui64v(
                        AtmosphereGpuQuery(pass, atmosphereGpuSlot, true),
                        GL_QUERY_RESULT,
                        &endNanoseconds);
                if (endNanoseconds >= startNanoseconds) {
                    const double sample = static_cast<double>(
                            endNanoseconds - startNanoseconds) / 1000000.0;
                    *values[pass] = *values[pass] <= 0.0
                            ? sample
                            : *values[pass] * 0.85 + sample * 0.15;
                }
            }
            atmosphereGpuIssuedMasks[atmosphereGpuSlot] = 0;
        }
    }
    if (enabled && EnsureAtmosphereGpuQueries()
            && atmosphereGpuIssuedMasks[atmosphereGpuSlot] == 0) {
        atmosphereGpuActive = true;
    }
    ++atmosphereGpuFrameIndex;
}

void SectorMeshRenderer::BeginAtmosphereGpuPass(std::size_t pass)
{
    if (!atmosphereGpuActive || pass >= AtmosphereGpuPassCount) return;
    glQueryCounter(
            AtmosphereGpuQuery(pass, atmosphereGpuSlot, false),
            GL_TIMESTAMP);
}

void SectorMeshRenderer::EndAtmosphereGpuPass(std::size_t pass)
{
    if (!atmosphereGpuActive || pass >= AtmosphereGpuPassCount) return;
    glQueryCounter(
            AtmosphereGpuQuery(pass, atmosphereGpuSlot, true),
            GL_TIMESTAMP);
    atmosphereGpuIssuedMasks[atmosphereGpuSlot] |=
            static_cast<std::uint8_t>(1u << pass);
}

void SectorMeshRenderer::RefreshAtmosphereDiagnostics(
        const SectorBillboardDynamicLightContext& dynamicLights)
{
    atmosphereDiagnostics.dynamicLightCount = dynamicLights.dynamicLightCount;
    atmosphereDiagnostics.analyticFogEligibleCount =
            analyticFogRenderer.EligibleVolumeCount();
    atmosphereDiagnostics.analyticFogActiveCount =
            analyticFogRenderer.ActiveVolumeCount();
    atmosphereDiagnostics.analyticFogScissorCoverage =
            analyticFogRenderer.ScissorCoverage();
    atmosphereDiagnostics.analyticShaftEligibleCount = analyticLightShaftRenderer.EligibleCount();
    atmosphereDiagnostics.analyticShaftActiveCount = analyticLightShaftRenderer.ActiveCount();
    atmosphereDiagnostics.analyticShaftScissorCoverage = analyticLightShaftRenderer.ScissorCoverage();
    atmosphereDiagnostics.analyticShaftDrawCallCount = analyticLightShaftRenderer.DrawCallCount();
    atmosphereDiagnostics.lightHaloEligibleCount = lightProxyRenderer.EligibleCount();
    atmosphereDiagnostics.lightHaloCount = lightProxyRenderer.HaloCount();
    atmosphereDiagnostics.lightHaloScissorCoverage = lightProxyRenderer.ScissorCoverage();
    atmosphereDiagnostics.lightHaloDrawCallCount = lightProxyRenderer.DrawCallCount();
    atmosphereDiagnostics.dustEligibleEmitterCount =
            lightDustRenderer.EligibleEmitterCount();
    atmosphereDiagnostics.dustActiveEmitterCount =
            lightDustRenderer.ActiveEmitterCount();
    atmosphereDiagnostics.dustVisibleParticleCount =
            lightDustRenderer.VisibleParticleCount();
}

bool SectorMeshRenderer::ApplyWorldAtmosphere(
        engine::RenderTarget& sceneTarget,
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        bool collectGpuDiagnostics)
{
    BeginAtmosphereGpuFrame(collectGpuDiagnostics);
    if (sceneTarget.descriptor.colorFormat
                    != engine::RenderTargetColorFormat::Rgba16Float
            || sceneTarget.actual.depth
                    != engine::RenderTargetDepthKind::SampleableTexture
            || !EnsureHdrSceneScratch(sceneTarget)) {
        RefreshAtmosphereDiagnostics(SectorBillboardDynamicLightContext{});
        return false;
    }
    RenderTexture2D& nativeScene = sceneTarget.native;
    const SectorBillboardDynamicLightContext dynamicLightContext =
            BuildBillboardDynamicLightContext();
    bool atmosphereFailed = false;
    BeginAtmosphereGpuPass(0);
    const bool distanceFogApplied = distanceFogRenderer.Apply(
            nativeScene, hdrSceneScratch.native, map.fogSettings, camera);
    if (distanceFogApplied && !CommitHdrScratch(sceneTarget)) atmosphereFailed = true;
    EndAtmosphereGpuPass(0);
    if (atmosphereFailed) {
        RefreshAtmosphereDiagnostics(dynamicLightContext);
        return false;
    }

    BeginAtmosphereGpuPass(1);
    bool analyticFogApplied = false;
    if (EnsureHdrSceneColorView(sceneTarget)) {
        analyticFogApplied = analyticFogRenderer.Apply(
                nativeScene,
                hdrSceneColorView,
                map,
                camera,
                runtimeSeconds,
                objectLightProbes);
    }
    EndAtmosphereGpuPass(1);

    BeginAtmosphereGpuPass(2);
    bool analyticShaftApplied = false;
    if (EnsureHdrSceneColorView(sceneTarget)) {
        analyticShaftApplied = analyticLightShaftRenderer.Apply(
                nativeScene, hdrSceneColorView, map.fogSettings,
                camera, dynamicLightContext, lightAtmosphereSources,
                visibilityResult, meshes.sectorReceiverBounds);
    }
    EndAtmosphereGpuPass(2);

    BeginAtmosphereGpuPass(3);
    bool lightHaloApplied = false;
    if (EnsureHdrSceneColorView(sceneTarget)) {
        lightHaloApplied = lightProxyRenderer.Apply(
                nativeScene, hdrSceneColorView, map.fogSettings,
                camera, dynamicLightContext, lightAtmosphereSources,
                visibilityResult, meshes.sectorReceiverBounds);
    }
    EndAtmosphereGpuPass(3);

    BeginAtmosphereGpuPass(4);
    const bool lightDustApplied = lightDustRenderer.Apply(
            nativeScene,
            hdrSceneScratch.native,
            map,
            camera,
            runtimeSeconds,
            objectLightProbes,
            dynamicLightContext,
            lightAtmosphereSources,
            visibilityResult,
            meshes.sectorReceiverBounds);
    EndAtmosphereGpuPass(4);
    RefreshAtmosphereDiagnostics(dynamicLightContext);
    return distanceFogApplied || analyticFogApplied || analyticShaftApplied
            || lightHaloApplied || lightDustApplied;
}

bool SectorMeshRenderer::ApplyHdrBloom(
        engine::RenderTarget& sceneTarget,
        const engine::HdrBloomSettings& settings,
        bool presentFromScratch)
{
    hdrPresentationSource = nullptr;
    if (!initialized || !EnsureHdrSceneScratch(sceneTarget)
            || !bloomRenderer.Apply(sceneTarget, hdrSceneScratch, settings)) {
        return false;
    }
    if (presentFromScratch) {
        hdrPresentationSource = &hdrSceneScratch;
        return true;
    }
    return CommitHdrScratch(sceneTarget);
}

SectorViewPose SectorMeshRenderer::Pose() const
{
    return RendererPose();
}

SectorViewPose SectorMeshRenderer::RendererPose() const
{
    return SectorViewPose{position, yawRadians, pitchRadians, rollRadians};
}

void SectorMeshRenderer::ApplyPose(const SectorViewPose& pose)
{
    ApplyRendererPose(pose);
}

void SectorMeshRenderer::ApplyRendererPose(
        const SectorViewPose& pose,
        bool refreshVisibility)
{
    position = pose.position;
    yawRadians = pose.yawRadians;
    pitchRadians = pose.pitchRadians;
    rollRadians = pose.rollRadians;
    UpdateCamera();
    if (refreshVisibility) {
        UpdateVisibilityDebug();
    }
}

void SectorMeshRenderer::SetVerticalFovDegrees(float value)
{
    if (!std::isfinite(value)) {
        return;
    }
    verticalFovDegrees = std::clamp(value, 1.0f, 179.0f);
    camera.fovy = verticalFovDegrees;
}

void SectorMeshRenderer::RefreshDynamicLightSources(const SectorTopologyMap& map)
{
    dynamicLightState.RebuildSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
    RebuildSectorStaticSpecularLights(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            meshes.sectorReceiverBounds,
            staticSpecularLightState);
    const SectorLightmapStatus currentLightmapStatus =
            GetSectorLightmapStatus(map);
    lightmapStatus = static_cast<int>(currentLightmapStatus);
    surfaceLightmapBakeCurrent =
            currentLightmapStatus == SectorLightmapStatus::Valid
            && !lightmapTextures.empty();
    objectProbeBakeCurrent =
            GetSectorBakedObjectLightProbeStatus(map)
                    == SectorLightmapStatus::Valid;
    BuildSectorLightAtmosphereSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            lightAtmosphereSources);
    analyticFogRenderer.Reserve(map.compiledLocalFogVolumes.size());
    analyticLightShaftRenderer.Reserve(lightAtmosphereSources.size());
    lightProxyRenderer.Reserve(lightAtmosphereSources.size());
    UpdateVisibilityDebug();
}

void SectorMeshRenderer::UpdateVisibilityDebug(
        int preferredStartSectorId,
        float visibilitySeedRadiusWorld,
        bool validateEyeY,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers,
        engine::World* runtimeObjectWorld)
{
    if (!visibilityGraphValid) {
        visibilityResult = RuntimePortalVisibilityResult{};
        visibilityResult.startSectorId = -1;
        visibilityResult.fallbackDrawAll = true;
        visibilityResult.status = "visibility graph unavailable; fallback draw all";
    } else {
        visibilityResult = ComputeRuntimeSectorVisibilityFromView(
                visibilityGraph,
                visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
                Vector2{camera.position.x, camera.position.z},
                PreviewYawForwardXZ(yawRadians),
                VisibilityDebugHorizontalFovRadians(camera, pitchRadians),
                preferredStartSectorId,
                0,
                visibilitySeedRadiusWorld,
                camera.position.y,
                validateEyeY,
                dynamicPortalBlockers);
    }
    portalVisibilityDebugText = FormatRuntimePortalVisibilityDebugText(visibilityResult);
    visibilityDebugText = portalVisibilityDebugText;
    const size_t visibleDrawRecordCount =
            CountSectorMeshDrawRecordsForVisibility(meshes.sectorDrawRecords, visibilityResult);
    dynamicLightState.UpdateSelection(
            visibilityResult,
            preferredStartSectorId,
            meshes.sectorReceiverBounds,
            runtimeObjectWorld,
            visibilityGraphValid ? &visibilityGraph : nullptr,
            dynamicPortalBlockers);
    renderDebugText = "draw records: "
            + std::to_string(visibleDrawRecordCount)
            + " / "
            + std::to_string(meshes.sectorDrawRecords.size());
    renderDebugText += " | "
            + FormatDynamicLightDebugText(
                    dynamicLightingEnabled,
                    dynamicLightState.SelectedLights().size(),
                    dynamicLightState.CandidateCount(),
                    dynamicLightState.SourceCount(),
                    dynamicLightState.SelectedLightKeys());
    const SectorDynamicLightSelectionStats& selectionStats =
            dynamicLightState.SelectionStats();
    renderDebugText += " | lighting component sectors "
            + std::to_string(selectionStats.reachableSectorCount)
            + (selectionStats.reachabilityCacheHit ? " cached" : " rebuilt")
            + " start "
            + std::to_string(selectionStats.lightingStartSectorId)
            + " blockers "
            + std::to_string(selectionStats.dynamicPortalBlockerCount)
            + (selectionStats.cameraVisibilityFallback
                    ? " camera-fallback"
                    : "")
            + " | visible receiver refs "
            + std::to_string(selectionStats.visibleReceiverLightReferences)
            + " total / "
            + std::to_string(selectionStats.maxVisibleReceiverLights)
            + " max across "
            + std::to_string(selectionStats.visibleReceiverCount)
            + " bounds";
    renderDebugText += " | "
            + FormatDynamicSpotLightShadowDebugText(
                    dynamicLightState.ShadowCasters().size(),
                    CountDynamicSpotLightShadowCandidates(dynamicLightState.SelectedLights()),
                    dynamicLightState.ShadowSlotBudget(),
                    dynamicLightState.ShadowCasters(),
                    dynamicLightState.SelectedLights());
    AppendBillboardRenderDebugText(renderDebugText, billboardRenderer.DebugText());
    visibilityDebugText += " | " + renderDebugText;
}

float SectorMeshRenderer::AssetProgress(engine::AssetManager& assets) const
{
    return RendererAssetProgress(assets);
}

float SectorMeshRenderer::RendererAssetProgress(engine::AssetManager& assets) const
{
    return engine::IsNull(assetScope) ? 1.0f : assets.GetScopeProgress(assetScope);
}

const char* SectorMeshRenderer::LightmapStatusText() const
{
    return RendererLightmapStatusText();
}

const char* SectorMeshRenderer::RendererLightmapStatusText() const
{
    return SectorLightmapStatusText(static_cast<SectorLightmapStatus>(lightmapStatus));
}

engine::TextureHandle SectorMeshRenderer::TextureForId(const std::string& textureId) const
{
    const auto it = textureHandlesById.find(textureId);
    if (it == textureHandlesById.end()) {
        return engine::NullTextureHandle();
    }

    return it->second;
}

engine::TextureHandle SectorMeshRenderer::NormalTextureForId(const std::string& textureId) const
{
    const auto it = normalTextureHandlesById.find(textureId);
    if (it == normalTextureHandlesById.end()) {
        return engine::NullTextureHandle();
    }
    return it->second;
}

const Texture2D* SectorMeshRenderer::ResolveShadowCasterTexture(
        void* userData,
        engine::AssetManager& assets,
        const std::string& textureId)
{
    const SectorMeshRenderer* preview = static_cast<const SectorMeshRenderer*>(userData);
    if (preview == nullptr) {
        return nullptr;
    }
    return assets.GetTexture(preview->TextureForId(textureId));
}

void SectorMeshRenderer::UpdateCamera()
{
    const SectorViewPose pose{position, yawRadians, pitchRadians, rollRadians};
    const Vector3 look = SectorViewForward(pose);

    camera.position = position;
    camera.target = Vector3Add(position, look);
    camera.up = SectorViewUp(pose);
}

} // namespace game
