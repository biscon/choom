#include "sector_demo/renderer/SectorAtmosphereDiagnostics.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereContracts.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::cerr << "FAILED: " << description << '\n';
        ++failures;
    }
}

bool Near(double left, double right)
{
    return std::fabs(left - right) < 0.000001;
}

void TestQualityContractsAndLegacyParity()
{
    using Quality = game::SectorTopologyFogSettings::LocalVolumeQuality;
    const game::SectorVolumetricQualityContract low =
            game::GetSectorVolumetricQualityContract(Quality::Low);
    const game::SectorVolumetricQualityContract medium =
            game::GetSectorVolumetricQualityContract(Quality::Medium);
    const game::SectorVolumetricQualityContract high =
            game::GetSectorVolumetricQualityContract(Quality::High);
    Check(low.referenceWidth == 120 && low.referenceHeight == 68
                    && low.depthSlices == 32 && low.clusterBands == 4
                    && !low.temporalResolve,
          "Low volumetric preset matches the fixed contract");
    Check(medium.referenceWidth == 160 && medium.referenceHeight == 90
                    && medium.depthSlices == 48 && medium.clusterBands == 6
                    && medium.temporalResolve,
          "Medium volumetric preset matches the fixed contract");
    Check(high.referenceWidth == 240 && high.referenceHeight == 135
                    && high.depthSlices == 64 && high.clusterBands == 8
                    && high.temporalResolve,
          "High volumetric preset matches the fixed contract");

    const game::SectorLegacyAtmosphereQualityContract legacyLow =
            game::GetSectorLegacyAtmosphereQualityContract(Quality::Low);
    const game::SectorLegacyAtmosphereQualityContract legacyMedium =
            game::GetSectorLegacyAtmosphereQualityContract(Quality::Medium);
    const game::SectorLegacyAtmosphereQualityContract legacyHigh =
            game::GetSectorLegacyAtmosphereQualityContract(Quality::High);
    Check(legacyLow.localFogTargetScale == 0.25f
                    && legacyLow.localFogMarchSteps == 4
                    && legacyLow.maximumLocalFogVolumes == 4
                    && legacyLow.maximumHazeVolumes == 2,
          "Low legacy quality preserves target, step, and volume limits");
    Check(legacyMedium.localFogTargetScale == 0.5f
                    && legacyMedium.localFogMarchSteps == 8
                    && legacyMedium.maximumLocalFogVolumes == 8
                    && legacyMedium.maximumHazeVolumes == 4,
          "Medium legacy quality preserves target, step, and volume limits");
    Check(legacyHigh.localFogTargetScale == 1.0f
                    && legacyHigh.localFogMarchSteps == 12
                    && legacyHigh.maximumLocalFogVolumes == 16
                    && legacyHigh.maximumHazeVolumes == 8,
          "High legacy quality preserves target, step, and volume limits");
}

