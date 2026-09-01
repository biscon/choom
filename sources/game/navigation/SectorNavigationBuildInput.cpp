#include "game/navigation/SectorNavigationBuildInput.h"

#include <raymath.h>

#include "sector_demo/SectorTopologyUnits.h"
#include "util/earcut.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace game {
namespace {

constexpr uint8_t NavigationRasterNullArea = 0;
constexpr uint8_t NavigationRasterWalkableArea = 63;

using EarcutPoint = std::array<double, 2>;
using EarcutRing = std::vector<EarcutPoint>;
using EarcutPolygon = std::vector<EarcutRing>;

struct Fnv64 {
    uint64_t value = 14695981039346656037ull;

    void Bytes(const void* data, size_t size)
    {
        const auto* bytes = static_cast<const uint8_t*>(data);
        for (size_t index = 0; index < size; ++index) {
            value ^= bytes[index];
            value *= 1099511628211ull;
        }
    }

    template<typename T>
    void Pod(const T& pod)
    {
        Bytes(&pod, sizeof(pod));
    }

    void Float(float number)
    {
        uint32_t bits = 0;
        std::memcpy(&bits, &number, sizeof(bits));
        Pod(bits);
    }

    void String(const std::string& text)
    {
        const uint64_t size = text.size();
        Pod(size);
        Bytes(text.data(), text.size());
    }
};

template<typename T>
std::vector<const T*> SortedById(const std::vector<T>& values)
{
    std::vector<const T*> result;
    result.reserve(values.size());
    for (const T& value : values) result.push_back(&value);
    std::sort(result.begin(), result.end(), [](const T* lhs, const T* rhs) {
        return lhs->id < rhs->id;
    });
    return result;
}

void ExpandBounds(Vector3 point, Vector3& minimum, Vector3& maximum)
{
    minimum.x = std::min(minimum.x, point.x);
    minimum.y = std::min(minimum.y, point.y);
    minimum.z = std::min(minimum.z, point.z);
    maximum.x = std::max(maximum.x, point.x);
    maximum.y = std::max(maximum.y, point.y);
    maximum.z = std::max(maximum.z, point.z);
}

void AddTriangle(
        SectorNavigationBuildInput& input,
        Vector3 a,
        Vector3 b,
        Vector3 c,
        uint8_t area,
        int sourceId,
        Vector3& minimum,
        Vector3& maximum)
{
    input.triangles.push_back({a, b, c, area, sourceId});
    ExpandBounds(a, minimum, maximum);
    ExpandBounds(b, minimum, maximum);
    ExpandBounds(c, minimum, maximum);
}

bool AppendSectorSurface(
        const SectorTopologyMap& map,
        const SectorTopologySector& sector,
        const SectorTopologyLoopSet& loops,
        float authoredHeight,
        bool faceUp,
        uint8_t area,
        SectorNavigationBuildInput& input,
        Vector3& minimum,
        Vector3& maximum,
        std::string& error)
{
    EarcutPolygon polygon;
    std::vector<const SectorTopologyVertex*> flattened;
    const auto appendLoop = [&](const SectorTopologyLoop& loop) {
        EarcutRing ring;
        ring.reserve(loop.vertexIds.size());
        for (int vertexId : loop.vertexIds) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
            if (vertex == nullptr) {
                error = "Navigation sector " + std::to_string(sector.id)
                        + " references missing vertex " + std::to_string(vertexId);
                return false;
            }
            ring.push_back({static_cast<double>(vertex->x), static_cast<double>(vertex->y)});
            flattened.push_back(vertex);
        }
        polygon.push_back(std::move(ring));
        return true;
    };

