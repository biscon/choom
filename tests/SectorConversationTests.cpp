#include "game/SectorScriptBindings.h"
#include "game/dialogue/SectorDialogue.h"
#include "game/cutscene/SectorCutsceneRuntime.h"
#include "engine/EngineContext.h"
#include "engine/scripting/ScriptSystem.h"
#include "engine/scripting/ScriptPersistence.h"
#include "engine/scripting/ScriptConsole.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorUseInteraction.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorStaticModelCollision.h"
#include "sector_demo/SectorTopologyMap.h"
#include <raymath.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace {
game::SectorTopologyMap Room()
{
    game::SectorTopologyMap map;
    game::SectorTopologySector sector;
    sector.id = 1;
    sector.ceilingZ = 32;
    map.sectors.push_back(sector);
    const game::SectorCoord points[4][2] = {{0, 0}, {2048, 0}, {2048, 2048}, {0, 2048}};
    for (int i = 0; i < 4; ++i) {
        map.vertices.push_back({i + 1, points[i][0], points[i][1]});
        map.lineDefs.push_back({i + 1, i + 1, (i + 1) % 4 + 1, i + 1, -1});
        game::SectorTopologySideDef side;
        side.id = i + 1; side.lineDefId = i + 1; side.sectorId = 1;
        map.sideDefs.push_back(side);
    }
    return map;
}

