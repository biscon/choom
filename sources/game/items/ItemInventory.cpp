#include "game/items/ItemInventory.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace game {
namespace {

void WarnCapacity(const char* collection, std::uint64_t& counter)
{
    ++counter;
    std::fprintf(
            stderr,
            "[ItemInventory WARNING] %s capacity exceeded; runtime allocation may occur\n",
            collection);
}

bool AddOverflows(std::uint64_t left, std::uint64_t right)
{
    return right > std::numeric_limits<std::uint64_t>::max() - left;
}

} // namespace

void InitializeItemCampaignState(
        ItemCampaignState& state,
        const PlayerInventoryApplicationSettings& settings)
{
    state = ItemCampaignState{};
    state.inventory.entries.reserve(static_cast<std::size_t>(
            std::max(1, settings.maxSlots)));
    state.levels.reserve(16);
}

double ComputeInventoryWeightKg(
        const PlayerInventoryState& inventory,
        const ItemRegistry& registry,
        bool* valid)
{
    double total = 0.0;
    bool resultValid = true;
    for (const ItemInventoryEntry& entry : inventory.entries) {
        const ItemDefinition* definition = FindItemDefinition(
                registry, entry.definitionId);
        if (definition == nullptr || entry.quantity == 0) {
            resultValid = false;
            continue;
        }
        const double contribution = static_cast<double>(definition->weightKg)
                * static_cast<double>(entry.quantity);
        if (!std::isfinite(contribution)
                || contribution > std::numeric_limits<double>::max() - total) {
            resultValid = false;
            total = std::numeric_limits<double>::infinity();
            break;
        }
        total += contribution;
    }
    if (valid != nullptr) *valid = resultValid;
    return total;
}

ItemPickupPlan PreflightItemPickup(
        const PlayerInventoryState& inventory,
        const ItemRegistry& registry,
        const PlayerInventoryApplicationSettings& settings,
        std::string_view definitionId,
        std::uint64_t quantity)
{
    ItemPickupPlan plan;
    plan.quantity = quantity;
    plan.definition = FindItemDefinition(registry, definitionId);
    if (plan.definition == nullptr) return plan;
    if (quantity == 0 || quantity > 1000000u) {
        plan.result = ItemPickupCapacityResult::InvalidQuantity;
        return plan;
    }
    bool validWeight = false;
    const double currentWeight = ComputeInventoryWeightKg(
            inventory, registry, &validWeight);
    const double addedWeight = static_cast<double>(plan.definition->weightKg)
            * static_cast<double>(quantity);
    if (!validWeight || !std::isfinite(addedWeight)
            || addedWeight > std::numeric_limits<double>::max() - currentWeight) {
        plan.result = ItemPickupCapacityResult::NumericOverflow;
        return plan;
    }
    plan.resultingWeightKg = currentWeight + addedWeight;
    if (plan.resultingWeightKg
            > static_cast<double>(settings.maxCarryWeightKg) + 1.0e-9) {
        plan.result = ItemPickupCapacityResult::WeightLimit;
        return plan;
    }
    if (plan.definition->type == ItemType::Ammo) {
        for (std::size_t index = 0; index < inventory.entries.size(); ++index) {
            const ItemInventoryEntry& entry = inventory.entries[index];
            if (entry.definitionId == plan.definition->id) {
                if (AddOverflows(entry.quantity, quantity)) {
                    plan.result = ItemPickupCapacityResult::NumericOverflow;
                    return plan;
                }
                plan.ammoEntryIndex = static_cast<int>(index);
                break;
            }
        }
        plan.addedSlots = plan.ammoEntryIndex >= 0 ? 0u : 1u;
    } else {
        plan.addedSlots = static_cast<std::size_t>(quantity);
    }
    if (plan.addedSlots > static_cast<std::size_t>(settings.maxSlots)
            || inventory.entries.size()
                    > static_cast<std::size_t>(settings.maxSlots)
                            - plan.addedSlots) {
        plan.result = ItemPickupCapacityResult::SlotLimit;
        return plan;
    }
    if (plan.addedSlots != 0
            && (inventory.nextRuntimeId == 0
                    || static_cast<std::uint64_t>(plan.addedSlots - 1)
                            > std::numeric_limits<std::uint64_t>::max()
                                    - inventory.nextRuntimeId)) {
        plan.result = ItemPickupCapacityResult::NumericOverflow;
        return plan;
    }
    plan.result = ItemPickupCapacityResult::Fits;
    return plan;
}

bool CommitItemPickup(
        PlayerInventoryState& inventory,
        const ItemPickupPlan& plan,
        std::string_view onUseScript)
{
    if (plan.result != ItemPickupCapacityResult::Fits
            || plan.definition == nullptr || plan.quantity == 0) {
        return false;
    }
    if (plan.ammoEntryIndex >= 0) {
        ItemInventoryEntry& entry = inventory.entries[
                static_cast<std::size_t>(plan.ammoEntryIndex)];
        if (AddOverflows(entry.quantity, plan.quantity)) return false;
        entry.quantity += plan.quantity;
        return true;
    }
    if (inventory.entries.size() + plan.addedSlots
            > inventory.entries.capacity()) {
        WarnCapacity("entry", inventory.capacityWarnings);
        inventory.entries.reserve(inventory.entries.size() + plan.addedSlots);
    }
    const bool retainUse = plan.definition->type == ItemType::Object;
    const std::uint64_t entryQuantity = plan.definition->type == ItemType::Ammo
            ? plan.quantity : 1u;
    const std::size_t count = plan.definition->type == ItemType::Ammo
            ? 1u : plan.addedSlots;
    for (std::size_t index = 0; index < count; ++index) {
        ItemInventoryEntry entry;
        entry.runtimeId = inventory.nextRuntimeId++;
        entry.definitionId = plan.definition->id;
        entry.quantity = entryQuantity;
        if (retainUse) entry.onUseScript = std::string{onUseScript};
        inventory.entries.push_back(std::move(entry));
    }
    return true;
}

