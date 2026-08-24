#include "sector_editor/SectorEditorTextureModals.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_editor/services/texture_picker/SectorEditorTexturePickerService.h"
#include "sector_demo/SectorTextureTypes.h"

#include <algorithm>

namespace game {

namespace {

float ScrollAreaContentWidthForVerticalScrollbar(float boundsWidth, const engine::UIConfig& config)
{
    const float clientWidth = std::max(0.0f, boundsWidth - config.borderThickness * 2.0f);
    return std::max(0.0f, clientWidth - config.scrollbarSize - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

float WrappedTextHeightForLines(const engine::UIConfig& config, int lineCount)
{
    const int clampedLineCount = std::max(1, lineCount);
    return config.paddingY * 2.0f
            + config.fontSize * static_cast<float>(clampedLineCount)
            + config.textSpacing * static_cast<float>(clampedLineCount - 1);
}

} // namespace

void DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        TexturePickerState& picker,
        SectorEditorTextureCatalogService& textureCatalog,
        const SectorEditorTexturePickerCallbacks& callbacks)
{
    if (!picker.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&callbacks, &picker](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    callbacks.close();
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    if (CurrentSectorEditorTexturePickerSelection(picker).valid) {
                        callbacks.applySelection();
                    }
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!picker.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});

    const Rectangle modal{
            (EditorWidth - 1025.0f) * 0.5f,
            (EditorHeight - 620.0f) * 0.5f,
            1025.0f,
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

    const float leftX = modal.x + 22.0f;
    const float leftWidth = 555.0f;
    const Rectangle filterBounds{leftX, y, leftWidth, 42.0f};
    const float filterLabelWidth = 82.0f;
    engine::Text(
            config,
            assets,
            Rectangle{filterBounds.x, filterBounds.y, filterLabelWidth, filterBounds.height},
            smallFont,
            "Filter",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    const engine::UITextInputResult filterResult = engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_texture_picker_filter",
            Rectangle{
                    filterBounds.x + filterLabelWidth,
                    filterBounds.y,
                    filterBounds.width - filterLabelWidth,
                    filterBounds.height},
            smallFont,
            picker.filterBuffer,
            sizeof(picker.filterBuffer),
            0,
            sizeof(picker.filterBuffer) - 1);
    if (filterResult.changed) {
        ApplySectorEditorTexturePickerFilter(picker);
    }
    y += 54.0f;

    const Rectangle listBounds{leftX, y, leftWidth, 476.0f};
    const float listContentW = ScrollAreaContentWidthForVerticalScrollbar(listBounds.width, config);
    const Vector2 contentSize{
            listContentW,
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
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y},
                font,
                picker.optionLabels.data(),
                picker.optionLabels.size(),
                picker.selectedTextureIndex
        );
    }
    engine::EndScrollArea(ui, config, input, scroll, picker.scroll);
    if (picker.optionLabels.empty()) {
        engine::Text(
                config,
                assets,
                listBounds,
                smallFont,
                picker.filterMessage.c_str(),
                engine::UITextJustify::Center,
                config.mutedTextColor,
                true);
    }

    std::string previewTextureId;
    if (picker.selectedTextureIndex >= 0 && picker.selectedTextureIndex < static_cast<int>(picker.materialIds.size())) {
        previewTextureId = picker.materialIds[static_cast<size_t>(picker.selectedTextureIndex)];
    }
    if (previewTextureId.empty()) {
        previewTextureId = callbacks.currentTextureForTarget();
    }

    const float rightX = modal.x + 607.0f;
    const float rightWidth = 376.0f;
    const Rectangle previewBounds{rightX, y, rightWidth, 300.0f};
    engine::Image(
            config,
            assets,
            previewBounds,
            textureCatalog.EnsureTextureHandleForId(previewTextureId, assets));
    y += 316.0f;

    const SectorMaterialDefinition* previewTexture = textureCatalog.FindTexture(previewTextureId);
    const std::string path = previewTexture == nullptr ? std::string{} : previewTexture->path;
    const float idHeight = WrappedTextHeightForLines(config, 2);
    engine::Text(
            config,
            assets,
            Rectangle{rightX, y, rightWidth, idHeight},
            font,
            TextFormat("Id: %s", previewTextureId.empty() ? "<none>" : previewTextureId.c_str()),
            engine::UITextJustify::Left,
            config.textColor,
            true);
    y += idHeight + 4.0f;

    const engine::UIConfig smallConfig =
            SectorEditorSmallFontConfig(config, assets, smallFont);
    const float pathHeight = WrappedTextHeightForLines(smallConfig, 2);
    engine::Text(
            smallConfig,
            assets,
            Rectangle{rightX, y, rightWidth, pathHeight},
            smallFont,
            TextFormat("Path: %s", path.empty() ? "<sector default>" : path.c_str()),
            engine::UITextJustify::Left,
            smallConfig.mutedTextColor,
            true);