struct Fixture {
    engine::EngineContext context;
    engine::ScriptRuntime scripts;
    engine::PersistentScriptStore persistent;
    game::SectorRuntimeObjectState objects;
    game::SectorTopologyMap map = Room();
    game::SectorCollisionWorld collision;
    game::SectorScriptHost host;
    game::SectorCutsceneRuntime cutscene;
    game::SectorDialogueRuntime dialogue;
    game::SectorFpsControllerState player;
    game::SectorFpsControllerConfig config;
    engine::Entity npc{};
    int holsterRequests = 0;
    std::filesystem::path root = std::filesystem::temp_directory_path()
            / ("engine_conversation_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture()
    {
        std::filesystem::create_directories(root);
        assert(collision.BuildFromTopology(map));
        context.world.ReserveEntities(4);
        context.world.ReserveComponentTypes(6);
        context.world.ReserveComponent<game::NpcRuntimeInstance>(4);
        context.world.ReserveComponent<game::SectorObjectTransform>(4);
        context.world.ReserveComponent<game::SectorObject>(4);
        context.world.ReserveComponent<game::SectorDynamicModel>(4);
        context.world.ReserveComponent<game::Health>(4);
        context.world.ReserveComponent<game::NpcCombatState>(4);
        context.world.LockComponentRegistration();
        npc = context.world.CreateEntity();
        game::NpcRuntimeInstance instance;
        instance.instanceId = "elin"; instance.displayName = "Elin"; instance.onUseScript = "useNpc";
        context.world.Add(npc, instance);
        context.world.Add(npc, game::SectorObjectTransform{{3, 0, 3}, 0});
        context.world.Add(npc, game::SectorObject{});
        context.world.Add(npc, game::SectorDynamicModel{});
        context.world.Add(npc, game::MakeHealth(100));
        context.world.Add(npc, game::NpcCombatState{});
        player.feetPosition = {3.5f, 0, 3}; player.yawRadians = PI; player.currentSectorId = 1; player.grounded = true;
        game::InitializeSectorCutsceneRuntime(cutscene);
        game::InitializeSectorScriptHost(host, objects, map, scripts, nullptr, nullptr, {}, nullptr,
                &cutscene, &player, &config);
        host.dialogue = &dialogue;
        host.controls.setControlsEnabled = [](void*, engine::EngineContext&, bool, std::string&) { return true; };
        host.controls.userData = this;
        host.controls.holsterWeapon = [](void* userData) {
            ++static_cast<Fixture*>(userData)->holsterRequests;
        };
        dialogue.sets.push_back({"topics", {{"tunnels", "Tunnels?"}, {"people", "People?"}, {"goodbye", "Goodbye"}}});
        dialogue.visible.reserve(3); dialogue.rows.reserve(3); dialogue.lines.reserve(100);
    }
    ~Fixture()
    {
        if (scripts.vm) engine::ScriptSystemShutdownForMap(context, scripts);
        game::EndSectorScriptConversation(context, host);
        std::filesystem::remove_all(root);
    }
    void Create(const std::string& source)
    {
        std::ofstream(root / "test.lua") << source;
        std::string error;
        assert(engine::ScriptSystemCreateForMap(context, scripts, persistent, "test",
                (root / "test.json").string(), root.string(), &host, game::RegisterSectorScriptBindings, false, error));
    }
    void Tick(float dt = 0.025f, bool frozen = false, bool block = false)
    {
        game::UpdateSectorScriptConversationOwnership(context, host);
        if (host.conversation.preparing && !frozen) {
            auto& transform = context.world.Get<game::SectorObjectTransform>(npc);
            const Vector2 movement = game::PrepareSectorConversationFrame(host.conversation,
                    player, config, transform.position, &collision, dt);
            if (!block) { player.feetPosition.x += movement.x; player.feetPosition.z += movement.y; }
            if (game::FinishSectorConversationFrame(host.conversation, player, config,
                    transform.position, game::SectorConversationTalkPoint(context.world, context.assets, npc),
                    transform.yawRadians, dt)) {
                const auto operation = host.conversation.preparation;
                host.conversation.preparation = {};
                engine::ScriptSystemCompleteOperation(scripts, operation);
            }
        }
        engine::ScriptSystemUpdate(context, scripts, dt);
        game::UpdateSectorScriptConversationOwnership(context, host);
        game::UpdateSectorScriptCutsceneControlOwnership(context, host);
    }
    void Call() {
        const engine::ScriptValue argument = std::string{"elin"};
        const auto result = engine::ScriptSystemCallForegroundHook(scripts, "useNpc", &argument, 1);
        assert(result.result == engine::ScriptCallResult::Started || result.result == engine::ScriptCallResult::Completed);
    }
};

void UseEligibilityAndOcclusion()
{
    Fixture f;
    auto& world = f.context.world;
    const auto pick = [&](Vector3 eye, Vector3 forward) {
        return game::FindSectorUseTarget(world, nullptr, eye, forward, &f.collision,
                true, nullptr, 1.75f, 1, &f.objects);
    };
    auto target = pick({5, 1.5f, 3}, {-1, 0, 0});
    assert(target.kind == game::SectorUseTargetKind::Npc && target.entity == f.npc);
    assert(game::SectorUseTargetTitle(world, target) == "Elin");
    assert(pick({3.4f, 1.5f, 3}, {-1, 0, 0}).entity == f.npc);
    assert(pick({5, 1.5f, 3}, {1, 0, 0}).kind == game::SectorUseTargetKind::None);
    assert(pick({5.81f, 1.5f, 3}, {-1, 0, 0}).kind == game::SectorUseTargetKind::None);
    auto& npc = world.Get<game::NpcRuntimeInstance>(f.npc);
    npc.hostile = true;
    assert(pick({5, 1.5f, 3}, {-1, 0, 0}).kind == game::SectorUseTargetKind::None);
    npc.hostile = false; npc.onUseScript.clear();
    assert(pick({5, 1.5f, 3}, {-1, 0, 0}).kind == game::SectorUseTargetKind::None);
    npc.onUseScript = "useNpc";
    world.Get<game::NpcCombatState>(f.npc).dead = true;
    assert(pick({5, 1.5f, 3}, {-1, 0, 0}).kind == game::SectorUseTargetKind::None);
    world.Get<game::NpcCombatState>(f.npc).dead = false;
    world.Get<game::SectorObject>(f.npc).visible = false;
    assert(pick({5, 1.5f, 3}, {-1, 0, 0}).kind == game::SectorUseTargetKind::None);
    world.Get<game::SectorObject>(f.npc).visible = true;
    game::SectorStaticModelCollider obstacle;
    obstacle.center = {4, 3}; obstacle.halfExtents = {0.1f, 1}; obstacle.bottom = 0; obstacle.top = 2; obstacle.resolved = true;
    f.objects.physicalModelColliders.push_back(obstacle);
    assert(pick({5, 1.5f, 3}, {-1, 0, 0}).kind == game::SectorUseTargetKind::None);
    f.objects.physicalModelColliders.clear();
    game::SectorDynamicDoorCollider door;
    door.center = {4, 3}; door.halfExtents = {0.1f, 1}; door.bottom = 0; door.top = 2;
    f.objects.dynamicDoorColliders.push_back(door);
    assert(pick({5, 1.5f, 3}, {-1, 0, 0}).kind == game::SectorUseTargetKind::None);
}

void BackwardPreparationAndCleanup()
{
    Fixture f;
    f.Create(R"(
        function useNpc(id)
            assert(id == 'elin')
            assert(startConversation(id))
            setFlag('ready', true)
            assert(dialogue('topics') == 'goodbye')
            assert(endConversation())
            setFlag('finished', true)
        end
    )");
    f.Call();
    assert(f.host.conversation.preparing && !f.cutscene.controlsEnabled);
    assert(f.holsterRequests == 1); // Holster before repositioning or the first menu.
    assert(f.context.world.Get<game::NpcRuntimeInstance>(f.npc).conversationHeld);
    for (int i = 0; i < 20; ++i) f.Tick(0.025f, true);
    assert(f.host.conversation.elapsed == 0 && f.player.feetPosition.x == 3.5f);
    const auto duplicate = engine::ScriptSystemCallForegroundHook(f.scripts, "useNpc");
    assert(duplicate.result == engine::ScriptCallResult::AlreadyRunning
            || duplicate.result == engine::ScriptCallResult::ForegroundBusy);
    for (int i = 0; i < 90 && !f.dialogue.active; ++i) {
        const float oldX = f.player.feetPosition.x;
        f.Tick();
        assert(f.player.feetPosition.x >= oldX && f.player.feetPosition.x <= 4.51f);
        assert(std::fabs(std::remainder(f.player.yawRadians - PI, 2 * PI)) < 0.01f);
    }
    assert(f.dialogue.active && f.persistent.bools.at("ready"));
    assert(f.holsterRequests == 2);
    assert(f.player.feetPosition.x > 4.45f);
    assert(std::fabs(f.context.world.Get<game::SectorObjectTransform>(f.npc).yawRadians - PI / 2) < 0.01f);
    const auto finalPosition = f.player.feetPosition;
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 2)); f.Tick();
    assert(f.persistent.bools.at("finished") && !f.host.conversation.active && f.cutscene.controlsEnabled);
    assert(!f.context.world.Get<game::NpcRuntimeInstance>(f.npc).conversationHeld);
    assert(f.player.feetPosition.x == finalPosition.x);
    assert(f.holsterRequests == 2); // Ending the conversation does not change weapon state.
}

