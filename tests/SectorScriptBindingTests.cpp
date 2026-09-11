#include "engine/EngineContext.h"
#include "engine/scripting/ScriptConsole.h"
#include "engine/scripting/ScriptSystem.h"
#include "engine/systems/AnimatedModelSystem.h"
#include "lua.hpp"
#include "game/Health.h"
#include "game/SectorScriptBindings.h"
#include "game/cutscene/SectorCutsceneRuntime.h"
#include "game/navigation/SectorNavigationWorld.h"
#include "game/npc/NpcCombatSystem.h"
#include "game/npc/NpcNavigationSystem.h"
#include "game/npc/NpcHeadLookSystem.h"
#include "game/npc/NpcPatrolSystem.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTriggers.h"

#include <raymath.h>
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
    context.world.Add(entity, game::SectorDoor{42, true, "test_door"});
    context.world.Add(entity, game::SectorDoorMotion{
            game::SectorDoorMotionType::SlideVertical,
            0.0f,
            0.0f,
            1.0f,
            2.0f});
    context.world.Add(entity, game::SectorDoorInteraction{});
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
    world.Add(entity, game::MakeHealth(25));
    world.Add(entity, game::NpcCombatState{});
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
    game::SectorCutsceneRuntime cutscene;
    game::SectorFpsControllerState playerState;
    game::SectorFpsControllerConfig playerConfig;
    game::Health playerHealth = game::MakeHealth(100);
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
        game::InitializeSectorCutsceneRuntime(cutscene);
        playerState.feetPosition = {2.0f, 0.0f, 8.0f};
        playerState.currentSectorId = 10;
        playerState.grounded = true;
        game::InitializeSectorScriptHost(
                host,
                objects,
                map,
                runtime,
                &navigation,
                &npcNavigation,
                {},
                &playerHealth,
                &cutscene,
                &playerState,
                &playerConfig);
    }

    ~NpcScriptFixture()
    {
        if (runtime.vm != nullptr) {
            engine::ScriptSystemShutdownForMap(context, runtime);
        }
        game::ResetSectorScriptHost(host);
        game::ResetSectorCutsceneRuntime(cutscene, &navigation);
        game::ShutdownNpcNavigationRuntime(
                context.world, navigation, npcNavigation);
        navigation.Shutdown();
    }

    // Supply a generated visual target to test camera math without GPU assets.
    void Update(float dt, const Vector3* arrivalTarget = nullptr)
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
                dt, nullptr, false, &playerState.feetPosition);
        game::UpdateSectorScriptOperations(context, host);
        game::UpdateSectorCutsceneTimelines(cutscene, runtime, dt);
        const Vector2 previous{playerState.feetPosition.x, playerState.feetPosition.z};
        const Vector2 delta = game::BuildSectorCutscenePlayerMoveDelta(
                cutscene, playerState, dt, nullptr);
        playerState.feetPosition.x += delta.x;
        playerState.feetPosition.z += delta.y;
        if (arrivalTarget != nullptr) {
            assert(game::AdvanceSectorCutscenePlayerCamera(cutscene, playerState,
                    game::SectorFpsControllerEyePosition(playerState, playerConfig),
                    arrivalTarget, dt));
        } else {
            game::UpdateSectorCutscenePlayerCamera(cutscene, navigation,
                    context.world, context.assets, playerState, playerConfig,
                    runtime, dt);
        }
        game::FinishSectorCutscenePlayerMoveFrame(
                cutscene,
                navigation,
                objects.objectSectorLookupWorld,
                playerState,
                previous,
                runtime,
                dt);
        engine::ScriptSystemUpdate(context, runtime, dt);
        game::UpdateSectorScriptCutsceneControlOwnership(context, host);
    }
};

void NpcAnimationBindingsRejectInvalidRequestsWithoutMutation()
{
    NpcScriptFixture fixture;
    fixture.context.world.Add(fixture.npc, engine::AnimatedModelInstance{});
    fixture.context.world.Add(fixture.npc, engine::AnimatedModelAnimator{});
    auto& animation = fixture.context.world.Get<game::NpcAnimationState>(fixture.npc);
    animation.resolved = true;
    animation.scriptLoopIndex = 1;
    animation.scriptLoopSpeed = 0.7f;
    fixture.files.Write(R"(
function init()
    assert(type(setNpcAnimation) == "function")
    assert(type(playNpcAnimation) == "function")
    assert(type(startPlayNpcAnimation) == "function")
    assert(startSetNpcAnimation == nil)
    local ok, reason = setNpcAnimation("missing", "Waving")
    assert(ok == false and reason:find("not found"))
    ok, reason = playNpcAnimation("missing", "Waving")
    assert(ok == false and reason:find("not found"))
    local operation
    operation, reason = startPlayNpcAnimation("missing", "Waving")
    assert(operation == nil and reason:find("not found"))
    ok, reason = setNpcAnimation("script_guard", "Waving")
    assert(ok == false and reason:find("not ready"))
    for _, value in ipairs({0, -1, math.huge, 0/0}) do
        ok, reason = setNpcAnimation("script_guard", "Waving", value)
        assert(ok == false and reason:find("positive"))
        operation, reason = startPlayNpcAnimation("script_guard", "Waving", value)
        assert(operation == nil and reason:find("positive"))
    end
    assert(not pcall(setNpcAnimation, {}, "Idle"))
    assert(not pcall(startPlayNpcAnimation, "script_guard", {}))
    setPersistentBool("animation_validation_passed", true)
end
)");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
    assert(fixture.persistent.bools.at("animation_validation_passed"));
    assert(animation.scriptLoopIndex == 1 && animation.scriptLoopSpeed == 0.7f);
    assert(fixture.host.npcAnimations.empty());
    const auto console = engine::ScriptSystemExecuteConsole(fixture.runtime,
            "playNpcAnimation('script_guard', 'Waving')");
    assert(!console.success);
    assert(animation.scriptLoopIndex == 1);
}

void NpcAnimationOperationsCompleteReplaceCancelAndUnload()
{
    NpcScriptFixture fixture;
    fixture.context.world.Add(fixture.npc, engine::AnimatedModelAnimator{});
    auto& animation = fixture.context.world.Get<game::NpcAnimationState>(fixture.npc);
    auto& animator = fixture.context.world.Get<engine::AnimatedModelAnimator>(fixture.npc);
    animation.resolved = true;
    animation.animationIndices[0] = 0;
    animation.animationSpeeds[0] = 0.8f;
    animator.animationIndex = 0;
    fixture.files.Write(R"(
function init() end
function waitForWave()
    local ok, reason = await(wave)
    setPersistentBool("wave_done", true)
    setPersistentBool("wave_ok", ok)
    setPersistentString("wave_reason", reason or "")
end
function cancelWave()
    assert(cancelOperation(wave))
    local ok, reason = await(wave)
    assert(not ok and reason ~= nil)
end
)");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
    const auto start = [&]() {
        const auto operation = game::BeginSectorScriptNpcAnimation(fixture.context,
                fixture.host, fixture.npc, {2, 1.0f, 1.0f},
                engine::ScriptOperationLaunchStyle::Async, {});
        assert(engine::IsValid(operation));
        engine::ScriptSystemPushOperationUserdata(fixture.runtime.vm, operation);
        lua_setglobal(fixture.runtime.vm, "wave");
        return operation;
    };
    const auto status = [&](engine::ScriptOperationHandle operation) {
        return fixture.runtime.operations[operation.index].state;
    };
    game::SetNpcScriptAnimation(animation, animator, 1, 0.6f, true, 0, 99);
    auto operation = start();
    assert(engine::ScriptSystemCallForegroundHook(fixture.runtime, "waitForWave").result
            == engine::ScriptCallResult::Started);
    assert(!fixture.persistent.bools.count("wave_done"));
    // Completion must not depend on navigation being installed in the script host.
    fixture.host.navigation = nullptr;
    fixture.host.npcNavigation = nullptr;
    animator.animationIndex = 2;
    animator.targetAnimationIndex = engine::InvalidModelAnimationIndex;
    animator.finished = true;
    animator.playing = false;
    game::UpdateSectorScriptOperations(fixture.context, fixture.host);
    assert(status(operation) == engine::ScriptOperationState::Succeeded);
    assert(animator.targetAnimationIndex == 1 && animator.speed == 0.6f);
    engine::ScriptSystemUpdate(fixture.context, fixture.runtime, 0.016f);
    assert(fixture.persistent.bools.at("wave_done") && fixture.persistent.bools.at("wave_ok"));

    operation = start();
    const auto oldCancel = fixture.runtime.operations[operation.index].cancelBackend;
    const uint64_t oldToken = fixture.runtime.operations[operation.index].backendToken;
    auto replacement = start();
    assert(status(operation) == engine::ScriptOperationState::Cancelled);
    assert(status(replacement) == engine::ScriptOperationState::Pending);
    const uint64_t newToken = animation.scriptRequestId;
    oldCancel(fixture.context, &fixture.host, oldToken);
    assert(animation.scriptRequestId == newToken
            && animation.scriptStatus == game::NpcScriptAnimationStatus::Playing);
    assert(engine::ScriptSystemCallForegroundHook(fixture.runtime, "cancelWave").result
            == engine::ScriptCallResult::Completed);
    assert(animation.scriptLoopIndex == 1 && animator.speed == 0.6f);

    operation = start();
    fixture.context.world.Get<game::NpcCombatState>(fixture.npc).hurtAnimationRequested = true;
    game::UpdateSectorScriptOperations(fixture.context, fixture.host);
    assert(status(operation) == engine::ScriptOperationState::Cancelled);
    assert(animation.scriptLoopIndex == engine::InvalidModelAnimationIndex);
    fixture.context.world.Get<game::NpcCombatState>(fixture.npc).hurtAnimationRequested = false;

    operation = start();
    engine::ScriptSystemShutdownForMap(fixture.context, fixture.runtime);
    assert(animation.scriptStatus == game::NpcScriptAnimationStatus::Cancelled);
    assert(std::none_of(fixture.host.npcAnimations.begin(), fixture.host.npcAnimations.end(),
            [](const game::SectorScriptNpcAnimation& playback) { return playback.active; }));
}

