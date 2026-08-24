#include "sector_editor/tools/SectorEditorToolModule.h"

#include "sector_editor/tools/insert_vertex/SectorEditorInsertVertexTool.h"
#include "sector_editor/tools/fog_volume/SectorEditorFogVolumeTool.h"
#include "sector_editor/tools/line/SectorEditorLineTool.h"
#include "sector_editor/tools/level_marker/SectorEditorLevelMarkerTool.h"
#include "sector_editor/tools/rectangle/SectorEditorRectangleTool.h"
#include "sector_editor/tools/reflection_probe/SectorEditorReflectionProbeTool.h"
#include "sector_editor/tools/select/SectorEditorSelectTool.h"
#include "sector_editor/tools/trigger/SectorEditorTriggerTool.h"

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
    if (tool == SectorEditorTool::AuthoringFogVolume) {
        return &SectorEditorFogVolumeToolModule();
    }
    if (tool == SectorEditorTool::ReflectionProbe) {
        return &SectorEditorReflectionProbeToolModule();
    }
    if (tool == SectorEditorTool::LevelMarker) {
        return &SectorEditorLevelMarkerToolModule();
    }
    if (tool == SectorEditorTool::Trigger) return &SectorEditorTriggerToolModule();
    return nullptr;
}

} // namespace game
