#include "sector_demo/renderer/SectorAtmosphereDiagnostics.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

namespace game {
namespace {

bool Near(float left, float right)
{
    return std::fabs(left - right) <= 0.0001f;
}

bool SamePose(const SectorViewPose& left, const SectorViewPose& right)
{
    return Near(left.position.x, right.position.x)
            && Near(left.position.y, right.position.y)
            && Near(left.position.z, right.position.z)
            && Near(left.yawRadians, right.yawRadians)
            && Near(left.pitchRadians, right.pitchRadians)
            && Near(left.rollRadians, right.rollRadians);
}

} // namespace

const char* SectorAtmosphereGpuPassName(SectorAtmosphereGpuPass pass)
{
    switch (pass) {
        case SectorAtmosphereGpuPass::Total: return "total";
        case SectorAtmosphereGpuPass::LocalFog: return "local fog";
        case SectorAtmosphereGpuPass::Haze: return "haze accumulation/composite";
        case SectorAtmosphereGpuPass::Dust: return "dust";
        case SectorAtmosphereGpuPass::Count: break;
    }
    return "unknown";
}

SectorAtmosphereSampleStatistics ComputeSectorAtmosphereSampleStatistics(
        const double* samples,
        std::size_t sampleCount)
{
    SectorAtmosphereSampleStatistics result;
    if (samples == nullptr || sampleCount == 0) return result;
    std::vector<double> sorted;
    sorted.reserve(sampleCount);
    for (std::size_t index = 0; index < sampleCount; ++index) {
        if (std::isfinite(samples[index]) && samples[index] >= 0.0) {
            sorted.push_back(samples[index]);
        }
    }
    if (sorted.empty()) return result;
    std::sort(sorted.begin(), sorted.end());
    result.sampleCount = sorted.size();
    result.minimum = sorted.front();
    result.maximum = sorted.back();
    const std::size_t middle = sorted.size() / 2;
    result.median = sorted.size() % 2 == 0
            ? (sorted[middle - 1] + sorted[middle]) * 0.5
            : sorted[middle];
    const std::size_t percentileIndex = std::max<std::size_t>(
            1, static_cast<std::size_t>(std::ceil(sorted.size() * 0.95))) - 1;
    result.percentile95 = sorted[std::min(percentileIndex, sorted.size() - 1)];
    result.valid = true;
    return result;
}

bool SameSectorAtmosphereCaptureView(
        const SectorAtmosphereCaptureMetadata& left,
        const SectorAtmosphereCaptureMetadata& right)
{
    return left.quality == right.quality
            && left.sceneWidth == right.sceneWidth
            && left.sceneHeight == right.sceneHeight
            && SamePose(left.cameraPose, right.cameraPose)
            && Near(left.verticalFovDegrees, right.verticalFovDegrees);
}

const char* SectorAtmosphereCaptureStateName(SectorAtmosphereCaptureState state)
{
    switch (state) {
        case SectorAtmosphereCaptureState::Idle: return "Idle";
        case SectorAtmosphereCaptureState::Warmup: return "Warmup";
        case SectorAtmosphereCaptureState::Capturing: return "Capturing";
        case SectorAtmosphereCaptureState::Draining: return "Draining";
        case SectorAtmosphereCaptureState::Complete: return "Complete";
        case SectorAtmosphereCaptureState::Aborted: return "Aborted";
    }
    return "Unknown";
}

void SectorAtmosphereCapture::Start(
        const SectorAtmosphereCaptureMetadata& newMetadata,
        std::uint64_t newFirstSequence,
        std::uint64_t newSkippedQueryFrames)
{
    state = SectorAtmosphereCaptureState::Warmup;
    metadata = newMetadata;
    firstSequence = newFirstSequence;
    skippedQueryFramesAtStart = newSkippedQueryFrames;
    skippedQueryFrames = newSkippedQueryFrames;
    warmupIssued = 0;
    captureIssued = 0;
    samplesResolved = 0;
    samples = {};
    receivedSamples = {};
    report.clear();
    status = "warming up 0/60";
}

void SectorAtmosphereCapture::Cancel(const char* reason)
{
    if (state == SectorAtmosphereCaptureState::Idle
            || state == SectorAtmosphereCaptureState::Complete) {
        return;
    }
    state = SectorAtmosphereCaptureState::Aborted;
    status = reason == nullptr || reason[0] == '\0' ? "aborted" : reason;
    report = "Atmosphere capture aborted: " + status;
}

void SectorAtmosphereCapture::ValidateView(
        const SectorAtmosphereCaptureMetadata& currentMetadata)
{
    if (state != SectorAtmosphereCaptureState::Warmup
            && state != SectorAtmosphereCaptureState::Capturing
            && state != SectorAtmosphereCaptureState::Draining) {
        return;
    }
    if (!SameSectorAtmosphereCaptureView(metadata, currentMetadata)) {
        Cancel("camera, quality, FOV, or target size changed");
    }
}

void SectorAtmosphereCapture::OnFrameIssued(std::uint64_t sequence)
{
    if (state == SectorAtmosphereCaptureState::Idle
            || state == SectorAtmosphereCaptureState::Complete
            || state == SectorAtmosphereCaptureState::Aborted
            || sequence < firstSequence) {
        return;
    }
    const std::uint64_t offset = sequence - firstSequence;
    if (offset < SectorAtmosphereCaptureWarmupFrames) {
        warmupIssued = std::max(warmupIssued, static_cast<std::size_t>(offset + 1));
        state = SectorAtmosphereCaptureState::Warmup;
        status = "warming up " + std::to_string(warmupIssued) + "/60";
        return;
    }
    const std::uint64_t captureOffset = offset - SectorAtmosphereCaptureWarmupFrames;
    if (captureOffset < SectorAtmosphereCaptureSampleFrames) {
        captureIssued = std::max(captureIssued,
                static_cast<std::size_t>(captureOffset + 1));
        state = captureIssued == SectorAtmosphereCaptureSampleFrames
                ? SectorAtmosphereCaptureState::Draining
                : SectorAtmosphereCaptureState::Capturing;
        status = state == SectorAtmosphereCaptureState::Draining
                ? "draining delayed GPU results"
                : "capturing " + std::to_string(captureIssued) + "/300";
    }
}

void SectorAtmosphereCapture::OnFrameResolved(
        const SectorAtmosphereGpuTimingFrame& frame)
{
    if (!frame.valid || state == SectorAtmosphereCaptureState::Idle
            || state == SectorAtmosphereCaptureState::Complete
            || state == SectorAtmosphereCaptureState::Aborted
            || frame.sequence < firstSequence + SectorAtmosphereCaptureWarmupFrames) {
        return;
    }
    const std::uint64_t sampleIndex = frame.sequence - firstSequence
            - SectorAtmosphereCaptureWarmupFrames;
    if (sampleIndex >= SectorAtmosphereCaptureSampleFrames) return;
    const std::size_t resolvedIndex = static_cast<std::size_t>(sampleIndex);
    if (receivedSamples[resolvedIndex]) return;
    for (std::size_t pass = 0; pass < SectorAtmosphereGpuPassCount; ++pass) {
        samples[pass][static_cast<std::size_t>(sampleIndex)] =
                frame.milliseconds[pass];
    }
    receivedSamples[resolvedIndex] = true;
    ++samplesResolved;
    if (samplesResolved == SectorAtmosphereCaptureSampleFrames) {
        Complete();
    } else if (captureIssued == SectorAtmosphereCaptureSampleFrames) {
        state = SectorAtmosphereCaptureState::Draining;
        status = "draining " + std::to_string(samplesResolved) + "/300";
    }
}

void SectorAtmosphereCapture::SetSkippedQueryFrames(std::uint64_t value)
{
    skippedQueryFrames = value;
}

SectorAtmosphereSampleStatistics SectorAtmosphereCapture::Statistics(
        SectorAtmosphereGpuPass pass) const
{
    const std::size_t index = static_cast<std::size_t>(pass);
    if (index >= SectorAtmosphereGpuPassCount) return {};
    std::array<double, SectorAtmosphereCaptureSampleFrames> compact{};
    std::size_t compactCount = 0;
    for (std::size_t sample = 0; sample < receivedSamples.size(); ++sample) {
        if (receivedSamples[sample]) compact[compactCount++] = samples[index][sample];
    }
    return ComputeSectorAtmosphereSampleStatistics(compact.data(), compactCount);
}

void SectorAtmosphereCapture::Complete()
{
    state = SectorAtmosphereCaptureState::Complete;
    status = "complete";
    std::array<SectorAtmosphereSampleStatistics,
            SectorAtmosphereGpuPassCount> statistics{};
    for (std::size_t pass = 0; pass < statistics.size(); ++pass) {
        statistics[pass] = ComputeSectorAtmosphereSampleStatistics(
                samples[pass].data(), SectorAtmosphereCaptureSampleFrames);
    }
    report = FormatSectorAtmosphereCaptureReport(
            metadata, statistics, SkippedQueryFrames());
}

SectorAtmosphereTimestampQueryRingState::Decision
SectorAtmosphereTimestampQueryRingState::Advance(bool occupiedSlotReady)
{
    Decision result;
    result.slot = CurrentSlot();
    if (occupied[result.slot] && !occupiedSlotReady) {
        ++skippedFrames;
        ++frameIndex;
        return result;
    }
    if (occupied[result.slot]) {
        result.resolvedPrevious = true;
        result.resolvedSequence = sequences[result.slot];
    }
    result.issueCurrent = true;
    result.issueSequence = nextSequence++;
    occupied[result.slot] = true;
    sequences[result.slot] = result.issueSequence;
    ++frameIndex;
    return result;
}

void SectorAtmosphereTimestampQueryRingState::Reset()
{
    occupied = {};
    sequences = {};
    frameIndex = 0;
    nextSequence = 0;
    skippedFrames = 0;
}

std::string FormatSectorAtmosphereBytes(std::uint64_t bytes)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(2);
    if (bytes >= 1024ull * 1024ull) {
        stream << static_cast<double>(bytes) / (1024.0 * 1024.0) << " MiB";
    } else if (bytes >= 1024ull) {
        stream << static_cast<double>(bytes) / 1024.0 << " KiB";
    } else {
        stream.unsetf(std::ios::floatfield);
        stream << bytes << " B";
    }
    return stream.str();
}

