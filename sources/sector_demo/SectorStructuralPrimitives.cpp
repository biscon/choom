#include "sector_demo/SectorStructuralPrimitives.h"

#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>

namespace game {
namespace {

constexpr float Pi = 3.14159265358979323846f;
constexpr float GeometryEpsilon = 0.000001f;

size_t GroupIndex(SectorStructuralSurfaceGroup group)
{
    return static_cast<size_t>(group);
}

bool Finite(Vector2 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool Finite(Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vector3 Cross(Vector3 a, Vector3 b)
{
    return Vector3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

Vector3 Subtract(Vector3 a, Vector3 b)
{
    return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

float Dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 Normalize(Vector3 value)
{
    const float lengthSquared = Dot(value, value);
    if (!(lengthSquared > GeometryEpsilon * GeometryEpsilon)
            || !std::isfinite(lengthSquared)) return {};
    const float inverse = 1.0f / std::sqrt(lengthSquared);
    return Vector3{value.x * inverse, value.y * inverse, value.z * inverse};
}

Vector2 ApplyUv(Vector2 uv, const SectorTopologyUvSettings& settings)
{
    return Vector2{uv.x * settings.scale.x + settings.offset.x,
            uv.y * settings.scale.y + settings.offset.y};
}

Vector3 LocalToWorld(
        const SectorAuthoringStructuralPrimitive& primitive,
        float localX,
        float authoredHeight,
        float localZ)
{
    return TransformSectorStructuralPrimitivePoint(
            primitive, localX, authoredHeight, localZ);
}

Vector3 RotateNormal(const SectorAuthoringStructuralPrimitive& primitive, Vector3 local)
{
    return Normalize(RotateSectorStructuralPrimitiveVector(primitive, local));
}

Vector3 InverseRotate(
        const SectorAuthoringStructuralPrimitive& primitive,
        Vector3 world)
{
    const float yaw = primitive.yawDegrees * Pi / 180.0f;
    const float pitch = primitive.pitchDegrees * Pi / 180.0f;
    const float roll = primitive.rollDegrees * Pi / 180.0f;
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cr = std::cos(roll);
    const float sr = std::sin(roll);

    const Vector3 afterYaw{
            cy * world.x + sy * world.z,
            world.y,
            -sy * world.x + cy * world.z};
    const Vector3 afterPitch{
            afterYaw.x,
            cp * afterYaw.y + sp * afterYaw.z,
            -sp * afterYaw.y + cp * afterYaw.z};
    return Vector3{
            cr * afterPitch.x + sr * afterPitch.y,
            -sr * afterPitch.x + cr * afterPitch.y,
            afterPitch.z};
}

Vector3 UnrotatedWorldPosition(
        const SectorAuthoringStructuralPrimitive& primitive,
        Vector3 world)
{
    const Vector2 center = SectorCoordToWorldPosition2(primitive.x, primitive.z);
    const float pivotY = SectorAuthoringToWorldDistance(
            SectorStructuralPrimitivePivotHeight(primitive));
    const Vector3 local = InverseRotate(primitive, Vector3{
            world.x - center.x,
            world.y - pivotY,
            world.z - center.y});
    return Vector3{center.x + local.x, pivotY + local.y, center.y + local.z};
}

const SectorStructuralMaterialSettings& ResolveMaterial(
        const SectorAuthoringStructuralPrimitive& primitive,
        SectorStructuralSurfaceGroup group)
{
    const SectorStructuralMaterialOverride& override =
            primitive.materials.overrides[GroupIndex(group)];
    return override.enabled ? override.settings : primitive.materials.defaultSurface;
}

SectorCompiledStructuralSurface MakeSurface(
        const SectorAuthoringStructuralPrimitive& primitive,
        SectorStructuralFaceRole role,
        int roleIndex,
        SectorStructuralSurfaceGroup group,
        Vector3 normal,
        float chartWidth,
        float chartHeight)
{
    const SectorStructuralMaterialSettings& material = ResolveMaterial(primitive, group);
    SectorCompiledStructuralSurface surface;
    surface.face = SectorStructuralFaceId{primitive.id, primitive.kind, role, roleIndex};
    surface.materialGroup = group;
    surface.materialId = material.materialId;
    surface.normal = normal;
    surface.chartWidth = std::max(chartWidth, 0.001f);
    surface.chartHeight = std::max(chartHeight, 0.001f);
    surface.receivesLightmap = primitive.receivesLightmap;
    surface.castsBakedShadow = primitive.castsBakedShadow;
    surface.castsDynamicShadow = primitive.castsDynamicShadow;
    return surface;
}

void AppendTriangle(
        SectorCompiledStructuralSurface& surface,
        SectorCompiledStructuralVertex a,
        SectorCompiledStructuralVertex b,
        SectorCompiledStructuralVertex c,
        Vector3 expectedNormal)
{
    Vector3 triangleNormal = Normalize(Cross(Subtract(b.position, a.position),
            Subtract(c.position, a.position)));
    if (!Finite(triangleNormal) || Dot(triangleNormal, triangleNormal) <= GeometryEpsilon) return;
    if (Dot(triangleNormal, expectedNormal) < 0.0f) {
        std::swap(b, c);
    }
    surface.vertices.push_back(a);
    surface.vertices.push_back(b);
    surface.vertices.push_back(c);
}

SectorCompiledStructuralVertex Vertex(
        Vector3 position,
        Vector3 normal,
        Vector2 baseUv,
        Vector2 chartUv,
        const SectorTopologyUvSettings& uv)
{
    return SectorCompiledStructuralVertex{position, normal, ApplyUv(baseUv, uv), chartUv, WHITE};
}

void AppendQuad(
        SectorCompiledStructuralSurface& surface,
        Vector3 a,
        Vector3 b,
        Vector3 c,
        Vector3 d,
        Vector3 normal,
        float width,
        float height,
        const SectorTopologyUvSettings& uv)
{
    const float u = width / 2.0f;
    const float v = height / 2.0f;
    const auto va = Vertex(a, normal, {0.0f, v}, {0.0f, height}, uv);
    const auto vb = Vertex(b, normal, {u, v}, {width, height}, uv);
    const auto vc = Vertex(c, normal, {u, 0.0f}, {width, 0.0f}, uv);
    const auto vd = Vertex(d, normal, {0.0f, 0.0f}, {0.0f, 0.0f}, uv);
    AppendTriangle(surface, va, vb, vc, normal);
    AppendTriangle(surface, va, vc, vd, normal);
}

Vector2 VerticalBaseUv(
        const SectorAuthoringStructuralPrimitive& primitive,
        Vector3 position,
        Vector3 outwardNormal)
{
    position = UnrotatedWorldPosition(primitive, position);
    outwardNormal = InverseRotate(primitive, outwardNormal);
    const Vector2 center = SectorCoordToWorldPosition2(primitive.x, primitive.z);
    const Vector3 relative{position.x - center.x, 0.0f, position.z - center.y};
    const Vector3 viewedRight{outwardNormal.z, 0.0f, -outwardNormal.x};
    return Vector2{
            (relative.x * viewedRight.x + relative.z * viewedRight.z)
                    / kSectorGeneratedTextureWorldSize,
            -position.y / kSectorGeneratedTextureWorldSize};
}

Vector2 FlatLocalBaseUv(
        const SectorAuthoringStructuralPrimitive& primitive,
        Vector3 position)
{
    position = UnrotatedWorldPosition(primitive, position);
    const Vector2 center = SectorCoordToWorldPosition2(primitive.x, primitive.z);
    const float relativeX = position.x - center.x;
    const float relativeZ = position.z - center.y;
    return Vector2{
            relativeX / kSectorGeneratedTextureWorldSize,
            relativeZ / kSectorGeneratedTextureWorldSize};
}

void AppendMappedQuad(
        SectorCompiledStructuralSurface& surface,
        Vector3 a,
        Vector3 b,
        Vector3 c,
        Vector3 d,
        Vector3 normal,
        Vector2 uvA,
        Vector2 uvB,
        Vector2 uvC,
        Vector2 uvD,
        Vector2 chartA,
        Vector2 chartB,
        Vector2 chartC,
        Vector2 chartD,
        const SectorTopologyUvSettings& uv)
{
    const auto va = Vertex(a, normal, uvA, chartA, uv);
    const auto vb = Vertex(b, normal, uvB, chartB, uv);
    const auto vc = Vertex(c, normal, uvC, chartC, uv);
    const auto vd = Vertex(d, normal, uvD, chartD, uv);
    AppendTriangle(surface, va, vb, vc, normal);
    AppendTriangle(surface, va, vc, vd, normal);
}

void AppendVerticalQuad(
        SectorCompiledStructuralSurface& surface,
        const SectorAuthoringStructuralPrimitive& primitive,
        Vector3 a,
        Vector3 b,
        Vector3 c,
        Vector3 d,
        Vector3 normal,
        const SectorTopologyUvSettings& uv)
{
    const Vector2 center = SectorCoordToWorldPosition2(primitive.x, primitive.z);
    const Vector3 localNormal = InverseRotate(primitive, normal);
    const Vector3 viewedRight{localNormal.z, 0.0f, -localNormal.x};
    const auto horizontal = [&](Vector3 position) {
        position = UnrotatedWorldPosition(primitive, position);
        return (position.x - center.x) * viewedRight.x
                + (position.z - center.y) * viewedRight.z;
    };
    const float ha = horizontal(a);
    const float hb = horizontal(b);
    const float hc = horizontal(c);
    const float hd = horizontal(d);
    const float minimumHorizontal = std::min({ha, hb, hc, hd});
    const Vector3 localA = UnrotatedWorldPosition(primitive, a);
    const Vector3 localB = UnrotatedWorldPosition(primitive, b);
    const Vector3 localC = UnrotatedWorldPosition(primitive, c);
    const Vector3 localD = UnrotatedWorldPosition(primitive, d);
    const float maximumY = std::max({localA.y, localB.y, localC.y, localD.y});
    AppendMappedQuad(
            surface, a, b, c, d, normal,
            VerticalBaseUv(primitive, a, normal),
            VerticalBaseUv(primitive, b, normal),
            VerticalBaseUv(primitive, c, normal),
            VerticalBaseUv(primitive, d, normal),
            {ha - minimumHorizontal, maximumY - localA.y},
            {hb - minimumHorizontal, maximumY - localB.y},
            {hc - minimumHorizontal, maximumY - localC.y},
            {hd - minimumHorizontal, maximumY - localD.y},
            uv);
}

void BuildBox(
        const SectorAuthoringStructuralPrimitive& primitive,
        std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    const float hx = SectorCoordToWorldDistance(primitive.box.width) * 0.5f;
    const float hz = SectorCoordToWorldDistance(primitive.box.depth) * 0.5f;
    const float height = SectorAuthoringToWorldDistance(primitive.box.top - primitive.box.bottom);
    const float width = hx * 2.0f;
    const float depth = hz * 2.0f;
    const Vector3 p000 = LocalToWorld(primitive, -hx, primitive.box.bottom, -hz);
    const Vector3 p100 = LocalToWorld(primitive, hx, primitive.box.bottom, -hz);
    const Vector3 p110 = LocalToWorld(primitive, hx, primitive.box.bottom, hz);
    const Vector3 p010 = LocalToWorld(primitive, -hx, primitive.box.bottom, hz);
    const Vector3 p001 = LocalToWorld(primitive, -hx, primitive.box.top, -hz);
    const Vector3 p101 = LocalToWorld(primitive, hx, primitive.box.top, -hz);
    const Vector3 p111 = LocalToWorld(primitive, hx, primitive.box.top, hz);
    const Vector3 p011 = LocalToWorld(primitive, -hx, primitive.box.top, hz);
    const Vector3 topNormal = RotateNormal(primitive, {0, 1, 0});
    auto top = MakeSurface(
            primitive,
            SectorStructuralFaceRole::BoxTop,
            0,
            SectorStructuralSurfaceGroup::Top,
            topNormal,
            width,
            depth);
    AppendMappedQuad(
            top,
            p001,
            p011,
            p111,
            p101,
            topNormal,
            FlatLocalBaseUv(primitive, p001),
            FlatLocalBaseUv(primitive, p011),
            FlatLocalBaseUv(primitive, p111),
            FlatLocalBaseUv(primitive, p101),
            {0.0f, 0.0f},
            {0.0f, depth},
            {width, depth},
            {width, 0.0f},
            ResolveMaterial(primitive, SectorStructuralSurfaceGroup::Top).uv);
    surfaces.push_back(std::move(top));
    const auto add = [&](SectorStructuralFaceRole role, int index,
                         SectorStructuralSurfaceGroup group, Vector3 localNormal,
                         Vector3 a, Vector3 b, Vector3 c, Vector3 d,
                         float chartWidth, float chartHeight) {
        const Vector3 normal = RotateNormal(primitive, localNormal);
        SectorCompiledStructuralSurface surface = MakeSurface(
                primitive, role, index, group, normal, chartWidth, chartHeight);
        if (std::fabs(localNormal.y) <= GeometryEpsilon) {
            AppendVerticalQuad(surface, primitive, a, b, c, d, normal,
                    ResolveMaterial(primitive, group).uv);
        } else {
            AppendQuad(surface, a, b, c, d, normal, chartWidth, chartHeight,
                    ResolveMaterial(primitive, group).uv);
        }
        surfaces.push_back(std::move(surface));
    };
    add(SectorStructuralFaceRole::BoxSide, 0, SectorStructuralSurfaceGroup::Sides,
            {0, 0, -1}, p000, p001, p101, p100, width, height);
    add(SectorStructuralFaceRole::BoxSide, 1, SectorStructuralSurfaceGroup::Sides,
            {1, 0, 0}, p100, p101, p111, p110, depth, height);
    add(SectorStructuralFaceRole::BoxSide, 2, SectorStructuralSurfaceGroup::Sides,
            {0, 0, 1}, p110, p111, p011, p010, width, height);
    add(SectorStructuralFaceRole::BoxSide, 3, SectorStructuralSurfaceGroup::Sides,
            {-1, 0, 0}, p010, p011, p001, p000, depth, height);
    add(SectorStructuralFaceRole::BoxBottom, 0, SectorStructuralSurfaceGroup::Bottom,
            {0, -1, 0}, p000, p100, p110, p010, width, depth);
}

void BuildRamp(
        const SectorAuthoringStructuralPrimitive& primitive,
        std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    const float hx = SectorCoordToWorldDistance(primitive.ramp.width) * 0.5f;
    const float hz = SectorCoordToWorldDistance(primitive.ramp.run) * 0.5f;
    const float width = hx * 2.0f;
    const float run = hz * 2.0f;
    const float bottom = primitive.ramp.solidBottom;
    const Vector3 bl0 = LocalToWorld(primitive, -hx, bottom, -hz);
    const Vector3 br0 = LocalToWorld(primitive, hx, bottom, -hz);
    const Vector3 br1 = LocalToWorld(primitive, hx, bottom, hz);
    const Vector3 bl1 = LocalToWorld(primitive, -hx, bottom, hz);
    const Vector3 tl0 = LocalToWorld(primitive, -hx, primitive.ramp.low, -hz);
    const Vector3 tr0 = LocalToWorld(primitive, hx, primitive.ramp.low, -hz);
    const Vector3 tr1 = LocalToWorld(primitive, hx, primitive.ramp.high, hz);
    const Vector3 tl1 = LocalToWorld(primitive, -hx, primitive.ramp.high, hz);
    const float riseWorld = SectorAuthoringToWorldDistance(primitive.ramp.high - primitive.ramp.low);
    const float incline = std::sqrt(run * run + riseWorld * riseWorld);
    const Vector3 topNormal = RotateNormal(primitive,
            Normalize(Vector3{0.0f, run, -riseWorld}));
    auto top = MakeSurface(primitive, SectorStructuralFaceRole::RampTop, 0,
            SectorStructuralSurfaceGroup::InclinedTop, topNormal, width, incline);
    AppendQuad(top, tl0, tr0, tr1, tl1, topNormal, width, incline,
            ResolveMaterial(primitive, SectorStructuralSurfaceGroup::InclinedTop).uv);
    surfaces.push_back(std::move(top));
    const auto addQuad = [&](int index, Vector3 localNormal,
                             Vector3 a, Vector3 b, Vector3 c, Vector3 d,
                             float w, float h) {
        const Vector3 normal = RotateNormal(primitive, localNormal);
        auto surface = MakeSurface(primitive, SectorStructuralFaceRole::RampSideOrEnd,
                index, SectorStructuralSurfaceGroup::SidesAndEnds, normal, w, h);
        AppendVerticalQuad(surface, primitive, a, b, c, d, normal,
                ResolveMaterial(primitive, SectorStructuralSurfaceGroup::SidesAndEnds).uv);
        if (!surface.vertices.empty()) surfaces.push_back(std::move(surface));
    };
    addQuad(0, {0, 0, -1}, bl0, tl0, tr0, br0, width,
            SectorAuthoringToWorldDistance(primitive.ramp.low - bottom));
    addQuad(1, {1, 0, 0}, br0, tr0, tr1, br1, run,
            SectorAuthoringToWorldDistance(primitive.ramp.high - bottom));
    addQuad(2, {0, 0, 1}, br1, tr1, tl1, bl1, width,
            SectorAuthoringToWorldDistance(primitive.ramp.high - bottom));
    addQuad(3, {-1, 0, 0}, bl1, tl1, tl0, bl0, run,
            SectorAuthoringToWorldDistance(primitive.ramp.high - bottom));
    const Vector3 bottomNormal = RotateNormal(primitive, {0, -1, 0});
    auto underside = MakeSurface(primitive, SectorStructuralFaceRole::RampBottom, 0,
            SectorStructuralSurfaceGroup::Bottom, bottomNormal, width, run);
    AppendQuad(underside, bl0, br0, br1, bl1, bottomNormal, width, run,
            ResolveMaterial(primitive, SectorStructuralSurfaceGroup::Bottom).uv);
    surfaces.push_back(std::move(underside));
}

void BuildStairs(
        const SectorAuthoringStructuralPrimitive& primitive,
        std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    const int steps = primitive.stairs.stepCount;
    const float hx = SectorCoordToWorldDistance(primitive.stairs.width) * 0.5f;
    const float run = SectorCoordToWorldDistance(primitive.stairs.run);
    const float zStart = -run * 0.5f;
    const float stepRun = run / static_cast<float>(steps);
    const float stepRise = primitive.stairs.rise / static_cast<float>(steps);
    const float width = hx * 2.0f;
    for (int step = 0; step < steps; ++step) {
        const float z0 = zStart + stepRun * step;
        const float z1 = z0 + stepRun;
        const float topHeight = primitive.stairs.bottom + stepRise * (step + 1);
        const float previousHeight = primitive.stairs.bottom + stepRise * step;
        const Vector3 up = RotateNormal(primitive, {0, 1, 0});
        auto tread = MakeSurface(primitive, SectorStructuralFaceRole::StairTread, step,
                SectorStructuralSurfaceGroup::Treads, up, width, stepRun);
        const Vector3 treadA = LocalToWorld(primitive, -hx, topHeight, z0);
        const Vector3 treadB = LocalToWorld(primitive, -hx, topHeight, z1);
        const Vector3 treadC = LocalToWorld(primitive, hx, topHeight, z1);
        const Vector3 treadD = LocalToWorld(primitive, hx, topHeight, z0);
        AppendMappedQuad(tread,
                treadA, treadB, treadC, treadD, up,
                FlatLocalBaseUv(primitive, treadA),
                FlatLocalBaseUv(primitive, treadB),
                FlatLocalBaseUv(primitive, treadC),
                FlatLocalBaseUv(primitive, treadD),
                {0.0f, 0.0f}, {0.0f, stepRun},
                {width, stepRun}, {width, 0.0f},
                ResolveMaterial(primitive, SectorStructuralSurfaceGroup::Treads).uv);
        surfaces.push_back(std::move(tread));
        const Vector3 back = RotateNormal(primitive, {0, 0, -1});
        auto riser = MakeSurface(primitive, SectorStructuralFaceRole::StairRiser, step,
                SectorStructuralSurfaceGroup::RisersAndSides, back, width,
                SectorAuthoringToWorldDistance(stepRise));
        AppendVerticalQuad(riser, primitive,
                LocalToWorld(primitive, -hx, previousHeight, z0),
                LocalToWorld(primitive, -hx, topHeight, z0),
                LocalToWorld(primitive, hx, topHeight, z0),
                LocalToWorld(primitive, hx, previousHeight, z0),
                back,
                ResolveMaterial(primitive, SectorStructuralSurfaceGroup::RisersAndSides).uv);
        surfaces.push_back(std::move(riser));
        for (int sideIndex = 0; sideIndex < 2; ++sideIndex) {
            const float x = sideIndex == 0 ? -hx : hx;
            const Vector3 localNormal = sideIndex == 0
                    ? Vector3{-1, 0, 0} : Vector3{1, 0, 0};
            const Vector3 normal = RotateNormal(primitive, localNormal);
            auto side = MakeSurface(primitive, SectorStructuralFaceRole::StairSide,
                    sideIndex * steps + step,
                    SectorStructuralSurfaceGroup::RisersAndSides, normal, stepRun,
                    SectorAuthoringToWorldDistance(topHeight - primitive.stairs.bottom));
            AppendVerticalQuad(side, primitive,
                    LocalToWorld(primitive, x, primitive.stairs.bottom, z0),
                    LocalToWorld(primitive, x, primitive.stairs.bottom, z1),
                    LocalToWorld(primitive, x, topHeight, z1),
                    LocalToWorld(primitive, x, topHeight, z0),
                    normal,
                    ResolveMaterial(primitive, SectorStructuralSurfaceGroup::RisersAndSides).uv);
            surfaces.push_back(std::move(side));
        }
    }
    const float high = primitive.stairs.bottom + primitive.stairs.rise;
    const Vector3 front = RotateNormal(primitive, {0, 0, 1});
    auto end = MakeSurface(primitive, SectorStructuralFaceRole::StairRiser, steps,
            SectorStructuralSurfaceGroup::RisersAndSides, front, width,
            SectorAuthoringToWorldDistance(primitive.stairs.rise));
    AppendVerticalQuad(end, primitive,
            LocalToWorld(primitive, hx, primitive.stairs.bottom, run * 0.5f),
            LocalToWorld(primitive, hx, high, run * 0.5f),
            LocalToWorld(primitive, -hx, high, run * 0.5f),
            LocalToWorld(primitive, -hx, primitive.stairs.bottom, run * 0.5f),
            front,
            ResolveMaterial(primitive, SectorStructuralSurfaceGroup::RisersAndSides).uv);
    surfaces.push_back(std::move(end));
    const Vector3 down = RotateNormal(primitive, {0, -1, 0});
    auto underside = MakeSurface(primitive, SectorStructuralFaceRole::StairUnderside, 0,
            SectorStructuralSurfaceGroup::Underside, down, width, run);
    const Vector3 undersideA = LocalToWorld(
            primitive, -hx, primitive.stairs.bottom, -run * 0.5f);
    const Vector3 undersideB = LocalToWorld(
            primitive, hx, primitive.stairs.bottom, -run * 0.5f);
    const Vector3 undersideC = LocalToWorld(
            primitive, hx, primitive.stairs.bottom, run * 0.5f);
    const Vector3 undersideD = LocalToWorld(
            primitive, -hx, primitive.stairs.bottom, run * 0.5f);
    AppendMappedQuad(underside,
            undersideA, undersideB, undersideC, undersideD, down,
            FlatLocalBaseUv(primitive, undersideA),
            FlatLocalBaseUv(primitive, undersideB),
            FlatLocalBaseUv(primitive, undersideC),
            FlatLocalBaseUv(primitive, undersideD),
            {0.0f, 0.0f}, {width, 0.0f},
            {width, run}, {0.0f, run},
            ResolveMaterial(primitive, SectorStructuralSurfaceGroup::Underside).uv);
    surfaces.push_back(std::move(underside));
}

void BuildCylinder(
        const SectorAuthoringStructuralPrimitive& primitive,
        std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    const int count = primitive.cylinder.radialSegments;
    const float radius = SectorCoordToWorldDistance(primitive.cylinder.radius);
    const float height = SectorAuthoringToWorldDistance(
            primitive.cylinder.top - primitive.cylinder.bottom);
    const float circumference = 2.0f * Pi * radius;
    auto side = MakeSurface(primitive, SectorStructuralFaceRole::CylinderSide, 0,
            SectorStructuralSurfaceGroup::CurvedSide, {}, circumference, height);
    const auto& sideUv = ResolveMaterial(primitive, SectorStructuralSurfaceGroup::CurvedSide).uv;
    for (int index = 0; index < count; ++index) {
        const float a0 = 2.0f * Pi * index / count;
        const float a1 = 2.0f * Pi * (index + 1) / count;
        const Vector3 n0 = RotateNormal(primitive, {std::cos(a0), 0, std::sin(a0)});
        const Vector3 n1 = RotateNormal(primitive, {std::cos(a1), 0, std::sin(a1)});
        const Vector3 p00 = LocalToWorld(primitive, radius * std::cos(a0), primitive.cylinder.bottom,
                radius * std::sin(a0));
        const Vector3 p01 = LocalToWorld(primitive, radius * std::cos(a0), primitive.cylinder.top,
                radius * std::sin(a0));
        const Vector3 p11 = LocalToWorld(primitive, radius * std::cos(a1), primitive.cylinder.top,
                radius * std::sin(a1));
        const Vector3 p10 = LocalToWorld(primitive, radius * std::cos(a1), primitive.cylinder.bottom,
                radius * std::sin(a1));
        const float u0 = circumference * index / count;
        const float u1 = circumference * (index + 1) / count;
        AppendTriangle(side,
                Vertex(p00, n0, {u0 / 2, height / 2}, {u0, height}, sideUv),
                Vertex(p01, n0, {u0 / 2, 0}, {u0, 0}, sideUv),
                Vertex(p11, n1, {u1 / 2, 0}, {u1, 0}, sideUv), n0);
        AppendTriangle(side,
                Vertex(p00, n0, {u0 / 2, height / 2}, {u0, height}, sideUv),
                Vertex(p11, n1, {u1 / 2, 0}, {u1, 0}, sideUv),
                Vertex(p10, n1, {u1 / 2, height / 2}, {u1, height}, sideUv), n1);
    }
    surfaces.push_back(std::move(side));
    const auto addCap = [&](bool top) {
        const auto group = top ? SectorStructuralSurfaceGroup::TopCap
                               : SectorStructuralSurfaceGroup::BottomCap;
        const auto role = top ? SectorStructuralFaceRole::CylinderTopCap
                              : SectorStructuralFaceRole::CylinderBottomCap;
        const Vector3 normal = RotateNormal(primitive, top ? Vector3{0, 1, 0} : Vector3{0, -1, 0});
        auto cap = MakeSurface(primitive, role, 0, group, normal, radius * 2, radius * 2);
        const auto& uv = ResolveMaterial(primitive, group).uv;
        const float authoredHeight = top ? primitive.cylinder.top : primitive.cylinder.bottom;
        const Vector3 center = LocalToWorld(primitive, 0, authoredHeight, 0);
        for (int index = 0; index < count; ++index) {
            const float a0 = 2.0f * Pi * index / count;
            const float a1 = 2.0f * Pi * (index + 1) / count;
            const Vector3 p0 = LocalToWorld(primitive, radius * std::cos(a0), authoredHeight,
                    radius * std::sin(a0));
            const Vector3 p1 = LocalToWorld(primitive, radius * std::cos(a1), authoredHeight,
                    radius * std::sin(a1));
            AppendTriangle(cap,
                    Vertex(center, normal, {0, 0}, {radius, radius}, uv),
                    Vertex(p0, normal, {std::cos(a0) * radius / 2, std::sin(a0) * radius / 2},
                            {(std::cos(a0) + 1) * radius, (std::sin(a0) + 1) * radius}, uv),
                    Vertex(p1, normal, {std::cos(a1) * radius / 2, std::sin(a1) * radius / 2},
                            {(std::cos(a1) + 1) * radius, (std::sin(a1) + 1) * radius}, uv), normal);
        }
        surfaces.push_back(std::move(cap));
    };
    addCap(true);
    addCap(false);
}

void BuildLadderRail(
        const SectorAuthoringStructuralPrimitive& primitive,
        float centerX,
        int railIndex,
        std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    const float half = SectorStructuralLadderFrameThicknessWorld
            * primitive.ladder.thicknessScale * 0.5f;
    const float x0 = centerX - half;
    const float x1 = centerX + half;
    const float z0 = -half;
    const float z1 = half;
    const float bottom = primitive.ladder.bottom;
    const float top = bottom + primitive.ladder.height;
    const float heightWorld = SectorAuthoringToWorldDistance(primitive.ladder.height);
    const float size = half * 2.0f;
    const Vector3 p000 = LocalToWorld(primitive, x0, bottom, z0);
    const Vector3 p100 = LocalToWorld(primitive, x1, bottom, z0);
    const Vector3 p110 = LocalToWorld(primitive, x1, bottom, z1);
    const Vector3 p010 = LocalToWorld(primitive, x0, bottom, z1);
    const Vector3 p001 = LocalToWorld(primitive, x0, top, z0);
    const Vector3 p101 = LocalToWorld(primitive, x1, top, z0);
    const Vector3 p111 = LocalToWorld(primitive, x1, top, z1);
    const Vector3 p011 = LocalToWorld(primitive, x0, top, z1);
    const auto& uv = ResolveMaterial(
            primitive, SectorStructuralSurfaceGroup::LadderFrame).uv;
    const auto add = [&](int faceIndex, Vector3 localNormal,
                         Vector3 a, Vector3 b, Vector3 c, Vector3 d,
                         float chartWidth, float chartHeight) {
        const Vector3 normal = RotateNormal(primitive, localNormal);
        auto surface = MakeSurface(
                primitive,
                SectorStructuralFaceRole::LadderFrameFace,
                railIndex * 6 + faceIndex,
                SectorStructuralSurfaceGroup::LadderFrame,
                normal,
                chartWidth,
                chartHeight);
        if (std::fabs(localNormal.y) <= GeometryEpsilon) {
            AppendVerticalQuad(surface, primitive, a, b, c, d, normal, uv);
        } else {
            AppendMappedQuad(
                    surface, a, b, c, d, normal,
                    FlatLocalBaseUv(primitive, a),
                    FlatLocalBaseUv(primitive, b),
                    FlatLocalBaseUv(primitive, c),
                    FlatLocalBaseUv(primitive, d),
                    {0.0f, 0.0f}, {size, 0.0f},
                    {size, size}, {0.0f, size}, uv);
        }
        surfaces.push_back(std::move(surface));
    };
    add(0, {0, 1, 0}, p001, p011, p111, p101, size, size);
    add(1, {0, 0, -1}, p000, p001, p101, p100, size, heightWorld);
    add(2, {1, 0, 0}, p100, p101, p111, p110, size, heightWorld);
    add(3, {0, 0, 1}, p110, p111, p011, p010, size, heightWorld);
    add(4, {-1, 0, 0}, p010, p011, p001, p000, size, heightWorld);
    add(5, {0, -1, 0}, p000, p100, p110, p010, size, size);
}

void BuildLadderRung(
        const SectorAuthoringStructuralPrimitive& primitive,
        float x0,
        float x1,
        float authoredHeight,
        int rungIndex,
        std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    const float radius = SectorStructuralLadderRungDiameterWorld
            * primitive.ladder.thicknessScale * 0.5f;
    const float length = x1 - x0;
    const float circumference = 2.0f * Pi * radius;
    const float radiusAuthored = SectorWorldToAuthoringDistance(radius);
    const int count = SectorStructuralLadderRadialSegments;
    const auto& uv = ResolveMaterial(
            primitive, SectorStructuralSurfaceGroup::LadderRungs).uv;
    auto side = MakeSurface(
            primitive,
            SectorStructuralFaceRole::LadderRungSide,
            rungIndex,
            SectorStructuralSurfaceGroup::LadderRungs,
            {}, circumference, length);
    for (int index = 0; index < count; ++index) {
        const float a0 = 2.0f * Pi * index / count;
        const float a1 = 2.0f * Pi * (index + 1) / count;
        const float y0 = std::sin(a0);
        const float y1 = std::sin(a1);
        const float z0 = radius * std::cos(a0);
        const float z1 = radius * std::cos(a1);
        const Vector3 n0 = RotateNormal(primitive, {0.0f, y0, std::cos(a0)});
        const Vector3 n1 = RotateNormal(primitive, {0.0f, y1, std::cos(a1)});
        const Vector3 p00 = LocalToWorld(
                primitive, x0, authoredHeight + radiusAuthored * y0, z0);
        const Vector3 p01 = LocalToWorld(
                primitive, x1, authoredHeight + radiusAuthored * y0, z0);
        const Vector3 p11 = LocalToWorld(
                primitive, x1, authoredHeight + radiusAuthored * y1, z1);
        const Vector3 p10 = LocalToWorld(
                primitive, x0, authoredHeight + radiusAuthored * y1, z1);
        const float u0 = circumference * index / count;
        const float u1 = circumference * (index + 1) / count;
        AppendTriangle(side,
                Vertex(p00, n0, {u0 / 2.0f, length / 2.0f}, {u0, length}, uv),
                Vertex(p01, n0, {u0 / 2.0f, 0.0f}, {u0, 0.0f}, uv),
                Vertex(p11, n1, {u1 / 2.0f, 0.0f}, {u1, 0.0f}, uv), n0);
        AppendTriangle(side,
                Vertex(p00, n0, {u0 / 2.0f, length / 2.0f}, {u0, length}, uv),
                Vertex(p11, n1, {u1 / 2.0f, 0.0f}, {u1, 0.0f}, uv),
                Vertex(p10, n1, {u1 / 2.0f, length / 2.0f}, {u1, length}, uv), n1);
    }
    surfaces.push_back(std::move(side));
    const auto addCap = [&](bool right) {
        const Vector3 normal = RotateNormal(
                primitive, right ? Vector3{1, 0, 0} : Vector3{-1, 0, 0});
        auto cap = MakeSurface(
                primitive,
                SectorStructuralFaceRole::LadderRungCap,
                rungIndex * 2 + (right ? 1 : 0),
                SectorStructuralSurfaceGroup::LadderRungs,
                normal, radius * 2.0f, radius * 2.0f);
        const float x = right ? x1 : x0;
        const Vector3 center = LocalToWorld(primitive, x, authoredHeight, 0.0f);
        for (int index = 0; index < count; ++index) {
            const float a0 = 2.0f * Pi * index / count;
            const float a1 = 2.0f * Pi * (index + 1) / count;
            const auto point = [&](float angle) {
                return LocalToWorld(
                        primitive, x,
                        authoredHeight + radiusAuthored * std::sin(angle),
                        radius * std::cos(angle));
            };
            AppendTriangle(cap,
                    Vertex(center, normal, {0, 0}, {radius, radius}, uv),
                    Vertex(point(a0), normal,
                            {std::cos(a0) * radius / 2.0f,
                                    std::sin(a0) * radius / 2.0f},
                            {(std::cos(a0) + 1.0f) * radius,
                                    (std::sin(a0) + 1.0f) * radius}, uv),
                    Vertex(point(a1), normal,
                            {std::cos(a1) * radius / 2.0f,
                                    std::sin(a1) * radius / 2.0f},
                            {(std::cos(a1) + 1.0f) * radius,
                                    (std::sin(a1) + 1.0f) * radius}, uv), normal);
        }
        surfaces.push_back(std::move(cap));
    };
    addCap(false);
    addCap(true);
}

void BuildLadder(
        const SectorAuthoringStructuralPrimitive& primitive,
        std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    const float width = SectorCoordToWorldDistance(primitive.ladder.width);
    const float frameThickness = SectorStructuralLadderFrameThicknessWorld
            * primitive.ladder.thicknessScale;
    const float halfWidth = width * 0.5f;
    BuildLadderRail(primitive, -halfWidth + frameThickness * 0.5f, 0, surfaces);
    BuildLadderRail(primitive, halfWidth - frameThickness * 0.5f, 1, surfaces);
    const float x0 = -halfWidth + frameThickness;
    const float x1 = halfWidth - frameThickness;
    const float spacing = primitive.ladder.height / (primitive.ladder.rungCount + 1);
    for (int rung = 0; rung < primitive.ladder.rungCount; ++rung) {
        BuildLadderRung(
                primitive, x0, x1,
                primitive.ladder.bottom + spacing * (rung + 1),
                rung, surfaces);
    }
}

void BuildSphere(
        const SectorAuthoringStructuralPrimitive& primitive,
        std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    const int latitudes = primitive.sphere.latitudeSegments;
    const int longitudes = primitive.sphere.longitudeSegments;
    const float radius = SectorCoordToWorldDistance(primitive.sphere.radius);
    auto surface = MakeSurface(primitive, SectorStructuralFaceRole::SphereSurface, 0,
            SectorStructuralSurfaceGroup::Top, {}, Pi * radius * 2.0f, Pi * radius);
    const auto& uv = primitive.materials.defaultSurface.uv;
    const auto make = [&](int latitude, int longitude) {
        const float v = static_cast<float>(latitude) / latitudes;
        const float u = static_cast<float>(longitude) / longitudes;
        const float phi = -Pi * 0.5f + Pi * v;
        const float theta = 2.0f * Pi * u;
        const Vector3 localNormal{std::cos(phi) * std::cos(theta), std::sin(phi),
                std::cos(phi) * std::sin(theta)};
        const Vector3 normal = RotateNormal(primitive, localNormal);
        const float centerHeightWorld = SectorAuthoringToWorldDistance(primitive.sphere.centerHeight);
        const Vector2 center = SectorCoordToWorldPosition2(primitive.x, primitive.z);
        const Vector3 position{center.x + normal.x * radius,
                centerHeightWorld + normal.y * radius,
                center.y + normal.z * radius};
        return Vertex(position, normal, {u * Pi * radius, (1.0f - v) * Pi * radius / 2.0f},
                {u * 2.0f * Pi * radius, (1.0f - v) * Pi * radius}, uv);
    };
    for (int latitude = 0; latitude < latitudes; ++latitude) {
        for (int longitude = 0; longitude < longitudes; ++longitude) {
            const auto a = make(latitude, longitude);
            const auto b = make(latitude, longitude + 1);
            const auto c = make(latitude + 1, longitude + 1);
            const auto d = make(latitude + 1, longitude);
            if (latitude == 0) {
                AppendTriangle(surface, a, c, d, d.normal);
            } else if (latitude == latitudes - 1) {
                AppendTriangle(surface, a, b, c, a.normal);
            } else {
                AppendTriangle(surface, a, b, c, a.normal);
                AppendTriangle(surface, a, c, d, a.normal);
            }
        }
    }
    surfaces.push_back(std::move(surface));
}

bool PointInPolygon(Vector2 point, const std::vector<Vector2>& polygon)
{
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        const Vector2 a = polygon[i];
        const Vector2 b = polygon[j];
        if ((a.y > point.y) != (b.y > point.y)
                && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) {
            inside = !inside;
        }
    }
    return inside;
}

float Cross2(Vector2 a, Vector2 b, Vector2 c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

int CrossSign(float value)
{
    if (value > GeometryEpsilon) return 1;
    if (value < -GeometryEpsilon) return -1;
    return 0;
}

bool PointOnSegment(Vector2 point, Vector2 a, Vector2 b)
{
    return std::fabs(Cross2(a, b, point)) <= GeometryEpsilon
            && point.x >= std::min(a.x, b.x) - GeometryEpsilon
            && point.x <= std::max(a.x, b.x) + GeometryEpsilon
            && point.y >= std::min(a.y, b.y) - GeometryEpsilon
            && point.y <= std::max(a.y, b.y) + GeometryEpsilon;
}

bool SegmentsIntersect(Vector2 a, Vector2 b, Vector2 c, Vector2 d)
{
    const int abC = CrossSign(Cross2(a, b, c));
    const int abD = CrossSign(Cross2(a, b, d));
    const int cdA = CrossSign(Cross2(c, d, a));
    const int cdB = CrossSign(Cross2(c, d, b));
    if (abC == 0 && PointOnSegment(c, a, b)) return true;
    if (abD == 0 && PointOnSegment(d, a, b)) return true;
    if (cdA == 0 && PointOnSegment(a, c, d)) return true;
    if (cdB == 0 && PointOnSegment(b, c, d)) return true;
    return abC * abD < 0 && cdA * cdB < 0;
}

std::vector<Vector2> LoopWorldPoints(const SectorTopologyMap& map, const SectorTopologyLoop& loop)
{
    std::vector<Vector2> points;
    points.reserve(loop.vertexIds.size());
    for (int id : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(map, id);
        if (vertex != nullptr) points.push_back(SectorCoordToWorldPosition2(vertex->x, vertex->y));
    }
    return points;
}

bool PolygonEdgesIntersect(const std::vector<Vector2>& a, const std::vector<Vector2>& b)
{
    for (size_t i = 0; i < a.size(); ++i) {
        for (size_t j = 0; j < b.size(); ++j) {
            if (SegmentsIntersect(a[i], a[(i + 1) % a.size()], b[j], b[(j + 1) % b.size()])) {
                return true;
            }
        }
    }
    return false;
}

bool FootprintOverlapsSector(
        const SectorTopologyMap& map,
        const SectorStructuralFootprint& footprint,
        int sectorId)
{
    SectorTopologyLoopSet loops;
    if (!ExtractSectorTopologyLoops(map, sectorId, loops, nullptr)) return false;
    const std::vector<Vector2> outer = LoopWorldPoints(map, loops.outer);
    if (outer.size() < 3 || footprint.pointsWorld.size() < 3) return false;
    const auto inSector = [&](Vector2 point) {
        if (!PointInPolygon(point, outer)) return false;
        for (const SectorTopologyLoop& hole : loops.holes) {
            if (PointInPolygon(point, LoopWorldPoints(map, hole))) return false;
        }
        return true;
    };
    for (Vector2 point : footprint.pointsWorld) if (inSector(point)) return true;
    for (Vector2 point : outer) if (PointInPolygon(point, footprint.pointsWorld)) return true;
    if (PolygonEdgesIntersect(footprint.pointsWorld, outer)) return true;
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (PolygonEdgesIntersect(footprint.pointsWorld, LoopWorldPoints(map, hole))) return true;
    }
    return false;
}

std::string Fingerprint(const std::vector<SectorCompiledStructuralSurface>& surfaces)
{
    uint64_t hash = 14695981039346656037ull;
    const auto bytes = [&hash](const void* data, size_t size) {
        const auto* values = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= values[i];
            hash *= 1099511628211ull;
        }
    };
    for (const auto& surface : surfaces) {
        const int role = static_cast<int>(surface.face.role);
        bytes(&role, sizeof(role));
        bytes(&surface.face.roleIndex, sizeof(surface.face.roleIndex));
        for (const auto& vertex : surface.vertices) {
            bytes(&vertex.position, sizeof(vertex.position));
            bytes(&vertex.normal, sizeof(vertex.normal));
            bytes(&vertex.uv, sizeof(vertex.uv));
        }
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}

void AddError(std::vector<SectorStructuralDiagnostic>& diagnostics, int id, std::string message)
{
    diagnostics.push_back({SectorStructuralDiagnosticSeverity::Error, id, std::move(message)});
}

bool ValidMaterialSet(const SectorStructuralMaterialSet& materials)
{
    const auto valid = [](const SectorStructuralMaterialSettings& value) {
        return Finite(value.uv.scale) && Finite(value.uv.offset)
                && value.uv.scale.x != 0.0f && value.uv.scale.y != 0.0f;
    };
    if (!valid(materials.defaultSurface)) return false;
    for (const auto& override : materials.overrides) {
        if (override.enabled && !valid(override.settings)) return false;
    }
    return true;
}

std::vector<Vector2> ConvexHull(std::vector<Vector2> points)
{
    std::sort(points.begin(), points.end(), [](Vector2 a, Vector2 b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });
    points.erase(std::unique(points.begin(), points.end(), [](Vector2 a, Vector2 b) {
        return std::fabs(a.x - b.x) <= GeometryEpsilon
                && std::fabs(a.y - b.y) <= GeometryEpsilon;
    }), points.end());
    if (points.size() <= 2) return points;
    std::vector<Vector2> hull;
    hull.reserve(points.size() * 2);
    for (Vector2 point : points) {
        while (hull.size() >= 2
                && Cross2(hull[hull.size() - 2], hull.back(), point)
                        <= GeometryEpsilon) {
            hull.pop_back();
        }
        hull.push_back(point);
    }
    const size_t lowerSize = hull.size();
    for (size_t index = points.size() - 1; index-- > 0;) {
        const Vector2 point = points[index];
        while (hull.size() > lowerSize
                && Cross2(hull[hull.size() - 2], hull.back(), point)
                        <= GeometryEpsilon) {
            hull.pop_back();
        }
        hull.push_back(point);
    }
    if (!hull.empty()) hull.pop_back();
    return hull;
}

void AppendProjectedPoint(
        const SectorAuthoringStructuralPrimitive& primitive,
        std::vector<Vector2>& points,
        float localX,
        float authoredHeight,
        float localZ)
{
    const Vector3 world = TransformSectorStructuralPrimitivePoint(
            primitive, localX, authoredHeight, localZ);
    points.push_back(Vector2{world.x, world.z});
}

} // namespace

float SectorStructuralPrimitivePivotHeight(
        const SectorAuthoringStructuralPrimitive& primitive)
{
    switch (primitive.kind) {
        case SectorStructuralPrimitiveKind::Box:
            return (primitive.box.bottom + primitive.box.top) * 0.5f;
        case SectorStructuralPrimitiveKind::Ramp:
            return (primitive.ramp.solidBottom + primitive.ramp.high) * 0.5f;
        case SectorStructuralPrimitiveKind::Stairs:
            return primitive.stairs.bottom + primitive.stairs.rise * 0.5f;
        case SectorStructuralPrimitiveKind::Cylinder:
            return (primitive.cylinder.bottom + primitive.cylinder.top) * 0.5f;
        case SectorStructuralPrimitiveKind::Sphere:
            return primitive.sphere.centerHeight;
        case SectorStructuralPrimitiveKind::Ladder:
            return primitive.ladder.bottom + primitive.ladder.height * 0.5f;
    }
    return 0.0f;
}

bool SectorStructuralPrimitiveHasTilt(
        const SectorAuthoringStructuralPrimitive& primitive)
{
    const auto nonZero = [](float degrees) {
        if (!std::isfinite(degrees)) return true;
        const float wrapped = std::fmod(std::fabs(degrees), 360.0f);
        return wrapped > 0.0001f && std::fabs(wrapped - 360.0f) > 0.0001f;
    };
    return nonZero(primitive.pitchDegrees) || nonZero(primitive.rollDegrees);
}

Vector3 RotateSectorStructuralPrimitiveVector(
        const SectorAuthoringStructuralPrimitive& primitive,
        Vector3 local)
{
    const float yaw = primitive.yawDegrees * Pi / 180.0f;
    const float pitch = primitive.pitchDegrees * Pi / 180.0f;
    const float roll = primitive.rollDegrees * Pi / 180.0f;
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cr = std::cos(roll);
    const float sr = std::sin(roll);

    const Vector3 afterRoll{
            cr * local.x - sr * local.y,
            sr * local.x + cr * local.y,
            local.z};
    const Vector3 afterPitch{
            afterRoll.x,
            cp * afterRoll.y - sp * afterRoll.z,
            sp * afterRoll.y + cp * afterRoll.z};
    return Vector3{
            cy * afterPitch.x - sy * afterPitch.z,
            afterPitch.y,
            sy * afterPitch.x + cy * afterPitch.z};
}

Vector3 TransformSectorStructuralPrimitivePoint(
        const SectorAuthoringStructuralPrimitive& primitive,
        float localXWorld,
        float authoredHeight,
        float localZWorld)
{
    const Vector2 center = SectorCoordToWorldPosition2(primitive.x, primitive.z);
    const float pivotHeight = SectorStructuralPrimitivePivotHeight(primitive);
    const float pivotWorld = SectorAuthoringToWorldDistance(pivotHeight);
    const Vector3 rotated = RotateSectorStructuralPrimitiveVector(
            primitive,
            Vector3{localXWorld,
                    SectorAuthoringToWorldDistance(authoredHeight - pivotHeight),
                    localZWorld});
    return Vector3{
            center.x + rotated.x,
            pivotWorld + rotated.y,
            center.y + rotated.z};
}

SectorAuthoringStructuralPrimitive DefaultSectorAuthoringStructuralPrimitive(
        SectorStructuralPrimitiveKind kind)
{
    SectorAuthoringStructuralPrimitive primitive;
    primitive.kind = kind;
    primitive.collision = kind != SectorStructuralPrimitiveKind::Sphere;
    return primitive;
}

const char* SectorStructuralPrimitiveKindName(SectorStructuralPrimitiveKind kind)
{
    switch (kind) {
        case SectorStructuralPrimitiveKind::Box: return "box";
        case SectorStructuralPrimitiveKind::Ramp: return "ramp";
        case SectorStructuralPrimitiveKind::Stairs: return "stairs";
        case SectorStructuralPrimitiveKind::Cylinder: return "cylinder";
        case SectorStructuralPrimitiveKind::Sphere: return "sphere";
        case SectorStructuralPrimitiveKind::Ladder: return "ladder";
    }
    return "unknown";
}

const char* SectorStructuralSurfaceGroupName(SectorStructuralSurfaceGroup group)
{
    switch (group) {
        case SectorStructuralSurfaceGroup::Top: return "top";
        case SectorStructuralSurfaceGroup::Sides: return "sides";
        case SectorStructuralSurfaceGroup::Bottom: return "bottom";
        case SectorStructuralSurfaceGroup::InclinedTop: return "inclinedTop";
        case SectorStructuralSurfaceGroup::SidesAndEnds: return "sidesAndEnds";
        case SectorStructuralSurfaceGroup::Treads: return "treads";
        case SectorStructuralSurfaceGroup::RisersAndSides: return "risersAndSides";
        case SectorStructuralSurfaceGroup::Underside: return "underside";
        case SectorStructuralSurfaceGroup::TopCap: return "topCap";
        case SectorStructuralSurfaceGroup::CurvedSide: return "curvedSide";
        case SectorStructuralSurfaceGroup::BottomCap: return "bottomCap";
        case SectorStructuralSurfaceGroup::LadderFrame: return "ladderFrame";
        case SectorStructuralSurfaceGroup::LadderRungs: return "ladderRungs";
        case SectorStructuralSurfaceGroup::Count: break;
    }
    return "unknown";
}

const char* SectorStructuralFaceRoleName(SectorStructuralFaceRole role)
{
    switch (role) {
        case SectorStructuralFaceRole::BoxTop: return "boxTop";
        case SectorStructuralFaceRole::BoxSide: return "boxSide";
        case SectorStructuralFaceRole::BoxBottom: return "boxBottom";
        case SectorStructuralFaceRole::RampTop: return "rampTop";
        case SectorStructuralFaceRole::RampSideOrEnd: return "rampSideOrEnd";
        case SectorStructuralFaceRole::RampBottom: return "rampBottom";
        case SectorStructuralFaceRole::StairTread: return "stairTread";
        case SectorStructuralFaceRole::StairRiser: return "stairRiser";
        case SectorStructuralFaceRole::StairSide: return "stairSide";
        case SectorStructuralFaceRole::StairUnderside: return "stairUnderside";
        case SectorStructuralFaceRole::CylinderTopCap: return "cylinderTopCap";
        case SectorStructuralFaceRole::CylinderSide: return "cylinderSide";
        case SectorStructuralFaceRole::CylinderBottomCap: return "cylinderBottomCap";
        case SectorStructuralFaceRole::SphereSurface: return "sphereSurface";
        case SectorStructuralFaceRole::LadderFrameFace: return "ladderFrameFace";
        case SectorStructuralFaceRole::LadderRungSide: return "ladderRungSide";
        case SectorStructuralFaceRole::LadderRungCap: return "ladderRungCap";
    }
    return "unknown";
}

SectorStructuralFootprint BuildSectorStructuralFootprint(
        const SectorAuthoringStructuralPrimitive& primitive)
{
    SectorStructuralFootprint result;
    result.centerWorld = SectorCoordToWorldPosition2(primitive.x, primitive.z);
    const Vector3 ascent = RotateSectorStructuralPrimitiveVector(
            primitive, Vector3{0.0f, 0.0f, 1.0f});
    const float ascentLength = std::hypot(ascent.x, ascent.z);
    if (ascentLength > GeometryEpsilon) {
        result.ascentDirectionWorld = Vector2{
                ascent.x / ascentLength, ascent.z / ascentLength};
    } else {
        const float yaw = primitive.yawDegrees * Pi / 180.0f;
        result.ascentDirectionWorld = Vector2{-std::sin(yaw), std::cos(yaw)};
    }

    if (primitive.kind == SectorStructuralPrimitiveKind::Sphere) {
        result.circular = true;
        result.radiusWorld = SectorCoordToWorldDistance(primitive.sphere.radius);
        const int segments = primitive.sphere.longitudeSegments;
        result.pointsWorld.reserve(static_cast<size_t>(segments));
        for (int i = 0; i < segments; ++i) {
            const float angle = 2.0f * Pi * i / segments;
            result.pointsWorld.push_back(Vector2{
                    result.centerWorld.x + std::cos(angle) * result.radiusWorld,
                    result.centerWorld.y + std::sin(angle) * result.radiusWorld});
        }
        return result;
    }

    if (primitive.kind != SectorStructuralPrimitiveKind::Cylinder
            && !SectorStructuralPrimitiveHasTilt(primitive)) {
        SectorCoord width = primitive.box.width;
        SectorCoord depth = primitive.box.depth;
        if (primitive.kind == SectorStructuralPrimitiveKind::Ramp) {
            width = primitive.ramp.width;
            depth = primitive.ramp.run;
        } else if (primitive.kind == SectorStructuralPrimitiveKind::Stairs) {
            width = primitive.stairs.width;
            depth = primitive.stairs.run;
        } else if (primitive.kind == SectorStructuralPrimitiveKind::Ladder) {
            width = primitive.ladder.width;
            depth = std::max<SectorCoord>(
                    1,
                    static_cast<SectorCoord>(std::lround(
                            SectorWorldToAuthoringDistance(
                                    SectorStructuralLadderFrameThicknessWorld
                                    * primitive.ladder.thicknessScale)
                            * SectorCoordSubdivisions)));
        }
        const float hx = SectorCoordToWorldDistance(width) * 0.5f;
        const float hz = SectorCoordToWorldDistance(depth) * 0.5f;
        const float yaw = primitive.yawDegrees * Pi / 180.0f;
        const float cosine = std::cos(yaw);
        const float sine = std::sin(yaw);
        result.pointsWorld.reserve(4);
        for (Vector2 local : std::array<Vector2, 4>{{
                    {-hx, -hz}, {hx, -hz}, {hx, hz}, {-hx, hz}}}) {
            result.pointsWorld.push_back(Vector2{
                    result.centerWorld.x + cosine * local.x - sine * local.y,
                    result.centerWorld.y + sine * local.x + cosine * local.y});
        }
        return result;
    }

    std::vector<Vector2> projected;
    if (primitive.kind == SectorStructuralPrimitiveKind::Cylinder) {
        const float radius = SectorCoordToWorldDistance(primitive.cylinder.radius);
        const int segments = primitive.cylinder.radialSegments;
        result.circular = !SectorStructuralPrimitiveHasTilt(primitive);
        result.radiusWorld = radius;
        projected.reserve(static_cast<size_t>(segments) * 2);
        for (int index = 0; index < segments; ++index) {
            const float angle = 2.0f * Pi * index / segments;
            const float x = radius * std::cos(angle);
            const float z = radius * std::sin(angle);
            AppendProjectedPoint(
                    primitive, projected, x, primitive.cylinder.bottom, z);
            AppendProjectedPoint(
                    primitive, projected, x, primitive.cylinder.top, z);
        }
    } else {
        SectorCoord width = primitive.box.width;
        SectorCoord depth = primitive.box.depth;
        float bottom = primitive.box.bottom;
        float top = primitive.box.top;
        if (primitive.kind == SectorStructuralPrimitiveKind::Ramp) {
            width = primitive.ramp.width;
            depth = primitive.ramp.run;
            bottom = primitive.ramp.solidBottom;
            top = primitive.ramp.high;
        } else if (primitive.kind == SectorStructuralPrimitiveKind::Stairs) {
            width = primitive.stairs.width;
            depth = primitive.stairs.run;
            bottom = primitive.stairs.bottom;
            top = primitive.stairs.bottom + primitive.stairs.rise;
        }
        const float hx = SectorCoordToWorldDistance(width) * 0.5f;
        const float hz = SectorCoordToWorldDistance(depth) * 0.5f;
        projected.reserve(primitive.kind == SectorStructuralPrimitiveKind::Stairs
                        ? static_cast<size_t>(primitive.stairs.stepCount + 1) * 4
                        : 8);
        for (float x : {-hx, hx}) {
            for (float z : {-hz, hz}) {
                AppendProjectedPoint(primitive, projected, x, bottom, z);
            }
        }
        if (primitive.kind == SectorStructuralPrimitiveKind::Ramp) {
            for (float x : {-hx, hx}) {
                AppendProjectedPoint(
                        primitive, projected, x, primitive.ramp.low, -hz);
                AppendProjectedPoint(
                        primitive, projected, x, primitive.ramp.high, hz);
            }
        } else if (primitive.kind == SectorStructuralPrimitiveKind::Stairs) {
            const float run = hz * 2.0f;
            const float stepRun = run / primitive.stairs.stepCount;
            const float stepRise = primitive.stairs.rise / primitive.stairs.stepCount;
            for (int step = 0; step < primitive.stairs.stepCount; ++step) {
                const float z0 = -hz + stepRun * step;
                const float z1 = z0 + stepRun;
                const float height = primitive.stairs.bottom + stepRise * (step + 1);
                for (float x : {-hx, hx}) {
                    AppendProjectedPoint(primitive, projected, x, height, z0);
                    AppendProjectedPoint(primitive, projected, x, height, z1);
                }
            }
        } else {
            for (float x : {-hx, hx}) {
                for (float z : {-hz, hz}) {
                    AppendProjectedPoint(primitive, projected, x, top, z);
                }
            }
        }
    }
    result.pointsWorld = ConvexHull(std::move(projected));
    return result;
}

std::vector<SectorStructuralDiagnostic> ValidateSectorAuthoringStructuralPrimitives(
        const std::vector<SectorAuthoringStructuralPrimitive>& primitives)
{
    std::vector<SectorStructuralDiagnostic> diagnostics;
    std::set<int> ids;
    for (const auto& primitive : primitives) {
        if (primitive.id <= 0) AddError(diagnostics, primitive.id, "Structural primitive ID must be positive");
        else if (!ids.insert(primitive.id).second) AddError(diagnostics, primitive.id, "Duplicate structural primitive ID");
        if (!std::isfinite(primitive.yawDegrees) || primitive.yawDegrees < 0.0f
                || primitive.yawDegrees >= 360.0f
                || !std::isfinite(primitive.pitchDegrees) || primitive.pitchDegrees < 0.0f
                || primitive.pitchDegrees >= 360.0f
                || !std::isfinite(primitive.rollDegrees) || primitive.rollDegrees < 0.0f
                || primitive.rollDegrees >= 360.0f) {
            AddError(diagnostics, primitive.id,
                    "Structural primitive rotation must be finite and in [0, 360)");
        }
        if (!ValidMaterialSet(primitive.materials)) {
            AddError(diagnostics, primitive.id, "Structural primitive UV settings must be finite and non-zero");
        }
        switch (primitive.kind) {
            case SectorStructuralPrimitiveKind::Box:
                if (primitive.box.width < SectorStructuralMinimumPlanarExtent
                        || primitive.box.depth < SectorStructuralMinimumPlanarExtent
                        || !std::isfinite(primitive.box.bottom) || !std::isfinite(primitive.box.top)
                        || primitive.box.top - primitive.box.bottom < SectorStructuralMinimumHeight) {
                    AddError(diagnostics, primitive.id, "Box dimensions or height span are invalid");
                }
                break;
            case SectorStructuralPrimitiveKind::Ramp:
                if (primitive.ramp.width < SectorStructuralMinimumPlanarExtent
                        || primitive.ramp.run < SectorStructuralMinimumPlanarExtent
                        || !std::isfinite(primitive.ramp.solidBottom)
                        || !std::isfinite(primitive.ramp.low) || !std::isfinite(primitive.ramp.high)
                        || primitive.ramp.low < primitive.ramp.solidBottom
                        || primitive.ramp.high - primitive.ramp.low < SectorStructuralMinimumHeight) {
                    AddError(diagnostics, primitive.id, "Ramp dimensions or elevations are invalid");
                }
                break;
            case SectorStructuralPrimitiveKind::Stairs:
                if (primitive.stairs.width < SectorStructuralMinimumPlanarExtent
                        || primitive.stairs.run < SectorStructuralMinimumPlanarExtent
                        || !std::isfinite(primitive.stairs.bottom) || !std::isfinite(primitive.stairs.rise)
                        || primitive.stairs.rise < SectorStructuralMinimumHeight
                        || primitive.stairs.stepCount < SectorStructuralMinimumStairSteps
                        || primitive.stairs.stepCount > SectorStructuralMaximumStairSteps) {
                    AddError(diagnostics, primitive.id, "Stair dimensions, rise, or step count are invalid");
                }
                break;
            case SectorStructuralPrimitiveKind::Cylinder:
                if (primitive.cylinder.radius < SectorStructuralMinimumPlanarExtent
                        || !std::isfinite(primitive.cylinder.bottom) || !std::isfinite(primitive.cylinder.top)
                        || primitive.cylinder.top - primitive.cylinder.bottom < SectorStructuralMinimumHeight
                        || primitive.cylinder.radialSegments < SectorStructuralMinimumCylinderSegments
                        || primitive.cylinder.radialSegments > SectorStructuralMaximumCylinderSegments) {
                    AddError(diagnostics, primitive.id, "Cylinder dimensions or segment count are invalid");
                }
                break;
            case SectorStructuralPrimitiveKind::Sphere:
                if (primitive.sphere.radius < SectorStructuralMinimumPlanarExtent
                        || !std::isfinite(primitive.sphere.centerHeight)
                        || primitive.sphere.latitudeSegments < SectorStructuralMinimumSphereLatitudeSegments
                        || primitive.sphere.latitudeSegments > SectorStructuralMaximumSphereLatitudeSegments
                        || primitive.sphere.longitudeSegments < SectorStructuralMinimumSphereLongitudeSegments
                        || primitive.sphere.longitudeSegments > SectorStructuralMaximumSphereLongitudeSegments) {
                    AddError(diagnostics, primitive.id, "Sphere dimensions or segment counts are invalid");
                }
                break;
            case SectorStructuralPrimitiveKind::Ladder: {
                const float frameThickness = SectorStructuralLadderFrameThicknessWorld
                        * primitive.ladder.thicknessScale;
                const float rungDiameter = SectorStructuralLadderRungDiameterWorld
                        * primitive.ladder.thicknessScale;
                const float widthWorld = SectorCoordToWorldDistance(primitive.ladder.width);
                const float spacingWorld = SectorAuthoringToWorldDistance(
                        primitive.ladder.height / (primitive.ladder.rungCount + 1));
                if (primitive.ladder.width < SectorStructuralMinimumPlanarExtent
                        || !std::isfinite(primitive.ladder.bottom)
                        || !std::isfinite(primitive.ladder.height)
                        || primitive.ladder.height < SectorStructuralMinimumHeight
                        || !std::isfinite(primitive.ladder.thicknessScale)
                        || primitive.ladder.thicknessScale
                                < SectorStructuralMinimumLadderThicknessScale
                        || primitive.ladder.thicknessScale
                                > SectorStructuralMaximumLadderThicknessScale
                        || primitive.ladder.rungCount < SectorStructuralMinimumLadderRungs
                        || primitive.ladder.rungCount > SectorStructuralMaximumLadderRungs
                        || widthWorld <= frameThickness * 2.0f
                        || spacingWorld < rungDiameter) {
                    AddError(diagnostics, primitive.id,
                            "Ladder dimensions, thickness, or rung count are invalid");
                }
                if (SectorStructuralPrimitiveHasTilt(primitive)) {
                    AddError(diagnostics, primitive.id,
                            "Ladders support yaw rotation only");
                }
                break;
            }
            default:
                AddError(diagnostics, primitive.id, "Unsupported structural primitive kind");
                break;
        }
    }
    return diagnostics;
}

bool CompileSectorStructuralPrimitives(
        const std::vector<SectorAuthoringStructuralPrimitive>& authored,
        const SectorTopologyMap& topology,
        std::vector<SectorCompiledStructuralPrimitive>& outCompiled,
        std::vector<SectorStructuralDiagnostic>& outDiagnostics)
{
    outCompiled.clear();
    outDiagnostics = ValidateSectorAuthoringStructuralPrimitives(authored);
    if (std::any_of(outDiagnostics.begin(), outDiagnostics.end(), [](const auto& diagnostic) {
                return diagnostic.severity == SectorStructuralDiagnosticSeverity::Error;
            })) return false;
    std::vector<const SectorAuthoringStructuralPrimitive*> sorted;
    sorted.reserve(authored.size());
    for (const auto& primitive : authored) sorted.push_back(&primitive);
    std::sort(sorted.begin(), sorted.end(), [](const auto* a, const auto* b) { return a->id < b->id; });
    outCompiled.reserve(sorted.size());
    for (const auto* source : sorted) {
        SectorCompiledStructuralPrimitive compiled;
        compiled.sourceAuthoringPrimitiveId = source->id;
        compiled.authored = *source;
        const SectorStructuralFootprint footprint = BuildSectorStructuralFootprint(*source);
        for (const SectorTopologySector& sector : topology.sectors) {
            if (FootprintOverlapsSector(topology, footprint, sector.id)) {
                compiled.owningSectorIds.push_back(sector.id);
            }
        }
        std::sort(compiled.owningSectorIds.begin(), compiled.owningSectorIds.end());
        if (compiled.owningSectorIds.empty()) {
            outDiagnostics.push_back({SectorStructuralDiagnosticSeverity::Warning, source->id,
                    "Structural primitive has no resolved sector membership; rendering will be conservative"});
        }
        if (source->enabled) {
            switch (source->kind) {
                case SectorStructuralPrimitiveKind::Box: BuildBox(*source, compiled.surfaces); break;
                case SectorStructuralPrimitiveKind::Ramp: BuildRamp(*source, compiled.surfaces); break;
                case SectorStructuralPrimitiveKind::Stairs: BuildStairs(*source, compiled.surfaces); break;
                case SectorStructuralPrimitiveKind::Cylinder: BuildCylinder(*source, compiled.surfaces); break;
                case SectorStructuralPrimitiveKind::Sphere: BuildSphere(*source, compiled.surfaces); break;
                case SectorStructuralPrimitiveKind::Ladder: BuildLadder(*source, compiled.surfaces); break;
            }
            for (auto& surface : compiled.surfaces) surface.owningSectorIds = compiled.owningSectorIds;
            const auto emptySurface = std::find_if(
                    compiled.surfaces.begin(), compiled.surfaces.end(),
                    [](const auto& surface) { return surface.vertices.empty(); });
            if (compiled.surfaces.empty() || emptySurface != compiled.surfaces.end()) {
                const std::string detail = emptySurface == compiled.surfaces.end()
                        ? std::string{}
                        : std::string{" ("}
                                + SectorStructuralFaceRoleName(emptySurface->face.role)
                                + " " + std::to_string(emptySurface->face.roleIndex) + ")";
                AddError(outDiagnostics, source->id,
                        "Structural primitive generated invalid or empty geometry" + detail);
                outCompiled.clear();
                return false;
            }
        }
        compiled.geometryFingerprint = Fingerprint(compiled.surfaces);
        outCompiled.push_back(std::move(compiled));
    }
    return true;
}

} // namespace game
