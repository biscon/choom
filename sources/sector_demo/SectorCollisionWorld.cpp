#include "sector_demo/SectorCollisionWorld.h"

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace game {
namespace {

constexpr float CollisionPointEpsilon = 0.001f;
constexpr float CollisionMoveEpsilon = 0.0001f;

enum class PointLoopContainment {
    Outside,
    Inside,
    Boundary
};

bool SetError(std::string* errorMessage, const std::string& message)
{
    if (errorMessage != nullptr) {
        *errorMessage = message;
    }
    return false;
}

bool IsFinite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

float DistanceSquared(Vector2 a, Vector2 b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    return dx * dx + dy * dy;
}

float Dot(Vector2 a, Vector2 b)
{
    return a.x * b.x + a.y * b.y;
}

float LengthSquared(Vector2 value)
{
    return Dot(value, value);
}

float Length(Vector2 value)
{
    return std::sqrt(LengthSquared(value));
}

Vector2 Add(Vector2 a, Vector2 b)
{
    return Vector2{a.x + b.x, a.y + b.y};
}

Vector2 Subtract(Vector2 a, Vector2 b)
{
    return Vector2{a.x - b.x, a.y - b.y};
}

Vector2 Scale(Vector2 value, float scale)
{
    return Vector2{value.x * scale, value.y * scale};
}

Vector2 NormalizeOrZero(Vector2 value)
{
    const float length = Length(value);
    if (!(length > CollisionMoveEpsilon) || !std::isfinite(length)) {
        return Vector2{};
    }
    return Scale(value, 1.0f / length);
}

Vector2 ClosestPointOnSegment(Vector2 point, Vector2 a, Vector2 b)
{
    const float lengthSquared = DistanceSquared(a, b);
    if (!(lengthSquared > 0.0f) || !std::isfinite(lengthSquared)) {
        return a;
    }

    const float t = std::clamp(
            Dot(Subtract(point, a), Subtract(b, a)) / lengthSquared,
            0.0f,
            1.0f);
    return Add(a, Scale(Subtract(b, a), t));
}

bool PointNearSegment(Vector2 point, Vector2 a, Vector2 b, float epsilon)
{
    const float lengthSquared = DistanceSquared(a, b);
    if (!(lengthSquared > 0.0f) || !std::isfinite(lengthSquared)) {
        return false;
    }

    const float t = std::clamp(
            ((point.x - a.x) * (b.x - a.x) + (point.y - a.y) * (b.y - a.y)) / lengthSquared,
            0.0f,
            1.0f);
    const Vector2 closest{
            a.x + (b.x - a.x) * t,
            a.y + (b.y - a.y) * t
    };
    return DistanceSquared(point, closest) <= epsilon * epsilon;
}

PointLoopContainment ClassifyPointInLoop(
        const SectorCollisionLoop& loop,
        Vector2 point,
        float epsilon)
{
    if (loop.points.size() < 3 || !IsFinite(point)) {
        return PointLoopContainment::Outside;
    }

    bool inside = false;
    for (size_t i = 0; i < loop.points.size(); ++i) {
        const Vector2 a = loop.points[i];
        const Vector2 b = loop.points[(i + 1) % loop.points.size()];
        if (!IsFinite(a) || !IsFinite(b)) {
            return PointLoopContainment::Outside;
        }
        if (PointNearSegment(point, a, b, epsilon)) {
            return PointLoopContainment::Boundary;
        }

        if ((a.y > point.y) != (b.y > point.y)) {
            const double xIntersection =
                    static_cast<double>(a.x)
                    + (static_cast<double>(point.y) - static_cast<double>(a.y))
                            * (static_cast<double>(b.x) - static_cast<double>(a.x))
                            / (static_cast<double>(b.y) - static_cast<double>(a.y));
            if (static_cast<double>(point.x) < xIntersection) {
                inside = !inside;
            }
        }
    }
    return inside ? PointLoopContainment::Inside : PointLoopContainment::Outside;
}

const SectorTopologyValidationIssue* FirstValidationError(
        const std::vector<SectorTopologyValidationIssue>& issues)
{
    const auto found = std::find_if(issues.begin(), issues.end(), [](const auto& issue) {
        return issue.severity == SectorTopologyValidationSeverity::Error;
    });
    return found == issues.end() ? nullptr : &(*found);
}

bool AppendWorldLoop(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& topologyLoop,
        SectorCollisionLoop& outLoop,
        std::string* errorMessage,
        int sectorId)
{
    outLoop = {};
    outLoop.points.reserve(topologyLoop.vertexIds.size());
    outLoop.vertexIds.reserve(topologyLoop.vertexIds.size());
    outLoop.sideDefIds = topologyLoop.sideDefIds;

    for (int vertexId : topologyLoop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, vertexId);
        if (vertex == nullptr) {
            return SetError(
                    errorMessage,
                    "Collision sector " + std::to_string(sectorId)
                            + " loop references missing vertex " + std::to_string(vertexId));
        }
        const Vector2 world = SectorCoordToWorldPosition2(vertex->x, vertex->y);
        if (!IsFinite(world)) {
            return SetError(
                    errorMessage,
                    "Collision sector " + std::to_string(sectorId)
                            + " loop contains non-finite world coordinates");
        }
        outLoop.points.push_back(world);
        outLoop.vertexIds.push_back(vertexId);
    }
    return true;
}

bool AddEdgeFromLoopEdge(
        const SectorTopologyMap& map,
        const SectorTopologyLoopEdge& loopEdge,
        int sectorId,
        SectorCollisionSector& collisionSector,
        std::string* errorMessage)
{
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, loopEdge.lineDefId);
    if (lineDef == nullptr) {
        return SetError(
                errorMessage,
                "Collision sector " + std::to_string(sectorId)
                        + " references missing linedef " + std::to_string(loopEdge.lineDefId));
    }
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, loopEdge.sideDefId);
    if (sideDef == nullptr) {
        return SetError(
                errorMessage,
                "Collision sector " + std::to_string(sectorId)
                        + " references missing sidedef " + std::to_string(loopEdge.sideDefId));
    }
    const SectorTopologyVertex* start = FindSectorTopologyVertex(map, loopEdge.startVertexId);
    const SectorTopologyVertex* end = FindSectorTopologyVertex(map, loopEdge.endVertexId);
    if (start == nullptr || end == nullptr) {
        return SetError(
                errorMessage,
                "Collision linedef " + std::to_string(loopEdge.lineDefId)
                        + " references missing boundary vertices");
    }

    SectorCollisionEdge edge;
    edge.a = SectorCoordToWorldPosition2(start->x, start->y);
    edge.b = SectorCoordToWorldPosition2(end->x, end->y);
    edge.lineDefId = lineDef->id;
    edge.sideDefId = sideDef->id;
    edge.sectorId = sectorId;
    edge.blocksPlayer = lineDef->flags.blocksPlayer;
    if (!IsFinite(edge.a) || !IsFinite(edge.b)
        || DistanceSquared(edge.a, edge.b) <= CollisionPointEpsilon * CollisionPointEpsilon) {
        return SetError(
                errorMessage,
                "Collision linedef " + std::to_string(lineDef->id)
                        + " has zero or invalid world length");
    }

    const int oppositeSideDefId = sideDef->side == SectorTopologySideKind::Front
            ? lineDef->backSideDefId
            : lineDef->frontSideDefId;
    if (oppositeSideDefId == -1) {
        edge.kind = SectorCollisionEdgeKind::BlockingWall;
        edge.neighborSectorId = 0;
    } else {
        const SectorTopologySideDef* opposite =
                FindOppositeSectorTopologySideDef(map, sideDef->id);
        if (opposite == nullptr) {
            return SetError(
                    errorMessage,
                    "Collision portal sidedef " + std::to_string(sideDef->id)
                            + " is missing a valid opposite sidedef");
        }
        if (FindSectorTopologySector(map, opposite->sectorId) == nullptr) {
            return SetError(
                    errorMessage,
                    "Collision portal sidedef " + std::to_string(sideDef->id)
                            + " references missing opposite sector "
                            + std::to_string(opposite->sectorId));
        }
        edge.kind = SectorCollisionEdgeKind::Portal;
        edge.neighborSectorId = opposite->sectorId;
        collisionSector.portalNeighbors.push_back(opposite->sectorId);
    }

    collisionSector.edges.push_back(edge);
    return true;
}

