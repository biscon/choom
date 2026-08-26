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

const ItemInventoryEntry* EntryByRuntimeId(
        const PlayerInventoryState& inventory,
        std::uint64_t runtimeId)
{
    const auto found = std::find_if(
            inventory.entries.begin(), inventory.entries.end(),
            [runtimeId](const ItemInventoryEntry& entry) {
                return entry.runtimeId == runtimeId;
            });
    return found == inventory.entries.end() ? nullptr : &*found;
}

int VisibleSlotAtPosition(
        const ItemInventoryUILayout& layout,
        int maximumSlots,
        int firstVisibleRow,
        Vector2 position)
{
    if (!Contains(layout.gridViewport, position)) return -1;
    for (int slotIndex = 0; slotIndex < maximumSlots; ++slotIndex) {
        const Rectangle cell = ItemInventoryCellBounds(
                layout, slotIndex, firstVisibleRow);
        if (Contains(cell, position)) return slotIndex;
    }
    return -1;
}

void DrawDraggedInventoryEntry(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle smallFont,
        const ItemModelAssetState& itemAssets,
        const ItemInventoryEntry& entry,
        Vector2 position)
{
    constexpr float Size = 88.0f;
    const Rectangle destination = IntegralRect(Rectangle{
            position.x - Size * 0.5f,
            position.y - Size * 0.5f,
            Size,
            Size});
    DrawRectangleRec(destination, Color{20, 24, 32, 235});
    const Texture2D* atlas = assets.GetTexture(itemAssets.iconAtlas);
    const ItemIconRegion* region = FindItemIconRegion(
            itemAssets.iconLayout, entry.definitionId);
    if (atlas != nullptr && region != nullptr) {
        DrawTexturePro(*atlas, region->source, destination,
                Vector2{}, 0.0f, WHITE);
    } else {
        DrawRectangleRec(destination, Color{46, 24, 50, 235});
    }
    DrawRectangleLinesEx(destination, 3.0f, config.accentColor);
    if (entry.quantity > 1) {
        char quantity[32] = {};
        std::snprintf(quantity, sizeof(quantity), "%llu",
                static_cast<unsigned long long>(entry.quantity));
        engine::Text(config, assets,
                IntegralRect(Rectangle{destination.x + 4.0f,
                        destination.y + destination.height - 30.0f,
                        destination.width - 8.0f, 26.0f}),
                smallFont, quantity, engine::UITextJustify::Right);
    }
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

    if (!state.splitModalOpen
            && Contains(layout.gridViewport, input.MousePosition())) {
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

    if (state.splitModalOpen) {
        const ItemInventoryEntry* splitSource = EntryByRuntimeId(
                campaign.inventory, state.splitSourceRuntimeId);
        if (splitSource == nullptr || splitSource->quantity < 2
                || state.splitTargetSlotIndex < 0
                || FindItemInventoryEntryAtSlot(
                        campaign.inventory, state.splitTargetSlotIndex)
                        != nullptr) {
            ClearItemInventoryInteraction(state);
        }
    }

    if (!state.splitModalOpen) {
        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [&state, &campaign, &registry, &layout, &settings, &input,
                        &selected, &definition](engine::InputEvent& event) {
                    if (event.mouseButton.button != MOUSE_LEFT_BUTTON) return;
                    const int slotIndex = VisibleSlotAtPosition(
                            layout, settings.maxSlots, state.firstVisibleRow,
                            event.mouseButton.position);
                    if (slotIndex < 0) return;
                    const ItemInventoryEntry* entry =
                            FindItemInventoryEntryAtSlot(
                                    campaign.inventory, slotIndex);
                    if (entry != nullptr) {
                        state.selectedRuntimeId = entry->runtimeId;
                        state.detailScroll = 0.0f;
                        state.dragCandidateRuntimeId = entry->runtimeId;
                        state.draggedRuntimeId = 0;
                        state.dragPressPosition = event.mouseButton.position;
                        state.splitDrag = input.IsKeyDown(KEY_LEFT_SHIFT)
                                || input.IsKeyDown(KEY_RIGHT_SHIFT);
                        selected = entry;
                        definition = FindItemDefinition(
                                registry, entry->definitionId);
                    }
                    engine::ConsumeEvent(event);
                });

        if (state.dragCandidateRuntimeId != 0
                && input.IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            const Vector2 mouse = input.MousePosition();
            const float dx = mouse.x - state.dragPressPosition.x;
            const float dy = mouse.y - state.dragPressPosition.y;
            if (dx * dx + dy * dy >= 16.0f) {
                state.draggedRuntimeId = state.dragCandidateRuntimeId;
            }
        }

        input.ForEachEvent(
                engine::InputEventType::MouseButtonReleased,
                true,
                [&state, &campaign, &layout, &settings,
                        &action](engine::InputEvent& event) {
                    if (event.mouseButton.button != MOUSE_LEFT_BUTTON
                            || state.dragCandidateRuntimeId == 0) {
                        return;
                    }
                    const std::uint64_t sourceRuntimeId =
                            state.dragCandidateRuntimeId;
                    const ItemInventoryEntry* source = EntryByRuntimeId(
                            campaign.inventory, sourceRuntimeId);
                    const int targetSlotIndex = VisibleSlotAtPosition(
                            layout, settings.maxSlots, state.firstVisibleRow,
                            event.mouseButton.position);
                    if (state.draggedRuntimeId != 0 && source != nullptr
                            && targetSlotIndex >= 0) {
                        if (state.splitDrag
                                && source->quantity >= 2
                                && FindItemInventoryEntryAtSlot(
                                        campaign.inventory, targetSlotIndex)
                                        == nullptr) {
                            state.splitModalOpen = true;
                            state.splitSourceRuntimeId = sourceRuntimeId;
                            state.splitTargetSlotIndex = targetSlotIndex;
                            state.splitQuantity = static_cast<int>(
                                    std::min<std::uint64_t>(
                                            source->quantity / 2u,
                                            static_cast<std::uint64_t>(
                                                    kMaximumItemStackSize)));
                            state.splitQuantityInput = {};
                        } else if (!state.splitDrag) {
                            action.type = ItemInventoryUIActionType::Transfer;
                            action.runtimeId = sourceRuntimeId;
                            action.targetSlotIndex = targetSlotIndex;
                        }
                    }
                    state.dragCandidateRuntimeId = 0;
                    state.draggedRuntimeId = 0;
                    state.dragPressPosition = {};
                    state.splitDrag = false;
                    engine::ConsumeEvent(event);
                });
        input.ForEachEvent(
                engine::InputEventType::MouseClick,
                true,
                [&layout](engine::InputEvent& event) {
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON
                            && Contains(layout.gridViewport,
                                    event.mouseClick.releasePosition)) {
                        engine::ConsumeEvent(event);
                    }
                });
        if (state.dragCandidateRuntimeId != 0
                && !input.IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            state.dragCandidateRuntimeId = 0;
            state.draggedRuntimeId = 0;
            state.dragPressPosition = {};
            state.splitDrag = false;
        }
    }

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
        const ItemInventoryEntry* entry = FindItemInventoryEntryAtSlot(
                campaign.inventory, slotIndex);
        const bool occupied = entry != nullptr;
        const bool hovered = Contains(cell, input.MousePosition());
        DrawRectangleRec(cell, occupied
                ? (hovered && !state.splitModalOpen
                        ? config.widgetHoverColor : config.widgetColor)
                : Color{29, 35, 46, 255});
        DrawRectangleLinesEx(cell, 1.0f, config.borderColor);
        if (state.draggedRuntimeId != 0 && hovered) {
            DrawRectangleLinesEx(cell, 5.0f, Color{235, 190, 70, 255});
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
        if (!state.splitModalOpen && layout.detailScrolls
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
            if (enabled && !state.splitModalOpen && engine::Button(
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
                && !state.splitModalOpen
                && engine::Button(
                        ui, config, input, assets,
                        "inventory_use_object", layout.useButton,
                        smallFont, "Use")) {
            action = ItemInventoryUIAction{
                    ItemInventoryUIActionType::UseObject,
                    selected->runtimeId};
        }
        if (!state.splitModalOpen && engine::Button(
                    ui, config, input, assets,
                    "inventory_drop", layout.dropButton,
                    smallFont, "Drop")) {
            action = ItemInventoryUIAction{
                    ItemInventoryUIActionType::Drop,
                    selected->runtimeId};
        }
    }

    if (state.splitModalOpen) {
        const ItemInventoryEntry* source = EntryByRuntimeId(
                campaign.inventory, state.splitSourceRuntimeId);
        if (source != nullptr && source->quantity >= 2) {
            const int maximumQuantity = static_cast<int>(
                    std::min<std::uint64_t>(
                            source->quantity - 1u,
                            static_cast<std::uint64_t>(
                                    kMaximumItemStackSize)));
            state.splitQuantity = std::clamp(
                    state.splitQuantity, 1, maximumQuantity);
            const Rectangle splitModal = IntegralRect(Rectangle{
                    layout.modal.x + (layout.modal.width - 520.0f) * 0.5f,
                    layout.modal.y + (layout.modal.height - 260.0f) * 0.5f,
                    520.0f,
                    260.0f});
            DrawRectangleRec(config.overlayBounds, Color{0, 0, 0, 135});
            DrawRectangleRec(splitModal, Color{24, 28, 38, 255});
            DrawRectangleLinesEx(
                    splitModal, config.borderThickness, config.borderColor);
            engine::Text(config, assets,
                    IntegralRect(Rectangle{splitModal.x + 24.0f,
                            splitModal.y + 20.0f,
                            splitModal.width - 48.0f, 44.0f}),
                    font, "Split stack", engine::UITextJustify::Center);
            char prompt[96] = {};
            std::snprintf(prompt, sizeof(prompt),
                    "Move how many? (1-%d)", maximumQuantity);
            engine::Text(config, assets,
                    IntegralRect(Rectangle{splitModal.x + 28.0f,
                            splitModal.y + 75.0f,
                            220.0f, 42.0f}),
                    smallFont, prompt, engine::UITextJustify::Left);
            engine::IntInput(
                    ui, config, input, assets, "inventory_split_quantity",
                    IntegralRect(Rectangle{splitModal.x + 270.0f,
                            splitModal.y + 75.0f,
                            splitModal.width - 298.0f, 42.0f}),
                    smallFont, state.splitQuantity,
                    state.splitQuantityInput,
                    1, maximumQuantity, 1);
            if (engine::Button(
                        ui, config, input, assets,
                        "inventory_split_confirm",
                        IntegralRect(Rectangle{splitModal.x + 208.0f,
                                splitModal.y + splitModal.height - 66.0f,
                                132.0f, 42.0f}),
                        smallFont, "Split")) {
                action.type = ItemInventoryUIActionType::Split;
                action.runtimeId = state.splitSourceRuntimeId;
                action.quantity = static_cast<std::uint64_t>(
                        state.splitQuantity);
                action.targetSlotIndex = state.splitTargetSlotIndex;
                ClearItemInventoryInteraction(state);
            }
            if (engine::Button(
                        ui, config, input, assets,
                        "inventory_split_cancel",
                        IntegralRect(Rectangle{splitModal.x + 360.0f,
                                splitModal.y + splitModal.height - 66.0f,
                                132.0f, 42.0f}),
                        smallFont, "Cancel")) {
                ClearItemInventoryInteraction(state);
            }
        }
    }
    engine::EndUI(ui, config, input, assets);
    if (state.draggedRuntimeId != 0) {
        const ItemInventoryEntry* dragged = EntryByRuntimeId(
                campaign.inventory, state.draggedRuntimeId);
        if (dragged != nullptr) {
            DrawDraggedInventoryEntry(
                    config, assets, smallFont, itemAssets,
                    *dragged, input.MousePosition());
        }
    }
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
