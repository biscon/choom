#include "game/LoadShader.h"
#include "sector_demo/renderer/SectorDoorRenderer.h"
#include "sector_demo/renderer/SectorReflectionProbePolicy.h"


#include "engine/assets/AssetManager.h"
#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorRuntimeObjects.h"

#include <raylib.h>
#include <rlgl.h>

#include <cstdio>
#include <limits>

namespace game {

namespace {

bool SameMatrixExact(const Matrix& a, const Matrix& b)
{
    return a.m0 == b.m0 && a.m1 == b.m1 && a.m2 == b.m2 && a.m3 == b.m3
            && a.m4 == b.m4 && a.m5 == b.m5 && a.m6 == b.m6 && a.m7 == b.m7
            && a.m8 == b.m8 && a.m9 == b.m9 && a.m10 == b.m10 && a.m11 == b.m11
            && a.m12 == b.m12 && a.m13 == b.m13 && a.m14 == b.m14 && a.m15 == b.m15;
}

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

void SectorDoorRenderer::ReserveRuntimeDoorCapacity(size_t capacity)
{
    visibleDraws.reserve(capacity);
    drawCapacityWarned = false;
    doorMeshCache.reserve(capacity);
    runtimeDoorShadowCasters.clear();
    runtimeDoorShadowCasters.reserve(capacity);
    runtimeDoorModelShadowCasters.clear();
    runtimeDoorModelShadowCasters.reserve(capacity * 2);
}

void SectorDoorRenderer::ResetOpaqueShaderLocations()
{
    opaqueShaderLocations = SectorDoorOpaqueShaderLocations{};
}

bool SectorDoorRenderer::LoadOpaqueResources()
{
    opaqueShader = LoadGameShader(GameShader::DoorOpaque);
    if (opaqueShader.id == 0) {
        opaqueShader = Shader{};
        ResetOpaqueShaderLocations();
        opaqueShaderLoaded = false;
        return false;
    }
    opaqueShaderLocations.reflections = LoadSectorReflectionShaderLocations(opaqueShader);

    opaqueShader.locs[SHADER_LOC_VERTEX_POSITION] = GetShaderLocationAttrib(opaqueShader, "vertexPosition");
    opaqueShader.locs[SHADER_LOC_VERTEX_NORMAL] = GetShaderLocationAttrib(opaqueShader, "vertexNormal");
    opaqueShader.locs[SHADER_LOC_VERTEX_TEXCOORD01] = GetShaderLocationAttrib(opaqueShader, "vertexTexCoord");
    opaqueShader.locs[SHADER_LOC_VERTEX_COLOR] = GetShaderLocationAttrib(opaqueShader, "vertexColor");
    opaqueShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(opaqueShader, "mvp");
    opaqueShader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(opaqueShader, "matModel");
    opaqueShader.locs[SHADER_LOC_MATRIX_NORMAL] = GetShaderLocation(opaqueShader, "matNormal");
    opaqueShader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(opaqueShader, "texture0");
    opaqueShader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(opaqueShader, "normalTexture");
    opaqueShader.locs[SHADER_LOC_MAP_BRDF] =
            GetShaderLocation(opaqueShader, "materialPropertiesTexture");
    opaqueShader.locs[SHADER_LOC_MAP_ROUGHNESS] = GetShaderLocation(opaqueShader, "shadowMap0");
    opaqueShader.locs[SHADER_LOC_MAP_OCCLUSION] = GetShaderLocation(opaqueShader, "shadowMap1");
    opaqueShader.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(
            opaqueShader, "environmentTexture");
    opaqueShaderLocations.texture = opaqueShader.locs[SHADER_LOC_MAP_DIFFUSE];
    opaqueShaderLocations.normalTexture = opaqueShader.locs[SHADER_LOC_MAP_NORMAL];
    opaqueShaderLocations.materialPropertiesTexture =
            opaqueShader.locs[SHADER_LOC_MAP_BRDF];
    opaqueShaderLocations.materialPropertiesKind = GetShaderLocation(
            opaqueShader, "materialPropertiesKind");
    opaqueShaderLocations.hasNormalMap = GetShaderLocation(opaqueShader, "hasNormalMap");
    opaqueShaderLocations.normalStrength = GetShaderLocation(opaqueShader, "normalStrength");
    opaqueShaderLocations.metallicFactor = GetShaderLocation(opaqueShader, "metallicFactor");
    opaqueShaderLocations.roughnessFactor = GetShaderLocation(opaqueShader, "roughnessFactor");
    opaqueShaderLocations.cameraPosition = GetShaderLocation(opaqueShader, "cameraPosition");
    opaqueShaderLocations.hasEnvironment = GetShaderLocation(opaqueShader, "hasEnvironment");
    opaqueShaderLocations.environmentExposure = GetShaderLocation(opaqueShader, "environmentExposure");
    opaqueShaderLocations.indirectDiffuseScale = GetShaderLocation(opaqueShader, "indirectDiffuseScale");
    opaqueShaderLocations.environmentSpecularScale = GetShaderLocation(opaqueShader, "environmentSpecularScale");
    opaqueShaderLocations.environmentBoxProjection = GetShaderLocation(opaqueShader, "environmentBoxProjection");
    opaqueShaderLocations.environmentCapturePosition = GetShaderLocation(opaqueShader, "environmentCapturePosition");
    opaqueShaderLocations.environmentInfluenceCenter = GetShaderLocation(opaqueShader, "environmentInfluenceCenter");
    opaqueShaderLocations.environmentHalfExtents = GetShaderLocation(opaqueShader, "environmentHalfExtents");
    opaqueShaderLocations.environmentYaw = GetShaderLocation(opaqueShader, "environmentYaw");
    opaqueShaderLocations.environmentMaxLod = GetShaderLocation(opaqueShader, "environmentMaxLod");
    opaqueShaderLocations.pbrDiagnosticMode = GetShaderLocation(opaqueShader, "pbrDiagnosticMode");
    opaqueShaderLocations.specularAaEnabled = GetShaderLocation(opaqueShader, "specularAaEnabled");
    opaqueShaderLocations.useObjectAmbientCube = GetShaderLocation(
            opaqueShader, "useObjectAmbientCube");
    opaqueShaderLocations.objectAmbientCube = GetShaderLocationArrayBase(
            opaqueShader, "objectAmbientCube");
    opaqueShaderLocations.useStaticSpecularLighting = GetShaderLocation(
            opaqueShader, "useStaticSpecularLighting");
    opaqueShaderLocations.dynamicLightCount = GetShaderLocation(opaqueShader, "dynamicLightCount");
    opaqueShaderLocations.dynamicLightPositions = GetShaderLocationArrayBase(opaqueShader, "dynamicLightPositions");
    opaqueShaderLocations.dynamicLightColors = GetShaderLocationArrayBase(opaqueShader, "dynamicLightColors");
    opaqueShaderLocations.dynamicLightRadii = GetShaderLocationArrayBase(opaqueShader, "dynamicLightRadii");
    opaqueShaderLocations.dynamicLightIntensities = GetShaderLocationArrayBase(opaqueShader, "dynamicLightIntensities");
    opaqueShaderLocations.dynamicLightTypes = GetShaderLocationArrayBase(opaqueShader, "dynamicLightTypes");
    opaqueShaderLocations.dynamicLightDirections = GetShaderLocationArrayBase(opaqueShader, "dynamicLightDirections");
    opaqueShaderLocations.dynamicLightInnerConeCos = GetShaderLocationArrayBase(opaqueShader, "dynamicLightInnerConeCos");
    opaqueShaderLocations.dynamicLightOuterConeCos = GetShaderLocationArrayBase(opaqueShader, "dynamicLightOuterConeCos");
    opaqueShaderLocations.dynamicLightSpotShadowRight = GetShaderLocationArrayBase(
            opaqueShader, "dynamicLightSpotShadowRight");
    opaqueShaderLocations.dynamicLightSpotShadowProjection = GetShaderLocationArrayBase(
            opaqueShader, "dynamicLightSpotShadowProjection");
    opaqueShaderLocations.dynamicLightProfiles = GetShaderLocationArrayBase(
            opaqueShader, "dynamicLightProfiles");
    opaqueShaderLocations.dynamicLightProfileParameters = GetShaderLocationArrayBase(
            opaqueShader, "dynamicLightProfileParameters");
    opaqueShaderLocations.flashlightCookie = GetShaderLocation(
            opaqueShader, "flashlightCookie");
    opaqueShaderLocations.hasPointShadows = GetShaderLocation(
            opaqueShader, "hasPointShadows");
    opaqueShaderLocations.dynamicLightShadowSlots = GetShaderLocationArrayBase(opaqueShader, "dynamicLightShadowSlots");
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        opaqueShaderLocations.shadowLightMatrices[i] =
                GetShaderLocationArrayElement(opaqueShader, "shadowLightMatrices", i);
    }
    opaqueShaderLocations.shadowBias = GetShaderLocationArrayBase(opaqueShader, "shadowBias");
    opaqueShaderLocations.shadowStrength = GetShaderLocationArrayBase(opaqueShader, "shadowStrength");
    opaqueShaderLocations.shadowSoftness = GetShaderLocationArrayBase(opaqueShader, "shadowSoftness");
    opaqueShaderLocations.shadowAtlasTilesPerRow = GetShaderLocation(opaqueShader, "shadowAtlasTilesPerRow");
    opaqueShaderLocations.tint = GetShaderLocation(opaqueShader, "doorTint");
    opaqueShaderLocations.staticSpecular = GetSectorStaticSpecularShaderLocations(
            opaqueShader);
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
        opaqueMaterial.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
        opaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
        opaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
        opaqueMaterial.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
        opaqueMaterial.maps[MATERIAL_MAP_BRDF].texture = Texture2D{};
        UnloadMaterial(opaqueMaterial);
        opaqueMaterial = Material{};
        opaqueDefaultMaterialTexture = Texture2D{};
        opaqueShader = Shader{};
        ResetOpaqueShaderLocations();
        opaqueMaterialLoaded = false;
        opaqueShaderLoaded = false;
    }
}

