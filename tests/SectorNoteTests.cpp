#include "game/note/SectorNote.h"
#include "game/keypad/SectorKeypad.h"
#include "game/SectorScriptBindings.h"
#include "game/cutscene/SectorCutsceneRuntime.h"
#include "game/dialogue/SectorDialogue.h"
#include "engine/EngineContext.h"
#include "engine/scripting/ScriptPersistence.h"
#include "engine/scripting/ScriptSystem.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "lua.hpp"
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>

namespace {
struct Fixture {
    engine::EngineContext context;
    engine::ScriptRuntime scripts;
    engine::PersistentScriptStore persistent;
    game::SectorScriptHost host;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorNoteRuntime note;
    game::SectorKeypadRuntime keypad;
    game::SectorCutsceneRuntime cutscene;
    game::SectorDialogueRuntime dialogue;
    int opens = 0, closes = 0;
    std::filesystem::path root = std::filesystem::temp_directory_path()
            / ("note_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture()
    {
        std::filesystem::create_directories(root);
        context.input.ReserveEvents(32);
        game::InitializeSectorScriptHost(host, objects, map, scripts, nullptr, nullptr, {}, nullptr, &cutscene);
        host.note = &note;
        host.keypad = &keypad;
        host.dialogue = &dialogue;
        host.controls.userData = this;
        host.controls.noteChanged = [](void* data, bool active) {
            auto& fixture = *static_cast<Fixture*>(data);
            if (active) ++fixture.opens; else ++fixture.closes;
        };
        note.lines.reserve(1024);
        note.wrappedText.reserve(32768);
        game::SectorKeypadSkin skin;
        skin.id = "fixture";
        skin.ready = true;
        keypad.skins.push_back(skin);
    }
    ~Fixture()
    {
        if (scripts.vm) engine::ScriptSystemShutdownForMap(context, scripts);
        assert(!note.active && opens == closes);
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    void Create(const std::string& script)
    {
        { std::ofstream file(root / "test.lua"); file << script; }
        std::string error;
        assert(engine::ScriptSystemCreateForMap(context, scripts, persistent, "test", (root/"test.json").string(),
                root.string(), &host, game::RegisterSectorScriptBindings, false, error));
    }
    void Call(const char* name = "use")
    {
        assert(engine::ScriptSystemCallForegroundHook(scripts, name).result != engine::ScriptCallResult::Error);
    }
    void Tick(float dt = 0.025f)
    {
        game::UpdateSectorNote(context, host, dt);
        engine::ScriptSystemUpdate(context, scripts, dt);
        context.input.Events().clear();
    }
    void Key(int key, engine::InputEventType type = engine::InputEventType::KeyPressed)
    {
        engine::InputEvent event{};
        event.type = type;
        event.key.key = key;
        context.input.Events().push_back(event);
    }
};

void FadeAndSequentialCalls()
{
    Fixture f;
    f.Create(R"lua(
        function use()
            assert(showNote('Reminder', 'First line\n\nSecond paragraph') == true)
            setFlag('first', true)
            assert(showNote('Body only') == true)
            setFlag('second', true)
        end
    )lua");
    f.Key(KEY_E); f.Call();
    assert(f.context.input.Events()[0].handled);
    assert(f.note.active && f.note.title == "Reminder" && f.note.body == "First line\n\nSecond paragraph");
    f.Tick(0.175f);
    assert(f.note.phase == game::SectorNotePhase::Opening && std::abs(f.note.opacity-0.5f) < 0.001f);
    f.Key(KEY_E, engine::InputEventType::KeyRepeated); f.Tick(0.175f);
    assert(f.note.phase == game::SectorNotePhase::Reading && f.note.opacity == 1);
    assert(f.persistent.bools.count("first") == 0);
    f.Key(KEY_E); f.Tick();
    assert(f.note.phase == game::SectorNotePhase::Closing && f.note.opacity == 1);
    f.Tick(0.175f);
    assert(f.note.active && std::abs(f.note.opacity-0.5f) < 0.001f && f.closes == 0);
    f.Tick(0.175f);
    assert(f.persistent.bools.at("first") && f.note.active && f.note.title.empty());
    assert(f.note.body == "Body only" && f.opens == 2 && f.closes == 1);
    f.Tick(0.1f);
    const float opacity = f.note.opacity;
    f.Key(KEY_ESCAPE); f.Tick();
    assert(f.note.phase == game::SectorNotePhase::Closing && f.note.opacity == opacity);
    f.Key(KEY_ESCAPE); f.Tick(0.175f); // Repeated dismissal cannot restart the fade.
    assert(f.note.opacity < opacity && !f.persistent.bools.count("second"));
    f.Tick(0.175f);
    assert(!f.note.active && f.persistent.bools.at("second"));
}

void ValidationConflictsAndCancellation()
{
    Fixture f;
    f.Create(R"lua(
        function invalid()
            assert(not pcall(showNote))
            assert(not pcall(showNote, 123))
            assert(not pcall(showNote, 'title', nil))
            assert(not pcall(showNote, 'a', 'b', 'c'))
            assert(not pcall(showNote, 'a\0b'))
        end
        function use()
            local ok, reason = showNote('Body')
            assert(ok == nil and reason == 'player died')
            setFlag('interrupted', true)
        end
        function unavailable()
            local ok, reason = showNote('Body')
            assert(ok == nil and reason)
        end
        function competitor()
            local ok, reason = showNote('Second')
            assert(ok == nil and reason)
            local code, why = promptPinCode({skin='fixture',digits=4})
            assert(code == nil and why)
            local choice, why = dialogue('missing')
            assert(choice == nil and why)
            local ok, why = text('caption')
            assert(not ok and why)
            setFlag('conflicts', true)
        end
        function stopUse() showNote('Stop test') end
    )lua");
    f.Call("invalid"); assert(f.opens == 0);
    // Direct VM calls are outside the scheduler and cannot open a note.
    assert(luaL_dostring(f.scripts.vm, "assert(not pcall(showNote, 'direct'))") == LUA_OK);
    assert(f.opens == 0);
    for (bool* conflict : {&f.keypad.active, &f.dialogue.active, &f.cutscene.caption.active,
            &f.host.conversation.active, &f.host.inventoryInteractionActive}) {
        *conflict = true;
        f.Call("unavailable");
        assert(f.opens == 0);
        *conflict = false;
    }
    f.host.note = nullptr; f.Call("unavailable"); f.host.note = &f.note;
    f.Call();
    std::string error;
    assert(engine::ScriptSystemQueueBackground(f.scripts, "competitor", error));
    f.Tick();
    assert(f.persistent.bools.at("conflicts") && f.opens == 1);
    game::CancelSectorNote(f.host, "player died"); f.Tick();
    assert(!f.note.active && f.persistent.bools.at("interrupted"));
    f.Call("stopUse");
    engine::ScriptSystemStopAll(f.context, f.scripts); f.Tick();
    assert(!f.note.active && f.opens == f.closes);
    f.Call("stopUse"); // Destructor also checks map shutdown cleanup.
}

struct TestFont {
    std::array<GlyphInfo, 96> glyphs{};
    std::array<Rectangle, 96> rects{};
    Font font{};
    explicit TestFont(int size)
    {
        for (size_t i = 0; i < glyphs.size(); ++i) {
            glyphs[i].value = static_cast<int>(i)+32;
            glyphs[i].advanceX = size/2;
        }
        font.baseSize = size;
        font.texture.id = 1; // MeasureTextEx's CPU-only validity gate; never drawn or unloaded.
        font.glyphCount = static_cast<int>(glyphs.size());
        font.glyphs = glyphs.data(); font.recs = rects.data();
    }
};

void LayoutAndScroll()
{
    Fixture f;
    TestFont body(18), title(24);
    f.Create("function use() showNote('Title', 'Body') end");
    f.Call();
    f.note.body = "first\n\nsecond\r\nthird";
    game::LayoutSectorNote(f.note, {0,0,640,480}, body.font, title.font);
    assert(f.note.lines.size() == 5 && f.note.lines[0].title);
    assert(std::string(f.note.wrappedText.c_str()+f.note.lines[2].offset).empty());
    f.note.title.clear(); f.note.layoutDirty = true;
    game::LayoutSectorNote(f.note, {0,0,640,480}, body.font, title.font);
    assert(f.note.lines.size() == 4 && f.note.lines[0].y == 0 && !f.note.lines[0].title);
    f.note.body = "";
    f.note.layoutDirty = true;
    game::LayoutSectorNote(f.note, {0,0,640,480}, body.font, title.font);
    assert(f.note.lines.empty() && f.note.maxScroll == 0);
    f.note.title = std::string(150, 'T');
    f.note.layoutDirty = true;
    game::LayoutSectorNote(f.note, {0,0,320,240}, body.font, title.font);
    assert(f.note.maxScroll > 0);
    for (const auto& line : f.note.lines) {
        assert(line.title);
        assert(MeasureTextEx(title.font, f.note.wrappedText.c_str()+line.offset, 24, 1).x <= f.note.content.width);
    }
    f.note.title.clear();
    f.note.body.clear();
    for (int i = 0; i < 100; ++i) f.note.body += "Words that wrap naturally. \n";
    f.note.body += std::string(200, 'x') + "æøåé";
    for (Rectangle viewport : {Rectangle{0,0,320,240}, Rectangle{30,45,640,480},
            Rectangle{0,0,1920,1080}, Rectangle{0,0,3840,2160}}) {
        f.note.layoutDirty = true;
        game::LayoutSectorNote(f.note, viewport, body.font, title.font);
        assert(f.note.panel.x >= viewport.x && f.note.panel.y >= viewport.y);
        assert(f.note.panel.x+f.note.panel.width <= viewport.x+viewport.width);
        assert(f.note.panel.y+f.note.panel.height+20 <= viewport.y+viewport.height);
        assert(f.note.maxScroll > 0);
        for (const auto& line : f.note.lines) {
            const char* text = f.note.wrappedText.c_str()+line.offset;
            assert((static_cast<unsigned char>(*text) & 0xc0) != 0x80);
            assert(MeasureTextEx(body.font, text, 18, 1).x <= f.note.content.width+0.01f);
        }
        const auto* storage = f.note.wrappedText.data();
        const auto* lines = f.note.lines.data();
        game::LayoutSectorNote(f.note, viewport, body.font, title.font);
        assert(f.note.wrappedText.data() == storage && f.note.lines.data() == lines);
        f.Key(KEY_END); f.Tick();
        assert(f.note.scroll == f.note.maxScroll);
        assert(f.note.lines.back().y + body.font.baseSize - f.note.scroll <= f.note.content.height);
        f.Key(KEY_HOME); f.Tick(); assert(f.note.scroll == 0);
        f.Key(KEY_PAGE_DOWN); f.Tick(); assert(f.note.scroll > 0);
        f.Key(KEY_PAGE_UP); f.Tick(); assert(f.note.scroll == 0);
        engine::InputEvent wheel{};
        wheel.type = engine::InputEventType::MouseWheel; wheel.wheel.value = -1;
        f.context.input.Events().push_back(wheel); f.Tick(); assert(f.note.scroll > 0);
    }
    // A click held while dismissing must not release into gameplay afterward.
    engine::InputEvent click{};
    click.type = engine::InputEventType::MouseButtonPressed; click.mouseButton.button = MOUSE_BUTTON_LEFT;
    f.context.input.Events().push_back(click); f.Key(KEY_E); f.Tick(); f.Tick(0.35f);
    click.type = engine::InputEventType::MouseButtonReleased;
    f.context.input.Events().push_back(click);
    game::UpdateSectorNote(f.context, f.host, 0);
    assert(f.context.input.Events()[0].handled);
}
} // namespace

void RunSectorNoteTests()
{
    FadeAndSequentialCalls();
    ValidationConflictsAndCancellation();
    LayoutAndScroll();
}
