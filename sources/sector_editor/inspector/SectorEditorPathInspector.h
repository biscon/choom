#pragma once
#include "sector_editor/inspector/SectorEditorInspectorPanel.h"
namespace game
{
inline float MeasureSectorEditorPathInspector(float rowHeight, float gap)
{
    return 7 * (rowHeight + gap) + 64;
}
void DrawSectorEditorPathInspector(SectorEditorInspectorPanelContext &context, float width,
                                   float rowHeight, float gap);
} // namespace game
