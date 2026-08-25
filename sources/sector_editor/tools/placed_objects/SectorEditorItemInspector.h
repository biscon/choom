#pragma once

namespace game {

struct SectorEditorPlacedObjectInspectorContext;
struct SectorEditorPlacedObjectInspectorMeasureContext;
struct SectorPlacedRuntimeObject;

float MeasureSectorEditorItemInspectorContentHeight(
        const SectorEditorPlacedObjectInspectorMeasureContext& context,
        const SectorPlacedRuntimeObject& object);

void DrawSectorEditorItemInspector(
        SectorEditorPlacedObjectInspectorContext& context,
        float& y);

} // namespace game
