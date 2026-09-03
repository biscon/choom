#include "sector_demo/renderer/SectorUnderwaterRenderer.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/TextureColorUsage.h"
#include "engine/assets/TextureLoadFlags.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorCollisionWorld.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace game {
namespace {

constexpr int CausticsTextureSize = 128;
constexpr int CausticsCellCount = 8;
constexpr float ParticleSpawnRadiusWorld = 3.5f;
constexpr float ParticleNearRadiusWorld = 0.35f;
constexpr float ParticleWakeRadiusWorld = 0.75f;
constexpr float ParticleTurbulenceWorld = 0.025f;
constexpr float MaximumParticleFlowSpeedWorld = 4.0f;
constexpr float MaximumWakeCameraSpeedWorld = 8.0f;
constexpr int ParticleSpawnRequestsPerFrame = 12;
constexpr int ParticleSpawnAttemptsPerRequest = 8;

const char* FullscreenVs = R"glsl(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
out vec2 fragUv;
uniform mat4 mvp;
void main() {
    fragUv = vertexTexCoord;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

const char* CausticsFs = R"glsl(
#version 330
in vec2 fragUv;
out vec4 finalColor;

uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D causticsLookup;
uniform vec2 viewportSize;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform float nearPlane;
uniform float farPlane;
uniform float liquidSurfaceY;
uniform float liquidVisibilityDepth;
// scale world, strength, speed, wave amount
uniform vec4 liquidRipple;
// direction radians, flow speed
uniform vec2 liquidFlow;
// caustics strength, scale multiplier, speed multiplier
uniform vec3 causticsVisual;
uniform float runtimeSeconds;

vec3 ReconstructWorldPosition(vec2 uv, float depth, out float sceneDistance)
{
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane
                    - zNdc * (farPlane - nearPlane), 0.00001);
    sceneDistance = forwardDistance
            / max(dot(rayDirection, cameraForward), 0.0001);
    return cameraPosition + rayDirection * sceneDistance;
}

mat2 Rotation(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, -s, s, c);
}

float SampleCausticLayer(
        vec2 projectedWorld,
        vec2 projectedFlowDirection,
        float rotationOffset)
{
    float worldScale = max(liquidRipple.x * causticsVisual.y, 0.05);
    float phase = runtimeSeconds * liquidRipple.z * causticsVisual.z;
    vec2 flowWorld = projectedFlowDirection * runtimeSeconds
            * min(max(liquidFlow.y, 0.0), 4.0) * 0.12;
    vec2 primaryUv = Rotation(0.21 + rotationOffset)
            * ((projectedWorld - flowWorld) / worldScale)
            + vec2(phase * 0.018, -phase * 0.013);
    vec2 secondaryUv = Rotation(-0.63 + rotationOffset * 0.37)
            * ((projectedWorld + flowWorld * 0.35) / (worldScale * 0.71))
            + vec2(-phase * 0.011, phase * 0.016)
            + vec2(0.37, 0.19);
    float primary = texture(causticsLookup, primaryUv).r;
    float secondary = texture(causticsLookup, secondaryUv).r;
    float softened = primary * (0.67 + secondary * 0.33);
    return smoothstep(0.10, 0.82, softened);
}

