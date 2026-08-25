#include "game/items/ItemDefinitions.h"
#include "game/items/ItemIconLayout.h"
#include "game/items/ItemInventory.h"
#include "sector_editor/items/SectorEditorItemEditorService.h"
#include "sector_editor/items/SectorItemReferenceScanner.h"

#include "util/json.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

namespace {

using Json = nlohmann::ordered_json;

game::FpsWeaponRegistry MakeWeapons()
{
    game::FpsWeaponRegistry registry;
    game::FpsWeaponDefinition pistol = game::MakeDefaultFpsWeaponDefinition();
    pistol.id = "pistol";
    pistol.weaponSlot = 1;
    registry.weapons.push_back(std::move(pistol));
    return registry;
}

game::ItemDefinition MakeObject(std::string id)
{
    game::ItemDefinition definition;
    definition.id = std::move(id);
    definition.title = "Useful Object";
    definition.description = "First line.\nSecond line.";
    definition.modelPath = "assets/models/box.glb";
    definition.weightKg = 0.5f;
    return definition;
}

Json DefinitionJson(
        const std::string& id,
        const std::string& type = "object")
{
    Json value = {
            {"id", id},
            {"title", "Title"},
            {"description", "Description"},
            {"modelPath", "assets/models/missing_but_valid.glb"},
            {"type", type},
            {"weightKg", 0.25}};
    if (type == "weapon" || type == "ammo") value["weaponId"] = "pistol";
    if (type == "health") value["healAmount"] = 25;
    return value;
}

void RegistryRoundTripAndValidation()
{
    const game::FpsWeaponRegistry weapons = MakeWeapons();
    Json root = {{"version", 1}, {"items", Json::array()}};
    root["items"].push_back(DefinitionJson("z_object"));
    root["items"].push_back(DefinitionJson("a_weapon", "weapon"));
    root["items"].push_back(DefinitionJson("m_ammo", "ammo"));
    Json health = DefinitionJson("b_health", "health");
    health["healOverTime"] = true;
    health["healDurationSeconds"] = 8.0;
    root["items"].push_back(std::move(health));

    game::ItemRegistry registry;
    std::string error;
    assert(game::ParseItemRegistryJson(root.dump(), weapons, registry, error));
    assert(registry.items.size() == 4);
    assert(registry.items.front().id == "a_weapon");
    assert(registry.items[1].id == "b_health");
    assert(game::FindItemDefinition(registry, "m_ammo") != nullptr);
    std::string serialized;
    assert(game::SerializeItemRegistryJson(
            registry, weapons, serialized, error));
    const size_t weaponPosition = serialized.find("a_weapon");
    const size_t healthPosition = serialized.find("b_health");
    const size_t ammoPosition = serialized.find("m_ammo");
    assert(weaponPosition < healthPosition && healthPosition < ammoPosition);
    game::ItemRegistry roundTrip;
    assert(game::ParseItemRegistryJson(
            serialized, weapons, roundTrip, error));
    assert(roundTrip.items.size() == registry.items.size());

    game::ItemRegistry empty;
    assert(game::ParseItemRegistryJson(
            R"({"version":1,"items":[]})", weapons, empty, error));
    assert(empty.items.empty());

    auto expectFailure = [&](Json invalid) {
        game::ItemRegistry ignored;
        assert(!game::ParseItemRegistryJson(
                invalid.dump(), weapons, ignored, error));
        assert(!error.empty());
    };
    Json invalid = root;
    invalid["items"][0]["id"] = "bad/id";
    expectFailure(invalid);
    invalid = root;
    invalid["items"].push_back(invalid["items"][0]);
    expectFailure(invalid);
    invalid = root;
    invalid["items"][0]["type"] = "quest";
    expectFailure(invalid);
    invalid = root;
    invalid["items"][0]["weightKg"] = -1.0;
    expectFailure(invalid);
    invalid = root;
    invalid["items"][0]["title"] = std::string(97, 'x');
    expectFailure(invalid);
    invalid = root;
    invalid["items"][1]["weaponId"] = "missing";
    expectFailure(invalid);
    invalid = root;
    invalid["items"][3]["healAmount"] = 0;
    expectFailure(invalid);
    invalid = root;
    invalid["items"][3]["healDurationSeconds"] = 0.0;
    expectFailure(invalid);

    game::ItemDefinition missingModel = MakeObject("missing_model");
    missingModel.modelPath = "assets/models/does_not_exist.glb";
    assert(game::ValidateItemDefinition(missingModel, weapons, error));
    missingModel.weightKg = std::numeric_limits<float>::infinity();
    assert(!game::ValidateItemDefinition(missingModel, weapons, error));
}

void ApplicationSettingsInventoryFields()
{
    game::FpsApplicationSettings settings;
    std::string error;
    assert(game::ParseFpsApplicationSettings(
            R"({"version":1})", settings, &error));
    assert(std::abs(settings.playerInventory.maxCarryWeightKg - 30.0f) < 0.001f);
    assert(settings.playerInventory.maxSlots == 24);
    assert(game::ParseFpsApplicationSettings(
            R"({"version":1,"playerInventory":{"maxCarryWeightKg":42.5,"maxSlots":64}})",
            settings, &error));
    assert(std::abs(settings.playerInventory.maxCarryWeightKg - 42.5f) < 0.001f);
    assert(settings.playerInventory.maxSlots == 64);
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerInventory":{"maxCarryWeightKg":0}})",
            settings, &error));
    assert(!game::ParseFpsApplicationSettings(
            R"({"version":1,"playerInventory":{"maxSlots":1025}})",
            settings, &error));

    settings = game::FpsApplicationSettings{};
    settings.playerInventory.maxCarryWeightKg = 35.5f;
    settings.playerInventory.maxSlots = 48;
    const std::filesystem::path path = std::filesystem::temp_directory_path()
            / "item_application_settings_test.json";
    assert(game::SaveFpsApplicationSettings(path.string(), settings, &error));
    game::FpsApplicationSettings loaded;
    assert(game::LoadFpsApplicationSettings(path.string(), loaded, &error));
    assert(std::abs(loaded.playerInventory.maxCarryWeightKg - 35.5f) < 0.001f);
    assert(loaded.playerInventory.maxSlots == 48);
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

