#include "engine/assets/SpriteAnimationAssets.h"
#include "engine/assets/TextureAssets.h"
#include "engine/assets/TextureColorUsage.h"
#include "engine/render/ColorTransfer.h"
#include "engine/render/RenderColorDiagnostics.h"
#include "engine/render/RenderTarget.h"
#include "engine/render/ScenePresentationShader.h"
#include "engine/render/ToneMapping.h"

#include <external/glad.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace {

int failures = 0;

void Check(bool condition, const char* description)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        ++failures;
    }
}

bool Near(float actual, float expected, float tolerance = 0.000001f)
{
    return std::fabs(actual - expected) <= tolerance;
}

void TestTransferFunctions()
{
    Check(Near(engine::SrgbNormalizedChannelToLinear(0.0f), 0.0f), "sRGB zero decodes to zero");
    Check(Near(engine::SrgbNormalizedChannelToLinear(1.0f), 1.0f), "sRGB one decodes to one");
    Check(Near(engine::LinearNormalizedChannelToSrgb(0.0f), 0.0f), "linear zero encodes to zero");
    Check(Near(engine::LinearNormalizedChannelToSrgb(1.0f), 1.0f), "linear one encodes to one");
    Check(Near(engine::SrgbNormalizedChannelToLinear(0.5f), 0.214041f), "sRGB 0.5 known value");
    Check(Near(engine::LinearNormalizedChannelToSrgb(0.18f), 0.461356f), "linear 0.18 known value");
    Check(Near(engine::SrgbNormalizedChannelToLinear(0.04045f), 0.04045f / 12.92f), "sRGB piecewise boundary");
    Check(Near(engine::LinearNormalizedChannelToSrgb(0.0031308f), 0.0031308f * 12.92f), "linear piecewise boundary");

    const Color bytes{64, 128, 192, 77};
    const Vector4 decoded = engine::SrgbColorBytesToLinearSceneRgba(bytes);
    Check(Near(decoded.x, engine::SrgbNormalizedChannelToLinear(64.0f / 255.0f)), "representative red byte decode");
    Check(Near(decoded.y, engine::SrgbNormalizedChannelToLinear(128.0f / 255.0f)), "representative green byte decode");
    Check(Near(decoded.z, engine::SrgbNormalizedChannelToLinear(192.0f / 255.0f)), "representative blue byte decode");
    Check(Near(decoded.w, 77.0f / 255.0f), "byte alpha is normalized without gamma transform");

    const Vector4 encoded = engine::LinearSceneRgbaToDisplaySrgb(
            Vector4{decoded.x, decoded.y, decoded.z, 0.37f});
    Check(Near(encoded.x, 64.0f / 255.0f), "representative red round trip");
    Check(Near(encoded.y, 128.0f / 255.0f), "representative green round trip");
    Check(Near(encoded.z, 192.0f / 255.0f), "representative blue round trip");
    Check(Near(encoded.w, 0.37f), "linear alpha is preserved without gamma transform");

    const Vector3 normalizedDecoded = engine::SrgbNormalizedRgbToLinearScene(
            Vector3{0.25f, 0.5f, 0.75f});
    Check(Near(normalizedDecoded.y, engine::SrgbNormalizedChannelToLinear(0.5f)),
          "normalized authored tint decodes to linear scene RGB");
    const Color decodedUnorm = engine::SrgbColorBytesToLinearSceneUnorm(bytes);
    Check(decodedUnorm.a == bytes.a, "linear vertex/unorm conversion preserves alpha byte");
    Check(decodedUnorm.r == static_cast<unsigned char>(std::lround(decoded.x * 255.0f)),
          "linear vertex/unorm conversion quantizes decoded RGB");

    constexpr float Values[] = {0.0f, 0.003f, 0.04f, 0.18f, 0.5f, 0.75f, 1.0f};
    for (float value : Values) {
        const float roundTrip = engine::LinearNormalizedChannelToSrgb(
                engine::SrgbNormalizedChannelToLinear(value));
        Check(Near(roundTrip, value, 0.00001f), "sRGB/linear channel round trip");
    }
    Check(Near(engine::SrgbNormalizedChannelToLinear(-1.0f), 0.0f), "sRGB input clamps low");
    Check(Near(engine::LinearNormalizedChannelToSrgb(2.0f), 1.0f), "linear input clamps high");
    Check(Near(engine::SrgbNormalizedChannelToLinear(
                       std::numeric_limits<float>::quiet_NaN()),
               0.0f),
          "non-finite transfer input is safe");
}

