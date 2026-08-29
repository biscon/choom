#include "engine/render/RenderColorDiagnostics.h"

#include <external/glad.h>
#include <raylib.h>

#include <sstream>

namespace engine {
namespace {

const char* SafeGlString(GLenum name)
{
    const GLubyte* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "unavailable";
}

std::string PlatformName()
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string BuildName()
{
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
}

std::string FramebufferEncodingName(int encoding)
{
    if (encoding == GL_SRGB) return "GL_SRGB";
    if (encoding == GL_LINEAR) return "GL_LINEAR";
    return "unavailable";
}

} // namespace

GraphicsContextDiagnostics CaptureGraphicsContextDiagnostics()
{
    GraphicsContextDiagnostics diagnostics;
    diagnostics.platform = PlatformName();
    diagnostics.build = BuildName();
    diagnostics.raylibVersion = RAYLIB_VERSION;
    diagnostics.glVendor = SafeGlString(GL_VENDOR);
    diagnostics.glRenderer = SafeGlString(GL_RENDERER);
    diagnostics.glVersion = SafeGlString(GL_VERSION);
    diagnostics.glslVersion = SafeGlString(GL_SHADING_LANGUAGE_VERSION);
    diagnostics.logicalWidth = GetScreenWidth();
    diagnostics.logicalHeight = GetScreenHeight();
    diagnostics.renderWidth = GetRenderWidth();
    diagnostics.renderHeight = GetRenderHeight();
    const Vector2 dpi = GetWindowScaleDPI();
    diagnostics.dpiScaleX = dpi.x;
    diagnostics.dpiScaleY = dpi.y;
    diagnostics.framebufferSrgbEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;

    int previousDrawFramebuffer = 0;
    int previousReadFramebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    int samples = 0;
    glGetIntegerv(GL_SAMPLES, &samples);
    diagnostics.sampleCount = samples > 0 ? samples : 1;

    int doubleBuffered = GL_TRUE;
    glGetIntegerv(GL_DOUBLEBUFFER, &doubleBuffered);
    const GLenum attachment = doubleBuffered == GL_TRUE
            ? GL_BACK_LEFT
            : GL_FRONT_LEFT;
    int encoding = 0;
    while (glGetError() != GL_NO_ERROR) {
    }
    glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER,
            attachment,
            GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE,
            &diagnostics.redBits);
    glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER,
            attachment,
            GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE,
            &diagnostics.greenBits);
    glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER,
            attachment,
            GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE,
            &diagnostics.blueBits);
    glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER,
            attachment,
            GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
            &diagnostics.alphaBits);
    glGetFramebufferAttachmentParameteriv(
            GL_FRAMEBUFFER,
            attachment,
            GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
            &encoding);
    const bool attachmentQueriesAvailable = glGetError() == GL_NO_ERROR;
    diagnostics.defaultFramebufferEncoding = attachmentQueriesAvailable
            ? FramebufferEncodingName(encoding)
            : "unavailable";
    if (!attachmentQueriesAvailable) {
        diagnostics.redBits = -1;
        diagnostics.greenBits = -1;
        diagnostics.blueBits = -1;
        diagnostics.alphaBits = -1;
    }
    glBindFramebuffer(
            GL_DRAW_FRAMEBUFFER,
            static_cast<GLuint>(previousDrawFramebuffer));
    glBindFramebuffer(
            GL_READ_FRAMEBUFFER,
            static_cast<GLuint>(previousReadFramebuffer));
    return diagnostics;
}

std::string FormatColorPipelineDiagnostics(
        const GraphicsContextDiagnostics& graphics,
        const ColorPipelineRuntimeState& pipeline)
{
    const ToneMappingSettings toneMapping = NormalizeToneMappingSettings(
            pipeline.toneMapping);
    std::ostringstream output;
    output << "COLOR PIPELINE slice-3 (active=linear-HDR, no legacy mode)\n"
           << "  platform=" << graphics.platform
           << " build=" << graphics.build
           << " raylib=" << graphics.raylibVersion << "\n"
           << "  GL vendor=" << graphics.glVendor
           << " renderer=" << graphics.glRenderer
           << " version=" << graphics.glVersion
           << " GLSL=" << graphics.glslVersion << "\n"
           << "  window=" << graphics.logicalWidth << "x" << graphics.logicalHeight
           << " render=" << graphics.renderWidth << "x" << graphics.renderHeight
           << " dpi=" << graphics.dpiScaleX << "x" << graphics.dpiScaleY << "\n"
           << "  default-fb rgba=" << graphics.redBits << "/"
           << graphics.greenBits << "/" << graphics.blueBits << "/"
           << graphics.alphaBits
           << " encoding=" << graphics.defaultFramebufferEncoding
           << " samples=" << graphics.sampleCount
           << " GL_FRAMEBUFFER_SRGB="
           << (graphics.framebufferSrgbEnabled ? "enabled" : "disabled") << "\n"
           << "  supersample=" << pipeline.supersampleScale
           << " FXAA=" << (pipeline.fxaaActive ? "active" : (pipeline.fxaaRequested ? "requested-unavailable" : "off")) << "\n"
           << "  resolve=2880x1620 linear RGBA16F -> 1920x1080 linear RGBA16F\n"
           << "  stages=bloom/fog/haze/dust bounded-linear RGBA8 accumulators -> linear HDR scene composites\n"
           << "  baked=CPU F32 linear -> disk RGBA16F-LE RGB+AO / probe F32-LE -> GPU linear RGBA16F\n"
           << "  tone/output="
           << ToneMappingOperatorDisplayName(toneMapping.toneMapper)
           << " exposure=" << toneMapping.exposureCompensationEv
           << " EV -> exact sRGB RGBA8 -> FXAA/final scale\n"
           << "  models=linear HDR output; glTF color textures manually decoded; no local tone map or encoding";
    return output.str();
}

void LogColorPipelineDiagnostics(const ColorPipelineRuntimeState& pipeline)
{
    const std::string formatted = FormatColorPipelineDiagnostics(
            CaptureGraphicsContextDiagnostics(), pipeline);
    TraceLog(LOG_INFO, "%s", formatted.c_str());
}

} // namespace engine