void TestGridAtlasAndClusterLayouts()
{
    using Quality = game::SectorTopologyFogSettings::LocalVolumeQuality;
    const game::SectorVolumetricGridSize low =
            game::ComputeSectorVolumetricGridSize(Quality::Low, 1920, 1080);
    Check(low.x == 120 && low.y == 68 && low.z == 32,
          "16:9 Low grid uses its reference cap");
    const game::SectorVolumetricGridSize fourByThree =
            game::ComputeSectorVolumetricGridSize(Quality::Low, 1024, 768);
    Check(fourByThree.x == 91 && fourByThree.y == 68,
          "grid sizing preserves a narrower scene aspect inside the cap");
    const game::SectorVolumetricGridSize small =
            game::ComputeSectorVolumetricGridSize(Quality::High, 80, 40);
    Check(small.x == 80 && small.y == 40 && small.z == 64,
          "grid sizing does not upscale a small scene target");
    Check(game::ComputeSectorVolumetricGridSize(Quality::Off, 1920, 1080).z == 0,
          "Off quality produces no grid");

    const game::SectorVolumetricAtlasLayout lowAtlas =
            game::ComputeSectorVolumetricAtlasLayout(low);
    Check(lowAtlas.tileColumns == 4 && lowAtlas.tileRows == 8
                    && lowAtlas.width == 480 && lowAtlas.height == 544,
          "Low atlas chooses the minimum-maximum-dimension near-square packing");
    const game::SectorVolumetricGridSize highGrid =
            game::ComputeSectorVolumetricGridSize(Quality::High, 1920, 1080);
    const game::SectorVolumetricAtlasLayout highAtlas =
            game::ComputeSectorVolumetricAtlasLayout(highGrid);
    Check(highAtlas.tileColumns == 6 && highAtlas.tileRows == 11
                    && highAtlas.width == 1440 && highAtlas.height == 1485,
          "High atlas packs 64 rectangular slices near-square");
    game::SectorVolumetricAtlasTexel texel;
    Check(game::ComputeSectorVolumetricAtlasTexel(
                    highAtlas, 239, 134, 63, texel)
                    && texel.x == 959 && texel.y == 1484,
          "last High froxel maps inside the final atlas tile");
    Check(!game::ComputeSectorVolumetricAtlasTexel(
                    highAtlas, 240, 0, 0, texel),
          "out-of-range froxel coordinates are rejected");

    const game::SectorVolumetricClusterListLayout clusters =
            game::ComputeSectorVolumetricClusterListLayout(highGrid, 8);
    Check(clusters.width == 960 && clusters.height == 1080
                    && clusters.estimatedBytes == 4147200,
          "High cluster list packs four RGBA8 texels per XY/band cluster");
    Check(game::SectorVolumetricMaximumViewLights == 254
                    && game::SectorVolumetricMaximumViewVolumes == 254
                    && game::SectorVolumetricMaximumClusterLights == 16
                    && game::SectorVolumetricMaximumClusterVolumes == 16
                    && game::SectorVolumetricFirstReservedIndex == 254
                    && game::SectorVolumetricListTerminator == 255,
          "final view, cluster, reserved-index, and terminator budgets are fixed");
}

void TestStatisticsAndFormatting()
{
    double samples[20]{};
    for (int index = 0; index < 20; ++index) samples[index] = index + 1.0;
    const game::SectorAtmosphereSampleStatistics statistics =
            game::ComputeSectorAtmosphereSampleStatistics(samples, 20);
    Check(statistics.valid && statistics.sampleCount == 20
                    && Near(statistics.minimum, 1.0)
                    && Near(statistics.median, 10.5)
                    && Near(statistics.percentile95, 19.0)
                    && Near(statistics.maximum, 20.0),
          "statistics use even median and nearest-rank p95");
    Check(!game::ComputeSectorAtmosphereSampleStatistics(nullptr, 0).valid,
          "empty timing samples remain unavailable");
    Check(game::FormatSectorAtmosphereBytes(4ull * 1024ull * 1024ull)
                    == "4.00 MiB",
          "diagnostic byte formatter produces stable MiB text");

    game::SectorAtmosphereDiagnostics diagnostics;
    diagnostics.effectiveQuality =
            game::SectorTopologyFogSettings::LocalVolumeQuality::Medium;
    diagnostics.sceneWidth = 2880;
    diagnostics.sceneHeight = 1620;
    diagnostics.haze.eligibleCount = 11;
    diagnostics.haze.activeCount = 4;
    diagnostics.fallbackStatus = "legacy backend active";
    const std::string summary =
            game::FormatSectorAtmosphereDiagnosticsSummary(diagnostics);
    Check(summary.find("Legacy Medium") != std::string::npos
                    && summary.find("haze 11/4") != std::string::npos
                    && summary.find("legacy backend active") != std::string::npos,
          "legacy diagnostic summary contains quality, counts, and status");
    diagnostics.backend = game::SectorAtmosphereBackend::Unified;
    diagnostics.unifiedIntegrationCount = 1;
    const std::string unifiedSummary =
            game::FormatSectorAtmosphereDiagnosticsSummary(diagnostics);
    Check(unifiedSummary.find("Unified Medium") != std::string::npos
                    && unifiedSummary.find("unified 1") != std::string::npos,
          "unified diagnostic summary identifies its backend and one integration");
    Check(std::string(game::SectorAtmosphereGpuPassName(
                    game::SectorAtmosphereGpuPass::Unified))
                    == "unified integration/composite",
          "unified GPU subpass has a stable report name");
}

game::SectorAtmosphereCaptureMetadata MakeMetadata()
{
    game::SectorAtmosphereCaptureMetadata metadata;
    metadata.quality = game::SectorTopologyFogSettings::LocalVolumeQuality::High;
    metadata.sceneWidth = 2880;
    metadata.sceneHeight = 1620;
    metadata.hazeEligible = 11;
    metadata.hazeActive = 8;
    metadata.cameraPose.position = Vector3{1.0f, 2.0f, 3.0f};
    metadata.cameraPose.yawRadians = 0.25f;
    metadata.verticalFovDegrees = 75.0f;
    return metadata;
}

