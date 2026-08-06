#include "sector_demo/renderer/SectorMeshRenderer.h"

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
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace game {

namespace {

constexpr float DefaultVisibilityDebugAspect = 16.0f / 9.0f;
constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
constexpr float DynamicLightingClamp = 4.0f;

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
uniform vec3 decalTint;

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

vec3 ApplySectorFog(vec3 surfaceRgb, vec3 worldPosition)
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
    return mix(surfaceRgb, fogColor, fogAmount);
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
    vec3 geometricNormal = SafeNormalize(fragWorldNormal, vec3(0.0, 1.0, 0.0));
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
                            geometricNormal,
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
    vec3 surfaceOutput = mix(litRgb, emissiveDecalRgb, emissiveDecalAlpha);
    finalColor = vec4(ApplySectorFog(surfaceOutput, fragWorldPosition), baseColor.a * fragColor.a);
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
        int& hasNormalMapLoc,
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
            useLightmapLayout = false;
            lightmapStatus =
                    static_cast<int>(SectorLightmapStatus::Stale);
            staticModelLightmapData = {};
        } else if (!AreSectorStaticModelLightmapAtlasIndicesValid(
                           staticModelLightmapData,
                           static_cast<int>(lightmapAtlases.size()))) {
            std::fprintf(
                    stderr,
                    "[SectorDemo WARNING] Static model lightmap disabled: atlas index is outside installed metadata\n");
            useLightmapLayout = false;
            lightmapStatus =
                    static_cast<int>(SectorLightmapStatus::Stale);
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
            lightmapTextures.push_back(assets.RequestTexture(
                    assetScope,
                    key.c_str(),
                    resolvedPath.c_str(),
                    engine::TextureLoad_BilinearFilter));
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

    dynamicLightState.RebuildSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
    BuildSectorLightAtmosphereSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            lightAtmosphereSources);
    doorRenderer.ReserveRuntimeDoorCapacity(kSectorRuntimeObjectInitialCapacity);
    runtimeSeconds = 0.0f;
    localFogRenderer.Shutdown();
    lightHazeRenderer.Shutdown();
    lightDustRenderer.Shutdown();

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
                fogShaderLocations,
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

void SectorMeshRenderer::Shutdown(engine::AssetManager& assets)
{
    ShutdownRendererResources(assets);
}

void SectorMeshRenderer::ShutdownRendererResources(engine::AssetManager& assets)
{
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
    lightAtmosphereSources.clear();
    doorRenderer.ClearPreparedShadowCasters();
    runtimeSeconds = 0.0f;
    localFogRenderer.Shutdown();
    lightHazeRenderer.Shutdown();
    lightDustRenderer.Shutdown();
    if (!initialized
            && engine::IsNull(assetScope)
            && meshes.batches.empty()
            && meshes.sectorDrawRecords.empty()
            && !materialLoaded
            && !bloomRenderer.IsLoaded()
            && !billboardRenderer.IsLoaded()
            && !staticModelRenderer.IsLoaded()
            && !doorRenderer.HasOpaqueResources()
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

    const SectorFogRenderContext fogContext =
            BuildSectorFogRenderContext(fogSettings, camera.position);
    UploadSectorFogShaderValues(material.shader, fogShaderLocations, fogContext);

    float useAo = useBakedAmbientOcclusion ? 1.0f : 0.0f;
    const Texture2D* shadowMap0 = dynamicLightState.ShadowMapDepthTexture(0);
    const Texture2D* shadowMap1 = dynamicLightState.ShadowMapDepthTexture(1);
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
    dynamicLightLocations.dynamicLightingClamp = dynamicLightingClampLoc;
    UploadSectorRendererDynamicPointLights(
            material.shader,
            dynamicLightLocations,
            dynamicLightingEnabled,
            runtimeSeconds,
            dynamicLightState.SelectedLights());
    const SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms =
            dynamicLightState.PackShadowUniforms();
    SectorDynamicSpotLightShadowShaderLocations shadowLocations;
    shadowLocations.dynamicLightShadowSlots = dynamicLightShadowSlotsLoc;
    shadowLocations.shadowLightMatrices = shadowLightMatrixLocs;
    shadowLocations.shadowBias = shadowBiasLoc;
    shadowLocations.shadowStrength = shadowStrengthLoc;
    shadowLocations.shadowSoftness = shadowSoftnessLoc;
    UploadSectorRendererDynamicSpotLightShadowUniforms(material.shader, shadowLocations, shadowUniforms);
    for (const SectorMeshBatch& batch : meshes.sectorDrawRecords) {
        if (!ShouldDrawSectorMeshRecordForVisibility(batch, visibilityResult)) {
            continue;
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
        const Vector3 decalTint = hasDecal != 0 ? batch.decalTint : Vector3{1.0f, 1.0f, 1.0f};
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
        doorDrawContext.dynamicLighting.shadowUniforms = dynamicLightState.PackShadowUniforms();
        doorDrawContext.dynamicLighting.shadowMaps = dynamicLightState.BuildShadowMapTextures();
        doorDrawContext.dynamicLighting.lightingClamp = DynamicLightingClamp;
        doorDrawContext.fog = fogContext;
        doorDrawContext.textureResolver.userData = this;
        doorDrawContext.textureResolver.resolve = &SectorMeshRenderer::ResolveShadowCasterTexture;
        doorDrawContext.defaultMaterialTexture = &defaultMaterialTexture;
        doorDrawContext.renderDebugText = &renderDebugText;
        doorRenderer.Draw(doorDrawContext);

        const SectorBillboardDynamicLightContext billboardLightContext = BuildBillboardDynamicLightContext();
        staticModelRenderer.Draw(
                assets,
                *runtimeObjectWorld,
                camera,
                billboardLightContext,
                fogContext,
                visibilityResult,
                lightmapTextures,
                assets.GetCubemap(pbrEnvironment.cubemap),
                useBakedAmbientOcclusion,
                renderDebugText);
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

SectorBillboardDynamicLightContext SectorMeshRenderer::BuildBillboardDynamicLightContext() const
{
    SectorBillboardDynamicLightContext context;
    context.dynamicLightCount = dynamicLightingEnabled
            ? static_cast<int>(std::min(dynamicLightState.SelectedLights().size(), static_cast<size_t>(MaxDynamicLights)))
            : 0;
    context.dynamicLightingClamp = DynamicLightingClamp;
    context.shadowUniforms = dynamicLightState.PackShadowUniforms();
    context.shadowMaps = dynamicLightState.BuildShadowMapTextures();

    for (int i = 0; i < context.dynamicLightCount; ++i) {
        const SectorPreviewDynamicPointLightUniform& light =
                dynamicLightState.SelectedLights()[static_cast<size_t>(i)];
        context.dynamicLightIds[static_cast<size_t>(i)] = light.lightId;
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

void SectorMeshRenderer::RenderDynamicSpotLightShadowMaps(
        engine::AssetManager& assets,
        engine::World* runtimeObjectWorld)
{
    if (!dynamicLightState.IsShadowRenderReady()) {
        return;
    }

    SectorDynamicSpotLightShadowRenderContext context;
    context.assets = &assets;
    context.sectorDrawRecords = &meshes.sectorDrawRecords;
    context.userData = this;
    context.textureResolver = &SectorMeshRenderer::ResolveShadowCasterTexture;
    doorRenderer.PrepareShadowRenderContext(context, runtimeObjectWorld);
    dynamicLightState.RenderShadowMaps(context);
}

void SectorMeshRenderer::ApplyEmissiveDecalBloom(
        engine::AssetManager& assets,
        RenderTexture2D& sceneTarget,
        const SectorTopologyFogSettings& fogSettings)
{
    ApplyEmissiveDecalBloomToScene(assets, sceneTarget, fogSettings);
}

void SectorMeshRenderer::ApplyEmissiveDecalBloomToScene(
        engine::AssetManager& assets,
        RenderTexture2D& sceneTarget,
        const SectorTopologyFogSettings& fogSettings)
{
    bloomRenderer.ApplyEmissiveDecalBloomToScene(
            assets,
            initialized,
            camera,
            meshes.sectorDrawRecords,
            visibilityResult,
            textureHandlesById,
            sceneTarget,
            BuildSectorFogRenderContext(fogSettings, camera.position));
}

bool SectorMeshRenderer::ApplyLocalFogToScene(
        RenderTexture2D& sceneTarget,
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes)
{
    const SectorBillboardDynamicLightContext dynamicLightContext =
            BuildBillboardDynamicLightContext();
    const bool localFogApplied = localFogRenderer.Apply(
            sceneTarget,
            map,
            camera,
            runtimeSeconds,
            objectLightProbes,
            dynamicLightContext);
    const bool lightHazeApplied = lightHazeRenderer.Apply(
            sceneTarget,
            map,
            camera,
            runtimeSeconds,
            objectLightProbes,
            dynamicLightContext,
            lightAtmosphereSources,
            visibilityResult,
            meshes.sectorReceiverBounds);
    const bool lightDustApplied = lightDustRenderer.Apply(
            sceneTarget,
            map,
            camera,
            runtimeSeconds,
            objectLightProbes,
            dynamicLightContext,
            lightAtmosphereSources,
            visibilityResult,
            meshes.sectorReceiverBounds);
    return localFogApplied || lightHazeApplied || lightDustApplied;
}

SectorViewPose SectorMeshRenderer::Pose() const
{
    return RendererPose();
}

SectorViewPose SectorMeshRenderer::RendererPose() const
{
    return SectorViewPose{position, yawRadians, pitchRadians};
}

void SectorMeshRenderer::ApplyPose(const SectorViewPose& pose)
{
    ApplyRendererPose(pose);
}

void SectorMeshRenderer::ApplyRendererPose(const SectorViewPose& pose)
{
    position = pose.position;
    yawRadians = pose.yawRadians;
    pitchRadians = pose.pitchRadians;
    UpdateCamera();
    UpdateVisibilityDebug();
}

void SectorMeshRenderer::RefreshDynamicLightSources(const SectorTopologyMap& map)
{
    dynamicLightState.RebuildSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr);
    BuildSectorLightAtmosphereSources(
            map,
            visibilityLookupWorldValid ? &visibilityLookupWorld : nullptr,
            lightAtmosphereSources);
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
