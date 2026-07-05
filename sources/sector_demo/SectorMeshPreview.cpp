#include "sector_demo/SectorMeshPreview.h"

#include "engine/assets/TextureLoadFlags.h"
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
#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace game {

namespace {

constexpr float DefaultVisibilityDebugAspect = 16.0f / 9.0f;
constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
constexpr float DynamicLightingClamp = 4.0f;

int DoorLightingDebugModeShaderValue(SectorDoorLightingDebugMode mode)
{
    return static_cast<int>(mode);
}

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

Vector2 PreviewYawForwardXZ(float yawRadians)
{
    return Vector2{std::cos(yawRadians), std::sin(yawRadians)};
}

float VisibilityDebugHorizontalFovRadians(const Camera3D& camera)
{
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    const float aspect = screenWidth > 0 && screenHeight > 0
            ? static_cast<float>(screenWidth) / static_cast<float>(screenHeight)
            : DefaultVisibilityDebugAspect;
    const float verticalFovRadians = camera.fovy * DegreesToRadians;
    return 2.0f * std::atan(std::tan(verticalFovRadians * 0.5f) * aspect);
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
uniform float useLightmap;
uniform float useBakedAmbientOcclusion;
uniform int hasLightmap;
uniform int alphaTest;
uniform float alphaCutoff;
uniform int hasDecal;
uniform float decalOpacity;
uniform int decalEmissive;
uniform vec3 decalTint;

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
uniform float dynamicLightingClamp;

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
    if (shadowSlot < 0 || shadowSlot >= MAX_DYNAMIC_SHADOW_CASTERS) {
        return 1.0;
    }

    vec4 lightClip = shadowLightMatrices[shadowSlot] * vec4(worldPosition, 1.0);
    if (lightClip.w <= 0.0) {
        return 1.0;
    }

    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec3 shadowCoord = lightNdc * 0.5 + 0.5;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
            shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
            shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        return 1.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap0, 0));
    float normalLightDot = max(dot(
            SafeNormalize(worldNormal, vec3(0.0, 1.0, 0.0)),
            SafeNormalize(surfaceToLightDirection, vec3(0.0, 1.0, 0.0))), 0.0);
    float effectiveBias = min(shadowBias[shadowSlot] * (1.0 + (1.0 - normalLightDot) * 2.0), 0.02);
    float compareDepth = shadowCoord.z - effectiveBias;
    float softness = clamp(shadowSoftness[shadowSlot], 0.0, 8.0);
    if (softness <= 0.0) {
        float shadowDepth = SampleShadowMap(shadowSlot, shadowCoord.xy);
        return compareDepth <= shadowDepth ? 1.0 : 0.0;
    }

    vec2 radius = max(0.25, softness) * texelSize;
    float visible = 0.0;
    for (int i = 0; i < 12; ++i) {
        vec2 sampleUv = clamp(shadowCoord.xy + kPoissonDisk[i] * radius, vec2(0.0), vec2(1.0));
        float shadowDepth = SampleShadowMap(shadowSlot, sampleUv);
        visible += compareDepth <= shadowDepth ? 1.0 : 0.0;
    }
    return visible / 12.0;
}

void main()
{
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
    vec3 worldNormal = SafeNormalize(fragWorldNormal, vec3(0.0, 1.0, 0.0));
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
                int shadowSlot = dynamicLightShadowSlots[i];
                if (shadowSlot >= 0) {
                    float visibility = DynamicSpotLightShadowVisibility(
                            shadowSlot,
                            fragWorldPosition,
                            worldNormal,
                            lightDirection);
                    coneAtten *= mix(1.0, visibility, clamp(shadowStrength[shadowSlot], 0.0, 1.0));
                }
            }
            dynamicDirect += dynamicLightColors[i] * dynamicLightIntensities[i] * atten * ndotl * coneAtten;
        }
    }
    vec3 bakedLighting = clamp(ambient + bakedDirect, 0.0, 1.0);
    vec3 lighting = clamp(bakedLighting + dynamicDirect, 0.0, dynamicLightingClamp);
    vec3 litRgb = surfaceRgb * lighting;
    finalColor = vec4(mix(litRgb, emissiveDecalRgb, emissiveDecalAlpha), baseColor.a * fragColor.a);
}
)";

const char* SectorDoorOpaqueVs = R"(
#version 330
in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
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
    fragColor = vertexColor;
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

