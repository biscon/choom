#include "game/save/GameSaveSerialization.h"
#include "game/save/GameSaveStorage.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

game::GameSaveData MakeSave()
{
    game::GameSaveData save;
    save.slot = 3;
    save.name = "Generator online";
    save.savedAtUtc = "2026-08-28T19:42:10Z";
    save.thumbnailFile = "slot03_test.png";
    save.currentLevelId = "refinery";
    save.player.feetPosition = {1.0f, 2.0f, 3.0f};
    save.player.yawRadians = 0.7f;
    save.player.pitchRadians = -0.2f;
    save.player.health = {100, 125, 73};
    save.player.stamina = {120.0f, 41.5f, true};

    save.itemCampaign.inventory.entries.push_back(
            {7, "medkit", 2, "useMedkit", 4});
    save.itemCampaign.inventory.nextRuntimeId = 8;
    save.itemCampaign.weapons.activeWeaponId = "pistol";
    save.itemCampaign.weapons.magazines.push_back({"pistol", 6});
    save.itemCampaign.healingEffects.push_back({20, 4.0f, 1.0f, 5});
    game::ItemLevelCampaignState itemLevel;
    itemLevel.levelId = "refinery";
    itemLevel.collectedAuthoredItemIds = {4, 9};
    itemLevel.nextDroppedObjectId = 101;
    game::SectorPlacedRuntimeObject drop;
    drop.id = 100;
    drop.kind = "item";
    drop.position = {4.0f, 0.5f, 8.0f};
    drop.item.definitionId = "ammo";
    drop.item.instanceId = "drop_100";
    drop.item.quantity = 12;
    drop.item.sessionDrop = true;
    itemLevel.droppedItems.push_back(drop);
    save.itemCampaign.levels.push_back(std::move(itemLevel));

    save.persistentScripts.bools["generator_started"] = true;
    save.persistentScripts.ints["fuses"] = 2;
    save.persistentScripts.strings["route"] = "service";

    game::GameSaveLevelState level;
    level.levelId = "refinery";
    level.doors.push_back({10, "generator_door", 0.65f, 1.0f, true});
    game::GameSavePropState prop;
    prop.placedObjectId = 11;
    prop.instanceId = "generator_lever";
    prop.emissiveScale = 2.0f;
    prop.useConsumed = true;
    prop.hasAnimator = true;
    prop.animator.animationName = "pull";
    prop.animator.frame = 13.5f;
    prop.animator.playing = false;
    level.props.push_back(prop);
    game::GameSaveNpcState npc;
    npc.placedObjectId = 12;
    npc.instanceId = "guard_a";
    npc.position = {8.0f, 0.0f, 9.0f};
    npc.yawRadians = 1.2f;
    npc.health = {100, 100, 0};
    npc.dead = true;
    npc.deathAnimationComplete = true;
    npc.corpseElapsedSeconds = 5.0f;
    npc.hasPatrol = true;
    npc.patrolEditorId = 2;
    npc.waypointIndex = 1;
    npc.direction = -1;
    npc.shuffleOrder = {2, 0, 1};
    npc.shuffleCursor = 1;
    npc.randomState = 42;
    npc.lookDirection = 1.0f;
    level.npcs.push_back(npc);
    level.billboards.push_back({13, 2.5f, 0.8f, true, true, false});
    level.dynamicLights.push_back(
            {"generator_glow", Color{20, 90, 255, 255}, 5.0f, true});
    level.triggers.push_back(
            {"generator_trigger", false, true, false, true, 0.0f});
    save.levels.push_back(std::move(level));
    return save;
}

void SerializationRoundTripsStableState()
{
    const game::GameSaveData source = MakeSave();
    std::string encoded;
    std::string error;
    assert(game::SerializeGameSave(source, encoded, error));
    assert(!encoded.empty());

    game::GameSaveData restored;
    assert(game::DeserializeGameSave(encoded, restored, error));
    assert(restored.slot == 3);
    assert(restored.name == source.name);
    assert(restored.player.health.current == 73);
    assert(restored.itemCampaign.inventory.entries.size() == 1);
    assert(restored.itemCampaign.levels[0].droppedItems[0].item.sessionDrop);
    assert(restored.persistentScripts.bools.at("generator_started"));
    assert(restored.levels.size() == 1);
    assert(restored.levels[0].props[0].animator.frame == 13.5f);
    assert(restored.levels[0].npcs[0].dead);
    assert(restored.levels[0].npcs[0].deathAnimationComplete);
    assert(restored.levels[0].npcs[0].shuffleOrder.size() == 3);
    assert(restored.levels[0].dynamicLights[0].color.b == 255);
    assert(restored.levels[0].triggers[0].consumed);
}

void InvalidInputDoesNotReplaceDestination()
{
    game::GameSaveData destination = MakeSave();
    destination.name = "Keep me";
    std::string error;
    assert(!game::DeserializeGameSave("{not json", destination, error));
    assert(destination.name == "Keep me");
    assert(!error.empty());
}

void StorageScansSlotsAndRejectsUnsafeNames()
{
    const auto unique = std::chrono::steady_clock::now()
            .time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path()
            / ("game_save_tests_" + std::to_string(unique));
    std::string error;
    game::GameSaveData save = MakeSave();
    assert(game::WriteGameSaveSlot(root, save, error));
    const std::vector<game::GameSaveSlotInfo> slots =
            game::ScanGameSaveSlots(root);
    assert(slots.size() == game::GameSaveSlotCount);
    assert(slots[0].status == game::GameSaveSlotStatus::Empty);
    assert(slots[2].status == game::GameSaveSlotStatus::Ready);
    assert(slots[2].name == save.name);
    assert(slots[2].displayTimestamp.find(':') != std::string::npos);
    assert(!game::IsValidGameSaveName("   \t"));
    assert(!game::IsValidGameSaveName(std::string(49, 'x')));
    assert(game::IsValidGameSaveName("Pump room 2"));
    std::filesystem::remove_all(root);
}

void IncompatibleVersionIsReportedPerSlot()
{
    const auto unique = std::chrono::steady_clock::now()
            .time_since_epoch().count();
    const std::filesystem::path root = std::filesystem::temp_directory_path()
            / ("game_save_version_tests_" + std::to_string(unique));
    std::filesystem::create_directories(root);
    std::ofstream output(root / "slot01.json");
    output << "{\"formatVersion\":999}";
    output.close();
    const auto slots = game::ScanGameSaveSlots(root);
    assert(slots[0].status == game::GameSaveSlotStatus::Incompatible);
    std::filesystem::remove_all(root);
}

} // namespace

int main()
{
    SerializationRoundTripsStableState();
    InvalidInputDoesNotReplaceDestination();
    StorageScansSlotsAndRejectsUnsafeNames();
    IncompatibleVersionIsReportedPerSlot();
    return 0;
}
