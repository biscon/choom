#include "sector_demo/renderer/SectorBloomRenderer.h"

#include <external/glad.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

constexpr int BloomDownsample = 4;
constexpr int BloomIterations = 3;

const char* PrefilterFs = R"(
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 sourceTexelSize;
uniform float threshold;
uniform float softKnee;
const float kRgba16fMaximumFinite = 65504.0;
float SanitizeLinearHdrChannelForRgba16f(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? kRgba16fMaximumFinite : 0.0;
    return min(max(value, 0.0), kRgba16fMaximumFinite);
}
vec3 SanitizeLinearHdrForRgba16f(vec3 value) {
    return vec3(SanitizeLinearHdrChannelForRgba16f(value.r),
            SanitizeLinearHdrChannelForRgba16f(value.g),
            SanitizeLinearHdrChannelForRgba16f(value.b));
}
vec3 Prefilter(vec3 inputColor) {
    vec3 color = SanitizeLinearHdrForRgba16f(inputColor);
    float brightness = max(max(color.r, color.g), color.b);
    if (brightness <= 0.0) return vec3(0.0);
    if (threshold <= 0.0) return color;
    float excess = 0.0;
    if (softKnee <= 0.0) {
        excess = max(brightness - threshold, 0.0);
    } else {
        float knee = threshold * softKnee;
        float q = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
        float softExcess = q * q / (4.0 * knee);
        excess = max(softExcess, max(brightness - threshold, 0.0));
    }
    return SanitizeLinearHdrForRgba16f(color * (excess / brightness));
}
void main() {
    vec3 seed = vec3(0.0);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            vec2 offset = (vec2(float(x), float(y)) - vec2(1.5)) * sourceTexelSize;
            seed += Prefilter(texture(texture0, clamp(fragTexCoord + offset,
                    vec2(0.0), vec2(1.0))).rgb);
        }
    }
    finalColor = vec4(SanitizeLinearHdrForRgba16f(seed * (1.0 / 16.0)), 0.0);
}
)";

const char* BlurFs = R"(
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 texelSize;
uniform vec2 direction;
uniform float radius;
const float kRgba16fMaximumFinite = 65504.0;
float SanitizeLinearHdrChannelForRgba16f(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? kRgba16fMaximumFinite : 0.0;
    return min(max(value, 0.0), kRgba16fMaximumFinite);
}
vec3 SanitizeLinearHdrForRgba16f(vec3 value) {
    return vec3(SanitizeLinearHdrChannelForRgba16f(value.r),
            SanitizeLinearHdrChannelForRgba16f(value.g),
            SanitizeLinearHdrChannelForRgba16f(value.b));
}
void main() {
    vec2 offset = direction * texelSize * radius;
    vec3 color = texture(texture0, fragTexCoord).rgb * 0.227027;
    color += texture(texture0, fragTexCoord + offset * 1.384615).rgb * 0.316216;
    color += texture(texture0, fragTexCoord - offset * 1.384615).rgb * 0.316216;
    color += texture(texture0, fragTexCoord + offset * 3.230769).rgb * 0.070270;
    color += texture(texture0, fragTexCoord - offset * 3.230769).rgb * 0.070270;
    finalColor = vec4(SanitizeLinearHdrForRgba16f(color), 0.0);
}
)";

const char* CompositeFs = R"(
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D bloomTexture;
uniform float intensity;
uniform int bloomOnly;
const float kRgba16fMaximumFinite = 65504.0;
float SanitizeLinearHdrChannelForRgba16f(float value) {
    if (isnan(value)) return 0.0;
    if (isinf(value)) return value > 0.0 ? kRgba16fMaximumFinite : 0.0;
    return min(max(value, 0.0), kRgba16fMaximumFinite);
}
vec3 SanitizeLinearHdrForRgba16f(vec3 value) {
    return vec3(SanitizeLinearHdrChannelForRgba16f(value.r),
            SanitizeLinearHdrChannelForRgba16f(value.g),
            SanitizeLinearHdrChannelForRgba16f(value.b));
}
float SafeAlpha(float value) {
    return (isnan(value) || isinf(value)) ? 1.0 : clamp(value, 0.0, 1.0);
}
void main() {
    vec3 bloom = texture(bloomTexture, fragTexCoord).rgb;
    if (bloomOnly != 0) {
        finalColor = vec4(SanitizeLinearHdrForRgba16f(bloom * intensity), 0.0);
        return;
    }
    vec4 scene = texture(texture0, fragTexCoord);
    vec3 rgb = scene.rgb + bloom * intensity;
    finalColor = vec4(SanitizeLinearHdrForRgba16f(rgb), SafeAlpha(scene.a));
}
)";

