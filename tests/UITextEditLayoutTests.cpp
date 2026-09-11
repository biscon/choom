#include "engine/ui/UITextEditLayout.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(float a, float b)
{
    return std::fabs(a - b) < 0.001f;
}

bool ContainsCaret(const engine::UITextEditLayout& layout)
{
    return layout.caretBounds.x >= layout.clipBounds.x
            && layout.caretBounds.x + layout.caretBounds.width
                    <= layout.clipBounds.x + layout.clipBounds.width + 0.001f;
}

void TestEditingSequence()
{
    engine::UIContext ui;
    engine::UIConfig config;
    config.paddingX = 10.0f;
    config.paddingY = 5.0f;
    const Rectangle bounds{30, 20, 120, 40};
    ui.focusedId = 1;
    auto layoutFor = [&](float width, float prefix) {
        return engine::BuildUITextEditLayout(ui, config, 1, bounds,
                width, prefix, engine::UITextJustify::Left);
    };

    auto layout = layoutFor(40, 40);
    Check(Near(ui.textScrollOffsetX, 0) && Near(layout.textPosition.x, 40),
            "short input retains its padded origin");
    // Typing and holding Backspace must keep the whole caret visible.
    for (int width = 40; width <= 400; ++width) {
        layout = layoutFor(static_cast<float>(width), static_cast<float>(width));
        Check(ContainsCaret(layout), "typing reveals the entire caret");
    }
    const float endOffset = ui.textScrollOffsetX;
    layout = layoutFor(400, 390);
    Check(Near(ui.textScrollOffsetX, endOffset),
            "movement within the viewport does not move the text");
    for (int prefix = 400; prefix >= 0; --prefix) {
        layout = layoutFor(400, static_cast<float>(prefix));
        Check(ContainsCaret(layout), "Left traverses the full string visibly");
    }
    Check(Near(ui.textScrollOffsetX, 0), "Home reveals the beginning");
    layout = layoutFor(400, 400);
    Check(ContainsCaret(layout) && ui.textScrollOffsetX > 0,
            "End immediately reveals the end");
    for (int width = 400; width >= 0; --width) {
        layout = layoutFor(static_cast<float>(width), static_cast<float>(width));
        Check(ContainsCaret(layout), "deletion clamps scrolling to remaining text");
    }
    Check(Near(ui.textScrollOffsetX, 0), "empty input resets scrolling");

    layoutFor(400, 400);
    layout = layoutFor(40, 0);
    Check(Near(ui.textScrollOffsetX, 0) && ContainsCaret(layout),
            "external replacement with short text removes the old offset");
    layout = layoutFor(100, 100);
    Check(ContainsCaret(layout), "exact-width text reserves room for the caret");

    for (float width : {24.0f, 50.0f, 120.0f, 500.0f}) {
        layout = engine::BuildUITextEditLayout(ui, config, 1,
                {30, 20, width, 40}, 400, 400, engine::UITextJustify::Left);
        Check(ContainsCaret(layout), "resizing keeps the end caret visible");
    }
    Check(Near(ui.textScrollOffsetX, 0), "expanding to fit resets scrolling");
    layout = engine::BuildUITextEditLayout(ui, config, 1,
            {30.25f, 20, 120.5f, 40}, 400, 400, engine::UITextJustify::Left);
    Check(ContainsCaret(layout)
                    && Near(layout.clipBounds.x, std::ceil(layout.clipBounds.x))
                    && Near(layout.clipBounds.width, std::floor(layout.clipBounds.width)),
            "fractional field bounds keep the caret within the pixel scissor");
    layout = engine::BuildUITextEditLayout(ui, config, 1,
            {30, 20, 0, 0}, 400, 400, engine::UITextJustify::Left);
    Check(Near(layout.clipBounds.width, 0) && Near(layout.clipBounds.height, 0)
                    && std::isfinite(ui.textScrollOffsetX),
            "collapsed fields have an empty clip and finite scrolling");
}

