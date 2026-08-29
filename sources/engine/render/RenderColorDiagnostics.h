#pragma once

#include "engine/render/ToneMapping.h"

#include <string>

namespace engine {

struct GraphicsContextDiagnostics {
    std::string platform;
    std::string build;
    std::string raylibVersion;
    std::string glVendor;
    std::string glRenderer;
    std::string glVersion;
    std::string glslVersion;
    int logicalWidth = 0;
    int logicalHeight = 0;
    int renderWidth = 0;
    int renderHeight = 0;
    float dpiScaleX = 1.0f;
    float dpiScaleY = 1.0f;
    int redBits = -1;
    int greenBits = -1;
    int blueBits = -1;
    int alphaBits = -1;
    int sampleCount = -1;
    std::string defaultFramebufferEncoding;
    bool framebufferSrgbEnabled = false;
};

struct ColorPipelineRuntimeState {
    float supersampleScale = 1.0f;
    bool fxaaRequested = false;
    bool fxaaActive = false;
    ToneMappingSettings toneMapping;
};

GraphicsContextDiagnostics CaptureGraphicsContextDiagnostics();
std::string FormatColorPipelineDiagnostics(
        const GraphicsContextDiagnostics& graphics,
        const ColorPipelineRuntimeState& pipeline);
void LogColorPipelineDiagnostics(const ColorPipelineRuntimeState& pipeline);

} // namespace engine
