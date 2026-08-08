#include "game/GameMainMenu.h"

#include <raylib.h>

namespace game {

namespace {

const char* MainMenuActionId(MainMenuAction action)
{
    switch (action) {
        case MainMenuAction::StartNewGame: return "main_menu_start_new_game";
        case MainMenuAction::Resume: return "main_menu_resume";
        case MainMenuAction::LoadGame: return "main_menu_load_game";
        case MainMenuAction::SaveGame: return "main_menu_save_game";
        case MainMenuAction::Editor: return "main_menu_editor";
        case MainMenuAction::Settings: return "main_menu_settings";
        case MainMenuAction::Quit: return "main_menu_quit";
    }
    return "main_menu_unknown";
}

} // namespace

std::optional<MainMenuAction> DrawGameMainMenu(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        bool gameRunning,
        const char* statusText)
{
    DrawRectangleRec(config.overlayBounds, Color{0, 0, 0, 128});

    constexpr float panelWidth = 520.0f;
    constexpr float buttonHeight = 56.0f;
    constexpr float buttonGap = 12.0f;
    constexpr float horizontalPadding = 44.0f;
    const MainMenuItems items = BuildMainMenuItems(gameRunning);
    const float buttonsHeight = static_cast<float>(items.count) * buttonHeight
            + static_cast<float>(items.count > 0 ? items.count - 1 : 0)
                    * buttonGap;
    const float panelHeight = 150.0f + buttonsHeight
            + (statusText != nullptr && statusText[0] != '\0' ? 72.0f : 24.0f);
    const Rectangle panel{
            config.overlayBounds.x
                    + (config.overlayBounds.width - panelWidth) * 0.5f,
            config.overlayBounds.y
                    + (config.overlayBounds.height - panelHeight) * 0.5f,
            panelWidth,
            panelHeight};

    DrawRectangleRounded(
            panel,
            config.cornerRadius,
            config.cornerSegments,
            config.panelColor);
    DrawRectangleRoundedLinesEx(
            panel,
            config.cornerRadius,
            config.cornerSegments,
            config.borderThickness,
            config.borderColor);

    engine::BeginUI(ui, input);
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + horizontalPadding, panel.y + 28.0f,
                    panel.width - horizontalPadding * 2.0f, 54.0f},
            font,
            "Engine",
            engine::UITextJustify::Center);

    float y = panel.y + 100.0f;
    std::optional<MainMenuAction> selected;
    for (size_t i = 0; i < items.count; ++i) {
        const MainMenuAction action = items.values[i];
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    MainMenuActionId(action),
                    Rectangle{panel.x + horizontalPadding, y,
                            panel.width - horizontalPadding * 2.0f,
                            buttonHeight},
                    font,
                    MainMenuActionLabel(action))) {
            selected = action;
        }
        y += buttonHeight + buttonGap;
    }

    if (statusText != nullptr && statusText[0] != '\0') {
        engine::Text(
                config,
                assets,
                Rectangle{panel.x + horizontalPadding, y + 2.0f,
                        panel.width - horizontalPadding * 2.0f, 56.0f},
                smallFont,
                statusText,
                engine::UITextJustify::Center,
                config.invalidColor,
                true);
    }
    engine::EndUI(ui, config, input, assets);
    return selected;
}

} // namespace game
