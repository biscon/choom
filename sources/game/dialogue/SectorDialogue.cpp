#include "game/dialogue/SectorDialogue.h"

#include "engine/scripting/ScriptSystem.h"
#include "util/json.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace game {
namespace {
constexpr size_t MaximumLabelBytes = 8192;
constexpr float Padding = 12.0f;

bool AdvancePress(const engine::InputEvent& event)
{
    return (event.type == engine::InputEventType::KeyPressed && event.key.key == KEY_ENTER)
            || (event.type == engine::InputEventType::MouseButtonPressed
                    && (event.mouseButton.button == MOUSE_BUTTON_LEFT
                            || event.mouseButton.button == MOUSE_BUTTON_RIGHT));
}

void SwallowMouseTail(SectorDialogueRuntime& runtime, engine::InputEvent& event)
{
    if (event.type == engine::InputEventType::MouseButtonReleased) {
        if (runtime.swallowedButtons & (1u << event.mouseButton.button))
            engine::ConsumeEvent(event);
    } else if (event.type == engine::InputEventType::MouseClick) {
        if (runtime.swallowedButtons & (1u << event.mouseClick.button))
            engine::ConsumeEvent(event);
    } else if (event.type == engine::InputEventType::MouseButtonPressed) {
        runtime.swallowedButtons &= ~(1u << event.mouseButton.button);
    }
}

void RememberPress(SectorDialogueRuntime& runtime, const engine::InputEvent& event)
{
    if (event.type == engine::InputEventType::MouseButtonPressed)
        runtime.swallowedButtons |= 1u << event.mouseButton.button;
}

float GlyphWidth(const Font& font, int codepoint, float size)
{
    const int index = GetGlyphIndex(font, codepoint);
    const float advance = font.glyphs[index].advanceX != 0
            ? static_cast<float>(font.glyphs[index].advanceX) : font.recs[index].width;
    return advance * size / static_cast<float>(font.baseSize) + 1.0f;
}

void ClampScroll(SectorDialogueRuntime& runtime)
{
    runtime.scroll = std::clamp(runtime.scroll, 0.0f,
            std::max(0.0f, runtime.contentHeight - runtime.panel.height + 2 * Padding));
}
}

void ResetSectorDialogueMenu(SectorDialogueRuntime& runtime)
{
    runtime.active = false;
    runtime.operation = {};
    runtime.visible.clear();
    runtime.rows.clear();
    runtime.lines.clear();
    runtime.layoutReady = false;
    runtime.scroll = 0.0f;
}

void LoadSectorDialogue(SectorDialogueRuntime& runtime, const std::filesystem::path& directory)
{
    runtime = {};
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec) && !ec) return;
    std::vector<std::filesystem::path> paths;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".json") paths.push_back(it->path());
    }
    if (ec) {
        TraceLog(LOG_WARNING, "[Dialogue] cannot read %s: %s", directory.string().c_str(), ec.message().c_str());
        return;
    }
    std::sort(paths.begin(), paths.end());
    std::unordered_set<std::string> loadedIds;
    size_t maximumOptions = 0;
    size_t maximumBytes = 0;
    for (const auto& path : paths) {
        try {
            std::ifstream input(path);
            const auto json = nlohmann::ordered_json::parse(input);
            if (!json.at("choiceSets").is_array()) throw std::runtime_error("choiceSets must be an array");
            std::vector<SectorDialogueSet> pending;
            auto ids = loadedIds;
            for (const auto& rawSet : json.at("choiceSets")) {
                SectorDialogueSet set;
                set.id = rawSet.at("id").get<std::string>();
                if (set.id.empty() || set.id.find('\0') != std::string::npos || !ids.insert(set.id).second)
                    throw std::runtime_error("empty or duplicate choice set ID: " + set.id);
                const auto& options = rawSet.at("options");
                if (!options.is_array() || options.empty()) throw std::runtime_error("options must be a nonempty array: " + set.id);
                std::unordered_set<std::string> optionIds;
                for (const auto& rawOption : options) {
                    SectorDialogueOption option{rawOption.at("id").get<std::string>(), rawOption.at("text").get<std::string>()};
                    if (option.id.empty() || option.id.find('\0') != std::string::npos || !optionIds.insert(option.id).second)
                        throw std::runtime_error("empty or duplicate option ID in " + set.id);
                    if (option.text.find_first_not_of(" \t\r\n") == std::string::npos
                            || option.text.size() > MaximumLabelBytes || option.text.find('\0') != std::string::npos)
                        throw std::runtime_error("empty or oversized option text in " + set.id);
                    set.options.push_back(std::move(option));
                }
                pending.push_back(std::move(set));
            }
            for (auto& set : pending) {
                maximumOptions = std::max(maximumOptions, set.options.size());
                size_t bytes = 0;
                for (const auto& option : set.options) bytes += option.text.size() + 1;
                maximumBytes = std::max(maximumBytes, bytes);
                runtime.sets.push_back(std::move(set));
            }
            loadedIds = std::move(ids);
        } catch (const std::exception& error) {
            TraceLog(LOG_WARNING, "[Dialogue] rejected %s: %s", path.string().c_str(), error.what());
        }
    }
    runtime.visible.reserve(maximumOptions);
    runtime.rows.reserve(maximumOptions);
    runtime.lines.reserve(maximumBytes);
}