bool AddEdgesFromLoop(
        const SectorTopologyMap& map,
        const SectorTopologyLoop& loop,
        int sectorId,
        SectorCollisionSector& collisionSector,
        std::string* errorMessage)
{
    for (const SectorTopologyLoopEdge& edge : loop.edges) {
        if (!AddEdgeFromLoopEdge(map, edge, sectorId, collisionSector, errorMessage)) {
            return false;
        }
    }
    return true;
}

SectorCollisionMoveConfig NormalizeMoveConfig(SectorCollisionMoveConfig config)
{
    if (!std::isfinite(config.radius)) {
        config.radius = 0.25f;
    }
    if (!std::isfinite(config.playerHeight)) {
        config.playerHeight = 1.6f;
    }
    if (!std::isfinite(config.stepHeight)) {
        config.stepHeight = 0.25f;
    }
    config.radius = std::clamp(config.radius, 0.001f, 64.0f);
    config.playerHeight = std::clamp(config.playerHeight, 0.001f, 64.0f);
    config.stepHeight = std::clamp(config.stepHeight, 0.0f, 64.0f);
    config.maxIterations = std::clamp(config.maxIterations, 1, 16);
    return config;
}

enum class PortalBlockReason {
    None,
    PlayerFlag,
    Step,
    Ceiling
};

