#pragma once

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace game {

enum class GameSettingsRow : std::size_t {
    Gamma, GammaHelp, Sensitivity, RenderScale, LightBudget, ShadowUpdates,
    ShadowQuality, Fov, Fxaa, DepthPrepass, Bloom, FpsCounter, Performance,
    Vsync, VsyncHelp, Count
};

inline constexpr std::array<const char*, static_cast<std::size_t>(GameSettingsRow::Count)>
        GameSettingsLabels = {
    "Gamma", "1.00 is neutral; higher values brighten dark areas.",
    "Mouse sensitivity", "Render scale", "Dynamic light budget",
    "Shadow updates/frame", "Shadow quality", "Horizontal FOV", "FXAA",
    "Depth pre-pass", "HDR bloom", "FPS counter", "Performance overlay (F9)",
    "VSync", "You must restart the game for VSync changes to take effect."
};

inline bool GameSettingsRowHasValue(GameSettingsRow row)
{
    return row <= GameSettingsRow::Fov && row != GameSettingsRow::GammaHelp;
}

struct GameSettingsLayout {
    std::array<Rectangle, GameSettingsLabels.size()> rows{};
    std::array<Rectangle, GameSettingsLabels.size()> fields{};
    std::array<Rectangle, 3> buttons{};
    Rectangle status{};
    float contentHeight = 0.0f;
};

// Coordinates are local to the scroll content. Drawing and measurement share
// the same rows, including wrapped help/status text and bottom padding.
inline GameSettingsLayout MeasureGameSettingsLayout(
        float width, float padding, float measuredLabelWidth, float controlHeight,
        const std::array<float, GameSettingsLabels.size()>& textHeights,
        float statusHeight, float minimumButtonWidth = 110.0f)
{
    GameSettingsLayout layout;
    const float rowWidth = std::max(0.0f, width - padding * 2.0f);
    const bool stacked = rowWidth < measuredLabelWidth + 16.0f + 200.0f;
    float y = 96.0f;
    for (std::size_t i = 0; i < layout.rows.size(); ++i) {
        const auto row = static_cast<GameSettingsRow>(i);
        if (GameSettingsRowHasValue(row)) {
            const float labelHeight = std::max(controlHeight, textHeights[i]);
            layout.rows[i] = {padding, y,
                    stacked ? rowWidth : measuredLabelWidth, labelHeight};
            layout.fields[i] = stacked
                    ? Rectangle{padding, y + labelHeight + 4.0f, rowWidth, controlHeight}
                    : Rectangle{padding + measuredLabelWidth + 16.0f, y,
                            rowWidth - measuredLabelWidth - 16.0f, controlHeight};
            y += stacked ? labelHeight + 4.0f + controlHeight : labelHeight;
        } else {
            const bool help = row == GameSettingsRow::GammaHelp
                    || row == GameSettingsRow::VsyncHelp;
            const float height = help ? textHeights[i]
                    : std::max(controlHeight, textHeights[i]);
            layout.rows[i] = {padding, y, rowWidth, height};
            y += height;
        }
        y += 12.0f;
    }
    // Put errors before the actions so even long messages cannot push the
    // final control out of view at maximum scroll.
    layout.status = {padding, y, rowWidth, statusHeight};
    if (statusHeight > 0.0f) y += statusHeight + 12.0f;
    // Stack action buttons if their labels would have too little room.
    const bool stackButtons = rowWidth < minimumButtonWidth * 3.0f + 20.0f;
    const float buttonWidth = stackButtons ? rowWidth : (rowWidth - 20.0f) / 3.0f;
    for (std::size_t i = 0; i < layout.buttons.size(); ++i) {
        layout.buttons[i] = {padding + (stackButtons ? 0.0f : i * (buttonWidth + 10.0f)),
                y, buttonWidth, controlHeight};
        if (stackButtons) y += controlHeight + 10.0f;
    }
    if (!stackButtons) y += controlHeight + 10.0f;
    layout.contentHeight = y + 24.0f;
    return layout;
}

} // namespace game