void StagingOwnershipAndFailures()
{
    Fixture f;
    f.Create(R"(
        function useNpc(id)
            assert(startCutscene())
            assert(startConversation(id, {reposition=false}))
            assert(not endCutscene())
            dialogue('topics')
            assert(endConversation())
            delay(500)
            assert(endCutscene())
        end
        function failing()
            assert(startConversation('elin', {reposition=false}))
            error('expected failure')
        end
        function held()
            assert(startConversation('elin'))
            delay(10000)
        end
    )");
    f.Call();
    assert(f.host.conversation.active && f.cutscene.presentation.active);
    assert(f.holsterRequests == 3); // Cutscene, staged conversation, then menu.
    assert(!f.context.world.Get<game::NpcRuntimeInstance>(f.npc).conversationHeld);
    f.Tick(); assert(f.player.feetPosition.x == 3.5f && f.player.yawRadians == PI);
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 2)); f.Tick();
    assert(!f.host.conversation.active && !f.cutscene.controlsEnabled && f.cutscene.presentation.active);
    for (int i = 0; i < 22; ++i) f.Tick();
    assert(f.cutscene.controlsEnabled);
    assert(f.holsterRequests == 3);
    engine::ScriptSystemCallForegroundHook(f.scripts, "failing"); f.Tick();
    assert(!f.host.conversation.active && f.cutscene.controlsEnabled);
    engine::ScriptSystemCallForegroundHook(f.scripts, "held");
    const auto owner = f.host.conversation.owner;
    assert(engine::ScriptSystemRequestStopTask(f.scripts, owner)); f.Tick();
    assert(!f.host.conversation.active && f.cutscene.controlsEnabled);
    engine::ScriptSystemCallForegroundHook(f.scripts, "held");
    f.context.world.Get<game::NpcCombatState>(f.npc).dead = true; f.Tick();
    assert(!f.host.conversation.active && f.cutscene.controlsEnabled);
}

void CutsceneStartHolstersWithoutRestoringWeaponOnExit()
{
    Fixture f;
    f.Create(R"(
        function useNpc(id)
            assert(enableControls(false))
            delay(10)
            assert(startCutscene())
            delay(10)
            assert(endCutscene())
        end
    )");
    assert(engine::ScriptSystemExecuteConsole(f.scripts, "assert(not startCutscene())").success);
    assert(f.holsterRequests == 0);
    f.Call();
    assert(!f.cutscene.controlsEnabled && !f.cutscene.presentation.active);
    assert(f.holsterRequests == 0); // A plain control lock does not holster.
    f.Tick();
    assert(f.cutscene.presentation.active && f.holsterRequests == 1);
    f.Tick();
    assert(f.cutscene.controlsEnabled && !f.cutscene.presentation.active);
    assert(f.holsterRequests == 1);
}