#define MAX_DYNAMIC_LIGHTS 8
uniform int dynamicLightCount;
uniform vec3 dynamicLightPositions[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightColors[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightRadii[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightIntensities[MAX_DYNAMIC_LIGHTS];
uniform int dynamicLightTypes[MAX_DYNAMIC_LIGHTS];
uniform vec3 dynamicLightDirections[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightInnerConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightOuterConeCos[MAX_DYNAMIC_LIGHTS];
uniform float dynamicLightingClamp;
uniform int dynamicLightShadowSlots[MAX_DYNAMIC_LIGHTS];

#define MAX_DYNAMIC_SHADOW_CASTERS 2
uniform mat4 shadowLightMatrices[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowBias[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowStrength[MAX_DYNAMIC_SHADOW_CASTERS];
uniform float shadowSoftness[MAX_DYNAMIC_SHADOW_CASTERS];
uniform sampler2D shadowMap0;
uniform sampler2D shadowMap1;

uniform int doorDebugMode;
uniform vec4 doorTint;

#define DOOR_DEBUG_NORMAL 0
#define DOOR_DEBUG_ALBEDO_ONLY 1
#define DOOR_DEBUG_BAKED_ONLY 2
#define DOOR_DEBUG_DYNAMIC_ONLY 3
#define DOOR_DEBUG_NORMAL_VISUALIZE 4
#define DOOR_DEBUG_FLAT_COLOR_NO_TEXTURE 5

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
    if (shadowSlot < 0 || shadowSlot >= MAX_DYNAMIC_SHADOW_CASTERS) {
        return 1.0;
    }

    vec4 lightClip = shadowLightMatrices[shadowSlot] * vec4(worldPosition, 1.0);
    if (lightClip.w <= 0.0) {
        return 1.0;
    }

    vec3 lightNdc = lightClip.xyz / lightClip.w;
    vec3 shadowCoord = lightNdc * 0.5 + 0.5;
    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 ||
            shadowCoord.y < 0.0 || shadowCoord.y > 1.0 ||
            shadowCoord.z < 0.0 || shadowCoord.z > 1.0) {
        return 1.0;
    }

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap0, 0));
    float normalLightDot = max(dot(
            SafeNormalize(worldNormal, vec3(0.0, 1.0, 0.0)),
            SafeNormalize(surfaceToLightDirection, vec3(0.0, 1.0, 0.0))), 0.0);
    float effectiveBias = min(shadowBias[shadowSlot] * (1.0 + (1.0 - normalLightDot) * 2.0), 0.02);
    float compareDepth = shadowCoord.z - effectiveBias;
    float softness = clamp(shadowSoftness[shadowSlot], 0.0, 8.0);
    if (softness <= 0.0) {
        float shadowDepth = SampleShadowMap(shadowSlot, shadowCoord.xy);
        return compareDepth <= shadowDepth ? 1.0 : 0.0;
    }

    vec2 radius = max(0.25, softness) * texelSize;
    float visible = 0.0;
    for (int i = 0; i < 12; ++i) {
        vec2 sampleUv = clamp(shadowCoord.xy + kPoissonDisk[i] * radius, vec2(0.0), vec2(1.0));
        float shadowDepth = SampleShadowMap(shadowSlot, sampleUv);
        visible += compareDepth <= shadowDepth ? 1.0 : 0.0;
    }
    return visible / 12.0;
}

void main()
{
    vec3 worldNormal = SafeNormalize(fragWorldNormal, vec3(0.0, 1.0, 0.0));
    vec3 staticProbeLighting = clamp(fragColor.rgb, 0.0, 1.0);
    vec3 tint = clamp(doorTint.rgb, 0.0, 1.0);

    if (doorDebugMode == DOOR_DEBUG_NORMAL_VISUALIZE) {
        finalColor = vec4(worldNormal * 0.5 + vec3(0.5), 1.0);
        return;
    }
    if (doorDebugMode == DOOR_DEBUG_FLAT_COLOR_NO_TEXTURE) {
        finalColor = vec4(0.18, 0.78, 0.92, 1.0);
        return;
    }
    if (doorDebugMode == DOOR_DEBUG_BAKED_ONLY) {
        finalColor = vec4(staticProbeLighting, 1.0);
        return;
    }
    if (doorDebugMode == DOOR_DEBUG_ALBEDO_ONLY) {
        vec4 sampled = texture(texture0, fragTexCoord);
        finalColor = vec4(sampled.rgb, sampled.a);
        return;
    }

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
                int shadowSlot = dynamicLightShadowSlots[i];
                if (shadowSlot >= 0) {
                    float visibility = DynamicSpotLightShadowVisibility(
                            shadowSlot,
                            fragWorldPosition,
                            worldNormal,
                            lightDirection);
                    coneAtten *= mix(1.0, visibility, clamp(shadowStrength[shadowSlot], 0.0, 1.0));
                }
            }
            dynamicDirect += dynamicLightColors[i] * dynamicLightIntensities[i] * atten * ndotl * coneAtten;
        }
    }

    if (doorDebugMode == DOOR_DEBUG_DYNAMIC_ONLY) {
        finalColor = vec4(clamp(dynamicDirect, 0.0, dynamicLightingClamp) / dynamicLightingClamp, 1.0);
        return;
    }

    vec4 sampled = texture(texture0, fragTexCoord);
    vec3 surfaceRgb = sampled.rgb;
    vec3 lighting = clamp(staticProbeLighting + dynamicDirect, 0.0, dynamicLightingClamp);
    finalColor = vec4(surfaceRgb * tint * lighting, sampled.a * doorTint.a);
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

std::string FormatDynamicLightDebugText(
        bool dynamicLightingEnabled,
        size_t selectedCount,
        size_t candidateCount,
        size_t totalCount,
        const std::vector<int>& selectedIds)
{
    std::ostringstream out;
    out << "dynamic lights: "
            << selectedCount
            << " / "
            << candidateCount
            << " / "
            << totalCount
            << " ("
            << (dynamicLightingEnabled ? "on" : "off")
            << ")";
    if (!selectedIds.empty()) {
        out << " | selected dynamic light ids: ";
        for (size_t i = 0; i < selectedIds.size(); ++i) {
            if (i > 0) {
                out << ",";
            }
            out << selectedIds[i];
        }
    }
    return out.str();
}

size_t CountDynamicSpotLightShadowCandidates(const std::vector<SectorPreviewDynamicPointLightUniform>& selectedLights)
{
    size_t count = 0;
    for (const SectorPreviewDynamicPointLightUniform& light : selectedLights) {
        if (light.kind == SectorPreviewDynamicLightKind::Spot && light.castsShadow) {
            ++count;
        }
    }
    return count;
}

