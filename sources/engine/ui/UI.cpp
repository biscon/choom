#include "engine/ui/UI.h"

#include <raylib.h>

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

namespace engine {

namespace {

static constexpr uint32_t FnvOffsetBasis = 2166136261u;
static constexpr uint32_t FnvPrime = 16777619u;

uint32_t HashId(const char* id)
{
    uint32_t hash = FnvOffsetBasis;
    if (id == nullptr) {
        return hash;
    }

    while (*id != '\0') {
        hash ^= static_cast<uint8_t>(*id);
        hash *= FnvPrime;
        ++id;
    }

    return hash == 0 ? 1u : hash;
}

bool Contains(Rectangle rect, Vector2 point)
{
    return point.x >= rect.x
        && point.x <= rect.x + rect.width
        && point.y >= rect.y
        && point.y <= rect.y + rect.height;
}

bool OpenOptionBlocksPoint(const UIContext& ui, Vector2 point)
{
    return ui.openOptionId != 0
        && ui.openOptionBoundsValid
        && (ui.openOptionBlocksNormalInput || Contains(ui.openOptionDropdownBounds, point));
}

Rectangle TransformBounds(const UIContext& ui, Rectangle bounds)
{
    if (!ui.inScrollArea) {
        return bounds;
    }

    return Rectangle{
            bounds.x + ui.scrollViewport.x - ui.scrollOffset.x,
            bounds.y + ui.scrollViewport.y - ui.scrollOffset.y,
            bounds.width,
            bounds.height
    };
}

bool AllowsWidgetHit(const UIContext& ui, Vector2 point)
{
    if (ui.mainMenuBlocksNormalInput) {
        return false;
    }
    if (OpenOptionBlocksPoint(ui, point)) {
        return false;
    }
    return !ui.inScrollArea || Contains(ui.scrollViewport, point);
}

bool ContainsWidget(const UIContext& ui, Rectangle bounds, Vector2 point)
{
    return AllowsWidgetHit(ui, point) && Contains(TransformBounds(ui, bounds), point);
}

Rectangle IntersectRect(Rectangle a, Rectangle b)
{
    const float x0 = std::max(a.x, b.x);
    const float y0 = std::max(a.y, b.y);
    const float x1 = std::min(a.x + a.width, b.x + b.width);
    const float y1 = std::min(a.y + a.height, b.y + b.height);
    return Rectangle{x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0)};
}

Color ResolveTint(Color requested, Color fallback)
{
    return requested.a == 0 ? fallback : requested;
}

size_t TextByteLength(const char* text)
{
    return text == nullptr ? 0u : std::strlen(text);
}

size_t CountUtf8Characters(const char* text)
{
    if (text == nullptr) {
        return 0;
    }

    size_t count = 0;
    const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);
    while (*cursor != '\0') {
        if ((*cursor & 0xC0u) != 0x80u) {
            ++count;
        }
        ++cursor;
    }

    return count;
}

bool IsUtf8ContinuationByte(char value)
{
    return (static_cast<unsigned char>(value) & 0xC0u) == 0x80u;
}

size_t ClampToUtf8Boundary(const char* text, size_t byteIndex)
{
    const size_t byteCount = TextByteLength(text);
    size_t index = std::min(byteIndex, byteCount);
    while (index > 0 && IsUtf8ContinuationByte(text[index])) {
        --index;
    }
    return index;
}

size_t PreviousUtf8Boundary(const char* text, size_t byteIndex)
{
    size_t index = ClampToUtf8Boundary(text, byteIndex);
    if (index == 0) {
        return 0;
    }

    --index;
    while (index > 0 && IsUtf8ContinuationByte(text[index])) {
        --index;
    }
    return index;
}

size_t NextUtf8Boundary(const char* text, size_t byteIndex)
{
    const size_t byteCount = TextByteLength(text);
    size_t index = ClampToUtf8Boundary(text, byteIndex);
    if (index >= byteCount) {
        return byteCount;
    }

    ++index;
    while (index < byteCount && IsUtf8ContinuationByte(text[index])) {
        ++index;
    }
    return index;
}

size_t Utf8BytesForCodepoint(uint32_t codepoint)
{
    if (codepoint <= 0x7Fu) {
        return 1;
    }
    if (codepoint <= 0x7FFu) {
        return 2;
    }
    if (codepoint <= 0xFFFFu) {
        return 3;
    }
    if (codepoint <= 0x10FFFFu) {
        return 4;
    }

    return 0;
}

bool InsertCodepointAtCursor(
        char* buffer,
        size_t capacity,
        size_t maxCharacters,
        size_t& cursorByteIndex,
        uint32_t codepoint)
{
    if (buffer == nullptr || capacity == 0 || codepoint == '\n' || codepoint == '\r') {
        return false;
    }

    const size_t characterCount = CountUtf8Characters(buffer);
    if (characterCount >= maxCharacters) {
        return false;
    }

    const size_t byteCount = TextByteLength(buffer);
    const size_t bytesNeeded = Utf8BytesForCodepoint(codepoint);
    if (bytesNeeded == 0 || byteCount + bytesNeeded + 1 > capacity) {
        return false;
    }

    cursorByteIndex = ClampToUtf8Boundary(buffer, cursorByteIndex);
    char* out = buffer + cursorByteIndex;
    std::memmove(
            out + bytesNeeded,
            out,
            byteCount - cursorByteIndex + 1
    );

    if (bytesNeeded == 1) {
        out[0] = static_cast<char>(codepoint);
    } else if (bytesNeeded == 2) {
        out[0] = static_cast<char>(0xC0u | ((codepoint >> 6u) & 0x1Fu));
        out[1] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
    } else if (bytesNeeded == 3) {
        out[0] = static_cast<char>(0xE0u | ((codepoint >> 12u) & 0x0Fu));
        out[1] = static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
    } else {
        out[0] = static_cast<char>(0xF0u | ((codepoint >> 18u) & 0x07u));
        out[1] = static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3Fu));
        out[2] = static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3Fu));
        out[3] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
    }

    cursorByteIndex += bytesNeeded;
    return true;
}

bool RemoveUtf8CharacterLeftOfCursor(char* buffer, size_t& cursorByteIndex)
{
    if (buffer == nullptr) {
        return false;
    }

    const size_t byteCount = TextByteLength(buffer);
    cursorByteIndex = ClampToUtf8Boundary(buffer, cursorByteIndex);
    if (byteCount == 0 || cursorByteIndex == 0) {
        return false;
    }

    const size_t deleteStart = PreviousUtf8Boundary(buffer, cursorByteIndex);
    const size_t deleteEnd = cursorByteIndex;

    std::memmove(
            buffer + deleteStart,
            buffer + deleteEnd,
            byteCount - deleteEnd + 1
    );
    cursorByteIndex = deleteStart;
    return true;
}

bool RemoveUtf8CharacterAtCursor(char* buffer, size_t& cursorByteIndex)
{
    if (buffer == nullptr) {
        return false;
    }

    const size_t byteCount = TextByteLength(buffer);
    cursorByteIndex = ClampToUtf8Boundary(buffer, cursorByteIndex);
    if (byteCount == 0 || cursorByteIndex >= byteCount) {
        return false;
    }

    const size_t deleteStart = cursorByteIndex;
    const size_t deleteEnd = NextUtf8Boundary(buffer, cursorByteIndex);
    std::memmove(
            buffer + deleteStart,
            buffer + deleteEnd,
            byteCount - deleteEnd + 1
    );
    return true;
}

Vector2 MeasureTextWithFont(
        AssetManager& assets,
        FontHandle font,
        const char* text,
        float fontSize,
        float spacing)
{
    const FontAsset* fontAsset = assets.GetFont(font);
    if (fontAsset == nullptr) {
        return Vector2{
                static_cast<float>(MeasureText(text == nullptr ? "" : text, static_cast<int>(fontSize))),
                fontSize
        };
    }

    return MeasureTextEx(fontAsset->font, text == nullptr ? "" : text, fontSize, spacing);
}

void DrawTextWithFont(
        AssetManager& assets,
        FontHandle font,
        const char* text,
        Vector2 position,
        float fontSize,
        float spacing,
        Color tint)
{
    const FontAsset* fontAsset = assets.GetFont(font);
    if (fontAsset == nullptr) {
        DrawText(
                text == nullptr ? "" : text,
                static_cast<int>(std::round(position.x)),
                static_cast<int>(std::round(position.y)),
                static_cast<int>(fontSize),
                tint
        );
        return;
    }

    DrawTextEx(fontAsset->font, text == nullptr ? "" : text, position, fontSize, spacing, tint);
}

Vector2 TextPositionRaw(
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify)
{
    const Vector2 measured = MeasureTextWithFont(assets, font, text, config.fontSize, config.textSpacing);
    float x = bounds.x + config.paddingX;
    if (justify == UITextJustify::Center) {
        x = bounds.x + (bounds.width - measured.x) * 0.5f;
    } else if (justify == UITextJustify::Right) {
        x = bounds.x + bounds.width - config.paddingX - measured.x;
    }

    const float y = bounds.y + (bounds.height - config.fontSize) * 0.5f;
    return Vector2{std::round(x), std::round(y)};
}

Vector2 TextPosition(
        const UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify)
{
    return TextPositionRaw(config, assets, TransformBounds(ui, bounds), font, text, justify);
}

void DrawWrappedTextLine(
        AssetManager& assets,
        FontHandle font,
        const char* text,
        size_t start,
        size_t length,
        Vector2 position,
        float fontSize,
        float spacing,
        Color tint)
{
    char line[2048] = {};
    const size_t copyLength = std::min(length, sizeof(line) - 1);
    std::memcpy(line, text + start, copyLength);
    line[copyLength] = '\0';
    DrawTextWithFont(assets, font, line, position, fontSize, spacing, tint);
}

struct WrappedTextMetrics {
    Font font = {};
    float fontSize = 0.0f;
    float spacing = 0.0f;
    float scale = 0.0f;
    bool valid = false;
};

WrappedTextMetrics ResolveWrappedTextMetrics(
        AssetManager& assets,
        FontHandle font,
        float fontSize,
        float spacing)
{
    WrappedTextMetrics metrics;
    if (const FontAsset* fontAsset = assets.GetFont(font)) {
        metrics.font = fontAsset->font;
        metrics.fontSize = fontSize;
        metrics.spacing = spacing;
    } else {
        metrics.font = GetFontDefault();
        const int defaultFontSize = std::max(10, static_cast<int>(fontSize));
        metrics.fontSize = static_cast<float>(defaultFontSize);
        metrics.spacing = static_cast<float>(defaultFontSize / 10);
    }
    metrics.valid = metrics.font.texture.id != 0
            && metrics.font.baseSize > 0
            && metrics.font.glyphs != nullptr
            && metrics.font.recs != nullptr;
    metrics.scale = metrics.valid
            ? metrics.fontSize / static_cast<float>(metrics.font.baseSize)
            : 0.0f;
    return metrics;
}

float MeasureTextRangeWidthLinear(
        const WrappedTextMetrics& metrics,
        const char* text,
        size_t start,
        size_t end)
{
    if (!metrics.valid || text == nullptr || start >= end) {
        return 0.0f;
    }

    float width = 0.0f;
    int glyphCount = 0;
    for (size_t cursor = start; cursor < end;) {
        int byteCount = 0;
        const int codepoint = GetCodepointNext(text + cursor, &byteCount);
        if (byteCount <= 0) {
            byteCount = 1;
        }
        const int glyphIndex = GetGlyphIndex(metrics.font, codepoint);
        if (glyphIndex >= 0 && glyphIndex < metrics.font.glyphCount) {
            if (glyphCount > 0) {
                width += metrics.spacing;
            }
            const GlyphInfo& glyph = metrics.font.glyphs[glyphIndex];
            width += static_cast<float>(
                    glyph.advanceX > 0
                    ? glyph.advanceX
                    : metrics.font.recs[glyphIndex].width + glyph.offsetX)
                    * metrics.scale;
            ++glyphCount;
        }
        cursor = std::min(end, cursor + static_cast<size_t>(byteCount));
    }
    return width;
}

float WrappedTextCodepointAdvance(
        const WrappedTextMetrics& metrics,
        int codepoint,
        bool& outValidGlyph)
{
    outValidGlyph = false;
    if (!metrics.valid) {
        return 0.0f;
    }
    const int glyphIndex = GetGlyphIndex(metrics.font, codepoint);
    if (glyphIndex < 0 || glyphIndex >= metrics.font.glyphCount) {
        return 0.0f;
    }
    outValidGlyph = true;
    const GlyphInfo& glyph = metrics.font.glyphs[glyphIndex];
    return static_cast<float>(
            glyph.advanceX > 0
            ? glyph.advanceX
            : metrics.font.recs[glyphIndex].width + glyph.offsetX)
            * metrics.scale;
}

