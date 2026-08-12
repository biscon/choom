#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <cstring>
#include <raymath.h>
#include <rlgl.h>

namespace game {

namespace {

const char* SectorSpotLightShadowVs = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;

uniform mat4 lightViewProjection;
uniform mat4 matModel;

out vec2 fragTexCoord;

void main()
{
    fragTexCoord = vertexTexCoord;
    gl_Position = lightViewProjection * matModel * vec4(vertexPosition, 1.0);
}
)";

const char* SectorSpotLightShadowFs = R"(
#version 330
in vec2 fragTexCoord;

uniform sampler2D texture0;
uniform int alphaTest;
uniform float alphaCutoff;

void main()
{
    if (alphaTest != 0 && texture(texture0, fragTexCoord).a < alphaCutoff) {
        discard;
    }
}
)";

RenderTexture2D LoadDepthOnlyRenderTexture(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    target.texture.width = width;
    target.texture.height = height;

    if (target.id <= 0) {
        return target;
    }

    rlEnableFramebuffer(target.id);
    target.depth.id = rlLoadTextureDepth(width, height, false);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    if (!rlFramebufferComplete(target.id)) {
        rlDisableFramebuffer();
        rlUnloadFramebuffer(target.id);
        return RenderTexture2D{};
    }

    rlDisableFramebuffer();
    return target;
}

void UnloadDepthOnlyRenderTexture(RenderTexture2D& target)
{
    if (target.id > 0) {
        rlUnloadFramebuffer(target.id);
    }
    target = RenderTexture2D{};
}

} // namespace

void UploadSectorRendererDynamicPointLights(
        Shader shader,
        const SectorDynamicLightShaderLocations& locations,
        bool dynamicLightingEnabled,
        float runtimeSeconds,
        const std::vector<SectorPreviewDynamicPointLightUniform>& lights)
{
    const int lightCount = dynamicLightingEnabled
            ? static_cast<int>(std::min(lights.size(), static_cast<size_t>(MaxDynamicLights)))
            : 0;
    SectorBillboardDynamicLightContext context;
    context.dynamicLightCount = lightCount;
    if (lightCount <= 0) {
        UploadSectorRendererDynamicPointLights(shader, locations, context);
        return;
    }

    std::array<Vector3, MaxDynamicLights> positions{};
    std::array<Vector3, MaxDynamicLights> colors{};
    std::array<float, MaxDynamicLights> radii{};
    std::array<float, MaxDynamicLights> intensities{};
    std::array<int, MaxDynamicLights> types{};
    std::array<Vector3, MaxDynamicLights> directions{};
    std::array<float, MaxDynamicLights> innerConeCos{};
    std::array<float, MaxDynamicLights> outerConeCos{};
    for (int i = 0; i < lightCount; ++i) {
        positions[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].position;
        colors[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].color;
        radii[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].radius;
        intensities[static_cast<size_t>(i)] = DynamicLightEffectiveUploadIntensity(
                lights[static_cast<size_t>(i)],
                runtimeSeconds);
        types[static_cast<size_t>(i)] = static_cast<int>(lights[static_cast<size_t>(i)].kind);
        directions[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].direction;
        innerConeCos[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].innerConeCos;
        outerConeCos[static_cast<size_t>(i)] = lights[static_cast<size_t>(i)].outerConeCos;
    }

    context.dynamicLightPositions = positions;
    context.dynamicLightColors = colors;
    context.dynamicLightRadii = radii;
    context.dynamicLightIntensities = intensities;
    context.dynamicLightTypes = types;
    context.dynamicLightDirections = directions;
    context.dynamicLightInnerConeCos = innerConeCos;
    context.dynamicLightOuterConeCos = outerConeCos;
    UploadSectorRendererDynamicPointLights(shader, locations, context);
}

