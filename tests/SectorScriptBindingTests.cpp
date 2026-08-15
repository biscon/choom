#include "engine/EngineContext.h"
#include "engine/scripting/ScriptSystem.h"
#include "game/SectorScriptBindings.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcNavigationSystem.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTriggers.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

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

void AddNavigationSide(
        game::SectorTopologyMap& map,
        int sideId,
        int lineId,
        int sectorId)
{
    game::SectorTopologySideDef side;
    side.id = sideId;
    side.lineDefId = lineId;
    side.side = game::SectorTopologySideKind::Front;
    side.sectorId = sectorId;
    map.sideDefs.push_back(side);
}

game::SectorTopologyMap MakeNpcNavigationMap()
{
    game::SectorTopologyMap map;
    game::SectorTopologySector sector;
    sector.id = 10;
    sector.floorZ = 0.0f;
    sector.ceilingZ = 32.0f;
    map.sectors.push_back(sector);
    const std::pair<game::SectorCoord, game::SectorCoord> points[] = {
            {0, 0}, {2048, 0}, {2048, 2048}, {0, 2048}};
    for (int index = 0; index < 4; ++index) {
        map.vertices.push_back(game::SectorTopologyVertex{
                index + 1, points[index].first, points[index].second});
    }
    for (int index = 0; index < 4; ++index) {
        const int id = index + 1;
        map.lineDefs.push_back(game::SectorTopologyLineDef{
                id, id, index == 3 ? 1 : id + 1, id, -1});
        AddNavigationSide(map, id, id, 10);
    }
    map.levelMarkers.push_back(game::SectorCompiledLevelMarker{
            1, "run_target", {112.0f, 24.0f, 64.0f}, 0.75f});
    map.levelMarkers.push_back(game::SectorCompiledLevelMarker{
            2, "walk_target", {32.0f, 8.0f, 64.0f}, 2.75f});
    map.levelMarkers.push_back(game::SectorCompiledLevelMarker{
            3, "outside_target", {4000.0f, 0.0f, 64.0f}, 0.0f});
    return map;
}

void FinishNavigationBuild(
        game::SectorNavigationWorld& navigation,
        const game::SectorTopologyMap& map)
{
    for (int iteration = 0; iteration < 1000
            && (navigation.State() == game::SectorNavigationState::Queued
                || navigation.State() == game::SectorNavigationState::Building);
            ++iteration) {
        navigation.UpdateBuild(map, {}, 0);
    }
    assert(navigation.State() == game::SectorNavigationState::Ready);
}

engine::Entity SpawnScriptNpc(engine::World& world)
{
    const engine::Entity entity = world.CreateEntity();
    game::SectorObjectTransform transform;
    transform.position = {2.0f, 0.0f, 8.0f};
    world.Add(entity, transform);
    game::SectorObject object;
    object.currentSectorId = 10;
    world.Add(entity, object);
    world.Add(entity, game::SectorObjectLighting{});
    world.Add(entity, game::SectorObjectVisualOffset{});
    game::SectorDynamicModel dynamicModel;
    dynamicModel.placedObjectId = 700;
    world.Add(entity, dynamicModel);
    game::NpcRuntimeInstance npc;
    npc.definitionId = "script_test";
    npc.instanceId = "script_guard";
    npc.walkSpeed = 2.0f;
    npc.runSpeed = 4.0f;
    world.Add(entity, npc);
    world.Add(entity, game::NpcAnimationState{});
    return entity;
}

struct NpcScriptFixture {
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map = MakeNpcNavigationMap();
    game::SectorNavigationWorld navigation;
    game::NpcNavigationRuntime npcNavigation;
    game::SectorScriptHost host;
    ScriptFiles files;
    engine::Entity npc = engine::NullEntity();

