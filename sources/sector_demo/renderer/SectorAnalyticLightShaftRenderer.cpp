#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorAnalyticLightShaftRenderer.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorMeshTypes.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <cstring>

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

void ConeBounds(
        Vector3 apex,
        Vector3 axis,
        float length,
        float radius,
        Vector3& minimum,
        Vector3& maximum)
{
    const Vector3 base = Vector3Add(apex, Vector3Scale(axis, length));
    const Vector3 diskExtent{
            radius * std::sqrt(std::max(1.0f - axis.x * axis.x, 0.0f)),
            radius * std::sqrt(std::max(1.0f - axis.y * axis.y, 0.0f)),
            radius * std::sqrt(std::max(1.0f - axis.z * axis.z, 0.0f))};
    minimum = Vector3{
            std::min(apex.x, base.x - diskExtent.x),
            std::min(apex.y, base.y - diskExtent.y),
            std::min(apex.z, base.z - diskExtent.z)};
    maximum = Vector3{
            std::max(apex.x, base.x + diskExtent.x),
            std::max(apex.y, base.y + diskExtent.y),
            std::max(apex.z, base.z + diskExtent.z)};
}

Vector2 RectShaftFarHalfSize(
        const SectorLightAtmosphereVolume& volume,
        float spreadScale)
{
    constexpr float SpreadDegreesAtScaleOne = 15.0f;
    const float spreadHalfAngleDegrees = std::clamp(
            SpreadDegreesAtScaleOne * spreadScale,
            SpreadDegreesAtScaleOne * 0.01f,
            SpreadDegreesAtScaleOne * 2.0f);
    const float expansion = volume.extentWorld
            * std::tan(spreadHalfAngleDegrees * DEG2RAD);
    return Vector2{
            volume.halfWidthWorld + expansion,
            volume.halfHeightWorld + expansion};
}

} // namespace

void SectorAnalyticLightShaftRenderer::Reserve(std::size_t sourceCount)
{
    visibleShafts.reserve(sourceCount);
}

