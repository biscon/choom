#pragma once

#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "game/PlayerOxygen.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorViewPose.h"

#include <vector>

namespace engine { class World; }

namespace game {

class SectorMeshRenderer;
struct SectorTopologyMap;
struct NpcCollisionCylinder;

SectorViewPose ActiveSectorEditorPreviewPose(
        const SectorEditorPreviewControllerState& controllerState,
        const SectorMeshRenderer& preview);
void ApplySectorEditorGameplayPoseToPreview(
        const SectorEditorPreviewControllerState& controllerState,
        SectorMeshRenderer& preview);
bool ToggleSectorEditorPreviewControlMode(
        bool preview3DActive,
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorStaticModelCollider>& staticModelColliders,
        SectorMeshRenderer& preview);
bool RebuildSectorEditorCollisionWorld(
        const SectorTopologyMap& topologyMap,
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorStaticModelCollider>& staticModelColliders);
SectorFpsVerticalContext BuildSectorEditorGameplayVerticalContext(
        const SectorEditorPreviewCollisionState& collisionState,
        const SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorStaticModelCollider>& staticModelColliders);
void RefreshSectorEditorGameplaySectorAndVerticalContext(
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState);
void InitializeSectorEditorGameplayVerticalState(
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const std::vector<SectorStaticModelCollider>& staticModelColliders);
void UpdateSectorEditorGameplayPreview(
        engine::World& world,
        const std::vector<SectorDynamicDoorCollider>& dynamicDoorColliders,
        const std::vector<SectorStaticModelCollider>& staticModelColliders,
        const std::vector<SectorStaticModelCollider>& ductAccessGateColliders,
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        const SectorTopologyMap* topologyMap,
        bool previewSettingsModalOpen,
        const SectorFpsControllerInput& controllerInput,
        const PlayerLiquidApplicationSettings& liquidSettings,
        const PlayerDuctTraversalApplicationSettings& ductSettings,
        float previousVisualEyeY,
        float dt,
        const std::vector<NpcCollisionCylinder>* npcCollisionCylinders = nullptr);

} // namespace game