    if (!appendLoop(loops.outer)) return false;
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (!appendLoop(hole)) return false;
    }
    const std::vector<uint32_t> indices = mapbox::earcut<uint32_t>(polygon);
    if (indices.empty() || indices.size() % 3 != 0) {
        error = "Navigation triangulation failed for sector " + std::to_string(sector.id);
        return false;
    }
    for (size_t index = 0; index < indices.size(); index += 3) {
        if (indices[index] >= flattened.size()
            || indices[index + 1] >= flattened.size()
            || indices[index + 2] >= flattened.size()) {
            error = "Navigation triangulation returned an invalid vertex for sector "
                    + std::to_string(sector.id);
            return false;
        }
        Vector3 a = SectorCoordToWorldPosition3(
                flattened[indices[index]]->x, authoredHeight,
                flattened[indices[index]]->y);
        Vector3 b = SectorCoordToWorldPosition3(
                flattened[indices[index + 1]]->x, authoredHeight,
                flattened[indices[index + 1]]->y);
        Vector3 c = SectorCoordToWorldPosition3(
                flattened[indices[index + 2]]->x, authoredHeight,
                flattened[indices[index + 2]]->y);
        const float normalY = (b.z - a.z) * (c.x - a.x)
                - (b.x - a.x) * (c.z - a.z);
        if ((faceUp && normalY < 0.0f) || (!faceUp && normalY > 0.0f)) {
            std::swap(b, c);
        }
        AddTriangle(input, a, b, c, area, sector.id, minimum, maximum);
    }
    return true;
}

void AddBarrierPrism(
        Vector2 start,
        Vector2 end,
        float bottom,
        float top,
        float thickness,
        int sourceId,
        SectorNavigationBuildInput& input,
        Vector3& minimum,
        Vector3& maximum)
{
    const float dx = end.x - start.x;
    const float dz = end.y - start.y;
    const float length = std::sqrt(dx * dx + dz * dz);
    if (!(length > 0.00001f)) return;
    const Vector2 normal{-dz / length, dx / length};
    const Vector2 offset{normal.x * thickness * 0.5f, normal.y * thickness * 0.5f};
    const std::array<Vector3, 8> v{{
            {start.x - offset.x, bottom, start.y - offset.y},
            {end.x - offset.x, bottom, end.y - offset.y},
            {end.x + offset.x, bottom, end.y + offset.y},
            {start.x + offset.x, bottom, start.y + offset.y},
            {start.x - offset.x, top, start.y - offset.y},
            {end.x - offset.x, top, end.y - offset.y},
            {end.x + offset.x, top, end.y + offset.y},
            {start.x + offset.x, top, start.y + offset.y}
    }};
    constexpr int faces[12][3] = {
            {0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7},
            {0, 1, 5}, {0, 5, 4}, {1, 2, 6}, {1, 6, 5},
            {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}
    };
    for (const auto& face : faces) {
        AddTriangle(input, v[face[0]], v[face[1]], v[face[2]],
                NavigationRasterNullArea, sourceId, minimum, maximum);
    }
}

void AddStaticObstacleGeometry(
        const SectorStaticModelCollider& collider,
        SectorNavigationBuildInput& input,
        Vector3& minimum,
        Vector3& maximum)
{
    const auto corner = [&](float x, float z, float y) {
        return Vector3{
                collider.center.x + collider.axisX.x * x + collider.axisZ.x * z,
                y,
                collider.center.y + collider.axisX.y * x + collider.axisZ.y * z};
    };
    const float ex = collider.halfExtents.x;
    const float ez = collider.halfExtents.y;
    const std::array<Vector3, 8> v{{
            corner(-ex, -ez, collider.bottom), corner(ex, -ez, collider.bottom),
            corner(ex, ez, collider.bottom), corner(-ex, ez, collider.bottom),
            corner(-ex, -ez, collider.top), corner(ex, -ez, collider.top),
            corner(ex, ez, collider.top), corner(-ex, ez, collider.top)
    }};
    constexpr int nullFaces[10][3] = {
            {0, 2, 1}, {0, 3, 2}, {0, 1, 5}, {0, 5, 4}, {1, 2, 6},
            {1, 6, 5}, {2, 3, 7}, {2, 7, 6}, {3, 0, 4}, {3, 4, 7}
    };
    for (const auto& face : nullFaces) {
        AddTriangle(input, v[face[0]], v[face[1]], v[face[2]],
                NavigationRasterNullArea, collider.placedObjectId, minimum, maximum);
    }
    AddTriangle(input, v[4], v[5], v[6], NavigationRasterWalkableArea,
            collider.placedObjectId, minimum, maximum);
    AddTriangle(input, v[4], v[6], v[7], NavigationRasterWalkableArea,
            collider.placedObjectId, minimum, maximum);
    input.staticObstacles.push_back(SectorNavigationDebugObstacle{
            collider.placedObjectId, collider.center, collider.axisX, collider.axisZ,
            collider.halfExtents, collider.bottom, collider.top});
}