bool BeginSectorDialogue(SectorDialogueRuntime& runtime, const std::string& setId,
        const std::vector<std::string>& hidden, std::string& error)
{
    if (runtime.active) { error = "a dialogue menu is already active"; return false; }
    const auto found = std::find_if(runtime.sets.begin(), runtime.sets.end(),
            [&](const auto& set) { return set.id == setId; });
    if (found == runtime.sets.end()) { error = "dialogue choice set was not found: " + setId; return false; }
    runtime.visible.clear();
    for (size_t i = 0; i < found->options.size(); ++i) {
        if (std::find(hidden.begin(), hidden.end(), found->options[i].id) == hidden.end()) runtime.visible.push_back(i);
    }
    if (runtime.visible.empty()) { error = "dialogue has no visible options"; return false; }
    runtime.setIndex = static_cast<size_t>(found - runtime.sets.begin());
    runtime.selected = 0;
    runtime.scroll = 0.0f;
    runtime.token = runtime.nextToken++;
    runtime.active = true;
    runtime.layoutReady = false;
    runtime.ensureSelectionVisible = true;
    error.clear();
    return true;
}

bool SelectSectorDialogue(SectorDialogueRuntime& runtime, engine::ScriptRuntime& scripts, size_t index)
{
    if (!runtime.active || index >= runtime.visible.size()) return false;
    const auto operation = runtime.operation;
    const std::string result = runtime.sets[runtime.setIndex].options[runtime.visible[index]].id;
    ResetSectorDialogueMenu(runtime);
    engine::ScriptSystemCompleteOperation(scripts, operation, {result});
    return true;
}

