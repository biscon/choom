#include "sector_demo/SectorCollisionWorld.h"

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace game {
namespace {

constexpr float CollisionPointEpsilon = 0.001f;
constexpr float CollisionMoveEpsilon = 0.0001f;

enum class StructuralFootprintEdge {
    None,
    Low,
    High,
    Side
};

enum class StructuralBlockReason {
    None,
    Side,
    Step,
    Ceiling
};

struct StructuralCollisionShape {
    SectorStructuralPrimitiveKind kind = SectorStructuralPrimitiveKind::Box;
    Vector2 center = {};
    Vector2 axisX = {1.0f, 0.0f};
    Vector2 axisZ = {0.0f, 1.0f};
    Vector2 halfExtents = {};
    float radius = 0.0f;
    float bottom = 0.0f;
    float low = 0.0f;
    float high = 0.0f;
    float sphereCenterY = 0.0f;
};

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

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

StructuralCollisionShape BuildStructuralCollisionShape(
        const SectorStructuralCollisionPrimitive& record)
{
    const SectorAuthoringStructuralPrimitive& primitive = record.authored;
    StructuralCollisionShape shape;
    shape.kind = primitive.kind;
    shape.center = SectorCoordToWorldPosition2(primitive.x, primitive.z);
    const float radians = primitive.yawDegrees * 3.14159265358979323846f / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    shape.axisX = {cosine, sine};
    shape.axisZ = {-sine, cosine};
    switch (primitive.kind) {
        case SectorStructuralPrimitiveKind::Box:
            shape.halfExtents = {
                    SectorCoordToWorldDistance(primitive.box.width) * 0.5f,
                    SectorCoordToWorldDistance(primitive.box.depth) * 0.5f};
            shape.bottom = SectorAuthoringToWorldDistance(primitive.box.bottom);
            shape.low = shape.high = SectorAuthoringToWorldDistance(primitive.box.top);
            break;
        case SectorStructuralPrimitiveKind::Ramp:
            shape.halfExtents = {
                    SectorCoordToWorldDistance(primitive.ramp.width) * 0.5f,
                    SectorCoordToWorldDistance(primitive.ramp.run) * 0.5f};
            shape.bottom = SectorAuthoringToWorldDistance(primitive.ramp.solidBottom);
            shape.low = SectorAuthoringToWorldDistance(primitive.ramp.low);
            shape.high = SectorAuthoringToWorldDistance(primitive.ramp.high);
            break;
        case SectorStructuralPrimitiveKind::Stairs:
            shape.halfExtents = {
                    SectorCoordToWorldDistance(primitive.stairs.width) * 0.5f,
                    SectorCoordToWorldDistance(primitive.stairs.run) * 0.5f};
            shape.bottom = SectorAuthoringToWorldDistance(primitive.stairs.bottom);
            shape.low = shape.bottom;
            shape.high = SectorAuthoringToWorldDistance(
                    primitive.stairs.bottom + primitive.stairs.rise);
            break;
        case SectorStructuralPrimitiveKind::Cylinder:
            shape.radius = SectorCoordToWorldDistance(primitive.cylinder.radius);
            shape.bottom = SectorAuthoringToWorldDistance(primitive.cylinder.bottom);
            shape.low = shape.high = SectorAuthoringToWorldDistance(primitive.cylinder.top);
            break;
        case SectorStructuralPrimitiveKind::Sphere:
            shape.radius = SectorCoordToWorldDistance(primitive.sphere.radius);
            shape.sphereCenterY = SectorAuthoringToWorldDistance(
                    primitive.sphere.centerHeight);
            shape.bottom = shape.sphereCenterY - shape.radius;
            shape.low = shape.high = shape.sphereCenterY + shape.radius;
            break;
        case SectorStructuralPrimitiveKind::Ladder:
            shape.halfExtents = {
                    SectorCoordToWorldDistance(primitive.ladder.width) * 0.5f,
                    SectorStructuralLadderFrameThicknessWorld
                            * primitive.ladder.thicknessScale * 0.5f};
            shape.bottom = SectorAuthoringToWorldDistance(primitive.ladder.bottom);
            shape.low = shape.high = SectorAuthoringToWorldDistance(
                    primitive.ladder.bottom + primitive.ladder.height);
            break;
    }
    return shape;
}

bool PointInsideConvexPolygon(
        Vector2 point,
        const std::vector<Vector2>& polygon)
{
    if (polygon.size() < 3) return false;
    for (size_t index = 0; index < polygon.size(); ++index) {
        const Vector2 a = polygon[index];
        const Vector2 b = polygon[(index + 1) % polygon.size()];
        const Vector2 edge{b.x - a.x, b.y - a.y};
        const Vector2 relative{point.x - a.x, point.y - a.y};
        if (edge.x * relative.y - edge.y * relative.x
                < -CollisionMoveEpsilon) {
            return false;
        }
    }
    return true;
}

bool ResolveCircleAgainstConvexPolygon(
        Vector2& position,
        float radius,
        const std::vector<Vector2>& polygon,
        Vector2& outNormal)
{
    if (polygon.size() < 3) return false;
    const bool inside = PointInsideConvexPolygon(position, polygon);
    float nearestDistanceSquared = std::numeric_limits<float>::infinity();
    Vector2 nearestPoint{};
    Vector2 nearestOutward{};
    float nearestInsideDistance = std::numeric_limits<float>::infinity();
    for (size_t index = 0; index < polygon.size(); ++index) {
        const Vector2 a = polygon[index];
        const Vector2 b = polygon[(index + 1) % polygon.size()];
        const Vector2 edge{b.x - a.x, b.y - a.y};
        const float edgeLengthSquared = edge.x * edge.x + edge.y * edge.y;
        if (!(edgeLengthSquared > CollisionMoveEpsilon)) continue;
        const float t = std::clamp(
                ((position.x - a.x) * edge.x
                        + (position.y - a.y) * edge.y) / edgeLengthSquared,
                0.0f,
                1.0f);
        const Vector2 closest{a.x + edge.x * t, a.y + edge.y * t};
        const float dx = position.x - closest.x;
        const float dy = position.y - closest.y;
        const float distanceSquared = dx * dx + dy * dy;
        if (distanceSquared < nearestDistanceSquared) {
            nearestDistanceSquared = distanceSquared;
            nearestPoint = closest;
        }
        if (inside) {
            const float edgeLength = std::sqrt(edgeLengthSquared);
            const float insideDistance =
                    (edge.x * (position.y - a.y)
                            - edge.y * (position.x - a.x)) / edgeLength;
            if (insideDistance < nearestInsideDistance) {
                nearestInsideDistance = insideDistance;
                nearestOutward = Vector2{edge.y / edgeLength, -edge.x / edgeLength};
            }
        }
    }
    if (inside) {
        if (!std::isfinite(nearestInsideDistance)) return false;
        outNormal = nearestOutward;
        const float push = radius + nearestInsideDistance + CollisionMoveEpsilon;
        position.x += outNormal.x * push;
        position.y += outNormal.y * push;
        return true;
    }
    if (!(nearestDistanceSquared < radius * radius - CollisionMoveEpsilon)) {
        return false;
    }
    const float distance = std::sqrt(std::max(nearestDistanceSquared, 0.0f));
    if (distance > CollisionMoveEpsilon) {
        outNormal = Vector2{
                (position.x - nearestPoint.x) / distance,
                (position.y - nearestPoint.y) / distance};
    } else {
        outNormal = Vector2{1.0f, 0.0f};
    }
    const float push = radius - distance + CollisionMoveEpsilon;
    position.x += outNormal.x * push;
    position.y += outNormal.y * push;
    return true;
}

bool CircleOverlapsConvexPolygon(
        Vector2 position,
        float radius,
        const std::vector<Vector2>& polygon)
{
    Vector2 normal{};
    return ResolveCircleAgainstConvexPolygon(
            position, radius, polygon, normal);
}

bool RayTriangle(
        Vector3 origin,
        Vector3 direction,
        Vector3 a,
        Vector3 b,
        Vector3 c,
        float* distance,
        Vector3* normal)
{
    const Vector3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    const Vector3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    const Vector3 p{
            direction.y * ac.z - direction.z * ac.y,
            direction.z * ac.x - direction.x * ac.z,
            direction.x * ac.y - direction.y * ac.x};
    const float determinant = ab.x * p.x + ab.y * p.y + ab.z * p.z;
    if (std::fabs(determinant) <= CollisionMoveEpsilon) return false;
    const float inverse = 1.0f / determinant;
    const Vector3 t{origin.x - a.x, origin.y - a.y, origin.z - a.z};
    const float u = (t.x * p.x + t.y * p.y + t.z * p.z) * inverse;
    if (u < 0.0f || u > 1.0f) return false;
    const Vector3 q{
            t.y * ab.z - t.z * ab.y,
            t.z * ab.x - t.x * ab.z,
            t.x * ab.y - t.y * ab.x};
    const float v = (direction.x * q.x + direction.y * q.y + direction.z * q.z)
            * inverse;
    if (v < 0.0f || u + v > 1.0f) return false;
    const float hitDistance = (ac.x * q.x + ac.y * q.y + ac.z * q.z) * inverse;
    if (hitDistance < 0.0f) return false;
    if (distance != nullptr) *distance = hitDistance;
    if (normal != nullptr) {
        const Vector3 cross{
                ab.y * ac.z - ab.z * ac.y,
                ab.z * ac.x - ab.x * ac.z,
                ab.x * ac.y - ab.y * ac.x};
        const float length = std::sqrt(
                cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
        *normal = length > CollisionMoveEpsilon
                ? Vector3{cross.x / length, cross.y / length, cross.z / length}
                : Vector3{};
    }
    return true;
}

bool CircleOverlapsTriangleBounds(
        Vector2 center,
        float radius,
        Vector3 a,
        Vector3 b,
        Vector3 c)
{
    const float minimumX = std::min({a.x, b.x, c.x});
    const float maximumX = std::max({a.x, b.x, c.x});
    const float minimumZ = std::min({a.z, b.z, c.z});
    const float maximumZ = std::max({a.z, b.z, c.z});
    const float closestX = std::clamp(center.x, minimumX, maximumX);
    const float closestZ = std::clamp(center.y, minimumZ, maximumZ);
    const float dx = center.x - closestX;
    const float dz = center.y - closestZ;
    return dx * dx + dz * dz <= radius * radius + CollisionMoveEpsilon;
}

float Cross(Vector2 a, Vector2 b)
{
    return a.x * b.y - a.y * b.x;
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

Vector2 ToStructuralLocalPoint(
        Vector2 point,
        const StructuralCollisionShape& shape)
{
    const Vector2 relative = Subtract(point, shape.center);
    return Vector2{Dot(relative, shape.axisX), Dot(relative, shape.axisZ)};
}

bool PointInsideStructuralFootprint(
        Vector2 position,
        const StructuralCollisionShape& shape);

bool CircleOverlapsStructuralFootprint(
        Vector2 position,
        float radius,
        const StructuralCollisionShape& shape)
{
    if (shape.kind == SectorStructuralPrimitiveKind::Cylinder
            || shape.kind == SectorStructuralPrimitiveKind::Sphere) {
        const float combined = radius + shape.radius;
        return DistanceSquared(position, shape.center)
                <= combined * combined + CollisionMoveEpsilon;
    }
    const Vector2 local = ToStructuralLocalPoint(position, shape);
    const Vector2 closest{
            std::clamp(local.x, -shape.halfExtents.x, shape.halfExtents.x),
            std::clamp(local.y, -shape.halfExtents.y, shape.halfExtents.y)};
    return DistanceSquared(local, closest)
            <= radius * radius + CollisionMoveEpsilon;
}

bool CirclePenetratesStructuralFootprint(
        Vector2 position,
        float radius,
        const StructuralCollisionShape& shape)
{
    if (PointInsideStructuralFootprint(position, shape)) {
        return true;
    }
    if (shape.kind == SectorStructuralPrimitiveKind::Cylinder
            || shape.kind == SectorStructuralPrimitiveKind::Sphere) {
        const float combined = std::max(
                0.0f, radius + shape.radius - CollisionMoveEpsilon);
        return DistanceSquared(position, shape.center) < combined * combined;
    }
    const Vector2 local = ToStructuralLocalPoint(position, shape);
    const Vector2 closest{
            std::clamp(local.x, -shape.halfExtents.x, shape.halfExtents.x),
            std::clamp(local.y, -shape.halfExtents.y, shape.halfExtents.y)};
    const float penetrationRadius = std::max(
            0.0f, radius - CollisionMoveEpsilon);
    return DistanceSquared(local, closest)
            < penetrationRadius * penetrationRadius;
}

float StructuralSupportHeight(
        Vector2 position,
        const StructuralCollisionShape& shape)
{
    if (shape.kind != SectorStructuralPrimitiveKind::Ramp
            && shape.kind != SectorStructuralPrimitiveKind::Stairs) {
        return shape.high;
    }
    const Vector2 local = ToStructuralLocalPoint(position, shape);
    const float run = shape.halfExtents.y * 2.0f;
    if (!(run > CollisionMoveEpsilon)) return shape.low;
    const float t = std::clamp(
            (local.y + shape.halfExtents.y) / run,
            0.0f,
            1.0f);
    return shape.low + (shape.high - shape.low) * t;
}

float SphereHorizontalRadiusForVerticalInterval(
        const StructuralCollisionShape& shape,
        float intervalBottom,
        float intervalTop)
{
    float verticalDistance = 0.0f;
    if (shape.sphereCenterY < intervalBottom) {
        verticalDistance = intervalBottom - shape.sphereCenterY;
    } else if (shape.sphereCenterY > intervalTop) {
        verticalDistance = shape.sphereCenterY - intervalTop;
    }
    if (verticalDistance >= shape.radius) return 0.0f;
    return std::sqrt(std::max(
            0.0f,
            shape.radius * shape.radius
                    - verticalDistance * verticalDistance));
}

bool ResolveCircleAgainstStructuralRectangle(
        Vector2& position,
        float radius,
        const StructuralCollisionShape& shape,
        Vector2& outNormal,
        StructuralFootprintEdge& outEdge)
{
    const Vector2 local = ToStructuralLocalPoint(position, shape);
    const Vector2 closest{
            std::clamp(local.x, -shape.halfExtents.x, shape.halfExtents.x),
            std::clamp(local.y, -shape.halfExtents.y, shape.halfExtents.y)};
    const Vector2 delta = Subtract(local, closest);
    const float distanceSquared = LengthSquared(delta);
    const float radiusSquared = radius * radius;
    Vector2 pushLocal{};
    if (distanceSquared > CollisionMoveEpsilon) {
        if (distanceSquared >= radiusSquared - CollisionMoveEpsilon) return false;
        const float distance = std::sqrt(distanceSquared);
        pushLocal = Scale(delta,
                (radius - distance + CollisionMoveEpsilon) / distance);
        if (std::fabs(delta.x) > std::fabs(delta.y)) {
            outEdge = StructuralFootprintEdge::Side;
        } else {
            outEdge = closest.y < 0.0f
                    ? StructuralFootprintEdge::Low
                    : StructuralFootprintEdge::High;
        }
    } else {
        const float overlapX = shape.halfExtents.x + radius - std::fabs(local.x);
        const float overlapZ = shape.halfExtents.y + radius - std::fabs(local.y);
        if (overlapX <= 0.0f || overlapZ <= 0.0f) return false;
        if (overlapX < overlapZ) {
            pushLocal.x = (local.x < 0.0f ? -1.0f : 1.0f)
                    * (overlapX + CollisionMoveEpsilon);
            outEdge = StructuralFootprintEdge::Side;
        } else {
            pushLocal.y = (local.y < 0.0f ? -1.0f : 1.0f)
                    * (overlapZ + CollisionMoveEpsilon);
            outEdge = local.y < 0.0f
                    ? StructuralFootprintEdge::Low
                    : StructuralFootprintEdge::High;
        }
    }
    const Vector2 pushWorld = Add(
            Scale(shape.axisX, pushLocal.x),
            Scale(shape.axisZ, pushLocal.y));
    position = Add(position, pushWorld);
    outNormal = NormalizeOrZero(pushWorld);
    return LengthSquared(outNormal) > CollisionMoveEpsilon;
}

bool ResolveCircleAgainstStructuralCircle(
        Vector2& position,
        float combinedRadius,
        const StructuralCollisionShape& shape,
        Vector2& outNormal)
{
    const Vector2 delta = Subtract(position, shape.center);
    const float distanceSquared = LengthSquared(delta);
    if (distanceSquared >= combinedRadius * combinedRadius
                    - CollisionMoveEpsilon) {
        return false;
    }
    if (distanceSquared > CollisionMoveEpsilon) {
        const float distance = std::sqrt(distanceSquared);
        outNormal = Scale(delta, 1.0f / distance);
        position = Add(position, Scale(
                outNormal,
                combinedRadius - distance + CollisionMoveEpsilon));
        return true;
    }
    outNormal = {1.0f, 0.0f};
    position.x += combinedRadius + CollisionMoveEpsilon;
    return true;
}

bool PointInsideStructuralFootprint(
        Vector2 position,
        const StructuralCollisionShape& shape)
{
    if (shape.kind == SectorStructuralPrimitiveKind::Cylinder
            || shape.kind == SectorStructuralPrimitiveKind::Sphere) {
        return DistanceSquared(position, shape.center)
                <= shape.radius * shape.radius + CollisionMoveEpsilon;
    }
    const Vector2 local = ToStructuralLocalPoint(position, shape);
    return std::fabs(local.x) <= shape.halfExtents.x + CollisionMoveEpsilon
            && std::fabs(local.y) <= shape.halfExtents.y + CollisionMoveEpsilon;
}

bool CanTraverseContinuousStructuralSupport(
        const StructuralCollisionShape& shape,
        Vector2 previousPosition,
        Vector2 candidate,
        float feetY,
        bool grounded,
        const SectorCollisionMoveConfig& config)
{
    if (!grounded
            || (shape.kind != SectorStructuralPrimitiveKind::Ramp
                    && shape.kind != SectorStructuralPrimitiveKind::Stairs)) {
        return false;
    }

    if (PointInsideStructuralFootprint(previousPosition, shape)) {
        const float previousSupport = StructuralSupportHeight(
                previousPosition, shape);
        if (std::fabs(feetY - previousSupport)
                > config.stepHeight + CollisionPointEpsilon) {
            return false;
        }
        return PointInsideStructuralFootprint(candidate, shape)
                || CircleOverlapsStructuralFootprint(
                        candidate, config.radius, shape);
    }

    if (!PointInsideStructuralFootprint(candidate, shape)) {
        return false;
    }

    const Vector2 previousLocal = ToStructuralLocalPoint(
            previousPosition, shape);
    if (previousLocal.y >= -shape.halfExtents.y + CollisionMoveEpsilon
            || std::fabs(previousLocal.x)
                    > shape.halfExtents.x + config.radius) {
        return false;
    }
    return StructuralSupportHeight(candidate, shape) - feetY
            <= config.stepHeight + CollisionPointEpsilon;
}

StructuralBlockReason StructuralBlockReasonForMove(
        const StructuralCollisionShape& shape,
        StructuralFootprintEdge edge,
        Vector2 candidate,
        float feetY,
        bool grounded,
        bool continuousTraversal,
        const SectorCollisionMoveConfig& config,
        float sectorCeiling)
{
    const float support = StructuralSupportHeight(candidate, shape);
    const float playerTop = feetY + config.playerHeight;
    if (playerTop <= shape.bottom + CollisionMoveEpsilon
            || feetY > support + CollisionMoveEpsilon) {
        return StructuralBlockReason::None;
    }

    const bool sloped = shape.kind == SectorStructuralPrimitiveKind::Ramp
            || shape.kind == SectorStructuralPrimitiveKind::Stairs;
    if (sloped && continuousTraversal) {
        return support + config.playerHeight
                        > sectorCeiling + CollisionMoveEpsilon
                ? StructuralBlockReason::Ceiling
                : StructuralBlockReason::None;
    }
    if (sloped && edge == StructuralFootprintEdge::Side) {
        return StructuralBlockReason::Side;
    }

    if (feetY >= support - CollisionMoveEpsilon) {
        return StructuralBlockReason::None;
    }

    const float rise = support - feetY;
    if (rise <= config.stepHeight + CollisionMoveEpsilon
            && grounded) {
        return support + config.playerHeight
                        > sectorCeiling + CollisionMoveEpsilon
                ? StructuralBlockReason::Ceiling
                : StructuralBlockReason::None;
    }
    return rise > CollisionMoveEpsilon
            ? StructuralBlockReason::Step
            : StructuralBlockReason::Side;
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
    Drop,
    Ceiling
};

PortalBlockReason PortalBlockReasonForMove(
        const SectorCollisionWorld& world,
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

    SectorCollisionHeights neighborHeights;
    if (!world.GetSectorFloorCeiling(edge.neighborSectorId, &neighborHeights)) {
        return PortalBlockReason::Ceiling;
    }

    if (moveState.grounded) {
        const float rise = neighborHeights.floorZ - moveState.feetY;
        if (rise > config.stepHeight + CollisionMoveEpsilon) {
            return PortalBlockReason::Step;
        }
        const float drop = moveState.feetY - neighborHeights.floorZ;
        if (config.constrainGroundedDropsToStepHeight
                && drop > config.stepHeight + CollisionMoveEpsilon) {
            return PortalBlockReason::Drop;
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

bool ShouldApplyRadiusCorrectionForBlockedEdge(
        PortalBlockReason portalReason,
        Vector2 remaining,
        Vector2 inward)
{
    if (portalReason != PortalBlockReason::Step) {
        return true;
    }

    return Dot(remaining, inward) < -CollisionMoveEpsilon;
}

bool CircleOverlapsSegment(Vector2 center, float radius, Vector2 a, Vector2 b)
{
    const Vector2 closest = ClosestPointOnSegment(center, a, b);
    const float radiusSquared = radius * radius;
    return DistanceSquared(center, closest) <= radiusSquared + CollisionMoveEpsilon;
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
    structuralPrimitives.clear();
    structuralSurfaces.clear();
    footprintTraversalSectorIds.clear();
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
        collisionSector.ceilingSolid = !sector->ceilingSky;
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
    for (const SectorCompiledStructuralPrimitive& primitive
            : map.compiledStructuralPrimitives) {
        if (!primitive.authored.enabled || !primitive.authored.collision) continue;
        SectorStructuralCollisionPrimitive collisionPrimitive;
        collisionPrimitive.authored = primitive.authored;
        collisionPrimitive.conservativeTilted =
                primitive.authored.kind != SectorStructuralPrimitiveKind::Sphere
                && SectorStructuralPrimitiveHasTilt(primitive.authored);
        if (collisionPrimitive.conservativeTilted) {
            collisionPrimitive.projectedHull =
                    BuildSectorStructuralFootprint(primitive.authored).pointsWorld;
            collisionPrimitive.minimumY = std::numeric_limits<float>::infinity();
            collisionPrimitive.maximumY = -std::numeric_limits<float>::infinity();
            for (const SectorCompiledStructuralSurface& surface
                    : primitive.surfaces) {
                for (const SectorCompiledStructuralVertex& vertex
                        : surface.vertices) {
                    collisionPrimitive.minimumY = std::min(
                            collisionPrimitive.minimumY, vertex.position.y);
                    collisionPrimitive.maximumY = std::max(
                            collisionPrimitive.maximumY, vertex.position.y);
                }
            }
            if (collisionPrimitive.projectedHull.size() < 3
                    || !std::isfinite(collisionPrimitive.minimumY)
                    || !std::isfinite(collisionPrimitive.maximumY)
                    || collisionPrimitive.maximumY
                            <= collisionPrimitive.minimumY
                                    + CollisionMoveEpsilon) {
                return SetError(errorMessage,
                        "Tilted structural primitive generated an invalid collision hull");
            }
        }
        structuralPrimitives.push_back(std::move(collisionPrimitive));
        structuralSurfaces.insert(
                structuralSurfaces.end(),
                primitive.surfaces.begin(),
                primitive.surfaces.end());
    }
    footprintTraversalSectorIds.assign(sectors.size(), 0);
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

bool SectorCollisionWorld::ResolveActorVerticalContext(
        int sectorId,
        const SectorCollisionVerticalQuery& query,
        SectorCollisionHeights* out) const
{
    if (out == nullptr || !GetSectorFloorCeiling(sectorId, out)
            || !IsFinite(query.positionXZ) || !std::isfinite(query.feetY)) {
        return false;
    }
    const float radius = std::max(query.radius, 0.0f);
    const float maximumSupport = query.feetY
            + (query.grounded ? std::max(query.stepHeight, 0.0f) : 0.0f)
            + CollisionPointEpsilon;
    for (const SectorStructuralCollisionPrimitive& primitive
            : structuralPrimitives) {
        if (primitive.conservativeTilted) {
            if (!CircleOverlapsConvexPolygon(
                        query.positionXZ,
                        radius,
                        primitive.projectedHull)) {
                continue;
            }
            if (primitive.maximumY <= maximumSupport
                    && primitive.maximumY
                            > out->floorZ + CollisionPointEpsilon) {
                out->floorZ = primitive.maximumY;
                out->continuousFloor = false;
                out->supportingStructuralPrimitiveId = primitive.authored.id;
            } else if (primitive.maximumY <= maximumSupport
                    && std::fabs(primitive.maximumY - out->floorZ)
                            <= CollisionPointEpsilon
                    && out->supportingStructuralPrimitiveId <= 0) {
                out->supportingStructuralPrimitiveId = primitive.authored.id;
            }
            if (primitive.minimumY
                    > query.feetY + CollisionPointEpsilon) {
                out->ceilingZ = std::min(
                        out->ceilingZ, primitive.minimumY);
            }
            continue;
        }
        const StructuralCollisionShape shape =
                BuildStructuralCollisionShape(primitive);
        if (!CircleOverlapsStructuralFootprint(
                    query.positionXZ, radius, shape)) {
            continue;
        }
        if (shape.kind == SectorStructuralPrimitiveKind::Sphere) {
            const float centerDistance = std::sqrt(std::max(
                    0.0f,
                    DistanceSquared(query.positionXZ, shape.center)));
            const float nearestHorizontal = std::max(
                    0.0f, centerDistance - radius);
            if (nearestHorizontal < shape.radius) {
                const float verticalExtent = std::sqrt(std::max(
                        0.0f,
                        shape.radius * shape.radius
                                - nearestHorizontal * nearestHorizontal));
                const float lowerSurface = shape.sphereCenterY - verticalExtent;
                const float upperSurface = shape.sphereCenterY + verticalExtent;
                const bool canLand = query.grounded
                        ? std::fabs(query.feetY - upperSurface)
                                <= CollisionPointEpsilon
                        : upperSurface <= query.feetY + CollisionPointEpsilon;
                if (canLand && upperSurface >= out->floorZ) {
                    out->floorZ = upperSurface;
                    out->continuousFloor = false;
                    out->supportingStructuralPrimitiveId = primitive.authored.id;
                }
                if (lowerSurface > query.feetY + CollisionPointEpsilon) {
                    out->ceilingZ = std::min(out->ceilingZ, lowerSurface);
                }
            }
            continue;
        }
        const bool centerInside = PointInsideStructuralFootprint(
                query.positionXZ, shape);
        const float support = StructuralSupportHeight(
                query.positionXZ, shape);
        float retentionTolerance = CollisionPointEpsilon;
        const bool continuous =
                shape.kind == SectorStructuralPrimitiveKind::Ramp
                || shape.kind == SectorStructuralPrimitiveKind::Stairs;
        if (continuous && !centerInside) {
            const Vector2 local = ToStructuralLocalPoint(
                    query.positionXZ, shape);
            const bool outsideEnd =
                    std::fabs(local.x)
                            <= shape.halfExtents.x + CollisionPointEpsilon
                    && std::fabs(local.y)
                            > shape.halfExtents.y + CollisionMoveEpsilon;
            if (outsideEnd) {
                retentionTolerance = std::max(query.stepHeight, 0.0f)
                        + CollisionPointEpsilon;
            }
        }
        const bool retainsSupport = !centerInside
                && query.grounded
                && std::fabs(query.feetY - support)
                        <= retentionTolerance;
        if (centerInside || retainsSupport) {
            if (retainsSupport || support <= maximumSupport) {
                if (support > out->floorZ + CollisionPointEpsilon) {
                    out->floorZ = support;
                    out->continuousFloor = continuous;
                    out->supportingStructuralPrimitiveId = primitive.authored.id;
                } else if (continuous
                        && std::fabs(support - out->floorZ)
                                <= CollisionPointEpsilon) {
                    out->continuousFloor = true;
                    if (out->supportingStructuralPrimitiveId <= 0) {
                        out->supportingStructuralPrimitiveId = primitive.authored.id;
                    }
                } else if (std::fabs(support - out->floorZ)
                                <= CollisionPointEpsilon
                        && out->supportingStructuralPrimitiveId <= 0) {
                    out->supportingStructuralPrimitiveId = primitive.authored.id;
                }
            }
        }
        if (shape.bottom > query.feetY + CollisionPointEpsilon
                && CirclePenetratesStructuralFootprint(
                        query.positionXZ, radius, shape)) {
            out->ceilingZ = std::min(out->ceilingZ, shape.bottom);
        }
    }
    return out->ceilingZ > out->floorZ;
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

int SectorCollisionWorld::FindSectorForPlayerFootprint(
        Vector2 xz,
        int currentSectorId,
        float feetY,
        bool grounded,
        const SectorCollisionMoveConfig& moveConfig) const
{
    const int pointSectorId = FindSectorContainingPointPreferCurrent(xz, currentSectorId);
    if (pointSectorId == 0 || pointSectorId == currentSectorId || !grounded) {
        return pointSectorId;
    }

    const SectorCollisionMoveConfig config = NormalizeMoveConfig(moveConfig);
    SectorCollisionHeights currentHeights;
    SectorCollisionHeights candidateHeights;
    if (!GetSectorFloorCeiling(currentSectorId, &currentHeights)
            || !GetSectorFloorCeiling(pointSectorId, &candidateHeights)) {
        return pointSectorId;
    }

    const float drop = currentHeights.floorZ - candidateHeights.floorZ;
    if (drop <= CollisionMoveEpsilon) {
        return pointSectorId;
    }
    if (std::fabs(feetY - currentHeights.floorZ) > CollisionPointEpsilon) {
        return pointSectorId;
    }
    const SectorCollisionSector* currentSector = FindSector(currentSectorId);
    return currentSector != nullptr
                    && SectorOverlapsFootprint(*currentSector, xz, config.radius)
            ? currentSectorId
            : pointSectorId;
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
    float resolvedFeetY = moveState.feetY;

    for (int substep = 0; substep < substeps; ++substep) {
        const Vector2 previousPosition = result.positionXZ;
        const int previousSectorId = result.currentSectorId;
        Vector2 candidate = Add(result.positionXZ, substepDelta);
        Vector2 remaining = substepDelta;

        for (int iteration = 0; iteration < config.maxIterations; ++iteration) {
            if (FindSector(result.currentSectorId) == nullptr
                    || footprintTraversalSectorIds.empty()) {
                break;
            }
            size_t traversalSectorCount = 1;
            footprintTraversalSectorIds[0] = result.currentSectorId;
            for (size_t traversalIndex = 0;
                    traversalIndex < traversalSectorCount;
                    ++traversalIndex) {
                const int traversalSectorId =
                        footprintTraversalSectorIds[traversalIndex];
                const std::vector<SectorCollisionEdge>* traversalEdges =
                        GetSectorEdges(traversalSectorId);
                if (traversalEdges == nullptr) {
                    continue;
                }
                for (const SectorCollisionEdge& edge : *traversalEdges) {
                    if (edge.kind != SectorCollisionEdgeKind::Portal
                            || PortalBlockReasonForMove(
                                    *this,
                                    edge,
                                    moveState,
                                    config) != PortalBlockReason::None
                            || !CircleOverlapsSegment(
                                    candidate,
                                    config.radius,
                                    edge.a,
                                    edge.b)
                            || std::find(
                                    footprintTraversalSectorIds.begin(),
                                    footprintTraversalSectorIds.begin()
                                            + traversalSectorCount,
                                    edge.neighborSectorId)
                                    != footprintTraversalSectorIds.begin()
                                            + traversalSectorCount) {
                        continue;
                    }
                    if (traversalSectorCount < footprintTraversalSectorIds.size()) {
                        footprintTraversalSectorIds[traversalSectorCount++] =
                                edge.neighborSectorId;
                    }
                }
            }

            bool changed = false;
            for (size_t collisionSectorIndex = 0;
                    collisionSectorIndex < traversalSectorCount;
                    ++collisionSectorIndex) {
                const int collisionSectorId =
                        footprintTraversalSectorIds[collisionSectorIndex];
                const std::vector<SectorCollisionEdge>* edges =
                        GetSectorEdges(collisionSectorId);
                if (edges == nullptr) {
                    continue;
                }
                for (const SectorCollisionEdge& edge : *edges) {
                    PortalBlockReason portalReason = PortalBlockReason::None;
                    bool blocking = edge.kind == SectorCollisionEdgeKind::BlockingWall;
                    if (!blocking) {
                        portalReason = PortalBlockReasonForMove(
                                *this,
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
                    if (!ShouldApplyRadiusCorrectionForBlockedEdge(portalReason, remaining, inward)) {
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
                    result.blockedByStep =
                            result.blockedByStep || portalReason == PortalBlockReason::Step;
                    result.blockedByDrop =
                            result.blockedByDrop || portalReason == PortalBlockReason::Drop;
                    result.blockedByCeiling =
                            result.blockedByCeiling || portalReason == PortalBlockReason::Ceiling;
                    changed = true;
                }
            }

            SectorCollisionHeights structuralSectorHeights;
            const int structuralSectorId = FindSectorContainingPointPreferCurrent(
                    candidate, result.currentSectorId);
            if (!GetSectorFloorCeiling(
                        structuralSectorId, &structuralSectorHeights)) {
                structuralSectorHeights.ceilingZ =
                        std::numeric_limits<float>::infinity();
            }
            for (const SectorStructuralCollisionPrimitive& primitive
                    : structuralPrimitives) {
                if (primitive.conservativeTilted) {
                    Vector2 resolved = candidate;
                    Vector2 normal{};
                    if (!ResolveCircleAgainstConvexPolygon(
                                resolved,
                                config.radius,
                                primitive.projectedHull,
                                normal)) {
                        continue;
                    }
                    const float playerTop = resolvedFeetY
                            + config.playerHeight;
                    if (playerTop
                                    <= primitive.minimumY
                                            + CollisionMoveEpsilon
                            || resolvedFeetY
                                    > primitive.maximumY
                                            + CollisionMoveEpsilon) {
                        continue;
                    }
                    StructuralBlockReason blockReason =
                            StructuralBlockReason::Step;
                    if (resolvedFeetY
                            >= primitive.maximumY - CollisionMoveEpsilon) {
                        blockReason = StructuralBlockReason::None;
                    } else if (moveState.grounded
                            && primitive.maximumY - resolvedFeetY
                                    <= config.stepHeight
                                            + CollisionPointEpsilon) {
                        blockReason = primitive.maximumY
                                                + config.playerHeight
                                        > structuralSectorHeights.ceilingZ
                                                + CollisionMoveEpsilon
                                ? StructuralBlockReason::Ceiling
                                : StructuralBlockReason::None;
                    } else if (primitive.minimumY
                            > resolvedFeetY + CollisionMoveEpsilon) {
                        blockReason = StructuralBlockReason::Ceiling;
                    }
                    if (blockReason == StructuralBlockReason::None) continue;
                    candidate = resolved;
                    const float intoSurface = Dot(remaining, normal);
                    if (intoSurface < 0.0f) {
                        remaining = Subtract(
                                remaining, Scale(normal, intoSurface));
                    }
                    result.hitWall = true;
                    result.blockedByStep = result.blockedByStep
                            || blockReason == StructuralBlockReason::Step;
                    result.blockedByCeiling = result.blockedByCeiling
                            || blockReason == StructuralBlockReason::Ceiling;
                    changed = true;
                    continue;
                }
                const StructuralCollisionShape shape =
                        BuildStructuralCollisionShape(primitive);
                Vector2 resolved = candidate;
                Vector2 normal{};
                StructuralBlockReason blockReason = StructuralBlockReason::None;
                if (shape.kind == SectorStructuralPrimitiveKind::Sphere) {
                    const float horizontalRadius =
                            SphereHorizontalRadiusForVerticalInterval(
                                    shape,
                                    resolvedFeetY,
                                    resolvedFeetY + config.playerHeight);
                    if (!(horizontalRadius > CollisionMoveEpsilon)
                            || !ResolveCircleAgainstStructuralCircle(
                                    resolved,
                                    horizontalRadius + config.radius,
                                    shape,
                                    normal)) {
                        continue;
                    }
                    blockReason = StructuralBlockReason::Side;
                } else if (shape.kind
                        == SectorStructuralPrimitiveKind::Cylinder) {
                    if (!ResolveCircleAgainstStructuralCircle(
                                resolved,
                                shape.radius + config.radius,
                                shape,
                                normal)) {
                        continue;
                    }
                    blockReason = StructuralBlockReasonForMove(
                            shape,
                            StructuralFootprintEdge::None,
                            candidate,
                            resolvedFeetY,
                            moveState.grounded,
                            false,
                            config,
                            structuralSectorHeights.ceilingZ);
                } else {
                    StructuralFootprintEdge edge =
                            StructuralFootprintEdge::None;
                    if (!ResolveCircleAgainstStructuralRectangle(
                                resolved,
                                config.radius,
                                shape,
                                normal,
                                edge)) {
                        continue;
                    }
                    blockReason = StructuralBlockReasonForMove(
                            shape,
                            edge,
                            candidate,
                            resolvedFeetY,
                            moveState.grounded,
                            CanTraverseContinuousStructuralSupport(
                                    shape,
                                    previousPosition,
                                    candidate,
                                    resolvedFeetY,
                                    moveState.grounded,
                                    config),
                            config,
                            structuralSectorHeights.ceilingZ);
                }
                if (blockReason == StructuralBlockReason::None) continue;
                candidate = resolved;
                const float intoSurface = Dot(remaining, normal);
                if (intoSurface < 0.0f) {
                    remaining = Subtract(
                            remaining, Scale(normal, intoSurface));
                }
                result.hitWall = true;
                result.blockedByStep = result.blockedByStep
                        || blockReason == StructuralBlockReason::Step;
                result.blockedByCeiling = result.blockedByCeiling
                        || blockReason == StructuralBlockReason::Ceiling;
                changed = true;
            }

            if (!changed) {
                break;
            }
        }

        float candidateFeetY = resolvedFeetY;
        bool hasContinuousSupport = false;
        for (const SectorStructuralCollisionPrimitive& primitive
                : structuralPrimitives) {
            if (primitive.conservativeTilted) continue;
            const StructuralCollisionShape shape =
                    BuildStructuralCollisionShape(primitive);
            if (CanTraverseContinuousStructuralSupport(
                        shape,
                        previousPosition,
                        candidate,
                        resolvedFeetY,
                        moveState.grounded,
                        config)) {
                const float support = StructuralSupportHeight(candidate, shape);
                candidateFeetY = hasContinuousSupport
                        ? std::max(candidateFeetY, support)
                        : support;
                hasContinuousSupport = true;
            }
        }

        const int resolvedSectorId =
                FindSectorForPlayerFootprint(
                        candidate,
                        result.currentSectorId,
                        candidateFeetY,
                        moveState.grounded,
                        config);
        if (resolvedSectorId == 0) {
            result.positionXZ = previousPosition;
            result.currentSectorId = previousSectorId;
            result.hitWall = true;
            break;
        }

        SectorCollisionVerticalQuery verticalQuery;
        verticalQuery.positionXZ = candidate;
        verticalQuery.feetY = candidateFeetY;
        verticalQuery.radius = config.radius;
        verticalQuery.actorHeight = config.playerHeight;
        verticalQuery.stepHeight = config.stepHeight;
        verticalQuery.grounded = moveState.grounded;
        SectorCollisionHeights verticalHeights;
        if (!structuralSurfaces.empty()
                && (!ResolveActorVerticalContext(resolvedSectorId, verticalQuery, &verticalHeights)
                || verticalHeights.floorZ > candidateFeetY + config.stepHeight
                        + CollisionPointEpsilon
                || verticalHeights.ceilingZ - verticalHeights.floorZ
                        < config.playerHeight - CollisionPointEpsilon)) {
            result.positionXZ = previousPosition;
            result.currentSectorId = previousSectorId;
            result.blockedByStep = true;
            break;
        }

        result.positionXZ = candidate;
        result.currentSectorId = resolvedSectorId;
        resolvedFeetY = candidateFeetY;
    }

    return result;
}

SectorCollisionRayHit SectorCollisionWorld::Raycast(
        Vector3 origin,
        Vector3 direction,
        float maximumDistance) const
{
    SectorCollisionRayHit result;
    if (!IsFinite(origin) || !IsFinite(direction)
            || !std::isfinite(maximumDistance)
            || maximumDistance <= 0.0f) {
        return result;
    }
    const float directionLength = std::sqrt(
            direction.x * direction.x
            + direction.y * direction.y
            + direction.z * direction.z);
    if (!(directionLength > CollisionMoveEpsilon)) return result;
    direction.x /= directionLength;
    direction.y /= directionLength;
    direction.z /= directionLength;
    float nearest = maximumDistance + CollisionPointEpsilon;

    const auto accept = [&result, &nearest, origin, direction](
            float distance,
            Vector3 normal,
            SectorCollisionRaySurfaceKind kind,
            int sectorId,
            int lineDefId,
            int sideDefId,
            int neighborSectorId) {
        if (!(distance >= 0.0f) || distance > nearest) return;
        if (result.hit && std::fabs(distance - nearest) <= CollisionPointEpsilon) return;
        if (normal.x * direction.x + normal.y * direction.y
                        + normal.z * direction.z > 0.0f) {
            normal = Vector3{-normal.x, -normal.y, -normal.z};
        }
        result.hit = true;
        result.distance = distance;
        result.position = Vector3{
                origin.x + direction.x * distance,
                origin.y + direction.y * distance,
                origin.z + direction.z * distance};
        result.normal = normal;
        result.surfaceKind = kind;
        result.sectorId = sectorId;
        result.lineDefId = lineDefId;
        result.sideDefId = sideDefId;
        result.neighborSectorId = neighborSectorId;
        nearest = distance;
    };

    for (const SectorCollisionSector& sector : sectors) {
        if (std::fabs(direction.y) > CollisionMoveEpsilon) {
            const float floorDistance =
                    (sector.heights.floorZ - origin.y) / direction.y;
            const Vector2 floorPoint{
                    origin.x + direction.x * floorDistance,
                    origin.z + direction.z * floorDistance};
            if (floorDistance >= 0.0f && floorDistance <= nearest
                    && SectorContainsPoint(sector, floorPoint)) {
                accept(floorDistance, Vector3{0.0f, 1.0f, 0.0f},
                        SectorCollisionRaySurfaceKind::Floor,
                        sector.sectorId, 0, 0, 0);
            }
            if (sector.ceilingSolid) {
                const float ceilingDistance =
                        (sector.heights.ceilingZ - origin.y) / direction.y;
                const Vector2 ceilingPoint{
                        origin.x + direction.x * ceilingDistance,
                        origin.z + direction.z * ceilingDistance};
                if (ceilingDistance >= 0.0f && ceilingDistance <= nearest
                        && SectorContainsPoint(sector, ceilingPoint)) {
                    accept(ceilingDistance, Vector3{0.0f, -1.0f, 0.0f},
                            SectorCollisionRaySurfaceKind::Ceiling,
                            sector.sectorId, 0, 0, 0);
                }
            }
        }

        const Vector2 rayOrigin{origin.x, origin.z};
        const Vector2 rayDirection{direction.x, direction.z};
        for (const SectorCollisionEdge& edge : sector.edges) {
            const Vector2 edgeDirection = Subtract(edge.b, edge.a);
            const float denominator = Cross(rayDirection, edgeDirection);
            if (std::fabs(denominator) <= CollisionMoveEpsilon) continue;
            const Vector2 toEdge = Subtract(edge.a, rayOrigin);
            const float distance = Cross(toEdge, edgeDirection) / denominator;
            const float edgeT = Cross(toEdge, rayDirection) / denominator;
            if (distance < 0.0f || distance > nearest
                    || edgeT < -CollisionPointEpsilon
                    || edgeT > 1.0f + CollisionPointEpsilon) {
                continue;
            }
            const float hitY = origin.y + direction.y * distance;
            if (hitY < sector.heights.floorZ - CollisionPointEpsilon
                    || hitY > sector.heights.ceilingZ + CollisionPointEpsilon) {
                continue;
            }

            SectorCollisionRaySurfaceKind kind =
                    SectorCollisionRaySurfaceKind::Wall;
            bool solid = edge.kind == SectorCollisionEdgeKind::BlockingWall;
            if (!solid) {
                const SectorCollisionSector* neighbor =
                        FindSector(edge.neighborSectorId);
                if (neighbor == nullptr) {
                    solid = true;
                } else {
                    const float openingFloor = std::max(
                            sector.heights.floorZ,
                            neighbor->heights.floorZ);
                    const float openingCeiling = std::min(
                            sector.heights.ceilingZ,
                            neighbor->heights.ceilingZ);
                    if (hitY < openingFloor - CollisionPointEpsilon) {
                        solid = true;
                        kind = SectorCollisionRaySurfaceKind::LowerWall;
                    } else if (hitY > openingCeiling + CollisionPointEpsilon) {
                        solid = true;
                        kind = SectorCollisionRaySurfaceKind::UpperWall;
                    }
                }
            }
            if (!solid) continue;
            const Vector2 inward = GetSectorCollisionEdgeInwardNormal(edge);
            accept(distance, Vector3{inward.x, 0.0f, inward.y}, kind,
                    sector.sectorId, edge.lineDefId, edge.sideDefId,
                    edge.neighborSectorId);
        }
    }
    for (const SectorCompiledStructuralSurface& surface : structuralSurfaces) {
        for (size_t index = 0; index + 2 < surface.vertices.size(); index += 3) {
            float distance = 0.0f;
            Vector3 normal{};
            if (!RayTriangle(
                        origin,
                        direction,
                        surface.vertices[index].position,
                        surface.vertices[index + 1].position,
                        surface.vertices[index + 2].position,
                        &distance,
                        &normal)
                    || distance > nearest) {
                continue;
            }
            const int sectorId = surface.owningSectorIds.empty()
                    ? 0 : surface.owningSectorIds.front();
            accept(distance, normal,
                    SectorCollisionRaySurfaceKind::StructuralPrimitive,
                    sectorId, 0, 0, 0);
            if (result.hit && std::fabs(result.distance - distance)
                    <= CollisionPointEpsilon) {
                result.structuralFace = surface.face;
            }
        }
    }
    return result;
}

bool SectorCollisionWorld::AllowsPrismPlacement(
        Vector2 center,
        float radius,
        float bottom,
        float top,
        int preferredSectorId,
        int* resolvedSectorId,
        int ignoredStructuralPrimitiveId,
        int ignoredSupportingStructuralPrimitiveId) const
{
    if (resolvedSectorId != nullptr) *resolvedSectorId = 0;
    if (!IsFinite(center) || !std::isfinite(radius)
            || !std::isfinite(bottom) || !std::isfinite(top)
            || radius <= 0.0f || top <= bottom
            || footprintTraversalSectorIds.empty()) {
        return false;
    }
    const int startSectorId = FindSectorContainingPointPreferCurrent(
            center, preferredSectorId);
    const SectorCollisionSector* start = FindSector(startSectorId);
    if (start == nullptr) return false;
    if (resolvedSectorId != nullptr) *resolvedSectorId = startSectorId;
    for (const SectorCompiledStructuralSurface& surface : structuralSurfaces) {
        if ((ignoredStructuralPrimitiveId > 0
                    && surface.face.primitiveId == ignoredStructuralPrimitiveId)
                || (ignoredSupportingStructuralPrimitiveId > 0
                    && surface.face.primitiveId
                            == ignoredSupportingStructuralPrimitiveId)) {
            continue;
        }
        for (size_t index = 0; index + 2 < surface.vertices.size(); index += 3) {
            const Vector3 a = surface.vertices[index].position;
            const Vector3 b = surface.vertices[index + 1].position;
            const Vector3 c = surface.vertices[index + 2].position;
            const float minimumY = std::min({a.y, b.y, c.y});
            const float maximumY = std::max({a.y, b.y, c.y});
            if (top <= minimumY + CollisionPointEpsilon
                    || bottom >= maximumY - CollisionPointEpsilon
                    || !CircleOverlapsTriangleBounds(center, radius, a, b, c)) {
                continue;
            }
            return false;
        }
    }
    size_t traversalCount = 1;
    footprintTraversalSectorIds[0] = startSectorId;
    for (size_t traversalIndex = 0;
            traversalIndex < traversalCount;
            ++traversalIndex) {
        const SectorCollisionSector* sector = FindSector(
                footprintTraversalSectorIds[traversalIndex]);
        if (sector == nullptr
                || bottom < sector->heights.floorZ - CollisionPointEpsilon
                || (sector->ceilingSolid
                        && top > sector->heights.ceilingZ
                                + CollisionPointEpsilon)) {
            return false;
        }
        for (const SectorCollisionEdge& edge : sector->edges) {
            if (!CircleOverlapsSegment(center, radius, edge.a, edge.b)) {
                continue;
            }
            if (edge.kind == SectorCollisionEdgeKind::BlockingWall
                    || edge.blocksPlayer) {
                return false;
            }
            const SectorCollisionSector* neighbor = FindSector(
                    edge.neighborSectorId);
            if (neighbor == nullptr) return false;
            const float openingBottom = std::max(
                    sector->heights.floorZ, neighbor->heights.floorZ);
            float openingTop = std::numeric_limits<float>::infinity();
            if (sector->ceilingSolid) {
                openingTop = std::min(openingTop, sector->heights.ceilingZ);
            }
            if (neighbor->ceilingSolid) {
                openingTop = std::min(openingTop, neighbor->heights.ceilingZ);
            }
            if (bottom < openingBottom - CollisionPointEpsilon
                    || top > openingTop + CollisionPointEpsilon) {
                return false;
            }
            if (std::find(
                        footprintTraversalSectorIds.begin(),
                        footprintTraversalSectorIds.begin() + traversalCount,
                        neighbor->sectorId)
                    == footprintTraversalSectorIds.begin() + traversalCount) {
                if (traversalCount >= footprintTraversalSectorIds.size()) {
                    return false;
                }
                footprintTraversalSectorIds[traversalCount++] =
                        neighbor->sectorId;
            }
        }
    }
    return true;
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

bool SectorCollisionWorld::SectorOverlapsFootprint(
        const SectorCollisionSector& sector,
        Vector2 xz,
        float radius) const
{
    if (SectorContainsPoint(sector, xz)) {
        return true;
    }

    const auto overlapsLoop = [&](const SectorCollisionLoop& loop) {
        for (size_t index = 0; index < loop.points.size(); ++index) {
            if (CircleOverlapsSegment(
                        xz,
                        radius,
                        loop.points[index],
                        loop.points[(index + 1) % loop.points.size()])) {
                return true;
            }
        }
        return false;
    };
    if (overlapsLoop(sector.outerLoop)) {
        return true;
    }
    for (const SectorCollisionLoop& hole : sector.holeLoops) {
        if (overlapsLoop(hole)) {
            return true;
        }
    }
    return false;
}

} // namespace game