void AddStructuralPrimitiveGeometry(
        const SectorCompiledStructuralPrimitive& primitive,
        const SectorNavigationSettings& settings,
        SectorNavigationBuildInput& input,
        Vector3& minimum,
        Vector3& maximum)
{
    if (!primitive.authored.enabled || !primitive.authored.collision) return;
    const float minimumWalkableNormalY = std::cos(
            settings.agentMaximumSlopeDegrees * DEG2RAD);
    for (const SectorCompiledStructuralSurface& surface : primitive.surfaces) {
        uint8_t area = surface.normal.y + 0.0001f >= minimumWalkableNormalY
                ? NavigationRasterWalkableArea
                : NavigationRasterNullArea;
        if (primitive.authored.kind == SectorStructuralPrimitiveKind::Stairs
                || primitive.authored.kind == SectorStructuralPrimitiveKind::Ladder) {
            area = NavigationRasterNullArea;
        }
        for (size_t index = 0; index + 2 < surface.vertices.size(); index += 3) {
            AddTriangle(
                    input,
                    surface.vertices[index].position,
                    surface.vertices[index + 1].position,
                    surface.vertices[index + 2].position,
                    area,
                    primitive.authored.id,
                    minimum,
                    maximum);
        }
    }

    if (primitive.authored.kind != SectorStructuralPrimitiveKind::Stairs) return;
    const float halfWidth = SectorCoordToWorldDistance(primitive.authored.stairs.width) * 0.5f;
    const float halfRun = SectorCoordToWorldDistance(primitive.authored.stairs.run) * 0.5f;
    const auto corner = [&](float rightOffset, float forwardOffset, float authoredY) {
        return TransformSectorStructuralPrimitivePoint(
                primitive.authored, rightOffset, authoredY, forwardOffset);
    };
    const float low = primitive.authored.stairs.bottom;
    const float high = primitive.authored.stairs.bottom
            + primitive.authored.stairs.rise;
    const Vector3 a = corner(-halfWidth, -halfRun, low);
    const Vector3 b = corner(halfWidth, -halfRun, low);
    const Vector3 c = corner(halfWidth, halfRun, high);
    const Vector3 d = corner(-halfWidth, halfRun, high);
    const float runWorld = halfRun * 2.0f;
    const float riseWorld = SectorAuthoringToWorldDistance(
            primitive.authored.stairs.rise);
    const Vector3 proxyNormal = Vector3Normalize(
            RotateSectorStructuralPrimitiveVector(
                    primitive.authored,
                    Vector3{0.0f, runWorld, -riseWorld}));
    const uint8_t proxyArea = proxyNormal.y + 0.0001f
                    >= minimumWalkableNormalY
            ? NavigationRasterWalkableArea
            : NavigationRasterNullArea;
    AddTriangle(input, a, b, c, proxyArea,
            primitive.authored.id, minimum, maximum);
    AddTriangle(input, a, c, d, proxyArea,
            primitive.authored.id, minimum, maximum);
}

const SectorTopologySector* SectorForSide(
        const SectorTopologyMap& map,
        int sideDefId)
{
    const SectorTopologySideDef* side = FindSectorTopologySideDef(map, sideDefId);
    return side != nullptr ? FindSectorTopologySector(map, side->sectorId) : nullptr;
}

bool IsPassablePortal(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& line,
        const SectorNavigationSettings& settings)
{
    if (line.flags.blocksPlayer || line.frontSideDefId <= 0 || line.backSideDefId <= 0) {
        return false;
    }
    const SectorTopologySector* front = SectorForSide(map, line.frontSideDefId);
    const SectorTopologySector* back = SectorForSide(map, line.backSideDefId);
    if (front == nullptr || back == nullptr) return false;
    const float floorDifference = std::fabs(SectorAuthoringToWorldDistance(
            front->floorZ - back->floorZ));
    if (floorDifference > settings.agentMaximumClimb + 0.0001f) return false;
    const float openBottom = std::max(front->floorZ, back->floorZ);
    float openTop = std::numeric_limits<float>::infinity();
    if (!front->ceilingSky) openTop = std::min(openTop, front->ceilingZ);
    if (!back->ceilingSky) openTop = std::min(openTop, back->ceilingZ);
    return !std::isfinite(openTop)
            || SectorAuthoringToWorldDistance(openTop - openBottom)
                    + 0.0001f >= settings.agentHeight;
}