void DrawTextWrappedRaw(
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify,
        Color tint)
{
    const char* safeText = text == nullptr ? "" : text;
    const size_t byteCount = TextByteLength(safeText);
    const float contentX = bounds.x + config.paddingX;
    const float contentY = bounds.y + config.paddingY;
    const float contentWidth = std::max(0.0f, bounds.width - config.paddingX * 2.0f);
    const float contentBottom = bounds.y + bounds.height - config.paddingY;
    const float lineHeight = config.fontSize;
    const float lineAdvance = config.fontSize + config.textSpacing;
    const Color textColor = ResolveTint(tint, config.textColor);
    const WrappedTextMetrics metrics = ResolveWrappedTextMetrics(
            assets,
            font,
            config.fontSize,
            config.textSpacing);

    if (byteCount == 0 || contentWidth <= 0.0f || lineHeight <= 0.0f) {
        return;
    }

    size_t lineStart = 0;
    size_t lineEnd = 0;
    size_t lastBreak = 0;
    bool hasBreak = false;
    float currentLineWidth = 0.0f;
    int currentLineGlyphCount = 0;
    float y = contentY;

    auto drawLine = [&](size_t start, size_t end) {
        while (start < end && (safeText[start] == ' ' || safeText[start] == '\t')) {
            ++start;
        }
        while (end > start && (safeText[end - 1] == ' ' || safeText[end - 1] == '\t')) {
            --end;
        }
        if (y + lineHeight > contentBottom) {
            return false;
        }
        if (start >= end) {
            y += lineAdvance;
            return true;
        }

        const float lineWidth = MeasureTextRangeWidthLinear(
                metrics,
                safeText,
                start,
                end);
        float x = contentX;
        if (justify == UITextJustify::Center) {
            x = bounds.x + (bounds.width - lineWidth) * 0.5f;
        } else if (justify == UITextJustify::Right) {
            x = bounds.x + bounds.width - config.paddingX - lineWidth;
        }

        DrawWrappedTextLine(
                assets,
                font,
                safeText,
                start,
                end - start,
                Vector2{std::round(x), std::round(y)},
                config.fontSize,
                config.textSpacing,
                textColor
        );
        y += lineAdvance;
        return true;
    };

    for (size_t cursor = 0; cursor < byteCount;) {
        const size_t next = NextUtf8Boundary(safeText, cursor);
        const char ch = safeText[cursor];

        if (ch == '\n') {
            if (!drawLine(lineStart, cursor)) {
                return;
            }
            lineStart = next;
            lineEnd = next;
            lastBreak = next;
            hasBreak = false;
            currentLineWidth = 0.0f;
            currentLineGlyphCount = 0;
            cursor = next;
            continue;
        }

        int codepointByteCount = 0;
        const int codepoint = GetCodepointNext(
                safeText + cursor,
                &codepointByteCount);
        bool validGlyph = false;
        const float glyphAdvance = WrappedTextCodepointAdvance(
                metrics,
                codepoint,
                validGlyph);
        const float measuredWidth = currentLineWidth
                + (validGlyph && currentLineGlyphCount > 0
                        ? metrics.spacing
                        : 0.0f)
                + glyphAdvance;

        const bool lineBufferFull = next - lineStart >= 2048;
        if ((measuredWidth > contentWidth || lineBufferFull)
                && lineEnd > lineStart) {
            const size_t breakEnd = hasBreak && lastBreak > lineStart ? lastBreak : lineEnd;
            if (!drawLine(lineStart, breakEnd)) {
                return;
            }
            lineStart = breakEnd;
            while (lineStart < byteCount && (safeText[lineStart] == ' ' || safeText[lineStart] == '\t')) {
                lineStart = NextUtf8Boundary(safeText, lineStart);
            }
            cursor = lineStart;
            lineEnd = lineStart;
            lastBreak = lineStart;
            hasBreak = false;
            currentLineWidth = 0.0f;
            currentLineGlyphCount = 0;
            continue;
        }

        lineEnd = next;
        currentLineWidth = measuredWidth;
        if (validGlyph) {
            ++currentLineGlyphCount;
        }
        if (ch == ' ' || ch == '\t') {
            lastBreak = next;
            hasBreak = true;
        }
        cursor = next;
    }

    (void)drawLine(lineStart, byteCount);
}

void DrawWidgetBackgroundRaw(const UIConfig& config, Rectangle bounds, Color fill, Color border)
{
    DrawRectangleRec(bounds, fill);
    DrawRectangleLinesEx(bounds, config.borderThickness, border);
}

void DrawWidgetBackground(const UIContext& ui, const UIConfig& config, Rectangle bounds, Color fill, Color border)
{
    DrawWidgetBackgroundRaw(config, TransformBounds(ui, bounds), fill, border);
}

Color InteractiveFill(const UIConfig& config, const UIContext& ui, uint32_t id)
{
    if (ui.activeId == id) {
        return config.widgetActiveColor;
    }
    if (ui.hotId == id) {
        return config.widgetHoverColor;
    }
    return config.widgetColor;
}

Color ToolButtonFill(const UIConfig& config, const UIContext& ui, uint32_t id, bool selected)
{
    if (ui.activeId == id) {
        return config.widgetActiveColor;
    }
    if (ui.hotId == id) {
        return config.widgetHoverColor;
    }
    return selected ? config.selectedToolColor : config.widgetColor;
}

bool ConsumeMouseClick(const UIContext& ui, Input& input, Rectangle bounds)
{
    bool clicked = false;
    input.ForEachEvent(
            InputEventType::MouseClick,
            true,
            [&ui, bounds, &clicked](InputEvent& event) {
                if (event.mouseClick.button != MOUSE_LEFT_BUTTON
                        || !ContainsWidget(ui, bounds, event.mouseClick.releasePosition)) {
                    return;
                }

                clicked = true;
                ConsumeEvent(event);
            }
    );
    return clicked;
}

void ConsumeFocusedPointerEvents(Input& input)
{
    input.ForEachEvent(
            InputEventType::MouseButtonPressed,
            true,
            [](InputEvent& event) {
                if (event.mouseButton.button == MOUSE_LEFT_BUTTON) {
                    ConsumeEvent(event);
                }
            }
    );
    input.ForEachEvent(
            InputEventType::MouseClick,
            true,
            [](InputEvent& event) {
                if (event.mouseClick.button == MOUSE_LEFT_BUTTON) {
                    ConsumeEvent(event);
                }
            }
    );
}

void ConsumeMousePresses(const UIContext& ui, Input& input, Rectangle bounds)
{
    input.ForEachEvent(
            InputEventType::MouseButtonPressed,
            true,
            [&ui, bounds](InputEvent& event) {
                if (event.mouseButton.button == MOUSE_LEFT_BUTTON
                        && ContainsWidget(ui, bounds, event.mouseButton.position)) {
                    ConsumeEvent(event);
                }
            }
    );
}

Rectangle ListItemBounds(Rectangle bounds, float itemHeight, size_t index)
{
    return Rectangle{
            bounds.x,
            bounds.y + static_cast<float>(index) * itemHeight,
            bounds.width,
            itemHeight
    };
}

void DrawSelectableRow(
        const UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        Rectangle row,
        FontHandle font,
        const char* text,
        bool hovered,
        bool selected)
{
    Color fill = config.widgetColor;
    if (selected) {
        fill = config.accentColor;
    } else if (hovered) {
        fill = config.widgetHoverColor;
    }

    const Rectangle drawRow = TransformBounds(ui, row);
    DrawRectangleRec(drawRow, fill);
    Text(ui, config, assets, row, font, text == nullptr ? "" : text, UITextJustify::Left, config.textColor);
}

void DrawSelectablePanelBackground(const UIContext& ui, const UIConfig& config, Rectangle bounds)
{
    DrawRectangleRec(TransformBounds(ui, bounds), config.panelColor);
}

void DrawSelectablePanelLines(const UIContext& ui, const UIConfig& config, Rectangle bounds, size_t itemCount)
{
    const Rectangle drawBounds = TransformBounds(ui, bounds);
    for (size_t i = 1; i < itemCount; ++i) {
        const float y = drawBounds.y + config.listItemHeight * static_cast<float>(i);
        DrawLineEx(
                Vector2{drawBounds.x, y},
                Vector2{drawBounds.x + drawBounds.width, y},
                config.borderThickness,
                config.borderColor
        );
    }
    DrawRectangleLinesEx(drawBounds, config.borderThickness, config.borderColor);
}

Rectangle ResolveOverlayBounds(const UIConfig& config)
{
    if (config.overlayBounds.width > 0.0f && config.overlayBounds.height > 0.0f) {
        return config.overlayBounds;
    }

    return Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(GetScreenWidth()),
            static_cast<float>(GetScreenHeight())
    };
}

struct OptionDropdownLayout {
    Rectangle bounds = {};
    size_t visibleOptionCount = 0;
};

OptionDropdownLayout BuildOptionDropdownLayout(
        const UIConfig& config,
        Rectangle fieldBounds,
        size_t optionCount)
{
    OptionDropdownLayout layout;
    if (optionCount == 0 || config.listItemHeight <= 0.0f) {
        return layout;
    }

    const Rectangle visibleBounds = ResolveOverlayBounds(config);
    const float rowHeight = config.listItemHeight;
    const float fullHeight = rowHeight * static_cast<float>(optionCount);
    const float fieldBottom = fieldBounds.y + fieldBounds.height;
    const float visibleBottom = visibleBounds.y + visibleBounds.height;
    const float belowSpace = std::max(0.0f, visibleBottom - fieldBottom);
    const float aboveSpace = std::max(0.0f, fieldBounds.y - visibleBounds.y);

    bool openBelow = true;
    float availableHeight = belowSpace;
    if (fullHeight <= belowSpace) {
        openBelow = true;
        availableHeight = belowSpace;
    } else if (fullHeight <= aboveSpace) {
        openBelow = false;
        availableHeight = aboveSpace;
    } else if (aboveSpace > belowSpace) {
        openBelow = false;
        availableHeight = aboveSpace;
    }

    const size_t fitRows = static_cast<size_t>(std::floor(availableHeight / rowHeight));
    const size_t visibleRows = std::min(optionCount, fitRows);
    if (visibleRows == 0) {
        return layout;
    }

    float width = fieldBounds.width;
    if (visibleBounds.width > 0.0f) {
        width = std::min(width, visibleBounds.width);
    }

    float x = fieldBounds.x;
    if (visibleBounds.width > width) {
        x = std::clamp(x, visibleBounds.x, visibleBounds.x + visibleBounds.width - width);
    } else {
        x = visibleBounds.x;
    }

    const float height = rowHeight * static_cast<float>(visibleRows);
    float y = openBelow ? fieldBottom : fieldBounds.y - height;
    if (visibleBounds.height > height) {
        y = std::clamp(y, visibleBounds.y, visibleBottom - height);
    } else {
        y = visibleBounds.y;
    }

    layout.bounds = Rectangle{x, y, width, height};
    layout.visibleOptionCount = visibleRows;
    return layout;
}

size_t ClampOptionFirstVisibleIndex(size_t firstVisibleIndex, size_t optionCount, size_t visibleOptionCount)
{
    if (visibleOptionCount == 0 || optionCount <= visibleOptionCount) {
        return 0;
    }

    return std::min(firstVisibleIndex, optionCount - visibleOptionCount);
}

size_t InitialOptionFirstVisibleIndex(int selectedIndex, size_t optionCount, size_t visibleOptionCount)
{
    if (visibleOptionCount == 0 || optionCount <= visibleOptionCount || selectedIndex < 0) {
        return 0;
    }

    const size_t selected = std::min(static_cast<size_t>(selectedIndex), optionCount - 1u);
    if (selected < visibleOptionCount) {
        return 0;
    }

    return ClampOptionFirstVisibleIndex(selected, optionCount, visibleOptionCount);
}

void ClearOpenOption(UIContext& ui)
{
    ui.openOptionId = 0;
    ui.openOptionBoundsValid = false;
    ui.openOptionFieldBounds = {};
    ui.openOptionDropdownBounds = {};
    ui.openOptionFirstVisibleIndex = 0;
    ui.openOptionVisibleCount = 0;
    ui.openOptionBlocksNormalInput = false;
    ui.openOptionClickPending = false;
    ui.openOptionClickPosition = {};
    ui.openOptionCloseRequested = false;
    ui.openOptionWheelDelta = 0.0f;
}

