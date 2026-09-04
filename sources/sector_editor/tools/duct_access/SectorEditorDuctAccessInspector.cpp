#include "sector_editor/tools/duct_access/SectorEditorDuctAccessInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/SectorEditorTextureModals.h"

#include <algorithm>
#include <functional>
#include <string>

namespace game {
namespace {

constexpr float DimensionLimit = 100000.0f;

std::string AccessStatus(
        const SectorTopologyMap& map,
        const SectorPlacedDuctAccess& access)
{
    const SectorResolvedDuctAccessAnchor resolved =
            ResolveSectorDuctAccessAnchor(map, access);
    if (!resolved.valid) {
        return TextFormat("Anchor invalid: %s",
                resolved.diagnostic.empty() ? "unable to resolve portal"
                                            : resolved.diagnostic.c_str());
    }
    return TextFormat(
            "Portal %d: %.3f x %.3f m | outside %d -> crawlspace %d",
            resolved.lineDefId, resolved.portalWidth, resolved.portalHeight,
            resolved.outsideSectorId, resolved.crawlspaceSectorId);
}

} // namespace

float MeasureSectorEditorDuctAccessInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object)
{
    const std::string status = AccessStatus(
            context.topologyMap, object.ductAccess);
    const float statusHeight = MeasureSectorEditorWrappedTextHeight(
            context.smallConfig, context.assets, context.smallFont,
            status.c_str(), context.contentW, 3);
    return 38.0f + 34.0f + statusHeight + context.gap
            + 40.0f + 7.0f * (context.rowH + context.gap)
            + 40.0f + 12.0f * (context.rowH + context.gap)
            + 80.0f;
}

void DrawSectorEditorDuctAccessInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y)
{
    const SectorPlacedRuntimeObject* selected = context.editing.SelectedObject();
    if (selected == nullptr || selected->kind != "duct_access") return;

    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(
            context.config, context.assets, context.smallFont);
    const auto refresh = [&]() -> const SectorPlacedRuntimeObject* {
        const SectorPlacedRuntimeObject* value = context.editing.SelectedObject();
        return value != nullptr && value->kind == "duct_access" ? value : nullptr;
    };
    const auto section = [&](const char* label) {
        engine::Text(context.ui, context.config, context.assets,
                Rectangle{0.0f, y, contentW, 30.0f}, context.font, label,
                engine::UITextJustify::Left, context.config.textColor);
        y += 34.0f + gap;
    };
    const auto drawFloat = [&] (
            const char* id, const char* label, float value,
            engine::UIFloatInputState& inputState, float minimum,
            float maximum,
            const std::function<bool(SectorPlacedDuctAccess&, float)>& apply) {
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
                    "Updated Duct Access",
                    [apply, newValue = result.value](
                            SectorPlacedRuntimeObject& object) {
                        return object.kind == "duct_access"
                                && apply(object.ductAccess, newValue);
                    });
        }
        y += rowH + gap;
    };

    const std::string status = AccessStatus(
            context.topologyMap, selected->ductAccess);
    const SectorResolvedDuctAccessAnchor resolved =
            ResolveSectorDuctAccessAnchor(context.topologyMap,
                    selected->ductAccess);
    const float statusHeight = MeasureSectorEditorWrappedTextHeight(
            smallConfig, context.assets, context.smallFont,
            status.c_str(), contentW, 3);
    engine::Text(context.ui, smallConfig, context.assets,
            Rectangle{0.0f, y, contentW, statusHeight}, context.smallFont,
            status.c_str(), engine::UITextJustify::Left,
            resolved.valid ? context.config.mutedTextColor
                           : context.config.invalidColor, true);
    y += statusHeight + gap;

    section("Geometry");
    drawFloat("duct_access_width", "Width", selected->ductAccess.width,
            context.uiState.widthInput, 0.0f, DimensionLimit,
            [](SectorPlacedDuctAccess& access, float value) {
                value = std::max(0.0f, value);
                if (access.width == value) return false;
                access.width = value; return true;
            });
    selected = refresh(); if (!selected) return;
    drawFloat("duct_access_height", "Height", selected->ductAccess.height,
            context.uiState.heightInput, 0.0f, DimensionLimit,
            [](SectorPlacedDuctAccess& access, float value) {
                value = std::max(0.0f, value);
                if (access.height == value) return false;
                access.height = value; return true;
            });
    selected = refresh(); if (!selected) return;
    drawFloat("duct_access_thickness", "Access depth",
            selected->ductAccess.thickness, context.uiState.thicknessInput,
            0.001f, 100.0f,
            [](SectorPlacedDuctAccess& access, float value) {
                value = std::max(0.001f, value);
                if (access.thickness == value) return false;
                access.thickness = value; return true;
            });
    selected = refresh(); if (!selected) return;
    drawFloat("duct_access_horizontal_offset", "Horizontal",
            selected->ductAccess.horizontalOffsetWorld,
            context.uiState.horizontalOffsetInput,
            -DimensionLimit, DimensionLimit,
            [](SectorPlacedDuctAccess& access, float value) {
                if (access.horizontalOffsetWorld == value) return false;
                access.horizontalOffsetWorld = value; return true;
            });
    selected = refresh(); if (!selected) return;
    drawFloat("duct_access_vertical_offset", "Vertical",
            selected->ductAccess.verticalOffsetWorld,
            context.uiState.verticalOffsetInput,
            -DimensionLimit, DimensionLimit,
            [](SectorPlacedDuctAccess& access, float value) {
                if (access.verticalOffsetWorld == value) return false;
                access.verticalOffsetWorld = value; return true;
            });
    selected = refresh(); if (!selected) return;
    drawFloat("duct_access_normal_offset", "Depth offset (+ into duct)",
            selected->ductAccess.normalOffset, context.uiState.normalOffsetInput,
            -DimensionLimit, DimensionLimit,
            [](SectorPlacedDuctAccess& access, float value) {
                if (access.normalOffset == value) return false;
                access.normalOffset = value; return true;
            });
    if (engine::Button(context.ui, context.config, context.input,
                context.assets, "duct_access_fit_portal",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                "Fit to opening")) {
        context.editing.MutateSelected(
                "Fit Duct Access to portal opening",
                [resolved](SectorPlacedRuntimeObject& object) {
                    if (object.kind != "duct_access" || !resolved.valid) return false;
                    auto& access = object.ductAccess;
                    const bool changed = access.width != resolved.portalWidth
                            || access.height != resolved.portalHeight
                            || access.horizontalOffsetWorld != 0.0f
                            || access.verticalOffsetWorld != 0.0f;
                    access.width = resolved.portalWidth;
                    access.height = resolved.portalHeight;
                    access.horizontalOffsetWorld = 0.0f;
                    access.verticalOffsetWorld = 0.0f;
                    return changed;
                });
    }
    y += rowH + gap;

    selected = refresh(); if (!selected) return;
    section("Vent Cover");
    bool coverEnabled = selected->ductAccess.cover.enabled;
    if (engine::Checkbox(context.ui, context.config, context.input,
                context.assets, "duct_access_cover_enabled",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                "Procedural removable cover", coverEnabled)) {
        context.editing.MutateSelected("Updated Vent Cover",
                [coverEnabled](SectorPlacedRuntimeObject& object) {
                    if (object.kind != "duct_access"
                            || object.ductAccess.cover.enabled == coverEnabled) return false;
                    object.ductAccess.cover.enabled = coverEnabled;
                    return true;
                });
    }
    y += rowH + gap;
    selected = refresh(); if (!selected) return;
    drawFloat("duct_cover_thickness", "Cover thickness",
            selected->ductAccess.cover.thickness,
            context.uiState.ductCoverThicknessInput, 0.001f, 1.0f,
            [](SectorPlacedDuctAccess& access, float value) {
                value = std::max(0.001f, value);
                if (access.cover.thickness == value) return false;
                access.cover.thickness = value; return true;
            });
    selected = refresh(); if (!selected) return;
    drawFloat("duct_cover_border", "Frame border",
            selected->ductAccess.cover.frameBorderWidthWorld,
            context.uiState.ductFrameBorderInput, 0.001f, 10.0f,
            [](SectorPlacedDuctAccess& access, float value) {
                value = std::max(0.001f, value);
                if (access.cover.frameBorderWidthWorld == value) return false;
                access.cover.frameBorderWidthWorld = value; return true;
            });
    selected = refresh(); if (!selected) return;
    const SectorEditorInspectorNumericRowLayout countLayout =
            BuildSectorEditorInspectorRightFloatRowLayout(y, contentW, rowH, gap);
    const SectorEditorIntInputResult countResult = DrawLabeledIntInput(
            context.ui, context.config, context.input, context.assets,
            context.font, "duct_cover_louver_count", "Louvers",
            countLayout.labelRect, countLayout.inputRect,
            engine::UITextJustify::Left,
            selected->ductAccess.cover.louverCount,
            context.uiState.ductLouverCountInput, 1, 64, 1);
    if (countResult.changed) {
        context.editing.MutateSelected("Updated Vent Cover",
                [value = std::clamp(countResult.value, 1, 64)](
                        SectorPlacedRuntimeObject& object) {
                    if (object.kind != "duct_access"
                            || object.ductAccess.cover.louverCount == value) return false;
                    object.ductAccess.cover.louverCount = value;
                    return true;
                });
    }
    y += rowH + gap;
    selected = refresh(); if (!selected) return;
    drawFloat("duct_cover_angle", "Louver angle",
            selected->ductAccess.cover.louverAngleDegrees,
            context.uiState.ductLouverAngleInput, -79.9f, 79.9f,
            [](SectorPlacedDuctAccess& access, float value) {
                value = std::clamp(value, -79.9f, 79.9f);
                if (access.cover.louverAngleDegrees == value) return false;
                access.cover.louverAngleDegrees = value; return true;
            });
    selected = refresh(); if (!selected) return;
    drawFloat("duct_cover_speed", "Removal speed",
            selected->ductAccess.cover.removalSpeedWorld,
            context.uiState.ductRemovalSpeedInput, 0.05f, 20.0f,
            [](SectorPlacedDuctAccess& access, float value) {
                value = std::max(0.05f, value);
                if (access.cover.removalSpeedWorld == value) return false;
                access.cover.removalSpeedWorld = value; return true;
            });
    selected = refresh(); if (!selected) return;
    const bool starts = selected->ductAccess.cover.slideSide
            == SectorDuctCoverSlideSide::PortalStart;
    if (engine::Button(context.ui, context.config, context.input,
                context.assets, "duct_cover_slide_side",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                starts ? "Slide: Portal Start" : "Slide: Portal End")) {
        context.editing.MutateSelected("Updated Vent Cover slide side",
                [starts](SectorPlacedRuntimeObject& object) {
                    if (object.kind != "duct_access") return false;
                    object.ductAccess.cover.slideSide = starts
                            ? SectorDuctCoverSlideSide::PortalEnd
                            : SectorDuctCoverSlideSide::PortalStart;
                    return true;
                });
    }
    y += rowH + gap;

    selected = refresh(); if (!selected) return;
    const std::string materialText = TextFormat(
            "Frame: %s\nLouvers: %s",
            selected->ductAccess.cover.frameMaterialId.empty()
                    ? "default" : selected->ductAccess.cover.frameMaterialId.c_str(),
            selected->ductAccess.cover.louverMaterialId.empty()
                    ? "default" : selected->ductAccess.cover.louverMaterialId.c_str());
    engine::Text(context.ui, smallConfig, context.assets,
            Rectangle{0.0f, y, contentW, 52.0f}, context.smallFont,
            materialText.c_str(), engine::UITextJustify::Left,
            context.config.mutedTextColor, true);
    y += 56.0f;
    if (engine::Button(context.ui, context.config, context.input,
                context.assets, "duct_cover_pick_frame_material",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                "Pick Frame Material")) {
        if (!OpenRuntimeDuctTexturePicker(
                    context.state, context.topologyMap,
                    context.authoringGraph, context.textureCatalog,
                    selected->id, false)) {
            context.statusText = "No vent cover material target";
        }
    }
    y += rowH + gap;
    if (engine::Button(context.ui, context.config, context.input,
                context.assets, "duct_cover_pick_louver_material",
                Rectangle{0.0f, y, contentW, rowH}, context.font,
                "Pick Louver Material")) {
        if (!OpenRuntimeDuctTexturePicker(
                    context.state, context.topologyMap,
                    context.authoringGraph, context.textureCatalog,
                    selected->id, true)) {
            context.statusText = "No vent cover material target";
        }
    }
    y += rowH + gap;

    if (engine::Button(context.ui, context.config, context.input,
                context.assets, "sector_editor_delete_runtime_object",
                Rectangle{0.0f, y, contentW, rowH}, context.font, "Delete")) {
        context.deleteRequested = true;
    }
}

} // namespace game
