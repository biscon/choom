#include "game/save/GameSaveMenu.h"

#include "game/save/GameSaveStorage.h"

#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace game {
namespace {

constexpr int VisibleSlotCount = 5;

const GameSaveSlotInfo* FindSlot(
        const GameSaveMenuState& state,
        int slot)
{
    const auto found = std::find_if(
            state.slots.begin(), state.slots.end(),
            [slot](const GameSaveSlotInfo& value) { return value.slot == slot; });
    return found == state.slots.end() ? nullptr : &*found;
}

const char* SlotStatusLabel(GameSaveSlotStatus status)
{
    switch (status) {
        case GameSaveSlotStatus::Empty: return "Empty slot";
        case GameSaveSlotStatus::Ready: return "";
        case GameSaveSlotStatus::Corrupt: return "Corrupt save";
        case GameSaveSlotStatus::Incompatible: return "Incompatible save";
    }
    return "Unavailable";
}

void CopyName(GameSaveMenuState& state, const std::string& name)
{
    const std::size_t count = std::min(
            name.size(), state.nameBuffer.size() - 1);
    std::memcpy(state.nameBuffer.data(), name.data(), count);
    state.nameBuffer[count] = '\0';
}

void SelectSlot(GameSaveMenuState& state, const GameSaveSlotInfo& slot)
{
    state.selectedSlot = slot.slot;
    state.confirmationOpen = false;
    state.status.clear();
    if (slot.status == GameSaveSlotStatus::Corrupt
            || slot.status == GameSaveSlotStatus::Incompatible) {
        state.status = state.mode == GameSaveMenuMode::Save
                ? "Saving here will replace the unavailable slot."
                : slot.error;
    }
    if (state.mode == GameSaveMenuMode::Save) {
        CopyName(state, slot.status == GameSaveSlotStatus::Ready
                ? slot.name : std::string{});
    }
}

} // namespace

void RefreshGameSaveMenu(
        GameSaveMenuState& state,
        const std::filesystem::path& saveRoot,
        engine::AssetManager& assets)
{
    if (!engine::IsNull(state.thumbnailScope)) {
        assets.UnloadScope(state.thumbnailScope);
    }
    state.thumbnailScope = assets.CreateScope("game_save_thumbnails");
    state.thumbnails.fill(engine::NullTextureHandle());
    state.slots = ScanGameSaveSlots(saveRoot);
    for (const GameSaveSlotInfo& slot : state.slots) {
        if (slot.slot < 1 || slot.slot > GameSaveSlotCount
                || slot.thumbnailPath.empty()) continue;
        char key[48]{};
        std::snprintf(key, sizeof(key), "game_save_thumbnail_%02d", slot.slot);
        state.thumbnails[static_cast<std::size_t>(slot.slot - 1)] =
                assets.RequestTexture(
                        state.thumbnailScope,
                        key,
                        slot.thumbnailPath.c_str(),
                        engine::TextureColorUsage::DisplaySrgb,
                        engine::TextureLoad_BilinearFilter);
    }
}

void OpenGameSaveMenu(
        GameSaveMenuState& state,
        GameSaveMenuMode mode,
        bool loadConfirmationRequired,
        const std::filesystem::path& saveRoot,
        engine::AssetManager& assets)
{
    state.mode = mode;
    state.selectedSlot = 0;
    state.firstVisibleSlot = 0;
    state.confirmationOpen = false;
    state.loadConfirmationRequired = loadConfirmationRequired;
    state.nameBuffer.fill('\0');
    state.status.clear();
    RefreshGameSaveMenu(state, saveRoot, assets);
}

void CloseGameSaveMenu(
        GameSaveMenuState& state,
        engine::AssetManager& assets)
{
    if (!engine::IsNull(state.thumbnailScope)) {
        assets.UnloadScope(state.thumbnailScope);
    }
    state = GameSaveMenuState{};
}

