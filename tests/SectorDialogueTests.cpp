#include "game/dialogue/SectorDialogue.h"
#include "game/SectorScriptBindings.h"
#include "game/cutscene/SectorCutsceneRuntime.h"
#include "engine/EngineContext.h"
#include "engine/scripting/ScriptConsole.h"
#include "engine/scripting/ScriptPersistence.h"
#include "engine/scripting/ScriptSystem.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"

#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>

namespace {
struct DialogueFixture {
    engine::EngineContext context;
    engine::ScriptRuntime scripts;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    game::SectorCutsceneRuntime cutscene;
    game::SectorDialogueRuntime dialogue;
    engine::DialogueVoiceLibrary voices;
    std::filesystem::path root = std::filesystem::temp_directory_path()
            / ("engine_dialogue_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int menuOpens = 0;
    int menuCloses = 0;

    DialogueFixture()
    {
        std::filesystem::create_directories(root / "dialogue");
        Write("dialogue/00_valid.json", R"({"choiceSets":[
            {"id":"topics","options":[{"id":"question","text":"A question?"},{"id":"goodbye","text":"Goodbye."}]},
            {"id":"nested","options":[{"id":"answer","text":"An answer."}]}]})");
        game::LoadSectorDialogue(dialogue, root / "dialogue");
        game::InitializeSectorCutsceneRuntime(cutscene);
        game::InitializeSectorScriptHost(host, objects, map, scripts, nullptr, nullptr,
                {}, nullptr, &cutscene);
        host.dialogue = &dialogue;
        host.controls.userData = this;
        host.controls.setControlsEnabled = [](void*, engine::EngineContext&, bool, std::string&) { return true; };
        host.controls.dialogueChanged = [](void* data, bool active) {
            auto& f = *static_cast<DialogueFixture*>(data);
            if (active) ++f.menuOpens; else ++f.menuCloses;
        };
        engine::DialogueVoice male;
        male.id = "male";
        male.banks[0].push_back({engine::SoundHandle{123, 1}, 0.09f, 17});
        voices.voices.push_back(std::move(male));
        engine::DialogueVoice female;
        female.id = "female";
        female.banks[0].push_back({engine::SoundHandle{124, 1}, 0.09f, 23});
        voices.voices.push_back(std::move(female));
        host.dialogueVoices = &voices;
    }
    ~DialogueFixture()
    {
        if (scripts.vm) engine::ScriptSystemShutdownForMap(context, scripts);
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    void Write(const char* file, const std::string& contents)
    {
        std::ofstream output(root / file);
        output << contents;
        assert(output.good());
    }
    void Create(const std::string& source)
    {
        Write("test.lua", source);
        std::string error;
        assert(engine::ScriptSystemCreateForMap(context, scripts, persistent, "test",
                (root / "test.json").string(), root.string(), &host,
                game::RegisterSectorScriptBindings, false, error));
    }
    engine::ScriptCallOutcome Call(const char* name)
    {
        return engine::ScriptSystemCallForegroundHook(scripts, name);
    }
    void Tick(float dt = 0.025f)
    {
        engine::ScriptSystemUpdate(context, scripts, dt);
        game::UpdateSectorScriptCutsceneControlOwnership(context, host);
    }
};

engine::InputEvent Key(int key, engine::InputEventType type = engine::InputEventType::KeyPressed)
{
    engine::InputEvent event{};
    event.type = type;
    event.key.key = key;
    return event;
}

void AssetsValidateTransactionallyAndFilteringPreservesLabels()
{
    DialogueFixture f;
    f.Write("dialogue/01_duplicate.json", R"({"choiceSets":[
        {"id":"must_not_leak","options":[{"id":"ok","text":"OK"}]},
        {"id":"topics","options":[{"id":"duplicate","text":"Duplicate"}]}]})");
    f.Write("dialogue/02_invalid.json", R"({"choiceSets":[{"id":"bad","options":[
        {"id":"same","text":"First"},{"id":"same","text":"Second"}]}]})");
    f.Write("dialogue/03_empty.json", R"({"choiceSets":[{"id":"empty","options":[]}]})");
    f.Write("dialogue/04_label.json", R"({"choiceSets":[{"id":"blank","options":[{"id":"x","text":"  "}]}]})");
    f.Write("dialogue/05_broken.json", "{broken");
    game::LoadSectorDialogue(f.dialogue, f.root / "dialogue");
    assert(f.dialogue.sets.size() == 2);
    assert(f.dialogue.sets[0].id == "topics");
    std::string error;
    assert(!game::BeginSectorDialogue(f.dialogue, "missing", {}, error));
    assert(!game::BeginSectorDialogue(f.dialogue, "topics", {"question", "goodbye"}, error));
    assert(game::BeginSectorDialogue(f.dialogue, "topics", {"question", "unknown"}, error));
    assert(f.dialogue.visible == std::vector<size_t>{1});
    const auto token = f.dialogue.token;
    assert(!game::BeginSectorDialogue(f.dialogue, "nested", {}, error));
    assert(f.dialogue.token == token);
    game::ResetSectorDialogueMenu(f.dialogue);
    assert(game::BeginSectorDialogue(f.dialogue, "topics", {}, error));
    assert(f.dialogue.visible.size() == 2);
    assert(f.dialogue.sets[0].options[0].text == "A question?");
    game::LoadSectorDialogue(f.dialogue, f.root / "absent");
    assert(f.dialogue.sets.empty() && !f.dialogue.active);
}

