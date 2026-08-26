#pragma once

#include "engine/assets/AssetHandles.h"
#include "game/items/ItemDefinitions.h"
#include "game/items/ItemIconLayout.h"

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

enum class ItemIconPreparationState {
    WaitingForModels,
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
    engine::TextureHandle iconAtlas = engine::NullTextureHandle();
    ItemIconAtlasLayout iconLayout;
    ItemIconPreparationState iconPreparation =
            ItemIconPreparationState::WaitingForModels;
    std::string iconDiagnostic;
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
inline ItemIconPreparationState ClassifyItemIconPreparation(
        std::uint64_t sourceRevision,
        std::uint64_t preparedRevision,
        std::size_t pendingModelCount,
        bool failed)
{
    (void)pendingModelCount;
    if (failed) return ItemIconPreparationState::Failed;
    return sourceRevision != 0 && preparedRevision == sourceRevision
            ? ItemIconPreparationState::Ready
            : ItemIconPreparationState::WaitingForModels;
}
bool UpdateItemIconPreparation(
        engine::AssetManager& assets,
        const ItemRegistry& registry,
        ItemModelAssetState& state);
inline bool IsItemIconPreparationTerminal(const ItemModelAssetState& state)
{
    return state.iconPreparation == ItemIconPreparationState::Ready
            || state.iconPreparation == ItemIconPreparationState::Failed;
}

} // namespace game
