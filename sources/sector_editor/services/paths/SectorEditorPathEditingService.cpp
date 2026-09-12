#include "sector_editor/services/paths/SectorEditorPathEditingService.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorUnits.h"
#include "sector_editor/SectorEditorHelpers.h"
#include <climits>
#include <cstdio>
namespace game
{
SectorAuthoringPath *SectorEditorPathEditingService::Selected()
{
    if (context_.selection.selectedAuthoring.kind != SectorAuthoringSelectionKind::Path)
        return nullptr;
    for (auto &path : context_.graph.paths)
        if (path.editorId == context_.selection.selectedAuthoring.pathId)
            return &path;
    return nullptr;
}
const SectorAuthoringPath *SectorEditorPathEditingService::Selected() const
{
    return const_cast<SectorEditorPathEditingService *>(this)->Selected();
}
bool SectorEditorPathEditingService::Select(int id)
{
    for (const auto &path : context_.graph.paths)
        if (path.editorId == id)
        {
            Cancel();
            context_.selection = {};
            context_.selection.selectedAuthoring.kind = SectorAuthoringSelectionKind::Path;
            context_.selection.selectedAuthoring.pathId = id;
            context_.editing.waypointId = 0;
            context_.editing.bufferedPathId = 0;
            return true;
        }
    return false;
}
bool SectorEditorPathEditingService::Commit(SectorAuthoringGraph candidate, const char *status)
{
    if (!context_.derivation.authoringDerivation.success)
    {
        context_.status = "Path editing requires valid authoring data";
        return false;
    }
    auto derived = DeriveSectorTopologyMapFromAuthoringGraph(candidate);
    if (!derived.success)
    {
        context_.status = "Path edit rejected: invalid authoring data";
        return false;
    }
    SectorTopologyMap mapData = context_.map;
    SectorCollisionWorld collision;
    collision.BuildFromTopology(derived.topology);
    for (auto &object : mapData.runtimeObjects)
    {
        if (object.kind != "dynamic_model")
            continue;
        const auto *path =
            FindSectorPath(derived.topology.paths, object.dynamicModel.drag.pathEditorId);
        if (!path || path->points.empty())
            continue;
        const auto p =
            object.dynamicModel.drag.startAtEnd ? path->points.back() : path->points.front();
        object.position.x = SectorWorldToAuthoringDistance(p.x);
        object.position.z = SectorWorldToAuthoringDistance(p.y);
        SectorCollisionHeights h;
        if (collision.GetSectorFloorCeiling(collision.FindSectorContainingPoint(p), &h))
            object.position.y = SectorWorldToAuthoringDistance(h.floorZ);
    }
    const bool ok = CommitSectorEditorAuthoringGraphCandidate(
        context_.state, context_.lifecycle, context_.map, context_.graph, context_.derivation,
        context_.selection, std::move(candidate), std::move(derived), mapData, status);
    context_.status = ok ? status : "Path edit could not be committed";
    return ok;
}
bool SectorEditorPathEditingService::Create()
{
    auto candidate = context_.graph;
    if (candidate.nextPathId <= 0 || candidate.nextPathId >= INT_MAX)
        return false;
    SectorAuthoringPath path;
    path.editorId = candidate.nextPathId++;
    path.id = "path_" + std::to_string(path.editorId);
    for (int suffix = 1; std::any_of(candidate.paths.begin(), candidate.paths.end(),
                                     [&](const auto &p) { return p.id == path.id; });
         ++suffix)
        path.id = "path_" + std::to_string(path.editorId) + "_" + std::to_string(suffix);
    path.waypoints = context_.editing.pending;
    path.nextWaypointId = static_cast<int>(path.waypoints.size()) + 1;
    std::string error;
    if (!ValidateSectorPath(path, error))
    {
        context_.status = error;
        return false;
    }
    const int id = path.editorId;
    candidate.paths.push_back(std::move(path));
    if (!Commit(std::move(candidate), "Created path"))
        return false;
    context_.editing.pending.clear();
    Select(id);
    return true;
}
bool SectorEditorPathEditingService::Replace(const SectorAuthoringPath &path, const char *status)
{
    std::string error;
    if (!ValidateSectorPath(path, error))
    {
        context_.status = error;
        return false;
    }
    auto candidate = context_.graph;
    for (auto &p : candidate.paths)
        if (p.editorId == path.editorId)
            p = path;
    return Commit(std::move(candidate), status);
}
bool SectorEditorPathEditingService::Rename(const std::string &name)
{
    const auto *selected = Selected();
    if (!selected)
        return false;
    for (const auto &p : context_.graph.paths)
        if (p.editorId != selected->editorId && p.id == name)
        {
            context_.status = "Path ID already exists";
            return false;
        }
    auto path = *selected;
    path.id = name;
    return Replace(path, "Renamed path");
}
bool SectorEditorPathEditingService::Delete()
{
    const auto *selected = Selected();
    if (!selected)
        return false;
    std::string users;
    for (const auto &o : context_.map.runtimeObjects)
        if (o.kind == "dynamic_model" && o.dynamicModel.drag.pathEditorId == selected->editorId)
            users += (users.empty() ? "" : ", ") + o.dynamicModel.useTitle + " (#" +
                     std::to_string(o.id) + ")";
    if (!users.empty())
    {
        context_.status = "Unassign path from: " + users;
        return false;
    }
    const int id = selected->editorId;
    auto candidate = context_.graph;
    candidate.paths.erase(std::remove_if(candidate.paths.begin(), candidate.paths.end(),
                                         [id](const auto &p) { return p.editorId == id; }),
                          candidate.paths.end());
    if (!Commit(std::move(candidate), "Deleted path"))
        return false;
    context_.selection = {};
    context_.editing = {};
    return true;
}
bool SectorEditorPathEditingService::Dissolve()
{
    const auto *selected = Selected();
    if (!selected || !context_.editing.waypointId)
        return false;
    auto path = *selected;
    const int id = context_.editing.waypointId;
    path.waypoints.erase(std::remove_if(path.waypoints.begin(), path.waypoints.end(),
                                        [id](const auto &p) { return p.id == id; }),
                         path.waypoints.end());
    if (!Replace(path, "Dissolved path waypoint"))
        return false;
    context_.editing.waypointId = 0;
    return true;
}
bool SectorEditorPathEditingService::Insert(size_t segment, SectorTopologyCoordPoint point)
{
    const auto *selected = Selected();
    if (!selected || segment + 1 >= selected->waypoints.size())
        return false;
    auto path = *selected;
    if (path.nextWaypointId >= INT_MAX)
        return false;
    const int id = path.nextWaypointId++;
    path.waypoints.insert(path.waypoints.begin() + segment + 1, {id, point.x, point.y});
    if (!Replace(path, "Inserted path waypoint"))
        return false;
    context_.editing.waypointId = id;
    return true;
}
bool SectorEditorPathEditingService::ArmMove(int waypointId, Vector2 screen,
                                             SectorTopologyCoordPoint point)
{
    const auto *path = Selected();
    if (!path)
        return false;
    auto &edit = context_.editing;
    edit.moveArmed = true;
    edit.armedPathId = path->editorId;
    edit.armedWaypointId = waypointId;
    edit.pressScreen = screen;
    edit.press = point;
    return true;
}
bool SectorEditorPathEditingService::UpdateMoveArm(Vector2 screen)
{
    auto &edit = context_.editing;
    if (!edit.moveArmed)
        return false;
    const auto *path = Selected();
    if (!path || path->editorId != edit.armedPathId ||
        context_.state.currentTool != SectorEditorTool::Select)
    {
        Cancel();
        return false;
    }
    if (!ShouldStartSectorEditorSelectDrag(edit.pressScreen, screen))
        return false;
    edit.moveArmed = false;
    edit.waypointId = edit.armedWaypointId;
    return BeginMove(edit.press);
}
bool SectorEditorPathEditingService::BeginMove(SectorTopologyCoordPoint point)
{
    const auto *selected = Selected();
    if (!selected)
        return false;
    auto &e = context_.editing;
    e.original = *selected;
    e.preview = *selected;
    e.press = point;
    e.moving = true;
    return true;
}
void SectorEditorPathEditingService::Move(SectorTopologyCoordPoint point)
{
    auto &e = context_.editing;
    if (!e.moving)
        return;
    const int64_t dx = int64_t(point.x) - e.press.x, dz = int64_t(point.y) - e.press.y;
    auto candidate = e.original;
    for (auto &p : candidate.waypoints)
        if (!e.waypointId || e.waypointId == p.id)
        {
            const int64_t x = int64_t(p.x) + dx, z = int64_t(p.z) + dz;
            if (x < INT_MIN || x > INT_MAX || z < INT_MIN || z > INT_MAX)
                return;
            p.x = static_cast<SectorCoord>(x);
            p.z = static_cast<SectorCoord>(z);
        }
    e.preview = std::move(candidate);
}
bool SectorEditorPathEditingService::FinishMove()
{
    auto &e = context_.editing;
    if (!e.moving)
        return false;
    e.moving = false;
    bool changed = false;
    for (size_t i = 0; i < e.original.waypoints.size(); ++i)
        changed |= e.original.waypoints[i].x != e.preview.waypoints[i].x ||
                   e.original.waypoints[i].z != e.preview.waypoints[i].z;
    return changed && Replace(e.preview, "Moved path");
}
void SectorEditorPathEditingService::Cancel()
{
    context_.editing.moving = false;
    context_.editing.moveArmed = false;
    context_.editing.pending.clear();
}
} // namespace game
