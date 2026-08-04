#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"

namespace game {

float MeasureSectorEditorStaticModelInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object);

void DrawSectorEditorStaticModelInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y);

} // namespace game
