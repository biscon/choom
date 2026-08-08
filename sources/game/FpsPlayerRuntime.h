#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/ecs/World.h"
#include "engine/input/Input.h"
#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"

#include <raylib.h>

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
    void End(engine::AssetManager& assets, SectorMeshRenderer& renderer);

    void Update(
            engine::AssetManager& assets,
            const FpsWeaponRegistry& registry,
            const FpsApplicationSettings& settings,
            float dt,
            const FpsPlayerRuntimeTuning* tuning = nullptr);
    void HandleInput(
            engine::Input& input,
            engine::AssetManager& assets,
            engine::AudioSystem& audio,
            const SectorCollisionWorld* collisionWorld,
            const SectorMeshRenderer& renderer,
            bool gameplayActive,
            bool mouseLookActive,
            bool uiCaptured);
    void HandleFireInput(
            engine::Input& input,
            engine::AssetManager& assets,
            engine::AudioSystem& audio,
            const SectorCollisionWorld* collisionWorld,
            const SectorMeshRenderer& renderer,
            bool gameplayActive,
            bool mouseLookActive,
            bool uiCaptured);
    void UpdateTransformsAndLight(
            SectorMeshRenderer& renderer,
            const SectorCollisionWorld* collisionWorld);
    void Render(
            engine::AssetManager& assets,
            SectorMeshRenderer& renderer,
            const SectorTopologyMap& map,
            const SectorRuntimeObjectState& runtimeObjects,
            int preferredSectorId);
    void RenderHud(
            Rectangle playableViewport,
            const FpsWeaponRegistry& registry) const;

    FpsViewmodelRuntimeState& State() { return state; }
    const FpsViewmodelRuntimeState& State() const { return state; }

private:
    FpsViewmodelRuntimeState state;
};

} // namespace game