void NpcMovementClearsAnimationOverridesOnlyWhenAccepted()
{
    for (const std::string destination : {"12.0, 8.0", "\"run_target\""}) {
        NpcScriptFixture fixture;
        fixture.context.world.Add(fixture.npc, engine::AnimatedModelAnimator{});
        fixture.context.world.Add(fixture.npc, engine::AnimatedModelInstance{});
        auto& animation = fixture.context.world.Get<game::NpcAnimationState>(fixture.npc);
        auto& animator = fixture.context.world.Get<engine::AnimatedModelAnimator>(fixture.npc);
        animation.resolved = true;
        animation.animationIndices = {0, 1, 1, 2, 2, 2};
        animator.animationIndex = 0;
        fixture.files.Write("function init() end\nfunction moveGuard()\n"
                "  assert(startMoveNpc('script_guard', " + destination + "))\n"
                "  local ok, reason = setNpcAnimation('script_guard', 'Waving')\n"
                "  assert(not ok and reason:find('movement'))\nend\n");
        assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
        game::SetNpcScriptAnimation(animation, animator, 1, 0.7f, true, 0, 42);
        const auto wave = game::BeginSectorScriptNpcAnimation(fixture.context, fixture.host,
                fixture.npc, {2, 1.0f, 1.0f}, engine::ScriptOperationLaunchStyle::Async, {});
        const auto rejected = game::RequestNpcMove(fixture.context.world, fixture.navigation,
                fixture.objects.objectSectorLookupWorld, fixture.npcNavigation,
                "script_guard", {-1000.0f, -1000.0f}, game::NpcMoveGait::Walk);
        assert(!rejected.accepted && animation.scriptLoopIndex == 1
                && animation.scriptStatus == game::NpcScriptAnimationStatus::Playing);
        assert(engine::ScriptSystemCallForegroundHook(fixture.runtime, "moveGuard").result
                == engine::ScriptCallResult::Completed);
        assert(animation.scriptLoopIndex == engine::InvalidModelAnimationIndex
                && animation.scriptStatus == game::NpcScriptAnimationStatus::Cancelled);
        game::UpdateSectorScriptOperations(fixture.context, fixture.host);
        assert(fixture.runtime.operations[wave.index].state == engine::ScriptOperationState::Cancelled);
        for (int i = 0; i < 500 && game::GetNpcMoveStatus(fixture.npcNavigation,
                "script_guard").phase == game::NpcMovePhase::FollowingPath; ++i) {
            fixture.Update(0.05f);
        }
        assert(game::GetNpcMoveStatus(fixture.npcNavigation, "script_guard").phase
                == game::NpcMovePhase::Arrived);
        assert(animation.scriptLoopIndex == engine::InvalidModelAnimationIndex);
        assert(fixture.context.world.Get<game::NpcRuntimeInstance>(fixture.npc).action
                == game::NpcAction::Idle);
        assert(animator.loop || animator.targetLoop);
    }
}

void NpcAnimationRemovalResolvesOperation()
{
    NpcScriptFixture fixture;
    fixture.context.world.Add(fixture.npc, engine::AnimatedModelAnimator{});
    fixture.files.Write("function init() end");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
    const auto operation = game::BeginSectorScriptNpcAnimation(fixture.context, fixture.host,
            fixture.npc, {2, 1.0f, 1.0f}, engine::ScriptOperationLaunchStyle::Async, {});
    fixture.context.world.DestroyLater(fixture.npc);
    fixture.context.world.FlushDestroyedEntities();
    game::UpdateSectorScriptOperations(fixture.context, fixture.host);
    assert(fixture.runtime.operations[operation.index].state == engine::ScriptOperationState::Failed);
}

void HealthBindingsSetPlayerAndNpcCurrentHealth()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    assert(setPlayerHealth(63))
    assert(setNpcHealth("script_guard", 12))
    local missing, reason = setNpcHealth("missing_guard", 10)
    setPersistentBool("missing_rejected", not missing)
    setPersistentString("missing_reason", reason)
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));
    assert(fixture.playerHealth.current == 63);
    assert(fixture.context.world.Get<game::Health>(fixture.npc).current == 12);
    assert(!fixture.context.world.Get<game::NpcCombatState>(fixture.npc).dead);
    assert(fixture.persistent.bools.at("missing_rejected"));
    assert(fixture.persistent.strings.at("missing_reason").find("not found")
            != std::string::npos);
}

void SettingNpcHealthToZeroUsesNpcDeathState()
{
    NpcScriptFixture fixture;
    assert(!fixture.context.world.Get<game::NpcRuntimeInstance>(fixture.npc).hostile);
    fixture.files.Write(R"(
function init()
    assert(setNpcHealth("script_guard", 0))
    local revived, reason = setNpcHealth("script_guard", 1)
    setPersistentBool("revive_rejected", not revived)
    setPersistentString("revive_reason", reason)
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));
    assert(game::IsDepleted(
            fixture.context.world.Get<game::Health>(fixture.npc)));
    const game::NpcCombatState& combat =
            fixture.context.world.Get<game::NpcCombatState>(fixture.npc);
    assert(combat.dead);
    assert(combat.deathAnimationRequested);
    assert(!fixture.npcNavigation.records.front().occupied);
    assert(fixture.persistent.bools.at("revive_rejected"));
    assert(fixture.persistent.strings.at("revive_reason").find("cannot be revived")
            != std::string::npos);
}

void BlockingNpcMoveCompletesAfterPhysicalArrival()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    local ok, reason = moveNpc("script_guard", 14.0, 8.0, "run", 8.0)
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
    assert(std::fabs(fixture.npcNavigation.records.front()
            .movementSpeedOverride - 8.0f) < 0.001f);
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

void AsyncPlayerMoveUsesNavigationMarkerAndSpeedOverride()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    local movement, reason = startMovePlayer("run_target", "walk", 1.25)
    assert(movement ~= nil, reason)
    setPersistentString("player_move_pending", operationStatus(movement))
    local ok, failure = await(movement)
    setPersistentBool("player_move_ok", ok)
    setPersistentString("player_move_reason", failure or "")
end
)");
    assert(Create(
            fixture.context,
            fixture.runtime,
            fixture.persistent,
            fixture.host,
            fixture.files));
    assert(!fixture.runtime.initFinished);
    assert(fixture.cutscene.playerMove.active);
    assert(std::fabs(fixture.cutscene.playerMove.movementSpeed - 1.25f)
            < 0.001f);
    for (int frame = 0; frame < 1000 && !fixture.runtime.initFinished; ++frame) {
        fixture.Update(0.025f);
    }
    assert(fixture.runtime.initFinished);
    assert(fixture.persistent.strings.at("player_move_pending") == "pending");
    assert(fixture.persistent.bools.at("player_move_ok"));
    assert(fixture.persistent.strings.at("player_move_reason").empty());
    assert(std::fabs(fixture.playerState.feetPosition.x - 14.0f) < 0.11f);
    assert(std::fabs(fixture.playerState.feetPosition.z - 8.0f) < 0.11f);
}

void PlayerRouteCameraAnticipatesAndSmoothsWithoutChangingMovement()
{
    game::SectorCutsceneRuntime runtime;
    auto& move = runtime.playerMove;
    move.active = true;
    move.movementSpeed = 2.0f;
    move.cornerCount = 2;
    move.corners[0] = {1.0f, 0.0f, 0.0f};
    move.corners[1] = {1.0f, 0.0f, 5.0f};
    game::SectorFpsControllerState player;
    player.feetPosition = {0.8f, 0.0f, 0.0f};
    player.yawRadians = 0.0f;
    float facing = 0.0f;
    const Vector2 movement = game::BuildSectorCutscenePlayerMoveDelta(
            runtime, player, 0.025f, &facing);
    assert(std::fabs(movement.x - 0.05f) < 0.0001f && movement.y == 0.0f);
    assert(facing > 0.5f); // Camera anticipates the corner; feet still go straight.
    assert(game::AdvanceSectorCutscenePlayerCamera(
            runtime, player, {}, nullptr, 0.025f));
    assert(player.yawRadians > 0.0f && player.yawRadians < facing);

    move.cornerDoorIds[0] = 42;
    game::BuildSectorCutscenePlayerMoveDelta(runtime, player, 0.025f, &facing);
    assert(facing == 0.0f); // Do not anticipate beyond the pending door.
    move.cornerDoorIds[0] = 0;
    runtime.look.active = true;
    const float beforeLook = player.yawRadians;
    game::AdvanceSectorCutscenePlayerCamera(runtime, player, {}, nullptr, 0.1f);
    assert(player.yawRadians == beforeLook && move.facingVelocity == 0.0f);
    runtime.look.active = false;

    auto turn = [](int frames, float dt, float start, Vector3 destination) {
        game::SectorCutsceneRuntime test;
        test.playerMove.active = true;
        test.playerMove.movementSpeed = 2.0f;
        test.playerMove.cornerCount = 1;
        test.playerMove.corners[0] = destination;
        game::SectorFpsControllerState camera;
        camera.yawRadians = start;
        const float target = start + std::remainder(
                std::atan2(destination.z, destination.x) - start, 2.0f * PI);
        for (int i = 0; i < frames; ++i) {
            const float previous = camera.yawRadians;
            game::AdvanceSectorCutscenePlayerCamera(test, camera, {}, nullptr, dt);
            assert(std::isfinite(camera.yawRadians));
            assert(std::fabs(camera.yawRadians - previous) <= 2.0f * PI * dt + 0.00001f);
            assert(camera.yawRadians >= std::min(start, target) - 0.00001f);
            assert(camera.yawRadians <= std::max(start, target) + 0.00001f);
        }
        return camera.yawRadians;
    };
    const Vector3 slightTurn{5.0f, 0.0f, 2.0f};
    assert(std::fabs(turn(30, 1.0f / 30.0f, 0.0f, slightTurn)
            - turn(144, 1.0f / 144.0f, 0.0f, slightTurn)) < 0.0001f);
    const float wrapped = turn(60, 1.0f / 60.0f, PI - 0.1f, {-5.0f, 0.0f, -0.5f});
    assert(wrapped > PI && wrapped < PI + 0.11f);
    turn(1, 2.0f, 0.0f, {-5.0f, 0.0f, 0.1f}); // Long frame remains stable.
    const float unchanged = player.yawRadians;
    game::AdvanceSectorCutscenePlayerCamera(runtime, player, {}, nullptr, -1.0f);
    game::AdvanceSectorCutscenePlayerCamera(runtime, player, {}, nullptr, NAN);
    assert(player.yawRadians == unchanged);
}

void PlayerArrivalLookBindingsValidateAndReserveOwnership()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    local invalid = {
        false, {}, {lookAtNpc="missing"}, {lookAtNpc=""},
        {lookAtNpc="script_guard", lookAtProp="anything"},
        {lookAtNpc="script_guard", turnDurationMs=0},
        {lookAtNpc="script_guard", turnDurationMs=math.huge},
        {lookAtNpc="script_guard", turnDurationMs="750"},
        {lookAtNpc="script_guard", targetHeight=-1},
        {lookAtNpc="script_guard", targetHeight=0/0},
    }
    for _, options in ipairs(invalid) do
        local op, reason = startMovePlayer("run_target", nil, nil, options)
        assert(op == nil and #reason > 0)
    end
    wave = assert(startMovePlayer("run_target", nil, nil, {lookAtNpc="script_guard"}))
    local look, reason = startLookAtNpc("script_guard", 100)
    assert(look == nil and #reason > 0)
    assert(cancelOperation(wave))
    look = assert(startLookAtNpc("script_guard", 100))
    local move = startMovePlayer(4, 8, "walk", 2, {lookAtNpc="script_guard"})
    assert(move == nil)
    -- Ordinary movement can still coexist with a separate look.
    move = assert(startMovePlayer(4, 8, "walk", 2))
    assert(cancelOperation(move))
    assert(cancelOperation(look))
    wave = assert(startMovePlayer("run_target", "walk", 2, {lookAtNpc="script_guard"}))
end
)");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent,
            fixture.host, fixture.files));
    assert(fixture.cutscene.playerMove.active);
    assert(fixture.cutscene.playerMove.arrivalLook.active);
    assert(fixture.cutscene.playerMove.arrivalLook.durationSeconds == 0.75);
    assert(fixture.cutscene.playerMove.arrivalLook.targetHeight == 0.5f);
    // The production target resolver fails safely for an unavailable model.
    fixture.Update(0.025f);
    assert(!fixture.cutscene.playerMove.active);
    assert(!fixture.cutscene.playerMove.arrivalLook.active);
    assert(game::IsNull(fixture.cutscene.playerMove.pathHandle));
}

