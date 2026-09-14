#include "game/keypad/SectorKeypad.h"
#include "game/SectorScriptBindings.h"
#include "game/note/SectorNote.h"
#include "game/cutscene/SectorCutsceneRuntime.h"
#include "game/dialogue/SectorDialogue.h"
#include "engine/EngineContext.h"
#include "engine/scripting/ScriptSystem.h"
#include "lua.hpp"
#include "util/json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace game {
namespace {
constexpr std::array<int, 6> FontSizes{10, 14, 18, 24, 32, 40};
using Json = nlohmann::ordered_json;

Rectangle ReadRect(const Json& raw)
{
    if (!raw.is_array() || raw.size() != 4) throw std::runtime_error("rectangle requires four numbers");
    Rectangle r{raw[0].get<float>(), raw[1].get<float>(), raw[2].get<float>(), raw[3].get<float>()};
    if (!std::isfinite(r.x) || !std::isfinite(r.y) || !std::isfinite(r.width) || !std::isfinite(r.height)
            || r.x < 0 || r.y < 0 || r.width <= 0 || r.height <= 0
            || r.x + r.width > 1 || r.y + r.height > 1)
        throw std::runtime_error("rectangle must fit within the image");
    return r;
}

Vector3 ReadColor(const Json& raw)
{
    if (!raw.is_array() || raw.size() != 3) throw std::runtime_error("colour requires three numbers");
    Vector3 c{raw[0].get<float>(), raw[1].get<float>(), raw[2].get<float>()};
    for (float value : {c.x, c.y, c.z})
        if (!std::isfinite(value) || value < 0 || value > 1) throw std::runtime_error("colour must be in 0..1");
    return c;
}

SectorScriptHost& Host(lua_State* state)
{
    auto* host = static_cast<SectorScriptHost*>(engine::ScriptSystemHostContextFromLua(state));
    if (!host || !host->scripts) luaL_error(state, "sector script host is unavailable");
    return *host;
}

int Failure(lua_State* state, const char* reason)
{
    lua_pushboolean(state, false);
    lua_pushstring(state, reason);
    return 2;
}

void PlayTone(engine::EngineContext& context, const SectorKeypadRuntime& runtime, size_t tone)
{
    if (runtime.skinIndex >= runtime.skins.size()) return;
    engine::SoundPlaybackSettings settings;
    settings.volume = 0.45f;
    settings.affectedByListenerEffects = false;
    context.audio.PlaySound(context.assets, runtime.skins[runtime.skinIndex].sounds[tone], settings);
}

void Close(SectorScriptHost& host, const char* reason)
{
    if (!host.keypad || !host.keypad->active) return;
    auto& runtime = *host.keypad;
    const auto lifetime = runtime.lifetime, input = runtime.inputOperation;
    runtime.active = false;
    runtime.cancelRequested = false;
    runtime.lifetime = runtime.inputOperation = {};
    runtime.owner = {};
    runtime.entry = {};
    runtime.length = 0;
    // Clear ownership before completing operations, including cancellation paths.
    engine::ScriptSystemFailOperation(*host.scripts, input, reason);
    engine::ScriptSystemCompleteOperation(*host.scripts, lifetime);
    if (host.controls.keypadChanged) host.controls.keypadChanged(host.controls.userData, false);
}

void CancelBackend(engine::EngineContext&, void* data, uint64_t token)
{
    auto& host = *static_cast<SectorScriptHost*>(data);
    if (host.keypad && host.keypad->active && host.keypad->token == token) Close(host, "cancelled");
}

bool Owns(lua_State* state, const SectorKeypadRuntime& runtime)
{
    return runtime.active && lua_isinteger(state, 1)
            && static_cast<uint64_t>(lua_tointeger(state, 1)) == runtime.token
            && runtime.owner == engine::ScriptSystemCurrentTaskFromLua(state);
}

int Begin(lua_State* state)
{
    const auto owner = engine::ScriptSystemCurrentTaskFromLua(state);
    auto& host = Host(state);
    luaL_checktype(state, 1, LUA_TTABLE);
    lua_getfield(state, 1, "skin");
    if (lua_type(state, -1) != LUA_TSTRING) return Failure(state, "skin must be a string");
    size_t bytes = 0;
    const char* id = lua_tolstring(state, -1, &bytes);
    lua_getfield(state, 1, "digits");
    if (!lua_isinteger(state, -1) || lua_tointeger(state, -1) < 1 || lua_tointeger(state, -1) > 8)
        return Failure(state, "digits must be an integer from 1 to 8");
    const int digits = static_cast<int>(lua_tointeger(state, -1));
    if (!host.keypad) return Failure(state, "keypad runtime is unavailable");
    auto& runtime = *host.keypad;
    if (runtime.active || (host.note && host.note->active) || host.inventoryInteractionActive || host.conversation.active
            || (host.dialogue && host.dialogue->active)
            || (host.cutscene && host.cutscene->caption.active))
        return Failure(state, "another interaction is active");
    const auto skin = std::find_if(runtime.skins.begin(), runtime.skins.end(),
            [&](const auto& candidate) { return candidate.id.size() == bytes && candidate.id.compare(0, bytes, id, bytes) == 0; });
    if (skin == runtime.skins.end()) return Failure(state, "unknown keypad skin");
    if (!skin->ready) return Failure(state, "keypad assets are unavailable");
    Vector3 color = skin->indicatorColor;
    lua_getfield(state, 1, "indicatorColor");
    if (!lua_isnil(state, -1)) {
        if (!lua_istable(state, -1) || lua_rawlen(state, -1) != 3)
            return Failure(state, "indicatorColor requires three linear RGB values");
        float rgb[3];
        for (int i = 0; i < 3; ++i) {
            lua_rawgeti(state, -1, i + 1);
            if (lua_type(state, -1) != LUA_TNUMBER) return Failure(state, "indicatorColor must be numeric");
            rgb[i] = static_cast<float>(lua_tonumber(state, -1));
            lua_pop(state, 1);
            if (!std::isfinite(rgb[i]) || rgb[i] < 0 || rgb[i] > 1)
                return Failure(state, "indicatorColor must be finite and in 0..1");
        }
        color = {rgb[0], rgb[1], rgb[2]};
    }
    runtime.token = runtime.nextToken++;
    runtime.lifetime = engine::ScriptSystemCreateOperation(*host.scripts,
            engine::ScriptOperationLaunchStyle::Async, owner, "keypad session", runtime.token, CancelBackend);
    if (!engine::IsValid(runtime.lifetime)) return Failure(state, "could not allocate keypad operation");
    runtime.skinIndex = static_cast<size_t>(skin - runtime.skins.begin());
    runtime.owner = owner;
    runtime.digits = digits;
    runtime.indicatorColor = color;
    runtime.active = true;
    runtime.cancelRequested = false;
    runtime.entry = {};
    runtime.length = 0;
    runtime.errorSeconds = runtime.pressedSeconds = 0;
    runtime.pressedAction = runtime.hoverAction = -1;
    if (host.controls.holsterWeapon) host.controls.holsterWeapon(host.controls.userData);
    if (host.controls.keypadChanged) host.controls.keypadChanged(host.controls.userData, true);
    lua_pushboolean(state, true);
    lua_pushinteger(state, static_cast<lua_Integer>(runtime.token));
    return 2;
}

int NextInput(lua_State* state)
{
    const int top = lua_gettop(state);
    auto& host = Host(state);
    if (!host.keypad || !Owns(state, *host.keypad)) return Failure(state, "keypad session ended");
    auto& runtime = *host.keypad;
    runtime.inputOperation = engine::ScriptSystemCreateOperation(*host.scripts,
            engine::ScriptOperationLaunchStyle::Blocking, runtime.owner, "keypad input", runtime.token, CancelBackend);
    if (!engine::IsValid(runtime.inputOperation)) { Close(host, "operation unavailable"); return Failure(state, "operation unavailable"); }
    return engine::ScriptSystemYieldForOperation(state, runtime.inputOperation, top);
}

int Reject(lua_State* state)
{
    auto& host = Host(state);
    if (host.keypad && Owns(state, *host.keypad)) {
        host.keypad->errorSeconds = 0.65f;
        host.keypad->entry = {};
        host.keypad->length = 0;
        PlayTone(engine::ScriptSystemEngineFromLua(state), *host.keypad, 1);
    }
    return 0;
}

int Finish(lua_State* state)
{
    auto& host = Host(state);
    if (host.keypad && Owns(state, *host.keypad)) {
        if (lua_toboolean(state, 2)) PlayTone(engine::ScriptSystemEngineFromLua(state), *host.keypad, 2);
        Close(host, "cancelled");
    }
    return 0;
}

int Validate(lua_State* state)
{
    luaL_checktype(state, 1, LUA_TFUNCTION);
    lua_settop(state, 2);
    // Non-continuation pcall deliberately prohibits yielding from validation.
    if (lua_pcall(state, 1, 1, 0) != LUA_OK) return lua_error(state);
    if (!lua_isboolean(state, -1)) return luaL_error(state, "keypad validator must return boolean");
    return 1;
}
} // namespace

