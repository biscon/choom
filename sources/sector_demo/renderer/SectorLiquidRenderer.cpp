#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorLiquidRenderer.h"
#include "sector_demo/renderer/SectorReflectionProbePolicy.h"

#include "engine/assets/AssetManager.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

bool VisibleSector(int sectorId, const RuntimePortalVisibilityResult* visibility)
{
    return visibility == nullptr
            || !visibility->validStartSector
            || visibility->fallbackDrawAll
            || ShouldDrawRuntimeSectorForVisibility(sectorId, *visibility);
}

template<typename SurfaceT>
void UnloadSurfaces(std::vector<SurfaceT>& surfaces)
{
    for (SurfaceT& surface : surfaces) {
        if (surface.mesh.vertexCount > 0) UnloadMesh(surface.mesh);
        surface.mesh = {};
    }
    surfaces.clear();
}

} // namespace

bool SectorLiquidRenderer::Initialize(std::size_t capacity)
{
    Shutdown();
    Reserve(capacity);
    shader = LoadGameShader(GameShader::Liquid);
    if (shader.id == 0) return false;
    reflectionLocations=LoadSectorReflectionShaderLocations(shader);
    shader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(shader, "vertexPosition");
    shader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocationAttrib(shader, "vertexNormal");
    shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(shader, "mvp");
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shader, "matModel");
    shader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(shader, "matNormal");
    shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shader, "sceneColor");
    shader.locs[SHADER_LOC_MAP_SPECULAR] = GetShaderLocation(shader, "sceneDepth");
    shader.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(shader, "environmentTexture");
    cameraPositionLoc = GetShaderLocation(shader, "cameraPosition");
    runtimeSecondsLoc = GetShaderLocation(shader, "runtimeSeconds");
    shallowColorLoc = GetShaderLocation(shader, "shallowColor");
    deepColorLoc = GetShaderLocation(shader, "deepColor");
    liquidParams0Loc = GetShaderLocation(shader, "liquidParams0");
    liquidParams1Loc = GetShaderLocation(shader, "liquidParams1");
    flowParamsLoc = GetShaderLocation(shader, "flowParams");
    advancedTransmissionLoc = GetShaderLocation(shader, "advancedTransmission");
    viewportSizeLoc = GetShaderLocation(shader, "viewportSize");
    inverseViewMatrixLoc = GetShaderLocation(shader, "matInverseView");
    inverseProjectionMatrixLoc = GetShaderLocation(shader, "matInverseProjection");
    hasEnvironmentLoc = GetShaderLocation(shader, "hasEnvironment");
    environmentBoxProjectionLoc = GetShaderLocation(shader, "environmentBoxProjection");
    environmentCapturePositionLoc = GetShaderLocation(shader, "environmentCapturePosition");
    environmentInfluenceCenterLoc = GetShaderLocation(shader, "environmentInfluenceCenter");
    environmentHalfExtentsLoc = GetShaderLocation(shader, "environmentHalfExtents");
    environmentYawLoc = GetShaderLocation(shader, "environmentYaw");
    environmentMaxLodLoc = GetShaderLocation(shader, "environmentMaxLod");
    environmentIntensityLoc = GetShaderLocation(shader, "environmentIntensity");
    environmentSpecularScaleLoc = GetShaderLocation(shader, "environmentSpecularScale");
    directionalLightEnabledLoc = GetShaderLocation(shader, "directionalLightEnabled");
    directionalLightDirectionLoc = GetShaderLocation(shader, "directionalLightDirection");
    directionalLightColorLoc = GetShaderLocation(shader, "directionalLightColor");
    directionalLightIntensityLoc = GetShaderLocation(shader, "directionalLightIntensity");
    fogLocations = GetSectorFogShaderLocations(shader);
    material = LoadMaterialDefault();
    material.shader = shader;
    materialLoaded = true;
    return true;
}

void SectorLiquidRenderer::Reserve(std::size_t capacity)
{
    surfaces.reserve(capacity);
    drawItems.reserve(capacity);
}

