#pragma once

#include "engine/EngineContext.h"
#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"
#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"
#include "sector_editor/services/fog_volumes/SectorEditorAuthoringFogVolumeEditingService.h"
#include "sector_editor/services/fog_volumes/SectorEditorFogVolumeEditingState.h"
#include "sector_editor/services/reflection_probes/SectorEditorReflectionProbeEditingService.h"
#include "sector_editor/services/footsteps/SectorEditorFootstepService.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingService.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingState.h"
#include "sector_editor/services/sound_emitters/SectorEditorSoundEmitterEditingService.h"
#include "sector_editor/services/sound_emitters/SectorEditorSoundEmitterEditingState.h"
#include "sector_editor/services/triggers/SectorEditorTriggerEditingService.h"
#include "sector_editor/services/authoring_faces/SectorEditorAuthoringFaceMergeService.h"
#include "sector_editor/services/structural_primitives/SectorEditorStructuralPrimitiveEditingService.h"

#include <array>
#include <string>

#include <raylib.h>

namespace game {

class SectorEditorTextureCatalogService;
class SectorEditorSoundService;

enum class SectorEditorInspectorPanelRequestKind {
    RebuildSectorCollisionWorld,
    RefreshStructuralPreviewRuntime,
    RefreshStructuralPreviewMaterials,
    BeginAuthoringInsertVertex,
    DeleteSelectedAuthoringVertex,
    DeleteSelectedRuntimeObject,
    OpenDeleteSelectedLightConfirmation,
    ConvertSelectedLight,
    OpenDeleteSelectedFogVolumeConfirmation,
    OpenDeleteSelectedReflectionProbeConfirmation,
    OpenDeleteSelectedLevelMarkerConfirmation,
    OpenDeleteSelectedSoundEmitterConfirmation,
    OpenDeleteSelectedTriggerConfirmation,
    BakeLightmaps,
    RefreshPreviewLightSources
};

struct SectorEditorInspectorPanelRequest {
    SectorEditorInspectorPanelRequestKind kind = SectorEditorInspectorPanelRequestKind::BakeLightmaps;
    std::string status;
    int lineId = -1;
};

struct SectorEditorInspectorPanelResult {
    std::array<SectorEditorInspectorPanelRequest, 8> requests{};
    int requestCount = 0;
};

struct SectorEditorInspectorPanelContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;
    Rectangle panelRect;

    SectorEditorState& state;
    SectorEditorDocumentLifecycleAccess lifecycle;
    SectorTopologyMap& topologyMap;
    SectorAuthoringGraph& authoringGraph;
    SectorEditorDerivationDocumentAccess derivation;
    SelectionState& selectionState;
    SectorEditorUiState& uiState;
    RuntimeObjectEditingState& runtimeObjectEditingState;
    RuntimeObjectEditingUiState& runtimeObjectEditingUiState;
    SectorRuntimeObjectState& runtimeObjects;
    InspectorIdUiState& inspectorIdUiState;
    MaterialEditingUiState& materialUiState;
    FogVolumeEditingUiState& fogVolumeUiState;
    ReflectionProbeEditingUiState& reflectionProbeUiState;
    LevelMarkerEditingUiState& levelMarkerUiState;
    SoundEmitterEditingUiState& soundEmitterUiState;
    TriggerEditingUiState& triggerUiState;
    SectorEditorStructuralPrimitiveEditingUiState& structuralPrimitiveUiState;
    std::string& statusText;

    SectorEditorSelectionServiceContext& selection;
    SectorEditorRuntimeObjectEditingService& runtimeObjectEditing;
    SectorEditorStaticModelPickerService& staticModelPicker;
    SectorEditorMaterialEditingService& materialEditing;
    SectorEditorFootstepService& footsteps;
    SectorEditorTextureCatalogService& textureCatalog;
    SectorEditorSoundService& sounds;
    SectorEditorLightEditingService& lightEditing;
    SectorEditorAuthoringFogVolumeEditingService& fogVolumeEditing;
    SectorEditorReflectionProbeEditingService& reflectionProbeEditing;
    SectorEditorLevelMarkerEditingService& levelMarkerEditing;
    SectorEditorSoundEmitterEditingService& soundEmitterEditing;
    SectorEditorTriggerEditingService& triggerEditing;
    SectorEditorAuthoringFaceMergeService& authoringFaceMerge;
    SectorEditorStructuralPrimitiveEditingService& structuralPrimitiveEditing;
    engine::EngineContext* engineContext = nullptr;
};

inline float MeasureSectorEditorAuthoringFaceInspectorContentHeight(
        const SectorAuthoringFaceAnchor& anchor,
        float rowHeight,
        float gap,
        float anchorSummaryHeight)
{
    const auto decalBlockHeight = [rowHeight, gap](
            const SectorTopologyDecalLayer& decal,
            bool includeTintAndFit) {
        float height = SectorEditorInspectorTextureRowHeight() + gap;
        if (decal.materialId.empty()) {
            return height;
        }
        height += rowHeight + gap; // Opacity.
        height += 36.0f + gap; // Emissive.
        if (decal.emissive) {
            height += rowHeight + gap; // Bloom.
        }
        if (includeTintAndFit) {
            height += rowHeight + gap; // Tint.
            height += 36.0f + gap; // Fit/Clear.
        }
        return height;
    };

    float height = 38.0f; // Face title.
    height += anchorSummaryHeight;
    height += 36.0f + gap; // Merge Selected Into.
    height += 4.0f * (rowHeight + gap); // Void, floor, ceiling, and ceiling sky.

    height += 18.0f + 30.0f; // Audio separator/title.
    height += SectorEditorInspectorTextureRowHeight() + gap; // Footsteps.
    height += 4.0f * (rowHeight + gap); // Roomtone mode, sound, volume, fade override.
    if (anchor.roomtone.fadeMilliseconds
            != SectorRoomtoneSettings::UseMapFadeMilliseconds) {
        height += rowHeight + gap;
    }

    height += 18.0f + 30.0f; // Lighting separator/title.
    height += 4.0f * (rowHeight + gap); // Intensity and RGB.

    height += 18.0f + 30.0f; // Materials separator/title.
    height += (SectorEditorInspectorTextureRowHeight() + gap) * 5.0f;
    height += decalBlockHeight(anchor.floorDecal, true);
    height += decalBlockHeight(anchor.ceilingDecal, true);
    height += decalBlockHeight(anchor.defaultWall.decal, false);
    height += decalBlockHeight(anchor.defaultLower.decal, false);
    height += decalBlockHeight(anchor.defaultUpper.decal, false);
    height += rowHeight + gap; // Bottom breathing room.
    return height;
}

inline float MeasureSectorEditorAuthoringFogVolumeInspectorContentHeight(
        const SectorAuthoringFogVolume& volume,
        float rowHeight,
        float gap)
{
    int rowCount = 21; // Common controls, shape, path controls, color, and delete.
    if (volume.shape == SectorLocalFogShape::Box) {
        ++rowCount; // Yaw.
    }
    float height = 38.0f + static_cast<float>(rowCount) * (rowHeight + gap);
    height += SectorEditorInspectorStackedOptionRowHeight(rowHeight, gap) + gap;
    return height;
}

SectorEditorInspectorPanelResult DrawSectorEditorInspectorPanel(
        SectorEditorInspectorPanelContext& context);

} // namespace game
