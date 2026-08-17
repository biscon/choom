#include "sector_demo/renderer/SectorLightDustRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/renderer/SectorFog.h"
#include "sector_demo/renderer/SectorLocalFogLighting.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace game {
namespace {

constexpr float DustFadeSeconds = 1.5f;
constexpr float DustMinimumLifetimeSeconds = 8.0f;
constexpr float DustMaximumLifetimeSeconds = 16.0f;
constexpr float DustProbeResampleDistanceWorld = 0.5f;

const char* DustVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragTexCoord;
out vec3 fragWorldPosition;
out vec3 fragStaticLighting;
out vec4 fragParticleColor;
void main() {
    fragTexCoord = vertexTexCoord;
    fragWorldPosition = vertexPosition;
    fragStaticLighting = vertexNormal;
    fragParticleColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* DustFs = R"(
#version 330
in vec2 fragTexCoord;
in vec3 fragWorldPosition;
in vec3 fragStaticLighting;
in vec4 fragParticleColor;
out vec4 finalColor;

float StoreFiniteHalfChannel(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? 65504.0 : 0.0;
    return min(max(value, 0.0), 65504.0);
}
vec3 StoreFiniteHalfRadiance(vec3 value) {
    return vec3(StoreFiniteHalfChannel(value.r), StoreFiniteHalfChannel(value.g),
            StoreFiniteHalfChannel(value.b));
}
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform float nearPlane;
uniform float farPlane;
uniform vec3 cameraPosition;
uniform int fogEnabled;
uniform float fogStartDistance;
uniform float fogDensity;
uniform float fogMaxOpacity;
uniform float fogReferenceHeight;
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

const vec2 kShadowDisk[4] = vec2[4](
    vec2(-0.707, -0.707), vec2(0.707, -0.707),
    vec2(-0.707, 0.707), vec2(0.707, 0.707));

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.00000001 ? value * inversesqrt(lengthSquared) : fallback;
}
float linearDepth(float depth) {
    float z = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) /
            max(farPlane + nearPlane - z * (farPlane - nearPlane), 0.00001);
}
float shadowDepth(int slot, vec2 uv) {
    return slot == 0 ? texture(shadowMap0, uv).r : texture(shadowMap1, uv).r;
}
float shadowVisibility(int slot, vec3 position) {
    if (slot < 0 || slot >= MAX_DYNAMIC_SHADOW_CASTERS) return 1.0;
    vec4 clip = shadowLightMatrices[slot] * vec4(position, 1.0);
    if (clip.w <= 0.0) return 1.0;
    vec3 coordinate = clip.xyz / clip.w * 0.5 + 0.5;
    if (any(lessThan(coordinate, vec3(0.0))) || any(greaterThan(coordinate, vec3(1.0)))) return 1.0;
    float compareDepth = coordinate.z - min(max(shadowBias[slot], 0.0), 0.02);
    float softness = clamp(shadowSoftness[slot], 0.0, 8.0);
    if (softness <= 0.0) return compareDepth <= shadowDepth(slot, coordinate.xy) ? 1.0 : 0.0;
    vec2 texel = 1.0 / vec2(textureSize(shadowMap0, 0));
    float visible = 0.0;
    for (int sampleIndex = 0; sampleIndex < 4; ++sampleIndex) {
        vec2 uv = clamp(coordinate.xy + kShadowDisk[sampleIndex] * max(0.25, softness) * texel,
                vec2(0.0), vec2(1.0));
        visible += compareDepth <= shadowDepth(slot, uv) ? 1.0 : 0.0;
    }
    return visible * 0.25;
}
vec3 dynamicLighting(vec3 position) {
    vec3 result = vec3(0.0);
    for (int index = 0; index < MAX_DYNAMIC_LIGHTS; ++index) {
        if (index >= dynamicLightCount) break;
        float radius = dynamicLightRadii[index];
        vec3 toLight = dynamicLightPositions[index] - position;
        float distanceSquared = dot(toLight, toLight);
        if (radius <= 0.0 || distanceSquared >= radius * radius) continue;
        float distanceToLight = sqrt(max(distanceSquared, 0.0));
        vec3 lightDirection = distanceToLight > 0.0001 ? toLight / distanceToLight : vec3(0.0, 1.0, 0.0);
        float attenuation = clamp(1.0 - distanceToLight / radius, 0.0, 1.0);
        attenuation *= attenuation;
        float cone = 1.0;
        if (dynamicLightTypes[index] == 1) {
            vec3 spotDirection = safeNormalize(dynamicLightDirections[index], vec3(0.0, -1.0, 0.0));
            float coneDot = dot(spotDirection, distanceToLight > 0.0001 ? -lightDirection : spotDirection);
            float inner = dynamicLightInnerConeCos[index];
            float outer = dynamicLightOuterConeCos[index];
            cone = abs(inner - outer) > 0.0001 ? smoothstep(outer, inner, coneDot) : step(inner, coneDot);
            int shadowSlot = dynamicLightShadowSlots[index];
            if (shadowSlot >= 0 && cone > 0.0) {
                cone *= mix(1.0, shadowVisibility(shadowSlot, position),
                        clamp(shadowStrength[shadowSlot], 0.0, 1.0));
            }
        }
        result += dynamicLightColors[index] * dynamicLightIntensities[index] * attenuation * cone;
    }
    return result;
}
float distanceFogTransmittance(vec3 position) {
    if (fogEnabled == 0 || fogDensity <= 0.0 || fogMaxOpacity <= 0.0) return 1.0;
    float distanceToCamera = max(length(position - cameraPosition) - fogStartDistance, 0.0);
    float midpointHeight = (cameraPosition.y + position.y) * 0.5;
    float aboveReference = max(midpointHeight - fogReferenceHeight, 0.0);
    float amount = min(1.0 - exp(-fogDensity * distanceToCamera *
            exp(-aboveReference * fogHeightFalloff)), fogMaxOpacity);
    return 1.0 - amount;
}
void main() {
    vec2 centered = fragTexCoord * 2.0 - 1.0;
    float radiusSquared = dot(centered, centered);
    if (radiusSquared >= 1.0) discard;
    float softMask = (1.0 - smoothstep(0.05, 1.0, radiusSquared));
    softMask *= softMask;
    vec2 screenUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    float opaqueDepth = texture(sceneDepth, screenUv).r;
    float opaqueDistance = linearDepth(opaqueDepth);
    float particleDistance = linearDepth(gl_FragCoord.z);
    float intersectionFade = clamp((opaqueDistance - particleDistance) / 0.08, 0.0, 1.0);
    if (intersectionFade <= 0.0) discard;
    vec3 illumination = max(fragStaticLighting + dynamicLighting(fragWorldPosition), vec3(0.0));
    float opacity = fragParticleColor.a * softMask * intersectionFade;
    vec3 srgb = fragParticleColor.rgb;
    vec3 low = srgb / 12.92;
    vec3 high = pow((srgb + 0.055) / 1.055, vec3(2.4));
    vec3 authoredLinearTint = mix(high, low, lessThanEqual(srgb, vec3(0.04045)));
    vec3 scattered = authoredLinearTint * illumination *
            distanceFogTransmittance(fragWorldPosition);
    finalColor = vec4(StoreFiniteHalfRadiance(scattered * opacity), 0.0);
}
)";

