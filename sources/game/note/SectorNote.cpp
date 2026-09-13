#include "game/note/SectorNote.h"
#include "game/SectorScriptBindings.h"
#include "game/keypad/SectorKeypad.h"
#include "game/dialogue/SectorDialogue.h"
#include "game/cutscene/SectorCutsceneRuntime.h"
#include "engine/EngineContext.h"
#include "engine/scripting/ScriptSystem.h"
#include "lua.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace game {
namespace {
constexpr std::array<int, 8> FontSizes{14, 18, 24, 32, 40, 48, 64, 80};

void Close(SectorScriptHost& host, const char* reason)
{
    if (!host.note || !host.note->active) return;
    auto& note = *host.note;
    const auto operation = note.operation;
    note.active = false;
    note.closeRequested = false;
    note.owner = {};
    note.operation = {};
    note.opacity = 0;
    if (reason) engine::ScriptSystemFailOperation(*host.scripts, operation, reason);
    else engine::ScriptSystemCompleteOperation(*host.scripts, operation);
    if (host.controls.noteChanged) host.controls.noteChanged(host.controls.userData, false);
}

void CancelBackend(engine::EngineContext&, void* data, uint64_t token)
{
    auto& host = *static_cast<SectorScriptHost*>(data);
    if (host.note && host.note->active && host.note->token == token) Close(host, "cancelled");
}

int Failure(lua_State* state, const char* reason)
{
    lua_pushboolean(state, false);
    lua_pushstring(state, reason);
    return 2;
}

int Begin(lua_State* state)
{
    const int count = lua_gettop(state);
    if (count != 1 && count != 2) return luaL_error(state, "showNote expects body or title, body");
    for (int i = 1; i <= count; ++i) {
        luaL_checktype(state, i, LUA_TSTRING);
        size_t bytes = 0;
        const char* text = lua_tolstring(state, i, &bytes);
        if (std::memchr(text, '\0', bytes)) return luaL_error(state, "showNote text cannot contain NUL bytes");
    }
    const auto owner = engine::ScriptSystemCurrentTaskFromLua(state);
    auto* host = static_cast<SectorScriptHost*>(engine::ScriptSystemHostContextFromLua(state));
    if (!host || !host->scripts || !host->note) return Failure(state, "note runtime is unavailable");
    auto& note = *host->note;
    if (note.active || host->inventoryInteractionActive || host->conversation.active
            || (host->keypad && host->keypad->active) || (host->dialogue && host->dialogue->active)
            || (host->cutscene && host->cutscene->caption.active))
        return Failure(state, "another interaction is active");
    note.token = note.nextToken++;
    note.operation = engine::ScriptSystemCreateOperation(*host->scripts,
            engine::ScriptOperationLaunchStyle::Blocking, owner, "show note", note.token, CancelBackend);
    if (!engine::IsValid(note.operation)) return Failure(state, "note operation is unavailable");
    if ((count == 2 && lua_rawlen(state, 1) > note.title.capacity())
            || lua_rawlen(state, count) > note.body.capacity())
        TraceLog(LOG_WARNING, "[Note] growing text storage for a large note");
    note.title = count == 2 ? lua_tostring(state, 1) : "";
    note.body = lua_tostring(state, count);
    note.owner = owner;
    note.active = true;
    note.closeRequested = false;
    note.layoutDirty = true;
    note.scroll = 0;
    note.phase = SectorNotePhase::Opening;
    note.phaseSeconds = note.opacity = 0;
    // Consume the opening interaction, including when called before input routing.
    auto& context = engine::ScriptSystemEngineFromLua(state);
    for (auto& event : context.input.Events()) {
        if (event.type == engine::InputEventType::KeyPressed && event.key.key == KEY_GRAVE) continue;
        if (event.type == engine::InputEventType::MouseButtonPressed)
            note.swallowedMouseButtons |= 1u << event.mouseButton.button;
        engine::ConsumeEvent(event);
    }
    if (host->controls.holsterWeapon) host->controls.holsterWeapon(host->controls.userData);
    if (host->controls.noteChanged) host->controls.noteChanged(host->controls.userData, true);
    return engine::ScriptSystemYieldForOperation(state, note.operation, count);
}

float GlyphWidth(const Font& font, int codepoint)
{
    if (!font.glyphs || !font.recs || font.glyphCount <= 0) return 0;
    const int index = GetGlyphIndex(font, codepoint);
    const auto& glyph = font.glyphs[index];
    return static_cast<float>(glyph.advanceX ? glyph.advanceX : font.recs[index].width + glyph.offsetX);
}

void Wrap(SectorNoteRuntime& note, const std::string& text, const Font& font, bool title, float& y)
{
    if (text.empty()) return;
    const float lineAdvance = std::ceil(font.baseSize * 1.35f);
    auto line = [&](size_t begin, size_t end) {
        while (end > begin && (text[end-1] == ' ' || text[end-1] == '\t')) --end;
        if (note.lines.size() == note.lines.capacity()) {
            TraceLog(LOG_WARNING, "[Note] growing line layout storage");
            note.lines.reserve(std::max(size_t{1024}, note.lines.capacity()*2));
        }
        note.lines.push_back({note.wrappedText.size(), y, title});
        note.wrappedText.append(text, begin, end-begin);
        note.wrappedText.push_back('\0');
        y += lineAdvance;
    };
    size_t begin = 0, cursor = 0, lastBreak = std::string::npos;
    float width = 0;
    while (cursor < text.size()) {
        if (text[cursor] == '\n' || text[cursor] == '\r') {
            line(begin, cursor);
            const bool crlf = text[cursor] == '\r' && cursor+1 < text.size() && text[cursor+1] == '\n';
            cursor += crlf ? 2 : 1;
            begin = cursor;
            width = 0;
            lastBreak = std::string::npos;
            continue;
        }
        int bytes = 0;
        const int codepoint = GetCodepointNext(text.c_str()+cursor, &bytes);
        const size_t next = std::min(text.size(), cursor + static_cast<size_t>(std::max(1, bytes)));
        const float advance = GlyphWidth(font, codepoint) + (cursor > begin ? 1.0f : 0.0f);
        if (width + advance > note.content.width && cursor > begin) {
            const bool whitespace = text[cursor] == ' ' || text[cursor] == '\t';
            const size_t end = whitespace || lastBreak == std::string::npos ? cursor : lastBreak;
            line(begin, end);
            begin = end;
            while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t')) ++begin;
            cursor = begin;
            width = 0;
            lastBreak = std::string::npos;
            continue;
        }
        if ((text[cursor] == ' ' || text[cursor] == '\t') && cursor > begin) lastBreak = cursor;
        width += advance;
        cursor = next;
    }
    line(begin, text.size());
}

size_t ChooseFont(float desired)
{
    size_t index = 0;
    for (size_t i = 1; i < FontSizes.size(); ++i)
        if (FontSizes[i] <= desired) index = i;
    return index;
}

Color Alpha(Color color, float opacity)
{
    color.a = static_cast<unsigned char>(std::round(color.a * opacity));
    return color;
}
} // namespace