void PlayerArrivalLooksOverlapAndCompleteWithTheirMove()
{
    for (const bool shortWalk : {false, true}) {
        NpcScriptFixture fixture;
        fixture.files.Write(shortWalk ? R"(
function init()
    local ok, reason = movePlayer(2.3, 8, "walk", 2, {
        lookAtNpc="script_guard", turnDurationMs=750, targetHeight=0.7})
    assert(ok, reason)
    setPersistentBool("done", true)
end
)" : R"(
function init()
    local op = assert(startMovePlayer("run_target", "walk", 2, {
        lookAtNpc="script_guard", turnDurationMs=750, targetHeight=0.7}))
    assert(await(op))
    setPersistentBool("done", true)
end
)");
        assert(Create(fixture.context, fixture.runtime, fixture.persistent,
                fixture.host, fixture.files));
        Vector3 target{14.0f, 2.0f, 12.0f};
        bool turnedWhileWalking = false;
        bool waitedAfterArrival = false;
        float startedAt = 0.0f;
        for (int frame = 0; frame < 400 && !fixture.runtime.initFinished; ++frame) {
            const bool wasStarted = fixture.cutscene.playerMove.arrivalLookStarted;
            fixture.Update(0.025f, &target);
            const auto& move = fixture.cutscene.playerMove;
            if (!wasStarted && move.arrivalLookStarted) startedAt = fixture.playerState.feetPosition.x;
            turnedWhileWalking |= move.arrivalLookStarted && !move.arrived
                    && fixture.playerState.yawRadians > 0.001f;
            waitedAfterArrival |= move.arrived && move.active;
        }
        assert(fixture.runtime.initFinished && fixture.persistent.bools.at("done"));
        assert(turnedWhileWalking);
        if (shortWalk) assert(waitedAfterArrival && startedAt < 2.1f);
        else assert(startedAt > 12.0f && startedAt < 13.0f);
        const Vector3 eye = game::SectorFpsControllerEyePosition(
                fixture.playerState, fixture.playerConfig);
        const float expectedYaw = std::atan2(target.z - eye.z, target.x - eye.x);
        assert(std::fabs(std::remainder(fixture.playerState.yawRadians - expectedYaw,
                2.0f * PI)) < 0.0001f);
        assert(!fixture.cutscene.playerMove.active);
        assert(!fixture.cutscene.playerMove.arrivalLook.active);
        assert(game::IsNull(fixture.cutscene.playerMove.pathHandle));
    }
}

void PlayerArrivalLookHandlesDelaysMovingTargetsAndZeroDistance()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
function init()
    local op = assert(startMovePlayer(2, 8, "walk", 2, {lookAtNpc="script_guard"}))
    assert(await(op))
    setPersistentBool("done", true)
end
)");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent,
            fixture.host, fixture.files));
    Vector3 target{2.0f, 2.0f, 12.0f};
    fixture.Update(0.025f, &target);
    assert(fixture.cutscene.playerMove.arrived && fixture.cutscene.playerMove.active);
    for (int i = 0; i < 40 && !fixture.runtime.initFinished; ++i) fixture.Update(0.025f, &target);
    assert(fixture.runtime.initFinished && fixture.persistent.bools.at("done"));

    game::SectorCutsceneRuntime runtime;
    auto& move = runtime.playerMove;
    move.active = true;
    move.movementSpeed = 2.0f;
    move.cornerCount = 2;
    move.corners[0] = {0.0f, 0.0f, 5.0f};
    move.corners[1] = {0.5f, 0.0f, 0.0f};
    move.arrivalLook.active = true;
    move.arrivalLook.durationSeconds = 0.75;
    game::SectorFpsControllerState player;
    game::AdvanceSectorCutscenePlayerCamera(runtime, player, {}, &target, 0.025f);
    assert(!move.arrivalLookStarted); // Nearby destination, long route.
    move.cornerCount = 1; // Simulate replacement by a shorter replanned route.
    move.corners[0] = {0.5f, 0.0f, 0.0f};
    move.doorPhase = game::NpcDoorTraversalPhase::WaitingForClearance;
    game::AdvanceSectorCutscenePlayerCamera(runtime, player, {}, &target, 0.25f);
    assert(!move.arrivalLookStarted);
    move.doorPhase = game::NpcDoorTraversalPhase::None;
    game::AdvanceSectorCutscenePlayerCamera(runtime, player, {}, &target, 0.25f);
    assert(move.arrivalLookStarted);
    move.doorPhase = game::NpcDoorTraversalPhase::WaitingForClearance;
    game::AdvanceSectorCutscenePlayerCamera(runtime, player, {}, &target, 0.5f);
    assert(move.arrivalLook.elapsedSeconds == 0.75);
    target = {-2.0f, 1.0f, 4.0f};
    game::AdvanceSectorCutscenePlayerCamera(runtime, player, {}, &target, 0.025f);
    assert(std::fabs(player.yawRadians - std::atan2(target.z, target.x)) < 0.0001f);
    assert(!game::AdvanceSectorCutscenePlayerCamera(runtime, player, target, &target, 0.025f));
}

void PlayerArrivalLookCancellationAndRemovalReleaseBothActions()
{
    for (int mode = 0; mode < 3; ++mode) {
        NpcScriptFixture fixture;
        fixture.files.Write(R"(
function init()
    wave = assert(startMovePlayer("run_target", "walk", 2, {lookAtNpc="script_guard"}))
end
function stop()
    assert(cancelOperation(wave))
end
)");
        assert(Create(fixture.context, fixture.runtime, fixture.persistent,
                fixture.host, fixture.files));
        if (mode == 0) {
            assert(engine::ScriptSystemExecuteConsole(fixture.runtime, "stop()").success);
        } else if (mode == 1) {
            fixture.context.world.DestroyLater(fixture.npc);
            fixture.context.world.FlushDestroyedEntities();
            fixture.Update(0.025f);
        } else {
            engine::ScriptSystemShutdownForMap(fixture.context, fixture.runtime);
        }
        assert(!fixture.cutscene.playerMove.active);
        assert(!fixture.cutscene.playerMove.arrivalLook.active);
        assert(game::IsNull(fixture.cutscene.playerMove.pathHandle));
    }
}

void KillingMovingNpcStopsRunawayPatrolWithoutFreezing()
{
    NpcScriptFixture fixture;
    fixture.context.world.Get<game::NpcRuntimeInstance>(fixture.npc).hostile = true;
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
    fixture.Update(0.016f);
    assert(engine::ScriptSystemIsFunctionRunning(
            fixture.runtime, "patrol"));
    assert(fixture.host.npcMoves.size() == 1);

    game::FpsWeaponImpactDefinition impact;
    impact.damage = 25;
    game::FpsShotResult shot;
    game::WeaponImpactEvent impactEvent;
    assert(game::ResolvePlayerWeaponShot(
            fixture.context.world,
            &fixture.context.assets,
            fixture.navigation,
            fixture.npcNavigation,
            &fixture.objects.objectSectorLookupWorld,
            fixture.objects.dynamicDoorColliders,
            fixture.objects.staticModelColliders,
            Vector3{2.0f, 0.8f, 4.0f},
            Vector3{0.0f, 0.0f, 1.0f},
            20.0f,
            impact,
            shot,
            impactEvent));
    assert(shot.hitKind == game::FpsShotHitKind::Npc);
    assert(game::IsDepleted(
            fixture.context.world.Get<game::Health>(fixture.npc)));
    assert(fixture.context.world.Get<game::NpcCombatState>(fixture.npc).dead);
    assert(!fixture.npcNavigation.records.front().occupied);

    fixture.Update(0.016f);
    assert(!engine::ScriptSystemIsFunctionRunning(
            fixture.runtime, "patrol"));
    assert(fixture.host.npcMoves.empty());
    assert(engine::ScriptSystemOperationSnapshot(
            fixture.runtime).empty());
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

void AiTakeoverCancelsScriptNpcMoveWithReason()
{
    NpcScriptFixture fixture;
    fixture.files.Write(R"(
local movement
function init()
    movement = startMoveNpc("script_guard", 12.0, 8.0, "walk")
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
    game::InterruptSectorScriptNpcMoveForAi(
            fixture.context, fixture.host, "script_guard");
    assert(engine::ScriptSystemCallForegroundHook(
            fixture.runtime, "inspectMovement").result
            == engine::ScriptCallResult::Completed);
    assert(fixture.persistent.strings.at("state") == "cancelled");
    assert(fixture.persistent.strings.at("reason")
            == "player detected; AI took control");
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
        FinishNavigationBuild(fixture.navigation, fixture.map);
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

void StableDoorAndDynamicLightBindingsMutateRuntimeTargets()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    const engine::Entity door = AddDoor(context, objects);
    const engine::Entity staticProp = context.world.CreateEntity();
    game::SectorStaticModel staticModel;
    staticModel.instanceId = "static_lamp";
    context.world.Add(staticProp, staticModel);
    const engine::Entity dynamicProp = context.world.CreateEntity();
    game::SectorDynamicModel dynamicModel;
    dynamicModel.instanceId = "dynamic_lamp";
    context.world.Add(dynamicProp, dynamicModel);
    game::SectorTopologyDynamicPointLight light;
    light.id = 7;
    light.instanceId = "warning_light";
    map.dynamicPointLights.push_back(light);
    ScriptFiles files;

    game::InitializeSectorScriptHost(host, objects, map, runtime);
    files.Write(R"(
function init()
    local opened = openDoor("test_door")
    local disabled = setDynamicLightEnabled("warning_light", false)
    local intensity = setDynamicLightIntensity("warning_light", 3.5)
    local colored = setDynamicLightColor("warning_light", 255, 40, 20)
    local staticEmission = setPropEmissiveScale("static_lamp", 0.0)
    local dynamicEmission = setPropEmissiveScale("dynamic_lamp", 2.5)
    local missingEmission, missingReason = setPropEmissiveScale("missing_lamp", 1.0)
    setPersistentBool("bindings_ok", opened and disabled and intensity and colored
        and staticEmission and dynamicEmission and not missingEmission
        and type(missingReason) == "string")
end
)");
    assert(Create(context, runtime, persistent, host, files));
    assert(persistent.bools.at("bindings_ok"));
    assert(context.world.Get<game::SectorDoorMotion>(door).targetOpenFraction == 1.0f);
    assert(!map.dynamicPointLights[0].enabled);
    assert(std::fabs(map.dynamicPointLights[0].intensity - 3.5f) < 0.0001f);
    assert(map.dynamicPointLights[0].color.r == 255
            && map.dynamicPointLights[0].color.g == 40
            && map.dynamicPointLights[0].color.b == 20);
    assert(context.world.Get<game::SectorStaticModel>(staticProp).emissiveScale
            == 0.0f);
    assert(std::fabs(
            context.world.Get<game::SectorDynamicModel>(dynamicProp).emissiveScale
                    - 2.5f) < 0.0001f);
    assert(host.dynamicLightsDirty);
    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);

    game::InitializeSectorScriptHost(host, objects, map, runtime);
    files.Write(R"(
function init()
    setPropEmissiveScale("static_lamp", -0.5)
end
)");
    assert(!Create(context, runtime, persistent, host, files));
    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);
}