void LayoutSectorDialogue(SectorDialogueRuntime& runtime, const Font& font,
        int pixelSize, Rectangle viewport, float preferredTop)
{
    if (!runtime.active || font.glyphs == nullptr || font.baseSize <= 0 || viewport.width <= 0 || viewport.height <= 0) return;
    const bool rebuild = !runtime.layoutReady || runtime.fontTexture != font.texture.id
            || runtime.fontSize != pixelSize || runtime.layoutViewport.width != viewport.width
            || runtime.layoutViewport.height != viewport.height;
    if (rebuild) {
        runtime.rows.clear();
        runtime.lines.clear();
        runtime.lineHeight = static_cast<float>(pixelSize) + 8.0f;
        char widestNumber[32];
        std::snprintf(widestNumber, sizeof(widestNumber), "%zu. ", runtime.visible.size());
        runtime.numberWidth = MeasureTextEx(font, widestNumber, static_cast<float>(pixelSize), 1).x + 8.0f;
        runtime.panel.width = viewport.width * 0.8f;
        const float width = std::max(1.0f, runtime.panel.width - 2 * Padding - runtime.numberWidth - 8.0f);
        float top = 0;
        for (const size_t index : runtime.visible) {
            const auto& text = runtime.sets[runtime.setIndex].options[index].text;
            SectorDialogueRow row{top, 0, runtime.lines.size(), 0};
            size_t start = 0;
            while (start < text.size()) {
                size_t cursor = start, lastSpace = std::string::npos;
                float measured = 0;
                while (cursor < text.size() && text[cursor] != '\n') {
                    int bytes = 0;
                    const int cp = GetCodepointNext(text.c_str() + cursor, &bytes);
                    const float advance = GlyphWidth(font, cp, static_cast<float>(pixelSize));
                    if (measured + advance > width && cursor > start) break;
                    measured += advance;
                    if (text[cursor] == ' ' || text[cursor] == '\t') lastSpace = cursor;
                    cursor += static_cast<size_t>(std::max(1, bytes));
                }
                size_t end = cursor;
                if (cursor < text.size() && text[cursor] != '\n' && lastSpace != std::string::npos && lastSpace > start) end = lastSpace;
                runtime.lines.push_back({start, end});
                start = end;
                if (start < text.size() && text[start] == '\n') ++start;
                else while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) ++start;
            }
            row.lineCount = runtime.lines.size() - row.firstLine;
            row.height = static_cast<float>(row.lineCount) * runtime.lineHeight + Padding;
            top += row.height;
            runtime.rows.push_back(row);
        }
        runtime.contentHeight = top;
        runtime.panel.height = std::min(top + Padding * 2, viewport.height * 0.65f);
        runtime.fontTexture = font.texture.id;
        runtime.fontSize = pixelSize;
        runtime.layoutReady = true;
    }
    runtime.layoutViewport = viewport;
    runtime.panel.x = viewport.x + (viewport.width - runtime.panel.width) * 0.5f;
    runtime.panel.y = std::clamp(preferredTop, viewport.y + Padding,
            std::max(viewport.y + Padding, viewport.y + viewport.height - runtime.panel.height - Padding));
    if (runtime.ensureSelectionVisible && runtime.selected < runtime.rows.size()) {
        const auto& row = runtime.rows[runtime.selected];
        const float visibleHeight = runtime.panel.height - 2 * Padding;
        if (row.top < runtime.scroll) runtime.scroll = row.top;
        else if (row.top + row.height > runtime.scroll + visibleHeight)
            runtime.scroll = row.top + std::min(row.height, visibleHeight) - visibleHeight;
        runtime.ensureSelectionVisible = false;
    }
    ClampScroll(runtime);
}

void DrawSectorDialogue(const SectorDialogueRuntime& runtime, const Font& font)
{
    if (!runtime.active || !runtime.layoutReady) return;
    DrawRectangleRec(runtime.panel, Color{12, 15, 20, 225});
    BeginScissorMode(static_cast<int>(runtime.panel.x), static_cast<int>(runtime.panel.y + Padding),
            static_cast<int>(runtime.panel.width), std::max(0, static_cast<int>(runtime.panel.height - 2 * Padding)));
    std::array<char, MaximumLabelBytes + 1> buffer{};
    for (size_t i = 0; i < runtime.rows.size(); ++i) {
        const auto& row = runtime.rows[i];
        const float y = runtime.panel.y + Padding + row.top - runtime.scroll;
        if (y + row.height < runtime.panel.y || y > runtime.panel.y + runtime.panel.height) continue;
        const bool selected = runtime.selected == i;
        if (selected) DrawRectangleRec({runtime.panel.x + 4, y - 2, runtime.panel.width - 12, row.height}, Color{60, 70, 84, 230});
        const Color color = selected ? Color{255, 225, 155, 255} : Color{245, 245, 240, 255};
        char number[32];
        std::snprintf(number, sizeof(number), "%zu.", i + 1);
        DrawTextEx(font, number, {runtime.panel.x + Padding, y}, static_cast<float>(runtime.fontSize), 1, color);
        const auto& text = runtime.sets[runtime.setIndex].options[runtime.visible[i]].text;
        for (size_t n = 0; n < row.lineCount; ++n) {
            const auto& line = runtime.lines[row.firstLine + n];
            const size_t length = line.end - line.begin;
            std::memcpy(buffer.data(), text.data() + line.begin, length);
            buffer[length] = '\0';
            DrawTextEx(font, buffer.data(), {runtime.panel.x + Padding + runtime.numberWidth,
                    y + static_cast<float>(n) * runtime.lineHeight}, static_cast<float>(runtime.fontSize), 1, color);
        }
    }
    EndScissorMode();
    const float viewHeight = runtime.panel.height - 2 * Padding;
    if (runtime.contentHeight > viewHeight && viewHeight > 0) {
        const float thumb = std::max(8.0f, viewHeight * viewHeight / runtime.contentHeight);
        const float y = runtime.panel.y + Padding + runtime.scroll / (runtime.contentHeight - viewHeight) * (viewHeight - thumb);
        DrawRectangleRec({runtime.panel.x + runtime.panel.width - 6, y, 3, thumb}, Color{190, 195, 205, 230});
    }
}

