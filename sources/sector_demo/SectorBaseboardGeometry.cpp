#include "sector_demo/SectorBaseboardGeometry.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace game {
namespace {

constexpr float Epsilon = 0.000001f;
constexpr float MiterLimit = 4.0f;

struct Board {
    const SectorTopologySideDef* side = nullptr;
    Vector2 a{}, b{}, direction{}, inward{};
    Vector2 startJoint{}, startFront{}, endJoint{}, endFront{};
    float length = 0, thickness = 0, bottom = 0, top = 0;
    float startNeighborTop = 0, endNeighborTop = 0;
    float startTrim = 0, endTrim = 0;
    bool enabled = false;
};

float Cross(Vector2 a, Vector2 b) { return a.x * b.y - a.y * b.x; }
Vector2 Add(Vector2 a, Vector2 b) { return Vector2Add(a, b); }
Vector2 Sub(Vector2 a, Vector2 b) { return Vector2Subtract(a, b); }
Vector2 Mul(Vector2 a, float value) { return Vector2Scale(a, value); }

void Join(Board& previous, Board& next)
{
    if (!previous.enabled || !next.enabled || previous.endTrim > 0 || next.startTrim > 0) return;
    const Vector2 vertex = previous.b;
    const float turn = Cross(previous.direction, next.direction);
    const float alignment = Vector2DotProduct(previous.direction, next.direction);
    const Vector2 p = Add(vertex, Mul(previous.inward, previous.thickness));
    const Vector2 q = Add(vertex, Mul(next.inward, next.thickness));
    Vector2 joint{};
    if (std::fabs(turn) <= Epsilon && alignment > 0) {
        // Shared portion of a straight joint; extra thickness gets an exposed cap.
        joint = Add(vertex, Mul(previous.inward,
                std::min(previous.thickness, next.thickness)));
    } else if (std::fabs(turn) > Epsilon) {
        const float distance = Cross(Sub(q, p), next.direction) / turn;
        joint = Add(p, Mul(previous.direction, distance));
        const float nextDistance = Vector2DotProduct(Sub(joint, q), next.direction);
        const bool withinLength = distance > -previous.length * 0.45f
                && nextDistance < next.length * 0.45f;
        const bool bounded = Vector2Distance(vertex, joint)
                <= MiterLimit * std::max(previous.thickness, next.thickness);
        if (!bounded && turn < 0) {
            // Exterior corner: split a bounded bevel between the two owners.
            joint = Mul(Add(p, q), 0.5f);
        } else if (!bounded || !withinLength) {
            // End both boards before the offset-line intersection. Their square
            // caps cannot overlap; a short span that cannot retain length is omitted.
            previous.endTrim = std::max(0.0f, -distance) + Epsilon;
            next.startTrim = std::max(0.0f, nextDistance) + Epsilon;
            if (previous.endTrim >= previous.length * 0.45f) previous.enabled = false;
            if (next.startTrim >= next.length * 0.45f) next.enabled = false;
            std::fprintf(stderr,
                    "WARNING: Baseboard join at sidedefs %d/%d cannot fit a miter; spans shortened/capped (exhausted spans omitted).\n",
                    previous.side->id, next.side->id);
            return;
        } else {
            previous.endFront = next.startFront = joint;
        }
    } else {
        previous.enabled = next.enabled = false;
        std::fprintf(stderr, "WARNING: Baseboard reversing join at sidedef %d omitted.\n",
                previous.side->id);
        return;
    }
    previous.endJoint = next.startJoint = joint;
    previous.endNeighborTop = next.top;
    next.startNeighborTop = previous.top;
}

SectorGeneratedSurface Surface(const Board& board, const SectorTopologySector& sector,
        int face, Vector3 normal)
{
    SectorGeneratedSurface surface;
    surface.ref.sourceKind = SectorGeneratedSurfaceSourceKind::Baseboard;
    surface.ref.kind = SectorGeneratedSurfaceKind::Wall;
    surface.ref.topologySectorId = sector.id;
    surface.ref.topologySideDefId = board.side->id;
    surface.ref.topologyLineDefId = board.side->lineDefId;
    surface.ref.topologySide = board.side->side;
    surface.ref.baseboardFaceIndex = face;
    surface.materialId = board.side->baseboard.materialId;
    surface.normal = normal;
    surface.owningSectorIds = {sector.id};
    return surface;
}

Color Ambient(const SectorTopologySector& sector)
{
    const float intensity = std::clamp(sector.ambientIntensity, 0.0f, 1.0f);
    return Color{static_cast<unsigned char>(std::round(sector.ambientColor.r * intensity)),
            static_cast<unsigned char>(std::round(sector.ambientColor.g * intensity)),
            static_cast<unsigned char>(std::round(sector.ambientColor.b * intensity)), 255};
}

SectorGeneratedVertex Vertex(Vector2 p, float height, Vector3 normal,
        Vector2 uv, Vector2 chart, Color color)
{
    SectorGeneratedVertex vertex;
    vertex.position = {p.x, height, p.y};
    vertex.normal = normal;
    vertex.uv = vertex.decalUv = uv;
    vertex.chartUv = chart;
    vertex.color = color;
    return vertex;
}

void Triangle(SectorGeneratedSurface& surface, SectorGeneratedVertex a,
        SectorGeneratedVertex b, SectorGeneratedVertex c)
{
    const Vector3 cross = Vector3CrossProduct(Vector3Subtract(b.position, a.position),
            Vector3Subtract(c.position, a.position));
    if (Vector3LengthSqr(cross) <= Epsilon * Epsilon * Epsilon * Epsilon) return;
    if (Vector3DotProduct(cross, surface.normal) < 0) std::swap(b, c);
    surface.vertices.insert(surface.vertices.end(), {a, b, c});
}

void EmitBoard(const Board& board, const SectorTopologySector& sector,
        SectorGeneratedGeometry& geometry)
{
    // CCW in X/Z. Back and floor contacts have no rendered faces.
    const Vector2 points[] = {board.a, board.b, board.endJoint,
            board.endFront, board.startFront, board.startJoint};
    const Color color = Ambient(sector);
    auto top = Surface(board, sector, 0, {0, 1, 0});
    float minU = INFINITY, maxU = -INFINITY, minV = INFINITY, maxV = -INFINITY;
    for (Vector2 p : points) {
        const float u = Vector2DotProduct(Sub(p, board.a), board.direction);
        const float v = Vector2DotProduct(Sub(p, board.a), board.inward);
        minU = std::min(minU, u); maxU = std::max(maxU, u);
        minV = std::min(minV, v); maxV = std::max(maxV, v);
    }
    top.chartWidth = maxU - minU;
    top.chartHeight = maxV - minV;
    const auto topVertex = [&](Vector2 p) {
        return Vertex(p, board.top, top.normal,
                {Vector2DotProduct(p, board.direction) / kSectorGeneratedTextureWorldSize,
                 Vector2DotProduct(p, board.inward) / kSectorGeneratedTextureWorldSize},
                {Vector2DotProduct(Sub(p, board.a), board.direction) - minU,
                 Vector2DotProduct(Sub(p, board.a), board.inward) - minV}, color);
    };
    for (int i = 1; i < 5; ++i) {
        Triangle(top, topVertex(points[0]), topVertex(points[i]), topVertex(points[i + 1]));
    }
    if (!top.vertices.empty() && board.top < SectorAuthoringToWorldDistance(sector.ceilingZ) - Epsilon) {
        geometry.surfaces.push_back(std::move(top));
    }
    for (int i = 1; i < 6; ++i) {
        const Vector2 a = points[i], b = points[(i + 1) % 6];
        const float width = Vector2Distance(a, b);
        const float bottom = i == 1 ? std::max(board.bottom, board.endNeighborTop)
                : i == 5 ? std::max(board.bottom, board.startNeighborTop) : board.bottom;
        if (width <= Epsilon || board.top - bottom <= Epsilon) continue;
        const Vector2 tangent = Mul(Sub(b, a), 1.0f / width);
        const Vector3 normal{tangent.y, 0, -tangent.x};
        auto surface = Surface(board, sector, i, normal);
        surface.chartWidth = width;
        surface.chartHeight = board.top - bottom;
        const Vector2 viewedRight{normal.z, -normal.x};
        const auto vertex = [&](Vector2 p, float height, float u) {
            return Vertex(p, height, normal,
                    {Vector2DotProduct(p, viewedRight) / kSectorGeneratedTextureWorldSize,
                     -height / kSectorGeneratedTextureWorldSize},
                    {u, board.top - height}, color);
        };
        const auto va = vertex(a, bottom, 0), vb = vertex(b, bottom, width);
        const auto vc = vertex(b, board.top, width), vd = vertex(a, board.top, 0);
        Triangle(surface, va, vb, vc);
        Triangle(surface, va, vc, vd);
        geometry.surfaces.push_back(std::move(surface));
    }
}

bool AppendLoop(const SectorTopologyMap& map, const SectorTopologySector& sector,
        const SectorTopologyLoop& loop, SectorGeneratedGeometry& geometry, std::string* error)
{
    std::vector<Board> boards(loop.edges.size());
    for (size_t i = 0; i < loop.edges.size(); ++i) {
        const auto& edge = loop.edges[i];
        Board& board = boards[i];
        board.side = FindSectorTopologySideDef(map, edge.sideDefId);
        if (!board.side || !board.side->baseboard.enabled) continue;
        if (!IsValidSectorBaseboardSettings(board.side->baseboard)) {
            if (error) *error = "Invalid baseboard dimensions on sidedef " + std::to_string(edge.sideDefId);
            return false;
        }
        const auto* a = FindSectorTopologyVertex(map, edge.startVertexId);
        const auto* b = FindSectorTopologyVertex(map, edge.endVertexId);
        if (!a || !b) return false;
        board.a = SectorCoordToWorldPosition2(a->x, a->y);
        board.b = SectorCoordToWorldPosition2(b->x, b->y);
        board.length = Vector2Distance(board.a, board.b);
        if (board.length <= Epsilon) continue;
        board.direction = Mul(Sub(board.b, board.a), 1.0f / board.length);
        board.inward = {-board.direction.y, board.direction.x};
        board.thickness = SectorAuthoringToWorldDistance(board.side->baseboard.thickness);
        board.bottom = SectorAuthoringToWorldDistance(sector.floorZ);
        float wallTop = sector.ceilingZ;
        if (const auto* opposite = FindOppositeSectorTopologySideDef(map, board.side->id)) {
            const auto* neighbor = FindSectorTopologySector(map, opposite->sectorId);
            if (!neighbor) return false;
            wallTop = std::min(wallTop, neighbor->floorZ);
        }
        board.top = SectorAuthoringToWorldDistance(std::min(wallTop,
                sector.floorZ + board.side->baseboard.height));
        if (board.top - board.bottom <= Epsilon) continue;
        board.startFront = board.startJoint = Add(board.a, Mul(board.inward, board.thickness));
        board.endFront = board.endJoint = Add(board.b, Mul(board.inward, board.thickness));
        board.startNeighborTop = board.endNeighborTop = board.bottom;
        board.enabled = true;
    }
    // Establish invalid spans first, then recompute joins so their neighbors retain caps.
    const auto unjoined = boards;
    for (size_t i = 0; i < boards.size(); ++i) Join(boards[i], boards[(i + 1) % boards.size()]);
    for (size_t i = 0; i < boards.size(); ++i) {
        const bool enabled = boards[i].enabled;
        const float startTrim = boards[i].startTrim, endTrim = boards[i].endTrim;
        boards[i] = unjoined[i];
        boards[i].enabled = enabled;
        boards[i].startTrim = startTrim;
        boards[i].endTrim = endTrim;
    }
    for (size_t i = 0; i < boards.size(); ++i) Join(boards[i], boards[(i + 1) % boards.size()]);
    for (Board& board : boards) {
        if (!board.enabled) continue;
        if (board.startTrim > 0) {
            board.a = Add(board.a, Mul(board.direction, board.startTrim));
            board.startFront = board.startJoint = Add(board.a, Mul(board.inward, board.thickness));
        }
        if (board.endTrim > 0) {
            board.b = Sub(board.b, Mul(board.direction, board.endTrim));
            board.endFront = board.endJoint = Add(board.b, Mul(board.inward, board.thickness));
        }
        EmitBoard(board, sector, geometry);
    }
    return true;
}

} // namespace

bool AppendSectorBaseboardGeometry(const SectorTopologyMap& map,
        const SectorTopologySector& sector, const SectorTopologyLoopSet& loops,
        SectorGeneratedGeometry& geometry, std::string* error)
{
    if (!AppendLoop(map, sector, loops.outer, geometry, error)) return false;
    for (const auto& hole : loops.holes) {
        if (!AppendLoop(map, sector, hole, geometry, error)) return false;
    }
    return true;
}

} // namespace game