void DoorPermissionCallbacksCanYieldAndMustReturnTrue()
{
    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    const engine::Entity door = AddDoor(context, objects);
    game::SectorDoorInteraction& interaction =
            context.world.Get<game::SectorDoorInteraction>(door);
    interaction.canOpenScript = "allowOpenLater";
    interaction.canCloseScript = "denyClose";
    ScriptFiles files;

    game::InitializeSectorScriptHost(host, objects, map, runtime);
    files.Write(R"(
function allowOpenLater()
    delay(0)
    return true
end
function denyClose()
    return false
end
)");
    assert(Create(context, runtime, persistent, host, files));
    assert(game::RequestSectorScriptDoorUse(context, host, door));
    assert(host.doorPermission.active);
    assert(context.world.Get<game::SectorDoorMotion>(door).targetOpenFraction == 0.0f);
    engine::ScriptSystemUpdate(context, runtime, 0.0f);
    game::UpdateSectorScriptDoorPermission(context, host);
    assert(!host.doorPermission.active);
    assert(context.world.Get<game::SectorDoorMotion>(door).targetOpenFraction == 1.0f);

    game::SectorDoorMotion& motion = context.world.Get<game::SectorDoorMotion>(door);
    motion.openFraction = 1.0f;
    motion.targetOpenFraction = 1.0f;
    assert(game::RequestSectorScriptDoorUse(context, host, door));
    assert(motion.targetOpenFraction == 1.0f);
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

void MapAudioBindingsForwardOptionalPlaybackSettings()
{
    struct Capture {
        bool mapPlayed = false;
        bool emitterPlayed = false;
        bool emitterStopped = false;
    } capture;
    game::SectorScriptAudioApi audio;
    audio.userData = &capture;
    audio.playMapSound = [](void* userData, engine::EngineContext&,
                                const std::string& id, float volume, float pitch,
                                std::string&) {
        Capture& value = *static_cast<Capture*>(userData);
        value.mapPlayed = id == "switch_click"
                && std::fabs(volume - 0.4f) < 0.001f
                && std::fabs(pitch - 1.2f) < 0.001f;
        return value.mapPlayed;
    };
    audio.playSoundEmitter = [](void* userData, engine::EngineContext&,
                                    const std::string& id, const float* volume,
                                    float pitch, std::string&) {
        Capture& value = *static_cast<Capture*>(userData);
        value.emitterPlayed = id == "office_fan" && volume == nullptr
                && std::fabs(pitch - 0.8f) < 0.001f;
        return value.emitterPlayed;
    };
    audio.stopSoundEmitter = [](void* userData, engine::EngineContext&,
                                    const std::string& id, std::string&) {
        Capture& value = *static_cast<Capture*>(userData);
        value.emitterStopped = id == "office_fan";
        return value.emitterStopped;
    };

    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    ScriptFiles files;
    game::InitializeSectorScriptHost(
            host, objects, map, runtime, nullptr, nullptr, audio);
    files.Write(R"(
function init()
    assert(playMapSound("switch_click", 0.4, 1.2))
    assert(playSoundEmitter("office_fan", nil, 0.8))
    assert(stopSoundEmitter("office_fan"))
end
)");
    assert(Create(context, runtime, persistent, host, files));
    assert(capture.mapPlayed && capture.emitterPlayed && capture.emitterStopped);
    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);
}

void CutsceneBindingsControlFadeAndCaptionTimelines(bool cinematic)
{
    struct ControlCapture {
        bool enabled = true;
        int calls = 0;
    } capture;
    game::SectorScriptControlApi controlApi;
    controlApi.userData = &capture;
    controlApi.setControlsEnabled = [](
            void* userData,
            engine::EngineContext&,
            bool enabled,
            std::string&) {
        auto& value = *static_cast<ControlCapture*>(userData);
        value.enabled = enabled;
        ++value.calls;
        return true;
    };

    engine::EngineContext context;
    SpawnScriptNpc(context.world);
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    game::SectorCutsceneRuntime cutscene;
    game::SectorFpsControllerState player;
    game::SectorFpsControllerConfig config;
    ScriptFiles files;
    game::InitializeSectorCutsceneRuntime(cutscene);
    game::InitializeSectorScriptHost(
            host,
            objects,
            map,
            runtime,
            nullptr,
            nullptr,
            {},
            nullptr,
            &cutscene,
            &player,
            &config,
            controlApi);
    std::string script = R"(
function init()
    assert(TOP == 1 and CENTER == 2 and BOTTOM == 3)
    assert(enableControls(false))
    fadeOut(100)
    setPersistentBool("faded_out", true)
    text("A centered cutscene card", CENTER, 100)
    setPersistentBool("text_done", true)
    say("script_guard", "Follow me this way.", nil, 100)
    setPersistentBool("say_done", true)
    fadeIn(100)
    assert(enableControls(true))
    setPersistentBool("sequence_done", true)
end
)";
    if (cinematic) {
        script.replace(script.find("enableControls(false)"), 21, "startCutscene()");
        script.replace(script.find("enableControls(true)"), 20, "endCutscene()");
        script.insert(script.find("    setPersistentBool(\"faded_out\""),
                "    assert(startCutscene())\n");
    }
    files.Write(script);
    assert(Create(context, runtime, persistent, host, files));
    assert(!runtime.initFinished);
    assert(!capture.enabled && capture.calls == 1);
    assert(!cutscene.controlsEnabled);
    assert(cutscene.presentation.active == cinematic);
    assert(cutscene.presentation.progress == 0.0);

    bool sawPartialTypewriterText = false;
    for (int frame = 0; frame < 300 && !runtime.initFinished; ++frame) {
        game::UpdateSectorCutsceneTimelines(cutscene, runtime, 0.025f);
        if (cutscene.caption.active
                && cutscene.caption.kind == game::SectorCutsceneCaptionKind::Say
                && cutscene.caption.visibleByteCount > 0
                && cutscene.caption.visibleByteCount
                        < cutscene.caption.text.size()) {
            sawPartialTypewriterText = true;
        }
        engine::ScriptSystemUpdate(context, runtime, 0.025f);
    }
    assert(runtime.initFinished);
    assert(persistent.bools.at("faded_out"));
    assert(persistent.bools.at("text_done"));
    assert(persistent.bools.at("say_done"));
    assert(persistent.bools.at("sequence_done"));
    assert(sawPartialTypewriterText);
    assert(capture.enabled && capture.calls == (cinematic ? 3 : 2));
    assert(cutscene.controlsEnabled);
    assert(!cutscene.presentation.active);
    if (cinematic) {
        assert(cutscene.presentation.progress == 1.0);
        game::UpdateSectorCutsceneTimelines(cutscene, runtime, 0.35f);
        assert(cutscene.presentation.progress < 0.0001);
    }
    assert(std::fabs(cutscene.fade.opacity) < 0.001f);

    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);
    game::ResetSectorCutsceneRuntime(cutscene);
}

void SpeechCompletionGatesCaptionHold()
{
    engine::EngineContext context;
    engine::ScriptRuntime scripts;
    game::SectorCutsceneRuntime cutscene;
    game::InitializeSectorCutsceneRuntime(cutscene);
    const auto speaker = SpawnScriptNpc(context.world);
    engine::DialogueVoice voice;
    voice.pitchRange = {1.0f, 1.0f};
    voice.banks[0].push_back({engine::SoundHandle{7, 1}, 2.0f, 0});
    game::SectorCutsceneSpeechOptions speech;
    speech.speaker = speaker;
    speech.voice = &voice;
    const double hold = 0.5;
    uint64_t token = 0;
    std::string error;
    assert(game::BeginSectorCutsceneCaption(cutscene, game::SectorCutsceneCaptionKind::Say,
            game::SectorCutsceneTextPosition::Bottom, "Wait.", &hold, token, error, &speech));
    const auto update = [&](float dt) {
        game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, dt);
        game::UpdateSectorCutsceneTimelines(cutscene, scripts, dt);
    };
    update(0); // start a full two-second recording, with no device -> estimated timing
    assert(cutscene.caption.speechDriven && cutscene.caption.visibleByteCount == 0);
    game::UpdateSectorCutsceneTimelines(cutscene, scripts, 100);
    assert(cutscene.caption.active && cutscene.caption.elapsedSeconds == 0);
    update(0.5f);
    assert(cutscene.caption.visibleByteCount == 1 && !cutscene.caption.speechFinished);
    update(10); // hitch consumes only this fragment, not punctuation or caption hold
    assert(cutscene.caption.active && !cutscene.caption.speechFinished);
    assert(cutscene.caption.elapsedSeconds == 2.0);
    update(0.3f);
    assert(cutscene.caption.speechFinished && cutscene.caption.opacity == 1);
    assert(cutscene.caption.elapsedSeconds == cutscene.caption.revealSeconds);
    update(0.49f);
    assert(cutscene.caption.opacity == 1 && cutscene.caption.active);
    update(0.1f);
    assert(cutscene.caption.opacity < 1 && cutscene.caption.active);
    update(0.4f);
    assert(!cutscene.caption.active);
    game::StopSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio);
}

