#include "sector_demo/SectorStructuralPrimitives.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace {

void Require(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
}

bool Near(float a, float b, float epsilon = 0.00001f)
{
    return std::fabs(a - b) <= epsilon;
}

const game::SectorCompiledStructuralSurface* FindSurface(
        const game::SectorCompiledStructuralPrimitive& primitive,
        game::SectorStructuralFaceRole role)
{
    const auto found = std::find_if(
            primitive.surfaces.begin(),
            primitive.surfaces.end(),
            [role](const auto& surface) { return surface.face.role == role; });
    return found == primitive.surfaces.end() ? nullptr : &(*found);
}

} // namespace

int main()
{
    using namespace game;

    const SectorStructuralPrimitiveKind kinds[] = {
            SectorStructuralPrimitiveKind::Box,
            SectorStructuralPrimitiveKind::Ramp,
            SectorStructuralPrimitiveKind::Stairs,
            SectorStructuralPrimitiveKind::Cylinder,
            SectorStructuralPrimitiveKind::Sphere};
    std::vector<SectorAuthoringStructuralPrimitive> authored;
    for (int index = 0; index < 5; ++index) {
        SectorAuthoringStructuralPrimitive primitive =
                DefaultSectorAuthoringStructuralPrimitive(kinds[index]);
        primitive.id = 50 - index;
        authored.push_back(primitive);
    }
    Require(!authored.back().collision,
            "sphere collision must default off");
    Require(ValidateSectorAuthoringStructuralPrimitives(authored).empty(),
            "default primitives must validate");

    SectorTopologyMap emptyMap;
    std::vector<SectorCompiledStructuralPrimitive> first;
    std::vector<SectorStructuralDiagnostic> diagnostics;
    Require(CompileSectorStructuralPrimitives(
                    authored, emptyMap, first, diagnostics),
            "all primitive kinds must compile");
    Require(first.size() == authored.size(),
            "every enabled primitive must produce compiled data");
    for (size_t index = 0; index < first.size(); ++index) {
        Require(!first[index].surfaces.empty(),
                "every primitive kind must produce surfaces");
        Require(!first[index].geometryFingerprint.empty(),
                "compiled geometry must have a fingerprint");
        if (index > 0) {
            Require(first[index - 1].sourceAuthoringPrimitiveId
                            < first[index].sourceAuthoringPrimitiveId,
                    "compiled primitives must be ordered by stable ID");
        }
        for (const SectorCompiledStructuralSurface& surface : first[index].surfaces) {
            Require(surface.face.primitiveId
                            == first[index].sourceAuthoringPrimitiveId,
                    "semantic face identity must retain the primitive ID");
            Require(surface.vertices.size() % 3 == 0,
                    "compiled surfaces must contain complete triangles");
        }
    }

    std::reverse(authored.begin(), authored.end());
    std::vector<SectorCompiledStructuralPrimitive> second;
    diagnostics.clear();
    Require(CompileSectorStructuralPrimitives(
                    authored, emptyMap, second, diagnostics),
            "reordered authoring input must compile");
    Require(second.size() == first.size(),
            "deterministic compile must retain primitive count");
    for (size_t index = 0; index < first.size(); ++index) {
        Require(first[index].sourceAuthoringPrimitiveId
                        == second[index].sourceAuthoringPrimitiveId
                        && first[index].geometryFingerprint
                                == second[index].geometryFingerprint,
                "compile order and fingerprints must be deterministic");
    }

    authored.front().id = authored.back().id;
    const auto invalid = ValidateSectorAuthoringStructuralPrimitives(authored);
    Require(!invalid.empty(), "duplicate stable IDs must fail validation");

    SectorAuthoringStructuralPrimitive box =
            DefaultSectorAuthoringStructuralPrimitive(
                    SectorStructuralPrimitiveKind::Box);
    box.id = 100;
    box.x = 64;
    box.z = 96;
    box.yawDegrees = 31.0f;
    box.box.width = 128;
    box.box.depth = 96;
    box.box.bottom = 3.0f;
    box.box.top = 11.0f;
    auto& topOverride = box.materials.overrides[static_cast<size_t>(
            SectorStructuralSurfaceGroup::Top)];
    topOverride.enabled = true;
    topOverride.settings.uv.scale = {1.5f, 0.75f};
    topOverride.settings.uv.offset = {-0.25f, 0.5f};
    box.materials.overrides[static_cast<size_t>(
            SectorStructuralSurfaceGroup::Sides)].enabled = true;
    auto& sideSettings = box.materials.overrides[static_cast<size_t>(
            SectorStructuralSurfaceGroup::Sides)].settings;
    sideSettings.uv.scale = {2.0f, 3.0f};
    sideSettings.uv.offset = {0.25f, -0.5f};
    std::vector<SectorCompiledStructuralPrimitive> uvCompiled;
    diagnostics.clear();
    Require(CompileSectorStructuralPrimitives(
                    {box}, emptyMap, uvCompiled, diagnostics),
            "box UV fixture compiles");
    const Vector2 boxCenter = SectorCoordToWorldPosition2(box.x, box.z);
    for (const SectorCompiledStructuralSurface& surface
            : uvCompiled.front().surfaces) {
        if (surface.face.role != SectorStructuralFaceRole::BoxSide) continue;
        const Vector3 viewedRight{
                surface.normal.z, 0.0f, -surface.normal.x};
        for (const SectorCompiledStructuralVertex& vertex : surface.vertices) {
            const float baseU = ((vertex.position.x - boxCenter.x) * viewedRight.x
                    + (vertex.position.z - boxCenter.y) * viewedRight.z)
                    / kSectorGeneratedTextureWorldSize;
            const float baseV = -vertex.position.y
                    / kSectorGeneratedTextureWorldSize;
            Require(Near(vertex.uv.x, baseU * 2.0f + 0.25f)
                            && Near(vertex.uv.y, baseV * 3.0f - 0.5f),
                    "box vertical faces use upright wall projection before semantic UV transform");
        }
    }
    const float boxRadians = box.yawDegrees
            * 3.14159265358979323846f / 180.0f;
    const float boxCosine = std::cos(boxRadians);
    const float boxSine = std::sin(boxRadians);
    const float boxWidth = SectorCoordToWorldDistance(box.box.width);
    const float boxDepth = SectorCoordToWorldDistance(box.box.depth);
    const auto boxLocalPosition = [&](Vector3 position) {
        const float relativeX = position.x - boxCenter.x;
        const float relativeZ = position.z - boxCenter.y;
        return Vector2{
                boxCosine * relativeX + boxSine * relativeZ,
                -boxSine * relativeX + boxCosine * relativeZ};
    };
    const SectorCompiledStructuralSurface* boxTop = FindSurface(
            uvCompiled.front(), SectorStructuralFaceRole::BoxTop);
    Require(boxTop != nullptr, "box top UV fixture is present");
    for (const SectorCompiledStructuralVertex& vertex : boxTop->vertices) {
        const Vector2 local = boxLocalPosition(vertex.position);
        const Vector2 baseUv{
                local.x / kSectorGeneratedTextureWorldSize,
                local.y / kSectorGeneratedTextureWorldSize};
        Require(Near(vertex.uv.x,
                             baseUv.x * topOverride.settings.uv.scale.x
                                     + topOverride.settings.uv.offset.x)
                        && Near(vertex.uv.y,
                                baseUv.y * topOverride.settings.uv.scale.y
                                        + topOverride.settings.uv.offset.y),
                "box top uses transformed primitive-local world-scale UVs");
        Require(Near(vertex.chartUv.x, local.x + boxWidth * 0.5f)
                        && Near(vertex.chartUv.y,
                                local.y + boxDepth * 0.5f),
                "box top lightmap chart follows its physical width and depth axes");
    }
    const SectorCompiledStructuralSurface* boxBottom = FindSurface(
            uvCompiled.front(), SectorStructuralFaceRole::BoxBottom);
    Require(boxBottom != nullptr, "box bottom UV fixture is present");
    for (const SectorCompiledStructuralVertex& vertex : boxBottom->vertices) {
        const Vector2 local = boxLocalPosition(vertex.position);
        Require(Near(vertex.uv.x,
                             (local.x + boxWidth * 0.5f)
                                     / kSectorGeneratedTextureWorldSize)
                        && Near(vertex.uv.y,
                                (boxDepth * 0.5f - local.y)
                                        / kSectorGeneratedTextureWorldSize),
                "box bottom keeps its existing correctly scaled projection");
    }

    SectorAuthoringStructuralPrimitive stairs =
            DefaultSectorAuthoringStructuralPrimitive(
                    SectorStructuralPrimitiveKind::Stairs);
    stairs.id = 101;
    stairs.x = 128;
    stairs.z = 160;
    stairs.yawDegrees = 27.0f;
    stairs.stairs.width = 128;
    stairs.stairs.run = 256;
    stairs.stairs.rise = 8.0f;
    stairs.stairs.stepCount = 4;
    diagnostics.clear();
    Require(CompileSectorStructuralPrimitives(
                    {stairs}, emptyMap, uvCompiled, diagnostics),
            "stair UV fixture compiles");
    const Vector2 stairCenter = SectorCoordToWorldPosition2(stairs.x, stairs.z);
    const float stairRadians = stairs.yawDegrees
            * 3.14159265358979323846f / 180.0f;
    const float stairCosine = std::cos(stairRadians);
    const float stairSine = std::sin(stairRadians);
    for (const SectorCompiledStructuralSurface& surface
            : uvCompiled.front().surfaces) {
        for (const SectorCompiledStructuralVertex& vertex : surface.vertices) {
            if (surface.face.role == SectorStructuralFaceRole::StairTread
                    || surface.face.role
                            == SectorStructuralFaceRole::StairUnderside) {
                const float relativeX = vertex.position.x - stairCenter.x;
                const float relativeZ = vertex.position.z - stairCenter.y;
                const float expectedU = (stairCosine * relativeX
                                + stairSine * relativeZ)
                        / kSectorGeneratedTextureWorldSize;
                const float expectedV = (-stairSine * relativeX
                                + stairCosine * relativeZ)
                        / kSectorGeneratedTextureWorldSize;
                Require(Near(vertex.uv.x, expectedU)
                                && Near(vertex.uv.y, expectedV),
                        "stair horizontal faces share a continuous primitive-local projection");
            } else if (surface.face.role
                            == SectorStructuralFaceRole::StairRiser
                    || surface.face.role
                            == SectorStructuralFaceRole::StairSide) {
                const Vector3 viewedRight{
                        surface.normal.z, 0.0f, -surface.normal.x};
                const float expectedU = ((vertex.position.x - stairCenter.x)
                                * viewedRight.x
                        + (vertex.position.z - stairCenter.y)
                                * viewedRight.z)
                        / kSectorGeneratedTextureWorldSize;
                Require(Near(vertex.uv.x, expectedU)
                                && Near(vertex.uv.y,
                                    -vertex.position.y
                                            / kSectorGeneratedTextureWorldSize),
                        "stair risers and sides share continuous upright projection");
            }
        }
    }

    const SectorCompiledStructuralPrimitive* cylinderCompiled = nullptr;
    const SectorCompiledStructuralPrimitive* sphereCompiled = nullptr;
    for (const SectorCompiledStructuralPrimitive& primitive : first) {
        if (primitive.authored.kind == SectorStructuralPrimitiveKind::Cylinder) {
            cylinderCompiled = &primitive;
        } else if (primitive.authored.kind == SectorStructuralPrimitiveKind::Sphere) {
            sphereCompiled = &primitive;
        }
    }
    Require(cylinderCompiled != nullptr && sphereCompiled != nullptr,
            "round primitive UV fixtures are present");
    const auto checkUvBounds = [](const SectorCompiledStructuralSurface& surface,
                                  float expectedMaxU,
                                  float expectedMaxV) {
        float minimumU = std::numeric_limits<float>::max();
        float minimumV = std::numeric_limits<float>::max();
        float maximumU = std::numeric_limits<float>::lowest();
        float maximumV = std::numeric_limits<float>::lowest();
        for (const auto& vertex : surface.vertices) {
            minimumU = std::min(minimumU, vertex.uv.x);
            minimumV = std::min(minimumV, vertex.uv.y);
            maximumU = std::max(maximumU, vertex.uv.x);
            maximumV = std::max(maximumV, vertex.uv.y);
        }
        return Near(minimumU, 0.0f) && Near(minimumV, 0.0f)
                && Near(maximumU, expectedMaxU)
                && Near(maximumV, expectedMaxV);
    };
    const float defaultRadius = SectorCoordToWorldDistance(
            SectorCoordSubdivisions);
    const auto* cylinderSide = FindSurface(
            *cylinderCompiled, SectorStructuralFaceRole::CylinderSide);
    const auto* sphereSurface = FindSurface(
            *sphereCompiled, SectorStructuralFaceRole::SphereSurface);
    Require(cylinderSide != nullptr
                    && checkUvBounds(
                            *cylinderSide,
                            3.14159265358979323846f * defaultRadius,
                            SectorAuthoringToWorldDistance(16.0f) / 2.0f),
            "cylinder unwrap projection remains unchanged");
    bool sphereUvValid = sphereSurface != nullptr;
    float sphereMinimumU = std::numeric_limits<float>::max();
    float sphereMaximumU = std::numeric_limits<float>::lowest();
    float sphereMinimumV = std::numeric_limits<float>::max();
    float sphereMaximumV = std::numeric_limits<float>::lowest();
    if (sphereSurface != nullptr) {
        const float maximumU = 3.14159265358979323846f * defaultRadius;
        const float maximumV = maximumU / 2.0f;
        for (const auto& vertex : sphereSurface->vertices) {
            sphereUvValid = sphereUvValid
                    && std::isfinite(vertex.uv.x)
                    && std::isfinite(vertex.uv.y)
                    && vertex.uv.x >= -0.00001f
                    && vertex.uv.x <= maximumU + 0.00001f
                    && vertex.uv.y >= -0.00001f
                    && vertex.uv.y <= maximumV + 0.00001f;
            sphereMinimumU = std::min(sphereMinimumU, vertex.uv.x);
            sphereMaximumU = std::max(sphereMaximumU, vertex.uv.x);
            sphereMinimumV = std::min(sphereMinimumV, vertex.uv.y);
            sphereMaximumV = std::max(sphereMaximumV, vertex.uv.y);
        }
    }
    Require(sphereUvValid
                    && Near(sphereMinimumU, 0.0f)
                    && Near(sphereMaximumU,
                            3.14159265358979323846f * defaultRadius)
                    && sphereMaximumV > sphereMinimumV,
            "sphere projection remains unchanged");

    SectorAuthoringStructuralPrimitive tiltedBox = box;
    tiltedBox.yawDegrees = 0.0f;
    tiltedBox.pitchDegrees = 90.0f;
    tiltedBox.rollDegrees = 0.0f;
    const float pivotHeight = SectorStructuralPrimitivePivotHeight(tiltedBox);
    const Vector2 tiltedCenter = SectorCoordToWorldPosition2(
            tiltedBox.x, tiltedBox.z);
    const Vector3 pivotWorld = TransformSectorStructuralPrimitivePoint(
            tiltedBox, 0.0f, pivotHeight, 0.0f);
    Require(Near(pivotWorld.x, tiltedCenter.x)
                    && Near(pivotWorld.y,
                            SectorAuthoringToWorldDistance(pivotHeight))
                    && Near(pivotWorld.z, tiltedCenter.y),
            "pitch and roll rotate around the structural shape center");
    const Vector3 pitchedUp = RotateSectorStructuralPrimitiveVector(
            tiltedBox, Vector3{0.0f, 1.0f, 0.0f});
    Require(Near(pitchedUp.x, 0.0f)
                    && Near(pitchedUp.y, 0.0f)
                    && Near(pitchedUp.z, 1.0f),
            "positive pitch rotates local up toward local positive Z");
    tiltedBox.pitchDegrees = 0.0f;
    tiltedBox.rollDegrees = 90.0f;
    const Vector3 rolledUp = RotateSectorStructuralPrimitiveVector(
            tiltedBox, Vector3{0.0f, 1.0f, 0.0f});
    Require(Near(rolledUp.x, -1.0f)
                    && Near(rolledUp.y, 0.0f)
                    && Near(rolledUp.z, 0.0f),
            "positive roll rotates local up toward local negative X");

    std::vector<SectorCompiledStructuralPrimitive> baselineBox;
    std::vector<SectorCompiledStructuralPrimitive> rotatedBox;
    diagnostics.clear();
    Require(CompileSectorStructuralPrimitives(
                    {box}, emptyMap, baselineBox, diagnostics),
            "baseline box compiles for rotation comparison");
    tiltedBox = box;
    tiltedBox.pitchDegrees = 23.0f;
    tiltedBox.rollDegrees = 41.0f;
    diagnostics.clear();
    Require(CompileSectorStructuralPrimitives(
                    {tiltedBox}, emptyMap, rotatedBox, diagnostics),
            "tilted box compiles");
    Require(baselineBox.front().geometryFingerprint
                    != rotatedBox.front().geometryFingerprint,
            "pitch and roll affect the deterministic geometry fingerprint");
    Require(baselineBox.front().surfaces.size()
                    == rotatedBox.front().surfaces.size(),
            "rotation preserves semantic box surfaces");
    for (size_t surfaceIndex = 0;
            surfaceIndex < baselineBox.front().surfaces.size();
            ++surfaceIndex) {
        const auto& baselineSurface = baselineBox.front().surfaces[surfaceIndex];
        const auto& rotatedSurface = rotatedBox.front().surfaces[surfaceIndex];
        Require(baselineSurface.vertices.size() == rotatedSurface.vertices.size(),
                "rotation preserves structural vertex ordering");
        for (size_t vertexIndex = 0;
                vertexIndex < baselineSurface.vertices.size();
                ++vertexIndex) {
            const auto& baselineVertex = baselineSurface.vertices[vertexIndex];
            const auto& rotatedVertex = rotatedSurface.vertices[vertexIndex];
            Require(Near(baselineVertex.uv.x, rotatedVertex.uv.x)
                            && Near(baselineVertex.uv.y, rotatedVertex.uv.y)
                            && Near(baselineVertex.chartUv.x,
                                    rotatedVertex.chartUv.x)
                            && Near(baselineVertex.chartUv.y,
                                    rotatedVertex.chartUv.y),
                    "rotation preserves local material and chart UVs");
        }
    }
    const SectorStructuralFootprint tiltedFootprint =
            BuildSectorStructuralFootprint(tiltedBox);
    Require(!tiltedFootprint.circular
                    && tiltedFootprint.pointsWorld.size() >= 4,
            "tilted box exposes a projected convex footprint");

    std::vector<SectorAuthoringStructuralPrimitive> rotatedKinds;
    for (int index = 0; index < 5; ++index) {
        SectorAuthoringStructuralPrimitive rotated =
                DefaultSectorAuthoringStructuralPrimitive(kinds[index]);
        rotated.id = 200 + index;
        rotated.pitchDegrees = 17.0f;
        rotated.rollDegrees = 29.0f;
        rotatedKinds.push_back(rotated);
    }
    std::vector<SectorCompiledStructuralPrimitive> allRotated;
    diagnostics.clear();
    Require(CompileSectorStructuralPrimitives(
                    rotatedKinds, emptyMap, allRotated, diagnostics)
                    && allRotated.size() == rotatedKinds.size(),
            "every structural primitive kind supports pitch and roll");
    for (const auto& primitive : allRotated) {
        for (const auto& surface : primitive.surfaces) {
            for (const auto& vertex : surface.vertices) {
                Require(std::isfinite(vertex.position.x)
                                && std::isfinite(vertex.position.y)
                                && std::isfinite(vertex.position.z)
                                && std::isfinite(vertex.normal.x)
                                && std::isfinite(vertex.normal.y)
                                && std::isfinite(vertex.normal.z),
                        "rotated structural geometry remains finite");
            }
        }
    }

    std::cout << "Sector structural primitive tests passed\n";
    return 0;
}
