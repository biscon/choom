#include "sector_editor/tools/windows/SectorEditorWindowInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace game {
namespace {

constexpr float WindowDimensionLimit = 100000.0f;

std::string WindowAnchorStatus(
        const SectorTopologyMap& map,
        const SectorPlacedRuntimeObject& object)
{
    const SectorResolvedWindowAnchor resolved =
            ResolveSectorWindowAnchor(map, object.window);
    if (!resolved.valid) {
        return TextFormat(
                "Anchor invalid: %s",
                resolved.diagnostic.empty()
                        ? "unable to resolve portal"
                        : resolved.diagnostic.c_str());
    }
    return TextFormat(
            "Portal %d: %.3f x %.3f m, sectors %d -> %d",
            object.window.anchor.lineDefId,
            resolved.portalWidth,
            resolved.portalHeight,
            object.window.anchor.frontSectorId,
            object.window.anchor.backSectorId);
}

} // namespace

float MeasureSectorEditorWindowInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object)
{
    const std::string status = WindowAnchorStatus(context.topologyMap, object);
    const float statusHeight = MeasureSectorEditorWrappedTextHeight(
            context.smallConfig,
            context.assets,
            context.smallFont,
            status.c_str(),
            context.contentW,
            3);
    // Shared object ID/type rows, anchor text, 12 controls, section labels,
    // fit action, and delete action.
    return 38.0f + 34.0f + statusHeight + context.gap
            + 52.0f + (context.rowH + context.gap) * 12.0f
            + 52.0f + (context.rowH + context.gap) * 5.0f
            + 52.0f + (context.rowH + context.gap) * 2.0f;
}

void DrawSectorEditorWindowInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y)
{
    const SectorPlacedRuntimeObject* selected = context.editing.SelectedObject();
    if (selected == nullptr || selected->kind != "window") return;

    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(
            context.config, context.assets, context.smallFont);
    const auto refresh = [&]() -> const SectorPlacedRuntimeObject* {
        const SectorPlacedRuntimeObject* value = context.editing.SelectedObject();
        return value != nullptr && value->kind == "window" ? value : nullptr;
    };
    const auto section = [&](const char* label) {
        engine::Text(
                context.ui, context.config, context.assets,
                Rectangle{0.0f, y, contentW, 30.0f}, context.font, label,
                engine::UITextJustify::Left, context.config.textColor);
        y += 34.0f + gap;
    };
    const auto drawFloat = [&] (
            const char* id,
            const char* label,
            float value,
            engine::UIFloatInputState& inputState,
            float minimum,
            float maximum,
            const std::function<bool(SectorPlacedWindow&, float)>& apply) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(
                        y, contentW, rowH, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                context.ui, context.config, context.input, context.assets,
                context.font, id, label, layout.labelRect, layout.inputRect,
                engine::UITextJustify::Left, value, inputState,
                minimum, maximum, 3);
        if (result.changed && result.finite && result.value != value) {
            context.editing.MutateSelected(
                    "Updated window properties",
                    [apply, newValue = result.value](
                            SectorPlacedRuntimeObject& object) {
                        return object.kind == "window"
                                && apply(object.window, newValue);
                    });
        }
        y += rowH + gap;
    };
    const auto drawTint = [&] (
            const char* id,
            const char* label,
            unsigned char value,
            engine::UIIntInputState& inputState,
            int channel) {
        const float labelW = 92.0f;
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                context.ui, context.config, context.input, context.assets,
                context.font, id, label,
                Rectangle{0.0f, y, labelW, rowH},
                Rectangle{labelW + gap, y,
                        std::max(0.0f, contentW - labelW - gap), rowH},
                engine::UITextJustify::Left, value, inputState);
        if (result.changed && result.channel != value) {
            context.editing.MutateSelected(
                    "Updated window tint",
                    [channel, value = result.channel](
                            SectorPlacedRuntimeObject& object) {
                        if (object.kind != "window") return false;
                        unsigned char* target = channel == 0
                                ? &object.window.tint.r
                                : channel == 1 ? &object.window.tint.g
                                : &object.window.tint.b;
                        if (*target == value) return false;
                        *target = value;
                        object.window.tint.a = 255;
                        return true;
                    });
        }
        y += rowH + gap;
    };

    const std::string anchorStatus = WindowAnchorStatus(
            context.topologyMap, *selected);
    const SectorResolvedWindowAnchor resolved =
            ResolveSectorWindowAnchor(context.topologyMap, selected->window);
    const float statusHeight = MeasureSectorEditorWrappedTextHeight(
            smallConfig, context.assets, context.smallFont,
            anchorStatus.c_str(), contentW, 3);
    engine::Text(
            context.ui, smallConfig, context.assets,
            Rectangle{0.0f, y, contentW, statusHeight}, context.smallFont,
            anchorStatus.c_str(), engine::UITextJustify::Left,
            resolved.valid ? context.config.mutedTextColor
                           : context.config.invalidColor,
            true);
    y += statusHeight + gap;

    section("Geometry");
    drawFloat("sector_editor_window_width", "Width", selected->window.width,
            context.uiState.widthInput, 0.0f, WindowDimensionLimit,
            [](SectorPlacedWindow& window, float value) {
                value = std::max(0.0f, value);
                if (window.width == value) return false;
                window.width = value; return true;
            });
    selected = refresh(); if (selected == nullptr) return;
    drawFloat("sector_editor_window_height", "Height", selected->window.height,
            context.uiState.heightInput, 0.0f, WindowDimensionLimit,
            [](SectorPlacedWindow& window, float value) {
                value = std::max(0.0f, value);
                if (window.height == value) return false;
                window.height = value; return true;
            });
    selected = refresh(); if (selected == nullptr) return;
    drawFloat("sector_editor_window_thickness", "Thickness",
            selected->window.thickness, context.uiState.thicknessInput,
            0.001f, 100.0f,
            [](SectorPlacedWindow& window, float value) {
                value = std::max(0.001f, value);
                if (window.thickness == value) return false;
                window.thickness = value; return true;
            });
    selected = refresh(); if (selected == nullptr) return;
    drawFloat("sector_editor_window_horizontal_offset", "Horizontal",
            selected->window.horizontalOffsetWorld,
            context.uiState.horizontalOffsetInput,
            -WindowDimensionLimit, WindowDimensionLimit,
            [](SectorPlacedWindow& window, float value) {
                if (window.horizontalOffsetWorld == value) return false;
                window.horizontalOffsetWorld = value; return true;
            });
    selected = refresh(); if (selected == nullptr) return;
    drawFloat("sector_editor_window_vertical_offset", "Vertical",
            selected->window.verticalOffsetWorld,
            context.uiState.verticalOffsetInput,
            -WindowDimensionLimit, WindowDimensionLimit,
            [](SectorPlacedWindow& window, float value) {
                if (window.verticalOffsetWorld == value) return false;
                window.verticalOffsetWorld = value; return true;
            });
    selected = refresh(); if (selected == nullptr) return;
    drawFloat("sector_editor_window_normal_offset", "Depth offset",
            selected->window.normalOffset, context.uiState.normalOffsetInput,
            -WindowDimensionLimit, WindowDimensionLimit,
            [](SectorPlacedWindow& window, float value) {
                if (window.normalOffset == value) return false;
                window.normalOffset = value; return true;
            });
    selected = refresh(); if (selected == nullptr) return;
    if (engine::Button(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_window_fit_portal",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                "Fit to opening")) {
        context.editing.MutateSelected(
                "Fit window to portal opening",
                [resolved](SectorPlacedRuntimeObject& object) {
                    if (object.kind != "window" || !resolved.valid) return false;
                    const bool changed = object.window.width != resolved.portalWidth
                            || object.window.height != resolved.portalHeight
                            || object.window.horizontalOffsetWorld != 0.0f
                            || object.window.verticalOffsetWorld != 0.0f;
                    object.window.width = resolved.portalWidth;
                    object.window.height = resolved.portalHeight;
                    object.window.horizontalOffsetWorld = 0.0f;
                    object.window.verticalOffsetWorld = 0.0f;
                    return changed;
                });
    }
    y += rowH + gap;

    selected = refresh(); if (selected == nullptr) return;
    section("Glass");
    DrawRectangleRec(
            Rectangle{0.0f, y + 5.0f, contentW, rowH - 10.0f},
            Color{selected->window.tint.r, selected->window.tint.g,
                    selected->window.tint.b, 180});
    y += rowH + gap;
    drawTint("sector_editor_window_tint_r", "Tint R",
            selected->window.tint.r, context.uiState.windowTintRedInput, 0);
    selected = refresh(); if (selected == nullptr) return;
    drawTint("sector_editor_window_tint_g", "Tint G",
            selected->window.tint.g, context.uiState.windowTintGreenInput, 1);
    selected = refresh(); if (selected == nullptr) return;
    drawTint("sector_editor_window_tint_b", "Tint B",
            selected->window.tint.b, context.uiState.windowTintBlueInput, 2);
    selected = refresh(); if (selected == nullptr) return;
    drawFloat("sector_editor_window_opacity", "Opacity",
            selected->window.opacity, context.uiState.opacityInput, 0.0f, 1.0f,
            [](SectorPlacedWindow& window, float value) {
                value = std::clamp(value, 0.0f, 1.0f);
                if (window.opacity == value) return false;
                window.opacity = value; return true;
            });
    selected = refresh(); if (selected == nullptr) return;
    drawFloat("sector_editor_window_roughness", "Roughness",
            selected->window.roughness, context.uiState.roughnessInput,
            0.0f, 1.0f,
            [](SectorPlacedWindow& window, float value) {
                value = std::clamp(value, 0.0f, 1.0f);
                if (window.roughness == value) return false;
                window.roughness = value; return true;
            });
    selected = refresh(); if (selected == nullptr) return;
    drawFloat("sector_editor_window_ior", "IOR",
            selected->window.indexOfRefraction,
            context.uiState.indexOfRefractionInput, 1.0f, 2.5f,
            [](SectorPlacedWindow& window, float value) {
                value = std::clamp(value, 1.0f, 2.5f);
                if (window.indexOfRefraction == value) return false;
                window.indexOfRefraction = value; return true;
            });

    selected = refresh(); if (selected == nullptr) return;
    section("Gameplay");
    bool collision = selected->window.collision;
    if (engine::Checkbox(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_window_collision",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                "Solid", collision)) {
        context.editing.MutateSelected(
                "Updated window collision",
                [collision](SectorPlacedRuntimeObject& object) {
                    if (object.kind != "window"
                            || object.window.collision == collision) return false;
                    object.window.collision = collision;
                    return true;
                });
    }
    y += rowH + gap;
    if (engine::Button(
                context.ui, context.config, context.input, context.assets,
                "sector_editor_delete_runtime_object",
                Rectangle{0.0f, y, contentW, rowH}, context.font, "Delete")) {
        context.deleteRequested = true;
    }
}

} // namespace game
