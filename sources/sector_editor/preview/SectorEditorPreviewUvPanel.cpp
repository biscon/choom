#include "sector_editor/preview/SectorEditorPreviewUvPanel.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <string>

namespace game {
namespace {

void ResetPreviewSurfaceUi(SectorEditorState& state, SectorEditorUiState& uiState)
{
    SectorEditorSelectionServiceContext context{state, uiState, nullptr, nullptr, nullptr};
    ResetSectorEditorSurface3DUiState(context);
}

void OpenPreviewSurfaceTexturePicker(
        SectorEditorPreviewUvPanelContext& context,
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer)
{
    SectorEditorState& state = context.state;
    SectorEditorMaterialEditingService& materialEditing = context.materialEditing;
    bool opened = false;
    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        const TopologySectorTextureField field = TopologyEditTargetSectorTextureField(target.kind);
        opened = HasAuthoringGraphData(state)
                ? materialEditing.OpenTexturePickerForAuthoringFaceAnchor(target.sectorId, field, layer)
                : materialEditing.OpenTexturePickerForSector(target.sectorId, field, layer);
        if (opened && state.texturePicker.open && HasAuthoringGraphData(state)) {
            state.texturePicker.authoringSurface3DFlatTarget = true;
        }
        if (!opened) {
            context.statusText = "No topology sector texture target";
        }
    } else {
        const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
        opened = HasAuthoringGraphData(state)
                ? materialEditing.OpenTexturePickerForAuthoringSide(target.sideDefId, wallPart, layer)
                : materialEditing.OpenTexturePickerForSideDef(target.sideDefId, wallPart, layer);
        if (!opened) {
            context.statusText = "No topology sidedef texture target";
        }
    }
    if (opened && state.texturePicker.open) {
        state.texturePicker.rebuildPreviewOnApply = true;
    }
}

} // namespace

Rectangle BuildSectorEditorPreviewUvPanelRect()
{
    return Rectangle{330.0f, EditorHeight - 252.0f, 1260.0f, 220.0f};
}

