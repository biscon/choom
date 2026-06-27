#include "sector_editor/SectorEditorLightInspector.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
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
            callbacks.markTopologyDocumentEdited(TextFormat("Updated topology light %d", light.id));
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
            callbacks.markTopologyDocumentEdited(TextFormat("Updated topology light %d color", light.id));
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

} // namespace game
