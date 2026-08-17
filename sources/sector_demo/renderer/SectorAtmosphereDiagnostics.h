#pragma once

#include "sector_demo/SectorViewPose.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereContracts.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereMath.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace game {

enum class SectorAtmosphereBackend {
    Legacy,
    Unified
};

const char* SectorAtmosphereBackendName(SectorAtmosphereBackend backend);

enum class SectorAtmosphereGpuPass : std::size_t {
    Total,
    LocalFog,
    Haze,
    Unified,
    Dust,
    Count
};

inline constexpr std::size_t SectorAtmosphereGpuPassCount =
        static_cast<std::size_t>(SectorAtmosphereGpuPass::Count);
inline constexpr std::size_t SectorAtmosphereGpuQueryLatency = 4;
inline constexpr std::size_t SectorAtmosphereCaptureWarmupFrames = 60;
inline constexpr std::size_t SectorAtmosphereCaptureSampleFrames = 300;

const char* SectorAtmosphereGpuPassName(SectorAtmosphereGpuPass pass);

struct SectorAtmosphereGpuTimingFrame {
    std::uint64_t sequence = 0;
    std::array<double, SectorAtmosphereGpuPassCount> milliseconds{};
    bool valid = false;
};

struct SectorAtmosphereSampleStatistics {
    std::size_t sampleCount = 0;
    double minimum = 0.0;
    double median = 0.0;
    double percentile95 = 0.0;
    double maximum = 0.0;
    bool valid = false;
};

SectorAtmosphereSampleStatistics ComputeSectorAtmosphereSampleStatistics(
        const double* samples,
        std::size_t sampleCount);

struct SectorAtmosphereCaptureMetadata {
    SectorAtmosphereBackend backend = SectorAtmosphereBackend::Unified;
    SectorTopologyFogSettings::VolumetricQuality quality =
            SectorTopologyFogSettings::VolumetricQuality::Off;
    int sceneWidth = 0;
    int sceneHeight = 0;
    int localFogEligible = 0;
    int localFogActive = 0;
    int hazeEligible = 0;
    int hazeActive = 0;
    int unifiedIntegrations = 0;
    int unifiedLightEligible = 0;
    int unifiedLightActive = 0;
    int dustEligible = 0;
    int dustActive = 0;
    SectorViewPose cameraPose;
    float verticalFovDegrees = 0.0f;
};

bool SameSectorAtmosphereCaptureView(
        const SectorAtmosphereCaptureMetadata& left,
        const SectorAtmosphereCaptureMetadata& right);

enum class SectorAtmosphereCaptureState {
    Idle,
    Warmup,
    Capturing,
    Draining,
    Complete,
    Aborted
};

const char* SectorAtmosphereCaptureStateName(SectorAtmosphereCaptureState state);

class SectorAtmosphereCapture {
public:
    void Start(
            const SectorAtmosphereCaptureMetadata& metadata,
            std::uint64_t firstSequence,
            std::uint64_t skippedQueryFrames);
    void Cancel(const char* reason = "cancelled");
    void ValidateView(const SectorAtmosphereCaptureMetadata& metadata);
    void OnFrameIssued(std::uint64_t sequence);
    void OnFrameResolved(const SectorAtmosphereGpuTimingFrame& frame);
    void SetSkippedQueryFrames(std::uint64_t value);

    SectorAtmosphereCaptureState State() const { return state; }
    std::size_t WarmupIssued() const { return warmupIssued; }
    std::size_t CaptureIssued() const { return captureIssued; }
    std::size_t SamplesResolved() const { return samplesResolved; }
    std::uint64_t SkippedQueryFrames() const {
        return skippedQueryFrames >= skippedQueryFramesAtStart
                ? skippedQueryFrames - skippedQueryFramesAtStart
                : 0;
    }
    const SectorAtmosphereCaptureMetadata& Metadata() const { return metadata; }
    const std::string& Report() const { return report; }
    const std::string& Status() const { return status; }
    SectorAtmosphereSampleStatistics Statistics(SectorAtmosphereGpuPass pass) const;

private:
    void Complete();

    SectorAtmosphereCaptureState state = SectorAtmosphereCaptureState::Idle;
    SectorAtmosphereCaptureMetadata metadata;
    std::uint64_t firstSequence = 0;
    std::uint64_t skippedQueryFramesAtStart = 0;
    std::uint64_t skippedQueryFrames = 0;
    std::size_t warmupIssued = 0;
    std::size_t captureIssued = 0;
    std::size_t samplesResolved = 0;
    std::array<std::array<double, SectorAtmosphereCaptureSampleFrames>,
            SectorAtmosphereGpuPassCount> samples{};
    std::array<bool, SectorAtmosphereCaptureSampleFrames> receivedSamples{};
    std::string report;
    std::string status = "idle";
};

