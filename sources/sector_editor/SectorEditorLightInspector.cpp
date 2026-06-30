#include "sector_editor/SectorEditorLightInspector.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>

namespace game {

float StaticLightInspectorContentHeight(float rowH, float gap, bool hasIdError)
{
    float height = 38.0f; // Light title.
    height += rowH + gap; // Id.
    if (hasIdError) {
        height += 36.0f;
    }
    height += rowH + gap; // Delete.
    height += 6.0f * (rowH + gap); // Position/intensity/radius/source radius.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Swatch.
    height += rowH + gap; // Bake.
    return height;
}

float StaticSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError)
{
    float height = 38.0f; // Light title.
    height += rowH + gap; // Id.
    if (hasIdError) {
        height += 36.0f;
    }
    height += rowH + gap; // Delete.
    height += 12.0f * (rowH + gap); // Position/target/intensity/range/source/cones.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Swatch.
    height += rowH + gap; // Bake.
    return height;
}

float DynamicLightInspectorContentHeight(float rowH, float gap, bool hasIdError)
{
    float height = 38.0f; // Light title.
    height += rowH + gap; // Id.
    if (hasIdError) {
        height += 36.0f;
    }
    height += rowH + gap; // Delete.
    height += rowH + gap; // Enabled.
    height += 3.0f * (rowH + gap); // Flicker controls.
    height += 5.0f * (rowH + gap); // Position/intensity/radius.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Swatch.
    return height;
}

float DynamicSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError)
{
    float height = 38.0f; // Light title.
    height += rowH + gap; // Id.
    if (hasIdError) {
        height += 36.0f;
    }
    height += rowH + gap; // Delete.
    height += rowH + gap; // Enabled.
    height += 3.0f * (rowH + gap); // Flicker controls.
    height += 4.0f * (rowH + gap); // Shadow controls.
    height += 38.0f + gap; // Shadow budget note.
    height += 11.0f * (rowH + gap); // Position/target/intensity/range/cones.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Swatch.
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
        const SectorEditorLightInspectorCallbacks& callbacks)
{
    float y = 0.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Static Light: %d", light.id), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 88.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;

    if (!uiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, uiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
        callbacks.deleteSelectedLight();
        return true;
    }
    y += rowH + gap;

    const float numberLabelW = 92.0f;
    const float numberFieldW = 112.0f;
    auto drawLightFloat = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            value = result.value;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated static light %d", light.id));
        }
        y += rowH + gap;
    };

    drawLightFloat("sector_editor_light_x", "X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_light_y", "Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2);
    drawLightFloat("sector_editor_light_z", "Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3);
    light.intensity = ClampLightIntensity(light.intensity);
    drawLightFloat("sector_editor_light_radius", "Radius:", light.radius, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2);
    light.radius = ClampLightRadius(light.radius);
    light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.radius);
    {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_light_source_radius",
                "Source:",
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                light.sourceRadius,
                uiState.lightSourceRadiusInput,
                0.0f,
                SectorWorldToAuthoringDistance(8.0f),
                3);
        const float edited = ClampLightSourceRadius(result.value, light.radius);
        if (result.changed && edited != light.sourceRadius) {
            light.sourceRadius = edited;
            callbacks.markTopologyDocumentEdited("Updated light source radius");
        }
        y += rowH + gap;
    }

    auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, contentW - numberLabelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            channel = result.channel;
            light.color.a = 255;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated static light %d color", light.id));
        }
        y += rowH + gap;
    };
    drawLightChannel("sector_editor_light_r", "R:", light.color.r, uiState.lightRedInput);
    drawLightChannel("sector_editor_light_g", "G:", light.color.g, uiState.lightGreenInput);
    drawLightChannel("sector_editor_light_b", "B:", light.color.b, uiState.lightBlueInput);

    const Rectangle swatch{
            scroll.viewport.x + numberLabelW,
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            std::min(120.0f, contentW - numberLabelW),
            28.0f
    };
    DrawColorSwatch(config, swatch, light.color, 1.0f);
    y += 36.0f + gap;

    if (engine::Button(ui, config, input, assets, "sector_editor_light_bake", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        callbacks.bakeLightmaps();
    }

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
        const SectorEditorLightInspectorCallbacks& callbacks)
{
    float y = 0.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Static Spot: %d", light.id), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 88.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;

    if (!uiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, uiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_static_spot_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
        callbacks.deleteSelectedLight();
        return true;
    }
    y += rowH + gap;

    const float numberLabelW = 126.0f;
    const float numberFieldW = 112.0f;
    auto drawLightFloat = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            value = result.value;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated static spot %d", light.id));
        }
        y += rowH + gap;
    };

    drawLightFloat("sector_editor_static_spot_light_x", "Position X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_static_spot_light_y", "Position Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2);
    drawLightFloat("sector_editor_static_spot_light_z", "Position Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_static_spot_light_target_x", "Target X:", light.target.x, uiState.lightTargetXInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_static_spot_light_target_y", "Target Y:", light.target.y, uiState.lightTargetYInput, -512.0f, 512.0f, 2);
    drawLightFloat("sector_editor_static_spot_light_target_z", "Target Z:", light.target.z, uiState.lightTargetZInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_static_spot_light_range", "Radius:", light.range, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2);
    light.range = ClampLightRadius(light.range);
    light.sourceRadius = ClampLightSourceRadius(light.sourceRadius, light.range);
    {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_static_spot_light_source_radius",
                "Source radius:",
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                light.sourceRadius,
                uiState.lightSourceRadiusInput,
                0.0f,
                SectorWorldToAuthoringDistance(8.0f),
                3);
        const float edited = ClampLightSourceRadius(result.value, light.range);
        if (result.changed && edited != light.sourceRadius) {
            light.sourceRadius = edited;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated static spot %d source radius", light.id));
        }
        y += rowH + gap;
    }
    drawLightFloat("sector_editor_static_spot_light_inner_cone", "Inner cone:", light.innerConeDegrees, uiState.lightInnerConeInput, 0.0f, 179.0f, 2);
    light.innerConeDegrees = std::clamp(light.innerConeDegrees, 0.0f, 179.0f);
    drawLightFloat("sector_editor_static_spot_light_outer_cone", "Outer cone:", light.outerConeDegrees, uiState.lightOuterConeInput, 0.0f, 179.0f, 2);
    light.outerConeDegrees = std::clamp(std::max(light.outerConeDegrees, light.innerConeDegrees), 0.0f, 179.0f);
    drawLightFloat("sector_editor_static_spot_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3);
    light.intensity = ClampLightIntensity(light.intensity);

    auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, contentW - numberLabelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            channel = result.channel;
            light.color.a = 255;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated static spot %d color", light.id));
        }
        y += rowH + gap;
    };
    drawLightChannel("sector_editor_static_spot_light_r", "R:", light.color.r, uiState.lightRedInput);
    drawLightChannel("sector_editor_static_spot_light_g", "G:", light.color.g, uiState.lightGreenInput);
    drawLightChannel("sector_editor_static_spot_light_b", "B:", light.color.b, uiState.lightBlueInput);

    const Rectangle swatch{
            scroll.viewport.x + numberLabelW,
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            std::min(120.0f, contentW - numberLabelW),
            28.0f
    };
    DrawColorSwatch(config, swatch, light.color, 1.0f);
    y += 36.0f + gap;

    if (engine::Button(ui, config, input, assets, "sector_editor_static_spot_light_bake", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        callbacks.bakeLightmaps();
    }

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
        const SectorEditorLightInspectorCallbacks& callbacks)
{
    float y = 0.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Dynamic Light: %d", light.id), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 88.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;

    if (!uiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, uiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_dynamic_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
        callbacks.deleteSelectedLight();
        return true;
    }
    y += rowH + gap;

    bool enabled = light.enabled;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_light_enabled", Rectangle{0.0f, y, contentW, rowH}, font, "Enabled", enabled)
            && enabled != light.enabled) {
        light.enabled = enabled;
        callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic light %d enabled", light.id));
    }
    y += rowH + gap;

    bool flicker = light.flicker;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_light_flicker", Rectangle{0.0f, y, contentW, rowH}, font, "Flicker", flicker)
            && flicker != light.flicker) {
        light.flicker = flicker;
        callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic light %d flicker", light.id));
    }
    y += rowH + gap;

    const float numberLabelW = 116.0f;
    const float numberFieldW = 112.0f;
    auto drawLightFloat = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            value = result.value;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic light %d", light.id));
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
            3);
    light.flickerSpeed = ClampDynamicLightFlickerSpeed(light.flickerSpeed);
    drawLightFloat(
            "sector_editor_dynamic_light_flicker_amount",
            "Flicker amount:",
            light.flickerAmount,
            uiState.lightFlickerAmountInput,
            DynamicLightFlickerMinAmount,
            DynamicLightFlickerMaxAmount,
            3);
    light.flickerAmount = ClampDynamicLightFlickerAmount(light.flickerAmount);

    drawLightFloat("sector_editor_dynamic_light_x", "X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_dynamic_light_y", "Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2);
    drawLightFloat("sector_editor_dynamic_light_z", "Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_dynamic_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3);
    light.intensity = ClampLightIntensity(light.intensity);
    drawLightFloat("sector_editor_dynamic_light_radius", "Radius:", light.radius, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2);
    light.radius = ClampLightRadius(light.radius);

    auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, contentW - numberLabelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            channel = result.channel;
            light.color.a = 255;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic light %d color", light.id));
        }
        y += rowH + gap;
    };
    drawLightChannel("sector_editor_dynamic_light_r", "R:", light.color.r, uiState.lightRedInput);
    drawLightChannel("sector_editor_dynamic_light_g", "G:", light.color.g, uiState.lightGreenInput);
    drawLightChannel("sector_editor_dynamic_light_b", "B:", light.color.b, uiState.lightBlueInput);

    const Rectangle swatch{
            scroll.viewport.x + numberLabelW,
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            std::min(120.0f, contentW - numberLabelW),
            28.0f
    };
    DrawColorSwatch(config, swatch, light.color, light.enabled ? 1.0f : 0.45f);

    return true;
}

bool DrawSelectedDynamicSpotLightInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologyDynamicSpotLight& light,
        SectorEditorUiState& uiState,
        const SectorEditorLightInspectorCallbacks& callbacks)
{
    float y = 0.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, TextFormat("Dynamic Spot: %d", light.id), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    const float labelW = 88.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Id", engine::UITextJustify::Left, config.mutedTextColor);
    engine::Text(ui, config, assets, Rectangle{labelW, y, contentW - labelW, rowH}, font, TextFormat("%d", light.id), engine::UITextJustify::Left, config.textColor);
    y += rowH + gap;

    if (!uiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, uiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_delete_dynamic_spot_light", Rectangle{0.0f, y, contentW, rowH}, font, "Delete Light")) {
        callbacks.deleteSelectedLight();
        return true;
    }
    y += rowH + gap;

    bool enabled = light.enabled;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_spot_light_enabled", Rectangle{0.0f, y, contentW, rowH}, font, "Enabled", enabled)
            && enabled != light.enabled) {
        light.enabled = enabled;
        callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic spot %d enabled", light.id));
    }
    y += rowH + gap;

    bool flicker = light.flicker;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_spot_light_flicker", Rectangle{0.0f, y, contentW, rowH}, font, "Flicker", flicker)
            && flicker != light.flicker) {
        light.flicker = flicker;
        callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic spot %d flicker", light.id));
    }
    y += rowH + gap;

    const float numberLabelW = 126.0f;
    const float numberFieldW = 112.0f;
    auto drawLightFloat = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            value = result.value;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic spot %d", light.id));
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
            3);
    light.flickerSpeed = ClampDynamicLightFlickerSpeed(light.flickerSpeed);
    drawLightFloat(
            "sector_editor_dynamic_spot_light_flicker_amount",
            "Flicker amount:",
            light.flickerAmount,
            uiState.lightFlickerAmountInput,
            DynamicLightFlickerMinAmount,
            DynamicLightFlickerMaxAmount,
            3);
    light.flickerAmount = ClampDynamicLightFlickerAmount(light.flickerAmount);

    bool castsShadow = light.castsShadow;
    if (engine::Checkbox(ui, config, input, assets, "sector_editor_dynamic_spot_light_casts_shadow", Rectangle{0.0f, y, contentW, rowH}, font, "Cast Shadows", castsShadow)
            && castsShadow != light.castsShadow) {
        light.castsShadow = castsShadow;
        callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic spot %d shadow request", light.id));
    }
    y += rowH + gap;
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 18.0f},
            font,
            TextFormat("Requests one of %zu shadow slots.", MaxDynamicSpotLightShadowCasters),
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 18.0f;
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 18.0f},
            font,
            "Priority decides budget; over-budget spots still light.",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 20.0f + gap;

    {
        const SectorEditorIntInputResult result = DrawLabeledIntInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_dynamic_spot_light_shadow_priority",
                "Shadow priority:",
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                light.shadowPriority,
                uiState.lightShadowPriorityInput,
                DynamicSpotLightMinShadowPriority,
                DynamicSpotLightMaxShadowPriority,
                1);
        const int edited = ClampDynamicSpotLightShadowPriority(result.value);
        if (result.changed && edited != light.shadowPriority) {
            light.shadowPriority = edited;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic spot %d shadow priority", light.id));
        }
        y += rowH + gap;
    }
    {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_dynamic_spot_light_shadow_bias",
                "Shadow bias:",
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                light.shadowBias,
                uiState.lightShadowBiasInput,
                DynamicSpotLightMinShadowBias,
                DynamicSpotLightMaxShadowBias,
                5);
        const float edited = ClampDynamicSpotLightShadowBias(result.value);
        if (result.changed && edited != light.shadowBias) {
            light.shadowBias = edited;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic spot %d shadow bias", light.id));
        }
        y += rowH + gap;
    }
    {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_dynamic_spot_light_shadow_strength",
                "Shadow strength:",
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                light.shadowStrength,
                uiState.lightShadowStrengthInput,
                DynamicSpotLightMinShadowStrength,
                DynamicSpotLightMaxShadowStrength,
                3);
        const float edited = ClampDynamicSpotLightShadowStrength(result.value);
        if (result.changed && edited != light.shadowStrength) {
            light.shadowStrength = edited;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic spot %d shadow strength", light.id));
        }
        y += rowH + gap;
    }

    drawLightFloat("sector_editor_dynamic_spot_light_x", "Position X:", light.position.x, uiState.lightXInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_dynamic_spot_light_y", "Position Y:", light.position.y, uiState.lightYInput, -512.0f, 512.0f, 2);
    drawLightFloat("sector_editor_dynamic_spot_light_z", "Position Z:", light.position.z, uiState.lightZInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_dynamic_spot_light_target_x", "Target X:", light.target.x, uiState.lightTargetXInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_dynamic_spot_light_target_y", "Target Y:", light.target.y, uiState.lightTargetYInput, -512.0f, 512.0f, 2);
    drawLightFloat("sector_editor_dynamic_spot_light_target_z", "Target Z:", light.target.z, uiState.lightTargetZInput, -8192.0f, 8192.0f, 2);
    drawLightFloat("sector_editor_dynamic_spot_light_intensity", "Intensity:", light.intensity, uiState.lightIntensityInput, 0.0f, 8.0f, 3);
    light.intensity = ClampLightIntensity(light.intensity);
    drawLightFloat("sector_editor_dynamic_spot_light_range", "Range:", light.range, uiState.lightRadiusInput, SectorWorldToAuthoringDistance(0.1f), SectorWorldToAuthoringDistance(64.0f), 2);
    light.range = ClampLightRadius(light.range);
    drawLightFloat("sector_editor_dynamic_spot_light_inner_cone", "Inner cone:", light.innerConeDegrees, uiState.lightInnerConeInput, 0.0f, 179.0f, 2);
    light.innerConeDegrees = std::clamp(light.innerConeDegrees, 0.0f, 179.0f);
    drawLightFloat("sector_editor_dynamic_spot_light_outer_cone", "Outer cone:", light.outerConeDegrees, uiState.lightOuterConeInput, 0.0f, 179.0f, 2);
    light.outerConeDegrees = std::clamp(std::max(light.outerConeDegrees, light.innerConeDegrees), 0.0f, 179.0f);

    auto drawLightChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, numberLabelW, rowH},
                Rectangle{numberLabelW, y, contentW - numberLabelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            channel = result.channel;
            light.color.a = 255;
            callbacks.markTopologyDocumentEdited(TextFormat("Updated dynamic spot %d color", light.id));
        }
        y += rowH + gap;
    };
    drawLightChannel("sector_editor_dynamic_spot_light_r", "R:", light.color.r, uiState.lightRedInput);
    drawLightChannel("sector_editor_dynamic_spot_light_g", "G:", light.color.g, uiState.lightGreenInput);
    drawLightChannel("sector_editor_dynamic_spot_light_b", "B:", light.color.b, uiState.lightBlueInput);

    const Rectangle swatch{
            scroll.viewport.x + numberLabelW,
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            std::min(120.0f, contentW - numberLabelW),
            28.0f
    };
    DrawColorSwatch(config, swatch, light.color, light.enabled ? 1.0f : 0.45f);

    return true;
}

} // namespace game
