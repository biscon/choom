#include "sector_demo/renderer/SectorLightProxyRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorMeshTypes.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace game {
namespace {

const char* ProxyVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
out vec2 fragProxyUv;
out vec3 fragWorldPosition;
out vec3 fragRadiance;
out vec2 fragProxyData;
void main() {
    fragProxyUv = vertexTexCoord;
    fragWorldPosition = vertexPosition;
    fragRadiance = vertexNormal;
    fragProxyData = vertexColor.rg;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* ProxyFs = R"(
#version 330
in vec2 fragProxyUv;
in vec3 fragWorldPosition;
in vec3 fragRadiance;
in vec2 fragProxyData; // softness, maximum opacity
out vec4 finalColor;
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform float nearPlane;
uniform float farPlane;
uniform vec4 fogParamsA; // mode, start, end/density, max opacity
uniform vec4 fogParamsB; // exponent, reference height, height falloff, unused

float linearDepth(float depth) {
    float zNdc = depth * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
}

float fogTransmittance(vec3 position) {
    if (fogParamsA.x < 0.5) return 1.0;
    float amount = 0.0;
    if (fogParamsA.x < 1.5) {
        float distance = max(length(position - cameraPosition) - fogParamsA.y, 0.0);
        float midpointHeight = (cameraPosition.y + position.y) * 0.5;
        float height = max(midpointHeight - fogParamsB.y, 0.0);
        amount = min(1.0 - exp(-fogParamsA.z * distance
                * exp(-height * fogParamsB.z)), fogParamsA.w);
    } else {
        amount = pow(clamp((dot(position - cameraPosition, cameraForward) - fogParamsA.y)
                / max(fogParamsA.z - fogParamsA.y, 0.0001), 0.0, 1.0),
                max(fogParamsB.x, 0.0001)) * fogParamsA.w;
    }
    return 1.0 - clamp(amount, 0.0, 1.0);
}

void main() {
    vec2 screenUv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    float sceneForward = linearDepth(texture(sceneDepth, screenUv).r);
    float proxyForward = dot(fragWorldPosition - cameraPosition, cameraForward);
    float depthDelta = proxyForward - sceneForward;
    float occlusionBias = 0.12;
    float occlusionFeather = 0.08;
    float fragmentVisibility = 1.0 - smoothstep(
            occlusionBias, occlusionBias + occlusionFeather, depthDelta);
    float softness = clamp(fragProxyData.x, 0.01, 1.0);
    float radius = length(fragProxyUv * 2.0 - 1.0);
    float mask = 1.0 - smoothstep(1.0 - softness, 1.0, radius);
    float opacity = clamp(fragProxyData.y, 0.0, 1.0) * mask
            * fragmentVisibility * fogTransmittance(fragWorldPosition);
    if (opacity <= 0.00001) discard;
    finalColor = vec4(min(max(fragRadiance, vec3(0.0)), vec3(65504.0)), opacity);
}
)";

int FindDynamicIndex(
        const SectorLightAtmosphereSource& source,
        const SectorBillboardDynamicLightContext& lights)
{
    if (!IsSectorLightAtmosphereSourceDynamic(source)) return -1;
    const int type = source.kind == SectorLightAtmosphereSourceKind::DynamicSpot ? 1 : 0;
    for (int index = 0; index < lights.dynamicLightCount; ++index) {
        if (lights.dynamicLightIds[static_cast<std::size_t>(index)] == source.lightId
                && lights.dynamicLightTypes[static_cast<std::size_t>(index)] == type) return index;
    }
    return -1;
}

Vector3 Multiply(Vector3 left, Vector3 right)
{
    return Vector3{left.x * right.x, left.y * right.y, left.z * right.z};
}

} // namespace

void SectorLightProxyRenderer::Reserve(std::size_t sourceCount)
{
    EnsureCapacity(sourceCount);
    visibleHalos.reserve(sourceCount);
}

bool SectorLightProxyRenderer::EnsureResources()
{
    if (shader.id != 0) return true;
    if (shaderFailed) return false;
    shader = LoadShaderFromMemory(ProxyVs, ProxyFs);
    if (shader.id == 0) { shaderFailed = true; return false; }
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocation(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocation(shader, "vertexTexCoord");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocation(shader, "vertexNormal");
    shader.locs[SHADER_LOC_VERTEX_COLOR] = GetShaderLocation(shader, "vertexColor");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "sceneDepth");
    viewportSizeLoc = GetShaderLocation(shader, "viewportSize");
    cameraPositionLoc = GetShaderLocation(shader, "cameraPosition");
    cameraForwardLoc = GetShaderLocation(shader, "cameraForward");
    nearPlaneLoc = GetShaderLocation(shader, "nearPlane");
    farPlaneLoc = GetShaderLocation(shader, "farPlane");
    fogParamsALoc = GetShaderLocation(shader, "fogParamsA");
    fogParamsBLoc = GetShaderLocation(shader, "fogParamsB");
    material = LoadMaterialDefault();
    material.shader = shader;
    return true;
}

