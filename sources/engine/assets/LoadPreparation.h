#pragma once

#include <raylib.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>

namespace engine {

using LoadClock = std::chrono::steady_clock;

struct LoadPreparationBudget {
    LoadClock::time_point deadline{};
    bool enabled = false;
    void Begin(float milliseconds = 12.0f) {
        enabled = true;
        deadline = LoadClock::now() + std::chrono::duration_cast<LoadClock::duration>(
                std::chrono::duration<float, std::milli>(milliseconds));
    }
    float RemainingMilliseconds() const {
        return enabled ? std::max(0.0f, std::chrono::duration<float, std::milli>(
                deadline - LoadClock::now()).count()) : 0.0f;
    }
};

enum class LoadPhase : size_t {
    Document, Fingerprints, Renderer, Geometry, Lightmaps, Collision, RuntimeObjects, ModelImport,
    ModelFinalize, ImageDecode, ImageConvert, TextureUpload, OtherAssets,
    Dialogue, Navigation, Activation, Reflections, Count
};

struct LoadDiagnostics {
    bool active = false;
    std::string label;
    LoadClock::time_point started{};
    std::array<double, static_cast<size_t>(LoadPhase::Count)> milliseconds{};
    std::array<uint64_t, 4> waitingFrames{};
    uint64_t frames = 0, modelHits = 0, modelMisses = 0, textureHits = 0, textureMisses = 0;
    uint64_t budgetOverruns = 0, retainedBytes = 0, peakRetainedBytes = 0;
    struct SlowModel { std::string path; double milliseconds = 0; };
    std::array<SlowModel, 5> slowModels{};

    void Begin(const std::string& name) {
        *this = {};
        label = name;
        active = true;
        started = LoadClock::now();
    }
    void ModelLoaded(const std::string& path, double elapsed) {
        if (!active) return;
        for (size_t i = 0; i < slowModels.size(); ++i) {
            if (elapsed <= slowModels[i].milliseconds) continue;
            for (size_t j = slowModels.size() - 1; j > i; --j)
                slowModels[j] = std::move(slowModels[j - 1]);
            slowModels[i] = {path, elapsed};
            break;
        }
    }
    void Finish(const char* outcome = "ready") {
        if (!active) return;
        active = false;
        const double total = std::chrono::duration<double, std::milli>(LoadClock::now() - started).count();
#ifdef NDEBUG
        const char* build = "optimized/NDEBUG";
#else
        const char* build = "debug";
#endif
        TraceLog(LOG_INFO, "LOAD: %s %s (%s): %.1f ms, %llu frames; models hit/miss %llu/%llu, textures %llu/%llu; unused cache %.1f MiB (peak %.1f), overruns %llu",
                label.c_str(), outcome, build, total, (unsigned long long)frames,
                (unsigned long long)modelHits, (unsigned long long)modelMisses,
                (unsigned long long)textureHits, (unsigned long long)textureMisses,
                retainedBytes / 1048576.0, peakRetainedBytes / 1048576.0,
                (unsigned long long)budgetOverruns);
        constexpr const char* names[] = {"document", "fingerprints", "renderer", "geometry", "lightmaps", "collision", "objects", "model import", "model finalize", "image decode", "image conversion", "texture upload", "other assets", "dialogue", "navigation", "activation", "reflections"};
        for (size_t i = 0; i < milliseconds.size(); ++i)
            if (milliseconds[i] > 0) TraceLog(LOG_INFO, "LOAD:   %s %.1f ms (nested CPU timings, not additive)", names[i], milliseconds[i]);
        TraceLog(LOG_INFO, "LOAD: waiting frames assets/objects/navigation/reflections %llu/%llu/%llu/%llu (overlapping)",
                (unsigned long long)waitingFrames[0], (unsigned long long)waitingFrames[1],
                (unsigned long long)waitingFrames[2], (unsigned long long)waitingFrames[3]);
        for (const auto& model : slowModels)
            if (model.milliseconds > 0) TraceLog(LOG_INFO, "LOAD:   model %.1f ms %s", model.milliseconds, model.path.c_str());
    }
};

class ScopedLoadPhase {
public:
    ScopedLoadPhase(LoadDiagnostics& diagnostics, LoadPhase phase)
        : diagnostics(diagnostics), phase(phase), started(diagnostics.active ? LoadClock::now() : LoadClock::time_point{}) {}
    ~ScopedLoadPhase() {
        if (diagnostics.active) diagnostics.milliseconds[static_cast<size_t>(phase)] +=
                std::chrono::duration<double, std::milli>(LoadClock::now() - started).count();
    }
private:
    LoadDiagnostics& diagnostics;
    LoadPhase phase;
    LoadClock::time_point started;
};

template<class Work>
decltype(auto) MeasureLoadPhase(LoadDiagnostics& diagnostics, LoadPhase phase, Work&& work)
{
    ScopedLoadPhase timer(diagnostics, phase);
    return work();
}

} // namespace engine
