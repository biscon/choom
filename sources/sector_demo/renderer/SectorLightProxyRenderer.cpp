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

const char* ScreenVs = R"(
#version 330
in vec3 vertexPosition;
void main() { gl_Position = vec4(vertexPosition.xy, 0.0, 1.0); }
)";

const char* AnalyticHaloFs = R"(
#version 330
out vec4 finalColor;
uniform sampler2D sceneDepth;
uniform vec2 viewportSize;
uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform float tanHalfFov;
uniform float aspectRatio;
uniform float nearPlane;
uniform float farPlane;
uniform vec3 sphereCenter;
uniform float sphereRadius;
uniform vec3 haloRadiance;
uniform vec2 haloParams; // edge softness, maximum extinction
uniform vec4 fogParamsA; // mode, start, end/density, maximum opacity
uniform vec4 fogParamsB; // exponent, reference height, height falloff, unused

bool intersectSphere(vec3 rayOrigin, vec3 rayDirection,
        out float enterT, out float exitT) {
    vec3 offset = rayOrigin - sphereCenter;
    float b = dot(offset, rayDirection);
    float c = dot(offset, offset) - sphereRadius * sphereRadius;
    float discriminant = b * b - c;
    if (discriminant < 0.0) return false;
    float root = sqrt(max(discriminant, 0.0));
    enterT = -b - root;
    exitT = -b + root;
    return exitT > max(enterT, 0.0);
}

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    return lengthSquared > 0.00000001 ? value * inversesqrt(lengthSquared) : fallback;
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
    vec2 uv = gl_FragCoord.xy / max(viewportSize, vec2(1.0));
    vec2 ndc = uv * 2.0 - 1.0;
    vec3 rayDirection = normalize(cameraForward
            + cameraRight * ndc.x * tanHalfFov * aspectRatio
            + cameraUp * ndc.y * tanHalfFov);
    float enterT = 0.0;
    float exitT = 0.0;
    if (!intersectSphere(cameraPosition, rayDirection, enterT, exitT)) discard;

    float unclippedEnterT = max(enterT, 0.0);
    float depth = texture(sceneDepth, uv).r;
    float zNdc = depth * 2.0 - 1.0;
    float forwardDistance = (2.0 * nearPlane * farPlane)
            / max(farPlane + nearPlane - zNdc * (farPlane - nearPlane), 0.00001);
    float sceneDistance = depth >= 0.999999 ? farPlane
            : forwardDistance / max(dot(rayDirection, cameraForward), 0.0001);
    enterT = unclippedEnterT;
    exitT = min(exitT, sceneDistance);
    float visibleChord = max(exitT - enterT, 0.0);
    if (visibleChord <= 0.00001) discard;

    float closestT = max(dot(sphereCenter - cameraPosition, rayDirection), 0.0);
    vec3 closestPoint = cameraPosition + rayDirection * closestT;
    float radial = clamp(length(closestPoint - sphereCenter)
            / max(sphereRadius, 0.0001), 0.0, 1.0);
    float mappedSoftness = clamp(haloParams.x * 2.0, 0.02, 2.0);
    float baseSoftness = min(mappedSoftness, 1.0);
    float extraSoftness = max(mappedSoftness - 1.0, 0.0);
    float broad = 1.0 - smoothstep(1.0 - baseSoftness, 1.0, radial);
    float core = pow(max(1.0 - radial, 0.0), mix(8.0, 3.0, baseSoftness));
    float profile = clamp(0.35 * broad + 0.65 * core, 0.0, 1.0);
    profile = pow(profile, 1.0 + 1.5 * extraSoftness);

    float midpointT = (enterT + exitT) * 0.5;
    vec3 midpoint = cameraPosition + rayDirection * midpointT;
    float normalizedChord = clamp(visibleChord / max(2.0 * sphereRadius, 0.0001), 0.0, 1.0);
    float opticalThickness = 1.0 - exp(-2.5 * normalizedChord * profile);
    float phaseT = mix(enterT, exitT, 0.35);
    vec3 phasePosition = cameraPosition + rayDirection * phaseT;
    vec3 lightTravel = safeNormalize(phasePosition - sphereCenter, -rayDirection);
    float phaseFacing = clamp(dot(lightTravel, -rayDirection) * 0.5 + 0.5, 0.0, 1.0);
    float phase = mix(0.35, 1.0, pow(phaseFacing, 3.0));
    float scatterWeight = opticalThickness * phase * fogTransmittance(midpoint);
    float extinction = clamp(haloParams.y, 0.0, 1.0) * opticalThickness;
    if (scatterWeight <= 0.00001 && extinction <= 0.00001) discard;
    vec3 inScattering = min(max(haloRadiance * scatterWeight, vec3(0.0)), vec3(65504.0));
    if (any(isnan(inScattering)) || any(isinf(inScattering))
            || isnan(extinction) || isinf(extinction)) discard;
    finalColor = vec4(inScattering, extinction);
}
)";