void UploadSectorRendererDynamicPointLights(
        Shader shader,
        const SectorDynamicLightShaderLocations& locations,
        const SectorBillboardDynamicLightContext& context)
{
    if (locations.dynamicLightCount >= 0) {
        SetShaderValue(shader, locations.dynamicLightCount, &context.dynamicLightCount, SHADER_UNIFORM_INT);
    }
    if (context.dynamicLightCount <= 0) {
        return;
    }

    if (locations.dynamicLightPositions >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightPositions,
                context.dynamicLightPositions.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightColors >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightColors,
                context.dynamicLightColors.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightRadii >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightRadii,
                context.dynamicLightRadii.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightIntensities >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightIntensities,
                context.dynamicLightIntensities.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightTypes >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightTypes,
                context.dynamicLightTypes.data(),
                SHADER_UNIFORM_INT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightDirections >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightDirections,
                context.dynamicLightDirections.data(),
                SHADER_UNIFORM_VEC3,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightInnerConeCos >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightInnerConeCos,
                context.dynamicLightInnerConeCos.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
    if (locations.dynamicLightOuterConeCos >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightOuterConeCos,
                context.dynamicLightOuterConeCos.data(),
                SHADER_UNIFORM_FLOAT,
                context.dynamicLightCount);
    }
}

void UploadSectorRendererDynamicSpotLightShadowUniforms(
        Shader shader,
        const SectorDynamicSpotLightShadowShaderLocations& locations,
        const SectorPreviewDynamicSpotLightShadowUniforms& uniforms)
{
    if (locations.dynamicLightShadowSlots >= 0) {
        SetShaderValueV(
                shader,
                locations.dynamicLightShadowSlots,
                uniforms.dynamicLightShadowSlots.data(),
                SHADER_UNIFORM_INT,
                static_cast<int>(MaxDynamicLights));
    }
    for (std::size_t i = 0; i < MaxDynamicSpotLightShadowCasters; ++i) {
        if (locations.shadowLightMatrices[i] >= 0) {
            SetShaderValueMatrix(shader, locations.shadowLightMatrices[i], uniforms.shadowLightMatrices[i]);
        }
    }
    if (locations.shadowBias >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowBias,
                uniforms.shadowBias.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
    if (locations.shadowStrength >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowStrength,
                uniforms.shadowStrength.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
    if (locations.shadowSoftness >= 0) {
        SetShaderValueV(
                shader,
                locations.shadowSoftness,
                uniforms.shadowSoftness.data(),
                SHADER_UNIFORM_FLOAT,
                static_cast<int>(MaxDynamicSpotLightShadowCasters));
    }
}

void SectorDynamicLightingRenderer::Reset()
{
    sources.clear();
    selectionSources.clear();
    runtimePointLight = {};
    runtimePointLightActive = false;
    candidates.clear();
    selectedLights.clear();
    selectedLightIds.clear();
    receiverBounds.clear();
    shadowCasters.clear();
    shadowMatrices.clear();
    cachedShadowMatrices.clear();
    shadowMapsCacheValid = false;
}

void SectorDynamicLightingRenderer::RebuildSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld)
{
    BuildSectorPreviewDynamicPointLightSources(map, sectorLookupWorld, sources);
    selectionSources.reserve(sources.size() + 1);
    ReserveSelectionBuffers();
}

void SectorDynamicLightingRenderer::ReserveReceiverBoundsCapacity(
        size_t sectorCapacity,
        size_t runtimeObjectCapacity)
{
    receiverBounds.clear();
    receiverBounds.reserve(sectorCapacity + runtimeObjectCapacity * 2);
}

void SectorDynamicLightingRenderer::SetRuntimePointLight(
        const SectorPreviewDynamicPointLightSource* light)
{
    runtimePointLightActive = light != nullptr;
    runtimePointLight = light != nullptr
            ? *light
            : SectorPreviewDynamicPointLightSource{};
}

void SectorDynamicLightingRenderer::UpdateSelection(
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld)
{
    BuildReceiverBounds(sectorReceiverBounds, runtimeObjectWorld);
    selectionSources.assign(sources.begin(), sources.end());
    if (runtimePointLightActive) selectionSources.push_back(runtimePointLight);
    CollectSectorPreviewDynamicPointLightCandidates(
            selectionSources,
            visibility,
            receiverBounds,
            candidates);
    SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visibility,
            receiverBounds,
            static_cast<std::size_t>(MaxDynamicLights),
            selectedLights,
            &selectedLightIds,
            &selectedLightIds);
    SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selectedLights,
            visibility,
            receiverBounds,
            MaxDynamicSpotLightShadowCasters,
            shadowCasters);
    BuildSectorPreviewDynamicSpotLightShadowMatrices(
            selectedLights,
            shadowCasters,
            shadowMatrices);
    bool matricesMatch = shadowMatrices.size() == cachedShadowMatrices.size();
    for (std::size_t i = 0; matricesMatch && i < shadowMatrices.size(); ++i) {
        const auto& current = shadowMatrices[i];
        const auto& cached = cachedShadowMatrices[i];
        matricesMatch = current.lightId == cached.lightId
                && current.shadowSlot == cached.shadowSlot
                && std::memcmp(&current.lightViewProjection,
                        &cached.lightViewProjection, sizeof(Matrix)) == 0;
    }
    if (!matricesMatch) {
        shadowMapsCacheValid = false;
        cachedShadowMatrices.assign(shadowMatrices.begin(), shadowMatrices.end());
    }
}

