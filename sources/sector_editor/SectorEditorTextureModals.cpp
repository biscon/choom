#include "sector_editor/SectorEditorTextureModals.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTextureTypes.h"

#include <algorithm>
#include <cstdio>

namespace game {

namespace {

void PopulateTexturePickerOptions(TexturePickerState& picker, const SectorTopologyMap& map, const std::string& currentTexture)
{
    picker.selectedTextureIndex = 0;
    picker.scroll = engine::UIScrollState{};
    picker.textureIds.clear();
    picker.optionLabels.clear();

    const std::vector<std::string> textureIds = SortedSectorTopologyTextureIds(map);
    picker.textureIds.insert(picker.textureIds.end(), textureIds.begin(), textureIds.end());

    for (size_t i = 0; i < picker.textureIds.size(); ++i) {
        picker.optionLabels.push_back(picker.textureIds[i].c_str());
        if (picker.textureIds[i] == currentTexture) {
            picker.selectedTextureIndex = static_cast<int>(i);
        }
    }
}

} // namespace

void DrawAddMapTextureModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        AddMapTextureState& modalState,
        const SectorEditorAddTextureModalCallbacks& callbacks)
{
    if (!modalState.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&callbacks](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    callbacks.close();
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    callbacks.addSelected();
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!modalState.open) {
        return;
    }

    callbacks.refreshPreview();

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});

    const Rectangle modal{
            (EditorWidth - 880.0f) * 0.5f,
            (EditorHeight - 660.0f) * 0.5f,
            880.0f,
            660.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 18.0f;
    engine::Text(config, assets, Rectangle{modal.x + 22.0f, y, modal.width - 44.0f, 36.0f}, font, "Add Map Texture");
    y += 50.0f;

    const Rectangle listBounds{modal.x + 22.0f, y, 380.0f, 470.0f};
    const Vector2 contentSize{
            listBounds.width,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(modalState.optionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_add_texture_scroll",
            listBounds,
            contentSize,
            modalState.scroll
    );
    if (!modalState.optionLabels.empty()) {
        const int oldSelection = modalState.selectedPathIndex;
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_add_texture_list",
                Rectangle{0.0f, 0.0f, listBounds.width - (scroll.scrollY ? config.scrollbarSize : 0.0f), contentSize.y},
                font,
                modalState.optionLabels.data(),
                modalState.optionLabels.size(),
                modalState.selectedPathIndex
        );
        if (modalState.selectedPathIndex != oldSelection) {
            callbacks.selectPath(modalState.selectedPathIndex);
        }
    }
    engine::EndScrollArea(ui, config, input, scroll, modalState.scroll);

    if (!modalState.scanMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{listBounds.x, listBounds.y + listBounds.height + 8.0f, listBounds.width, 34.0f},
                font,
                modalState.scanMessage.c_str(),
                engine::UITextJustify::Left,
                modalState.paths.empty() ? config.invalidColor : config.mutedTextColor
        );
    }

    const float rightX = modal.x + 430.0f;
    y = modal.y + 68.0f;
    const Rectangle previewBounds{rightX, y, 410.0f, 260.0f};
    engine::Image(config, assets, previewBounds, modalState.previewTexture);
    y += 276.0f;

    const std::string selectedPath = modalState.selectedPathIndex >= 0
            && modalState.selectedPathIndex < static_cast<int>(modalState.paths.size())
            ? modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)]
            : std::string{};
    engine::Text(config, assets, Rectangle{rightX, y, 410.0f, 68.0f}, font, TextFormat("Path: %s", selectedPath.empty() ? "<none>" : selectedPath.c_str()), engine::UITextJustify::Left, config.mutedTextColor);
    y += 76.0f;

    engine::Text(config, assets, Rectangle{rightX, y, 110.0f, 38.0f}, font, "Texture ID", engine::UITextJustify::Left, config.mutedTextColor);
    engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_add_texture_id",
            Rectangle{rightX + 136.0f, y, 274.0f, 38.0f},
            font,
            modalState.textureIdBuffer,
            sizeof(modalState.textureIdBuffer),
            0,
            sizeof(modalState.textureIdBuffer) - 1
    );
    y += 54.0f;

    engine::Text(config, assets, Rectangle{rightX, y, 110.0f, 38.0f}, font, "Filtering", engine::UITextJustify::Left, config.mutedTextColor);
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_texture_filter_point", Rectangle{rightX + 136.0f, y, 132.0f, 38.0f}, font, "Point", modalState.filter == SectorTextureFilter::Point)) {
        modalState.filter = SectorTextureFilter::Point;
    }
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_texture_filter_bilinear", Rectangle{rightX + 278.0f, y, 132.0f, 38.0f}, font, "Bilinear", modalState.filter == SectorTextureFilter::Bilinear)) {
        modalState.filter = SectorTextureFilter::Bilinear;
    }
    y += 44.0f;
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_texture_filter_trilinear", Rectangle{rightX + 136.0f, y, 132.0f, 38.0f}, font, "Trilinear", modalState.filter == SectorTextureFilter::Trilinear)) {
        modalState.filter = SectorTextureFilter::Trilinear;
    }
    if (engine::ToolButton(ui, config, input, assets, "sector_editor_add_texture_filter_aniso8x", Rectangle{rightX + 278.0f, y, 132.0f, 38.0f}, font, "Aniso 8x", modalState.filter == SectorTextureFilter::Anisotropic8x)) {
        modalState.filter = SectorTextureFilter::Anisotropic8x;
    }
    y += 54.0f;

    std::string validation;
    callbacks.validateId(validation);
    modalState.validationMessage = validation;
    if (!modalState.validationMessage.empty()) {
        engine::Text(config, assets, Rectangle{rightX, y, 410.0f, 56.0f}, font, modalState.validationMessage.c_str(), engine::UITextJustify::Left, config.invalidColor);
    }

    const float buttonY = modal.y + modal.height - 64.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_add_texture_add", Rectangle{modal.x + modal.width - buttonW * 2.0f - 34.0f, buttonY, buttonW, 44.0f}, font, "Add")) {
        callbacks.addSelected();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_add_texture_cancel", Rectangle{modal.x + modal.width - buttonW - 22.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        callbacks.close();
    }

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            }
    );
}

void DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        TexturePickerState& picker,
        const SectorTopologyMap& map,
        const SectorEditorTexturePickerCallbacks& callbacks)
{
    if (!picker.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&callbacks](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    callbacks.close();
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    callbacks.applySelection();
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!picker.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});

    const Rectangle modal{
            (EditorWidth - 820.0f) * 0.5f,
            (EditorHeight - 620.0f) * 0.5f,
            820.0f,
            620.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 18.0f;
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + 22.0f, y, modal.width - 44.0f, 36.0f},
            font,
            TextFormat("Pick %s", TopologyPickerTargetLabel(picker)));
    y += 50.0f;

    const Rectangle listBounds{modal.x + 22.0f, y, 350.0f, 420.0f};
    const Vector2 contentSize{
            listBounds.width,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(picker.optionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_texture_picker_scroll",
            listBounds,
            contentSize,
            picker.scroll
    );
    if (!picker.optionLabels.empty()) {
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_texture_picker_list",
                Rectangle{0.0f, 0.0f, listBounds.width - (scroll.scrollY ? config.scrollbarSize : 0.0f), contentSize.y},
                font,
                picker.optionLabels.data(),
                picker.optionLabels.size(),
                picker.selectedTextureIndex
        );
    }
    engine::EndScrollArea(ui, config, input, scroll, picker.scroll);

    std::string previewTextureId;
    if (picker.selectedTextureIndex >= 0 && picker.selectedTextureIndex < static_cast<int>(picker.textureIds.size())) {
        previewTextureId = picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
    }
    if (previewTextureId.empty()) {
        previewTextureId = callbacks.currentTextureForTarget();
    }

    const Rectangle previewBounds{modal.x + 402.0f, y, 376.0f, 300.0f};
    engine::Image(config, assets, previewBounds, callbacks.textureHandleForId(previewTextureId));
    y += 316.0f;

    const SectorTextureDefinition* previewTexture = FindSectorTopologyTexture(map, previewTextureId);
    const std::string path = previewTexture == nullptr ? std::string{} : previewTexture->path;
    engine::Text(config, assets, Rectangle{modal.x + 402.0f, y, 376.0f, 34.0f}, font, TextFormat("Id: %s", previewTextureId.empty() ? "<none>" : previewTextureId.c_str()), engine::UITextJustify::Left, config.textColor);
    y += 38.0f;
    engine::Text(config, assets, Rectangle{modal.x + 402.0f, y, 376.0f, 72.0f}, font, TextFormat("Path: %s", path.empty() ? "<sector default>" : path.c_str()), engine::UITextJustify::Left, config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 64.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_texture_picker_select", Rectangle{modal.x + modal.width - buttonW * 2.0f - 34.0f, buttonY, buttonW, 44.0f}, font, "Select")) {
        callbacks.applySelection();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_texture_picker_cancel", Rectangle{modal.x + modal.width - buttonW - 22.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        callbacks.close();
    }

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) {
                engine::ConsumeEvent(event);
            }
    );
}