Rectangle SourceRect(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f,
            static_cast<float>(texture.width),
            -static_cast<float>(texture.height)};
}

Rectangle DestinationRect(Texture2D texture)
{
    return Rectangle{0.0f, 0.0f,
            static_cast<float>(texture.width),
            static_cast<float>(texture.height)};
}

void CopyTexture(Texture2D source, RenderTexture2D& destination)
{
    rlDrawRenderBatchActive();
    BeginTextureMode(destination);
    rlDisableColorBlend();
    DrawTexturePro(source, SourceRect(source), DestinationRect(destination.texture),
            Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
    EndTextureMode();
}

bool ValidSceneTargets(
        const engine::RenderTarget& scene,
        const engine::RenderTarget& scratch)
{
    return engine::IsRenderTargetReady(scene)
            && engine::IsRenderTargetReady(scratch)
            && scene.descriptor.colorFormat
                    == engine::RenderTargetColorFormat::Rgba16Float
            && scene.actual.internalFormat == GL_RGBA16F
            && scratch.descriptor.colorFormat
                    == engine::RenderTargetColorFormat::Rgba32Float
            && scratch.actual.internalFormat == GL_RGBA32F
            && scene.native.texture.width == scratch.native.texture.width
            && scene.native.texture.height == scratch.native.texture.height;
}

} // namespace

const char* SectorBloomDebugViewName(SectorBloomDebugView view)
{
    switch (view) {
        case SectorBloomDebugView::Normal: return "normal";
        case SectorBloomDebugView::SceneBefore: return "scene before bloom";
        case SectorBloomDebugView::Prefilter: return "HDR prefilter";
        case SectorBloomDebugView::BlurredBloom: return "blurred bloom";
        case SectorBloomDebugView::BloomOnly: return "bloom only";
        case SectorBloomDebugView::SceneAfter: return "scene after bloom";
    }
    return "invalid";
}

void SectorBloomRenderer::DisableForCurrentKey(
        const std::string& reason,
        int width,
        int height)
{
    failedForCurrentKey = true;
    failedWidth = width;
    failedHeight = height;
    diagnostics.disabled = true;
    diagnostics.ready = false;
    diagnostics.status = reason;
    TraceLog(LOG_WARNING, "HDR BLOOM: %s", reason.c_str());
}