bool EnsureScreenTriangle(Mesh& mesh)
{
    if (mesh.vaoId != 0) return true;
    constexpr float vertices[] = {
            -1.0f, -1.0f, 0.0f,
             3.0f, -1.0f, 0.0f,
            -1.0f,  3.0f, 0.0f};
    mesh.vertexCount = 3;
    mesh.triangleCount = 1;
    mesh.vertices = static_cast<float*>(MemAlloc(sizeof(vertices)));
    if (mesh.vertices == nullptr) {
        mesh = {};
        return false;
    }
    std::memcpy(mesh.vertices, vertices, sizeof(vertices));
    UploadMesh(&mesh, false);
    if (mesh.vaoId == 0) {
        UnloadMesh(mesh);
        mesh = {};
        return false;
    }
    return true;
}

int FindDynamicIndex(
        const SectorLightAtmosphereSource& source,
        const SectorBillboardDynamicLightContext& lights)
{
    if (!IsSectorLightAtmosphereSourceDynamic(source)) return -1;
    const int type = source.kind == SectorLightAtmosphereSourceKind::DynamicSpot ? 1
            : source.kind == SectorLightAtmosphereSourceKind::DynamicRect ? 2 : 0;
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
    visibleHalos.reserve(sourceCount);
}

bool SectorLightProxyRenderer::EnsureResources()
{
    if (shader.id != 0 && screenTriangle.vaoId != 0
            && material.maps != nullptr) return true;
    if (shaderFailed) return false;
    shader = LoadShaderFromMemory(ScreenVs, AnalyticHaloFs);
    if (shader.id == 0) { shaderFailed = true; return false; }
#define LOC(field, name) field = GetShaderLocation(shader, name)
    LOC(sceneDepthLoc, "sceneDepth"); LOC(viewportSizeLoc, "viewportSize");
    LOC(cameraPositionLoc, "cameraPosition"); LOC(cameraForwardLoc, "cameraForward");
    LOC(cameraRightLoc, "cameraRight"); LOC(cameraUpLoc, "cameraUp");
    LOC(tanHalfFovLoc, "tanHalfFov"); LOC(aspectRatioLoc, "aspectRatio");
    LOC(nearPlaneLoc, "nearPlane"); LOC(farPlaneLoc, "farPlane");
    LOC(sphereCenterLoc, "sphereCenter"); LOC(sphereRadiusLoc, "sphereRadius");
    LOC(haloRadianceLoc, "haloRadiance"); LOC(haloParamsLoc, "haloParams");
    LOC(fogParamsALoc, "fogParamsA"); LOC(fogParamsBLoc, "fogParamsB");
#undef LOC
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = sceneDepthLoc;
    if (!EnsureScreenTriangle(screenTriangle)) {
        UnloadShader(shader);
        shader = {};
        shaderFailed = true;
        return false;
    }
    material = LoadMaterialDefault();
    if (material.maps == nullptr) {
        UnloadMesh(screenTriangle);
        screenTriangle = {};
        UnloadShader(shader);
        shader = {};
        shaderFailed = true;
        return false;
    }
    material.shader = shader;
    return true;
}

