#include "engine/render/RenderTarget.h"

#include <external/glad.h>
#include <rlgl.h>

#include <algorithm>
#include <limits>
#include <sstream>

namespace engine {
namespace {

struct OpenGlBindings {
    int activeTexture = GL_TEXTURE0;
    int texture2D = 0;
    int renderbuffer = 0;
    int drawFramebuffer = 0;
    int readFramebuffer = 0;
};

OpenGlBindings CaptureBindings()
{
    OpenGlBindings bindings;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &bindings.activeTexture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bindings.texture2D);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &bindings.renderbuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bindings.drawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &bindings.readFramebuffer);
    return bindings;
}

void RestoreBindings(const OpenGlBindings& bindings)
{
    glActiveTexture(static_cast<GLenum>(bindings.activeTexture));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bindings.texture2D));
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(bindings.renderbuffer));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(bindings.drawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(bindings.readFramebuffer));
}

void ClearGlErrors()
{
    while (glGetError() != GL_NO_ERROR) {
    }
}

bool QueryTextureParameter(unsigned int texture, GLenum parameter, int& value)
{
    ClearGlErrors();
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, parameter, &value);
    return glGetError() == GL_NO_ERROR;
}

bool QueryTextureSetting(unsigned int texture, GLenum parameter, int& value)
{
    ClearGlErrors();
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexParameteriv(GL_TEXTURE_2D, parameter, &value);
    return glGetError() == GL_NO_ERROR;
}

void SetError(std::string* error, const std::string& value)
{
    if (error != nullptr) {
        *error = value;
    }
}

const char* GlInternalFormatName(unsigned int format)
{
    switch (format) {
        case GL_RGBA8: return "GL_RGBA8";
        case GL_RGBA16F: return "GL_RGBA16F";
        default: return "unavailable/other";
    }
}

const char* GlFilterName(int filter)
{
    switch (filter) {
        case GL_NEAREST: return "nearest";
        case GL_LINEAR: return "linear";
        case GL_NEAREST_MIPMAP_NEAREST: return "nearest-mipmap-nearest";
        case GL_LINEAR_MIPMAP_NEAREST: return "linear-mipmap-nearest";
        case GL_NEAREST_MIPMAP_LINEAR: return "nearest-mipmap-linear";
        case GL_LINEAR_MIPMAP_LINEAR: return "linear-mipmap-linear";
        default: return "unavailable";
    }
}

const char* GlWrapName(int wrap)
{
    switch (wrap) {
        case GL_REPEAT: return "repeat";
        case GL_CLAMP_TO_EDGE: return "clamp";
        case GL_MIRRORED_REPEAT: return "mirror";
        default: return "unavailable";
    }
}

} // namespace