void LoadSectorKeypads(engine::AssetManager& assets, SectorKeypadRuntime& runtime,
        const std::filesystem::path& assetRoot)
{
    UnloadSectorKeypads(assets, runtime);
    runtime.scope = assets.CreateScope("level_keypads");
    const auto directory = assetRoot / "keypads";
    std::vector<std::filesystem::path> paths;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec))
        if (it->path().extension() == ".json") paths.push_back(it->path());
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
        try {
            std::ifstream file(path);
            const auto json = Json::parse(file);
            SectorKeypadSkin skin;
            skin.id = json.at("id").get<std::string>();
            if (skin.id.empty() || skin.id.find('\0') != std::string::npos
                    || std::any_of(runtime.skins.begin(), runtime.skins.end(), [&](const auto& s) { return s.id == skin.id; }))
                throw std::runtime_error("empty or duplicate skin ID");
            skin.display = ReadRect(json.at("display"));
            skin.indicator = ReadRect(json.at("indicator"));
            skin.indicatorColor = ReadColor(json.at("indicatorColor"));
            const auto color = json.at("displayColor").get<std::array<int, 3>>();
            for (int c : color) if (c < 0 || c > 255) throw std::runtime_error("invalid display colour");
            skin.displayColor = {static_cast<unsigned char>(color[0]), static_cast<unsigned char>(color[1]), static_cast<unsigned char>(color[2]), 255};
            const auto& buttons = json.at("buttons");
            if (!buttons.is_array() || buttons.size() != 12) throw std::runtime_error("skin requires twelve buttons");
            std::array<bool, 12> seen{};
            size_t index = 0;
            for (const auto& button : buttons) {
                const auto action = button.at("action").get<std::string>();
                const int value = action == "backspace" ? KeypadBackspace : action == "submit" ? KeypadSubmit
                        : action.size() == 1 && action[0] >= '0' && action[0] <= '9' ? action[0] - '0' : -1;
                if (value < 0 || seen[value]) throw std::runtime_error("unknown or duplicate button action");
                seen[value] = true;
                const Rectangle rect = ReadRect(button.at("rect"));
                for (size_t i = 0; i < index; ++i)
                    if (CheckCollisionRecs(rect, skin.buttons[i].rect)) throw std::runtime_error("overlapping buttons");
                skin.buttons[index++] = {value, rect};
            }
            const auto imagePath = (directory / json.at("image").get<std::string>()).string();
            const auto indicatorPath = (directory / json.at("indicatorImage").get<std::string>()).string();
            const auto sounds = json.at("sounds").get<std::array<std::string, 3>>();
            skin.image = assets.RequestTexture(runtime.scope, imagePath.c_str(), imagePath.c_str(), engine::TextureColorUsage::DisplaySrgb, engine::TextureLoad_BilinearFilter);
            skin.indicatorImage = assets.RequestTexture(runtime.scope, indicatorPath.c_str(), indicatorPath.c_str(), engine::TextureColorUsage::DisplaySrgb, engine::TextureLoad_BilinearFilter);
            for (size_t i = 0; i < sounds.size(); ++i)
                skin.sounds[i] = assets.RequestSound(runtime.scope, (directory / sounds[i]).string().c_str());
            runtime.skins.push_back(std::move(skin));
        } catch (const std::exception& error) {
            TraceLog(LOG_WARNING, "[Keypad] rejected %s: %s", path.string().c_str(), error.what());
        }
    }
    const auto fontPath = (assetRoot / "fonts/Inconsolata.otf").string();
    for (size_t i = 0; i < FontSizes.size(); ++i) {
        const auto key = "keypad_display_" + std::to_string(FontSizes[i]);
        runtime.fonts[i] = assets.RequestFont(runtime.scope, key.c_str(), fontPath.c_str(), FontSizes[i]);
    }
    runtime.helpFont = assets.RequestFont(runtime.scope, "keypad_help", fontPath.c_str(), 14);
}