bool PointOnNavigationLoopSegment(
        Vector2 point,
        Vector2 start,
        Vector2 end)
{
    constexpr float Epsilon = 0.0001f;
    const float dx = end.x - start.x;
    const float dz = end.y - start.y;
    const float cross = dx * (point.y - start.y) - dz * (point.x - start.x);
    if (std::fabs(cross) > Epsilon * std::max(1.0f, std::sqrt(dx * dx + dz * dz))) {
        return false;
    }
    const float dot = (point.x - start.x) * dx + (point.y - start.y) * dz;
    return dot >= -Epsilon && dot <= dx * dx + dz * dz + Epsilon;
}

bool NavigationLoopContainsPoint(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& loop,
        Vector2 point)
{
    bool inside = false;
    for (size_t index = 0; index < loop.vertexIds.size(); ++index) {
        const SectorTopologyVertex* start = FindSectorTopologyVertex(
                map, loop.vertexIds[index]);
        const SectorTopologyVertex* end = FindSectorTopologyVertex(
                map, loop.vertexIds[(index + 1) % loop.vertexIds.size()]);
        if (start == nullptr || end == nullptr) return false;
        const Vector2 a = SectorCoordToWorldPosition2(start->x, start->y);
        const Vector2 b = SectorCoordToWorldPosition2(end->x, end->y);
        if (PointOnNavigationLoopSegment(point, a, b)) return true;
        if ((a.y > point.y) != (b.y > point.y)
            && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) {
            inside = !inside;
        }
    }
    return inside;
}

bool NavigationSectorContainsPoint(
        const SectorTopologyMap& map,
        const SectorTopologyLoopSet& loops,
        Vector2 point)
{
    if (!NavigationLoopContainsPoint(map, loops.outer, point)) return false;
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (NavigationLoopContainsPoint(map, hole, point)) return false;
    }
    return true;
}

bool DoorStageBelongsToSideRegion(
        const SectorTopologyMap& map,
        const std::unordered_map<int, SectorTopologyLoopSet>& loopsBySectorId,
        const std::unordered_map<int, std::vector<int>>& adjacency,
        int anchorSectorId,
        Vector2 stage,
        int& outContainingSectorId)
{
    outContainingSectorId = 0;
    std::vector<int> pending;
    std::unordered_set<int> visited;
    pending.reserve(map.sectors.size());
    visited.reserve(map.sectors.size());
    pending.push_back(anchorSectorId);
    visited.insert(anchorSectorId);
    for (size_t index = 0; index < pending.size(); ++index) {
        const int sectorId = pending[index];
        const auto loops = loopsBySectorId.find(sectorId);
        if (loops != loopsBySectorId.end()
            && NavigationSectorContainsPoint(map, loops->second, stage)) {
            outContainingSectorId = sectorId;
            return true;
        }
        const auto neighbors = adjacency.find(sectorId);
        if (neighbors == adjacency.end()) continue;
        for (int neighborId : neighbors->second) {
            if (visited.insert(neighborId).second) pending.push_back(neighborId);
        }
    }

    for (const auto& entry : loopsBySectorId) {
        if (NavigationSectorContainsPoint(map, entry.second, stage)) {
            outContainingSectorId = entry.first;
            break;
        }
    }
    return false;
}

void HashSettings(Fnv64& hash, const SectorNavigationSettings& settings)
{
    hash.Float(settings.agentRadius);
    hash.Float(settings.agentHeight);
    hash.Float(settings.agentMaximumClimb);
    hash.Float(settings.agentMaximumSlopeDegrees);
    hash.Float(settings.cellSize);
    hash.Float(settings.cellHeight);
    hash.Pod(settings.tileSizeCells);
    hash.Float(settings.boundsPaddingWorld);
    hash.Pod(settings.minimumRegionSizeCells);
    hash.Pod(settings.mergeRegionSizeCells);
    hash.Float(settings.maximumEdgeLengthWorld);
    hash.Float(settings.maximumSimplificationErrorCells);
    hash.Pod(settings.maximumVerticesPerPolygon);
}

} // namespace