PortalBlockReason PortalBlockReasonForMove(
        const SectorCollisionWorld& world,
        int currentSectorId,
        const SectorCollisionEdge& edge,
        const SectorCollisionMoveState& moveState,
        const SectorCollisionMoveConfig& config)
{
    if (edge.kind == SectorCollisionEdgeKind::BlockingWall) {
        return PortalBlockReason::None;
    }
    if (edge.blocksPlayer) {
        return PortalBlockReason::PlayerFlag;
    }

    SectorCollisionHeights currentHeights;
    SectorCollisionHeights neighborHeights;
    if (!world.GetSectorFloorCeiling(currentSectorId, &currentHeights)
        || !world.GetSectorFloorCeiling(edge.neighborSectorId, &neighborHeights)) {
        return PortalBlockReason::Ceiling;
    }

    if (moveState.grounded) {
        const float floorDelta = neighborHeights.floorZ - currentHeights.floorZ;
        if (floorDelta > config.stepHeight + CollisionMoveEpsilon) {
            return PortalBlockReason::Step;
        }
        if (neighborHeights.floorZ + config.playerHeight
            > neighborHeights.ceilingZ + CollisionMoveEpsilon) {
            return PortalBlockReason::Ceiling;
        }
        return PortalBlockReason::None;
    }

    if (moveState.feetY + CollisionMoveEpsilon < neighborHeights.floorZ) {
        return PortalBlockReason::Step;
    }
    if (moveState.feetY + config.playerHeight
        > neighborHeights.ceilingZ + CollisionMoveEpsilon) {
        return PortalBlockReason::Ceiling;
    }
    return PortalBlockReason::None;
}

} // namespace

Vector2 GetSectorCollisionEdgeInwardNormal(const SectorCollisionEdge& edge)
{
    const Vector2 d = Subtract(edge.b, edge.a);
    return NormalizeOrZero(Vector2{-d.y, d.x});
}

