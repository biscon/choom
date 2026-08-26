#pragma once

#include "engine/ui/UI.h"
#include "game/Health.h"
#include "game/items/ItemAssets.h"
#include "game/items/ItemInventory.h"

#include <cstdint>

namespace game {

enum class ItemInventoryUIActionType {
    None,
    UseHealth,
    UseObject,
    Drop
};

enum class ItemHeldUsePhase {
    Inactive,
    Targeting,
    Pending
};

enum class ItemHeldUseInput {
    ToggleInventory,
    Escape,
    RightClick,
    InvalidLeftClick,
    ValidLeftClick
};

enum class ItemHeldUseEffect {
    None,
    CancelToGameplay,
    ReopenInventory,
    InvokeTarget
};

struct ItemHeldUseInputDecision {
    ItemHeldUseEffect effect = ItemHeldUseEffect::None;
    bool consumeEvent = false;
};

struct ItemInventoryUIAction {
    ItemInventoryUIActionType type = ItemInventoryUIActionType::None;
    std::uint64_t runtimeId = 0;
};

struct ItemInventoryUIState {
    bool open = false;
    std::uint64_t selectedRuntimeId = 0;
    int firstVisibleRow = 0;
    float detailScroll = 0.0f;
};

struct ItemInventoryUILayout {
    Rectangle modal = {};
    Rectangle gridViewport = {};
    Rectangle detail = {};
    Rectangle weightSummary = {};
    Rectangle slotSummary = {};
    Rectangle useButton = {};
    Rectangle dropButton = {};
    int columns = 6;
    int visibleRows = 0;
    int totalRows = 0;
    float cellSize = 136.0f;
    float cellGap = 8.0f;
    bool detailScrolls = false;
};

ItemInventoryUILayout BuildItemInventoryUILayout(
        Rectangle viewport,
        int maximumSlots,
        float measuredDescriptionHeight);
Rectangle ItemInventoryCellBounds(
        const ItemInventoryUILayout& layout,
        int slotIndex,
        int firstVisibleRow);
float EstimateItemDescriptionHeight(
        std::string_view text,
        float width,
        float fontSize,
        float spacing);
void NormalizeItemInventorySelection(
        ItemInventoryUIState& state,
        const PlayerInventoryState& inventory,
        std::size_t preferredIndex = 0);
ItemHeldUseInputDecision EvaluateItemHeldUseInput(
        ItemHeldUsePhase phase,
        ItemHeldUseInput input);

ItemInventoryUIAction DrawItemInventoryUI(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        const ItemRegistry& registry,
        const ItemModelAssetState& itemAssets,
        const ItemCampaignState& campaign,
        const PlayerInventoryApplicationSettings& settings,
        const Health& health,
        ItemInventoryUIState& state);

void DrawHeldItemCursor(
        engine::AssetManager& assets,
        const ItemModelAssetState& itemAssets,
        const PlayerInventoryState& inventory,
        std::uint64_t runtimeId,
        Vector2 cursorPosition);

} // namespace game