void PrepareSectorKeypadAssets(const engine::AssetManager& assets, SectorKeypadRuntime& runtime)
{
    for (auto& skin : runtime.skins) {
        const auto* image = assets.GetTexture(skin.image);
        const auto* indicator = assets.GetTexture(skin.indicatorImage);
        skin.ready = image && indicator && image->width == indicator->width && image->height == indicator->height
                && assets.IsReady(runtime.helpFont)
                && std::all_of(runtime.fonts.begin(), runtime.fonts.end(), [&](auto font) { return assets.IsReady(font); });
        if (image && image->height > 0) skin.aspectRatio = static_cast<float>(image->width)/image->height;
    }
}

void UnloadSectorKeypads(engine::AssetManager& assets, SectorKeypadRuntime& runtime)
{
    if (!engine::IsNull(runtime.scope)) assets.UnloadScope(runtime.scope);
    runtime = {};
}

Rectangle SectorKeypadRect(Rectangle panel, Rectangle r)
{
    return {panel.x + panel.width*r.x, panel.y + panel.height*r.y, panel.width*r.width, panel.height*r.height};
}

void LayoutSectorKeypad(SectorKeypadRuntime& runtime, Rectangle viewport)
{
    runtime.viewport = viewport;
    const float ratio = runtime.skinIndex < runtime.skins.size()
            ? runtime.skins[runtime.skinIndex].aspectRatio : 1024.0f/1920.0f;
    const float height = std::max(0.0f, std::min(viewport.height - 70.0f, (viewport.width - 32.0f)/ratio));
    runtime.panel = {viewport.x + (viewport.width-height*ratio)*0.5f,
            viewport.y + (viewport.height-height-40.0f)*0.5f, height*ratio, height};
}

