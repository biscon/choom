#pragma once

#include "sector_demo/SectorTopologyTypes.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace game {

struct SectorTopologyMap;

enum class SectorStructuralPrimitiveKind {
    Box,
    Ramp,
    Stairs,
    Cylinder,
    Sphere
};

enum class SectorStructuralSurfaceGroup {
    Top,
    Sides,
    Bottom,
    InclinedTop,
    SidesAndEnds,
    Treads,
    RisersAndSides,
    Underside,
    TopCap,
    CurvedSide,
    BottomCap,
    Count
};

enum class SectorStructuralFaceRole {
    BoxTop,
    BoxSide,
    BoxBottom,
    RampTop,
    RampSideOrEnd,
    RampBottom,
    StairTread,
    StairRiser,
    StairSide,
    StairUnderside,
    CylinderTopCap,
    CylinderSide,
    CylinderBottomCap,
    SphereSurface
};

struct SectorStructuralFaceId {
    int primitiveId = -1;
    SectorStructuralPrimitiveKind primitiveKind = SectorStructuralPrimitiveKind::Box;
    SectorStructuralFaceRole role = SectorStructuralFaceRole::BoxTop;
    int roleIndex = 0;
};

struct SectorStructuralMaterialSettings {
    std::string materialId;
    SectorTopologyUvSettings uv;
};

struct SectorStructuralMaterialOverride {
    bool enabled = false;
    SectorStructuralMaterialSettings settings;
};

struct SectorStructuralMaterialSet {
    SectorStructuralMaterialSettings defaultSurface;
    std::array<SectorStructuralMaterialOverride,
            static_cast<size_t>(SectorStructuralSurfaceGroup::Count)> overrides;
};

struct SectorStructuralBoxParameters {
    SectorCoord width = SectorCoordSubdivisions;
    SectorCoord depth = SectorCoordSubdivisions;
    float bottom = 0.0f;
    float top = 8.0f;
};

struct SectorStructuralRampParameters {
    SectorCoord width = SectorCoordSubdivisions;
    SectorCoord run = SectorCoordSubdivisions;
    float solidBottom = 0.0f;
    float low = 0.0f;
    float high = 8.0f;
};

struct SectorStructuralStairParameters {
    SectorCoord width = SectorCoordSubdivisions;
    SectorCoord run = SectorCoordSubdivisions;
    float bottom = 0.0f;
    float rise = 8.0f;
    int stepCount = 8;
};

struct SectorStructuralCylinderParameters {
    SectorCoord radius = SectorCoordSubdivisions;
    float bottom = 0.0f;
    float top = 16.0f;
    int radialSegments = 16;
};

struct SectorStructuralSphereParameters {
    SectorCoord radius = SectorCoordSubdivisions;
    float centerHeight = 8.0f;
    int latitudeSegments = 12;
    int longitudeSegments = 24;
};

struct SectorAuthoringStructuralPrimitive {
    int id = -1;
    SectorStructuralPrimitiveKind kind = SectorStructuralPrimitiveKind::Box;
    bool enabled = true;
    SectorCoord x = 0;
    SectorCoord z = 0;
    float yawDegrees = 0.0f;
    bool collision = true;
    bool receivesLightmap = true;
    bool castsBakedShadow = true;
    bool castsDynamicShadow = true;
    SectorStructuralMaterialSet materials;
    SectorStructuralBoxParameters box;
    SectorStructuralRampParameters ramp;
    SectorStructuralStairParameters stairs;
    SectorStructuralCylinderParameters cylinder;
    SectorStructuralSphereParameters sphere;
};

struct SectorStructuralFootprint {
    std::vector<Vector2> pointsWorld;
    Vector2 centerWorld = {};
    float radiusWorld = 0.0f;
    bool circular = false;
    Vector2 ascentDirectionWorld = {0.0f, 1.0f};
};

struct SectorCompiledStructuralVertex {
    Vector3 position = {};
    Vector3 normal = {};
    Vector2 uv = {};
    Vector2 chartUv = {};
    Color color = WHITE;
};

struct SectorCompiledStructuralSurface {
    SectorStructuralFaceId face;
    SectorStructuralSurfaceGroup materialGroup = SectorStructuralSurfaceGroup::Top;
    std::string materialId;
    Vector3 normal = {};
    float chartWidth = 0.0f;
    float chartHeight = 0.0f;
    bool receivesLightmap = true;
    bool castsBakedShadow = true;
    bool castsDynamicShadow = true;
    std::vector<int> owningSectorIds;
    std::vector<SectorCompiledStructuralVertex> vertices;
};

struct SectorCompiledStructuralPrimitive {
    int sourceAuthoringPrimitiveId = -1;
    SectorAuthoringStructuralPrimitive authored;
    std::vector<int> owningSectorIds;
    std::vector<SectorCompiledStructuralSurface> surfaces;
    std::string geometryFingerprint;
};

enum class SectorStructuralDiagnosticSeverity {
    Warning,
    Error
};

struct SectorStructuralDiagnostic {
    SectorStructuralDiagnosticSeverity severity = SectorStructuralDiagnosticSeverity::Error;
    int primitiveId = -1;
    std::string message;
};

inline constexpr SectorCoord SectorStructuralMinimumPlanarExtent = SectorCoordSubdivisions;
inline constexpr float SectorStructuralMinimumHeight = 1.0f;
inline constexpr int SectorStructuralMinimumCylinderSegments = 3;
inline constexpr int SectorStructuralMaximumCylinderSegments = 64;
inline constexpr int SectorStructuralDefaultCylinderSegments = 16;
inline constexpr int SectorStructuralMinimumSphereLatitudeSegments = 2;
inline constexpr int SectorStructuralMaximumSphereLatitudeSegments = 32;
inline constexpr int SectorStructuralDefaultSphereLatitudeSegments = 12;
inline constexpr int SectorStructuralMinimumSphereLongitudeSegments = 3;
inline constexpr int SectorStructuralMaximumSphereLongitudeSegments = 64;
inline constexpr int SectorStructuralDefaultSphereLongitudeSegments = 24;
inline constexpr int SectorStructuralMinimumStairSteps = 1;
inline constexpr int SectorStructuralMaximumStairSteps = 256;
inline constexpr int SectorStructuralDefaultStairSteps = 8;

SectorAuthoringStructuralPrimitive DefaultSectorAuthoringStructuralPrimitive(
        SectorStructuralPrimitiveKind kind);
const char* SectorStructuralPrimitiveKindName(SectorStructuralPrimitiveKind kind);
const char* SectorStructuralSurfaceGroupName(SectorStructuralSurfaceGroup group);
const char* SectorStructuralFaceRoleName(SectorStructuralFaceRole role);
SectorStructuralFootprint BuildSectorStructuralFootprint(
        const SectorAuthoringStructuralPrimitive& primitive);
std::vector<SectorStructuralDiagnostic> ValidateSectorAuthoringStructuralPrimitives(
        const std::vector<SectorAuthoringStructuralPrimitive>& primitives);
bool CompileSectorStructuralPrimitives(
        const std::vector<SectorAuthoringStructuralPrimitive>& authored,
        const SectorTopologyMap& topology,
        std::vector<SectorCompiledStructuralPrimitive>& outCompiled,
        std::vector<SectorStructuralDiagnostic>& outDiagnostics);

} // namespace game