bool IsShaderReady(Shader shader) { return shader.id != 0; }

int ArrayLocation(Shader shader, const char* name)
{
    const int direct = GetShaderLocation(shader, name);
    if (direct >= 0) return direct;
    const std::string indexed = std::string(name) + "[0]";
    return GetShaderLocation(shader, indexed.c_str());
}

int ArrayElementLocation(Shader shader, const char* name, int index)
{
    const std::string indexed = std::string(name) + "[" + std::to_string(index) + "]";
    return GetShaderLocation(shader, indexed.c_str());
}

float Hash01(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return static_cast<float>(value & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

Vector3 SafeNormalize(Vector3 value, Vector3 fallback)
{
    return Vector3LengthSqr(value) > 0.00000001f ? Vector3Normalize(value) : fallback;
}

void VolumeBasis(const SectorLightAtmosphereVolume& volume, Vector3& outRight, Vector3& outUp)
{
    const Vector3 reference = std::fabs(volume.directionWorld.y) > 0.98f
            ? Vector3{0.0f, 0.0f, 1.0f}
            : Vector3{0.0f, 1.0f, 0.0f};
    outRight = SafeNormalize(Vector3CrossProduct(volume.directionWorld, reference), Vector3{1.0f, 0.0f, 0.0f});
    outUp = SafeNormalize(Vector3CrossProduct(outRight, volume.directionWorld), Vector3{0.0f, 1.0f, 0.0f});
}

Vector3 RandomPositionInVolume(const SectorLightAtmosphereVolume& volume, std::uint32_t sequence)
{
    const float u = Hash01(sequence * 4u + 0u);
    const float v = Hash01(sequence * 4u + 1u);
    const float w = Hash01(sequence * 4u + 2u);
    if (volume.source->shape == SectorLightAtmosphereShape::Sphere) {
        const float z = w * 2.0f - 1.0f;
        const float angle = u * 2.0f * PI;
        const float radius = volume.extentWorld * std::cbrt(std::max(v, 0.000001f));
        const float planar = std::sqrt(std::max(1.0f - z * z, 0.0f));
        return Vector3Add(volume.originWorld, Vector3Scale(
                Vector3{std::cos(angle) * planar, z, std::sin(angle) * planar}, radius));
    }
    Vector3 right;
    Vector3 up;
    VolumeBasis(volume, right, up);
    const float axial01 = std::cbrt(std::max(u, 0.000001f));
    const float radius = volume.coneRadiusWorld * axial01 * std::sqrt(v);
    const float angle = w * 2.0f * PI;
    return Vector3Add(
            Vector3Add(volume.originWorld, Vector3Scale(volume.directionWorld, axial01 * volume.extentWorld)),
            Vector3Add(Vector3Scale(right, std::cos(angle) * radius), Vector3Scale(up, std::sin(angle) * radius)));
}

unsigned char Byte(float value)
{
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

Rectangle SourceRectangle(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f, static_cast<float>(texture.width), -static_cast<float>(texture.height)};
}

Rectangle DestinationRectangle(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
}

} // namespace

bool SectorLightDustRenderer::EnsureShader()
{
    if (IsShaderReady(shader) && materialLoaded) return true;
    if (shaderFailed) return false;
    shader = LoadShaderFromMemory(DustVs, DustFs);
    if (!IsShaderReady(shader)) {
        shaderFailed = true;
        resourceDiagnostic = "disabled: shader unavailable";
        return false;
    }
    material = LoadMaterialDefault();
    defaultMaterialTexture = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    material.shader = shader;
    materialLoaded = true;
#define LOC(field, name) locations.field = GetShaderLocation(shader, name)
    LOC(sceneDepth, "sceneDepth");
    LOC(viewportSize, "viewportSize");
    LOC(nearPlane, "nearPlane");
    LOC(farPlane, "farPlane");
    LOC(cameraPosition, "cameraPosition");
    LOC(fogEnabled, "fogEnabled");
    LOC(fogStartDistance, "fogStartDistance");
    LOC(fogDensity, "fogDensity");
    LOC(fogMaxOpacity, "fogMaxOpacity");
    LOC(fogReferenceHeight, "fogReferenceHeight");
    LOC(fogHeightFalloff, "fogHeightFalloff");
#undef LOC
    locations.dynamicLights.dynamicLightCount = GetShaderLocation(shader, "dynamicLightCount");
    locations.dynamicLights.dynamicLightPositions = ArrayLocation(shader, "dynamicLightPositions");
    locations.dynamicLights.dynamicLightColors = ArrayLocation(shader, "dynamicLightColors");
    locations.dynamicLights.dynamicLightRadii = ArrayLocation(shader, "dynamicLightRadii");
    locations.dynamicLights.dynamicLightIntensities = ArrayLocation(shader, "dynamicLightIntensities");
    locations.dynamicLights.dynamicLightTypes = ArrayLocation(shader, "dynamicLightTypes");
    locations.dynamicLights.dynamicLightDirections = ArrayLocation(shader, "dynamicLightDirections");
    locations.dynamicLights.dynamicLightInnerConeCos = ArrayLocation(shader, "dynamicLightInnerConeCos");
    locations.dynamicLights.dynamicLightOuterConeCos = ArrayLocation(shader, "dynamicLightOuterConeCos");
    locations.shadows.dynamicLightShadowSlots = ArrayLocation(shader, "dynamicLightShadowSlots");
    for (int index = 0; index < MaxDynamicSpotLightShadowCasters; ++index) {
        locations.shadows.shadowLightMatrices[static_cast<std::size_t>(index)] =
                ArrayElementLocation(shader, "shadowLightMatrices", index);
    }
    locations.shadows.shadowBias = ArrayLocation(shader, "shadowBias");
    locations.shadows.shadowStrength = ArrayLocation(shader, "shadowStrength");
    locations.shadows.shadowSoftness = ArrayLocation(shader, "shadowSoftness");
    locations.shadowMap0 = GetShaderLocation(shader, "shadowMap0");
    locations.shadowMap1 = GetShaderLocation(shader, "shadowMap1");
    // DrawMesh binds material maps directly; auxiliary batch samplers do not
    // survive the render-target changes that precede this pass.
    material.shader.locs[SHADER_LOC_MAP_DIFFUSE] = locations.sceneDepth;
    material.shader.locs[SHADER_LOC_MAP_ROUGHNESS] = locations.shadowMap0;
    material.shader.locs[SHADER_LOC_MAP_OCCLUSION] = locations.shadowMap1;
    return true;
}

bool SectorLightDustRenderer::EnsureMesh()
{
    if (mesh.vaoId != 0) return true;
    mesh.vertexCount = MaxParticles * 4;
    mesh.triangleCount = MaxParticles * 2;
    mesh.vertices = static_cast<float*>(MemAlloc(sizeof(float) * vertices.size()));
    mesh.normals = static_cast<float*>(MemAlloc(sizeof(float) * normals.size()));
    mesh.texcoords = static_cast<float*>(MemAlloc(sizeof(float) * texcoords.size()));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(sizeof(unsigned char) * colors.size()));
    mesh.indices = static_cast<unsigned short*>(MemAlloc(sizeof(unsigned short) * indices.size()));
    if (mesh.vertices == nullptr || mesh.normals == nullptr || mesh.texcoords == nullptr
            || mesh.colors == nullptr || mesh.indices == nullptr) {
        UnloadMesh(mesh);
        mesh = {};
        return false;
    }
    for (int particleIndex = 0; particleIndex < MaxParticles; ++particleIndex) {
        const int vertex = particleIndex * 4;
        const int index = particleIndex * 6;
        indices[static_cast<std::size_t>(index + 0)] = static_cast<unsigned short>(vertex + 0);
        indices[static_cast<std::size_t>(index + 1)] = static_cast<unsigned short>(vertex + 1);
        indices[static_cast<std::size_t>(index + 2)] = static_cast<unsigned short>(vertex + 2);
        indices[static_cast<std::size_t>(index + 3)] = static_cast<unsigned short>(vertex + 0);
        indices[static_cast<std::size_t>(index + 4)] = static_cast<unsigned short>(vertex + 2);
        indices[static_cast<std::size_t>(index + 5)] = static_cast<unsigned short>(vertex + 3);
    }
    std::memcpy(mesh.indices, indices.data(), sizeof(unsigned short) * indices.size());
    UploadMesh(&mesh, true);
    return mesh.vaoId != 0;
}

