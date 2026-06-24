#pragma once

#include "sector_editor/SectorEditorTypes.h"
#include "sector_demo/SectorMeshPreview.h"

namespace game {

SectorMeshPreviewPose ActiveSectorEditorPreviewPose(
        const SectorEditorState& state,
        const SectorMeshPreview& preview);
void ApplySectorEditorGameplayPoseToPreview(
        const SectorEditorState& state,
        SectorMeshPreview& preview);
bool ToggleSectorEditorPreviewControlMode(
        SectorEditorState& state,
        SectorMeshPreview& preview);
bool RebuildSectorEditorCollisionWorld(SectorEditorState& state);
SectorFpsVerticalContext BuildSectorEditorGameplayVerticalContext(
        const SectorEditorState& state);
void RefreshSectorEditorGameplaySectorAndVerticalContext(SectorEditorState& state);
void InitializeSectorEditorGameplayVerticalState(SectorEditorState& state);

} // namespace game
