#pragma once

#include "raylib.h"
#include <algorithm>
#include <array>
#include <cstddef>

namespace game {

enum class SectorMaterialFormRow : std::size_t {
    Id, Albedo, Filter, Metallic, Roughness, NormalStrength,
    MacroEnabled, MacroMask, MacroClear, MacroRepeat, MacroDarkening, MacroRoughness,
    MacroHelp, NormalStatus, PropertyStatus, PreviewTitle, PreviewImage, Count
};

struct SectorMaterialFormLayout {
    std::array<Rectangle, static_cast<std::size_t>(SectorMaterialFormRow::Count)> rows{};
    float fieldX = 0.0f;
    float fieldOffsetY = 0.0f;
    float labelWidth = 0.0f;
    float contentHeight = 0.0f;
};

inline SectorMaterialFormLayout MeasureSectorMaterialForm(
        float width, float measuredLabelWidth, float labelHeight, bool macroExpanded,
        float macroHelpHeight, float normalStatusHeight, float propertyStatusHeight)
{
    SectorMaterialFormLayout layout;
    layout.labelWidth = measuredLabelWidth;
    const bool stacked = width < measuredLabelWidth + 12.0f + 160.0f;
    layout.fieldX = stacked ? 0.0f : measuredLabelWidth + 12.0f;
    layout.fieldOffsetY = stacked ? labelHeight + 6.0f : 0.0f;
    for (std::size_t i = 0; i < layout.rows.size(); ++i) {
        const auto row = static_cast<SectorMaterialFormRow>(i);
        if (!macroExpanded && row >= SectorMaterialFormRow::MacroMask
                && row <= SectorMaterialFormRow::MacroHelp) continue;
        float height = 40.0f;
        if (row <= SectorMaterialFormRow::NormalStrength
                || row == SectorMaterialFormRow::MacroMask
                || (row >= SectorMaterialFormRow::MacroRepeat
                        && row <= SectorMaterialFormRow::MacroRoughness)) {
            height += layout.fieldOffsetY;
        }
        if (row == SectorMaterialFormRow::MacroHelp) height = macroHelpHeight;
        if (row == SectorMaterialFormRow::NormalStatus) height = normalStatusHeight;
        if (row == SectorMaterialFormRow::PropertyStatus) height = propertyStatusHeight;
        if (row == SectorMaterialFormRow::PreviewImage) height = 320.0f;
        layout.rows[i] = {0.0f, layout.contentHeight, width, height};
        layout.contentHeight += height + 9.0f;
    }
    layout.contentHeight += 12.0f;
    return layout;
}

} // namespace game
