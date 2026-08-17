#pragma once

#include "sector_demo/renderer/SectorAtmosphereDiagnostics.h"

#include <array>
#include <cstdint>
#include <string>

namespace game {

class SectorAtmosphereGpuProfiler {
public:
    bool Initialize();
    void Shutdown();

    bool BeginFrame(SectorAtmosphereGpuTimingFrame& outResolvedFrame);
    void Begin(SectorAtmosphereGpuPass pass);
    void End(SectorAtmosphereGpuPass pass);
    void EndFrame();

    bool IsAvailable() const { return initialized; }
    bool IsFrameActive() const { return frameActive; }
    std::uint64_t CurrentSequence() const { return currentSequence; }
    std::uint64_t NextSequence() const { return ring.NextSequence(); }
    std::uint64_t SkippedFrames() const { return ring.SkippedFrames(); }
    const std::string& Status() const { return status; }

private:
    static constexpr std::size_t QueriesPerPass = 2;
    static constexpr std::size_t QueryCount = SectorAtmosphereGpuQueryLatency
            * SectorAtmosphereGpuPassCount * QueriesPerPass;

    unsigned int Query(
            std::size_t slot,
            std::size_t pass,
            std::size_t endpoint) const;
    bool SlotReady(std::size_t slot) const;
    SectorAtmosphereGpuTimingFrame ReadSlot(
            std::size_t slot,
            std::uint64_t sequence) const;

    std::array<unsigned int, QueryCount> queries{};
    SectorAtmosphereTimestampQueryRingState ring;
    std::array<bool, SectorAtmosphereGpuPassCount> begun{};
    std::array<bool, SectorAtmosphereGpuPassCount> ended{};
    std::size_t currentSlot = 0;
    std::uint64_t currentSequence = 0;
    bool initialized = false;
    bool frameActive = false;
    std::string status = "not initialized";
};

} // namespace game
