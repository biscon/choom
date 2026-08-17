#include "sector_demo/renderer/SectorFog.h"

#include "engine/render/ColorTransfer.h"

namespace game {

SectorFogRenderContext BuildSectorFogRenderContext(
        const SectorTopologyFogSettings& settings,
        Vector3 cameraPosition,
        bool volumetricHandoffEnabled,
        float volumetricMaximumDistanceWorld)
{
    return SectorFogRenderContext{
            NormalizeSectorTopologyFogSettings(settings),
            cameraPosition,
            volumetricHandoffEnabled,
            volumetricMaximumDistanceWorld
    };
}

SectorFogShaderLocations GetSectorFogShaderLocations(Shader shader)
{
    SectorFogShaderLocations locations;
    locations.enabled = GetShaderLocation(shader, "fogEnabled");
    locations.color = GetShaderLocation(shader, "fogColor");
    locations.cameraPosition = GetShaderLocation(shader, "fogCameraPosition");
    locations.startDistanceWorld = GetShaderLocation(shader, "fogStartDistanceWorld");
    locations.density = GetShaderLocation(shader, "fogDensity");
    locations.maxOpacity = GetShaderLocation(shader, "fogMaxOpacity");
    locations.referenceHeightWorld = GetShaderLocation(shader, "fogReferenceHeightWorld");
    locations.heightFalloff = GetShaderLocation(shader, "fogHeightFalloff");
    locations.volumetricHandoffEnabled = GetShaderLocation(
            shader, "fogVolumetricHandoffEnabled");
    locations.volumetricMaximumDistanceWorld = GetShaderLocation(
            shader, "fogVolumetricMaximumDistanceWorld");
    return locations;
}

void UploadSectorFogShaderValues(
        Shader shader,
        const SectorFogShaderLocations& locations,
        const SectorFogRenderContext& context)
{
    const SectorTopologyFogSettings settings =
            NormalizeSectorTopologyFogSettings(context.settings);
    const int enabled = settings.enabled ? 1 : 0;
    const Vector3 color = engine::SrgbColorBytesToLinearSceneRgb(settings.color);
    if (locations.enabled >= 0) {
        SetShaderValue(shader, locations.enabled, &enabled, SHADER_UNIFORM_INT);
    }
    if (locations.color >= 0) {
        SetShaderValue(shader, locations.color, &color, SHADER_UNIFORM_VEC3);
    }
    if (locations.cameraPosition >= 0) {
        SetShaderValue(
                shader,
                locations.cameraPosition,
                &context.cameraPosition,
                SHADER_UNIFORM_VEC3);
    }
    if (locations.startDistanceWorld >= 0) {
        SetShaderValue(
                shader,
                locations.startDistanceWorld,
                &settings.startDistanceWorld,
                SHADER_UNIFORM_FLOAT);
    }
    if (locations.density >= 0) {
        SetShaderValue(shader, locations.density, &settings.density, SHADER_UNIFORM_FLOAT);
    }
    if (locations.maxOpacity >= 0) {
        SetShaderValue(shader, locations.maxOpacity, &settings.maxOpacity, SHADER_UNIFORM_FLOAT);
    }
    if (locations.referenceHeightWorld >= 0) {
        SetShaderValue(
                shader,
                locations.referenceHeightWorld,
                &settings.referenceHeightWorld,
                SHADER_UNIFORM_FLOAT);
    }
    if (locations.heightFalloff >= 0) {
        SetShaderValue(
                shader,
                locations.heightFalloff,
                &settings.heightFalloff,
                SHADER_UNIFORM_FLOAT);
    }
    if (locations.volumetricHandoffEnabled >= 0) {
        const int enabled = context.volumetricHandoffEnabled ? 1 : 0;
        SetShaderValue(shader, locations.volumetricHandoffEnabled,
                &enabled, SHADER_UNIFORM_INT);
    }
    if (locations.volumetricMaximumDistanceWorld >= 0) {
        SetShaderValue(shader, locations.volumetricMaximumDistanceWorld,
                &context.volumetricMaximumDistanceWorld, SHADER_UNIFORM_FLOAT);
    }
}

} // namespace game
