#include "sector_demo/renderer/SectorAtmosphereGpuProfiler.h"

#include <external/glad.h>
#include <rlgl.h>

#include <algorithm>

namespace game {

bool SectorAtmosphereGpuProfiler::Initialize()
{
    Shutdown();
    if (glQueryCounter == nullptr || glGetQueryObjectiv == nullptr
            || glGetQueryObjectui64v == nullptr) {
        status = "disabled: OpenGL timestamp queries unavailable";
        return false;
    }
    GLint timestampBits = 0;
    glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &timestampBits);
    if (timestampBits <= 0) {
        status = "disabled: OpenGL timestamp counter has zero bits";
        return false;
    }
    glGenQueries(static_cast<GLsizei>(queries.size()), queries.data());
    initialized = std::all_of(queries.begin(), queries.end(),
            [](unsigned int query) { return query != 0; });
    if (!initialized) {
        if (std::any_of(queries.begin(), queries.end(),
                    [](unsigned int query) { return query != 0; })) {
            glDeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
        }
        queries = {};
        status = "disabled: failed to allocate timestamp queries";
        return false;
    }
    status = "available: four-frame delayed OpenGL timestamps";
    return true;
}

void SectorAtmosphereGpuProfiler::Shutdown()
{
    if (initialized) {
        glDeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
    }
    queries = {};
    ring.Reset();
    begun = {};
    ended = {};
    currentSlot = 0;
    currentSequence = 0;
    initialized = false;
    frameActive = false;
    status = "not initialized";
}

bool SectorAtmosphereGpuProfiler::BeginFrame(
        SectorAtmosphereGpuTimingFrame& outResolvedFrame)
{
    outResolvedFrame = {};
    frameActive = false;
    begun = {};
    ended = {};
    if (!initialized) return false;

    const std::size_t slot = ring.CurrentSlot();
    const bool occupied = ring.CurrentSlotOccupied();
    const bool ready = !occupied || SlotReady(slot);
    const SectorAtmosphereTimestampQueryRingState::Decision decision =
            ring.Advance(ready);
    if (decision.resolvedPrevious) {
        outResolvedFrame = ReadSlot(
                decision.slot, decision.resolvedSequence);
    }
    if (!decision.issueCurrent) return false;
    currentSlot = decision.slot;
    currentSequence = decision.issueSequence;
    frameActive = true;
    return true;
}

void SectorAtmosphereGpuProfiler::Begin(SectorAtmosphereGpuPass pass)
{
    const std::size_t index = static_cast<std::size_t>(pass);
    if (!frameActive || index >= SectorAtmosphereGpuPassCount || begun[index]) return;
    rlDrawRenderBatchActive();
    glQueryCounter(Query(currentSlot, index, 0), GL_TIMESTAMP);
    begun[index] = true;
}

void SectorAtmosphereGpuProfiler::End(SectorAtmosphereGpuPass pass)
{
    const std::size_t index = static_cast<std::size_t>(pass);
    if (!frameActive || index >= SectorAtmosphereGpuPassCount
            || !begun[index] || ended[index]) {
        return;
    }
    rlDrawRenderBatchActive();
    glQueryCounter(Query(currentSlot, index, 1), GL_TIMESTAMP);
    ended[index] = true;
}

void SectorAtmosphereGpuProfiler::EndFrame()
{
    if (!frameActive) return;
    const bool complete = std::all_of(begun.begin(), begun.end(),
            [](bool value) { return value; })
            && std::all_of(ended.begin(), ended.end(),
                    [](bool value) { return value; });
    if (!complete) {
        status = "disabled: incomplete atmosphere timestamp frame";
        Shutdown();
        status = "disabled: incomplete atmosphere timestamp frame";
        return;
    }
    frameActive = false;
}

unsigned int SectorAtmosphereGpuProfiler::Query(
        std::size_t slot,
        std::size_t pass,
        std::size_t endpoint) const
{
    return queries[(slot * SectorAtmosphereGpuPassCount + pass)
            * QueriesPerPass + endpoint];
}

bool SectorAtmosphereGpuProfiler::SlotReady(std::size_t slot) const
{
    for (std::size_t pass = 0; pass < SectorAtmosphereGpuPassCount; ++pass) {
        for (std::size_t endpoint = 0; endpoint < QueriesPerPass; ++endpoint) {
            GLint available = GL_FALSE;
            glGetQueryObjectiv(Query(slot, pass, endpoint),
                    GL_QUERY_RESULT_AVAILABLE, &available);
            if (available != GL_TRUE) return false;
        }
    }
    return true;
}

SectorAtmosphereGpuTimingFrame SectorAtmosphereGpuProfiler::ReadSlot(
        std::size_t slot,
        std::uint64_t sequence) const
{
    SectorAtmosphereGpuTimingFrame result;
    result.sequence = sequence;
    result.valid = true;
    for (std::size_t pass = 0; pass < SectorAtmosphereGpuPassCount; ++pass) {
        GLuint64 start = 0;
        GLuint64 end = 0;
        glGetQueryObjectui64v(Query(slot, pass, 0), GL_QUERY_RESULT, &start);
        glGetQueryObjectui64v(Query(slot, pass, 1), GL_QUERY_RESULT, &end);
        result.milliseconds[pass] = end >= start
                ? static_cast<double>(end - start) / 1000000.0
                : 0.0;
    }
    return result;
}

} // namespace game
