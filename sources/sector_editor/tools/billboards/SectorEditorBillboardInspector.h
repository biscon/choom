#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

namespace game {

float MeasureSectorEditorBillboardInspectorContentHeight(
    const SectorEditorPlacedObjectInspectorMeasureContext &context,
    const SectorPlacedRuntimeObject &object);

void DrawSectorEditorBillboardInspector(
    SectorEditorPlacedObjectInspectorContext &context, float &y);

} // namespace game
