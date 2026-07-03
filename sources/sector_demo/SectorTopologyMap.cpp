#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace game {
namespace {

constexpr float PreviewWalkSpeedMin = 0.1f;
constexpr float PreviewWalkSpeedMax = 100.0f;
constexpr float PreviewRunSpeedMin = 0.1f;
constexpr float PreviewRunSpeedMax = 200.0f;
constexpr float PreviewMouseSensitivityMin = 0.01f;
constexpr float PreviewMouseSensitivityMax = 20.0f;
constexpr float PreviewEyeHeightMin = 0.1f;
constexpr float PreviewEyeHeightMax = 3.0f;
constexpr float PreviewGravityMin = 0.0f;
constexpr float PreviewGravityMax = 200.0f;
constexpr float PreviewPlayerRadiusMin = 0.05f;
constexpr float PreviewPlayerRadiusMax = 2.0f;
constexpr float PreviewPlayerHeightMin = 0.5f;
constexpr float PreviewPlayerHeightMax = 3.0f;
constexpr float PreviewStepHeightMin = 0.0f;
constexpr float PreviewStepHeightMax = 2.0f;
constexpr float PreviewJumpHeightMin = 0.0f;
constexpr float PreviewJumpHeightMax = 3.0f;
constexpr float PreviewHeadBobStrengthMin = 0.0f;
constexpr float PreviewHeadBobStrengthMax = 0.25f;
constexpr float PreviewHeadBobFrequencyMin = 0.0f;
constexpr float PreviewHeadBobFrequencyMax = 20.0f;
constexpr float PreviewObjectProbeDebugDrawMaxDistanceMin = 0.0f;
constexpr float PreviewObjectProbeDebugDrawMaxDistanceMax = 512.0f;
constexpr float SkyVerticalScaleMin = 0.01f;
constexpr float SkyVerticalScaleMax = 100.0f;
constexpr float DirectionalLightMinLengthSqr = 0.000001f;
constexpr float DoorAnchorSideProbeDistance = 0.001f;
constexpr float DoorAnchorSideEpsilon = 0.000001f;

float ClampFinite(float value, float fallback, float minValue, float maxValue)
{
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minValue, maxValue);
}

float SectorCoordToWorldDistanceLocal(SectorCoord value)
{
    return static_cast<float>(value)
            / static_cast<float>(SectorCoordSubdivisions)
            * kSectorWorldUnitsPerAuthoringUnit;
}

Vector2 SectorCoordToWorldPosition2Local(SectorCoord x, SectorCoord y)
{
    return Vector2{SectorCoordToWorldDistanceLocal(x), SectorCoordToWorldDistanceLocal(y)};
}

float SignedDistanceFromDirectedLine(Vector2 origin, Vector2 tangent, Vector2 point)
{
    return tangent.x * (point.y - origin.y) - tangent.y * (point.x - origin.x);
}

bool ProbeIsInsideDirectedSide(Vector2 origin, Vector2 tangent, Vector2 point)
{
    return SignedDistanceFromDirectedLine(origin, tangent, point) > DoorAnchorSideEpsilon;
}

bool ProbeIsOutsideDirectedSide(Vector2 origin, Vector2 tangent, Vector2 point)
{
    return SignedDistanceFromDirectedLine(origin, tangent, point) < -DoorAnchorSideEpsilon;
}

template<typename T>
int AllocateNextId(const std::vector<T>& values)
{
    int maxId = 0;
    for (const T& value : values) {
        if (value.id > maxId) {
            maxId = value.id;
        }
    }

    if (maxId == std::numeric_limits<int>::max()) {
        return -1;
    }
    return maxId + 1;
}

template<typename T>
const T* FindById(const std::vector<T>& values, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return nullptr;
    }

    for (const T& value : values) {
        if (value.id == id) {
            return &value;
        }
    }
    return nullptr;
}

template<typename T>
T* FindById(std::vector<T>& values, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return nullptr;
    }

    for (T& value : values) {
        if (value.id == id) {
            return &value;
        }
    }
    return nullptr;
}

} // namespace

