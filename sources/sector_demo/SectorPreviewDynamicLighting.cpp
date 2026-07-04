#include "sector_demo/SectorPreviewDynamicLighting.h"

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>

namespace game {

namespace {

constexpr float DynamicLightingClamp = 4.0f;

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