void SectorDoorRenderer::PrepareRuntimeDoorMeshes(
        engine::AssetManager& assets,
        engine::World& runtimeObjectWorld)
{
    for (auto& entry : doorMeshCache) {
        entry.second.seenThisFrame = false;
    }
    runtimeDoorShadowCasters.clear();
    runtimeDoorModelShadowCasters.clear();

    runtimeObjectWorld.ForEach<
            SectorObjectTransform,
            SectorObject,
            SectorDoor,
            SectorDoorResolvedAnchor,
            SectorDoorRender>(
            [this, &assets, &runtimeObjectWorld](
                    engine::Entity entity,
                    SectorObjectTransform& transform,
                    SectorObject& object,
                    SectorDoor& door,
                    SectorDoorResolvedAnchor& anchor,
                    SectorDoorRender& render) {
                if (runtimeObjectWorld.Has<SectorDoorModelRender>(entity)) {
                    const SectorDoorModelRender& model =
                            runtimeObjectWorld.Get<SectorDoorModelRender>(entity);
                    const SectorDoorModelDrawPolicy policy =
                            ResolveSectorDoorModelDrawPolicy(
                                    model,
                                    assets.GetModelAsset(model.leafModel) != nullptr,
                                    assets.GetModelAsset(model.frameModel) != nullptr);
                    if (!policy.drawProcedural) {
                        return;
                    }
                }
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
                    cacheEntry.staticLightingValid = false;
                }
            });

    CollectSectorDoorModelShadowCasters(
            runtimeObjectWorld, assets, runtimeDoorModelShadowCasters);

    RefreshSectorDoorShadowCasterRevision(
            shadowCasterRevisionState,
            runtimeDoorShadowCasters,
            runtimeDoorModelShadowCasters);

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