void DisabledDialogueVoicesKeepTextAndCanBeReenabled()
{
    engine::EngineContext context;
    engine::ScriptRuntime scripts;
    game::SectorCutsceneRuntime cutscene;
    game::InitializeSectorCutsceneRuntime(cutscene);
    const auto speaker = SpawnScriptNpc(context.world);
    auto& npc = context.world.Get<game::NpcRuntimeInstance>(speaker);
    engine::DialogueVoice voice;
    voice.pitchRange = {1.0f, 1.0f};
    voice.banks[0].push_back({engine::SoundHandle{7, 1}, 2.0f, 0});
    game::SectorCutsceneSpeechOptions speech;
    speech.speaker = speaker;
    speech.voice = &voice;
    const double hold = 0.5;
    uint64_t token = 0;
    std::string error;
    const auto begin = [&](std::string_view text) {
        assert(game::BeginSectorCutsceneCaption(cutscene, game::SectorCutsceneCaptionKind::Say,
                game::SectorCutsceneTextPosition::Bottom, text, &hold, token, error, &speech));
    };
    const auto update = [&](float dt, bool enabled) {
        game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, dt, enabled);
        game::UpdateSectorCutsceneTimelines(cutscene, scripts, dt);
    };
    // Every codepoint, including spaces and punctuation, takes exactly 25 ms.
    // Neither multi-byte UTF-8 nor mood or two-second recordings alter the rate.
    const size_t prefixes[] = {1, 3, 4, 5, 8, 9, 10, 11, 12};
    for (size_t mood = 0; mood < engine::DialogueMoodCount; ++mood) {
        speech.mood = static_cast<engine::DialogueMood>(mood);
        begin(u8"Aé, 人! B?");
        update(0, false);
        assert(!npc.dialogueSpeaking && engine::IsNull(cutscene.speechPlayback.handle));
        assert(!cutscene.caption.voiceTiming && !cutscene.caption.speechDriven);
        assert(std::fabs(cutscene.caption.revealSeconds - 9.0/40.0) < 0.000001);
        for (const size_t prefix : prefixes) {
            update(0.025f, false);
            assert(cutscene.caption.visibleByteCount == prefix);
        }
        assert(cutscene.caption.speechFinished && cutscene.caption.active);
        assert(npc.dialogueHistory.banks[0].available == 0);
        assert(cutscene.speechPlayback.sequence.nextCue == 0);
        update(0.49f, false);
        assert(cutscene.caption.opacity == 1.0f);
        update(0.02f, false);
        assert(cutscene.caption.opacity < 1.0f && cutscene.caption.active);
        update(0.4f, false);
        assert(!cutscene.caption.active);
    }

    speech.mood = engine::DialogueMood::Neutral;
    begin("Wait. Another person!");
    update(0, true);
    update(0.5f, true);
    assert(npc.dialogueSpeaking && cutscene.caption.visibleByteCount == 1);
    cutscene.speechPlayback.sequence.usesDevice = true;
    update(0, false);
    assert(!npc.dialogueSpeaking && engine::IsNull(cutscene.speechSpeaker));
    assert(cutscene.caption.visibleByteCount == 1);
    assert(std::fabs(cutscene.caption.elapsedSeconds - 1.0/40.0) < 0.000001);
    update(0.025f, false);
    assert(cutscene.caption.visibleByteCount == 2);
    update(0, true);
    assert(npc.dialogueSpeaking && cutscene.caption.visibleByteCount == 2);
    assert(cutscene.speechPlayback.sequence.nextCue == 1); // skip the partially revealed word
    assert(!cutscene.speechPlayback.sequence.fragmentActive);

    // Switching timing after revelation preserves elapsed hold/fade progress.
    update(0, false);
    update(0.6f, false);
    assert(cutscene.caption.speechFinished && cutscene.caption.active);
    const double elapsedHold = cutscene.caption.elapsedSeconds - cutscene.caption.revealSeconds;
    update(0, true);
    assert(std::fabs(cutscene.caption.elapsedSeconds - cutscene.caption.revealSeconds - elapsedHold) < 0.000001);
    assert(cutscene.caption.speechFinished && cutscene.caption.visibleByteCount == cutscene.caption.text.size());
    update(0, false);
    assert(std::fabs(cutscene.caption.elapsedSeconds - cutscene.caption.revealSeconds - elapsedHold) < 0.000001);
    update(0.8f, false);
    assert(!cutscene.caption.active);
    game::StopSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio);
}

void AsyncCaptionsAreConsoleSafeAndBlockingCallsAreSideEffectFree()
{
    engine::EngineContext context;
    const engine::Entity speaker = SpawnScriptNpc(context.world);
    context.world.Get<game::NpcRuntimeInstance>(speaker).voice = "female";
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    game::SectorCutsceneRuntime cutscene;
    game::SectorFpsControllerState player;
    game::SectorFpsControllerConfig config;
    ScriptFiles files;
    game::InitializeSectorCutsceneRuntime(cutscene);
    game::InitializeSectorScriptHost(
            host,
            objects,
            map,
            runtime,
            nullptr,
            nullptr,
            {},
            nullptr,
            &cutscene,
            &player,
            &config);
    engine::DialogueVoiceLibrary voices;
    engine::DialogueVoice female;
    female.id = "female";
    female.banks[0].push_back({engine::SoundHandle{123, 1}, 0.09f, 7});
    voices.voices.push_back(std::move(female));
    host.dialogueVoices = &voices;
    files.Write("function init() end\n");
    assert(Create(context, runtime, persistent, host, files));
    assert(runtime.initFinished);

    const engine::ScriptConsoleResult blocking =
            engine::ScriptSystemExecuteConsole(
                    runtime, "say('script_guard', 'must not start', nil, 100)");
    assert(!blocking.success);
    assert(!cutscene.caption.active);
    const engine::ScriptConsoleResult blockingFade =
            engine::ScriptSystemExecuteConsole(runtime, "fadeOut(100)");
    assert(!blockingFade.success);
    assert(!cutscene.fade.active);

    const engine::ScriptConsoleResult say =
            engine::ScriptSystemExecuteConsole(
                    runtime, "startSay('script_guard', 'console bark', nil, 100)");
    assert(say.success);
    assert(cutscene.caption.active);
    assert(cutscene.caption.kind == game::SectorCutsceneCaptionKind::Say);
    assert(cutscene.caption.speaker == speaker);
    assert(cutscene.caption.mood == engine::DialogueMood::Neutral);
    assert(std::fabs(cutscene.caption.holdSeconds - 0.1) < 0.0001);
    assert(!cutscene.caption.speechTimeline.cues.empty());
    assert(cutscene.caption.speechTimeline.cues.front().source == 7);
    const uint64_t originalToken = cutscene.caption.token;
    assert(engine::ScriptSystemExecuteConsole(runtime, R"(
        local op, err = startSay('missing_npc', 'must not replace')
        assert(op == nil and type(err) == 'string')
        op, err = startSay('script_guard', 'must not replace', 'unknown')
        assert(op == nil and type(err) == 'string')
        op, err = startSay('script_guard', 'must not replace', 'happy', -1)
        assert(op == nil and type(err) == 'string')
        assert(not pcall(function() startSay(123) end))
        assert(not pcall(function() startSay('script_guard', 'bad mood type', 25) end))
    )").success);
    assert(cutscene.caption.token == originalToken);
    game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, 0.01f);
    assert(context.world.Get<game::NpcRuntimeInstance>(speaker).dialogueSpeaking);
    context.world.Get<game::NpcCombatState>(speaker).dead = true;
    game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, 0.01f);
    assert(!context.world.Get<game::NpcRuntimeInstance>(speaker).dialogueSpeaking);
    context.world.Get<game::NpcCombatState>(speaker).dead = false;
    const std::vector<engine::ScriptOperationSnapshot> sayOperations =
            engine::ScriptSystemOperationSnapshot(runtime);
    const auto sayOperation = std::find_if(
            sayOperations.begin(),
            sayOperations.end(),
            [](const engine::ScriptOperationSnapshot& operation) {
                return operation.debugLabel == "say"
                        && operation.state
                                == engine::ScriptOperationState::Pending;
            });
    assert(sayOperation != sayOperations.end());

    const engine::ScriptConsoleResult text =
            engine::ScriptSystemExecuteConsole(
                    runtime, "startText('console card', CENTER, 100)");
    assert(text.success);
    assert(cutscene.caption.active);
    assert(cutscene.caption.kind == game::SectorCutsceneCaptionKind::Text);
    game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, 0.01f);
    assert(!context.world.Get<game::NpcRuntimeInstance>(speaker).dialogueSpeaking);
    assert(cutscene.caption.position
            == game::SectorCutsceneTextPosition::Center);
    const std::vector<engine::ScriptOperationSnapshot> textOperations =
            engine::ScriptSystemOperationSnapshot(runtime);
    const auto replacedSay = std::find_if(
            textOperations.begin(),
            textOperations.end(),
            [&sayOperation](const engine::ScriptOperationSnapshot& operation) {
                return operation.handle == sayOperation->handle;
            });
    const auto textOperation = std::find_if(
            textOperations.begin(),
            textOperations.end(),
            [](const engine::ScriptOperationSnapshot& operation) {
                return operation.debugLabel == "text"
                        && operation.state
                                == engine::ScriptOperationState::Pending;
            });
    assert(replacedSay != textOperations.end());
    assert(replacedSay->state == engine::ScriptOperationState::Cancelled);
    assert(textOperation != textOperations.end());
    assert(engine::ScriptSystemCancelOperation(
            context,
            runtime,
            textOperation->handle,
            "caption console cancellation test"));
    assert(!cutscene.caption.active);

    for (const char* mood : {"neutral", "happy", "angry", "afraid", "panicked", "pained", "relieved"}) {
        const std::string command = std::string("startSay('script_guard', 'Another person?', '") + mood + "', 0)";
        assert(engine::ScriptSystemExecuteConsole(runtime, command).success);
        assert(std::string(engine::DialogueMoodName(cutscene.caption.mood)) == mood);
        assert(!cutscene.caption.speechTimeline.cues.empty()); // missing moods fall back to neutral
        assert(cutscene.caption.holdSeconds == 0);
    }
    game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, 0.01f);
    assert(context.world.Get<game::NpcRuntimeInstance>(speaker).dialogueSpeaking);
    game::CancelSectorCutsceneCaption(cutscene, cutscene.caption.token);
    game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, 0.01f);
    assert(!context.world.Get<game::NpcRuntimeInstance>(speaker).dialogueSpeaking);
    game::StopSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio);
    assert(engine::IsNull(cutscene.speechSpeaker));
    assert(engine::ScriptSystemExecuteConsole(runtime,
            "startSay('script_guard', 'Still speaking')").success);
    game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, 0.01f);
    context.world.DestroyLater(speaker);
    context.world.FlushDestroyedEntities();
    game::UpdateSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio, 0.01f);
    assert(cutscene.caption.active); // a stale generational speaker never dereferences or cancels the text
    game::StopSectorCutsceneSpeech(cutscene, context.world, context.assets, context.audio);

    const engine::ScriptConsoleResult finalText =
            engine::ScriptSystemExecuteConsole(
                    runtime, "startText('final card', CENTER, 100)");
    assert(finalText.success);
    for (int frame = 0; frame < 100 && cutscene.caption.active; ++frame) {
        game::UpdateSectorCutsceneTimelines(cutscene, runtime, 0.025f);
        engine::ScriptSystemUpdate(context, runtime, 0.025f);
    }
    assert(!cutscene.caption.active);

    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);
    game::ResetSectorCutsceneRuntime(cutscene);
}

