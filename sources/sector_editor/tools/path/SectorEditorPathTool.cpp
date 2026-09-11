#include "sector_editor/tools/path/SectorEditorPathTool.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/services/paths/SectorEditorPathEditingService.h"
#include <numeric>
#include <raymath.h>
namespace game
{
namespace
{
Vector2 Position(const SectorPathWaypoint &p)
{
    return {SectorCoordToVisibleAuthoring(p.x), SectorCoordToVisibleAuthoring(p.z)};
}
float SegmentDistance(Vector2 p, Vector2 a, Vector2 b, Vector2 *nearest = nullptr)
{
    const Vector2 d = Vector2Subtract(b, a);
    const float t = std::clamp(Vector2DotProduct(Vector2Subtract(p, a), d) /
                                   std::max(Vector2LengthSqr(d), 0.000001f),
                               0.0f, 1.0f);
    const Vector2 q = Vector2Add(a, Vector2Scale(d, t));
    if (nearest)
        *nearest = q;
    return Vector2Distance(p, q);
}
struct PathHit
{
    bool hit = false;
    int waypointId = 0;
    float distance2 = 0;
};
PathHit HitPath(const SectorAuthoringPath &path, Vector2 screen,
                const std::function<Vector2(Vector2)> &mapToScreen)
{
    PathHit result;
    if (!mapToScreen)
        return result;
    float best = 9.0f;
    for (const auto &point : path.waypoints)
    {
        const float distance = Vector2Distance(screen, mapToScreen(Position(point)));
        if (distance <= best)
        {
            result = {true, point.id, distance * distance};
            best = distance;
        }
    }
    if (result.hit)
        return result;
    best = 7.0f;
    for (size_t i = 1; i < path.waypoints.size(); ++i)
    {
        const float distance = SegmentDistance(screen, mapToScreen(Position(path.waypoints[i - 1])),
                                               mapToScreen(Position(path.waypoints[i])));
        if (distance <= best)
        {
            result = {true, 0, distance * distance};
            best = distance;
        }
    }
    return result;
}
bool Snapped(SectorEditorToolContext &c, SectorTopologyCoordPoint &p)
{
    std::string error;
    return c.currentSnappedSectorPoint && c.toTopologyCoordPoint &&
           c.toTopologyCoordPoint(c.currentSnappedSectorPoint(), p, error);
}
bool Cancel(SectorEditorToolContext &c, const char *message)
{
    if (!c.pathEditing)
        return false;
    c.pathEditing->Cancel();
    if (message)
        c.statusText = message;
    return true;
}
bool Update(SectorEditorToolContext &c)
{
    if (!c.input || !c.pathEditing)
        return false;
    bool handled = false;
    c.input->ForEachEvent(engine::InputEventType::KeyPressed, true,
                          [&](engine::InputEvent &e)
                          {
                              if (e.key.key == KEY_ENTER)
                              {
                                  if (c.pathEditing->Create())
                                      c.currentTool = SectorEditorTool::Select;
                              }
                              else if (e.key.key == KEY_ESCAPE)
                                  Cancel(c, "Path drawing cancelled");
                              else
                                  return;
                              engine::ConsumeEvent(e);
                              handled = true;
                          });
    c.input->ForEachEvent(
        engine::InputEventType::MouseClick, true,
        [&](engine::InputEvent &e)
        {
            if (!CheckCollisionPointRec(e.mouseClick.releasePosition, c.canvasRect))
                return;
            if (e.mouseClick.button == MOUSE_RIGHT_BUTTON)
                Cancel(c, "Path drawing cancelled");
            else if (e.mouseClick.button == MOUSE_LEFT_BUTTON)
            {
                SectorTopologyCoordPoint p;
                if (!Snapped(c, p))
                    return;
                auto &points = c.pathEditing->State().pending;
                if (points.empty() || points.back().x != p.x || points.back().z != p.y)
                    points.push_back({static_cast<int>(points.size() + 1), p.x, p.y});
            }
            else
                return;
            engine::ConsumeEvent(e);
            handled = true;
        });
    return handled;
}
} // namespace
void AppendSectorEditorPathPicks(const SectorAuthoringGraph &graph, Vector2 screen,
                                 const std::function<Vector2(Vector2)> &mapToScreen,
                                 std::vector<SectorEditorPickCandidate> &out)
{
    for (const auto &path : graph.paths)
    {
        const auto hit = HitPath(path, screen, mapToScreen);
        if (hit.hit)
            out.push_back({{SectorEditorPickKind::Path, path.editorId}, hit.distance2});
    }
}
void SelectSectorEditorPathPart(SectorEditorToolContext &c, Vector2 screen)
{
    if (!c.pathEditing || !c.pathEditing->Selected())
        return;
    const auto hit = HitPath(*c.pathEditing->Selected(), screen, c.mapToScreen);
    c.pathEditing->State().waypointId = hit.hit ? hit.waypointId : 0;
}
bool ArmSectorEditorPathMove(SectorEditorToolContext &c, Vector2 screen)
{
    if (!c.pathEditing || !c.pathEditing->Selected())
        return false;
    const auto hit = HitPath(*c.pathEditing->Selected(), screen, c.mapToScreen);
    SectorTopologyCoordPoint point;
    return hit.hit && Snapped(c, point) && c.pathEditing->ArmMove(hit.waypointId, screen, point);
}
bool UpdateSectorEditorPathSelection(SectorEditorToolContext &c)
{
    if (!c.pathEditing || !c.input)
        return false;
    auto &e = c.pathEditing->State();
    if (!c.pathEditing->Selected())
    {
        if (e.moveArmed || e.moving)
            c.pathEditing->Cancel();
        return false;
    }
    bool handled = false;
    c.input->ForEachEvent(engine::InputEventType::KeyPressed, true,
                          [&](engine::InputEvent &key)
                          {
                              if (key.key.key == KEY_ESCAPE)
                              {
                                  c.pathEditing->Cancel();
                              }
                              else if (key.key.key == KEY_DELETE)
                              {
                                  if (e.waypointId)
                                      c.pathEditing->Dissolve();
                                  else
                                      c.pathEditing->Delete();
                              }
                              else
                                  return;
                              engine::ConsumeEvent(key);
                              handled = true;
                          });
    c.input->ForEachEvent(engine::InputEventType::MouseButtonPressed, true,
                          [&](engine::InputEvent &event)
                          {
                              if (event.mouseButton.button == MOUSE_RIGHT_BUTTON &&
                                  (e.moveArmed || e.moving))
                              {
                                  c.pathEditing->Cancel();
                                  engine::ConsumeEvent(event);
                                  handled = true;
                              }
                          });
    bool leftReleased = false;
    Vector2 pointer = c.input->MousePosition();
    c.input->ForEachEvent(engine::InputEventType::MouseButtonReleased, true,
                          [&](engine::InputEvent &event)
                          {
                              if (event.mouseButton.button == MOUSE_LEFT_BUTTON)
                              {
                                  leftReleased = true;
                                  pointer = event.mouseButton.position;
                              }
                          });
    if (e.moveArmed)
    {
        if (c.input->IsMouseButtonDown(MOUSE_LEFT_BUTTON) || leftReleased)
            c.pathEditing->UpdateMoveArm(pointer);
        if (leftReleased || !c.input->IsMouseButtonDown(MOUSE_LEFT_BUTTON))
            e.moveArmed = false;
    }
    // A stationary press/release belongs to the shared click-selection cycle.
    if (e.moving)
    {
        SectorTopologyCoordPoint p;
        if (Snapped(c, p))
            c.pathEditing->Move(p);
        c.input->ForEachEvent(engine::InputEventType::MouseButtonReleased, true,
                              [&](engine::InputEvent &event)
                              {
                                  if (event.mouseButton.button == MOUSE_LEFT_BUTTON)
                                  {
                                      c.pathEditing->FinishMove();
                                      engine::ConsumeEvent(event);
                                      handled = true;
                                  }
                              });
        c.input->ForEachEvent(engine::InputEventType::MouseClick, true,
                              [&](engine::InputEvent &event)
                              {
                                  if (event.mouseClick.button == MOUSE_RIGHT_BUTTON)
                                      c.pathEditing->Cancel();
                                  engine::ConsumeEvent(event);
                                  handled = true;
                              });
    }
    return handled;
}
bool InsertSectorEditorPathWaypoint(SectorEditorToolContext &c, Vector2 screen)
{
    if (!c.pathEditing || !c.pathEditing->Selected())
        return false;
    const auto &path = *c.pathEditing->Selected();
    float best = 9;
    size_t segment = path.waypoints.size();
    Vector2 nearest;
    for (size_t i = 1; i < path.waypoints.size(); ++i)
    {
        Vector2 q;
        float distance = SegmentDistance(screen, c.mapToScreen(Position(path.waypoints[i - 1])),
                                         c.mapToScreen(Position(path.waypoints[i])), &q);
        if (distance < best)
        {
            best = distance;
            segment = i - 1;
            nearest = q;
        }
    }
    if (segment < path.waypoints.size())
    {
        const auto &a = path.waypoints[segment];
        const auto &b = path.waypoints[segment + 1];
        const int64_t dx = int64_t(b.x) - a.x, dz = int64_t(b.z) - a.z;
        const int64_t steps = std::gcd(std::abs(dx), std::abs(dz));
        if (steps < 2)
        {
            c.statusText = "Segment has no interior point at the map coordinate precision";
            return true;
        }
        // Choose an exact lattice point on the segment, preserving its shape.
        const Vector2 start = c.mapToScreen(Position(a));
        const Vector2 end = c.mapToScreen(Position(b));
        const double t = Vector2Distance(start, nearest) / Vector2Distance(start, end);
        const int64_t step = std::clamp(int64_t(std::llround(t * steps)), int64_t(1), steps - 1);
        const SectorTopologyCoordPoint point{
            static_cast<SectorCoord>(int64_t(a.x) + dx / steps * step),
            static_cast<SectorCoord>(int64_t(a.z) + dz / steps * step)};
        if (c.pathEditing->Insert(segment, point))
            c.currentTool = SectorEditorTool::Select;
    }
    return true;
}
void DrawSectorEditorPaths(SectorEditorToolContext &c)
{
    if (!c.pathEditing || !c.mapToScreen)
        return;
    auto &e = c.pathEditing->State();
    const auto *selected = c.pathEditing->Selected();
    const auto draw = [&](const std::vector<SectorPathWaypoint> &points, Color color, bool handles)
    {
        for (size_t i = 1; i < points.size(); ++i)
        {
            Vector2 a = c.mapToScreen(Position(points[i - 1])),
                    b = c.mapToScreen(Position(points[i]));
            DrawLineEx(a, b, 2, color);
            const Vector2 mid = Vector2Scale(Vector2Add(a, b), 0.5f),
                          d = Vector2Normalize(Vector2Subtract(b, a));
            const Vector2 back = Vector2Subtract(mid, Vector2Scale(d, 7)), side{-d.y * 4, d.x * 4};
            DrawLineEx(mid, Vector2Add(back, side), 2, color);
            DrawLineEx(mid, Vector2Subtract(back, side), 2, color);
        }
        for (size_t i = 0; i < points.size(); ++i)
        {
            const Vector2 p = c.mapToScreen(Position(points[i]));
            const Color tint = i == 0 ? GREEN : i + 1 == points.size() ? ORANGE : color;
            if (handles || i == 0 || i + 1 == points.size())
                DrawCircleV(p, handles && e.waypointId == points[i].id ? 7 : 4, tint);
        }
    };
    for (const auto &path : c.authoringGraph.paths)
    {
        const bool active = selected && selected->editorId == path.editorId;
        draw(active && e.moving ? e.preview.waypoints : path.waypoints,
             active || c.highlightedPathId == path.editorId ? YELLOW : Color{100, 190, 235, 190},
             active);
    }
    draw(e.pending, YELLOW, true);
    if (!e.pending.empty() && c.currentTool == SectorEditorTool::Path)
    {
        SectorTopologyCoordPoint p;
        if (Snapped(c, p))
            DrawLineEx(c.mapToScreen(Position(e.pending.back())),
                       c.mapToScreen({SectorCoordToVisibleAuthoring(p.x),
                                      SectorCoordToVisibleAuthoring(p.y)}),
                       2, YELLOW);
    }
}
const SectorEditorToolModule &SectorEditorPathToolModule()
{
    static const SectorEditorToolModule module{
        SectorEditorTool::Path, "Path", nullptr, nullptr, nullptr, Update, nullptr, Cancel};
    return module;
}
} // namespace game
