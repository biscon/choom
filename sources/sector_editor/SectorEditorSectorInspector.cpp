#include "sector_editor/SectorEditorSectorInspector.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace game {

float SectorInspectorContentHeight(float rowH, float gap, bool hasIdError)
{
    const float textureRowH = 36.0f + gap;
    const float uvSettingsH = 2.0f * (62.0f + gap);
    const float materialSurfaceSectionH = 18.0f + 30.0f
            + (36.0f + gap) // Layer toggle.
            + textureRowH
            + uvSettingsH
            + rowH + gap // Opacity.
            + 36.0f + gap // Emissive.
            + rowH + gap // Bloom intensity for emissive decals.
            + rowH + gap // Tint.
            + 36.0f + gap; // Fit/Clear decal action row.
    const float defaultWallSectionH = 18.0f + 30.0f + textureRowH + uvSettingsH;

    float height = 38.0f; // Sector title.
    height += rowH + gap; // Name.
    if (hasIdError) {
        height += 36.0f;
    }
    height += 3.0f * (rowH + gap); // Delete/insert/cut.
    height += 2.0f * (rowH + gap); // Floor/ceiling heights.
    height += 18.0f + 30.0f; // Lighting separator/title.
    height += rowH + gap; // Ambient intensity.
    height += 3.0f * (rowH + gap); // RGB.
    height += 36.0f + gap; // Ambient swatch.
    height += 2.0f * materialSurfaceSectionH; // Floor/ceiling material sections.
    height += 3.0f * defaultWallSectionH; // Default wall/lower/upper sections.
    height += 96.0f; // Bottom breathing room for the last controls.
    return height;
}