bool SectorCollisionWorld::BuildFromTopology(
        const SectorTopologyMap& map,
        std::string* errorMessage)
{
    sectors.clear();
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    const std::vector<SectorTopologyValidationIssue> issues = ValidateSectorTopologyMap(map);
    if (const SectorTopologyValidationIssue* issue = FirstValidationError(issues)) {
        return SetError(
                errorMessage,
                "Topology validation failed: " + FormatSectorTopologyValidationIssue(*issue));
    }

    std::vector<const SectorTopologySector*> sortedSectors;
    sortedSectors.reserve(map.sectors.size());
    for (const SectorTopologySector& sector : map.sectors) {
        sortedSectors.push_back(&sector);
    }
    std::sort(sortedSectors.begin(), sortedSectors.end(), [](const auto* first, const auto* second) {
        return first->id < second->id;
    });

    std::vector<SectorCollisionSector> builtSectors;
    builtSectors.reserve(sortedSectors.size());
    for (const SectorTopologySector* sector : sortedSectors) {
        if (!std::isfinite(sector->floorZ) || !std::isfinite(sector->ceilingZ)) {
            return SetError(
                    errorMessage,
                    "Collision sector " + std::to_string(sector->id)
                            + " has non-finite floor or ceiling height");
        }
        if (!(sector->ceilingZ > sector->floorZ)) {
            return SetError(
                    errorMessage,
                    "Collision sector " + std::to_string(sector->id)
                            + " must have ceilingZ greater than floorZ");
        }

        SectorTopologyLoopSet loops;
        std::vector<SectorTopologyValidationIssue> loopIssues;
        if (!ExtractSectorTopologyLoops(map, sector->id, loops, &loopIssues)) {
            const std::string detail = loopIssues.empty()
                    ? "unknown loop extraction failure"
                    : FormatSectorTopologyValidationIssue(loopIssues.front());
            return SetError(
                    errorMessage,
                    "Failed to extract collision loops for sector "
                            + std::to_string(sector->id) + ": " + detail);
        }

        SectorCollisionSector collisionSector;
        collisionSector.sectorId = sector->id;
        // Topology stores authored heights; collision/runtime Y matches rendered world-space geometry.
        collisionSector.heights = SectorCollisionHeights{
                SectorAuthoringToWorldDistance(sector->floorZ),
                SectorAuthoringToWorldDistance(sector->ceilingZ)};
        if (!AppendWorldLoop(map, loops.outer, collisionSector.outerLoop, errorMessage, sector->id)) {
            return false;
        }
        collisionSector.holeLoops.reserve(loops.holes.size());
        for (const SectorTopologyLoop& hole : loops.holes) {
            SectorCollisionLoop collisionHole;
            if (!AppendWorldLoop(map, hole, collisionHole, errorMessage, sector->id)) {
                return false;
            }
            collisionSector.holeLoops.push_back(std::move(collisionHole));
        }

        if (!AddEdgesFromLoop(map, loops.outer, sector->id, collisionSector, errorMessage)) {
            return false;
        }
        for (const SectorTopologyLoop& hole : loops.holes) {
            if (!AddEdgesFromLoop(map, hole, sector->id, collisionSector, errorMessage)) {
                return false;
            }
        }

        std::sort(collisionSector.portalNeighbors.begin(), collisionSector.portalNeighbors.end());
        collisionSector.portalNeighbors.erase(
                std::unique(
                        collisionSector.portalNeighbors.begin(),
                        collisionSector.portalNeighbors.end()),
                collisionSector.portalNeighbors.end());
        builtSectors.push_back(std::move(collisionSector));
    }

    sectors = std::move(builtSectors);
    return true;
}

const SectorCollisionSector* SectorCollisionWorld::FindSector(int sectorId) const
{
    const auto found = std::lower_bound(
            sectors.begin(),
            sectors.end(),
            sectorId,
            [](const SectorCollisionSector& sector, int id) {
                return sector.sectorId < id;
            });
    return found != sectors.end() && found->sectorId == sectorId ? &(*found) : nullptr;
}

bool SectorCollisionWorld::GetSectorFloorCeiling(
        int sectorId,
        SectorCollisionHeights* out) const
{
    const SectorCollisionSector* sector = FindSector(sectorId);
    if (sector == nullptr || out == nullptr) {
        return false;
    }
    *out = sector->heights;
    return true;
}

const std::vector<SectorCollisionEdge>* SectorCollisionWorld::GetSectorEdges(
        int sectorId) const
{
    const SectorCollisionSector* sector = FindSector(sectorId);
    return sector == nullptr ? nullptr : &sector->edges;
}

const std::vector<int>* SectorCollisionWorld::GetPortalNeighbors(int sectorId) const
{
    const SectorCollisionSector* sector = FindSector(sectorId);
    return sector == nullptr ? nullptr : &sector->portalNeighbors;
}

int SectorCollisionWorld::FindSectorContainingPoint(Vector2 xz) const
{
    if (!IsFinite(xz)) {
        return 0;
    }

    for (const SectorCollisionSector& sector : sectors) {
        if (SectorContainsPoint(sector, xz)) {
            return sector.sectorId;
        }
    }
    return 0;
}

int SectorCollisionWorld::FindSectorContainingPointPreferCurrent(
        Vector2 xz,
        int currentSectorId) const
{
    if (!IsFinite(xz)) {
        return 0;
    }

    const SectorCollisionSector* current = FindSector(currentSectorId);
    if (current != nullptr) {
        if (SectorContainsPoint(*current, xz)) {
            return current->sectorId;
        }
        for (int neighborSectorId : current->portalNeighbors) {
            const SectorCollisionSector* neighbor = FindSector(neighborSectorId);
            if (neighbor != nullptr && SectorContainsPoint(*neighbor, xz)) {
                return neighbor->sectorId;
            }
        }
    }

    return FindSectorContainingPoint(xz);
}

