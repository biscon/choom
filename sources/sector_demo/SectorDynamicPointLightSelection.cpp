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

bool operator==(
        const SectorPreviewDynamicLightKey& left,
        const SectorPreviewDynamicLightKey& right)
{
    return left.kind == right.kind && left.lightId == right.lightId;
}

bool operator!=(
        const SectorPreviewDynamicLightKey& left,
        const SectorPreviewDynamicLightKey& right)
{
    return !(left == right);
}

SectorPreviewDynamicLightKey MakeSectorPreviewDynamicLightKey(
        const SectorPreviewDynamicPointLightUniform& light)
{
    return SectorPreviewDynamicLightKey{light.kind, light.lightId};
}

void BuildSectorDynamicSpotShadowProjectionUpload(
        const SectorPreviewDynamicPointLightUniform& light,
        Vector3& outRight,
        Vector2& outProjection)
{
    constexpr float NearPlane = 0.05f;
    const Vector3 forward = Vector3LengthSqr(light.direction) > 0.00000001f
            ? Vector3Normalize(light.direction)
            : Vector3{0.0f, -1.0f, 0.0f};
    const Vector3 upReference = std::fabs(forward.y) > 0.98f
            ? Vector3{0.0f, 0.0f, 1.0f}
            : Vector3{0.0f, 1.0f, 0.0f};
    outRight = Vector3Normalize(Vector3CrossProduct(forward, upReference));
    const float halfAngle = std::clamp(
            std::acos(std::clamp(light.outerConeCos, -0.999f, 0.999f)),
            0.5f * DEG2RAD,
            89.0f * DEG2RAD);
    const float farPlane = std::max(light.radius, NearPlane + 0.0001f);
    outProjection = Vector2{
            1.0f / std::tan(halfAngle),
            farPlane / (farPlane - NearPlane)};
}

void SortSectorDynamicShadowUpdateRequests(
        std::vector<SectorDynamicShadowUpdateRequest>& requests)
{
    std::sort(
            requests.begin(), requests.end(),
            [](const SectorDynamicShadowUpdateRequest& left,
                    const SectorDynamicShadowUpdateRequest& right) {
                if (left.invalid != right.invalid) return left.invalid;
                return left.dirtySerial < right.dirtySerial;
            });
}

std::size_t SectorDynamicShadowUpdateCount(
        std::size_t pendingCount,
        std::size_t maximumUpdatesPerFrame)
{
    return maximumUpdatesPerFrame == 0
            ? pendingCount
            : std::min(pendingCount, maximumUpdatesPerFrame);
}

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

bool DynamicLightKeyLess(
        const SectorPreviewDynamicLightKey& left,
        const SectorPreviewDynamicLightKey& right)
{
    if (left.lightId != right.lightId) return left.lightId < right.lightId;
    return static_cast<int>(left.kind) < static_cast<int>(right.kind);
}

bool ContainsLightKey(
        const std::vector<SectorPreviewDynamicLightKey>& lightKeys,
        SectorPreviewDynamicLightKey lightKey)
{
    return std::find(lightKeys.begin(), lightKeys.end(), lightKey) != lightKeys.end();
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
    return DynamicLightKeyLess(
            MakeSectorPreviewDynamicLightKey(lhs.source->light),
            MakeSectorPreviewDynamicLightKey(rhs.source->light));
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
    return DynamicLightKeyLess(
            MakeSectorPreviewDynamicLightKey(*lhs.light),
            MakeSectorPreviewDynamicLightKey(*rhs.light));
}

