#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

namespace game {

float MeasureSectorEditorDuctAccessInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object);

void DrawSectorEditorDuctAccessInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y);

} // namespace game
