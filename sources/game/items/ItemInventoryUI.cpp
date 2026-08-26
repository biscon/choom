#include "game/items/ItemInventoryUI.h"

#include "engine/input/InputEvents.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {
namespace {

constexpr float ModalPadding = 28.0f;
constexpr float HeaderHeight = 48.0f;

float Integral(float value) { return std::round(value); }

Rectangle IntegralRect(Rectangle value)
{
    return Rectangle{
            Integral(value.x), Integral(value.y),
            Integral(value.width), Integral(value.height)};
}

bool Contains(Rectangle bounds, Vector2 point)
{
    return point.x >= bounds.x && point.y >= bounds.y
            && point.x < bounds.x + bounds.width
            && point.y < bounds.y + bounds.height;
}

const ItemInventoryEntry* SelectedEntry(
        const ItemCampaignState& campaign,
        const ItemInventoryUIState& state)
{
    const auto found = std::find_if(
            campaign.inventory.entries.begin(),
            campaign.inventory.entries.end(),
            [&state](const ItemInventoryEntry& entry) {
                return entry.runtimeId == state.selectedRuntimeId;
            });
    return found == campaign.inventory.entries.end() ? nullptr : &*found;
}

} // namespace

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
        ItemInventoryUIState& state)
{
    ItemInventoryUIAction action;
    if (!state.open) return action;
    NormalizeItemInventorySelection(state, campaign.inventory);
    const ItemInventoryEntry* selected = SelectedEntry(campaign, state);
    const ItemDefinition* definition = selected != nullptr
            ? FindItemDefinition(registry, selected->definitionId) : nullptr;
    const ItemInventoryUILayout preliminary = BuildItemInventoryUILayout(
            config.overlayBounds, settings.maxSlots, 0.0f);
    const float descriptionHeight = definition != nullptr
            ? EstimateItemDescriptionHeight(
                    definition->description,
                    preliminary.detail.width - config.paddingX * 2.0f,
                    config.fontSize,
                    config.textSpacing)
            : 0.0f;
    const ItemInventoryUILayout layout = BuildItemInventoryUILayout(
            config.overlayBounds, settings.maxSlots, descriptionHeight);

    DrawRectangleRec(config.overlayBounds, Color{0, 0, 0, 150});
    DrawRectangleRec(layout.modal, Color{20, 24, 32, 255});
    DrawRectangleLinesEx(layout.modal, config.borderThickness, config.borderColor);
    engine::BeginUI(ui, input);
    engine::Text(
            config, assets,
            IntegralRect(Rectangle{layout.modal.x + ModalPadding,
                    layout.modal.y + ModalPadding,
                    layout.modal.width - ModalPadding * 2.0f,
                    HeaderHeight}),
            font, "Inventory", engine::UITextJustify::Center);
    char weight[96] = {};
    std::snprintf(weight, sizeof(weight), "Weight: %.2f / %.2f kg",
            ComputeInventoryWeightKg(campaign.inventory, registry),
            settings.maxCarryWeightKg);
    char slots[64] = {};
    std::snprintf(slots, sizeof(slots), "Slots: %zu / %d",
            campaign.inventory.entries.size(), settings.maxSlots);
    engine::Text(config, assets, layout.weightSummary, smallFont, weight,
            engine::UITextJustify::Left);
    engine::Text(config, assets, layout.slotSummary, smallFont, slots,
            engine::UITextJustify::Right);

    if (Contains(layout.gridViewport, input.MousePosition())) {
        input.ForEachEvent(
                engine::InputEventType::MouseWheel,
                true,
                [&state, &layout](engine::InputEvent& event) {
                    state.firstVisibleRow += event.wheel.value < 0.0f ? 1 : -1;
                    state.firstVisibleRow = std::clamp(
                            state.firstVisibleRow,
                            0,
                            std::max(0, layout.totalRows - layout.visibleRows));
                    engine::ConsumeEvent(event);
                });
    }
    state.firstVisibleRow = std::clamp(
            state.firstVisibleRow,
            0,
            std::max(0, layout.totalRows - layout.visibleRows));

    BeginScissorMode(
            static_cast<int>(layout.gridViewport.x),
            static_cast<int>(layout.gridViewport.y),
            static_cast<int>(layout.gridViewport.width),
            static_cast<int>(layout.gridViewport.height));
    const Texture2D* atlas = assets.GetTexture(itemAssets.iconAtlas);
    for (int slotIndex = 0; slotIndex < settings.maxSlots; ++slotIndex) {
        const Rectangle cell = ItemInventoryCellBounds(
                layout, slotIndex, state.firstVisibleRow);
        if (cell.y + cell.height <= layout.gridViewport.y
                || cell.y >= layout.gridViewport.y + layout.gridViewport.height) {
            continue;
        }
        const bool occupied = slotIndex
                < static_cast<int>(campaign.inventory.entries.size());
        const ItemInventoryEntry* entry = occupied
                ? &campaign.inventory.entries[static_cast<std::size_t>(slotIndex)]
                : nullptr;
        char id[64] = {};
        std::snprintf(id, sizeof(id), "inventory_slot_%d", slotIndex);
        if (occupied && engine::Button(
                    ui, config, input, assets, id, cell, smallFont, "")) {
            state.selectedRuntimeId = entry->runtimeId;
            state.detailScroll = 0.0f;
            selected = entry;
            definition = FindItemDefinition(registry, entry->definitionId);
        } else if (!occupied) {
            DrawRectangleRec(cell, Color{29, 35, 46, 255});
            DrawRectangleLinesEx(cell, 1.0f, config.borderColor);
        }
        if (!occupied) continue;
        const Rectangle iconDestination = IntegralRect(Rectangle{
                cell.x + (cell.width - 128.0f) * 0.5f,
                cell.y + (cell.height - 128.0f) * 0.5f,
                128.0f,
                128.0f});
        const ItemIconRegion* region = FindItemIconRegion(
                itemAssets.iconLayout, entry->definitionId);
        if (atlas != nullptr && region != nullptr) {
            DrawTexturePro(*atlas, region->source, iconDestination,
                    Vector2{}, 0.0f, WHITE);
        } else {
            DrawRectangleRec(iconDestination, Color{46, 24, 50, 255});
            DrawRectangleLinesEx(iconDestination, 3.0f,
                    Color{220, 70, 210, 255});
        }
        if (entry->quantity > 1) {
            char quantity[32] = {};
            std::snprintf(quantity, sizeof(quantity), "%llu",
                    static_cast<unsigned long long>(entry->quantity));
            engine::Text(config, assets,
                    IntegralRect(Rectangle{cell.x + 4.0f,
                            cell.y + cell.height - 34.0f,
                            cell.width - 8.0f, 30.0f}),
                    smallFont, quantity, engine::UITextJustify::Right);
        }
        if (entry->runtimeId == state.selectedRuntimeId) {
            DrawRectangleLinesEx(cell, 4.0f, config.accentColor);
        }
    }
    EndScissorMode();

    if (definition != nullptr && selected != nullptr) {
        engine::Text(config, assets,
                IntegralRect(Rectangle{layout.detail.x, layout.detail.y,
                        layout.detail.width, 50.0f}),
                font, definition->title.c_str(), engine::UITextJustify::Left);
        const float descriptionTop = layout.detail.y + 62.0f;
        const float descriptionBottom = layout.useButton.y - 12.0f;
        const float descriptionViewportHeight = std::max(
                0.0f, descriptionBottom - descriptionTop);
        if (layout.detailScrolls
                && Contains(Rectangle{layout.detail.x, descriptionTop,
                            layout.detail.width, descriptionViewportHeight},
                        input.MousePosition())) {
            input.ForEachEvent(
                    engine::InputEventType::MouseWheel,
                    true,
                    [&state](engine::InputEvent& event) {
                        state.detailScroll += event.wheel.value < 0.0f
                                ? 48.0f : -48.0f;
                        engine::ConsumeEvent(event);
                    });
        }
        state.detailScroll = std::clamp(
                state.detailScroll,
                0.0f,
                std::max(0.0f, descriptionHeight - descriptionViewportHeight));
        BeginScissorMode(
                static_cast<int>(layout.detail.x),
                static_cast<int>(descriptionTop),
                static_cast<int>(layout.detail.width),
                static_cast<int>(descriptionViewportHeight));
        engine::Text(config, assets,
                IntegralRect(Rectangle{layout.detail.x,
                        descriptionTop - state.detailScroll,
                        layout.detail.width,
                        std::max(descriptionViewportHeight,
                                descriptionHeight + config.paddingY * 2.0f)}),
                smallFont, definition->description.c_str(),
                engine::UITextJustify::Left, config.textColor, true);
        EndScissorMode();
        if (definition->type == ItemType::Health) {
            const bool enabled = health.current < health.maximum;
            if (enabled && engine::Button(
                        ui, config, input, assets,
                        "inventory_use_health", layout.useButton,
                        smallFont, "Use")) {
                action = ItemInventoryUIAction{
                        ItemInventoryUIActionType::UseHealth,
                        selected->runtimeId};
            } else if (!enabled) {
                DrawRectangleRec(layout.useButton, config.disabledColor);
                engine::Text(config, assets, layout.useButton, smallFont,
                        "Use", engine::UITextJustify::Center,
                        config.mutedTextColor);
            }
        } else if (definition->type == ItemType::Object
                && !selected->onUseScript.empty()
                && engine::Button(
                        ui, config, input, assets,
                        "inventory_use_object", layout.useButton,
                        smallFont, "Use")) {
            action = ItemInventoryUIAction{
                    ItemInventoryUIActionType::UseObject,
                    selected->runtimeId};
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "inventory_drop", layout.dropButton,
                    smallFont, "Drop")) {
            action = ItemInventoryUIAction{
                    ItemInventoryUIActionType::Drop,
                    selected->runtimeId};
        }
    }
    engine::EndUI(ui, config, input, assets);
    return action;
}