void RefreshAddMapTextureScan(AddMapTextureState& modalState)
{
    modalState.paths = ScanAssetImagePngs(modalState.scanMessage);
    modalState.optionLabels.clear();
    modalState.optionLabels.reserve(modalState.paths.size());
    for (const std::string& path : modalState.paths) {
        modalState.optionLabels.push_back(path.c_str());
    }
    modalState.scanned = true;
    modalState.selectedPathIndex = modalState.paths.empty() ? -1 : 0;
}

void SelectAddMapTexturePath(
        AddMapTextureState& modalState,
        const SectorTopologyMap& map,
        int pathIndex)
{
    if (pathIndex < 0 || pathIndex >= static_cast<int>(modalState.paths.size())) {
        modalState.selectedPathIndex = -1;
        modalState.textureIdBuffer[0] = '\0';
        return;
    }

    modalState.selectedPathIndex = pathIndex;
    const std::string base = GeneratedTextureIdBase(modalState.paths[static_cast<size_t>(pathIndex)]);
    std::string uniqueId = base;
    int suffix = 1;
    while (FindSectorTopologyTexture(map, uniqueId) != nullptr) {
        char suffixBuffer[16] = {};
        std::snprintf(suffixBuffer, sizeof(suffixBuffer), "_%03d", suffix);
        uniqueId = base + suffixBuffer;
        ++suffix;
    }

    std::snprintf(modalState.textureIdBuffer, sizeof(modalState.textureIdBuffer), "%s", uniqueId.c_str());
    modalState.previewPath.clear();
    modalState.previewTexture = engine::NullTextureHandle();
}

void RefreshAddMapTexturePreview(AddMapTextureState& modalState, engine::AssetManager& assets)
{
    const bool hasSelection = modalState.selectedPathIndex >= 0
            && modalState.selectedPathIndex < static_cast<int>(modalState.paths.size());
    if (!hasSelection) {
        modalState.previewTexture = engine::NullTextureHandle();
        modalState.previewPath.clear();
        return;
    }

    const std::string& path = modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)];
    if (modalState.previewTexture != engine::NullTextureHandle()
            && modalState.previewPath == path
            && modalState.previewFilter == modalState.filter) {
        return;
    }

    if (!engine::IsNull(modalState.previewScope)) {
        assets.UnloadScope(modalState.previewScope);
        modalState.previewScope = engine::NullAssetScopeHandle();
    }
    modalState.previewTexture = engine::NullTextureHandle();
    modalState.previewPath = path;
    modalState.previewFilter = modalState.filter;

    modalState.previewScope = assets.CreateScope("sector_editor_add_texture_preview");
    if (engine::IsNull(modalState.previewScope)) {
        return;
    }

    const std::string resolvedPath = ResolveEditorAssetPath(path);
    modalState.previewTexture = assets.RequestTexture(
            modalState.previewScope,
            "selected_preview",
            resolvedPath.c_str(),
            SectorTextureLoadFlags(modalState.filter)
    );
}

bool ValidateAddMapTextureId(const AddMapTextureState& modalState, std::string& error)
{
    error.clear();
    if (modalState.selectedPathIndex < 0 || modalState.selectedPathIndex >= static_cast<int>(modalState.paths.size())) {
        error = "Select a PNG file";
        return false;
    }

    const std::string id = modalState.textureIdBuffer;
    if (id.empty()) {
        error = "Texture ID is required";
        return false;
    }
    if (!IsValidTextureId(id)) {
        error = "Texture ID may only contain letters, digits, underscores, and dashes";
        return false;
    }
    return true;
}

