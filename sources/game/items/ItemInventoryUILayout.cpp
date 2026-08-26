#include "game/items/ItemInventoryUI.h"

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr float SafeMargin = 48.0f;
constexpr float ModalPadding = 28.0f;
constexpr float SectionGap = 28.0f;
constexpr float HeaderHeight = 48.0f;
constexpr float SummaryHeight = 34.0f;
constexpr float ButtonHeight = 48.0f;

float Integral(float value) { return std::round(value); }

Rectangle IntegralRect(Rectangle value)
{
    return Rectangle{
            Integral(value.x), Integral(value.y),
            Integral(value.width), Integral(value.height)};
}

} // namespace

float EstimateItemDescriptionHeight(
        std::string_view text,
        float width,
        float fontSize,
        float spacing)
{
    if (text.empty() || width <= 0.0f || fontSize <= 0.0f) return 0.0f;
    const int charactersPerLine = std::max(
            1, static_cast<int>(width / std::max(1.0f, fontSize * 0.55f)));
    int lines = 1;
    int column = 0;
    for (char value : text) {
        if (value == '\n') {
            ++lines;
            column = 0;
        } else if (++column > charactersPerLine) {
            ++lines;
            column = 1;
        }
    }
    return Integral(static_cast<float>(lines) * (fontSize + spacing));
}

ItemInventoryUILayout BuildItemInventoryUILayout(
        Rectangle viewport,
        int maximumSlots,
        float measuredDescriptionHeight)
{
    ItemInventoryUILayout layout;
    maximumSlots = std::max(1, maximumSlots);
    layout.totalRows = (maximumSlots + layout.columns - 1) / layout.columns;
    const float gridWidth = layout.columns * layout.cellSize
            + (layout.columns - 1) * layout.cellGap;
    const float maximumWidth = std::max(
            1.0f, viewport.width - SafeMargin * 2.0f);
    const float desiredWidth = ModalPadding * 2.0f + gridWidth
            + SectionGap + 500.0f;
    const float modalWidth = std::min(maximumWidth, desiredWidth);
    const float detailWidth = std::max(
            1.0f,
            modalWidth - ModalPadding * 2.0f - gridWidth - SectionGap);
    const float detailContentHeight = HeaderHeight + 28.0f
            + std::max(80.0f, measuredDescriptionHeight)
            + ButtonHeight * 2.0f + 36.0f;
    const float gridHeight = layout.cellSize * 4.0f
            + layout.cellGap * 3.0f;
    const float maximumHeight = std::max(
            1.0f, viewport.height - SafeMargin * 2.0f);
    const float modalHeight = std::min(
            maximumHeight,
            ModalPadding * 2.0f + HeaderHeight + SummaryHeight + 20.0f
                    + std::max(gridHeight, detailContentHeight));
    layout.modal = IntegralRect(Rectangle{
            viewport.x + (viewport.width - modalWidth) * 0.5f,
            viewport.y + (viewport.height - modalHeight) * 0.5f,
            modalWidth,
            modalHeight});
    const float bodyY = layout.modal.y + ModalPadding + HeaderHeight
            + SummaryHeight + 20.0f;
    const float bodyHeight = layout.modal.y + layout.modal.height
            - ModalPadding - bodyY;
    layout.gridViewport = IntegralRect(Rectangle{
            layout.modal.x + ModalPadding, bodyY, gridWidth, bodyHeight});
    layout.detail = IntegralRect(Rectangle{
            layout.gridViewport.x + gridWidth + SectionGap,
            bodyY, detailWidth, bodyHeight});
    layout.weightSummary = IntegralRect(Rectangle{
            layout.modal.x + ModalPadding,
            layout.modal.y + ModalPadding + HeaderHeight,
            gridWidth * 0.62f, SummaryHeight});
    layout.slotSummary = IntegralRect(Rectangle{
            layout.weightSummary.x + layout.weightSummary.width,
            layout.weightSummary.y,
            gridWidth - layout.weightSummary.width, SummaryHeight});
    layout.visibleRows = std::max(1, static_cast<int>(std::floor(
            (layout.gridViewport.height + layout.cellGap)
            / (layout.cellSize + layout.cellGap))));
    layout.dropButton = IntegralRect(Rectangle{
            layout.detail.x,
            layout.detail.y + layout.detail.height - ButtonHeight,
            layout.detail.width, ButtonHeight});
    layout.useButton = IntegralRect(Rectangle{
            layout.detail.x,
            layout.dropButton.y - ButtonHeight - 10.0f,
            layout.detail.width, ButtonHeight});
    layout.detailScrolls = detailContentHeight > bodyHeight;
    return layout;
}

