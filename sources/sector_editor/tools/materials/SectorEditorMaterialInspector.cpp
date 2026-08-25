#include "sector_editor/tools/materials/SectorEditorMaterialInspector.h"

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <string>

namespace game {

bool DrawTopologySideDefMaterialInspector(SectorEditorMaterialInspectorContext& context)
{
    engine::UIContext& ui = context.ui;
    const engine::UIConfig& config = context.config;
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    const engine::FontHandle font = context.font;
    const engine::FontHandle smallFont = context.smallFont;
    const engine::UIScrollAreaResult scroll = context.scroll;
    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    SectorTopologyMap& topologyMap = context.topologyMap;
    const SectorAuthoringGraph& authoringGraph = context.authoringGraph;
    SelectionState& selectionState = context.selectionState;
    engine::UIScrollState& inspectorScroll = context.inspectorScroll;
    MaterialEditingUiState& materialUiState = context.materialUiState;
    std::string& statusText = context.statusText;
    const SectorEditorMaterialInspectorCallbacks& callbacks = context.callbacks;
    SectorEditorMaterialEditingService& materialEditing = context.materialEditing;
    SectorEditorTextureCatalogService& textureCatalog = context.textureCatalog;

    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);
    const SectorTopologyLineDef* lineDef =
            (selectionState.topologySelectionKind == TopologySelectionKind::LineDef
             || selectionState.topologySelectionKind == TopologySelectionKind::SideDef)
            ? FindSectorTopologyLineDef(topologyMap, selectionState.selectedTopologyLineDefId)
            : nullptr;
    if (lineDef == nullptr) {
        return false;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    const bool hasEndpoints = GetSectorTopologyLineVertices(topologyMap, *lineDef, start, end);

    float y = 0.0f;
    SectorTopologySideDef* sideDef = selectionState.topologySelectionKind == TopologySelectionKind::SideDef
            ? FindSectorTopologySideDef(topologyMap, selectionState.selectedTopologySideDefId)
            : nullptr;
    if (sideDef != nullptr) {
        selectionState.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
                topologyMap,
                sideDef,
                selectionState.selectedTopologyWallPart);
    }
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 34.0f},
            font,
            sideDef != nullptr
                    ? TextFormat(
                            "Topology %s SideDef: %d",
                            SectorTopologySideKindName(sideDef->side),
                            sideDef->id)
                    : TextFormat("Topology LineDef: %d", lineDef->id),
            engine::UITextJustify::Left,
            config.textColor);
    y += 38.0f;

    if (hasEndpoints) {
        const Vector2 from = SectorTopologyVertexToMap(*start);
        const Vector2 to = SectorTopologyVertexToMap(*end);
        const char* endpointText = TextFormat("Line %d  From %.2f, %.2f  To %.2f, %.2f", lineDef->id, from.x, from.y, to.x, to.y);
        const float endpointHeight = MeasureSectorEditorWrappedTextHeight(
                smallConfig,
                assets,
                smallFont,
                endpointText,
                contentW);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{0.0f, y, contentW, endpointHeight},
                smallFont,
                endpointText,
                engine::UITextJustify::Left,
                smallConfig.mutedTextColor,
                true);
        y += endpointHeight;
    } else {
        const float endpointHeight = MeasureSectorEditorWrappedTextHeight(
                smallConfig,
                assets,
                smallFont,
                "Line endpoints are invalid",
                contentW);
        engine::Text(ui, smallConfig, assets, Rectangle{0.0f, y, contentW, endpointHeight}, smallFont, "Line endpoints are invalid", engine::UITextJustify::Left, config.invalidColor, true);
        y += endpointHeight;
    }

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 38.0f},
            font,
            "Split Linedef retired; derivation auto-splits authoring lines.",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 38.0f + gap;

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 38.0f},
            font,
            "Split At Point retired; move or redraw authoring vertices.",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 38.0f + gap;

    if (sideDef == nullptr) {
        const int preferredSideDefId = selectionState.selectedTopologySideKind == SectorTopologySideKind::Front
                ? lineDef->frontSideDefId
                : lineDef->backSideDefId;
        const SectorTopologySideDef* preferredSideDef = FindSectorTopologySideDef(
                topologyMap,
                preferredSideDefId);
        const SectorTopologySideDef* opposite = preferredSideDef != nullptr
                ? FindOppositeSectorTopologySideDef(topologyMap, preferredSideDef->id)
                : nullptr;
        if (preferredSideDef != nullptr && opposite != nullptr
                && preferredSideDef->sectorId != opposite->sectorId) {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    "Join Sectors retired; remove authoring boundaries instead.",
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 38.0f + gap;
        }

        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 64.0f},
                font,
                preferredSideDef == nullptr
                        ? "Line-only selection: this linedef has no sidedef to edit."
                        : "Select a sidedef to edit wall settings.",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        return true;
    }

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 30.0f},
            font,
            TextFormat("Sector %d  Line %d", sideDef->sectorId, sideDef->lineDefId),
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 34.0f;

    const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(
            topologyMap,
            sideDef->id);
    const bool middleEligible = IsTopologyMiddleEligible(topologyMap, sideDef);
    if (opposite != nullptr) {
        if (opposite->sectorId != sideDef->sectorId) {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    "Join Sectors retired; remove authoring boundaries instead.",
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 38.0f + gap;
        } else {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 34.0f},
                    font,
                    "Join Sectors unavailable: both sides already belong to the same sector.",
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 38.0f;
        }

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_switch_opposite_side",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Switch to opposite side (%s)", SectorTopologySideKindName(opposite->side)))) {
            const int oppositeId = opposite->id;
            callbacks.selectTopologySideDef(
                    oppositeId,
                    ValidTopologyWallPartForSideDef(
                            topologyMap,
                            FindSectorTopologySideDef(topologyMap, oppositeId),
                            selectionState.selectedTopologyWallPart));
            statusText = TextFormat("Selected opposite topology sidedef %d", oppositeId);
            return true;
        }
        y += 38.0f + gap;
    } else {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                "Join Sectors unavailable: opposite side is missing.",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 34.0f;
    }

    if (lineDef->frontSideDefId != -1 && lineDef->backSideDefId != -1) {
        bool blocksPlayer = lineDef->flags.blocksPlayer;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_linedef_blocks_player",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Blocks Player",
                    blocksPlayer)) {
            callbacks.setAuthoringLineDefBlocksPlayer(lineDef->id, blocksPlayer);
            return true;
        }
        y += 36.0f + gap;
    }

    auto drawTextureRow = [&](const char* id, const char* label, const std::string& materialId, TopologyWallPart wallPart, TopologyMaterialLayer layer) {
        const float buttonW = 38.0f;
        const float labelColumnW = 74.0f;
        const Rectangle row{0.0f, y, contentW, 36.0f};
        const bool missing = !materialId.empty() && !textureCatalog.HasTexture(materialId);
        engine::Text(ui, config, assets, Rectangle{row.x, row.y, labelColumnW, row.height}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{row.x + labelColumnW, row.y, row.width - labelColumnW - buttonW - gap, row.height},
                smallFont,
                materialId.empty() ? "<none>" : materialId.c_str(),
                engine::UITextJustify::Left,
                missing ? config.invalidColor : config.mutedTextColor);
        if (engine::Button(ui, config, input, assets, id, Rectangle{row.x + row.width - buttonW, row.y, buttonW, row.height}, font, ">")) {
            if (!materialEditing.OpenMaterialPickerForDerivedSideDef(sideDef->id, wallPart, layer)) {
                statusText = HasAuthoringGraphData(authoringGraph)
                        ? "No derived sidedef authoring material target"
                        : "Cannot edit material: authoring data is required.";
            }
        }
        y += row.height + gap;
    };

    y += 4.0f;
    const int partCount = middleEligible ? 4 : 3;
    const float partButtonW = (contentW - gap * static_cast<float>(partCount - 1)) / static_cast<float>(partCount);
    const TopologyWallPart parts[] = {
            TopologyWallPart::Wall,
            TopologyWallPart::Lower,
            TopologyWallPart::Upper,
            TopologyWallPart::Middle};
    for (int i = 0; i < partCount; ++i) {
        const TopologyWallPart part = parts[i];
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("sector_editor_topology_sidedef_part_%d", i),
                    Rectangle{static_cast<float>(i) * (partButtonW + gap), y, partButtonW, 38.0f},
                    font,
                    TopologyWallPartName(part),
                    selectionState.selectedTopologyWallPart == part)) {
            selectionState.selectedTopologyWallPart = part;
            for (engine::UIFloatInputState& inputState : materialUiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            statusText = TextFormat("Editing topology %s UV", TopologyWallPartStatusName(part));
        }
    }
    y += 38.0f + gap;

    const bool selectedMiddle = selectionState.selectedTopologyWallPart == TopologyWallPart::Middle;
    if (selectedMiddle && selectionState.activeTopologyMaterialLayer != TopologyMaterialLayer::Base) {
        selectionState.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
        for (engine::UIFloatInputState& inputState : materialUiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
        materialUiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
        materialUiState.topologySideDefDecalEmissiveStrengthInput = engine::UIFloatInputState{};
    }
    const TopologySurfaceEditTarget selectedMaterialTarget{
            TopologyWallPartEditTargetKind(selectionState.selectedTopologyWallPart),
            sideDef->sectorId,
            sideDef->lineDefId,
            sideDef->id,
            sideDef->side};
    if (!selectedMiddle) {
        const float layerLabelW = 74.0f;
        const float layerButtonW = (contentW - layerLabelW - gap) * 0.5f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, layerLabelW, 36.0f}, font, "Layer:", engine::UITextJustify::Left, config.mutedTextColor);
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_layer_base",
                    Rectangle{layerLabelW, y, layerButtonW, 36.0f},
                    font,
                    "Base",
                    selectionState.activeTopologyMaterialLayer == TopologyMaterialLayer::Base)) {
            selectionState.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
            for (engine::UIFloatInputState& inputState : materialUiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            materialUiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
            materialUiState.topologySideDefDecalEmissiveStrengthInput = engine::UIFloatInputState{};
        }
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_layer_decal",
                    Rectangle{layerLabelW + layerButtonW + gap, y, layerButtonW, 36.0f},
                    font,
                    "Decal",
                    selectionState.activeTopologyMaterialLayer == TopologyMaterialLayer::Decal)) {
            selectionState.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
            for (engine::UIFloatInputState& inputState : materialUiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            materialUiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
            materialUiState.topologySideDefDecalEmissiveStrengthInput = engine::UIFloatInputState{};
        }
        y += 36.0f + gap;
    }

    SectorTopologyWallPartSettings& selectedPart = TopologyWallPartSettingsFor(*sideDef, selectionState.selectedTopologyWallPart);
    const TopologyMaterialLayer layer = selectedMiddle
            ? TopologyMaterialLayer::Base
            : selectionState.activeTopologyMaterialLayer;
    drawTextureRow(
            "sector_editor_topology_sidedef_pick_selected_part",
            "Material:",
            layer == TopologyMaterialLayer::Decal ? selectedPart.decal.materialId : selectedPart.materialId,
            selectionState.selectedTopologyWallPart,
            layer);

    if (selectedMiddle && selectedPart.materialId.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 32.0f}, font, "No middle texture assigned", engine::UITextJustify::Left, config.mutedTextColor);
        y += 32.0f + gap;
        if (!IsDefaultWallPartSettings(selectedPart)) {
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_topology_sidedef_clear_middle_empty",
                        Rectangle{0.0f, y, contentW, 38.0f},
                        font,
                        "Clear Middle")) {
                materialEditing.ClearMiddleTexture(selectedMaterialTarget, &assets);
            }
            y += 38.0f + gap;
        }
        return true;
    }

    if (layer == TopologyMaterialLayer::Decal && selectedPart.decal.materialId.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 32.0f}, font, "No decal assigned", engine::UITextJustify::Left, config.mutedTextColor);
        y += 32.0f + gap;
        return true;
    }

    const SectorTopologyUvSettings& selectedUv = layer == TopologyMaterialLayer::Decal
            ? selectedPart.decal.uv
            : selectedPart.uv;
    const float uvColumnW = (contentW - gap) * 0.5f;
    const float uvBlockH = 62.0f;
    auto drawUvInput = [&](int stateIndex, const char* id, const char* label, float value, float minValue, float maxValue, Rectangle bounds) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{bounds.x, bounds.y, bounds.width, 26.0f},
                Rectangle{bounds.x, bounds.y + 26.0f, bounds.width, 36.0f},
                engine::UITextJustify::Left,
                value,
                materialUiState.topologySideDefUvInputs[stateIndex],
                minValue,
                maxValue,
                3);
        if (result.changed && result.value != value) {
            if (!result.finite) {
                statusText = "Invalid topology sidedef UV value";
                return false;
            }
            return materialEditing.ApplyInspectorSideDefUvValue(
                    selectedMaterialTarget,
                    layer,
                    stateIndex,
                    result.value,
                    assets);
        }
        return false;
    };

    if (drawUvInput(
            0,
            "sector_editor_topology_sidedef_uv_scale_u",
            "Scale U",
            selectedUv.scale.x,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            Rectangle{0.0f, y, uvColumnW, uvBlockH})) {
        return true;
    }
    if (drawUvInput(
            1,
            "sector_editor_topology_sidedef_uv_scale_v",
            "Scale V",
            selectedUv.scale.y,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH})) {
        return true;
    }
    y += uvBlockH + gap;
    if (drawUvInput(
            2,
            "sector_editor_topology_sidedef_uv_offset_u",
            "Offset U",
            selectedUv.offset.x,
            -1024.0f,
            1024.0f,
            Rectangle{0.0f, y, uvColumnW, uvBlockH})) {
        return true;
    }
    if (drawUvInput(
            3,
            "sector_editor_topology_sidedef_uv_offset_v",
            "Offset V",
            selectedUv.offset.y,
            -1024.0f,
            1024.0f,
            Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH})) {
        return true;
    }
    y += uvBlockH + gap;

    if (layer == TopologyMaterialLayer::Decal) {
        const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_topology_sidedef_decal_opacity",
                "Opacity:",
                Rectangle{0.0f, y, 82.0f, rowH},
                Rectangle{82.0f, y, contentW - 82.0f, rowH},
                engine::UITextJustify::Left,
                selectedPart.decal.opacity,
                materialUiState.topologySideDefDecalOpacityInput,
                0.0f,
                1.0f,
                3);
        if (opacityResult.changed && opacityResult.value != selectedPart.decal.opacity && opacityResult.finite) {
            materialEditing.ApplyDecalOpacity(selectedMaterialTarget, opacityResult.value, &assets);
        }
        y += rowH + gap;

        bool emissive = selectedPart.decal.emissive;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_decal_emissive",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Emissive",
                    emissive)) {
            materialEditing.ApplyDecalEmissive(selectedMaterialTarget, emissive, &assets);
        }
        y += 36.0f + gap;

        if (selectedPart.decal.emissive) {
            const SectorEditorFloatInputResult emissiveStrengthResult = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    "sector_editor_topology_sidedef_decal_emissive_strength",
                    "Emissive strength:",
                    Rectangle{0.0f, y, 82.0f, rowH},
                    Rectangle{82.0f, y, contentW - 82.0f, rowH},
                    engine::UITextJustify::Left,
                    selectedPart.decal.bloomIntensity,
                    materialUiState.topologySideDefDecalEmissiveStrengthInput,
                    0.0f,
                    10.0f,
                    3);
            if (emissiveStrengthResult.changed && emissiveStrengthResult.value != selectedPart.decal.bloomIntensity) {
                materialEditing.ApplyDecalEmissiveStrength(
                        selectedMaterialTarget,
                        emissiveStrengthResult.value,
                        &assets);
            }
            y += rowH + gap;
        }

        engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
        const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_decal_tint",
                    swatchLocal,
                    font,
                    "")) {
            materialEditing.OpenDecalTintModal(selectedMaterialTarget);
        }
        const Rectangle swatchScreen{
                scroll.viewport.x + swatchLocal.x,
                scroll.viewport.y - inspectorScroll.offset.y + swatchLocal.y,
                swatchLocal.width,
                swatchLocal.height};
        DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(selectedPart.decal.tint), config.borderThickness);
        y += rowH + gap;

        const float decalButtonW = (contentW - gap) * 0.5f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_fit_decal",
                    Rectangle{0.0f, y, decalButtonW, 38.0f},
                    font,
                    "Fit Decal")) {
            materialEditing.FitSelectedDecal(selectedMaterialTarget, &assets);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_clear_decal",
                    Rectangle{decalButtonW + gap, y, decalButtonW, 38.0f},
                    font,
                    "Clear Decal")) {
            materialEditing.ClearSurfaceDecal(selectedMaterialTarget, &assets);
        }
        y += 38.0f + gap;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_reset_uv",
                Rectangle{0.0f, y, contentW, 38.0f},
                font,
                TextFormat("Reset %s UV", TopologyWallPartName(selectionState.selectedTopologyWallPart)))) {
        if (materialEditing.ResetInspectorSideDefUv(selectedMaterialTarget, layer, assets)) {
            return true;
        }
    }
    y += 38.0f + gap;

    const float fitButtonW = (contentW - gap * 2.0f) / 3.0f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_width",
                Rectangle{0.0f, y, fitButtonW, 34.0f},
                font,
                "Fit Width")) {
        materialEditing.FitSelectedWallMaterial(
                selectedMaterialTarget,
                TopologyUvFitMode::Width,
                &assets,
                layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_height",
                Rectangle{fitButtonW + gap, y, fitButtonW, 34.0f},
                font,
                "Fit Height")) {
        materialEditing.FitSelectedWallMaterial(
                selectedMaterialTarget,
                TopologyUvFitMode::Height,
                &assets,
                layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_both",
                Rectangle{(fitButtonW + gap) * 2.0f, y, fitButtonW, 34.0f},
                font,
                "Fit Both")) {
        materialEditing.FitSelectedWallMaterial(
                selectedMaterialTarget,
                TopologyUvFitMode::Both,
                &assets,
                layer);
    }
    y += 34.0f + gap;

    if (selectedMiddle) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_clear_middle",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    "Clear Middle")) {
            materialEditing.ClearMiddleTexture(selectedMaterialTarget, &assets);
        }
        return true;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_vertical",
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                "Align Vertical")) {
        materialEditing.AlignSelectedWallMaterialVertical(
                selectedMaterialTarget,
                &assets,
                layer);
    }
    y += 34.0f + gap;

    const float alignButtonW = (contentW - gap) * 0.5f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_u_prev",
                Rectangle{0.0f, y, alignButtonW, 34.0f},
                font,
                "Align U Prev")) {
        materialEditing.AlignSelectedWallMaterialU(
                selectedMaterialTarget,
                TopologyUAlignDirection::Previous,
                &assets,
                layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_u_next",
                Rectangle{alignButtonW + gap, y, alignButtonW, 34.0f},
                font,
                "Align U Next")) {
        materialEditing.AlignSelectedWallMaterialU(
                selectedMaterialTarget,
                TopologyUAlignDirection::Next,
                &assets,
                layer);
    }

    return true;
}

} // namespace game
