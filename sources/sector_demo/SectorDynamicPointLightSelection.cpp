#include "sector_demo/SectorDynamicPointLightSelection.h"

#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorTopologyTypes.h"
#include "sector_demo/SectorUnits.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {

namespace {

constexpr float ReceiverBoundsPadding = 0.05f;
constexpr float DynamicPointLightHysteresisReplacementFactor = 1.2f;

struct ScoredDynamicPointLightCandidate {
    const SectorPreviewDynamicPointLightSource* source = nullptr;
    float score = -1.0f;
    bool previouslySelected = false;
};

bool IsFiniteVector3(Vector3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

Vector3 ColorToUnitRgb(Color color)
{
    return Vector3{
            static_cast<float>(color.r) / 255.0f,
            static_cast<float>(color.g) / 255.0f,
            static_cast<float>(color.b) / 255.0f};
}

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
    return Vector3{
            std::clamp(value.x, bounds.min.x - ReceiverBoundsPadding, bounds.max.x + ReceiverBoundsPadding),
            std::clamp(value.y, bounds.min.y - ReceiverBoundsPadding, bounds.max.y + ReceiverBoundsPadding),
            std::clamp(value.z, bounds.min.z - ReceiverBoundsPadding, bounds.max.z + ReceiverBoundsPadding)};
}

bool IsValidReceiverBounds(const SectorReceiverBounds& bounds)
{
    return bounds.sectorId > 0
            && IsFiniteVector3(bounds.min)
            && IsFiniteVector3(bounds.max)
            && bounds.min.x <= bounds.max.x
            && bounds.min.y <= bounds.max.y
            && bounds.min.z <= bounds.max.z;
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

} // namespace

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

    outLight.position = SectorAuthoringToWorldPosition(light.position);
    outLight.color = ColorToUnitRgb(light.color);
    outLight.radius = SectorAuthoringToWorldDistance(light.radius);
    outLight.intensity = light.intensity;
    return std::isfinite(outLight.radius)
            && outLight.radius > 0.0f
            && std::isfinite(outLight.intensity)
            && outLight.intensity > 0.0f
            && IsFiniteVector3(outLight.position)
            && IsFiniteVector3(outLight.color);
}

void BuildSectorPreviewDynamicPointLightSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld,
        std::vector<SectorPreviewDynamicPointLightSource>& outSources)
{
    outSources.clear();
    outSources.reserve(map.dynamicPointLights.size());

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
    const bool includeAllConservatively = includeAll || relevantBounds.empty();

    for (const SectorPreviewDynamicPointLightSource& source : sources) {
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

} // namespace game
