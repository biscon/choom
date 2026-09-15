#include "game/GameMainMenu.h"
#include "game/GameSettingsLayout.h"

#include <raylib.h>

#include <algorithm>
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
        engine::UIScrollState& scroll,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        bool gameRunning,
        bool saveEnabled,
        const char* saveBlockedReason,
        const char* statusText)
{
    DrawRectangleRec(config.overlayBounds, Color{0, 0, 0, 128});

    const float panelWidth = std::min(520.0f,
            std::max(0.0f, config.overlayBounds.width - 48.0f));
    constexpr float buttonHeight = 56.0f;
    constexpr float buttonGap = 12.0f;
    const float horizontalPadding = std::min(44.0f, panelWidth * 0.1f);
    // Reserve the scrollbar width before measuring text so it cannot change
    // wrapping when the panel becomes scrollable.
    const float contentWidth = std::max(0.0f, panelWidth - config.scrollbarSize);
    const float rowWidth = std::max(0.0f, contentWidth - horizontalPadding * 2.0f);
    engine::UIConfig statusConfig = config;
    if (const engine::FontAsset* asset = assets.GetFont(smallFont)) {
        statusConfig.fontSize = static_cast<float>(asset->pixelSize);
    }
    const float statusHeight = engine::MeasureWrappedTextHeight(
            statusConfig, assets, rowWidth, smallFont, statusText);
    const float blockedHeight = gameRunning && !saveEnabled
            ? engine::MeasureWrappedTextHeight(
                    statusConfig, assets, rowWidth, smallFont, saveBlockedReason)
            : 0.0f;
    const MainMenuItems items = BuildMainMenuItems(gameRunning);
    const float buttonsHeight = static_cast<float>(items.count) * buttonHeight
            + static_cast<float>(items.count > 0 ? items.count - 1 : 0)
                    * buttonGap;
    const float messageY = 100.0f + buttonsHeight + buttonGap + 2.0f;
    const float messageHeight = std::max(statusHeight, blockedHeight);
    const float contentHeight = messageY + messageHeight + 24.0f;
    const float panelHeight = std::min(contentHeight,
            std::max(0.0f, config.overlayBounds.height - 48.0f));
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
    const engine::UIScrollAreaResult scrollArea = engine::BeginScrollArea(
            ui, config, input, "main_menu_content", panel,
            {contentWidth, contentHeight}, scroll, false, 0.0f);
    engine::Text(
            ui, config,
            assets,
            Rectangle{horizontalPadding, 28.0f, rowWidth, 54.0f},
            font,
            "Engine",
            engine::UITextJustify::Center);

    float y = 100.0f;
    std::optional<MainMenuAction> selected;
    bool saveHovered = false;
    for (size_t i = 0; i < items.count; ++i) {
        const MainMenuAction action = items.values[i];
        const bool enabled = action != MainMenuAction::SaveGame || saveEnabled;
        const Rectangle buttonBounds{horizontalPadding, y, rowWidth, buttonHeight};
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    MainMenuActionId(action),
                    buttonBounds,
                    font,
                    MainMenuActionLabel(action),
                    engine::UITextJustify::Center,
                    enabled)) {
            selected = action;
        }
        if (action == MainMenuAction::SaveGame && !enabled) {
            const Rectangle screenBounds{
                    scrollArea.viewport.x + buttonBounds.x - scroll.offset.x,
                    scrollArea.viewport.y + buttonBounds.y - scroll.offset.y,
                    buttonBounds.width, buttonBounds.height};
            saveHovered = CheckCollisionPointRec(ui.mousePosition, scrollArea.viewport)
                    && CheckCollisionPointRec(ui.mousePosition, screenBounds);
        }
        y += buttonHeight + buttonGap;
    }

    const char* visibleStatus = saveHovered
            && saveBlockedReason != nullptr && saveBlockedReason[0] != '\0'
            ? saveBlockedReason : statusText;
    if (visibleStatus != nullptr && visibleStatus[0] != '\0') {
        engine::Text(
                ui, statusConfig,
                assets,
                Rectangle{horizontalPadding, messageY, rowWidth, messageHeight},
                smallFont,
                visibleStatus,
                engine::UITextJustify::Left,
                config.invalidColor,
                true);
    }
    engine::EndScrollArea(ui, config, input, scrollArea, scroll);
    engine::EndUI(ui, config, input, assets);
    return selected;
}