bool IsFloatEditCharacter(uint32_t codepoint)
{
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '-' || codepoint == '.';
}

bool IsIntEditCharacter(uint32_t codepoint)
{
    return (codepoint >= '0' && codepoint <= '9') || codepoint == '-';
}

bool ParseFloatStrict(const char* text, float& value)
{
    if (text == nullptr || text[0] == '\0') {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const float parsed = std::strtof(text, &end);
    if (end == text || end == nullptr || *end != '\0' || errno == ERANGE || !std::isfinite(parsed)) {
        return false;
    }

    value = parsed;
    return true;
}

bool ParseIntStrict(const char* text, int& value)
{
    if (text == nullptr || text[0] == '\0') {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (end == text || end == nullptr || *end != '\0' || errno == ERANGE) {
        return false;
    }
    if (parsed < static_cast<long>(INT32_MIN) || parsed > static_cast<long>(INT32_MAX)) {
        return false;
    }

    value = static_cast<int>(parsed);
    return true;
}

void FormatFloatValue(char* buffer, size_t capacity, float value, int decimalPlaces)
{
    if (buffer == nullptr || capacity == 0) {
        return;
    }

    const int places = std::clamp(decimalPlaces, 0, 8);
    std::snprintf(buffer, capacity, "%.*f", places, value);
}

void FormatIntValue(char* buffer, size_t capacity, int value)
{
    if (buffer == nullptr || capacity == 0) {
        return;
    }

    std::snprintf(buffer, capacity, "%d", value);
}

template <typename CharacterFilter>
bool EditAsciiBuffer(
        UIContext& ui,
        Input& input,
        char* buffer,
        size_t bufferCapacity,
        CharacterFilter allowCharacter,
        bool& submitted,
        bool& cancelled)
{
    bool changed = false;
    bool insertedSpaceFromTextEvent = false;

    input.ForEachEvent(
            InputEventType::TextInput,
            true,
            [&ui, buffer, bufferCapacity, allowCharacter, &changed](InputEvent& event) {
                if (allowCharacter(event.text.codepoint)
                        && InsertCodepointAtCursor(
                                buffer,
                                bufferCapacity,
                                bufferCapacity > 0 ? bufferCapacity - 1 : 0,
                                ui.textCursorByteIndex,
                                event.text.codepoint)) {
                    changed = true;
                    ui.textCursorBlinkStartTime = GetTime();
                }
                ConsumeEvent(event);
            }
    );

    auto handleKey = [&ui, buffer, &changed, &submitted, &cancelled, &insertedSpaceFromTextEvent](InputEvent& event) {
        if (event.key.key == KEY_LEFT) {
            const size_t previous = ui.textCursorByteIndex;
            ui.textCursorByteIndex = PreviousUtf8Boundary(buffer, ui.textCursorByteIndex);
            if (ui.textCursorByteIndex != previous) {
                ui.textCursorBlinkStartTime = GetTime();
            }
            ConsumeEvent(event);
        } else if (event.key.key == KEY_RIGHT) {
            const size_t previous = ui.textCursorByteIndex;
            ui.textCursorByteIndex = NextUtf8Boundary(buffer, ui.textCursorByteIndex);
            if (ui.textCursorByteIndex != previous) {
                ui.textCursorBlinkStartTime = GetTime();
            }
            ConsumeEvent(event);
        } else if (event.key.key == KEY_HOME) {
            if (ui.textCursorByteIndex != 0) {
                ui.textCursorByteIndex = 0;
                ui.textCursorBlinkStartTime = GetTime();
            }
            ConsumeEvent(event);
        } else if (event.key.key == KEY_END) {
            const size_t end = TextByteLength(buffer);
            if (ui.textCursorByteIndex != end) {
                ui.textCursorByteIndex = end;
                ui.textCursorBlinkStartTime = GetTime();
            }
            ConsumeEvent(event);
        } else if (event.key.key == KEY_BACKSPACE) {
            if (RemoveUtf8CharacterLeftOfCursor(buffer, ui.textCursorByteIndex)) {
                changed = true;
                ui.textCursorBlinkStartTime = GetTime();
            }
            ConsumeEvent(event);
        } else if (event.key.key == KEY_DELETE) {
            if (RemoveUtf8CharacterAtCursor(buffer, ui.textCursorByteIndex)) {
                changed = true;
                ui.textCursorBlinkStartTime = GetTime();
            }
            ConsumeEvent(event);
        } else if (event.key.key == KEY_SPACE) {
            insertedSpaceFromTextEvent = true;
            ConsumeEvent(event);
        } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
            submitted = true;
            ConsumeEvent(event);
        } else if (event.key.key == KEY_ESCAPE) {
            cancelled = true;
            ConsumeEvent(event);
        }
    };

    input.ForEachEvent(InputEventType::KeyPressed, true, handleKey);
    input.ForEachEvent(InputEventType::KeyRepeated, true, handleKey);
    (void)insertedSpaceFromTextEvent;

    return changed;
}

void DrawTextEditField(
        UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        uint32_t widgetId,
        bool valid,
        UITextJustify justify)
{
    const Color border = valid ? config.borderColor : config.invalidColor;
    const Color textColor = valid ? config.textColor : config.invalidColor;
    DrawWidgetBackground(ui, config, bounds, InteractiveFill(config, ui, widgetId), border);
    Text(ui, config, assets, bounds, font, text == nullptr ? "" : text, justify, textColor);

    if (ui.focusedId != widgetId || text == nullptr) {
        return;
    }

    ui.textCursorByteIndex = ClampToUtf8Boundary(text, ui.textCursorByteIndex);

    const double elapsed = GetTime() - ui.textCursorBlinkStartTime;
    const double blinkInterval = std::max(0.01f, config.caretBlinkInterval);
    const bool caretVisible = (static_cast<int>(elapsed / blinkInterval) % 2) == 0;
    if (!caretVisible) {
        return;
    }

    char prefix[128] = {};
    const size_t prefixBytes = std::min(ui.textCursorByteIndex, sizeof(prefix) - 1);
    std::memcpy(prefix, text, prefixBytes);
    prefix[prefixBytes] = '\0';

    const Vector2 textPos = TextPosition(ui, config, assets, bounds, font, text, justify);
    const Vector2 prefixSize = MeasureTextWithFont(
            assets,
            font,
            prefix,
            config.fontSize,
            config.textSpacing
    );

    const Rectangle drawBounds = TransformBounds(ui, bounds);
    const float caretX = std::min(drawBounds.x + drawBounds.width - config.paddingX, textPos.x + prefixSize.x + 2.0f);
    const Rectangle caret{caretX, drawBounds.y + config.paddingY, config.caretWidth, drawBounds.height - config.paddingY * 2.0f};
    DrawRectangleRec(caret, textColor);
}

bool StepButton(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* text)
{
    return Button(ui, config, input, assets, id, bounds, font, text, UITextJustify::Center);
}

float ClampScrollOffset(float value, float contentExtent, float viewportExtent)
{
    return std::clamp(value, 0.0f, std::max(0.0f, contentExtent - viewportExtent));
}

Rectangle ScrollClientBounds(const UIConfig& config, Rectangle bounds)
{
    const float inset = std::max(0.0f, config.borderThickness);
    return Rectangle{
            bounds.x + inset,
            bounds.y + inset,
            std::max(0.0f, bounds.width - inset * 2.0f),
            std::max(0.0f, bounds.height - inset * 2.0f)
    };
}

Rectangle VerticalScrollTrack(const UIConfig& config, const UIScrollAreaResult& area)
{
    const Rectangle client = area.drawFrame ? ScrollClientBounds(config, area.bounds) : area.bounds;
    return Rectangle{
            client.x + std::max(0.0f, client.width - config.scrollbarSize),
            area.viewport.y,
            config.scrollbarSize,
            area.viewport.height
    };
}

Rectangle HorizontalScrollTrack(const UIConfig& config, const UIScrollAreaResult& area)
{
    const Rectangle client = area.drawFrame ? ScrollClientBounds(config, area.bounds) : area.bounds;
    return Rectangle{
            area.viewport.x,
            client.y + std::max(0.0f, client.height - config.scrollbarSize),
            area.viewport.width,
            config.scrollbarSize
    };
}

Rectangle ScrollThumb(float offset, float contentExtent, float viewportExtent, Rectangle track, bool vertical, float minThumbSize)
{
    if (contentExtent <= viewportExtent || viewportExtent <= 0.0f) {
        return track;
    }

    const float trackExtent = vertical ? track.height : track.width;
    const float thumbExtent = std::clamp(
            trackExtent * (viewportExtent / contentExtent),
            std::min(trackExtent, minThumbSize),
            trackExtent
    );
    const float maxScroll = std::max(1.0f, contentExtent - viewportExtent);
    const float travel = std::max(0.0f, trackExtent - thumbExtent);
    const float thumbOffset = travel * (offset / maxScroll);

    if (vertical) {
        return Rectangle{track.x, track.y + thumbOffset, track.width, thumbExtent};
    }

    return Rectangle{track.x + thumbOffset, track.y, thumbExtent, track.height};
}

uint32_t ScrollAxisId(uint32_t baseId, int axis)
{
    return baseId ^ (axis == 1 ? 0x9E3779B9u : 0x85EBCA6Bu);
}

} // namespace

