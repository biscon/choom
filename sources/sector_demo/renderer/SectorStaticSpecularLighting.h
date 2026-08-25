#pragma once

#include "sector_demo/SectorMeshTypes.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <vector>

namespace game {

class SectorCollisionWorld;
struct RuntimePortalVisibilityResult;
struct SectorTopologyMap;

constexpr std::size_t MaxStaticSpecularLights = 4;

enum class SectorStaticSpecularLightKind : int {
    Point = 0,
    Spot = 1,
    Rect = 2
};

struct SectorStaticSpecularLightSource {
    int lightId = 0;
    int ownerSectorId = 0;
    SectorStaticSpecularLightKind kind =
            SectorStaticSpecularLightKind::Point;
    Vector3 position = {};
    Vector3 direction = {0.0f, -1.0f, 0.0f};
    Vector3 color = {};
    float radius = 0.0f;
    float intensity = 0.0f;
    float innerConeCos = -1.0f;
    float outerConeCos = -1.0f;
    float startFeather = 0.0f;
};

struct SectorStaticSpecularSectorCandidates {
    int sectorId = 0;
    std::vector<std::size_t> sourceIndices;
};

struct SectorStaticSpecularLightState {
    std::vector<SectorStaticSpecularLightSource> sources;
    std::vector<SectorStaticSpecularSectorCandidates> sectorCandidates;
};

struct SectorStaticSpecularLightContext {
    int lightCount = 0;
    std::array<int, MaxStaticSpecularLights> lightIds{};
    std::array<Vector3, MaxStaticSpecularLights> positions{};
    std::array<Vector3, MaxStaticSpecularLights> colors{};
    std::array<float, MaxStaticSpecularLights> radii{};
    std::array<float, MaxStaticSpecularLights> intensities{};
    std::array<int, MaxStaticSpecularLights> types{};
    std::array<Vector3, MaxStaticSpecularLights> directions{};
    std::array<float, MaxStaticSpecularLights> innerConeCos{};
    std::array<float, MaxStaticSpecularLights> outerConeCos{};
    std::array<float, MaxStaticSpecularLights> startFeathers{};
};

struct SectorStaticSpecularShaderLocations {
    int lightCount = -1;
    int positions = -1;
    int colors = -1;
    int radii = -1;
    int intensities = -1;
    int types = -1;
    int directions = -1;
    int innerConeCos = -1;
    int outerConeCos = -1;
    int startFeathers = -1;
};

void ResetSectorStaticSpecularLights(
        SectorStaticSpecularLightState& state);

void RebuildSectorStaticSpecularLights(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld,
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        SectorStaticSpecularLightState& outState);

SectorReceiverBounds TransformSectorStaticSpecularReceiverBounds(
        BoundingBox localBounds,
        Matrix transform,
        int sectorId,
        Vector3 fallbackPosition);

SectorStaticSpecularLightContext SelectSectorStaticSpecularLights(
        const SectorStaticSpecularLightState& state,
        const SectorReceiverBounds& receiverBounds,
        int receiverSectorId,
        const RuntimePortalVisibilityResult& visibility,
        bool enabled);

SectorStaticSpecularShaderLocations GetSectorStaticSpecularShaderLocations(
        Shader shader);

void UploadSectorStaticSpecularLights(
        Shader shader,
        const SectorStaticSpecularShaderLocations& locations,
        const SectorStaticSpecularLightContext& context);

} // namespace game
