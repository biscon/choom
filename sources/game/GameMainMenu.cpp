#include "game/GameMainMenu.h"

#include <raylib.h>

#include <cmath>
#include <cstdio>

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

GameGraphicsSettingsAction DrawGameGraphicsSettings(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        FpsApplicationSettings& draft,
        const char* statusText)
{
    DrawRectangleRec(config.overlayBounds, Color{0, 0, 0, 128});

    constexpr float panelWidth = 620.0f;
    constexpr float panelHeight = 790.0f;
    constexpr float padding = 44.0f;
    constexpr float rowHeight = 48.0f;
    const Rectangle panel{
            config.overlayBounds.x + (config.overlayBounds.width - panelWidth) * 0.5f,
            config.overlayBounds.y + (config.overlayBounds.height - panelHeight) * 0.5f,
            panelWidth,
            panelHeight};
    DrawRectangleRounded(panel, config.cornerRadius, config.cornerSegments, config.panelColor);
    DrawRectangleRoundedLinesEx(panel, config.cornerRadius, config.cornerSegments,
            config.borderThickness, config.borderColor);

    engine::BeginUI(ui, input);
    engine::Text(config, assets,
            Rectangle{panel.x + padding, panel.y + 24.0f,
                    panel.width - padding * 2.0f, 54.0f},
            font, "Graphics Settings", engine::UITextJustify::Center);

    float y = panel.y + 96.0f;
    const float labelWidth = 225.0f;
    const float controlX = panel.x + padding + labelWidth;
    const float controlWidth = panel.width - padding * 2.0f - labelWidth;
    const char* renderScaleOptions[] = {"75%", "100%", "125%", "150%", "200%"};
    const float renderScales[] = {0.75f, 1.0f, 1.25f, 1.5f, 2.0f};
    int renderScaleIndex = 0;
    float closest = 100.0f;
    for (int i = 0; i < 5; ++i) {
        const float distance = std::fabs(draft.graphics.renderScale - renderScales[i]);
        if (distance < closest) {
            closest = distance;
            renderScaleIndex = i;
        }
    }
    engine::Text(config, assets, Rectangle{panel.x + padding, y, labelWidth, rowHeight},
            smallFont, "Render scale", engine::UITextJustify::Left);
    if (engine::Option(ui, config, input, assets, "graphics_render_scale",
                Rectangle{controlX, y, controlWidth, rowHeight}, smallFont,
                renderScaleOptions, 5, renderScaleIndex)) {
        draft.graphics.renderScale = renderScales[renderScaleIndex];
    }
    y += rowHeight + 14.0f;

    const char* qualityOptions[] = {"Off", "Low", "Medium", "High"};
    int volumetricQuality = static_cast<int>(draft.graphics.volumetricQuality);
    engine::Text(config, assets, Rectangle{panel.x + padding, y, labelWidth, rowHeight},
            smallFont, "Volumetric fog quality", engine::UITextJustify::Left);
    if (engine::Option(ui, config, input, assets, "graphics_volumetric_quality",
                Rectangle{controlX, y, controlWidth, rowHeight}, smallFont,
                qualityOptions, 4, volumetricQuality)) {
        draft.graphics.volumetricQuality =
                static_cast<SectorVolumetricQuality>(volumetricQuality);
    }
    y += rowHeight + 14.0f;

    int shadowQuality = static_cast<int>(draft.graphics.shadowQuality);
    engine::Text(config, assets, Rectangle{panel.x + padding, y, labelWidth, rowHeight},
            smallFont, "Shadow quality", engine::UITextJustify::Left);
    if (engine::Option(ui, config, input, assets, "graphics_shadow_quality",
                Rectangle{controlX, y, controlWidth, rowHeight}, smallFont,
                qualityOptions, 4, shadowQuality)) {
        draft.graphics.shadowQuality = static_cast<FpsShadowQuality>(shadowQuality);
    }
    y += rowHeight + 14.0f;

    engine::Text(config, assets, Rectangle{panel.x + padding, y, labelWidth, rowHeight},
            smallFont, "Horizontal FOV", engine::UITextJustify::Left);
    constexpr float fovValueWidth = 64.0f;
    engine::IntSlider(
            ui,
            config,
            input,
            "graphics_horizontal_fov",
            Rectangle{controlX, y, controlWidth - fovValueWidth, rowHeight},
            MinFpsHorizontalFovDegrees,
            MaxFpsHorizontalFovDegrees,
            draft.graphics.horizontalFovDegrees);
    char fovText[16];
    std::snprintf(
            fovText,
            sizeof(fovText),
            "%d",
            draft.graphics.horizontalFovDegrees);
    engine::Text(
            config,
            assets,
            Rectangle{controlX + controlWidth - fovValueWidth, y,
                    fovValueWidth, rowHeight},
            smallFont,
            fovText,
            engine::UITextJustify::Right);
    y += rowHeight + 14.0f;

    engine::Checkbox(ui, config, input, assets, "graphics_fxaa",
            Rectangle{panel.x + padding, y, panel.width - padding * 2.0f, rowHeight},
            smallFont, "FXAA", draft.graphics.fxaa);
    y += rowHeight + 8.0f;
    engine::Checkbox(ui, config, input, assets, "graphics_bloom",
            Rectangle{panel.x + padding, y, panel.width - padding * 2.0f, rowHeight},
            smallFont, "HDR bloom", draft.hdrBloom.enabled);
    y += rowHeight + 8.0f;
    engine::Checkbox(ui, config, input, assets, "graphics_performance_overlay",
            Rectangle{panel.x + padding, y, panel.width - padding * 2.0f, rowHeight},
            smallFont, "Performance overlay (F9)", draft.graphics.performanceOverlay);
    y += rowHeight + 8.0f;
    engine::Checkbox(ui, config, input, assets, "graphics_vsync",
            Rectangle{panel.x + padding, y, panel.width - padding * 2.0f, rowHeight},
            smallFont, "VSync", draft.graphics.vsync);
    y += rowHeight + 4.0f;
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + padding, y,
                    panel.width - padding * 2.0f, 42.0f},
            smallFont,
            "You must restart the game for VSync changes to take effect.",
            engine::UITextJustify::Left,
            config.mutedTextColor,
            true);
    y += 42.0f + 12.0f;

    GameGraphicsSettingsAction result = GameGraphicsSettingsAction::None;
    const float buttonGap = 10.0f;
    const float buttonWidth = (panel.width - padding * 2.0f - buttonGap * 2.0f) / 3.0f;
    if (engine::Button(ui, config, input, assets, "graphics_defaults",
                Rectangle{panel.x + padding, y, buttonWidth, rowHeight},
                smallFont, "Defaults")) {
        result = GameGraphicsSettingsAction::Defaults;
    }
    if (engine::Button(ui, config, input, assets, "graphics_cancel",
                Rectangle{panel.x + padding + buttonWidth + buttonGap, y,
                        buttonWidth, rowHeight}, smallFont, "Cancel")) {
        result = GameGraphicsSettingsAction::Cancel;
    }
    if (engine::Button(ui, config, input, assets, "graphics_apply",
                Rectangle{panel.x + padding + (buttonWidth + buttonGap) * 2.0f, y,
                        buttonWidth, rowHeight}, smallFont, "Apply")) {
        result = GameGraphicsSettingsAction::Apply;
    }
    y += rowHeight + 8.0f;
    if (statusText != nullptr && statusText[0] != '\0') {
        engine::Text(config, assets,
                Rectangle{panel.x + padding, y, panel.width - padding * 2.0f, 42.0f},
                smallFont, statusText, engine::UITextJustify::Center,
                config.invalidColor, true);
    }
    engine::EndUI(ui, config, input, assets);
    return result;
}

} // namespace game