void StandaloneDialogueHolstersOnlyOnSuccessfulStart()
{
    Fixture f;
    f.Create(R"(
        function useNpc(id)
            assert(not startConversation('missing'))
            assert(not dialogue('missing'))
            delay(10)
            assert(dialogue('topics') == 'goodbye')
            setFlag('finished', true)
        end
    )");
    f.Call();
    assert(f.holsterRequests == 0);
    f.Tick();
    assert(f.dialogue.active && !f.host.conversation.active);
    assert(f.holsterRequests == 1);
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 2));
    f.Tick();
    assert(f.persistent.bools.at("finished") && !f.dialogue.active);
    assert(f.holsterRequests == 1);
}

void BlockedRetreatAndRemainingTopics()
{
    Fixture f;
    f.Create(R"(
        function useNpc(id)
            assert(startConversation(id))
            runConversationDynamic('topics', {
                tunnels=function() setFlag('tunnels', true) end,
                people=function() setFlag('people', true) end,
                goodbye=function()
                    setInt('visits', getInt('visits')+1)
                    return 'exit'
                end
            }, function() return hiddenOptions({tunnels=flag('tunnels'), people=flag('people')}) end)
            assert(endConversation())
        end
    )");
    f.Call();
    for (int i = 0; i < 90 && !f.dialogue.active; ++i) f.Tick(0.025f, false, true);
    assert(f.dialogue.active && f.player.feetPosition.x == 3.5f);
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 2)); f.Tick();
    assert(f.persistent.ints.at("visits") == 1);
    f.Call(); for (int i = 0; i < 90 && !f.dialogue.active; ++i) f.Tick();
    assert(f.dialogue.visible.size() == 3);
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 0)); f.Tick();
    assert(f.dialogue.visible.size() == 2 && f.dialogue.visible[0] == 1);
    assert(game::SelectSectorDialogue(f.dialogue, f.scripts, 1)); f.Tick();
    std::string json, error;
    assert(engine::SavePersistentScriptStoreToJsonString(f.persistent, json, error));
    f.persistent = {};
    assert(engine::LoadPersistentScriptStoreFromJsonString(json, f.persistent, error));
    engine::ScriptSystemShutdownForMap(f.context, f.scripts);
    assert(engine::ScriptSystemCreateForMap(f.context, f.scripts, f.persistent, "test",
            (f.root / "test.json").string(), f.root.string(), &f.host,
            game::RegisterSectorScriptBindings, true, error));
    f.Call(); for (int i = 0; i < 90 && !f.dialogue.active; ++i) f.Tick();
    assert(f.dialogue.visible.size() == 2 && f.dialogue.visible[0] == 1);
}
void UnsupportedFloorSidewaysAndTargetRemoval()
{
    Fixture f;
    f.Create(R"(
        function useNpc(id)
            assert(startConversation(id))
            dialogue('topics')
            assert(endConversation())
        end
    )");
    f.player.yawRadians = PI / 2;
    f.Call();
    const auto& transform = f.context.world.Get<game::SectorObjectTransform>(f.npc);
    auto movement = game::PrepareSectorConversationFrame(f.host.conversation, f.player,
            f.config, transform.position, &f.collision, 0.025f);
    assert(movement.x == 0 && movement.y == 0); // turn before retreating
    for (int i = 0; i < 90 && !f.dialogue.active; ++i) f.Tick();
    assert(f.dialogue.active && f.player.feetPosition.x > 3.5f);
    assert(std::fabs(std::remainder(f.player.yawRadians - PI, 2 * PI)) < 0.01f);
    game::SelectSectorDialogue(f.dialogue, f.scripts, 2); f.Tick();

    // An unsupported destination is not accepted even if its planar sector exists.
    f.player.feetPosition = {3.5f, 1, 3}; f.player.grounded = true;
    f.Call();
    movement = game::PrepareSectorConversationFrame(f.host.conversation, f.player,
            f.config, transform.position, &f.collision, 0.025f);
    assert(movement.x == 0 && movement.y == 0 && f.host.conversation.retreatStopped);
    for (int i = 0; i < 40 && !f.dialogue.active; ++i) f.Tick();
    assert(f.dialogue.active && f.player.feetPosition.x == 3.5f);
    f.context.world.DestroyLater(f.npc); f.context.world.FlushDestroyedEntities();
    f.Tick();
    assert(!f.host.conversation.active && !f.dialogue.active && f.cutscene.controlsEnabled);
}

}
void RunSectorConversationTests()
{
    UnsupportedFloorSidewaysAndTargetRemoval();
    UseEligibilityAndOcclusion();
    BackwardPreparationAndCleanup();
    StagingOwnershipAndFailures();
    CutsceneStartHolstersWithoutRestoringWeaponOnExit();
    StandaloneDialogueHolstersOnlyOnSuccessfulStart();
    BlockedRetreatAndRemainingTopics();
}
