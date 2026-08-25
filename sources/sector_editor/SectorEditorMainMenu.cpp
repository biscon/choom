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
        const SectorEditorState& editorState,
        engine::UIMainMenuState& menuState,
        bool enabled)
{
    const std::array<engine::UIMenuItem, 4> levelItems{{
            {"New", CommandId(SectorEditorMainMenuCommand::NewLevel)},
            {"Load", CommandId(SectorEditorMainMenuCommand::LoadLevel)},
            {"Save", CommandId(SectorEditorMainMenuCommand::SaveLevel)},
            {"Reload", CommandId(SectorEditorMainMenuCommand::ReloadLevel)}
    }};
    const std::array<engine::UIMenuItem, 5> editorItems{{
            {"3D Mode", CommandId(SectorEditorMainMenuCommand::Toggle3DMode)},
            {"Material Editor", CommandId(SectorEditorMainMenuCommand::OpenMaterialEditor)},
            {"Sound Editor", CommandId(SectorEditorMainMenuCommand::OpenSoundEditor)},
            {"NPC Editor", CommandId(SectorEditorMainMenuCommand::OpenNpcEditor)},
            {"Weapons Editor", CommandId(SectorEditorMainMenuCommand::OpenWeaponEditor)}
    }};
    const std::array<engine::UIMenuItem, 3> viewItems{{
            {"Show Grid", CommandId(SectorEditorMainMenuCommand::ToggleShowGrid),
                    engine::UIMenuItemKind::Checkbox, true, editorState.showGrid},
            {"Show Axes", CommandId(SectorEditorMainMenuCommand::ToggleShowAxes),
                    engine::UIMenuItemKind::Checkbox, true, editorState.showAxes},
            {"Show ids", CommandId(SectorEditorMainMenuCommand::ToggleShowIds),
                    engine::UIMenuItemKind::Checkbox, true, editorState.showSectorIds}
    }};
    const std::array<engine::UIMenuItem, 1> settingsItems{{
            {"Level", CommandId(SectorEditorMainMenuCommand::OpenLevelSettings)}
    }};
    const std::array<engine::UIMenuRoot, 4> roots{{
            {"Level", levelItems.data(), levelItems.size()},
            {"Editors", editorItems.data(), editorItems.size()},
            {"View", viewItems.data(), viewItems.size()},
            {"Settings", settingsItems.data(), settingsItems.size()}
    }};

    const engine::UIMainMenuResult result = engine::MainMenu(
            ui,
            config,
            input,
            assets,
            Rectangle{0.0f, 0.0f, EditorWidth, EditorMainMenuHeight},
            font,
            roots.data(),
            roots.size(),
            menuState,
            enabled);
    return result.activated
            ? static_cast<SectorEditorMainMenuCommand>(result.commandId)
            : SectorEditorMainMenuCommand::None;
}

} // namespace game
