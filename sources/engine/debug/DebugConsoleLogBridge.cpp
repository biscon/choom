#include "engine/debug/DebugConsoleLogBridge.h"

#include "engine/debug/DebugConsole.h"

#include <raylib.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace engine {
namespace {

struct PendingConsoleLog {
    uint64_t sequence = 0;
    int level = LOG_INFO;
    std::string text;
};

struct DebugConsoleLogInbox {
    std::mutex mutex;
    std::vector<PendingConsoleLog> pending;
    uint64_t nextSequence = 1;
    uint64_t droppedCount = 0;
    size_t maxPending = 512;

    DebugConsoleLogInbox()
    {
        pending.reserve(maxPending);
    }
};

DebugConsoleLogInbox& Inbox()
{
    static DebugConsoleLogInbox inbox;
    return inbox;
}

std::atomic<bool>& CaptureEnabled()
{
    static std::atomic<bool> enabled{true};
    return enabled;
}

const char* LevelName(int level)
{
    switch (level) {
        case LOG_TRACE: return "TRACE";
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO: return "INFO";
        case LOG_WARNING: return "WARNING";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default: return "LOG";
    }
}

DebugConsoleSeverity Severity(int level)
{
    switch (level) {
        case LOG_TRACE: return DebugConsoleSeverity::Trace;
        case LOG_DEBUG: return DebugConsoleSeverity::Debug;
        case LOG_WARNING: return DebugConsoleSeverity::Warning;
        case LOG_ERROR: return DebugConsoleSeverity::Error;
        case LOG_FATAL: return DebugConsoleSeverity::Fatal;
        case LOG_INFO:
        default: return DebugConsoleSeverity::Info;
    }
}

void TraceCallback(int level, const char* format, va_list args)
{
    char buffer[4096] = {};
    va_list copy;
    va_copy(copy, args);
    const int written = std::vsnprintf(
            buffer, sizeof(buffer), format != nullptr ? format : "", copy);
    va_end(copy);
    const std::string message = written < 0
            ? std::string{"<trace formatting failed>"}
            : std::string{buffer};

    FILE* sink = level >= LOG_WARNING ? stderr : stdout;
    std::fprintf(sink, "[%s] %s\n", LevelName(level), message.c_str());
    std::fflush(sink);

    if (!CaptureEnabled().load(std::memory_order_relaxed)) return;
    DebugConsoleLogInbox& inbox = Inbox();
    std::lock_guard<std::mutex> lock(inbox.mutex);
    if (inbox.pending.size() >= inbox.maxPending) {
        ++inbox.droppedCount;
        return;
    }
    inbox.pending.push_back(PendingConsoleLog{
            inbox.nextSequence++, level, message});
}

} // namespace

void InstallDebugConsoleTraceLogBridge()
{
    SetTraceLogCallback(TraceCallback);
}

void SetDebugConsoleLogCaptureEnabled(bool enabled)
{
    CaptureEnabled().store(enabled, std::memory_order_relaxed);
    if (!enabled) DiscardPendingDebugConsoleLogs();
}

void FlushPendingDebugConsoleLogs(DebugConsoleData& console)
{
    if (!CaptureEnabled().load(std::memory_order_relaxed)) return;
    static std::vector<PendingConsoleLog> pending = [] {
        std::vector<PendingConsoleLog> value;
        value.reserve(512);
        return value;
    }();
    uint64_t dropped = 0;
    DebugConsoleLogInbox& inbox = Inbox();
    {
        std::lock_guard<std::mutex> lock(inbox.mutex);
        pending.clear();
        pending.swap(inbox.pending);
        dropped = inbox.droppedCount;
        inbox.droppedCount = 0;
    }
    if (dropped != 0) {
        DebugConsoleAddLine(
                console,
                "[WARNING] " + std::to_string(dropped)
                        + " console log messages were dropped",
                DebugConsoleSeverity::Warning);
    }
    for (PendingConsoleLog& record : pending) {
        DebugConsoleAddLine(
                console,
                std::string{"["} + LevelName(record.level) + "] "
                        + record.text,
                Severity(record.level));
    }
}

void DiscardPendingDebugConsoleLogs()
{
    DebugConsoleLogInbox& inbox = Inbox();
    std::lock_guard<std::mutex> lock(inbox.mutex);
    inbox.pending.clear();
    inbox.droppedCount = 0;
}

} // namespace engine