void BeginUI(UIContext& ui, Input& input)
{
    ui.hotId = 0;
    ui.mousePosition = input.MousePosition();
    ui.mouseDown = input.IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    ui.optionOverlay = UIContext::OptionOverlay{};
    ui.mainMenuOverlay = UIContext::MainMenuOverlay{};
    ui.mainMenuBlocksNormalInput = false;
    ui.openOptionIssuedThisFrame = false;
    ui.openOptionClickPending = false;
    ui.openOptionClickPosition = {};
    ui.openOptionCloseRequested = false;
    ui.openOptionWheelDelta = 0.0f;
    ui.openOptionBlocksNormalInput = false;

    if (ui.openOptionId != 0 && ui.openOptionBoundsValid) {
        const Rectangle fieldBounds = ui.openOptionFieldBounds;
        const Rectangle dropdownBounds = ui.openOptionDropdownBounds;
        ui.openOptionBlocksNormalInput = !Contains(fieldBounds, ui.mousePosition);

        input.ForEachEvent(
                InputEventType::MouseWheel,
                true,
                [&ui, dropdownBounds](InputEvent& event) {
                    if (Contains(dropdownBounds, ui.mousePosition)) {
                        ui.openOptionWheelDelta += event.wheel.value;
                        ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                InputEventType::MouseButtonPressed,
                true,
                [fieldBounds, dropdownBounds](InputEvent& event) {
                    if (event.mouseButton.button != MOUSE_LEFT_BUTTON) {
                        return;
                    }

                    const Vector2 position = event.mouseButton.position;
                    if (Contains(dropdownBounds, position)
                            || (!Contains(fieldBounds, position) && !Contains(dropdownBounds, position))) {
                        ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                InputEventType::MouseClick,
                true,
                [&ui, fieldBounds, dropdownBounds](InputEvent& event) {
                    if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                        return;
                    }

                    const Vector2 position = event.mouseClick.releasePosition;
                    if (Contains(dropdownBounds, position)) {
                        ui.openOptionClickPending = true;
                        ui.openOptionClickPosition = position;
                        ConsumeEvent(event);
                    } else if (!Contains(fieldBounds, position)) {
                        ui.openOptionCloseRequested = true;
                        ConsumeEvent(event);
                    }
                }
        );
    }
}

void EndUI(UIContext& ui, Input&)
{
    if (ui.openOptionId != 0 && !ui.openOptionIssuedThisFrame) {
        ClearOpenOption(ui);
    }

    if (!ui.mouseDown) {
        ui.activeId = 0;
    }
}

void EndUI(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets)
{
    (void)input;

    if (ui.optionOverlay.active && !ui.optionOverlay.options.empty()) {
        UIContext::OptionOverlay& overlay = ui.optionOverlay;

        DrawSelectablePanelBackground(ui, config, overlay.dropdownBounds);

        const size_t firstVisibleIndex = std::min(overlay.firstVisibleIndex, overlay.options.size());
        const size_t visibleCount = overlay.visibleOptionCount == 0
                ? overlay.options.size() - firstVisibleIndex
                : std::min(overlay.visibleOptionCount, overlay.options.size() - firstVisibleIndex);
        for (size_t i = 0; i < visibleCount; ++i) {
            const size_t optionIndex = firstVisibleIndex + i;
            const Rectangle row = ListItemBounds(
                    overlay.dropdownBounds,
                    config.listItemHeight,
                    i
            );
            const bool hovered = Contains(row, ui.mousePosition);
            const bool selected = overlay.selectedIndexSnapshot == static_cast<int>(optionIndex);
            DrawSelectableRow(
                    ui,
                    config,
                    assets,
                    row,
                    overlay.font,
                    overlay.options[optionIndex].c_str(),
                    hovered,
                    selected
            );
        }

        DrawSelectablePanelLines(ui, config, overlay.dropdownBounds, visibleCount);
    }

    if (ui.mainMenuOverlay.active) {
        const UIContext::MainMenuOverlay& overlay = ui.mainMenuOverlay;
        for (size_t popupIndex = 0; popupIndex < overlay.popupCount; ++popupIndex) {
            const UIContext::MainMenuOverlayPopup& popup = overlay.popups[popupIndex];
            DrawRectangleRec(popup.bounds, config.panelColor);
            for (size_t rowOffset = 0; rowOffset < popup.rowCount; ++rowOffset) {
                const UIContext::MainMenuOverlayRow& row =
                        overlay.rows[popup.firstRow + rowOffset];
                if (row.kind == UIMenuItemKind::Separator) {
                    const float lineY = row.bounds.y + row.bounds.height * 0.5f;
                    DrawLineEx(
                            Vector2{row.bounds.x + config.paddingX, lineY},
                            Vector2{row.bounds.x + row.bounds.width - config.paddingX, lineY},
                            config.borderThickness,
                            config.borderColor);
                    continue;
                }

                const Color fill = row.hovered && row.enabled
                        ? config.widgetHoverColor : config.widgetColor;
                DrawRectangleRec(row.bounds, fill);
                Rectangle textBounds = row.bounds;
                textBounds.x += 28.0f;
                textBounds.width = std::max(0.0f, textBounds.width - 56.0f);
                Text(
                        config,
                        assets,
                        textBounds,
                        overlay.font,
                        row.label == nullptr ? "" : row.label,
                        UITextJustify::Left,
                        row.enabled ? config.textColor : config.disabledColor);
                if (row.kind == UIMenuItemKind::Checkbox && row.checked) {
                    const float x = row.bounds.x + 10.0f;
                    const float y = row.bounds.y + row.bounds.height * 0.5f;
                    const Color tint = row.enabled
                            ? config.accentColor : config.disabledColor;
                    DrawLineEx(Vector2{x, y}, Vector2{x + 5.0f, y + 5.0f}, 3.0f, tint);
                    DrawLineEx(Vector2{x + 5.0f, y + 5.0f}, Vector2{x + 14.0f, y - 6.0f}, 3.0f, tint);
                }
                if (row.kind == UIMenuItemKind::Submenu) {
                    const float x = row.bounds.x + row.bounds.width - 14.0f;
                    const float y = row.bounds.y + row.bounds.height * 0.5f;
                    const Color tint = row.enabled
                            ? config.textColor : config.disabledColor;
                    DrawTriangle(
                            Vector2{x - 4.0f, y - 6.0f},
                            Vector2{x - 4.0f, y + 6.0f},
                            Vector2{x + 4.0f, y},
                            tint);
                }
            }
            DrawRectangleLinesEx(
                    popup.bounds,
                    config.borderThickness,
                    config.borderColor);
        }
    }

    EndUI(ui, input);
}

UIScrollAreaResult BeginScrollArea(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        const char* id,
        Rectangle bounds,
        Vector2 contentSize,
        UIScrollState& state,
        bool drawFrame,
        float paddingPx)
{
    assert(!ui.inScrollArea && "Nested scroll areas are not supported.");
    UIScrollAreaResult result;
    result.bounds = bounds;
    result.contentSize = contentSize;
    result.drawFrame = drawFrame;
    result.paddingPx = std::max(0.0f, paddingPx);

    if (ui.inScrollArea) {
        return result;
    }

    const uint32_t scrollId = HashId(id);

    const Rectangle client = drawFrame ? ScrollClientBounds(config, bounds) : bounds;
    const auto buildViewport = [client, padding = result.paddingPx, scrollbarSize = config.scrollbarSize](bool scrollX, bool scrollY) {
        const float rightInset = scrollY ? padding + scrollbarSize : padding;
        const float bottomInset = scrollX ? padding + scrollbarSize : padding;
        return Rectangle{
                client.x + padding,
                client.y + padding,
                std::max(0.0f, client.width - padding - rightInset),
                std::max(0.0f, client.height - padding - bottomInset)
        };
    };

    Rectangle viewport = buildViewport(false, false);
    bool scrollX = contentSize.x > viewport.width;
    bool scrollY = contentSize.y > viewport.height;
    viewport = buildViewport(scrollX, scrollY);
    if (!scrollX && contentSize.x > viewport.width) {
        scrollX = true;
        viewport = buildViewport(scrollX, scrollY);
    }
    if (!scrollY && contentSize.y > viewport.height) {
        scrollY = true;
        viewport = buildViewport(scrollX, scrollY);
    }

    result.viewport = viewport;
    result.scrollX = scrollX;
    result.scrollY = scrollY;

    state.offset.x = ClampScrollOffset(state.offset.x, contentSize.x, viewport.width);
    state.offset.y = ClampScrollOffset(state.offset.y, contentSize.y, viewport.height);

    const Rectangle verticalTrack = VerticalScrollTrack(config, result);
    const Rectangle horizontalTrack = HorizontalScrollTrack(config, result);
    const Rectangle verticalThumb = ScrollThumb(
            state.offset.y,
            contentSize.y,
            viewport.height,
            verticalTrack,
            true,
            config.scrollbarMinThumbSize
    );
    const Rectangle horizontalThumb = ScrollThumb(
            state.offset.x,
            contentSize.x,
            viewport.width,
            horizontalTrack,
            false,
            config.scrollbarMinThumbSize
    );

    if (scrollY && Contains(bounds, ui.mousePosition)) {
        input.ForEachEvent(
                InputEventType::MouseWheel,
                true,
                [&state, contentSize, viewport, &config](InputEvent& event) {
                    state.offset.y = ClampScrollOffset(
                            state.offset.y - event.wheel.value * config.mouseWheelScrollAmount,
                            contentSize.y,
                            viewport.height
                    );
                    ConsumeEvent(event);
                }
        );
    }

    if (ui.activeScrollId == scrollId && ui.scrollDragAxis != 0 && ui.mouseDown) {
        if (ui.scrollDragAxis == 1 && scrollY) {
            const float thumbTravel = std::max(1.0f, verticalTrack.height - verticalThumb.height);
            const float scrollTravel = std::max(0.0f, contentSize.y - viewport.height);
            const float mouseDelta = ui.mousePosition.y - ui.scrollDragMouseStart.y;
            state.offset.y = ClampScrollOffset(
                    ui.scrollDragOffsetStart.y + (mouseDelta / thumbTravel) * scrollTravel,
                    contentSize.y,
                    viewport.height
            );
        } else if (ui.scrollDragAxis == 2 && scrollX) {
            const float thumbTravel = std::max(1.0f, horizontalTrack.width - horizontalThumb.width);
            const float scrollTravel = std::max(0.0f, contentSize.x - viewport.width);
            const float mouseDelta = ui.mousePosition.x - ui.scrollDragMouseStart.x;
            state.offset.x = ClampScrollOffset(
                    ui.scrollDragOffsetStart.x + (mouseDelta / thumbTravel) * scrollTravel,
                    contentSize.x,
                    viewport.width
            );
        }
    }

    input.ForEachEvent(
            InputEventType::MouseButtonPressed,
            true,
            [&ui, scrollId, scrollX, scrollY, verticalTrack, verticalThumb, horizontalTrack, horizontalThumb, viewport, contentSize, &state](InputEvent& event) {
                if (event.mouseButton.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                const Vector2 position = event.mouseButton.position;
                if (scrollY && Contains(verticalThumb, position)) {
                    ui.activeScrollId = scrollId;
                    ui.scrollDragAxis = 1;
                    ui.activeId = ScrollAxisId(scrollId, 1);
                    ui.scrollDragMouseStart = position;
                    ui.scrollDragOffsetStart = state.offset;
                    ConsumeEvent(event);
                } else if (scrollX && Contains(horizontalThumb, position)) {
                    ui.activeScrollId = scrollId;
                    ui.scrollDragAxis = 2;
                    ui.activeId = ScrollAxisId(scrollId, 2);
                    ui.scrollDragMouseStart = position;
                    ui.scrollDragOffsetStart = state.offset;
                    ConsumeEvent(event);
                } else if (scrollY && Contains(verticalTrack, position)) {
                    const float direction = position.y < verticalThumb.y ? -1.0f : 1.0f;
                    state.offset.y = ClampScrollOffset(
                            state.offset.y + direction * viewport.height,
                            contentSize.y,
                            viewport.height
                    );
                    ConsumeEvent(event);
                } else if (scrollX && Contains(horizontalTrack, position)) {
                    const float direction = position.x < horizontalThumb.x ? -1.0f : 1.0f;
                    state.offset.x = ClampScrollOffset(
                            state.offset.x + direction * viewport.width,
                            contentSize.x,
                            viewport.width
                    );
                    ConsumeEvent(event);
                }
            }
    );

    input.ForEachEvent(
            InputEventType::MouseClick,
            true,
            [scrollX, scrollY, verticalTrack, horizontalTrack](InputEvent& event) {
                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }
                const Vector2 position = event.mouseClick.releasePosition;
                if ((scrollY && Contains(verticalTrack, position))
                        || (scrollX && Contains(horizontalTrack, position))) {
                    ConsumeEvent(event);
                }
            }
    );

    if (drawFrame) {
        DrawWidgetBackgroundRaw(config, bounds, config.panelColor, config.borderColor);
    }

    ui.inScrollArea = true;
    ui.scrollAreaId = scrollId;
    ui.scrollViewport = viewport;
    ui.scrollOffset = state.offset;

    BeginScissorMode(
            static_cast<int>(std::round(viewport.x)),
            static_cast<int>(std::round(viewport.y)),
            static_cast<int>(std::round(viewport.width)),
            static_cast<int>(std::round(viewport.height))
    );

    return result;
}

void EndScrollArea(
        UIContext& ui,
        const UIConfig& config,
        Input&,
        const UIScrollAreaResult& scrollArea,
        UIScrollState& state)
{
    if (!ui.inScrollArea || ui.scrollAreaId == 0) {
        return;
    }

    EndScissorMode();
    ui.inScrollArea = false;
    ui.scrollAreaId = 0;
    ui.scrollViewport = {};
    ui.scrollOffset = {};

    state.offset.x = ClampScrollOffset(state.offset.x, scrollArea.contentSize.x, scrollArea.viewport.width);
    state.offset.y = ClampScrollOffset(state.offset.y, scrollArea.contentSize.y, scrollArea.viewport.height);

    if (scrollArea.drawFrame) {
        DrawRectangleLinesEx(scrollArea.bounds, config.borderThickness, config.borderColor);
    }

    if (scrollArea.scrollY) {
        const Rectangle track = VerticalScrollTrack(config, scrollArea);
        const Rectangle thumb = ScrollThumb(
                state.offset.y,
                scrollArea.contentSize.y,
                scrollArea.viewport.height,
                track,
                true,
                config.scrollbarMinThumbSize
        );
        DrawRectangleRec(track, config.widgetColor);
        DrawRectangleRec(thumb, config.accentColor);
    }

    if (scrollArea.scrollX) {
        const Rectangle track = HorizontalScrollTrack(config, scrollArea);
        const Rectangle thumb = ScrollThumb(
                state.offset.x,
                scrollArea.contentSize.x,
                scrollArea.viewport.width,
                track,
                false,
                config.scrollbarMinThumbSize
        );
        DrawRectangleRec(track, config.widgetColor);
        DrawRectangleRec(thumb, config.accentColor);
    }

    if (scrollArea.scrollX && scrollArea.scrollY) {
        DrawRectangleRec(
                Rectangle{
                        scrollArea.viewport.x + scrollArea.viewport.width,
                        scrollArea.viewport.y + scrollArea.viewport.height,
                        scrollArea.paddingPx + config.scrollbarSize,
                        scrollArea.paddingPx + config.scrollbarSize
                },
                config.panelColor
        );
    }

    if (!ui.mouseDown) {
        ui.activeScrollId = 0;
        ui.scrollDragAxis = 0;
    }
}

void Text(
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify,
        Color tint,
        bool wordWrap)
{
    if (wordWrap) {
        DrawTextWrappedRaw(config, assets, bounds, font, text, justify, tint);
        return;
    }

    DrawTextWithFont(
            assets,
            font,
            text,
            TextPositionRaw(config, assets, bounds, font, text, justify),
            config.fontSize,
            config.textSpacing,
            ResolveTint(tint, config.textColor)
    );
}

void Text(
        const UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify,
        Color tint,
        bool wordWrap)
{
    if (wordWrap) {
        DrawTextWrappedRaw(config, assets, TransformBounds(ui, bounds), font, text, justify, tint);
        return;
    }

    DrawTextWithFont(
            assets,
            font,
            text,
            TextPosition(ui, config, assets, bounds, font, text, justify),
            config.fontSize,
            config.textSpacing,
            ResolveTint(tint, config.textColor)
    );
}

UITextInputResult TextInput(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        char* buffer,
        size_t bufferCapacity,
        size_t minCharacters,
        size_t maxCharacters,
        UITextJustify justify)
{
    const uint32_t widgetId = HashId(id);
    const bool focusedBeforePointerInput = ui.focusedId == widgetId;
    const bool hovered = ContainsWidget(ui, bounds, ui.mousePosition);
    if (hovered) {
        ui.hotId = widgetId;
        ConsumeMousePresses(ui, input, bounds);
    }

    const bool clicked = ConsumeMouseClick(ui, input, bounds);
    if (clicked) {
        ui.focusedId = widgetId;
        ui.activeId = widgetId;
        ui.textCursorByteIndex = TextByteLength(buffer);
        ui.textCursorBlinkStartTime = GetTime();
    }

    bool clickedOutside = false;
    input.ForEachEvent(
            InputEventType::MouseClick,
            true,
            [&ui, bounds, &clickedOutside](InputEvent& event) {
                if (event.mouseClick.button == MOUSE_LEFT_BUTTON
                        && !ContainsWidget(ui, bounds, event.mouseClick.releasePosition)) {
                    clickedOutside = true;
                }
            }
    );
    if (clickedOutside && ui.focusedId == widgetId) {
        ui.focusedId = 0;
    }

    UITextInputResult result;
    result.focusLost = clickedOutside && focusedBeforePointerInput;
    size_t characterCount = CountUtf8Characters(buffer);
    result.valid = characterCount >= minCharacters && characterCount <= maxCharacters;

    if (ui.focusedId == widgetId && buffer != nullptr && bufferCapacity > 0) {
        ui.textCursorByteIndex = ClampToUtf8Boundary(buffer, ui.textCursorByteIndex);

        bool insertedSpaceFromTextEvent = false;
        input.ForEachEvent(
                InputEventType::TextInput,
                true,
                [&ui, buffer, bufferCapacity, maxCharacters, &result, &insertedSpaceFromTextEvent](InputEvent& event) {
                    if (InsertCodepointAtCursor(
                            buffer,
                            bufferCapacity,
                            maxCharacters,
                            ui.textCursorByteIndex,
                            event.text.codepoint)) {
                        result.changed = true;
                        ui.textCursorBlinkStartTime = GetTime();
                        if (event.text.codepoint == ' ') {
                            insertedSpaceFromTextEvent = true;
                        }
                    }
                    ConsumeEvent(event);
                }
        );

        auto handleKey = [&ui, widgetId, buffer, bufferCapacity, maxCharacters, &result, &insertedSpaceFromTextEvent](InputEvent& event) {
            if (event.key.key == KEY_LEFT) {
                const size_t previous = ui.textCursorByteIndex;
                ui.textCursorByteIndex = PreviousUtf8Boundary(buffer, ui.textCursorByteIndex);
                if (ui.textCursorByteIndex != previous) {
                    ui.textCursorBlinkStartTime = GetTime();
                }
                ConsumeEvent(event);
            } else if (event.key.key == KEY_RIGHT) {
                const size_t previous = ui.textCursorByteIndex;
                ui.textCursorByteIndex = NextUtf8Boundary(buffer, ui.textCursorByteIndex);
                if (ui.textCursorByteIndex != previous) {
                    ui.textCursorBlinkStartTime = GetTime();
                }
                ConsumeEvent(event);
            } else if (event.key.key == KEY_HOME) {
                if (ui.textCursorByteIndex != 0) {
                    ui.textCursorByteIndex = 0;
                    ui.textCursorBlinkStartTime = GetTime();
                }
                ConsumeEvent(event);
            } else if (event.key.key == KEY_END) {
                const size_t end = TextByteLength(buffer);
                if (ui.textCursorByteIndex != end) {
                    ui.textCursorByteIndex = end;
                    ui.textCursorBlinkStartTime = GetTime();
                }
                ConsumeEvent(event);
            } else if (event.key.key == KEY_BACKSPACE) {
                if (RemoveUtf8CharacterLeftOfCursor(buffer, ui.textCursorByteIndex)) {
                    result.changed = true;
                    ui.textCursorBlinkStartTime = GetTime();
                }
                ConsumeEvent(event);
            } else if (event.key.key == KEY_DELETE) {
                if (RemoveUtf8CharacterAtCursor(buffer, ui.textCursorByteIndex)) {
                    result.changed = true;
                    ui.textCursorBlinkStartTime = GetTime();
                }
                ConsumeEvent(event);
            } else if (event.key.key == KEY_SPACE) {
                if (!insertedSpaceFromTextEvent
                        && InsertCodepointAtCursor(
                                buffer,
                                bufferCapacity,
                                maxCharacters,
                                ui.textCursorByteIndex,
                                ' ')) {
                    result.changed = true;
                    ui.textCursorBlinkStartTime = GetTime();
                    insertedSpaceFromTextEvent = true;
                }
                ConsumeEvent(event);
            } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                result.submitted = true;
                ui.focusedId = 0;
                ConsumeEvent(event);
            } else if (event.key.key == KEY_ESCAPE) {
                result.cancelled = true;
                ui.focusedId = 0;
                ConsumeEvent(event);
            }
        };

        input.ForEachEvent(InputEventType::KeyPressed, true, handleKey);
        input.ForEachEvent(InputEventType::KeyRepeated, true, handleKey);
    }

    characterCount = CountUtf8Characters(buffer);
    result.valid = characterCount >= minCharacters && characterCount <= maxCharacters;

    const Color border = result.valid ? config.borderColor : config.invalidColor;
    DrawWidgetBackground(ui, config, bounds, InteractiveFill(config, ui, widgetId), border);

    const char* displayText = buffer == nullptr ? "" : buffer;
    Text(ui, config, assets, bounds, font, displayText, justify, config.textColor);

    if (ui.focusedId == widgetId && buffer != nullptr) {
        ui.textCursorByteIndex = ClampToUtf8Boundary(buffer, ui.textCursorByteIndex);

        const double elapsed = GetTime() - ui.textCursorBlinkStartTime;
        const double blinkInterval = std::max(0.01f, config.caretBlinkInterval);
        const bool caretVisible = (static_cast<int>(elapsed / blinkInterval) % 2) == 0;

        const Vector2 textPos = TextPosition(ui, config, assets, bounds, font, displayText, justify);
        const char restored = buffer[ui.textCursorByteIndex];
        buffer[ui.textCursorByteIndex] = '\0';
        const Vector2 prefixSize = MeasureTextWithFont(
                assets,
                font,
                displayText,
                config.fontSize,
                config.textSpacing
        );
        buffer[ui.textCursorByteIndex] = restored;

        if (caretVisible) {
            const Rectangle drawBounds = TransformBounds(ui, bounds);
            const float caretX = std::min(drawBounds.x + drawBounds.width - config.paddingX, textPos.x + prefixSize.x + 2.0f);
            const Rectangle caret{caretX, drawBounds.y + config.paddingY, config.caretWidth, drawBounds.height - config.paddingY * 2.0f};
            DrawRectangleRec(caret, config.textColor);
        }
    }

    return result;
}

bool Button(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        UITextJustify justify)
{
    const uint32_t widgetId = HashId(id);
    if (ContainsWidget(ui, bounds, ui.mousePosition)) {
        ui.hotId = widgetId;
        if (ui.mouseDown) {
            ui.activeId = widgetId;
        }
        ConsumeMousePresses(ui, input, bounds);
    }

    const bool clicked = ConsumeMouseClick(ui, input, bounds);
    DrawWidgetBackground(ui, config, bounds, InteractiveFill(config, ui, widgetId), config.borderColor);
    Text(ui, config, assets, bounds, font, text, justify, config.textColor);
    return clicked;
}

bool ToolButton(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        bool selected)
{
    const uint32_t widgetId = HashId(id);
    if (ContainsWidget(ui, bounds, ui.mousePosition)) {
        ui.hotId = widgetId;
        if (ui.mouseDown) {
            ui.activeId = widgetId;
        }
        ConsumeMousePresses(ui, input, bounds);
    }

    const bool clicked = ConsumeMouseClick(ui, input, bounds);
    DrawWidgetBackground(ui, config, bounds, ToolButtonFill(config, ui, widgetId, selected), config.borderColor);
    Text(ui, config, assets, bounds, font, text, UITextJustify::Center, config.textColor);
    return clicked;
}

bool Checkbox(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* text,
        bool& checked,
        UITextJustify justify)
{
    const uint32_t widgetId = HashId(id);
    if (ContainsWidget(ui, bounds, ui.mousePosition)) {
        ui.hotId = widgetId;
        if (ui.mouseDown) {
            ui.activeId = widgetId;
        }
        ConsumeMousePresses(ui, input, bounds);
    }

    bool changed = false;
    if (ConsumeMouseClick(ui, input, bounds)) {
        checked = !checked;
        changed = true;
    }

    DrawWidgetBackground(ui, config, bounds, InteractiveFill(config, ui, widgetId), config.borderColor);

    const float boxSize = std::max(0.0f, bounds.height - config.paddingY * 2.0f);
    const Rectangle boxLocal{
            bounds.x + config.paddingX,
            bounds.y + (bounds.height - boxSize) * 0.5f,
            boxSize,
            boxSize
    };
    const Rectangle box = TransformBounds(ui, boxLocal);
    DrawRectangleRec(box, config.panelColor);
    DrawRectangleLinesEx(box, config.borderThickness, config.borderColor);
    if (checked) {
        const Rectangle mark{
                box.x + config.checkboxMarkPadding,
                box.y + config.checkboxMarkPadding,
                box.width - config.checkboxMarkPadding * 2.0f,
                box.height - config.checkboxMarkPadding * 2.0f
        };
        DrawRectangleRec(mark, config.accentColor);
    }

    Rectangle textBounds = bounds;
    textBounds.x = boxLocal.x + boxLocal.width + config.paddingX;
    textBounds.width = std::max(0.0f, bounds.x + bounds.width - textBounds.x - config.paddingX);
    Text(ui, config, assets, textBounds, font, text, justify, config.textColor);
    return changed;
}

bool Slider(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        const char* id,
        Rectangle bounds,
        float minValue,
        float maxValue,
        float& value)
{
    const uint32_t widgetId = HashId(id);
    const bool hovered = ContainsWidget(ui, bounds, ui.mousePosition);
    if (hovered) {
        ui.hotId = widgetId;
        if (ui.mouseDown) {
            ui.activeId = widgetId;
        }
        ConsumeMousePresses(ui, input, bounds);
    }
    if (ConsumeMouseClick(ui, input, bounds)) {
        ui.activeId = widgetId;
    }

    const float low = std::min(minValue, maxValue);
    const float high = std::max(minValue, maxValue);
    const float range = high - low;
    value = range > 0.0f ? std::clamp(value, low, high) : low;

    bool changed = false;
    if (ui.activeId == widgetId && ui.mouseDown && range > 0.0f && bounds.width > 0.0f) {
        const Rectangle drawBounds = TransformBounds(ui, bounds);
        const float t = std::clamp((ui.mousePosition.x - drawBounds.x) / drawBounds.width, 0.0f, 1.0f);
        const float nextValue = low + range * t;
        if (nextValue != value) {
            value = nextValue;
            changed = true;
        }
    }

    DrawWidgetBackground(ui, config, bounds, InteractiveFill(config, ui, widgetId), config.borderColor);

    const Rectangle drawBounds = TransformBounds(ui, bounds);
    const float trackY = bounds.y + (bounds.height - config.sliderTrackHeight) * 0.5f;
    const Rectangle trackLocal{
            bounds.x + config.paddingX,
            trackY,
            std::max(0.0f, bounds.width - config.paddingX * 2.0f),
            config.sliderTrackHeight
    };
    const Rectangle track = TransformBounds(ui, trackLocal);
    DrawRectangleRec(track, config.panelColor);

    const float t = range > 0.0f ? (value - low) / range : 0.0f;
    const Rectangle fill{track.x, track.y, track.width * t, track.height};
    DrawRectangleRec(fill, config.accentColor);

    const float handleX = track.x + track.width * t - config.sliderHandleWidth * 0.5f;
    const Rectangle handle{
            handleX,
            drawBounds.y + config.paddingY,
            config.sliderHandleWidth,
            bounds.height - config.paddingY * 2.0f
    };
    DrawRectangleRec(handle, config.textColor);
    return changed;
}

bool IntSlider(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        const char* id,
        Rectangle bounds,
        int minValue,
        int maxValue,
        int& value)
{
    const uint32_t widgetId = HashId(id);
    const bool hovered = ContainsWidget(ui, bounds, ui.mousePosition);
    if (hovered) {
        ui.hotId = widgetId;
        if (ui.mouseDown) {
            ui.activeId = widgetId;
        }
        ConsumeMousePresses(ui, input, bounds);
    }
    if (ConsumeMouseClick(ui, input, bounds)) {
        ui.activeId = widgetId;
    }

    const int low = std::min(minValue, maxValue);
    const int high = std::max(minValue, maxValue);
    const int range = high - low;
    value = std::clamp(value, low, high);

    bool changed = false;
    if (ui.activeId == widgetId && ui.mouseDown && range > 0
            && bounds.width > 0.0f) {
        const Rectangle drawBounds = TransformBounds(ui, bounds);
        const float t = std::clamp(
                (ui.mousePosition.x - drawBounds.x) / drawBounds.width,
                0.0f,
                1.0f);
        const int nextValue = low + static_cast<int>(
                std::lround(static_cast<float>(range) * t));
        if (nextValue != value) {
            value = nextValue;
            changed = true;
        }
    }

    DrawWidgetBackground(
            ui,
            config,
            bounds,
            InteractiveFill(config, ui, widgetId),
            config.borderColor);

    const Rectangle drawBounds = TransformBounds(ui, bounds);
    const float trackY = bounds.y
            + (bounds.height - config.sliderTrackHeight) * 0.5f;
    const Rectangle trackLocal{
            bounds.x + config.paddingX,
            trackY,
            std::max(0.0f, bounds.width - config.paddingX * 2.0f),
            config.sliderTrackHeight};
    const Rectangle track = TransformBounds(ui, trackLocal);
    DrawRectangleRec(track, config.panelColor);

    const float t = range > 0
            ? static_cast<float>(value - low) / static_cast<float>(range)
            : 0.0f;
    DrawRectangleRec(
            Rectangle{track.x, track.y, track.width * t, track.height},
            config.accentColor);

    const float handleX = track.x + track.width * t
            - config.sliderHandleWidth * 0.5f;
    DrawRectangleRec(
            Rectangle{
                    handleX,
                    drawBounds.y + config.paddingY,
                    config.sliderHandleWidth,
                    bounds.height - config.paddingY * 2.0f},
            config.textColor);
    return changed;
}

UINumericInputResult FloatInput(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        float& value,
        UIFloatInputState& state,
        float minValue,
        float maxValue,
        int decimalPlaces)
{
    const uint32_t widgetId = HashId(id);
    const float low = std::min(minValue, maxValue);
    const float high = std::max(minValue, maxValue);
    value = std::clamp(value, low, high);

    if (ContainsWidget(ui, bounds, ui.mousePosition)) {
        ui.hotId = widgetId;
        if (ui.mouseDown) {
            ui.activeId = widgetId;
        }
        ConsumeMousePresses(ui, input, bounds);
    }

    const bool clicked = ConsumeMouseClick(ui, input, bounds);
    if (clicked) {
        state.editing = true;
        state.editingSourceValue = value;
        FormatFloatValue(state.buffer, sizeof(state.buffer), value, decimalPlaces);
        ui.focusedId = widgetId;
        ui.activeId = widgetId;
        ui.textCursorByteIndex = TextByteLength(state.buffer);
        ui.textCursorBlinkStartTime = GetTime();
    }

    UINumericInputResult result;
    result.valid = true;

    if (state.editing && ui.focusedId == widgetId) {
        bool clickedOutside = false;
        input.ForEachEvent(
                InputEventType::MouseClick,
                true,
                [&ui, bounds, &clickedOutside](InputEvent& event) {
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON
                            && !ContainsWidget(ui, bounds, event.mouseClick.releasePosition)) {
                        clickedOutside = true;
                    }
                }
        );

        bool submitted = false;
        bool cancelled = false;
        EditAsciiBuffer(ui, input, state.buffer, sizeof(state.buffer), IsFloatEditCharacter, submitted, cancelled);

        float parsed = 0.0f;
        const bool valid = ParseFloatStrict(state.buffer, parsed);
        result.valid = valid;

        if (cancelled || (clickedOutside && !valid)) {
            value = state.editingSourceValue;
            state.editing = false;
            ui.focusedId = 0;
            result.valid = true;
        } else if ((submitted || clickedOutside) && valid) {
            const float nextValue = std::clamp(parsed, low, high);
            if (nextValue != value) {
                value = nextValue;
                result.changed = true;
            }
            state.editing = false;
            ui.focusedId = 0;
            result.submitted = submitted;
            result.valid = true;
        } else if (submitted && !valid) {
            result.submitted = false;
        }

        ConsumeFocusedPointerEvents(input);
    } else if (state.editing) {
        state.editing = false;
    }

    char display[64] = {};
    const char* displayText = display;
    if (state.editing && ui.focusedId == widgetId) {
        displayText = state.buffer;
    } else {
        FormatFloatValue(display, sizeof(display), value, decimalPlaces);
    }

    DrawTextEditField(
            ui,
            config,
            assets,
            bounds,
            font,
            displayText,
            widgetId,
            result.valid,
            UITextJustify::Left
    );
    return result;
}

UINumericInputResult IntInput(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        int& value,
        UIIntInputState& state,
        int minValue,
        int maxValue,
        int step)
{
    const uint32_t widgetId = HashId(id);
    const int low = std::min(minValue, maxValue);
    const int high = std::max(minValue, maxValue);
    const int amount = std::max(1, step);
    value = std::clamp(value, low, high);

    const float buttonWidth = std::max(bounds.height, 42.0f);
    const Rectangle minusBounds{bounds.x, bounds.y, buttonWidth, bounds.height};
    const Rectangle plusBounds{bounds.x + bounds.width - buttonWidth, bounds.y, buttonWidth, bounds.height};
    const Rectangle fieldBounds{
            minusBounds.x + minusBounds.width + config.rowSpacing,
            bounds.y,
            std::max(0.0f, bounds.width - buttonWidth * 2.0f - config.rowSpacing * 2.0f),
            bounds.height
    };

    auto drawStepperButton = [&](const char* buttonId, Rectangle buttonBounds, const char* label) {
        const uint32_t buttonHash = HashId(buttonId);
        if (ContainsWidget(ui, buttonBounds, ui.mousePosition)) {
            ui.hotId = buttonHash;
            if (ui.mouseDown) {
                ui.activeId = buttonHash;
            }
            ConsumeMousePresses(ui, input, buttonBounds);
        }

        bool clickedButton = false;
        input.ForEachEvent(
                InputEventType::MouseClick,
                true,
                [&ui, buttonBounds, &clickedButton](InputEvent& event) {
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON
                            && ContainsWidget(ui, buttonBounds, event.mouseClick.releasePosition)) {
                        clickedButton = true;
                        ConsumeEvent(event);
                    }
                }
        );

        DrawWidgetBackground(ui, config, buttonBounds, InteractiveFill(config, ui, buttonHash), config.borderColor);
        Text(ui, config, assets, buttonBounds, font, label, UITextJustify::Center, config.textColor);
        return clickedButton;
    };

    char minusId[128];
    char plusId[128];
    std::snprintf(minusId, sizeof(minusId), "%s_minus", id == nullptr ? "" : id);
    std::snprintf(plusId, sizeof(plusId), "%s_plus", id == nullptr ? "" : id);

    UINumericInputResult result;
    result.valid = true;

    if (drawStepperButton(minusId, minusBounds, "-")) {
        if (state.editing && ui.focusedId == widgetId) {
            state.editing = false;
            ui.focusedId = 0;
        }
        const long stepped = static_cast<long>(value) - static_cast<long>(amount);
        const int nextValue = static_cast<int>(std::clamp(stepped, static_cast<long>(low), static_cast<long>(high)));
        if (nextValue != value) {
            value = nextValue;
            result.changed = true;
        }
    }

    if (drawStepperButton(plusId, plusBounds, "+")) {
        if (state.editing && ui.focusedId == widgetId) {
            state.editing = false;
            ui.focusedId = 0;
        }
        const long stepped = static_cast<long>(value) + static_cast<long>(amount);
        const int nextValue = static_cast<int>(std::clamp(stepped, static_cast<long>(low), static_cast<long>(high)));
        if (nextValue != value) {
            value = nextValue;
            result.changed = true;
        }
    }

    if (ContainsWidget(ui, fieldBounds, ui.mousePosition)) {
        ui.hotId = widgetId;
        if (ui.mouseDown) {
            ui.activeId = widgetId;
        }
        ConsumeMousePresses(ui, input, fieldBounds);
    }

    const bool clickedField = ConsumeMouseClick(ui, input, fieldBounds);
    if (clickedField) {
        state.editing = true;
        state.editingSourceValue = value;
        FormatIntValue(state.buffer, sizeof(state.buffer), value);
        ui.focusedId = widgetId;
        ui.activeId = widgetId;
        ui.textCursorByteIndex = TextByteLength(state.buffer);
        ui.textCursorBlinkStartTime = GetTime();
    }

    if (state.editing && ui.focusedId == widgetId) {
        bool clickedOutside = false;
        input.ForEachEvent(
                InputEventType::MouseClick,
                true,
                [&ui, fieldBounds, &clickedOutside](InputEvent& event) {
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON
                            && !ContainsWidget(ui, fieldBounds, event.mouseClick.releasePosition)) {
                        clickedOutside = true;
                    }
                }
        );

        bool submitted = false;
        bool cancelled = false;
        EditAsciiBuffer(ui, input, state.buffer, sizeof(state.buffer), IsIntEditCharacter, submitted, cancelled);

        int parsed = 0;
        const bool valid = ParseIntStrict(state.buffer, parsed);
        result.valid = valid;

        if (cancelled || (clickedOutside && !valid)) {
            value = state.editingSourceValue;
            state.editing = false;
            ui.focusedId = 0;
            result.valid = true;
        } else if ((submitted || clickedOutside) && valid) {
            const int nextValue = std::clamp(parsed, low, high);
            if (nextValue != value) {
                value = nextValue;
                result.changed = true;
            }
            state.editing = false;
            ui.focusedId = 0;
            result.submitted = submitted;
            result.valid = true;
        } else if (submitted && !valid) {
            result.submitted = false;
        }

        ConsumeFocusedPointerEvents(input);
    } else if (state.editing) {
        state.editing = false;
    }

    char display[32] = {};
    const char* displayText = display;
    if (state.editing && ui.focusedId == widgetId) {
        displayText = state.buffer;
    } else {
        FormatIntValue(display, sizeof(display), value);
    }

    DrawTextEditField(
            ui,
            config,
            assets,
            fieldBounds,
            font,
            displayText,
            widgetId,
            result.valid,
            UITextJustify::Left
    );
    return result;
}

