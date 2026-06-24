#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"

#include <raylib.h>

namespace game {

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

} // namespace game
