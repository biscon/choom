#include "sector_demo/renderer/SectorStaticSpecularLighting.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorRectLight.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

namespace game {
namespace {

constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
constexpr float ReceiverBoundsPadding = 0.05f;

int GetShaderArrayLocation(Shader shader, const char* name)
{
    const int baseLocation = GetShaderLocation(shader, name);
    if (baseLocation >= 0) return baseLocation;
    const std::string firstElement = std::string(name) + "[0]";
    return GetShaderLocation(shader, firstElement.c_str());
}

struct ScoredStaticSpecularLight {
    const SectorStaticSpecularLightSource* source = nullptr;
    float score = -1.0f;
};

bool IsFinite(Vector3 value)
{
    return std::isfinite(value.x)
            && std::isfinite(value.y)
            && std::isfinite(value.z);
}

float DistanceSquared(Vector3 a, Vector3 b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return x * x + y * y + z * z;
}

SectorAabb3 PaddedBounds(const SectorReceiverBounds& bounds)
{
    return SectorAabb3{
            Vector3{
                    bounds.min.x - ReceiverBoundsPadding,
                    bounds.min.y - ReceiverBoundsPadding,
                    bounds.min.z - ReceiverBoundsPadding},
            Vector3{
                    bounds.max.x + ReceiverBoundsPadding,
                    bounds.max.y + ReceiverBoundsPadding,
                    bounds.max.z + ReceiverBoundsPadding}};
}

bool IsValidReceiverBounds(const SectorReceiverBounds& bounds)
{
    return IsValidSectorAabb3(SectorAabb3{bounds.min, bounds.max});
}

bool SphereOverlapsBounds(
        const SectorStaticSpecularLightSource& light,
        const SectorReceiverBounds& bounds)
{
    if (!IsValidReceiverBounds(bounds)) return true;
    const Vector3 closest = ClosestPointOnSectorAabb3(
            PaddedBounds(bounds), light.position);
    return DistanceSquared(light.position, closest)
            <= light.radius * light.radius;
}

float SmoothStep(float edge0, float edge1, float value)
{
    if (std::fabs(edge1 - edge0) <= 0.000001f) {
        return value >= edge1 ? 1.0f : 0.0f;
    }
    const float t = std::clamp(
            (value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float StaticSpecularLightScore(
        const SectorStaticSpecularLightSource& light,
        const SectorReceiverBounds& receiverBounds)
{
    if (!(light.radius > 0.0f) || !(light.intensity > 0.0f)
            || !std::isfinite(light.radius)
            || !std::isfinite(light.intensity)) {
        return -1.0f;
    }

    const SectorAabb3 bounds = IsValidReceiverBounds(receiverBounds)
            ? PaddedBounds(receiverBounds)
            : SectorAabb3FromPoint(receiverBounds.min);
    const Vector3 closest = ClosestPointOnSectorAabb3(
            bounds, light.position);
    const float distanceSquared = DistanceSquared(light.position, closest);
    if (!std::isfinite(distanceSquared)
            || distanceSquared >= light.radius * light.radius) {
        return -1.0f;
    }
    const float distance = std::sqrt(std::max(distanceSquared, 0.0f));
    float attenuation = std::clamp(
            1.0f - distance / light.radius, 0.0f, 1.0f);
    attenuation *= attenuation;

    float cone = 1.0f;
    if (light.kind == SectorStaticSpecularLightKind::Spot) {
        const Vector3 center = SectorAabb3Center(bounds);
        Vector3 fromLight = Vector3Subtract(center, light.position);
        const float lengthSquared = Vector3LengthSqr(fromLight);
        if (!(lengthSquared > 0.00000001f)) {
            fromLight = light.direction;
        } else {
            fromLight = Vector3Scale(
                    fromLight, 1.0f / std::sqrt(lengthSquared));
        }
        cone = SmoothStep(
                light.outerConeCos,
                light.innerConeCos,
                Vector3DotProduct(light.direction, fromLight));
        if (!(cone > 0.0f)) return -1.0f;
    } else if (light.kind == SectorStaticSpecularLightKind::Rect) {
        const Vector3 center = SectorAabb3Center(bounds);
        cone = SectorRectLightStartFeatherAttenuation(
                Vector3DotProduct(
                        Vector3Subtract(center, light.position),
                        light.direction),
                light.startFeather);
        if (!(cone > 0.0f)) return -1.0f;
    }

    const float brightness = std::max(
            light.color.x, std::max(light.color.y, light.color.z));
    const float score = light.intensity * brightness * attenuation * cone;
    return std::isfinite(score) && score > 0.0f ? score : -1.0f;
}

bool Better(
        const ScoredStaticSpecularLight& left,
        const ScoredStaticSpecularLight& right)
{
    if (left.score != right.score) return left.score > right.score;
    if (left.source->lightId != right.source->lightId) {
        return left.source->lightId < right.source->lightId;
    }
    return static_cast<int>(left.source->kind)
            < static_cast<int>(right.source->kind);
}

bool SourceVisible(
        const SectorStaticSpecularLightSource& source,
        const RuntimePortalVisibilityResult& visibility)
{
    return source.ownerSectorId <= 0
            || !visibility.validStartSector
            || visibility.fallbackDrawAll
            || ShouldDrawRuntimeSectorForVisibility(
                    source.ownerSectorId, visibility);
}

const SectorStaticSpecularSectorCandidates* FindSectorCandidates(
        const SectorStaticSpecularLightState& state,
        int sectorId)
{
    const auto it = std::lower_bound(
            state.sectorCandidates.begin(),
            state.sectorCandidates.end(),
            sectorId,
            [](const SectorStaticSpecularSectorCandidates& entry, int id) {
                return entry.sectorId < id;
            });
    return it != state.sectorCandidates.end() && it->sectorId == sectorId
            ? &*it
            : nullptr;
}

bool MakePointSource(
        const SectorTopologyStaticPointLight& authored,
        const SectorCollisionWorld* sectorLookupWorld,
        SectorStaticSpecularLightSource& outSource)
{
    const Vector3 position = SectorAuthoringToWorldPosition(authored.position);
    const float radius = SectorAuthoringToWorldDistance(authored.radius);
    if (authored.id <= 0 || !IsFinite(position)
            || !std::isfinite(radius) || radius <= 0.0f
            || !std::isfinite(authored.intensity)
            || authored.intensity <= 0.0f) {
        return false;
    }
    outSource = SectorStaticSpecularLightSource{};
    outSource.lightId = authored.id;
    outSource.ownerSectorId = sectorLookupWorld != nullptr
            ? sectorLookupWorld->FindSectorContainingPoint(
                    Vector2{position.x, position.z})
            : 0;
    outSource.position = position;
    outSource.color = engine::SrgbColorBytesToLinearSceneRgb(authored.color);
    outSource.radius = radius;
    outSource.intensity = authored.intensity;
    return true;
}

bool MakeSpotSource(
        const SectorTopologyStaticSpotLight& authored,
        const SectorCollisionWorld* sectorLookupWorld,
        SectorStaticSpecularLightSource& outSource)
{
    const Vector3 position = SectorAuthoringToWorldPosition(authored.position);
    const Vector3 target = SectorAuthoringToWorldPosition(authored.target);
    const float radius = SectorAuthoringToWorldDistance(authored.range);
    if (authored.id <= 0 || !IsFinite(position) || !IsFinite(target)
            || !std::isfinite(radius) || radius <= 0.0f
            || !std::isfinite(authored.intensity)
            || authored.intensity <= 0.0f) {
        return false;
    }
    Vector3 direction = Vector3Subtract(target, position);
    const float directionLengthSquared = Vector3LengthSqr(direction);
    direction = directionLengthSquared > 0.00000001f
            ? Vector3Scale(direction, 1.0f / std::sqrt(directionLengthSquared))
            : Vector3{0.0f, -1.0f, 0.0f};

    const float innerDegrees = std::clamp(
            authored.innerConeDegrees, 0.0f, 179.0f);
    const float outerDegrees = std::clamp(
            std::max(authored.outerConeDegrees, innerDegrees),
            0.0f,
            179.0f);
    outSource = SectorStaticSpecularLightSource{};
    outSource.lightId = authored.id;
    outSource.ownerSectorId = sectorLookupWorld != nullptr
            ? sectorLookupWorld->FindSectorContainingPoint(
                    Vector2{position.x, position.z})
            : 0;
    outSource.kind = SectorStaticSpecularLightKind::Spot;
    outSource.position = position;
    outSource.direction = direction;
    outSource.color = engine::SrgbColorBytesToLinearSceneRgb(authored.color);
    outSource.radius = radius;
    outSource.intensity = authored.intensity;
    outSource.innerConeCos = std::cos(innerDegrees * DegreesToRadians);
    outSource.outerConeCos = std::cos(outerDegrees * DegreesToRadians);
    return true;
}

bool MakeRectSource(
        const SectorTopologyStaticRectLight& authored,
        const SectorCollisionWorld* sectorLookupWorld,
        SectorStaticSpecularLightSource& outSource)
{
    const Vector3 position = SectorAuthoringToWorldPosition(authored.position);
    const Vector3 target = SectorAuthoringToWorldPosition(authored.target);
    const float radius = SectorAuthoringToWorldDistance(authored.range);
    if (authored.id <= 0 || !IsFinite(position) || !IsFinite(target)
            || !std::isfinite(radius) || radius <= 0.0f
            || !std::isfinite(authored.intensity) || authored.intensity <= 0.0f) return false;
    outSource = {};
    outSource.lightId = authored.id;
    outSource.ownerSectorId = sectorLookupWorld != nullptr
            ? sectorLookupWorld->FindSectorContainingPoint({position.x, position.z}) : 0;
    outSource.kind = SectorStaticSpecularLightKind::Rect;
    outSource.position = position;
    outSource.direction = Vector3Normalize(Vector3Subtract(target, position));
    if (!IsFinite(outSource.direction)) outSource.direction = {0.0f, -1.0f, 0.0f};
    outSource.color = engine::SrgbColorBytesToLinearSceneRgb(authored.color);
    outSource.radius = radius;
    outSource.intensity = authored.intensity;
    outSource.startFeather = std::clamp(
            SectorAuthoringToWorldDistance(authored.startFeather),
            0.0f,
            radius);
    return true;
}

} // namespace

void ResetSectorStaticSpecularLights(
        SectorStaticSpecularLightState& state)
{
    state.sources.clear();
    state.sectorCandidates.clear();
}

void RebuildSectorStaticSpecularLights(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld,
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        SectorStaticSpecularLightState& outState)
{
    outState.sources.clear();
    outState.sources.reserve(
            map.staticLights.size() + map.staticSpotLights.size() + map.staticRectLights.size());
    for (const SectorTopologyStaticPointLight& authored : map.staticLights) {
        SectorStaticSpecularLightSource source;
        if (MakePointSource(authored, sectorLookupWorld, source)) {
            outState.sources.push_back(source);
        }
    }
    for (const SectorTopologyStaticSpotLight& authored : map.staticSpotLights) {
        SectorStaticSpecularLightSource source;
        if (MakeSpotSource(authored, sectorLookupWorld, source)) {
            outState.sources.push_back(source);
        }
    }
    for (const SectorTopologyStaticRectLight& authored : map.staticRectLights) {
        SectorStaticSpecularLightSource source;
        if (MakeRectSource(authored, sectorLookupWorld, source)) {
            outState.sources.push_back(source);
        }
    }

    outState.sectorCandidates.clear();
    outState.sectorCandidates.reserve(sectorReceiverBounds.size());
    for (const SectorReceiverBounds& bounds : sectorReceiverBounds) {
        if (bounds.sectorId <= 0 || !IsValidReceiverBounds(bounds)) continue;
        auto it = std::lower_bound(
                outState.sectorCandidates.begin(),
                outState.sectorCandidates.end(),
                bounds.sectorId,
                [](const SectorStaticSpecularSectorCandidates& entry, int id) {
                    return entry.sectorId < id;
                });
        if (it == outState.sectorCandidates.end()
                || it->sectorId != bounds.sectorId) {
            it = outState.sectorCandidates.insert(
                    it,
                    SectorStaticSpecularSectorCandidates{bounds.sectorId, {}});
        }
        for (std::size_t sourceIndex = 0;
                sourceIndex < outState.sources.size();
                ++sourceIndex) {
            if (!SphereOverlapsBounds(outState.sources[sourceIndex], bounds)) {
                continue;
            }
            if (std::find(
                        it->sourceIndices.begin(),
                        it->sourceIndices.end(),
                        sourceIndex) == it->sourceIndices.end()) {
                it->sourceIndices.push_back(sourceIndex);
            }
        }
    }
}

SectorReceiverBounds TransformSectorStaticSpecularReceiverBounds(
        BoundingBox localBounds,
        Matrix transform,
        int sectorId,
        Vector3 fallbackPosition)
{
    SectorAabb3 bounds = EmptySectorAabb3();
    for (float x : {localBounds.min.x, localBounds.max.x}) {
        for (float y : {localBounds.min.y, localBounds.max.y}) {
            for (float z : {localBounds.min.z, localBounds.max.z}) {
                ExpandSectorAabb3(
                        bounds,
                        Vector3Transform(Vector3{x, y, z}, transform));
            }
        }
    }
    if (!IsValidSectorAabb3(bounds)) {
        bounds = SectorAabb3FromPoint(fallbackPosition);
    }
    return SectorReceiverBounds{sectorId, bounds.min, bounds.max};
}

SectorStaticSpecularLightContext SelectSectorStaticSpecularLights(
        const SectorStaticSpecularLightState& state,
        const SectorReceiverBounds& receiverBounds,
        int receiverSectorId,
        const RuntimePortalVisibilityResult& visibility,
        bool enabled)
{
    SectorStaticSpecularLightContext result;
    if (!enabled || state.sources.empty()) return result;

    std::array<ScoredStaticSpecularLight, MaxStaticSpecularLights> selected{};
    int selectedCount = 0;
    const auto consider = [&](std::size_t sourceIndex) {
        if (sourceIndex >= state.sources.size()) return;
        const SectorStaticSpecularLightSource& source =
                state.sources[sourceIndex];
        if (!SourceVisible(source, visibility)) return;
        const float score = StaticSpecularLightScore(source, receiverBounds);
        if (!(score > 0.0f)) return;

        const ScoredStaticSpecularLight scored{&source, score};
        int insertAt = selectedCount;
        for (int index = 0; index < selectedCount; ++index) {
            if (Better(scored, selected[static_cast<std::size_t>(index)])) {
                insertAt = index;
                break;
            }
        }
        if (insertAt >= static_cast<int>(MaxStaticSpecularLights)) return;
        const int shiftEnd = std::min(
                selectedCount,
                static_cast<int>(MaxStaticSpecularLights) - 1);
        for (int index = shiftEnd; index > insertAt; --index) {
            selected[static_cast<std::size_t>(index)] =
                    selected[static_cast<std::size_t>(index - 1)];
        }
        selected[static_cast<std::size_t>(insertAt)] = scored;
        selectedCount = std::min(
                selectedCount + 1,
                static_cast<int>(MaxStaticSpecularLights));
    };

    const SectorStaticSpecularSectorCandidates* candidates =
            FindSectorCandidates(state, receiverSectorId);
    if (candidates != nullptr) {
        for (std::size_t sourceIndex : candidates->sourceIndices) {
            consider(sourceIndex);
        }
    } else {
        for (std::size_t sourceIndex = 0;
                sourceIndex < state.sources.size();
                ++sourceIndex) {
            consider(sourceIndex);
        }
    }

    result.lightCount = selectedCount;
    for (int index = 0; index < selectedCount; ++index) {
        const SectorStaticSpecularLightSource& source =
                *selected[static_cast<std::size_t>(index)].source;
        const std::size_t outputIndex = static_cast<std::size_t>(index);
        result.lightIds[outputIndex] = source.lightId;
        result.positions[outputIndex] = source.position;
        result.colors[outputIndex] = source.color;
        result.radii[outputIndex] = source.radius;
        result.intensities[outputIndex] = source.intensity;
        result.types[outputIndex] = static_cast<int>(source.kind);
        result.directions[outputIndex] = source.direction;
        result.innerConeCos[outputIndex] = source.innerConeCos;
        result.outerConeCos[outputIndex] = source.outerConeCos;
        result.startFeathers[outputIndex] = source.startFeather;
    }
    return result;
}

SectorStaticSpecularShaderLocations GetSectorStaticSpecularShaderLocations(
        Shader shader)
{
    SectorStaticSpecularShaderLocations locations;
    locations.lightCount = GetShaderLocation(shader, "staticSpecularLightCount");
    locations.positions = GetShaderArrayLocation(shader, "staticSpecularLightPositions");
    locations.colors = GetShaderArrayLocation(shader, "staticSpecularLightColors");
    locations.radii = GetShaderArrayLocation(shader, "staticSpecularLightRadii");
    locations.intensities = GetShaderArrayLocation(shader, "staticSpecularLightIntensities");
    locations.types = GetShaderArrayLocation(shader, "staticSpecularLightTypes");
    locations.directions = GetShaderArrayLocation(shader, "staticSpecularLightDirections");
    locations.innerConeCos = GetShaderArrayLocation(shader, "staticSpecularLightInnerConeCos");
    locations.outerConeCos = GetShaderArrayLocation(shader, "staticSpecularLightOuterConeCos");
    locations.startFeathers = GetShaderArrayLocation(shader, "staticSpecularLightStartFeathers");
    return locations;
}

void UploadSectorStaticSpecularLights(
        Shader shader,
        const SectorStaticSpecularShaderLocations& locations,
        const SectorStaticSpecularLightContext& context)
{
    if (locations.lightCount >= 0) {
        SetShaderValue(
                shader,
                locations.lightCount,
                &context.lightCount,
                SHADER_UNIFORM_INT);
    }
    if (context.lightCount <= 0) return;
    if (locations.positions >= 0) SetShaderValueV(shader, locations.positions, context.positions.data(), SHADER_UNIFORM_VEC3, context.lightCount);
    if (locations.colors >= 0) SetShaderValueV(shader, locations.colors, context.colors.data(), SHADER_UNIFORM_VEC3, context.lightCount);
    if (locations.radii >= 0) SetShaderValueV(shader, locations.radii, context.radii.data(), SHADER_UNIFORM_FLOAT, context.lightCount);
    if (locations.intensities >= 0) SetShaderValueV(shader, locations.intensities, context.intensities.data(), SHADER_UNIFORM_FLOAT, context.lightCount);
    if (locations.types >= 0) SetShaderValueV(shader, locations.types, context.types.data(), SHADER_UNIFORM_INT, context.lightCount);
    if (locations.directions >= 0) SetShaderValueV(shader, locations.directions, context.directions.data(), SHADER_UNIFORM_VEC3, context.lightCount);
    if (locations.innerConeCos >= 0) SetShaderValueV(shader, locations.innerConeCos, context.innerConeCos.data(), SHADER_UNIFORM_FLOAT, context.lightCount);
    if (locations.outerConeCos >= 0) SetShaderValueV(shader, locations.outerConeCos, context.outerConeCos.data(), SHADER_UNIFORM_FLOAT, context.lightCount);
    if (locations.startFeathers >= 0) SetShaderValueV(shader, locations.startFeathers, context.startFeathers.data(), SHADER_UNIFORM_FLOAT, context.lightCount);
}

} // namespace game
