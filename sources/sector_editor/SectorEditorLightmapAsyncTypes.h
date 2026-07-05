#pragma once

#include "sector_demo/SectorLightmap.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace game {

struct SectorLightmapBakeAsyncResult {
    bool succeeded = false;
    bool cancelled = false;
    std::string errorMessage;
    std::string bakeReportText;
    SectorLightmapBakeResult bakeResult;
    std::string expectedSourceHash;
    uint64_t sourceMapRevision = 0;
    std::string finalOutputPath;
    std::string temporaryOutputPath;
};

struct LightmapBakeProgress {
    std::atomic<SectorLightmapBakePhase> phase{SectorLightmapBakePhase::Idle};
    std::atomic<uint32_t> completedWork{0};
    std::atomic<uint32_t> totalWork{0};
    std::atomic<bool> cancelRequested{false};
    std::atomic<bool> running{false};
};

struct LightmapBakeAsyncState {
    std::thread worker;
    LightmapBakeProgress progress;
    bool modalOpen = false;
    bool awaitingAcknowledgement = false;
    bool cancelButtonPressed = false;
    double startTimeSeconds = 0.0;
    double completedTimeSeconds = 0.0;
    std::string terminalMessage;
    bool terminalSuccess = false;
    bool terminalCancelled = false;
    std::string temporaryOutputPath;
    std::mutex resultMutex;
    std::optional<SectorLightmapBakeAsyncResult> pendingResult;
};

} // namespace game