void TestTextureSemantics()
{
    using engine::TextureColorUsage;
    Check(engine::IsValidTextureRequestDescriptor(
                  TextureColorUsage::SceneSrgb,
                  engine::TextureLoad_BilinearFilter),
          "scene-sRGB texture descriptor is valid");
    Check(engine::IsValidTextureRequestDescriptor(
                  TextureColorUsage::LinearData,
                  engine::TextureLoad_PointFilter),
          "linear-data texture descriptor is valid");
    Check(engine::IsValidTextureRequestDescriptor(
                  TextureColorUsage::DisplaySrgb,
                  engine::TextureLoad_None),
          "display-sRGB texture descriptor is valid");
    Check(!engine::IsValidTextureRequestDescriptor(
                  TextureColorUsage::Count,
                  engine::TextureLoad_PointFilter),
          "invalid semantic sentinel is rejected");
    Check(!engine::IsValidTextureRequestDescriptor(
                  TextureColorUsage::SceneSrgb,
                  engine::TextureLoad_PointFilter | engine::TextureLoad_BilinearFilter),
          "conflicting texture filters are rejected");

    const engine::AssetScopeHandle scope{0, 1};
    engine::TextureAssets textures;
    textures.OnScopeCreated(scope);
    const auto scene = textures.RequestTexture(
            scope,
            "shared",
            "/tmp/color_semantic.png",
            TextureColorUsage::SceneSrgb,
            engine::TextureLoad_BilinearFilter);
    const auto sceneAgain = textures.RequestTexture(
            scope,
            "shared",
            "/tmp/color_semantic.png",
            TextureColorUsage::SceneSrgb,
            engine::TextureLoad_BilinearFilter);
    const auto data = textures.RequestTexture(
            scope,
            "shared",
            "/tmp/color_semantic.png",
            TextureColorUsage::LinearData,
            engine::TextureLoad_BilinearFilter);
    Check(scene.handle == sceneAgain.handle, "identical semantic request deduplicates");
    Check(scene.shouldQueue && !sceneAgain.shouldQueue, "only first identical request queues");
    Check(scene.handle != data.handle, "different texture semantics do not alias");
    Check(scene.colorUsage == TextureColorUsage::SceneSrgb, "request result retains scene semantic");
    Check(data.colorUsage == TextureColorUsage::LinearData, "request result retains data semantic");
    Check(engine::TextureInternalFormatForColorUsage(
                  TextureColorUsage::SceneSrgb,
                  PIXELFORMAT_UNCOMPRESSED_R8G8B8) == GL_SRGB8,
          "scene RGB texture semantic selects GL_SRGB8 storage");
    Check(engine::TextureInternalFormatForColorUsage(
                  TextureColorUsage::SceneSrgb,
                  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) == GL_SRGB8_ALPHA8,
          "scene RGBA texture semantic selects GL_SRGB8_ALPHA8 storage");
    Check(engine::TextureInternalFormatForColorUsage(
                  TextureColorUsage::SceneSrgb,
                  PIXELFORMAT_UNCOMPRESSED_GRAYSCALE) == 0,
          "unsupported scene-sRGB upload format requires conversion");

    engine::SpriteAnimationAssets sprites;
    sprites.OnScopeCreated(scope);
    const auto sceneSprite = sprites.RequestSpriteAnimation(
            scope,
            "sprite",
            "/tmp/sprite.json",
            TextureColorUsage::SceneSrgb,
            engine::TextureLoad_PointFilter);
    const auto displaySprite = sprites.RequestSpriteAnimation(
            scope,
            "sprite",
            "/tmp/sprite.json",
            TextureColorUsage::DisplaySrgb,
            engine::TextureLoad_PointFilter);
    Check(sceneSprite.handle != displaySprite.handle, "sprite atlas semantic participates in identity");
}

void TestNeutralToneMapping()
{
    const Vector3 black = engine::ToneMapNeutralMaxChannel(Vector3{});
    Check(Near(black.x, 0.0f) && Near(black.y, 0.0f) && Near(black.z, 0.0f),
          "neutral tone map preserves black");
    const Vector3 mapped = engine::ToneMapNeutralMaxChannel(Vector3{4.0f, 2.0f, 1.0f});
    Check(Near(mapped.x, 0.8f) && Near(mapped.y, 0.4f) && Near(mapped.z, 0.2f),
          "neutral tone map scales all channels by the maximum-channel curve");
    Check(Near(mapped.x / mapped.y, 2.0f) && Near(mapped.y / mapped.z, 2.0f),
          "neutral tone map preserves channel ratios");
    const Vector3 safe = engine::ToneMapNeutralMaxChannel(Vector3{
            -1.0f,
            std::numeric_limits<float>::infinity(),
            std::numeric_limits<float>::quiet_NaN()});
    Check(Near(safe.x, 0.0f) && Near(safe.y, 0.0f) && Near(safe.z, 0.0f),
          "neutral tone map handles negative and non-finite inputs safely");

    const std::string shader = engine::BuildScenePresentationFragmentShader();
    Check(shader.find("ToneMapNeutralMaxChannel") != std::string::npos,
          "presentation shader applies the selected neutral tone curve");
    Check(shader.find("LinearSceneToDisplaySrgb") != std::string::npos,
          "presentation shader performs the exact sRGB transfer");
    Check(shader.find("ACES") == std::string::npos && shader.find("Aces") == std::string::npos,
          "presentation shader contains no alternate ACES operator");
}

