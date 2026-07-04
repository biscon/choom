#include "sector_demo/SectorPreviewDynamicLighting.h"

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <raymath.h>
#include <rlgl.h>

namespace game {

namespace {

constexpr float DynamicLightingClamp = 4.0f;

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

void UploadSectorPreviewDynamicPointLights(
        Shader shader,
        const SectorPreviewDynamicLightShaderLocations& locations,
        bool dynamicLightingEnabled,
        float runtimeSeconds,
        const std::vector<SectorPreviewDynamicPointLightUniform>& lights)
{
    const int lightCount = dynamicLightingEnabled
            ? static_cast<int>(std::min(lights.size(), static_cast<size_t>(MaxDynamicLights)))
            : 0;
    SectorPreviewBillboardDynamicLightContext context;
    context.dynamicLightCount = lightCount;
    context.dynamicLightingClamp = DynamicLightingClamp;
    if (lightCount <= 0) {
        UploadSectorPreviewDynamicPointLights(shader, locations, context);
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
    UploadSectorPreviewDynamicPointLights(shader, locations, context);
}

void UploadSectorPreviewDynamicPointLights(
        Shader shader,
        const SectorPreviewDynamicLightShaderLocations& locations,
        const SectorPreviewBillboardDynamicLightContext& context)
{
    if (locations.dynamicLightCount >= 0) {
        SetShaderValue(shader, locations.dynamicLightCount, &context.dynamicLightCount, SHADER_UNIFORM_INT);
    }
    if (locations.dynamicLightingClamp >= 0) {
        SetShaderValue(shader, locations.dynamicLightingClamp, &context.dynamicLightingClamp, SHADER_UNIFORM_FLOAT);
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

void UploadSectorPreviewDynamicSpotLightShadowUniforms(
        Shader shader,
        const SectorPreviewDynamicSpotLightShadowShaderLocations& locations,
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

void SectorPreviewDynamicLighting::Reset()
{
    sources.clear();
    candidates.clear();
    selectedLights.clear();
    selectedLightIds.clear();
    receiverBounds.clear();
    shadowCasters.clear();
    shadowMatrices.clear();
}

void SectorPreviewDynamicLighting::RebuildSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld)
{
    BuildSectorPreviewDynamicPointLightSources(map, sectorLookupWorld, sources);
    ReserveSelectionBuffers();
}

void SectorPreviewDynamicLighting::UpdateSelection(
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld)
{
    BuildReceiverBounds(sectorReceiverBounds, runtimeObjectWorld);
    CollectSectorPreviewDynamicPointLightCandidates(
            sources,
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
}

SectorPreviewDynamicSpotLightShadowUniforms SectorPreviewDynamicLighting::PackShadowUniforms() const
{
    return PackSectorPreviewDynamicSpotLightShadowUniforms(selectedLights, shadowCasters, shadowMatrices);
}

bool SectorPreviewDynamicLighting::EnsureShadowMapResources()
{
    for (RenderTexture2D& shadowMap : shadowMaps) {
        if (shadowMap.id != 0 && shadowMap.depth.id != 0) {
            continue;
        }

        shadowMap = LoadDepthOnlyRenderTexture(
                DynamicSpotLightShadowMapResolution,
                DynamicSpotLightShadowMapResolution);
        if (shadowMap.id == 0 || shadowMap.depth.id == 0) {
            UnloadShadowMapResources();
            return false;
        }
        SetTextureFilter(shadowMap.depth, TEXTURE_FILTER_POINT);
        SetTextureWrap(shadowMap.depth, TEXTURE_WRAP_CLAMP);
    }

    return true;
}

void SectorPreviewDynamicLighting::UnloadShadowMapResources()
{
    for (RenderTexture2D& shadowMap : shadowMaps) {
        UnloadDepthOnlyRenderTexture(shadowMap);
    }
}

bool SectorPreviewDynamicLighting::HasShadowMapResources() const
{
    for (const RenderTexture2D& shadowMap : shadowMaps) {
        if (shadowMap.id != 0 || shadowMap.depth.id != 0) {
            return true;
        }
    }
    return false;
}

RenderTexture2D* SectorPreviewDynamicLighting::ShadowMap(std::size_t index)
{
    if (index >= shadowMaps.size()) {
        return nullptr;
    }
    return &shadowMaps[index];
}

const RenderTexture2D* SectorPreviewDynamicLighting::ShadowMap(std::size_t index) const
{
    if (index >= shadowMaps.size()) {
        return nullptr;
    }
    return &shadowMaps[index];
}

const Texture2D* SectorPreviewDynamicLighting::ShadowMapDepthTexture(std::size_t index) const
{
    const RenderTexture2D* shadowMap = ShadowMap(index);
    if (shadowMap == nullptr || shadowMap->depth.id == 0) {
        return nullptr;
    }
    return &shadowMap->depth;
}

SectorPreviewDynamicShadowMapTextures SectorPreviewDynamicLighting::BuildShadowMapTextures() const
{
    SectorPreviewDynamicShadowMapTextures textures;
    textures.shadowMap0 = ShadowMapDepthTexture(0);
    textures.shadowMap1 = ShadowMapDepthTexture(1);
    return textures;
}

void SectorPreviewDynamicLighting::RenderShadowMaps(
        const SectorPreviewDynamicSpotLightShadowRenderContext& context)
{
    if (context.assets == nullptr
            || context.material == nullptr
            || context.lightViewProjectionLoc < 0
            || context.sectorDrawRecords == nullptr
            || shadowMatrices.empty()) {
        return;
    }

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
                context.material->shader,
                context.lightViewProjectionLoc,
                matrix.lightViewProjection);
        for (const SectorMeshBatch& batch : *context.sectorDrawRecords) {
            const int alphaTest = batch.alphaTest ? 1 : 0;
            const float alphaCutoff = batch.alphaCutoff;
            const Texture2D* texture = nullptr;
            if (batch.alphaTest && context.textureResolver != nullptr) {
                texture = context.textureResolver(context.userData, *context.assets, batch.textureId);
            }
            context.material->maps[MATERIAL_MAP_DIFFUSE].texture = (texture != nullptr)
                    ? *texture
                    : context.defaultTexture;
            if (context.alphaTestLoc >= 0) {
                SetShaderValue(
                        context.material->shader,
                        context.alphaTestLoc,
                        &alphaTest,
                        SHADER_UNIFORM_INT);
            }
            if (context.alphaCutoffLoc >= 0) {
                SetShaderValue(
                        context.material->shader,
                        context.alphaCutoffLoc,
                        &alphaCutoff,
                        SHADER_UNIFORM_FLOAT);
            }
            DrawMesh(batch.mesh, *context.material, MatrixIdentity());
        }
        const int doorAlphaTest = 0;
        const float doorAlphaCutoff = 0.0f;
        context.material->maps[MATERIAL_MAP_DIFFUSE].texture = context.defaultTexture;
        if (context.alphaTestLoc >= 0) {
            SetShaderValue(
                    context.material->shader,
                    context.alphaTestLoc,
                    &doorAlphaTest,
                    SHADER_UNIFORM_INT);
        }
        if (context.alphaCutoffLoc >= 0) {
            SetShaderValue(
                    context.material->shader,
                    context.alphaCutoffLoc,
                    &doorAlphaCutoff,
                    SHADER_UNIFORM_FLOAT);
        }
        if (context.doorShadowCasters != nullptr && context.doorMeshResolver != nullptr) {
            for (const SectorDoorShadowCaster& caster : *context.doorShadowCasters) {
                float doorWidth = 0.0f;
                float doorHeight = 0.0f;
                const Mesh* doorMesh = context.doorMeshResolver(
                        context.userData,
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
                DrawMesh(*doorMesh, *context.material, shadowModel);
            }
        }
        context.material->maps[MATERIAL_MAP_DIFFUSE].texture = context.defaultTexture;
        rlDisableDepthTest();
        EndTextureMode();
    }
}

void SectorPreviewDynamicLighting::ReserveSelectionBuffers()
{
    candidates.clear();
    candidates.reserve(sources.size());
    selectedLights.clear();
    selectedLights.reserve(MaxDynamicLights);
    selectedLightIds.clear();
    selectedLightIds.reserve(MaxDynamicLights);
    shadowCasters.clear();
    shadowCasters.reserve(MaxDynamicSpotLightShadowCasters);
    shadowMatrices.clear();
    shadowMatrices.reserve(MaxDynamicSpotLightShadowCasters);
}

void SectorPreviewDynamicLighting::BuildReceiverBounds(
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld)
{
    receiverBounds.clear();
    receiverBounds.reserve(sectorReceiverBounds.size());
    receiverBounds.insert(receiverBounds.end(), sectorReceiverBounds.begin(), sectorReceiverBounds.end());
    if (runtimeObjectWorld != nullptr) {
        CollectSectorDoorReceiverBounds(*runtimeObjectWorld, receiverBounds);
    }
}

} // namespace game
