#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

namespace game {

float MeasureSectorEditorWindowInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object);

void DrawSectorEditorWindowInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y);

} // namespace game