void main()
{
    vec2 screenUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec4 scene = texture(sceneColor, screenUv);
    float depth = texture(sceneDepth, screenUv).r;
    if (depth >= 0.999999 || causticsVisual.x <= 0.0) {
        finalColor = scene;
        return;
    }

    float sceneDistance = 0.0;
    vec3 world = ReconstructWorldPosition(screenUv, depth, sceneDistance);
    float belowSurface = liquidSurfaceY - world.y;
    if (belowSurface < 0.0) {
        finalColor = scene;
        return;
    }

    vec3 dx = dFdx(world);
    vec3 dy = dFdy(world);
    float expectedPixelWorld = max(
            2.0 * sceneDistance * tanHalfFov
                    / max(viewportSize.y, 1.0),
            0.0005);
    float largestDerivative = max(length(dx), length(dy));
    float continuity = 1.0 - smoothstep(
            expectedPixelWorld * 6.0,
            expectedPixelWorld * 18.0,
            largestDerivative);
    vec3 normalCross = cross(dx, dy);
    float normalLength = length(normalCross);
    if (continuity <= 0.0001 || normalLength <= 0.0000001) {
        finalColor = scene;
        return;
    }
    vec3 normal = normalCross / normalLength;
    vec3 absoluteNormal = abs(normal);
    vec3 projectionWeights = pow(absoluteNormal, vec3(4.0));
    projectionWeights /= max(
            projectionWeights.x + projectionWeights.y + projectionWeights.z,
            0.000001);
    vec2 horizontalFlowDirection = vec2(
            cos(liquidFlow.x), sin(liquidFlow.x));
    float pattern = 0.0;
    if (projectionWeights.x > 0.001) {
        pattern += SampleCausticLayer(
                        world.zy,
                        vec2(horizontalFlowDirection.y, 0.0),
                        1.17)
                * projectionWeights.x;
    }
    if (projectionWeights.y > 0.001) {
        pattern += SampleCausticLayer(
                        world.xz,
                        horizontalFlowDirection,
                        0.0)
                * projectionWeights.y;
    }
    if (projectionWeights.z > 0.001) {
        pattern += SampleCausticLayer(
                        world.xy,
                        vec2(horizontalFlowDirection.x, 0.0),
                        -0.91)
                * projectionWeights.z;
    }

    float depthFade = exp(-belowSurface / max(liquidVisibilityDepth, 0.05));
    float receiver = mix(0.35, 1.0, absoluteNormal.y);
    float surfaceMotion = mix(
            0.55,
            1.0,
            clamp(liquidRipple.w / 0.5, 0.0, 1.0));
    float effectStrength = clamp(causticsVisual.x, 0.0, 1.0)
            * depthFade * receiver * continuity * surfaceMotion;
    float signedModulation = (pattern - 0.38) * effectStrength * 0.65;
    float luminance = dot(max(scene.rgb, vec3(0.0)),
            vec3(0.2126, 0.7152, 0.0722));
    if (signedModulation > 0.0) {
        signedModulation *= 1.0 - smoothstep(0.80, 1.25, luminance);
    }
    float modulation = clamp(1.0 + signedModulation, 0.75, 1.35);
    float darkSurfaceFactor = 1.0 - smoothstep(0.08, 0.30, luminance);
    float positivePattern = max(pattern - 0.38, 0.0);
    float darkSurfaceLift = min(
            positivePattern * effectStrength * 0.10 * darkSurfaceFactor,
            0.04);
    vec3 rgb = max(scene.rgb, vec3(0.0)) * modulation
            + vec3(darkSurfaceLift);
    finalColor = vec4(min(rgb, vec3(65504.0)), scene.a);
}
)glsl";

const char* ParticleVs = R"glsl(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragUv;
out vec4 fragParticleColor;
void main() {
    fragUv = vertexTexCoord;
    fragParticleColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)glsl";

const char* ParticleFs = R"glsl(
#version 330
in vec2 fragUv;
in vec4 fragParticleColor;
out vec4 finalColor;
uniform vec3 particulateTint;
void main() {
    vec2 centered = fragUv * 2.0 - 1.0;
    float radiusSquared = dot(centered, centered);
    if (radiusSquared >= 1.0) discard;
    float mask = 1.0 - smoothstep(0.08, 1.0, radiusSquared);
    mask *= mask;
    float alpha = fragParticleColor.a * mask;
    finalColor = vec4(particulateTint * alpha, alpha);
}
)glsl";

