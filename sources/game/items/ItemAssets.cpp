#include "game/items/ItemAssets.h"

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorAssetPaths.h"

#include <algorithm>

namespace game {

void RebuildItemModelAssets(
        engine::AssetManager& assets,
        const ItemRegistry& registry,
        ItemModelAssetState& state)
{
    if (!engine::IsNull(state.scope)) assets.UnloadScope(state.scope);
    state = ItemModelAssetState{};
    state.scope = assets.CreateScope("global_item_models");
    state.sourceRegistryRevision = registry.revision;
    state.entries.reserve(registry.items.size());
    std::vector<const ItemDefinition*> sorted;
    sorted.reserve(registry.items.size());
    for (const ItemDefinition& definition : registry.items) sorted.push_back(&definition);
    std::sort(sorted.begin(), sorted.end(),
            [](const ItemDefinition* left, const ItemDefinition* right) {
                return left->id < right->id;
            });
    for (const ItemDefinition* definition : sorted) {
        ItemModelAssetEntry entry;
        entry.definitionId = definition->id;
        entry.modelPath = definition->modelPath;
        const std::string resolved = ResolveSectorAssetPath(definition->modelPath);
        const std::string key = "global_item_model_" + definition->id;
        entry.model = assets.RequestModel(
                state.scope, key.c_str(), resolved.c_str(),
                engine::ModelLoad_None);
        state.entries.push_back(std::move(entry));
    }
}

void ShutdownItemModelAssets(
        engine::AssetManager& assets,
        ItemModelAssetState& state)
{
    if (!engine::IsNull(state.scope)) assets.UnloadScope(state.scope);
    state = ItemModelAssetState{};
}

const ItemModelAssetEntry* FindItemModelAsset(
        const ItemModelAssetState& state,
        std::string_view definitionId)
{
    const auto found = std::find_if(
            state.entries.begin(), state.entries.end(),
            [definitionId](const ItemModelAssetEntry& entry) {
                return entry.definitionId == definitionId;
            });
    return found == state.entries.end() ? nullptr : &*found;
}

ItemModelAssetStatus GetItemModelAssetStatus(
        const engine::AssetManager& assets,
        const ItemModelAssetEntry& entry)
{
    if (assets.IsReady(entry.model)) return ItemModelAssetStatus::Ready;
    if (assets.HasFailed(entry.model)) return ItemModelAssetStatus::Failed;
    return ItemModelAssetStatus::Pending;
}

} // namespace game
