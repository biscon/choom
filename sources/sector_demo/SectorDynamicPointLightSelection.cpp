#include "sector_demo/SectorDynamicPointLightSelection.h"

#include "engine/render/ColorTransfer.h"
#include "sector_demo/SectorBounds.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorMath.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorTopologyTypes.h"
#include "sector_demo/SectorUnits.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {

namespace {

constexpr float ReceiverBoundsPadding = 0.05f;
constexpr float DynamicPointLightHysteresisReplacementFactor = 1.2f;
constexpr float DynamicLightFlickerTargetExponent = 3.0f;
constexpr uint32_t DynamicLightFlickerSegmentSalt = 0x9e3779b9u;
constexpr uint32_t DynamicLightFlickerPhaseSalt = 0x85ebca6bu;
constexpr float DegreesToRadians = 3.14159265358979323846f / 180.0f;
constexpr float ShadowNearPlane = 0.05f;

struct ScoredDynamicPointLightCandidate {
    const SectorPreviewDynamicPointLightSource* source = nullptr;
    float score = -1.0f;
    bool previouslySelected = false;
};

struct ScoredDynamicSpotLightShadowCandidate {
    const SectorPreviewDynamicPointLightUniform* light = nullptr;
    int dynamicLightIndex = -1;
    float score = -1.0f;
};

bool ContainsSectorId(const std::vector<int>& sectorIds, int sectorId)
{
    return std::find(sectorIds.begin(), sectorIds.end(), sectorId) != sectorIds.end();
}

bool ContainsLightId(const std::vector<int>& lightIds, int lightId)
{
    return std::find(lightIds.begin(), lightIds.end(), lightId) != lightIds.end();
}

float DistanceSq(Vector3 a, Vector3 b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

Vector3 ClampToBounds(Vector3 value, const SectorReceiverBounds& bounds)
{
    const SectorAabb3 paddedBounds{
            Vector3{
                    bounds.min.x - ReceiverBoundsPadding,
                    bounds.min.y - ReceiverBoundsPadding,
                    bounds.min.z - ReceiverBoundsPadding},
            Vector3{
                    bounds.max.x + ReceiverBoundsPadding,
                    bounds.max.y + ReceiverBoundsPadding,
                    bounds.max.z + ReceiverBoundsPadding}};
    return ClosestPointOnSectorAabb3(paddedBounds, value);
}

bool IsValidReceiverBounds(const SectorReceiverBounds& bounds)
{
    return bounds.sectorId > 0
            && IsValidSectorAabb3(SectorAabb3{bounds.min, bounds.max});
}

bool ShouldUseAllReceiverBounds(const RuntimePortalVisibilityResult& visibility)
{
    return !visibility.validStartSector || visibility.fallbackDrawAll;
}

void BuildRelevantReceiverBounds(
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::vector<const SectorReceiverBounds*>& outBounds)
{
    outBounds.clear();
    const bool useAll = ShouldUseAllReceiverBounds(visibility);
    outBounds.reserve(receiverBounds.size());
    for (const SectorReceiverBounds& bounds : receiverBounds) {
        if (!IsValidReceiverBounds(bounds)) {
            continue;
        }
        if (useAll || ContainsSectorId(visibility.visibleSectorIds, bounds.sectorId)) {
            outBounds.push_back(&bounds);
        }
    }
}

bool SphereOverlapsBounds(const SectorPreviewDynamicPointLightUniform& light, const SectorReceiverBounds& bounds)
{
    const Vector3 closest = ClampToBounds(light.position, bounds);
    return DistanceSq(light.position, closest) <= light.radius * light.radius;
}

float NearestDistanceToBounds(
        const SectorPreviewDynamicPointLightUniform& light,
        const std::vector<const SectorReceiverBounds*>& receiverBounds)
{
    float bestDistanceSq = std::numeric_limits<float>::max();
    for (const SectorReceiverBounds* bounds : receiverBounds) {
        const Vector3 closest = ClampToBounds(light.position, *bounds);
        const float distanceSq = DistanceSq(light.position, closest);
        if (std::isfinite(distanceSq)) {
            bestDistanceSq = std::min(bestDistanceSq, distanceSq);
        }
    }

    return bestDistanceSq == std::numeric_limits<float>::max()
            ? std::numeric_limits<float>::infinity()
            : std::sqrt(bestDistanceSq);
}

float DynamicPointLightBrightness(Vector3 color)
{
    return std::max(color.x, std::max(color.y, color.z));
}

Vector3 NormalizeOrFallback(Vector3 value, Vector3 fallback)
{
    return NormalizeVector3OrFallback(value, fallback, 0.00000001f);
}

Vector3 SpotlightShadowUpVector(Vector3 direction)
{
    const Vector3 worldUp{0.0f, 1.0f, 0.0f};
    const Vector3 worldForward{0.0f, 0.0f, 1.0f};
    return std::fabs(Vector3DotProduct(direction, worldUp)) > 0.98f ? worldForward : worldUp;
}

float ConeCosine(float degrees)
{
    return std::cos(std::clamp(degrees, 0.0f, 179.0f) * DegreesToRadians);
}

float DynamicPointLightSelectionScore(
        const SectorPreviewDynamicPointLightUniform& light,
        const std::vector<const SectorReceiverBounds*>& receiverBounds)
{
    if (light.radius <= 0.0f || !std::isfinite(light.radius)) {
        return -1.0f;
    }

    const float brightness = DynamicPointLightBrightness(light.color);
    if (receiverBounds.empty()) {
        const float score = light.intensity * brightness;
        return std::isfinite(score) ? score : -1.0f;
    }

    const float distance = NearestDistanceToBounds(light, receiverBounds);
    if (!std::isfinite(distance)) {
        return -1.0f;
    }

    const float atten = std::max(0.0f, 1.0f - (distance / light.radius));
    const float score = light.intensity * brightness * atten * atten;
    return std::isfinite(score) ? score : -1.0f;
}

bool BetterScoredDynamicPointLight(
        const ScoredDynamicPointLightCandidate& lhs,
        const ScoredDynamicPointLightCandidate& rhs)
{
    if (lhs.score != rhs.score) {
        return lhs.score > rhs.score;
    }
    return lhs.source->lightId < rhs.source->lightId;
}

bool BetterScoredDynamicSpotLightShadowCandidate(
        const ScoredDynamicSpotLightShadowCandidate& lhs,
        const ScoredDynamicSpotLightShadowCandidate& rhs)
{
    if (lhs.light->shadowPriority != rhs.light->shadowPriority) {
        return lhs.light->shadowPriority > rhs.light->shadowPriority;
    }
    if (lhs.score != rhs.score) {
        return lhs.score > rhs.score;
    }
    return lhs.light->lightId < rhs.light->lightId;
}

int FindSelectedLightIndex(
        const std::vector<ScoredDynamicPointLightCandidate>& selected,
        int lightId)
{
    for (std::size_t i = 0; i < selected.size(); ++i) {
        if (selected[i].source->lightId == lightId) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int FindWeakestPreviouslySelectedLightIndex(
        const std::vector<ScoredDynamicPointLightCandidate>& selected)
{
    int weakestIndex = -1;
    for (std::size_t i = 0; i < selected.size(); ++i) {
        if (!selected[i].previouslySelected) {
            continue;
        }
        if (weakestIndex < 0
                || selected[i].score < selected[static_cast<std::size_t>(weakestIndex)].score
                || (selected[i].score == selected[static_cast<std::size_t>(weakestIndex)].score
                    && selected[i].source->lightId
                            > selected[static_cast<std::size_t>(weakestIndex)].source->lightId)) {
            weakestIndex = static_cast<int>(i);
        }
    }
    return weakestIndex;
}

uint32_t MixDynamicLightFlickerBits(uint32_t value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float HashDynamicLightFlicker01(int lightId, int segment, uint32_t salt)
{
    uint32_t value = static_cast<uint32_t>(lightId);
    value ^= static_cast<uint32_t>(segment) + DynamicLightFlickerSegmentSalt + (value << 6u) + (value >> 2u);
    value ^= salt + (value << 6u) + (value >> 2u);
    value = MixDynamicLightFlickerBits(value);
    return static_cast<float>(value >> 8u) * (1.0f / 16777215.0f);
}

float DynamicLightFlickerTarget(int lightId, int segment, float flickerAmount)
{
    const float r = HashDynamicLightFlicker01(lightId, segment, DynamicLightFlickerSegmentSalt);
    const float dip = std::pow(r, DynamicLightFlickerTargetExponent);
    return std::clamp(1.0f - flickerAmount * dip, 0.0f, 1.0f);
}

float SmoothStep(float edge0, float edge1, float value)
{
    if (edge0 == edge1) {
        return value < edge1 ? 0.0f : 1.0f;
    }
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return std::isfinite(t) ? SmoothStep01(t) : t;
}

} // namespace

float EvaluateDynamicLightFlickerMultiplier(
        int lightId,
        float runtimeSeconds,
        float flickerSpeed,
        float flickerAmount)
{
    if (!std::isfinite(runtimeSeconds) || !std::isfinite(flickerSpeed) || !std::isfinite(flickerAmount)) {
        return 1.0f;
    }

    const float amount = ClampDynamicLightFlickerAmount(flickerAmount);
    if (amount <= 0.0f) {
        return 1.0f;
    }

    const float speed = ClampDynamicLightFlickerSpeed(flickerSpeed);
    const float rateHz = DynamicLightFlickerBaseRateHz * speed;
    const float stablePhase = HashDynamicLightFlicker01(lightId, 0, DynamicLightFlickerPhaseSalt);
    const float x = runtimeSeconds * rateHz + stablePhase;
    const float segmentFloat = std::floor(x);
    const int segment = static_cast<int>(segmentFloat);
    const float u = x - segmentFloat;

    const float targetA = DynamicLightFlickerTarget(lightId, segment, amount);
    const float targetB = DynamicLightFlickerTarget(lightId, segment + 1, amount);
    const float transitionWeight = SmoothStep(0.0f, DynamicLightFlickerTransitionFraction, u);
    const float multiplier = targetA + (targetB - targetA) * transitionWeight;
    return std::isfinite(multiplier) ? std::clamp(multiplier, 0.0f, 1.0f) : 1.0f;
}

float DynamicLightEffectiveUploadIntensity(
        const SectorPreviewDynamicPointLightUniform& light,
        float runtimeSeconds)
{
    if (!light.flicker || light.flickerAmount <= 0.0f) {
        return light.intensity;
    }
    return light.intensity * EvaluateDynamicLightFlickerMultiplier(
            light.lightId,
            runtimeSeconds,
            light.flickerSpeed,
            light.flickerAmount);
}

bool MakeSectorPreviewDynamicPointLightUniform(
        const SectorTopologyDynamicPointLight& light,
        SectorPreviewDynamicPointLightUniform& outLight)
{
    if (!light.enabled
            || !std::isfinite(light.radius)
            || !std::isfinite(light.intensity)
            || light.radius <= 0.0f
            || light.intensity <= 0.0f
            || !IsFiniteVector3(light.position)) {
        return false;
    }

    outLight.lightId = light.id;
    outLight.kind = SectorPreviewDynamicLightKind::Point;
    outLight.position = SectorAuthoringToWorldPosition(light.position);
    outLight.direction = Vector3{0.0f, -1.0f, 0.0f};
    outLight.color = engine::SrgbColorBytesToLinearSceneRgb(light.color);
    outLight.radius = SectorAuthoringToWorldDistance(light.radius);
    outLight.innerConeCos = -1.0f;
    outLight.outerConeCos = -1.0f;
    outLight.intensity = light.intensity;
    outLight.flicker = light.flicker;
    outLight.flickerSpeed = ClampDynamicLightFlickerSpeed(light.flickerSpeed);
    outLight.flickerAmount = ClampDynamicLightFlickerAmount(light.flickerAmount);
    outLight.castsShadow = light.castsShadow;
    outLight.shadowPriority = ClampDynamicSpotLightShadowPriority(light.shadowPriority);
    outLight.shadowBias = ClampDynamicSpotLightShadowBias(light.shadowBias);
    outLight.shadowStrength = ClampDynamicSpotLightShadowStrength(light.shadowStrength);
    outLight.shadowSoftness = ClampDynamicSpotLightShadowSoftness(light.shadowSoftness);
    return std::isfinite(outLight.radius)
            && outLight.radius > 0.0f
            && std::isfinite(outLight.intensity)
            && outLight.intensity > 0.0f
            && IsFiniteVector3(outLight.position)
            && IsFiniteVector3(outLight.color);
}

bool MakeSectorPreviewDynamicSpotLightUniform(
        const SectorTopologyDynamicSpotLight& light,
        SectorPreviewDynamicPointLightUniform& outLight)
{
    if (!light.enabled
            || !std::isfinite(light.range)
            || !std::isfinite(light.intensity)
            || !std::isfinite(light.innerConeDegrees)
            || !std::isfinite(light.outerConeDegrees)
            || light.range <= 0.0f
            || light.intensity <= 0.0f
            || !IsFiniteVector3(light.position)
            || !IsFiniteVector3(light.target)) {
        return false;
    }

    const Vector3 worldPosition = SectorAuthoringToWorldPosition(light.position);
    const Vector3 worldTarget = SectorAuthoringToWorldPosition(light.target);
    const Vector3 direction = NormalizeOrFallback(
            Vector3{
                    worldTarget.x - worldPosition.x,
                    worldTarget.y - worldPosition.y,
                    worldTarget.z - worldPosition.z},
            Vector3{0.0f, -1.0f, 0.0f});
    const float innerDegrees = std::clamp(light.innerConeDegrees, 0.0f, 179.0f);
    const float outerDegrees = std::max(innerDegrees, std::clamp(light.outerConeDegrees, 0.0f, 179.0f));

    outLight.lightId = light.id;
    outLight.kind = SectorPreviewDynamicLightKind::Spot;
    outLight.position = worldPosition;
    outLight.direction = direction;
    outLight.color = engine::SrgbColorBytesToLinearSceneRgb(light.color);
    outLight.radius = SectorAuthoringToWorldDistance(light.range);
    outLight.innerConeCos = ConeCosine(innerDegrees);
    outLight.outerConeCos = ConeCosine(outerDegrees);
    outLight.intensity = light.intensity;
    outLight.flicker = light.flicker;
    outLight.flickerSpeed = ClampDynamicLightFlickerSpeed(light.flickerSpeed);
    outLight.flickerAmount = ClampDynamicLightFlickerAmount(light.flickerAmount);
    outLight.castsShadow = light.castsShadow;
    outLight.shadowPriority = ClampDynamicSpotLightShadowPriority(light.shadowPriority);
    outLight.shadowBias = ClampDynamicSpotLightShadowBias(light.shadowBias);
    outLight.shadowStrength = ClampDynamicSpotLightShadowStrength(light.shadowStrength);
    outLight.shadowSoftness = ClampDynamicSpotLightShadowSoftness(light.shadowSoftness);
    return std::isfinite(outLight.radius)
            && outLight.radius > 0.0f
            && std::isfinite(outLight.intensity)
            && outLight.intensity > 0.0f
            && IsFiniteVector3(outLight.position)
            && IsFiniteVector3(outLight.direction)
            && IsFiniteVector3(outLight.color)
            && std::isfinite(outLight.innerConeCos)
            && std::isfinite(outLight.outerConeCos);
}

void BuildSectorPreviewDynamicPointLightSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld,
        std::vector<SectorPreviewDynamicPointLightSource>& outSources)
{
    outSources.clear();
    outSources.reserve(map.dynamicPointLights.size() + map.dynamicSpotLights.size());

    for (const SectorTopologyDynamicPointLight& light : map.dynamicPointLights) {
        SectorPreviewDynamicPointLightUniform uniformLight;
        if (!MakeSectorPreviewDynamicPointLightUniform(light, uniformLight)) {
            continue;
        }

        int ownerSectorId = 0;
        if (sectorLookupWorld != nullptr) {
            ownerSectorId = sectorLookupWorld->FindSectorContainingPoint(
                    Vector2{uniformLight.position.x, uniformLight.position.z});
        }

        outSources.push_back(SectorPreviewDynamicPointLightSource{
                light.id,
                ownerSectorId,
                uniformLight});
    }

    for (const SectorTopologyDynamicSpotLight& light : map.dynamicSpotLights) {
        SectorPreviewDynamicPointLightUniform uniformLight;
        if (!MakeSectorPreviewDynamicSpotLightUniform(light, uniformLight)) {
            continue;
        }

        int ownerSectorId = 0;
        if (sectorLookupWorld != nullptr) {
            ownerSectorId = sectorLookupWorld->FindSectorContainingPoint(
                    Vector2{uniformLight.position.x, uniformLight.position.z});
        }

        outSources.push_back(SectorPreviewDynamicPointLightSource{
                light.id,
                ownerSectorId,
                uniformLight});
    }
}

void CollectSectorPreviewDynamicPointLightCandidates(
        const std::vector<SectorPreviewDynamicPointLightSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::vector<SectorPreviewDynamicPointLightSource>& outCandidates,
        const RuntimeSectorVisibilityGraph* visibilityGraph,
        const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers)
{
    outCandidates.clear();
    outCandidates.reserve(sources.size());

    const bool includeAll = !visibility.validStartSector || visibility.fallbackDrawAll;
    std::vector<const SectorReceiverBounds*> relevantBounds;
    BuildRelevantReceiverBounds(visibility, receiverBounds, relevantBounds);
    const bool includeAllConservatively = includeAll || relevantBounds.empty();

    for (const SectorPreviewDynamicPointLightSource& source : sources) {
        if (!includeAllConservatively
                && visibilityGraph != nullptr
                && source.ownerSectorId > 0) {
            const RuntimePortalVisibilityResult lightReachability =
                    TraverseRuntimeSectorVisibility(
                            *visibilityGraph,
                            source.ownerSectorId,
                            dynamicPortalBlockers);
            bool reachesVisibleSector = false;
            for (const int sectorId : lightReachability.visibleSectorIds) {
                if (ContainsSectorId(visibility.visibleSectorIds, sectorId)) {
                    reachesVisibleSector = true;
                    break;
                }
            }
            if (!reachesVisibleSector) continue;
        }
        bool include = includeAllConservatively
                || (source.ownerSectorId > 0
                    && ContainsSectorId(visibility.visibleSectorIds, source.ownerSectorId));

        if (!include) {
            for (const SectorReceiverBounds* bounds : relevantBounds) {
                if (SphereOverlapsBounds(source.light, *bounds)) {
                    include = true;
                    break;
                }
            }
        }

        if (include) {
            outCandidates.push_back(source);
        }
    }
}

void SelectRankedSectorPreviewDynamicPointLights(
        std::vector<SectorPreviewDynamicPointLightSource>& candidates,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::size_t maxLights,
        std::vector<SectorPreviewDynamicPointLightUniform>& outSelectedLights,
        std::vector<int>* outSelectedLightIds,
        const std::vector<int>* previousSelectedLightIds)
{
    const std::vector<int> previousIds = previousSelectedLightIds != nullptr
            ? *previousSelectedLightIds
            : std::vector<int>{};

    outSelectedLights.clear();
    outSelectedLights.reserve(std::min(candidates.size(), maxLights));
    if (outSelectedLightIds != nullptr) {
        outSelectedLightIds->clear();
        outSelectedLightIds->reserve(std::min(candidates.size(), maxLights));
    }
    if (maxLights == 0) {
        return;
    }

    std::vector<const SectorReceiverBounds*> relevantBounds;
    BuildRelevantReceiverBounds(visibility, receiverBounds, relevantBounds);

    std::vector<ScoredDynamicPointLightCandidate> ranked;
    ranked.reserve(candidates.size());
    for (const SectorPreviewDynamicPointLightSource& candidate : candidates) {
        const float score = DynamicPointLightSelectionScore(candidate.light, relevantBounds);
        if (score < 0.0f) {
            continue;
        }
        ranked.push_back(ScoredDynamicPointLightCandidate{
                &candidate,
                score,
                ContainsLightId(previousIds, candidate.lightId)});
    }

    std::sort(ranked.begin(), ranked.end(), BetterScoredDynamicPointLight);

    if (previousIds.empty()) {
        for (const ScoredDynamicPointLightCandidate& candidate : ranked) {
            if (outSelectedLights.size() >= maxLights) {
                break;
            }
            outSelectedLights.push_back(candidate.source->light);
            if (outSelectedLightIds != nullptr) {
                outSelectedLightIds->push_back(candidate.source->lightId);
            }
        }
        return;
    }

    std::vector<ScoredDynamicPointLightCandidate> selected;
    selected.reserve(std::min(ranked.size(), maxLights));

    for (const ScoredDynamicPointLightCandidate& candidate : ranked) {
        if (!candidate.previouslySelected || candidate.score <= 0.0f) {
            continue;
        }
        if (selected.size() >= maxLights) {
            break;
        }
        selected.push_back(candidate);
    }

    for (const ScoredDynamicPointLightCandidate& candidate : ranked) {
        if (FindSelectedLightIndex(selected, candidate.source->lightId) >= 0) {
            continue;
        }
        if (candidate.previouslySelected && candidate.score <= 0.0f) {
            continue;
        }

        if (selected.size() < maxLights) {
            selected.push_back(candidate);
            continue;
        }

        if (candidate.previouslySelected) {
            continue;
        }

        const int replaceIndex = FindWeakestPreviouslySelectedLightIndex(selected);
        if (replaceIndex < 0) {
            continue;
        }

        const ScoredDynamicPointLightCandidate& retained = selected[static_cast<std::size_t>(replaceIndex)];
        if (candidate.score >= retained.score * DynamicPointLightHysteresisReplacementFactor) {
            selected[static_cast<std::size_t>(replaceIndex)] = candidate;
        }
    }

    std::sort(selected.begin(), selected.end(), BetterScoredDynamicPointLight);
    for (const ScoredDynamicPointLightCandidate& candidate : selected) {
        outSelectedLights.push_back(candidate.source->light);
        if (outSelectedLightIds != nullptr) {
            outSelectedLightIds->push_back(candidate.source->lightId);
        }
    }
}

void SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::size_t maxShadowCasters,
        std::vector<SectorPreviewDynamicSpotLightShadowCaster>& outShadowCasters)
{
    outShadowCasters.clear();
    outShadowCasters.reserve(std::min(selectedDynamicLights.size(), maxShadowCasters));
    if (maxShadowCasters == 0) {
        return;
    }

    std::vector<const SectorReceiverBounds*> relevantBounds;
    BuildRelevantReceiverBounds(visibility, receiverBounds, relevantBounds);

    std::vector<ScoredDynamicSpotLightShadowCandidate> ranked;
    ranked.reserve(selectedDynamicLights.size());
    for (std::size_t i = 0; i < selectedDynamicLights.size(); ++i) {
        const SectorPreviewDynamicPointLightUniform& light = selectedDynamicLights[i];
        if (!light.castsShadow) {
            continue;
        }

        const float score = DynamicPointLightSelectionScore(light, relevantBounds);
        if (score < 0.0f) {
            continue;
        }

        ranked.push_back(ScoredDynamicSpotLightShadowCandidate{
                &light,
                static_cast<int>(i),
                score});
    }

    std::sort(ranked.begin(), ranked.end(), BetterScoredDynamicSpotLightShadowCandidate);

    std::size_t usedSlots = 0;
    for (const ScoredDynamicSpotLightShadowCandidate& candidate : ranked) {
        const std::size_t requiredSlots = candidate.light->kind
                == SectorPreviewDynamicLightKind::Point ? 2u : 1u;
        if (usedSlots + requiredSlots > maxShadowCasters) {
            continue;
        }
        const int shadowSlot = static_cast<int>(usedSlots);
        outShadowCasters.push_back(SectorPreviewDynamicSpotLightShadowCaster{
                candidate.light->lightId,
                candidate.dynamicLightIndex,
                shadowSlot,
                candidate.light->shadowPriority,
                candidate.score,
                candidate.light->shadowBias,
                candidate.light->shadowStrength,
                candidate.light->shadowSoftness,
                static_cast<int>(requiredSlots)});
        usedSlots += requiredSlots;
    }
}

bool MakeSectorPreviewDynamicSpotLightShadowMatrix(
        const SectorPreviewDynamicPointLightUniform& light,
        int dynamicLightIndex,
        int shadowSlot,
        SectorPreviewDynamicSpotLightShadowMatrix& outMatrix)
{
    if (light.kind != SectorPreviewDynamicLightKind::Spot
            || light.radius <= ShadowNearPlane
            || !std::isfinite(light.radius)
            || !IsFiniteVector3(light.position)
            || !IsFiniteVector3(light.direction)
            || !std::isfinite(light.outerConeCos)) {
        return false;
    }

    const Vector3 direction = NormalizeOrFallback(light.direction, Vector3{0.0f, -1.0f, 0.0f});
    if (!IsFiniteVector3(direction)) {
        return false;
    }

    const float outerHalfAngleRadians = std::acos(std::clamp(light.outerConeCos, -0.999f, 0.999f));
    const float fovyRadians = std::clamp(outerHalfAngleRadians * 2.0f, 1.0f * DegreesToRadians, 178.0f * DegreesToRadians);
    const Vector3 target = Vector3Add(light.position, direction);
    const Matrix view = MatrixLookAt(light.position, target, SpotlightShadowUpVector(direction));
    const Matrix projection = MatrixPerspective(static_cast<double>(fovyRadians), 1.0, ShadowNearPlane, light.radius);
    const Matrix lightViewProjection = MatrixMultiply(view, projection);

    if (!IsFiniteMatrix(view) || !IsFiniteMatrix(projection) || !IsFiniteMatrix(lightViewProjection)) {
        return false;
    }

    outMatrix.lightId = light.lightId;
    outMatrix.dynamicLightIndex = dynamicLightIndex;
    outMatrix.shadowSlot = shadowSlot;
    outMatrix.kind = SectorPreviewDynamicLightKind::Spot;
    outMatrix.pointHemisphere = 0;
    outMatrix.lightPosition = light.position;
    outMatrix.lightRadius = light.radius;
    outMatrix.view = view;
    outMatrix.projection = projection;
    outMatrix.lightViewProjection = lightViewProjection;
    return true;
}

void BuildSectorPreviewDynamicSpotLightShadowMatrices(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& shadowCasters,
        std::vector<SectorPreviewDynamicSpotLightShadowMatrix>& outMatrices)
{
    outMatrices.clear();
    outMatrices.reserve(shadowCasters.size());

    for (const SectorPreviewDynamicSpotLightShadowCaster& caster : shadowCasters) {
        if (caster.dynamicLightIndex < 0
                || static_cast<std::size_t>(caster.dynamicLightIndex) >= selectedDynamicLights.size()) {
            continue;
        }

        const SectorPreviewDynamicPointLightUniform& light =
                selectedDynamicLights[static_cast<std::size_t>(caster.dynamicLightIndex)];
        if (light.kind == SectorPreviewDynamicLightKind::Point) {
            for (int hemisphere = 0; hemisphere < 2; ++hemisphere) {
                SectorPreviewDynamicSpotLightShadowMatrix matrix;
                matrix.lightId = light.lightId;
                matrix.dynamicLightIndex = caster.dynamicLightIndex;
                matrix.shadowSlot = caster.shadowSlot + hemisphere;
                matrix.kind = SectorPreviewDynamicLightKind::Point;
                matrix.pointHemisphere = hemisphere == 0 ? 1 : -1;
                matrix.lightPosition = light.position;
                matrix.lightRadius = light.radius;
                matrix.view = MatrixIdentity();
                matrix.projection = MatrixIdentity();
                matrix.lightViewProjection = MatrixIdentity();
                outMatrices.push_back(matrix);
            }
            continue;
        }

        SectorPreviewDynamicSpotLightShadowMatrix matrix;
        if (MakeSectorPreviewDynamicSpotLightShadowMatrix(
                    light,
                    caster.dynamicLightIndex,
                    caster.shadowSlot,
                    matrix)) {
            outMatrices.push_back(matrix);
        }
    }
}

SectorPreviewDynamicSpotLightShadowUniforms PackSectorPreviewDynamicSpotLightShadowUniforms(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& shadowCasters,
        const std::vector<SectorPreviewDynamicSpotLightShadowMatrix>& shadowMatrices)
{
    SectorPreviewDynamicSpotLightShadowUniforms uniforms;
    uniforms.dynamicLightShadowSlots.fill(-1);
    uniforms.shadowLightMatrices.fill(MatrixIdentity());
    uniforms.shadowBias.fill(DynamicSpotLightDefaultShadowBias);
    uniforms.shadowStrength.fill(0.0f);
    uniforms.shadowSoftness.fill(DynamicSpotLightDefaultShadowSoftness);

    for (const SectorPreviewDynamicSpotLightShadowMatrix& matrix : shadowMatrices) {
        if (matrix.shadowSlot < 0
                || static_cast<std::size_t>(matrix.shadowSlot) >= MaxDynamicSpotLightShadowCasters
                || matrix.dynamicLightIndex < 0
                || static_cast<std::size_t>(matrix.dynamicLightIndex) >= selectedDynamicLights.size()
                || static_cast<std::size_t>(matrix.dynamicLightIndex) >= MaxDynamicLights
                || !IsFiniteMatrix(matrix.lightViewProjection)) {
            continue;
        }

        const SectorPreviewDynamicPointLightUniform& light =
                selectedDynamicLights[static_cast<std::size_t>(matrix.dynamicLightIndex)];
        if (light.lightId != matrix.lightId) {
            continue;
        }

        const SectorPreviewDynamicSpotLightShadowCaster* caster = nullptr;
        for (const SectorPreviewDynamicSpotLightShadowCaster& candidate : shadowCasters) {
            if (candidate.lightId == matrix.lightId
                    && candidate.dynamicLightIndex == matrix.dynamicLightIndex
                    && candidate.shadowSlot == matrix.shadowSlot) {
                caster = &candidate;
                break;
            }
        }
        if (caster == nullptr) {
            continue;
        }

        const std::size_t shadowSlot = static_cast<std::size_t>(matrix.shadowSlot);
        uniforms.dynamicLightShadowSlots[static_cast<std::size_t>(matrix.dynamicLightIndex)] = caster->shadowSlot;
        uniforms.shadowLightMatrices[shadowSlot] = matrix.lightViewProjection;
        uniforms.shadowBias[shadowSlot] = ClampDynamicSpotLightShadowBias(caster->shadowBias);
        uniforms.shadowStrength[shadowSlot] = ClampDynamicSpotLightShadowStrength(caster->shadowStrength);
        uniforms.shadowSoftness[shadowSlot] = ClampDynamicSpotLightShadowSoftness(caster->shadowSoftness);
        if (caster->shadowSlotCount == 2
                && matrix.shadowSlot == caster->shadowSlot
                && shadowSlot + 1 < uniforms.shadowBias.size()) {
            uniforms.shadowBias[shadowSlot + 1] = uniforms.shadowBias[shadowSlot];
            uniforms.shadowStrength[shadowSlot + 1] = uniforms.shadowStrength[shadowSlot];
            uniforms.shadowSoftness[shadowSlot + 1] = uniforms.shadowSoftness[shadowSlot];
        }
    }

    return uniforms;
}

} // namespace game
