#pragma once

#include "game/FpsWeaponRegistry.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace game {

inline constexpr int kItemRegistryFormatVersion = 1;
inline constexpr const char* kItemRegistryAssetPath = "assets/config/items.json";
inline constexpr size_t kMaximumItemIdBytes = 63;
inline constexpr size_t kMaximumItemTitleCodepoints = 96;
inline constexpr size_t kMaximumItemDescriptionCodepoints = 2048;

enum class ItemType {
    Object,
    Weapon,
    Ammo,
    Health
};

struct ItemDefinition {
    std::string id;
    std::string title;
    std::string description;
    std::string modelPath;
    ItemType type = ItemType::Object;
    float weightKg = 0.0f;
    std::string weaponId;
    int healAmount = 0;
    bool healOverTime = false;
    float healDurationSeconds = 0.0f;
};

struct ItemRegistry {
    int version = kItemRegistryFormatVersion;
    std::vector<ItemDefinition> items;
    // Runtime/editor-only. It is intentionally omitted from JSON.
    std::uint64_t revision = 1;
};

const char* ItemTypeName(ItemType type);
bool ParseItemType(std::string_view text, ItemType& outType);
bool IsValidItemDefinitionId(std::string_view id);
bool IsValidItemModelPath(std::string_view path);
ItemDefinition MakeDefaultItemDefinition();
bool ValidateItemDefinition(
        const ItemDefinition& definition,
        const FpsWeaponRegistry& weapons,
        std::string& error);
bool ValidateItemRegistry(
        const ItemRegistry& registry,
        const FpsWeaponRegistry& weapons,
        std::string& error);
const ItemDefinition* FindItemDefinition(
        const ItemRegistry& registry,
        std::string_view id);
std::vector<std::string> SortedItemDefinitionIds(const ItemRegistry& registry);

bool ParseItemRegistryJson(
        std::string_view jsonText,
        const FpsWeaponRegistry& weapons,
        ItemRegistry& outRegistry,
        std::string& error);
bool SerializeItemRegistryJson(
        const ItemRegistry& registry,
        const FpsWeaponRegistry& weapons,
        std::string& outJson,
        std::string& error);
bool LoadItemRegistry(
        const std::filesystem::path& path,
        const FpsWeaponRegistry& weapons,
        ItemRegistry& outRegistry,
        std::string& error);
bool SaveItemRegistry(
        const std::filesystem::path& path,
        const ItemRegistry& registry,
        const FpsWeaponRegistry& weapons,
        std::string& error);

} // namespace game