void CutsceneControlOwnershipRecoversAndRejectsCompetingTasks(bool cinematic)
{
    struct ControlCapture {
        bool enabled = true;
        int disabledCalls = 0;
        int enabledCalls = 0;
    } capture;
    game::SectorScriptControlApi controlApi;
    controlApi.userData = &capture;
    controlApi.setControlsEnabled = [](
            void* userData,
            engine::EngineContext&,
            bool enabled,
            std::string&) {
        auto& value = *static_cast<ControlCapture*>(userData);
        value.enabled = enabled;
        if (enabled) ++value.enabledCalls;
        else ++value.disabledCalls;
        return true;
    };

    engine::EngineContext context;
    engine::ScriptRuntime runtime;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map;
    game::SectorScriptHost host;
    game::SectorCutsceneRuntime cutscene;
    game::SectorFpsControllerState player;
    game::SectorFpsControllerConfig config;
    ScriptFiles files;
    game::InitializeSectorCutsceneRuntime(cutscene);
    game::InitializeSectorScriptHost(
            host,
            objects,
            map,
            runtime,
            nullptr,
            nullptr,
            {},
            nullptr,
            &cutscene,
            &player,
            &config,
            controlApi);
    std::string script = R"(
function init() end

function controls_complete()
    assert(enableControls(false))
end

function controls_fail()
    assert(enableControls(false))
    error("intentional control owner failure")
end

function controls_wait()
    assert(enableControls(false))
    delay(10000)
end

function controls_compete()
    local ok, reason = enableControls(false)
    setPersistentBool("competing_controls_rejected", not ok)
    setPersistentString("competing_controls_reason", reason or "")
end
)";
    if (cinematic) {
        size_t position = 0;
        while ((position = script.find("enableControls(false)", position)) != std::string::npos) {
            script.replace(position, 21, "startCutscene()");
            position += 15;
        }
    }
    files.Write(script);
    assert(Create(context, runtime, persistent, host, files));

    std::string queueError;
    assert(engine::ScriptSystemQueueBackground(
            runtime, "controls_complete", queueError));
    engine::ScriptSystemUpdate(context, runtime, 0.025f);
    game::UpdateSectorScriptCutsceneControlOwnership(context, host);
    assert(capture.enabled);
    assert(cutscene.controlsEnabled);
    assert(!cutscene.presentation.active);
    assert(capture.disabledCalls == 1 && capture.enabledCalls == 1);

    assert(engine::ScriptSystemQueueBackground(
            runtime, "controls_fail", queueError));
    engine::ScriptSystemUpdate(context, runtime, 0.025f);
    game::UpdateSectorScriptCutsceneControlOwnership(context, host);
    assert(capture.enabled);
    assert(cutscene.controlsEnabled);
    assert(!cutscene.presentation.active);
    assert(capture.disabledCalls == 2 && capture.enabledCalls == 2);

    assert(engine::ScriptSystemQueueBackground(
            runtime, "controls_wait", queueError));
    engine::ScriptSystemUpdate(context, runtime, 0.025f);
    game::UpdateSectorScriptCutsceneControlOwnership(context, host);
    assert(!capture.enabled);
    assert(!cutscene.controlsEnabled);
    assert(cutscene.presentation.active == cinematic);
    assert(engine::IsValid(cutscene.controlsOwnerTask));

    assert(engine::ScriptSystemQueueBackground(
            runtime, "controls_compete", queueError));
    engine::ScriptSystemUpdate(context, runtime, 0.025f);
    game::UpdateSectorScriptCutsceneControlOwnership(context, host);
    assert(!capture.enabled);
    assert(persistent.bools.at("competing_controls_rejected"));
    assert(!persistent.strings.at("competing_controls_reason").empty());

    std::string stopError;
    assert(engine::ScriptSystemStopFunction(
            context, runtime, "controls_wait", stopError));
    engine::ScriptSystemUpdate(context, runtime, 0.025f);
    game::UpdateSectorScriptCutsceneControlOwnership(context, host);
    assert(capture.enabled);
    assert(cutscene.controlsEnabled);
    assert(!cutscene.presentation.active);
    assert(!engine::IsValid(cutscene.controlsOwnerTask));
    assert(capture.enabledCalls == 3);

    const engine::ScriptConsoleResult consoleDisable =
            engine::ScriptSystemExecuteConsole(
                    runtime, cinematic ? "startCutscene()" : "enableControls(false)");
    assert(consoleDisable.success);
    assert(capture.enabled);
    assert(cutscene.controlsEnabled);
    assert(!cutscene.presentation.active);
    assert(capture.disabledCalls == 3);

    if (cinematic) {
        // Both the explicit end command and enableControls(true) are console recovery paths.
        for (const char* recovery : {"assert(endCutscene()); assert(endCutscene())",
                "assert(enableControls(true))"}) {
            assert(engine::ScriptSystemQueueBackground(runtime, "controls_wait", queueError));
            engine::ScriptSystemUpdate(context, runtime, 0.025f);
            game::UpdateSectorCutsceneTimelines(cutscene, runtime, 0.175f);
            assert(cutscene.presentation.active);
            const double progress = cutscene.presentation.progress;
            assert(engine::ScriptSystemExecuteConsole(runtime, recovery).success);
            assert(cutscene.controlsEnabled && !cutscene.presentation.active);
            assert(cutscene.presentation.progress == progress);
            assert(engine::ScriptSystemStopFunction(context, runtime, "controls_wait", stopError));
            engine::ScriptSystemUpdate(context, runtime, 0.025f);
            game::UpdateSectorScriptCutsceneControlOwnership(context, host);
        }
    }

    engine::ScriptSystemShutdownForMap(context, runtime);
    game::ResetSectorScriptHost(host);
    game::ResetSectorCutsceneRuntime(cutscene);
}

void CinematicTransitionsAndCaptionLayout()
{
    game::SectorCutsceneRuntime cutscene;
    engine::ScriptRuntime scripts;
    game::InitializeSectorCutsceneRuntime(cutscene);
    cutscene.presentation.active = true;
    game::UpdateSectorCutsceneTimelines(cutscene, scripts, 0.175f);
    assert(std::fabs(cutscene.presentation.progress - 0.5) < 0.0001);
    cutscene.presentation.active = false;
    game::UpdateSectorCutsceneTimelines(cutscene, scripts, 0.0875f);
    assert(std::fabs(cutscene.presentation.progress - 0.25) < 0.0001);
    cutscene.presentation.active = true;
    game::UpdateSectorCutsceneTimelines(cutscene, scripts, -1.0f);
    game::UpdateSectorCutsceneTimelines(cutscene, scripts, NAN);
    assert(std::fabs(cutscene.presentation.progress - 0.25) < 0.0001);
    game::UpdateSectorCutsceneTimelines(cutscene, scripts, 1.0f);
    assert(cutscene.presentation.progress == 1.0);
    using Position = game::SectorCutsceneTextPosition;
    for (Rectangle viewport : {Rectangle{0, 0, 1920, 1080}, Rectangle{30, 70, 640, 360},
            Rectangle{100, 50, 2560, 1080}}) {
        for (float blockHeight : {48.0f, 104.0f, 216.0f}) {
            const auto layout = game::BuildSectorCutscenePresentationLayout(
                    cutscene.presentation, viewport, Position::Bottom, blockHeight);
            assert(std::fabs(layout.topBar.height - viewport.height * 0.15f) < 0.001f);
            assert(layout.topBar.height == layout.bottomBar.height);
            assert(layout.topBar.x == viewport.x && layout.topBar.y == viewport.y);
            assert(std::fabs(layout.bottomBar.y + layout.bottomBar.height
                    - viewport.y - viewport.height) < 0.001f);
            assert(layout.captionY >= viewport.y);
            assert(layout.captionY + blockHeight <= viewport.y + viewport.height);
            if (blockHeight <= viewport.height * 0.12f) {
                assert(std::fabs(layout.captionY - layout.bottomBar.y
                        - viewport.height * 0.015f) < 0.001f);
            }
            const auto normal = game::BuildSectorCutscenePresentationLayout(
                    {}, viewport, Position::Bottom, blockHeight);
            assert(std::fabs(normal.captionY - (viewport.y + viewport.height * 0.88f
                    - blockHeight)) < 0.001f);
            for (Position position : {Position::Top, Position::Center}) {
                const auto plain = game::BuildSectorCutscenePresentationLayout(
                        {}, viewport, position, blockHeight);
                const auto cinematic = game::BuildSectorCutscenePresentationLayout(
                        cutscene.presentation, viewport, position, blockHeight);
                assert(plain.captionY == cinematic.captionY);
            }
        }
    }
    cutscene.presentation.active = false;
    game::UpdateSectorCutsceneTimelines(cutscene, scripts, 1.0f);
    assert(cutscene.presentation.progress == 0.0);
    cutscene.presentation.active = true;
    game::UpdateSectorCutsceneTimelines(cutscene, scripts, 1.0f);
    game::ResetSectorCutsceneRuntime(cutscene);
    assert(!cutscene.presentation.active && cutscene.presentation.progress == 0.0);
    assert(cutscene.controlsEnabled);
}

void NpcMarkerArrivalOrientationTurnsAfterStopping()
{
    for (float distance : {0.0f, 0.3f, 12.0f}) {
        for (bool async : {false, true}) {
            NpcScriptFixture fixture;
            NpcScriptFixture baseline;
            fixture.map.levelMarkers[0].position = {(2.0f + distance) * 8.0f, 0.0f, 64.0f};
            fixture.map.levelMarkers[0].yawRadians = -0.8f;
            baseline.map.levelMarkers[0] = fixture.map.levelMarkers[0];
            const std::string call = "moveNpc(\"script_guard\", \"run_target\", nil, nil, true)";
            fixture.files.Write("function init() assert(" + (async
                    ? std::string("await(startMoveNpc(\"script_guard\", \"run_target\", nil, nil, true))")
                    : call) + ") end");
            baseline.files.Write("function init() assert(moveNpc(\"script_guard\", \"run_target\")) end");
            assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
            assert(Create(baseline.context, baseline.runtime, baseline.persistent, baseline.host, baseline.files));
            int stationaryTurnFrames = 0;
            bool waitedAfterArrival = false;
            for (int frame = 0; frame < 600 && !fixture.runtime.initFinished; ++frame) {
                auto& record = fixture.npcNavigation.records.front();
                const float previousX = fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc).position.x;
                fixture.Update(0.025f);
                baseline.Update(0.025f);
                const auto& transform = fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc);
                const auto& plain = baseline.context.world.Get<game::SectorObjectTransform>(baseline.npc);
                assert(std::fabs(transform.position.x - plain.position.x) < 0.001f);
                assert(std::fabs(transform.position.z - plain.position.z) < 0.001f);
                if (transform.position.x > previousX) assert(!record.arrivalTurn.started);
                if (!record.arrivalTurn.started) assert(std::fabs(transform.yawRadians - plain.yawRadians) < 0.001f);
                else ++stationaryTurnFrames;
                if (record.arrivalReached) {
                    waitedAfterArrival = true;
                    assert(game::IsNull(record.pathHandle));
                    assert(!record.holdsDoor && !fixture.runtime.initFinished);
                }
            }
            assert(fixture.runtime.initFinished);
            assert(std::fabs(fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc).yawRadians + 0.8f) < 0.001f);
            assert(waitedAfterArrival && stationaryTurnFrames >= 30);
        }
    }
}

