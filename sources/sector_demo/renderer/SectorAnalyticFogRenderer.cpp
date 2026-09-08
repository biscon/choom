#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorAnalyticFogRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/renderer/SectorAtmosphereCulling.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>

namespace game {
namespace {

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

bool SameVector3(Vector3 left, Vector3 right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool SameColor(Color left, Color right)
{
    return left.r == right.r && left.g == right.g
            && left.b == right.b && left.a == right.a;
}

} // namespace

void SectorAnalyticFogRenderer::Reserve(std::size_t volumeCount)
{
    visibleVolumes.reserve(volumeCount);
}

bool SectorAnalyticFogRenderer::Initialize()
{
    if (shader.id != 0 && screenTriangle.vaoId != 0
            && material.maps != nullptr) return true;
    if (shaderFailed) return false;
    shader = LoadGameShader(GameShader::AnalyticFog);
    if (shader.id == 0) { shaderFailed = true; return false; }
#define LOC(field, name) field = GetShaderLocation(shader, name)
    LOC(sceneDepthLoc, "sceneDepth"); LOC(viewportSizeLoc, "viewportSize");
    LOC(cameraPositionLoc, "cameraPosition"); LOC(cameraForwardLoc, "cameraForward");
    LOC(cameraRightLoc, "cameraRight"); LOC(cameraUpLoc, "cameraUp");
    LOC(tanHalfFovLoc, "tanHalfFov"); LOC(aspectRatioLoc, "aspectRatio");
    LOC(nearPlaneLoc, "nearPlane"); LOC(farPlaneLoc, "farPlane");
    LOC(centerLoc, "fogCenter"); LOC(radiiLoc, "fogRadii");
    LOC(colorLoc, "fogColor"); LOC(fogParamsLoc, "fogParams");
    LOC(fogShapeLoc, "fogShape"); LOC(fogStyleLoc, "fogStyle");
    LOC(fogYawLoc, "fogYaw");
    LOC(fogNoiseParamsLoc, "fogNoiseParams"); LOC(fogFlowLoc, "fogFlow");
    LOC(fogEdgeParamsLoc, "fogEdgeParams");
    LOC(fogLightingLocs[0], "fogLighting0");
    LOC(fogLightingLocs[1], "fogLighting1");
    LOC(fogLightingLocs[2], "fogLighting2");
    LOC(fogLightingLocs[3], "fogLighting3");
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

void SectorAnalyticFogRenderer::ClearStaticLightingCache()
{
    for (StaticLightingCacheEntry& entry : staticLightingCache) {
        entry = StaticLightingCacheEntry{};
    }
}

void SectorAnalyticFogRenderer::RefreshStaticLightingCacheIdentity(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes)
{
    const SectorBakedObjectLightProbe* probeData = objectLightProbes.probes.empty()
            ? nullptr
            : objectLightProbes.probes.data();
    const std::size_t probeSourceHashValue =
            std::hash<std::string>{}(objectLightProbes.metadata.sourceHash);
    const std::size_t mapProbeSourceHashValue =
            std::hash<std::string>{}(map.bakedLightmap.objectProbes.sourceHash);
    if (cachedProbeData == probeData
            && cachedProbeCount == objectLightProbes.probes.size()
            && cachedProbeSourceHashValue == probeSourceHashValue
            && cachedMapProbeSourceHashValue == mapProbeSourceHashValue) {
        return;
    }
    ClearStaticLightingCache();
    cachedProbeData = probeData;
    cachedProbeCount = objectLightProbes.probes.size();
    cachedProbeSourceHashValue = probeSourceHashValue;
    cachedMapProbeSourceHashValue = mapProbeSourceHashValue;
}

const SectorLocalFogStaticLightingSamples& SectorAnalyticFogRenderer::StaticLightingForVolume(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const SectorCompiledLocalFogVolume& volume)
{
    const SectorTopologySector* sector = FindSectorTopologySector(map, volume.topologySectorId);
    const Color sectorAmbientColor = sector != nullptr ? sector->ambientColor : Color{};
    const float sectorAmbientIntensity = sector != nullptr ? sector->ambientIntensity : 0.0f;
    const float lightingYaw = volume.shape == SectorLocalFogShape::Box
            ? volume.yawRadians
            : 0.0f;
    StaticLightingCacheEntry* available = nullptr;
    for (StaticLightingCacheEntry& entry : staticLightingCache) {
        if (entry.valid && entry.sourceFogVolumeId == volume.sourceAuthoringFogVolumeId) {
            if (entry.topologySectorId == volume.topologySectorId
                    && SameVector3(entry.centerWorld, volume.centerWorld)
                    && SameVector3(entry.radiiWorld, volume.radiiWorld)
                    && entry.yawRadians == lightingYaw
                    && SameColor(entry.sectorAmbientColor, sectorAmbientColor)
                    && entry.sectorAmbientIntensity == sectorAmbientIntensity) {
                return entry.samples;
            }
            available = &entry;
            break;
        }
        if (!entry.valid && available == nullptr) available = &entry;
    }
    if (available == nullptr) {
        const std::size_t slot = static_cast<std::size_t>(
                std::max(volume.sourceAuthoringFogVolumeId, 0)) % staticLightingCache.size();
        available = &staticLightingCache[slot];
    }
    available->valid = true;
    available->sourceFogVolumeId = volume.sourceAuthoringFogVolumeId;
    available->topologySectorId = volume.topologySectorId;
    available->centerWorld = volume.centerWorld;
    available->radiiWorld = volume.radiiWorld;
    available->yawRadians = lightingYaw;
    available->sectorAmbientColor = sectorAmbientColor;
    available->sectorAmbientIntensity = sectorAmbientIntensity;
    available->samples = SampleSectorLocalFogStaticLighting(
            map, objectLightProbes, volume, lightingYaw);
    return available->samples;
}

bool SectorAnalyticFogRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& colorOnlyTarget,
        const SectorTopologyMap& map,
        const Camera3D& camera,
        float runtimeSeconds,
        const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
        const RuntimePortalVisibilityResult& visibility)
{
    eligibleCount = 0;
    activeCount = 0;
    scissorCoverage = 0.0f;
    visibleVolumes.clear();
    if (sceneTarget.texture.id == 0
            || sceneTarget.depth.id == 0 || colorOnlyTarget.id == 0) return false;
    const float nearPlane = static_cast<float>(rlGetCullDistanceNear());
    const float farPlane = static_cast<float>(rlGetCullDistanceFar());
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane || !(shader.id != 0 && screenTriangle.vaoId != 0 && material.maps != nullptr)) return false;
    const int width = sceneTarget.texture.width;
    const int height = sceneTarget.texture.height;
    const float aspect = static_cast<float>(width) / std::max(height, 1);
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
    const float tanHalfFov = std::tan(camera.fovy * DEG2RAD * 0.5f);
    SectorAtmosphereScissorRect unionScissor{};
    for (const SectorCompiledLocalFogVolume& volume : map.compiledLocalFogVolumes) {
        if (!volume.enabled
                || volume.maxOpacity <= 0.0f
                || !ShouldDrawRuntimeSectorForVisibility(
                        volume.topologySectorId, visibility)) {
            continue;
        }
        const bool roomStyle = volume.analyticStyle == SectorAnalyticFogStyle::Room;
        const float edgeExpansion = roomStyle
                ? 0.0f
                : ComputeSectorAnalyticFogCloudyEdgeExpansion(
                        volume.radiiWorld,
                        volume.edgeSoftness,
                        volume.noiseAmount);
        const Vector3 renderRadii{
                volume.radiiWorld.x + edgeExpansion,
                volume.radiiWorld.y + edgeExpansion,
                volume.radiiWorld.z + edgeExpansion};
        const Vector3 boundsExtents = volume.shape == SectorLocalFogShape::Box
                ? ComputeSectorAtmosphereYawedHalfExtents(
                        renderRadii, volume.yawRadians)
                : renderRadii;
        const Vector3 minimum = Vector3Subtract(volume.centerWorld, boundsExtents);
        const Vector3 maximum = Vector3Add(volume.centerWorld, boundsExtents);
        const SectorAtmosphereScissorRect scissor = ProjectSectorAtmosphereBoundsToScissor(
                camera, aspect, nearPlane, minimum, maximum, width, height);
        if (scissor.Empty()) continue;
        ++eligibleCount;
        visibleVolumes.push_back(VisibleVolume{
                &volume,
                scissor,
                Vector3DistanceSqr(camera.position, volume.centerWorld)});
        unionScissor = UnionSectorAtmosphereScissors(unionScissor, scissor, width, height);
    }
    std::sort(visibleVolumes.begin(), visibleVolumes.end(), [](const auto& left, const auto& right) {
        if (left.distanceSquared != right.distanceSquared) return left.distanceSquared > right.distanceSquared;
        return left.volume->sourceAuthoringFogVolumeId < right.volume->sourceAuthoringFogVolumeId;
    });
    activeCount = static_cast<int>(visibleVolumes.size());
    scissorCoverage = SectorAtmosphereScissorCoverage(unionScissor, width, height);
    if (visibleVolumes.empty()) return false;
    RefreshStaticLightingCacheIdentity(map, objectLightProbes);
    const Vector2 viewport{static_cast<float>(width), static_cast<float>(height)};
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
    material.maps[MATERIAL_MAP_DIFFUSE].texture = sceneTarget.depth;
    BeginBlendMode(BLEND_ALPHA);
    rlEnableScissorTest();
    for (const VisibleVolume& entry : visibleVolumes) {
        const SectorCompiledLocalFogVolume& volume = *entry.volume;
        const Vector3 color = engine::SrgbColorBytesToLinearSceneRgb(volume.color);
        const Vector4 params{volume.analyticStartDistanceWorld,
                volume.analyticEndDistanceWorld, volume.analyticFalloffExponent,
                volume.maxOpacity};
        const int shape = volume.shape == SectorLocalFogShape::Box ? 1 : 0;
        const int analyticStyle = volume.analyticStyle == SectorAnalyticFogStyle::Room ? 1 : 0;
        const Vector4 noiseParams{
                volume.edgeSoftness,
                volume.noiseScaleWorld,
                volume.noiseAmount,
                runtimeSeconds};
        const Vector2 flow{
                volume.flowDirectionDegrees * DEG2RAD,
                volume.flowSpeedWorld};
        const bool roomStyle = volume.analyticStyle == SectorAnalyticFogStyle::Room;
        const Vector2 edgeParams{
                ComputeSectorAnalyticFogEdgeWidth(
                        volume.radiiWorld,
                        volume.edgeSoftness,
                        roomStyle),
                roomStyle
                        ? 0.0f
                        : ComputeSectorAnalyticFogCloudyEdgeExpansion(
                                volume.radiiWorld,
                                volume.edgeSoftness,
                                volume.noiseAmount)};
        const SectorLocalFogStaticLightingSamples& lighting =
                StaticLightingForVolume(map, objectLightProbes, volume);
        SetShaderValue(shader, centerLoc, &volume.centerWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, radiiLoc, &volume.radiiWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, colorLoc, &color, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, fogParamsLoc, &params, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, fogShapeLoc, &shape, SHADER_UNIFORM_INT);
        SetShaderValue(shader, fogStyleLoc, &analyticStyle, SHADER_UNIFORM_INT);
        SetShaderValue(shader, fogYawLoc, &volume.yawRadians, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, fogNoiseParamsLoc, &noiseParams, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, fogFlowLoc, &flow, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, fogEdgeParamsLoc, &edgeParams, SHADER_UNIFORM_VEC2);
        for (std::size_t index = 0; index < lighting.corners.size(); ++index) {
            SetShaderValue(
                    shader,
                    fogLightingLocs[index],
                    &lighting.corners[index],
                    SHADER_UNIFORM_VEC3);
        }
        rlScissor(
                entry.scissor.x,
                entry.scissor.y,
                entry.scissor.width,
                entry.scissor.height);
        DrawMesh(screenTriangle, material, MatrixIdentity());
    }
    rlDisableScissorTest();
    EndBlendMode();
    EndTextureMode();
    material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
    return true;
}

void SectorAnalyticFogRenderer::Shutdown()
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
    visibleVolumes.clear();
    visibleVolumes.shrink_to_fit();
    ClearStaticLightingCache();
    cachedProbeData = nullptr;
    cachedProbeCount = 0;
    cachedProbeSourceHashValue = 0;
    cachedMapProbeSourceHashValue = 0;
}

} // namespace game
