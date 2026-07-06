#include "sector_editor/tools/materials/SectorEditorMaterialInspector.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
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
    SectorEditorState& state = context.state;
    SectorEditorUiState& uiState = context.uiState;
    std::string& statusText = context.statusText;
    const SectorEditorMaterialInspectorCallbacks& callbacks = context.callbacks;
    SectorEditorMaterialEditingService& materialEditing = context.materialEditing;

    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);
    const SectorTopologyLineDef* lineDef =
            (state.topologySelectionKind == TopologySelectionKind::LineDef
             || state.topologySelectionKind == TopologySelectionKind::SideDef)
            ? FindSectorTopologyLineDef(state.topologyMap, state.selectedTopologyLineDefId)
            : nullptr;
    if (lineDef == nullptr) {
        return false;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    const bool hasEndpoints = GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end);

    float y = 0.0f;
    SectorTopologySideDef* sideDef = state.topologySelectionKind == TopologySelectionKind::SideDef
            ? FindSectorTopologySideDef(state.topologyMap, state.selectedTopologySideDefId)
            : nullptr;
    if (sideDef != nullptr) {
        state.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
                state.topologyMap,
                sideDef,
                state.selectedTopologyWallPart);
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
        const int preferredSideDefId = state.selectedTopologySideKind == SectorTopologySideKind::Front
                ? lineDef->frontSideDefId
                : lineDef->backSideDefId;
        const SectorTopologySideDef* preferredSideDef = FindSectorTopologySideDef(
                state.topologyMap,
                preferredSideDefId);
        const SectorTopologySideDef* opposite = preferredSideDef != nullptr
                ? FindOppositeSectorTopologySideDef(state.topologyMap, preferredSideDef->id)
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
            state.topologyMap,
            sideDef->id);
    const bool middleEligible = IsTopologyMiddleEligible(state.topologyMap, sideDef);
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
                            state.topologyMap,
                            FindSectorTopologySideDef(state.topologyMap, oppositeId),
                            state.selectedTopologyWallPart));
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
            callbacks.setLineDefBlocksPlayer(lineDef->id, blocksPlayer);
            return true;
        }
        y += 36.0f + gap;
    }

    auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TopologyWallPart wallPart, TopologyMaterialLayer layer) {
        const float buttonW = 38.0f;
        const float labelColumnW = 74.0f;
        const Rectangle row{0.0f, y, contentW, 36.0f};
        const bool missing = !textureId.empty() && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
        engine::Text(ui, config, assets, Rectangle{row.x, row.y, labelColumnW, row.height}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{row.x + labelColumnW, row.y, row.width - labelColumnW - buttonW - gap, row.height},
                smallFont,
                textureId.empty() ? "<none>" : textureId.c_str(),
                engine::UITextJustify::Left,
                missing ? config.invalidColor : config.mutedTextColor);
        if (engine::Button(ui, config, input, assets, id, Rectangle{row.x + row.width - buttonW, row.y, buttonW, row.height}, font, ">")) {
            callbacks.openSideDefTexturePicker(sideDef->id, wallPart, layer);
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
                    state.selectedTopologyWallPart == part)) {
            state.selectedTopologyWallPart = part;
            for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            statusText = TextFormat("Editing topology %s UV", TopologyWallPartStatusName(part));
        }
    }
    y += 38.0f + gap;

    const bool selectedMiddle = state.selectedTopologyWallPart == TopologyWallPart::Middle;
    if (selectedMiddle && state.activeTopologyMaterialLayer != TopologyMaterialLayer::Base) {
        state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
        for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
        uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
        uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
    }
    const TopologySurfaceEditTarget selectedMaterialTarget{
            TopologyWallPartEditTargetKind(state.selectedTopologyWallPart),
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
                    state.activeTopologyMaterialLayer == TopologyMaterialLayer::Base)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
            for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
            uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
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
                    state.activeTopologyMaterialLayer == TopologyMaterialLayer::Decal)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
            for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
            uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
        }
        y += 36.0f + gap;
    }

    SectorTopologyWallPartSettings& selectedPart = TopologyWallPartSettingsFor(*sideDef, state.selectedTopologyWallPart);
    const TopologyMaterialLayer layer = selectedMiddle
            ? TopologyMaterialLayer::Base
            : state.activeTopologyMaterialLayer;
    drawTextureRow(
            "sector_editor_topology_sidedef_pick_selected_part",
            "Texture:",
            layer == TopologyMaterialLayer::Decal ? selectedPart.decal.textureId : selectedPart.textureId,
            state.selectedTopologyWallPart,
            layer);

    if (selectedMiddle && selectedPart.textureId.empty()) {
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

    if (layer == TopologyMaterialLayer::Decal && selectedPart.decal.textureId.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 32.0f}, font, "No decal assigned", engine::UITextJustify::Left, config.mutedTextColor);
        y += 32.0f + gap;
        return true;
    }

    if (layer == TopologyMaterialLayer::Base && !selectedMiddle) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_copy_material",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Copy %s Material", TopologyWallPartName(state.selectedTopologyWallPart)))) {
            materialEditing.CopyMaterial(selectedMaterialTarget);
        }
        y += 38.0f + gap;

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_paste_material",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Paste %s Material", TopologyWallPartName(state.selectedTopologyWallPart)))) {
            materialEditing.PasteMaterial(selectedMaterialTarget, assets);
        }
        y += 38.0f + gap;
    }

    SectorTopologyUvSettings& selectedUv = layer == TopologyMaterialLayer::Decal ? selectedPart.decal.uv : selectedPart.uv;
    const float uvColumnW = (contentW - gap) * 0.5f;
    const float uvBlockH = 62.0f;
    auto drawUvInput = [&](int stateIndex, const char* id, const char* label, float value, float minValue, float maxValue, Rectangle bounds, auto applyValue) {
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
                uiState.topologySideDefUvInputs[stateIndex],
                minValue,
                maxValue,
                3);
        if (result.changed && result.value != value) {
            if (!result.finite) {
                statusText = "Invalid topology sidedef UV value";
                return;
            }
            applyValue(result.value);
            materialEditing.ApplyInspectorSideDefUvValue(
                    selectedMaterialTarget,
                    layer,
                    stateIndex,
                    result.value,
                    assets);
        }
    };

    drawUvInput(
            0,
            "sector_editor_topology_sidedef_uv_scale_u",
            "Scale U",
            selectedUv.scale.x,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            Rectangle{0.0f, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.scale.x = value; });
    drawUvInput(
            1,
            "sector_editor_topology_sidedef_uv_scale_v",
            "Scale V",
            selectedUv.scale.y,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.scale.y = value; });
    y += uvBlockH + gap;
    drawUvInput(
            2,
            "sector_editor_topology_sidedef_uv_offset_u",
            "Offset U",
            selectedUv.offset.x,
            -1024.0f,
            1024.0f,
            Rectangle{0.0f, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.offset.x = value; });
    drawUvInput(
            3,
            "sector_editor_topology_sidedef_uv_offset_v",
            "Offset V",
            selectedUv.offset.y,
            -1024.0f,
            1024.0f,
            Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.offset.y = value; });
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
                uiState.topologySideDefDecalOpacityInput,
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
            const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    "sector_editor_topology_sidedef_decal_bloom_intensity",
                    "Bloom:",
                    Rectangle{0.0f, y, 82.0f, rowH},
                    Rectangle{82.0f, y, contentW - 82.0f, rowH},
                    engine::UITextJustify::Left,
                    selectedPart.decal.bloomIntensity,
                    uiState.topologySideDefDecalBloomIntensityInput,
                    0.0f,
                    10.0f,
                    3);
            if (bloomResult.changed && bloomResult.value != selectedPart.decal.bloomIntensity) {
                materialEditing.ApplyDecalBloomIntensity(
                        selectedMaterialTarget,
                        bloomResult.value,
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
                scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
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
                TextFormat("Reset %s UV", TopologyWallPartName(state.selectedTopologyWallPart)))) {
        materialEditing.ResetInspectorSideDefUv(selectedMaterialTarget, layer, assets);
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