    const float buttonY = modal.y + modal.height - 64.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_texture_picker_select", Rectangle{modal.x + modal.width - buttonW * 2.0f - 34.0f, buttonY, buttonW, 44.0f}, font, "Select")) {
        if (CurrentSectorEditorTexturePickerSelection(picker).valid) {
            callbacks.applySelection();
        }
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

void DrawSpritePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SpritePickerState& picker,
        const SectorEditorSpritePickerCallbacks& callbacks)
{
    if (!picker.open) {
        return;
    }

    if (!picker.scanned) {
        callbacks.refreshScan();
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

    callbacks.refreshPreview();

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});

    const Rectangle modal{
            (EditorWidth - 900.0f) * 0.5f,
            (EditorHeight - 660.0f) * 0.5f,
            900.0f,
            660.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 18.0f;
    engine::Text(config, assets, Rectangle{modal.x + 22.0f, y, modal.width - 44.0f, 36.0f}, font, "Pick Sprite");
    y += 50.0f;

    const Rectangle listBounds{modal.x + 22.0f, y, 390.0f, 420.0f};
    const float listContentW = ScrollAreaContentWidthForVerticalScrollbar(listBounds.width, config);
    const Vector2 contentSize{
            listContentW,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(picker.spriteOptionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_sprite_picker_scroll",
            listBounds,
            contentSize,
            picker.spriteScroll
    );
    if (!picker.spriteOptionLabels.empty()) {
        const int oldSelection = picker.selectedSpriteIndex;
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_sprite_picker_list",
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y},
                font,
                picker.spriteOptionLabels.data(),
                picker.spriteOptionLabels.size(),
                picker.selectedSpriteIndex
        );
        if (picker.selectedSpriteIndex != oldSelection) {
            callbacks.selectSprite(picker.selectedSpriteIndex);
        }
    }
    engine::EndScrollArea(ui, config, input, scroll, picker.spriteScroll);

    if (!picker.scanMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{listBounds.x, listBounds.y + listBounds.height + 8.0f, listBounds.width, 34.0f},
                font,
                picker.scanMessage.c_str(),
                engine::UITextJustify::Left,
                picker.sprites.empty() ? config.invalidColor : config.mutedTextColor
        );
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_sprite_picker_refresh",
                Rectangle{listBounds.x, modal.y + modal.height - 64.0f, 140.0f, 44.0f},
                font,
                "Refresh")) {
        callbacks.refreshScan();
    }

    const float rightX = modal.x + 440.0f;
    y = modal.y + 68.0f;
    const Rectangle previewBounds{rightX, y, 420.0f, 260.0f};
    engine::Image(config, assets, previewBounds, picker.previewTexture);
    y += 276.0f;

    const SectorEditorSpritePickerResult selected = SelectedSpritePickerResult(picker);
    engine::Text(
            config,
            assets,
            Rectangle{rightX, y, 420.0f, 54.0f},
            font,
            TextFormat("Sprite: %s", selected.spriteAnimationPath.empty() ? "<none>" : selected.spriteAnimationPath.c_str()),
            engine::UITextJustify::Left,
            config.textColor);
    y += 58.0f;
    engine::Text(
            config,
            assets,
            Rectangle{rightX, y, 420.0f, 54.0f},
            font,
            TextFormat("Atlas: %s", selected.atlasImagePath.empty() ? "<none>" : selected.atlasImagePath.c_str()),
            engine::UITextJustify::Left,
            config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 64.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_sprite_picker_select", Rectangle{modal.x + modal.width - buttonW * 2.0f - 34.0f, buttonY, buttonW, 44.0f}, font, "Select")) {
        callbacks.applySelection();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_sprite_picker_cancel", Rectangle{modal.x + modal.width - buttonW - 22.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
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

void RefreshSpritePickerPreview(SpritePickerState& picker, engine::AssetManager& assets)
{
    const bool hasSelection = picker.selectedSpriteIndex >= 0
            && picker.selectedSpriteIndex < static_cast<int>(picker.sprites.size());
    if (!hasSelection) {
        if (!engine::IsNull(picker.previewScope)) {
            assets.UnloadScope(picker.previewScope);
            picker.previewScope = engine::NullAssetScopeHandle();
        }
        picker.previewTexture = engine::NullTextureHandle();
        picker.previewAtlasPath.clear();
        return;
    }

    const std::string& atlasPath = picker.sprites[static_cast<size_t>(picker.selectedSpriteIndex)].atlasImagePath;
    if (picker.previewTexture != engine::NullTextureHandle() && picker.previewAtlasPath == atlasPath) {
        return;
    }

    if (!engine::IsNull(picker.previewScope)) {
        assets.UnloadScope(picker.previewScope);
        picker.previewScope = engine::NullAssetScopeHandle();
    }
    picker.previewTexture = engine::NullTextureHandle();
    picker.previewAtlasPath = atlasPath;

    if (atlasPath.empty()) {
        return;
    }

    picker.previewScope = assets.CreateScope("sector_editor_sprite_picker_preview");
    if (engine::IsNull(picker.previewScope)) {
        return;
    }

    const std::string resolvedPath = ResolveEditorAssetPath(atlasPath);
    picker.previewTexture = assets.RequestTexture(
            picker.previewScope,
            "atlas_preview",
            resolvedPath.c_str(),
            engine::TextureColorUsage::DisplaySrgb,
            engine::TextureLoad_PointFilter
    );
}

void CloseSpritePicker(SpritePickerState& picker, engine::AssetManager& assets)
{
    if (!engine::IsNull(picker.previewScope)) {
        assets.UnloadScope(picker.previewScope);
    }
    picker = SpritePickerState{};
}

} // namespace game