bool DrawTopologySectorInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologySector& sector,
        SectorEditorState& state,
        SectorEditorUiState& uiState,
        const SectorEditorSectorInspectorCallbacks& callbacks)
{
    float y = 0.0f;
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 34.0f},
            font,
            TextFormat("Topology Sector: %d", sector.id),
            engine::UITextJustify::Left,
            config.textColor);
    y += 38.0f;

    const float labelW = 92.0f;
    const float numberFieldW = 112.0f;

    engine::Text(ui, config, assets, Rectangle{0.0f, y, labelW, rowH}, font, "Name", engine::UITextJustify::Left, config.mutedTextColor);
    const engine::UITextInputResult nameResult = engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_selected_topology_sector_name",
            Rectangle{labelW, y, contentW - labelW, rowH},
            font,
            uiState.selectedSectorIdBuffer,
            sizeof(uiState.selectedSectorIdBuffer),
            0,
            sizeof(uiState.selectedSectorIdBuffer) - 1,
            engine::UITextJustify::Left);
    if (nameResult.submitted) {
        callbacks.tryRenameSelectedTopologySector();
    }
    y += rowH + gap;

    if (!uiState.idEditError.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 34.0f}, font, uiState.idEditError.c_str(), engine::UITextJustify::Left, config.invalidColor);
        y += 36.0f;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_delete_sector",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Delete Sector")) {
        callbacks.openDeleteSelectedTopologySectorConfirmation();
    }
    y += rowH + gap;

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_insert_sector_inside",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Insert Sector Inside")) {
        callbacks.startInsertSectorInside();
    }
    y += rowH + gap;

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_cut_sector",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Cut Sector")) {
        callbacks.startPendingTopologySectorCut();
        return true;
    }
    y += rowH + gap;

    auto drawHeight = [&](const char* id, const char* label, float current, engine::UIFloatInputState& inputState, bool floorField) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, labelW, rowH},
                Rectangle{labelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                current,
                inputState,
                -512.0f,
                512.0f,
                2);
        if (result.changed && result.value != current) {
            const float nextFloor = floorField ? result.value : sector.floorZ;
            const float nextCeiling = floorField ? sector.ceilingZ : result.value;
            if (!std::isfinite(nextFloor) || !std::isfinite(nextCeiling) || nextCeiling <= nextFloor) {
                callbacks.setStatusText("Invalid topology sector heights: ceiling must be greater than floor");
            } else {
                callbacks.applySectorHeights(nextFloor, nextCeiling);
            }
        }
        y += rowH + gap;
    };

    drawHeight("sector_editor_topology_floor", "Floor:", sector.floorZ, uiState.floorInput, true);
    drawHeight("sector_editor_topology_ceiling", "Ceiling:", sector.ceilingZ, uiState.ceilingInput, false);

    bool ceilingSky = sector.ceilingSky;
    if (engine::Checkbox(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_ceiling_sky",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Ceiling Sky",
                ceilingSky)) {
        callbacks.applySectorCeilingSky(ceilingSky);
    }
    y += rowH + gap;

    engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
    y += 18.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Lighting", engine::UITextJustify::Left, config.textColor);
    y += 30.0f;

    float ambientIntensity = ClampAmbientIntensity(sector.ambientIntensity);
    const SectorEditorFloatInputResult ambientResult = DrawLabeledFloatInput(
            ui,
            config,
            input,
            assets,
            font,
            "sector_editor_topology_ambient_intensity",
            "Intensity:",
            Rectangle{0.0f, y, labelW, rowH},
            Rectangle{labelW, y, numberFieldW, rowH},
            engine::UITextJustify::Right,
            ambientIntensity,
            uiState.ambientIntensityInput,
            0.0f,
            1.0f,
            3);
    if (ambientResult.changed && ambientResult.value != sector.ambientIntensity) {
        callbacks.applySectorAmbientIntensity(ambientResult.value);
    }
    y += rowH + gap;

    auto drawAmbientChannel = [&](const char* id, const char* label, unsigned char& channel, engine::UIIntInputState& inputState) {
        const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, labelW, rowH},
                Rectangle{labelW, y, contentW - labelW, rowH},
                engine::UITextJustify::Right,
                channel,
                inputState);
        if (result.changed && result.channel != channel) {
            Color nextColor = sector.ambientColor;
            unsigned char& nextChannel =
                    &channel == &sector.ambientColor.r ? nextColor.r
                    : &channel == &sector.ambientColor.g ? nextColor.g
                    : nextColor.b;
            nextChannel = result.channel;
            nextColor.a = 255;
            callbacks.applySectorAmbientColor(nextColor);
        }
        y += rowH + gap;
    };
    drawAmbientChannel("sector_editor_topology_ambient_r", "R:", sector.ambientColor.r, uiState.ambientRedInput);
    drawAmbientChannel("sector_editor_topology_ambient_g", "G:", sector.ambientColor.g, uiState.ambientGreenInput);
    drawAmbientChannel("sector_editor_topology_ambient_b", "B:", sector.ambientColor.b, uiState.ambientBlueInput);

    const Rectangle swatch{
            scroll.viewport.x + labelW,
            scroll.viewport.y - uiState.inspectorScroll.offset.y + y + 2.0f,
            std::min(120.0f, contentW - labelW),
            28.0f};
    DrawColorSwatch(config, swatch, TopologySectorAmbientPreviewColor(sector, 255), 1.0f);
    y += 36.0f + gap;

    auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TopologySectorTextureField field, TopologyMaterialLayer layer) {
        const float buttonW = 38.0f;
        const float labelColumnW = 82.0f;
        const Rectangle row{0.0f, y, contentW, 36.0f};
        const bool missing = !textureId.empty() && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
        engine::Text(ui, config, assets, Rectangle{row.x, row.y, labelColumnW, row.height}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{row.x + labelColumnW, row.y, row.width - labelColumnW - buttonW - gap, row.height},
                font,
                textureId.empty() ? "<none>" : textureId.c_str(),
                engine::UITextJustify::Left,
                missing ? config.invalidColor : config.mutedTextColor);
        if (engine::Button(ui, config, input, assets, id, Rectangle{row.x + row.width - buttonW, row.y, buttonW, row.height}, font, ">")) {
            callbacks.openTopologyTexturePicker(sector.id, field, layer);
        }
        y += row.height + gap;
    };

    auto drawUvSettings = [&](const char* idPrefix, SectorTopologyUvSettings& uv, int stateOffset) {
        const float uvColumnW = (contentW - gap) * 0.5f;
        const float uvBlockH = 62.0f;
        auto drawFloat = [&](int stateIndex, const char* suffix, const char* label, float value, float minValue, float maxValue, Rectangle bounds, auto applyValue) {
            const std::string inputId = std::string(idPrefix) + suffix;
            const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    inputId.c_str(),
                    label,
                    Rectangle{bounds.x, bounds.y, bounds.width, 26.0f},
                    Rectangle{bounds.x, bounds.y + 26.0f, bounds.width, 36.0f},
                    engine::UITextJustify::Left,
                    value,
                    uiState.topologySectorUvInputs[stateOffset + stateIndex],
                    minValue,
                    maxValue,
                    3);
            if (result.changed && result.value != value && result.finite) {
                const SectorTopologyUvSettings previousUv = uv;
                applyValue(result.value);
                const SectorTopologyUvSettings nextUv = uv;
                uv = previousUv;
                state.topologyRenderWarning.clear();
                const TopologySectorTextureField field =
                        stateOffset == 0 ? TopologySectorTextureField::Floor
                        : stateOffset == 4 ? TopologySectorTextureField::Ceiling
                        : stateOffset == 8 ? TopologySectorTextureField::DefaultWall
                        : stateOffset == 12 ? TopologySectorTextureField::DefaultLower
                        : stateOffset == 16 ? TopologySectorTextureField::DefaultUpper
                        : TopologySectorTextureField::None;
                callbacks.applySectorUv(field, nextUv);
            }
        };

        drawFloat(0, "_scale_u", "Scale U", uv.scale.x, TopologyUvScaleMin, TopologyUvScaleMax, Rectangle{0.0f, y, uvColumnW, uvBlockH}, [&](float value) { uv.scale.x = value; });
        drawFloat(1, "_scale_v", "Scale V", uv.scale.y, TopologyUvScaleMin, TopologyUvScaleMax, Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH}, [&](float value) { uv.scale.y = value; });
        y += uvBlockH + gap;
        drawFloat(2, "_offset_u", "Offset U", uv.offset.x, -1024.0f, 1024.0f, Rectangle{0.0f, y, uvColumnW, uvBlockH}, [&](float value) { uv.offset.x = value; });
        drawFloat(3, "_offset_v", "Offset V", uv.offset.y, -1024.0f, 1024.0f, Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH}, [&](float value) { uv.offset.y = value; });
        y += uvBlockH + gap;
    };

    auto drawLayerToggle = [&](const char* idPrefix) {
        const float labelColumnW = 82.0f;
        const float buttonW = (contentW - labelColumnW - gap) * 0.5f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, labelColumnW, 36.0f}, font, "Layer:", engine::UITextJustify::Left, config.mutedTextColor);
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_layer_base", idPrefix),
                    Rectangle{labelColumnW, y, buttonW, 36.0f},
                    font,
                    "Base",
                    state.activeTopologyMaterialLayer == TopologyMaterialLayer::Base)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
            for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
        }
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_layer_decal", idPrefix),
                    Rectangle{labelColumnW + buttonW + gap, y, buttonW, 36.0f},
                    font,
                    "Decal",
                    state.activeTopologyMaterialLayer == TopologyMaterialLayer::Decal)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
            for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
        }
        y += 36.0f + gap;
    };

    auto drawSurfaceSection = [&](const char* title, const char* textureButtonId, const std::string& textureId, TopologySectorTextureField field, const char* uvPrefix, SectorTopologyUvSettings& uv, SectorTopologyDecalLayer& decal, int stateOffset, int opacityStateIndex, TopologySurfaceEditTargetKind materialKind) {
        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, title, engine::UITextJustify::Left, config.textColor);
        y += 30.0f;
        const TopologySurfaceEditTarget target{materialKind, sector.id};
        drawLayerToggle(uvPrefix);
        const TopologyMaterialLayer layer = state.activeTopologyMaterialLayer;
        if (layer == TopologyMaterialLayer::Base) {
            const float buttonW = (contentW - gap) * 0.5f;
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        TextFormat("%s_copy_material", uvPrefix),
                        Rectangle{0.0f, y, buttonW, 36.0f},
                        font,
                        "Copy")) {
                callbacks.copyTopologyMaterial(target);
            }
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        TextFormat("%s_paste_material", uvPrefix),
                        Rectangle{buttonW + gap, y, buttonW, 36.0f},
                        font,
                        "Paste")) {
                callbacks.pasteTopologyMaterial(target, assets);
            }
            y += 36.0f + gap;
            drawTextureRow(textureButtonId, "Texture:", textureId, field, TopologyMaterialLayer::Base);
            drawUvSettings(uvPrefix, uv, stateOffset);
            return;
        }

        drawTextureRow(textureButtonId, "Texture:", decal.textureId, field, TopologyMaterialLayer::Decal);
        if (decal.textureId.empty()) {
            engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 32.0f}, font, "No decal assigned", engine::UITextJustify::Left, config.mutedTextColor);
            y += 32.0f + gap;
            return;
        }

        drawUvSettings(uvPrefix, decal.uv, stateOffset);
        const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                TextFormat("%s_decal_opacity", uvPrefix),
                "Opacity:",
                Rectangle{0.0f, y, 82.0f, rowH},
                Rectangle{82.0f, y, contentW - 82.0f, rowH},
                engine::UITextJustify::Left,
                decal.opacity,
                uiState.topologySectorDecalOpacityInputs[opacityStateIndex],
                0.0f,
                1.0f,
                3);
        if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
            callbacks.applySurfaceDecalOpacity(target, opacityResult.value);
        }
        y += rowH + gap;
        bool emissive = decal.emissive;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_decal_emissive", uvPrefix),
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Emissive",
                    emissive)) {
            callbacks.applySurfaceDecalEmissive(target, emissive);
        }
        y += 36.0f + gap;

        if (decal.emissive) {
            const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    TextFormat("%s_decal_bloom_intensity", uvPrefix),
                    "Bloom:",
                    Rectangle{0.0f, y, 82.0f, rowH},
                    Rectangle{82.0f, y, contentW - 82.0f, rowH},
                    engine::UITextJustify::Left,
                    decal.bloomIntensity,
                    uiState.topologySectorDecalBloomIntensityInputs[opacityStateIndex],
                    0.0f,
                    10.0f,
                    3);
            if (bloomResult.changed && bloomResult.value != decal.bloomIntensity) {
                callbacks.applySurfaceDecalBloomIntensity(target, bloomResult.value);
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
                    TextFormat("%s_decal_tint", uvPrefix),
                    swatchLocal,
                    font,
                    "")) {
            callbacks.openDecalTintModal(target);
        }
        const Rectangle swatchScreen{
                scroll.viewport.x + swatchLocal.x,
                scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                swatchLocal.width,
                swatchLocal.height};
        DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(decal.tint), config.borderThickness);
        y += rowH + gap;

        const float decalButtonW = (contentW - gap) * 0.5f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_fit_decal", uvPrefix),
                    Rectangle{0.0f, y, decalButtonW, 36.0f},
                    font,
                    "Fit Decal")) {
            callbacks.fitSelectedDecal(target);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("%s_clear_decal", uvPrefix),
                    Rectangle{decalButtonW + gap, y, decalButtonW, 36.0f},
                    font,
                    "Clear Decal")) {
            callbacks.clearSurfaceDecal(target);
        }
        y += 36.0f + gap;
    };

    drawSurfaceSection("Floor", "sector_editor_topology_pick_floor", sector.floorTextureId, TopologySectorTextureField::Floor, "sector_editor_topology_floor_uv", sector.floorUv, sector.floorDecal, 0, 0, TopologySurfaceEditTargetKind::SectorFloor);
    drawSurfaceSection("Ceiling", "sector_editor_topology_pick_ceiling", sector.ceilingTextureId, TopologySectorTextureField::Ceiling, "sector_editor_topology_ceiling_uv", sector.ceilingUv, sector.ceilingDecal, 4, 1, TopologySurfaceEditTargetKind::SectorCeiling);

    auto drawWallDefaultSection = [&](const char* title, const char* textureButtonId, SectorTopologyWallPartSettings& part, TopologySectorTextureField field, const char* uvPrefix, int stateOffset) {
        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, title, engine::UITextJustify::Left, config.textColor);
        y += 30.0f;
        drawTextureRow(textureButtonId, "Texture:", part.textureId, field, TopologyMaterialLayer::Base);
        drawUvSettings(uvPrefix, part.uv, stateOffset);
    };

    drawWallDefaultSection("Default Wall", "sector_editor_topology_pick_default_wall", sector.defaultWall, TopologySectorTextureField::DefaultWall, "sector_editor_topology_wall_uv", 8);
    drawWallDefaultSection("Default Lower", "sector_editor_topology_pick_default_lower", sector.defaultLower, TopologySectorTextureField::DefaultLower, "sector_editor_topology_lower_uv", 12);
    drawWallDefaultSection("Default Upper", "sector_editor_topology_pick_default_upper", sector.defaultUpper, TopologySectorTextureField::DefaultUpper, "sector_editor_topology_upper_uv", 16);

    return true;
}

} // namespace game