SectorCollisionMoveResult SectorCollisionWorld::ResolveMovement(
        const SectorCollisionMoveState& moveState,
        Vector2 desiredDelta,
        const SectorCollisionMoveConfig& moveConfig) const
{
    const SectorCollisionMoveConfig config = NormalizeMoveConfig(moveConfig);
    SectorCollisionMoveResult result;
    result.positionXZ = moveState.positionXZ;
    result.currentSectorId = moveState.currentSectorId;

    if (!IsFinite(moveState.positionXZ) || !IsFinite(desiredDelta)) {
        return result;
    }

    if (FindSector(result.currentSectorId) == nullptr) {
        result.currentSectorId = FindSectorContainingPoint(moveState.positionXZ);
    }
    if (result.currentSectorId == 0) {
        return result;
    }

    const float desiredLength = Length(desiredDelta);
    if (!(desiredLength > CollisionMoveEpsilon)) {
        return result;
    }

    const float maxSubstep = std::max(config.radius * 0.5f, 0.05f);
    const int substeps = std::clamp(
            static_cast<int>(std::ceil(desiredLength / maxSubstep)),
            1,
            64);
    const Vector2 substepDelta = Scale(desiredDelta, 1.0f / static_cast<float>(substeps));

    for (int substep = 0; substep < substeps; ++substep) {
        const Vector2 previousPosition = result.positionXZ;
        const int previousSectorId = result.currentSectorId;
        Vector2 candidate = Add(result.positionXZ, substepDelta);
        Vector2 remaining = substepDelta;

        for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
            const std::vector<SectorCollisionEdge>* edges = GetSectorEdges(result.currentSectorId);
            if (edges == nullptr) {
                break;
            }

            bool changed = false;
            for (const SectorCollisionEdge& edge : *edges) {
                PortalBlockReason portalReason = PortalBlockReason::None;
                bool blocking = edge.kind == SectorCollisionEdgeKind::BlockingWall;
                if (!blocking) {
                    portalReason = PortalBlockReasonForMove(
                            *this,
                            result.currentSectorId,
                            edge,
                            moveState,
                            config);
                    blocking = portalReason != PortalBlockReason::None;
                }
                if (!blocking) {
                    continue;
                }

                const Vector2 closest = ClosestPointOnSegment(candidate, edge.a, edge.b);
                const Vector2 separation = Subtract(candidate, closest);
                const float distanceSquared = LengthSquared(separation);
                const float radiusSquared = config.radius * config.radius;
                if (distanceSquared >= radiusSquared - CollisionMoveEpsilon) {
                    continue;
                }

                Vector2 normal = NormalizeOrZero(separation);
                const Vector2 inward = GetSectorCollisionEdgeInwardNormal(edge);
                if (Dot(normal, inward) < 0.0f) {
                    normal = inward;
                }
                if (LengthSquared(normal) <= CollisionMoveEpsilon) {
                    normal = inward;
                }
                if (LengthSquared(normal) <= CollisionMoveEpsilon) {
                    continue;
                }

                const float distance = std::sqrt(std::max(distanceSquared, 0.0f));
                const float penetration = config.radius - distance + CollisionMoveEpsilon;
                candidate = Add(candidate, Scale(normal, penetration));
                const float intoWall = Dot(remaining, normal);
                if (intoWall < 0.0f) {
                    remaining = Subtract(remaining, Scale(normal, intoWall));
                }
                result.hitWall = result.hitWall
                        || edge.kind == SectorCollisionEdgeKind::BlockingWall
                        || portalReason == PortalBlockReason::PlayerFlag;
                result.blockedByStep = result.blockedByStep || portalReason == PortalBlockReason::Step;
                result.blockedByCeiling =
                        result.blockedByCeiling || portalReason == PortalBlockReason::Ceiling;
                changed = true;
            }

            if (!changed) {
                break;
            }
        }

        const int resolvedSectorId =
                FindSectorContainingPointPreferCurrent(candidate, result.currentSectorId);
        if (resolvedSectorId == 0) {
            result.positionXZ = previousPosition;
            result.currentSectorId = previousSectorId;
            result.hitWall = true;
            break;
        }

        result.positionXZ = candidate;
        result.currentSectorId = resolvedSectorId;
    }

    return result;
}

bool SectorCollisionWorld::SectorContainsPoint(
        const SectorCollisionSector& sector,
        Vector2 xz) const
{
    const PointLoopContainment outer =
            ClassifyPointInLoop(sector.outerLoop, xz, CollisionPointEpsilon);
    if (outer == PointLoopContainment::Outside) {
        return false;
    }

    for (const SectorCollisionLoop& hole : sector.holeLoops) {
        const PointLoopContainment holeContainment =
                ClassifyPointInLoop(hole, xz, CollisionPointEpsilon);
        if (holeContainment != PointLoopContainment::Outside) {
            return false;
        }
    }
    return true;
}

} // namespace game