void LoadSectorNote(engine::AssetManager& assets, SectorNoteRuntime& note,
        const std::filesystem::path& assetRoot)
{
    UnloadSectorNote(assets, note);
    note.scope = assets.CreateScope("level_note");
    const auto path = (assetRoot / "ui/notes/paper.png").string();
    note.paper = assets.RequestTexture(note.scope, path.c_str(), path.c_str(),
            engine::TextureColorUsage::DisplaySrgb, engine::TextureLoad_BilinearFilter);
    const auto font = (assetRoot / "fonts/Inconsolata.otf").string();
    for (size_t i = 0; i < FontSizes.size(); ++i) {
        const auto key = "note_font_" + std::to_string(FontSizes[i]);
        note.fonts[i] = assets.RequestFont(note.scope, key.c_str(), font.c_str(), FontSizes[i]);
    }
    note.title.reserve(1024);
    note.body.reserve(16384);
    note.wrappedText.reserve(32768);
    note.lines.reserve(1024);
}

void UnloadSectorNote(engine::AssetManager& assets, SectorNoteRuntime& note)
{
    if (!engine::IsNull(note.scope)) assets.UnloadScope(note.scope);
    note = {};
}

void LayoutSectorNote(SectorNoteRuntime& note, Rectangle viewport, const Font& body, const Font& title)
{
    if (!note.layoutDirty && note.viewport.x == viewport.x && note.viewport.y == viewport.y
            && note.viewport.width == viewport.width && note.viewport.height == viewport.height
            && note.bodySize == body.baseSize && note.titleSize == title.baseSize) return;
    note.viewport = viewport;
    note.bodySize = static_cast<float>(body.baseSize);
    note.titleSize = static_cast<float>(title.baseSize);
    const float height = std::max(0.0f, std::min(viewport.height * 0.85f, (viewport.width-32) * 1.5f));
    note.panel = {viewport.x + (viewport.width-height/1.5f)*0.5f,
            viewport.y + (viewport.height-height)*0.5f-10, height/1.5f, height};
    const float margin = note.panel.width * 0.09f;
    note.content = {note.panel.x + margin, note.panel.y + margin,
            std::max(0.0f, note.panel.width-2*margin-10), std::max(0.0f, height-2*margin)};
    note.lines.clear();
    note.wrappedText.clear();
    // Grow only when opening/resizing unusually large notes, never on steady draw.
    const size_t estimatedBytes = 2*(note.title.size() + note.body.size()) + 4;
    if (estimatedBytes > note.wrappedText.capacity()) {
        TraceLog(LOG_WARNING, "[Note] growing text layout storage for a large note");
        note.wrappedText.reserve(estimatedBytes);
    }
    float y = 0;
    Wrap(note, note.title, title, true, y);
    if (!note.title.empty()) y += std::ceil(body.baseSize * 0.7f);
    Wrap(note, note.body, body, false, y);
    note.contentHeight = y + body.baseSize * 0.5f;
    note.maxScroll = std::max(0.0f, note.contentHeight-note.content.height);
    note.scroll = std::clamp(note.scroll, 0.0f, note.maxScroll);
    note.layoutDirty = false;
}

