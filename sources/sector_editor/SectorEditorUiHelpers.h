#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"

#include <raylib.h>

#include <algorithm>
#include <string_view>

namespace game {

inline constexpr float SectorEditorInspectorTextureActionHeight = 36.0f;
inline constexpr float SectorEditorInspectorTextureValueHeight = 22.0f;
inline constexpr float SectorEditorInspectorTextureValueIndent = 12.0f;
inline constexpr float SectorEditorInspectorCompactInputLabelWidth = 82.0f;
inline constexpr float SectorEditorInspectorCompactInputWidth = 104.0f;
inline constexpr float SectorEditorInspectorFloatInputWidth = 112.0f;
inline constexpr float SectorEditorInspectorIntInputWidth = 150.0f;
inline constexpr float SectorEditorInspectorStackedLabelHeight = 26.0f;

inline std::string_view SectorEditorModelFilename(std::string_view modelPath)
{
    const size_t separator = modelPath.find_last_of("/\\");
    return separator == std::string_view::npos
            ? modelPath
            : modelPath.substr(separator + 1);
}

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

struct SectorEditorInspectorStackedOptionRowLayout {
    Rectangle labelRect = {};
    Rectangle fieldRect = {};
    float height = 0.0f;
};

struct SectorEditorDoorTextureSettingsModalLayout {
    Rectangle titleRect = {};
    Rectangle faceButtonRects[6] = {};
    Rectangle uvLabelRects[4] = {};
    Rectangle uvInputRects[4] = {};
    Rectangle actionButtonRects[6] = {};
    Rectangle statusRect = {};
    Rectangle doneButtonRect = {};
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

inline float SectorEditorInspectorStackedOptionRowHeight(float rowH, float gap)
{
    return SectorEditorInspectorStackedLabelHeight + gap + rowH;
}

inline SectorEditorInspectorStackedOptionRowLayout BuildSectorEditorInspectorStackedOptionRowLayout(
        float y,
        float contentW,
        float rowH,
        float gap)
{
    return SectorEditorInspectorStackedOptionRowLayout{
            Rectangle{0.0f, y, contentW, SectorEditorInspectorStackedLabelHeight},
            Rectangle{0.0f, y + SectorEditorInspectorStackedLabelHeight + gap, contentW, rowH},
            SectorEditorInspectorStackedOptionRowHeight(rowH, gap)};
}

inline float SectorEditorRuntimeObjectInspectorContentHeight(
        float rowH,
        float gap,
        bool isBillboard,
        bool keepAspectWarningVisible,
        bool directionalBillboard,
        float spriteLabelHeight,
        float aspectWarningHeight)
{
    float height = 0.0f;
    height += 38.0f;
    height += 34.0f;
    height += spriteLabelHeight + gap;
    if (isBillboard) {
        height += rowH + gap;
    }
    height += (rowH + gap) * 4.0f;
    if (isBillboard) {
        height += (rowH + gap) * 2.0f;
        height += rowH + gap;
        if (keepAspectWarningVisible) {
            height += aspectWarningHeight + gap;
        }
        height += (rowH + gap) * 2.0f;
        height += rowH + gap;
        height += (SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap)
                * static_cast<float>(directionalBillboard ? 4 : 1);
        height += rowH + gap;
    }
    height += rowH + gap;
    return height;
}

inline float SectorEditorDoorInspectorContentHeight(
        float rowH,
        float gap,
        float anchorStatusHeight,
        float assetStatusHeight,
        float modelDiagnosticHeight,
        bool modelVisual,
        bool swingMotion)
{
    float height = 0.0f;
    height += 38.0f;
    height += 34.0f;
    height += anchorStatusHeight + gap;
    height += (rowH + gap) * 3.0f;
    height += SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap;
    if (modelVisual) {
        height += SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap;
        height += modelDiagnosticHeight + gap;
        height += SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap;
        height += (rowH + gap) * 2.0f;
        height += (SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap) * 2.0f;
        height += (rowH + gap) * 2.0f;
    } else {
        height += rowH + gap;
        height += SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap;
        height += swingMotion
                ? (SectorEditorInspectorStackedOptionRowHeight(rowH, gap) + gap) * 2.0f
                        + (rowH + gap) * 2.0f
                : (rowH + gap) * 2.0f;
    }
    height += (rowH + gap) * 5.0f;
    height += assetStatusHeight + gap;
    height += (rowH + gap) * (modelVisual ? 3.0f : 5.0f);
    return height;
}

inline SectorEditorDoorTextureSettingsModalLayout BuildSectorEditorDoorTextureSettingsModalLayout(
        Rectangle modal,
        float padding,
        float gap)
{
    SectorEditorDoorTextureSettingsModalLayout layout;
    const float contentW = std::max(0.0f, modal.width - padding * 2.0f);
    float y = modal.y + 24.0f;

    layout.titleRect = Rectangle{modal.x + padding, y, contentW, 38.0f};
    y += 56.0f;

    const float faceButtonH = 36.0f;
    const float faceButtonW = (contentW - gap * 2.0f) / 3.0f;
    for (int i = 0; i < 6; ++i) {
        const int col = i % 3;
        const int row = i / 3;
        layout.faceButtonRects[i] = Rectangle{
                modal.x + padding + static_cast<float>(col) * (faceButtonW + gap),
                y + static_cast<float>(row) * (faceButtonH + gap),
                faceButtonW,
                faceButtonH};
    }
    y += faceButtonH * 2.0f + gap + 24.0f;

    const float labelW = 116.0f;
    const float inputW = 160.0f;
    const float inputH = 36.0f;
    for (int i = 0; i < 4; ++i) {
        layout.uvLabelRects[i] = Rectangle{modal.x + padding, y, labelW, inputH};
        layout.uvInputRects[i] = Rectangle{modal.x + padding + labelW + gap, y, inputW, inputH};
        y += inputH + gap;
    }
    y += 8.0f;

    const float actionButtonH = 36.0f;
    const float actionButtonW = (contentW - gap * 2.0f) / 3.0f;
    for (int i = 0; i < 6; ++i) {
        const int col = i % 3;
        const int row = i / 3;
        layout.actionButtonRects[i] = Rectangle{
                modal.x + padding + static_cast<float>(col) * (actionButtonW + gap),
                y + static_cast<float>(row) * (actionButtonH + gap),
                actionButtonW,
                actionButtonH};
    }
    y += actionButtonH * 2.0f + gap + 18.0f;

    const float doneW = 124.0f;
    const float doneH = 38.0f;
    layout.doneButtonRect = Rectangle{
            modal.x + modal.width - padding - doneW,
            modal.y + modal.height - padding - doneH,
            doneW,
            doneH};
    layout.statusRect = Rectangle{
            modal.x + padding,
            y,
            std::max(0.0f, layout.doneButtonRect.x - modal.x - padding - gap),
            std::max(0.0f, layout.doneButtonRect.y - y),
    };
    return layout;
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
