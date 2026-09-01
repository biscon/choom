#pragma once

#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyTypes.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <vector>

namespace game {

constexpr std::size_t MaxDynamicLights = 32;
constexpr float DynamicLightDefaultFadeInSeconds = 0.25f;
constexpr float DynamicLightMaximumFadeInSeconds = 2.0f;
// The atlas budget is independent from the forward-light budget. A spot uses
// one slot, a rect uses the five cube faces in its emitting hemisphere, and a
// point light uses all six planar cube faces.
constexpr std::size_t MaxDynamicSpotLightShadowCasters = 64;
constexpr int DynamicPointLightShadowFaceCount = 6;
constexpr int DynamicRectLightShadowFaceCount = 5;
constexpr int DynamicShadowAtlasTilesPerRow = 8;
constexpr int DynamicSpotLightShadowMapResolution = 1024;
constexpr int DynamicShadowAtlasLowResolution = 4096;
constexpr int DynamicShadowAtlasHighResolution = 8192;

class SectorCollisionWorld;
struct SectorReceiverBounds;
struct SectorTopologyDynamicPointLight;
struct SectorTopologyDynamicSpotLight;
struct SectorTopologyDynamicRectLight;
struct SectorTopologyMap;
struct SectorPreviewDynamicPointLightUniform;

enum class SectorPreviewDynamicLightKind {
    Point = 0,
    Spot = 1,
    Rect = 2
};

enum class SectorDynamicLightProfile {
    None = 0,
    Flashlight = 1
};

constexpr int SectorDynamicShadowFaceCount(
        SectorPreviewDynamicLightKind kind)
{
    return kind == SectorPreviewDynamicLightKind::Point
            ? DynamicPointLightShadowFaceCount
            : kind == SectorPreviewDynamicLightKind::Rect
                    ? DynamicRectLightShadowFaceCount
                    : 1;
}

struct SectorPreviewDynamicLightKey {
    SectorPreviewDynamicLightKind kind = SectorPreviewDynamicLightKind::Point;
    int lightId = 0;
};

bool operator==(
        const SectorPreviewDynamicLightKey& left,
        const SectorPreviewDynamicLightKey& right);

bool operator!=(
        const SectorPreviewDynamicLightKey& left,
        const SectorPreviewDynamicLightKey& right);

SectorPreviewDynamicLightKey MakeSectorPreviewDynamicLightKey(
        const SectorPreviewDynamicPointLightUniform& light);

struct SectorPreviewDynamicPointLightUniform {
    int lightId = 0;
    SectorPreviewDynamicLightKind kind = SectorPreviewDynamicLightKind::Point;
    Vector3 position = {};
    Vector3 direction = {0.0f, -1.0f, 0.0f};
    Vector3 rectRight = {1.0f, 0.0f, 0.0f};
    Vector3 color = {};
    float radius = 0.0f;
    float innerConeCos = -1.0f;
    float outerConeCos = -1.0f;
    // Authored/base intensity used for selection. Upload applies runtime-only
    // flicker and selection-fade multipliers to a local effective value.
    float intensity = 0.0f;
    float selectionFadeMultiplier = 1.0f;
    bool selectionFadeEnabled = true;
    bool flicker = false;
    float flickerSpeed = DynamicLightFlickerDefaultSpeed;
    float flickerAmount = DynamicLightFlickerDefaultAmount;
    bool castsShadow = false;
    int shadowPriority = DynamicSpotLightDefaultShadowPriority;
    float shadowBias = DynamicSpotLightDefaultShadowBias;
    float shadowStrength = DynamicSpotLightDefaultShadowStrength;
    float shadowSoftness = DynamicSpotLightDefaultShadowSoftness;
    SectorDynamicLightProfile profile = SectorDynamicLightProfile::None;
    // x = hotspot radius ratio, y = spill brightness, z = edge softness.
    Vector3 profileParameters = {};
    // Reserved runtime lights are retained ahead of normally ranked lights and
    // shadow casters while their respective budgets are non-zero.
    bool reserveSelection = false;
    bool reserveShadow = false;
};

struct SectorPreviewDynamicPointLightSource {
    int lightId = 0;
    int ownerSectorId = 0;
    SectorPreviewDynamicPointLightUniform light = {};
};

struct SectorPreviewDynamicSpotLightShadowCaster {
    int lightId = 0;
    int dynamicLightIndex = -1;
    int shadowSlot = -1;
    int shadowPriority = DynamicSpotLightDefaultShadowPriority;
    float selectionScore = 0.0f;
    float shadowBias = DynamicSpotLightDefaultShadowBias;
    float shadowStrength = DynamicSpotLightDefaultShadowStrength;
    float shadowSoftness = DynamicSpotLightDefaultShadowSoftness;
    int shadowSlotCount = 1;
};

struct SectorPreviewDynamicSpotLightShadowMatrix {
    int lightId = 0;
    int dynamicLightIndex = -1;
    int shadowSlot = -1;
    SectorPreviewDynamicLightKind kind = SectorPreviewDynamicLightKind::Spot;
    int cubeFace = -1;
    Vector3 lightPosition = {};
    float lightRadius = 0.0f;
    Matrix view = {};
    Matrix projection = {};
    Matrix lightViewProjection = {};
};

struct SectorPreviewDynamicSpotLightShadowUniforms {
    std::array<int, MaxDynamicLights> dynamicLightShadowSlots{};
    std::array<Matrix, MaxDynamicSpotLightShadowCasters> shadowLightMatrices{};
    std::array<float, MaxDynamicSpotLightShadowCasters> shadowBias{};
    std::array<float, MaxDynamicSpotLightShadowCasters> shadowStrength{};
    std::array<float, MaxDynamicSpotLightShadowCasters> shadowSoftness{};
    int shadowAtlasTilesPerRow = 4;
};

struct SectorDynamicShadowUpdateRequest {
    std::size_t casterIndex = 0;
    bool invalid = true;
    bool reserved = false;
    uint64_t dirtySerial = 0;
    int shadowSlotCount = 1;
};

struct SectorDynamicShadowSlotOwner {
    SectorPreviewDynamicLightKey lightKey{};
    int spanStart = -1;
    int spanCount = 0;
    bool occupied = false;
    bool claimed = false;
};

struct SectorDynamicLightFadeEntry {
    SectorPreviewDynamicLightKey lightKey{};
    float startSeconds = 0.0f;
    bool occupied = false;
    bool seen = false;
    bool started = false;
    bool complete = false;
};

struct SectorDynamicLightFadeTracker {
    std::array<SectorDynamicLightFadeEntry, MaxDynamicLights> entries{};
    bool initialized = false;
};

void ResetSectorDynamicLightFadeTracker(
        SectorDynamicLightFadeTracker& tracker);

void SynchronizeSectorDynamicLightFadeTracker(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedLights,
        SectorDynamicLightFadeTracker& tracker);

float EvaluateSectorDynamicLightFadeMultiplier(
        SectorDynamicLightFadeTracker& tracker,
        SectorPreviewDynamicLightKey lightKey,
        float runtimeSeconds,
        float fadeInSeconds,
        bool ready);

void BuildSectorDynamicSpotShadowProjectionUpload(
        const SectorPreviewDynamicPointLightUniform& light,
        Vector3& outRight,
        Vector2& outProjection);

void SortSectorDynamicShadowUpdateRequests(
        std::vector<SectorDynamicShadowUpdateRequest>& requests);

std::size_t SectorDynamicShadowUpdateCount(
        std::size_t pendingCount,
        std::size_t maximumUpdatesPerFrame);

bool MakeSectorPreviewDynamicPointLightUniform(
        const SectorTopologyDynamicPointLight& light,
        SectorPreviewDynamicPointLightUniform& outLight);

bool MakeSectorPreviewDynamicSpotLightUniform(
        const SectorTopologyDynamicSpotLight& light,
        SectorPreviewDynamicPointLightUniform& outLight);

bool MakeSectorPreviewDynamicRectLightUniform(
        const SectorTopologyDynamicRectLight& light,
        SectorPreviewDynamicPointLightUniform& outLight);

float EvaluateDynamicLightFlickerMultiplier(
        int lightId,
        float runtimeSeconds,
        float flickerSpeed,
        float flickerAmount);

float DynamicLightEffectiveUploadIntensity(
        const SectorPreviewDynamicPointLightUniform& light,
        float runtimeSeconds);

void BuildSectorPreviewDynamicPointLightSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld,
        std::vector<SectorPreviewDynamicPointLightSource>& outSources);