GameSaveMenuAction DrawGameSaveMenu(
        GameSaveMenuState& state,
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont)
{
    GameSaveMenuAction result;
    DrawRectangleRec(config.overlayBounds, Color{0, 0, 0, 180});
    constexpr float panelWidth = 1120.0f;
    constexpr float panelHeight = 930.0f;
    constexpr float padding = 36.0f;
    constexpr float rowHeight = 116.0f;
    constexpr float rowGap = 8.0f;
    const Rectangle panel{
            config.overlayBounds.x
                    + (config.overlayBounds.width - panelWidth) * 0.5f,
            config.overlayBounds.y
                    + (config.overlayBounds.height - panelHeight) * 0.5f,
            panelWidth,
            panelHeight};
    DrawRectangleRounded(panel, config.cornerRadius, config.cornerSegments,
            config.panelColor);
    DrawRectangleRoundedLinesEx(panel, config.cornerRadius,
            config.cornerSegments, config.borderThickness, config.borderColor);

    engine::BeginUI(ui, input);
    engine::Text(config, assets,
            {panel.x + padding, panel.y + 18.0f,
                    panel.width - padding * 2.0f, 50.0f},
            font,
            state.mode == GameSaveMenuMode::Save ? "Save Game" : "Load Game",
            engine::UITextJustify::Center);

    const bool modal = state.confirmationOpen;
    float y = panel.y + 76.0f;
    for (int offset = 0; offset < VisibleSlotCount; ++offset) {
        const int index = state.firstVisibleSlot + offset;
        if (index < 0 || index >= static_cast<int>(state.slots.size())) continue;
        const GameSaveSlotInfo& slot = state.slots[static_cast<std::size_t>(index)];
        char id[48]{};
        std::snprintf(id, sizeof(id), "game_save_slot_%02d", slot.slot);
        const Rectangle row{panel.x + padding, y,
                panel.width - padding * 2.0f, rowHeight};
        if (engine::Button(ui, config, input, assets, id, row, smallFont, "",
                    engine::UITextJustify::Left, !modal)) {
            SelectSlot(state, slot);
        }
        if (slot.slot == state.selectedSlot) {
            DrawRectangleLinesEx(row, 3.0f, config.accentColor);
        }

        const Rectangle thumbnailBounds{row.x + 10.0f, row.y + 10.0f,
                160.0f, 90.0f};
        DrawRectangleRec(thumbnailBounds, Color{10, 12, 17, 255});
        const engine::TextureHandle thumbnail =
                state.thumbnails[static_cast<std::size_t>(slot.slot - 1)];
        if (const Texture2D* texture = assets.GetTexture(thumbnail)) {
            DrawTexturePro(*texture,
                    {0.0f, 0.0f, static_cast<float>(texture->width),
                            static_cast<float>(texture->height)},
                    thumbnailBounds, {}, 0.0f, WHITE);
        }

        char slotNumber[24]{};
        std::snprintf(slotNumber, sizeof(slotNumber), "Slot %02d", slot.slot);
        engine::Text(config, assets,
                {row.x + 188.0f, row.y + 8.0f, 160.0f, 38.0f},
                smallFont, slotNumber, engine::UITextJustify::Left,
                config.mutedTextColor);
        const char* title = slot.status == GameSaveSlotStatus::Ready
                ? slot.name.c_str() : SlotStatusLabel(slot.status);
        engine::Text(config, assets,
                {row.x + 188.0f, row.y + 42.0f, 490.0f, 42.0f},
                smallFont, title, engine::UITextJustify::Left);
        if (slot.status == GameSaveSlotStatus::Ready) {
            engine::Text(config, assets,
                    {row.x + 700.0f, row.y + 20.0f, 330.0f, 35.0f},
                    smallFont, slot.displayTimestamp.c_str(),
                    engine::UITextJustify::Right,
                    config.mutedTextColor);
            engine::Text(config, assets,
                    {row.x + 700.0f, row.y + 60.0f, 330.0f, 35.0f},
                    smallFont, slot.currentLevelId.c_str(),
                    engine::UITextJustify::Right,
                    config.mutedTextColor);
        }
        y += rowHeight + rowGap;
    }

    constexpr float smallButtonWidth = 130.0f;
    const bool canPrevious = state.firstVisibleSlot > 0;
    const bool canNext = state.firstVisibleSlot + VisibleSlotCount
            < static_cast<int>(state.slots.size());
    if (engine::Button(ui, config, input, assets, "game_save_previous",
                {panel.x + padding, y, smallButtonWidth, 44.0f},
                smallFont, "Previous", engine::UITextJustify::Center,
                canPrevious && !modal)) {
        state.firstVisibleSlot = std::max(
                0, state.firstVisibleSlot - VisibleSlotCount);
    }
    if (engine::Button(ui, config, input, assets, "game_save_next",
                {panel.x + padding + smallButtonWidth + 10.0f, y,
                        smallButtonWidth, 44.0f},
                smallFont, "Next", engine::UITextJustify::Center,
                canNext && !modal)) {
        state.firstVisibleSlot = std::min(
                (GameSaveSlotCount - 1) / VisibleSlotCount
                        * VisibleSlotCount,
                state.firstVisibleSlot + VisibleSlotCount);
    }
    char page[32]{};
    std::snprintf(page, sizeof(page), "%d / %d",
            state.firstVisibleSlot / VisibleSlotCount + 1,
            (GameSaveSlotCount + VisibleSlotCount - 1) / VisibleSlotCount);
    engine::Text(config, assets,
            {panel.x + padding + 280.0f, y, 120.0f, 44.0f},
            smallFont, page, engine::UITextJustify::Center,
            config.mutedTextColor);

    const GameSaveSlotInfo* selected = FindSlot(state, state.selectedSlot);
    const bool selectedReady = selected != nullptr
            && selected->status == GameSaveSlotStatus::Ready;
    const bool selectedOccupied = selected != nullptr
            && selected->status != GameSaveSlotStatus::Empty;
    float actionY = y + 58.0f;
    if (state.mode == GameSaveMenuMode::Save) {
        engine::Text(config, assets,
                {panel.x + padding, actionY, 160.0f, 48.0f},
                smallFont, "Save name", engine::UITextJustify::Left);
        engine::TextInput(ui, config, input, assets, "game_save_name",
                {panel.x + padding + 170.0f, actionY,
                        panel.width - padding * 2.0f - 430.0f, 48.0f},
                smallFont, state.nameBuffer.data(), state.nameBuffer.size(),
                1, GameSaveMaximumNameCharacters);
    }
    const float mainActionX = panel.x + panel.width - padding - 240.0f;
    const bool validSave = selected != nullptr
            && IsValidGameSaveName(state.nameBuffer.data());
    const bool canAct = state.mode == GameSaveMenuMode::Save
            ? validSave : selectedReady;
    if (engine::Button(ui, config, input, assets, "game_save_primary_action",
                {mainActionX, actionY, 240.0f, 48.0f}, smallFont,
                state.mode == GameSaveMenuMode::Save ? "Save" : "Load",
                engine::UITextJustify::Center, canAct && !modal)) {
        if ((state.mode == GameSaveMenuMode::Save && selectedOccupied)
                || (state.mode == GameSaveMenuMode::Load
                        && state.loadConfirmationRequired)) {
            state.confirmationOpen = true;
        } else {
            result.type = state.mode == GameSaveMenuMode::Save
                    ? GameSaveMenuActionType::Save
                    : GameSaveMenuActionType::Load;
            result.slot = state.selectedSlot;
            result.name = state.nameBuffer.data();
        }
    }
    if (engine::Button(ui, config, input, assets, "game_save_back",
                {panel.x + padding, panel.y + panel.height - padding - 48.0f,
                        170.0f, 48.0f},
                smallFont, "Back", engine::UITextJustify::Center, !modal)) {
        result.type = GameSaveMenuActionType::Back;
    }
    if (!state.status.empty()) {
        engine::Text(config, assets,
                {panel.x + 220.0f, panel.y + panel.height - padding - 48.0f,
                        panel.width - 480.0f, 48.0f},
                smallFont, state.status.c_str(), engine::UITextJustify::Center,
                config.invalidColor, true);
    }

    if (modal) {
        const Rectangle confirmation{panel.x + 250.0f, panel.y + 320.0f,
                panel.width - 500.0f, 260.0f};
        DrawRectangleRounded(confirmation, config.cornerRadius,
                config.cornerSegments, Color{20, 24, 32, 250});
        DrawRectangleRoundedLinesEx(confirmation, config.cornerRadius,
                config.cornerSegments, config.borderThickness,
                config.accentColor);
        const char* prompt = state.mode == GameSaveMenuMode::Save
                ? "Overwrite this save slot?" : "Load this save game?";
        engine::Text(config, assets,
                {confirmation.x + 30.0f, confirmation.y + 38.0f,
                        confirmation.width - 60.0f, 60.0f},
                font, prompt, engine::UITextJustify::Center);
        if (state.mode == GameSaveMenuMode::Load) {
            engine::Text(config, assets,
                    {confirmation.x + 30.0f, confirmation.y + 100.0f,
                            confirmation.width - 60.0f, 40.0f},
                    smallFont, "Unsaved progress will be lost.",
                    engine::UITextJustify::Center,
                    config.mutedTextColor);
        }
        if (engine::Button(ui, config, input, assets,
                    "game_save_confirmation_cancel",
                    {confirmation.x + 70.0f, confirmation.y + 170.0f,
                            210.0f, 52.0f},
                    smallFont, "Cancel")) {
            state.confirmationOpen = false;
        }
        if (engine::Button(ui, config, input, assets,
                    "game_save_confirmation_accept",
                    {confirmation.x + confirmation.width - 280.0f,
                            confirmation.y + 170.0f, 210.0f, 52.0f},
                    smallFont,
                    state.mode == GameSaveMenuMode::Save ? "Overwrite" : "Load")) {
            result.type = state.mode == GameSaveMenuMode::Save
                    ? GameSaveMenuActionType::Save
                    : GameSaveMenuActionType::Load;
            result.slot = state.selectedSlot;
            result.name = state.nameBuffer.data();
            state.confirmationOpen = false;
        }
    }

    engine::EndUI(ui, config, input, assets);
    return result;
}

} // namespace game