bool List(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* const* options,
        size_t optionCount,
        int& selectedIndex)
{
    if (options == nullptr || optionCount == 0) {
        selectedIndex = -1;
        DrawWidgetBackground(ui, config, bounds, config.widgetColor, config.borderColor);
        return false;
    }

    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(optionCount)) {
        selectedIndex = 0;
    }

    bool changed = false;
    const uint32_t baseId = HashId(id);
    DrawSelectablePanelBackground(ui, config, bounds);

    for (size_t i = 0; i < optionCount; ++i) {
        const Rectangle row = ListItemBounds(bounds, config.listItemHeight, i);
        const uint32_t rowId = baseId ^ static_cast<uint32_t>((i + 1u) * 16777619u);
        const bool hovered = ContainsWidget(ui, row, ui.mousePosition);
        if (hovered) {
            ui.hotId = rowId;
            if (ui.mouseDown) {
                ui.activeId = rowId;
            }
            ConsumeMousePresses(ui, input, row);
        }

        if (ConsumeMouseClick(ui, input, row)) {
            const int nextIndex = static_cast<int>(i);
            if (selectedIndex != nextIndex) {
                selectedIndex = nextIndex;
                changed = true;
            }
        }

        DrawSelectableRow(
                ui,
                config,
                assets,
                row,
                font,
                options[i],
                hovered,
                selectedIndex == static_cast<int>(i)
        );
    }

    DrawSelectablePanelLines(ui, config, bounds, optionCount);
    return changed;
}