bool BuildSectorNavigationBuildInput(
        const SectorTopologyMap& map,
        const std::vector<SectorStaticModelCollider>& resolvedStaticColliders,
        const SectorNavigationSettings& rawSettings,
        SectorNavigationBuildInput& outInput,
        std::vector<std::string>& outWarnings,
        std::string& outError)
{
    outInput = {};
    outWarnings.clear();
    outError.clear();
    const SectorNavigationSettings settings =
            NormalizeSectorNavigationSettings(rawSettings);
    outInput.sourceHash = ComputeSectorNavigationSourceHash(
            map, resolvedStaticColliders, settings);
    if (map.sectors.empty()) return true;

    const auto issues = ValidateSectorTopologyMap(map);
    const auto firstError = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == SectorTopologyValidationSeverity::Error;
    });
    if (firstError != issues.end()) {
        outError = "Navigation topology validation failed: "
                + FormatSectorTopologyValidationIssue(*firstError);
        return false;
    }

    const SectorTopologyIndexes indexes = BuildSectorTopologyIndexes(map);
    std::unordered_map<int, SectorTopologyLoopSet> loopsBySectorId;
    loopsBySectorId.reserve(map.sectors.size());
    Vector3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max()};
    Vector3 maximum{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()};
    float minimumFloor = std::numeric_limits<float>::max();
    float maximumTop = std::numeric_limits<float>::lowest();

    std::unordered_set<int> doorLineIds;
    std::vector<const SectorPlacedRuntimeObject*> objects;
    objects.reserve(map.runtimeObjects.size());
    for (const auto& object : map.runtimeObjects) objects.push_back(&object);
    std::sort(objects.begin(), objects.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->id < rhs->id;
    });
    struct ResolvedNavigationDoor {
        const SectorPlacedRuntimeObject* object = nullptr;
        SectorResolvedDoorAnchor anchor;
    };
    std::vector<ResolvedNavigationDoor> resolvedDoors;
    resolvedDoors.reserve(map.runtimeObjects.size());
    for (const SectorPlacedRuntimeObject* object : objects) {
        if (object->kind != "door") continue;
        const SectorResolvedDoorAnchor door = ResolveSectorDoorAnchor(map, object->door);
        if (!door.valid) {
            outWarnings.push_back("Door " + std::to_string(object->id)
                    + " was omitted from navigation: " + door.diagnostic);
            continue;
        }
        doorLineIds.insert(door.lineDefId);
        resolvedDoors.push_back({object, door});
    }

    std::unordered_map<int, std::vector<int>> walkableSectorAdjacency;
    walkableSectorAdjacency.reserve(map.sectors.size());
    for (const SectorTopologyLineDef* line : SortedById(map.lineDefs)) {
        if (doorLineIds.find(line->id) != doorLineIds.end()
            || !IsPassablePortal(map, *line, settings)) {
            continue;
        }
        const SectorTopologySideDef* front = FindSectorTopologySideDef(
                map, line->frontSideDefId);
        const SectorTopologySideDef* back = FindSectorTopologySideDef(
                map, line->backSideDefId);
        if (front == nullptr || back == nullptr) continue;
        walkableSectorAdjacency[front->sectorId].push_back(back->sectorId);
        walkableSectorAdjacency[back->sectorId].push_back(front->sectorId);
    }

    // Recast may merge a run of discrete treads into one sloped polygon. Give
    // neighboring sectors at different floor heights distinct walkable areas
    // so Detour retains each physical height transition as a path crossing.
    std::unordered_map<int, uint8_t> groundAreaBySectorId;
    groundAreaBySectorId.reserve(map.sectors.size());
    for (const SectorTopologySector* sector : SortedById(map.sectors)) {
        std::array<bool, SectorNavigationLastGroundVariantArea + 1> unavailable{};
        const auto adjacent = walkableSectorAdjacency.find(sector->id);
        if (adjacent != walkableSectorAdjacency.end()) {
            for (int neighborId : adjacent->second) {
                const SectorTopologySector* neighbor =
                        FindSectorTopologySector(map, neighborId);
                const auto colored = groundAreaBySectorId.find(neighborId);
                if (neighbor != nullptr && colored != groundAreaBySectorId.end()
                        && neighbor->floorZ != sector->floorZ) {
                    unavailable[colored->second] = true;
                }
            }
        }
        uint8_t area = static_cast<uint8_t>(SectorNavigationArea::Ground);
        if (unavailable[area]) {
            for (area = SectorNavigationFirstGroundVariantArea;
                    area <= SectorNavigationLastGroundVariantArea
                            && unavailable[area]; ++area) {}
            if (area > SectorNavigationLastGroundVariantArea) {
                outError = "Navigation height-transition area capacity was exceeded";
                return false;
            }
        }
        groundAreaBySectorId.emplace(sector->id, area);
    }

    for (const SectorTopologySector* sector : SortedById(map.sectors)) {
        if (!std::isfinite(sector->floorZ) || !std::isfinite(sector->ceilingZ)
            || sector->ceilingZ <= sector->floorZ) {
            outError = "Navigation sector " + std::to_string(sector->id)
                    + " has invalid floor/ceiling heights";
            return false;
        }
        minimumFloor = std::min(minimumFloor, SectorAuthoringToWorldDistance(sector->floorZ));
        maximumTop = std::max(maximumTop, SectorAuthoringToWorldDistance(
                sector->ceilingSky
                        ? sector->floorZ + SectorWorldToAuthoringDistance(settings.agentHeight + 1.0f)
                        : sector->ceilingZ));
        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(map, indexes, sector->id, loops, &loopIssues)) {
            outError = "Navigation loop extraction failed for sector "
                    + std::to_string(sector->id);
            if (!loopIssues.empty()) {
                outError += ": " + FormatSectorTopologyValidationIssue(loopIssues.front());
            }
            return false;
        }
        if (!AppendSectorSurface(map, *sector, loops, sector->floorZ, true,
                    groundAreaBySectorId.at(sector->id), outInput,
                    minimum, maximum, outError)) {
            return false;
        }
        if (!sector->ceilingSky
            && !AppendSectorSurface(map, *sector, loops, sector->ceilingZ, false,
                    NavigationRasterNullArea, outInput, minimum, maximum, outError)) {
            return false;
        }
        loopsBySectorId.emplace(sector->id, std::move(loops));
    }

    for (const ResolvedNavigationDoor& resolvedDoor : resolvedDoors) {
        const SectorPlacedRuntimeObject* object = resolvedDoor.object;
        const SectorResolvedDoorAnchor& door = resolvedDoor.anchor;
        const float bottom = door.openBottom + object->door.heightOffsetWorld;
        const float top = bottom + door.height;
        outInput.doorPlaceholders.push_back(SectorNavigationDebugDoorPlaceholder{
                object->id,
                door.lineDefId,
                {door.endpointA.x, bottom, door.endpointA.y},
                {door.endpointB.x, bottom, door.endpointB.y},
                bottom,
                top});
        const Vector2 midpoint = Vector2Scale(
                Vector2Add(door.endpointA, door.endpointB), 0.5f);
        const Vector2 normal = Vector2Normalize(door.normal);
        const float closedHalfThickness = std::max(
                settings.cellSize,
                object->door.thickness * 0.5f);
        const float swingSweep = object->door.motion == SectorDoorMotionType::Swing
                ? std::max(door.width, door.portalWidth)
                : 0.0f;
        const float baseStagingDistance = settings.agentRadius
                + closedHalfThickness
                + settings.cellSize * 2.0f;
        const float frontStagingDistance = baseStagingDistance
                + (object->door.swingSide == SectorDoorSwingSide::Front
                        ? swingSweep : 0.0f);
        const float backStagingDistance = baseStagingDistance
                + (object->door.swingSide == SectorDoorSwingSide::Back
                        ? swingSweep : 0.0f);
        const SectorNavigationBuildDoorLink link{
                object->id,
                {midpoint.x - normal.x * frontStagingDistance,
                 bottom,
                 midpoint.y - normal.y * frontStagingDistance},
                {midpoint.x + normal.x * backStagingDistance,
                 bottom,
                 midpoint.y + normal.y * backStagingDistance},
                settings.agentRadius};
        int frontStageSectorId = 0;
        int backStageSectorId = 0;
        const bool frontStageValid = DoorStageBelongsToSideRegion(
                map,
                loopsBySectorId,
                walkableSectorAdjacency,
                door.frontSectorId,
                {link.frontStage.x, link.frontStage.z},
                frontStageSectorId);
        const bool backStageValid = DoorStageBelongsToSideRegion(
                map,
                loopsBySectorId,
                walkableSectorAdjacency,
                door.backSectorId,
                {link.backStage.x, link.backStage.z},
                backStageSectorId);
        if (!frontStageValid || !backStageValid) {
            const char* side = !frontStageValid ? "front" : "back";
            const int anchorSectorId = !frontStageValid
                    ? door.frontSectorId : door.backSectorId;
            const int stageSectorId = !frontStageValid
                    ? frontStageSectorId : backStageSectorId;
            outWarnings.push_back("Door " + std::to_string(object->id)
                    + " was omitted from navigation: " + side
                    + " staging point is in "
                    + (stageSectorId > 0
                            ? "disconnected sector " + std::to_string(stageSectorId)
                            : std::string("no sector"))
                    + " rather than the walkable region from anchor sector "
                    + std::to_string(anchorSectorId));
            continue;
        }
        outInput.doorLinks.push_back(link);
    }

    for (const SectorTopologyLineDef* line : SortedById(map.lineDefs)) {
        if (IsPassablePortal(map, *line, settings)
            && doorLineIds.find(line->id) == doorLineIds.end()) {
            continue;
        }
        const SectorTopologyVertex* startVertex = nullptr;
        const SectorTopologyVertex* endVertex = nullptr;
        if (!GetSectorTopologyLineVertices(map, *line, startVertex, endVertex)
            || startVertex == nullptr || endVertex == nullptr) {
            outError = "Navigation could not resolve linedef " + std::to_string(line->id);
            return false;
        }
        const Vector2 start = SectorCoordToWorldPosition2(startVertex->x, startVertex->y);
        const Vector2 end = SectorCoordToWorldPosition2(endVertex->x, endVertex->y);
        AddBarrierPrism(start, end, minimumFloor - settings.cellHeight,
                maximumTop + settings.cellHeight, settings.cellSize, line->id,
                outInput, minimum, maximum);
    }

    std::unordered_map<int, const SectorStaticModelCollider*> collidersByObject;
    for (const SectorStaticModelCollider& collider : resolvedStaticColliders) {
        if (collider.resolved && !collider.failed) {
            collidersByObject[collider.placedObjectId] = &collider;
        }
    }
    for (const SectorPlacedRuntimeObject* object : objects) {
        const bool staticModel = object->kind == "static_model"
                && object->staticModel.collision;
        const bool window = object->kind == "window"
                && object->window.collision;
        if (!staticModel && !window) continue;
        const auto found = collidersByObject.find(object->id);
        if (found == collidersByObject.end()) {
            outWarnings.push_back(std::string("Collision-enabled ")
                    + (window ? "window " : "static model ")
                    + std::to_string(object->id)
                    + " has no resolved collider and was omitted from navigation");
            continue;
        }
        AddStaticObstacleGeometry(*found->second, outInput, minimum, maximum);
    }

    for (const SectorCompiledStructuralPrimitive& primitive
            : map.compiledStructuralPrimitives) {
        AddStructuralPrimitiveGeometry(
                primitive, settings, outInput, minimum, maximum);
    }

    if (outInput.triangles.empty()) return true;
    const float padding = settings.boundsPaddingWorld;
    minimum.x -= padding;
    minimum.y -= settings.cellHeight;
    minimum.z -= padding;
    maximum.x += padding;
    maximum.y += settings.agentHeight + settings.cellHeight;
    maximum.z += padding;
    outInput.bounds = {minimum, maximum};
    return true;
}