std::uint32_t Hash(std::uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

float Hash01(std::uint32_t value)
{
    return static_cast<float>(Hash(value) & 0x00ffffffu)
            / static_cast<float>(0x01000000u);
}

int WrapCell(int value)
{
    value %= CausticsCellCount;
    return value < 0 ? value + CausticsCellCount : value;
}

Vector2 CausticFeaturePoint(int x, int y)
{
    const int wrappedX = WrapCell(x);
    const int wrappedY = WrapCell(y);
    const std::uint32_t seed = static_cast<std::uint32_t>(
            wrappedX + wrappedY * CausticsCellCount + 1);
    return Vector2{
            static_cast<float>(x) + 0.15f + Hash01(seed * 2u) * 0.70f,
            static_cast<float>(y) + 0.15f + Hash01(seed * 2u + 1u) * 0.70f};
}

engine::TextureHandle CreateCausticsLookup(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope)
{
    Image image = GenImageColor(CausticsTextureSize, CausticsTextureSize, BLACK);
    for (int y = 0; y < CausticsTextureSize; ++y) {
        for (int x = 0; x < CausticsTextureSize; ++x) {
            const float px = (static_cast<float>(x) + 0.5f)
                    / static_cast<float>(CausticsTextureSize)
                    * static_cast<float>(CausticsCellCount);
            const float py = (static_cast<float>(y) + 0.5f)
                    / static_cast<float>(CausticsTextureSize)
                    * static_cast<float>(CausticsCellCount);
            const int cellX = static_cast<int>(std::floor(px));
            const int cellY = static_cast<int>(std::floor(py));
            float nearest = std::numeric_limits<float>::max();
            float second = std::numeric_limits<float>::max();
            for (int oy = -1; oy <= 1; ++oy) {
                for (int ox = -1; ox <= 1; ++ox) {
                    const Vector2 feature = CausticFeaturePoint(
                            cellX + ox, cellY + oy);
                    const float dx = feature.x - px;
                    const float dy = feature.y - py;
                    const float distanceSquared = dx * dx + dy * dy;
                    if (distanceSquared < nearest) {
                        second = nearest;
                        nearest = distanceSquared;
                    } else if (distanceSquared < second) {
                        second = distanceSquared;
                    }
                }
            }
            const float edgeDistance = std::sqrt(second) - std::sqrt(nearest);
            const float ridge = std::pow(
                    std::clamp(1.0f - edgeDistance * 4.2f, 0.0f, 1.0f),
                    3.0f);
            const unsigned char channel = static_cast<unsigned char>(
                    std::lround(ridge * 255.0f));
            ImageDrawPixel(&image, x, y,
                    Color{channel, channel, channel, 255});
        }
    }
    const engine::TextureHandle result = assets.CreateTextureFromImage(
            scope,
            "procedural_underwater_caustics",
            image,
            engine::TextureColorUsage::LinearData,
            engine::TextureLoad_Mipmaps
                    | engine::TextureLoad_TrilinearFilter);
    UnloadImage(image);
    return result;
}

Rectangle Source(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f,
            static_cast<float>(texture.width), -static_cast<float>(texture.height)};
}

Rectangle Destination(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f,
            static_cast<float>(texture.width), static_cast<float>(texture.height)};
}

Vector3 SafeNormalize(Vector3 value, Vector3 fallback)
{
    return Vector3LengthSqr(value) > 0.0000001f
            ? Vector3Normalize(value) : fallback;
}