SectorPreviewDynamicSpotLightShadowUniforms SectorDynamicLightingRenderer::PackShadowUniforms(
        bool enabled) const
{
    if (!enabled) {
        SectorPreviewDynamicSpotLightShadowUniforms result;
        result.dynamicLightShadowSlots.fill(-1);
        return result;
    }
    return PackSectorPreviewDynamicSpotLightShadowUniforms(selectedLights, shadowCasters, shadowMatrices);
}

bool SectorDynamicLightingRenderer::EnsureShadowMapResources()
{
    for (RenderTexture2D& shadowMap : shadowMaps) {
        if (shadowMap.id != 0 && shadowMap.depth.id != 0) {
            continue;
        }

        shadowMap = LoadDepthOnlyRenderTexture(
                shadowMapResolution,
                shadowMapResolution);
        if (shadowMap.id == 0 || shadowMap.depth.id == 0) {
            UnloadShadowMapResources();
            return false;
        }
        SetTextureFilter(shadowMap.depth, TEXTURE_FILTER_POINT);
        SetTextureWrap(shadowMap.depth, TEXTURE_WRAP_CLAMP);
    }

    return true;
}

void SectorDynamicLightingRenderer::SetShadowMapResolution(int resolution)
{
    resolution = std::clamp(resolution, 256, 2048);
    if (shadowMapResolution == resolution) {
        return;
    }
    shadowMapResolution = resolution;
    shadowMapsCacheValid = false;
    UnloadShadowMapResources();
    EnsureShadowMapResources();
}

void SectorDynamicLightingRenderer::UnloadShadowMapResources()
{
    for (RenderTexture2D& shadowMap : shadowMaps) {
        UnloadDepthOnlyRenderTexture(shadowMap);
    }
}

bool SectorDynamicLightingRenderer::HasShadowMapResources() const
{
    for (const RenderTexture2D& shadowMap : shadowMaps) {
        if (shadowMap.id != 0 || shadowMap.depth.id != 0) {
            return true;
        }
    }
    return false;
}

bool SectorDynamicLightingRenderer::LoadShadowMaterial()
{
    shadowMaterial = LoadMaterialDefault();
    Shader shader = LoadShaderFromMemory(SectorSpotLightShadowVs, SectorSpotLightShadowFs);
    if (shader.id == 0) {
        UnloadMaterial(shadowMaterial);
        shadowMaterial = Material{};
        return false;
    }
    shadowMaterial.shader = shader;
    shadowMaterial.shader.locs[SHADER_LOC_VERTEX_POSITION] =
            GetShaderLocationAttrib(shadowMaterial.shader, "vertexPosition");
    shadowMaterial.shader.locs[SHADER_LOC_VERTEX_TEXCOORD01] =
            GetShaderLocationAttrib(shadowMaterial.shader, "vertexTexCoord");
    shadowMaterial.shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(shadowMaterial.shader, "matModel");
    shadowMaterial.shader.locs[SHADER_LOC_MAP_DIFFUSE] = GetShaderLocation(shadowMaterial.shader, "texture0");
    shadowLightViewProjectionLoc = GetShaderLocation(shadowMaterial.shader, "lightViewProjection");
    shadowAlphaTestLoc = GetShaderLocation(shadowMaterial.shader, "alphaTest");
    shadowAlphaCutoffLoc = GetShaderLocation(shadowMaterial.shader, "alphaCutoff");
    shadowDefaultTexture = shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture;
    shadowMaterialLoaded = true;
    return true;
}