bool SectorLiquidRenderer::Rebuild(
        const SectorTopologyMap& map,
        const SectorGeneratedGeometry& geometry,
        std::string& error)
{
    error.clear();
    if (!materialLoaded) {
        error = "Liquid renderer is not initialized";
        return false;
    }
    std::vector<Surface> candidates;
    candidates.reserve(map.sectors.size());
    for (const SectorTopologySector& sector : map.sectors) {
        const SectorLiquidSettings settings = NormalizeSectorLiquidSettingsForSpan(
                sector.liquid, sector.floorZ, sector.ceilingZ);
        const float surfaceHeight = ResolveSectorLiquidSurfaceHeight(
                settings, sector.floorZ, sector.ceilingZ);
        if (!settings.enabled || surfaceHeight - sector.floorZ <= 0.0001f) continue;
        const float surfaceY = SectorAuthoringToWorldDistance(surfaceHeight);
        for (const SectorGeneratedSurface& generated : geometry.surfaces) {
            if (generated.ref.sourceKind != SectorGeneratedSurfaceSourceKind::Topology
                    || generated.ref.kind != SectorGeneratedSurfaceKind::Floor
                    || generated.ref.topologySectorId != sector.id
                    || generated.vertices.size() < 3) continue;
            Surface candidate;
            candidate.sectorId = sector.id;
            candidate.settings = settings;
            candidate.surfaceY = surfaceY;
            candidate.mesh.vertexCount = static_cast<int>(generated.vertices.size());
            candidate.mesh.triangleCount = candidate.mesh.vertexCount / 3;
            candidate.mesh.vertices = static_cast<float*>(MemAlloc(
                    generated.vertices.size() * 3 * sizeof(float)));
            candidate.mesh.normals = static_cast<float*>(MemAlloc(
                    generated.vertices.size() * 3 * sizeof(float)));
            if (candidate.mesh.vertices == nullptr || candidate.mesh.normals == nullptr) {
                if (candidate.mesh.vertices != nullptr) MemFree(candidate.mesh.vertices);
                if (candidate.mesh.normals != nullptr) MemFree(candidate.mesh.normals);
                UnloadSurfaces(candidates);
                error = "Could not allocate liquid surface mesh";
                return false;
            }
            Vector3 center{};
            for (std::size_t i = 0; i < generated.vertices.size(); ++i) {
                const Vector3 position{
                        generated.vertices[i].position.x,
                        surfaceY,
                        generated.vertices[i].position.z};
                if (i == 0) candidate.bounds = {position, position};
                else {
                    candidate.bounds.min = Vector3Min(candidate.bounds.min, position);
                    candidate.bounds.max = Vector3Max(candidate.bounds.max, position);
                }
                candidate.mesh.vertices[i * 3 + 0] = position.x;
                candidate.mesh.vertices[i * 3 + 1] = position.y;
                candidate.mesh.vertices[i * 3 + 2] = position.z;
                candidate.mesh.normals[i * 3 + 0] = 0.0f;
                candidate.mesh.normals[i * 3 + 1] = 1.0f;
                candidate.mesh.normals[i * 3 + 2] = 0.0f;
                center = Vector3Add(center, position);
            }
            candidate.center = Vector3Scale(
                    center, 1.0f / static_cast<float>(generated.vertices.size()));
            UploadMesh(&candidate.mesh, false);
            candidates.push_back(std::move(candidate));
        }
    }
    UnloadSurfaces(surfaces);
    surfaces = std::move(candidates);
    drawItems.reserve(std::max(drawItems.capacity(), surfaces.size()));
    return true;
}

void SectorLiquidRenderer::Shutdown()
{
    UnloadSurfaces(surfaces);
    drawItems.clear();
    if (materialLoaded) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = {};
        material.maps[MATERIAL_MAP_SPECULAR].texture = {};
        material.maps[MATERIAL_MAP_CUBEMAP].texture = {};
        UnloadMaterial(material);
    } else if (shader.id != 0) {
        UnloadShader(shader);
    }
    material = {};
    shader = {};
    materialLoaded = false;
    drawnCount = 0;
}

bool SectorLiquidRenderer::HasVisibleLiquids(
        const RuntimePortalVisibilityResult* visibility,
        Vector3 cameraPosition) const
{
    for (const Surface& surface : surfaces) {
        if (VisibleSector(surface.sectorId, visibility)) return true;
    }
    return false;
}

