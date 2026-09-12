#pragma once
#include "sector_editor/tools/SectorEditorToolModule.h"
namespace game
{
const SectorEditorToolModule &SectorEditorPathToolModule();
void DrawSectorEditorPaths(SectorEditorToolContext &context);
void AppendSectorEditorPathPicks(const SectorAuthoringGraph &graph, Vector2 screenPoint,
                                 const std::function<Vector2(Vector2)> &mapToScreen,
                                 std::vector<SectorEditorPickCandidate> &candidates);
bool UpdateSectorEditorPathSelection(SectorEditorToolContext &context);
bool ArmSectorEditorPathMove(SectorEditorToolContext &context, Vector2 screenPoint);
void SelectSectorEditorPathPart(SectorEditorToolContext &context, Vector2 screenPoint);
bool InsertSectorEditorPathWaypoint(SectorEditorToolContext &context, Vector2 screenPoint);
} // namespace game