bool List(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* const* options,
        size_t optionCount,
        bool* selected)
{
    if (options == nullptr || selected == nullptr || optionCount == 0) {
        DrawWidgetBackground(ui, config, bounds, config.widgetColor, config.borderColor);
        return false;
    }

    bool changed = false;
    const uint32_t baseId = HashId(id);
    DrawSelectablePanelBackground(ui, config, bounds);

    for (size_t i = 0; i < optionCount; ++i) {
        const Rectangle row = ListItemBounds(bounds, config.listItemHeight, i);
        const uint32_t rowId = baseId ^ static_cast<uint32_t>((i + 1u) * 16777619u);
        const bool hovered = ContainsWidget(ui, row, ui.mousePosition);
        if (hovered) {
            ui.hotId = rowId;
            if (ui.mouseDown) {
                ui.activeId = rowId;
            }
            ConsumeMousePresses(ui, input, row);
        }

        if (ConsumeMouseClick(ui, input, row)) {
            selected[i] = !selected[i];
            changed = true;
        }

        DrawSelectableRow(
                ui,
                config,
                assets,
                row,
                font,
                options[i],
                hovered,
                selected[i]
        );
    }

    DrawSelectablePanelLines(ui, config, bounds, optionCount);
    return changed;
}

bool Option(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* const* options,
        size_t optionCount,
        int& selectedIndex)
{
    const uint32_t widgetId = HashId(id);
    if (options == nullptr || optionCount == 0) {
        if (ui.openOptionId == widgetId) {
            ClearOpenOption(ui);
        }
        selectedIndex = -1;
        DrawWidgetBackground(ui, config, bounds, config.widgetColor, config.borderColor);
        Text(ui, config, assets, bounds, font, "", UITextJustify::Left, config.mutedTextColor);
        return false;
    }

    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(optionCount)) {
        selectedIndex = 0;
    }

    const Rectangle fieldBounds = TransformBounds(ui, bounds);
    const OptionDropdownLayout dropdownLayout = BuildOptionDropdownLayout(config, fieldBounds, optionCount);
    const Rectangle dropdownBounds = dropdownLayout.bounds;
    bool open = ui.openOptionId == widgetId && dropdownLayout.visibleOptionCount > 0;

    if (open) {
        ui.openOptionIssuedThisFrame = true;

        if (ui.openOptionCloseRequested) {
            ClearOpenOption(ui);
            open = false;
        }
    }

    bool changed = false;
    if (open) {
        ui.openOptionVisibleCount = dropdownLayout.visibleOptionCount;
        ui.openOptionFirstVisibleIndex = ClampOptionFirstVisibleIndex(
                ui.openOptionFirstVisibleIndex,
                optionCount,
                dropdownLayout.visibleOptionCount
        );

        if (ui.openOptionWheelDelta != 0.0f && optionCount > dropdownLayout.visibleOptionCount) {
            const int rowDelta = ui.openOptionWheelDelta > 0.0f
                    ? -static_cast<int>(std::ceil(ui.openOptionWheelDelta))
                    : static_cast<int>(std::ceil(-ui.openOptionWheelDelta));
            const int current = static_cast<int>(ui.openOptionFirstVisibleIndex);
            const int maxFirst = static_cast<int>(optionCount - dropdownLayout.visibleOptionCount);
            ui.openOptionFirstVisibleIndex = static_cast<size_t>(std::clamp(current + rowDelta, 0, maxFirst));
        }

        const size_t firstVisibleIndex = ui.openOptionFirstVisibleIndex;
        const size_t visibleCount = std::min(dropdownLayout.visibleOptionCount, optionCount - firstVisibleIndex);

        for (size_t i = 0; i < visibleCount; ++i) {
            const size_t optionIndex = firstVisibleIndex + i;
            const Rectangle row = ListItemBounds(dropdownBounds, config.listItemHeight, i);
            const uint32_t rowId = widgetId ^ static_cast<uint32_t>((optionIndex + 1u) * 16777619u);
            if (Contains(row, ui.mousePosition)) {
                ui.hotId = rowId;
                if (ui.mouseDown) {
                    ui.activeId = rowId;
                }
            }

            if (ui.openOptionClickPending && Contains(row, ui.openOptionClickPosition)) {
                if (selectedIndex != static_cast<int>(optionIndex)) {
                    selectedIndex = static_cast<int>(optionIndex);
                    changed = true;
                }
                ClearOpenOption(ui);
                open = false;
                break;
            }
        }
    }

    const bool hovered = ContainsWidget(ui, bounds, ui.mousePosition);
    if (hovered) {
        ui.hotId = widgetId;
        if (ui.mouseDown) {
            ui.activeId = widgetId;
        }
        ConsumeMousePresses(ui, input, bounds);
    }

    if (ConsumeMouseClick(ui, input, bounds)) {
        if (open) {
            ClearOpenOption(ui);
            open = false;
        } else if (dropdownLayout.visibleOptionCount > 0) {
            if (ui.openOptionId != widgetId) {
                ui.openOptionFirstVisibleIndex = InitialOptionFirstVisibleIndex(
                        selectedIndex,
                        optionCount,
                        dropdownLayout.visibleOptionCount
                );
            }
            ui.openOptionId = widgetId;
            ui.openOptionVisibleCount = dropdownLayout.visibleOptionCount;
            open = true;
        }
    }

    DrawWidgetBackground(ui, config, bounds, InteractiveFill(config, ui, widgetId), config.borderColor);
    Text(ui, config, assets, bounds, font, options[selectedIndex], UITextJustify::Left, config.textColor);

    const Rectangle drawBounds = TransformBounds(ui, bounds);
    const float arrowX = drawBounds.x + drawBounds.width - config.paddingX - 10.0f;
    const float arrowY = drawBounds.y + drawBounds.height * 0.5f;
    DrawTriangle(
            Vector2{arrowX - 6.0f, arrowY - 3.0f},
            Vector2{arrowX + 6.0f, arrowY - 3.0f},
            Vector2{arrowX, arrowY + 5.0f},
            config.textColor
    );

    if (open && ui.openOptionId == widgetId) {
        ui.openOptionIssuedThisFrame = true;
        ui.openOptionBoundsValid = true;
        ui.openOptionFieldBounds = fieldBounds;
        ui.openOptionDropdownBounds = dropdownBounds;
        ui.openOptionVisibleCount = dropdownLayout.visibleOptionCount;
        ui.openOptionFirstVisibleIndex = ClampOptionFirstVisibleIndex(
                ui.openOptionFirstVisibleIndex,
                optionCount,
                dropdownLayout.visibleOptionCount
        );
        UIContext::OptionOverlay overlay;
        overlay.active = true;
        overlay.id = widgetId;
        overlay.fieldBounds = fieldBounds;
        overlay.dropdownBounds = dropdownBounds;
        overlay.font = font;
        overlay.options.reserve(optionCount);
        for (size_t i = 0; i < optionCount; ++i) {
            overlay.options.emplace_back(options[i] == nullptr ? "" : options[i]);
        }
        overlay.firstVisibleIndex = ui.openOptionFirstVisibleIndex;
        overlay.visibleOptionCount = dropdownLayout.visibleOptionCount;
        overlay.selectedIndexSnapshot = selectedIndex;
        ui.optionOverlay = std::move(overlay);
    } else if (ui.openOptionId == widgetId) {
        ClearOpenOption(ui);
    }

    return changed;
}