std::string FormatDynamicSpotLightShadowDebugText(
        size_t casterCount,
        size_t candidateCount,
        size_t maxCasterCount,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& casters)
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
            out << casters[i].lightId;
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
        int& alphaTestLoc,
        int& alphaCutoffLoc,
        int& hasDecalLoc,
        int& decalOpacityLoc,
        int& decalEmissiveLoc,
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
        int& dynamicLightShadowSlotsLoc,
        std::array<int, MaxDynamicSpotLightShadowCasters>& shadowLightMatrixLocs,
        int& shadowBiasLoc,
        int& shadowStrengthLoc,
        int& shadowSoftnessLoc,
        int& dynamicLightingClampLoc,
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
    material.shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(material.shader, "shadowMap0");
    material.shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(material.shader, "shadowMap1");
    useLightmapLoc = GetShaderLocation(material.shader, "useLightmap");
    useBakedAmbientOcclusionLoc = GetShaderLocation(material.shader, "useBakedAmbientOcclusion");
    hasLightmapLoc = GetShaderLocation(material.shader, "hasLightmap");
    alphaTestLoc = GetShaderLocation(material.shader, "alphaTest");
    alphaCutoffLoc = GetShaderLocation(material.shader, "alphaCutoff");
    hasDecalLoc = GetShaderLocation(material.shader, "hasDecal");
    decalOpacityLoc = GetShaderLocation(material.shader, "decalOpacity");
    decalEmissiveLoc = GetShaderLocation(material.shader, "decalEmissive");
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
    dynamicLightShadowSlotsLoc = GetShaderLocationArrayBase(material.shader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        shadowLightMatrixLocs[i] = GetShaderLocationArrayElement(material.shader, "shadowLightMatrices", i);
    }
    shadowBiasLoc = GetShaderLocationArrayBase(material.shader, "shadowBias");
    shadowStrengthLoc = GetShaderLocationArrayBase(material.shader, "shadowStrength");
    shadowSoftnessLoc = GetShaderLocationArrayBase(material.shader, "shadowSoftness");
    dynamicLightingClampLoc = GetShaderLocation(material.shader, "dynamicLightingClamp");
    defaultMaterialTexture = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    materialLoaded = true;
    return true;
}

bool LoadDoorOpaqueShader(
        Shader& shader,
        int& textureLoc,
        int& dynamicLightCountLoc,
        int& dynamicLightPositionsLoc,
        int& dynamicLightColorsLoc,
        int& dynamicLightRadiiLoc,
        int& dynamicLightIntensitiesLoc,
        int& dynamicLightTypesLoc,
        int& dynamicLightDirectionsLoc,
        int& dynamicLightInnerConeCosLoc,
        int& dynamicLightOuterConeCosLoc,
        int& dynamicLightShadowSlotsLoc,
        std::array<int, MaxDynamicSpotLightShadowCasters>& shadowLightMatrixLocs,
        int& shadowBiasLoc,
        int& shadowStrengthLoc,
        int& shadowSoftnessLoc,
        int& dynamicLightingClampLoc,
        int& debugModeLoc,
        int& tintLoc,
        bool& shaderLoaded)
{
    shader = LoadShaderFromMemory(SectorDoorOpaqueVs, SectorDoorOpaqueFs);
    if (shader.id == 0) {
        shader = Shader{};
        textureLoc = -1;
        dynamicLightCountLoc = -1;
        dynamicLightPositionsLoc = -1;
        dynamicLightColorsLoc = -1;
        dynamicLightRadiiLoc = -1;
        dynamicLightIntensitiesLoc = -1;
        dynamicLightTypesLoc = -1;
        dynamicLightDirectionsLoc = -1;
        dynamicLightInnerConeCosLoc = -1;
        dynamicLightOuterConeCosLoc = -1;
        dynamicLightShadowSlotsLoc = -1;
        shadowLightMatrixLocs.fill(-1);
        shadowBiasLoc = -1;
        shadowStrengthLoc = -1;
        shadowSoftnessLoc = -1;
        dynamicLightingClampLoc = -1;
        debugModeLoc = -1;
        tintLoc = -1;
        shaderLoaded = false;
        return false;
    }

    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocationAttrib(shader, "vertexNormal");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_VERTEX_COLOR] = GetShaderLocationAttrib(shader, "vertexColor");
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "texture0");
    shader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(shader, "shadowMap0");
    shader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(shader, "shadowMap1");
    textureLoc = shader.locs[SHADER_LOC_MAP_DIFFUSE];
    dynamicLightCountLoc = GetShaderLocation(shader, "dynamicLightCount");
    dynamicLightPositionsLoc = GetShaderLocationArrayBase(shader, "dynamicLightPositions");
    dynamicLightColorsLoc = GetShaderLocationArrayBase(shader, "dynamicLightColors");
    dynamicLightRadiiLoc = GetShaderLocationArrayBase(shader, "dynamicLightRadii");
    dynamicLightIntensitiesLoc = GetShaderLocationArrayBase(shader, "dynamicLightIntensities");
    dynamicLightTypesLoc = GetShaderLocationArrayBase(shader, "dynamicLightTypes");
    dynamicLightDirectionsLoc = GetShaderLocationArrayBase(shader, "dynamicLightDirections");
    dynamicLightInnerConeCosLoc = GetShaderLocationArrayBase(shader, "dynamicLightInnerConeCos");
    dynamicLightOuterConeCosLoc = GetShaderLocationArrayBase(shader, "dynamicLightOuterConeCos");
    dynamicLightShadowSlotsLoc = GetShaderLocationArrayBase(shader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        shadowLightMatrixLocs[i] = GetShaderLocationArrayElement(shader, "shadowLightMatrices", i);
    }
    shadowBiasLoc = GetShaderLocationArrayBase(shader, "shadowBias");
    shadowStrengthLoc = GetShaderLocationArrayBase(shader, "shadowStrength");
    shadowSoftnessLoc = GetShaderLocationArrayBase(shader, "shadowSoftness");
    dynamicLightingClampLoc = GetShaderLocation(shader, "dynamicLightingClamp");
    debugModeLoc = GetShaderLocation(shader, "doorDebugMode");
    tintLoc = GetShaderLocation(shader, "doorTint");
    shaderLoaded = true;
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