void TestFocusAndAlignment()
{
    engine::UIContext ui;
    engine::UIConfig config;
    const Rectangle bounds{0, 0, 160, 40};
    ui.focusedId = 1;
    engine::BuildUITextEditLayout(ui, config, 1, bounds, 500, 500,
            engine::UITextJustify::Left);
    const float offset = ui.textScrollOffsetX;
    auto layout = engine::BuildUITextEditLayout(ui, config, 2, bounds, 500, 0,
            engine::UITextJustify::Left);
    Check(Near(layout.textPosition.x, config.paddingX)
                    && Near(ui.textScrollOffsetX, offset) && ui.textScrollOwnerId == 1,
            "unfocused fields show their beginning without altering the active offset");

    ui.focusedId = 2;
    layout = engine::BuildUITextEditLayout(ui, config, 1, bounds, 500, 500,
            engine::UITextJustify::Left);
    Check(Near(layout.textPosition.x, config.paddingX),
            "the previous field restores its beginning on blur");
    layout = engine::BuildUITextEditLayout(ui, config, 2, bounds, 500, 20,
            engine::UITextJustify::Left);
    Check(Near(ui.textScrollOffsetX, 0) && ui.textScrollOwnerId == 2,
            "a new focused field starts with its own offset");

    for (auto justify : {engine::UITextJustify::Left,
            engine::UITextJustify::Center, engine::UITextJustify::Right}) {
        layout = engine::BuildUITextEditLayout(ui, config, 2, bounds, 500, 500, justify);
        Check(ContainsCaret(layout), "overflow reveals the caret for every justification");
        ui.focusedId = 0;
        layout = engine::BuildUITextEditLayout(ui, config, 2, bounds, 40, 0, justify);
        const float expected = justify == engine::UITextJustify::Left ? 14.0f
                : justify == engine::UITextJustify::Center ? 60.0f : 106.0f;
        Check(Near(layout.textPosition.x, expected),
                "blur restores configured alignment");
        ui.focusedId = 2;
        layout = engine::BuildUITextEditLayout(ui, config, 2, bounds, 40, 40, justify);
        Check(ContainsCaret(layout), "short aligned input keeps its whole caret inside");
    }
}

void TestPaneClipping()
{
    engine::UIContext ui;
    engine::UIConfig config;
    config.paddingX = 5;
    config.paddingY = 5;
    ui.inScrollArea = true;
    ui.scrollViewport = {40, 30, 80, 50};
    const auto layout = engine::BuildUITextEditLayout(ui, config, 1,
            {20, 10, 100, 40}, 400, 0, engine::UITextJustify::Left);
    Check(Near(layout.clipBounds.x, 40) && Near(layout.clipBounds.y, 30)
                    && Near(layout.clipBounds.width, 75) && Near(layout.clipBounds.height, 18),
            "field clipping intersects a partially visible scroll-pane row");
    const auto hidden = engine::BuildUITextEditLayout(ui, config, 1,
            {20, 100, 100, 40}, 400, 0, engine::UITextJustify::Left);
    Check(Near(hidden.clipBounds.height, 0), "offscreen rows have an empty clip");
    ui.inScrollArea = false;
    config.paddingY = 8;
    const auto compact = engine::BuildUITextEditLayout(ui, config, 1,
            {0, 0, 120, 40}, 20, 0, engine::UITextJustify::Left);
    Check(compact.clipBounds.y <= compact.textPosition.y
                    && compact.clipBounds.y + compact.clipBounds.height
                            >= compact.textPosition.y + config.fontSize,
            "compact editor rows retain their full centered text height");
}

void TestFontMeasurement()
{
    GlyphInfo glyphs[4] = {};
    Rectangle rectangles[4] = {};
    const int codepoints[] = {'?', 'i', 'W', 0x00e9};
    const int advances[] = {5, 3, 11, 7};
    for (int i = 0; i < 4; ++i) {
        glyphs[i].value = codepoints[i];
        glyphs[i].advanceX = advances[i];
        rectangles[i].width = static_cast<float>(advances[i]);
    }
    Font font{};
    font.baseSize = 10;
    font.glyphCount = 4;
    font.texture.id = 1; // CPU-only metrics; no GPU resource is created or used.
    font.glyphs = glyphs;
    font.recs = rectangles;
    const char* text = "iW\xc3\xa9i";
    for (float size : {10.0f, 17.5f, 30.0f}) {
        for (float spacing : {0.0f, 1.0f, 2.5f}) {
            for (size_t bytes : {size_t{0}, size_t{1}, size_t{2}, size_t{4}, size_t{5}}) {
                const std::string prefix(text, bytes);
                Check(Near(engine::MeasureUITextEditPrefix(font, size, spacing, text, bytes),
                                MeasureTextEx(font, prefix.c_str(), size, spacing).x),
                        "UTF-8 prefixes match raylib variable-width font measurement");
            }
        }
    }
    Check(Near(engine::MeasureUITextEditPrefix(font, 10, 1, text, 3), 15),
            "a partial UTF-8 codepoint is excluded from prefix measurement");
    const std::string longText(1024, 'W');
    Check(Near(engine::MeasureUITextEditPrefix(font, 10, 1, longText.c_str(), 900), 10799),
            "caret measurement continues past fixed-size prefix limits");
    Check(Near(engine::MeasureUITextEditPrefix({}, 10, 1, text, 5), 0)
                    && Near(engine::MeasureUITextEditPrefix(font, 10, 1, nullptr, 5), 0),
            "missing font and buffer have safe measurements");
}

} // namespace

int main()
{
    TestEditingSequence();
    TestFocusAndAlignment();
    TestPaneClipping();
    TestFontMeasurement();
    return failures == 0 ? 0 : 1;
}
