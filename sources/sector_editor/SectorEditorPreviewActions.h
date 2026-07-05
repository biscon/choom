#pragma once

#include "sector_editor/SectorEditorTypes.h"
#include "sector_demo/SectorViewPose.h"

namespace game {

class SectorMeshRenderer;

SectorViewPose ActiveSectorEditorPreviewPose(
        const SectorEditorState& state,
        const SectorMeshRenderer& preview);
void ApplySectorEditorGameplayPoseToPreview(
        const SectorEditorState& state,
        SectorMeshRenderer& preview);
bool ToggleSectorEditorPreviewControlMode(
        SectorEditorState& state,
        SectorMeshRenderer& preview);
bool RebuildSectorEditorCollisionWorld(SectorEditorState& state);
SectorFpsVerticalContext BuildSectorEditorGameplayVerticalContext(
        const SectorEditorState& state);
void RefreshSectorEditorGameplaySectorAndVerticalContext(SectorEditorState& state);
void InitializeSectorEditorGameplayVerticalState(SectorEditorState& state);
void UpdateSectorEditorGameplayPreview(
        SectorEditorState& state,
        const SectorFpsControllerInput& controllerInput,
        float previousVisualEyeY,
        float dt);

} // namespace game
