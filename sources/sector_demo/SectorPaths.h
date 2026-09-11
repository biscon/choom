#pragma once

#include "sector_demo/SectorTopologyUnits.h"
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace game
{

struct SectorPathWaypoint
{
    int id = 0;
    SectorCoord x = 0;
    SectorCoord z = 0;
};

struct SectorAuthoringPath
{
    int editorId = 0;
    std::string id;
    std::vector<SectorPathWaypoint> waypoints;
    int nextWaypointId = 1;
};

struct SectorCompiledPath
{
    int editorId = 0;
    std::string id;
    std::vector<Vector2> points;
    std::vector<float> distances;
    float length = 0.0f;
};

struct SectorPropDragSettings
{
    int pathEditorId = 0;
    bool startAtEnd = false;
    float speedWorld = 0.5f;
    std::string startSound;
    std::string movingSound;
    std::string endSound;
};

inline bool ValidateSectorPath(const SectorAuthoringPath &path, std::string &error)
{
    if (path.editorId <= 0 || path.id.empty() || path.id.size() > 63)
    {
        error = "Path needs a positive editor ID and a 1-63 character name";
        return false;
    }
    for (unsigned char c : path.id)
    {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-'))
        {
            error = "Path ID accepts letters, digits, underscores and dashes";
            return false;
        }
    }
    if (path.waypoints.size() < 2)
    {
        error = "Path needs at least two waypoints";
        return false;
    }
    for (size_t i = 0; i < path.waypoints.size(); ++i)
    {
        const auto &p = path.waypoints[i];
        if (p.id <= 0)
        {
            error = "Waypoint ID must be positive";
            return false;
        }
        for (size_t j = 0; j < i; ++j)
            if (path.waypoints[j].id == p.id)
            {
                error = "Duplicate waypoint ID";
                return false;
            }
        if (i && p.x == path.waypoints[i - 1].x && p.z == path.waypoints[i - 1].z)
        {
            error = "Consecutive waypoints must differ";
            return false;
        }
    }
    return true;
}

inline SectorCompiledPath CompileSectorPath(const SectorAuthoringPath &path)
{
    SectorCompiledPath result;
    result.editorId = path.editorId;
    result.id = path.id;
    result.points.reserve(path.waypoints.size());
    result.distances.reserve(path.waypoints.size());
    for (const auto &point : path.waypoints)
    {
        const Vector2 p = SectorCoordToWorldPosition2(point.x, point.z);
        if (!result.points.empty())
            result.length += Vector2Distance(result.points.back(), p);
        result.points.push_back(p);
        result.distances.push_back(result.length);
    }
    return result;
}

inline const SectorCompiledPath *FindSectorPath(const std::vector<SectorCompiledPath> &paths,
                                                int id)
{
    for (const auto &path : paths)
        if (path.editorId == id)
            return &path;
    return nullptr;
}

// At an exact corner, direction chooses the segment we are about to travel on.
inline size_t SectorPathSegment(const SectorCompiledPath &path, float distance, int direction = 1)
{
    if (path.points.size() < 2)
        return 0;
    auto it = direction < 0
                  ? std::lower_bound(path.distances.begin(), path.distances.end(), distance)
                  : std::upper_bound(path.distances.begin(), path.distances.end(), distance);
    const size_t next = static_cast<size_t>(it - path.distances.begin());
    return std::min(next > 0 ? next - 1 : 0, path.points.size() - 2);
}

inline Vector2 EvaluateSectorPath(const SectorCompiledPath &path, float distance,
                                  Vector2 *tangent = nullptr, int direction = 1)
{
    if (path.points.size() < 2)
        return {};
    distance = std::clamp(distance, 0.0f, path.length);
    const size_t i = SectorPathSegment(path, distance, direction);
    const Vector2 delta = Vector2Subtract(path.points[i + 1], path.points[i]);
    const float length = path.distances[i + 1] - path.distances[i];
    if (tangent)
        *tangent = length > 0 ? Vector2Scale(delta, 1.0f / length) : Vector2{};
    return Vector2Add(
        path.points[i],
        Vector2Scale(delta, length > 0 ? (distance - path.distances[i]) / length : 0));
}

} // namespace game