int PickSectorKeypad(const SectorKeypadRuntime& runtime, Vector2 point)
{
    if (!runtime.active || runtime.skinIndex >= runtime.skins.size()) return -1;
    for (const auto& button : runtime.skins[runtime.skinIndex].buttons)
        if (CheckCollisionPointRec(point, SectorKeypadRect(runtime.panel, button.rect))) return button.action;
    return -1;
}

bool ApplySectorKeypadAction(SectorKeypadRuntime& runtime, int action)
{
    if (!runtime.active || runtime.errorSeconds > 0 || action < 0 || action > KeypadSubmit) return false;
    runtime.pressedAction = action;
    runtime.pressedSeconds = 0.12f;
    if (action == KeypadBackspace && runtime.length > 0) runtime.entry[--runtime.length] = '\0';
    else if (action >= 0 && action <= 9 && runtime.length < runtime.digits) {
        runtime.entry[runtime.length++] = static_cast<char>('0' + action);
        runtime.entry[runtime.length] = '\0';
    }
    return action == KeypadSubmit && runtime.length == runtime.digits;
}

void CancelSectorKeypad(SectorScriptHost& host, const char* reason) { Close(host, reason); }

void UpdateSectorKeypad(engine::EngineContext& context, SectorScriptHost& host, Rectangle inputViewport, float dt)
{
    if (!host.keypad) return;
    auto& runtime = *host.keypad;
    // Mouse releases may arrive after the owning Lua call has returned.
    for (auto& event : context.input.Events()) {
        if (event.type == engine::InputEventType::MouseButtonPressed)
            runtime.swallowedMouseButtons &= ~(1u << event.mouseButton.button);
        if ((event.type == engine::InputEventType::MouseButtonReleased && (runtime.swallowedMouseButtons & (1u << event.mouseButton.button)))
                || (event.type == engine::InputEventType::MouseClick && (runtime.swallowedMouseButtons & (1u << event.mouseClick.button))))
            engine::ConsumeEvent(event);
    }
    if (!runtime.active) return;
    if (!engine::ScriptSystemIsTaskActive(*host.scripts, runtime.owner)) { Close(host, "task ended"); return; }
    if (runtime.cancelRequested) { Close(host, "cancelled"); return; }
    runtime.errorSeconds = std::max(0.0f, runtime.errorSeconds - dt);
    runtime.pressedSeconds = std::max(0.0f, runtime.pressedSeconds - dt);
    auto point = [&](Vector2 p) {
        if (inputViewport.width > 0 && inputViewport.height > 0) {
            p.x = runtime.viewport.x + (p.x-inputViewport.x)*runtime.viewport.width/inputViewport.width;
            p.y = runtime.viewport.y + (p.y-inputViewport.y)*runtime.viewport.height/inputViewport.height;
        }
        return p;
    };
    runtime.hoverAction = PickSectorKeypad(runtime, point(context.input.MousePosition()));
    bool submitted = false;
    for (auto& event : context.input.Events()) {
        if (event.handled) continue;
        int action = -1;
        if (event.type == engine::InputEventType::KeyPressed || event.type == engine::InputEventType::KeyRepeated) {
            const int key = event.key.key;
            if (key == KEY_GRAVE) continue;
            if (key == KEY_ESCAPE) runtime.cancelRequested = true;
            if (key == KEY_BACKSPACE) action = KeypadBackspace;
            if (event.type == engine::InputEventType::KeyPressed) {
                if (key >= KEY_ZERO && key <= KEY_NINE) action = key - KEY_ZERO;
                if (key >= KEY_KP_0 && key <= KEY_KP_9) action = key - KEY_KP_0;
                if (key == KEY_ENTER || key == KEY_KP_ENTER) action = KeypadSubmit;
            }
        } else if (event.type == engine::InputEventType::MouseButtonPressed) {
            runtime.swallowedMouseButtons |= 1u << event.mouseButton.button;
            if (event.mouseButton.button == MOUSE_BUTTON_LEFT) action = PickSectorKeypad(runtime, point(event.mouseButton.position));
        }
        if (!submitted && !runtime.cancelRequested && engine::IsValid(runtime.inputOperation) && action >= 0 && runtime.errorSeconds <= 0) {
            submitted = ApplySectorKeypadAction(runtime, action);
            PlayTone(context, runtime, 0);
        }
        engine::ConsumeEvent(event);
    }
    if (runtime.cancelRequested) Close(host, "cancelled");
    else if (submitted) {
        const auto operation = runtime.inputOperation;
        runtime.inputOperation = {};
        engine::ScriptSystemCompleteOperation(*host.scripts, operation, {std::string(runtime.entry.data())});
    }
}

