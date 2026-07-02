#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"

#include <raylib.h>

#include <algorithm>

namespace game {

inline constexpr float SectorEditorInspectorTextureActionHeight = 36.0f;
inline constexpr float SectorEditorInspectorTextureValueHeight = 22.0f;
inline constexpr float SectorEditorInspectorTextureValueIndent = 12.0f;
inline constexpr float SectorEditorInspectorCompactInputLabelWidth = 82.0f;
inline constexpr float SectorEditorInspectorCompactInputWidth = 104.0f;
inline constexpr float SectorEditorInspectorFloatInputWidth = 112.0f;
inline constexpr float SectorEditorInspectorIntInputWidth = 150.0f;

struct SectorEditorInspectorTextureRowLayout {
    Rectangle labelRect = {};
    Rectangle clearButtonRect = {};
    Rectangle pickerButtonRect = {};
    Rectangle valueRect = {};
    float height = 0.0f;
};

struct SectorEditorInspectorNumericRowLayout {
    Rectangle labelRect = {};
    Rectangle inputRect = {};
};

inline float SectorEditorInspectorTextureRowHeight()
{
    return SectorEditorInspectorTextureActionHeight
            + 2.0f
            + SectorEditorInspectorTextureValueHeight;
}

inline SectorEditorInspectorTextureRowLayout BuildSectorEditorInspectorTextureRowLayout(
        float y,
        float contentW,
        float gap,
        float pickerButtonW,
        float clearButtonW)
{
    SectorEditorInspectorTextureRowLayout layout;
    layout.height = SectorEditorInspectorTextureRowHeight();
    layout.pickerButtonRect = Rectangle{
            std::max(0.0f, contentW - pickerButtonW),
            y,
            pickerButtonW,
            SectorEditorInspectorTextureActionHeight};
    if (clearButtonW > 0.0f) {
        layout.clearButtonRect = Rectangle{
                std::max(0.0f, contentW - pickerButtonW - gap - clearButtonW),
                y,
                clearButtonW,
                SectorEditorInspectorTextureActionHeight};
    }
    const float actionStartX = clearButtonW > 0.0f
            ? layout.clearButtonRect.x
            : layout.pickerButtonRect.x;
    layout.labelRect = Rectangle{
            0.0f,
            y,
            std::max(0.0f, actionStartX - gap),
            SectorEditorInspectorTextureActionHeight};
    layout.valueRect = Rectangle{
            SectorEditorInspectorTextureValueIndent,
            y + SectorEditorInspectorTextureActionHeight + 2.0f,
            std::max(0.0f, contentW - SectorEditorInspectorTextureValueIndent),
            SectorEditorInspectorTextureValueHeight};
    return layout;
}

inline SectorEditorInspectorNumericRowLayout BuildSectorEditorInspectorCompactNumericRowLayout(
        float y,
        float contentW,
        float rowH)
{
    const float inputW = std::min(
            SectorEditorInspectorCompactInputWidth,
            std::max(0.0f, contentW - SectorEditorInspectorCompactInputLabelWidth));
    return SectorEditorInspectorNumericRowLayout{
            Rectangle{0.0f, y, SectorEditorInspectorCompactInputLabelWidth, rowH},
            Rectangle{SectorEditorInspectorCompactInputLabelWidth, y, inputW, rowH}};
}

inline SectorEditorInspectorNumericRowLayout BuildSectorEditorInspectorRightNumericRowLayout(
        float y,
        float contentW,
        float rowH,
        float gap,
        float inputWidth)
{
    const float inputW = std::min(std::max(0.0f, inputWidth), std::max(0.0f, contentW));
    const float inputX = std::max(0.0f, contentW - inputW);
    return SectorEditorInspectorNumericRowLayout{
            Rectangle{0.0f, y, std::max(0.0f, inputX - gap), rowH},
            Rectangle{inputX, y, inputW, rowH}};
}

inline SectorEditorInspectorNumericRowLayout BuildSectorEditorInspectorRightFloatRowLayout(
        float y,
        float contentW,
        float rowH,
        float gap)
{
    return BuildSectorEditorInspectorRightNumericRowLayout(
            y,
            contentW,
            rowH,
            gap,
            SectorEditorInspectorFloatInputWidth);
}

inline SectorEditorInspectorNumericRowLayout BuildSectorEditorInspectorRightIntRowLayout(
        float y,
        float contentW,
        float rowH,
        float gap)
{
    return BuildSectorEditorInspectorRightNumericRowLayout(
            y,
            contentW,
            rowH,
            gap,
            SectorEditorInspectorIntInputWidth);
}

struct SectorEditorFloatInputResult {
    bool changed = false;
    float value = 0.0f;
    bool finite = true;
};

struct SectorEditorIntInputResult {
    bool changed = false;
    int value = 0;
};

struct SectorEditorRgb8InputResult {
    bool changed = false;
    unsigned char channel = 0;
};

struct SectorEditorTintFloatInputResult {
    bool changed = false;
    float value = 0.0f;
    bool finite = true;
};

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
        int decimals);

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
        int step);

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
        engine::UIIntInputState& inputState);

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
        engine::UIFloatInputState& inputState);

void DrawColorSwatch(
        const engine::UIConfig& config,
        Rectangle bounds,
        Color color,
        float borderThickness);

engine::UIConfig SectorEditorSmallFontConfig(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle smallFont);

float MeasureSectorEditorWrappedTextHeight(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const char* text,
        float boundsWidth,
        int minimumLines = 1);

} // namespace game