int FindSelectedLightIndex(
        const std::vector<ScoredDynamicPointLightCandidate>& selected,
        SectorPreviewDynamicLightKey lightKey)
{
    for (std::size_t i = 0; i < selected.size(); ++i) {
        if (MakeSectorPreviewDynamicLightKey(selected[i].source->light) == lightKey) {
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
                    && DynamicLightKeyLess(
                            MakeSectorPreviewDynamicLightKey(
                                    selected[static_cast<std::size_t>(weakestIndex)].source->light),
                            MakeSectorPreviewDynamicLightKey(selected[i].source->light)))) {
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
    const float fadeMultiplier = std::isfinite(light.selectionFadeMultiplier)
            ? std::clamp(light.selectionFadeMultiplier, 0.0f, 1.0f)
            : 1.0f;
    const float flickerMultiplier = !light.flicker || light.flickerAmount <= 0.0f
            ? 1.0f
            : EvaluateDynamicLightFlickerMultiplier(
                    light.lightId,
                    runtimeSeconds,
                    light.flickerSpeed,
                    light.flickerAmount);
    return light.intensity * fadeMultiplier * flickerMultiplier;
}

void ResetSectorDynamicLightFadeTracker(
        SectorDynamicLightFadeTracker& tracker)
{
    tracker = {};
}

void SynchronizeSectorDynamicLightFadeTracker(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedLights,
        SectorDynamicLightFadeTracker& tracker)
{
    for (SectorDynamicLightFadeEntry& entry : tracker.entries) {
        entry.seen = false;
    }

    const bool initializeFullyVisible = !tracker.initialized;
    tracker.initialized = true;
    for (const SectorPreviewDynamicPointLightUniform& light : selectedLights) {
        if (!light.selectionFadeEnabled) continue;
        const SectorPreviewDynamicLightKey lightKey =
                MakeSectorPreviewDynamicLightKey(light);
        SectorDynamicLightFadeEntry* entry = nullptr;
        for (SectorDynamicLightFadeEntry& candidate : tracker.entries) {
            if (candidate.occupied && candidate.lightKey == lightKey) {
                entry = &candidate;
                break;
            }
        }
        if (entry == nullptr) {
            for (SectorDynamicLightFadeEntry& candidate : tracker.entries) {
                if (!candidate.occupied) {
                    candidate = {};
                    candidate.lightKey = lightKey;
                    candidate.occupied = true;
                    candidate.complete = initializeFullyVisible;
                    candidate.started = initializeFullyVisible;
                    entry = &candidate;
                    break;
                }
            }
        }
        if (entry != nullptr) entry->seen = true;
    }

    for (SectorDynamicLightFadeEntry& entry : tracker.entries) {
        if (entry.occupied && !entry.seen) entry = {};
    }
}

float EvaluateSectorDynamicLightFadeMultiplier(
        SectorDynamicLightFadeTracker& tracker,
        SectorPreviewDynamicLightKey lightKey,
        float runtimeSeconds,
        float fadeInSeconds,
        bool ready)
{
    SectorDynamicLightFadeEntry* entry = nullptr;
    for (SectorDynamicLightFadeEntry& candidate : tracker.entries) {
        if (candidate.occupied && candidate.lightKey == lightKey) {
            entry = &candidate;
            break;
        }
    }
    if (entry == nullptr || entry->complete) return 1.0f;
    if (!std::isfinite(fadeInSeconds) || fadeInSeconds <= 0.0f) {
        entry->complete = true;
        entry->started = true;
        return 1.0f;
    }
    if (!ready) return 0.0f;
    if (!std::isfinite(runtimeSeconds)) {
        entry->complete = true;
        entry->started = true;
        return 1.0f;
    }
    if (!entry->started) {
        entry->startSeconds = runtimeSeconds;
        entry->started = true;
        return 0.0f;
    }

    const float elapsedSeconds = std::max(
            runtimeSeconds - entry->startSeconds, 0.0f);
    const float linear = std::clamp(
            elapsedSeconds / fadeInSeconds, 0.0f, 1.0f);
    const float multiplier = linear * linear * (3.0f - 2.0f * linear);
    if (linear >= 1.0f) entry->complete = true;
    return multiplier;
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
    outLight.selectionFadeMultiplier = 1.0f;
    outLight.selectionFadeEnabled = true;
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
    outLight.selectionFadeMultiplier = 1.0f;
    outLight.selectionFadeEnabled = true;
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
        std::vector<SectorPreviewDynamicPointLightSource>& outCandidates)
{
    outCandidates.clear();
    outCandidates.reserve(sources.size());

    const bool includeAll = !visibility.validStartSector || visibility.fallbackDrawAll;
    std::vector<const SectorReceiverBounds*> relevantBounds;
    BuildRelevantReceiverBounds(visibility, receiverBounds, relevantBounds);

    for (const SectorPreviewDynamicPointLightSource& source : sources) {
        bool include = includeAll
                || (source.ownerSectorId > 0
                    && ContainsSectorId(visibility.visibleSectorIds, source.ownerSectorId));

        if (!include && source.ownerSectorId <= 0) {
            include = relevantBounds.empty();
            if (!include) {
                for (const SectorReceiverBounds* bounds : relevantBounds) {
                    if (SphereOverlapsBounds(source.light, *bounds)) {
                        include = true;
                        break;
                    }
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
        std::vector<SectorPreviewDynamicLightKey>* outSelectedLightKeys,
        const std::vector<SectorPreviewDynamicLightKey>* previousSelectedLightKeys)
{
    const std::vector<SectorPreviewDynamicLightKey> previousKeys =
            previousSelectedLightKeys != nullptr
            ? *previousSelectedLightKeys
            : std::vector<SectorPreviewDynamicLightKey>{};

    outSelectedLights.clear();
    outSelectedLights.reserve(std::min(candidates.size(), maxLights));
    if (outSelectedLightKeys != nullptr) {
        outSelectedLightKeys->clear();
        outSelectedLightKeys->reserve(std::min(candidates.size(), maxLights));
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
                ContainsLightKey(
                        previousKeys,
                        MakeSectorPreviewDynamicLightKey(candidate.light))});
    }

    std::sort(ranked.begin(), ranked.end(), BetterScoredDynamicPointLight);

    if (previousKeys.empty()) {
        for (const ScoredDynamicPointLightCandidate& candidate : ranked) {
            if (outSelectedLights.size() >= maxLights) {
                break;
            }
            outSelectedLights.push_back(candidate.source->light);
            if (outSelectedLightKeys != nullptr) {
                outSelectedLightKeys->push_back(
                        MakeSectorPreviewDynamicLightKey(candidate.source->light));
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
        if (FindSelectedLightIndex(
                    selected,
                    MakeSectorPreviewDynamicLightKey(candidate.source->light)) >= 0) {
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
        if (outSelectedLightKeys != nullptr) {
            outSelectedLightKeys->push_back(
                    MakeSectorPreviewDynamicLightKey(candidate.source->light));
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
                == SectorPreviewDynamicLightKind::Point
                ? static_cast<std::size_t>(DynamicPointLightShadowFaceCount)
                : 1u;
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

void AssignPersistentSectorDynamicShadowSlots(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        std::size_t shadowSlotBudget,
        std::vector<SectorPreviewDynamicSpotLightShadowCaster>& shadowCasters,
        std::array<SectorDynamicShadowSlotOwner,
                MaxDynamicSpotLightShadowCasters>& slotOwners)
{
    const std::size_t budget = std::min(
            shadowSlotBudget,
            slotOwners.size());
    for (std::size_t slot = 0; slot < slotOwners.size(); ++slot) {
        slotOwners[slot].claimed = false;
        if (slot >= budget) {
            slotOwners[slot] = {};
        }
    }
    for (SectorPreviewDynamicSpotLightShadowCaster& caster : shadowCasters) {
        caster.shadowSlot = -1;
    }

    const auto casterKey = [&selectedDynamicLights](
            const SectorPreviewDynamicSpotLightShadowCaster& caster,
            SectorPreviewDynamicLightKey& outKey) {
        if (caster.dynamicLightIndex < 0
                || static_cast<std::size_t>(caster.dynamicLightIndex)
                        >= selectedDynamicLights.size()) {
            return false;
        }
        outKey = MakeSectorPreviewDynamicLightKey(
                selectedDynamicLights[static_cast<std::size_t>(
                        caster.dynamicLightIndex)]);
        return true;
    };

    for (SectorPreviewDynamicSpotLightShadowCaster& caster : shadowCasters) {
        SectorPreviewDynamicLightKey key;
        if (!casterKey(caster, key)
                || caster.shadowSlotCount <= 0
                || static_cast<std::size_t>(caster.shadowSlotCount) > budget) {
            continue;
        }
        for (std::size_t slot = 0; slot < budget; ++slot) {
            const SectorDynamicShadowSlotOwner& owner = slotOwners[slot];
            if (!owner.occupied
                    || owner.claimed
                    || owner.lightKey != key
                    || owner.spanStart != static_cast<int>(slot)
                    || owner.spanCount != caster.shadowSlotCount
                    || slot + static_cast<std::size_t>(owner.spanCount) > budget) {
                continue;
            }
            bool compatible = true;
            for (int offset = 0; offset < owner.spanCount; ++offset) {
                const SectorDynamicShadowSlotOwner& spanOwner =
                        slotOwners[slot + static_cast<std::size_t>(offset)];
                compatible = compatible
                        && spanOwner.occupied
                        && !spanOwner.claimed
                        && spanOwner.lightKey == key
                        && spanOwner.spanStart == owner.spanStart
                        && spanOwner.spanCount == owner.spanCount;
            }
            if (!compatible) continue;

            caster.shadowSlot = static_cast<int>(slot);
            for (int offset = 0; offset < owner.spanCount; ++offset) {
                slotOwners[slot + static_cast<std::size_t>(offset)].claimed = true;
            }
            break;
        }
    }

    for (std::size_t slot = 0; slot < budget; ++slot) {
        if (!slotOwners[slot].claimed) {
            slotOwners[slot] = {};
        }
    }

    for (SectorPreviewDynamicSpotLightShadowCaster& caster : shadowCasters) {
        if (caster.shadowSlot >= 0) continue;
        SectorPreviewDynamicLightKey key;
        if (!casterKey(caster, key)
                || caster.shadowSlotCount <= 0
                || static_cast<std::size_t>(caster.shadowSlotCount) > budget) {
            continue;
        }

        const std::size_t spanCount = static_cast<std::size_t>(
                caster.shadowSlotCount);
        for (std::size_t slot = 0; slot + spanCount <= budget; ++slot) {
            bool free = true;
            for (std::size_t offset = 0; offset < spanCount; ++offset) {
                if (slotOwners[slot + offset].occupied) {
                    free = false;
                    break;
                }
            }
            if (!free) continue;

            caster.shadowSlot = static_cast<int>(slot);
            for (std::size_t offset = 0; offset < spanCount; ++offset) {
                slotOwners[slot + offset] = SectorDynamicShadowSlotOwner{
                        key,
                        static_cast<int>(slot),
                        caster.shadowSlotCount,
                        true,
                        true};
            }
            break;
        }
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
    outMatrix.pointFace = -1;
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
    outMatrices.reserve(MaxDynamicSpotLightShadowCasters);

    for (const SectorPreviewDynamicSpotLightShadowCaster& caster : shadowCasters) {
        if (caster.dynamicLightIndex < 0
                || static_cast<std::size_t>(caster.dynamicLightIndex) >= selectedDynamicLights.size()) {
            continue;
        }

        const SectorPreviewDynamicPointLightUniform& light =
                selectedDynamicLights[static_cast<std::size_t>(caster.dynamicLightIndex)];
        if (light.kind == SectorPreviewDynamicLightKind::Point) {
            if (light.radius <= ShadowNearPlane
                    || !std::isfinite(light.radius)
                    || !IsFiniteVector3(light.position)
                    || caster.shadowSlot < 0
                    || caster.shadowSlotCount != DynamicPointLightShadowFaceCount) {
                continue;
            }
            constexpr std::array<Vector3, DynamicPointLightShadowFaceCount>
                    FaceDirections = {{
                            {1.0f, 0.0f, 0.0f},
                            {-1.0f, 0.0f, 0.0f},
                            {0.0f, 1.0f, 0.0f},
                            {0.0f, -1.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f},
                            {0.0f, 0.0f, -1.0f}}};
            constexpr std::array<Vector3, DynamicPointLightShadowFaceCount>
                    FaceUpVectors = {{
                            {0.0f, -1.0f, 0.0f},
                            {0.0f, -1.0f, 0.0f},
                            {0.0f, 0.0f, 1.0f},
                            {0.0f, 0.0f, -1.0f},
                            {0.0f, -1.0f, 0.0f},
                            {0.0f, -1.0f, 0.0f}}};
            const Matrix projection = MatrixPerspective(
                    90.0 * static_cast<double>(DegreesToRadians),
                    1.0,
                    ShadowNearPlane,
                    light.radius);
            for (int face = 0; face < DynamicPointLightShadowFaceCount; ++face) {
                SectorPreviewDynamicSpotLightShadowMatrix matrix;
                matrix.lightId = light.lightId;
                matrix.dynamicLightIndex = caster.dynamicLightIndex;
                matrix.shadowSlot = caster.shadowSlot + face;
                matrix.kind = SectorPreviewDynamicLightKind::Point;
                matrix.pointFace = face;
                matrix.lightPosition = light.position;
                matrix.lightRadius = light.radius;
                matrix.view = MatrixLookAt(
                        light.position,
                        Vector3Add(light.position, FaceDirections[face]),
                        FaceUpVectors[face]);
                matrix.projection = projection;
                matrix.lightViewProjection = MatrixMultiply(
                        matrix.view, matrix.projection);
                if (!IsFiniteMatrix(matrix.view)
                        || !IsFiniteMatrix(matrix.projection)
                        || !IsFiniteMatrix(matrix.lightViewProjection)) {
                    continue;
                }
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
        if (light.lightId != matrix.lightId || light.kind != matrix.kind) {
            continue;
        }

        const SectorPreviewDynamicSpotLightShadowCaster* caster = nullptr;
        for (const SectorPreviewDynamicSpotLightShadowCaster& candidate : shadowCasters) {
            if (candidate.lightId == matrix.lightId
                    && candidate.dynamicLightIndex == matrix.dynamicLightIndex
                    && matrix.shadowSlot >= candidate.shadowSlot
                    && matrix.shadowSlot
                            < candidate.shadowSlot + candidate.shadowSlotCount) {
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
    }

    return uniforms;
}

} // namespace game