class SectorAtmosphereTimestampQueryRingState {
public:
    static constexpr std::size_t Latency = SectorAtmosphereGpuQueryLatency;

    struct Decision {
        std::size_t slot = 0;
        bool resolvedPrevious = false;
        std::uint64_t resolvedSequence = 0;
        bool issueCurrent = false;
        std::uint64_t issueSequence = 0;
    };

    std::size_t CurrentSlot() const { return frameIndex % Latency; }
    bool CurrentSlotOccupied() const { return occupied[CurrentSlot()]; }
    Decision Advance(bool occupiedSlotReady);
    void Reset();

    std::uint64_t NextSequence() const { return nextSequence; }
    std::uint64_t SkippedFrames() const { return skippedFrames; }

private:
    std::array<bool, Latency> occupied{};
    std::array<std::uint64_t, Latency> sequences{};
    std::size_t frameIndex = 0;
    std::uint64_t nextSequence = 0;
    std::uint64_t skippedFrames = 0;
};

struct SectorAtmosphereTargetDiagnostics {
    int width = 0;
    int height = 0;
    int marchSteps = 0;
    int eligibleCount = 0;
    int activeCount = 0;
    std::uint64_t estimatedBytes = 0;
    std::string resourceStatus = "not allocated";
};

struct SectorAtmosphereDiagnostics {
    SectorAtmosphereBackend backend = SectorAtmosphereBackend::Unified;
    SectorTopologyFogSettings::VolumetricQuality requestedQuality =
            SectorTopologyFogSettings::VolumetricQuality::Off;
    SectorTopologyFogSettings::VolumetricQuality effectiveQuality =
            SectorTopologyFogSettings::VolumetricQuality::Off;
    int sceneWidth = 0;
    int sceneHeight = 0;
    std::uint64_t estimatedIntermediateBytes = 0;
    SectorAtmosphereTargetDiagnostics localFog;
    SectorAtmosphereTargetDiagnostics haze;
    SectorAtmosphereTargetDiagnostics unified;
    int unifiedIntegrationCount = 0;
    int unifiedLightEligibleCount = 0;
    int unifiedLightActiveCount = 0;
    SectorVolumetricGridSize unifiedGrid;
    int unifiedAtlasWidth = 0;
    int unifiedAtlasHeight = 0;
    int unifiedClusterListWidth = 0;
    int unifiedClusterListHeight = 0;
    int unifiedLightViewOverflowCount = 0;
    std::uint64_t unifiedLightClusterOverflowCount = 0;
    int unifiedVolumeViewOverflowCount = 0;
    std::uint64_t unifiedVolumeClusterOverflowCount = 0;
    bool unifiedHistoryEnabled = false;
    bool unifiedHistoryValid = false;
    bool unifiedHistoryFrozen = false;
    std::uint64_t unifiedHistoryFrameCount = 0;
    int unifiedJitterPeriod = 1;
    float unifiedBaseCurrentFrameWeight = 1.0f;
    float unifiedResponsiveCurrentFrameWeight = 1.0f;
    SectorVolumetricHistoryResetReason unifiedHistoryResetReason =
            SectorVolumetricHistoryResetReason::FirstFrame;
    SectorVolumetricDebugView unifiedDebugView =
            SectorVolumetricDebugView::Composite;
    int unifiedShadowedSpotLightCount = 0;
    int dustEligible = 0;
    int dustActive = 0;
    int dustVisibleParticles = 0;
    std::string dustResourceStatus = "not allocated";
    std::string scratchResourceStatus = "not allocated";
    std::string fallbackStatus = "not rendered";
    std::string gpuTimingStatus = "not initialized";
    SectorAtmosphereGpuTimingFrame latestGpuTimings;
    std::uint64_t skippedGpuQueryFrames = 0;
};

std::string FormatSectorAtmosphereBytes(std::uint64_t bytes);
std::string FormatSectorAtmosphereDiagnosticsSummary(
        const SectorAtmosphereDiagnostics& diagnostics);
std::string FormatSectorAtmosphereCaptureReport(
        const SectorAtmosphereCaptureMetadata& metadata,
        const std::array<SectorAtmosphereSampleStatistics,
                SectorAtmosphereGpuPassCount>& statistics,
        std::uint64_t skippedQueryFrames);

} // namespace game
