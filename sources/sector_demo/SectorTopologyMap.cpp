#include "sector_demo/SectorTopologyMap.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
constexpr float SkyVerticalScaleMin = 0.01f;
constexpr float SkyVerticalScaleMax = 100.0f;
constexpr float DirectionalLightMinLengthSqr = 0.000001f;

float ClampFinite(float value, float fallback, float minValue, float maxValue)
{
    if (!std::isfinite(value)) {
        value = fallback;
    }
    return std::clamp(value, minValue, maxValue);
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

int AllocateSectorTopologyDynamicLightId(const SectorTopologyMap& map)
{
    return AllocateNextId(map.dynamicPointLights);
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
