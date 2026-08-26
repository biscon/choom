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

bool EntriesCanStack(
        const ItemInventoryEntry& left,
        const ItemInventoryEntry& right)
{
    return left.definitionId == right.definitionId
            && left.onUseScript == right.onUseScript;
}

bool EntryMatchesPickup(
        const ItemInventoryEntry& entry,
        const ItemDefinition& definition,
        std::string_view onUseScript)
{
    return entry.definitionId == definition.id
            && std::string_view{entry.onUseScript} == onUseScript;
}

int FindLowestFreeSlot(
        const PlayerInventoryState& inventory,
        int maximumSlots)
{
    for (int slotIndex = 0; slotIndex < maximumSlots; ++slotIndex) {
        if (FindItemInventoryEntryAtSlot(inventory, slotIndex) == nullptr) {
            return slotIndex;
        }
    }
    return -1;
}

} // namespace

void InitializeItemCampaignState(
        ItemCampaignState& state,
        const PlayerInventoryApplicationSettings& settings)
{
    state = ItemCampaignState{};
    state.inventory.entries.reserve(static_cast<std::size_t>(
            std::max(1, settings.maxSlots)));
    state.healingEffects.reserve(static_cast<std::size_t>(
            std::max(1, settings.maxSlots)));
    state.levels.reserve(16);
}

bool RemoveInventoryEntryQuantity(
        PlayerInventoryState& inventory,
        std::uint64_t runtimeId,
        std::uint64_t quantity,
        std::size_t* removedIndex)
{
    if (runtimeId == 0 || quantity == 0) return false;
    const auto found = std::find_if(
            inventory.entries.begin(), inventory.entries.end(),
            [runtimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == runtimeId;
            });
    if (found == inventory.entries.end() || found->quantity < quantity) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(
            found - inventory.entries.begin());
    const std::size_t affectedSlot = found->slotIndex >= 0
            ? static_cast<std::size_t>(found->slotIndex) : index;
    if (found->quantity == quantity) {
        inventory.entries.erase(found);
        if (removedIndex != nullptr) *removedIndex = affectedSlot;
    } else {
        found->quantity -= quantity;
        if (removedIndex != nullptr) *removedIndex = affectedSlot;
    }
    return true;
}

ItemHealthUseResult UseHealthInventoryEntry(
        ItemCampaignState& campaign,
        const ItemRegistry& registry,
        Health& health,
        std::uint64_t runtimeId)
{
    const auto found = std::find_if(
            campaign.inventory.entries.begin(),
            campaign.inventory.entries.end(),
            [runtimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == runtimeId;
            });
    if (found == campaign.inventory.entries.end()) {
        return ItemHealthUseResult::MissingEntry;
    }
    const ItemDefinition* definition = FindItemDefinition(
            registry, found->definitionId);
    if (definition == nullptr || definition->type != ItemType::Health
            || definition->healAmount <= 0) {
        return ItemHealthUseResult::InvalidDefinition;
    }
    if (health.current >= health.maximum) {
        return ItemHealthUseResult::DisabledAtFullHealth;
    }
    if (!definition->healOverTime) {
        ApplyHealing(health, definition->healAmount);
        RemoveInventoryEntryQuantity(
                campaign.inventory, runtimeId, 1, nullptr);
        return ItemHealthUseResult::AppliedInstantly;
    }
    if (!std::isfinite(definition->healDurationSeconds)
            || definition->healDurationSeconds <= 0.0f) {
        return ItemHealthUseResult::InvalidDefinition;
    }
    if (campaign.healingEffects.size()
            == campaign.healingEffects.capacity()) {
        WarnCapacity("healing effect", campaign.capacityWarnings);
    }
    campaign.healingEffects.push_back(ItemHealingEffect{
            definition->healAmount,
            definition->healDurationSeconds,
            0.0f,
            0});
    RemoveInventoryEntryQuantity(campaign.inventory, runtimeId, 1, nullptr);
    return ItemHealthUseResult::StartedTimedEffect;
}

