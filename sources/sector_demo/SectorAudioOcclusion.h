#pragma once

#include "sector_demo/SectorDoorRuntime.h"

#include <raylib.h>

#include <vector>

namespace game {

class SectorCollisionWorld;

inline constexpr float SectorOccludedSoundVolumeScale = 0.2f;

struct SectorAudioOcclusionContext {
    const SectorCollisionWorld* collisionWorld = nullptr;
    const std::vector<SectorDynamicDoorCollider>* doorColliders = nullptr;
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

} // namespace game