void NpcTurnTimingRespectsDoorsReplansAndAngleWrap()
{
    game::NpcNavigationRecord record;
    record.matchArrivalOrientation = true;
    record.cornerCount = 2;
    record.corners[0] = {10, 0, 0};
    record.corners[1] = {0.2f, 0, 0};
    record.arrivalTurn.targetYaw = -3.0f;
    float yaw = 3.0f;
    game::UpdateNpcArrivalTurn(record, yaw, 0.1f);
    assert(!record.arrivalTurn.started); // Remaining route, not straight-line distance.
    record.cornerCount = 1;
    record.corners[0] = {0.2f, 0, 0};
    record.cornerDoorIds[0] = 42;
    game::UpdateNpcArrivalTurn(record, yaw, 0.1f);
    assert(!record.arrivalTurn.started);
    record.cornerDoorIds[0] = 0;
    record.doorPhase = game::NpcDoorTraversalPhase::Crossing;
    game::UpdateNpcArrivalTurn(record, yaw, 0.1f);
    assert(!record.arrivalTurn.started);
    record.doorPhase = game::NpcDoorTraversalPhase::None;
    record.tileReplanPending = true;
    game::UpdateNpcArrivalTurn(record, yaw, 0.1f);
    assert(!record.arrivalTurn.started);
    record.tileReplanPending = false;
    game::UpdateNpcArrivalTurn(record, yaw, 0.1f);
    assert(!record.arrivalTurn.started && yaw == 3.0f);
    record.arrivalReached = true;
    game::UpdateNpcArrivalTurn(record, yaw, 0.1f);
    assert(record.arrivalTurn.started && yaw > 3.0f);
    const double elapsed = record.arrivalTurn.elapsedSeconds;
    record.doorPhase = game::NpcDoorTraversalPhase::WaitingForClearance;
    game::UpdateNpcArrivalTurn(record, yaw, 0.1f);
    assert(record.arrivalTurn.elapsedSeconds > elapsed); // Once started, delays do not restart it.
    const float held = yaw;
    game::UpdateNpcArrivalTurn(record, yaw, NAN);
    game::UpdateNpcArrivalTurn(record, yaw, -1.0f);
    assert(yaw == held);
    game::UpdateNpcArrivalTurn(record, yaw, 1.0f);
    assert(std::fabs(yaw - (2.0f * PI - 3.0f)) < 0.001f);
}

void NpcLookTargetsCompleteAndPreserveHeadAnimation()
{
    for (const std::string& target : {"Player", "Npc", "Prop", "Marker"}) {
        for (bool async : {false, true}) {
            NpcScriptFixture fixture;
            auto& world = fixture.context.world;
            const auto other = SpawnScriptNpc(world);
            world.Get<game::NpcRuntimeInstance>(other).instanceId = "target";
            world.Get<game::SectorObjectTransform>(other).position = {8, 0, 8};
            const auto prop = world.CreateEntity();
            game::SectorStaticModel model;
            model.instanceId = "target";
            world.Add(prop, model);
            game::SectorObjectTransform propTransform;
            propTransform.position = {8, 0, 8};
            world.Add(prop, propTransform);
            fixture.playerState.feetPosition = {8, 0, 8};
            fixture.map.levelMarkers[0].position = {64, 0, 64};
            game::NpcHeadLookState head;
            head.currentYawRadians = 0.2f;
            head.boneName = "Head";
            world.Add(fixture.npc, head);
            auto& animation = world.Get<game::NpcAnimationState>(fixture.npc);
            animation.scriptRequestId = 77;
            animation.scriptStatus = game::NpcScriptAnimationStatus::Playing;
            const std::string args = target == "Player" ? "\"script_guard\", 200"
                    : "\"script_guard\", \"" + (target == "Marker" ? std::string("run_target") : std::string("target")) + "\", 200";
            const std::string call = (async ? "startNpcLookAt" : "npcLookAt") + target + "(" + args + ")";
            fixture.files.Write("function init() assert(" + (async ? "await(" + call + ")" : call) + ") end");
            assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
            assert(!fixture.runtime.initFinished);
            fixture.Update(0.1f);
            assert(!fixture.runtime.initFinished);
            float yaw = world.Get<game::SectorObjectTransform>(fixture.npc).yawRadians;
            assert(yaw > 0.0f && yaw < PI / 2.0f);
            fixture.Update(0.11f);
            assert(fixture.runtime.initFinished);
            yaw = world.Get<game::SectorObjectTransform>(fixture.npc).yawRadians;
            assert(std::fabs(yaw - PI / 2.0f) < 0.001f);
            assert(animation.scriptRequestId == 77 && animation.scriptStatus == game::NpcScriptAnimationStatus::Playing);
            assert(world.Get<game::NpcHeadLookState>(fixture.npc).currentYawRadians == 0.2f);
            game::NpcHeadLookDefinition definition;
            definition.enabled = true;
            definition.rangeWorld = 20.0f;
            const auto angles = game::EvaluateNpcHeadLookTargetAngles({2, 0, 8}, yaw,
                    {2, 1.6f, 8}, {8, 1.6f, 9}, definition, true);
            assert(angles.active && angles.yawRadians < 0.0f);
            world.Get<game::SectorObjectTransform>(other).position.z = 14;
            world.Get<game::SectorObjectTransform>(prop).position.z = 14;
            fixture.playerState.feetPosition.z = 14;
            fixture.Update(0.1f);
            assert(world.Get<game::SectorObjectTransform>(fixture.npc).yawRadians == yaw);
        }
    }
}

void NpcLookValidationReplacementMovementAndLifecycle()
{
    NpcScriptFixture fixture;
    fixture.playerState.feetPosition = {8, 0, 8};
    fixture.files.Write("function init() end");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
    const auto console = [&](const char* source) {
        assert(engine::ScriptSystemExecuteConsole(fixture.runtime, source).success);
    };
    console(R"(
assert(npcLookAtPlayer("script_guard", 100) == false)
assert(startNpcLookAtNpc("script_guard", "script_guard", 100) == nil)
assert(startNpcLookAtNpc("script_guard", "missing", 100) == nil)
assert(startNpcLookAtPlayer("missing", 100) == nil)
assert(startNpcLookAtPlayer("script_guard", 0) == nil)
assert(startNpcLookAtPlayer("script_guard", 0/0) == nil)
assert(startNpcLookAtPlayer("script_guard", math.huge) == nil)
assert(startNpcLookAtPlayer("script_guard", "100") == nil)
assert(startMoveNpc("script_guard", "run_target", nil, nil, "yes") == nil)
turn = assert(startNpcLookAtPlayer("script_guard", 1000))
assert(startNpcLookAtMarker("script_guard", "missing", 100) == nil)
)");
    auto* turn = game::FindNpcBodyTurn(fixture.npcNavigation, fixture.npc);
    const uint64_t first = turn->requestId;
    fixture.Update(0.1f);
    fixture.playerState.feetPosition = {2, 0, 14};
    fixture.Update(0.1f);
    assert(std::fabs(fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc).yawRadians) < 0.001f);
    console("nextTurn = assert(startNpcLookAtMarker('script_guard', 'run_target', 1000)); assert(operationStatus(turn) == 'cancelled')");
    assert(turn->requestId != first);
    console("assert(startMoveNpc('script_guard', 'outside_target') == nil)");
    assert(turn->status == game::NpcBodyTurnStatus::Playing);
    console("movement = assert(startMoveNpc('script_guard', 'run_target')); assert(startNpcLookAtPlayer('script_guard', 100) == nil)");
    fixture.Update(0.025f);
    assert(turn->status == game::NpcBodyTurnStatus::Cancelled);
    console("assert(operationStatus(nextTurn) == 'cancelled'); assert(cancelOperation(movement)); turn = assert(startNpcLookAtPlayer('script_guard', 1000)); assert(cancelOperation(turn))");
    assert(!game::HasNpcBodyTurn(fixture.npcNavigation, fixture.npc));
    console("turn = assert(startNpcLookAtPlayer('script_guard', 1000))");
    game::InterruptSectorScriptNpcMoveForAi(fixture.context, fixture.host, "script_guard");
    console("assert(operationStatus(turn) == 'cancelled')");
    console("turn = assert(startNpcLookAtPlayer('script_guard', 1000))");
    engine::ScriptSystemShutdownForMap(fixture.context, fixture.runtime);
    assert(!game::HasNpcBodyTurn(fixture.npcNavigation, fixture.npc));
}

void ConversationHoldPausesAndResumesNpcTravel()
{
    NpcScriptFixture fixture;
    fixture.files.Write("function init() end");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
    const auto request = game::RequestNpcMove(fixture.context.world, fixture.navigation,
            fixture.objects.objectSectorLookupWorld, fixture.npcNavigation, "script_guard", {6, 8},
            game::NpcMoveGait::Walk, game::NpcMoveAuthority::Patrol);
    assert(request.accepted);
    auto& npc = fixture.context.world.Get<game::NpcRuntimeInstance>(fixture.npc);
    auto& transform = fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc);
    npc.conversationHeld = true;
    const Vector3 heldPosition = transform.position;
    for (int i = 0; i < 20; ++i) fixture.Update(0.05f);
    assert(Vector3Distance(transform.position, heldPosition) == 0);
    assert(npc.action == game::NpcAction::Idle);
    npc.conversationHeld = false;
    for (int i = 0; i < 20; ++i) fixture.Update(0.05f);
    assert(Vector3Distance(transform.position, heldPosition) > 0.2f);

    game::SectorCompiledPatrol patrol;
    patrol.sourceAuthoringPatrolId = 9;
    patrol.id = "conversation_patrol";
    patrol.waypoints.push_back({1, 5000, game::SectorPatrolGait::Walk, true, 90.0f});
    fixture.map.patrols.push_back(patrol);
    game::NpcPatrolState state;
    state.patrolEditorId = 9; state.phase = game::NpcPatrolPhase::Waiting;
    state.waitRemainingSeconds = 5;
    fixture.context.world.Add(fixture.npc, state);
    game::NpcPatrolRuntime runtime;
    game::InitializeNpcPatrolRuntime(runtime, 4);
    npc.conversationHeld = true;
    game::UpdateNpcPatrolSystem(fixture.context.world, fixture.navigation,
            fixture.objects.objectSectorLookupWorld, fixture.npcNavigation, runtime, fixture.map, 0.2f, false);
    assert(fixture.context.world.Get<game::NpcPatrolState>(fixture.npc).waitRemainingSeconds == 5);
    npc.conversationHeld = false;
    game::UpdateNpcPatrolSystem(fixture.context.world, fixture.navigation,
            fixture.objects.objectSectorLookupWorld, fixture.npcNavigation, runtime, fixture.map, 0.2f, false);
    assert(fixture.context.world.Get<game::NpcPatrolState>(fixture.npc).waitRemainingSeconds < 5);
}

void EndingConversationRestoresOnlyRepositionedNpcFacing()
{
    for (bool reposition : {false, true}) {
        NpcScriptFixture fixture;
        fixture.host.controls.setControlsEnabled =
                [](void*, engine::EngineContext&, bool, std::string&) { return true; };
        auto& transform = fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc);
        auto& npc = fixture.context.world.Get<game::NpcRuntimeInstance>(fixture.npc);
        transform.yawRadians = 170.0f * DEG2RAD;
        fixture.files.Write(reposition
                ? "function init() assert(startConversation('script_guard')); assert(endConversation()); ended = true end"
                : "function init() assert(startConversation('script_guard', {reposition=false})); delay(10); assert(endConversation()); ended = true end");
        assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
        assert(fixture.host.conversation.active);
        assert(!fixture.cutscene.controlsEnabled);
        // Simulate the prepared facing, including a wrap across +/- pi.
        transform.yawRadians = -170.0f * DEG2RAD;
        if (reposition) {
            const auto preparation = fixture.host.conversation.preparation;
            fixture.host.conversation.preparing = false;
            fixture.host.conversation.preparation = {};
            engine::ScriptSystemCompleteOperation(fixture.runtime, preparation);
        }
        engine::ScriptSystemUpdate(fixture.context, fixture.runtime, 0.02f);
        assert(engine::ScriptSystemExecuteConsole(fixture.runtime, "assert(ended)").success);
        assert(!fixture.host.conversation.active && fixture.cutscene.controlsEnabled);
        assert(npc.conversationHeld == reposition);
        assert(game::HasNpcBodyTurn(fixture.npcNavigation, fixture.npc) == reposition);
        assert(std::fabs(transform.yawRadians - (-170.0f * DEG2RAD)) < 0.0001f);
        const Vector3 position = transform.position;
        fixture.Update(0.375f);
        const float halfway = (reposition ? -180.0f : -170.0f) * DEG2RAD;
        assert(std::fabs(transform.yawRadians - halfway) < 0.0001f);
        fixture.Update(0.375f);
        const float finalYaw = (reposition ? -190.0f : -170.0f) * DEG2RAD;
        assert(std::fabs(transform.yawRadians - finalYaw) < 0.0001f);
        assert(Vector3Distance(transform.position, position) == 0);
        assert(!npc.conversationHeld && !game::HasNpcBodyTurn(fixture.npcNavigation, fixture.npc));
    }
}