bool SectorAnalyticLightShaftRenderer::Initialize()
{
    if (shader.id != 0 && screenTriangle.vaoId != 0
            && material.maps != nullptr) return true;
    if (shaderFailed) return false;
    shader = LoadGameShader(GameShader::AnalyticShaft);
    if (shader.id == 0) { shaderFailed = true; return false; }
#define LOC(field, name) field = GetShaderLocation(shader, name)
    LOC(sceneDepthLoc, "sceneDepth"); LOC(viewportSizeLoc, "viewportSize");
    LOC(cameraPositionLoc, "cameraPosition"); LOC(cameraForwardLoc, "cameraForward");
    LOC(cameraRightLoc, "cameraRight"); LOC(cameraUpLoc, "cameraUp");
    LOC(tanHalfFovLoc, "tanHalfFov"); LOC(aspectRatioLoc, "aspectRatio");
    LOC(nearPlaneLoc, "nearPlane"); LOC(farPlaneLoc, "farPlane");
    LOC(coneApexLoc, "coneApex"); LOC(coneDirectionLoc, "coneDirection");
    LOC(coneLengthLoc, "coneLength"); LOC(coneBaseRadiusLoc, "coneBaseRadius");
    LOC(shaftShapeLoc, "shaftShape"); LOC(rectRightLoc, "rectRight");
    LOC(rectUpLoc, "rectUp"); LOC(rectNearHalfSizeLoc, "rectNearHalfSize");
    LOC(rectFarHalfSizeLoc, "rectFarHalfSize");
    LOC(shaftRadianceLoc, "shaftRadiance"); LOC(shaftParamsLoc, "shaftParams");
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

bool SectorAnalyticLightShaftRenderer::Apply(
        RenderTexture2D& sceneTarget,
        RenderTexture2D& colorOnlyTarget,
        const SectorTopologyFogSettings& sourceFogSettings,
        const Camera3D& camera,
        const SectorBillboardDynamicLightContext& dynamicLights,
        const std::vector<SectorLightAtmosphereSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds)
{
    eligibleCount = activeCount = drawCallCount = 0;
    scissorCoverage = 0.0f;
    visibleShafts.clear();
    if (sources.empty()
            || sceneTarget.depth.id == 0 || colorOnlyTarget.id == 0 || !(shader.id != 0 && screenTriangle.vaoId != 0 && material.maps != nullptr)) return false;
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
        const SectorLightProxyShaftSettings& settings = source.atmosphere.proxy.shaft;
        if ((source.shape != SectorLightAtmosphereShape::Cone
                    && source.shape != SectorLightAtmosphereShape::RectPrism) || !settings.enabled
                || settings.brightness <= 0.0f
                || !IsSectorLightAtmosphereSourceSelected(source, dynamicLights)) continue;
        SectorLightAtmosphereVolume volume;
        if (!MakeSectorLightAtmosphereVolume(
                    source,
                    settings.lengthScale,
                    settings.originOffsetWorld,
                    volume)) continue;
        // The shared volume helper has a general 0.05 lower scale bound;
        // shafts deliberately support the authored 0.01 minimum.
        const float authoredExtent = source.rangeWorld * settings.lengthScale;
        if (!std::isfinite(authoredExtent) || authoredExtent <= 0.0f) continue;
        if (source.shape == SectorLightAtmosphereShape::Cone) {
            volume.coneRadiusWorld *= authoredExtent / volume.extentWorld;
        }
        volume.extentWorld = authoredExtent;
        if (source.shape == SectorLightAtmosphereShape::Cone) {
            volume.coneRadiusWorld *= settings.widthScale;
        }
        const Vector2 rectFarHalfSize = source.shape == SectorLightAtmosphereShape::RectPrism
                ? RectShaftFarHalfSize(volume, settings.widthScale)
                : Vector2{};
        volume.boundsCenterWorld = Vector3Add(
                volume.originWorld,
                Vector3Scale(volume.directionWorld, volume.extentWorld * 0.5f));
        volume.boundsRadiusWorld = source.shape == SectorLightAtmosphereShape::Cone
                ? std::sqrt(volume.extentWorld * volume.extentWorld * 0.25f
                        + volume.coneRadiusWorld * volume.coneRadiusWorld)
                : std::sqrt(volume.extentWorld * volume.extentWorld * 0.25f
                        + rectFarHalfSize.x * rectFarHalfSize.x
                        + rectFarHalfSize.y * rectFarHalfSize.y);
        if (!IsSectorLightAtmosphereVolumeVisible(volume, visibility, receiverBounds,
                        camera, aspect, nearPlane, farPlane)) continue;
        const float baseRadius = volume.coneRadiusWorld;
        Vector3 minimum;
        Vector3 maximum;
        if (source.shape == SectorLightAtmosphereShape::Cone) {
            ConeBounds(volume.originWorld, volume.directionWorld, volume.extentWorld,
                    baseRadius, minimum, maximum);
        } else {
            const Vector3 center = volume.boundsCenterWorld;
            const Vector3 extent{
                    std::fabs(volume.directionWorld.x) * volume.extentWorld * 0.5f
                            + std::fabs(volume.rightWorld.x) * rectFarHalfSize.x
                            + std::fabs(volume.upWorld.x) * rectFarHalfSize.y,
                    std::fabs(volume.directionWorld.y) * volume.extentWorld * 0.5f
                            + std::fabs(volume.rightWorld.y) * rectFarHalfSize.x
                            + std::fabs(volume.upWorld.y) * rectFarHalfSize.y,
                    std::fabs(volume.directionWorld.z) * volume.extentWorld * 0.5f
                            + std::fabs(volume.rightWorld.z) * rectFarHalfSize.x
                            + std::fabs(volume.upWorld.z) * rectFarHalfSize.y};
            minimum = Vector3Subtract(center, extent);
            maximum = Vector3Add(center, extent);
        }
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
                settings.scatteringTint);
        visibleShafts.push_back(VisibleShaft{
                &source,
                volume,
                rectFarHalfSize,
                scissor,
                Vector3Scale(Multiply(lightColor, tint), intensity * settings.brightness),
                Vector3DistanceSqr(camera.position, volume.boundsCenterWorld)});
        unionScissor = UnionSectorAtmosphereScissors(unionScissor, scissor, width, height);
    }
    std::sort(visibleShafts.begin(), visibleShafts.end(), [](const auto& left, const auto& right) {
        if (left.distanceSquared != right.distanceSquared) {
            return left.distanceSquared > right.distanceSquared;
        }
        if (left.source->lightId != right.source->lightId) {
            return left.source->lightId < right.source->lightId;
        }
        return static_cast<int>(left.source->kind) < static_cast<int>(right.source->kind);
    });
    eligibleCount = activeCount = static_cast<int>(visibleShafts.size());
    scissorCoverage = SectorAtmosphereScissorCoverage(unionScissor, width, height);
    if (visibleShafts.empty()) return false;

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
    for (const VisibleShaft& visible : visibleShafts) {
        const SectorLightProxyShaftSettings& settings = visible.source->atmosphere.proxy.shaft;
        const float baseRadius = visible.volume.coneRadiusWorld;
        const Vector2 shaftParams{settings.edgeSoftness, settings.maxExtinction};
        SetShaderValue(shader, coneApexLoc, &visible.volume.originWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, coneDirectionLoc, &visible.volume.directionWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, coneLengthLoc, &visible.volume.extentWorld, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, coneBaseRadiusLoc, &baseRadius, SHADER_UNIFORM_FLOAT);
        const int shaftShape = visible.source->shape == SectorLightAtmosphereShape::RectPrism ? 1 : 0;
        const Vector2 rectNearHalfSize{
                visible.volume.halfWidthWorld,
                visible.volume.halfHeightWorld};
        SetShaderValue(shader, shaftShapeLoc, &shaftShape, SHADER_UNIFORM_INT);
        SetShaderValue(shader, rectRightLoc, &visible.volume.rightWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, rectUpLoc, &visible.volume.upWorld, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, rectNearHalfSizeLoc, &rectNearHalfSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, rectFarHalfSizeLoc, &visible.rectFarHalfSize, SHADER_UNIFORM_VEC2);
        SetShaderValue(shader, shaftRadianceLoc, &visible.radiance, SHADER_UNIFORM_VEC3);
        SetShaderValue(shader, shaftParamsLoc, &shaftParams, SHADER_UNIFORM_VEC2);
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

void SectorAnalyticLightShaftRenderer::Shutdown()
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
    visibleShafts.clear();
    visibleShafts.shrink_to_fit();
}

} // namespace game