    NpcScriptFixture()
    {
        std::string collisionError;
        objects.objectSectorLookupWorldValid =
                objects.objectSectorLookupWorld.BuildFromTopology(
                        map, &collisionError);
        assert(objects.objectSectorLookupWorldValid);
        assert(navigation.Initialize());
        navigation.RequestRebuild();
        FinishNavigationBuild(navigation, map);
        game::ReserveSectorRuntimeObjectWorld(context.world, 4);
        npc = SpawnScriptNpc(context.world);
        game::InitializeNpcNavigationRuntime(
                context.world, navigation, npcNavigation);
        game::InitializeSectorScriptHost(
                host,
                objects,
                map,
                runtime,
                &navigation,
                &npcNavigation);
    }

    ~NpcScriptFixture()
    {
        if (runtime.vm != nullptr) {
            engine::ScriptSystemShutdownForMap(context, runtime);
        }
        game::ResetSectorScriptHost(host);
        game::ShutdownNpcNavigationRuntime(
                context.world, navigation, npcNavigation);
        navigation.Shutdown();
    }

    void Update(float dt)
    {
        navigation.UpdateDynamicObstacles(
                objects.dynamicModelColliders,
                dt);
        game::UpdateNpcNavigationAndLocomotionSystem(
                context.world,
                context.assets,
                navigation,
                npcNavigation,
                objects.npcDefinitionCatalog,
                objects.objectSectorLookupWorld,
                objects.dynamicDoorColliders,
                objects.staticModelColliders,
                objects.objectLightProbes,
                map,
                dt);
        game::UpdateSectorScriptOperations(context, host);
        engine::ScriptSystemUpdate(context, runtime, dt);
    }
};

void BlockingNpcMoveCompletesAfterPhysicalArrival()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    local ok, reason = moveNpc("script_guard", 14.0, 8.0, "run")
    setPersistentBool("move_ok", ok)
    setPersistentString("move_reason", reason or "")
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));
    assert(!fixture.runtime.initFinished);
    assert(fixture.host.npcMoves.size() == 1);
    assert(fixture.persistent.bools.find("move_ok")
            == fixture.persistent.bools.end());
    for (int frame = 0; frame < 500 && !fixture.runtime.initFinished; ++frame) {
        fixture.Update(0.05f);
    }
    assert(fixture.runtime.initFinished);
    assert(fixture.persistent.bools.at("move_ok"));
    assert(fixture.persistent.strings.at("move_reason").empty());
    const game::NpcMoveStatus status = game::GetNpcMoveStatus(
            fixture.npcNavigation, "script_guard");
    assert(status.phase == game::NpcMovePhase::Arrived);
    assert(status.authority == game::NpcMoveAuthority::Script);
    assert(status.requestId != 0);
    assert(std::fabs(
            fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc)
                    .position.x - 14.0f) < 0.11f);
    assert(fixture.host.npcMoveDiagnostics.successes == 1);
}

void BackgroundNpcPatrolYieldsWhenNavigationIsPrepared()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    assert(startScript("patrol"))
end
function patrol()
    while true do
        moveNpc("script_guard", "run_target", "run")
        moveNpc("script_guard", "walk_target", "walk")
    end
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));
    assert(fixture.runtime.initFinished);
    fixture.Update(0.016f);

    const std::vector<engine::ScriptTaskSnapshot> tasks =
            engine::ScriptSystemTaskSnapshot(fixture.runtime);
    const auto patrol = std::find_if(
            tasks.begin(),
            tasks.end(),
            [](const engine::ScriptTaskSnapshot& task) {
                return task.functionName == "patrol";
            });
    assert(patrol != tasks.end());
    assert(patrol->state == engine::ScriptTaskState::Waiting);
    assert(patrol->operationLabel == "moveNpc:script_guard");
}