const char* SectorDoorLightingDebugModeName(SectorDoorLightingDebugMode mode)
{
    switch (mode) {
        case SectorDoorLightingDebugMode::Normal:
            return "Normal";
        case SectorDoorLightingDebugMode::AlbedoOnly:
            return "AlbedoOnly";
        case SectorDoorLightingDebugMode::BakedOnly:
            return "BakedOnly";
        case SectorDoorLightingDebugMode::DynamicOnly:
            return "DynamicOnly";
        case SectorDoorLightingDebugMode::NormalVisualize:
            return "NormalVisualize";
        case SectorDoorLightingDebugMode::FlatColorNoTexture:
            return "FlatColorNoTexture";
    }
    return "Normal";
}

bool SectorMeshPreview::Rebuild(
        engine::AssetManager& assets,
        const SectorTopologyMap& map,
        const char* scopeName,
        std::string& error)
{
    return RebuildRendererResources(assets, map, scopeName, error);
}

bool SectorMeshPreview::RebuildRendererResources(
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
                SectorTextureLoadFlags(texture.filter));
        textureHandlesById.emplace(texture.id, handle);
    }

    if (ShouldRenderSkyCylinder(map)) {
        const SectorTextureDefinition* skyTexture = FindSkyTexture(map);
        const engine::TextureHandle skyTextureHandle = skyTexture == nullptr
                ? engine::NullTextureHandle()
                : TextureForId(skyTexture->id);
        skyRenderer.Rebuild(map, skyTextureHandle);
    }

    SectorLightmapLayout lightmapLayout;
    const SectorLightmapStatus status = GetSectorLightmapStatus(map);
    lightmapStatus = static_cast<int>(status);
    const bool useLightmapLayout = status == SectorLightmapStatus::Valid
            && BuildSectorLightmapLayout(map, lightmapLayout, error);
    if (status == SectorLightmapStatus::Valid && !useLightmapLayout) {
        std::fprintf(stderr, "[SectorDemo WARNING] %s\n", error.c_str());
        error.clear();
    }

    if (useLightmapLayout) {
        const std::string resolvedPath = ResolveSectorAssetPath(map.bakedLightmap.path);
        lightmapTexture = assets.RequestTexture(
                assetScope,
                "sector_lightmap_atlas",
                resolvedPath.c_str(),
                engine::TextureLoad_BilinearFilter);
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

    dynamicLightState.RebuildSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
    doorRenderer.ReserveRuntimeDoorCapacity(kSectorRuntimeObjectInitialCapacity);
    runtimeSeconds = 0.0f;

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

    if (!billboardRenderer.Load()) {
        Shutdown(assets);
        error = "Preview failed: could not load billboard cutout shader";
        return false;
    }

    if (!LoadDoorOpaqueShader(
                doorOpaqueShader,
                doorOpaqueTextureLoc,
                doorOpaqueDynamicLightCountLoc,
                doorOpaqueDynamicLightPositionsLoc,
                doorOpaqueDynamicLightColorsLoc,
                doorOpaqueDynamicLightRadiiLoc,
                doorOpaqueDynamicLightIntensitiesLoc,
                doorOpaqueDynamicLightTypesLoc,
                doorOpaqueDynamicLightDirectionsLoc,
                doorOpaqueDynamicLightInnerConeCosLoc,
                doorOpaqueDynamicLightOuterConeCosLoc,
                doorOpaqueDynamicLightShadowSlotsLoc,
                doorOpaqueShadowLightMatrixLocs,
                doorOpaqueShadowBiasLoc,
                doorOpaqueShadowStrengthLoc,
                doorOpaqueShadowSoftnessLoc,
                doorOpaqueDynamicLightingClampLoc,
                doorOpaqueDebugModeLoc,
                doorOpaqueTintLoc,
                doorOpaqueShaderLoaded)) {
        Shutdown(assets);
        error = "Preview failed: could not load door opaque shader";
        return false;
    }
    doorOpaqueMaterial = LoadMaterialDefault();
    doorOpaqueDefaultMaterialTexture = doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    doorOpaqueMaterial.shader = doorOpaqueShader;
    doorOpaqueMaterialLoaded = true;

    if (!LoadPreviewMaterial(
                material,
                defaultMaterialTexture,
                materialLoaded,
                useLightmapLoc,
                useBakedAmbientOcclusionLoc,
                hasLightmapLoc,
                alphaTestLoc,
                alphaCutoffLoc,
                hasDecalLoc,
                decalOpacityLoc,
                decalEmissiveLoc,
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
                dynamicLightShadowSlotsLoc,
                shadowLightMatrixLocs,
                shadowBiasLoc,
                shadowStrengthLoc,
                shadowSoftnessLoc,
                dynamicLightingClampLoc,
                error)) {
        Shutdown(assets);
        return false;
    }

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
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    UpdateCamera();
    UpdateVisibilityDebug();

    initialized = true;
    return true;
}

void SectorMeshPreview::Shutdown(engine::AssetManager& assets)
{
    ShutdownRendererResources(assets);
}

