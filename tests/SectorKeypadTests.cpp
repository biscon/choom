#include "game/keypad/SectorKeypad.h"
#include "game/SectorScriptBindings.h"
#include "game/cutscene/SectorCutsceneRuntime.h"
#include "game/dialogue/SectorDialogue.h"
#include "engine/EngineContext.h"
#include "engine/scripting/ScriptPersistence.h"
#include "engine/scripting/ScriptSystem.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <limits>

namespace {
struct Fixture {
    engine::EngineContext context;
    engine::ScriptRuntime scripts;
    engine::PersistentScriptStore persistent;
    game::SectorScriptHost host;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorKeypadRuntime keypad;
    game::SectorCutsceneRuntime cutscene;
    game::SectorDialogueRuntime dialogue;
    int opens = 0, closes = 0;
    std::filesystem::path root = std::filesystem::temp_directory_path()
            / ("keypad_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture()
    {
        std::filesystem::create_directories(root);
        context.input.ReserveEvents(32);
        game::InitializeSectorScriptHost(host, objects, map, scripts, nullptr, nullptr, {}, nullptr, &cutscene);
        host.keypad = &keypad;
        host.dialogue = &dialogue;
        host.controls.userData = this;
        host.controls.keypadChanged = [](void* data, bool active) {
            auto& fixture = *static_cast<Fixture*>(data);
            if (active) ++fixture.opens; else ++fixture.closes;
        };
        game::SectorKeypadSkin skin;
        skin.id = "fixture";
        skin.ready = true; // CPU fixture represents an already prepared skin.
        for (int i = 0; i < 12; ++i) skin.buttons[i] = {i, {0.1f+(i%3)*0.25f, 0.15f+(i/3)*0.18f, 0.2f, 0.13f}};
        keypad.skins.push_back(skin);
    }
    ~Fixture()
    {
        if (scripts.vm) engine::ScriptSystemShutdownForMap(context, scripts);
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
        const auto result = engine::ScriptSystemCallForegroundHook(scripts, name);
        assert(result.result != engine::ScriptCallResult::Error);
    }
    void Tick(float dt = 0.025f)
    {
        game::UpdateSectorKeypad(context, host, {0, 0, 1920, 1080}, dt);
        engine::ScriptSystemUpdate(context, scripts, dt);
        context.input.Events().clear();
    }
    void Key(int key)
    {
        engine::InputEvent event{};
        event.type = engine::InputEventType::KeyPressed;
        event.key.key = key;
        context.input.Events().push_back(event);
    }
    void Submit(const char* code)
    {
        for (; *code; ++code) Key(KEY_ZERO+*code-'0');
        Key(KEY_ENTER);
        Tick();
    }
};

void RetryAndStringResults()
{
    Fixture f;
    f.Create(R"lua(
        function use()
            local code = promptPinCode({skin='fixture', digits=4,
                validate=function(code) return code == '0042' end})
            assert(code == '0042')
            setFlag('accepted', true)
        end
    )lua");
    f.Call();
    assert(f.keypad.active && f.opens == 1);
    f.Submit("1984");
    assert(f.keypad.active && f.keypad.errorSeconds > 0 && f.closes == 0);
    f.Key(KEY_ONE); f.Tick();
    assert(f.keypad.length == 0);
    f.Tick(0.7f);
    f.Submit("0042");
    assert(!f.keypad.active && f.closes == 1 && f.persistent.bools.at("accepted"));
}

void InputAndScaledPicking()
{
    Fixture f;
    f.Create("function use() local code=promptPinCode({skin='fixture',digits=4}); assert(code=='1984') end");
    f.Call();
    for (Rectangle viewport : {Rectangle{0,0,640,480}, Rectangle{30,45,1280,720}, Rectangle{0,0,3840,2160}}) {
        game::LayoutSectorKeypad(f.keypad, viewport);
        assert(f.keypad.panel.x >= viewport.x && f.keypad.panel.y >= viewport.y);
        for (const auto& button : f.keypad.skins[0].buttons) {
            const auto bounds = game::SectorKeypadRect(f.keypad.panel, button.rect);
            assert(game::PickSectorKeypad(f.keypad, {bounds.x+bounds.width/2, bounds.y+bounds.height/2}) == button.action);
        }
    }
    game::LayoutSectorKeypad(f.keypad, {20,40,960,540});
    for (int action : {1, 9, 8, 4}) {
        const auto bounds = game::SectorKeypadRect(f.keypad.panel, f.keypad.skins[0].buttons[action].rect);
        engine::InputEvent click{};
        click.type = engine::InputEventType::MouseButtonPressed;
        click.mouseButton.button = MOUSE_BUTTON_LEFT;
        click.mouseButton.position = {(bounds.x+bounds.width/2-20)*2, (bounds.y+bounds.height/2-40)*2};
        f.context.input.Events().push_back(click);
        f.Tick();
    }
    assert(std::string(f.keypad.entry.data()) == "1984");
    f.Key(KEY_KP_7); f.Tick();
    assert(std::string(f.keypad.entry.data()) == "1984");
    f.Key(KEY_BACKSPACE); f.Key(KEY_KP_4); f.Key(KEY_KP_ENTER); f.Tick();
    assert(!f.keypad.active);
    engine::InputEvent release{};
    release.type = engine::InputEventType::MouseButtonReleased;
    release.mouseButton.button = MOUSE_BUTTON_LEFT;
    f.context.input.Events().push_back(release);
    game::UpdateSectorKeypad(f.context, f.host, {}, 0);
    assert(f.context.input.Events()[0].handled);
}

void CancelErrorsAndOwnership()
{
    Fixture f;
    f.Create(R"lua(
        function use()
            local code, reason=promptPinCode({skin='fixture',digits=4})
            assert(code==nil and reason=='cancelled')
            setFlag('cancelled',true)
        end
        function competitor()
            local code, reason=promptPinCode({skin='fixture',digits=4})
            assert(code==nil and reason)
            local choice, why=dialogue('unused')
            assert(choice==nil and why)
        end
        function invalid()
            for _, options in ipairs({{skin='missing',digits=4}, {skin='fixture',digits=9}, {skin='fixture',digits=0},
                    {skin='fixture',digits=4,indicatorColor={0/0,0,0}}}) do
                local code, reason=promptPinCode(options)
                assert(code==nil and reason)
            end
            assert(not setPropEmissiveColor('absent','Light',0,0,1))
        end
        function errorUse()
            local ok = pcall(function() promptPinCode({skin='fixture',digits=1,validate=function() error('bad validator') end}) end)
            assert(not ok)
        end
        function yieldUse()
            local ok = pcall(function() promptPinCode({skin='fixture',digits=1,validate=function() delay(10); return true end}) end)
            assert(not ok)
        end
        function typeUse()
            local ok = pcall(function() promptPinCode({skin='fixture',digits=1,validate=function() return 'yes' end}) end)
            assert(not ok)
        end
    )lua");
    f.Call("invalid"); assert(!f.keypad.active && f.opens == 0);
    f.Call();
    std::string error;
    assert(engine::ScriptSystemQueueBackground(f.scripts, "competitor", error));
    f.Tick();
    assert(f.opens == 1 && f.keypad.active);
    f.Key(KEY_ESCAPE); f.Tick();
    assert(!f.keypad.active && f.closes == 1 && f.persistent.bools.at("cancelled"));
    for (const char* function : {"errorUse", "yieldUse", "typeUse"}) {
        f.Call(function); f.Submit("1");
        assert(!f.keypad.active && f.opens == f.closes);
        assert(f.scripts.timers.empty());
    }
    f.Call();
    engine::ScriptSystemStopAll(f.context, f.scripts);
    f.Tick(); // Stop requests run at the scheduler's explicit cancellation phase.
    assert(!f.keypad.active && f.opens == f.closes);
    f.Call();
    game::CancelSectorKeypad(f.host, "player died");
    assert(!f.keypad.active && f.opens == f.closes);
}

void MaterialOverridesAreIsolatedAndRestorable()
{
    engine::ModelAsset model;
    model.materials.resize(3);
    model.materialNames = {"", "Base", "Light"};
    const Vector3 red{1,0.025f,0.019f};
    model.materials[2].emissiveFactor = red;
    game::SectorPropEmissionColors a(3), b(3);
    std::string error;
    assert(game::SetSectorPropEmissionColor(a, model, "Light", Vector3{0,0,1}, error));
    assert(game::SectorPropEmissionFactor(a, 2, red).z == 1);
    assert(game::SectorPropEmissionFactor(b, 2, red).x == 1);
    assert(!a[1] && model.materials[2].emissiveFactor.x == 1);
    const auto saved = game::CaptureSectorPropEmissionColors(a, &model);
    // Named restoration remains correct if material slots are reordered.
    std::swap(model.materialNames[1], model.materialNames[2]);
    game::RestoreSectorPropEmissionColors(b, &model, saved);
    assert(b[1] && b[1]->z == 1 && !b[2]);
    assert(!game::SetSectorPropEmissionColor(a, model, "Missing", Vector3{1,0,0}, error));
    assert(!game::SetSectorPropEmissionColor(a, model, "Light", Vector3{std::numeric_limits<float>::infinity(),0,0}, error));
    assert(game::SetSectorPropEmissionColor(b, model, "Light", std::nullopt, error));
    assert(!b[1]);
}
} // namespace

void RunSectorKeypadTests()
{
    RetryAndStringResults();
    InputAndScaledPicking();
    CancelErrorsAndOwnership();
    MaterialOverridesAreIsolatedAndRestorable();
}