bool DrawSectorEditorPreviewUvPanel(SectorEditorPreviewUvPanelContext& context)
{
    engine::UIContext& ui = context.ui;
    const engine::UIConfig& config = context.config;
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    const engine::FontHandle font = context.font;
    SectorEditorState& state = context.state;
    SectorEditorUiState& uiState = context.uiState;
    SectorEditorMaterialEditingService& materialEditing = context.materialEditing;

    const TopologySurfaceEditTarget target = state.selectedTopologySurface3D;
    const bool targetIsMiddle = IsMiddleTopologyEditTarget(target.kind);
    if (targetIsMiddle && state.activeTopologyMaterialLayer != TopologyMaterialLayer::Base) {
        state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
        ResetPreviewSurfaceUi(state, uiState);
    }
    const TopologyMaterialLayer layer = EffectiveTopologyMaterialLayer(
            target.kind,
            state.activeTopologyMaterialLayer);
    const Rectangle panel = context.panelRect;
    DrawRectangleRec(panel, Color{12, 15, 20, 230});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    int portalLineDefId = -1;
    bool portalBlocksPlayer = false;

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            state.selectedSurface3D = SectorSurfaceRef{};
            state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            return false;
        }
    } else {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
        if (sideDef == nullptr) {
            state.selectedSurface3D = SectorSurfaceRef{};
            state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            return false;
        }
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
                state.topologyMap,
                sideDef->lineDefId);
        if (lineDef != nullptr
                && lineDef->frontSideDefId != -1
                && lineDef->backSideDefId != -1) {
            portalLineDefId = lineDef->id;
            portalBlocksPlayer = lineDef->flags.blocksPlayer;
        }
    }
    const std::string targetLabel =
            BuildSectorEditorSurface3DTargetLabel(state, state.selectedSurface3D, target);

    const float margin = 18.0f;
    const float top = panel.y + margin;
    const float inputTop = panel.y + 104.0f;
    const float colW = 132.0f;
    const float gap = 14.0f;
    const float startX = panel.x + 390.0f;

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{panel.x + margin, top, 350.0f, 34.0f},
            font,
            targetLabel.c_str(),
            engine::UITextJustify::Left,
            config.textColor
    );
    if (!targetIsMiddle) {
        const float layerLabelW = 68.0f;
        const float layerButtonW = 78.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{panel.x + margin, top + 36.0f, layerLabelW, 30.0f},
                font,
                "Layer:",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_layer_base",
                    Rectangle{panel.x + margin + layerLabelW, top + 34.0f, layerButtonW, 32.0f},
                    font,
                    "Base",
                    layer == TopologyMaterialLayer::Base)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
            ResetPreviewSurfaceUi(state, uiState);
        }
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_layer_decal",
                    Rectangle{panel.x + margin + layerLabelW + layerButtonW + 8.0f, top + 34.0f, layerButtonW, 32.0f},
                    font,
                    "Decal",
                    layer == TopologyMaterialLayer::Decal)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
            ResetPreviewSurfaceUi(state, uiState);
        }
    }

    const std::string currentTexture = materialEditing.CurrentTextureForSurface(target, layer);
    const bool missingTexture = !currentTexture.empty()
            && FindSectorTopologyTexture(state.topologyMap, currentTexture) == nullptr;
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{panel.x + margin, top + 72.0f, 350.0f, 30.0f},
            font,
            targetIsMiddle
                    ? TextFormat("Middle texture %s", currentTexture.empty() ? "<none>" : currentTexture.c_str())
                    : TextFormat("%s texture %s", TopologyMaterialLayerName(layer), currentTexture.empty() ? "<none>" : currentTexture.c_str()),
            engine::UITextJustify::Left,
            missingTexture ? config.invalidColor : config.mutedTextColor
    );

    const bool decalAssigned = targetIsMiddle
            ? !currentTexture.empty()
            : (layer != TopologyMaterialLayer::Decal || materialEditing.IsDecalAssigned(target));
    const SectorTopologyUvSettings* uv = materialEditing.UvForSurface(target, layer);
    Vector2 uvScale = uv == nullptr ? Vector2{1.0f, 1.0f} : uv->scale;
    Vector2 uvOffset = uv == nullptr ? Vector2{0.0f, 0.0f} : uv->offset;
    if (!decalAssigned) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{startX, inputTop, 390.0f, 34.0f},
                font,
                targetIsMiddle ? "No middle texture assigned" : "No decal assigned",
                engine::UITextJustify::Left,
                config.mutedTextColor);
    }

    auto drawFloat = [&](const char* id, const char* label, float value, engine::UIFloatInputState& inputState, int component, float minValue, float maxValue, float x) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{x, inputTop - 28.0f, colW, 24.0f},
                Rectangle{x, inputTop, colW, 38.0f},
                engine::UITextJustify::Left,
                value,
                inputState,
                minValue,
                maxValue,
                3);
        if (result.changed && result.value != value && result.finite) {
            materialEditing.ApplySurfaceUvValue(
                    target,
                    layer,
                    component,
                    result.value,
                    state.selectedSurface3D.kind,
                    assets);
        }
    };

    if (decalAssigned) {
        drawFloat("sector_editor_3d_uv_scale_u", "Scale U", uvScale.x, uiState.surface3DUvScaleUInput, 0, TopologyUvScaleMin, TopologyUvScaleMax, startX);
        drawFloat("sector_editor_3d_uv_scale_v", "Scale V", uvScale.y, uiState.surface3DUvScaleVInput, 1, TopologyUvScaleMin, TopologyUvScaleMax, startX + (colW + gap));
        drawFloat("sector_editor_3d_uv_offset_u", "Offset U", uvOffset.x, uiState.surface3DUvOffsetUInput, 2, -1024.0f, 1024.0f, startX + (colW + gap) * 2.0f);
        drawFloat("sector_editor_3d_uv_offset_v", "Offset V", uvOffset.y, uiState.surface3DUvOffsetVInput, 3, -1024.0f, 1024.0f, startX + (colW + gap) * 3.0f);
    }

    const float actionTop = inputTop + 52.0f;
    const float actionH = 34.0f;
    const float smallActionW = 96.0f;
    float actionX = startX;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_texture",
                Rectangle{actionX, actionTop, smallActionW, actionH},
                font,
                "Texture")) {
        OpenPreviewSurfaceTexturePicker(context, target, layer);
    }
    actionX += smallActionW + gap;

    if (portalLineDefId != -1) {
        bool blocksPlayer = portalBlocksPlayer;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_linedef_blocks_player",
                    Rectangle{actionX, actionTop, 146.0f, actionH},
                    font,
                    "Blocks Player",
                    blocksPlayer)) {
            if (context.setLineDefBlocksPlayer) {
                context.setLineDefBlocksPlayer(portalLineDefId, blocksPlayer);
            }
        }
        actionX += 146.0f + gap;
    }

    if (decalAssigned) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_reset_uv",
                    Rectangle{actionX, actionTop, smallActionW, actionH},
                    font,
                    "Reset UV")) {
            materialEditing.ResetSurfaceUv(target, layer, state.selectedSurface3D.kind, assets);
        }
        actionX += smallActionW + gap;
    }

    if (targetIsMiddle && decalAssigned) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_clear_middle",
                    Rectangle{actionX, actionTop, 118.0f, actionH},
                    font,
                    "Clear Middle")) {
            materialEditing.ClearMiddleTexture(target, &assets);
        }
        actionX += 118.0f + gap;
    }

    if (!targetIsMiddle && layer == TopologyMaterialLayer::Decal && decalAssigned) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_decal",
                    Rectangle{actionX, actionTop, smallActionW, actionH},
                    font,
                    "Fit Decal")) {
            materialEditing.FitSelectedDecal(target, &assets);
        }
        actionX += smallActionW + gap;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_clear_decal",
                    Rectangle{actionX, actionTop, 104.0f, actionH},
                    font,
                    "Clear Decal")) {
            materialEditing.ClearSurfaceDecal(target, &assets);
        }
        actionX += 104.0f + gap;
    }

    if (IsWallTopologyEditTarget(target.kind) && decalAssigned && layer == TopologyMaterialLayer::Base) {
        const float fitButtonW = 118.0f;
        const float fitTop = panel.y + 194.0f;
        const float fitButtonH = 26.0f;
        const float alignStartX = startX + (fitButtonW + gap) * 3.0f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_width",
                    Rectangle{startX, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Width")) {
            materialEditing.FitSelectedWallMaterial(target, TopologyUvFitMode::Width, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_height",
                    Rectangle{startX + fitButtonW + gap, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Height")) {
            materialEditing.FitSelectedWallMaterial(target, TopologyUvFitMode::Height, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_both",
                    Rectangle{startX + (fitButtonW + gap) * 2.0f, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Both")) {
            materialEditing.FitSelectedWallMaterial(target, TopologyUvFitMode::Both, &assets, layer);
        }
        if (!targetIsMiddle) {
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_align_vertical",
                        Rectangle{alignStartX, fitTop, fitButtonW, fitButtonH},
                        font,
                        "Align Vertical")) {
                materialEditing.AlignSelectedWallMaterialVertical(target, &assets, layer);
            }
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_align_u_prev",
                        Rectangle{alignStartX + fitButtonW + gap, fitTop, fitButtonW, fitButtonH},
                        font,
                        "Align U Prev")) {
                materialEditing.AlignSelectedWallMaterialU(
                        target,
                        TopologyUAlignDirection::Previous,
                        &assets,
                        layer);
            }
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_align_u_next",
                        Rectangle{alignStartX + (fitButtonW + gap) * 2.0f, fitTop, fitButtonW, fitButtonH},
                        font,
                        "Align U Next")) {
                materialEditing.AlignSelectedWallMaterialU(
                        target,
                        TopologyUAlignDirection::Next,
                        &assets,
                        layer);
            }
        }
    }

    if (!targetIsMiddle && layer == TopologyMaterialLayer::Decal && decalAssigned) {
        const SectorTopologyDecalLayer* decal = materialEditing.DecalForSurface(target);
        if (decal != nullptr) {
            const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    "sector_editor_3d_decal_opacity",
                    "Opacity",
                    Rectangle{startX + (colW + gap) * 4.0f, inputTop - 28.0f, colW, 24.0f},
                    Rectangle{startX + (colW + gap) * 4.0f, inputTop, colW, 38.0f},
                    engine::UITextJustify::Left,
                    decal->opacity,
                    uiState.surface3DDecalOpacityInput,
                    0.0f,
                    1.0f,
                    3);
            if (result.changed && result.value != decal->opacity && result.finite) {
                materialEditing.ApplyDecalOpacity(target, result.value, &assets);
            }
            if (decal->emissive) {
                const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                        ui,
                        config,
                        input,
                        assets,
                        font,
                        "sector_editor_3d_decal_bloom_intensity",
                        "Bloom",
                        Rectangle{startX + (colW + gap) * 5.0f, inputTop - 28.0f, colW, 24.0f},
                        Rectangle{startX + (colW + gap) * 5.0f, inputTop, colW, 38.0f},
                        engine::UITextJustify::Left,
                        decal->bloomIntensity,
                        uiState.surface3DDecalBloomIntensityInput,
                        0.0f,
                        10.0f,
                        3);
                if (bloomResult.changed && bloomResult.value != decal->bloomIntensity) {
                    materialEditing.ApplyDecalBloomIntensity(target, bloomResult.value, &assets);
                }
            }
            bool emissive = decal->emissive;
            if (engine::Checkbox(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_decal_emissive",
                        Rectangle{actionX, actionTop, 112.0f, actionH},
                        font,
                        "Emissive",
                        emissive)) {
                materialEditing.ApplyDecalEmissive(target, emissive, &assets);
            }
            actionX += 112.0f + gap;
            const Rectangle label{actionX, actionTop, 36.0f, actionH};
            engine::Text(ui, config, assets, label, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
            const Rectangle swatch{label.x + label.width + 8.0f, label.y + 1.0f, 48.0f, actionH - 2.0f};
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_decal_tint",
                        swatch,
                        font,
                        "")) {
                materialEditing.OpenDecalTintModal(target);
            }
            DrawColorSwatch(config, swatch, DecalTintPreviewColor(decal->tint), config.borderThickness);
        }
    } else if (!targetIsMiddle && layer == TopologyMaterialLayer::Base) {
        if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_copy_material",
                Rectangle{actionX, actionTop, 112.0f, actionH},
                font,
                "Copy Material")) {
            materialEditing.CopyMaterial(target);
        }
        actionX += 112.0f + gap;

        if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_paste_material",
                Rectangle{actionX, actionTop, 112.0f, actionH},
                font,
                "Paste Material")) {
            materialEditing.PasteMaterial(target, assets);
        }
    }

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [panel](engine::InputEvent& event) {
                if (Contains(panel, event.mouseClick.releasePosition)
                        || Contains(panel, event.mouseClick.pressPosition)) {
                    engine::ConsumeEvent(event);
                }
            }
    );
    return true;
}

} // namespace game