void SectorMeshPreview::ShutdownRendererResources(engine::AssetManager& assets)
{
    generatedGeometry = {};
    visibilityGraph = {};
    visibilityResult = {};
    portalVisibilityDebugText.clear();
    visibilityDebugText.clear();
    renderDebugText.clear();
    billboardRenderer.ResetDebugState();
    visibilityLookupWorld = SectorCollisionWorld{};
    visibilityGraphValid = false;
    visibilityLookupWorldValid = false;
    dynamicLightState.Reset();
    doorRenderer.ClearPreparedShadowCasters();
    runtimeSeconds = 0.0f;
    if (!initialized
            && engine::IsNull(assetScope)
            && meshes.batches.empty()
            && meshes.sectorDrawRecords.empty()
            && !materialLoaded
            && !bloomRenderer.IsLoaded()
            && !billboardRenderer.IsLoaded()
            && !doorOpaqueShaderLoaded
            && !doorOpaqueMaterialLoaded
            && !doorRenderer.HasCachedDoorMeshes()
            && !dynamicLightState.HasShadowMapResources()
            && !dynamicLightState.HasShadowMaterial()
            && !skyRenderer.IsLoaded()) {
        return;
    }

    bloomRenderer.Shutdown();
    dynamicLightState.UnloadShadowMaterial();
    dynamicLightState.UnloadShadowMapResources();
    skyRenderer.Shutdown();
    doorRenderer.UnloadDoorMeshes();
    UnloadSectorMeshes(meshes);
    textureHandlesById.clear();
    lightmapTexture = engine::NullTextureHandle();
    sectorCount = 0;

    if (materialLoaded) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = defaultMaterialTexture;
        material.maps[MATERIAL_MAP_SPECULAR].texture = Texture2D{};
        material.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
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

    billboardRenderer.Shutdown();

    if (doorOpaqueMaterialLoaded) {
        doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = doorOpaqueDefaultMaterialTexture;
        doorOpaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
        doorOpaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
        UnloadMaterial(doorOpaqueMaterial);
        doorOpaqueMaterial = Material{};
        doorOpaqueDefaultMaterialTexture = Texture2D{};
        doorOpaqueShader = Shader{};
        doorOpaqueTextureLoc = -1;
        doorOpaqueDynamicLightCountLoc = -1;
        doorOpaqueDynamicLightPositionsLoc = -1;
        doorOpaqueDynamicLightColorsLoc = -1;
        doorOpaqueDynamicLightRadiiLoc = -1;
        doorOpaqueDynamicLightIntensitiesLoc = -1;
        doorOpaqueDynamicLightTypesLoc = -1;
        doorOpaqueDynamicLightDirectionsLoc = -1;
        doorOpaqueDynamicLightInnerConeCosLoc = -1;
        doorOpaqueDynamicLightOuterConeCosLoc = -1;
        doorOpaqueDynamicLightShadowSlotsLoc = -1;
        doorOpaqueShadowLightMatrixLocs.fill(-1);
        doorOpaqueShadowBiasLoc = -1;
        doorOpaqueShadowStrengthLoc = -1;
        doorOpaqueShadowSoftnessLoc = -1;
        doorOpaqueDynamicLightingClampLoc = -1;
        doorOpaqueDebugModeLoc = -1;
        doorOpaqueTintLoc = -1;
        doorOpaqueMaterialLoaded = false;
        doorOpaqueShaderLoaded = false;
    }

    if (!engine::IsNull(assetScope)) {
        assets.UnloadScope(assetScope);
        assetScope = engine::NullAssetScopeHandle();
    }

    initialized = false;
}

void SectorMeshPreview::AdvanceRuntime(float dt)
{
    if (std::isfinite(dt) && dt > 0.0f) {
        runtimeSeconds += dt;
    }
}

void SectorMeshPreview::Render(
        engine::AssetManager& assets,
        bool useBakedAmbientOcclusion,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting)
{
    RenderDynamicSpotLightShadowMaps(assets, runtimeObjectWorld);
    DrawScene(assets, useBakedAmbientOcclusion, runtimeObjectWorld, doorLighting);
}

