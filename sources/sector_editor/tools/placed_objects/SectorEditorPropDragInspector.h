#pragma once
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectInspector.h"
namespace game
{
inline float MeasureSectorEditorPropDragInspector(float rowHeight, float gap, bool assigned)
{
    return (assigned ? 14 : 2) * (rowHeight + gap);
}
void DrawSectorEditorPropDragInspector(SectorEditorPlacedObjectInspectorContext &context, float &y);
} // namespace game