std::string FormatSectorAtmosphereDiagnosticsSummary(
        const SectorAtmosphereDiagnostics& diagnostics)
{
    std::ostringstream stream;
    stream << "Legacy " << SectorVolumetricQualityName(diagnostics.effectiveQuality)
           << " | scene " << diagnostics.sceneWidth << 'x' << diagnostics.sceneHeight
           << " | fog " << diagnostics.localFog.eligibleCount << '/'
           << diagnostics.localFog.activeCount
           << " | haze " << diagnostics.haze.eligibleCount << '/'
           << diagnostics.haze.activeCount
           << " | dust " << diagnostics.dustEligible << '/'
           << diagnostics.dustActive
           << " | " << FormatSectorAtmosphereBytes(
                   diagnostics.estimatedIntermediateBytes)
           << " | " << diagnostics.fallbackStatus;
    return stream.str();
}

std::string FormatSectorAtmosphereCaptureReport(
        const SectorAtmosphereCaptureMetadata& metadata,
        const std::array<SectorAtmosphereSampleStatistics,
                SectorAtmosphereGpuPassCount>& statistics,
        std::uint64_t skippedQueryFrames)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3);
    stream << "ATMOSPHERE_CAPTURE legacy "
           << SectorVolumetricQualityName(metadata.quality)
           << " target=" << metadata.sceneWidth << 'x' << metadata.sceneHeight
           << " fog=" << metadata.localFogEligible << '/' << metadata.localFogActive
           << " haze=" << metadata.hazeEligible << '/' << metadata.hazeActive
           << " dust=" << metadata.dustEligible << '/' << metadata.dustActive
           << " camera_pos=" << metadata.cameraPose.position.x << ','
           << metadata.cameraPose.position.y << ',' << metadata.cameraPose.position.z
           << " camera_ypr=" << metadata.cameraPose.yawRadians << ','
           << metadata.cameraPose.pitchRadians << ',' << metadata.cameraPose.rollRadians
           << " fov_y=" << metadata.verticalFovDegrees
           << " skipped_queries=" << skippedQueryFrames << '\n';
    for (std::size_t pass = 0; pass < SectorAtmosphereGpuPassCount; ++pass) {
        const SectorAtmosphereSampleStatistics& value = statistics[pass];
        stream << "  " << SectorAtmosphereGpuPassName(
                static_cast<SectorAtmosphereGpuPass>(pass)) << ": ";
        if (!value.valid) {
            stream << "unavailable\n";
            continue;
        }
        stream << "n=" << value.sampleCount
               << " min=" << value.minimum << "ms"
               << " median=" << value.median << "ms"
               << " p95=" << value.percentile95 << "ms"
               << " max=" << value.maximum << "ms\n";
    }
    return stream.str();
}

} // namespace game