SectorEditorAddTextureResult AddSelectedMapTexture(SectorEditorState& state)
{
    SectorEditorAddTextureResult result;
    if (!ValidateAddMapTextureId(state.addMapTexture, result.error)) {
        state.addMapTexture.validationMessage = result.error;
        return result;
    }

    AddMapTextureState& modalState = state.addMapTexture;
    const std::string id = modalState.textureIdBuffer;
    const std::string path = modalState.paths[static_cast<size_t>(modalState.selectedPathIndex)];
    result.replacing = FindSectorTopologyTexture(state.topologyMap, id) != nullptr;
    result.textureId = id;

    SectorTextureDefinition definition;
    definition.id = id;
    definition.path = path;
    definition.filter = modalState.filter;
    state.topologyMap.texturesById[id] = std::move(definition);

    result.success = true;
    return result;
}

std::string CurrentTextureForPickerTarget(const SectorEditorState& state)
{
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        return state.previewSettingsModal.open
                ? state.previewSettingsModal.draftSkySettings.textureId
                : state.topologyMap.skySettings.textureId;
    }

    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                state.texturePicker.topologySideDefId);
        if (sideDef == nullptr) {
            return std::string{};
        }
        const SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(
                *sideDef,
                state.texturePicker.topologyWallPart);
        const TopologyMaterialLayer layer = state.texturePicker.topologyWallPart == TopologyWallPart::Middle
                ? TopologyMaterialLayer::Base
                : state.texturePicker.topologyLayer;
        return layer == TopologyMaterialLayer::Decal
                ? part.decal.textureId
                : part.textureId;
    }

    const SectorTopologySector* sector = FindSectorTopologySector(
            state.topologyMap,
            state.texturePicker.topologySectorId);
    if (sector == nullptr) {
        return std::string{};
    }

    switch (state.texturePicker.topologyField) {
        case TopologySectorTextureField::Floor:
            return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->floorDecal.textureId
                    : sector->floorTextureId;
        case TopologySectorTextureField::Ceiling:
            return state.texturePicker.topologyLayer == TopologyMaterialLayer::Decal
                    ? sector->ceilingDecal.textureId
                    : sector->ceilingTextureId;
        case TopologySectorTextureField::DefaultWall: return sector->defaultWall.textureId;
        case TopologySectorTextureField::DefaultLower: return sector->defaultLower.textureId;
        case TopologySectorTextureField::DefaultUpper: return sector->defaultUpper.textureId;
        case TopologySectorTextureField::None: break;
    }
    return std::string{};
}

