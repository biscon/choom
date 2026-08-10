#pragma once

#include <raylib.h>

#include <cstdint>
#include <string>

namespace engine {

enum class RenderTargetColorFormat {
    Rgba8Unorm,
    Rgba16Float
};

enum class RenderTargetFilter {
    Point,
    Bilinear
};

enum class RenderTargetWrap {
    Repeat,
    Clamp
};

enum class RenderTargetDepthKind {
    None,
    Renderbuffer,
    SampleableTexture
};

struct RenderTargetDescriptor {
    std::string debugName;
    int width = 0;
    int height = 0;
    RenderTargetColorFormat colorFormat = RenderTargetColorFormat::Rgba8Unorm;
    RenderTargetFilter filter = RenderTargetFilter::Point;
    RenderTargetWrap wrap = RenderTargetWrap::Repeat;
    RenderTargetDepthKind depth = RenderTargetDepthKind::Renderbuffer;
    int sampleCount = 1;
};

struct RenderTargetActualInfo {
    unsigned int internalFormat = 0;
    int minimumFilter = 0;
    int magnificationFilter = 0;
    int wrapS = 0;
    int wrapT = 0;
    int sampleCount = 1;
    int depthBits = 0;
    RenderTargetDepthKind depth = RenderTargetDepthKind::None;
    std::uint64_t estimatedAllocationBytes = 0;
};

struct RenderTarget {
    RenderTexture2D native = {};
    RenderTargetDescriptor descriptor;
    RenderTargetActualInfo actual;
};

bool ValidateRenderTargetDescriptor(
        const RenderTargetDescriptor& descriptor,
        std::string* error = nullptr);
int RaylibPixelFormatForRenderTarget(RenderTargetColorFormat format);
int RenderTargetColorBytesPerPixel(RenderTargetColorFormat format);
std::uint64_t EstimateRenderTargetAllocationBytes(
        const RenderTargetDescriptor& descriptor,
        int actualDepthBits,
        int actualSampleCount);
const char* RenderTargetColorFormatName(RenderTargetColorFormat format);
const char* RenderTargetDepthKindName(RenderTargetDepthKind kind);
std::string FormatRenderTargetDiagnostic(const RenderTarget& target);

bool LoadRenderTarget(
        const RenderTargetDescriptor& descriptor,
        RenderTarget& target,
        std::string* error = nullptr);
void UnloadRenderTarget(RenderTarget& target);
bool IsRenderTargetReady(const RenderTarget& target);
RenderTexture2D& NativeRenderTexture(RenderTarget& target);
const RenderTexture2D& NativeRenderTexture(const RenderTarget& target);

} // namespace engine
