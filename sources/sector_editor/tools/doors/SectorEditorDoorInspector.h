#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

namespace game {

float MeasureSectorEditorDoorInspectorContentHeight(
    const SectorEditorPlacedObjectInspectorMeasureContext &context,
    const SectorPlacedRuntimeObject &object);

void DrawSectorEditorDoorInspector(
    SectorEditorPlacedObjectInspectorContext &context, float &y);

} // namespace game