unsigned char Byte(float value)
{
    return static_cast<unsigned char>(
            std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

} // namespace

bool SectorUnderwaterRenderer::Initialize(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope)
{
    Shutdown();
    const bool causticsReady = InitializeCaustics(assets, scope);
    const bool particlesReady = InitializeParticles();
    resourcesReady = causticsReady && particlesReady;
    return resourcesReady;
}

bool SectorUnderwaterRenderer::InitializeCaustics(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope)
{
    causticsShader = LoadShaderFromMemory(FullscreenVs, CausticsFs);
    if (causticsShader.id == 0) return false;
#define LOC(field, name) causticsLocations.field = GetShaderLocation(causticsShader, name)
    LOC(sceneColor, "sceneColor");
    LOC(sceneDepth, "sceneDepth");
    LOC(lookup, "causticsLookup");
    LOC(viewportSize, "viewportSize");
    LOC(cameraPosition, "cameraPosition");
    LOC(cameraForward, "cameraForward");
    LOC(cameraRight, "cameraRight");
    LOC(cameraUp, "cameraUp");
    LOC(tanHalfFov, "tanHalfFov");
    LOC(aspectRatio, "aspectRatio");
    LOC(nearPlane, "nearPlane");
    LOC(farPlane, "farPlane");
    LOC(surfaceY, "liquidSurfaceY");
    LOC(visibilityDepth, "liquidVisibilityDepth");
    LOC(ripple, "liquidRipple");
    LOC(flow, "liquidFlow");
    LOC(visual, "causticsVisual");
    LOC(runtimeSeconds, "runtimeSeconds");
#undef LOC
    causticsLookup = CreateCausticsLookup(assets, scope);
    return !engine::IsNull(causticsLookup);
}

bool SectorUnderwaterRenderer::InitializeParticles()
{
    particleShader = LoadShaderFromMemory(ParticleVs, ParticleFs);
    if (particleShader.id == 0) return false;
    particleShader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(particleShader, "vertexPosition");
    particleShader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(particleShader, "vertexTexCoord");
    particleShader.locs[SHADER_LOC_VERTEX_COLOR] =
            GetShaderLocationAttrib(particleShader, "vertexColor");
    particleShader.locs[SHADER_LOC_MATRIX_MVP] =
            GetShaderLocation(particleShader, "mvp");
    particleLocations.tint = GetShaderLocation(particleShader, "particulateTint");
    particleMaterial = LoadMaterialDefault();
    particleDefaultTexture = particleMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    particleMaterial.shader = particleShader;

    particleMesh.vertexCount = MaxParticles * 4;
    particleMesh.triangleCount = MaxParticles * 2;
    particleMesh.vertices = static_cast<float*>(MemAlloc(
            sizeof(float) * particleVertices.size()));
    particleMesh.texcoords = static_cast<float*>(MemAlloc(
            sizeof(float) * particleTexcoords.size()));
    particleMesh.colors = static_cast<unsigned char*>(MemAlloc(
            sizeof(unsigned char) * particleColors.size()));
    particleMesh.indices = static_cast<unsigned short*>(MemAlloc(
            sizeof(unsigned short) * particleIndices.size()));
    if (particleMesh.vertices == nullptr || particleMesh.texcoords == nullptr
            || particleMesh.colors == nullptr || particleMesh.indices == nullptr) {
        return false;
    }
    for (int particleIndex = 0; particleIndex < MaxParticles; ++particleIndex) {
        const int vertex = particleIndex * 4;
        const int index = particleIndex * 6;
        particleIndices[static_cast<std::size_t>(index + 0)] =
                static_cast<unsigned short>(vertex + 0);
        particleIndices[static_cast<std::size_t>(index + 1)] =
                static_cast<unsigned short>(vertex + 1);
        particleIndices[static_cast<std::size_t>(index + 2)] =
                static_cast<unsigned short>(vertex + 2);
        particleIndices[static_cast<std::size_t>(index + 3)] =
                static_cast<unsigned short>(vertex + 0);
        particleIndices[static_cast<std::size_t>(index + 4)] =
                static_cast<unsigned short>(vertex + 2);
        particleIndices[static_cast<std::size_t>(index + 5)] =
                static_cast<unsigned short>(vertex + 3);
    }
    std::memcpy(particleMesh.indices, particleIndices.data(),
            sizeof(unsigned short) * particleIndices.size());
    UploadMesh(&particleMesh, true);
    return particleMesh.vaoId != 0;
}

void SectorUnderwaterRenderer::ResetParticles()
{
    for (Particle& particle : particles) particle = Particle{};
    visibleParticleCount = 0;
}

bool SectorUnderwaterRenderer::SpawnParticle(
        Particle& particle,
        const SectorCollisionWorld* sectorLookup,
        const Camera3D& camera,
        const SectorLiquidContact& contact)
{
    const float lowerY = std::max(
            contact.bottomY + 0.02f,
            camera.position.y - ParticleSpawnRadiusWorld);
    const float upperY = std::min(
            contact.surfaceY - 0.02f,
            camera.position.y + ParticleSpawnRadiusWorld);
    if (upperY <= lowerY) return false;

    for (int attempt = 0; attempt < ParticleSpawnAttemptsPerRequest; ++attempt) {
        const std::uint32_t sequence = spawnSequence++;
        const float angle = Hash01(sequence * 5u + 0u) * 2.0f * PI;
        const float radius = ParticleNearRadiusWorld
                + (ParticleSpawnRadiusWorld - ParticleNearRadiusWorld)
                        * std::sqrt(Hash01(sequence * 5u + 1u));
        const Vector3 position{
                camera.position.x + std::cos(angle) * radius,
                lowerY + (upperY - lowerY) * Hash01(sequence * 5u + 2u),
                camera.position.z + std::sin(angle) * radius};
        if (Vector3DistanceSqr(position, camera.position)
                > ParticleSpawnRadiusWorld * ParticleSpawnRadiusWorld) {
            continue;
        }
        if (sectorLookup != nullptr
                && sectorLookup->FindSectorContainingPointPreferCurrent(
                           Vector2{position.x, position.z}, contact.sectorId)
                        != contact.sectorId) {
            continue;
        }
        particle = Particle{};
        particle.active = true;
        particle.position = position;
        particle.phase = Hash01(sequence * 5u + 3u) * 2.0f * PI;
        particle.sizeScale = 0.65f + Hash01(sequence * 5u + 4u) * 0.70f;
        particle.driftDirection = SafeNormalize(
                Vector3{
                        Hash01(sequence * 7u + 1u) * 2.0f - 1.0f,
                        Hash01(sequence * 7u + 2u) * 2.0f - 1.0f,
                        Hash01(sequence * 7u + 3u) * 2.0f - 1.0f},
                Vector3{0.0f, 1.0f, 0.0f});
        return true;
    }
    particle = Particle{};
    return false;
}

int SectorUnderwaterRenderer::BuildParticleMesh(
        const Camera3D& camera,
        const SectorLiquidParticulateSettings& settings)
{
    const Vector3 forward = SafeNormalize(
            Vector3Subtract(camera.target, camera.position),
            Vector3{0.0f, 0.0f, -1.0f});
    const Vector3 right = SafeNormalize(
            Vector3CrossProduct(forward, camera.up),
            Vector3{1.0f, 0.0f, 0.0f});
    const Vector3 up = SafeNormalize(
            Vector3CrossProduct(right, forward),
            Vector3{0.0f, 1.0f, 0.0f});
    constexpr std::array<Vector2, 4> corners{
            Vector2{-1.0f, -1.0f}, Vector2{1.0f, -1.0f},
            Vector2{1.0f, 1.0f}, Vector2{-1.0f, 1.0f}};
    constexpr std::array<Vector2, 4> uvs{
            Vector2{0.0f, 0.0f}, Vector2{1.0f, 0.0f},
            Vector2{1.0f, 1.0f}, Vector2{0.0f, 1.0f}};
    int visible = 0;
    for (const Particle& particle : particles) {
        if (!particle.active || visible >= settings.amount) continue;
        const float distance = Vector3Distance(particle.position, camera.position);
        const float outerFade = std::clamp(
                (ParticleSpawnRadiusWorld - distance) / 0.7f,
                0.0f,
                1.0f);
        const float alpha = settings.opacity * outerFade;
        if (alpha <= 0.001f) continue;
        const int base = visible * 4;
        const float size = settings.sizeWorld * particle.sizeScale;
        for (int corner = 0; corner < 4; ++corner) {
            const Vector3 position = Vector3Add(
                    particle.position,
                    Vector3Add(
                            Vector3Scale(right,
                                    corners[static_cast<std::size_t>(corner)].x
                                            * size),
                            Vector3Scale(up,
                                    corners[static_cast<std::size_t>(corner)].y
                                            * size)));
            const std::size_t vertex3 = static_cast<std::size_t>(base + corner) * 3;
            const std::size_t vertex2 = static_cast<std::size_t>(base + corner) * 2;
            const std::size_t vertex4 = static_cast<std::size_t>(base + corner) * 4;
            particleVertices[vertex3 + 0] = position.x;
            particleVertices[vertex3 + 1] = position.y;
            particleVertices[vertex3 + 2] = position.z;
            particleTexcoords[vertex2 + 0] = uvs[static_cast<std::size_t>(corner)].x;
            particleTexcoords[vertex2 + 1] = uvs[static_cast<std::size_t>(corner)].y;
            particleColors[vertex4 + 0] = 255;
            particleColors[vertex4 + 1] = 255;
            particleColors[vertex4 + 2] = 255;
            particleColors[vertex4 + 3] = Byte(alpha);
        }
        ++visible;
    }
    if (visible <= 0) return 0;
    const int vertexCount = visible * 4;
    const int indexCount = visible * 6;
    std::memcpy(particleMesh.vertices, particleVertices.data(),
            sizeof(float) * static_cast<std::size_t>(vertexCount) * 3);
    std::memcpy(particleMesh.texcoords, particleTexcoords.data(),
            sizeof(float) * static_cast<std::size_t>(vertexCount) * 2);
    std::memcpy(particleMesh.colors, particleColors.data(),
            sizeof(unsigned char) * static_cast<std::size_t>(vertexCount) * 4);
    particleMesh.vertexCount = vertexCount;
    particleMesh.triangleCount = visible * 2;
    UpdateMeshBuffer(particleMesh, 0, particleMesh.vertices,
            vertexCount * 3 * static_cast<int>(sizeof(float)), 0);
    UpdateMeshBuffer(particleMesh, 1, particleMesh.texcoords,
            vertexCount * 2 * static_cast<int>(sizeof(float)), 0);
    UpdateMeshBuffer(particleMesh, 3, particleMesh.colors,
            vertexCount * 4 * static_cast<int>(sizeof(unsigned char)), 0);
    UpdateMeshBuffer(particleMesh, 6, particleMesh.indices,
            indexCount * static_cast<int>(sizeof(unsigned short)), 0);
    return visible;
}

bool SectorUnderwaterRenderer::ApplyCaustics(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& sceneScratch,
        engine::AssetManager& assets,
        const Camera3D& camera,
        float runtimeSeconds,
        const SectorUnderwaterRenderContext& context)
{
    if (!context.cameraSubmerged || !context.contact.hasLiquid
            || context.causticsStrength <= 0.0f
            || causticsShader.id == 0
            || camera.projection != CAMERA_PERSPECTIVE
            || sceneTarget.texture.id == 0 || sceneTarget.depth.id == 0) {
        return false;
    }
    const Texture2D* lookup = assets.GetTexture(causticsLookup);
    if (lookup == nullptr || lookup->id == 0) return false;
    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane) return false;
    const float aspect = static_cast<float>(sceneTarget.texture.width)
            / static_cast<float>(std::max(sceneTarget.texture.height, 1));
    const Vector2 viewportSize{
            static_cast<float>(sceneTarget.texture.width),
            static_cast<float>(sceneTarget.texture.height)};
    const Vector3 forward = SafeNormalize(
            Vector3Subtract(camera.target, camera.position),
            Vector3{0.0f, 0.0f, -1.0f});
    const Vector3 right = SafeNormalize(
            Vector3CrossProduct(forward, camera.up),
            Vector3{1.0f, 0.0f, 0.0f});
    const Vector3 up = SafeNormalize(
            Vector3CrossProduct(right, forward),
            Vector3{0.0f, 1.0f, 0.0f});
    const float tanHalfFov = std::tan(camera.fovy * DEG2RAD * 0.5f);
    const SectorLiquidSettings& liquid = context.contact.settings;
    const Vector4 ripple{
            liquid.rippleScaleWorld,
            liquid.rippleStrength,
            liquid.rippleSpeed,
            liquid.rippleStrength};
    const Vector2 flow{
            liquid.flowDirectionDegrees * DEG2RAD,
            liquid.flowSpeedWorld};
    const Vector3 visual{
            context.causticsStrength,
            context.causticsScaleMultiplier,
            context.causticsSpeedMultiplier};
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneScratch);
    ClearBackground(BLANK);
    BeginShaderMode(causticsShader);
    SetShaderValueTexture(causticsShader, causticsLocations.sceneColor,
            sceneTarget.texture);
    SetShaderValueTexture(causticsShader, causticsLocations.sceneDepth,
            sceneTarget.depth);
    SetShaderValueTexture(causticsShader, causticsLocations.lookup, *lookup);
    SetShaderValue(causticsShader, causticsLocations.viewportSize,
            &viewportSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(causticsShader, causticsLocations.cameraPosition,
            &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(causticsShader, causticsLocations.cameraForward,
            &forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(causticsShader, causticsLocations.cameraRight,
            &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(causticsShader, causticsLocations.cameraUp,
            &up, SHADER_UNIFORM_VEC3);
    SetShaderValue(causticsShader, causticsLocations.tanHalfFov,
            &tanHalfFov, SHADER_UNIFORM_FLOAT);
    SetShaderValue(causticsShader, causticsLocations.aspectRatio,
            &aspect, SHADER_UNIFORM_FLOAT);
    SetShaderValue(causticsShader, causticsLocations.nearPlane,
            &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(causticsShader, causticsLocations.farPlane,
            &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(causticsShader, causticsLocations.surfaceY,
            &context.contact.surfaceY, SHADER_UNIFORM_FLOAT);
    SetShaderValue(causticsShader, causticsLocations.visibilityDepth,
            &liquid.visibilityDepthWorld, SHADER_UNIFORM_FLOAT);
    SetShaderValue(causticsShader, causticsLocations.ripple,
            &ripple, SHADER_UNIFORM_VEC4);
    SetShaderValue(causticsShader, causticsLocations.flow,
            &flow, SHADER_UNIFORM_VEC2);
    SetShaderValue(causticsShader, causticsLocations.visual,
            &visual, SHADER_UNIFORM_VEC3);
    SetShaderValue(causticsShader, causticsLocations.runtimeSeconds,
            &runtimeSeconds, SHADER_UNIFORM_FLOAT);
    rlDisableColorBlend();
    DrawTexturePro(sceneTarget.texture, Source(sceneTarget.texture),
            Destination(sceneScratch.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndShaderMode();
    EndTextureMode();
    return true;
}

bool SectorUnderwaterRenderer::DrawParticles(
        RenderTexture2D& sceneTarget,
        const SectorCollisionWorld* sectorLookup,
        const Camera3D& camera,
        float runtimeSeconds,
        const SectorUnderwaterRenderContext& context)
{
    visibleParticleCount = 0;
    if (!context.cameraSubmerged || !context.contact.hasLiquid
            || context.contact.settings.particulates.amount <= 0
            || context.contact.settings.particulates.opacity <= 0.0f
            || particleMesh.vaoId == 0 || particleShader.id == 0) {
        ResetParticles();
        hasPreviousCamera = false;
        previousRuntimeSeconds = runtimeSeconds;
        return false;
    }
    const SectorLiquidParticulateSettings& settings =
            context.contact.settings.particulates;
    float dt = previousRuntimeSeconds > 0.0f
            ? runtimeSeconds - previousRuntimeSeconds
            : 1.0f / 60.0f;
    previousRuntimeSeconds = runtimeSeconds;
    if (!std::isfinite(dt) || dt <= 0.0f || dt > 0.1f) dt = 1.0f / 60.0f;

    const bool sectorChanged = activeSectorId != context.contact.sectorId;
    const float cameraTravel = hasPreviousCamera
            ? Vector3Distance(camera.position, previousCameraPosition) : 0.0f;
    const bool teleported = hasPreviousCamera && cameraTravel > 2.0f;
    if (!hasPreviousCamera || sectorChanged || teleported) {
        ResetParticles();
        activeSectorId = context.contact.sectorId;
    }
    Vector3 cameraVelocity{};
    if (hasPreviousCamera && !teleported) {
        cameraVelocity = Vector3Scale(
                Vector3Subtract(camera.position, previousCameraPosition),
                1.0f / dt);
        const float speed = Vector3Length(cameraVelocity);
        if (speed > MaximumWakeCameraSpeedWorld) {
            cameraVelocity = Vector3Scale(
                    cameraVelocity,
                    MaximumWakeCameraSpeedWorld / speed);
        }
    }
    previousCameraPosition = camera.position;
    hasPreviousCamera = true;

    const int requested = std::clamp(
            settings.amount, 0, MaxParticles);
    const Vector3 flowVelocity{
            std::cos(context.contact.settings.flowDirectionDegrees * DEG2RAD)
                    * std::min(context.contact.settings.flowSpeedWorld,
                            MaximumParticleFlowSpeedWorld)
                    * settings.flowInfluence,
            0.0f,
            std::sin(context.contact.settings.flowDirectionDegrees * DEG2RAD)
                    * std::min(context.contact.settings.flowSpeedWorld,
                            MaximumParticleFlowSpeedWorld)
                    * settings.flowInfluence};
    const Vector3 cameraDirection = SafeNormalize(
            cameraVelocity, Vector3{0.0f, 0.0f, -1.0f});
    const float cameraSpeed = Vector3Length(cameraVelocity);
    int spawnRequestsRemaining = ParticleSpawnRequestsPerFrame;

    for (int particleIndex = 0; particleIndex < MaxParticles; ++particleIndex) {
        Particle& particle = particles[static_cast<std::size_t>(particleIndex)];
        if (particleIndex >= requested) {
            particle = Particle{};
            continue;
        }
        if (!particle.active) {
            if (spawnRequestsRemaining <= 0) continue;
            --spawnRequestsRemaining;
            if (!SpawnParticle(
                        particle, sectorLookup, camera, context.contact)) {
                continue;
            }
        }
        const Vector3 wobble{
                std::sin(runtimeSeconds * 0.43f + particle.phase),
                std::sin(runtimeSeconds * 0.31f + particle.phase * 1.7f),
                std::cos(runtimeSeconds * 0.37f + particle.phase * 0.8f)};
        Vector3 velocity = Vector3Add(
                Vector3Scale(particle.driftDirection, ParticleTurbulenceWorld),
                Vector3Scale(wobble, ParticleTurbulenceWorld));
        velocity = Vector3Add(velocity, flowVelocity);

        const Vector3 fromCamera = Vector3Subtract(
                particle.position, camera.position);
        const float wakeDistance = Vector3Length(fromCamera);
        if (cameraSpeed > 0.01f && wakeDistance < ParticleWakeRadiusWorld) {
            const Vector3 along = Vector3Scale(
                    cameraDirection,
                    Vector3DotProduct(fromCamera, cameraDirection));
            const Vector3 lateral = Vector3Subtract(fromCamera, along);
            const Vector3 pushDirection = SafeNormalize(
                    lateral,
                    SafeNormalize(Vector3CrossProduct(cameraDirection, camera.up),
                            Vector3{1.0f, 0.0f, 0.0f}));
            const float wake = 1.0f - wakeDistance / ParticleWakeRadiusWorld;
            velocity = Vector3Add(
                    velocity,
                    Vector3Scale(pushDirection,
                            cameraSpeed * settings.wakeInfluence * wake * wake));
        }
        particle.position = Vector3Add(
                particle.position, Vector3Scale(velocity, dt));

        const bool outsideRadius = Vector3DistanceSqr(
                particle.position, camera.position)
                > ParticleSpawnRadiusWorld * ParticleSpawnRadiusWorld;
        const bool outsideVertical = particle.position.y
                        <= context.contact.bottomY + 0.01f
                || particle.position.y >= context.contact.surfaceY - 0.01f;
        if (outsideRadius || outsideVertical) {
            particle = Particle{};
            if (spawnRequestsRemaining > 0) {
                --spawnRequestsRemaining;
                SpawnParticle(
                        particle, sectorLookup, camera, context.contact);
            }
        }
    }

    visibleParticleCount = BuildParticleMesh(camera, settings);
    if (visibleParticleCount <= 0) return false;
    const Vector3 shallow = engine::SrgbColorBytesToLinearSceneRgb(
            context.contact.settings.shallowColor);
    const Vector3 tint = Vector3Lerp(shallow, Vector3{1.0f, 1.0f, 1.0f}, 0.35f);
    if (particleLocations.tint >= 0) {
        SetShaderValue(particleShader, particleLocations.tint,
                &tint, SHADER_UNIFORM_VEC3);
    }
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneTarget);
    BeginMode3D(camera);
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY);
    rlEnableDepthTest();
    rlDisableDepthMask();
    DrawMesh(particleMesh, particleMaterial, MatrixIdentity());
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlSetBlendMode(BLEND_ALPHA);
    EndMode3D();
    EndTextureMode();
    return true;
}

void SectorUnderwaterRenderer::Shutdown()
{
    ResetParticles();
    if (particleMesh.vaoId != 0 || particleMesh.vertices != nullptr) {
        UnloadMesh(particleMesh);
    }
    particleMesh = {};
    if (particleMaterial.maps != nullptr) {
        particleMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
                particleDefaultTexture;
        UnloadMaterial(particleMaterial);
    } else if (particleShader.id != 0) {
        UnloadShader(particleShader);
    }
    particleMaterial = {};
    particleDefaultTexture = {};
    particleShader = {};
    particleLocations = {};
    if (causticsShader.id != 0) UnloadShader(causticsShader);
    causticsShader = {};
    causticsLocations = {};
    causticsLookup = engine::NullTextureHandle();
    previousCameraPosition = {};
    previousRuntimeSeconds = 0.0f;
    spawnSequence = 1;
    activeSectorId = 0;
    hasPreviousCamera = false;
    resourcesReady = false;
}

} // namespace game