SectorPreviewSettings DefaultSectorPreviewSettings()
{
    return SectorPreviewSettings{};
}

SectorPreviewSettings NormalizeSectorPreviewSettings(SectorPreviewSettings settings)
{
    const SectorPreviewSettings defaults = DefaultSectorPreviewSettings();
    settings.walkSpeed = ClampFinite(
            settings.walkSpeed,
            defaults.walkSpeed,
            PreviewWalkSpeedMin,
            PreviewWalkSpeedMax);
    settings.runSpeed = ClampFinite(
            settings.runSpeed,
            defaults.runSpeed,
            PreviewRunSpeedMin,
            PreviewRunSpeedMax);
    settings.mouseSensitivity = ClampFinite(
            settings.mouseSensitivity,
            defaults.mouseSensitivity,
            PreviewMouseSensitivityMin,
            PreviewMouseSensitivityMax);
    settings.eyeHeight = ClampFinite(
            settings.eyeHeight,
            defaults.eyeHeight,
            PreviewEyeHeightMin,
            PreviewEyeHeightMax);
    settings.gravity = ClampFinite(
            settings.gravity,
            defaults.gravity,
            PreviewGravityMin,
            PreviewGravityMax);
    settings.playerRadius = ClampFinite(
            settings.playerRadius,
            defaults.playerRadius,
            PreviewPlayerRadiusMin,
            PreviewPlayerRadiusMax);
    settings.playerHeight = ClampFinite(
            settings.playerHeight,
            defaults.playerHeight,
            PreviewPlayerHeightMin,
            PreviewPlayerHeightMax);
    settings.playerHeight = std::max(settings.playerHeight, settings.eyeHeight);
    settings.stepHeight = ClampFinite(
            settings.stepHeight,
            defaults.stepHeight,
            PreviewStepHeightMin,
            PreviewStepHeightMax);
    settings.jumpHeight = ClampFinite(
            settings.jumpHeight,
            defaults.jumpHeight,
            PreviewJumpHeightMin,
            PreviewJumpHeightMax);
    settings.headBobStrength = ClampFinite(
            settings.headBobStrength,
            defaults.headBobStrength,
            PreviewHeadBobStrengthMin,
            PreviewHeadBobStrengthMax);
    settings.headBobFrequency = ClampFinite(
            settings.headBobFrequency,
            defaults.headBobFrequency,
            PreviewHeadBobFrequencyMin,
            PreviewHeadBobFrequencyMax);
    settings.objectProbeDebugDrawMaxDistanceWorld = ClampFinite(
            settings.objectProbeDebugDrawMaxDistanceWorld,
            defaults.objectProbeDebugDrawMaxDistanceWorld,
            PreviewObjectProbeDebugDrawMaxDistanceMin,
            PreviewObjectProbeDebugDrawMaxDistanceMax);
    return settings;
}

SectorTopologySkySettings DefaultSectorTopologySkySettings()
{
    return SectorTopologySkySettings{};
}

SectorTopologySkySettings NormalizeSectorTopologySkySettings(SectorTopologySkySettings settings)
{
    const SectorTopologySkySettings defaults = DefaultSectorTopologySkySettings();
    if (!std::isfinite(settings.yawOffsetDegrees)) {
        settings.yawOffsetDegrees = defaults.yawOffsetDegrees;
    }
    if (!std::isfinite(settings.verticalOffset)) {
        settings.verticalOffset = defaults.verticalOffset;
    }
    settings.verticalScale = ClampFinite(
            settings.verticalScale,
            defaults.verticalScale,
            SkyVerticalScaleMin,
            SkyVerticalScaleMax);
    settings.topColor.a = 255;
    return settings;
}

SectorTopologyDirectionalLightSettings DefaultSectorTopologyDirectionalLightSettings()
{
    SectorTopologyDirectionalLightSettings settings;
    settings.directionToLight = NormalizeSectorTopologyDirectionalLightSettings(settings).directionToLight;
    return settings;
}