void TestCaptureWarmupAndCompletion()
{
    game::SectorAtmosphereCapture capture;
    capture.Start(MakeMetadata(), 100, 7);
    for (std::uint64_t sequence = 100; sequence < 460; ++sequence) {
        capture.OnFrameIssued(sequence);
        game::SectorAtmosphereGpuTimingFrame frame;
        frame.sequence = sequence;
        frame.valid = true;
        frame.milliseconds.fill(static_cast<double>(sequence));
        capture.OnFrameResolved(frame);
    }
    capture.SetSkippedQueryFrames(9);
    Check(capture.State() == game::SectorAtmosphereCaptureState::Complete
                    && capture.WarmupIssued() == 60
                    && capture.CaptureIssued() == 300
                    && capture.SamplesResolved() == 300,
          "capture excludes 60 issued warmup frames and resolves 300 samples");
    const game::SectorAtmosphereSampleStatistics statistics =
            capture.Statistics(game::SectorAtmosphereGpuPass::Total);
    Check(statistics.valid && Near(statistics.minimum, 160.0)
                    && Near(statistics.median, 309.5)
                    && Near(statistics.percentile95, 444.0)
                    && Near(statistics.maximum, 459.0),
          "capture statistics exclude every warmup sample");
    Check(capture.Report().find("n=300") != std::string::npos,
          "completed capture produces a paste-ready report");
}

void TestCaptureAbortAndEmptyReport()
{
    game::SectorAtmosphereCapture capture;
    const game::SectorAtmosphereCaptureMetadata metadata = MakeMetadata();
    capture.Start(metadata, 0, 0);
    game::SectorAtmosphereCaptureMetadata moved = metadata;
    moved.cameraPose.position.x += 0.01f;
    capture.ValidateView(moved);
    Check(capture.State() == game::SectorAtmosphereCaptureState::Aborted
                    && capture.Report().find("camera") != std::string::npos,
          "capture aborts when its fixed view changes");

    game::SectorAtmosphereCapture backendCapture;
    backendCapture.Start(metadata, 0, 0);
    game::SectorAtmosphereCaptureMetadata switched = metadata;
    switched.backend = game::SectorAtmosphereBackend::Unified;
    backendCapture.ValidateView(switched);
    Check(backendCapture.State() == game::SectorAtmosphereCaptureState::Aborted,
          "capture aborts when the comparison backend changes");

    std::array<game::SectorAtmosphereSampleStatistics,
            game::SectorAtmosphereGpuPassCount> empty{};
    const std::string report = game::FormatSectorAtmosphereCaptureReport(
            metadata, empty, 0);
    Check(report.find("unavailable") != std::string::npos,
          "empty capture statistics format as unavailable");
}

void TestQueryRingLatencyAndUnavailableSlots()
{
    game::SectorAtmosphereTimestampQueryRingState ring;
    for (std::uint64_t sequence = 0; sequence < 4; ++sequence) {
        const auto decision = ring.Advance(false);
        Check(decision.issueCurrent && !decision.resolvedPrevious
                        && decision.issueSequence == sequence,
              "first four query frames fill distinct latency slots");
    }
    const auto unavailable = ring.Advance(false);
    Check(!unavailable.issueCurrent && !unavailable.resolvedPrevious
                    && ring.SkippedFrames() == 1 && ring.NextSequence() == 4,
          "unavailable reused slot is neither blocked on nor overwritten");
    ring.Advance(true);
    ring.Advance(true);
    ring.Advance(true);
    const auto recovered = ring.Advance(true);
    Check(recovered.resolvedPrevious && recovered.resolvedSequence == 0
                    && recovered.issueCurrent && recovered.issueSequence == 7,
          "delayed unavailable slot is consumed only when revisited ready");
}

} // namespace

int main()
{
    TestQualityContractsAndLegacyParity();
    TestGridAtlasAndClusterLayouts();
    TestStatisticsAndFormatting();
    TestCaptureWarmupAndCompletion();
    TestCaptureAbortAndEmptyReport();
    TestQueryRingLatencyAndUnavailableSlots();
    if (failures != 0) {
        std::cerr << failures << " SectorAtmosphereDiagnosticsTests failure(s)\n";
        return 1;
    }
    return 0;
}
