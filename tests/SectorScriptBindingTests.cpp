#include "engine/EngineContext.h"
#include "engine/scripting/ScriptSystem.h"
#include "game/SectorScriptBindings.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTriggers.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

struct ScriptFiles {
    std::filesystem::path root;
    std::filesystem::path map;
    std::filesystem::path script;

    ScriptFiles()
    {
        const auto unique = std::chrono::steady_clock::now()
                .time_since_epoch().count();
        root = std::filesystem::temp_directory_path()
                / ("engine_sector_lua_" + std::to_string(unique));
        std::filesystem::create_directories(root / "levels" / "test");
        std::filesystem::create_directories(root / "scripts");
        map = root / "levels" / "test" / "test.json";
        script = root / "levels" / "test" / "test.lua";
    }

    ~ScriptFiles()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    void Write(const std::string& source)
    {
        std::ofstream output(script, std::ios::binary);
        output << source;
        assert(output.good());
    }
};

bool Create(
        engine::EngineContext& context,
        engine::ScriptRuntime& runtime,
        engine::PersistentScriptStore& persistent,
        game::SectorScriptHost& host,
        const ScriptFiles& files)
{
    std::string error;
    const bool result = engine::ScriptSystemCreateForMap(
            context,
            runtime,
            persistent,
            "test",
            files.map.string(),
            files.root.string(),
            &host,
            game::RegisterSectorScriptBindings,
            false,
            error);
    if (!result) assert(!error.empty());
    return result;
}

engine::Entity AddDoor(
        engine::EngineContext& context,
        game::SectorRuntimeObjectState& objects)
{
    const engine::Entity entity = context.world.CreateEntity();
    context.world.Add(entity, game::SectorDoor{42, true});
    context.world.Add(entity, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            0.0f,
            1.0f,
            2.0f});
    objects.placedObjectEntities.push_back({42, entity});
    return entity;
}

void DoorCompletionAndCancellationShareTheBackend()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    const engine::Entity door = AddDoor(context, objects);
    ScriptFiles files;

    game::InitializeSectorScriptHost(host, objects, map, runtime);
    files.Write(R"(
function init()
    local ok = moveDoor(42, 1.0, 1000)
    setPersistentBool("door_complete", ok)
end
)");
    assert(Create(context, runtime, persistent, host, files));
    assert(!runtime.initFinished);
    assert(game::AdvanceSectorDoorMotionSystem(context.world, 1.0f));
    game::UpdateSectorScriptOperations(context, host);
    engine::ScriptSystemUpdate(context, runtime, 1.0f);
    assert(runtime.initFinished);
    assert(persistent.bools.at("door_complete"));
    assert(std::fabs(context.world.Get<game::SectorDoorMotion>(door).travelSpeed - 2.0f)
            < 0.0001f);
    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);

    game::SectorDoorMotion& motion = context.world.Get<game::SectorDoorMotion>(door);
    motion.openFraction = 0.0f;
    motion.targetOpenFraction = 0.0f;
    motion.travelSpeed = 2.0f;
    game::InitializeSectorScriptHost(host, objects, map, runtime);
    files.Write(R"(
function init()
    local operation = startMoveDoor(42, 1.0, 1000)
    assert(operation ~= nil)
    assert(cancelOperation(operation))
    local ok, reason = await(operation)
    setPersistentBool("door_cancelled", not ok)
    setPersistentString("door_cancel_reason", reason)
end
)");
    assert(Create(context, runtime, persistent, host, files));
    assert(runtime.initFinished);
    assert(persistent.bools.at("door_cancelled"));
    assert(persistent.strings.at("door_cancel_reason") == "cancelled");
    assert(std::fabs(motion.targetOpenFraction - motion.openFraction) < 0.0001f);
    assert(std::fabs(motion.travelSpeed - 2.0f) < 0.0001f);
    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);
}

void TravelPreservesFirstRequest()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    ScriptFiles files;
    game::InitializeSectorScriptHost(host, objects, map, runtime);
    files.Write(R"(
function init()
    local first = changeMap("next-map", "entry")
    local second, reason = changeMap("ignored")
    setPersistentBool("first", first)
    setPersistentBool("second", second)
    setPersistentString("reason", reason)
end
)");
    assert(Create(context, runtime, persistent, host, files));
    assert(persistent.bools.at("first"));
    assert(!persistent.bools.at("second"));
    assert(runtime.mapChangeRequested);
    assert(runtime.requestedMapId == "next-map");
    assert(runtime.requestedSpawnId == "entry");
    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);
}

