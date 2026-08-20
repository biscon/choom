#include "sector_editor/SectorEditorLightInspector.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <type_traits>

namespace game {

namespace {

bool SameVector3(Vector3 left, Vector3 right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

bool SameColor(Color left, Color right)
{
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

bool SameLightAtmosphere(
        const SectorLightAtmosphereSettings& left,
        const SectorLightAtmosphereSettings& right)
{
    const SectorLightAtmosphereSettings a = NormalizeSectorLightAtmosphereSettings(left);
    const SectorLightAtmosphereSettings b = NormalizeSectorLightAtmosphereSettings(right);
    return a.proxy.halo.enabled == b.proxy.halo.enabled
            && a.proxy.halo.radiusWorld == b.proxy.halo.radiusWorld
            && SameVector3(a.proxy.halo.centerOffsetWorld, b.proxy.halo.centerOffsetWorld)
            && a.proxy.halo.brightness == b.proxy.halo.brightness
            && a.proxy.halo.maxExtinction == b.proxy.halo.maxExtinction
            && a.proxy.halo.edgeSoftness == b.proxy.halo.edgeSoftness
            && SameColor(a.proxy.halo.scatteringTint, b.proxy.halo.scatteringTint)
            && a.proxy.shaft.enabled == b.proxy.shaft.enabled
            && SameVector3(a.proxy.shaft.originOffsetWorld, b.proxy.shaft.originOffsetWorld)
            && a.proxy.shaft.lengthScale == b.proxy.shaft.lengthScale
            && a.proxy.shaft.widthScale == b.proxy.shaft.widthScale
            && a.proxy.shaft.brightness == b.proxy.shaft.brightness
            && a.proxy.shaft.maxExtinction == b.proxy.shaft.maxExtinction
            && a.proxy.shaft.edgeSoftness == b.proxy.shaft.edgeSoftness
            && SameColor(a.proxy.shaft.scatteringTint, b.proxy.shaft.scatteringTint)
            && a.dust.enabled == b.dust.enabled
            && a.dust.amount == b.dust.amount
            && a.dust.extentScale == b.dust.extentScale
            && a.dust.minimumSizeWorld == b.dust.minimumSizeWorld
            && a.dust.maximumSizeWorld == b.dust.maximumSizeWorld
            && a.dust.opacity == b.dust.opacity
            && a.dust.driftSpeedWorld == b.dust.driftSpeedWorld
            && a.dust.turbulenceWorld == b.dust.turbulenceWorld
            && SameColor(a.dust.scatteringTint, b.dust.scatteringTint);
}

bool AtmosphereSourceChanged(
        const SectorTopologyStaticPointLight& before,
        const SectorTopologyStaticPointLight& after)
{
    return !SameVector3(before.position, after.position)
            || before.radius != after.radius
            || before.intensity != after.intensity
            || !SameColor(before.color, after.color)
            || !SameLightAtmosphere(before.atmosphere, after.atmosphere);
}

bool AtmosphereSourceChanged(
        const SectorTopologyStaticSpotLight& before,
        const SectorTopologyStaticSpotLight& after)
{
    return !SameVector3(before.position, after.position)
            || !SameVector3(before.target, after.target)
            || before.range != after.range
            || before.outerConeDegrees != after.outerConeDegrees
            || before.intensity != after.intensity
            || !SameColor(before.color, after.color)
            || !SameLightAtmosphere(before.atmosphere, after.atmosphere);
}

bool AtmosphereSourceChanged(
        const SectorTopologyDynamicPointLight& before,
        const SectorTopologyDynamicPointLight& after)
{
    return before.enabled != after.enabled
            || before.flicker != after.flicker
            || before.flickerSpeed != after.flickerSpeed
            || before.flickerAmount != after.flickerAmount
            || !SameVector3(before.position, after.position)
            || before.intensity != after.intensity
            || before.radius != after.radius
            || before.castsShadow != after.castsShadow
            || before.shadowPriority != after.shadowPriority
            || before.shadowBias != after.shadowBias
            || before.shadowStrength != after.shadowStrength
            || before.shadowSoftness != after.shadowSoftness
            || !SameColor(before.color, after.color)
            || !SameLightAtmosphere(before.atmosphere, after.atmosphere);
}

bool AtmosphereSourceChanged(
        const SectorTopologyDynamicSpotLight& before,
        const SectorTopologyDynamicSpotLight& after)
{
    return before.enabled != after.enabled
            || before.flicker != after.flicker
            || before.flickerSpeed != after.flickerSpeed
            || before.flickerAmount != after.flickerAmount
            || !SameVector3(before.position, after.position)
            || !SameVector3(before.target, after.target)
            || before.intensity != after.intensity
            || before.range != after.range
            || before.innerConeDegrees != after.innerConeDegrees
            || before.outerConeDegrees != after.outerConeDegrees
            || before.castsShadow != after.castsShadow
            || before.shadowPriority != after.shadowPriority
            || before.shadowBias != after.shadowBias
            || before.shadowStrength != after.shadowStrength
            || before.shadowSoftness != after.shadowSoftness
            || !SameColor(before.color, after.color)
            || !SameLightAtmosphere(before.atmosphere, after.atmosphere);
}

float LightAtmosphereInspectorContentHeight(
        float rowH,
        float gap,
        const SectorLightAtmosphereSettings& atmosphere,
        bool spotLight)
{
    float height = 2.0f * 26.0f + 2.0f * (rowH + gap);
    if (atmosphere.proxy.halo.enabled) height += 11.0f * (rowH + gap);
    if (spotLight) {
        height += rowH + gap;
        if (atmosphere.proxy.shaft.enabled) height += 12.0f * (rowH + gap);
    }
    if (atmosphere.dust.enabled) height += 10.0f * (rowH + gap);
    return height;
}

template<typename ApplyFn>
void DrawLightAtmosphereInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        float contentW,
        float rowH,
        float gap,
        float& y,
        SectorLightAtmosphereSettings atmosphere,
        SectorEditorUiState& uiState,
        ApplyFn&& apply,
        bool& sourceRefreshRequested,
        bool spotLight)
{
    auto commit = [&]() {
        atmosphere = NormalizeSectorLightAtmosphereSettings(atmosphere);
        if (apply(atmosphere)) sourceRefreshRequested = true;
    };
    auto drawFloat = [&](const char* id,
                         const char* label,
                         float& value,
                         engine::UIFloatInputState& state,
                         float minimum,
                         float maximum,
                         int decimals) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                value,
                state,
                minimum,
                maximum,
                decimals);
        if (result.changed && result.value != value) {
            value = result.value;
            commit();
        }
        y += rowH + gap;
    };
    auto drawChannel = [&](const char* id,
                           const char* label,
                           unsigned char& channel,
                           engine::UIIntInputState& state) {
        const float labelWidth = 126.0f;
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, labelWidth, rowH},
                Rectangle{labelWidth, y, contentW - labelWidth, rowH},
                engine::UITextJustify::Right,
                channel,
                state);
        if (result.changed && result.channel != channel) {
            channel = result.channel;
            commit();
        }
        y += rowH + gap;
    };

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 22.0f}, font,
            "Atmosphere", engine::UITextJustify::Left, config.textColor);
    y += 26.0f;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_light_proxy_halo_enabled",
            Rectangle{0.0f, y, contentW, rowH}, font, "Haze enabled", atmosphere.proxy.halo.enabled)) {
        commit();
    }
    y += rowH + gap;
    if (atmosphere.proxy.halo.enabled) {
        drawFloat("sector_editor_light_proxy_halo_radius", "Haze radius (m):",
                atmosphere.proxy.halo.radiusWorld, uiState.lightProxyHaloRadiusInput, 0.01f, 64.0f, 3);
        drawFloat("sector_editor_light_proxy_halo_offset_x", "Haze offset X (m):",
                atmosphere.proxy.halo.centerOffsetWorld.x,
                uiState.lightProxyHaloOffsetXInput, -100000.0f, 100000.0f, 3);
        drawFloat("sector_editor_light_proxy_halo_offset_y", "Haze offset Y (m):",
                atmosphere.proxy.halo.centerOffsetWorld.y,
                uiState.lightProxyHaloOffsetYInput, -100000.0f, 100000.0f, 3);
        drawFloat("sector_editor_light_proxy_halo_offset_z", "Haze offset Z (m):",
                atmosphere.proxy.halo.centerOffsetWorld.z,
                uiState.lightProxyHaloOffsetZInput, -100000.0f, 100000.0f, 3);
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_light_proxy_halo_offset_reset",
                    Rectangle{0.0f, y, contentW, rowH}, font, "Reset haze offset")) {
            atmosphere.proxy.halo.centerOffsetWorld = {};
            commit();
        }
        y += rowH + gap;
        drawFloat("sector_editor_light_proxy_halo_brightness", "Haze brightness:",
                atmosphere.proxy.halo.brightness, uiState.lightProxyHaloBrightnessInput, 0.0f, 16.0f, 3);
        drawFloat("sector_editor_light_proxy_halo_max_extinction", "Maximum extinction:",
                atmosphere.proxy.halo.maxExtinction,
                uiState.lightProxyHaloMaxExtinctionInput, 0.0f, 1.0f, 3);
        drawFloat("sector_editor_light_proxy_halo_softness", "Haze softness:",
                atmosphere.proxy.halo.edgeSoftness, uiState.lightProxyHaloSoftnessInput, 0.01f, 1.0f, 3);
        drawChannel("sector_editor_light_proxy_halo_r", "Haze tint R:",
                atmosphere.proxy.halo.scatteringTint.r, uiState.lightProxyHaloRedInput);
        drawChannel("sector_editor_light_proxy_halo_g", "Haze tint G:",
                atmosphere.proxy.halo.scatteringTint.g, uiState.lightProxyHaloGreenInput);
        drawChannel("sector_editor_light_proxy_halo_b", "Haze tint B:",
                atmosphere.proxy.halo.scatteringTint.b, uiState.lightProxyHaloBlueInput);
    }
    if (spotLight) {
        if (engine::Checkbox(ui, config, input, assets, "sector_editor_light_proxy_shaft_enabled",
                Rectangle{0.0f, y, contentW, rowH}, font, "Shaft enabled", atmosphere.proxy.shaft.enabled)) {
            commit();
        }
        y += rowH + gap;
        if (atmosphere.proxy.shaft.enabled) {
            drawFloat("sector_editor_light_proxy_shaft_offset_x", "Shaft offset X (m):",
                    atmosphere.proxy.shaft.originOffsetWorld.x,
                    uiState.lightProxyShaftOffsetXInput, -100000.0f, 100000.0f, 3);
            drawFloat("sector_editor_light_proxy_shaft_offset_y", "Shaft offset Y (m):",
                    atmosphere.proxy.shaft.originOffsetWorld.y,
                    uiState.lightProxyShaftOffsetYInput, -100000.0f, 100000.0f, 3);
            drawFloat("sector_editor_light_proxy_shaft_offset_z", "Shaft offset Z (m):",
                    atmosphere.proxy.shaft.originOffsetWorld.z,
                    uiState.lightProxyShaftOffsetZInput, -100000.0f, 100000.0f, 3);
            if (engine::Button(ui, config, input, assets,
                        "sector_editor_light_proxy_shaft_offset_reset",
                        Rectangle{0.0f, y, contentW, rowH}, font, "Reset shaft offset")) {
                atmosphere.proxy.shaft.originOffsetWorld = {};
                commit();
            }
            y += rowH + gap;
            drawFloat("sector_editor_light_proxy_shaft_length", "Shaft length scale:",
                    atmosphere.proxy.shaft.lengthScale, uiState.lightProxyShaftLengthInput, 0.01f, 2.0f, 3);
            drawFloat("sector_editor_light_proxy_shaft_width", "Shaft width scale:",
                    atmosphere.proxy.shaft.widthScale, uiState.lightProxyShaftWidthInput, 0.01f, 2.0f, 3);
            drawFloat("sector_editor_light_proxy_shaft_brightness", "Shaft brightness:",
                    atmosphere.proxy.shaft.brightness, uiState.lightProxyShaftBrightnessInput, 0.0f, 16.0f, 3);
            drawFloat("sector_editor_light_proxy_shaft_max_extinction", "Maximum extinction:",
                    atmosphere.proxy.shaft.maxExtinction,
                    uiState.lightProxyShaftMaxExtinctionInput, 0.0f, 1.0f, 3);
            drawFloat("sector_editor_light_proxy_shaft_softness", "Shaft softness:",
                    atmosphere.proxy.shaft.edgeSoftness, uiState.lightProxyShaftSoftnessInput, 0.01f, 1.0f, 3);
            drawChannel("sector_editor_light_proxy_shaft_r", "Shaft tint R:",
                    atmosphere.proxy.shaft.scatteringTint.r, uiState.lightProxyShaftRedInput);
            drawChannel("sector_editor_light_proxy_shaft_g", "Shaft tint G:",
                    atmosphere.proxy.shaft.scatteringTint.g, uiState.lightProxyShaftGreenInput);
            drawChannel("sector_editor_light_proxy_shaft_b", "Shaft tint B:",
                    atmosphere.proxy.shaft.scatteringTint.b, uiState.lightProxyShaftBlueInput);
        }
    }

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 22.0f}, font,
            "Atmosphere: Dust", engine::UITextJustify::Left, config.textColor);
    y += 26.0f;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_light_dust_enabled",
            Rectangle{0.0f, y, contentW, rowH}, font, "Dust enabled", atmosphere.dust.enabled)) {
        commit();
    }
    y += rowH + gap;
    if (atmosphere.dust.enabled) {
        const SectorEditorInspectorNumericRowLayout amountLayout =
                BuildSectorEditorInspectorRightIntRowLayout(y, contentW, rowH, gap);
        engine::Text(ui, config, assets, amountLayout.labelRect, font, "Amount:",
                engine::UITextJustify::Right, config.mutedTextColor);
        const engine::UINumericInputResult amountResult = engine::IntInput(
                ui, config, input, assets, "sector_editor_light_dust_amount", amountLayout.inputRect,
                font, atmosphere.dust.amount, uiState.lightDustAmountInput, 0, 128, 1);
        if (amountResult.changed) commit();
        y += rowH + gap;
        drawFloat("sector_editor_light_dust_extent", "Extent scale:", atmosphere.dust.extentScale,
                uiState.lightDustExtentScaleInput, 0.05f, 2.0f, 3);
        drawFloat("sector_editor_light_dust_min_size", "Min size (m):", atmosphere.dust.minimumSizeWorld,
                uiState.lightDustMinimumSizeInput, 0.002f, 0.25f, 3);
        drawFloat("sector_editor_light_dust_max_size", "Max size (m):", atmosphere.dust.maximumSizeWorld,
                uiState.lightDustMaximumSizeInput, 0.002f, 0.25f, 3);
        drawFloat("sector_editor_light_dust_opacity", "Opacity:", atmosphere.dust.opacity,
                uiState.lightDustOpacityInput, 0.0f, 1.0f, 3);
        drawFloat("sector_editor_light_dust_drift", "Drift (m/s):", atmosphere.dust.driftSpeedWorld,
                uiState.lightDustDriftSpeedInput, 0.0f, 0.5f, 3);
        drawFloat("sector_editor_light_dust_turbulence", "Turbulence:", atmosphere.dust.turbulenceWorld,
                uiState.lightDustTurbulenceInput, 0.0f, 0.5f, 3);
        drawChannel("sector_editor_light_dust_r", "Tint R:", atmosphere.dust.scatteringTint.r, uiState.lightDustRedInput);
        drawChannel("sector_editor_light_dust_g", "Tint G:", atmosphere.dust.scatteringTint.g, uiState.lightDustGreenInput);
        drawChannel("sector_editor_light_dust_b", "Tint B:", atmosphere.dust.scatteringTint.b, uiState.lightDustBlueInput);
    }
}

} // namespace

float StaticLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere)
{
    float height = 38.0f; // Light title.
    height += rowH + gap; // Id.
    if (hasIdError) {
        height += 36.0f;
    }
    height += rowH + gap; // Delete.
    height += rowH + gap; // Shadow.
    height += 6.0f * (rowH + gap); // Position/intensity/radius/source radius.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Swatch.
    height += LightAtmosphereInspectorContentHeight(rowH, gap, atmosphere, false);
    height += rowH + gap; // Bake.
    return height;
}

float StaticSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere)
{
    float height = 38.0f; // Light title.
    height += rowH + gap; // Id.
    if (hasIdError) {
        height += 36.0f;
    }
    height += rowH + gap; // Delete.
    height += rowH + gap; // Shadow.
    height += 12.0f * (rowH + gap); // Position/target/point down/intensity/range/source/cones.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Swatch.
    height += LightAtmosphereInspectorContentHeight(rowH, gap, atmosphere, true);
    height += rowH + gap; // Bake.
    return height;
}

float DynamicLightInspectorContentHeight(float rowH, float gap, bool hasIdError, const SectorLightAtmosphereSettings& atmosphere)
{
    float height = 38.0f; // Light title.
    height += rowH + gap; // Id.
    if (hasIdError) {
        height += 36.0f;
    }
    height += rowH + gap; // Delete.
    height += rowH + gap; // Enabled.
    height += 3.0f * (rowH + gap); // Flicker controls.
    height += 5.0f * (rowH + gap); // Shadow controls.
    height += 5.0f * (rowH + gap); // Position/intensity/radius.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Swatch.
    height += LightAtmosphereInspectorContentHeight(rowH, gap, atmosphere, false);
    return height;
}

float DynamicSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError, float shadowNoteHeight, const SectorLightAtmosphereSettings& atmosphere)
{
    float height = 38.0f; // Light title.
    height += rowH + gap; // Id.
    if (hasIdError) {
        height += 36.0f;
    }
    height += rowH + gap; // Delete.
    height += rowH + gap; // Enabled.
    height += 3.0f * (rowH + gap); // Flicker controls.
    height += 5.0f * (rowH + gap); // Shadow controls.
    height += shadowNoteHeight + gap; // Shadow budget note.
    height += 11.0f * (rowH + gap); // Position/target/point down/intensity/range/cones.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Swatch.
    height += LightAtmosphereInspectorContentHeight(rowH, gap, atmosphere, true);
    return height;
}

bool DrawSelectedStaticLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyStaticPointLight& light,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested,
        bool& bakeRequested,
        bool& sourceRefreshRequested)
{
    const SectorTopologyStaticPointLight sourceBefore = light;
    float y = 0.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Static Light: %d", light.id), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 88.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;

    if (!inspectorIdUiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, inspectorIdUiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
        deleteRequested = true;
        return true;
    }
    y += rowH + gap;

    bool castsShadow = light.castsShadow;
    if (engine::Checkbox(
                ui,
                config,
                input,
                assets,
                "sector_editor_static_light_casts_shadow",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Shadow",
                castsShadow)
            && castsShadow != light.castsShadow
            && lightEditing.SetStaticLightCastsShadow(light, castsShadow)) {
        sourceRefreshRequested = true;
    }
    y += rowH + gap;

    enum class StaticLightFloatField {
        PositionX,
        PositionY,
        PositionZ,
        Intensity,
        Radius
    };
    auto drawLightFloat = [&](const char* id, const char* label, float value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals, StaticLightFloatField field) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            if (field == StaticLightFloatField::Intensity) {
                lightEditing.SetStaticLightIntensity(light, result.value);
            } else if (field == StaticLightFloatField::Radius) {
                lightEditing.SetStaticLightRadius(light, result.value);
            } else {
                Vector3 position = light.position;
                if (field == StaticLightFloatField::PositionX) {
                    position.x = result.value;
                } else if (field == StaticLightFloatField::PositionY) {
                    position.y = result.value;
                } else {
                    position.z = result.value;
                }
                lightEditing.SetStaticLightPosition(light, position);
            }
        }
        y += rowH + gap;
    };

    drawLightFloat("sector_editor_light_x", "X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2, StaticLightFloatField::PositionX);
    drawLightFloat("sector_editor_light_y", "Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2, StaticLightFloatField::PositionY);
    drawLightFloat("sector_editor_light_z", "Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2, StaticLightFloatField::PositionZ);
    drawLightFloat("sector_editor_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3, StaticLightFloatField::Intensity);
    drawLightFloat("sector_editor_light_radius", "Radius:", light.radius, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2, StaticLightFloatField::Radius);
    {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_light_source_radius",
                "Source:",
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                light.sourceRadius,
                uiState.lightSourceRadiusInput,
                0.0f,
                SectorWorldToAuthoringDistance(8.0f),
                3);
        const float edited = ClampLightSourceRadius(result.value, light.radius);
        if (result.changed && edited != light.sourceRadius) {
            lightEditing.SetStaticLightSourceRadius(light, result.value);
        }
        y += rowH + gap;
    }

    auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const float colorLabelW = 92.0f;
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, colorLabelW, rowH},
                Rectangle{colorLabelW, y, contentW - colorLabelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            Color color = light.color;
            if (&channel == &light.color.r) {
                color.r = result.channel;
            } else if (&channel == &light.color.g) {
                color.g = result.channel;
            } else {
                color.b = result.channel;
            }
            lightEditing.SetStaticLightColor(light, color);
        }
        y += rowH + gap;
    };
    drawLightChannel("sector_editor_light_r", "R:", light.color.r, uiState.lightRedInput);
    drawLightChannel("sector_editor_light_g", "G:", light.color.g, uiState.lightGreenInput);
    drawLightChannel("sector_editor_light_b", "B:", light.color.b, uiState.lightBlueInput);

    const float swatchW = std::min(120.0f, contentW);
    const Rectangle swatch{
            scroll.viewport.x + std::max(0.0f, contentW - swatchW),
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            swatchW,
            28.0f
    };
    DrawColorSwatch(config, swatch, light.color, 1.0f);
    y += 36.0f + gap;

    DrawLightAtmosphereInspector(
            ui, config, input, assets, font, contentW, rowH, gap, y,
            light.atmosphere, uiState,
            [&lightEditing, &light](SectorLightAtmosphereSettings settings) {
                return lightEditing.SetStaticLightAtmosphere(light, settings);
            },
            sourceRefreshRequested, false);

    if (engine::Button(ui, config, input, assets, "sector_editor_light_bake", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        bakeRequested = true;
    }

    sourceRefreshRequested = sourceRefreshRequested || AtmosphereSourceChanged(sourceBefore, light);
    return true;
}

bool DrawSelectedStaticSpotLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyStaticSpotLight& light,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested,
        bool& bakeRequested,
        bool& sourceRefreshRequested)
{
    const SectorTopologyStaticSpotLight sourceBefore = light;
    float y = 0.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Static Spot: %d", light.id), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 88.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;

    if (!inspectorIdUiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, inspectorIdUiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_static_spot_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
        deleteRequested = true;
        return true;
    }
    y += rowH + gap;

    bool castsShadow = light.castsShadow;
    if (engine::Checkbox(
                ui,
                config,
                input,
                assets,
                "sector_editor_static_spot_light_casts_shadow",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Shadow",
                castsShadow)
            && castsShadow != light.castsShadow
            && lightEditing.SetStaticSpotLightCastsShadow(light, castsShadow)) {
        sourceRefreshRequested = true;
    }
    y += rowH + gap;

    enum class StaticSpotFloatField {
        PositionX,
        PositionY,
        PositionZ,
        TargetX,
        TargetY,
        TargetZ,
        Range,
        InnerCone,
        OuterCone,
        Intensity
    };
    auto drawLightFloat = [&](const char* id, const char* label, float value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals, StaticSpotFloatField field) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            if (field == StaticSpotFloatField::Range) {
                lightEditing.SetStaticSpotLightRange(light, result.value);
            } else if (field == StaticSpotFloatField::InnerCone) {
                lightEditing.SetStaticSpotLightInnerCone(light, result.value);
            } else if (field == StaticSpotFloatField::OuterCone) {
                lightEditing.SetStaticSpotLightOuterCone(light, result.value);
            } else if (field == StaticSpotFloatField::Intensity) {
                lightEditing.SetStaticSpotLightIntensity(light, result.value);
            } else if (field == StaticSpotFloatField::TargetX
                    || field == StaticSpotFloatField::TargetY
                    || field == StaticSpotFloatField::TargetZ) {
                Vector3 target = light.target;
                if (field == StaticSpotFloatField::TargetX) {
                    target.x = result.value;
                } else if (field == StaticSpotFloatField::TargetY) {
                    target.y = result.value;
                } else {
                    target.z = result.value;
                }
                lightEditing.SetStaticSpotLightTarget(light, target);
            } else {
                Vector3 position = light.position;
                if (field == StaticSpotFloatField::PositionX) {
                    position.x = result.value;
                } else if (field == StaticSpotFloatField::PositionY) {
                    position.y = result.value;
                } else {
                    position.z = result.value;
                }
                lightEditing.SetStaticSpotLightPosition(light, position);
            }
        }
        y += rowH + gap;
    };

    drawLightFloat("sector_editor_static_spot_light_x", "Position X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2, StaticSpotFloatField::PositionX);
    drawLightFloat("sector_editor_static_spot_light_y", "Position Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2, StaticSpotFloatField::PositionY);
    drawLightFloat("sector_editor_static_spot_light_z", "Position Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2, StaticSpotFloatField::PositionZ);
    drawLightFloat("sector_editor_static_spot_light_target_x", "Target X:", light.target.x, uiState.lightTargetXInput, -8192.0f, 8192.0f, 2, StaticSpotFloatField::TargetX);
    drawLightFloat("sector_editor_static_spot_light_target_y", "Target Y:", light.target.y, uiState.lightTargetYInput, -512.0f, 512.0f, 2, StaticSpotFloatField::TargetY);
    drawLightFloat("sector_editor_static_spot_light_target_z", "Target Z:", light.target.z, uiState.lightTargetZInput, -8192.0f, 8192.0f, 2, StaticSpotFloatField::TargetZ);
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_static_spot_light_point_down",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Point Down")) {
        lightEditing.PointStaticSpotLightDown(light);
    }
    y += rowH + gap;
    drawLightFloat("sector_editor_static_spot_light_range", "Radius:", light.range, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2, StaticSpotFloatField::Range);
    {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_static_spot_light_source_radius",
                "Source radius:",
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                light.sourceRadius,
                uiState.lightSourceRadiusInput,
                0.0f,
                SectorWorldToAuthoringDistance(8.0f),
                3);
        const float edited = ClampLightSourceRadius(result.value, light.range);
        if (result.changed && edited != light.sourceRadius) {
            lightEditing.SetStaticSpotLightSourceRadius(light, result.value);
        }
        y += rowH + gap;
    }
    drawLightFloat("sector_editor_static_spot_light_inner_cone", "Inner cone:", light.innerConeDegrees, uiState.lightInnerConeInput, 0.0f, 179.0f, 2, StaticSpotFloatField::InnerCone);
    drawLightFloat("sector_editor_static_spot_light_outer_cone", "Outer cone:", light.outerConeDegrees, uiState.lightOuterConeInput, 0.0f, 179.0f, 2, StaticSpotFloatField::OuterCone);
    drawLightFloat("sector_editor_static_spot_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3, StaticSpotFloatField::Intensity);

    auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const float colorLabelW = 126.0f;
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, colorLabelW, rowH},
                Rectangle{colorLabelW, y, contentW - colorLabelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            Color color = light.color;
            if (&channel == &light.color.r) {
                color.r = result.channel;
            } else if (&channel == &light.color.g) {
                color.g = result.channel;
            } else {
                color.b = result.channel;
            }
            lightEditing.SetStaticSpotLightColor(light, color);
        }
        y += rowH + gap;
    };
    drawLightChannel("sector_editor_static_spot_light_r", "R:", light.color.r, uiState.lightRedInput);
    drawLightChannel("sector_editor_static_spot_light_g", "G:", light.color.g, uiState.lightGreenInput);
    drawLightChannel("sector_editor_static_spot_light_b", "B:", light.color.b, uiState.lightBlueInput);

    const float swatchW = std::min(120.0f, contentW);
    const Rectangle swatch{
            scroll.viewport.x + std::max(0.0f, contentW - swatchW),
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            swatchW,
            28.0f
    };
    DrawColorSwatch(config, swatch, light.color, 1.0f);
    y += 36.0f + gap;

    DrawLightAtmosphereInspector(
            ui, config, input, assets, font, contentW, rowH, gap, y,
            light.atmosphere, uiState,
            [&lightEditing, &light](SectorLightAtmosphereSettings settings) {
                return lightEditing.SetStaticSpotLightAtmosphere(light, settings);
            },
            sourceRefreshRequested, true);

    if (engine::Button(ui, config, input, assets, "sector_editor_static_spot_light_bake", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        bakeRequested = true;
    }

    sourceRefreshRequested = sourceRefreshRequested || AtmosphereSourceChanged(sourceBefore, light);
    return true;
}