bool SectorLightProxyRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& colorOnlyTarget,
        const SectorTopologyFogSettings& sourceFogSettings,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds)
{
    eligibleCount = haloCount = drawCallCount = 0;
    scissorCoverage = 0.0f;
    visibleHalos.clear();
    if (sources.empty()
            || sceneTarget.depth.id == 0 || colorOnlyTarget.id == 0 || !EnsureResources()) return false;
    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane) return false;
    const int width = sceneTarget.texture.width;
    const int height = sceneTarget.texture.height;
    const float aspect = static_cast<float>(width) / std::max(height, 1);
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float tanHalfFov = std::tan(camera.fovy * DEG2RAD * 0.5f);
    SectorAtmosphereScissorRect unionScissor{};
    for (const SectorLightAtmosphereSource& source : sources) {
        if (!IsSectorLightAtmosphereSourceSelected(source, dynamicLights)) continue;
        const SectorLightProxySettings& proxy = source.atmosphere.proxy;
        if (!proxy.halo.enabled || proxy.halo.brightness <= 0.0f
                || proxy.halo.radiusWorld <= 0.0f) continue;
        const Vector3 centerWorld = Vector3Add(
                source.positionWorld, proxy.halo.centerOffsetWorld);
        SectorLightAtmosphereVolume volume;
        volume.source = &source;
        volume.originWorld = centerWorld;
        volume.boundsCenterWorld = centerWorld;
        volume.boundsRadiusWorld = proxy.halo.radiusWorld;
        volume.extentWorld = proxy.halo.radiusWorld;
        if (!IsSectorLightAtmosphereVolumeVisible(volume, visibility, receiverBounds,
                    camera, aspect, nearPlane, farPlane)) continue;
        const Vector3 radiusVector{proxy.halo.radiusWorld,
                proxy.halo.radiusWorld, proxy.halo.radiusWorld};
        const Vector3 minimum = Vector3Subtract(centerWorld, radiusVector);
        const Vector3 maximum = Vector3Add(centerWorld, radiusVector);
        const SectorAtmosphereScissorRect scissor = ProjectSectorAtmosphereBoundsToScissor(
                camera, aspect, nearPlane, minimum, maximum, width, height);
        if (scissor.Empty()) continue;
        Vector3 lightColor = engine::SrgbColorBytesToLinearSceneRgb(source.color);
        float intensity = source.intensity;
        const int dynamicIndex = FindDynamicIndex(source, dynamicLights);
        if (dynamicIndex >= 0) {
            lightColor = dynamicLights.dynamicLightColors[static_cast<std::size_t>(dynamicIndex)];
            intensity = dynamicLights.dynamicLightIntensities[static_cast<std::size_t>(dynamicIndex)];
        }
        const Vector3 tint = engine::SrgbColorBytesToLinearSceneRgb(
                proxy.halo.scatteringTint);
        visibleHalos.push_back(VisibleHalo{
                &source,
                centerWorld,
                scissor,
                Vector3Scale(Multiply(lightColor, tint), intensity * proxy.halo.brightness),
                Vector3DistanceSqr(camera.position, centerWorld)});
        unionScissor = UnionSectorAtmosphereScissors(unionScissor, scissor, width, height);
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
    scissorCoverage = SectorAtmosphereScissorCoverage(unionScissor, width, height);
    if (visibleHalos.empty()) return false;

    const Vector2 viewport{static_cast<float>(width), static_cast<float>(height)};
    const SectorTopologyFogSettings fog = NormalizeSectorTopologyFogSettings(sourceFogSettings);
    const float fogMode = !fog.enabled ? 0.0f
            : (fog.mode == SectorTopologyFogMode::Distance ? 2.0f : 1.0f);
    const Vector4 fogA{fogMode, fog.startDistanceWorld,
            fog.mode == SectorTopologyFogMode::Distance ? fog.endDistanceWorld : fog.density,
            fog.maxOpacity};
    const Vector4 fogB{fog.falloffExponent, fog.referenceHeightWorld, fog.heightFalloff, 0.0f};
    rlDrawRenderBatchActive();
    BeginTextureMode(colorOnlyTarget);
    SetShaderValue(shader, viewportSizeLoc, &viewport, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, cameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraForwardLoc, &forward, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraRightLoc, &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, cameraUpLoc, &up, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, tanHalfFovLoc, &tanHalfFov, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, aspectRatioLoc, &aspect, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, nearPlaneLoc, &nearPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, farPlaneLoc, &farPlane, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, fogParamsALoc, &fogA, SHADER_UNIFORM_VEC4);
    SetShaderValue(shader, fogParamsBLoc, &fogB, SHADER_UNIFORM_VEC4);
    material.maps[MATERIAL_MAP_DIFFUSE].texture = sceneTarget.depth;
    BeginBlendMode(BLEND_ALPHA_PREMULTIPLY);
    rlColorMask(true, true, true, false);
    rlEnableScissorTest();
    for (const VisibleHalo& visible : visibleHalos) {
        const SectorLightProxyHaloSettings& settings = visible.source->atmosphere.proxy.halo;
        const Vector2 haloParams{settings.edgeSoftness, settings.maxExtinction};
        SetShaderValue(shader, sphereCenterLoc, &visible.centerWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, sphereRadiusLoc, &settings.radiusWorld, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, haloRadianceLoc, &visible.radiance, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, haloParamsLoc, &haloParams, SHADER_UNIFORM_VEC2);
        rlScissor(visible.scissor.x, visible.scissor.y,
                visible.scissor.width, visible.scissor.height);
        DrawMesh(screenTriangle, material, MatrixIdentity());
        ++drawCallCount;
    }
    rlDisableScissorTest();
    rlColorMask(true, true, true, true);
    EndBlendMode();
    EndTextureMode();
    material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
    return true;
}

void SectorLightProxyRenderer::Shutdown()
{
    if (material.maps != nullptr) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
        material.shader = {};
        UnloadMaterial(material);
    }
    material = {};
    if (screenTriangle.vaoId != 0) UnloadMesh(screenTriangle);
    screenTriangle = {};
    if (shader.id != 0) UnloadShader(shader);
    shader = {};
    shaderFailed = false;
    visibleHalos.clear();
    visibleHalos.shrink_to_fit();
}

} // namespace game