bool DrawGameOverOverlay(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    DrawRectangleRec(config.overlayBounds, Color{0, 0, 0, 205});
    constexpr float panelWidth = 520.0f;
    constexpr float panelHeight = 270.0f;
    constexpr float padding = 44.0f;
    const Rectangle panel{
            config.overlayBounds.x
                    + (config.overlayBounds.width - panelWidth) * 0.5f,
            config.overlayBounds.y
                    + (config.overlayBounds.height - panelHeight) * 0.5f,
            panelWidth,
            panelHeight};
    DrawRectangleRounded(
            panel, config.cornerRadius, config.cornerSegments,
            config.panelColor);
    DrawRectangleRoundedLinesEx(
            panel, config.cornerRadius, config.cornerSegments,
            config.borderThickness, config.borderColor);
    engine::BeginUI(ui, input);
    engine::Text(
            config, assets,
            {panel.x + padding, panel.y + 30.0f,
                    panel.width - padding * 2.0f, 64.0f},
            font, "Game Over", engine::UITextJustify::Center);
    engine::Text(
            config, assets,
            {panel.x + padding, panel.y + 98.0f,
                    panel.width - padding * 2.0f, 40.0f},
            smallFont, "You died.", engine::UITextJustify::Center);
    const bool selected = engine::Button(
            ui, config, input, assets,
            "game_over_main_menu",
            {panel.x + padding, panel.y + 170.0f,
                    panel.width - padding * 2.0f, 56.0f},
            font, "Main Menu");
    engine::EndUI(ui, config, input, assets);
    return selected;
}

