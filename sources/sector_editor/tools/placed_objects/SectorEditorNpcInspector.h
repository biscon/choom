#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

namespace game {

float MeasureSectorEditorNpcInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object);

void DrawSectorEditorNpcInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y);

} // namespace game