void ConversationsYieldFilterNestAndPersist(bool cinematic)
{
    DialogueFixture f;
    std::string source = R"lua(
        assert(type(appendIf) == "function" and type(dialogue) == "function")
        assert(not flag("absent") and getInt("absent") == 0 and getString("absent") == "")
        function init() end
        function talk()
            CINEMATIC_START
            local count = 40
            local result = runConversationDynamic("topics", {
                question = function(id)
                    assert(id == "question")
                    assert(say("Hello there.", {mood = "afraid", holdMs = 10}))
                    delay(20)
                    assert(dialogue("nested") == "answer")
                    count = count + 2
                    setInt("count", count)
                    setFlag("asked", true)
                end,
                goodbye = function()
                    assert(say("Bye."))
                    return "exit"
                end,
            }, function() return hiddenOptions({question = flag("asked")}) end)
            setString("result", result)
            assert(getPersistentBool("asked") and getPersistentInt("count") == 42)
            CINEMATIC_END
        end
    )lua";
    source.replace(source.find("CINEMATIC_START"), 15, cinematic ? "assert(startCutscene())" : "");
    source.replace(source.find("CINEMATIC_END"), 13, cinematic ? "assert(endCutscene())" : "");
    f.Create(source);
    assert(f.Call("talk").result == engine::ScriptCallResult::Started);
    assert(f.dialogue.active && f.cutscene.controlsEnabled == !cinematic);
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 0));
    f.Tick();
    assert(f.cutscene.caption.active && f.cutscene.caption.playerSpeaker);
    assert(f.cutscene.caption.mood == engine::DialogueMood::Afraid);
    assert(std::fabs(f.cutscene.caption.holdSeconds - 0.01) < 0.0001);
    assert(!f.cutscene.caption.speechTimeline.cues.empty());
    assert(f.cutscene.caption.speechTimeline.cues[0].source == 17); // male neutral fallback
    auto& input = f.context.input;
    input.Events().push_back(Key(KEY_ENTER));
    input.Events().push_back(Key(KEY_ENTER));
    assert(game::ConsumeSectorSpeechAdvance(f.dialogue, input, true));
    assert(game::AdvanceSectorCutsceneSpeech(f.cutscene, f.scripts));
    assert(!game::AdvanceSectorCutsceneSpeech(f.cutscene, f.scripts));
    f.Tick(0);
    assert(!f.dialogue.active && !f.persistent.bools["asked"]); // delay is a separate wait
    f.Tick(0.025f);
    assert(f.dialogue.active && f.dialogue.setIndex == 1);
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    assert(f.dialogue.active); // neither consumed advance press selects the nested menu
    input.BeginFrame();
    input.Events().push_back(Key(KEY_ONE));
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    f.Tick();
    assert(f.dialogue.active && f.dialogue.visible == std::vector<size_t>{1});
    assert(f.persistent.bools.at("asked") && f.persistent.ints.at("count") == 42);
    assert(f.cutscene.controlsEnabled == !cinematic);
    input.BeginFrame();
    input.Events().push_back(Key(KEY_ENTER));
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    f.Tick();
    assert(f.cutscene.caption.text == "Bye.");
    game::SetSectorCutsceneCaptionVoiceTiming(f.cutscene, false);
    game::UpdateSectorCutsceneTimelines(f.cutscene, f.scripts, 30);
    f.Tick();
    assert(f.persistent.strings.at("result") == "goodbye");
    assert(!f.dialogue.active && f.cutscene.controlsEnabled);
    std::string json, error;
    assert(engine::SavePersistentScriptStoreToJsonString(f.persistent, json, error));
    engine::PersistentScriptStore loaded;
    assert(engine::LoadPersistentScriptStoreFromJsonString(json, loaded, error));
    assert(loaded.bools.at("asked") && loaded.ints.at("count") == 42 && loaded.strings.at("result") == "goodbye");
}