void IconLayoutAndCameraFit()
{
    game::ItemRegistry registry;
    for (int index = 9; index >= 1; --index) {
        registry.items.push_back(MakeObject(
                "item_" + std::to_string(index)));
    }
    game::ItemIconAtlasLayout layout;
    std::string error;
    assert(game::BuildItemIconAtlasLayout(registry, layout, error));
    assert(layout.columns == 8 && layout.rows == 2);
    assert(layout.widthPixels == 1024 && layout.heightPixels == 256);
    assert(layout.regions.front().definitionId == "item_1");
    assert(layout.regions[8].source.y == 128.0f);
    assert(game::FindItemIconRegion(layout, "item_7") != nullptr);

    game::ItemRegistry empty;
    assert(game::BuildItemIconAtlasLayout(empty, layout, error));
    assert(layout.columns == 0 && layout.regions.empty());

    const game::ItemIconCameraFit fit = game::BuildItemIconCameraFit(
            BoundingBox{Vector3{-1.0f, -2.0f, -0.5f},
                        Vector3{1.0f, 2.0f, 0.5f}});
    assert(fit.valid);
    assert(fit.orthographicSize > 4.0f);
    assert(fit.farPlane > fit.nearPlane);
    const game::ItemIconCameraFit invalid = game::BuildItemIconCameraFit(
            BoundingBox{Vector3{1.0f, 0.0f, 0.0f},
                        Vector3{-1.0f, 0.0f, 0.0f}});
    assert(!invalid.valid);
}

std::filesystem::path MakeTemporaryRoot(const char* suffix)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path()
            / (std::string{"engine_item_slice1_"} + suffix);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    return root;
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << text;
    assert(output.good());
}

