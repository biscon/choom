#pragma once

#include "game/FpsWeaponRegistry.h"
#include "game/Health.h"
#include "game/items/ItemDefinitions.h"
#include "sector_demo/SectorTopologyMap.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace game {

struct ItemInventoryEntry {
    std::uint64_t runtimeId = 0;
    std::string definitionId;
    std::uint64_t quantity = 1;
    std::string onUseScript;
    int slotIndex = -1;
};

struct PlayerInventoryState {
    std::vector<ItemInventoryEntry> entries;
    std::uint64_t nextRuntimeId = 1;
    std::uint64_t capacityWarnings = 0;
};

struct ItemHealingEffect {
    int totalAmount = 0;
    float durationSeconds = 0.0f;
    float elapsedSeconds = 0.0f;
    int appliedAmount = 0;
};

struct ItemLevelCampaignState {
    std::string levelId;
    std::vector<int> collectedAuthoredItemIds;
    std::vector<SectorPlacedRuntimeObject> droppedItems;
    int nextDroppedObjectId = 1;
    std::uint64_t capacityWarnings = 0;
};

struct ItemCampaignState {
    PlayerInventoryState inventory;
    std::vector<ItemHealingEffect> healingEffects;
    std::vector<ItemLevelCampaignState> levels;
    std::uint64_t capacityWarnings = 0;
};

enum class ItemHealthUseResult {
    AppliedInstantly,
    StartedTimedEffect,
    DisabledAtFullHealth,
    MissingEntry,
    InvalidDefinition
};

enum class ItemObjectUseResult {
    Consumed,
    Denied,
    MissingEntry,
    InvalidDefinition
};

enum class ItemPickupCapacityResult {
    Fits,
    MissingDefinition,
    InvalidQuantity,
    WeightLimit,
    SlotLimit,
    NumericOverflow
};

struct ItemPickupPlan {
    ItemPickupCapacityResult result = ItemPickupCapacityResult::MissingDefinition;
    const ItemDefinition* definition = nullptr;
    std::uint64_t quantity = 0;
    std::string_view retainedOnUseScript;
    int maximumSlots = 0;
    std::size_t addedSlots = 0;
    double resultingWeightKg = 0.0;
};

enum class ItemInventoryTransactionType {
    Rejected,
    Moved,
    Swapped,
    Merged,
    PartiallyMerged,
    Split
};

struct ItemInventoryTransactionResult {
    ItemInventoryTransactionType type =
            ItemInventoryTransactionType::Rejected;
    std::uint64_t selectedRuntimeId = 0;
};

void InitializeItemCampaignState(
        ItemCampaignState& state,
        const PlayerInventoryApplicationSettings& settings);

double ComputeInventoryWeightKg(
        const PlayerInventoryState& inventory,
        const ItemRegistry& registry,
        bool* valid = nullptr);

ItemPickupPlan PreflightItemPickup(
        const PlayerInventoryState& inventory,
        const ItemRegistry& registry,
        const PlayerInventoryApplicationSettings& settings,
        std::string_view definitionId,
        std::uint64_t quantity,
        std::string_view onUseScript = {});

bool CommitItemPickup(
        PlayerInventoryState& inventory,
        const ItemPickupPlan& plan);

const ItemInventoryEntry* FindItemInventoryEntryAtSlot(
        const PlayerInventoryState& inventory,
        int slotIndex);
ItemInventoryEntry* FindItemInventoryEntryAtSlot(
        PlayerInventoryState& inventory,
        int slotIndex);

ItemInventoryTransactionResult TransferItemInventoryEntry(
        PlayerInventoryState& inventory,
        const ItemRegistry& registry,
        std::uint64_t sourceRuntimeId,
        int targetSlotIndex,
        int maximumSlots);

ItemInventoryTransactionResult SplitItemInventoryEntry(
        PlayerInventoryState& inventory,
        std::uint64_t sourceRuntimeId,
        std::uint64_t quantity,
        int targetSlotIndex,
        int maximumSlots);

ItemHealthUseResult UseHealthInventoryEntry(
        ItemCampaignState& campaign,
        const ItemRegistry& registry,
        Health& health,
        std::uint64_t runtimeId);

ItemObjectUseResult CompleteObjectInventoryUse(
        ItemCampaignState& campaign,
        const ItemRegistry& registry,
        std::uint64_t runtimeId,
        bool permitted,
        std::size_t* removedIndex = nullptr);

void UpdateItemHealingEffects(
        ItemCampaignState& campaign,
        Health& health,
        float dt);

bool RemoveInventoryEntryQuantity(
        PlayerInventoryState& inventory,
        std::uint64_t runtimeId,
        std::uint64_t quantity,
        std::size_t* removedIndex = nullptr);

ItemLevelCampaignState& FindOrCreateItemLevelCampaignState(
        ItemCampaignState& campaign,
        std::string_view levelId,
        std::size_t authoredItemCapacity = 0);

bool IsAuthoredItemCollected(
        const ItemLevelCampaignState& level,
        int placedObjectId);

bool RecordAuthoredItemCollected(
        ItemLevelCampaignState& level,
        int placedObjectId);

bool RemoveSessionDroppedItem(
        ItemLevelCampaignState& level,
        int placedObjectId);

void ReconcileItemCampaignLevel(
        ItemCampaignState& campaign,
        std::string_view levelId,
        SectorTopologyMap& map);

} // namespace game
