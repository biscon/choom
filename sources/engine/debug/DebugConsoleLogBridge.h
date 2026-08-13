#pragma once

#include <cstdarg>

namespace engine {

struct DebugConsoleData;

void InstallDebugConsoleTraceLogBridge();
void SetDebugConsoleLogCaptureEnabled(bool enabled);
void FlushPendingDebugConsoleLogs(DebugConsoleData& console);
void DiscardPendingDebugConsoleLogs();

} // namespace engine