void TestRenderTargetMetadata()
{
    Check(engine::RaylibPixelFormatForRenderTarget(
                  engine::RenderTargetColorFormat::Rgba8Unorm)
                  == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
          "RGBA8 target maps to real raylib RGBA8 format");
    Check(engine::RaylibPixelFormatForRenderTarget(
                  engine::RenderTargetColorFormat::Rgba16Float)
                  == PIXELFORMAT_UNCOMPRESSED_R16G16B16A16,
          "RGBA16F target maps to real raylib half-float format");
    Check(engine::RenderTargetColorBytesPerPixel(
                  engine::RenderTargetColorFormat::Rgba8Unorm) == 4,
          "RGBA8 target reports four bytes per pixel");
    Check(engine::RenderTargetColorBytesPerPixel(
                  engine::RenderTargetColorFormat::Rgba16Float) == 8,
          "RGBA16F target reports eight bytes per pixel");

    engine::RenderTargetDescriptor descriptor{
            "test-hdr",
            100,
            50,
            engine::RenderTargetColorFormat::Rgba16Float,
            engine::RenderTargetFilter::Bilinear,
            engine::RenderTargetWrap::Clamp,
            engine::RenderTargetDepthKind::SampleableTexture,
            1};
    std::string error;
    Check(engine::ValidateRenderTargetDescriptor(descriptor, &error), "valid HDR target descriptor accepted");
    Check(engine::EstimateRenderTargetAllocationBytes(descriptor, 24, 1) == 55000,
          "target memory estimate includes color and depth precision");
    descriptor.sampleCount = 4;
    Check(!engine::ValidateRenderTargetDescriptor(descriptor, &error), "unsupported multisample descriptor rejected");
    descriptor.sampleCount = 1;
    descriptor.width = 0;
    Check(!engine::ValidateRenderTargetDescriptor(descriptor, &error), "zero target dimension rejected");

    engine::RenderTarget target;
    target.descriptor = engine::RenderTargetDescriptor{
            "diagnostic-target", 64, 32,
            engine::RenderTargetColorFormat::Rgba16Float,
            engine::RenderTargetFilter::Bilinear,
            engine::RenderTargetWrap::Clamp,
            engine::RenderTargetDepthKind::SampleableTexture,
            1};
    target.actual.internalFormat = 0x881A; // GL_RGBA16F
    target.actual.minimumFilter = 0x2601; // GL_LINEAR
    target.actual.magnificationFilter = 0x2601;
    target.actual.wrapS = 0x812F; // GL_CLAMP_TO_EDGE
    target.actual.wrapT = 0x812F;
    target.actual.sampleCount = 1;
    target.actual.depthBits = 24;
    target.actual.depth = engine::RenderTargetDepthKind::SampleableTexture;
    target.actual.estimatedAllocationBytes = 22528;
    const std::string formatted = engine::FormatRenderTargetDiagnostic(target);
    Check(formatted.find("actual=GL_RGBA16F") != std::string::npos, "target diagnostic reports queried format");
    Check(formatted.find("depth=sampleable-texture(24-bit)") != std::string::npos, "target diagnostic reports depth kind");
    Check(formatted.find("color-bpp=8") != std::string::npos, "target diagnostic reports color bytes per pixel");
}

void TestPipelineDiagnosticFormatting()
{
    engine::GraphicsContextDiagnostics graphics;
    graphics.platform = "TestOS";
    graphics.build = "Debug";
    graphics.raylibVersion = "6.0";
    graphics.glVendor = "Vendor";
    graphics.glRenderer = "Renderer";
    graphics.glVersion = "3.3";
    graphics.glslVersion = "330";
    graphics.logicalWidth = 1920;
    graphics.logicalHeight = 1080;
    graphics.renderWidth = 2880;
    graphics.renderHeight = 1620;
    graphics.redBits = 8;
    graphics.greenBits = 8;
    graphics.blueBits = 8;
    graphics.alphaBits = 8;
    graphics.sampleCount = 1;
    graphics.defaultFramebufferEncoding = "GL_LINEAR";
    const std::string formatted = engine::FormatColorPipelineDiagnostics(
            graphics,
            engine::ColorPipelineRuntimeState{1.5f, true, true});
    Check(formatted.find("active=linear-HDR") != std::string::npos, "pipeline diagnostic reports linear HDR");
    Check(formatted.find("default-fb rgba=8/8/8/8 encoding=GL_LINEAR") != std::string::npos,
          "pipeline diagnostic formats framebuffer state");
    Check(formatted.find("neutral max-channel") != std::string::npos,
          "pipeline diagnostic reports the fixed neutral global tone curve");
    Check(formatted.find("no local tone map") != std::string::npos,
          "pipeline diagnostic reports linear model output");
}

} // namespace

int main()
{
    TestTransferFunctions();
    TestTextureSemantics();
    TestNeutralToneMapping();
    TestRenderTargetMetadata();
    TestPipelineDiagnosticFormatting();

    if (failures != 0) {
        std::fprintf(stderr, "%d RenderColorFoundationsTests failure(s)\n", failures);
        return 1;
    }
    return 0;
}