void UpdateSectorDialogueInput(SectorDialogueRuntime& runtime, engine::ScriptRuntime& scripts,
        engine::Input& input, Rectangle inputViewport)
{
    if (!runtime.active) return;
    // HUD text uses physical presentation pixels; Input uses the logical UI
    // viewport (including its letterbox offset/scale configured by Main).
    auto presentationPoint = [&](Vector2 position) {
        if (inputViewport.width > 0 && inputViewport.height > 0) {
            position.x = runtime.layoutViewport.x + (position.x - inputViewport.x)
                    * runtime.layoutViewport.width / inputViewport.width;
            position.y = runtime.layoutViewport.y + (position.y - inputViewport.y)
                    * runtime.layoutViewport.height / inputViewport.height;
        }
        return position;
    };
    const Vector2 mouse = presentationPoint(input.MousePosition());
    auto pick = [&](Vector2 position) -> size_t {
        const Rectangle inner{runtime.panel.x, runtime.panel.y + Padding, runtime.panel.width, runtime.panel.height - 2 * Padding};
        if (!runtime.layoutReady || !CheckCollisionPointRec(position, inner)) return runtime.visible.size();
        const float y = position.y - runtime.panel.y - Padding + runtime.scroll;
        for (size_t i = 0; i < runtime.rows.size(); ++i)
            if (y >= runtime.rows[i].top && y < runtime.rows[i].top + runtime.rows[i].height) return i;
        return runtime.visible.size();
    };
    if (mouse.x != runtime.previousMouse.x || mouse.y != runtime.previousMouse.y) {
        const size_t index = pick(mouse);
        if (index < runtime.visible.size()) runtime.selected = index;
    }
    runtime.previousMouse = mouse;
    size_t choice = runtime.visible.size();
    for (auto& event : input.Events()) {
        SwallowMouseTail(runtime, event);
        if (event.handled) continue;
        if (event.type == engine::InputEventType::KeyPressed || event.type == engine::InputEventType::KeyRepeated) {
            const int key = event.key.key;
            if (key == KEY_ESCAPE || key == KEY_GRAVE) continue;
            if (key == KEY_UP || key == KEY_DOWN) {
                const size_t count = runtime.visible.size();
                runtime.selected = (runtime.selected + (key == KEY_UP ? count - 1 : 1)) % count;
                runtime.ensureSelectionVisible = true;
            } else if (event.type == engine::InputEventType::KeyPressed && choice == runtime.visible.size()) {
                if (key == KEY_ENTER) choice = runtime.selected;
                else if (key >= KEY_ONE && key <= KEY_NINE && static_cast<size_t>(key - KEY_ONE) < runtime.visible.size()) choice = static_cast<size_t>(key - KEY_ONE);
            }
            engine::ConsumeEvent(event);
        } else if (event.type == engine::InputEventType::MouseButtonPressed) {
            if (AdvancePress(event) && choice == runtime.visible.size()) choice = pick(presentationPoint(event.mouseButton.position));
            RememberPress(runtime, event);
            engine::ConsumeEvent(event);
        } else if (event.type == engine::InputEventType::MouseWheel) {
            runtime.scroll -= event.wheel.value * runtime.lineHeight * 3;
            ClampScroll(runtime);
            engine::ConsumeEvent(event);
        } else if (event.type == engine::InputEventType::MouseClick || event.type == engine::InputEventType::MouseButtonReleased) {
            engine::ConsumeEvent(event);
        }
    }
    if (choice < runtime.visible.size()) SelectSectorDialogue(runtime, scripts, choice);
}

bool ConsumeSectorSpeechAdvance(SectorDialogueRuntime& runtime, engine::Input& input, bool speechActive)
{
    bool advance = false;
    for (auto& event : input.Events()) {
        SwallowMouseTail(runtime, event);
        if (!event.handled && speechActive && AdvancePress(event)) {
            advance = true;
            RememberPress(runtime, event);
            engine::ConsumeEvent(event);
        }
    }
    return advance;
}
} // namespace game