SectorTopologyDirectionalLightSettings NormalizeSectorTopologyDirectionalLightSettings(
        SectorTopologyDirectionalLightSettings settings)
{
    const Vector3 fallback{-0.35f, 0.80f, -0.25f};
    const float fallbackLength = std::sqrt(
            fallback.x * fallback.x + fallback.y * fallback.y + fallback.z * fallback.z);
    const Vector3 defaultDirection{
            fallback.x / fallbackLength,
            fallback.y / fallbackLength,
            fallback.z / fallbackLength
    };

    const Vector3 direction = settings.directionToLight;
    const float lengthSqr = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
    if (!std::isfinite(direction.x)
            || !std::isfinite(direction.y)
            || !std::isfinite(direction.z)
            || lengthSqr <= DirectionalLightMinLengthSqr) {
        settings.directionToLight = defaultDirection;
    } else {
        const float length = std::sqrt(lengthSqr);
        settings.directionToLight = Vector3{
                direction.x / length,
                direction.y / length,
                direction.z / length
        };
    }
    settings.color.a = 255;
    if (!std::isfinite(settings.intensity) || settings.intensity < 0.0f) {
        settings.intensity = 0.0f;
    }
    return settings;
}

SectorTopologyIndexes BuildSectorTopologyIndexes(const SectorTopologyMap& map)
{
    SectorTopologyIndexes indexes;

    for (size_t i = 0; i < map.vertices.size(); ++i) {
        indexes.vertexIndicesById[map.vertices[i].id].push_back(i);
    }
    for (size_t i = 0; i < map.lineDefs.size(); ++i) {
        indexes.lineDefIndicesById[map.lineDefs[i].id].push_back(i);
    }
    for (size_t i = 0; i < map.sideDefs.size(); ++i) {
        const SectorTopologySideDef& sideDef = map.sideDefs[i];
        indexes.sideDefIndicesById[sideDef.id].push_back(i);
        indexes.sideDefIndicesBySectorId[sideDef.sectorId].push_back(i);
        if (sideDef.side == SectorTopologySideKind::Front) {
            indexes.frontSideDefIndicesByLineDefId[sideDef.lineDefId].push_back(i);
        } else if (sideDef.side == SectorTopologySideKind::Back) {
            indexes.backSideDefIndicesByLineDefId[sideDef.lineDefId].push_back(i);
        }
    }
    for (size_t i = 0; i < map.sectors.size(); ++i) {
        indexes.sectorIndicesById[map.sectors[i].id].push_back(i);
    }

    return indexes;
}

bool IsValidSectorTopologyId(int id)
{
    return id > 0;
}

const char* SectorTopologySideKindName(SectorTopologySideKind side)
{
    switch (side) {
        case SectorTopologySideKind::Front:
            return "Front";
        case SectorTopologySideKind::Back:
            return "Back";
    }
    return "Unknown";
}

SectorTopologySideKind OppositeSectorTopologySideKind(SectorTopologySideKind side)
{
    return side == SectorTopologySideKind::Front
           ? SectorTopologySideKind::Back
           : SectorTopologySideKind::Front;
}

int AllocateSectorTopologyVertexId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.vertices);
}

int AllocateSectorTopologyLineDefId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.lineDefs);
}

int AllocateSectorTopologySideDefId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.sideDefs);
}

int AllocateSectorTopologySectorId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.sectors);
}

int AllocateSectorTopologyStaticLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.staticLights);
}

int AllocateSectorTopologyStaticSpotLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.staticSpotLights);
}

int AllocateSectorTopologyDynamicLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.dynamicPointLights);
}

int AllocateSectorTopologyDynamicSpotLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.dynamicSpotLights);
}

int AllocateSectorPlacedRuntimeObjectId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.runtimeObjects);
}

const SectorTopologyVertex* FindSectorTopologyVertex(const SectorTopologyMap& map, int id)
{
    return FindById(map.vertices, id);
}