bool SectorLightDustRenderer::EnsureResources()
{
    if (!EnsureShader() || !EnsureMesh()) {
        if (resourceDiagnostic == "not allocated") {
            resourceDiagnostic = "disabled: particle mesh unavailable";
        }
        return false;
    }
    resourceDiagnostic = "shared RGBA32F scratch; additive RGB, alpha=0";
    return true;
}

void SectorLightDustRenderer::ClearBorrowedMaterialTextures()
{
    if (!materialLoaded || material.maps == nullptr) return;
    material.maps[MATERIAL_MAP_DIFFUSE].texture = defaultMaterialTexture;
    material.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
    material.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
}

const SectorLightDustRenderer::Emitter* SectorLightDustRenderer::FindEmitter(
        SectorLightAtmosphereSourceKind kind,
        int lightId) const
{
    for (int index = 0; index < activeEmitterCount; ++index) {
        const Emitter& emitter = emitters[static_cast<std::size_t>(index)];
        if (emitter.volume.source->kind == kind && emitter.volume.source->lightId == lightId) return &emitter;
    }
    return nullptr;
}

SectorLightDustRenderer::Particle* SectorLightDustRenderer::FindFreeParticle()
{
    for (Particle& particle : particles) {
        if (!particle.active) return &particle;
    }
    return nullptr;
}

