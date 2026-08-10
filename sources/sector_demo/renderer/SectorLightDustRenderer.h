#pragma once

#include "engine/render/RenderTarget.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <vector>

namespace game {

class SectorLightDustRenderer {
public:
    static constexpr int MaxEmitters = 16;
    static constexpr int MaxParticles = 512;
    static constexpr int MaxParticlesPerEmitter = 128;

    bool Apply(
            RenderTexture2D& sceneTarget,
            const SectorTopologyMap& map,
            const Camera3D& camera,
            float runtimeSeconds,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            const SectorBillboardDynamicLightContext& dynamicLights,
            const std::vector<SectorLightAtmosphereSource>& sources,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& receiverBounds);
    void Shutdown();

    int EligibleEmitterCount() const { return eligibleEmitterCount; }
    int ActiveEmitterCount() const { return activeEmitterCount; }
    int VisibleParticleCount() const { return visibleParticleCount; }

private:
    struct Emitter {
        SectorLightAtmosphereVolume volume;
        SectorLightDustSettings settings;
    };

    struct Particle {
        bool active = false;
        SectorLightAtmosphereSourceKind sourceKind = SectorLightAtmosphereSourceKind::StaticPoint;
        int lightId = 0;
        int preferredSectorId = 0;
        Vector3 position = {};
        Vector3 velocity = {};
        Vector3 staticLighting = {};
        Vector3 lightingSamplePosition = {};
        float sizeWorld = 0.01f;
        float ageSeconds = 0.0f;
        float lifetimeSeconds = 10.0f;
        float phase = 0.0f;
        float opacity = 0.0f;
        Color scatteringTint = WHITE;
    };

    struct ShaderLocations {
        int sceneDepth = -1;
        int viewportSize = -1;
        int nearPlane = -1;
        int farPlane = -1;
        int cameraPosition = -1;
        int fogEnabled = -1;
        int fogStartDistance = -1;
        int fogDensity = -1;
        int fogMaxOpacity = -1;
        int fogReferenceHeight = -1;
        int fogHeightFalloff = -1;
        SectorDynamicLightShaderLocations dynamicLights;
        SectorDynamicSpotLightShadowShaderLocations shadows;
        int shadowMap0 = -1;
        int shadowMap1 = -1;
    };

    bool EnsureResources(int width, int height);
    bool EnsureShader();
    bool EnsureMesh();
    bool EnsureTarget(int width, int height);
    void ClearBorrowedMaterialTextures();
    void BuildEmitters(
            const SectorTopologyMap& map,
            const Camera3D& camera,
            const SectorBillboardDynamicLightContext& dynamicLights,
            const std::vector<SectorLightAtmosphereSource>& sources,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& receiverBounds,
            float nearPlane,
            float farPlane,
            float aspectRatio);
    void UpdateParticles(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            float dt,
            float runtimeSeconds);
    int BuildMesh(const Camera3D& camera);
    const Emitter* FindEmitter(SectorLightAtmosphereSourceKind kind, int lightId) const;
    Particle* FindFreeParticle();
    void SpawnParticle(
            Particle& particle,
            const Emitter& emitter,
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            bool initialFill);

    Shader shader = {};
    Material material = {};
    Texture2D defaultMaterialTexture = {};
    Mesh mesh = {};
    engine::RenderTarget dustTarget;
    ShaderLocations locations;
    std::array<Emitter, MaxEmitters> emitters{};
    std::array<Particle, MaxParticles> particles{};
    std::array<float, MaxParticles * 4 * 3> vertices{};
    std::array<float, MaxParticles * 4 * 3> normals{};
    std::array<float, MaxParticles * 4 * 2> texcoords{};
    std::array<unsigned char, MaxParticles * 4 * 4> colors{};
    std::array<unsigned short, MaxParticles * 6> indices{};
    float previousRuntimeSeconds = 0.0f;
    std::uint32_t spawnSequence = 1;
    int targetWidth = 0;
    int targetHeight = 0;
    int eligibleEmitterCount = 0;
    int activeEmitterCount = 0;
    int visibleParticleCount = 0;
    bool materialLoaded = false;
    bool warnedUnavailable = false;
};

} // namespace game
