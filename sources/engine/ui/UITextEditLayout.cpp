#include "engine/ui/UITextEditLayout.h"

#include <algorithm>
#include <cmath>

namespace engine {

float MeasureUITextEditPrefix(
        Font font, float fontSize, float spacing,
        const char* text, size_t endByteIndex)
{
    if (text == nullptr || font.baseSize <= 0 || font.glyphCount <= 0
            || font.glyphs == nullptr || font.recs == nullptr) {
        return 0.0f;
    }

    float width = 0.0f;
    size_t glyphCount = 0;
    const float scale = fontSize / static_cast<float>(font.baseSize);
    for (size_t cursor = 0; cursor < endByteIndex && text[cursor] != '\0';) {
        int byteCount = 0;
        const int codepoint = GetCodepointNext(text + cursor, &byteCount);
        if (byteCount <= 0 || static_cast<size_t>(byteCount) > endByteIndex - cursor) {
            break;
        }
        const int index = GetGlyphIndex(font, codepoint);
        if (index < 0 || index >= font.glyphCount) break;
        if (glyphCount > 0) width += spacing;
        const GlyphInfo& glyph = font.glyphs[index];
        width += (glyph.advanceX > 0 ? static_cast<float>(glyph.advanceX)
                                   : font.recs[index].width + glyph.offsetX) * scale;
        ++glyphCount;
        cursor += static_cast<size_t>(byteCount);
    }
    return width;
}

UITextEditLayout BuildUITextEditLayout(
        UIContext& ui, const UIConfig& config, uint32_t widgetId,
        Rectangle bounds, float textWidth, float prefixWidth,
        UITextJustify justify)
{
    const float interiorLeft = std::ceil(bounds.x + config.paddingX);
    const Rectangle interior{
            interiorLeft,
            bounds.y + config.paddingY,
            std::max(0.0f, std::floor(bounds.x + bounds.width - config.paddingX)
                    - interiorLeft),
            std::max(0.0f, bounds.height - config.paddingY * 2.0f)};
    UITextEditLayout layout;
    layout.clipBounds = interior;
    // Existing compact rows can have less vertical padding than the configured
    // value. Clip at the border vertically to preserve the centered glyphs.
    const float border = std::max(0.0f, config.borderThickness);
    layout.clipBounds.y = std::ceil(bounds.y + border);
    layout.clipBounds.height = std::max(0.0f,
            std::floor(bounds.y + bounds.height - border) - layout.clipBounds.y);
    if (ui.inScrollArea) {
        const float left = std::max(interior.x, ui.scrollViewport.x);
        const float top = std::max(layout.clipBounds.y, ui.scrollViewport.y);
        const float right = std::min(interior.x + interior.width,
                ui.scrollViewport.x + ui.scrollViewport.width);
        const float bottom = std::min(layout.clipBounds.y + layout.clipBounds.height,
                ui.scrollViewport.y + ui.scrollViewport.height);
        layout.clipBounds = {left, top, std::max(0.0f, right - left),
                std::max(0.0f, bottom - top)};
    }

    float textX = interior.x;
    if (justify == UITextJustify::Center) {
        textX = bounds.x + (bounds.width - textWidth) * 0.5f;
    } else if (justify == UITextJustify::Right) {
        textX = interior.x + interior.width - textWidth;
    }
    textX = std::round(textX);

    constexpr float caretGap = 2.0f;
    const float caretWidth = std::max(0.0f, config.caretWidth);
    if (ui.focusedId == widgetId) {
        if (ui.textScrollOwnerId != widgetId) {
            ui.textScrollOwnerId = widgetId;
            ui.textScrollOffsetX = 0.0f;
        }
        const float extent = textWidth + caretGap + caretWidth;
        const float maxScroll = std::max(0.0f, extent - interior.width);
        float& offset = ui.textScrollOffsetX;
        offset = std::clamp(offset, 0.0f, maxScroll);
        if (maxScroll > 0.0f) {
            const float caretLeft = prefixWidth + caretGap;
            const float caretRight = caretLeft + caretWidth;
            if (caretLeft < offset) offset = caretLeft;
            if (caretRight > offset + interior.width) {
                offset = caretRight - interior.width;
            }
            // Home always restores the beginning, including its padding.
            if (prefixWidth == 0.0f) offset = 0.0f;
            offset = std::clamp(offset, 0.0f, maxScroll);
            textX = interior.x - offset;
        } else {
            // Keep normal justification, with enough space for the whole caret.
            textX = std::clamp(textX, interior.x,
                    interior.x + interior.width - extent);
        }
    }

    layout.textPosition = {textX,
            std::round(bounds.y + (bounds.height - config.fontSize) * 0.5f)};
    layout.caretBounds = {textX + prefixWidth + caretGap, interior.y,
            caretWidth, interior.height};
    return layout;
}

} // namespace engine