bool ValidateRenderTargetDescriptor(
        const RenderTargetDescriptor& descriptor,
        std::string* error)
{
    if (descriptor.debugName.empty()) {
        SetError(error, "Render target debug name must not be empty");
        return false;
    }
    if (descriptor.width <= 0 || descriptor.height <= 0) {
        SetError(error, "Render target dimensions must be positive");
        return false;
    }
    if (descriptor.sampleCount != 1) {
        SetError(error, "Only single-sample render targets are supported");
        return false;
    }
    if (descriptor.colorFormat != RenderTargetColorFormat::Rgba8Unorm
            && descriptor.colorFormat != RenderTargetColorFormat::Rgba16Float) {
        SetError(error, "Unsupported render target color format");
        return false;
    }
    if (descriptor.filter != RenderTargetFilter::Point
            && descriptor.filter != RenderTargetFilter::Bilinear) {
        SetError(error, "Unsupported render target filter");
        return false;
    }
    if (descriptor.wrap != RenderTargetWrap::Repeat
            && descriptor.wrap != RenderTargetWrap::Clamp) {
        SetError(error, "Unsupported render target wrap mode");
        return false;
    }
    if (descriptor.depth != RenderTargetDepthKind::None
            && descriptor.depth != RenderTargetDepthKind::Renderbuffer
            && descriptor.depth != RenderTargetDepthKind::SampleableTexture) {
        SetError(error, "Unsupported render target depth kind");
        return false;
    }
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

int RaylibPixelFormatForRenderTarget(RenderTargetColorFormat format)
{
    switch (format) {
        case RenderTargetColorFormat::Rgba8Unorm:
            return PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        case RenderTargetColorFormat::Rgba16Float:
            return PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    }
    return 0;
}

int RenderTargetColorBytesPerPixel(RenderTargetColorFormat format)
{
    switch (format) {
        case RenderTargetColorFormat::Rgba8Unorm: return 4;
        case RenderTargetColorFormat::Rgba16Float: return 8;
    }
    return 0;
}

std::uint64_t EstimateRenderTargetAllocationBytes(
        const RenderTargetDescriptor& descriptor,
        int actualDepthBits,
        int actualSampleCount)
{
    const std::uint64_t pixels = static_cast<std::uint64_t>(descriptor.width)
            * static_cast<std::uint64_t>(descriptor.height);
    const std::uint64_t colorBytes = static_cast<std::uint64_t>(
            std::max(0, RenderTargetColorBytesPerPixel(descriptor.colorFormat)));
    const std::uint64_t depthBytes = actualDepthBits > 0
            ? static_cast<std::uint64_t>((actualDepthBits + 7) / 8)
            : 0;
    const std::uint64_t samples = static_cast<std::uint64_t>(
            std::max(actualSampleCount, 1));
    const std::uint64_t bytesPerPixel = colorBytes + depthBytes;
    if (bytesPerPixel == 0 || pixels > std::numeric_limits<std::uint64_t>::max() / bytesPerPixel
            || pixels * bytesPerPixel > std::numeric_limits<std::uint64_t>::max() / samples) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return pixels * bytesPerPixel * samples;
}

const char* RenderTargetColorFormatName(RenderTargetColorFormat format)
{
    switch (format) {
        case RenderTargetColorFormat::Rgba8Unorm: return "RGBA8_UNORM";
        case RenderTargetColorFormat::Rgba16Float: return "RGBA16_FLOAT";
    }
    return "invalid";
}

const char* RenderTargetDepthKindName(RenderTargetDepthKind kind)
{
    switch (kind) {
        case RenderTargetDepthKind::None: return "none";
        case RenderTargetDepthKind::Renderbuffer: return "renderbuffer";
        case RenderTargetDepthKind::SampleableTexture: return "sampleable-texture";
    }
    return "invalid";
}

std::string FormatRenderTargetDiagnostic(const RenderTarget& target)
{
    std::ostringstream output;
    output << target.descriptor.debugName
           << " " << target.descriptor.width << "x" << target.descriptor.height
           << " requested=" << RenderTargetColorFormatName(target.descriptor.colorFormat)
           << " actual=" << GlInternalFormatName(target.actual.internalFormat)
           << " filter=" << GlFilterName(target.actual.minimumFilter)
           << "/" << GlFilterName(target.actual.magnificationFilter)
           << " wrap=" << GlWrapName(target.actual.wrapS)
           << "/" << GlWrapName(target.actual.wrapT)
           << " samples=" << target.actual.sampleCount
           << " depth=" << RenderTargetDepthKindName(target.actual.depth)
           << "(" << target.actual.depthBits << "-bit)"
           << " color-bpp=" << RenderTargetColorBytesPerPixel(target.descriptor.colorFormat)
           << " estimated-bytes=" << target.actual.estimatedAllocationBytes;
    return output.str();
}

bool LoadRenderTarget(
        const RenderTargetDescriptor& descriptor,
        RenderTarget& target,
        std::string* error)
{
    UnloadRenderTarget(target);
    if (!ValidateRenderTargetDescriptor(descriptor, error)) {
        return false;
    }

    const OpenGlBindings bindings = CaptureBindings();
    RenderTexture2D native{};
    native.id = rlLoadFramebuffer();
    if (native.id == 0) {
        RestoreBindings(bindings);
        SetError(error, "Could not create framebuffer for " + descriptor.debugName);
        TraceLog(LOG_ERROR, "RENDER TARGET: %s", error != nullptr ? error->c_str() : "framebuffer creation failed");
        return false;
    }

    const int pixelFormat = RaylibPixelFormatForRenderTarget(descriptor.colorFormat);
    native.texture.id = rlLoadTexture(
            nullptr, descriptor.width, descriptor.height, pixelFormat, 1);
    native.texture.width = descriptor.width;
    native.texture.height = descriptor.height;
    native.texture.format = pixelFormat;
    native.texture.mipmaps = 1;

    if (descriptor.depth != RenderTargetDepthKind::None) {
        const bool useRenderbuffer = descriptor.depth == RenderTargetDepthKind::Renderbuffer;
        native.depth.id = rlLoadTextureDepth(
                descriptor.width, descriptor.height, useRenderbuffer);
        native.depth.width = descriptor.width;
        native.depth.height = descriptor.height;
        native.depth.format = 19;
        native.depth.mipmaps = 1;
    }

    if (native.texture.id != 0) {
        rlFramebufferAttach(
                native.id,
                native.texture.id,
                RL_ATTACHMENT_COLOR_CHANNEL0,
                RL_ATTACHMENT_TEXTURE2D,
                0);
    }
    if (native.depth.id != 0) {
        rlFramebufferAttach(
                native.id,
                native.depth.id,
                RL_ATTACHMENT_DEPTH,
                descriptor.depth == RenderTargetDepthKind::Renderbuffer
                        ? RL_ATTACHMENT_RENDERBUFFER
                        : RL_ATTACHMENT_TEXTURE2D,
                0);
    }

    const bool complete = native.texture.id != 0
            && (descriptor.depth == RenderTargetDepthKind::None || native.depth.id != 0)
            && rlFramebufferComplete(native.id);
    if (!complete) {
        UnloadRenderTexture(native);
        RestoreBindings(bindings);
        SetError(error, "Framebuffer is incomplete for " + descriptor.debugName);
        TraceLog(LOG_ERROR, "RENDER TARGET: framebuffer is incomplete for %s", descriptor.debugName.c_str());
        return false;
    }

    SetTextureFilter(
            native.texture,
            descriptor.filter == RenderTargetFilter::Bilinear
                    ? TEXTURE_FILTER_BILINEAR
                    : TEXTURE_FILTER_POINT);
    SetTextureWrap(
            native.texture,
            descriptor.wrap == RenderTargetWrap::Clamp
                    ? TEXTURE_WRAP_CLAMP
                    : TEXTURE_WRAP_REPEAT);

    RenderTargetActualInfo actual;
    int internalFormat = 0;
    const bool internalFormatAvailable = QueryTextureParameter(
            native.texture.id, GL_TEXTURE_INTERNAL_FORMAT, internalFormat);
    unsigned int expectedInternalFormat = 0;
    unsigned int unusedFormat = 0;
    unsigned int unusedType = 0;
    rlGetGlTextureFormats(
            pixelFormat, &expectedInternalFormat, &unusedFormat, &unusedType);
    if (!internalFormatAvailable
            || expectedInternalFormat == 0
            || static_cast<unsigned int>(internalFormat) != expectedInternalFormat) {
        UnloadRenderTexture(native);
        RestoreBindings(bindings);
        SetError(error, "Render target internal format validation failed for " + descriptor.debugName);
        TraceLog(LOG_ERROR, "RENDER TARGET: internal format validation failed for %s", descriptor.debugName.c_str());
        return false;
    }
    actual.internalFormat = static_cast<unsigned int>(internalFormat);
    QueryTextureSetting(native.texture.id, GL_TEXTURE_MIN_FILTER, actual.minimumFilter);
    QueryTextureSetting(native.texture.id, GL_TEXTURE_MAG_FILTER, actual.magnificationFilter);
    QueryTextureSetting(native.texture.id, GL_TEXTURE_WRAP_S, actual.wrapS);
    QueryTextureSetting(native.texture.id, GL_TEXTURE_WRAP_T, actual.wrapT);

    glBindFramebuffer(GL_FRAMEBUFFER, native.id);
    int samples = 0;
    int depthBits = 0;
    int depthObjectType = GL_NONE;
    glGetIntegerv(GL_SAMPLES, &samples);
    if (descriptor.depth != RenderTargetDepthKind::None) {
        ClearGlErrors();
        glGetFramebufferAttachmentParameteriv(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE,
                &depthBits);
        glGetFramebufferAttachmentParameteriv(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,
                &depthObjectType);
        if (glGetError() != GL_NO_ERROR) {
            depthBits = 0;
            depthObjectType = GL_NONE;
        }
    }
    actual.sampleCount = std::max(samples, 1);
    actual.depthBits = std::max(depthBits, 0);
    if (depthObjectType == GL_TEXTURE) {
        actual.depth = RenderTargetDepthKind::SampleableTexture;
    } else if (depthObjectType == GL_RENDERBUFFER) {
        actual.depth = RenderTargetDepthKind::Renderbuffer;
    } else {
        actual.depth = RenderTargetDepthKind::None;
    }
    actual.estimatedAllocationBytes = EstimateRenderTargetAllocationBytes(
            descriptor, actual.depthBits, actual.sampleCount);

    target.native = native;
    target.descriptor = descriptor;
    target.actual = actual;
    RestoreBindings(bindings);
    if (error != nullptr) {
        error->clear();
    }
    const std::string diagnostic = FormatRenderTargetDiagnostic(target);
    TraceLog(LOG_INFO, "COLOR PIPELINE target: %s", diagnostic.c_str());
    return true;
}

void UnloadRenderTarget(RenderTarget& target)
{
    if (target.native.id != 0) {
        UnloadRenderTexture(target.native);
    }
    target = RenderTarget{};
}

bool IsRenderTargetReady(const RenderTarget& target)
{
    return target.native.id != 0 && target.native.texture.id != 0;
}

RenderTexture2D& NativeRenderTexture(RenderTarget& target)
{
    return target.native;
}

const RenderTexture2D& NativeRenderTexture(const RenderTarget& target)
{
    return target.native;
}

} // namespace engine