void DrawHeldItemCursor(
        engine::AssetManager& assets,
        const ItemModelAssetState& itemAssets,
        const PlayerInventoryState& inventory,
        std::uint64_t runtimeId,
        Vector2 cursorPosition)
{
    const auto found = std::find_if(
            inventory.entries.begin(), inventory.entries.end(),
            [runtimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == runtimeId;
            });
    if (found == inventory.entries.end()) return;
    constexpr float Size = 64.0f;
    const Rectangle destination = IntegralRect(Rectangle{
            cursorPosition.x - Size * 0.5f,
            cursorPosition.y - Size * 0.5f,
            Size,
            Size});
    DrawRectangleRec(destination, Color{20, 24, 32, 235});
    const Texture2D* atlas = assets.GetTexture(itemAssets.iconAtlas);
    const ItemIconRegion* region = FindItemIconRegion(
            itemAssets.iconLayout, found->definitionId);
    if (atlas != nullptr && region != nullptr) {
        DrawTexturePro(
                *atlas, region->source, destination,
                Vector2{}, 0.0f, WHITE);
    } else {
        DrawRectangleRec(destination, Color{46, 24, 50, 235});
    }
    DrawRectangleLinesEx(destination, 2.0f, Color{220, 70, 210, 255});
}

} // namespace game