void SectorDynamicLightingRenderer::UnloadShadowMaterial()
{
    if (!shadowMaterialLoaded) {
        return;
    }

    shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = shadowDefaultTexture;
    UnloadMaterial(shadowMaterial);
    shadowMaterial = Material{};
    shadowDefaultTexture = Texture2D{};
    shadowMaterialLoaded = false;
    shadowLightViewProjectionLoc = -1;
    shadowAlphaTestLoc = -1;
    shadowAlphaCutoffLoc = -1;
}

bool SectorDynamicLightingRenderer::IsShadowRenderReady() const
{
    return shadowMaterialLoaded
            && shadowMaterial.shader.id != 0
            && shadowLightViewProjectionLoc >= 0
            && !shadowMatrices.empty();
}

RenderTexture2D* SectorDynamicLightingRenderer::ShadowMap(std::size_t index)
{
    if (index >= shadowMaps.size()) {
        return nullptr;
    }
    return &shadowMaps[index];
}

const RenderTexture2D* SectorDynamicLightingRenderer::ShadowMap(std::size_t index) const
{
    if (index >= shadowMaps.size()) {
        return nullptr;
    }
    return &shadowMaps[index];
}

const Texture2D* SectorDynamicLightingRenderer::ShadowMapDepthTexture(std::size_t index) const
{
    const RenderTexture2D* shadowMap = ShadowMap(index);
    if (shadowMap == nullptr || shadowMap->depth.id == 0) {
        return nullptr;
    }
    return &shadowMap->depth;
}

SectorDynamicShadowMapTextures SectorDynamicLightingRenderer::BuildShadowMapTextures(
        bool enabled) const
{
    SectorDynamicShadowMapTextures textures;
    if (!enabled) {
        return textures;
    }
    textures.shadowMap0 = ShadowMapDepthTexture(0);
    textures.shadowMap1 = ShadowMapDepthTexture(1);
    return textures;
}

