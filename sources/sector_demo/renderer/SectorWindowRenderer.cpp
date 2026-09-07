#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorWindowRenderer.h"
#include "sector_demo/renderer/SectorReflectionProbePolicy.h"

#include "engine/assets/AssetManager.h"
#include "engine/ecs/World.h"
#include "engine/render/ColorTransfer.h"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

} // namespace

bool SectorWindowRenderer::Initialize(std::size_t capacity)
{
    Shutdown();
    Reserve(capacity);
    for (int variant = 0; variant < 3; ++variant) {
        active = {};
        active.shader = LoadGameShader(WindowShaderVariant(variant));
        if (active.shader.id == 0) {
            Shutdown();
            return false;
        }
        active.reflectionLocations = LoadSectorReflectionShaderLocations(active.shader);
        active.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(active.shader, "vertexPosition");
        active.shader.locs[SHADER_LOC_VERTEX_NORMAL] =
            GetShaderLocationAttrib(active.shader, "vertexNormal");
        active.shader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(active.shader, "mvp");
        active.shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(active.shader, "matModel");
        active.shader.locs[SHADER_LOC_MATRIX_NORMAL] =
            GetShaderLocation(active.shader, "matNormal");
        active.shader.locs[SHADER_LOC_MAP_CUBEMAP] =
            GetShaderLocation(active.shader, "environmentTexture");
        active.cameraPositionLoc = GetShaderLocation(active.shader, "cameraPosition");
        active.tintLoc = GetShaderLocation(active.shader, "glassTint");
        active.opacityLoc = GetShaderLocation(active.shader, "glassOpacity");
        active.roughnessLoc = GetShaderLocation(active.shader, "glassRoughness");
        active.surfaceHazeLoc = GetShaderLocation(active.shader, "glassSurfaceHaze");
        active.imperfectionStrengthLoc =
            GetShaderLocation(active.shader, "glassImperfectionStrength");
        active.dimensionsLoc = GetShaderLocation(active.shader, "glassDimensions");
        active.patternSeedLoc = GetShaderLocation(active.shader, "glassPatternSeed");
        active.iorLoc = GetShaderLocation(active.shader, "glassIor");
        active.thicknessLoc = GetShaderLocation(active.shader, "glassThickness");
        active.advancedTransmissionLoc = GetShaderLocation(active.shader, "advancedTransmission");
        active.flatGlassPassLoc = GetShaderLocation(active.shader, "flatGlassPass");
        active.sceneColorLoc = GetShaderLocation(active.shader, "sceneColor");
        active.sceneDepthLoc = GetShaderLocation(active.shader, "sceneDepth");
        active.shader.locs[SHADER_LOC_MAP_DIFFUSE] = active.sceneColorLoc;
        active.shader.locs[SHADER_LOC_MAP_SPECULAR] = active.sceneDepthLoc;
        active.viewportSizeLoc = GetShaderLocation(active.shader, "viewportSize");
        active.viewMatrixLoc = GetShaderLocation(active.shader, "matView");
        active.projectionMatrixLoc = GetShaderLocation(active.shader, "matProjection");
        active.hasEnvironmentLoc = GetShaderLocation(active.shader, "hasEnvironment");
        active.environmentBoxProjectionLoc =
            GetShaderLocation(active.shader, "environmentBoxProjection");
        active.environmentCapturePositionLoc =
            GetShaderLocation(active.shader, "environmentCapturePosition");
        active.environmentInfluenceCenterLoc =
            GetShaderLocation(active.shader, "environmentInfluenceCenter");
        active.environmentHalfExtentsLoc =
            GetShaderLocation(active.shader, "environmentHalfExtents");
        active.environmentYawLoc = GetShaderLocation(active.shader, "environmentYaw");
        active.environmentMaxLodLoc = GetShaderLocation(active.shader, "environmentMaxLod");
        active.environmentIntensityLoc = GetShaderLocation(active.shader, "environmentIntensity");
        active.environmentSpecularScaleLoc =
            GetShaderLocation(active.shader, "environmentSpecularScale");
        active.directionalLightEnabledLoc =
            GetShaderLocation(active.shader, "directionalLightEnabled");
        active.directionalLightDirectionLoc =
            GetShaderLocation(active.shader, "directionalLightDirection");
        active.directionalLightColorLoc = GetShaderLocation(active.shader, "directionalLightColor");
        active.directionalLightIntensityLoc =
            GetShaderLocation(active.shader, "directionalLightIntensity");
        active.fogLocations = GetSectorFogShaderLocations(active.shader);

        active.material = LoadMaterialDefault();
        active.material.shader = active.shader;
        variants[variant] = active;
    }
    materialLoaded = true;
    cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    meshLoaded = cube.vertexCount > 0;
    return meshLoaded;
}