uint64_t ComputeSectorNavigationSourceHash(
        const SectorTopologyMap& map,
        const std::vector<SectorStaticModelCollider>& resolvedStaticColliders,
        const SectorNavigationSettings& rawSettings)
{
    Fnv64 hash;
    hash.String("SectorNavigationSourceV4");
    const SectorNavigationSettings settings = NormalizeSectorNavigationSettings(rawSettings);
    HashSettings(hash, settings);

    for (const SectorTopologyVertex* vertex : SortedById(map.vertices)) {
        hash.Pod(vertex->id); hash.Pod(vertex->x); hash.Pod(vertex->y);
    }
    for (const SectorTopologyLineDef* line : SortedById(map.lineDefs)) {
        hash.Pod(line->id); hash.Pod(line->startVertexId); hash.Pod(line->endVertexId);
        hash.Pod(line->frontSideDefId); hash.Pod(line->backSideDefId);
        hash.Pod(static_cast<uint8_t>(line->flags.blocksPlayer));
    }
    for (const SectorTopologySideDef* side : SortedById(map.sideDefs)) {
        hash.Pod(side->id); hash.Pod(side->lineDefId);
        hash.Pod(static_cast<uint8_t>(side->side)); hash.Pod(side->sectorId);
    }
    for (const SectorTopologySector* sector : SortedById(map.sectors)) {
        hash.Pod(sector->id); hash.Float(sector->floorZ); hash.Float(sector->ceilingZ);
        hash.Pod(static_cast<uint8_t>(sector->ceilingSky));
    }
    for (const SectorCompiledStructuralPrimitive& primitive
            : map.compiledStructuralPrimitives) {
        hash.String("structural_primitive");
        hash.Pod(primitive.authored.id);
        hash.Pod(static_cast<uint8_t>(primitive.authored.enabled));
        hash.Pod(static_cast<uint8_t>(primitive.authored.collision));
        hash.String(primitive.geometryFingerprint);
    }

    std::unordered_map<int, const SectorStaticModelCollider*> collidersByObject;
    for (const SectorStaticModelCollider& collider : resolvedStaticColliders) {
        if (collider.resolved && !collider.failed) {
            collidersByObject[collider.placedObjectId] = &collider;
        }
    }
    std::vector<const SectorPlacedRuntimeObject*> objects;
    for (const auto& object : map.runtimeObjects) objects.push_back(&object);
    std::sort(objects.begin(), objects.end(), [](const auto* lhs, const auto* rhs) {
        return lhs->id < rhs->id;
    });
    for (const SectorPlacedRuntimeObject* object : objects) {
        if (object->kind == "static_model" && object->staticModel.collision) {
            hash.String("static_model"); hash.Pod(object->id);
            hash.String(object->staticModel.geometryFingerprint);
            const auto found = collidersByObject.find(object->id);
            hash.Pod(static_cast<uint8_t>(found != collidersByObject.end()));
            if (found != collidersByObject.end()) {
                const SectorStaticModelCollider& c = *found->second;
                hash.Float(c.center.x); hash.Float(c.center.y);
                hash.Float(c.axisX.x); hash.Float(c.axisX.y);
                hash.Float(c.axisZ.x); hash.Float(c.axisZ.y);
                hash.Float(c.halfExtents.x); hash.Float(c.halfExtents.y);
                hash.Float(c.bottom); hash.Float(c.top);
            }
        } else if (object->kind == "door") {
            hash.String("door"); hash.Pod(object->id);
            const SectorResolvedDoorAnchor door = ResolveSectorDoorAnchor(map, object->door);
            hash.Pod(static_cast<uint8_t>(door.valid));
            hash.Pod(object->door.anchor.lineDefId);
            if (door.valid) {
                hash.Float(door.endpointA.x); hash.Float(door.endpointA.y);
                hash.Float(door.endpointB.x); hash.Float(door.endpointB.y);
                hash.Float(door.openBottom); hash.Float(door.openTop);
                hash.Float(door.width); hash.Float(door.height);
                hash.Float(object->door.thickness);
                hash.Float(object->door.heightOffsetWorld);
                hash.Pod(static_cast<uint8_t>(object->door.motion));
                hash.Pod(static_cast<uint8_t>(object->door.swingSide));
            }
        } else if (object->kind == "window" && object->window.collision) {
            hash.String("window"); hash.Pod(object->id);
            const SectorResolvedWindowAnchor window =
                    ResolveSectorWindowAnchor(map, object->window);
            hash.Pod(static_cast<uint8_t>(window.valid));
            hash.Pod(object->window.anchor.lineDefId);
            const auto found = collidersByObject.find(object->id);
            hash.Pod(static_cast<uint8_t>(found != collidersByObject.end()));
            if (window.valid) {
                hash.Float(window.endpointA.x); hash.Float(window.endpointA.y);
                hash.Float(window.endpointB.x); hash.Float(window.endpointB.y);
                hash.Float(window.openBottom); hash.Float(window.openTop);
                hash.Float(window.width); hash.Float(window.height);
                hash.Float(object->window.thickness);
                hash.Float(object->window.horizontalOffsetWorld);
                hash.Float(object->window.verticalOffsetWorld);
                hash.Float(object->window.normalOffset);
            }
        }
    }
    return hash.value;
}

std::string FormatSectorNavigationSourceHash(uint64_t hash)
{
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

} // namespace game
