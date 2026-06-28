#include "sector_editor/SectorEditorTextureModals.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTextureTypes.h"

#include <algorithm>

namespace game {

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

} // namespace game