void SectorDoorRenderer::PrepareVisibleDraws(engine::AssetManager &assets, engine::World &world,
                                             const Camera3D &camera, float aspect,
                                             const RuntimePortalVisibilityResult &visibility)
{
    PrepareRuntimeDoorMeshes(assets, world);
    visibleDraws.clear();
    culledOpaqueObjects = 0;
    culledTriangles = 0;
    world.ForEach<SectorObjectTransform, SectorObject, SectorDoor, SectorDoorResolvedAnchor,
                  SectorDoorRender>(
        [&](engine::Entity entity, SectorObjectTransform &transform, SectorObject &object,
            SectorDoor &door, SectorDoorResolvedAnchor &anchor, SectorDoorRender &render) {
            if (!object.visible || !door.enabled || !render.visible || render.width <= 0 ||
                render.height <= 0 || render.thickness <= 0)
                return;
            if (world.Has<SectorDoorModelRender>(entity)) {
                const auto &model = world.Get<SectorDoorModelRender>(entity);
                if (!ResolveSectorDoorModelDrawPolicy(
                         model, assets.GetModelAsset(model.leafModel) != nullptr,
                         assets.GetModelAsset(model.frameModel) != nullptr)
                         .drawProcedural)
                    return;
            }
            const auto *mesh = FindDoorMesh(door.placedObjectId);
            if (!mesh || mesh->mesh.vertexCount <= 0)
                return;
            SectorReceiverBounds receiver{object.currentSectorId, transform.position,
                                          transform.position};
            BuildSectorDoorReceiverBounds(transform, object, door, anchor, render,
                                          object.currentSectorId, receiver);
            const BoundingBox bounds{receiver.min, receiver.max};
            const bool inView = SectorBoundsInView(camera, aspect, rlGetCullDistanceNear(),
                                                   rlGetCullDistanceFar(), bounds);
            if (!inView || !ShouldDrawSectorDoorForVisibility(anchor, visibility, inView)) {
                ++culledOpaqueObjects;
                culledTriangles += mesh->mesh.triangleCount;
                return;
            }
            AppendSectorOpaqueDraw(visibleDraws,
                                   {entity, door.placedObjectId, 0,
                                    BuildSectorDoorSlabModelMatrix(transform, anchor, render),
                                    bounds, SectorNearestViewDepth(camera, bounds)},
                                   drawCapacityWarned);
        });
    std::sort(visibleDraws.begin(), visibleDraws.end(), SectorOpaqueDrawLess);
}

