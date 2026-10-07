#pragma once
#include "engine/particles/ParticleSystem.h"
#include "engine/assets/AssetManager.h"
#include "sector_demo/particles/SectorParticleSettings.h"
#include <vector>

namespace game {
class SectorCollisionWorld;
struct SectorTopologyMap;
struct SectorBakedObjectLightProbeRuntimeData;
struct SectorBillboardDynamicLightContext;
struct RuntimePortalVisibilityResult;

class SectorParticleRenderer {
public:
    bool Initialize(engine::AssetManager& assets, engine::AssetScopeHandle scope,
            const std::vector<SectorCompiledParticleEmitter>& emitters, size_t capacity = 8192);
    bool Reconfigure(const std::vector<SectorCompiledParticleEmitter>& emitters);
    void Shutdown();
    void Restart();
    void Update(float dt, const std::vector<SectorCompiledParticleEmitter>& emitters,
            const SectorCollisionWorld* collision, const Camera3D& camera, bool prewarm = true, bool previewFloor = false);
    // Caller supplies a color-only target when depth is sampled, avoiding FBO feedback.
    void Draw(engine::AssetManager& assets, RenderTexture2D target, const Texture2D* depth,
            const Camera3D& camera, const SectorTopologyMap* map = nullptr,
            const SectorCollisionWorld* collision = nullptr,
            const SectorBakedObjectLightProbeRuntimeData* probes = nullptr,
            const SectorBillboardDynamicLightContext* lights = nullptr,
            const RuntimePortalVisibilityResult* visibility = nullptr, bool displaySrgb = false);
    engine::ParticlePool& Pool() { return pool; }
    const engine::ParticleDiagnostics& Diagnostics() const { return pool.diagnostics; }
private:
    struct Instance {
        Vector4 position;
        Vector4 right;
        Vector4 up;
        Vector4 color;
        Vector4 uv;
    };
    struct DrawItem { size_t particle; float depth; unsigned int texture; int role; int atlasCell = -1; };
    engine::ParticlePool pool;
    std::vector<SectorCompiledParticleEmitter> sources;
    std::vector<std::array<engine::TextureHandle, 4>> textures;
    std::array<engine::TextureHandle, 4> defaults{};
    std::vector<DrawItem> drawItems;
    std::vector<Instance> instances;
    Shader shader{};
    unsigned int vao = 0, quadVbo = 0, instanceVbo = 0;
    int matrixLoc = -1, textureLoc = -1, depthLoc = -1, hasDepthLoc = -1;
    int viewportLoc = -1, nearFarLoc = -1, displayLoc = -1;
    bool needsPrewarm = true;
};
} // namespace game
