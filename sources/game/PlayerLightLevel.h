#pragma once

#include <raylib.h>

#include <array>
#include <vector>

namespace game {

class SectorCollisionWorld;
struct SectorBakedObjectLightProbeRuntimeData;
struct SectorDynamicDoorCollider;
struct SectorPreviewDynamicPointLightSource;
struct SectorStaticModelCollider;
struct SectorTopologyMap;

inline constexpr size_t PlayerLightSamplePointCount = 3;

struct PlayerLightPointSample {
    Vector3 positionWorld{};
    float bakedLight = 0.0f;
    float dynamicLight = 0.0f;
    float combinedLight = 0.0f;
    float normalizedLight = 0.0f;
};

struct PlayerLightLevelSample {
    std::array<PlayerLightPointSample, PlayerLightSamplePointCount> points{};
    float bakedLight = 0.0f;
    float dynamicLight = 0.0f;
    float combinedLight = 0.0f;
    float normalizedLight = 0.0f;
    bool bakedProbeAvailable = false;
};

PlayerLightLevelSample SamplePlayerLightLevel(
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorTopologyMap& map,
        const SectorCollisionWorld& collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        const std::vector<SectorStaticModelCollider>& staticColliders,
        const std::vector<SectorStaticModelCollider>& dynamicColliders,
        const SectorPreviewDynamicPointLightSource* runtimePointLight,
        Vector3 playerFeetPosition,
        float playerHeight,
        float playerRadius,
        int playerSectorId,
        float fullVisibilityLightLevel,
        float runtimeSeconds);

} // namespace game