void SectorLiquidRenderer::Draw(const SectorLiquidDrawContext& context)
{
    drawnCount = 0;
    drawItems.clear();
    if (!materialLoaded || shader.id == 0 || context.assets == nullptr) return;
    for (std::size_t i = 0; i < surfaces.size(); ++i) {
        const Surface& surface = surfaces[i];
        if (!VisibleSector(surface.sectorId, context.visibility)) continue;
        drawItems.push_back(DrawItem{i, Vector3DistanceSqr(
                context.camera.position, surface.center)});
    }
    std::sort(drawItems.begin(), drawItems.end(), [](const DrawItem& a, const DrawItem& b) {
        if (a.distanceSquared != b.distanceSquared) return a.distanceSquared > b.distanceSquared;
        return a.surfaceIndex < b.surfaceIndex;
    });
    if (drawItems.empty()) return;

    if (cameraPositionLoc >= 0) SetShaderValue(shader, cameraPositionLoc,
            &context.camera.position, SHADER_UNIFORM_VEC3);
    if (runtimeSecondsLoc >= 0) SetShaderValue(shader, runtimeSecondsLoc,
            &context.runtimeSeconds, SHADER_UNIFORM_FLOAT);
    const int advanced = context.advancedTransmission
                    && context.sceneColor != nullptr && context.sceneDepth != nullptr
            ? 1 : 0;
    if (advancedTransmissionLoc >= 0) SetShaderValue(shader, advancedTransmissionLoc,
            &advanced, SHADER_UNIFORM_INT);
    if (viewportSizeLoc >= 0) SetShaderValue(shader, viewportSizeLoc,
            &context.viewportSize, SHADER_UNIFORM_VEC2);
    const Matrix inverseView = MatrixInvert(GetCameraMatrix(context.camera));
    const Matrix inverseProjection = MatrixInvert(rlGetMatrixProjection());
    if (inverseViewMatrixLoc >= 0) SetShaderValueMatrix(shader, inverseViewMatrixLoc, inverseView);
    if (inverseProjectionMatrixLoc >= 0) SetShaderValueMatrix(shader, inverseProjectionMatrixLoc, inverseProjection);
    const SectorPbrContributionSettings pbr = NormalizeSectorPbrContributionSettings(context.pbr);
    if (environmentSpecularScaleLoc >= 0) SetShaderValue(shader,
            environmentSpecularScaleLoc, &pbr.worldEnvironmentSpecularScale,
            SHADER_UNIFORM_FLOAT);
    const SectorTopologyDirectionalLightSettings directional =
            NormalizeSectorTopologyDirectionalLightSettings(context.directionalLight);
    const int directionalEnabled = directional.enabled ? 1 : 0;
    const Vector3 directionalColor = engine::SrgbColorBytesToLinearSceneRgb(directional.color);
    if (directionalLightEnabledLoc >= 0) SetShaderValue(shader, directionalLightEnabledLoc,
            &directionalEnabled, SHADER_UNIFORM_INT);
    if (directionalLightDirectionLoc >= 0) SetShaderValue(shader, directionalLightDirectionLoc,
            &directional.directionToLight, SHADER_UNIFORM_VEC3);
    if (directionalLightColorLoc >= 0) SetShaderValue(shader, directionalLightColorLoc,
            &directionalColor, SHADER_UNIFORM_VEC3);
    if (directionalLightIntensityLoc >= 0) SetShaderValue(shader, directionalLightIntensityLoc,
            &directional.intensity, SHADER_UNIFORM_FLOAT);
    UploadSectorFogShaderValues(shader, fogLocations, context.fog);

    const Texture2D originalDiffuse = material.maps[MATERIAL_MAP_DIFFUSE].texture;
    const Texture2D originalSpecular = material.maps[MATERIAL_MAP_SPECULAR].texture;
    if (advanced != 0) {
        material.maps[MATERIAL_MAP_DIFFUSE].texture = *context.sceneColor;
        material.maps[MATERIAL_MAP_SPECULAR].texture = *context.sceneDepth;
    }

    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY);
    if (advanced != 0) rlDisableDepthTest();
    else rlEnableDepthTest();
    rlDisableDepthMask();
    rlDisableBackfaceCulling();

    for (const DrawItem& item : drawItems) {
        Surface& surface = surfaces[item.surfaceIndex];
        const SectorLiquidSettings& liquid = surface.settings;
        const Vector3 shallow = engine::SrgbColorBytesToLinearSceneRgb(liquid.shallowColor);
        const Vector3 deep = engine::SrgbColorBytesToLinearSceneRgb(liquid.deepColor);
        const Vector4 params0{liquid.visibilityDepthWorld, liquid.roughness,
                liquid.refractionStrength, 0.0f};
        const Vector4 params1{liquid.rippleScaleWorld, liquid.rippleStrength,
                liquid.rippleSpeed, 0.0f};
        const Vector2 flow{liquid.flowDirectionDegrees * DEG2RAD,
                liquid.flowSpeedWorld};
        if (shallowColorLoc >= 0) SetShaderValue(shader, shallowColorLoc, &shallow, SHADER_UNIFORM_VEC3);
        if (deepColorLoc >= 0) SetShaderValue(shader, deepColorLoc, &deep, SHADER_UNIFORM_VEC3);
        if (liquidParams0Loc >= 0) SetShaderValue(shader, liquidParams0Loc, &params0, SHADER_UNIFORM_VEC4);
        if (liquidParams1Loc >= 0) SetShaderValue(shader, liquidParams1Loc, &params1, SHADER_UNIFORM_VEC4);
        if (flowParamsLoc >= 0) SetShaderValue(shader, flowParamsLoc, &flow, SHADER_UNIFORM_VEC2);

        const auto reflectionBlend=context.environment
                ? SelectSectorPbrEnvironmentBlend(*context.environment,surface.center,surface.sectorId,
                        true, nullptr, SectorReflectionDemandForBounds(
                                context.environment->demandCollector, surface.bounds))
                : SectorPbrEnvironmentBlend{};
        UploadSectorReflectionBlend(shader,reflectionLocations,reflectionBlend,*context.assets);
        const auto& selection = reflectionBlend.first;
        const TextureCubemap* cubemap = context.assets->GetCubemap(selection.cubemap);
        const int hasEnvironment = cubemap != nullptr && cubemap->id != 0
                        && pbr.worldEnvironmentSpecularScale > 0.0f
                ? 1 : 0;
        material.maps[MATERIAL_MAP_CUBEMAP].texture = hasEnvironment != 0
                ? *cubemap : Texture2D{};
        const int boxProjection = selection.boxProjection ? 1 : 0;
        const float intensity = selection.localProbe ? selection.intensity : 0.15f;
        if (hasEnvironmentLoc >= 0) SetShaderValue(shader, hasEnvironmentLoc, &hasEnvironment, SHADER_UNIFORM_INT);
        if (environmentBoxProjectionLoc >= 0) SetShaderValue(shader, environmentBoxProjectionLoc, &boxProjection, SHADER_UNIFORM_INT);
        if (environmentCapturePositionLoc >= 0) SetShaderValue(shader, environmentCapturePositionLoc, &selection.capturePosition, SHADER_UNIFORM_VEC3);
        if (environmentInfluenceCenterLoc >= 0) SetShaderValue(shader, environmentInfluenceCenterLoc, &selection.influenceCenter, SHADER_UNIFORM_VEC3);
        if (environmentHalfExtentsLoc >= 0) SetShaderValue(shader, environmentHalfExtentsLoc, &selection.halfExtents, SHADER_UNIFORM_VEC3);
        if (environmentYawLoc >= 0) SetShaderValue(shader, environmentYawLoc, &selection.yawRadians, SHADER_UNIFORM_FLOAT);
        if (environmentMaxLodLoc >= 0) SetShaderValue(shader, environmentMaxLodLoc, &selection.maxLod, SHADER_UNIFORM_FLOAT);
        if (environmentIntensityLoc >= 0) SetShaderValue(shader, environmentIntensityLoc, &intensity, SHADER_UNIFORM_FLOAT);
        DrawMesh(surface.mesh, material, MatrixIdentity());
        ++drawnCount;
    }

    rlDrawRenderBatchActive();
    material.maps[MATERIAL_MAP_CUBEMAP].texture = {};
    material.maps[MATERIAL_MAP_DIFFUSE].texture = originalDiffuse;
    material.maps[MATERIAL_MAP_SPECULAR].texture = originalSpecular;
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    if (context.renderDebugText != nullptr) {
        *context.renderDebugText += " | liquids: " + std::to_string(drawnCount)
                + " drawn / " + std::to_string(surfaces.size())
                + (advanced != 0 ? "; refractive" : "; fallback");
    }
}

} // namespace game