SectorTopologyVertex* FindSectorTopologyVertex(SectorTopologyMap& map, int id)
{
    return FindById(map.vertices, id);
}

const SectorTopologyLineDef* FindSectorTopologyLineDef(const SectorTopologyMap& map, int id)
{
    return FindById(map.lineDefs, id);
}

SectorTopologyLineDef* FindSectorTopologyLineDef(SectorTopologyMap& map, int id)
{
    return FindById(map.lineDefs, id);
}

const SectorTopologySideDef* FindSectorTopologySideDef(const SectorTopologyMap& map, int id)
{
    return FindById(map.sideDefs, id);
}

SectorTopologySideDef* FindSectorTopologySideDef(SectorTopologyMap& map, int id)
{
    return FindById(map.sideDefs, id);
}

const SectorTopologySector* FindSectorTopologySector(const SectorTopologyMap& map, int id)
{
    return FindById(map.sectors, id);
}

SectorTopologySector* FindSectorTopologySector(SectorTopologyMap& map, int id)
{
    return FindById(map.sectors, id);
}

const SectorTopologyStaticPointLight* FindSectorTopologyStaticLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.staticLights, id);
}

SectorTopologyStaticPointLight* FindSectorTopologyStaticLight(SectorTopologyMap& map, int id)
{
    return FindById(map.staticLights, id);
}

bool RemoveSectorTopologyStaticLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.staticLights.begin(),
            map.staticLights.end(),
            [id](const SectorTopologyStaticPointLight& light) { return light.id == id; });
    if (found == map.staticLights.end()) {
        return false;
    }

    map.staticLights.erase(found);
    return true;
}

const SectorTopologyStaticSpotLight* FindSectorTopologyStaticSpotLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.staticSpotLights, id);
}

SectorTopologyStaticSpotLight* FindSectorTopologyStaticSpotLight(SectorTopologyMap& map, int id)
{
    return FindById(map.staticSpotLights, id);
}

bool RemoveSectorTopologyStaticSpotLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.staticSpotLights.begin(),
            map.staticSpotLights.end(),
            [id](const SectorTopologyStaticSpotLight& light) { return light.id == id; });
    if (found == map.staticSpotLights.end()) {
        return false;
    }

    map.staticSpotLights.erase(found);
    return true;
}

const SectorTopologyDynamicPointLight* FindSectorTopologyDynamicLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.dynamicPointLights, id);
}

SectorTopologyDynamicPointLight* FindSectorTopologyDynamicLight(SectorTopologyMap& map, int id)
{
    return FindById(map.dynamicPointLights, id);
}

bool RemoveSectorTopologyDynamicLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.dynamicPointLights.begin(),
            map.dynamicPointLights.end(),
            [id](const SectorTopologyDynamicPointLight& light) { return light.id == id; });
    if (found == map.dynamicPointLights.end()) {
        return false;
    }

    map.dynamicPointLights.erase(found);
    return true;
}

const SectorTopologyDynamicSpotLight* FindSectorTopologyDynamicSpotLight(const SectorTopologyMap& map, int id)
{
    return FindById(map.dynamicSpotLights, id);
}

SectorTopologyDynamicSpotLight* FindSectorTopologyDynamicSpotLight(SectorTopologyMap& map, int id)
{
    return FindById(map.dynamicSpotLights, id);
}

bool RemoveSectorTopologyDynamicSpotLight(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.dynamicSpotLights.begin(),
            map.dynamicSpotLights.end(),
            [id](const SectorTopologyDynamicSpotLight& light) { return light.id == id; });
    if (found == map.dynamicSpotLights.end()) {
        return false;
    }

    map.dynamicSpotLights.erase(found);
    return true;
}

const SectorPlacedRuntimeObject* FindSectorPlacedRuntimeObject(const SectorTopologyMap& map, int id)
{
    return FindById(map.runtimeObjects, id);
}

SectorPlacedRuntimeObject* FindSectorPlacedRuntimeObject(SectorTopologyMap& map, int id)
{
    return FindById(map.runtimeObjects, id);
}