void CollectSectorPreviewDynamicPointLightCandidates(
        const std::vector<SectorPreviewDynamicPointLightSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::vector<SectorPreviewDynamicPointLightSource>& outCandidates);

void SelectRankedSectorPreviewDynamicPointLights(
        std::vector<SectorPreviewDynamicPointLightSource>& candidates,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::size_t maxLights,
        std::vector<SectorPreviewDynamicPointLightUniform>& outSelectedLights,
        std::vector<SectorPreviewDynamicLightKey>* outSelectedLightKeys = nullptr,
        const std::vector<SectorPreviewDynamicLightKey>* previousSelectedLightKeys = nullptr);

void SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::size_t maxShadowCasters,
        std::vector<SectorPreviewDynamicSpotLightShadowCaster>& outShadowCasters);

void AssignPersistentSectorDynamicShadowSlots(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        std::size_t shadowSlotBudget,
        std::vector<SectorPreviewDynamicSpotLightShadowCaster>& shadowCasters,
        std::array<SectorDynamicShadowSlotOwner,
                MaxDynamicSpotLightShadowCasters>& slotOwners);

bool MakeSectorPreviewDynamicSpotLightShadowMatrix(
        const SectorPreviewDynamicPointLightUniform& light,
        int dynamicLightIndex,
        int shadowSlot,
        SectorPreviewDynamicSpotLightShadowMatrix& outMatrix);

void BuildSectorPreviewDynamicSpotLightShadowMatrices(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& shadowCasters,
        std::vector<SectorPreviewDynamicSpotLightShadowMatrix>& outMatrices);

SectorPreviewDynamicSpotLightShadowUniforms PackSectorPreviewDynamicSpotLightShadowUniforms(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& shadowCasters,
        const std::vector<SectorPreviewDynamicSpotLightShadowMatrix>& shadowMatrices);

} // namespace game