bool OpenTopologyTexturePicker(
        SectorEditorState& state,
        int sectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    if (FindSectorTopologySector(state.topologyMap, sectorId) == nullptr
            || field == TopologySectorTextureField::None
            || (layer == TopologyMaterialLayer::Decal
                    && field != TopologySectorTextureField::Floor
                    && field != TopologySectorTextureField::Ceiling)) {
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::Sector;
    picker.topologyLayer = layer;
    picker.topologySectorId = sectorId;
    picker.topologyField = field;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenTopologySideDefTexturePicker(
        SectorEditorState& state,
        int sideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    TexturePickerState& picker = state.texturePicker;
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, sideDefId);
    if (sideDef == nullptr
            || (wallPart == TopologyWallPart::Middle
                    && !IsTopologyMiddleEligible(state.topologyMap, sideDef))) {
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::SideDef;
    picker.topologyLayer = wallPart == TopologyWallPart::Middle
            ? TopologyMaterialLayer::Base
            : layer;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = sideDefId;
    picker.topologyWallPart = wallPart;

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

bool OpenMapSkyTexturePicker(SectorEditorState& state)
{
    TexturePickerState& picker = state.texturePicker;
    if (!state.previewSettingsModal.open) {
        picker = TexturePickerState{};
        return false;
    }

    picker.open = true;
    picker.rebuildPreviewOnApply = false;
    picker.topologyTargetKind = TopologyTexturePickerTargetKind::MapSky;
    picker.topologyLayer = TopologyMaterialLayer::Base;
    picker.topologySectorId = -1;
    picker.topologyField = TopologySectorTextureField::None;
    picker.topologySideDefId = -1;
    picker.topologyWallPart = TopologyWallPart::Wall;

    PopulateTexturePickerOptions(picker, state.topologyMap, CurrentTextureForPickerTarget(state));
    return true;
}

SectorEditorTexturePickerApplyResult ApplyTexturePickerSelection(SectorEditorState& state)
{
    SectorEditorTexturePickerApplyResult result;
    TexturePickerState& picker = state.texturePicker;
    if (!picker.open
            || picker.topologyTargetKind == TopologyTexturePickerTargetKind::None
            || picker.selectedTextureIndex < 0
            || picker.selectedTextureIndex >= static_cast<int>(picker.textureIds.size())) {
        picker = TexturePickerState{};
        return result;
    }

    const std::string selectedTexture = picker.textureIds[static_cast<size_t>(picker.selectedTextureIndex)];
    result.rebuildPreviewOnApply = picker.rebuildPreviewOnApply;

    auto assignTexture = [&](std::string& field) {
        if (field != selectedTexture) {
            field = selectedTexture;
            result.changed = true;
        }
    };
    auto assignDecalTexture = [&](SectorTopologyDecalLayer& decal) {
        if (decal.textureId.empty()) {
            ResetTopologyUv(decal.uv);
            decal.opacity = 1.0f;
            decal.emissive = false;
            decal.tint = Vector3{1.0f, 1.0f, 1.0f};
            decal.bloomIntensity = 1.0f;
        }
        assignTexture(decal.textureId);
    };

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::MapSky) {
        if (state.previewSettingsModal.open
                && state.previewSettingsModal.draftSkySettings.textureId != selectedTexture) {
            state.previewSettingsModal.draftSkySettings.textureId = selectedTexture;
            state.previewSettingsModal.errorMessage.clear();
        }
        picker = TexturePickerState{};
        return result;
    }

    if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::Sector) {
        SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, picker.topologySectorId);
        if (sector == nullptr) {
            picker = TexturePickerState{};
            return result;
        }

        switch (picker.topologyField) {
            case TopologySectorTextureField::Floor:
                if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                    assignDecalTexture(sector->floorDecal);
                } else {
                    assignTexture(sector->floorTextureId);
                }
                break;
            case TopologySectorTextureField::Ceiling:
                if (picker.topologyLayer == TopologyMaterialLayer::Decal) {
                    assignDecalTexture(sector->ceilingDecal);
                } else {
                    assignTexture(sector->ceilingTextureId);
                }
                break;
            case TopologySectorTextureField::DefaultWall:
                assignTexture(sector->defaultWall.textureId);
                break;
            case TopologySectorTextureField::DefaultLower:
                assignTexture(sector->defaultLower.textureId);
                break;
            case TopologySectorTextureField::DefaultUpper:
                assignTexture(sector->defaultUpper.textureId);
                break;
            case TopologySectorTextureField::None:
                break;
        }
        result.status = picker.topologyLayer == TopologyMaterialLayer::Decal
                ? TextFormat("Selected %s decal texture.", picker.topologyField == TopologySectorTextureField::Floor ? "floor" : "ceiling")
                : TextFormat("Changed %s", TopologySectorTextureFieldLabel(picker.topologyField));
    } else if (picker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef) {
        SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, picker.topologySideDefId);
        if (sideDef == nullptr) {
            picker = TexturePickerState{};
            return result;
        }
        SectorTopologyWallPartSettings& part = TopologyWallPartSettingsFor(*sideDef, picker.topologyWallPart);
        const TopologyMaterialLayer layer = picker.topologyWallPart == TopologyWallPart::Middle
                ? TopologyMaterialLayer::Base
                : picker.topologyLayer;
        if (layer == TopologyMaterialLayer::Decal) {
            assignDecalTexture(part.decal);
            result.status = TextFormat(
                    "Selected %s decal texture.",
                    TopologyWallPartStatusName(picker.topologyWallPart));
        } else {
            assignTexture(part.textureId);
            result.status = picker.topologyWallPart == TopologyWallPart::Middle
                    ? "Selected middle texture."
                    : TextFormat(
                            "Changed topology sidedef %d %s texture",
                            sideDef->id,
                            TopologyWallPartStatusName(picker.topologyWallPart));
        }
        result.useMaterialMutationFinish = picker.topologyWallPart == TopologyWallPart::Middle;
    }

    picker = TexturePickerState{};
    return result;
}

} // namespace game