void DrawSectorKeypad(const engine::AssetManager& assets, const SectorKeypadRuntime& runtime)
{
    if (!runtime.active || runtime.skinIndex >= runtime.skins.size()) return;
    const auto& skin = runtime.skins[runtime.skinIndex];
    DrawRectangleRec(runtime.viewport, Color{0, 0, 0, 166});
    auto draw = [&](engine::TextureHandle handle, Color tint) {
        if (const auto* image = assets.GetTexture(handle))
            DrawTexturePro(*image, {0, 0, static_cast<float>(image->width), static_cast<float>(image->height)}, runtime.panel, {}, 0, tint);
    };
    draw(skin.image, WHITE);
    auto srgb = [](float c) { return static_cast<unsigned char>(std::round(255*(c <= 0.0031308f ? c*12.92f : 1.055f*std::pow(c, 1/2.4f)-0.055f))); };
    draw(skin.indicatorImage, {srgb(runtime.indicatorColor.x), srgb(runtime.indicatorColor.y), srgb(runtime.indicatorColor.z), 255});
    for (const auto& button : skin.buttons) {
        const bool pressed = runtime.pressedSeconds > 0 && runtime.pressedAction == button.action;
        if (pressed || runtime.hoverAction == button.action)
            DrawRectangleRounded(SectorKeypadRect(runtime.panel, button.rect), 0.2f, 4,
                    pressed ? Color{0, 0, 0, 110} : Color{220, 235, 210, 30});
    }
    char display[9]{};
    if (runtime.errorSeconds > 0) std::snprintf(display, sizeof(display), "ERROR");
    else {
        std::copy_n(runtime.entry.data(), runtime.length, display);
        std::fill(display+runtime.length, display+runtime.digits, '_');
    }
    const Rectangle bounds = SectorKeypadRect(runtime.panel, skin.display);
    for (size_t i = runtime.fonts.size(); i-- > 0;) {
        const auto* font = assets.GetFont(runtime.fonts[i]);
        if (!font) continue;
        const Vector2 size = MeasureTextEx(font->font, display, static_cast<float>(font->pixelSize), 1);
        if (size.x > bounds.width || size.y > bounds.height) continue;
        DrawTextEx(font->font, display, {std::round(bounds.x+(bounds.width-size.x)/2), std::round(bounds.y+(bounds.height-size.y)/2)},
                static_cast<float>(font->pixelSize), 1, runtime.errorSeconds > 0 ? Color{240, 90, 65, 255} : skin.displayColor);
        break;
    }
    if (const auto* font = assets.GetFont(runtime.helpFont)) {
        const char* help = "0-9: Enter code   Enter: Submit\nBackspace: Delete   Esc: Leave";
        const Vector2 size = MeasureTextEx(font->font, help, static_cast<float>(font->pixelSize), 1);
        DrawTextEx(font->font, help, {std::round(runtime.viewport.x+(runtime.viewport.width-size.x)/2), runtime.panel.y+runtime.panel.height+8},
                static_cast<float>(font->pixelSize), 1, Color{190, 192, 181, 255});
    }
}

void RegisterSectorKeypadBindings(lua_State* state)
{
    constexpr const char* wrapper = R"lua(
        local begin, nextInput, validate, reject, finish = ...
        function promptPinCode(options)
            if type(options) ~= 'table' then error('promptPinCode expects an options table', 2) end
            local validator = options.validate
            if validator ~= nil and type(validator) ~= 'function' then error('validate must be a function', 2) end
            local opened, token = begin(options)
            if not opened then return nil, token end
            local ok, code, reason = pcall(function()
                while true do
                    local entered, candidate = nextInput(token)
                    if not entered then return nil, candidate end
                    if validator == nil or validate(validator, candidate) then
                        finish(token, true)
                        return candidate
                    end
                    reject(token)
                end
            end)
            finish(token, false)
            if not ok then error(code, 0) end
            return code, reason
        end
    )lua";
    if (luaL_loadstring(state, wrapper) != LUA_OK) lua_error(state);
    for (lua_CFunction fn : {Begin, NextInput, Validate, Reject, Finish}) lua_pushcfunction(state, fn);
    if (lua_pcall(state, 5, 0, 0) != LUA_OK) lua_error(state);
}
} // namespace game