bool SectorBloomRenderer::EnsureResources(int width, int height)
{
    if (width <= 0 || height <= 0) return false;
    if (failedForCurrentKey && failedWidth == width && failedHeight == height) {
        return false;
    }
    if (sceneWidth != width || sceneHeight != height) Shutdown();

    if (prefilterShader.id == 0) {
        prefilterShader = LoadShaderFromMemory(nullptr, PrefilterFs);
        blurShader = LoadShaderFromMemory(nullptr, BlurFs);
        compositeShader = LoadShaderFromMemory(nullptr, CompositeFs);
        if (prefilterShader.id == 0 || blurShader.id == 0
                || compositeShader.id == 0) {
            DisableForCurrentKey("required HDR bloom shader failed to compile", width, height);
            return false;
        }
        prefilterSourceTexelSizeLoc = GetShaderLocation(prefilterShader, "sourceTexelSize");
        prefilterThresholdLoc = GetShaderLocation(prefilterShader, "threshold");
        prefilterSoftKneeLoc = GetShaderLocation(prefilterShader, "softKnee");
        blurTexelSizeLoc = GetShaderLocation(blurShader, "texelSize");
        blurDirectionLoc = GetShaderLocation(blurShader, "direction");
        blurRadiusLoc = GetShaderLocation(blurShader, "radius");
        compositeBloomTextureLoc = GetShaderLocation(compositeShader, "bloomTexture");
        compositeIntensityLoc = GetShaderLocation(compositeShader, "intensity");
        compositeBloomOnlyLoc = GetShaderLocation(compositeShader, "bloomOnly");
    }

    const int bloomWidth = std::max(1, (width + BloomDownsample - 1) / BloomDownsample);
    const int bloomHeight = std::max(1, (height + BloomDownsample - 1) / BloomDownsample);
    if (!engine::IsRenderTargetReady(prefilterTarget)) {
        const auto descriptor = [bloomWidth, bloomHeight](const char* name) {
            return engine::RenderTargetDescriptor{
                    name, bloomWidth, bloomHeight,
                    engine::RenderTargetColorFormat::Rgba16Float,
                    engine::RenderTargetFilter::Bilinear,
                    engine::RenderTargetWrap::Clamp,
                    engine::RenderTargetDepthKind::None, 1};
        };
        std::string error;
        if (!engine::LoadRenderTarget(descriptor("bloom-prefilter-quarter"), prefilterTarget, &error)
                || !engine::LoadRenderTarget(descriptor("bloom-blur-a-quarter"), blurA, &error)
                || !engine::LoadRenderTarget(descriptor("bloom-blur-b-quarter"), blurB, &error)) {
            engine::UnloadRenderTarget(prefilterTarget);
            engine::UnloadRenderTarget(blurA);
            engine::UnloadRenderTarget(blurB);
            DisableForCurrentKey("RGBA16F bloom targets unavailable: " + error, width, height);
            return false;
        }
    }
    sceneWidth = width;
    sceneHeight = height;
    diagnostics.ready = true;
    diagnostics.disabled = false;
    diagnostics.sceneWidth = width;
    diagnostics.sceneHeight = height;
    diagnostics.bloomWidth = bloomWidth;
    diagnostics.bloomHeight = bloomHeight;
    diagnostics.status = "scene-wide linear HDR; max-channel prefilter before 4x4 downsample";
    diagnostics.prefilterTarget = engine::FormatRenderTargetDiagnostic(prefilterTarget);
    diagnostics.blurATarget = engine::FormatRenderTargetDiagnostic(blurA);
    diagnostics.blurBTarget = engine::FormatRenderTargetDiagnostic(blurB);
    return true;
}