void SectorMeshPreview::DrawScene(
        engine::AssetManager& assets,
        bool useBakedAmbientOcclusion,
        engine::World* runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting)
{
    if (!initialized) {
        return;
    }

    BeginMode3D(camera);
    skyRenderer.Draw(assets, camera);

    const Texture2D* lightmap = assets.GetTexture(lightmapTexture);
    float useLightmap = lightmap != nullptr ? 1.0f : 0.0f;
    float useAo = useBakedAmbientOcclusion ? 1.0f : 0.0f;
    material.maps[MATERIAL_MAP_SPECULAR].texture = (lightmap != nullptr)
            ? *lightmap
            : Texture2D{};
    const Texture2D* shadowMap0 = dynamicLightState.ShadowMapDepthTexture(0);
    const Texture2D* shadowMap1 = dynamicLightState.ShadowMapDepthTexture(1);
    material.maps[MATERIAL_MAP_ROUGHNESS].texture = shadowMap0 != nullptr ? *shadowMap0 : Texture2D{};
    material.maps[MATERIAL_MAP_OCCLUSION].texture = shadowMap1 != nullptr ? *shadowMap1 : Texture2D{};
    if (useLightmapLoc >= 0) {
        SetShaderValue(material.shader, useLightmapLoc, &useLightmap, SHADER_UNIFORM_FLOAT);
    }
    if (useBakedAmbientOcclusionLoc >= 0) {
        SetShaderValue(material.shader, useBakedAmbientOcclusionLoc, &useAo, SHADER_UNIFORM_FLOAT);
    }
    SectorPreviewDynamicLightShaderLocations dynamicLightLocations;
    dynamicLightLocations.dynamicLightCount = dynamicLightCountLoc;
    dynamicLightLocations.dynamicLightPositions = dynamicLightPositionsLoc;
    dynamicLightLocations.dynamicLightColors = dynamicLightColorsLoc;
    dynamicLightLocations.dynamicLightRadii = dynamicLightRadiiLoc;
    dynamicLightLocations.dynamicLightIntensities = dynamicLightIntensitiesLoc;
    dynamicLightLocations.dynamicLightTypes = dynamicLightTypesLoc;
    dynamicLightLocations.dynamicLightDirections = dynamicLightDirectionsLoc;
    dynamicLightLocations.dynamicLightInnerConeCos = dynamicLightInnerConeCosLoc;
    dynamicLightLocations.dynamicLightOuterConeCos = dynamicLightOuterConeCosLoc;
    dynamicLightLocations.dynamicLightingClamp = dynamicLightingClampLoc;
    UploadSectorPreviewDynamicPointLights(
            material.shader,
            dynamicLightLocations,
            dynamicLightingEnabled,
            runtimeSeconds,
            dynamicLightState.SelectedLights());
    const SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms =
            dynamicLightState.PackShadowUniforms();
    SectorPreviewDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    UploadSectorPreviewDynamicSpotLightShadowUniforms(material.shader, shadowLocations, shadowUniforms);
    for (const SectorMeshBatch& batch : meshes.sectorDrawRecords) {
        if (!ShouldDrawSectorMeshRecordForVisibility(batch, visibilityResult)) {
            continue;
        }

        const engine::TextureHandle textureHandle = TextureForId(batch.textureId);
        const Texture2D* texture = assets.GetTexture(textureHandle);
        material.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                ? *texture
                : defaultMaterialTexture;

        const Texture2D* decalTexture = nullptr;
        if (!batch.decalTextureId.empty()) {
            decalTexture = assets.GetTexture(TextureForId(batch.decalTextureId));
        }

        const int hasDecal = decalTexture != nullptr ? 1 : 0;
        const int hasLightmap = batch.receivesLightmap ? 1 : 0;
        const int alphaTest = batch.alphaTest ? 1 : 0;
        const float alphaCutoff = batch.alphaCutoff;
        const float decalOpacity = batch.decalOpacity;
        const int decalEmissive = hasDecal != 0 && batch.decalEmissive ? 1 : 0;
        const Vector3 decalTint = hasDecal != 0 ? batch.decalTint : Vector3{1.0f, 1.0f, 1.0f};
        material.maps[MATERIAL_MAP_NORMAL].texture = (decalTexture != nullptr)
                ? *decalTexture
                : Texture2D{};
        if (hasLightmapLoc >= 0) {
            SetShaderValue(material.shader, hasLightmapLoc, &hasLightmap, SHADER_UNIFORM_INT);
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
        if (decalTintLoc >= 0) {
            SetShaderValue(material.shader, decalTintLoc, &decalTint, SHADER_UNIFORM_VEC3);
        }
        DrawMesh(batch.mesh, material, MatrixIdentity());
    }
    if (runtimeObjectWorld != nullptr) {
        DrawRuntimeDoors(assets, *runtimeObjectWorld, doorLighting);
        const SectorPreviewBillboardDynamicLightContext billboardLightContext = BuildBillboardDynamicLightContext();
        billboardRenderer.Draw(assets, *runtimeObjectWorld, camera, billboardLightContext, renderDebugText);
    }
    EndMode3D();
}

void SectorMeshPreview::DrawRuntimeDoors(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld,
        SectorRuntimeDoorLightingContext doorLighting)
{
    if (!doorOpaqueMaterialLoaded || !doorOpaqueShaderLoaded || doorOpaqueMaterial.shader.id == 0) {
        doorRenderStats = {};
        AppendDoorRenderDebugText(renderDebugText, "doors: shader unavailable");
        return;
    }

    doorRenderer.PrepareRuntimeDoorMeshes(runtimeObjectWorld);

    size_t consideredCount = 0;
    size_t drawnCount = 0;
    size_t skippedCount = 0;
    const SectorBakedObjectLightProbeRuntimeData emptyObjectLightProbes;
    const SectorBakedObjectLightProbeRuntimeData& objectLightProbes =
            doorLighting.objectLightProbes != nullptr
            ? *doorLighting.objectLightProbes
            : emptyObjectLightProbes;

    rlDisableColorBlend();
    rlDisableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
    SectorPreviewDynamicLightShaderLocations dynamicLightLocations;
    dynamicLightLocations.dynamicLightCount = doorOpaqueDynamicLightCountLoc;
    dynamicLightLocations.dynamicLightPositions = doorOpaqueDynamicLightPositionsLoc;
    dynamicLightLocations.dynamicLightColors = doorOpaqueDynamicLightColorsLoc;
    dynamicLightLocations.dynamicLightRadii = doorOpaqueDynamicLightRadiiLoc;
    dynamicLightLocations.dynamicLightIntensities = doorOpaqueDynamicLightIntensitiesLoc;
    dynamicLightLocations.dynamicLightTypes = doorOpaqueDynamicLightTypesLoc;
    dynamicLightLocations.dynamicLightDirections = doorOpaqueDynamicLightDirectionsLoc;
    dynamicLightLocations.dynamicLightInnerConeCos = doorOpaqueDynamicLightInnerConeCosLoc;
    dynamicLightLocations.dynamicLightOuterConeCos = doorOpaqueDynamicLightOuterConeCosLoc;
    dynamicLightLocations.dynamicLightingClamp = doorOpaqueDynamicLightingClampLoc;
    UploadSectorPreviewDynamicPointLights(
            doorOpaqueMaterial.shader,
            dynamicLightLocations,
            dynamicLightingEnabled,
            runtimeSeconds,
            dynamicLightState.SelectedLights());
    const SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms =
            dynamicLightState.PackShadowUniforms();
    SectorPreviewDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = doorOpaqueDynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = doorOpaqueShadowLightMatrixLocs;
    shadowLocations.shadowBias = doorOpaqueShadowBiasLoc;
    shadowLocations.shadowStrength = doorOpaqueShadowStrengthLoc;
    shadowLocations.shadowSoftness = doorOpaqueShadowSoftnessLoc;
    UploadSectorPreviewDynamicSpotLightShadowUniforms(doorOpaqueMaterial.shader, shadowLocations, shadowUniforms);
    const Texture2D* shadowMap0 = dynamicLightState.ShadowMapDepthTexture(0);
    const Texture2D* shadowMap1 = dynamicLightState.ShadowMapDepthTexture(1);
    doorOpaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = shadowMap0 != nullptr ? *shadowMap0 : Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = shadowMap1 != nullptr ? *shadowMap1 : Texture2D{};
    if (doorOpaqueDebugModeLoc >= 0) {
        const int debugMode = DoorLightingDebugModeShaderValue(doorLightingDebugMode);
        SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueDebugModeLoc, &debugMode, SHADER_UNIFORM_INT);
    }
    if (doorOpaqueTextureLoc >= 0) {
        const int diffuseTextureUnit = 0;
        SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueTextureLoc, &diffuseTextureUnit, SHADER_UNIFORM_INT);
    }

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender>(
            [this,
             &assets,
             &consideredCount,
             &drawnCount,
             &skippedCount,
             &objectLightProbes,
             doorLighting](
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
                if (render.width <= 0.0f || render.height <= 0.0f || render.thickness <= 0.0f) {
                    ++skippedCount;
                    return;
                }

                const engine::TextureHandle textureHandle = !render.textureId.empty()
                        ? TextureForId(render.textureId)
                        : engine::NullTextureHandle();
                const Texture2D* texture = assets.GetTexture(textureHandle);
                if (texture == nullptr) {
                    texture = &defaultMaterialTexture;
                }
                if (texture == nullptr || texture->id == 0) {
                    ++skippedCount;
                    return;
                }

                SectorPreviewDoorRenderer::DoorMeshCacheEntry* cacheEntry =
                        doorRenderer.FindMutableDoorMesh(door.placedObjectId);
                if (cacheEntry == nullptr || cacheEntry->mesh.vertexCount <= 0) {
                    ++skippedCount;
                    return;
                }

                if (!BuildSectorDoorStaticLightingColors(
                            cacheEntry->meshData,
                            transform,
                            object,
                            anchor,
                            objectLightProbes,
                            doorLighting.mapForFallback,
                            cacheEntry->staticLightingColors)) {
                    cacheEntry->staticLightingColors.assign(
                            static_cast<size_t>(cacheEntry->mesh.vertexCount),
                            WHITE);
                }
                if (cacheEntry->mesh.colors != nullptr
                        && cacheEntry->staticLightingColors.size() == static_cast<size_t>(cacheEntry->mesh.vertexCount)) {
                    for (int i = 0; i < cacheEntry->mesh.vertexCount; ++i) {
                        const Color color = cacheEntry->staticLightingColors[static_cast<size_t>(i)];
                        cacheEntry->mesh.colors[i * 4 + 0] = color.r;
                        cacheEntry->mesh.colors[i * 4 + 1] = color.g;
                        cacheEntry->mesh.colors[i * 4 + 2] = color.b;
                        cacheEntry->mesh.colors[i * 4 + 3] = color.a;
                    }
                    UpdateMeshBuffer(
                            cacheEntry->mesh,
                            RL_DEFAULT_SHADER_ATTRIB_LOCATION_COLOR,
                            cacheEntry->mesh.colors,
                            cacheEntry->mesh.vertexCount * 4 * static_cast<int>(sizeof(unsigned char)),
                            0);
                }

                if (doorOpaqueTintLoc >= 0) {
                    const Vector4 tint{
                            static_cast<float>(render.tint.r) / 255.0f,
                            static_cast<float>(render.tint.g) / 255.0f,
                            static_cast<float>(render.tint.b) / 255.0f,
                            static_cast<float>(render.tint.a) / 255.0f};
                    SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueTintLoc, &tint, SHADER_UNIFORM_VEC4);
                }

                doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = *texture;
                doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                DrawMesh(
                        cacheEntry->mesh,
                        doorOpaqueMaterial,
                        BuildSectorDoorSlabModelMatrix(transform, anchor));
                ++drawnCount;
            });

    doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = doorOpaqueDefaultMaterialTexture;
    doorOpaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
    rlActiveTextureSlot(0);
    rlSetTexture(0);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();

    doorRenderStats.considered = consideredCount;
    doorRenderStats.drawn = drawnCount;
    doorRenderStats.skipped = skippedCount;
    AppendDoorRenderDebugText(
            renderDebugText,
            "doors: "
                    + std::to_string(drawnCount)
                    + " drawn / "
                    + std::to_string(consideredCount)
                    + " considered, "
                    + std::to_string(skippedCount)
                    + " skipped");
}