bool DrawSelectedDynamicLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyDynamicPointLight& light,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested,
        bool& sourceRefreshRequested)
{
    const SectorTopologyDynamicPointLight sourceBefore = light;
    float y = 0.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Dynamic Light: %d", light.id), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 88.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;

    if (!inspectorIdUiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, inspectorIdUiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_dynamic_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
        deleteRequested = true;
        return true;
    }
    y += rowH + gap;

    bool enabled = light.enabled;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_light_enabled", Rectangle{0.0f, y, contentW, rowH}, font, "Enabled", enabled)
            && enabled != light.enabled) {
        lightEditing.SetDynamicLightEnabled(light, enabled);
    }
    y += rowH + gap;

    bool flicker = light.flicker;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_light_flicker", Rectangle{0.0f, y, contentW, rowH}, font, "Flicker", flicker)
            && flicker != light.flicker) {
        lightEditing.SetDynamicLightFlicker(light, flicker);
    }
    y += rowH + gap;

    enum class DynamicLightFloatField {
        FlickerSpeed,
        FlickerAmount,
        PositionX,
        PositionY,
        PositionZ,
        Intensity,
        Radius
    };
    auto drawLightFloat = [&](const char* id, const char* label, float value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals, DynamicLightFloatField field) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            if (field == DynamicLightFloatField::FlickerSpeed) {
                lightEditing.SetDynamicLightFlickerSpeed(light, result.value);
            } else if (field == DynamicLightFloatField::FlickerAmount) {
                lightEditing.SetDynamicLightFlickerAmount(light, result.value);
            } else if (field == DynamicLightFloatField::Intensity) {
                lightEditing.SetDynamicLightIntensity(light, result.value);
            } else if (field == DynamicLightFloatField::Radius) {
                lightEditing.SetDynamicLightRadius(light, result.value);
            } else {
                Vector3 position = light.position;
                if (field == DynamicLightFloatField::PositionX) {
                    position.x = result.value;
                } else if (field == DynamicLightFloatField::PositionY) {
                    position.y = result.value;
                } else {
                    position.z = result.value;
                }
                lightEditing.SetDynamicLightPosition(light, position);
            }
        }
        y += rowH + gap;
    };

    drawLightFloat(
            "sector_editor_dynamic_light_flicker_speed",
            "Flicker speed:",
            light.flickerSpeed,
            uiState.lightFlickerSpeedInput,
            DynamicLightFlickerMinSpeed,
            DynamicLightFlickerMaxSpeed,
            3,
            DynamicLightFloatField::FlickerSpeed);
    drawLightFloat(
            "sector_editor_dynamic_light_flicker_amount",
            "Flicker amount:",
            light.flickerAmount,
            uiState.lightFlickerAmountInput,
            DynamicLightFlickerMinAmount,
            DynamicLightFlickerMaxAmount,
            3,
            DynamicLightFloatField::FlickerAmount);

    bool castsShadow = light.castsShadow;
    if (engine::Checkbox(ui, config, input, assets,
                "sector_editor_dynamic_light_casts_shadow",
                Rectangle{0.0f, y, contentW, rowH}, font,
                "Cast Shadows (2 atlas slots)", castsShadow)
            && castsShadow != light.castsShadow) {
        lightEditing.SetDynamicLightCastsShadow(light, castsShadow);
    }
    y += rowH + gap;
    {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightIntRowLayout(y, contentW, rowH, gap);
        const SectorEditorIntInputResult result = DrawLabeledIntInput(
                ui, config, input, assets, font,
                "sector_editor_dynamic_light_shadow_priority",
                "Shadow priority:", layout.labelRect, layout.inputRect,
                engine::UITextJustify::Right, light.shadowPriority,
                uiState.lightShadowPriorityInput,
                DynamicSpotLightMinShadowPriority,
                DynamicSpotLightMaxShadowPriority, 1);
        if (result.changed) {
            lightEditing.SetDynamicLightShadowPriority(light, result.value);
        }
        y += rowH + gap;
    }
    auto drawShadowFloat = [&](const char* id, const char* label, float value,
            engine::UIFloatInputState& state, float minimum, float maximum,
            int decimals, int field) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui, config, input, assets, font, id, label,
                layout.labelRect, layout.inputRect, engine::UITextJustify::Right,
                value, state, minimum, maximum, decimals);
        if (result.changed) {
            if (field == 0) lightEditing.SetDynamicLightShadowBias(light, result.value);
            else if (field == 1) lightEditing.SetDynamicLightShadowStrength(light, result.value);
            else lightEditing.SetDynamicLightShadowSoftness(light, result.value);
        }
        y += rowH + gap;
    };
    drawShadowFloat("sector_editor_dynamic_light_shadow_bias", "Shadow bias:",
            light.shadowBias, uiState.lightShadowBiasInput,
            DynamicSpotLightMinShadowBias, DynamicSpotLightMaxShadowBias, 5, 0);
    drawShadowFloat("sector_editor_dynamic_light_shadow_strength", "Shadow strength:",
            light.shadowStrength, uiState.lightShadowStrengthInput,
            DynamicSpotLightMinShadowStrength, DynamicSpotLightMaxShadowStrength, 3, 1);
    drawShadowFloat("sector_editor_dynamic_light_shadow_softness", "Softness:",
            light.shadowSoftness, uiState.lightShadowSoftnessInput,
            DynamicSpotLightMinShadowSoftness, DynamicSpotLightMaxShadowSoftness, 3, 2);

    drawLightFloat("sector_editor_dynamic_light_x", "X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2, DynamicLightFloatField::PositionX);
    drawLightFloat("sector_editor_dynamic_light_y", "Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2, DynamicLightFloatField::PositionY);
    drawLightFloat("sector_editor_dynamic_light_z", "Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2, DynamicLightFloatField::PositionZ);
    drawLightFloat("sector_editor_dynamic_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3, DynamicLightFloatField::Intensity);
    drawLightFloat("sector_editor_dynamic_light_radius", "Radius:", light.radius, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2, DynamicLightFloatField::Radius);

    auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const float colorLabelW = 116.0f;
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, colorLabelW, rowH},
                Rectangle{colorLabelW, y, contentW - colorLabelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            Color color = light.color;
            if (&channel == &light.color.r) {
                color.r = result.channel;
            } else if (&channel == &light.color.g) {
                color.g = result.channel;
            } else {
                color.b = result.channel;
            }
            lightEditing.SetDynamicLightColor(light, color);
        }
        y += rowH + gap;
    };
    drawLightChannel("sector_editor_dynamic_light_r", "R:", light.color.r, uiState.lightRedInput);
    drawLightChannel("sector_editor_dynamic_light_g", "G:", light.color.g, uiState.lightGreenInput);
    drawLightChannel("sector_editor_dynamic_light_b", "B:", light.color.b, uiState.lightBlueInput);

    const float swatchW = std::min(120.0f, contentW);
    const Rectangle swatch{
            scroll.viewport.x + std::max(0.0f, contentW - swatchW),
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            swatchW,
            28.0f
    };
    DrawColorSwatch(config, swatch, light.color, light.enabled ? 1.0f : 0.45f);
    y += 36.0f + gap;

    DrawLightAtmosphereInspector(
            ui, config, input, assets, font, contentW, rowH, gap, y,
            light.atmosphere, uiState,
            [&lightEditing, &light](SectorLightAtmosphereSettings settings) {
                return lightEditing.SetDynamicLightAtmosphere(light, settings);
            },
            sourceRefreshRequested, false);

    sourceRefreshRequested = sourceRefreshRequested || AtmosphereSourceChanged(sourceBefore, light);
    return true;
}

bool DrawSelectedDynamicSpotLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyDynamicSpotLight& light,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested,
        bool& sourceRefreshRequested)
{
    const SectorTopologyDynamicSpotLight sourceBefore = light;
    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);
    float y = 0.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Dynamic Spot: %d", light.id), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 88.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;

    if (!inspectorIdUiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, inspectorIdUiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_dynamic_spot_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
        deleteRequested = true;
        return true;
    }
    y += rowH + gap;

    bool enabled = light.enabled;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_spot_light_enabled", Rectangle{0.0f, y, contentW, rowH}, font, "Enabled", enabled)
            && enabled != light.enabled) {
        lightEditing.SetDynamicSpotLightEnabled(light, enabled);
    }
    y += rowH + gap;

    bool flicker = light.flicker;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_spot_light_flicker", Rectangle{0.0f, y, contentW, rowH}, font, "Flicker", flicker)
            && flicker != light.flicker) {
        lightEditing.SetDynamicSpotLightFlicker(light, flicker);
    }
    y += rowH + gap;

    enum class DynamicSpotFloatField {
        FlickerSpeed,
        FlickerAmount,
        PositionX,
        PositionY,
        PositionZ,
        TargetX,
        TargetY,
        TargetZ,
        Intensity,
        Range,
        InnerCone,
        OuterCone
    };
    auto drawLightFloat = [&](const char* id, const char* label, float value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals, DynamicSpotFloatField field) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            if (field == DynamicSpotFloatField::FlickerSpeed) {
                lightEditing.SetDynamicSpotLightFlickerSpeed(light, result.value);
            } else if (field == DynamicSpotFloatField::FlickerAmount) {
                lightEditing.SetDynamicSpotLightFlickerAmount(light, result.value);
            } else if (field == DynamicSpotFloatField::Intensity) {
                lightEditing.SetDynamicSpotLightIntensity(light, result.value);
            } else if (field == DynamicSpotFloatField::Range) {
                lightEditing.SetDynamicSpotLightRange(light, result.value);
            } else if (field == DynamicSpotFloatField::InnerCone) {
                lightEditing.SetDynamicSpotLightInnerCone(light, result.value);
            } else if (field == DynamicSpotFloatField::OuterCone) {
                lightEditing.SetDynamicSpotLightOuterCone(light, result.value);
            } else if (field == DynamicSpotFloatField::TargetX
                    || field == DynamicSpotFloatField::TargetY
                    || field == DynamicSpotFloatField::TargetZ) {
                Vector3 target = light.target;
                if (field == DynamicSpotFloatField::TargetX) {
                    target.x = result.value;
                } else if (field == DynamicSpotFloatField::TargetY) {
                    target.y = result.value;
                } else {
                    target.z = result.value;
                }
                lightEditing.SetDynamicSpotLightTarget(light, target);
            } else {
                Vector3 position = light.position;
                if (field == DynamicSpotFloatField::PositionX) {
                    position.x = result.value;
                } else if (field == DynamicSpotFloatField::PositionY) {
                    position.y = result.value;
                } else {
                    position.z = result.value;
                }
                lightEditing.SetDynamicSpotLightPosition(light, position);
            }
        }
        y += rowH + gap;
    };

    drawLightFloat(
            "sector_editor_dynamic_spot_light_flicker_speed",
            "Flicker speed:",
            light.flickerSpeed,
            uiState.lightFlickerSpeedInput,
            DynamicLightFlickerMinSpeed,
            DynamicLightFlickerMaxSpeed,
            3,
            DynamicSpotFloatField::FlickerSpeed);
    drawLightFloat(
            "sector_editor_dynamic_spot_light_flicker_amount",
            "Flicker amount:",
            light.flickerAmount,
            uiState.lightFlickerAmountInput,
            DynamicLightFlickerMinAmount,
            DynamicLightFlickerMaxAmount,
            3,
            DynamicSpotFloatField::FlickerAmount);

    bool castsShadow = light.castsShadow;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_spot_light_casts_shadow", Rectangle{0.0f, y, contentW, rowH}, font, "Cast Shadows", castsShadow)
            && castsShadow != light.castsShadow) {
        lightEditing.SetDynamicSpotLightCastsShadow(light, castsShadow);
    }
    y += rowH + gap;
    const char* shadowNote = TextFormat(
            "Requests one shadow-atlas slot. Quality and priority decide the budget; over-budget spots still light.");
    const float shadowNoteHeight = MeasureSectorEditorWrappedTextHeight(
            smallConfig,
            assets,
            smallFont,
            shadowNote,
            contentW,
            2);
    engine::Text(
            ui,
            smallConfig,
            assets,
            Rectangle{0.0f, y, contentW, shadowNoteHeight},
            smallFont,
            shadowNote,
            engine::UITextJustify::Left,
            smallConfig.mutedTextColor,
            true);
    y += shadowNoteHeight + gap;

    {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightIntRowLayout(y, contentW, rowH, gap);
        const SectorEditorIntInputResult result = DrawLabeledIntInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_dynamic_spot_light_shadow_priority",
                "Shadow priority:",
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                light.shadowPriority,
                uiState.lightShadowPriorityInput,
                DynamicSpotLightMinShadowPriority,
                DynamicSpotLightMaxShadowPriority,
                1);
        const int edited = ClampDynamicSpotLightShadowPriority(result.value);
        if (result.changed && edited != light.shadowPriority) {
            lightEditing.SetDynamicSpotLightShadowPriority(light, result.value);
        }
        y += rowH + gap;
    }
    {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_dynamic_spot_light_shadow_bias",
                "Shadow bias:",
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                light.shadowBias,
                uiState.lightShadowBiasInput,
                DynamicSpotLightMinShadowBias,
                DynamicSpotLightMaxShadowBias,
                5);
        const float edited = ClampDynamicSpotLightShadowBias(result.value);
        if (result.changed && edited != light.shadowBias) {
            lightEditing.SetDynamicSpotLightShadowBias(light, result.value);
        }
        y += rowH + gap;
    }
    {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_dynamic_spot_light_shadow_strength",
                "Shadow strength:",
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                light.shadowStrength,
                uiState.lightShadowStrengthInput,
                DynamicSpotLightMinShadowStrength,
                DynamicSpotLightMaxShadowStrength,
                3);
        const float edited = ClampDynamicSpotLightShadowStrength(result.value);
        if (result.changed && edited != light.shadowStrength) {
            lightEditing.SetDynamicSpotLightShadowStrength(light, result.value);
        }
        y += rowH + gap;
    }
    {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_dynamic_spot_light_shadow_softness",
                "Softness",
                layout.labelRect,
                layout.inputRect,
                engine::UITextJustify::Right,
                light.shadowSoftness,
                uiState.lightShadowSoftnessInput,
                DynamicSpotLightMinShadowSoftness,
                DynamicSpotLightMaxShadowSoftness,
                3);
        const float edited = ClampDynamicSpotLightShadowSoftness(result.value);
        if (result.changed && edited != light.shadowSoftness) {
            lightEditing.SetDynamicSpotLightShadowSoftness(light, result.value);
        }
        y += rowH + gap;
    }

    drawLightFloat("sector_editor_dynamic_spot_light_x", "Position X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2, DynamicSpotFloatField::PositionX);
    drawLightFloat("sector_editor_dynamic_spot_light_y", "Position Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2, DynamicSpotFloatField::PositionY);
    drawLightFloat("sector_editor_dynamic_spot_light_z", "Position Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2, DynamicSpotFloatField::PositionZ);
    drawLightFloat("sector_editor_dynamic_spot_light_target_x", "Target X:", light.target.x, uiState.lightTargetXInput, -8192.0f, 8192.0f, 2, DynamicSpotFloatField::TargetX);
    drawLightFloat("sector_editor_dynamic_spot_light_target_y", "Target Y:", light.target.y, uiState.lightTargetYInput, -512.0f, 512.0f, 2, DynamicSpotFloatField::TargetY);
    drawLightFloat("sector_editor_dynamic_spot_light_target_z", "Target Z:", light.target.z, uiState.lightTargetZInput, -8192.0f, 8192.0f, 2, DynamicSpotFloatField::TargetZ);
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_dynamic_spot_light_point_down",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Point Down")) {
        lightEditing.PointDynamicSpotLightDown(light);
    }
    y += rowH + gap;
    drawLightFloat("sector_editor_dynamic_spot_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3, DynamicSpotFloatField::Intensity);
    drawLightFloat("sector_editor_dynamic_spot_light_range", "Range:", light.range, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2, DynamicSpotFloatField::Range);
    drawLightFloat("sector_editor_dynamic_spot_light_inner_cone", "Inner cone:", light.innerConeDegrees, uiState.lightInnerConeInput, 0.0f, 179.0f, 2, DynamicSpotFloatField::InnerCone);
    drawLightFloat("sector_editor_dynamic_spot_light_outer_cone", "Outer cone:", light.outerConeDegrees, uiState.lightOuterConeInput, 0.0f, 179.0f, 2, DynamicSpotFloatField::OuterCone);

    auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const float colorLabelW = 126.0f;
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, colorLabelW, rowH},
                Rectangle{colorLabelW, y, contentW - colorLabelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            Color color = light.color;
            if (&channel == &light.color.r) {
                color.r = result.channel;
            } else if (&channel == &light.color.g) {
                color.g = result.channel;
            } else {
                color.b = result.channel;
            }
            lightEditing.SetDynamicSpotLightColor(light, color);
        }
        y += rowH + gap;
    };
    drawLightChannel("sector_editor_dynamic_spot_light_r", "R:", light.color.r, uiState.lightRedInput);
    drawLightChannel("sector_editor_dynamic_spot_light_g", "G:", light.color.g, uiState.lightGreenInput);
    drawLightChannel("sector_editor_dynamic_spot_light_b", "B:", light.color.b, uiState.lightBlueInput);

    const float swatchW = std::min(120.0f, contentW);
    const Rectangle swatch{
            scroll.viewport.x + std::max(0.0f, contentW - swatchW),
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            swatchW,
            28.0f
    };
    DrawColorSwatch(config, swatch, light.color, light.enabled ? 1.0f : 0.45f);
    y += 36.0f + gap;

    DrawLightAtmosphereInspector(
            ui, config, input, assets, font, contentW, rowH, gap, y,
            light.atmosphere, uiState,
            [&lightEditing, &light](SectorLightAtmosphereSettings settings) {
                return lightEditing.SetDynamicSpotLightAtmosphere(light, settings);
            },
            sourceRefreshRequested, true);

    sourceRefreshRequested = sourceRefreshRequested || AtmosphereSourceChanged(sourceBefore, light);
    return true;
}

