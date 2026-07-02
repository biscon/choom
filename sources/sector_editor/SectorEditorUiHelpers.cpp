#include "sector_editor/SectorEditorUiHelpers.h"

#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace game {
namespace {

float MeasureTextSliceWidth(
        engine::AssetManager& assets,
        engine::FontHandle font,
        const char* text,
        size_t start,
        size_t count,
        float fontSize,
        float spacing)
{
    std::string line(text + start, count);
    if (const engine::FontAsset* fontAsset = assets.GetFont(font)) {
        return MeasureTextEx(fontAsset->font, line.c_str(), fontSize, spacing).x;
    }
    return static_cast<float>(MeasureText(line.c_str(), static_cast<int>(fontSize)));
}

} // namespace

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

engine::UIConfig SectorEditorSmallFontConfig(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle smallFont)
{
    engine::UIConfig smallConfig = config;
    if (const engine::FontAsset* fontAsset = assets.GetFont(smallFont)) {
        smallConfig.fontSize = static_cast<float>(fontAsset->pixelSize);
    }
    return smallConfig;
}

float MeasureSectorEditorWrappedTextHeight(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const char* text,
        float boundsWidth,
        int minimumLines)
{
    const char* safeText = text == nullptr ? "" : text;
    const size_t byteCount = std::strlen(safeText);
    const float contentWidth = std::max(0.0f, boundsWidth - config.paddingX * 2.0f);
    const float lineAdvance = config.fontSize + config.textSpacing;
    int lineCount = 0;
    if (byteCount == 0 || contentWidth <= 0.0f || config.fontSize <= 0.0f) {
        lineCount = 1;
    } else {
        size_t lineStart = 0;
        size_t lineEnd = 0;
        size_t lastBreak = 0;
        bool hasBreak = false;
        for (size_t cursor = 0; cursor < byteCount;) {
            const size_t next = cursor + 1;
            const char ch = safeText[cursor];
            if (ch == '\n') {
                ++lineCount;
                lineStart = next;
                lineEnd = next;
                lastBreak = next;
                hasBreak = false;
                cursor = next;
                continue;
            }
            const float measuredWidth = MeasureTextSliceWidth(
                    assets,
                    font,
                    safeText,
                    lineStart,
                    next - lineStart,
                    config.fontSize,
                    config.textSpacing);
            if (measuredWidth > contentWidth && lineEnd > lineStart) {
                const size_t breakEnd = hasBreak && lastBreak > lineStart ? lastBreak : lineEnd;
                ++lineCount;
                lineStart = breakEnd;
                while (lineStart < byteCount && (safeText[lineStart] == ' ' || safeText[lineStart] == '\t')) {
                    ++lineStart;
                }
                cursor = lineStart;
                lineEnd = lineStart;
                lastBreak = lineStart;
                hasBreak = false;
                continue;
            }
            lineEnd = next;
            if (ch == ' ' || ch == '\t') {
                lastBreak = next;
                hasBreak = true;
            }
            cursor = next;
        }
        ++lineCount;
    }
    lineCount = std::max(lineCount, std::max(1, minimumLines));
    return config.paddingY * 2.0f
            + config.fontSize
            + static_cast<float>(lineCount - 1) * lineAdvance;
}

} // namespace game