void SectorDoorRenderer::DrawPreparedDepth(Material depthMaterial)
{
    if (!IsOpaqueReady())
        return;
    rlDisableBackfaceCulling(); // Procedural slabs keep their existing two-sided policy.
    for (const auto &item : visibleDraws) {
        const auto *entry = FindDoorMesh(item.id);
        if (entry)
            DrawMesh(entry->mesh, depthMaterial, item.transform);
    }
    rlEnableBackfaceCulling();
}

std::size_t SectorDoorRenderer::SubmittedTriangles() const
{
    std::size_t count = 0;
    for (const auto &item : visibleDraws) {
        const auto *entry = FindDoorMesh(item.id);
        if (entry)
            count += entry->mesh.triangleCount;
    }
    return count;
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

    size_t consideredCount = 0;
    size_t drawnCount = 0;
    size_t skippedCount = 0;
    const SectorBakedObjectLightProbeRuntimeData emptyObjectLightProbes;
    const SectorBakedObjectLightProbeRuntimeData& objectLightProbes =
            context.lighting.objectLightProbes != nullptr
            ? *context.lighting.objectLightProbes
            : emptyObjectLightProbes;
    const SectorStaticSpecularLightState emptyStaticSpecularLights;
    const SectorStaticSpecularLightState& staticSpecularLights =
            context.staticSpecularLights != nullptr
            ? *context.staticSpecularLights
            : emptyStaticSpecularLights;
    const RuntimePortalVisibilityResult emptyVisibility;
    const RuntimePortalVisibilityResult& visibility =
            context.visibility != nullptr
            ? *context.visibility
            : emptyVisibility;
    const SectorPbrContributionSettings pbr =
            NormalizeSectorPbrContributionSettings(context.pbr);
    const bool environmentActive = context.environment != nullptr
            && context.environment->id != 0
            && pbr.worldEnvironmentSpecularScale > 0.0f;

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
    dynamicLightLocations.dynamicLightSpotShadowRight =
            doorOpaqueLocations.dynamicLightSpotShadowRight;
    dynamicLightLocations.dynamicLightSpotShadowProjection =
            doorOpaqueLocations.dynamicLightSpotShadowProjection;
    dynamicLightLocations.dynamicLightProfiles =
            doorOpaqueLocations.dynamicLightProfiles;
    dynamicLightLocations.dynamicLightProfileParameters =
            doorOpaqueLocations.dynamicLightProfileParameters;
    dynamicLightLocations.flashlightCookie =
            doorOpaqueLocations.flashlightCookie;
    dynamicLightLocations.hasPointShadows = doorOpaqueLocations.hasPointShadows;
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
    shadowLocations.shadowAtlasTilesPerRow = doorOpaqueLocations.shadowAtlasTilesPerRow;
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
    doorOpaqueMaterial.maps[MATERIAL_MAP_CUBEMAP].texture = environmentActive
            ? *context.environment
            : Texture2D{};
    if (doorOpaqueLocations.texture >= 0) {
        const int diffuseTextureUnit = 0;
        SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueLocations.texture, &diffuseTextureUnit, SHADER_UNIFORM_INT);
    }
    if (doorOpaqueLocations.normalTexture >= 0) {
        const int normalTextureUnit = MATERIAL_MAP_NORMAL;
        SetShaderValue(
                doorOpaqueMaterial.shader,
                doorOpaqueLocations.normalTexture,
                &normalTextureUnit,
                SHADER_UNIFORM_INT);
    }
    if (doorOpaqueLocations.cameraPosition >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.cameraPosition,
            &context.camera.position,
            SHADER_UNIFORM_VEC3);
    const int hasEnvironment = environmentActive ? 1 : 0;
    if (doorOpaqueLocations.hasEnvironment >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.hasEnvironment,
            &hasEnvironment,
            SHADER_UNIFORM_INT);
    const float environmentExposure = SanitizeSectorPbrNonnegative(
            context.environmentExposure);
    if (doorOpaqueLocations.environmentExposure >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.environmentExposure,
            &environmentExposure,
            SHADER_UNIFORM_FLOAT);
    if (doorOpaqueLocations.indirectDiffuseScale >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.indirectDiffuseScale,
            &pbr.worldIndirectDiffuseScale,
            SHADER_UNIFORM_FLOAT);
    if (doorOpaqueLocations.environmentSpecularScale >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.environmentSpecularScale,
            &pbr.worldEnvironmentSpecularScale,
            SHADER_UNIFORM_FLOAT);
    const int boxProjection = context.environmentBoxProjection ? 1 : 0;
    if (doorOpaqueLocations.environmentBoxProjection >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentBoxProjection,
            &boxProjection, SHADER_UNIFORM_INT);
    if (doorOpaqueLocations.environmentCapturePosition >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentCapturePosition,
            &context.environmentCapturePosition, SHADER_UNIFORM_VEC3);
    if (doorOpaqueLocations.environmentInfluenceCenter >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentInfluenceCenter,
            &context.environmentInfluenceCenter, SHADER_UNIFORM_VEC3);
    if (doorOpaqueLocations.environmentHalfExtents >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentHalfExtents,
            &context.environmentHalfExtents, SHADER_UNIFORM_VEC3);
    if (doorOpaqueLocations.environmentYaw >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentYaw,
            &context.environmentYaw, SHADER_UNIFORM_FLOAT);
    if (doorOpaqueLocations.environmentMaxLod >= 0) SetShaderValue(
            doorOpaqueMaterial.shader, doorOpaqueLocations.environmentMaxLod,
            &context.environmentMaxLod, SHADER_UNIFORM_FLOAT);
    const int pbrDiagnosticMode = pbr.reflectionCapture ? 11 : static_cast<int>(pbr.diagnosticMode);
    const int specularAaEnabled = pbr.specularAaEnabled && !pbr.reflectionCapture ? 1 : 0;
    if (doorOpaqueLocations.specularAaEnabled >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.specularAaEnabled,
            &specularAaEnabled,
            SHADER_UNIFORM_INT);
    if (doorOpaqueLocations.pbrDiagnosticMode >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.pbrDiagnosticMode,
            &pbrDiagnosticMode,
            SHADER_UNIFORM_INT);
    const int useObjectAmbientCube = 0;
    if (doorOpaqueLocations.useObjectAmbientCube >= 0) SetShaderValue(
            doorOpaqueMaterial.shader,
            doorOpaqueLocations.useObjectAmbientCube,
            &useObjectAmbientCube,
            SHADER_UNIFORM_INT);

    const auto drawDoor =
            [this,
             &context,
             &consideredCount,
             &drawnCount,
             &skippedCount,
             &objectLightProbes,
             &staticSpecularLights,
             &visibility,
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
                if (context.runtimeObjectWorld->Has<SectorDoorModelRender>(entity)) {
                    const SectorDoorModelRender& model =
                            context.runtimeObjectWorld->Get<SectorDoorModelRender>(entity);
                    const SectorDoorModelDrawPolicy policy =
                            ResolveSectorDoorModelDrawPolicy(
                                    model,
                                    context.assets->GetModelAsset(model.leafModel) != nullptr,
                                    context.assets->GetModelAsset(model.frameModel) != nullptr);
                    if (!policy.drawProcedural) {
                        ++skippedCount;
                        return;
                    }
                }
                if (render.width <= 0.0f || render.height <= 0.0f || render.thickness <= 0.0f) {
                    ++skippedCount;
                    return;
                }

                const int receiverSectorId = object.currentSectorId > 0
                        ? object.currentSectorId
                        : (anchor.frontSectorId > 0 ? anchor.frontSectorId : anchor.backSectorId);
                SectorReceiverBounds receiverBounds{receiverSectorId, transform.position, transform.position};
                BuildSectorDoorReceiverBounds(transform, object, door, anchor, render,
                        receiverSectorId, receiverBounds);
                const BoundingBox reflectionBounds{receiverBounds.min, receiverBounds.max};
                if (!AcceptSectorReflectionObject(context.captureCulling, reflectionBounds)) {
                    ++skippedCount;
                    return;
                }

                SectorDoorResolvedMaterial resolvedMaterial;
                if (!render.materialId.empty()
                        && context.materialResolver.resolve != nullptr) {
                    resolvedMaterial = context.materialResolver.resolve(
                            context.materialResolver.userData,
                            *context.assets,
                            render.materialId);
                }
                if (resolvedMaterial.albedo == nullptr) {
                    resolvedMaterial.albedo = context.defaultMaterialTexture != nullptr
                            ? context.defaultMaterialTexture
                            : &opaqueDefaultMaterialTexture;
                }
                if (resolvedMaterial.albedo == nullptr
                        || resolvedMaterial.albedo->id == 0) {
                    ++skippedCount;
                    return;
                }
                const bool hasNormalMap = resolvedMaterial.normal != nullptr
                        && resolvedMaterial.normal->id != 0;
                const bool hasPropertyMap = resolvedMaterial.properties != nullptr
                        && resolvedMaterial.properties->id != 0;
                resolvedMaterial.normalStrength = std::isfinite(
                            resolvedMaterial.normalStrength)
                        ? std::clamp(resolvedMaterial.normalStrength, 0.0f, 1.0f)
                        : 1.0f;
                resolvedMaterial.metallicFactor = std::isfinite(
                            resolvedMaterial.metallicFactor)
                        ? std::clamp(resolvedMaterial.metallicFactor, 0.0f, 1.0f)
                        : 0.0f;
                resolvedMaterial.roughnessFactor = std::isfinite(
                            resolvedMaterial.roughnessFactor)
                        ? std::clamp(resolvedMaterial.roughnessFactor, 0.0f, 1.0f)
                        : 0.8f;

                DoorMeshCacheEntry* cacheEntry = FindMutableDoorMesh(door.placedObjectId);
                if (cacheEntry == nullptr || cacheEntry->mesh.vertexCount <= 0) {
                    ++skippedCount;
                    return;
                }

                const Matrix doorModel = BuildSectorDoorSlabModelMatrix(
                        transform,
                        anchor,
                        render);
                const bool staticLightingDirty = !cacheEntry->staticLightingValid
                        || cacheEntry->staticLightingSectorId != object.currentSectorId
                        || cacheEntry->staticLightingRevision != context.lighting.revision
                        || !SameMatrixExact(cacheEntry->staticLightingModel, doorModel);
                if (staticLightingDirty && !BuildSectorDoorStaticLightingColors(
                            cacheEntry->meshData,
                            transform,
                            object,
                            anchor,
                            render,
                            objectLightProbes,
                            context.lighting.mapForFallback,
                            cacheEntry->staticLightingValues)) {
                    cacheEntry->staticLightingValues.assign(
                            static_cast<size_t>(cacheEntry->mesh.vertexCount),
                            Vector3{1.0f, 1.0f, 1.0f});
                }
                if (staticLightingDirty
                        && cacheEntry->mesh.tangents != nullptr
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
                if (staticLightingDirty) {
                    cacheEntry->staticLightingModel = doorModel;
                    cacheEntry->staticLightingSectorId = object.currentSectorId;
                    cacheEntry->staticLightingRevision = context.lighting.revision;
                    cacheEntry->staticLightingValid = true;
                }

                if (doorOpaqueLocations.tint >= 0) {
                    const Vector4 tint = engine::SrgbColorBytesToLinearSceneRgba(
                            render.tint);
                    SetShaderValue(doorOpaqueMaterial.shader, doorOpaqueLocations.tint, &tint, SHADER_UNIFORM_VEC4);
                }

                const int hasNormalMapValue = hasNormalMap ? 1 : 0;
                if (doorOpaqueLocations.hasNormalMap >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.hasNormalMap,
                        &hasNormalMapValue,
                        SHADER_UNIFORM_INT);
                if (doorOpaqueLocations.normalStrength >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.normalStrength,
                        &resolvedMaterial.normalStrength,
                        SHADER_UNIFORM_FLOAT);
                const int propertyMapKind = hasPropertyMap
                        ? static_cast<int>(resolvedMaterial.propertyMapKind)
                        : static_cast<int>(SectorMaterialPropertyMapKind::None);
                if (doorOpaqueLocations.materialPropertiesKind >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.materialPropertiesKind,
                        &propertyMapKind,
                        SHADER_UNIFORM_INT);
                if (doorOpaqueLocations.metallicFactor >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.metallicFactor,
                        &resolvedMaterial.metallicFactor,
                        SHADER_UNIFORM_FLOAT);
                if (doorOpaqueLocations.roughnessFactor >= 0) SetShaderValue(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.roughnessFactor,
                        &resolvedMaterial.roughnessFactor,
                        SHADER_UNIFORM_FLOAT);

                const SectorStaticSpecularLightContext staticSpecularContext =
                        SelectSectorStaticSpecularLights(
                                staticSpecularLights,
                                receiverBounds,
                                receiverSectorId,
                                visibility,
                                context.staticSpecularEligible);
                UploadSectorStaticSpecularLights(
                        doorOpaqueMaterial.shader,
                        doorOpaqueLocations.staticSpecular,
                        staticSpecularContext);
                const int useStaticSpecularLighting =
                        staticSpecularContext.lightCount > 0 ? 1 : 0;
                if (doorOpaqueLocations.useStaticSpecularLighting >= 0) {
                    SetShaderValue(
                            doorOpaqueMaterial.shader,
                            doorOpaqueLocations.useStaticSpecularLighting,
                            &useStaticSpecularLighting,
                            SHADER_UNIFORM_INT);
                }

                doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture =
                        *resolvedMaterial.albedo;
                doorOpaqueMaterial.maps[MATERIAL_MAP_NORMAL].texture = hasNormalMap
                        ? *resolvedMaterial.normal
                        : Texture2D{};
                doorOpaqueMaterial.maps[MATERIAL_MAP_BRDF].texture = hasPropertyMap
                        ? *resolvedMaterial.properties
                        : Texture2D{};
                doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
                const auto blend=context.reflectionEnvironment && !context.pbr.reflectionCapture
                        ? SelectSectorPbrEnvironmentBlend(*context.reflectionEnvironment,transform.position,object.currentSectorId,
                                true, nullptr, SectorReflectionDemandForBounds(
                                        context.reflectionEnvironment->demandCollector, reflectionBounds))
                        : SectorPbrEnvironmentBlend{};
                UploadSectorReflectionBlend(doorOpaqueMaterial.shader,doorOpaqueLocations.reflections,blend,*context.assets);
                DrawMesh(
                        cacheEntry->mesh,
                        doorOpaqueMaterial,
                        doorModel);
                ++drawnCount;
            };
    for (const auto& item : visibleDraws) {
        if (!context.runtimeObjectWorld->IsAlive(item.entity)) continue;
        auto& world = *context.runtimeObjectWorld;
        drawDoor(item.entity, world.Get<SectorObjectTransform>(item.entity),
                world.Get<SectorObject>(item.entity), world.Get<SectorDoor>(item.entity),
                world.Get<SectorDoorResolvedAnchor>(item.entity), world.Get<SectorDoorRender>(item.entity));
    }

    doorOpaqueMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = doorOpaqueDefaultMaterialTexture;
    doorOpaqueMaterial.maps[MATERIAL_MAP_NORMAL].texture = Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_OCCLUSION].texture = Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_CUBEMAP].texture = Texture2D{};
    doorOpaqueMaterial.maps[MATERIAL_MAP_BRDF].texture = Texture2D{};
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
        if (context.assets != nullptr) {
            PrepareRuntimeDoorMeshes(*context.assets, *runtimeObjectWorld);
        } else {
            ClearPreparedShadowCasters();
        }
    } else {
        ClearPreparedShadowCasters();
    }

    context.doorShadowCasters = &ShadowCasters();
    context.doorModelShadowCasters = &runtimeDoorModelShadowCasters;
    context.doorShadowCasterRevision = shadowCasterRevisionState.revision;
    context.doorMeshResolverUserData = this;
    context.doorMeshResolver = &SectorDoorRenderer::ResolveDoorShadowCasterMesh;
}

void SectorDoorRenderer::ClearPreparedShadowCasters()
{
    runtimeDoorShadowCasters.clear();
    runtimeDoorModelShadowCasters.clear();
    RefreshSectorDoorShadowCasterRevision(
            shadowCasterRevisionState,
            runtimeDoorShadowCasters,
            runtimeDoorModelShadowCasters);
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
    runtimeDoorModelShadowCasters.clear();
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