void SectorLightDustRenderer::BuildEmitters(
        const SectorTopologyMap& map,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        float nearPlane,
        float farPlane,
        float aspectRatio)
{
    static_cast<void>(map);
    eligibleEmitterCount = 0;
    activeEmitterCount = 0;
    std::array<float, MaxEmitters> distances;
    distances.fill(std::numeric_limits<float>::max());
    for (const SectorLightAtmosphereSource& source : sources) {
        if (!source.atmosphere.dust.enabled || source.atmosphere.dust.amount <= 0
                || source.atmosphere.dust.opacity <= 0.0f
                || !IsSectorLightAtmosphereSourceSelected(source, dynamicLights)) {
            continue;
        }
        SectorLightAtmosphereVolume volume;
        if (!MakeSectorLightAtmosphereVolume(source, source.atmosphere.dust.extentScale, 0.0f, volume)
                || !IsSectorLightAtmosphereVolumeVisible(
                        volume, visibility, receiverBounds, camera, aspectRatio, nearPlane, farPlane)) {
            continue;
        }
        ++eligibleEmitterCount;
        const float distance = Vector3DistanceSqr(camera.position, volume.boundsCenterWorld);
        int insert = std::min(activeEmitterCount, MaxEmitters - 1);
        if (activeEmitterCount >= MaxEmitters && distance >= distances.back()) continue;
        if (activeEmitterCount < MaxEmitters) ++activeEmitterCount;
        while (insert > 0 && distance < distances[static_cast<std::size_t>(insert - 1)]) {
            emitters[static_cast<std::size_t>(insert)] = emitters[static_cast<std::size_t>(insert - 1)];
            distances[static_cast<std::size_t>(insert)] = distances[static_cast<std::size_t>(insert - 1)];
            --insert;
        }
        emitters[static_cast<std::size_t>(insert)] = Emitter{volume, source.atmosphere.dust};
        distances[static_cast<std::size_t>(insert)] = distance;
    }
}

