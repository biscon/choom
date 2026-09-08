#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"

#include <algorithm>

namespace game {

inline bool DrawSectorEditorAssetPickerFilter(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const char* id,
        Rectangle bounds,
        char* buffer,
        size_t capacity)
{
    const engine::FontAsset* fontAsset = assets.GetFont(font);
    const float textWidth = fontAsset != nullptr
            ? MeasureTextEx(fontAsset->font, "Filter", config.fontSize, config.textSpacing).x
            : static_cast<float>(MeasureText("Filter", static_cast<int>(config.fontSize)));
    const float labelWidth = std::max(82.0f, textWidth + config.paddingX * 2.0f);
    engine::Text(config, assets,
            Rectangle{bounds.x, bounds.y, labelWidth, bounds.height},
            font, "Filter", engine::UITextJustify::Left, config.mutedTextColor);
    return engine::TextInput(
            ui, config, input, assets, id,
            Rectangle{bounds.x + labelWidth, bounds.y,
                    std::max(0.0f, bounds.width - labelWidth), bounds.height},
            font, buffer, capacity, 0, capacity - 1).changed;
}

} // namespace game
