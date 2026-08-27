#include "sector_editor/SectorEditorMainMenu.h"

#include "sector_editor/SectorEditorHelpers.h"

#include <array>

namespace game {

namespace {

constexpr uint32_t CommandId(SectorEditorMainMenuCommand command)
{
    return static_cast<uint32_t>(command);
}

} // namespace

SectorEditorMainMenuCommand DrawSectorEditorMainMenu(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        const SectorEditorState& editorState,
        engine::UIMainMenuState& menuState,
        bool gameSessionExists,
        bool canCopyConfig,
        bool canPasteConfig,
        bool visible,
        bool enabled)
{
    const std::array<engine::UIMenuItem, 5> levelItems{{
            {"New", CommandId(SectorEditorMainMenuCommand::NewLevel)},
            {"Load", CommandId(SectorEditorMainMenuCommand::LoadLevel),
                    engine::UIMenuItemKind::Action, true, false, "CTRL-O",
                    engine::UIMenuShortcut{KEY_O, true}},
            {"Save", CommandId(SectorEditorMainMenuCommand::SaveLevel),
                    engine::UIMenuItemKind::Action, true, false, "CTRL-S",
                    engine::UIMenuShortcut{KEY_S, true}},
            {"Save As...", CommandId(SectorEditorMainMenuCommand::SaveLevelAs)},
            {"Reload", CommandId(SectorEditorMainMenuCommand::ReloadLevel)}
    }};
    const std::array<engine::UIMenuItem, 5> editorItems{{
            {"Material Editor", CommandId(SectorEditorMainMenuCommand::OpenMaterialEditor)},
            {"Sound Editor", CommandId(SectorEditorMainMenuCommand::OpenSoundEditor)},
            {"NPC Editor", CommandId(SectorEditorMainMenuCommand::OpenNpcEditor)},
            {"Weapons Editor", CommandId(SectorEditorMainMenuCommand::OpenWeaponEditor)},
            {"Item Editor", CommandId(SectorEditorMainMenuCommand::OpenItemEditor)}
    }};
    const std::array<engine::UIMenuItem, 1> gameItems{{
            {"Clear", CommandId(SectorEditorMainMenuCommand::ClearGameSession),
                    engine::UIMenuItemKind::Action, gameSessionExists}
    }};
    const std::array<engine::UIMenuItem, 2> editItems{{
            {"Copy config", CommandId(SectorEditorMainMenuCommand::CopyConfig),
                    engine::UIMenuItemKind::Action, canCopyConfig, false, "CTRL-C",
                    engine::UIMenuShortcut{KEY_C, true}},
            {"Paste config", CommandId(SectorEditorMainMenuCommand::PasteConfig),
                    engine::UIMenuItemKind::Action, canPasteConfig, false, "CTRL-V",
                    engine::UIMenuShortcut{KEY_V, true}}
    }};
    const std::array<engine::UIMenuItem, 4> viewItems{{
            {"3D Mode", CommandId(SectorEditorMainMenuCommand::Toggle3DMode),
                    engine::UIMenuItemKind::Action, true, false, "CTRL-D",
                    engine::UIMenuShortcut{KEY_D, true}},
            {"Show Grid", CommandId(SectorEditorMainMenuCommand::ToggleShowGrid),
                    engine::UIMenuItemKind::Checkbox, true, editorState.showGrid},
            {"Show Axes", CommandId(SectorEditorMainMenuCommand::ToggleShowAxes),
                    engine::UIMenuItemKind::Checkbox, true, editorState.showAxes},
            {"Show ids", CommandId(SectorEditorMainMenuCommand::ToggleShowIds),
                    engine::UIMenuItemKind::Checkbox, true, editorState.showSectorIds}
    }};
    const std::array<engine::UIMenuItem, 3> settingsItems{{
            {"Level", CommandId(SectorEditorMainMenuCommand::OpenLevelSettings)},
            {"Player", CommandId(SectorEditorMainMenuCommand::OpenPlayerSettings)},
            {"Sneaking", CommandId(SectorEditorMainMenuCommand::OpenSneakSettings)}
    }};
    const std::array<engine::UIMenuRoot, 6> roots{{
            {"Level", levelItems.data(), levelItems.size()},
            {"Game", gameItems.data(), gameItems.size()},
            {"Edit", editItems.data(), editItems.size()},
            {"Editors", editorItems.data(), editorItems.size()},
            {"View", viewItems.data(), viewItems.size()},
            {"Settings", settingsItems.data(), settingsItems.size()}
    }};

    const engine::UIMainMenuResult shortcutResult =
            engine::ActivateMainMenuShortcut(
                    input,
                    roots.data(),
                    roots.size(),
                    enabled && ui.focusedId == 0);
    if (shortcutResult.activated) {
        engine::CloseMainMenu(menuState);
        return static_cast<SectorEditorMainMenuCommand>(
                shortcutResult.commandId);
    }
    if (!visible) {
        engine::CloseMainMenu(menuState);
        return SectorEditorMainMenuCommand::None;
    }

    const engine::UIMainMenuResult result = engine::MainMenu(
            ui,
            config,
            input,
            assets,
            Rectangle{0.0f, 0.0f, EditorWidth, EditorMainMenuHeight},
            font,
            smallFont,
            roots.data(),
            roots.size(),
            menuState,
            enabled);
    return result.activated
            ? static_cast<SectorEditorMainMenuCommand>(result.commandId)
            : SectorEditorMainMenuCommand::None;
}

} // namespace game