void CancelSectorNote(SectorScriptHost& host, const char* reason) { Close(host, reason); }

void UpdateSectorNote(engine::EngineContext& context, SectorScriptHost& host, float dt)
{
    if (!host.note) return;
    auto& note = *host.note;
    for (auto& event : context.input.Events()) {
        if (!event.handled && event.type == engine::InputEventType::MouseButtonPressed)
            note.swallowedMouseButtons &= ~(1u << event.mouseButton.button);
        if ((event.type == engine::InputEventType::MouseButtonReleased && (note.swallowedMouseButtons & (1u << event.mouseButton.button)))
                || (event.type == engine::InputEventType::MouseClick && (note.swallowedMouseButtons & (1u << event.mouseClick.button))))
            engine::ConsumeEvent(event);
    }
    if (!note.active) return;
    if (!engine::ScriptSystemIsTaskActive(*host.scripts, note.owner)) { Close(host, "task ended"); return; }
    for (auto& event : context.input.Events()) {
        if (event.handled) continue;
        if (event.type == engine::InputEventType::KeyPressed || event.type == engine::InputEventType::KeyRepeated) {
            const int key = event.key.key;
            if (key == KEY_GRAVE) continue;
            if (event.type == engine::InputEventType::KeyPressed && (key == KEY_E || key == KEY_ESCAPE))
                note.closeRequested = true;
            if (key == KEY_UP) note.scroll -= note.bodySize * 3;
            if (key == KEY_DOWN) note.scroll += note.bodySize * 3;
            if (key == KEY_PAGE_UP) note.scroll -= note.content.height * 0.9f;
            if (key == KEY_PAGE_DOWN) note.scroll += note.content.height * 0.9f;
            if (key == KEY_HOME) note.scroll = 0;
            if (key == KEY_END) note.scroll = note.maxScroll;
        } else if (event.type == engine::InputEventType::MouseWheel) {
            note.scroll -= event.wheel.value * note.bodySize * 3;
        } else if (event.type == engine::InputEventType::MouseButtonPressed) {
            note.swallowedMouseButtons |= 1u << event.mouseButton.button;
        }
        engine::ConsumeEvent(event);
    }
    note.scroll = std::clamp(note.scroll, 0.0f, note.maxScroll);
    if (note.closeRequested && note.phase != SectorNotePhase::Closing) {
        note.phase = SectorNotePhase::Closing;
        note.phaseSeconds = 0;
        note.closingOpacity = note.opacity;
        return; // Start the full 350 ms transition at the current visible opacity.
    }
    note.phaseSeconds += std::max(0.0f, dt);
    const float t = std::min(1.0f, note.phaseSeconds / SectorNoteFadeSeconds);
    const float eased = t*t*(3-2*t);
    if (note.phase == SectorNotePhase::Opening) {
        note.opacity = eased;
        if (t >= 1) note.phase = SectorNotePhase::Reading;
    } else if (note.phase == SectorNotePhase::Closing) {
        note.opacity = note.closingOpacity * (1-eased);
        if (t >= 1) Close(host, nullptr);
    }
}