game::SectorCompiledTrigger MakeTrigger(
        int editorId,
        const char* id,
        int minX,
        int maxX,
        bool repeat,
        int delayMilliseconds,
        const char* script)
{
    return game::SectorCompiledTrigger{
            editorId,
            id,
            game::SectorTriggerShapeKind::Rectangle,
            {{minX, 0}, {maxX, 0}, {maxX, 32}, {minX, 32}},
            true,
            repeat,
            delayMilliseconds,
            script};
}

void TriggerDispatchDelayRepeatAndEnableControls()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    map.triggers.push_back(MakeTrigger(1, "once", 0, 32, false, 0, "onOnce"));
    map.triggers.push_back(MakeTrigger(2, "repeat", 64, 96, true, 0, "onRepeat"));
    map.triggers.push_back(MakeTrigger(3, "delayed", 128, 160, false, 100, "onDelayed"));
    game::SectorScriptHost host;
    ScriptFiles files;
    game::InitializeSectorScriptHost(host, objects, map, runtime);
    files.Write(R"(
function init()
    assert(disableTrigger("repeat"))
    assert(enableTrigger("repeat"))
end
function onOnce()
    setPersistentInt("once", getPersistentInt("once") + 1)
end
function onRepeat()
    setPersistentInt("repeat", getPersistentInt("repeat") + 1)
end
function onDelayed()
    setPersistentInt("delayed", getPersistentInt("delayed") + 1)
end
)");
    assert(Create(context, runtime, persistent, host, files));

    const auto update = [&](float x, float dt) {
        game::UpdateSectorScriptTriggers(host, Vector2{x, 0.125f}, dt);
        engine::ScriptSystemUpdate(context, runtime, dt);
    };

    update(0.125f, 0.016f);
    assert(persistent.ints.at("once") == 1);
    update(0.125f, 0.016f);
    update(-0.125f, 0.016f);
    update(0.125f, 0.016f);
    assert(persistent.ints.at("once") == 1);

    update(0.625f, 0.016f);
    assert(persistent.ints.at("repeat") == 1);
    update(0.625f, 0.016f);
    update(0.875f, 0.016f);
    update(0.625f, 0.016f);
    assert(persistent.ints.at("repeat") == 2);

    update(1.125f, 0.050f);
    assert(persistent.ints.find("delayed") == persistent.ints.end());
    update(1.5f, 0.050f);
    assert(persistent.ints.find("delayed") == persistent.ints.end());
    update(1.5f, 0.050f);
    assert(persistent.ints.at("delayed") == 1);

    std::string error;
    update(0.875f, 0.016f);
    assert(game::SetSectorScriptTriggerEnabled(host, "repeat", false, error));
    update(0.625f, 0.016f);
    assert(persistent.ints.at("repeat") == 2);
    assert(game::SetSectorScriptTriggerEnabled(host, "repeat", true, error));
    update(0.625f, 0.016f);
    assert(persistent.ints.at("repeat") == 2);
    update(0.875f, 0.016f);
    update(0.625f, 0.016f);
    assert(persistent.ints.at("repeat") == 3);
    assert(!game::SetSectorScriptTriggerEnabled(host, "missing", false, error)
            && !error.empty());

    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);
}

void TriggerContainmentUsesExplicitCoordinateSpaces()
{
    const std::vector<game::SectorTriggerPoint> hubLikeTrigger{
            {-256, 384}, {384, 384}, {384, 512}, {-256, 512}};
    assert(game::SectorTriggerContainsAuthoringPoint(hubLikeTrigger, 0.0f, 28.0f));
    assert(game::SectorTriggerContainsWorldPoint(hubLikeTrigger, 0.0f, 3.5f));
    assert(!game::SectorTriggerContainsWorldPoint(hubLikeTrigger, 0.0f, 28.0f));
}

} // namespace

void RunSectorScriptBindingTests()
{
    DoorCompletionAndCancellationShareTheBackend();
    TravelPreservesFirstRequest();
    TriggerContainmentUsesExplicitCoordinateSpaces();
    TriggerDispatchDelayRepeatAndEnableControls();
}
