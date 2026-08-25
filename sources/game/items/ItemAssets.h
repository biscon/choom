#pragma once

#include "engine/assets/AssetHandles.h"
#include "game/items/ItemDefinitions.h"

#include <string>
#include <string_view>
#include <vector>

namespace engine { class AssetManager; }

namespace game {

enum class ItemModelAssetStatus {
    Pending,
    Ready,
    Failed
};

struct ItemModelAssetEntry {
    std::string definitionId;
    std::string modelPath;
    engine::ModelHandle model = engine::NullModelHandle();
};

struct ItemModelAssetState {
    engine::AssetScopeHandle scope = engine::NullAssetScopeHandle();
    std::vector<ItemModelAssetEntry> entries;
    std::uint64_t sourceRegistryRevision = 0;
    std::uint64_t preparedIconRevision = 0;
};

void RebuildItemModelAssets(
        engine::AssetManager& assets,
        const ItemRegistry& registry,
        ItemModelAssetState& state);
void ShutdownItemModelAssets(
        engine::AssetManager& assets,
        ItemModelAssetState& state);
const ItemModelAssetEntry* FindItemModelAsset(
        const ItemModelAssetState& state,
        std::string_view definitionId);
ItemModelAssetStatus GetItemModelAssetStatus(
        const engine::AssetManager& assets,
        const ItemModelAssetEntry& entry);

} // namespace game
