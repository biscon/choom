#include "engine/debug/DebugConsole.h"

#include "engine/assets/AssetManager.h"
#include "engine/assets/FontAssets.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace engine {
namespace {

const Font* ResolveFont(AssetManager& assets, FontHandle handle)
{
    const FontAsset* asset = assets.GetFont(handle);
    return asset != nullptr ? &asset->font : nullptr;
}

float TextWidth(
        AssetManager& assets,
        FontHandle font,
        const char* text,
        float fontSize,
        float spacing)
{
    const char* safeText = text != nullptr ? text : "";
    const Font* native = ResolveFont(assets, font);
    if (native != nullptr) {
        return MeasureTextEx(*native, safeText, fontSize, spacing).x;
    }
    return static_cast<float>(MeasureText(safeText, static_cast<int>(fontSize)));
}

void DrawConsoleText(
        AssetManager& assets,
        FontHandle font,
        const char* text,
        Vector2 position,
        float fontSize,
        float spacing,
        Color tint)
{
    const char* safeText = text != nullptr ? text : "";
    const Font* native = ResolveFont(assets, font);
    if (native != nullptr) {
        DrawTextEx(*native, safeText, position, fontSize, spacing, tint);
    } else {
        DrawText(
                safeText,
                static_cast<int>(std::round(position.x)),
                static_cast<int>(std::round(position.y)),
                static_cast<int>(fontSize),
                tint);
    }
}

std::string EncodeRange(const std::vector<int>& codepoints, int begin, int end)
{
    DebugConsoleEditor editor;
    editor.codepoints.assign(
            codepoints.begin() + begin,
            codepoints.begin() + end);
    return DebugConsoleEditorText(editor);
}

struct WrapFragment {
    int begin = 0;
    int end = 0;
};

std::vector<WrapFragment> WrapCodepoints(
        AssetManager& assets,
        FontHandle font,
        const std::vector<int>& codepoints,
        float firstWidth,
        float continuationWidth,
        float fontSize,
        float spacing)
{
    std::vector<WrapFragment> result;
    if (codepoints.empty()) {
        result.push_back(WrapFragment{});
        return result;
    }
    int lineStart = 0;
    while (lineStart < static_cast<int>(codepoints.size())) {
        const float width = result.empty() ? firstWidth : continuationWidth;
        int lineEnd = lineStart;
        int lastBreak = -1;
        while (lineEnd < static_cast<int>(codepoints.size())) {
            const int candidateEnd = lineEnd + 1;
            const std::string candidate = EncodeRange(
                    codepoints, lineStart, candidateEnd);
            if (TextWidth(assets, font, candidate.c_str(), fontSize, spacing) > width
                    && lineEnd > lineStart) {
                break;
            }
            lineEnd = candidateEnd;
            if (codepoints[lineEnd - 1] == ' '
                    || codepoints[lineEnd - 1] == '\t') {
                lastBreak = lineEnd;
            }
        }
        if (lineEnd < static_cast<int>(codepoints.size())
                && lastBreak > lineStart) {
            lineEnd = lastBreak;
        }
        if (lineEnd == lineStart) ++lineEnd;
        result.push_back(WrapFragment{lineStart, lineEnd});
        lineStart = lineEnd;
    }
    return result;
}

void RebuildOutputLayout(
        DebugConsoleData& console,
        AssetManager& assets,
        float width)
{
    console.visualLines.clear();
    const float promptWidth = TextWidth(
            assets, console.font, "> ", console.style.fontSize,
            console.style.fontSpacing);
    for (const DebugConsoleLine& line : console.lines) {
        const bool input = line.severity == DebugConsoleSeverity::Input;
        const std::vector<int> codepoints = DebugConsoleDecodeUtf8(line.text, false);
        const std::vector<WrapFragment> fragments = WrapCodepoints(
                assets,
                console.font,
                codepoints,
                std::max(1.0f, width - (input ? promptWidth : 0.0f)),
                std::max(1.0f, width - (input ? promptWidth : 0.0f)),
                console.style.fontSize,
                console.style.fontSpacing);
        for (size_t i = 0; i < fragments.size(); ++i) {
            const WrapFragment fragment = fragments[i];
            console.visualLines.push_back(DebugConsoleVisualLine{
                    line.sequence,
                    static_cast<int>(i),
                    line.severity,
                    input ? (i == 0 ? "> " : "  ") : "",
                    EncodeRange(codepoints, fragment.begin, fragment.end)});
        }
    }
    console.cachedWrapWidth = width;
    console.layoutDirty = false;
    if (console.preserveScrollOnLayout) {
        if (console.visualLines.size() > console.anchorVisualLineCount) {
            console.scrollOffsetVisualLines += static_cast<int>(
                    console.visualLines.size() - console.anchorVisualLineCount);
        }
        console.preserveScrollOnLayout = false;
        console.anchorVisualLineCount = 0;
    }
}

void RebuildEditorLayout(
        DebugConsoleData& console,
        AssetManager& assets,
        float width)
{
    console.editorVisualLines.clear();
    const float promptWidth = TextWidth(
            assets, console.font, "> ", console.style.fontSize,
            console.style.fontSpacing);
    const std::vector<WrapFragment> fragments = WrapCodepoints(
            assets,
            console.font,
            console.editor.codepoints,
            std::max(1.0f, width - promptWidth),
            std::max(1.0f, width - promptWidth),
            console.style.fontSize,
            console.style.fontSpacing);
    for (size_t i = 0; i < fragments.size(); ++i) {
        const WrapFragment fragment = fragments[i];
        console.editorVisualLines.push_back(DebugConsoleEditorVisualLine{
                fragment.begin,
                fragment.end,
                i == 0 ? "> " : "  ",
                EncodeRange(
                        console.editor.codepoints,
                        fragment.begin,
                        fragment.end)});
    }
    console.cachedEditorRevision = console.editor.revision;
    console.cachedEditorWrapWidth = width;
    console.cachedCaretVisualLine = 0;
    console.cachedCaretOffsetX = 0.0f;
    for (int i = 0; i < static_cast<int>(console.editorVisualLines.size()); ++i) {
        const DebugConsoleEditorVisualLine& line =
                console.editorVisualLines[static_cast<size_t>(i)];
        if (console.editor.caret < line.startCodepoint
                || console.editor.caret > line.endCodepoint) {
            continue;
        }
        console.cachedCaretVisualLine = i;
        const int caret = std::clamp(
                console.editor.caret,
                line.startCodepoint,
                line.endCodepoint);
        const std::string before = line.prefix + EncodeRange(
                console.editor.codepoints, line.startCodepoint, caret);
        console.cachedCaretOffsetX = TextWidth(
                assets,
                console.font,
                before.c_str(),
                console.style.fontSize,
                console.style.fontSpacing);
        break;
    }
}

Color SeverityColor(DebugConsoleSeverity severity)
{
    switch (severity) {
        case DebugConsoleSeverity::Trace: return Color{150, 155, 165, 255};
        case DebugConsoleSeverity::Debug: return Color{190, 195, 205, 255};
        case DebugConsoleSeverity::Info: return Color{115, 190, 245, 255};
        case DebugConsoleSeverity::Success: return Color{105, 225, 145, 255};
        case DebugConsoleSeverity::Warning: return Color{245, 210, 90, 255};
        case DebugConsoleSeverity::Error: return Color{255, 125, 95, 255};
        case DebugConsoleSeverity::Fatal: return Color{255, 70, 80, 255};
        case DebugConsoleSeverity::Input: return RAYWHITE;
        case DebugConsoleSeverity::LuaResult: return Color{100, 225, 235, 255};
    }
    return RAYWHITE;
}

} // namespace

