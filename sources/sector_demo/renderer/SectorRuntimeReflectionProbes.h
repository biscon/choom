#pragma once

#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "engine/render/RenderTarget.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include <array>

namespace game
{
class SectorMeshRenderer;
struct SectorRuntimeDoorLightingContext;

struct SectorReflectionCaptureDrawContext
{
    Camera3D camera{};
    RuntimePortalVisibilityResult visibility;
    SectorDynamicLightingRenderer *lighting = nullptr;
    float seconds = 0;
};

struct SectorRuntimeReflectionStats
{
    int activeProbeId = -1;
    int face = 0, mip = 0;
    std::size_t queued = 0, ready = 0, failed = 0, required = 0, prepared = 0;
    std::size_t completed = 0, discarded = 0, overruns = 0;
    double gpuMilliseconds = 0, cpuMilliseconds = 0;
    std::uint64_t allocationBytes = 0;
};

// One level-owned GPU job. The pure probe records contain scheduling state;
// this backend owns only framebuffer/shader/query and independent shadow state.
class SectorRuntimeReflectionProbes
{
  public:
    bool Initialize(engine::AssetManager &assets, engine::AssetScopeHandle scope,
                    SectorPbrEnvironment &environment, std::size_t lightCapacity,
                    std::size_t receiverCapacity, std::size_t objectCapacity);
    void Shutdown();
    void Invalidate(SectorPbrEnvironment &environment, bool discontinuity = true);
    void ObserveLights(SectorPbrEnvironment &environment,
                       const SectorDynamicLightingRenderer &lights, float seconds);
    void RequireInitial(SectorPbrEnvironment &environment,
                        const RuntimePortalVisibilityResult &visibility, int startSector);
    bool InitialReady(const SectorPbrEnvironment &environment) const;
    void Step(engine::AssetManager &assets, SectorPbrEnvironment &environment,
              SectorMeshRenderer &renderer, engine::World *world,
              SectorRuntimeDoorLightingContext doorLighting, bool preparing);
    const SectorRuntimeReflectionStats &Stats() const
    {
        return stats;
    }
    bool paused = false;

  private:
    bool initialized = false, initialRequired = false;
    int active = -1, face = 0, mip = 1, filterFace = 0, tileX = 0, tileY = 0;
    bool shadowsReady = false;
    int shadowFrames = 0;
    std::uint64_t capturedRevision = 0, capturedDiscontinuity = 0;
    double capturedAt = 0;
    std::array<engine::TextureHandle, 3> raw{};
    std::array<engine::RenderTarget, 3> targets;
    unsigned int framebuffer = 0, vertexArray = 0;
    Shader filterShader{};
    int faceLoc = -1, sizeLoc = -1, roughnessLoc = -1, copyLoc = -1;
    std::array<unsigned int, 8> queries{};
    std::array<bool, 4> queryPending{};
    std::size_t querySlot = 0;
    double estimatedTileMs = 0.1;
    std::array<int, 4> timedTiles{};
    struct DoorPose
    {
        engine::Entity entity;
        SectorObjectTransform transform;
        Vector2 widthAxis{}, thicknessAxis{};
        Matrix leaf{}, frame{};
        BoundingBox receiverBounds{}, analyticBounds{};
        bool model = false;
    };
    std::vector<DoorPose> doors;
    void SnapshotDoors(engine::World *world);
    void SwapDoorPoses(engine::World *world);
    SectorDynamicLightingRenderer captureLights;
    SectorReflectionCaptureDrawContext draw;
    std::vector<SectorPreviewDynamicPointLightSource> observed, current, snapshot;
    SectorRuntimeReflectionStats stats;
    bool FilterTile(engine::AssetManager &assets, unsigned int output, int resolution,
                    int outputFace, int outputMip, int x, int y, bool copy);
};
} // namespace game
