#include "sector_editor/SectorEditorUiHelpers.h"

#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <cmath>

namespace game {

SectorEditorFloatInputResult DrawLabeledFloatInput(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const char* id,
        const char* label,
        Rectangle labelRect,
        Rectangle inputRect,
        engine::UITextJustify labelJustify,
        float value,
        engine::UIFloatInputState& inputState,
        float minValue,
        float maxValue,
        int decimals)
{
    engine::Text(ui, config, assets, labelRect, font, label, labelJustify, config.mutedTextColor);
    float edited = value;
    const engine::UINumericInputResult result = engine::FloatInput(
            ui,
            config,
            input,
            assets,
            id,
            inputRect,
            font,
            edited,
            inputState,
            minValue,
            maxValue,
            decimals);
    return SectorEditorFloatInputResult{result.changed, edited, std::isfinite(edited)};
}

SectorEditorIntInputResult DrawLabeledIntInput(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const char* id,
        const char* label,
        Rectangle labelRect,
        Rectangle inputRect,
        engine::UITextJustify labelJustify,
        int value,
        engine::UIIntInputState& inputState,
        int minValue,
        int maxValue,
        int step)
{
    engine::Text(ui, config, assets, labelRect, font, label, labelJustify, config.mutedTextColor);
    int edited = value;
    const engine::UINumericInputResult result = engine::IntInput(
            ui,
            config,
            input,
            assets,
            id,
            inputRect,
            font,
            edited,
            inputState,
            minValue,
            maxValue,
            step);
    return SectorEditorIntInputResult{result.changed, edited};
}

SectorEditorRgb8InputResult DrawRgb8ChannelInput(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const char* id,
        const char* label,
        Rectangle labelRect,
        Rectangle inputRect,
        engine::UITextJustify labelJustify,
        unsigned char channel,
        engine::UIIntInputState& inputState)
{
    const SectorEditorIntInputResult result = DrawLabeledIntInput(
            ui,
            config,
            input,
            assets,
            font,
            id,
            label,
            labelRect,
            inputRect,
            labelJustify,
            static_cast<int>(channel),
            inputState,
            0,
            255,
            1);
    return SectorEditorRgb8InputResult{
            result.changed,
            static_cast<unsigned char>(ClampAmbientChannel(result.value))};
}

SectorEditorTintFloatInputResult DrawNormalizedTintFloatInput(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const char* id,
        const char* label,
        Rectangle labelRect,
        Rectangle inputRect,
        engine::UITextJustify labelJustify,
        float value,
        engine::UIFloatInputState& inputState)
{
    const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
            ui,
            config,
            input,
            assets,
            font,
            id,
            label,
            labelRect,
            inputRect,
            labelJustify,
            value,
            inputState,
            0.0f,
            1.0f,
            3);
    return SectorEditorTintFloatInputResult{
            result.changed,
            std::clamp(result.value, 0.0f, 1.0f),
            result.finite};
}

void DrawColorSwatch(
        const engine::UIConfig& config,
        Rectangle bounds,
        Color color,
        float borderThickness)
{
    DrawRectangleRec(bounds, color);
    DrawRectangleLinesEx(bounds, borderThickness, config.borderColor);
}

} // namespace game