void BlockingNpcMoveReplansAfterDynamicObstacleChange()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    local ok, reason = moveNpc("script_guard", 14.0, 8.0)
    setPersistentBool("dynamic_move_done", true)
    setPersistentBool("dynamic_move_ok", ok)
    setPersistentString("dynamic_move_reason", reason or "")
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));

    game::SectorStaticModelCollider obstacle;
    obstacle.placedObjectId = 777;
    obstacle.center = {8.0f, 8.0f};
    obstacle.axisX = {1.0f, 0.0f};
    obstacle.axisZ = {0.0f, 1.0f};
    obstacle.halfExtents = {0.75f, 2.5f};
    obstacle.bottom = 0.0f;
    obstacle.top = 2.0f;
    obstacle.resolved = true;
    fixture.objects.dynamicModelColliders = {obstacle};
    fixture.objects.staticModelColliders = {obstacle};
    for (int frame = 0; frame < 1500
            && fixture.persistent.bools.find("dynamic_move_done")
                    == fixture.persistent.bools.end();
            ++frame) {
        fixture.Update(0.016f);
    }
    assert(fixture.persistent.bools.at("dynamic_move_done"));
    assert(fixture.persistent.bools.at("dynamic_move_ok"));
    assert(fixture.persistent.strings.at("dynamic_move_reason").empty());
    assert(game::GetNpcMoveStatus(
            fixture.npcNavigation, "script_guard").replanCount > 0);
}

void NpcMoveLevelMarkerOverloadsResolvePositionOnly()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    local empty, emptyReason = startMoveNpc("script_guard", "")
    setPersistentBool("empty_rejected", empty == nil)
    setPersistentString("empty_reason", emptyReason)

    local missing, missingReason = startMoveNpc("script_guard", "missing")
    setPersistentBool("missing_rejected", missing == nil)
    setPersistentString("missing_reason", missingReason)

    local badGait, badGaitReason = startMoveNpc(
            "script_guard", "run_target", "sprint")
    setPersistentBool("marker_gait_rejected", badGait == nil)
    setPersistentString("marker_gait_reason", badGaitReason)

    local outside, outsideReason = startMoveNpc(
            "script_guard", "outside_target")
    setPersistentBool("marker_outside_rejected", outside == nil)
    setPersistentString("marker_outside_reason", outsideReason)

    local blockingOk, blockingReason = moveNpc(
            "script_guard", "run_target", "run")
    setPersistentBool("marker_blocking_ok", blockingOk)
    setPersistentString("marker_blocking_reason", blockingReason or "")

    local movement, movementReason = startMoveNpc(
            "script_guard", "walk_target")
    assert(movement ~= nil, movementReason)
    local asyncOk, asyncReason = await(movement)
    setPersistentBool("marker_async_ok", asyncOk)
    setPersistentString("marker_async_reason", asyncReason or "")
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));
    assert(!fixture.runtime.initFinished);
    game::NpcMoveStatus status = game::GetNpcMoveStatus(
            fixture.npcNavigation, "script_guard");
    assert(status.phase == game::NpcMovePhase::FollowingPath);
    assert(status.gait == game::NpcMoveGait::Run);
    assert(std::fabs(status.requestedDestinationXZ.x - 14.0f) < 0.001f);
    assert(std::fabs(status.requestedDestinationXZ.y - 8.0f) < 0.001f);

    for (int frame = 0; frame < 1000 && !fixture.runtime.initFinished; ++frame) {
        fixture.Update(0.05f);
    }
    assert(fixture.runtime.initFinished);
    assert(fixture.persistent.bools.at("empty_rejected"));
    assert(fixture.persistent.strings.at("empty_reason").find("empty")
            != std::string::npos);
    assert(fixture.persistent.bools.at("missing_rejected"));
    assert(fixture.persistent.strings.at("missing_reason").find("not found")
            != std::string::npos);
    assert(fixture.persistent.bools.at("marker_gait_rejected"));
    assert(fixture.persistent.strings.at("marker_gait_reason").find("gait")
            != std::string::npos);
    assert(fixture.persistent.bools.at("marker_outside_rejected"));
    assert(fixture.persistent.strings.at("marker_outside_reason").find("outside")
            != std::string::npos);
    assert(fixture.persistent.bools.at("marker_blocking_ok"));
    assert(fixture.persistent.strings.at("marker_blocking_reason").empty());
    assert(fixture.persistent.bools.at("marker_async_ok"));
    assert(fixture.persistent.strings.at("marker_async_reason").empty());

    status = game::GetNpcMoveStatus(
            fixture.npcNavigation, "script_guard");
    assert(status.phase == game::NpcMovePhase::Arrived);
    assert(status.gait == game::NpcMoveGait::Walk);
    assert(std::fabs(status.requestedDestinationXZ.x - 4.0f) < 0.001f);
    assert(std::fabs(status.requestedDestinationXZ.y - 8.0f) < 0.001f);
    const game::SectorObjectTransform& transform =
            fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc);
    assert(std::fabs(transform.position.x - 4.0f) < 0.11f);
    assert(std::fabs(transform.position.z - 8.0f) < 0.11f);
    assert(std::fabs(transform.position.y) < 0.001f);
    assert(std::fabs(transform.yawRadians - 2.75f) > 0.25f);
}

