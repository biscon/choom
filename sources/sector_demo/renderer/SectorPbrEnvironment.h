#pragma once

#include "engine/assets/AssetHandles.h"
#include "sector_demo/SectorReflectionProbeTypes.h"
#include "sector_demo/SectorPortalVisibility.h"

#include <raylib.h>

#include <vector>

namespace engine {
class AssetManager;
}

namespace game {

struct SectorTopologyMap;

// Written only by main-view receiver draws, consumed at the next frame boundary.
struct SectorReflectionDemand {
    std::vector<unsigned char> collecting;
    std::vector<unsigned char> requested;
    Camera3D camera{};
    float aspect = 1.0f, nearPlane = 0.01f, farPlane = 1000.0f;
};

struct SectorPbrEnvironment {
    engine::TextureHandle cubemap = engine::NullTextureHandle();
    struct LocalProbe {
        SectorCompiledReflectionProbe definition;
        engine::TextureHandle cubemap = engine::NullTextureHandle();
        int mipCount = 1;
        engine::TextureHandle inactive = engine::NullTextureHandle();
        bool ready = false;
        bool failed = false; // Terminal failure; recoverable captures stay queued.
        bool resourceFailed = false;
        int captureFailures = 0;
        double retryAt = 0.0;
        bool required = false;
        bool dirty = true;
        bool hasPrevious = false;
        std::uint64_t revision = 1;
        std::uint64_t discontinuity = 0;
        double dirtySince = 0.0;
        double lastStarted = -1.0;
        double publishedAt = -1.0;
    };
    std::vector<LocalProbe> localProbes;
    std::vector<RuntimePortalEdge> portals;
    std::vector<RuntimePortalDynamicBlocker> blockers;
    // Explicit non-owning draw context; owned by the runtime capture backend.
    SectorReflectionDemand* demandCollector = nullptr;
    double seconds = 0.0;
    bool active = false;
    bool usedSky = false;
};

struct SectorPbrEnvironmentSelection {
    engine::TextureHandle cubemap = engine::NullTextureHandle();
    Vector3 capturePosition = {};
    Vector3 influenceCenter = {};
    Vector3 halfExtents = {1.0f, 1.0f, 1.0f};
    float yawRadians = 0.0f;
    float intensity = 1.0f;
    float maxLod = 0.0f;
    bool boxProjection = false;
    bool localProbe = false;
    int probeId = -1;
    engine::TextureHandle previous = engine::NullTextureHandle();
    float transition = 1.0f;
    float blendDistance = 0.5f;
};

struct SectorPbrEnvironmentBlend {
    SectorPbrEnvironmentSelection first;
    SectorPbrEnvironmentSelection second;
    // Plane points from the first probe's sector into the second's sector.
    Vector4 portalPlane = {};
    Vector2 portalWidths = {};
    // Tangent x/z, projected start, aperture length; vertical aperture limits.
    Vector4 portalAperture = {};
    Vector2 portalHeights = {};
    bool portal = false;
};

SectorPbrEnvironmentBlend SelectSectorPbrEnvironmentBlend(
        const SectorPbrEnvironment& environment, Vector3 receiverPosition,
        int receiverSectorId = -1, bool includeLocalProbes = true,
        const BoundingBox* receiverBounds = nullptr,
        SectorReflectionDemand* demand = nullptr);
float SectorReflectionBlendWeight(const SectorPbrEnvironmentBlend& blend,
        Vector3 position);

inline bool IsSectorPbrEnvironmentActive(
        const SectorPbrEnvironment& environment,
        const TextureCubemap* cubemap)
{
    return environment.active
            && !engine::IsNull(environment.cubemap)
            && cubemap != nullptr
            && cubemap->id != 0;
}

bool BuildSectorPbrEnvironment(
        engine::AssetManager& assets,
        engine::AssetScopeHandle scope,
        const SectorTopologyMap& map,
        SectorPbrEnvironment& outEnvironment);

SectorPbrEnvironmentSelection SelectSectorPbrEnvironment(
        const SectorPbrEnvironment& environment,
        Vector3 receiverPosition,
        int receiverSectorId = -1,
        bool includeLocalProbes = true);

} // namespace game
