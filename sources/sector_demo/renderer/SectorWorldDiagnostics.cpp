#include <external/glad.h>
#include <raylib.h>
#include <rlgl.h>
#include "sector_demo/renderer/SectorWorldDiagnostics.h"

namespace game
{
void SectorWorldProfiler::Initialize(std::size_t maxIntervals)
{
    Shutdown();
    for (auto &entry : slots) {
        entry.queries.resize(maxIntervals * 2);
        entry.stages.resize(maxIntervals);
        glGenQueries(static_cast<GLsizei>(entry.queries.size()), entry.queries.data());
    }
}
void SectorWorldProfiler::Shutdown()
{
    for (auto &entry : slots) {
        if (!entry.queries.empty())
            glDeleteQueries(static_cast<GLsizei>(entry.queries.size()), entry.queries.data());
        entry = {};
    }
    diagnostics = {};
    frame = slot = 0;
    enabled = available = recording = warned = false;
    stage = SectorWorldStage::Count;
}
void SectorWorldProfiler::BeginFrame(bool collect)
{
    diagnostics.cpuMs.fill(0);
    currentMask = 0;
    slot = frame++ % slots.size();
    auto &entry = slots[slot];
    if (entry.issued) {
        GLint ready = GL_FALSE;
        glGetQueryObjectiv(entry.queries[entry.issued * 2 - 1], GL_QUERY_RESULT_AVAILABLE, &ready);
        if (ready) {
            std::array<double, SectorWorldStageCount> samples{};
            for (std::size_t i = 0; i < entry.issued; ++i) {
                GLuint64 begin = 0, end = 0;
                glGetQueryObjectui64v(entry.queries[i * 2], GL_QUERY_RESULT, &begin);
                glGetQueryObjectui64v(entry.queries[i * 2 + 1], GL_QUERY_RESULT, &end);
                samples[static_cast<std::size_t>(entry.stages[i])] += (end - begin) / 1000000.0;
            }
            diagnostics.gpuMs = samples;
            entry.issued = 0;
        }
    }
    enabled = collect;
    available = collect && entry.issued == 0 && !entry.queries.empty();
}
void SectorWorldProfiler::Begin(SectorWorldStage next)
{
    stage = next;
    cpuStart = GetTime();
    currentMask |= 1u << static_cast<std::size_t>(stage);
    auto &entry = slots[slot];
    recording = available && entry.issued < entry.stages.size();
    if (available && !recording && !warned) {
        TraceLog(LOG_WARNING, "RENDER: world timing capacity exceeded; skipping GPU intervals");
        warned = true;
    }
    if (recording) {
        rlDrawRenderBatchActive();
        glQueryCounter(entry.queries[entry.issued * 2], GL_TIMESTAMP);
    }
}
void SectorWorldProfiler::End()
{
    if (stage == SectorWorldStage::Count)
        return;
    diagnostics.cpuMs[static_cast<std::size_t>(stage)] += (GetTime() - cpuStart) * 1000;
    if (recording) {
        auto &entry = slots[slot];
        rlDrawRenderBatchActive();
        glQueryCounter(entry.queries[entry.issued * 2 + 1], GL_TIMESTAMP);
        entry.stages[entry.issued++] = stage;
    }
    stage = SectorWorldStage::Count;
    recording = false;
}
void SectorWorldProfiler::FinishFrame()
{
    for (std::size_t i = 0; i < SectorWorldStageCount; ++i) {
        if (!enabled || (currentMask & (1u << i)) == 0)
            diagnostics.gpuMs[i] = 0;
    }
}
} // namespace game