float RectLightInspectorContentHeight(
        float rowH, float gap, bool hasIdError, bool dynamic,
        const SectorLightAtmosphereSettings& atmosphere)
{
    float height = 38.0f + rowH + gap + (hasIdError ? 36.0f : 0.0f);
    height += rowH + gap; // Delete.
    height += (dynamic ? 9.0f : 1.0f) * (rowH + gap); // Runtime and shadow controls.
    height += 13.0f * (rowH + gap); // Position, target, Point Down, roll, size, range, intensity.
    height += 3.0f * (rowH + gap) + 36.0f + gap;
    height += LightAtmosphereInspectorContentHeight(rowH, gap, atmosphere, true);
    if (!dynamic) height += rowH + gap;
    return height;
}

template<typename Light>
bool DrawRectLightInspector(
        engine::UIContext& ui, const engine::UIConfig& config, engine::Input& input,
        engine::AssetManager& assets, engine::FontHandle font,
        engine::UIScrollAreaResult scroll, float contentW, float rowH, float gap,
        Light& light, SectorEditorUiState& uiState, InspectorIdUiState& inspectorIdUiState,
        SectorEditorLightEditingService& editing, bool& deleteRequested,
        bool* bakeRequested, bool& sourceRefreshRequested)
{
    constexpr bool Dynamic = std::is_same_v<Light, SectorTopologyDynamicRectLight>;
    float y = 0.0f;
    engine::Text(ui, config, assets, {0.0f, y, contentW, 34.0f}, font,
            TextFormat("%s Rect Light: %d", Dynamic ? "Dynamic" : "Static", light.id),
            engine::UITextJustify::Left, config.textColor);
    y += 38.0f;
    engine::Text(ui, config, assets, {0.0f, y, 88.0f, rowH}, font, "Id",
            engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, {88.0f, y, contentW - 88.0f, rowH}, font,
            TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;
    if (!inspectorIdUiState.idEditError.empty()) {
        engine::Text(ui, config, assets, {0.0f, y, contentW, 34.0f}, font,
                inspectorIdUiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }
    if (engine::Button(ui, config, input, assets, Dynamic
                ? "sector_editor_delete_dynamic_rect_light" : "sector_editor_delete_static_rect_light",
                {0.0f, y, contentW, rowH}, font, "Delete Light")) {
        deleteRequested = true;
        return true;
    }
    y += rowH + gap;
    if constexpr (Dynamic) {
        bool enabled = light.enabled;
        if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_rect_enabled",
                    {0.0f, y, contentW, rowH}, font, "Enabled", enabled)
                && editing.SetDynamicRectLightEnabled(light, enabled)) sourceRefreshRequested = true;
        y += rowH + gap;
        bool flicker = light.flicker;
        if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_rect_flicker",
                    {0.0f, y, contentW, rowH}, font, "Flicker", flicker)
                && editing.SetDynamicRectLightFlicker(light, flicker)) sourceRefreshRequested = true;
        y += rowH + gap;
        auto dynamicFloatRow = [&](const char* id, const char* label, float value,
                engine::UIFloatInputState& state, float minimum, float maximum,
                int decimals, auto setter) {
            const auto layout = BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
            const auto result = DrawLabeledFloatInput(ui, config, input, assets, font,
                    id, label, layout.labelRect, layout.inputRect,
                    engine::UITextJustify::Right, value, state, minimum, maximum, decimals);
            if (result.changed && result.value != value
                    && (editing.*setter)(light, result.value)) sourceRefreshRequested = true;
            y += rowH + gap;
        };
        dynamicFloatRow("sector_editor_dynamic_rect_flicker_speed", "Flicker speed:",
                light.flickerSpeed, uiState.lightFlickerSpeedInput,
                DynamicLightFlickerMinSpeed, DynamicLightFlickerMaxSpeed, 3,
                &SectorEditorLightEditingService::SetDynamicRectLightFlickerSpeed);
        dynamicFloatRow("sector_editor_dynamic_rect_flicker_amount", "Flicker amount:",
                light.flickerAmount, uiState.lightFlickerAmountInput,
                DynamicLightFlickerMinAmount, DynamicLightFlickerMaxAmount, 3,
                &SectorEditorLightEditingService::SetDynamicRectLightFlickerAmount);
    }
    bool shadow = light.castsShadow;
    if (engine::Checkbox(ui, config, input, assets, Dynamic
                ? "sector_editor_dynamic_rect_shadow" : "sector_editor_static_rect_shadow",
                {0.0f, y, contentW, rowH}, font, "Shadow", shadow)) {
        const bool changed = [&]() {
            if constexpr (Dynamic) return editing.SetDynamicRectLightCastsShadow(light, shadow);
            else return editing.SetStaticRectLightCastsShadow(light, shadow);
        }();
        sourceRefreshRequested = sourceRefreshRequested || changed;
    }
    y += rowH + gap;
    if constexpr (Dynamic) {
        {
            const auto layout = BuildSectorEditorInspectorRightIntRowLayout(y, contentW, rowH, gap);
            const auto result = DrawLabeledIntInput(ui, config, input, assets, font,
                    "sector_editor_dynamic_rect_shadow_priority", "Shadow priority:",
                    layout.labelRect, layout.inputRect, engine::UITextJustify::Right,
                    light.shadowPriority, uiState.lightShadowPriorityInput,
                    DynamicSpotLightMinShadowPriority, DynamicSpotLightMaxShadowPriority, 1);
            if (result.changed && result.value != light.shadowPriority
                    && editing.SetDynamicRectLightShadowPriority(light, result.value)) {
                sourceRefreshRequested = true;
            }
            y += rowH + gap;
        }
        auto shadowFloatRow = [&](const char* id, const char* label, float value,
                engine::UIFloatInputState& state, float minimum, float maximum,
                int decimals, auto setter) {
            const auto layout = BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
            const auto result = DrawLabeledFloatInput(ui, config, input, assets, font,
                    id, label, layout.labelRect, layout.inputRect,
                    engine::UITextJustify::Right, value, state, minimum, maximum, decimals);
            if (result.changed && result.value != value
                    && (editing.*setter)(light, result.value)) sourceRefreshRequested = true;
            y += rowH + gap;
        };
        shadowFloatRow("sector_editor_dynamic_rect_shadow_bias", "Shadow bias:",
                light.shadowBias, uiState.lightShadowBiasInput,
                DynamicSpotLightMinShadowBias, DynamicSpotLightMaxShadowBias, 5,
                &SectorEditorLightEditingService::SetDynamicRectLightShadowBias);
        shadowFloatRow("sector_editor_dynamic_rect_shadow_strength", "Shadow strength:",
                light.shadowStrength, uiState.lightShadowStrengthInput,
                DynamicSpotLightMinShadowStrength, DynamicSpotLightMaxShadowStrength, 3,
                &SectorEditorLightEditingService::SetDynamicRectLightShadowStrength);
        shadowFloatRow("sector_editor_dynamic_rect_shadow_softness", "Softness:",
                light.shadowSoftness, uiState.lightShadowSoftnessInput,
                DynamicSpotLightMinShadowSoftness, DynamicSpotLightMaxShadowSoftness, 3,
                &SectorEditorLightEditingService::SetDynamicRectLightShadowSoftness);
    }

    enum Field { PX, PY, PZ, TX, TY, TZ, Roll, Width, Height, Range, Intensity };
    auto row = [&](const char* id, const char* label, float value,
            engine::UIFloatInputState& state, float minimum, float maximum, Field field) {
        const auto layout = BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
        const auto result = DrawLabeledFloatInput(ui, config, input, assets, font, id, label,
                layout.labelRect, layout.inputRect, engine::UITextJustify::Right,
                value, state, minimum, maximum, 2);
        if (result.changed && result.value != value) {
            bool changed = false;
            if (field <= PZ) {
                Vector3 v = light.position; (&v.x)[field] = result.value;
                if constexpr (Dynamic) changed = editing.SetDynamicRectLightPosition(light, v);
                else changed = editing.SetStaticRectLightPosition(light, v);
            } else if (field <= TZ) {
                Vector3 v = light.target; (&v.x)[field - TX] = result.value;
                if constexpr (Dynamic) changed = editing.SetDynamicRectLightTarget(light, v);
                else changed = editing.SetStaticRectLightTarget(light, v);
            } else if (field == Roll) {
                if constexpr (Dynamic) changed = editing.SetDynamicRectLightRoll(light, result.value);
                else changed = editing.SetStaticRectLightRoll(light, result.value);
            } else if (field == Width) {
                if constexpr (Dynamic) changed = editing.SetDynamicRectLightWidth(light, result.value);
                else changed = editing.SetStaticRectLightWidth(light, result.value);
            } else if (field == Height) {
                if constexpr (Dynamic) changed = editing.SetDynamicRectLightHeight(light, result.value);
                else changed = editing.SetStaticRectLightHeight(light, result.value);
            } else if (field == Range) {
                if constexpr (Dynamic) changed = editing.SetDynamicRectLightRange(light, result.value);
                else changed = editing.SetStaticRectLightRange(light, result.value);
            } else {
                if constexpr (Dynamic) changed = editing.SetDynamicRectLightIntensity(light, result.value);
                else changed = editing.SetStaticRectLightIntensity(light, result.value);
            }
            sourceRefreshRequested = sourceRefreshRequested || changed;
        }
        y += rowH + gap;
    };
    const char* prefix = Dynamic ? "dynamic_rect" : "static_rect";
    row(TextFormat("%s_px", prefix), "Position X:", light.position.x, uiState.lightXInput, -8192, 8192, PX);
    row(TextFormat("%s_py", prefix), "Position Y:", light.position.y, uiState.lightYInput, -512, 512, PY);
    row(TextFormat("%s_pz", prefix), "Position Z:", light.position.z, uiState.lightZInput, -8192, 8192, PZ);
    row(TextFormat("%s_tx", prefix), "Target X:", light.target.x, uiState.lightTargetXInput, -8192, 8192, TX);
    row(TextFormat("%s_ty", prefix), "Target Y:", light.target.y, uiState.lightTargetYInput, -512, 512, TY);
    row(TextFormat("%s_tz", prefix), "Target Z:", light.target.z, uiState.lightTargetZInput, -8192, 8192, TZ);
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                Dynamic ? "sector_editor_dynamic_rect_light_point_down"
                        : "sector_editor_static_rect_light_point_down",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Point Down")) {
        const bool changed = [&]() {
            if constexpr (Dynamic) return editing.PointDynamicRectLightDown(light);
            else return editing.PointStaticRectLightDown(light);
        }();
        sourceRefreshRequested = sourceRefreshRequested || changed;
    }
    y += rowH + gap;
    row(TextFormat("%s_roll", prefix), "Roll:", light.rollDegrees, uiState.lightSourceRadiusInput, -180, 180, Roll);
    row(TextFormat("%s_width", prefix), "Width:", light.width, uiState.lightInnerConeInput,
            SectorWorldToAuthoringDistance(0.05f), SectorWorldToAuthoringDistance(64.0f), Width);
    row(TextFormat("%s_height", prefix), "Height:", light.height, uiState.lightOuterConeInput,
            SectorWorldToAuthoringDistance(0.05f), SectorWorldToAuthoringDistance(64.0f), Height);
    row(TextFormat("%s_range", prefix), "Range:", light.range, uiState.lightRadiusInput,
            SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), Range);
    row(TextFormat("%s_intensity", prefix), "Intensity:", light.intensity, uiState.lightIntensityInput, 0, 8, Intensity);

    auto colorRow = [&](const char* suffix, const char* label, unsigned char& channel,
            engine::UIIntInputState& state) {
        const auto result = DrawRgb8ChannelInput(ui, config, input, assets, font,
                TextFormat("%s_%s", prefix, suffix), label, {0.0f, y, 126.0f, rowH},
                {126.0f, y, contentW - 126.0f, rowH}, engine::UITextJustify::Right,
                channel, state);
        if (result.changed) {
            Color value = light.color;
            if (&channel == &light.color.r) value.r = result.channel;
            else if (&channel == &light.color.g) value.g = result.channel;
            else value.b = result.channel;
            if constexpr (Dynamic) editing.SetDynamicRectLightColor(light, value);
            else editing.SetStaticRectLightColor(light, value);
            sourceRefreshRequested = true;
        }
        y += rowH + gap;
    };
    colorRow("r", "R:", light.color.r, uiState.lightRedInput);
    colorRow("g", "G:", light.color.g, uiState.lightGreenInput);
    colorRow("b", "B:", light.color.b, uiState.lightBlueInput);
    DrawColorSwatch(config, {scroll.viewport.x + std::max(0.0f, contentW - 120.0f),
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f, 120.0f, 28.0f},
            light.color, 1.0f);
    y += 36.0f + gap;
    DrawLightAtmosphereInspector(ui, config, input, assets, font, contentW, rowH, gap, y,
            light.atmosphere, uiState,
            [&editing, &light](SectorLightAtmosphereSettings settings) {
                if constexpr (Dynamic) return editing.SetDynamicRectLightAtmosphere(light, settings);
                else return editing.SetStaticRectLightAtmosphere(light, settings);
            }, sourceRefreshRequested, true);
    if (bakeRequested != nullptr && engine::Button(ui, config, input, assets,
                "sector_editor_static_rect_bake", {0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        *bakeRequested = true;
    }
    return true;
}

bool DrawSelectedStaticRectLightInspector(
        engine::UIContext& ui, const engine::UIConfig& config, engine::Input& input,
        engine::AssetManager& assets, engine::FontHandle font, engine::UIScrollAreaResult scroll,
        float contentW, float rowH, float gap, SectorTopologyStaticRectLight& light,
        SectorEditorUiState& state, InspectorIdUiState& idState,
        SectorEditorLightEditingService& editing, bool& deleteRequested,
        bool& bakeRequested, bool& sourceRefreshRequested)
{
    return DrawRectLightInspector(ui, config, input, assets, font, scroll, contentW,
            rowH, gap, light, state, idState, editing, deleteRequested,
            &bakeRequested, sourceRefreshRequested);
}

bool DrawSelectedDynamicRectLightInspector(
        engine::UIContext& ui, const engine::UIConfig& config, engine::Input& input,
        engine::AssetManager& assets, engine::FontHandle font, engine::UIScrollAreaResult scroll,
        float contentW, float rowH, float gap, SectorTopologyDynamicRectLight& light,
        SectorEditorUiState& state, InspectorIdUiState& idState,
        SectorEditorLightEditingService& editing, bool& deleteRequested,
        bool& sourceRefreshRequested)
{
    return DrawRectLightInspector(ui, config, input, assets, font, scroll, contentW,
            rowH, gap, light, state, idState, editing, deleteRequested,
            nullptr, sourceRefreshRequested);
}

} // namespace game
