#include "sector_demo/SectorPreviewDoorRenderer.h"

#include "sector_demo/SectorRuntimeObjects.h"

#include <raylib.h>

#include <cstdio>
#include <limits>

namespace game {

namespace {

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
    mesh.colors = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(mesh.vertexCount * 4 * sizeof(unsigned char))));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(static_cast<unsigned int>(data.indices.size() * sizeof(unsigned short))));

    if (mesh.vertices == nullptr
            || mesh.normals == nullptr
            || mesh.texcoords == nullptr
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

void SectorPreviewDoorRenderer::ReserveRuntimeDoorCapacity(size_t capacity)
{
    doorMeshCache.reserve(capacity);
    runtimeDoorShadowCasters.clear();
    runtimeDoorShadowCasters.reserve(capacity);
}

void SectorPreviewDoorRenderer::ResetOpaqueShaderLocations()
{
    opaqueShaderLocations = SectorPreviewDoorOpaqueShaderLocations{};
}

bool SectorPreviewDoorRenderer::LoadOpaqueResources()
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
    opaqueShaderLoaded = true;

    opaqueMaterial = LoadMaterialDefault();
    opaqueDefaultMaterialTexture = opaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    opaqueMaterial.shader = opaqueShader;
    opaqueMaterialLoaded = true;
    return true;
}

void SectorPreviewDoorRenderer::ShutdownOpaqueResources()
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

void SectorPreviewDoorRenderer::PrepareRuntimeDoorMeshes(engine::World& runtimeObjectWorld)
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

void SectorPreviewDoorRenderer::ClearPreparedShadowCasters()
{
    runtimeDoorShadowCasters.clear();
}

void SectorPreviewDoorRenderer::UnloadDoorMeshes()
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

SectorPreviewDoorRenderer::DoorMeshCacheEntry* SectorPreviewDoorRenderer::FindMutableDoorMesh(int placedObjectId)
{
    auto cacheIt = doorMeshCache.find(placedObjectId);
    if (cacheIt == doorMeshCache.end()) {
        return nullptr;
    }
    return &cacheIt->second;
}

const SectorPreviewDoorRenderer::DoorMeshCacheEntry* SectorPreviewDoorRenderer::FindDoorMesh(int placedObjectId) const
{
    const auto cacheIt = doorMeshCache.find(placedObjectId);
    if (cacheIt == doorMeshCache.end()) {
        return nullptr;
    }
    return &cacheIt->second;
}

const Mesh* SectorPreviewDoorRenderer::ResolveDoorShadowCasterMesh(
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

const Mesh* SectorPreviewDoorRenderer::ResolveDoorShadowCasterMesh(
        void* userData,
        const SectorDoorShadowCaster& caster,
        float& outWidth,
        float& outHeight)
{
    const SectorPreviewDoorRenderer* renderer = static_cast<const SectorPreviewDoorRenderer*>(userData);
    if (renderer == nullptr) {
        return nullptr;
    }
    return renderer->ResolveDoorShadowCasterMesh(caster, outWidth, outHeight);
}

} // namespace game
