#include "sector_editor/tools/SectorEditorToolModule.h"

#include "sector_editor/tools/insert_vertex/SectorEditorInsertVertexTool.h"
#include "sector_editor/tools/line/SectorEditorLineTool.h"
#include "sector_editor/tools/rectangle/SectorEditorRectangleTool.h"
#include "sector_editor/tools/select/SectorEditorSelectTool.h"

namespace game {

const SectorEditorToolModule* FindSectorEditorToolModule(SectorEditorTool tool)
{
    if (tool == SectorEditorTool::AuthoringLine) {
        return &SectorEditorLineToolModule();
    }
    if (tool == SectorEditorTool::AuthoringRectangle) {
        return &SectorEditorRectangleToolModule();
    }
    if (tool == SectorEditorTool::AuthoringInsertVertex) {
        return &SectorEditorInsertVertexToolModule();
    }
    if (tool == SectorEditorTool::Select) {
        return &SectorEditorSelectToolModule();
    }
    return nullptr;
}

} // namespace game