bool Option(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const std::vector<std::string>& options,
        int& selectedIndex)
{
    std::vector<const char*> optionLabels;
    optionLabels.reserve(options.size());
    for (const std::string& option : options) {
        optionLabels.push_back(option.c_str());
    }
    return Option(
            ui,
            config,
            input,
            assets,
            id,
            bounds,
            font,
            optionLabels.empty() ? nullptr : optionLabels.data(),
            optionLabels.size(),
            selectedIndex);
}

void CloseMainMenu(UIMainMenuState& state)
{
    state.openRootIndex = -1;
    state.openDepth = 0;
    state.openItemIndices.fill(0);
    state.firstVisibleIndices.fill(0);
}

UIMainMenuResult MainMenu(
        UIContext& ui,
        const UIConfig& config,
        Input& input,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const UIMenuRoot* roots,
        size_t rootCount,
        UIMainMenuState& state,
        bool enabled)
{
    UIMainMenuResult result;
    const size_t visibleRootCount = std::min(rootCount, UIMainMenuMaxRoots);
    if (roots == nullptr || visibleRootCount == 0 || bounds.width <= 0.0f
            || bounds.height <= 0.0f) {
        CloseMainMenu(state);
        return result;
    }

    if (state.openRootIndex < 0
            || state.openRootIndex >= static_cast<int>(visibleRootCount)) {
        CloseMainMenu(state);
    }
    if (!enabled) {
        CloseMainMenu(state);
    }

    std::array<Rectangle, UIMainMenuMaxRoots> rootBounds{};
    float rootX = bounds.x + config.paddingX * 0.5f;
    for (size_t index = 0; index < visibleRootCount; ++index) {
        const Vector2 measured = MeasureTextWithFont(
                assets,
                font,
                roots[index].label,
                config.fontSize,
                config.textSpacing);
        const float width = measured.x + config.paddingX * 2.0f;
        rootBounds[index] = Rectangle{
                rootX,
                bounds.y,
                std::min(width, std::max(0.0f, bounds.x + bounds.width - rootX)),
                bounds.height};
        rootX += width;
    }

    bool escapeRequested = false;
    input.ForEachEvent(
            InputEventType::KeyPressed,
            true,
            [&escapeRequested, &state](InputEvent& event) {
                if (event.key.key == KEY_ESCAPE && state.openRootIndex >= 0) {
                    escapeRequested = true;
                    ConsumeEvent(event);
                }
            });
    if (escapeRequested) {
        CloseMainMenu(state);
    }

    if (enabled) {
        input.ForEachEvent(
                InputEventType::MouseClick,
                true,
                [&](InputEvent& event) {
                    if (event.mouseClick.button != MOUSE_LEFT_BUTTON) return;
                    for (size_t index = 0; index < visibleRootCount; ++index) {
                        if (!Contains(rootBounds[index], event.mouseClick.releasePosition)) {
                            continue;
                        }
                        if (!roots[index].enabled || roots[index].items == nullptr
                                || roots[index].itemCount == 0) {
                            ConsumeEvent(event);
                            return;
                        }
                        const int requested = static_cast<int>(index);
                        if (state.openRootIndex == requested) {
                            CloseMainMenu(state);
                        } else {
                            state.openRootIndex = requested;
                            state.openDepth = 0;
                            state.openItemIndices.fill(0);
                            state.firstVisibleIndices.fill(0);
                            ClearOpenOption(ui);
                            ui.focusedId = 0;
                        }
                        ConsumeEvent(event);
                        return;
                    }
                });

        if (state.openRootIndex >= 0) {
            for (size_t index = 0; index < visibleRootCount; ++index) {
                if (static_cast<int>(index) == state.openRootIndex
                        || !roots[index].enabled
                        || roots[index].items == nullptr
                        || roots[index].itemCount == 0) {
                    continue;
                }
                if (Contains(rootBounds[index], ui.mousePosition)) {
                    state.openRootIndex = static_cast<int>(index);
                    state.openDepth = 0;
                    state.openItemIndices.fill(0);
                    state.firstVisibleIndices.fill(0);
                    break;
                }
            }
        }
    }

    DrawRectangleRec(bounds, config.panelHeaderColor);
    DrawLineEx(
            Vector2{bounds.x, bounds.y + bounds.height},
            Vector2{bounds.x + bounds.width, bounds.y + bounds.height},
            config.borderThickness,
            config.borderColor);
    for (size_t index = 0; index < visibleRootCount; ++index) {
        const bool hovered = Contains(rootBounds[index], ui.mousePosition);
        const bool selected = state.openRootIndex == static_cast<int>(index);
        if ((hovered || selected) && enabled && roots[index].enabled) {
            DrawRectangleRec(
                    rootBounds[index],
                    selected ? config.widgetActiveColor : config.widgetHoverColor);
        }
        Text(
                config,
                assets,
                rootBounds[index],
                font,
                roots[index].label == nullptr ? "" : roots[index].label,
                UITextJustify::Center,
                enabled && roots[index].enabled
                        ? config.textColor : config.disabledColor);
    }

    const bool menuWasOpened = state.openRootIndex >= 0;
    if (enabled && state.openRootIndex >= 0) {
        UIContext::MainMenuOverlay& overlay = ui.mainMenuOverlay;
        overlay = UIContext::MainMenuOverlay{};
        overlay.active = true;
        overlay.font = font;

        const Rectangle visibleBounds = ResolveOverlayBounds(config);
        const float rowHeight = std::max(1.0f, config.listItemHeight);
        const UIMenuRoot& root = roots[static_cast<size_t>(state.openRootIndex)];
        const UIMenuItem* levelItems = root.items;
        size_t levelItemCount = root.itemCount;
        Rectangle anchor = rootBounds[static_cast<size_t>(state.openRootIndex)];

        for (size_t depth = 0;
                depth < UIMainMenuMaxDepth
                        && levelItems != nullptr
                        && levelItemCount > 0
                        && overlay.popupCount < overlay.popups.size();
                ++depth) {
            float popupWidth = 180.0f;
            for (size_t itemIndex = 0; itemIndex < levelItemCount; ++itemIndex) {
                const UIMenuItem& item = levelItems[itemIndex];
                const Vector2 measured = MeasureTextWithFont(
                        assets,
                        font,
                        item.label,
                        config.fontSize,
                        config.textSpacing);
                popupWidth = std::max(
                        popupWidth,
                        measured.x + config.paddingX * 2.0f + 56.0f);
            }
            popupWidth = std::min(popupWidth, visibleBounds.width);

            const size_t fitRows = std::max<size_t>(
                    1,
                    static_cast<size_t>(std::floor(visibleBounds.height / rowHeight)));
            const size_t visibleRows = std::min(levelItemCount, fitRows);
            size_t& firstVisible = state.firstVisibleIndices[depth];
            firstVisible = std::min(
                    firstVisible,
                    levelItemCount > visibleRows ? levelItemCount - visibleRows : 0u);

            float popupX = depth == 0
                    ? anchor.x : anchor.x + anchor.width;
            float popupY = depth == 0
                    ? anchor.y + anchor.height : anchor.y;
            const float popupHeight = rowHeight * static_cast<float>(visibleRows);
            if (depth > 0
                    && popupX + popupWidth > visibleBounds.x + visibleBounds.width) {
                popupX = anchor.x - popupWidth;
            }
            popupX = visibleBounds.width > popupWidth
                    ? std::clamp(
                            popupX,
                            visibleBounds.x,
                            visibleBounds.x + visibleBounds.width - popupWidth)
                    : visibleBounds.x;
            popupY = visibleBounds.height > popupHeight
                    ? std::clamp(
                            popupY,
                            visibleBounds.y,
                            visibleBounds.y + visibleBounds.height - popupHeight)
                    : visibleBounds.y;
            Rectangle popupBounds{popupX, popupY, popupWidth, popupHeight};

            input.ForEachEvent(
                    InputEventType::MouseWheel,
                    true,
                    [&](InputEvent& event) {
                        if (!Contains(popupBounds, ui.mousePosition)
                                || levelItemCount <= visibleRows) {
                            return;
                        }
                        const int delta = event.wheel.value > 0.0f ? -1 : 1;
                        const int maximum = static_cast<int>(levelItemCount - visibleRows);
                        firstVisible = static_cast<size_t>(std::clamp(
                                static_cast<int>(firstVisible) + delta,
                                0,
                                maximum));
                        ConsumeEvent(event);
                    });

            UIContext::MainMenuOverlayPopup& popup =
                    overlay.popups[overlay.popupCount++];
            popup.bounds = popupBounds;
            popup.firstRow = overlay.rowCount;

            bool pointerInsidePopup = Contains(popupBounds, ui.mousePosition);
            bool hoveredRowFound = false;
            Rectangle nextAnchor{};
            bool hasNextAnchor = false;
            const UIMenuItem* nextItems = nullptr;
            size_t nextItemCount = 0;

            for (size_t visibleIndex = 0;
                    visibleIndex < visibleRows
                            && overlay.rowCount < overlay.rows.size();
                    ++visibleIndex) {
                const size_t itemIndex = firstVisible + visibleIndex;
                const UIMenuItem& item = levelItems[itemIndex];
                const Rectangle row{
                        popupBounds.x,
                        popupBounds.y + rowHeight * static_cast<float>(visibleIndex),
                        popupBounds.width,
                        rowHeight};
                const bool hovered = Contains(row, ui.mousePosition);
                UIContext::MainMenuOverlayRow& overlayRow =
                        overlay.rows[overlay.rowCount++];
                overlayRow.bounds = row;
                overlayRow.label = item.label;
                overlayRow.kind = item.kind;
                overlayRow.enabled = item.enabled;
                overlayRow.checked = item.checked;
                overlayRow.hovered = hovered;
                ++popup.rowCount;

                if (hovered) {
                    hoveredRowFound = true;
                    if (item.enabled
                            && item.kind == UIMenuItemKind::Submenu
                            && item.children != nullptr
                            && item.childCount > 0) {
                        state.openItemIndices[depth] = itemIndex;
                        state.openDepth = depth + 1;
                    } else {
                        state.openDepth = depth;
                    }
                }

                if (depth < state.openDepth
                        && state.openItemIndices[depth] == itemIndex
                        && item.enabled
                        && item.kind == UIMenuItemKind::Submenu
                        && item.children != nullptr
                        && item.childCount > 0) {
                    nextAnchor = row;
                    hasNextAnchor = true;
                    nextItems = item.children;
                    nextItemCount = item.childCount;
                }

                input.ForEachEvent(
                        InputEventType::MouseClick,
                        true,
                        [&](InputEvent& event) {
                            if (event.mouseClick.button != MOUSE_LEFT_BUTTON
                                    || !Contains(row, event.mouseClick.releasePosition)) {
                                return;
                            }
                            ConsumeEvent(event);
                            if (!item.enabled
                                    || item.kind == UIMenuItemKind::Separator) {
                                return;
                            }
                            if (item.kind == UIMenuItemKind::Submenu) {
                                if (item.children != nullptr && item.childCount > 0) {
                                    state.openItemIndices[depth] = itemIndex;
                                    state.openDepth = depth + 1;
                                }
                                return;
                            }
                            result.activated = true;
                            result.commandId = item.commandId;
                            CloseMainMenu(state);
                        });
            }

            if (result.activated || state.openRootIndex < 0) break;
            if (pointerInsidePopup && !hoveredRowFound) {
                state.openDepth = depth;
            }
            if (depth >= state.openDepth || !hasNextAnchor) break;
            anchor = nextAnchor;
            levelItems = nextItems;
            levelItemCount = nextItemCount;
        }

        input.ForEachEvent(
                InputEventType::MouseClick,
                true,
                [&](InputEvent& event) {
                    if (event.mouseClick.button != MOUSE_LEFT_BUTTON) return;
                    bool inside = Contains(bounds, event.mouseClick.releasePosition);
                    for (size_t index = 0; index < overlay.popupCount && !inside; ++index) {
                        inside = Contains(
                                overlay.popups[index].bounds,
                                event.mouseClick.releasePosition);
                    }
                    if (!inside) {
                        CloseMainMenu(state);
                    }
                    ConsumeEvent(event);
                });
    }

    const bool menuOpenNow = state.openRootIndex >= 0;
    input.ForEachEvent(
            InputEventType::MouseButtonPressed,
            true,
            [&](InputEvent& event) {
                if (event.mouseButton.button == MOUSE_LEFT_BUTTON
                        && (menuOpenNow || menuWasOpened
                                || Contains(bounds, event.mouseButton.position))) {
                    ConsumeEvent(event);
                }
            });
    input.ForEachEvent(
            InputEventType::MouseClick,
            true,
            [&](InputEvent& event) {
                if (event.mouseClick.button == MOUSE_LEFT_BUTTON
                        && Contains(bounds, event.mouseClick.releasePosition)) {
                    ConsumeEvent(event);
                }
            });

    ui.mainMenuBlocksNormalInput = state.openRootIndex >= 0;
    result.open = state.openRootIndex >= 0;
    if (!result.open) {
        ui.mainMenuOverlay = UIContext::MainMenuOverlay{};
    }
    return result;
}