void DrawSectorNote(const engine::AssetManager& assets, SectorNoteRuntime& note, Rectangle viewport)
{
    if (!note.active) return;
    const float scale = std::max(0.0f, std::min(viewport.height*0.85f, (viewport.width-32)*1.5f)) / 900.0f;
    note.bodyFont = ChooseFont(24*scale);
    note.titleFont = std::min(note.fonts.size()-1, std::max(note.bodyFont+1, ChooseFont(36*scale)));
    const auto* bodyAsset = assets.GetFont(note.fonts[note.bodyFont]);
    const auto* titleAsset = assets.GetFont(note.fonts[note.titleFont]);
    const Font body = bodyAsset ? bodyAsset->font : GetFontDefault();
    const Font title = titleAsset ? titleAsset->font : GetFontDefault();
    LayoutSectorNote(note, viewport, body, title);
    DrawRectangleRec(viewport, Alpha({0, 0, 0, 166}, note.opacity));
    const auto* paper = assets.GetTexture(note.paper);
    if (paper) {
        const Rectangle source{0, 0, static_cast<float>(paper->width), static_cast<float>(paper->height)};
        Rectangle shadow = note.panel;
        shadow.x += 5; shadow.y += 8;
        DrawTexturePro(*paper, source, shadow, {}, 0, Alpha({0, 0, 0, 80}, note.opacity));
        DrawTexturePro(*paper, source, note.panel, {}, 0, Alpha(WHITE, note.opacity));
    } else {
        DrawRectangleRec(note.panel, Alpha({238, 228, 205, 255}, note.opacity));
    }
    BeginScissorMode(static_cast<int>(std::ceil(note.content.x)), static_cast<int>(std::ceil(note.content.y)),
            static_cast<int>(std::floor(note.content.width)), static_cast<int>(std::floor(note.content.height)));
    for (const auto& line : note.lines) {
        const Font& font = line.title ? title : body;
        const float y = note.content.y + line.y-note.scroll;
        if (y + font.baseSize < note.content.y || y > note.content.y+note.content.height) continue;
        DrawTextEx(font, note.wrappedText.c_str()+line.offset, {std::round(note.content.x), std::round(y)},
                static_cast<float>(font.baseSize), 1, Alpha({48, 39, 30, 255}, note.opacity));
    }
    EndScissorMode();
    if (note.maxScroll > 0) {
        const float height = std::max(12.0f, note.content.height*note.content.height/note.contentHeight);
        const float y = note.content.y+(note.content.height-height)*note.scroll/note.maxScroll;
        DrawRectangleRec({note.content.x+note.content.width+5, y, 3, height}, Alpha({90, 73, 51, 110}, note.opacity));
    }
    const auto* helpAsset = assets.GetFont(note.fonts[0]);
    const Font help = helpAsset ? helpAsset->font : GetFontDefault();
    const char* hint = note.maxScroll > 0 ? "Wheel / Arrows: Scroll    E / Esc: Close" : "E / Esc: Close";
    const Vector2 size = MeasureTextEx(help, hint, static_cast<float>(help.baseSize), 1);
    DrawTextEx(help, hint, {std::round(viewport.x+(viewport.width-size.x)/2), note.panel.y+note.panel.height+6},
            static_cast<float>(help.baseSize), 1, Alpha({220, 216, 204, 255}, note.opacity));
}

void RegisterSectorNoteBindings(lua_State* state)
{
    constexpr const char* wrapper = R"lua(
        local begin = ...
        function showNote(...)
            local ok, reason = begin(...)
            if not ok then return nil, reason end
            return true
        end
    )lua";
    if (luaL_loadstring(state, wrapper) != LUA_OK) lua_error(state);
    lua_pushcfunction(state, Begin);
    if (lua_pcall(state, 1, 0, 0) != LUA_OK) lua_error(state);
}
} // namespace game
