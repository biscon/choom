#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

namespace game {

inline bool InitializeSectorEditorModelSwingDoor(
        SectorPlacedDoor& door,
        const SectorSwingDoorCatalog& catalog)
{
    if (catalog.assets.empty()) {
        return false;
    }
    door.visual = SectorDoorVisualType::Model;
    door.modelAssetId = catalog.assets.front().id;
    door.modelFit = SectorDoorModelFit::FitInside;
    door.modelScale = 1.0f;
    door.motion = SectorDoorMotionType::Swing;
    door.hinge = SectorDoorHinge::Start;
    door.swingSide = SectorDoorSwingSide::Front;
    door.openAngleDegrees = 90.0f;
    door.angularSpeedDegrees = 90.0f;
    return true;
}

inline bool SelectSectorEditorSwingDoorStyle(
        SectorPlacedDoor& door,
        const SectorSwingDoorCatalog& catalog,
        const std::string& styleId)
{
    if (door.modelAssetId == styleId
            || catalog.assetIndexById.find(styleId)
                    == catalog.assetIndexById.end()) {
        return false;
    }
    door.modelAssetId = styleId;
    return true;
}

inline bool SectorEditorDoorInspectorShowsSlideMotionOptions(
        const SectorPlacedDoor& door)
{
    return door.visual == SectorDoorVisualType::Procedural;
}

inline bool SectorEditorDoorInspectorShowsProceduralMaterialControls(
        const SectorPlacedDoor& door)
{
    return door.visual == SectorDoorVisualType::Procedural;
}

float MeasureSectorEditorDoorInspectorContentHeight(
    const SectorEditorPlacedObjectInspectorMeasureContext &context,
    const SectorPlacedRuntimeObject &object);

void DrawSectorEditorDoorInspector(
    SectorEditorPlacedObjectInspectorContext &context, float &y);

} // namespace game