SectorPreviewBillboardDynamicLightContext SectorMeshPreview::BuildBillboardDynamicLightContext() const
{
    SectorPreviewBillboardDynamicLightContext context;
    context.dynamicLightCount = dynamicLightingEnabled
            ? static_cast<int>(std::min(dynamicLightState.SelectedLights().size(), static_cast<size_t>(MaxDynamicLights)))
            : 0;
    context.dynamicLightingClamp = DynamicLightingClamp;
    context.shadowUniforms = dynamicLightState.PackShadowUniforms();
    context.shadowMaps = dynamicLightState.BuildShadowMapTextures();

    for (int i = 0; i < context.dynamicLightCount; ++i) {
        const SectorPreviewDynamicPointLightUniform& light =
                dynamicLightState.SelectedLights()[static_cast<size_t>(i)];
        context.dynamicLightPositions[static_cast<size_t>(i)] = light.position;
        context.dynamicLightColors[static_cast<size_t>(i)] = light.color;
        context.dynamicLightRadii[static_cast<size_t>(i)] = light.radius;
        context.dynamicLightIntensities[static_cast<size_t>(i)] = DynamicLightEffectiveUploadIntensity(
                light,
                runtimeSeconds);
        context.dynamicLightTypes[static_cast<size_t>(i)] = static_cast<int>(light.kind);
        context.dynamicLightDirections[static_cast<size_t>(i)] = light.direction;
        context.dynamicLightInnerConeCos[static_cast<size_t>(i)] = light.innerConeCos;
        context.dynamicLightOuterConeCos[static_cast<size_t>(i)] = light.outerConeCos;
    }

    return context;
}