void SectorWindowRenderer::Shutdown()
{
    drawItems.clear();
    if (meshLoaded)
        UnloadMesh(cube);
    cube = {};
    meshLoaded = false;
    for (auto &resources : variants) {
        if (resources.material.maps) {
            resources.material.maps[MATERIAL_MAP_CUBEMAP].texture = {};
            UnloadMaterial(resources.material);
        } else if (resources.shader.id)
            UnloadShader(resources.shader);
        resources = {};
    }
    active = {};
    materialLoaded = false;
    consideredCount = 0;
    drawnCount = 0;
    localEnvironmentCount = 0;
    globalEnvironmentCount = 0;
    missingEnvironmentCount = 0;
}

void SectorWindowRenderer::Reserve(std::size_t capacity)
{
    drawItems.reserve(capacity);
}

bool SectorWindowRenderer::PrepareVisibleWindows(engine::World &world, const Camera3D &camera,
                                                 float aspect,
                                                 const RuntimePortalVisibilityResult *visibility,
                                                 bool enabled)
{
    consideredCount = drawnCount = 0;
    drawItems.clear();
    world.ForEach<SectorObjectTransform, SectorObject, SectorWindow>(
        [&](engine::Entity entity, SectorObjectTransform &transform, SectorObject &object,
            SectorWindow &window) {
            ++consideredCount;
            if (!enabled)
                return;
            const BoundingBox bounds =
                TransformSectorDoorModelBounds({{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
                                               BuildSectorWindowModelMatrix(transform, window));
            if (!SectorPaneVisible(object.visible, window.visible, window.frontSectorId,
                                   window.backSectorId, visibility, camera, aspect,
                                   rlGetCullDistanceNear(), rlGetCullDistanceFar(), bounds))
                return;
            if (drawItems.size() == drawItems.capacity() && !capacityWarned) {
                TraceLog(LOG_WARNING, "RENDER: window draw capacity exceeded; frame allocation");
                capacityWarned = true;
            }
            drawItems.push_back({entity, window.placedObjectId,
                                 Vector3DistanceSqr(transform.position, camera.position)});
        });
    std::sort(drawItems.begin(), drawItems.end(), [](const DrawItem &a, const DrawItem &b) {
        if (a.distanceSquared != b.distanceSquared) {
            return a.distanceSquared > b.distanceSquared;
        }
        return a.placedObjectId < b.placedObjectId;
    });
    return !drawItems.empty();
}

void SectorWindowRenderer::Draw(const SectorWindowDrawContext &context)
{
    drawnCount = localEnvironmentCount = globalEnvironmentCount = missingEnvironmentCount = 0;
    if (!materialLoaded || !meshLoaded || context.assets == nullptr || context.world == nullptr)
        return;

    const int advancedTransmission =
        context.advancedTransmission && context.sceneColor && context.sceneDepth ? 1 : 0;
    const auto pbr = NormalizeSectorPbrContributionSettings(context.pbr);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    if (advancedTransmission != 0) {
        rlSetBlendMode(BLEND_ALPHA_PREMULTIPLY);
    }
    if (advancedTransmission != 0)
        rlDisableDepthTest();
    else
        rlEnableDepthTest();
    rlDisableDepthMask();
    rlEnableBackfaceCulling();

    for (const DrawItem &item : drawItems) {
        if (!context.world->IsAlive(item.entity) ||
            !context.world->Has<SectorObjectTransform>(item.entity) ||
            !context.world->Has<SectorObject>(item.entity) ||
            !context.world->Has<SectorWindow>(item.entity))
            continue;
        const SectorObjectTransform &transform =
            context.world->Get<SectorObjectTransform>(item.entity);
        const SectorObject &object = context.world->Get<SectorObject>(item.entity);
        const SectorWindow &window = context.world->Get<SectorWindow>(item.entity);

        const int firstPass = advancedTransmission ? 0 : 1;
        const int lastPass = advancedTransmission ? 0 : 2;
        for (int pass = firstPass; pass <= lastPass; ++pass) {
            active = variants[pass];
            if (context.profiler)
                context.profiler->Begin(pass == 1 ? SectorWorldStage::GlassTransmission
                                                  : SectorWorldStage::GlassReflection);
            if (active.cameraPositionLoc >= 0)
                SetShaderValue(active.shader, active.cameraPositionLoc, &context.camera.position,
                               SHADER_UNIFORM_VEC3);
            const Texture2D originalDiffuse = active.material.maps[MATERIAL_MAP_DIFFUSE].texture;
            const Texture2D originalSpecular = active.material.maps[MATERIAL_MAP_SPECULAR].texture;
            if (advancedTransmission != 0) {
                active.material.maps[MATERIAL_MAP_DIFFUSE].texture = *context.sceneColor;
                active.material.maps[MATERIAL_MAP_SPECULAR].texture = *context.sceneDepth;
            }
            if (active.advancedTransmissionLoc >= 0)
                SetShaderValue(active.shader, active.advancedTransmissionLoc, &advancedTransmission,
                               SHADER_UNIFORM_INT);
            if (active.viewportSizeLoc >= 0)
                SetShaderValue(active.shader, active.viewportSizeLoc, &context.viewportSize,
                               SHADER_UNIFORM_VEC2);
            const Matrix viewMatrix = GetCameraMatrix(context.camera);
            const Matrix projectionMatrix = rlGetMatrixProjection();
            if (active.viewMatrixLoc >= 0)
                SetShaderValueMatrix(active.shader, active.viewMatrixLoc, viewMatrix);
            if (active.projectionMatrixLoc >= 0)
                SetShaderValueMatrix(active.shader, active.projectionMatrixLoc, projectionMatrix);
            if (active.environmentSpecularScaleLoc >= 0)
                SetShaderValue(active.shader, active.environmentSpecularScaleLoc,
                               &pbr.worldEnvironmentSpecularScale, SHADER_UNIFORM_FLOAT);
            const SectorTopologyDirectionalLightSettings directionalLight =
                NormalizeSectorTopologyDirectionalLightSettings(context.directionalLight);
            const int directionalLightEnabled = directionalLight.enabled ? 1 : 0;
            const Vector3 directionalLightColor =
                engine::SrgbColorBytesToLinearSceneRgb(directionalLight.color);
            if (active.directionalLightEnabledLoc >= 0)
                SetShaderValue(active.shader, active.directionalLightEnabledLoc,
                               &directionalLightEnabled, SHADER_UNIFORM_INT);
            if (active.directionalLightDirectionLoc >= 0)
                SetShaderValue(active.shader, active.directionalLightDirectionLoc,
                               &directionalLight.directionToLight, SHADER_UNIFORM_VEC3);
            if (active.directionalLightColorLoc >= 0)
                SetShaderValue(active.shader, active.directionalLightColorLoc,
                               &directionalLightColor, SHADER_UNIFORM_VEC3);
            if (active.directionalLightIntensityLoc >= 0)
                SetShaderValue(active.shader, active.directionalLightIntensityLoc,
                               &directionalLight.intensity, SHADER_UNIFORM_FLOAT);
            UploadSectorFogShaderValues(active.shader, active.fogLocations, context.fog);

            const Vector3 tint = engine::SrgbColorBytesToLinearSceneRgb(window.tint);
            if (active.tintLoc >= 0)
                SetShaderValue(active.shader, active.tintLoc, &tint, SHADER_UNIFORM_VEC3);
            if (active.opacityLoc >= 0)
                SetShaderValue(active.shader, active.opacityLoc, &window.opacity,
                               SHADER_UNIFORM_FLOAT);
            if (active.roughnessLoc >= 0)
                SetShaderValue(active.shader, active.roughnessLoc, &window.roughness,
                               SHADER_UNIFORM_FLOAT);
            if (active.surfaceHazeLoc >= 0)
                SetShaderValue(active.shader, active.surfaceHazeLoc, &window.surfaceHaze,
                               SHADER_UNIFORM_FLOAT);
            if (active.imperfectionStrengthLoc >= 0)
                SetShaderValue(active.shader, active.imperfectionStrengthLoc,
                               &window.imperfectionStrength, SHADER_UNIFORM_FLOAT);
            const Vector3 glassDimensions{window.width, window.height, window.thickness};
            if (active.dimensionsLoc >= 0)
                SetShaderValue(active.shader, active.dimensionsLoc, &glassDimensions,
                               SHADER_UNIFORM_VEC3);
            const float patternSeed = static_cast<float>(window.placedObjectId);
            if (active.patternSeedLoc >= 0)
                SetShaderValue(active.shader, active.patternSeedLoc, &patternSeed,
                               SHADER_UNIFORM_FLOAT);
            if (active.iorLoc >= 0)
                SetShaderValue(active.shader, active.iorLoc, &window.indexOfRefraction,
                               SHADER_UNIFORM_FLOAT);
            if (active.thicknessLoc >= 0)
                SetShaderValue(active.shader, active.thicknessLoc, &window.thickness,
                               SHADER_UNIFORM_FLOAT);

            if (pass != 1) {
                SectorPbrEnvironmentBlend reflectionBlend;
                if (context.environment) {
                    const Vector3 portalNormal{window.normal.x, 0.0f, window.normal.y};
                    const bool back = Vector3DotProduct(Vector3Subtract(context.camera.position,
                                                                        transform.position),
                                                        portalNormal) > 0;
                    const Vector3 receiver = Vector3Add(
                        transform.position, Vector3Scale(portalNormal, back ? 0.25f : -0.25f));
                    const BoundingBox reflectionBounds = TransformSectorDoorModelBounds(
                        {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}},
                        BuildSectorWindowModelMatrix(transform, window));
                    reflectionBlend = SelectSectorPbrEnvironmentBlend(
                        *context.environment, receiver,
                        back ? window.backSectorId : window.frontSectorId, true, nullptr,
                        SectorReflectionDemandForBounds(context.environment->demandCollector,
                                                        reflectionBounds));
                }
                UploadSectorReflectionBlend(active.shader, active.reflectionLocations,
                                            reflectionBlend, *context.assets);
                const auto &selection = reflectionBlend.first;
                const TextureCubemap *cubemap = context.assets->GetCubemap(selection.cubemap);
                const int hasEnvironment = cubemap != nullptr && cubemap->id != 0 &&
                                                   pbr.worldEnvironmentSpecularScale > 0.0f
                                               ? 1
                                               : 0;
                active.material.maps[MATERIAL_MAP_CUBEMAP].texture =
                    hasEnvironment != 0 ? *cubemap : Texture2D{};
                if (active.hasEnvironmentLoc >= 0)
                    SetShaderValue(active.shader, active.hasEnvironmentLoc, &hasEnvironment,
                                   SHADER_UNIFORM_INT);
                const int boxProjection = selection.boxProjection ? 1 : 0;
                if (active.environmentBoxProjectionLoc >= 0)
                    SetShaderValue(active.shader, active.environmentBoxProjectionLoc,
                                   &boxProjection, SHADER_UNIFORM_INT);
                if (active.environmentCapturePositionLoc >= 0)
                    SetShaderValue(active.shader, active.environmentCapturePositionLoc,
                                   &selection.capturePosition, SHADER_UNIFORM_VEC3);
                if (active.environmentInfluenceCenterLoc >= 0)
                    SetShaderValue(active.shader, active.environmentInfluenceCenterLoc,
                                   &selection.influenceCenter, SHADER_UNIFORM_VEC3);
                if (active.environmentHalfExtentsLoc >= 0)
                    SetShaderValue(active.shader, active.environmentHalfExtentsLoc,
                                   &selection.halfExtents, SHADER_UNIFORM_VEC3);
                if (active.environmentYawLoc >= 0)
                    SetShaderValue(active.shader, active.environmentYawLoc, &selection.yawRadians,
                                   SHADER_UNIFORM_FLOAT);
                if (active.environmentMaxLodLoc >= 0)
                    SetShaderValue(active.shader, active.environmentMaxLodLoc, &selection.maxLod,
                                   SHADER_UNIFORM_FLOAT);
                const float environmentIntensity =
                    selection.localProbe ? selection.intensity : 0.15f;
                if (active.environmentIntensityLoc >= 0)
                    SetShaderValue(active.shader, active.environmentIntensityLoc,
                                   &environmentIntensity, SHADER_UNIFORM_FLOAT);
                if (hasEnvironment == 0)
                    ++missingEnvironmentCount;
                else if (selection.localProbe)
                    ++localEnvironmentCount;
                else
                    ++globalEnvironmentCount;
            }

            const Matrix modelMatrix = BuildSectorWindowModelMatrix(transform, window);
            if (pass == 1) {
                rlSetBlendFactorsSeparate(RL_ZERO, RL_SRC_COLOR, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                                          RL_FUNC_ADD);
                rlSetBlendMode(BLEND_CUSTOM_SEPARATE);
            } else if (pass == 2) {
                rlSetBlendFactorsSeparate(RL_ONE, RL_ONE, RL_ZERO, RL_ONE, RL_FUNC_ADD,
                                          RL_FUNC_ADD);
                rlSetBlendMode(BLEND_CUSTOM_SEPARATE);
            }
            DrawMesh(cube, active.material, modelMatrix);
            if (context.profiler)
                context.profiler->End();
            active.material.maps[MATERIAL_MAP_CUBEMAP].texture = {};
            active.material.maps[MATERIAL_MAP_DIFFUSE].texture = originalDiffuse;
            active.material.maps[MATERIAL_MAP_SPECULAR].texture = originalSpecular;
        }
        ++drawnCount;
    }

    rlDrawRenderBatchActive();
    active.material.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableDepthMask();
    rlEnableDepthTest();
    rlEnableBackfaceCulling();
    if (context.renderDebugText != nullptr) {
        *context.renderDebugText +=
            " | windows: " + std::to_string(drawnCount) + " drawn / " +
            std::to_string(consideredCount) + " considered; env local/global/none " +
            std::to_string(localEnvironmentCount) + "/" + std::to_string(globalEnvironmentCount) +
            "/" + std::to_string(missingEnvironmentCount) +
            (advancedTransmission != 0 ? "; advanced" : "; flat two-pass");
    }
}

} // namespace game