void SectorLightDustRenderer::SpawnParticle(
        Particle& particle,
        const Emitter& emitter,
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        bool initialFill)
{
    const std::uint32_t sequence = spawnSequence++;
    particle = Particle{};
    particle.active = true;
    particle.sourceKind = emitter.volume.source->kind;
    particle.lightId = emitter.volume.source->lightId;
    particle.preferredSectorId = emitter.volume.source->ownerSectorId;
    particle.position = RandomPositionInVolume(emitter.volume, sequence);
    particle.lightingSamplePosition = particle.position;
    particle.sizeWorld = emitter.settings.minimumSizeWorld
            + (emitter.settings.maximumSizeWorld - emitter.settings.minimumSizeWorld) * Hash01(sequence * 7u + 1u);
    particle.lifetimeSeconds = DustMinimumLifetimeSeconds
            + (DustMaximumLifetimeSeconds - DustMinimumLifetimeSeconds) * Hash01(sequence * 7u + 2u);
    particle.ageSeconds = initialFill ? Hash01(sequence * 7u + 3u) * particle.lifetimeSeconds : 0.0f;
    particle.phase = Hash01(sequence * 7u + 4u) * 2.0f * PI;
    const Vector3 randomDirection = SafeNormalize(Vector3{
            Hash01(sequence * 7u + 5u) * 2.0f - 1.0f,
            Hash01(sequence * 7u + 6u) * 2.0f - 1.0f,
            Hash01(sequence * 7u + 7u) * 2.0f - 1.0f}, Vector3{0.0f, 1.0f, 0.0f});
    particle.velocity = Vector3Scale(randomDirection, emitter.settings.driftSpeedWorld);
    particle.opacity = emitter.settings.opacity;
    particle.scatteringTint = emitter.settings.scatteringTint;
    particle.staticLighting = EvaluateSectorLocalFogProbeLighting(
            SampleBakedObjectLighting(probes, particle.position, particle.preferredSectorId, &map));
}