void SectorMeshPreview::RenderDynamicSpotLightShadowMaps(
        engine::AssetManager& assets,
        engine::World* runtimeObjectWorld)
{
    if (!dynamicLightState.IsShadowRenderReady()) {
        return;
    }

    if (runtimeObjectWorld != nullptr) {
        doorRenderer.PrepareRuntimeDoorMeshes(*runtimeObjectWorld);
    } else {
        doorRenderer.ClearPreparedShadowCasters();
    }

    SectorPreviewDynamicSpotLightShadowRenderContext context;
    context.assets = &assets;
    context.sectorDrawRecords = &meshes.sectorDrawRecords;
    context.doorShadowCasters = &doorRenderer.ShadowCasters();
    context.userData = this;
    context.textureResolver = &SectorMeshPreview::ResolveShadowCasterTexture;
    context.doorMeshResolver = &SectorMeshPreview::ResolveDoorShadowCasterMesh;
    dynamicLightState.RenderShadowMaps(context);
}

void SectorMeshPreview::ApplyEmissiveDecalBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget)
{
    ApplyEmissiveDecalBloomToScene(assets, sceneTarget);
}

void SectorMeshPreview::ApplyEmissiveDecalBloomToScene(engine::AssetManager& assets, RenderTexture2D& sceneTarget)
{
    bloomRenderer.ApplyEmissiveDecalBloomToScene(
            assets,
            initialized,
            camera,
            meshes.sectorDrawRecords,
            visibilityResult,
            textureHandlesById,
            sceneTarget);
}

SectorViewPose SectorMeshPreview::Pose() const
{
    return RendererPose();
}

SectorViewPose SectorMeshPreview::RendererPose() const
{
    return SectorViewPose{position, yawRadians, pitchRadians};
}

void SectorMeshPreview::ApplyPose(const SectorViewPose& pose)
{
    ApplyRendererPose(pose);
}

void SectorMeshPreview::ApplyRendererPose(const SectorViewPose& pose)
{
    position = pose.position;
    yawRadians = pose.yawRadians;
    pitchRadians = pose.pitchRadians;
    UpdateCamera();
    UpdateVisibilityDebug();
}

void SectorMeshPreview::RefreshDynamicLightSources(const SectorTopologyMap& map)
{
    dynamicLightState.RebuildSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
    UpdateVisibilityDebug();
}

void SectorMeshPreview::UpdateVisibilityDebug(
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
                VisibilityDebugHorizontalFovRadians(camera),
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
    dynamicLightState.UpdateSelection(visibilityResult, meshes.sectorReceiverBounds, runtimeObjectWorld);
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
                    dynamicLightState.SelectedLightIds());
    renderDebugText += " | "
            + FormatDynamicSpotLightShadowDebugText(
                    dynamicLightState.ShadowCasters().size(),
                    CountDynamicSpotLightShadowCandidates(dynamicLightState.SelectedLights()),
                    MaxDynamicSpotLightShadowCasters,
                    dynamicLightState.ShadowCasters());
    AppendBillboardRenderDebugText(renderDebugText, billboardRenderer.DebugText());
    visibilityDebugText += " | " + renderDebugText;
}

float SectorMeshPreview::AssetProgress(engine::AssetManager& assets) const
{
    return RendererAssetProgress(assets);
}

float SectorMeshPreview::RendererAssetProgress(engine::AssetManager& assets) const
{
    return engine::IsNull(assetScope) ? 1.0f : assets.GetScopeProgress(assetScope);
}

const char* SectorMeshPreview::LightmapStatusText() const
{
    return RendererLightmapStatusText();
}

const char* SectorMeshPreview::RendererLightmapStatusText() const
{
    return SectorLightmapStatusText(static_cast<SectorLightmapStatus>(lightmapStatus));
}

engine::TextureHandle SectorMeshPreview::TextureForId(const std::string& textureId) const
{
    const auto it = textureHandlesById.find(textureId);
    if (it == textureHandlesById.end()) {
        return engine::NullTextureHandle();
    }

    return it->second;
}

const Texture2D* SectorMeshPreview::ResolveShadowCasterTexture(
        void* userData,
        engine::AssetManager& assets,
        const std::string& textureId)
{
    const SectorMeshPreview* preview = static_cast<const SectorMeshPreview*>(userData);
    if (preview == nullptr) {
        return nullptr;
    }
    return assets.GetTexture(preview->TextureForId(textureId));
}

const Mesh* SectorMeshPreview::ResolveDoorShadowCasterMesh(
        void* userData,
        const SectorDoorShadowCaster& caster,
        float& outWidth,
        float& outHeight)
{
    const SectorMeshPreview* preview = static_cast<const SectorMeshPreview*>(userData);
    if (preview == nullptr) {
        return nullptr;
    }
    return preview->doorRenderer.ResolveDoorShadowCasterMesh(caster, outWidth, outHeight);
}

void SectorMeshPreview::UpdateCamera()
{
    const float cosPitch = std::cos(pitchRadians);
    const Vector3 look{
            std::cos(yawRadians) * cosPitch,
            std::sin(pitchRadians),
            std::sin(yawRadians) * cosPitch
    };

    camera.position = position;
    camera.target = Vector3Add(position, look);
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
}

} // namespace game