void OwnershipFailureCancellationAndPlayerOverloads()
{
    DialogueFixture f;
    f.Create(R"lua(
        function init() end
        function wait_menu()
            assert(startCutscene())
            local choice, reason = dialogue("topics")
            assert(choice == nil and type(reason) == "string")
            setFlag("cancelled", true)
            delay(1000)
        end
        function competitor()
            local choice, reason = dialogue("nested")
            assert(choice == nil and type(reason) == "string")
            assert(startSay("Cannot replace a menu") == nil)
            setFlag("competitor", true)
        end
        function bad_inputs()
            assert(dialogue("missing") == nil)
            assert(dialogue("topics", {"question", "goodbye"}) == nil)
            assert(dialogue("topics", {question = true}) == nil)
            assert(dialogue("topics", {[1]="question", [3]="goodbye"}) == nil)
            assert(dialogue("topics", {false}) == nil)
            assert(dialogue("topics", "question") == nil)
            assert(dialogue("topics", nil, "extra") == nil)
        end
        function simple_menu() setString("simple", dialogue("topics") or "nil") end
        function fail_after_menu() dialogue("topics"); error("expected failure") end
        function caption_busy()
            local op = assert(startSay("In progress"))
            assert(dialogue("topics") == nil)
            assert(cancelOperation(op))
        end
        function static_helper()
            assert(runConversation("topics", {question = function() return "break" end}) == "question")
            assert(runConversation("topics", {}) == "goodbye")
            assert(runConversationDynamic("missing", {}) == nil)
            setFlag("static_done", true)
        end
    )lua");
    assert(!engine::ScriptSystemExecuteConsole(f.scripts, "dialogue('topics')").success);
    assert(!engine::ScriptSystemExecuteConsole(f.scripts, "say('player line')").success);
    assert(!f.dialogue.active && !f.cutscene.caption.active);
    assert(f.Call("bad_inputs").result == engine::ScriptCallResult::Completed);
    assert(f.Call("caption_busy").result == engine::ScriptCallResult::Completed);
    assert(f.Call("wait_menu").result == engine::ScriptCallResult::Started);
    const auto operation = f.dialogue.operation;
    const auto token = f.dialogue.token;
    std::string error;
    assert(engine::ScriptSystemQueueBackground(f.scripts, "competitor", error));
    f.Tick();
    assert(f.persistent.bools.at("competitor") && f.dialogue.token == token);
    assert(engine::ScriptSystemCancelOperation(f.context, f.scripts, operation, "test cancelled"));
    assert(!f.dialogue.active && f.menuCloses == 1);
    f.Tick();
    assert(f.persistent.bools.at("cancelled") && !f.cutscene.controlsEnabled);
    assert(engine::ScriptSystemStopFunction(f.context, f.scripts, "wait_menu", error));
    f.Tick();
    assert(f.cutscene.controlsEnabled);
    assert(f.Call("static_helper").result == engine::ScriptCallResult::Started);
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 0)); f.Tick();
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 1)); f.Tick();
    assert(f.persistent.bools.at("static_done"));
    assert(f.Call("fail_after_menu").result == engine::ScriptCallResult::Started);
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 0)); f.Tick();
    assert(!f.dialogue.active && f.cutscene.controlsEnabled);
    assert(f.Call("simple_menu").result == engine::ScriptCallResult::Started);
    assert(engine::ScriptSystemStopFunction(f.context, f.scripts, "simple_menu", error)); f.Tick();
    assert(!f.dialogue.active && !f.persistent.strings.count("simple"));
    assert(engine::ScriptSystemExecuteConsole(f.scripts, R"lua(
        op = assert(startSay("Player test", {mood="happy", holdMs=125}))
        assert(startSay("Invalid", {mood="unknown"}) == nil)
        assert(startSay("Invalid", {holdMs=-1}) == nil)
        assert(startSay("Invalid", {}, "extra") == nil)
    )lua").success);
    assert(f.cutscene.caption.playerSpeaker && f.cutscene.caption.text == "Player test");
    assert(f.cutscene.caption.mood == engine::DialogueMood::Happy);
    assert(std::fabs(f.cutscene.caption.holdSeconds - 0.125) < 0.0001);
    assert(f.cutscene.caption.speechTimeline.cues[0].source == 17);
    assert(engine::ScriptSystemExecuteConsole(f.scripts, "assert(cancelOperation(op))").success);
    assert(!f.cutscene.caption.active);
    assert(f.Call("simple_menu").result == engine::ScriptCallResult::Started);
    engine::ScriptSystemShutdownForMap(f.context, f.scripts);
    assert(!f.dialogue.active && f.cutscene.controlsEnabled);
    assert(!engine::ScriptSystemCompleteOperation(f.scripts, operation, {std::string{"stale"}}));
}