bool SectorLightProxyRenderer::EnsureCapacity(std::size_t requested)
{
    if (requested <= quadCapacity) return true;
    if (mesh.vaoId != 0) UnloadMesh(mesh);
    quadCapacity = std::max(requested, quadCapacity * 2 + 8);
    const std::size_t vertexCapacity = quadCapacity * 6;
    vertices.assign(vertexCapacity * 3, 0.0f);
    texcoords.assign(vertexCapacity * 2, 0.0f);
    normals.assign(vertexCapacity * 3, 0.0f);
    colors.assign(vertexCapacity * 4, 0);
    mesh = {};
    mesh.vertexCount = static_cast<int>(vertexCapacity);
    mesh.triangleCount = static_cast<int>(vertexCapacity / 3);
    mesh.vertices = static_cast<float*>(MemAlloc(vertices.size() * sizeof(float)));
    mesh.texcoords = static_cast<float*>(MemAlloc(texcoords.size() * sizeof(float)));
    mesh.normals = static_cast<float*>(MemAlloc(normals.size() * sizeof(float)));
    mesh.colors = static_cast<unsigned char*>(MemAlloc(colors.size() * sizeof(unsigned char)));
    if (mesh.vertices == nullptr || mesh.texcoords == nullptr || mesh.normals == nullptr
            || mesh.colors == nullptr) {
        UnloadMesh(mesh);
        mesh = {};
        quadCapacity = 0;
        return false;
    }
    std::memcpy(mesh.vertices, vertices.data(), vertices.size() * sizeof(float));
    std::memcpy(mesh.texcoords, texcoords.data(), texcoords.size() * sizeof(float));
    std::memcpy(mesh.normals, normals.data(), normals.size() * sizeof(float));
    std::memcpy(mesh.colors, colors.data(), colors.size() * sizeof(unsigned char));
    UploadMesh(&mesh, true);
    return mesh.vaoId != 0;
}

