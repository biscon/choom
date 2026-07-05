#include "sector_editor/document/SectorEditorDocumentModals.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>

namespace game {

namespace {

float ScrollAreaContentWidthForVerticalScrollbar(
        float boundsWidth,
        const engine::UIConfig& config,
        float paddingPx,
        bool drawFrame)
{
    const float clientWidth = std::max(
            0.0f,
            boundsWidth - (drawFrame ? config.borderThickness * 2.0f : 0.0f));
    return std::max(0.0f, clientWidth - config.scrollbarSize - paddingPx * 2.0f);
}

} // namespace

void DrawSectorEditorSaveLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SaveLevelModalState& modalState,
        const SectorEditorSaveLevelModalCallbacks& callbacks)
{
    if (!modalState.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&callbacks](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    if (callbacks.close) {
                        callbacks.close();
                    }
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    if (callbacks.save) {
                        callbacks.save();
                    }
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!modalState.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 660.0f) * 0.5f,
            (EditorHeight - 300.0f) * 0.5f,
            660.0f,
            300.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 20.0f, modal.width - 48.0f, 40.0f}, font, "Save Level");
    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 82.0f, 100.0f, 42.0f}, font, "Name:", engine::UITextJustify::Left, config.mutedTextColor);
    const engine::UITextInputResult inputResult = engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_save_level_name",
            Rectangle{modal.x + 126.0f, modal.y + 80.0f, modal.width - 150.0f, 42.0f},
            font,
            modalState.nameBuffer,
            sizeof(modalState.nameBuffer),
            0,
            sizeof(modalState.nameBuffer) - 1
    );
    if (inputResult.changed) {
        modalState.errorMessage.clear();
    }

    if (!modalState.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 24.0f, modal.y + 140.0f, modal.width - 48.0f, 48.0f},
                font,
                modalState.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor
        );
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_save_level_confirm", Rectangle{modal.x + modal.width - buttonW * 2.0f - 36.0f, buttonY, buttonW, 44.0f}, font, "Save")) {
        if (callbacks.save) {
            callbacks.save();
        }
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_save_level_cancel", Rectangle{modal.x + modal.width - buttonW - 24.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        if (callbacks.close) {
            callbacks.close();
        }
    }

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });
}

void DrawSectorEditorLoadLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        LoadLevelModalState& modalState,
        const SectorEditorLoadLevelModalCallbacks& callbacks)
{
    if (!modalState.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&callbacks](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    if (callbacks.close) {
                        callbacks.close();
                    }
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    if (callbacks.loadSelected) {
                        callbacks.loadSelected();
                    }
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!modalState.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 760.0f) * 0.5f,
            (EditorHeight - 660.0f) * 0.5f,
            760.0f,
            660.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 20.0f, modal.width - 48.0f, 40.0f}, font, "Load Level");

    const Rectangle listBounds{modal.x + 24.0f, modal.y + 74.0f, modal.width - 48.0f, 450.0f};
    const float listContentW = ScrollAreaContentWidthForVerticalScrollbar(
            listBounds.width,
            config,
            engine::DefaultScrollAreaPaddingPx,
            true);
    const Vector2 contentSize{
            listContentW,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(modalState.optionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_load_level_scroll",
            listBounds,
            contentSize,
            modalState.scroll
    );
    if (!modalState.optionLabels.empty()) {
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_load_level_list",
                Rectangle{0.0f, 0.0f, scroll.viewport.width, contentSize.y},
                font,
                modalState.optionLabels.data(),
                modalState.optionLabels.size(),
                modalState.selectedIndex
        );
    }
    engine::EndScrollArea(ui, config, input, scroll, modalState.scroll);

    const char* message = modalState.errorMessage.empty()
            ? (modalState.levels.empty() ? "No levels found." : "")
            : modalState.errorMessage.c_str();
    if (message[0] != '\0') {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 24.0f, modal.y + 536.0f, modal.width - 48.0f, 40.0f},
                font,
                message,
                engine::UITextJustify::Left,
                modalState.errorMessage.empty() ? config.mutedTextColor : config.invalidColor
        );
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_load_level_confirm", Rectangle{modal.x + modal.width - buttonW * 2.0f - 36.0f, buttonY, buttonW, 44.0f}, font, "Load")) {
        if (callbacks.loadSelected) {
            callbacks.loadSelected();
        }
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_load_level_cancel", Rectangle{modal.x + modal.width - buttonW - 24.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        if (callbacks.close) {
            callbacks.close();
        }
    }

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });
}

void DrawSectorEditorConfirmationModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        ConfirmationModalState& modalState,
        const SectorEditorConfirmationModalCallbacks& callbacks)
{
    if (!modalState.open) {
        return;
    }

    bool okayRequested = false;
    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                }
            }
    );

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 680.0f) * 0.5f,
            (EditorHeight - 300.0f) * 0.5f,
            680.0f,
            300.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, modal.y + 22.0f, modal.width - 52.0f, 42.0f}, font, modalState.title.c_str());
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, modal.y + 86.0f, modal.width - 52.0f, 88.0f}, font, modalState.message.c_str(), engine::UITextJustify::Left, config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 68.0f;
    const float buttonW = 150.0f;
    okayRequested = okayRequested || engine::Button(ui, config, input, assets, "sector_editor_confirmation_okay", Rectangle{modal.x + modal.width - buttonW * 2.0f - 38.0f, buttonY, buttonW, 44.0f}, font, "Okay");
    cancelRequested = cancelRequested || engine::Button(ui, config, input, assets, "sector_editor_confirmation_cancel", Rectangle{modal.x + modal.width - buttonW - 26.0f, buttonY, buttonW, 44.0f}, font, "Cancel");

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (cancelRequested) {
        if (callbacks.cancel) {
            callbacks.cancel();
        }
        return;
    }
    if (okayRequested && callbacks.okay) {
        callbacks.okay();
    }
}

} // namespace game