bool SectorBloomRenderer::Apply(
        engine::RenderTarget& sceneTarget,
        engine::RenderTarget& sceneScratch,
        const engine::HdrBloomSettings& settingsValue)
{
    debugSource = nullptr;
    const engine::HdrBloomSettings settings =
            engine::NormalizeHdrBloomSettings(settingsValue);
    diagnostics.settings = settings;
    if (!settings.enabled) {
        diagnostics.disabled = true;
        diagnostics.status = "disabled by application settings";
        return false;
    }
    const int width = sceneTarget.native.texture.width;
    const int height = sceneTarget.native.texture.height;
    if (!ValidSceneTargets(sceneTarget, sceneScratch)) {
        DisableForCurrentKey("scene/scratch target is not validated RGBA16F/RGBA32F", width, height);
        return false;
    }
    if (!EnsureResources(width, height)) return false;

    const Vector2 sourceTexelSize{
            1.0f / static_cast<float>(width),
            1.0f / static_cast<float>(height)};
    rlDrawRenderBatchActive();
    BeginTextureMode(prefilterTarget.native);
    ClearBackground(BLANK);
    rlDisableColorBlend();
    BeginShaderMode(prefilterShader);
    SetShaderValue(prefilterShader, prefilterSourceTexelSizeLoc,
            &sourceTexelSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(prefilterShader, prefilterThresholdLoc,
            &settings.threshold, SHADER_UNIFORM_FLOAT);
    SetShaderValue(prefilterShader, prefilterSoftKneeLoc,
            &settings.softKnee, SHADER_UNIFORM_FLOAT);
    DrawTexturePro(sceneTarget.native.texture,
            SourceRect(sceneTarget.native.texture),
            DestinationRect(prefilterTarget.native.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

    RenderTexture2D* input = &prefilterTarget.native;
    RenderTexture2D* output = &blurA.native;
    const Vector2 texelSize{
            1.0f / static_cast<float>(prefilterTarget.native.texture.width),
            1.0f / static_cast<float>(prefilterTarget.native.texture.height)};
    for (int iteration = 0; iteration < BloomIterations; ++iteration) {
        for (int axis = 0; axis < 2; ++axis) {
            const Vector2 direction = axis == 0
                    ? Vector2{1.0f, 0.0f}
                    : Vector2{0.0f, 1.0f};
            rlDrawRenderBatchActive();
            BeginTextureMode(*output);
            ClearBackground(BLANK);
            rlDisableColorBlend();
            BeginShaderMode(blurShader);
            SetShaderValue(blurShader, blurTexelSizeLoc, &texelSize, SHADER_UNIFORM_VEC2);
            SetShaderValue(blurShader, blurDirectionLoc, &direction, SHADER_UNIFORM_VEC2);
            SetShaderValue(blurShader, blurRadiusLoc, &settings.radius, SHADER_UNIFORM_FLOAT);
            DrawTexturePro(input->texture, SourceRect(input->texture),
                    DestinationRect(output->texture), Vector2{}, 0.0f, WHITE);
            rlDrawRenderBatchActive();
            EndShaderMode();
            rlEnableColorBlend();
            EndTextureMode();
            input = output;
            output = output == &blurA.native ? &blurB.native : &blurA.native;
        }
    }

    const int bloomOnly = 0;
    rlDrawRenderBatchActive();
    BeginTextureMode(sceneScratch.native);
    ClearBackground(BLANK);
    rlDisableColorBlend();
    BeginShaderMode(compositeShader);
    SetShaderValueTexture(compositeShader, compositeBloomTextureLoc, input->texture);
    SetShaderValue(compositeShader, compositeIntensityLoc,
            &settings.intensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(compositeShader, compositeBloomOnlyLoc,
            &bloomOnly, SHADER_UNIFORM_INT);
    DrawTexturePro(sceneTarget.native.texture,
            SourceRect(sceneTarget.native.texture),
            DestinationRect(sceneScratch.native.texture), Vector2{}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    EndShaderMode();
    rlEnableColorBlend();
    EndTextureMode();

    if (debugView == SectorBloomDebugView::SceneBefore) {
        debugSource = &sceneTarget;
    } else if (debugView == SectorBloomDebugView::Prefilter
            || debugView == SectorBloomDebugView::BlurredBloom
            || debugView == SectorBloomDebugView::BloomOnly) {
        const Texture2D debugTexture = debugView == SectorBloomDebugView::Prefilter
                ? prefilterTarget.native.texture
                : input->texture;
        const float debugIntensity = debugView == SectorBloomDebugView::BloomOnly
                ? settings.intensity : 1.0f;
        const int debugBloomOnly = 1;
        rlDrawRenderBatchActive();
        BeginTextureMode(sceneScratch.native);
        ClearBackground(BLANK);
        rlDisableColorBlend();
        BeginShaderMode(compositeShader);
        SetShaderValueTexture(compositeShader, compositeBloomTextureLoc, debugTexture);
        SetShaderValue(compositeShader, compositeIntensityLoc,
                &debugIntensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(compositeShader, compositeBloomOnlyLoc,
                &debugBloomOnly, SHADER_UNIFORM_INT);
        DrawTexturePro(sceneTarget.native.texture,
                SourceRect(sceneTarget.native.texture),
                DestinationRect(sceneScratch.native.texture), Vector2{}, 0.0f, WHITE);
        rlDrawRenderBatchActive();
        EndShaderMode();
        rlEnableColorBlend();
        EndTextureMode();
        debugSource = &sceneScratch;
    } else if (debugView == SectorBloomDebugView::SceneAfter) {
        debugSource = &sceneScratch;
    }
    return true;
}

bool SectorBloomRenderer::IsLoaded() const
{
    return prefilterShader.id != 0 || blurShader.id != 0
            || compositeShader.id != 0
            || engine::IsRenderTargetReady(prefilterTarget)
            || engine::IsRenderTargetReady(blurA)
            || engine::IsRenderTargetReady(blurB);
}

void SectorBloomRenderer::Shutdown()
{
    if (prefilterShader.id != 0) UnloadShader(prefilterShader);
    if (blurShader.id != 0) UnloadShader(blurShader);
    if (compositeShader.id != 0) UnloadShader(compositeShader);
    prefilterShader = {};
    blurShader = {};
    compositeShader = {};
    engine::UnloadRenderTarget(prefilterTarget);
    engine::UnloadRenderTarget(blurA);
    engine::UnloadRenderTarget(blurB);
    sceneWidth = 0;
    sceneHeight = 0;
    failedWidth = 0;
    failedHeight = 0;
    failedForCurrentKey = false;
    debugSource = nullptr;
    diagnostics = {};
}

} // namespace game
