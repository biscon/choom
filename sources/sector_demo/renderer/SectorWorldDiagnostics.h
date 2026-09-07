#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace game
{
enum class SectorWorldStage : std::size_t {
    Depth,
    Sectors,
    ModelsDoors,
    PreGlass,
    GlassTransmission,
    GlassReflection,
    Count
};
constexpr std::size_t SectorWorldStageCount = static_cast<std::size_t>(SectorWorldStage::Count);
inline constexpr const char *SectorWorldStageNames[] = {
    "depth", "sectors", "models/doors", "pre-glass", "glass transmit", "glass reflect"};
struct SectorWorldDiagnostics {
    std::array<double, SectorWorldStageCount> cpuMs{}, gpuMs{};
    std::size_t sectorMeshes = 0, sectorCulled = 0, sectorTriangles = 0;
    std::size_t objects = 0, objectsCulled = 0, modelMeshes = 0, modelTriangles = 0;
    std::size_t panes = 0, panesCulled = 0;
    std::size_t sectorTrianglesCulled = 0, modelMeshesCulled = 0, modelTrianglesCulled = 0;
};

// Timestamp pairs may be used inside the top-level elapsed-time query. Each pane
// keeps its own pair: the two blend passes must remain interleaved back-to-front.
class SectorWorldProfiler
{
  public:
    void Initialize(std::size_t maxIntervals);
    void Shutdown();
    void BeginFrame(bool enabled);
    void Begin(SectorWorldStage stage);
    void End();
    void FinishFrame();
    SectorWorldDiagnostics diagnostics;

  private:
    struct Slot {
        std::vector<unsigned int> queries;
        std::vector<SectorWorldStage> stages;
        std::size_t issued = 0;
    };
    std::array<Slot, 4> slots;
    std::size_t frame = 0, slot = 0;
    SectorWorldStage stage = SectorWorldStage::Count;
    double cpuStart = 0;
    unsigned int currentMask = 0;
    bool enabled = false, available = false, recording = false, warned = false;
};
} // namespace game