void ReferenceScanningAndEditorService()
{
    const game::FpsWeaponRegistry weapons = MakeWeapons();
    const std::filesystem::path root = MakeTemporaryRoot("editor");
    const std::filesystem::path levels = root / "levels";
    const std::filesystem::path registryPath = root / "items.json";
    WriteText(
            levels / "test" / "test.json",
            R"({"formatVersion":4,"topology":"authoringGraph","runtimeObjects":[]})");

    game::ItemRegistry registry;
    registry.items.push_back(MakeObject("existing"));
    game::SectorEditorItemEditorState state;
    game::SectorEditorItemEditorSessionState session;
    std::string status;
    game::SectorEditorItemEditorService editor(
            state, session, registry, weapons, false, status,
            registryPath, levels);
    assert(editor.Open());
    assert(editor.SelectedItem()->id == "existing");
    editor.AddItem();
    assert(editor.SelectedItem()->id == "new_item");
    editor.AddItem();
    assert(editor.SelectedItem()->id == "new_item_2");
    const std::string immutableId = editor.SelectedItem()->id;
    editor.SetType(game::ItemType::Weapon);
    assert(editor.SelectedItem()->weaponId == "pistol");
    editor.SetType(game::ItemType::Health);
    assert(editor.SelectedItem()->weaponId.empty());
    assert(editor.SelectedItem()->healAmount == 1);
    assert(editor.SelectedItem()->id == immutableId);
    editor.Cancel();
    assert(registry.items.size() == 1);

    WriteText(
            levels / "test" / "test.json",
            R"({"formatVersion":4,"topology":"authoringGraph","runtimeObjects":[{"id":1,"kind":"item","item":{"definitionId":"existing"}}]})");
    assert(editor.Open());
    assert(!editor.RequestDeleteSelected());
    assert(state.validationMessage.find("referenced") != std::string::npos);
    editor.Cancel();

    WriteText(levels / "test" / "test.json", "{ malformed");
    assert(editor.Open());
    assert(!editor.RequestDeleteSelected());
    assert(!state.validationMessage.empty());
    editor.Cancel();

    WriteText(
            levels / "test" / "test.json",
            R"({"formatVersion":4,"topology":"authoringGraph","runtimeObjects":[{"id":1,"kind":"item","item":{}}]})");
    assert(editor.Open());
    assert(!editor.RequestDeleteSelected());
    assert(state.validationMessage.find("definitionId") != std::string::npos);
    editor.Cancel();

    WriteText(
            levels / "test" / "test.json",
            R"({"formatVersion":4,"topology":"authoringGraph","runtimeObjects":[]})");
    game::SectorEditorItemEditorService liveEditor(
            state, session, registry, weapons, true, status,
            registryPath, levels);
    assert(liveEditor.Open());
    assert(!liveEditor.SaveAndClose());
    assert(state.open);
    liveEditor.Cancel();

    game::SectorEditorItemEditorService saveEditor(
            state, session, registry, weapons, false, status,
            registryPath, levels);
    assert(saveEditor.Open());
    saveEditor.AddItem();
    saveEditor.SetModelPath("assets/models/box.glb");
    assert(saveEditor.SaveAndClose());
    assert(registry.items.size() == 2);
    assert(registry.revision == 2);
    std::ifstream savedInput(registryPath);
    const std::string saved{
            std::istreambuf_iterator<char>(savedInput),
            std::istreambuf_iterator<char>()};
    assert(saved.find("new_item") != std::string::npos);

    std::unordered_map<std::string, size_t> counts;
    std::string error;
    WriteText(root / "not_a_directory", "not a directory");
    assert(!game::CountItemDefinitionReferencesInLevels(
            root / "not_a_directory", {"existing"}, counts, error));
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}

game::ItemDefinition MakeInventoryDefinition(
        std::string id,
        game::ItemType type,
        float weightKg)
{
    game::ItemDefinition definition = MakeObject(std::move(id));
    definition.type = type;
    definition.weightKg = weightKg;
    if (type == game::ItemType::Weapon || type == game::ItemType::Ammo) {
        definition.weaponId = "pistol";
    }
    if (type == game::ItemType::Health) definition.healAmount = 10;
    return definition;
}