UIPanelResult BeginPanel(
        UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        const char* id,
        Rectangle bounds,
        FontHandle font,
        const char* title)
{
    (void)id;

    const bool hasTitle = title != nullptr && title[0] != '\0';
    DrawWidgetBackground(ui, config, bounds, config.panelColor, config.borderColor);

    float contentY = bounds.y + config.paddingY;
    if (hasTitle) {
        const Rectangle header{
                bounds.x,
                bounds.y,
                bounds.width,
                std::min(bounds.height, config.panelHeaderHeight)
        };
        DrawRectangleRec(TransformBounds(ui, header), config.panelHeaderColor);
        Text(ui, config, assets, header, font, title, UITextJustify::Left, config.textColor);

        const Rectangle headerLine{
                bounds.x,
                bounds.y + header.height - config.borderThickness,
                bounds.width,
                config.borderThickness
        };
        DrawRectangleRec(TransformBounds(ui, headerLine), config.borderColor);
        contentY = bounds.y + header.height + config.paddingY;
    }

    UIPanelResult result;
    result.contentRect = Rectangle{
            bounds.x + config.paddingX,
            contentY,
            std::max(0.0f, bounds.width - config.paddingX * 2.0f),
            std::max(0.0f, bounds.y + bounds.height - contentY - config.paddingY)
    };
    return result;
}

void EndPanel(
        UIContext& ui,
        const UIConfig& config,
        const UIPanelResult& panel)
{
    (void)ui;
    (void)config;
    (void)panel;
}

void Separator(
        const UIConfig& config,
        Rectangle bounds)
{
    const float y = bounds.y + bounds.height * 0.5f;
    DrawLineEx(
            Vector2{bounds.x, y},
            Vector2{bounds.x + bounds.width, y},
            config.borderThickness,
            config.borderColor
    );
}

UILabelFieldRowResult LabelFieldRow(
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        FontHandle font,
        const char* label,
        float labelWidth)
{
    const float clampedLabelWidth = std::clamp(labelWidth, 0.0f, bounds.width);
    UILabelFieldRowResult result;
    result.labelRect = Rectangle{
            bounds.x,
            bounds.y,
            clampedLabelWidth,
            bounds.height
    };
    result.fieldRect = Rectangle{
            bounds.x + clampedLabelWidth + config.rowSpacing,
            bounds.y,
            std::max(0.0f, bounds.width - clampedLabelWidth - config.rowSpacing),
            bounds.height
    };

    Text(config, assets, result.labelRect, font, label == nullptr ? "" : label, UITextJustify::Left, config.mutedTextColor);
    return result;
}

void Image(
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        TextureHandle texture,
        Color tint)
{
    const Texture2D* resolvedTexture = assets.GetTexture(texture);
    if (resolvedTexture == nullptr) {
        DrawRectangleLinesEx(bounds, config.borderThickness, config.placeholderColor);
        DrawLineEx(
                Vector2{bounds.x, bounds.y},
                Vector2{bounds.x + bounds.width, bounds.y + bounds.height},
                config.borderThickness,
                config.placeholderColor
        );
        DrawLineEx(
                Vector2{bounds.x + bounds.width, bounds.y},
                Vector2{bounds.x, bounds.y + bounds.height},
                config.borderThickness,
                config.placeholderColor
        );
        return;
    }

    const Rectangle source{
            0.0f,
            0.0f,
            static_cast<float>(resolvedTexture->width),
            static_cast<float>(resolvedTexture->height)
    };
    DrawTexturePro(*resolvedTexture, source, bounds, Vector2{}, 0.0f, tint);
}

void Image(
        const UIContext& ui,
        const UIConfig& config,
        AssetManager& assets,
        Rectangle bounds,
        TextureHandle texture,
        Color tint)
{
    Image(config, assets, TransformBounds(ui, bounds), texture, tint);
}

} // namespace engine