bool RemoveSectorPlacedRuntimeObject(SectorTopologyMap& map, int id)
{
    if (!IsValidSectorTopologyId(id)) {
        return false;
    }

    const auto found = std::find_if(
            map.runtimeObjects.begin(),
            map.runtimeObjects.end(),
            [id](const SectorPlacedRuntimeObject& object) { return object.id == id; });
    if (found == map.runtimeObjects.end()) {
        return false;
    }

    map.runtimeObjects.erase(found);
    return true;
}

SectorResolvedDoorAnchor ResolveSectorDoorAnchor(
        const SectorTopologyMap& map,
        const SectorPlacedDoor& door)
{
    SectorResolvedDoorAnchor resolved;

    const SectorDoorAnchor& anchor = door.anchor;
    resolved.lineDefId = anchor.lineDefId;
    resolved.frontSectorId = anchor.frontSectorId;
    resolved.backSectorId = anchor.backSectorId;
    resolved.frontSideDefId = anchor.frontSideDefId;
    resolved.backSideDefId = anchor.backSideDefId;
    resolved.width = door.width;
    resolved.height = door.height;

    const auto fail = [&resolved](std::string diagnostic) {
        resolved.valid = false;
        resolved.diagnostic = std::move(diagnostic);
        return resolved;
    };

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, anchor.lineDefId);
    if (lineDef == nullptr) {
        return fail("door anchor linedef is missing");
    }
    if (!IsValidSectorTopologyId(lineDef->frontSideDefId)
            || !IsValidSectorTopologyId(lineDef->backSideDefId)) {
        return fail("door anchor linedef is not a two-sided portal");
    }
    if (lineDef->frontSideDefId != anchor.frontSideDefId
            || lineDef->backSideDefId != anchor.backSideDefId) {
        return fail("door anchor sidedef IDs no longer match the linedef");
    }

    const SectorTopologySideDef* frontSide = FindSectorTopologySideDef(map, lineDef->frontSideDefId);
    const SectorTopologySideDef* backSide = FindSectorTopologySideDef(map, lineDef->backSideDefId);
    if (frontSide == nullptr || backSide == nullptr) {
        return fail("door anchor sidedef is missing");
    }
    if (frontSide->lineDefId != lineDef->id
            || frontSide->side != SectorTopologySideKind::Front
            || backSide->lineDefId != lineDef->id
            || backSide->side != SectorTopologySideKind::Back) {
        return fail("door anchor sidedefs are not the linedef front/back pair");
    }
    if (frontSide->sectorId != anchor.frontSectorId
            || backSide->sectorId != anchor.backSectorId) {
        return fail("door anchor sector pair no longer matches the portal");
    }

    const SectorTopologySector* frontSector = FindSectorTopologySector(map, frontSide->sectorId);
    const SectorTopologySector* backSector = FindSectorTopologySector(map, backSide->sectorId);
    if (frontSector == nullptr || backSector == nullptr) {
        return fail("door anchor sector is missing");
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(map, *lineDef, start, end)) {
        return fail("door anchor linedef vertex is missing");
    }

    resolved.endpointA = SectorCoordToWorldPosition2Local(start->x, start->y);
    resolved.endpointB = SectorCoordToWorldPosition2Local(end->x, end->y);
    const Vector2 delta{
            resolved.endpointB.x - resolved.endpointA.x,
            resolved.endpointB.y - resolved.endpointA.y
    };
    const float lengthSqr = delta.x * delta.x + delta.y * delta.y;
    if (!std::isfinite(lengthSqr) || lengthSqr <= 0.0f) {
        return fail("door anchor portal has zero length");
    }

    resolved.portalWidth = std::sqrt(lengthSqr);
    resolved.tangent = Vector2{
            delta.x / resolved.portalWidth,
            delta.y / resolved.portalWidth
    };
    resolved.midpoint = Vector2{
            (resolved.endpointA.x + resolved.endpointB.x) * 0.5f,
            (resolved.endpointA.y + resolved.endpointB.y) * 0.5f
    };

    // Front sidedefs use the linedef start->end direction; back sidedefs use end->start.
    // A sidedef's sector lies on the positive side of that directed edge. Resolve and
    // verify the door normal so positive normalOffset always moves front sector -> back sector.
    Vector2 candidateNormal{resolved.tangent.y, -resolved.tangent.x};
    const Vector2 backTangent{-resolved.tangent.x, -resolved.tangent.y};
    const auto pointsTowardBack = [&](Vector2 normal) {
        const Vector2 probe{
                resolved.midpoint.x + normal.x * DoorAnchorSideProbeDistance,
                resolved.midpoint.y + normal.y * DoorAnchorSideProbeDistance};
        return ProbeIsOutsideDirectedSide(resolved.endpointA, resolved.tangent, probe)
                && ProbeIsInsideDirectedSide(resolved.endpointB, backTangent, probe);
    };
    if (!pointsTowardBack(candidateNormal)) {
        candidateNormal = Vector2{-candidateNormal.x, -candidateNormal.y};
        if (!pointsTowardBack(candidateNormal)) {
            return fail("door anchor normal could not be verified against portal front/back sides");
        }
    }
    resolved.normal = candidateNormal;

    resolved.openBottom = SectorAuthoringToWorldDistance(std::max(frontSector->floorZ, backSector->floorZ));
    resolved.openTop = SectorAuthoringToWorldDistance(std::min(frontSector->ceilingZ, backSector->ceilingZ));
    if (!std::isfinite(resolved.openBottom)
            || !std::isfinite(resolved.openTop)
            || resolved.openBottom >= resolved.openTop) {
        return fail("door anchor portal has no positive vertical opening");
    }

    resolved.portalHeight = resolved.openTop - resolved.openBottom;
    if (resolved.width == 0.0f) {
        resolved.width = resolved.portalWidth;
    }
    if (resolved.height == 0.0f) {
        resolved.height = resolved.portalHeight;
    }
    resolved.valid = true;
    resolved.diagnostic.clear();
    return resolved;
}

