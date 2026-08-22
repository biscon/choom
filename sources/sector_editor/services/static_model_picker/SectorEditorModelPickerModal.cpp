#include "sector_editor/services/static_model_picker/SectorEditorModelPickerModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>

namespace game {
namespace {

const char* PickerTitle(ModelPickerTarget target)
{
    if (target == ModelPickerTarget::DynamicModel) {
        return "Choose Dynamic Prop Model";
    }
    if (target == ModelPickerTarget::NpcDefinition) {
        return "Choose NPC Character Model";
    }
    if (target == ModelPickerTarget::WeaponArms) {
        return "Choose Animated Arms Model";
    }
    if (target == ModelPickerTarget::WeaponAttachment) {
        return "Choose Attached Weapon Model";
    }
    return "Choose 3D Prop Model";
}

} // namespace

SectorEditorModelPickerModalResult DrawSectorEditorModelPickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorStaticModelPickerService& picker)
{
    StaticModelPickerState& state = picker.State();
    if (!state.open) return SectorEditorModelPickerModalResult::None;
    if (!state.scanned) picker.Refresh();

    bool cancelRequested = false;
    bool selectRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested, &selectRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER
                        || event.key.key == KEY_KP_ENTER) {
                    selectRequested = true;
                    engine::ConsumeEvent(event);
                }
            });

    DrawRectangle(
            0,
            0,
            static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight),
            Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 820.0f) * 0.5f,
            (EditorHeight - 650.0f) * 0.5f,
            820.0f,
            650.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(
            config,
            assets,
            Rectangle{modal.x + 22.0f, modal.y + 18.0f, modal.width - 44.0f, 36.0f},
            font,
            PickerTitle(state.target));

    const Rectangle listBounds{
            modal.x + 22.0f,
            modal.y + 68.0f,
            modal.width - 44.0f,
            450.0f};
    const float clientWidth = std::max(
            0.0f,
            listBounds.width - config.borderThickness * 2.0f);
    const float listContentW = std::max(
            0.0f,
            clientWidth - config.scrollbarSize
                    - engine::DefaultScrollAreaPaddingPx * 2.0f);
    const Vector2 contentSize{
            listContentW,
            std::max(
                    listBounds.height,
                    config.listItemHeight
                            * static_cast<float>(state.optionLabels.size()))};
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_model_picker_scroll",
            listBounds,
            contentSize,
            state.scroll);
    if (!state.optionLabels.empty()) {
        const int previous = state.selectedModelIndex;
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_model_picker_list",
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y},
                font,
                state.optionLabels.data(),
                state.optionLabels.size(),
                state.selectedModelIndex);
        if (state.selectedModelIndex != previous) {
            picker.SelectIndex(state.selectedModelIndex);
        }
    }
    engine::EndScrollArea(ui, config, input, scroll, state.scroll);

    engine::Text(
            config,
            assets,
            Rectangle{
                    listBounds.x,
                    listBounds.y + listBounds.height + 8.0f,
                    listBounds.width,
                    32.0f},
            font,
            state.scanMessage.c_str(),
            engine::UITextJustify::Left,
            state.modelPaths.empty()
                    ? config.invalidColor
                    : config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 64.0f;
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_model_picker_refresh",
                Rectangle{modal.x + 22.0f, buttonY, 130.0f, 44.0f},
                font, "Refresh")) {
        picker.Refresh();
    }
    selectRequested = selectRequested || engine::Button(
            ui, config, input, assets,
            "sector_editor_model_picker_select",
            Rectangle{modal.x + modal.width - 322.0f, buttonY, 140.0f, 44.0f},
            font, "Select");
    cancelRequested = cancelRequested || engine::Button(
            ui, config, input, assets,
            "sector_editor_model_picker_cancel",
            Rectangle{modal.x + modal.width - 162.0f, buttonY, 140.0f, 44.0f},
            font, "Cancel");

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
    if (cancelRequested) {
        picker.Close();
        return SectorEditorModelPickerModalResult::Cancelled;
    }
    if (selectRequested && picker.HasSelection()) {
        return SectorEditorModelPickerModalResult::Selected;
    }
    return SectorEditorModelPickerModalResult::None;
}

} // namespace game