void DebugConsoleRender(
        DebugConsoleData& console,
        AssetManager& assets,
        int logicalWidth,
        int logicalHeight)
{
    if (!DebugConsoleIsOpen(console) || logicalWidth <= 0 || logicalHeight <= 0) {
        return;
    }
    const DebugConsoleStyle& style = console.style;
    const float maxPanelHeight = std::max(
            80.0f,
            static_cast<float>(logicalHeight) - style.topMargin - style.outerMargin);
    const float panelHeight = std::clamp(
            static_cast<float>(logicalHeight) * style.heightFraction,
            std::min(style.minHeight, maxPanelHeight),
            std::min(style.maxHeight, maxPanelHeight));
    const Rectangle panel{
            style.outerMargin,
            std::min(style.topMargin,
                    std::max(0.0f, static_cast<float>(logicalHeight) - panelHeight)),
            std::max(40.0f,
                    static_cast<float>(logicalWidth) - style.outerMargin * 2.0f),
            panelHeight};
    const float contentWidth = std::max(
            1.0f, panel.width - style.contentPadding * 2.0f);
    if (console.layoutDirty
            || std::fabs(console.cachedWrapWidth - contentWidth) > 0.5f) {
        RebuildOutputLayout(console, assets, contentWidth);
    }
    if (console.cachedEditorRevision != console.editor.revision
            || std::fabs(console.cachedEditorWrapWidth - contentWidth) > 0.5f) {
        RebuildEditorLayout(console, assets, contentWidth);
    }

    const int editorLineCount = std::max(
            1,
            std::min(
                    style.maxVisibleInputLines,
                    static_cast<int>(console.editorVisualLines.size())));
    const float helpHeight = style.lineHeight + 6.0f;
    const float inputHeight = editorLineCount * style.lineHeight
            + style.contentPadding * 2.0f;
    const float dividerY = panel.y + panel.height - helpHeight - inputHeight;
    const Rectangle outputArea{
            panel.x + style.contentPadding,
            panel.y + style.contentPadding,
            contentWidth,
            std::max(1.0f,
                    dividerY - panel.y - style.contentPadding * 2.0f)};
    const Rectangle inputArea{
            panel.x,
            dividerY,
            panel.width,
            inputHeight};

    DrawRectangleRounded(panel, 0.025f, 8, Color{6, 9, 14, 238});
    DrawRectangleRoundedLinesEx(
            panel, 0.025f, 8, 2.0f, Color{85, 125, 160, 245});
    DrawRectangleRec(inputArea, Color{15, 22, 31, 245});
    DrawLineEx(
            Vector2{panel.x, dividerY},
            Vector2{panel.x + panel.width, dividerY},
            1.0f,
            Color{85, 125, 160, 230});

    const int visibleOutputLines = std::max(
            0, static_cast<int>(std::floor(outputArea.height / style.lineHeight)));
    const int maximumScroll = std::max(
            0, static_cast<int>(console.visualLines.size()) - visibleOutputLines);
    console.scrollOffsetVisualLines = std::clamp(
            console.scrollOffsetVisualLines, 0, maximumScroll);
    const int end = std::max(
            0,
            static_cast<int>(console.visualLines.size())
                    - console.scrollOffsetVisualLines);
    const int begin = std::max(0, end - visibleOutputLines);

    BeginScissorMode(
            static_cast<int>(outputArea.x),
            static_cast<int>(outputArea.y),
            static_cast<int>(outputArea.width),
            static_cast<int>(outputArea.height));
    float outputY = outputArea.y;
    for (int i = begin; i < end; ++i) {
        const DebugConsoleVisualLine& line = console.visualLines[
                static_cast<size_t>(i)];
        const Color color = SeverityColor(line.severity);
        float x = outputArea.x;
        if (!line.prefix.empty()) {
            DrawConsoleText(
                    assets, console.font, line.prefix.c_str(),
                    Vector2{x, outputY}, style.fontSize,
                    style.fontSpacing, color);
            x += TextWidth(
                    assets, console.font, line.prefix.c_str(),
                    style.fontSize, style.fontSpacing);
        }
        DrawConsoleText(
                assets, console.font, line.text.c_str(),
                Vector2{x, outputY}, style.fontSize,
                style.fontSpacing, color);
        outputY += style.lineHeight;
    }
    EndScissorMode();

    const int caretLine = console.cachedCaretVisualLine;
    const int firstEditorLine = std::clamp(
            caretLine - editorLineCount + 1,
            0,
            std::max(0,
                    static_cast<int>(console.editorVisualLines.size())
                            - editorLineCount));
    float inputY = inputArea.y + style.contentPadding;
    for (int i = firstEditorLine;
            i < firstEditorLine + editorLineCount
                    && i < static_cast<int>(console.editorVisualLines.size());
            ++i) {
        const DebugConsoleEditorVisualLine& line =
                console.editorVisualLines[static_cast<size_t>(i)];
        const float x = panel.x + style.contentPadding;
        DrawConsoleText(
                assets, console.font, line.prefix.c_str(),
                Vector2{x, inputY}, style.fontSize,
                style.fontSpacing, RAYWHITE);
        const float textX = x + TextWidth(
                assets, console.font, line.prefix.c_str(),
                style.fontSize, style.fontSpacing);
        DrawConsoleText(
                assets, console.font, line.text.c_str(),
                Vector2{textX, inputY}, style.fontSize,
                style.fontSpacing, RAYWHITE);
        if (i == caretLine && console.editor.caretVisible) {
            const float caretX = x + console.cachedCaretOffsetX;
            DrawRectangle(
                    static_cast<int>(std::round(caretX)),
                    static_cast<int>(std::round(inputY)),
                    2,
                    static_cast<int>(style.fontSize),
                    RAYWHITE);
        }
        inputY += style.lineHeight;
    }

    const char* help =
            "F1 close  |  Enter submit  |  Up/Down history  |  PgUp/PgDn scroll  |  Ctrl+V paste  |  /help";
    DrawConsoleText(
            assets,
            console.font,
            help,
            Vector2{
                    panel.x + style.contentPadding,
                    panel.y + panel.height - helpHeight + 3.0f},
            16.0f,
            1.0f,
            Color{170, 180, 195, 255});
}

} // namespace engine