const SectorTopologySideDef* FindOppositeSectorTopologySideDef(
        const SectorTopologyMap& map,
        int sideDefId)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(map, sideDefId);
    if (sideDef == nullptr) {
        return nullptr;
    }

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(map, sideDef->lineDefId);
    if (lineDef == nullptr) {
        return nullptr;
    }

    int oppositeSideDefId = -1;
    SectorTopologySideKind expectedOppositeKind = SectorTopologySideKind::Front;
    if (sideDef->side == SectorTopologySideKind::Front) {
        if (lineDef->frontSideDefId != sideDef->id) {
            return nullptr;
        }
        oppositeSideDefId = lineDef->backSideDefId;
        expectedOppositeKind = SectorTopologySideKind::Back;
    } else if (sideDef->side == SectorTopologySideKind::Back) {
        if (lineDef->backSideDefId != sideDef->id) {
            return nullptr;
        }
        oppositeSideDefId = lineDef->frontSideDefId;
        expectedOppositeKind = SectorTopologySideKind::Front;
    } else {
        return nullptr;
    }

    const SectorTopologySideDef* opposite = FindSectorTopologySideDef(map, oppositeSideDefId);
    if (opposite == nullptr
        || opposite->lineDefId != lineDef->id
        || opposite->side != expectedOppositeKind) {
        return nullptr;
    }
    return opposite;
}

bool GetSectorTopologyLineVertices(
        const SectorTopologyMap& map,
        const SectorTopologyLineDef& line,
        const SectorTopologyVertex*& outStart,
        const SectorTopologyVertex*& outEnd)
{
    outStart = nullptr;
    outEnd = nullptr;

    const SectorTopologyVertex* start = FindSectorTopologyVertex(map, line.startVertexId);
    const SectorTopologyVertex* end = FindSectorTopologyVertex(map, line.endVertexId);
    if (start == nullptr || end == nullptr) {
        return false;
    }

    outStart = start;
    outEnd = end;
    return true;
}

} // namespace game