ItemObjectUseResult CompleteObjectInventoryUse(
        ItemCampaignState& campaign,
        const ItemRegistry& registry,
        std::uint64_t runtimeId,
        bool permitted,
        std::size_t* removedIndex)
{
    const auto found = std::find_if(
            campaign.inventory.entries.begin(),
            campaign.inventory.entries.end(),
            [runtimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == runtimeId;
            });
    if (found == campaign.inventory.entries.end()) {
        return ItemObjectUseResult::MissingEntry;
    }
    const ItemDefinition* definition = FindItemDefinition(
            registry, found->definitionId);
    if (definition == nullptr || definition->type != ItemType::Object
            || found->quantity == 0 || found->onUseScript.empty()) {
        return ItemObjectUseResult::InvalidDefinition;
    }
    if (!permitted) return ItemObjectUseResult::Denied;
    if (!RemoveInventoryEntryQuantity(
                campaign.inventory, runtimeId, 1, removedIndex)) {
        return ItemObjectUseResult::InvalidDefinition;
    }
    return ItemObjectUseResult::Consumed;
}

void UpdateItemHealingEffects(
        ItemCampaignState& campaign,
        Health& health,
        float dt)
{
    if (!std::isfinite(dt) || dt <= 0.0f) return;
    for (ItemHealingEffect& effect : campaign.healingEffects) {
        if (effect.totalAmount <= 0
                || !std::isfinite(effect.durationSeconds)
                || effect.durationSeconds <= 0.0f) {
            effect.elapsedSeconds = effect.durationSeconds;
            effect.appliedAmount = std::max(0, effect.totalAmount);
            continue;
        }
        effect.elapsedSeconds = std::min(
                effect.durationSeconds, effect.elapsedSeconds + dt);
        const int cumulativeTarget = effect.elapsedSeconds
                        >= effect.durationSeconds
                ? effect.totalAmount
                : static_cast<int>(std::floor(
                        static_cast<double>(effect.totalAmount)
                        * static_cast<double>(effect.elapsedSeconds)
                        / static_cast<double>(effect.durationSeconds)));
        const int delta = std::max(0, cumulativeTarget - effect.appliedAmount);
        ApplyHealing(health, delta);
        // Count attempted healing, including healing discarded at maximum.
        effect.appliedAmount = cumulativeTarget;
    }
    campaign.healingEffects.erase(
            std::remove_if(
                    campaign.healingEffects.begin(),
                    campaign.healingEffects.end(),
                    [](const ItemHealingEffect& effect) {
                        return effect.appliedAmount >= effect.totalAmount;
                    }),
            campaign.healingEffects.end());
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
        std::uint64_t quantity,
        std::string_view onUseScript)
{
    ItemPickupPlan plan;
    plan.quantity = quantity;
    plan.maximumSlots = settings.maxSlots;
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
    const std::string_view retainedUse = plan.definition->type == ItemType::Object
            ? onUseScript : std::string_view{};
    plan.retainedOnUseScript = retainedUse;
    for (std::size_t index = 0; index < inventory.entries.size(); ++index) {
        const int slotIndex = inventory.entries[index].slotIndex;
        if (slotIndex < 0 || slotIndex >= settings.maxSlots) {
            plan.result = ItemPickupCapacityResult::SlotLimit;
            return plan;
        }
        for (std::size_t other = 0; other < index; ++other) {
            if (inventory.entries[other].slotIndex == slotIndex) {
                plan.result = ItemPickupCapacityResult::SlotLimit;
                return plan;
            }
        }
    }
    std::uint64_t remaining = quantity;
    for (const ItemInventoryEntry& entry : inventory.entries) {
        if (!EntryMatchesPickup(entry, *plan.definition, retainedUse)) continue;
        const std::uint64_t maximum = static_cast<std::uint64_t>(
                plan.definition->maxStackSize);
        if (entry.quantity > maximum) {
            plan.result = ItemPickupCapacityResult::NumericOverflow;
            return plan;
        }
        const std::uint64_t available = entry.quantity < maximum
                ? maximum - entry.quantity : 0u;
        remaining -= std::min(remaining, available);
        if (remaining == 0) break;
    }
    const std::uint64_t maximum = static_cast<std::uint64_t>(
            plan.definition->maxStackSize);
    const std::uint64_t requiredSlots = remaining == 0
            ? 0u : 1u + (remaining - 1u) / maximum;
    if (requiredSlots > std::numeric_limits<std::size_t>::max()) {
        plan.result = ItemPickupCapacityResult::NumericOverflow;
        return plan;
    }
    plan.addedSlots = static_cast<std::size_t>(requiredSlots);
    if (plan.addedSlots > static_cast<std::size_t>(settings.maxSlots)
            || inventory.entries.size()
                    > static_cast<std::size_t>(settings.maxSlots)
                            - plan.addedSlots) {
        plan.result = ItemPickupCapacityResult::SlotLimit;
        return plan;
    }
    if (plan.addedSlots != 0
            && (inventory.nextRuntimeId == 0
                    || static_cast<std::uint64_t>(plan.addedSlots)
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
        const ItemPickupPlan& plan)
{
    if (plan.result != ItemPickupCapacityResult::Fits
            || plan.definition == nullptr || plan.quantity == 0) {
        return false;
    }
    if (inventory.entries.size() + plan.addedSlots
            > inventory.entries.capacity()) {
        WarnCapacity("entry", inventory.capacityWarnings);
        inventory.entries.reserve(inventory.entries.size() + plan.addedSlots);
    }
    const std::string retainedUse{plan.retainedOnUseScript};
    const std::uint64_t maximum = static_cast<std::uint64_t>(
            plan.definition->maxStackSize);
    std::uint64_t remaining = plan.quantity;
    for (int slotIndex = 0;
            slotIndex < plan.maximumSlots && remaining != 0;
            ++slotIndex) {
        ItemInventoryEntry* entry = FindItemInventoryEntryAtSlot(
                inventory, slotIndex);
        if (entry == nullptr
                || !EntryMatchesPickup(
                        *entry, *plan.definition, retainedUse)) {
            continue;
        }
        const std::uint64_t available = entry->quantity < maximum
                ? maximum - entry->quantity : 0u;
        const std::uint64_t transfer = std::min(remaining, available);
        entry->quantity += transfer;
        remaining -= transfer;
    }
    while (remaining != 0) {
        const int slotIndex = FindLowestFreeSlot(
                inventory, plan.maximumSlots);
        if (slotIndex < 0 || inventory.nextRuntimeId == 0
                || inventory.nextRuntimeId
                        == std::numeric_limits<std::uint64_t>::max()) {
            return false;
        }
        ItemInventoryEntry entry;
        entry.runtimeId = inventory.nextRuntimeId++;
        entry.definitionId = plan.definition->id;
        entry.quantity = std::min(remaining, maximum);
        entry.onUseScript = retainedUse;
        entry.slotIndex = slotIndex;
        inventory.entries.push_back(std::move(entry));
        remaining -= std::min(remaining, maximum);
    }
    return true;
}

const ItemInventoryEntry* FindItemInventoryEntryAtSlot(
        const PlayerInventoryState& inventory,
        int slotIndex)
{
    const auto found = std::find_if(
            inventory.entries.begin(), inventory.entries.end(),
            [slotIndex](const ItemInventoryEntry& entry) {
                return entry.slotIndex == slotIndex;
            });
    return found == inventory.entries.end() ? nullptr : &*found;
}

ItemInventoryEntry* FindItemInventoryEntryAtSlot(
        PlayerInventoryState& inventory,
        int slotIndex)
{
    return const_cast<ItemInventoryEntry*>(FindItemInventoryEntryAtSlot(
            static_cast<const PlayerInventoryState&>(inventory), slotIndex));
}

ItemInventoryTransactionResult TransferItemInventoryEntry(
        PlayerInventoryState& inventory,
        const ItemRegistry& registry,
        std::uint64_t sourceRuntimeId,
        int targetSlotIndex,
        int maximumSlots)
{
    ItemInventoryTransactionResult result;
    if (sourceRuntimeId == 0 || targetSlotIndex < 0
            || targetSlotIndex >= maximumSlots) {
        return result;
    }
    const auto sourceIt = std::find_if(
            inventory.entries.begin(), inventory.entries.end(),
            [sourceRuntimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == sourceRuntimeId;
            });
    if (sourceIt == inventory.entries.end()
            || sourceIt->slotIndex == targetSlotIndex
            || sourceIt->quantity == 0) {
        return result;
    }
    ItemInventoryEntry* target = FindItemInventoryEntryAtSlot(
            inventory, targetSlotIndex);
    if (target == nullptr) {
        sourceIt->slotIndex = targetSlotIndex;
        return ItemInventoryTransactionResult{
                ItemInventoryTransactionType::Moved, sourceRuntimeId};
    }
    if (!EntriesCanStack(*sourceIt, *target)) {
        std::swap(sourceIt->slotIndex, target->slotIndex);
        return ItemInventoryTransactionResult{
                ItemInventoryTransactionType::Swapped, sourceRuntimeId};
    }
    const ItemDefinition* definition = FindItemDefinition(
            registry, sourceIt->definitionId);
    if (definition == nullptr || definition->maxStackSize < 1) return result;
    const std::uint64_t maximum = static_cast<std::uint64_t>(
            definition->maxStackSize);
    if (sourceIt->quantity > maximum || target->quantity > maximum) {
        return result;
    }
    const std::uint64_t available = target->quantity < maximum
            ? maximum - target->quantity : 0u;
    const std::uint64_t transfer = std::min(sourceIt->quantity, available);
    if (transfer == 0) return result;
    const std::uint64_t targetRuntimeId = target->runtimeId;
    target->quantity += transfer;
    sourceIt->quantity -= transfer;
    if (sourceIt->quantity != 0) {
        return ItemInventoryTransactionResult{
                ItemInventoryTransactionType::PartiallyMerged,
                sourceRuntimeId};
    }
    inventory.entries.erase(sourceIt);
    return ItemInventoryTransactionResult{
            ItemInventoryTransactionType::Merged, targetRuntimeId};
}

ItemInventoryTransactionResult SplitItemInventoryEntry(
        PlayerInventoryState& inventory,
        std::uint64_t sourceRuntimeId,
        std::uint64_t quantity,
        int targetSlotIndex,
        int maximumSlots)
{
    ItemInventoryTransactionResult result;
    if (sourceRuntimeId == 0 || quantity == 0
            || quantity > static_cast<std::uint64_t>(kMaximumItemStackSize)
            || targetSlotIndex < 0
            || targetSlotIndex >= maximumSlots
            || FindItemInventoryEntryAtSlot(inventory, targetSlotIndex) != nullptr
            || inventory.nextRuntimeId == 0
            || inventory.nextRuntimeId
                    == std::numeric_limits<std::uint64_t>::max()) {
        return result;
    }
    const auto sourceIt = std::find_if(
            inventory.entries.begin(), inventory.entries.end(),
            [sourceRuntimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == sourceRuntimeId;
            });
    if (sourceIt == inventory.entries.end() || quantity >= sourceIt->quantity) {
        return result;
    }
    const std::size_t sourceIndex = static_cast<std::size_t>(
            sourceIt - inventory.entries.begin());
    if (inventory.entries.size() == inventory.entries.capacity()) {
        WarnCapacity("entry", inventory.capacityWarnings);
        inventory.entries.reserve(inventory.entries.size() + 1u);
    }
    ItemInventoryEntry& source = inventory.entries[sourceIndex];
    source.quantity -= quantity;
    ItemInventoryEntry split = source;
    split.runtimeId = inventory.nextRuntimeId++;
    split.quantity = quantity;
    split.slotIndex = targetSlotIndex;
    const std::uint64_t splitRuntimeId = split.runtimeId;
    inventory.entries.push_back(std::move(split));
    return ItemInventoryTransactionResult{
            ItemInventoryTransactionType::Split, splitRuntimeId};
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
