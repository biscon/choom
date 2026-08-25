#include "game/FpsWeaponRegistry.h"
#include "engine/assets/AssetManager.h"
#include "sector_editor/weapons/SectorEditorWeaponEditorService.h"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>

namespace {

game::FpsWeaponRegistry MakeRegistry()
{
    game::FpsWeaponRegistry registry;
    game::FpsWeaponDefinition pistol = game::MakeDefaultFpsWeaponDefinition();
    pistol.id = "pistol";
    pistol.weaponSlot = 1;
    pistol.viewmodel.modelPath = "assets/models/weapons/shared_arms.glb";
    pistol.viewmodel.idleAnimation = "Idle";
    pistol.viewmodel.attachment.modelPath = "assets/models/weapons/pistol.glb";
    pistol.viewmodel.attachment.boneName = "RightHand";
    registry.weapons.push_back(std::move(pistol));
    return registry;
}

void AddDuplicateDeleteAndCancel()
{
    game::SectorEditorWeaponEditorState state;
    game::SectorEditorWeaponEditorSessionState session;
    game::FpsWeaponRegistry registry = MakeRegistry();
    game::ItemRegistry itemRegistry;
    game::FpsApplicationSettings settings;
    game::FpsViewmodelPresentationOverride legacyOverride;
    legacyOverride.position = Vector3{0.2f, -1.4f, 0.1f};
    game::SetFpsViewmodelOverride(settings, "pistol", legacyOverride);
    std::string status;
    game::SectorEditorWeaponEditorService service(
            state,
            session,
            registry,
            itemRegistry,
            settings,
            status,
            "unused_weapons.json",
            "unused_application_settings.json");

    assert(service.Open("pistol", true));
    assert(!service.ConsumePreviewReloadRequest());
    assert(service.SelectedWeaponId() == "pistol");
    assert(state.listLabelStorage.front().find("slot 1")
            != std::string::npos);
    assert(service.SelectedWeapon()->viewmodel.presentation.position.x == 0.2f);

    service.DuplicateSelected();
    assert(state.draftRegistry.weapons.size() == 2);
    assert(service.SelectedWeaponId() == "pistol_copy");
    assert(service.SelectedWeapon()->viewmodel.modelPath
            == "assets/models/weapons/shared_arms.glb");
    assert(service.SelectedWeapon()->weaponSlot == 0);
    assert(!service.SetSelectedWeaponSlot(1));
    assert(service.SelectedWeapon()->weaponSlot == 0);
    assert(state.warningMessage.find("pistol") != std::string::npos);
    service.SelectedWeapon()->viewmodel.attachment.gripCorrection.translation.x = 0.4f;
    assert(state.draftRegistry.weapons.front()
                    .viewmodel.attachment.gripCorrection.translation.x
            != service.SelectedWeapon()
                    ->viewmodel.attachment.gripCorrection.translation.x);

    assert(service.SelectIndex(0));
    assert(service.SetSelectedWeaponSlot(0));
    assert(service.SelectIndex(1));
    assert(service.SetSelectedWeaponSlot(1));
    assert(state.draftRegistry.weapons[0].weaponSlot == 0);
    assert(state.draftRegistry.weapons[1].weaponSlot == 1);

    assert(service.SelectIndex(0));
    service.RequestDeleteSelected();
    assert(state.deleteConfirmationOpen);
    service.ConfirmDeleteSelected();
    assert(state.draftRegistry.weapons.size() == 1);
    assert(service.SetSelectedWeaponSlot(1));
    assert(state.draftRegistry.weapons.front().weaponSlot == 1);
    assert(state.listLabelStorage.front().find("slot 1")
            != std::string::npos);

    service.RequestDeleteSelected();
    assert(!state.deleteConfirmationOpen);
    assert(!state.validationMessage.empty());

    service.AddDefault();
    service.AddDefault();
    assert(state.draftRegistry.weapons.size() == 3);
    assert(state.draftRegistry.weapons[1].id == "new_weapon");
    assert(state.draftRegistry.weapons[2].id == "new_weapon_2");

    service.Cancel();
    assert(!state.open);
    assert(registry.weapons.size() == 1);
    assert(registry.weapons.front().weaponSlot == 1);
}

void ReferencedWeaponsCannotBeOrphaned()
{
    game::SectorEditorWeaponEditorState state;
    game::SectorEditorWeaponEditorSessionState session;
    game::FpsWeaponRegistry registry = MakeRegistry();
    game::FpsWeaponDefinition rifle = registry.weapons.front();
    rifle.id = "rifle";
    rifle.weaponSlot = 2;
    registry.weapons.push_back(std::move(rifle));
    game::ItemRegistry items;
    game::ItemDefinition ammo;
    ammo.id = "pistol_ammo";
    ammo.title = "Pistol Ammo";
    ammo.description = "Ammunition for the pistol.";
    ammo.modelPath = "assets/models/box.glb";
    ammo.type = game::ItemType::Ammo;
    ammo.weightKg = 0.02f;
    ammo.weaponId = "pistol";
    items.items.push_back(std::move(ammo));
    game::FpsApplicationSettings settings;
    std::string status;
    const std::filesystem::path root = std::filesystem::temp_directory_path()
            / "weapon_item_reference_test";
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    std::filesystem::create_directories(root);
    game::SectorEditorWeaponEditorService service(
            state,
            session,
            registry,
            items,
            settings,
            status,
            root / "weapons.json",
            root / "application_settings.json");
    assert(service.Open("pistol", false));
    service.RequestDeleteSelected();
    assert(!state.deleteConfirmationOpen);
    assert(state.validationMessage.find("pistol_ammo") != std::string::npos);

    std::strncpy(state.idBuffer, "renamed_pistol", sizeof(state.idBuffer) - 1);
    service.ApplyIdBuffer();
    assert(service.SelectedWeaponId() == "pistol");
    assert(std::string(state.idBuffer) == "pistol");

    service.SelectedWeapon()->id = "direct_rename";
    engine::AssetManager assets;
    assert(assets.Initialize());
    assert(!service.SaveAndClose(assets));
    assert(state.validationMessage.find("orphan") != std::string::npos);
    assets.Shutdown();
    service.Cancel();
    std::filesystem::remove_all(root, ignored);
}

} // namespace

int main()
{
    AddDuplicateDeleteAndCancel();
    ReferencedWeaponsCannotBeOrphaned();
    std::cout << "Weapon editor service tests passed\n";
}