void InventoryTransactionsAndCampaignReconciliation()
{
    game::ItemRegistry registry;
    registry.items.push_back(MakeInventoryDefinition(
            "ammo", game::ItemType::Ammo, 0.1f));
    registry.items.push_back(MakeInventoryDefinition(
            "object", game::ItemType::Object, 0.5f));
    registry.items.push_back(MakeInventoryDefinition(
            "weapon", game::ItemType::Weapon, 1.0f));

    game::PlayerInventoryApplicationSettings settings;
    settings.maxCarryWeightKg = 10.0f;
    settings.maxSlots = 6;
    game::ItemCampaignState campaign;
    game::InitializeItemCampaignState(campaign, settings);

    game::ItemPickupPlan plan = game::PreflightItemPickup(
            campaign.inventory, registry, settings, "ammo", 10);
    assert(plan.result == game::ItemPickupCapacityResult::Fits);
    assert(game::CommitItemPickup(campaign.inventory, plan, {}));
    assert(campaign.inventory.entries.size() == 1);
    assert(campaign.inventory.entries.front().quantity == 10);
    plan = game::PreflightItemPickup(
            campaign.inventory, registry, settings, "ammo", 5);
    assert(plan.result == game::ItemPickupCapacityResult::Fits);
    assert(plan.addedSlots == 0);
    assert(game::CommitItemPickup(campaign.inventory, plan, {}));
    assert(campaign.inventory.entries.front().quantity == 15);

    plan = game::PreflightItemPickup(
            campaign.inventory, registry, settings, "object", 2);
    assert(plan.result == game::ItemPickupCapacityResult::Fits);
    assert(game::CommitItemPickup(
            campaign.inventory, plan, "useCarriedObject"));
    assert(campaign.inventory.entries.size() == 3);
    assert(campaign.inventory.entries[1].quantity == 1);
    assert(campaign.inventory.entries[2].quantity == 1);
    assert(campaign.inventory.entries[1].onUseScript == "useCarriedObject");

    plan = game::PreflightItemPickup(
            campaign.inventory, registry, settings, "weapon", 2);
    assert(plan.result == game::ItemPickupCapacityResult::Fits);
    assert(game::CommitItemPickup(campaign.inventory, plan, {}));
    assert(campaign.inventory.entries.size() == 5);
    assert(campaign.inventory.entries[3].runtimeId
            != campaign.inventory.entries[4].runtimeId);

    const std::size_t entryCount = campaign.inventory.entries.size();
    const std::uint64_t ammoQuantity = campaign.inventory.entries.front().quantity;
    plan = game::PreflightItemPickup(
            campaign.inventory, registry, settings, "weapon", 2);
    assert(plan.result == game::ItemPickupCapacityResult::WeightLimit
            || plan.result == game::ItemPickupCapacityResult::SlotLimit);
    assert(campaign.inventory.entries.size() == entryCount);
    assert(campaign.inventory.entries.front().quantity == ammoQuantity);

    game::PlayerInventoryState overflowInventory;
    overflowInventory.entries.push_back(game::ItemInventoryEntry{
            1,
            "ammo",
            std::numeric_limits<std::uint64_t>::max(),
            {}});
    registry.items.front().weightKg = 0.0f;
    plan = game::PreflightItemPickup(
            overflowInventory, registry, settings, "ammo", 1);
    assert(plan.result == game::ItemPickupCapacityResult::NumericOverflow);

    game::SectorTopologyMap authoredMap;
    for (int id : {1, 2}) {
        game::SectorPlacedRuntimeObject item;
        item.id = id;
        item.kind = "item";
        item.item.definitionId = "object";
        item.item.instanceId = "authored_" + std::to_string(id);
        authoredMap.runtimeObjects.push_back(std::move(item));
    }
    game::ItemLevelCampaignState& level =
            game::FindOrCreateItemLevelCampaignState(campaign, "test_map", 2);
    assert(game::RecordAuthoredItemCollected(level, 1));
    game::SectorPlacedRuntimeObject drop;
    drop.id = 2;
    drop.kind = "item";
    drop.item.definitionId = "object";
    drop.item.instanceId = "session_drop";
    level.droppedItems.push_back(drop);

    game::ReconcileItemCampaignLevel(campaign, "test_map", authoredMap);
    assert(authoredMap.runtimeObjects.size() == 2);
    assert(authoredMap.runtimeObjects.front().id == 2);
    const int rebasedDropId = authoredMap.runtimeObjects.back().id;
    assert(rebasedDropId > 2);
    assert(authoredMap.runtimeObjects.back().item.sessionDrop);
    assert(level.droppedItems.front().id == rebasedDropId);

    game::SectorTopologyMap revisited;
    for (int id : {1, 2}) {
        game::SectorPlacedRuntimeObject item;
        item.id = id;
        item.kind = "item";
        item.item.definitionId = "object";
        item.item.instanceId = "authored_" + std::to_string(id);
        revisited.runtimeObjects.push_back(std::move(item));
    }
    game::ReconcileItemCampaignLevel(campaign, "test_map", revisited);
    assert(revisited.runtimeObjects.size() == 2);
    assert(revisited.runtimeObjects.back().id == rebasedDropId);
    assert(game::RemoveSessionDroppedItem(level, rebasedDropId));
    assert(level.droppedItems.empty());
}

} // namespace

int main()
{
    RegistryRoundTripAndValidation();
    ApplicationSettingsInventoryFields();
    IconLayoutAndCameraFit();
    ReferenceScanningAndEditorService();
    InventoryTransactionsAndCampaignReconciliation();
    std::cout << "Item definition tests passed\n";
}