Rectangle ItemInventoryCellBounds(
        const ItemInventoryUILayout& layout,
        int slotIndex,
        int firstVisibleRow)
{
    const int row = slotIndex / layout.columns - firstVisibleRow;
    const int column = slotIndex % layout.columns;
    return IntegralRect(Rectangle{
            layout.gridViewport.x
                    + column * (layout.cellSize + layout.cellGap),
            layout.gridViewport.y + row * (layout.cellSize + layout.cellGap),
            layout.cellSize, layout.cellSize});
}

void NormalizeItemInventorySelection(
        ItemInventoryUIState& state,
        const PlayerInventoryState& inventory,
        std::size_t preferredIndex)
{
    const auto found = std::find_if(
            inventory.entries.begin(), inventory.entries.end(),
            [&state](const ItemInventoryEntry& entry) {
                return entry.runtimeId == state.selectedRuntimeId;
            });
    if (found != inventory.entries.end()) return;
    if (inventory.entries.empty()) {
        state.selectedRuntimeId = 0;
        return;
    }
    const ItemInventoryEntry* nearestAfter = nullptr;
    const ItemInventoryEntry* nearestBefore = nullptr;
    for (const ItemInventoryEntry& entry : inventory.entries) {
        if (entry.slotIndex < 0) continue;
        if (static_cast<std::size_t>(entry.slotIndex) >= preferredIndex) {
            if (nearestAfter == nullptr
                    || entry.slotIndex < nearestAfter->slotIndex) {
                nearestAfter = &entry;
            }
        } else if (nearestBefore == nullptr
                || entry.slotIndex > nearestBefore->slotIndex) {
            nearestBefore = &entry;
        }
    }
    const ItemInventoryEntry* selected = nearestAfter != nullptr
            ? nearestAfter : nearestBefore;
    if (selected == nullptr) selected = &inventory.entries.front();
    state.selectedRuntimeId = selected->runtimeId;
}

void ClearItemInventoryInteraction(ItemInventoryUIState& state)
{
    state.dragCandidateRuntimeId = 0;
    state.draggedRuntimeId = 0;
    state.dragPressPosition = {};
    state.splitDrag = false;
    state.splitModalOpen = false;
    state.splitSourceRuntimeId = 0;
    state.splitTargetSlotIndex = -1;
    state.splitQuantity = 1;
    state.splitQuantityInput = {};
}

bool CancelItemInventorySplit(ItemInventoryUIState& state)
{
    if (!state.splitModalOpen) return false;
    ClearItemInventoryInteraction(state);
    return true;
}

ItemHeldUseInputDecision EvaluateItemHeldUseInput(
        ItemHeldUsePhase phase,
        ItemHeldUseInput input)
{
    if (phase == ItemHeldUsePhase::Inactive) return {};
    if (phase == ItemHeldUsePhase::Pending) {
        return ItemHeldUseInputDecision{ItemHeldUseEffect::None, true};
    }
    switch (input) {
        case ItemHeldUseInput::ToggleInventory:
            return ItemHeldUseInputDecision{
                    ItemHeldUseEffect::ReopenInventory, true};
        case ItemHeldUseInput::Escape:
        case ItemHeldUseInput::RightClick:
            return ItemHeldUseInputDecision{
                    ItemHeldUseEffect::CancelToGameplay, true};
        case ItemHeldUseInput::InvalidLeftClick:
            return ItemHeldUseInputDecision{ItemHeldUseEffect::None, true};
        case ItemHeldUseInput::ValidLeftClick:
            return ItemHeldUseInputDecision{
                    ItemHeldUseEffect::InvokeTarget, true};
    }
    return {};
}

} // namespace game
