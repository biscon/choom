#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/ecs/World.h"
#include "engine/input/Input.h"
#include "game/FpsViewmodel.h"
#include "game/Health.h"
#include "game/FpsViewmodelEffectsRenderer.h"
#include "game/FpsWeaponRegistry.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"

#include <raylib.h>

#include <limits>
#include <string>
#include <vector>

namespace game {

struct FpsPlayerRuntimeTuning {
    const FpsViewmodelPresentation* presentation = nullptr;
    const FpsViewmodelHolsterTransition* holsterTransition = nullptr;
    const FpsWeaponFiringDefinition* firing = nullptr;
    const FpsViewmodelGripCorrection* gripCorrection = nullptr;
    const FpsViewmodelAttachmentLighting* attachmentLighting = nullptr;
};

class FpsPlayerRuntime {
public:
    void Begin(
            engine::AssetManager& assets,
            SectorMeshRenderer& renderer,
            const FpsWeaponRegistry& registry,
            const FpsApplicationSettings& settings,
            const char* scopeName);
    bool SelectWeapon(
            engine::AssetManager& assets,
            SectorMeshRenderer& renderer,
            const FpsWeaponRegistry& registry,
            const FpsApplicationSettings& settings,
            std::string_view weaponId,
            const char* scopeName = "fps_viewmodel");
    void End(engine::AssetManager& assets, SectorMeshRenderer& renderer);

    void Update(
            engine::AssetManager& assets,
            SectorMeshRenderer& renderer,
            const FpsWeaponRegistry& registry,
            const FpsApplicationSettings& settings,
            float dt,
            const FpsPlayerRuntimeTuning* tuning = nullptr);
    bool HandleInput(
            engine::Input& input,
            const FpsWeaponRegistry& registry,
            engine::AssetManager& assets,
            engine::AudioSystem& audio,
            const SectorCollisionWorld* collisionWorld,
            const SectorMeshRenderer& renderer,
            bool gameplayActive,
            bool mouseLookActive,
            bool uiCaptured);
    bool HandleWeaponSlotInput(
            engine::Input& input,
            const FpsWeaponRegistry& registry,
            bool gameplayActive,
            bool uiCaptured);
    bool HandleFireInput(
            engine::Input& input,
            engine::AssetManager& assets,
            engine::AudioSystem& audio,
            const SectorCollisionWorld* collisionWorld,
            const SectorMeshRenderer& renderer,
            bool gameplayActive,
            bool mouseLookActive,
            bool uiCaptured);
    bool TriggerPreviewShot(
            engine::AssetManager& assets,
            engine::AudioSystem& audio,
            const SectorMeshRenderer& renderer);
    void UpdateTransformsAndLight(
            SectorMeshRenderer& renderer,
            const SectorCollisionWorld* collisionWorld);
    void RecordShotResolution(const FpsShotResult& shot)
    {
        state.firing.lastShot = shot;
        state.firing.lastShot.accepted = true;
        state.firing.hasLastShot = true;
    }
    void Render(
            engine::AssetManager& assets,
            SectorMeshRenderer& renderer,
            const SectorTopologyMap& map,
            const SectorRuntimeObjectState& runtimeObjects,
            int preferredSectorId);
    void RenderHud(
            Rectangle playableViewport,
            const FpsWeaponRegistry& registry,
            const engine::FontAsset* font = nullptr,
            const Health* health = nullptr,
            const PlayerStamina* stamina = nullptr) const;

    FpsViewmodelRuntimeState& State() { return state; }
    const FpsViewmodelRuntimeState& State() const { return state; }
    bool IsWeaponSwitchInProgress() const { return pendingWeaponSlot != 0; }

private:
    struct WeaponAssetEntry {
        std::string weaponId;
        std::string modelPath;
        std::string attachmentModelPath;
        std::string resolvedModelPath;
        std::string resolvedAttachmentModelPath;
        engine::ModelHandle model;
        engine::ModelHandle attachmentModel;
        engine::AnimatedModelInstance modelInstance;
    };

    static constexpr size_t InvalidWeaponAssetIndex =
            std::numeric_limits<size_t>::max();

    size_t FindWeaponAssetEntry(const FpsWeaponDefinition& definition) const;
    size_t EnsureWeaponAssetEntry(
            engine::AssetManager& assets,
            const FpsWeaponDefinition& definition);
    void PrepareInactiveWeaponInstances(engine::AssetManager& assets);
    bool ActivateWeapon(
            SectorMeshRenderer& renderer,
            const FpsWeaponRegistry& registry,
            const FpsApplicationSettings& settings,
            std::string_view weaponId,
            bool allowAssetRequest,
            engine::AssetManager* assets = nullptr);
    void ResetActiveWeapon(SectorMeshRenderer& renderer);
    bool LoadWeapon(
            engine::AssetManager& assets,
            SectorMeshRenderer& renderer,
            const FpsWeaponRegistry& registry,
            const FpsApplicationSettings& settings,
            std::string_view weaponId,
            const char* scopeName);

    FpsViewmodelRuntimeState state;
    engine::AssetScopeHandle weaponAssetScope = engine::NullAssetScopeHandle();
    std::vector<WeaponAssetEntry> weaponAssets;
    size_t activeWeaponAssetIndex = InvalidWeaponAssetIndex;
    int pendingWeaponSlot = 0;
    FpsMuzzleFlashRenderResources muzzleFlashRenderResources;
    std::string cameraRecoilWeaponId;
};

} // namespace game
