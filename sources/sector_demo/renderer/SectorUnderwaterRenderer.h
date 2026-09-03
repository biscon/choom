#pragma once

#include "engine/assets/AssetHandles.h"
#include "sector_demo/SectorLiquidInteraction.h"

#include <raylib.h>

#include <array>
#include <cstdint>

namespace engine {
class AssetManager;
}

namespace game {

class SectorCollisionWorld;

struct SectorUnderwaterRenderContext {
    bool cameraSubmerged = false;
    SectorLiquidContact contact;
    float causticsStrength = 0.0f;
    float causticsScaleMultiplier = 1.0f;
    float causticsSpeedMultiplier = 1.0f;
};

class SectorUnderwaterRenderer {
public:
    static constexpr int MaxParticles = SectorLiquidMaxParticulateAmount;

    bool Initialize(
            engine::AssetManager& assets,
            engine::AssetScopeHandle scope);
    void Shutdown();

    bool ApplyCaustics(
            RenderTexture2D& sceneTarget,
            RenderTexture2D& sceneScratch,
            engine::AssetManager& assets,
            const Camera3D& camera,
            float runtimeSeconds,
            const SectorUnderwaterRenderContext& context);
    bool DrawParticles(
            RenderTexture2D& sceneTarget,
            const SectorCollisionWorld* sectorLookup,
            const Camera3D& camera,
            float runtimeSeconds,
            const SectorUnderwaterRenderContext& context);

    int VisibleParticleCount() const { return visibleParticleCount; }
    bool ResourcesReady() const { return resourcesReady; }

private:
    struct Particle {
        bool active = false;
        Vector3 position = {};
        Vector3 driftDirection = {};
        float phase = 0.0f;
        float sizeScale = 1.0f;
    };

    struct CausticsLocations {
        int sceneColor = -1;
        int sceneDepth = -1;
        int lookup = -1;
        int viewportSize = -1;
        int cameraPosition = -1;
        int cameraForward = -1;
        int cameraRight = -1;
        int cameraUp = -1;
        int tanHalfFov = -1;
        int aspectRatio = -1;
        int nearPlane = -1;
        int farPlane = -1;
        int surfaceY = -1;
        int visibilityDepth = -1;
        int ripple = -1;
        int flow = -1;
        int visual = -1;
        int runtimeSeconds = -1;
    };

    struct ParticleLocations {
        int tint = -1;
    };

    bool InitializeCaustics(
            engine::AssetManager& assets,
            engine::AssetScopeHandle scope);
    bool InitializeParticles();
    bool SpawnParticle(
            Particle& particle,
            const SectorCollisionWorld* sectorLookup,
            const Camera3D& camera,
            const SectorLiquidContact& contact);
    void ResetParticles();
    int BuildParticleMesh(
            const Camera3D& camera,
            const SectorLiquidParticulateSettings& settings);

    Shader causticsShader = {};
    CausticsLocations causticsLocations;
    engine::TextureHandle causticsLookup;

    Shader particleShader = {};
    Material particleMaterial = {};
    Texture2D particleDefaultTexture = {};
    Mesh particleMesh = {};
    ParticleLocations particleLocations;
    std::array<Particle, MaxParticles> particles{};
    std::array<float, MaxParticles * 4 * 3> particleVertices{};
    std::array<float, MaxParticles * 4 * 2> particleTexcoords{};
    std::array<unsigned char, MaxParticles * 4 * 4> particleColors{};
    std::array<unsigned short, MaxParticles * 6> particleIndices{};

    Vector3 previousCameraPosition = {};
    float previousRuntimeSeconds = 0.0f;
    std::uint32_t spawnSequence = 1;
    int activeSectorId = 0;
    int visibleParticleCount = 0;
    bool hasPreviousCamera = false;
    bool resourcesReady = false;
};

} // namespace game
