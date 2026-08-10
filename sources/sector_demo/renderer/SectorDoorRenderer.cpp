#include "sector_demo/renderer/SectorDoorRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorRuntimeObjects.h"

#include <raylib.h>
#include <rlgl.h>

#include <cstdio>
#include <limits>

namespace game {

namespace {

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

uniform int fogEnabled;
uniform vec3 fogColor;
uniform vec3 fogCameraPosition;
uniform float fogStartDistanceWorld;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeightWorld;
uniform float fogHeightFalloff;

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
    vec3 worldNormal = SafeNormalize(fragWorldNormal, vec3(0.0, 1.0, 0.0));
    vec3 staticProbeLighting = max(fragColor.rgb, vec3(0.0));
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
    vec3 lighting = max(staticProbeLighting + dynamicDirect, vec3(0.0));
    vec3 outputRgb = ApplySectorFog(surfaceRgb * tint * lighting, fragWorldPosition);
    finalColor = vec4(outputRgb, sampled.a * doorTint.a);
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

void SectorDoorRenderer::ReserveRuntimeDoorCapacity(size_t capacity)
{
    doorMeshCache.reserve(capacity);
    runtimeDoorShadowCasters.clear();
    runtimeDoorShadowCasters.reserve(capacity);
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
    opaqueShader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(opaqueShader, "shadowMap0");
    opaqueShader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(opaqueShader, "shadowMap1");
    opaqueShaderLocations.texture = opaqueShader.locs[SHADER_LOC_MAP_DIFFUSE];
    opaqueShaderLocations.dynamicLightCount = GetShaderLocation(opaqueShader, "dynamicLightCount");
    opaqueShaderLocations.dynamicLightPositions = GetShaderLocationArrayBase(opaqueShader, "dynamicLightPositions");
    opaqueShaderLocations.dynamicLightColors = GetShaderLocationArrayBase(opaqueShader, "dynamicLightColors");
    opaqueShaderLocations.dynamicLightRadii = GetShaderLocationArrayBase(opaqueShader, "dynamicLightRadii");
    opaqueShaderLocations.dynamicLightIntensities = GetShaderLocationArrayBase(opaqueShader, "dynamicLightIntensities");
    opaqueShaderLocations.dynamicLightTypes = GetShaderLocationArrayBase(opaqueShader, "dynamicLightTypes");
    opaqueShaderLocations.dynamicLightDirections = GetShaderLocationArrayBase(opaqueShader, "dynamicLightDirections");
    opaqueShaderLocations.dynamicLightInnerConeCos = GetShaderLocationArrayBase(opaqueShader, "dynamicLightInnerConeCos");
    opaqueShaderLocations.dynamicLightOuterConeCos = GetShaderLocationArrayBase(opaqueShader, "dynamicLightOuterConeCos");
    opaqueShaderLocations.dynamicLightShadowSlots = GetShaderLocationArrayBase(opaqueShader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        opaqueShaderLocations.shadowLightMatrices[i] =
                GetShaderLocationArrayElement(opaqueShader, "shadowLightMatrices", i);
    }
    opaqueShaderLocations.shadowBias = GetShaderLocationArrayBase(opaqueShader, "shadowBias");
    opaqueShaderLocations.shadowStrength = GetShaderLocationArrayBase(opaqueShader, "shadowStrength");
    opaqueShaderLocations.shadowSoftness = GetShaderLocationArrayBase(opaqueShader, "shadowSoftness");
    opaqueShaderLocations.dynamicLightingClamp = GetShaderLocation(opaqueShader, "dynamicLightingClamp");
    opaqueShaderLocations.debugMode = GetShaderLocation(opaqueShader, "doorDebugMode");
    opaqueShaderLocations.tint = GetShaderLocation(opaqueShader, "doorTint");
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
        opaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
        opaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
        UnloadMaterial(opaqueMaterial);
        opaqueMaterial = Material{};
        opaqueDefaultMaterialTexture = Texture2D{};
        opaqueShader = Shader{};
        ResetOpaqueShaderLocations();
        opaqueMaterialLoaded = false;
        opaqueShaderLoaded = false;
    }
}

void SectorDoorRenderer::PrepareRuntimeDoorMeshes(engine::World& runtimeObjectWorld)
{
    for (auto& entry : doorMeshCache) {
        entry.second.seenThisFrame = false;
    }
    runtimeDoorShadowCasters.clear();

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender>(
            [this](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render) {
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
                }
            });

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
    PrepareRuntimeDoorMeshes(*context.runtimeObjectWorld);

    size_t consideredCount = 0;
    size_t drawnCount = 0;
    size_t skippedCount = 0;
    const SectorBakedObjectLightProbeRuntimeData emptyObjectLightProbes;
    const SectorBakedObjectLightProbeRuntimeData& objectLightProbes =
            context.lighting.objectLightProbes != nullptr
            ? *context.lighting.objectLightProbes
            : emptyObjectLightProbes;

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
    dynamicLightLocations.dynamicLightingClamp = doorOpaqueLocations.dynamicLightingClamp;
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
    if (doorOpaqueLocations.debugMode >= 0) {
        const int debugMode = DoorLightingDebugModeShaderValue();
        SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueLocations.debugMode, &debugMode, SHADER_UNIFORM_INT);
    }
    if (doorOpaqueLocations.texture >= 0) {
        const int diffuseTextureUnit = 0;
        SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueLocations.texture, &diffuseTextureUnit, SHADER_UNIFORM_INT);
    }

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
                if (render.width <= 0.0f || render.height <= 0.0f || render.thickness <= 0.0f) {
                    ++skippedCount;
                    return;
                }

                const Texture2D* texture = nullptr;
                if (!render.textureId.empty()
                        && context.textureResolver.resolve != nullptr) {
                    texture = context.textureResolver.resolve(
                            context.textureResolver.userData,
                            *context.assets,
                            render.textureId);
                }
                if (texture == nullptr) {
                    texture = context.defaultMaterialTexture != nullptr
                            ? context.defaultMaterialTexture
                            : &opaqueDefaultMaterialTexture;
                }
                if (texture == nullptr || texture->id == 0) {
                    ++skippedCount;
                    return;
                }

                DoorMeshCacheEntry* cacheEntry = FindMutableDoorMesh(door.placedObjectId);
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
                            context.lighting.mapForFallback,
                            cacheEntry->staticLightingValues)) {
                    cacheEntry->staticLightingValues.assign(
                            static_cast<size_t>(cacheEntry->mesh.vertexCount),
                            Vector3{1.0f, 1.0f, 1.0f});
                }
                if (cacheEntry->mesh.tangents != nullptr
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

                if (doorOpaqueLocations.tint >= 0) {
                    const Vector4 tint = engine::SrgbColorBytesToLinearSceneRgba(
                            render.tint);
                    SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueLocations.tint, &tint, SHADER_UNIFORM_VEC4);
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
        PrepareRuntimeDoorMeshes(*runtimeObjectWorld);
    } else {
        ClearPreparedShadowCasters();
    }

    context.doorShadowCasters = &ShadowCasters();
    context.doorMeshResolverUserData = this;
    context.doorMeshResolver = &SectorDoorRenderer::ResolveDoorShadowCasterMesh;
}

void SectorDoorRenderer::ClearPreparedShadowCasters()
{
    runtimeDoorShadowCasters.clear();
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