void SectorDynamicLightingRenderer::RenderShadowMaps(
        const SectorDynamicSpotLightShadowRenderContext& context)
{
    if (context.assets == nullptr
            || !IsShadowRenderReady()
            || context.sectorDrawRecords == nullptr
            || shadowMatrices.empty()) {
        return;
    }

    const bool hasDoorCasters = context.doorShadowCasters != nullptr
            && !context.doorShadowCasters->empty();
    const bool hasDoorModelCasters = context.doorModelShadowCasters != nullptr
            && !context.doorModelShadowCasters->empty();
    if (hasDoorCasters || hasDoorModelCasters) {
        shadowMapsCacheValid = false;
    } else if (shadowMapsCacheValid) {
        return;
    }
    bool cacheable = !hasDoorCasters && !hasDoorModelCasters;

    for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix : shadowMatrices) {
        if (matrix.shadowSlot < 0) {
            continue;
        }

        RenderTexture2D* shadowMap = ShadowMap(static_cast<std::size_t>(matrix.shadowSlot));
        if (shadowMap == nullptr) {
            continue;
        }
        if (shadowMap->id == 0 || shadowMap->depth.id == 0) {
            continue;
        }

        BeginTextureMode(*shadowMap);
        ClearBackground(WHITE);
        rlEnableDepthTest();
        SetShaderValueMatrix(
                shadowMaterial.shader,
                shadowLightViewProjectionLoc,
                matrix.lightViewProjection);
        for (const SectorMeshBatch& batch : *context.sectorDrawRecords) {
            const int alphaTest = batch.alphaTest ? 1 : 0;
            const float alphaCutoff = batch.alphaCutoff;
            const Texture2D* texture = nullptr;
            if (batch.alphaTest && context.textureResolver != nullptr) {
                texture = context.textureResolver(context.userData, *context.assets, batch.textureId);
                if (texture == nullptr || texture->id == 0) {
                    cacheable = false;
                }
            }
            shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                    ? *texture
                    : shadowDefaultTexture;
            if (shadowAlphaTestLoc >= 0) {
                SetShaderValue(
                        shadowMaterial.shader,
                        shadowAlphaTestLoc,
                        &alphaTest,
                        SHADER_UNIFORM_INT);
            }
            if (shadowAlphaCutoffLoc >= 0) {
                SetShaderValue(
                        shadowMaterial.shader,
                        shadowAlphaCutoffLoc,
                        &alphaCutoff,
                        SHADER_UNIFORM_FLOAT);
            }
            DrawMesh(batch.mesh, shadowMaterial, MatrixIdentity());
        }
        const int doorAlphaTest = 0;
        const float doorAlphaCutoff = 0.0f;
        shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = shadowDefaultTexture;
        if (shadowAlphaTestLoc >= 0) {
            SetShaderValue(
                    shadowMaterial.shader,
                    shadowAlphaTestLoc,
                    &doorAlphaTest,
                    SHADER_UNIFORM_INT);
        }
        if (shadowAlphaCutoffLoc >= 0) {
            SetShaderValue(
                    shadowMaterial.shader,
                    shadowAlphaCutoffLoc,
                    &doorAlphaCutoff,
                    SHADER_UNIFORM_FLOAT);
        }
        if (context.doorShadowCasters != nullptr && context.doorMeshResolver != nullptr) {
            for (const SectorDoorShadowCaster& caster : *context.doorShadowCasters) {
                float doorWidth = 0.0f;
                float doorHeight = 0.0f;
                const Mesh* doorMesh = context.doorMeshResolver(
                        context.doorMeshResolverUserData,
                        caster,
                        doorWidth,
                        doorHeight);
                if (doorMesh == nullptr || doorMesh->vertexCount <= 0) {
                    continue;
                }
                const Matrix shadowModel = BuildSectorDoorShadowCasterModelMatrix(
                        caster,
                        doorWidth,
                        doorHeight);
                DrawMesh(*doorMesh, shadowMaterial, shadowModel);
            }
        }
        if (context.doorModelShadowCasters != nullptr) {
            for (const SectorDoorModelShadowCaster& caster
                    : *context.doorModelShadowCasters) {
                const engine::ModelAsset* asset =
                        context.assets->GetModelAsset(caster.model);
                if (asset == nullptr) {
                    continue;
                }
                const Model& model = asset->model;
                const Matrix modelTransform = MatrixMultiply(
                        model.transform, caster.transform);
                for (int meshIndex = 0; meshIndex < model.meshCount; ++meshIndex) {
                    if (model.meshes[meshIndex].vertexCount <= 0) {
                        continue;
                    }
                    DrawMesh(
                            model.meshes[meshIndex],
                            shadowMaterial,
                            modelTransform);
                }
            }
        }
        shadowMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = shadowDefaultTexture;
        rlDisableDepthTest();
        EndTextureMode();
    }
    shadowMapsCacheValid = cacheable;
}

void SectorDynamicLightingRenderer::ReserveSelectionBuffers()
{
    candidates.clear();
    candidates.reserve(sources.size() + 1);
    selectedLights.clear();
    selectedLights.reserve(MaxDynamicLights);
    selectedLightIds.clear();
    selectedLightIds.reserve(MaxDynamicLights);
    shadowCasters.clear();
    shadowCasters.reserve(MaxDynamicSpotLightShadowCasters);
    shadowMatrices.clear();
    shadowMatrices.reserve(MaxDynamicSpotLightShadowCasters);
    cachedShadowMatrices.clear();
    cachedShadowMatrices.reserve(MaxDynamicSpotLightShadowCasters);
}

void SectorDynamicLightingRenderer::BuildReceiverBounds(
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld)
{
    receiverBounds.clear();
    receiverBounds.insert(receiverBounds.end(), sectorReceiverBounds.begin(), sectorReceiverBounds.end());
    if (runtimeObjectWorld != nullptr) {
        CollectSectorDoorReceiverBounds(*runtimeObjectWorld, receiverBounds);
    }
}

} // namespace game
