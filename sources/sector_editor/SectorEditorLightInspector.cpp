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

float DynamicSpotLightInspectorContentHeight(float rowH, float gap, bool hasIdError, float shadowNoteHeight)
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
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested,
        bool& bakeRequested)
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
        deleteRequested = true;
        return true;
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

    if (engine::Button(ui, config, input, assets, "sector_editor_light_bake", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        bakeRequested = true;
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
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested,
        bool& bakeRequested)
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
        deleteRequested = true;
        return true;
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

    if (engine::Button(ui, config, input, assets, "sector_editor_static_spot_light_bake", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        bakeRequested = true;
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
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested)
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
        SectorEditorLightEditingService& lightEditing,
        bool& deleteRequested)
{
    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);
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
            "Requests one of %zu shadow slots. Priority decides budget; over-budget spots still light.",
            MaxDynamicSpotLightShadowCasters);
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

    return true;
}

} // namespace game
