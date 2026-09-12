#pragma once

#include "engine/ui/UI.h"

namespace engine {

struct UITextEditLayout {
    Vector2 textPosition = {};
    Rectangle caretBounds = {};
    Rectangle clipBounds = {};
};

// Measures a UTF-8 prefix without copying or temporarily terminating the buffer.
float MeasureUITextEditPrefix(
        Font font, float fontSize, float spacing,
        const char* text, size_t endByteIndex);

// Bounds are already transformed into screen coordinates. Only the focused
// widget updates the shared scrolling state.
UITextEditLayout BuildUITextEditLayout(
        UIContext& ui, const UIConfig& config, uint32_t widgetId,
        Rectangle bounds, float textWidth, float prefixWidth,
        UITextJustify justify);

} // namespace engine