ItemLevelCampaignState& FindOrCreateItemLevelCampaignState(
        ItemCampaignState& campaign,
        std::string_view levelId,
        std::size_t authoredItemCapacity)
{
    const auto found = std::find_if(
            campaign.levels.begin(), campaign.levels.end(),
            [levelId](const ItemLevelCampaignState& level) {
                return level.levelId == levelId;
            });
    if (found != campaign.levels.end()) {
        if (found->collectedAuthoredItemIds.capacity() < authoredItemCapacity) {
            found->collectedAuthoredItemIds.reserve(authoredItemCapacity);
        }
        return *found;
    }
    if (campaign.levels.size() == campaign.levels.capacity()) {
        WarnCapacity("level ledger", campaign.capacityWarnings);
    }
    ItemLevelCampaignState level;
    level.levelId = std::string{levelId};
    level.collectedAuthoredItemIds.reserve(authoredItemCapacity);
    level.droppedItems.reserve(authoredItemCapacity);
    campaign.levels.push_back(std::move(level));
    return campaign.levels.back();
}

bool IsAuthoredItemCollected(
        const ItemLevelCampaignState& level,
        int placedObjectId)
{
    return std::binary_search(
            level.collectedAuthoredItemIds.begin(),
            level.collectedAuthoredItemIds.end(),
            placedObjectId);
}

bool RecordAuthoredItemCollected(
        ItemLevelCampaignState& level,
        int placedObjectId)
{
    const auto found = std::lower_bound(
            level.collectedAuthoredItemIds.begin(),
            level.collectedAuthoredItemIds.end(),
            placedObjectId);
    if (found != level.collectedAuthoredItemIds.end()
            && *found == placedObjectId) return true;
    if (level.collectedAuthoredItemIds.size()
            == level.collectedAuthoredItemIds.capacity()) {
        WarnCapacity("collected item ledger", level.capacityWarnings);
    }
    level.collectedAuthoredItemIds.insert(found, placedObjectId);
    return true;
}

bool RemoveSessionDroppedItem(
        ItemLevelCampaignState& level,
        int placedObjectId)
{
    const auto found = std::find_if(
            level.droppedItems.begin(), level.droppedItems.end(),
            [placedObjectId](const SectorPlacedRuntimeObject& object) {
                return object.id == placedObjectId;
            });
    if (found == level.droppedItems.end()) return false;
    level.droppedItems.erase(found);
    return true;
}

void ReconcileItemCampaignLevel(
        ItemCampaignState& campaign,
        std::string_view levelId,
        SectorTopologyMap& map)
{
    std::size_t authoredCount = 0;
    int maximumId = 0;
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        maximumId = std::max(maximumId, object.id);
        if (object.kind == "item") ++authoredCount;
    }
    ItemLevelCampaignState& level = FindOrCreateItemLevelCampaignState(
            campaign, levelId, authoredCount);
    map.runtimeObjects.erase(
            std::remove_if(
                    map.runtimeObjects.begin(), map.runtimeObjects.end(),
                    [&level](const SectorPlacedRuntimeObject& object) {
                        return object.kind == "item"
                                && IsAuthoredItemCollected(level, object.id);
                    }),
            map.runtimeObjects.end());
    std::vector<int> usedIds;
    usedIds.reserve(map.runtimeObjects.size() + level.droppedItems.size());
    for (const SectorPlacedRuntimeObject& object : map.runtimeObjects) {
        usedIds.push_back(object.id);
        maximumId = std::max(maximumId, object.id);
    }
    std::sort(usedIds.begin(), usedIds.end());
    level.nextDroppedObjectId = std::max(level.nextDroppedObjectId, maximumId + 1);
    for (SectorPlacedRuntimeObject& drop : level.droppedItems) {
        if (!IsValidSectorTopologyId(drop.id)
                || std::binary_search(usedIds.begin(), usedIds.end(), drop.id)) {
            while (level.nextDroppedObjectId > 0
                    && std::binary_search(
                            usedIds.begin(), usedIds.end(),
                            level.nextDroppedObjectId)) {
                ++level.nextDroppedObjectId;
            }
            drop.id = level.nextDroppedObjectId++;
            drop.item.instanceId = AllocateSectorItemInstanceId(map, drop.id);
        }
        drop.kind = "item";
        drop.item.sessionDrop = true;
        usedIds.insert(
                std::lower_bound(usedIds.begin(), usedIds.end(), drop.id),
                drop.id);
        map.runtimeObjects.push_back(drop);
        maximumId = std::max(maximumId, drop.id);
    }
    level.nextDroppedObjectId = std::max(level.nextDroppedObjectId, maximumId + 1);
}

} // namespace game