void SectorLightDustRenderer::UpdateParticles(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        float dt,
        float runtimeSeconds)
{
    for (Particle& particle : particles) {
        if (!particle.active) continue;
        const Emitter* emitter = FindEmitter(particle.sourceKind, particle.lightId);
        particle.ageSeconds += dt;
        if (emitter == nullptr) {
            particle.ageSeconds = std::max(particle.ageSeconds, particle.lifetimeSeconds - DustFadeSeconds);
        } else {
            const float turbulence = emitter->settings.turbulenceWorld;
            const Vector3 wobble{
                    std::sin(runtimeSeconds * 0.73f + particle.phase),
                    std::sin(runtimeSeconds * 0.51f + particle.phase * 1.7f),
                    std::cos(runtimeSeconds * 0.67f + particle.phase * 0.8f)};
            particle.position = Vector3Add(
                    particle.position,
                    Vector3Scale(Vector3Add(particle.velocity, Vector3Scale(wobble, turbulence)), dt));
            if (!IsPointInsideSectorLightAtmosphereVolume(emitter->volume, particle.position)) {
                particle.ageSeconds = std::max(particle.ageSeconds, particle.lifetimeSeconds - DustFadeSeconds);
            }
            if (Vector3DistanceSqr(particle.position, particle.lightingSamplePosition)
                    >= DustProbeResampleDistanceWorld * DustProbeResampleDistanceWorld) {
                particle.staticLighting = EvaluateSectorLocalFogProbeLighting(
                        SampleBakedObjectLighting(
                                probes, particle.position, particle.preferredSectorId, &map));
                particle.lightingSamplePosition = particle.position;
            }
        }
        if (particle.ageSeconds >= particle.lifetimeSeconds) particle.active = false;
    }

    for (int emitterIndex = 0; emitterIndex < activeEmitterCount; ++emitterIndex) {
        const Emitter& emitter = emitters[static_cast<std::size_t>(emitterIndex)];
        const int requested = std::min(emitter.settings.amount, MaxParticlesPerEmitter);
        int count = 0;
        for (const Particle& particle : particles) {
            if (particle.active
                    && particle.sourceKind == emitter.volume.source->kind
                    && particle.lightId == emitter.volume.source->lightId) {
                ++count;
            }
        }
        const bool initialFill = count == 0;
        while (count < requested) {
            Particle* particle = FindFreeParticle();
            if (particle == nullptr) break;
            SpawnParticle(*particle, emitter, map, probes, initialFill);
            ++count;
        }
    }
}