bool SectorLightProxyRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& colorOnlyTarget,
        const SectorTopologyFogSettings& sourceFogSettings,
        SectorVolumetricQuality quality,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds)
{
    eligibleCount = haloCount = drawCallCount = 0;
    visibleHalos.clear();
    if (quality == SectorVolumetricQuality::Off || sources.empty()
            || sceneTarget.depth.id == 0 || colorOnlyTarget.id == 0
            || !EnsureResources() || !EnsureCapacity(sources.size())) return false;
    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    const int width = sceneTarget.texture.width;
    const int height = sceneTarget.texture.height;
    const float aspect = static_cast<float>(width) / std::max(height, 1);
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 cameraRight = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 cameraUp = Vector3Normalize(Vector3CrossProduct(cameraRight, forward));
    for (const SectorLightAtmosphereSource& source : sources) {
        if (!IsSectorLightAtmosphereSourceSelected(source, dynamicLights)) continue;
        const SectorLightProxySettings& proxy = source.atmosphere.proxy;
        if (!proxy.halo.enabled || proxy.halo.brightness <= 0.0f
                || proxy.halo.maxOpacity <= 0.0f) continue;
        SectorLightAtmosphereVolume volume;
        volume.source = &source;
        volume.originWorld = source.positionWorld;
        volume.boundsCenterWorld = source.positionWorld;
        volume.boundsRadiusWorld = proxy.halo.radiusWorld;
        volume.extentWorld = proxy.halo.radiusWorld;
        if (!IsSectorLightAtmosphereVolumeVisible(volume, visibility, receiverBounds,
                    camera, aspect, nearPlane, farPlane)) continue;
        Vector3 lightColor = engine::SrgbColorBytesToLinearSceneRgb(source.color);
        float intensity = source.intensity;
        const int dynamicIndex = FindDynamicIndex(source, dynamicLights);
        if (dynamicIndex >= 0) {
            lightColor = dynamicLights.dynamicLightColors[static_cast<std::size_t>(dynamicIndex)];
            intensity = dynamicLights.dynamicLightIntensities[static_cast<std::size_t>(dynamicIndex)];
        }
        const Vector3 tint = engine::SrgbColorBytesToLinearSceneRgb(proxy.tint);
        visibleHalos.push_back(VisibleHalo{
                &source,
                Vector3Scale(Multiply(lightColor, tint), intensity * proxy.halo.brightness),
                Vector3DistanceSqr(camera.position, source.positionWorld)});
    }
    std::sort(visibleHalos.begin(), visibleHalos.end(), [](const auto& left, const auto& right) {
        if (left.distanceSquared != right.distanceSquared) {
            return left.distanceSquared > right.distanceSquared;
        }
        if (left.source->lightId != right.source->lightId) {
            return left.source->lightId < right.source->lightId;
        }
        return static_cast<int>(left.source->kind) < static_cast<int>(right.source->kind);
    });
    eligibleCount = haloCount = static_cast<int>(visibleHalos.size());
    if (visibleHalos.empty()) return false;

    std::size_t vertex = 0;
    const auto appendQuad = [&](const Vector3 (&positions)[4], const Vector3 radiance,
                                float softness, float maxOpacity) {
        const int order[6] = {0, 1, 2, 0, 2, 3};
        const Vector2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (int i = 0; i < 6; ++i, ++vertex) {
            const int corner = order[i];
            std::memcpy(&vertices[vertex * 3], &positions[corner], sizeof(Vector3));
            std::memcpy(&normals[vertex * 3], &radiance, sizeof(Vector3));
            texcoords[vertex * 2] = uv[corner].x;
            texcoords[vertex * 2 + 1] = uv[corner].y;
            colors[vertex * 4] = static_cast<unsigned char>(std::clamp(softness, 0.0f, 1.0f) * 255.0f);
            colors[vertex * 4 + 1] = static_cast<unsigned char>(std::clamp(maxOpacity, 0.0f, 1.0f) * 255.0f);
            colors[vertex * 4 + 2] = 0;
            colors[vertex * 4 + 3] = 255;
        }
    };
    for (const VisibleHalo& visible : visibleHalos) {
        const SectorLightAtmosphereSource& source = *visible.source;
        const SectorLightProxySettings& proxy = source.atmosphere.proxy;
        const Vector3 horizontal = Vector3Scale(cameraRight, proxy.halo.radiusWorld);
        const Vector3 vertical = Vector3Scale(cameraUp, proxy.halo.radiusWorld);
        const Vector3 p[4] = {
            Vector3Subtract(Vector3Subtract(source.positionWorld, horizontal), vertical),
            Vector3Add(Vector3Subtract(source.positionWorld, vertical), horizontal),
            Vector3Add(Vector3Add(source.positionWorld, horizontal), vertical),
            Vector3Add(Vector3Subtract(source.positionWorld, horizontal), vertical)};
        appendQuad(p, visible.radiance, proxy.halo.edgeSoftness, proxy.halo.maxOpacity);
    }
    if (vertex == 0) return false;
    mesh.vertexCount = static_cast<int>(vertex);
    mesh.triangleCount = static_cast<int>(vertex / 3);
    UpdateMeshBuffer(mesh, 0, vertices.data(), static_cast<int>(vertex * 3 * sizeof(float)), 0);
    UpdateMeshBuffer(mesh, 1, texcoords.data(), static_cast<int>(vertex * 2 * sizeof(float)), 0);
    UpdateMeshBuffer(mesh, 2, normals.data(), static_cast<int>(vertex * 3 * sizeof(float)), 0);
    UpdateMeshBuffer(mesh, 3, colors.data(), static_cast<int>(vertex * 4 * sizeof(unsigned char)), 0);
    const Vector2 viewport{static_cast<float>(width), static_cast<float>(height)};
    const SectorTopologyFogSettings fog = NormalizeSectorTopologyFogSettings(sourceFogSettings);
    const float fogMode = !fog.enabled ? 0.0f
            : (fog.mode == SectorTopologyFogMode::Distance ? 2.0f : 1.0f);
    const Vector4 fogA{fogMode, fog.startDistanceWorld,
            fog.mode == SectorTopologyFogMode::Distance ? fog.endDistanceWorld : fog.density,
            fog.maxOpacity};
    const Vector4 fogB{fog.falloffExponent, fog.referenceHeightWorld, fog.heightFalloff, 0.0f};
    material.maps[MATERIAL_MAP_DIFFUSE].texture = sceneTarget.depth;
    rlDrawRenderBatchActive();
    BeginTextureMode(colorOnlyTarget);
    BeginMode3D(camera);
    SetShaderValue(shader, viewportSizeLoc, &viewport, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, cameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraForwardLoc, &forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, nearPlaneLoc, &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, farPlaneLoc, &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, fogParamsALoc, &fogA, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, fogParamsBLoc, &fogB, SHADER_UNIFORM_VEC4);
    // Brightness controls HDR radiance while alpha independently caps coverage.
    BeginBlendMode(BLEND_ALPHA);
    rlColorMask(true, true, true, false);
    rlDisableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();
    DrawMesh(mesh, material, MatrixIdentity());
    ++drawCallCount;
    rlEnableBackfaceCulling();
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlColorMask(true, true, true, true);
    EndBlendMode();
    EndMode3D();
    EndTextureMode();
    material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
    return true;
}

void SectorLightProxyRenderer::Shutdown()
{
    if (mesh.vaoId != 0) {
        UnloadMesh(mesh);
    }
    mesh = {};
    if (material.maps != nullptr) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
        material.shader = {};
        UnloadMaterial(material);
    }
    material = {};
    if (shader.id != 0) UnloadShader(shader);
    shader = {};
    shaderFailed = false;
    quadCapacity = 0;
    vertices.clear(); texcoords.clear(); normals.clear(); colors.clear();
    visibleHalos.clear();
    visibleHalos.shrink_to_fit();
}

} // namespace game