void ConversationReturnPausesTravelAndReleasesItsHold()
{
    NpcScriptFixture fixture;
    fixture.files.Write("function init() end");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
    const auto request = game::RequestNpcMove(fixture.context.world, fixture.navigation,
            fixture.objects.objectSectorLookupWorld, fixture.npcNavigation, "script_guard", {6, 8},
            game::NpcMoveGait::Walk, game::NpcMoveAuthority::Patrol);
    assert(request.accepted);
    auto& transform = fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc);
    auto& npc = fixture.context.world.Get<game::NpcRuntimeInstance>(fixture.npc);
    const Vector3 position = transform.position;
    assert(game::BeginNpcConversationReturn(fixture.context.world, fixture.npcNavigation, fixture.npc, PI));
    for (int i = 0; i < 15; ++i) {
        fixture.Update(0.05f);
        assert(Vector3Distance(transform.position, position) == 0);
    }
    assert(!npc.conversationHeld);
    assert(std::fabs(std::fabs(transform.yawRadians) - PI) < 0.0001f);
    for (int i = 0; i < 20; ++i) fixture.Update(0.05f);
    assert(Vector3Distance(transform.position, position) > 0.2f);
}

void ConversationReturnInterruptionsReleaseTheNpc()
{
    for (int mode = 0; mode < 4; ++mode) {
        NpcScriptFixture fixture;
        fixture.files.Write("function init() end");
        assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
        auto& npc = fixture.context.world.Get<game::NpcRuntimeInstance>(fixture.npc);
        assert(game::BeginNpcConversationReturn(fixture.context.world, fixture.npcNavigation, fixture.npc, PI));
        assert(npc.conversationHeld);
        if (mode == 0) {
            game::CancelNpcBodyTurn(fixture.npcNavigation, fixture.npc, 0, "test cancellation");
            fixture.Update(0.1f);
        } else if (mode == 1) {
            fixture.context.world.Get<game::NpcCombatState>(fixture.npc).dead = true;
            fixture.Update(0.1f);
        } else if (mode == 2) {
            game::DeactivateNpcNavigation(fixture.context.world, fixture.navigation, fixture.npcNavigation, fixture.npc);
        } else {
            game::ShutdownNpcNavigationRuntime(fixture.context.world, fixture.navigation, fixture.npcNavigation);
        }
        assert(!npc.conversationHeld);
        assert(!game::HasNpcBodyTurn(fixture.npcNavigation, fixture.npc));
    }
}

void NpcLookPatrolPauseAndTargetRemoval()
{
    NpcScriptFixture fixture;
    game::SectorCompiledPatrol patrol;
    patrol.sourceAuthoringPatrolId = 9;
    patrol.id = "patrol";
    patrol.waypoints.push_back({1, 5000, game::SectorPatrolGait::Walk, true, 90.0f});
    fixture.map.patrols.push_back(patrol);
    game::NpcPatrolState state;
    state.patrolEditorId = 9;
    state.phase = game::NpcPatrolPhase::Waiting;
    state.waitRemainingSeconds = 5.0f;
    state.scriptMoveStopsPatrol = true;
    fixture.context.world.Add(fixture.npc, state);
    game::NpcPatrolRuntime patrolRuntime;
    game::InitializeNpcPatrolRuntime(patrolRuntime, 4);
    const auto target = SpawnScriptNpc(fixture.context.world);
    fixture.context.world.Get<game::NpcRuntimeInstance>(target).instanceId = "target";
    fixture.context.world.Get<game::SectorObjectTransform>(target).position = {8, 0, 8};
    fixture.files.Write("function init() turn = assert(startNpcLookAtNpc('script_guard', 'target', 1000)) end");
    assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
    game::UpdateNpcPatrolSystem(fixture.context.world, fixture.navigation,
            fixture.objects.objectSectorLookupWorld, fixture.npcNavigation, patrolRuntime, fixture.map, 0.2f, false);
    auto& current = fixture.context.world.Get<game::NpcPatrolState>(fixture.npc);
    assert(current.waitRemainingSeconds == 5.0f && !current.stoppedByScript);
    fixture.context.world.DestroyLater(target);
    fixture.context.world.FlushDestroyedEntities();
    fixture.Update(0.1f);
    assert(engine::ScriptSystemExecuteConsole(fixture.runtime, "assert(operationStatus(turn) == 'failed')").success);
    game::UpdateNpcPatrolSystem(fixture.context.world, fixture.navigation,
            fixture.objects.objectSectorLookupWorld, fixture.npcNavigation, patrolRuntime, fixture.map, 0.2f, false);
    assert(current.waitRemainingSeconds < 5.0f && !current.stoppedByScript);
}

void NpcFacingCancellationDeathAndRemovalReleaseOwnership()
{
    for (int mode = 0; mode < 4; ++mode) {
        NpcScriptFixture fixture;
        fixture.playerState.feetPosition = {8, 0, 8};
        fixture.files.Write("function init() turn = assert(startNpcLookAtPlayer('script_guard', 1000)) end");
        assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
        if (mode == 0) {
            fixture.context.world.Get<game::NpcCombatState>(fixture.npc).dead = true;
        } else if (mode == 1) {
            game::NpcAiState ai;
            ai.awareness = game::NpcAwarenessState::InvestigatingTravel;
            fixture.context.world.Add(fixture.npc, ai);
        } else if (mode == 2) {
            fixture.context.world.DestroyLater(fixture.npc);
            fixture.context.world.FlushDestroyedEntities();
        } else {
            fixture.playerState.feetPosition = {2, 0, 8};
        }
        fixture.Update(0.1f);
        assert(!game::HasNpcBodyTurn(fixture.npcNavigation, fixture.npc));
        assert(engine::ScriptSystemExecuteConsole(fixture.runtime,
                "assert(operationStatus(turn) ~= 'running'); assert(startNpcLookAtPlayer('script_guard', 100) == nil)").success);
    }
    for (bool takeover : {false, true}) {
        NpcScriptFixture fixture;
        fixture.map.levelMarkers[0].position = {16, 0, 64};
        fixture.files.Write("function init() movement = assert(startMoveNpc('script_guard', 'run_target', nil, nil, true)) end");
        assert(Create(fixture.context, fixture.runtime, fixture.persistent, fixture.host, fixture.files));
        fixture.Update(0.1f);
        auto& record = fixture.npcNavigation.records.front();
        assert(record.arrivalReached && game::IsNull(record.pathHandle));
        if (takeover) {
            game::InterruptSectorScriptNpcMoveForAi(fixture.context, fixture.host, "script_guard");
        } else {
            assert(engine::ScriptSystemExecuteConsole(fixture.runtime, "assert(cancelOperation(movement))").success);
        }
        assert(record.phase == game::NpcMovePhase::Cancelled && !record.matchArrivalOrientation);
        const float yaw = fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc).yawRadians;
        fixture.Update(0.2f);
        assert(fixture.context.world.Get<game::SectorObjectTransform>(fixture.npc).yawRadians == yaw);
    }
}

} // namespace

void RunSectorScriptBindingTests()
{
    extern void RunSectorDialogueTests();
    RunSectorDialogueTests();
    NpcMarkerArrivalOrientationTurnsAfterStopping();
    NpcTurnTimingRespectsDoorsReplansAndAngleWrap();
    NpcLookTargetsCompleteAndPreserveHeadAnimation();
    NpcLookValidationReplacementMovementAndLifecycle();
    ConversationHoldPausesAndResumesNpcTravel();
    EndingConversationRestoresOnlyRepositionedNpcFacing();
    ConversationReturnPausesTravelAndReleasesItsHold();
    ConversationReturnInterruptionsReleaseTheNpc();
    NpcLookPatrolPauseAndTargetRemoval();
    NpcFacingCancellationDeathAndRemovalReleaseOwnership();
    DoorCompletionAndCancellationShareTheBackend();
    StableDoorAndDynamicLightBindingsMutateRuntimeTargets();
    DoorPermissionCallbacksCanYieldAndMustReturnTrue();
    NpcAnimationBindingsRejectInvalidRequestsWithoutMutation();
    NpcAnimationOperationsCompleteReplaceCancelAndUnload();
    NpcMovementClearsAnimationOverridesOnlyWhenAccepted();
    NpcAnimationRemovalResolvesOperation();
    HealthBindingsSetPlayerAndNpcCurrentHealth();
    SettingNpcHealthToZeroUsesNpcDeathState();
    BlockingNpcMoveCompletesAfterPhysicalArrival();
    BackgroundNpcPatrolYieldsWhenNavigationIsPrepared();
    AsyncPlayerMoveUsesNavigationMarkerAndSpeedOverride();
    PlayerRouteCameraAnticipatesAndSmoothsWithoutChangingMovement();
    PlayerArrivalLookBindingsValidateAndReserveOwnership();
    PlayerArrivalLooksOverlapAndCompleteWithTheirMove();
    PlayerArrivalLookHandlesDelaysMovingTargetsAndZeroDistance();
    PlayerArrivalLookCancellationAndRemovalReleaseBothActions();
    KillingMovingNpcStopsRunawayPatrolWithoutFreezing();
    BlockingNpcMoveReplansAfterDynamicObstacleChange();
    NpcMoveLevelMarkerOverloadsResolvePositionOnly();
    AsyncNpcMoveSupportsAwaitDuplicateValidationAndCancellation();
    AsyncNpcMoveCancellationAndLifecycleFailuresResolve();
    AiTakeoverCancelsScriptNpcMoveWithReason();
    NpcMoveRebuildDeletionUnloadAndImmediateFailuresResolve();
    TravelPreservesFirstRequest();
    TriggerContainmentUsesExplicitCoordinateSpaces();
    MapAudioBindingsForwardOptionalPlaybackSettings();
    CutsceneBindingsControlFadeAndCaptionTimelines(false);
    CutsceneBindingsControlFadeAndCaptionTimelines(true);
    AsyncCaptionsAreConsoleSafeAndBlockingCallsAreSideEffectFree();
    SpeechCompletionGatesCaptionHold();
    DisabledDialogueVoicesKeepTextAndCanBeReenabled();
    CutsceneControlOwnershipRecoversAndRejectsCompetingTasks(false);
    CutsceneControlOwnershipRecoversAndRejectsCompetingTasks(true);
    CinematicTransitionsAndCaptionLayout();
    TriggerDispatchDelayRepeatAndEnableControls();
}