int SectorLightDustRenderer::BuildMesh(const Camera3D& camera)
{
    Vector3 forward = SafeNormalize(Vector3Subtract(camera.target, camera.position), Vector3{0.0f, 0.0f, -1.0f});
    Vector3 right = SafeNormalize(Vector3CrossProduct(forward, camera.up), Vector3{1.0f, 0.0f, 0.0f});
    Vector3 up = SafeNormalize(Vector3CrossProduct(right, forward), Vector3{0.0f, 1.0f, 0.0f});
    int visible = 0;
    constexpr std::array<Vector2, 4> corners{
            Vector2{-1.0f, -1.0f}, Vector2{1.0f, -1.0f},
            Vector2{1.0f, 1.0f}, Vector2{-1.0f, 1.0f}};
    constexpr std::array<Vector2, 4> uv{
            Vector2{0.0f, 0.0f}, Vector2{1.0f, 0.0f},
            Vector2{1.0f, 1.0f}, Vector2{0.0f, 1.0f}};
    for (const Particle& particle : particles) {
        if (!particle.active || FindEmitter(particle.sourceKind, particle.lightId) == nullptr) continue;
        const float fadeIn = std::clamp(particle.ageSeconds / DustFadeSeconds, 0.0f, 1.0f);
        const float fadeOut = std::clamp((particle.lifetimeSeconds - particle.ageSeconds) / DustFadeSeconds, 0.0f, 1.0f);
        const float opacity = particle.opacity * std::min(fadeIn, fadeOut);
        if (opacity <= 0.001f || visible >= MaxParticles) continue;
        const Color scatteringTint = engine::SrgbColorBytesToLinearSceneUnorm(
                particle.scatteringTint);
        const int base = visible * 4;
        for (int corner = 0; corner < 4; ++corner) {
            const Vector3 position = Vector3Add(
                    particle.position,
                    Vector3Add(
                            Vector3Scale(right, corners[static_cast<std::size_t>(corner)].x * particle.sizeWorld),
                            Vector3Scale(up, corners[static_cast<std::size_t>(corner)].y * particle.sizeWorld)));
            const std::size_t vertex3 = static_cast<std::size_t>(base + corner) * 3;
            const std::size_t vertex2 = static_cast<std::size_t>(base + corner) * 2;
            const std::size_t vertex4 = static_cast<std::size_t>(base + corner) * 4;
            vertices[vertex3 + 0] = position.x;
            vertices[vertex3 + 1] = position.y;
            vertices[vertex3 + 2] = position.z;
            normals[vertex3 + 0] = particle.staticLighting.x;
            normals[vertex3 + 1] = particle.staticLighting.y;
            normals[vertex3 + 2] = particle.staticLighting.z;
            texcoords[vertex2 + 0] = uv[static_cast<std::size_t>(corner)].x;
            texcoords[vertex2 + 1] = uv[static_cast<std::size_t>(corner)].y;
            colors[vertex4 + 0] = scatteringTint.r;
            colors[vertex4 + 1] = scatteringTint.g;
            colors[vertex4 + 2] = scatteringTint.b;
            colors[vertex4 + 3] = Byte(opacity);
        }
        ++visible;
    }
    if (visible <= 0) return 0;
    const int vertexCount = visible * 4;
    const int indexCount = visible * 6;
    std::memcpy(mesh.vertices, vertices.data(), sizeof(float) * static_cast<std::size_t>(vertexCount) * 3);
    std::memcpy(mesh.normals, normals.data(), sizeof(float) * static_cast<std::size_t>(vertexCount) * 3);
    std::memcpy(mesh.texcoords, texcoords.data(), sizeof(float) * static_cast<std::size_t>(vertexCount) * 2);
    std::memcpy(mesh.colors, colors.data(), sizeof(unsigned char) * static_cast<std::size_t>(vertexCount) * 4);
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = visible * 2;
    UpdateMeshBuffer(mesh, 0, mesh.vertices, vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
    UpdateMeshBuffer(mesh, 1, mesh.texcoords, vertexCount * 2 * static_cast<int>(sizeof(float)), 0);
    UpdateMeshBuffer(mesh, 2, mesh.normals, vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
    UpdateMeshBuffer(mesh, 3, mesh.colors, vertexCount * 4 * static_cast<int>(sizeof(unsigned char)), 0);
    UpdateMeshBuffer(mesh, 6, mesh.indices, indexCount * static_cast<int>(sizeof(unsigned short)), 0);
    return visible;
}

bool SectorLightDustRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& sceneScratch,
        const SectorTopologyMap& map,
        const Camera3D& camera,
        float runtimeSeconds,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds)
{
    (void)sceneScratch;
    visibleParticleCount = 0;
    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (sceneTarget.texture.id == 0 || sceneTarget.depth.id == 0
            || !std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane) {
        return false;
    }
    const float aspectRatio = static_cast<float>(sceneTarget.texture.width)
            / static_cast<float>(std::max(sceneTarget.texture.height, 1));
    BuildEmitters(map, camera, dynamicLights, sources, visibility, receiverBounds,
            nearPlane, farPlane, aspectRatio);
    float dt = previousRuntimeSeconds > 0.0f
            ? std::clamp(runtimeSeconds - previousRuntimeSeconds, 0.0f, 0.1f)
            : 1.0f / 60.0f;
    previousRuntimeSeconds = runtimeSeconds;
    UpdateParticles(map, probes, dt, runtimeSeconds);
    if (activeEmitterCount <= 0) return false;
    if (!EnsureResources()) {
        if (!warnedUnavailable) {
            TraceLog(LOG_WARNING, "LIGHT DUST: render resources unavailable; dust disabled");
            warnedUnavailable = true;
        }
        return false;
    }
    visibleParticleCount = BuildMesh(camera);
    if (visibleParticleCount <= 0) return false;

    const Vector2 viewportSize{static_cast<float>(sceneTarget.texture.width), static_cast<float>(sceneTarget.texture.height)};
    const SectorFogRenderContext fog = BuildSectorFogRenderContext(map.fogSettings, camera.position);
    const SectorTopologyFogSettings& fogSettings = fog.settings;
    const int fogEnabled = fogSettings.enabled ? 1 : 0;
    material.maps[MATERIAL_MAP_DIFFUSE].texture = sceneTarget.depth;
    material.maps[MATERIAL_MAP_ROUGHNESS].texture =
            dynamicLights.shadowMaps.shadowMap0 != nullptr
                    && dynamicLights.shadowMaps.shadowMap0->id != 0
            ? *dynamicLights.shadowMaps.shadowMap0
            : Texture2D{};
    material.maps[MATERIAL_MAP_OCCLUSION].texture =
            dynamicLights.shadowMaps.shadowMap1 != nullptr
                    && dynamicLights.shadowMaps.shadowMap1->id != 0
            ? *dynamicLights.shadowMaps.shadowMap1
            : Texture2D{};
#define SET(field, value, type) SetShaderValue(shader, locations.field, &value, type)
    SET(viewportSize, viewportSize, SHADER_UNIFORM_VEC2);
    SET(nearPlane, nearPlane, SHADER_UNIFORM_FLOAT);
    SET(farPlane, farPlane, SHADER_UNIFORM_FLOAT);
    SET(cameraPosition, camera.position, SHADER_UNIFORM_VEC3);
    SET(fogEnabled, fogEnabled, SHADER_UNIFORM_INT);
    SET(fogStartDistance, fogSettings.startDistanceWorld, SHADER_UNIFORM_FLOAT);
    SET(fogDensity, fogSettings.density, SHADER_UNIFORM_FLOAT);
    SET(fogMaxOpacity, fogSettings.maxOpacity, SHADER_UNIFORM_FLOAT);
    SET(fogReferenceHeight, fogSettings.referenceHeightWorld, SHADER_UNIFORM_FLOAT);
    SET(fogHeightFalloff, fogSettings.heightFalloff, SHADER_UNIFORM_FLOAT);
#undef SET
    UploadSectorRendererDynamicPointLights(shader, locations.dynamicLights, dynamicLights);
    UploadSectorRendererDynamicSpotLightShadowUniforms(shader, locations.shadows, dynamicLights.shadowUniforms);

    // Dust is additive and does not need the current scene color, so draw it
    // directly into the active HDR target while borrowing its depth texture.
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneTarget);
    BeginMode3D(camera);
    // DustFs already writes premultiplied radiance, so accumulate it without
    // applying particle alpha again.
    BeginBlendMode(BLEND_ADD_COLORS);
    rlEnableDepthTest();
    rlDisableDepthMask();
    DrawMesh(mesh, material, MatrixIdentity());
    rlDrawRenderBatchActive();
    ClearBorrowedMaterialTextures();
    rlEnableDepthMask();
    EndBlendMode();
    EndMode3D();
    EndTextureMode();
    return true;
}

void SectorLightDustRenderer::Shutdown()
{
    if (mesh.vaoId != 0 || mesh.vertices != nullptr) UnloadMesh(mesh);
    mesh = {};
    if (materialLoaded) {
        ClearBorrowedMaterialTextures();
        UnloadMaterial(material);
    } else if (IsShaderReady(shader)) {
        UnloadShader(shader);
    }
    material = {};
    defaultMaterialTexture = {};
    shader = {};
    locations = {};
    materialLoaded = false;
    for (Particle& particle : particles) particle = Particle{};
    previousRuntimeSeconds = 0.0f;
    eligibleEmitterCount = 0;
    activeEmitterCount = 0;
    visibleParticleCount = 0;
    shaderFailed = false;
    resourceDiagnostic = "not allocated";
}

} // namespace game