void AsyncNpcMoveSupportsAwaitDuplicateValidationAndCancellation()
{
    NpcScriptFixture fixture;
    std::vector<game::SectorScriptNpcMove>{}.swap(fixture.host.npcMoves);
    fixture.files.Write(R"(
local movement
function init()
    movement = startMoveNpc("script_guard", 12.0, 8.0)
    assert(movement ~= nil)
    setPersistentString("pending", operationStatus(movement))
    local duplicate, duplicateReason = startMoveNpc("script_guard", 10.0, 8.0)
    setPersistentBool("duplicate_rejected", duplicate == nil)
    setPersistentString("duplicate_reason", duplicateReason)
    local invalid, invalidReason = startMoveNpc("", 10.0, 8.0)
    setPersistentBool("invalid_rejected", invalid == nil)
    setPersistentString("invalid_reason", invalidReason)
    local badGait, badGaitReason = startMoveNpc("script_guard", 10.0, 8.0, "sprint")
    setPersistentBool("gait_rejected", badGait == nil)
    setPersistentString("gait_reason", badGaitReason)
end
function awaitMovement()
    local ok, reason = await(movement)
    setPersistentBool("await_ok", ok)
    setPersistentString("await_reason", reason or "")
end
function cancelMovement()
    local cancelled = cancelOperation(movement)
    setPersistentBool("cancelled", cancelled)
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));
    assert(fixture.runtime.initFinished);
    assert(fixture.persistent.strings.at("pending") == "pending");
    assert(fixture.persistent.bools.at("duplicate_rejected"));
    assert(fixture.persistent.strings.at("duplicate_reason").find("scripted")
            != std::string::npos);
    assert(fixture.persistent.bools.at("invalid_rejected"));
    assert(fixture.persistent.bools.at("gait_rejected"));
    assert(fixture.persistent.strings.at("gait_reason").find("gait")
            != std::string::npos);
    assert(fixture.host.npcMoveDiagnostics.capacityWarnings == 1);

    const engine::ScriptCallOutcome awaitOutcome =
            engine::ScriptSystemCallForegroundHook(
                    fixture.runtime, "awaitMovement");
    assert(awaitOutcome.result == engine::ScriptCallResult::Started);
    for (int frame = 0;
            frame < 500
                && fixture.persistent.bools.find("await_ok")
                        == fixture.persistent.bools.end();
            ++frame) {
        fixture.Update(0.05f);
    }
    assert(fixture.persistent.bools.at("await_ok"));

}

void AsyncNpcMoveCancellationAndLifecycleFailuresResolve()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
local movement
function init()
    movement = startMoveNpc("script_guard", 12.0, 8.0, "walk")
    assert(movement ~= nil)
end
function cancelMovement()
    setPersistentBool("cancelled", cancelOperation(movement))
    local ok, reason = await(movement)
    setPersistentBool("cancel_ok", ok)
    setPersistentString("cancel_reason", reason)
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));
    assert(engine::ScriptSystemCallForegroundHook(
            fixture.runtime, "cancelMovement").result
            == engine::ScriptCallResult::Completed);
    assert(fixture.persistent.bools.at("cancelled"));
    assert(!fixture.persistent.bools.at("cancel_ok"));
    assert(fixture.persistent.strings.at("cancel_reason") == "cancelled");
    assert(game::GetNpcMoveStatus(
            fixture.npcNavigation, "script_guard").phase
            == game::NpcMovePhase::Cancelled);

}

