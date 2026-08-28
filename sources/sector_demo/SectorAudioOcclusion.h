#pragma once

#include "engine/audio/AudioSystem.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorPortalVisibility.h"

#include <raylib.h>

#include <cstddef>
#include <string>
#include <vector>

namespace game {

class SectorCollisionWorld;

inline constexpr float SectorOccludedSoundVolumeScale = 0.2f;
inline constexpr float SectorUnfilteredSoundCutoffHz = 20000.0f;
inline constexpr float SectorFirstBarrierSoundCutoffHz = 2000.0f;
inline constexpr float SectorMinimumSoundCutoffHz = 250.0f;

struct SectorSoundPropagationPath {
    bool valid = false;
    Vector3 apparentPosition{};
    float distanceWorld = 0.0f;
    float volumeScale = 1.0f;
    float lowPassCutoffHz = SectorUnfilteredSoundCutoffHz;
    int barrierCount = 0;
    int portalCount = 0;
    float diffractionAmount = 0.0f;
};

struct SectorSoundPropagationResult {
    SectorSoundPropagationPath transmission;
    SectorSoundPropagationPath portal;
};

class SectorSoundPropagationWorld {
public:
    bool Build(const SectorTopologyMap& map, std::string* errorMessage = nullptr);
    void Clear();
    bool IsValid() const { return valid; }

    SectorSoundPropagationResult Evaluate(
            const SectorCollisionWorld* collisionWorld,
            const std::vector<SectorDynamicDoorCollider>& doorColliders,
            const std::vector<RuntimePortalDynamicBlocker>& portalBlockers,
            Vector3 listenerPosition,
            Vector3 sourcePosition) const;

private:
    RuntimeSectorVisibilityGraph graph;
    mutable std::vector<float> portalDistances;
    mutable std::vector<int> portalPredecessors;
    mutable std::vector<unsigned char> portalVisited;
    mutable std::vector<int> pathPortalIndices;
    bool valid = false;
};

struct SectorAudioOcclusionContext {
    const SectorCollisionWorld* collisionWorld = nullptr;
    const std::vector<SectorDynamicDoorCollider>* doorColliders = nullptr;
    const std::vector<RuntimePortalDynamicBlocker>* portalBlockers = nullptr;
    const SectorSoundPropagationWorld* propagationWorld = nullptr;
};

float ComputeSectorSoundOcclusion(
        const SectorCollisionWorld* collisionWorld,
        const std::vector<SectorDynamicDoorCollider>& doorColliders,
        Vector3 listenerPosition,
        Vector3 sourcePosition);

float QuerySectorSoundOcclusion(
        void* context,
        Vector3 listenerPosition,
        Vector3 sourcePosition);

engine::PositionalSoundPropagation QuerySectorSoundPropagation(
        void* context,
        Vector3 listenerPosition,
        const engine::PositionalSoundSettings& source);

} // namespace game
