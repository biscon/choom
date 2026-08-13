#include "engine/EngineContext.h"
#include "engine/scripting/ScriptSystem.h"
#include "game/SectorScriptBindings.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"

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

} // namespace

void RunSectorScriptBindingTests()
{
    DoorCompletionAndCancellationShareTheBackend();
    TravelPreservesFirstRequest();
}
