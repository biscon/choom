#pragma once

#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorViewPose.h"

#include <vector>

namespace game {

class SectorMeshRenderer;
struct SectorTopologyMap;

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
        const std::vector<SectorDynamicDoorCollider>& dynamicDoorColliders,
        const std::vector<SectorStaticModelCollider>& staticModelColliders,
        SectorEditorPreviewCollisionState& collisionState,
        SectorEditorPreviewControllerState& controllerState,
        bool previewSettingsModalOpen,
        const SectorFpsControllerInput& controllerInput,
        float previousVisualEyeY,
        float dt);

} // namespace game
