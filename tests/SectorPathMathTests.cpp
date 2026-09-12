#include "sector_demo/SectorPaths.h"
#include "sector_demo/SectorBoxSweep.h"
#include <cassert>
#include <cmath>
int main()
{
    using namespace game;
    SectorAuthoringPath path;
    path.editorId = 1;
    path.id = "route";
    path.waypoints = {{1, 0, 0}, {2, 256, 0}, {3, 256, 384}};
    std::string error;
    assert(ValidateSectorPath(path, error));
    const auto compiled = CompileSectorPath(path);
    assert(std::abs(compiled.length - 5) < 0.0001f);
    auto p = EvaluateSectorPath(compiled, 3);
    assert(std::abs(p.x - 2) < 0.0001f && std::abs(p.y - 1) < 0.0001f);
    assert(SectorPathSegment(compiled, 2, 1) == 1 && SectorPathSegment(compiled, 2, -1) == 0);
    assert(EvaluateSectorPath(compiled, -1).x == 0 && EvaluateSectorPath(compiled, 100).y == 3);
    path.waypoints[2] = path.waypoints[1];
    assert(!ValidateSectorPath(path, error));
    const Vector2 wall[]{{4, 0}, {4, 5}};
    const float box =
        SweepSectorBoxPolygon({2, 2}, {1, 0}, {0, 1}, {0.5f, 0.5f}, {100, 0}, wall, 2);
    const float circle = SweepSectorCirclePolygon({2, 2}, 0.5f, {100, 0}, wall, 2);
    assert(std::abs(box - 0.015f) < 0.00001f && std::abs(circle - 0.015f) < 0.00001f);
    assert(SweepSectorCirclePolygon({3.5f, 2}, 0.5f, {-1, 0}, wall, 2) == 1);
    assert(SweepSectorBoxPolygon({3.5f, 2}, {1, 0}, {0, 1}, {0.5f, 0.5f}, {-1, 0}, wall, 2) == 1);
    // A circle can pass the end of the wall where its enclosing box cannot.
    assert(SweepSectorCirclePolygon({3.6f, 5.4f}, 0.5f, {0, 1}, wall, 2) == 1);
}