void LayoutScrollingAndInputAreBounded()
{
    DialogueFixture f;
    game::SectorDialogueSet set;
    set.id = "long";
    for (int i = 0; i < 12; ++i) set.options.push_back({std::to_string(i), "A long question with several words and an explicit\nsecond line?"});
    f.dialogue.sets.push_back(std::move(set));
    f.dialogue.visible.reserve(12);
    f.dialogue.rows.reserve(12);
    f.dialogue.lines.reserve(2048);
    std::array<GlyphInfo, 128> glyphs{};
    std::array<Rectangle, 128> rectangles{};
    for (size_t i = 0; i < glyphs.size(); ++i) {
        glyphs[i].value = static_cast<int>(i);
        glyphs[i].advanceX = 10;
        rectangles[i] = {0, 0, 10, 20};
    }
    Font font{};
    font.baseSize = 20;
    font.glyphCount = static_cast<int>(glyphs.size());
    font.glyphs = glyphs.data();
    font.recs = rectangles.data();
    std::string error;
    assert(game::BeginSectorDialogue(f.dialogue, "long", {}, error));
    const auto rowsCapacity = f.dialogue.rows.capacity(), linesCapacity = f.dialogue.lines.capacity();
    for (const Rectangle viewport : {Rectangle{0, 0, 1920, 1080}, Rectangle{30, 50, 640, 360}, Rectangle{10, 20, 320, 240}}) {
        for (const int size : {20, 48}) {
            game::LayoutSectorDialogue(f.dialogue, font, size, viewport, viewport.y + viewport.height * 0.85f);
            assert(f.dialogue.layoutReady && f.dialogue.rows.size() == 12);
            assert(f.dialogue.panel.x >= viewport.x && f.dialogue.panel.y >= viewport.y);
            assert(f.dialogue.panel.y + f.dialogue.panel.height <= viewport.y + viewport.height);
            assert(f.dialogue.numberWidth + 32 < f.dialogue.panel.width);
            f.dialogue.selected = 11;
            f.dialogue.ensureSelectionVisible = true;
            game::LayoutSectorDialogue(f.dialogue, font, size, viewport, viewport.y + viewport.height * 0.85f);
            const auto& last = f.dialogue.rows.back();
            const float innerHeight = f.dialogue.panel.height - 24;
            assert(last.top >= f.dialogue.scroll - 0.001f);
            assert(last.top < f.dialogue.scroll + innerHeight);
            assert(f.dialogue.scroll <= f.dialogue.contentHeight - innerHeight + 0.001f);
        }
    }
    assert(f.dialogue.rows.capacity() == rowsCapacity && f.dialogue.lines.capacity() == linesCapacity);
    auto& input = f.context.input;
    input.Events().push_back(Key(KEY_DOWN));
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    assert(f.dialogue.selected == 0 && input.Events()[0].handled);
    input.BeginFrame();
    input.Events().push_back(Key(KEY_ESCAPE));
    engine::InputEvent click{};
    click.type = engine::InputEventType::MouseButtonPressed;
    click.mouseButton = {{0, 0}, MOUSE_BUTTON_LEFT};
    input.Events().push_back(click);
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    assert(f.dialogue.active && !input.Events()[0].handled && input.Events()[1].handled);
    input.BeginFrame();
    engine::InputEvent wheel{};
    wheel.type = engine::InputEventType::MouseWheel;
    wheel.wheel.value = -1000;
    input.Events().push_back(wheel);
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    assert(f.dialogue.scroll == f.dialogue.contentHeight - f.dialogue.panel.height + 24);
    input.BeginFrame();
    click.mouseButton.position = {f.dialogue.panel.x + 20, f.dialogue.panel.y + f.dialogue.panel.height - 13};
    input.Events().push_back(click);
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    assert(!f.dialogue.active); // last row remains clickable at maximum scroll
    assert(game::BeginSectorDialogue(f.dialogue, "topics", {}, error));
    input.BeginFrame();
    engine::InputEvent release{};
    release.type = engine::InputEventType::MouseButtonReleased;
    release.mouseButton = click.mouseButton;
    input.Events().push_back(release);
    engine::InputEvent tail{};
    tail.type = engine::InputEventType::MouseClick;
    tail.mouseClick.button = MOUSE_BUTTON_LEFT;
    input.Events().push_back(tail);
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    assert(f.dialogue.active && input.Events()[0].handled && input.Events()[1].handled);
    input.BeginFrame();
    input.Events().push_back(Key(KEY_TWO));
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input);
    assert(!f.dialogue.active);
    // Input lives in logical coordinates; HUD choices live in presentation
    // pixels. Verify picking in an offset, scaled presentation viewport.
    assert(game::BeginSectorDialogue(f.dialogue, "topics", {}, error));
    const Rectangle physical{100, 70, 960, 540};
    const Rectangle logical{0, 0, 1920, 1080};
    game::LayoutSectorDialogue(f.dialogue, font, 20, physical, 400);
    input.BeginFrame();
    const Vector2 target{f.dialogue.panel.x + 30, f.dialogue.panel.y + 14};
    click.mouseButton.position = {(target.x - physical.x) * 2, (target.y - physical.y) * 2};
    input.Events().push_back(click);
    game::UpdateSectorDialogueInput(f.dialogue, f.scripts, input, logical);
    assert(!f.dialogue.active);
}
}

void RunSectorDialogueTests()
{
    AssetsValidateTransactionallyAndFilteringPreservesLabels();
    ConversationsYieldFilterNestAndPersist(false);
    ConversationsYieldFilterNestAndPersist(true);
    OwnershipFailureCancellationAndPlayerOverloads();
    LayoutScrollingAndInputAreBounded();
}
