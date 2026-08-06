#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

namespace game {

float MeasureSectorEditorDynamicModelInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object);

void DrawSectorEditorDynamicModelInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y);

} // namespace game