void NpcMoveRebuildDeletionUnloadAndImmediateFailuresResolve()
{
    {
        NpcScriptFixture fixture;
        fixture.files.Write(R"(
local movement
function init()
    movement = startMoveNpc("script_guard", 12.0, 8.0)
    assert(movement ~= nil)
end
function inspectMovement()
    local state, reason = operationStatus(movement)
    setPersistentString("state", state)
    setPersistentString("reason", reason or "")
end
)");
        assert(Create(
                fixture.context,
                fixture.runtime,
                fixture.persistent,
                fixture.host,
                fixture.files));
        fixture.navigation.RequestRebuild();
        fixture.Update(0.016f);
        assert(engine::ScriptSystemCallForegroundHook(
                fixture.runtime, "inspectMovement").result
                == engine::ScriptCallResult::Completed);
        assert(fixture.persistent.strings.at("state") == "failed");
        assert(fixture.persistent.strings.at("reason").find("navigation")
                != std::string::npos);
    }

    {
        NpcScriptFixture fixture;
        fixture.files.Write(R"(
local movement
function init()
    movement = startMoveNpc("script_guard", 12.0, 8.0)
    assert(movement ~= nil)
end
function inspectMovement()
    local state, reason = operationStatus(movement)
    setPersistentString("state", state)
    setPersistentString("reason", reason or "")
end
)");
        assert(Create(
                fixture.context,
                fixture.runtime,
                fixture.persistent,
                fixture.host,
                fixture.files));
        fixture.context.world.DestroyLater(fixture.npc);
        fixture.context.world.FlushDestroyedEntities();
        fixture.Update(0.016f);
        assert(engine::ScriptSystemCallForegroundHook(
                fixture.runtime, "inspectMovement").result
                == engine::ScriptCallResult::Completed);
        assert(fixture.persistent.strings.at("state") == "failed");
        assert(fixture.persistent.strings.at("reason").find("removed")
                != std::string::npos);
    }

    {
        NpcScriptFixture fixture;
        fixture.files.Write(R"(
function init()
    local outside, outsideReason = startMoveNpc("script_guard", 500.0, 8.0)
    setPersistentBool("outside_rejected", outside == nil)
    setPersistentString("outside_reason", outsideReason)
end
)");
        assert(Create(
                fixture.context,
                fixture.runtime,
                fixture.persistent,
                fixture.host,
                fixture.files));
        assert(fixture.persistent.bools.at("outside_rejected"));
        assert(fixture.persistent.strings.at("outside_reason").find("outside")
                != std::string::npos);
    }

    {
        NpcScriptFixture fixture;
        fixture.files.Write(R"(
function init()
    local movement = startMoveNpc("script_guard", 12.0, 8.0)
    assert(movement ~= nil)
end
)");
        assert(Create(
                fixture.context,
                fixture.runtime,
                fixture.persistent,
                fixture.host,
                fixture.files));
        engine::ScriptSystemShutdownForMap(fixture.context, fixture.runtime);
        assert(game::GetNpcMoveStatus(
                fixture.npcNavigation, "script_guard").phase
                == game::NpcMovePhase::Cancelled);
        assert(fixture.host.npcMoveDiagnostics.cancellations == 1);
    }
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
    BlockingNpcMoveCompletesAfterPhysicalArrival();
    BackgroundNpcPatrolYieldsWhenNavigationIsPrepared();
    BlockingNpcMoveReplansAfterDynamicObstacleChange();
    NpcMoveLevelMarkerOverloadsResolvePositionOnly();
    AsyncNpcMoveSupportsAwaitDuplicateValidationAndCancellation();
    AsyncNpcMoveCancellationAndLifecycleFailuresResolve();
    NpcMoveRebuildDeletionUnloadAndImmediateFailuresResolve();
    TravelPreservesFirstRequest();
    TriggerContainmentUsesExplicitCoordinateSpaces();
    TriggerDispatchDelayRepeatAndEnableControls();
}
