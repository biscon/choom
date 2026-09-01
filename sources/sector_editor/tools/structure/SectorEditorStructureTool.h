#pragma once

#include "sector_editor/tools/SectorEditorToolModule.h"

namespace game {

const SectorEditorToolModule& SectorEditorStructureToolModule();
bool BeginSectorEditorStructuralManipulation(
        SectorEditorToolContext& context,
        Vector2 screenPoint);
bool UpdateSectorEditorStructuralManipulation(SectorEditorToolContext& context);
bool CancelSectorEditorStructuralManipulation(
        SectorEditorToolContext& context,
        const char* message);
void DrawSectorEditorStructuralSelectionOverlay(SectorEditorToolContext& context);

} // namespace game