GameGraphicsSettingsAction DrawGameGraphicsSettings(
        engine::UIContext& ui,
        engine::UIScrollState& scroll,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        FpsApplicationSettings& draft,
        const char* statusText)
{
    DrawRectangleRec(config.overlayBounds, Color{0, 0, 0, 128});
    const float panelWidth = std::min(680.0f,
            std::max(0.0f, config.overlayBounds.width - 48.0f));
    const float padding = std::min(32.0f, panelWidth * 0.08f);
    const float contentWidth = std::max(0.0f, panelWidth - config.scrollbarSize);
    const float rowWidth = std::max(0.0f, contentWidth - padding * 2.0f);
    engine::UIConfig textConfig = config;
    const engine::FontAsset* textFont = assets.GetFont(smallFont);
    if (textFont != nullptr) textConfig.fontSize = static_cast<float>(textFont->pixelSize);
    const float rowHeight = std::max(48.0f, textConfig.fontSize + config.paddingY * 2.0f);
    const auto textWidth = [&](const char* text) {
        return textFont != nullptr
                ? MeasureTextEx(textFont->font, text, textConfig.fontSize,
                        textConfig.textSpacing).x
                : static_cast<float>(MeasureText(text, static_cast<int>(textConfig.fontSize)));
    };
    float labelWidth = 0.0f;
    for (std::size_t i = 0; i < GameSettingsLabels.size(); ++i) {
        if (GameSettingsRowHasValue(static_cast<GameSettingsRow>(i))) {
            labelWidth = std::max(labelWidth,
                    textWidth(GameSettingsLabels[i]) + config.paddingX * 2.0f);
        }
    }
    std::array<float, GameSettingsLabels.size()> textHeights{};
    const float checkboxTextOffset = rowHeight + config.paddingX * 2.0f;
    for (std::size_t i = 0; i < textHeights.size(); ++i) {
        const auto row = static_cast<GameSettingsRow>(i);
        const bool checkbox = row >= GameSettingsRow::Fxaa && row <= GameSettingsRow::Vsync;
        textHeights[i] = engine::MeasureWrappedTextHeight(textConfig, assets,
                std::max(1.0f, rowWidth - (checkbox ? checkboxTextOffset : 0.0f)),
                smallFont, GameSettingsLabels[i]);
    }
    const float statusHeight = engine::MeasureWrappedTextHeight(
            textConfig, assets, rowWidth, smallFont, statusText);
    const GameSettingsLayout layout = MeasureGameSettingsLayout(
            contentWidth, padding, labelWidth, rowHeight, textHeights, statusHeight,
            textWidth("Defaults") + config.paddingX * 2.0f + 16.0f);
    const float panelHeight = std::min(layout.contentHeight,
            std::max(0.0f, config.overlayBounds.height - 48.0f));
    const Rectangle panel{
            config.overlayBounds.x + (config.overlayBounds.width - panelWidth) * 0.5f,
            config.overlayBounds.y + (config.overlayBounds.height - panelHeight) * 0.5f,
            panelWidth, panelHeight};
    DrawRectangleRounded(panel, config.cornerRadius, config.cornerSegments, config.panelColor);
    DrawRectangleRoundedLinesEx(panel, config.cornerRadius, config.cornerSegments,
            config.borderThickness, config.borderColor);

    engine::BeginUI(ui, input);
    const engine::UIScrollAreaResult scrollArea = engine::BeginScrollArea(
            ui, config, input, "game_settings_content", panel,
            {contentWidth, layout.contentHeight}, scroll, false, 0.0f);
    engine::Text(ui, config, assets,
            {padding, 24.0f, rowWidth, 54.0f}, font, "Settings", engine::UITextJustify::Center);

    // Slider value space follows the actual font metrics.
    const float valueWidth = std::max(64.0f, textWidth("2.00") + config.paddingX * 2.0f);
    const auto slider = [&](const char* id, Rectangle bounds, float minValue,
                            float maxValue, float& value) {
        Rectangle field = bounds;
        field.width = std::max(0.0f, bounds.width - valueWidth - 8.0f);
        engine::Slider(ui, config, input, id, field, minValue, maxValue, value);
        char text[16];
        std::snprintf(text, sizeof(text), "%.2f", value);
        engine::Text(ui, textConfig, assets,
                {bounds.x + bounds.width - valueWidth, bounds.y, valueWidth, bounds.height},
                smallFont, text, engine::UITextJustify::Right);
    };
    const auto intSlider = [&](const char* id, Rectangle bounds, int minValue,
                               int maxValue, int& value, bool zeroMeansAll = false) {
        Rectangle field = bounds;
        field.width = std::max(0.0f, bounds.width - valueWidth - 8.0f);
        engine::IntSlider(ui, config, input, id, field, minValue, maxValue, value);
        char text[16];
        if (zeroMeansAll && value == 0) std::snprintf(text, sizeof(text), "All");
        else std::snprintf(text, sizeof(text), "%d", value);
        engine::Text(ui, textConfig, assets,
                {bounds.x + bounds.width - valueWidth, bounds.y, valueWidth, bounds.height},
                smallFont, text, engine::UITextJustify::Right);
    };
    bool* checkboxValues[] = {&draft.graphics.fxaa, &draft.graphics.depthPrepass,
            &draft.hdrBloom.enabled, &draft.graphics.showFpsCounter,
            &draft.graphics.performanceOverlay, &draft.graphics.vsync};
    const char* checkboxIds[] = {"graphics_fxaa", "graphics_depth_prepass", "graphics_bloom",
            "graphics_fps_counter", "graphics_performance_overlay", "graphics_vsync"};
    static_assert(sizeof(checkboxValues) / sizeof(checkboxValues[0])
            == static_cast<std::size_t>(GameSettingsRow::Vsync)
                    - static_cast<std::size_t>(GameSettingsRow::Fxaa) + 1);
    for (std::size_t i = 0; i < layout.rows.size(); ++i) {
        const auto row = static_cast<GameSettingsRow>(i);
        const Rectangle bounds = layout.rows[i];
        const Rectangle field = layout.fields[i];
        if (row >= GameSettingsRow::Fxaa && row <= GameSettingsRow::Vsync) {
            const std::size_t index = i - static_cast<std::size_t>(GameSettingsRow::Fxaa);
            // Keep the check mark its usual size when the label wraps, while
            // retaining a click target over the entire row.
            engine::UIConfig checkboxConfig = config;
            checkboxConfig.paddingY += (bounds.height - rowHeight) * 0.5f;
            engine::Checkbox(ui, checkboxConfig, input, assets, checkboxIds[index],
                    bounds, smallFont, "", *checkboxValues[index]);
            engine::Text(ui, textConfig, assets,
                    {bounds.x + checkboxTextOffset, bounds.y,
                            std::max(0.0f, bounds.width - checkboxTextOffset), bounds.height},
                    smallFont, GameSettingsLabels[i], engine::UITextJustify::Left, BLANK, true);
            continue;
        }
        engine::Text(ui, textConfig, assets, bounds, smallFont, GameSettingsLabels[i],
                engine::UITextJustify::Left,
                GameSettingsRowHasValue(row) ? config.textColor : config.mutedTextColor, true);
        switch (row) {
            case GameSettingsRow::Gamma:
                slider("graphics_gamma", field, engine::MinimumDisplayGamma,
                        engine::MaximumDisplayGamma, draft.graphics.gamma);
                break;
            case GameSettingsRow::Sensitivity:
                slider("settings_mouse_sensitivity", field, 0.0f, 5.0f,
                        draft.playerCamera.mouseSensitivity);
                break;
            case GameSettingsRow::RenderScale: {
                const char* options[] = {"75%", "100%", "125%", "150%", "200%"};
                const float scales[] = {0.75f, 1.0f, 1.25f, 1.5f, 2.0f};
                int selected = 0;
                for (int j = 1; j < 5; ++j) {
                    if (std::fabs(draft.graphics.renderScale - scales[j])
                            < std::fabs(draft.graphics.renderScale - scales[selected])) selected = j;
                }
                if (engine::Option(ui, textConfig, input, assets, "graphics_render_scale",
                            field, smallFont, options, 5, selected)) draft.graphics.renderScale = scales[selected];
                break;
            }
            case GameSettingsRow::LightBudget:
                intSlider("graphics_dynamic_light_budget", field,
                        MinFpsDynamicLights, MaxFpsDynamicLights, draft.graphics.maxDynamicLights);
                break;
            case GameSettingsRow::ShadowUpdates:
                intSlider("graphics_shadow_updates_per_frame", field,
                        MinFpsShadowLightUpdatesPerFrame, MaxFpsShadowLightUpdatesPerFrame,
                        draft.graphics.maxShadowLightUpdatesPerFrame, true);
                break;
            case GameSettingsRow::ShadowQuality: {
                const char* options[] = {"Off", "Low", "Medium", "High"};
                int selected = static_cast<int>(draft.graphics.shadowQuality);
                if (engine::Option(ui, textConfig, input, assets, "graphics_shadow_quality",
                            field, smallFont, options, 4, selected)) {
                    draft.graphics.shadowQuality = static_cast<FpsShadowQuality>(selected);
                }
                break;
            }
            case GameSettingsRow::Fov:
                intSlider("graphics_horizontal_fov", field,
                        MinFpsHorizontalFovDegrees, MaxFpsHorizontalFovDegrees,
                        draft.graphics.horizontalFovDegrees);
                break;
            default: break;
        }
    }
    GameGraphicsSettingsAction result = GameGraphicsSettingsAction::None;
    const char* buttonLabels[] = {"Defaults", "Cancel", "Apply"};
    const char* buttonIds[] = {"graphics_defaults", "graphics_cancel", "graphics_apply"};
    const GameGraphicsSettingsAction actions[] = {GameGraphicsSettingsAction::Defaults,
            GameGraphicsSettingsAction::Cancel, GameGraphicsSettingsAction::Apply};
    for (std::size_t i = 0; i < layout.buttons.size(); ++i) {
        if (engine::Button(ui, textConfig, input, assets, buttonIds[i], layout.buttons[i],
                    smallFont, buttonLabels[i])) result = actions[i];
    }
    if (statusHeight > 0.0f) {
        engine::Text(ui, textConfig, assets, layout.status, smallFont, statusText,
                engine::UITextJustify::Left, config.invalidColor, true);
    }
    engine::EndScrollArea(ui, config, input, scrollArea, scroll);
    engine::EndUI(ui, config, input, assets);
    return result;
}

} // namespace game
