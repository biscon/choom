#include <raylib.h>
#include <external/glad.h>
#include <rlgl.h>

#include "engine/EngineContext.h"
#include "engine/assets/FontLoadFlags.h"
#include "engine/debug/DebugConsoleLogBridge.h"
#include "engine/render/ColorTransfer.h"
#include "engine/render/FxaaShader.h"
#include "engine/render/RenderColorDiagnostics.h"
#include "engine/render/RenderTarget.h"
#include "engine/render/ScenePresentationShader.h"
#include "game/GameApplication.h"

#include <cmath>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <utility>

#if defined(__linux__)
#include <time.h>
#endif

static constexpr int INTERNAL_WIDTH = 1920;
static constexpr int INTERNAL_HEIGHT = 1080;
static constexpr float DEFAULT_WORLD_RENDER_SCALE = 1.5f;
static constexpr const char* APPLICATION_SETTINGS_PATH =
        ASSETS_PATH "config/application_settings.json";

enum class RenderProfilePass : std::size_t {
    Shadows,
    World,
    Atmosphere,
    Viewmodel,
    Bloom,
    Presentation,
    FinalComposite,
    Count
};

class RenderPerformanceProfiler {
public:
    static constexpr std::size_t PassCount =
            static_cast<std::size_t>(RenderProfilePass::Count);
    static constexpr std::size_t QueryLatency = 4;

    void Initialize()
    {
        glGenQueries(static_cast<GLsizei>(queries.size()), queries.data());
        initialized = queries[0] != 0;
    }

    void Shutdown()
    {
        if (initialized) {
            glDeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
        }
        *this = {};
    }

    void BeginFrame(bool enabled)
    {
        lastCpuMilliseconds.fill(0.0);
        active = enabled && initialized;
        activePass = PassCount;
        slot = frameIndex % QueryLatency;
        const std::uint32_t issuedMask = issuedMasks[slot];
        if (issuedMask != 0) {
            bool ready = true;
            for (std::size_t pass = 0; pass < PassCount; ++pass) {
                if ((issuedMask & (1u << pass)) == 0) continue;
                GLint available = GL_FALSE;
                glGetQueryObjectiv(Query(pass, slot), GL_QUERY_RESULT_AVAILABLE,
                        &available);
                ready = ready && available == GL_TRUE;
            }
            if (ready) {
                for (std::size_t pass = 0; pass < PassCount; ++pass) {
                    if ((issuedMask & (1u << pass)) == 0) continue;
                    GLuint64 nanoseconds = 0;
                    glGetQueryObjectui64v(Query(pass, slot), GL_QUERY_RESULT,
                            &nanoseconds);
                    Smooth(gpuMilliseconds[pass],
                            static_cast<double>(nanoseconds) / 1000000.0);
                }
                issuedMasks[slot] = 0;
            } else {
                active = false;
            }
        }
        ++frameIndex;
    }

    void Begin(RenderProfilePass pass)
    {
        const std::size_t index = static_cast<std::size_t>(pass);
        cpuStart[index] = GetTime();
        if (active) {
            glBeginQuery(GL_TIME_ELAPSED, Query(index, slot));
            activePass = index;
        }
    }

    void End(RenderProfilePass pass)
    {
        const std::size_t index = static_cast<std::size_t>(pass);
        lastCpuMilliseconds[index] = (GetTime() - cpuStart[index]) * 1000.0;
        Smooth(cpuMilliseconds[index], lastCpuMilliseconds[index]);
        if (active && activePass == index) {
            glEndQuery(GL_TIME_ELAPSED);
            activePass = PassCount;
            issuedMasks[slot] |= (1u << index);
        }
    }

    double LastCpuMilliseconds(RenderProfilePass pass) const
    {
        return lastCpuMilliseconds[static_cast<std::size_t>(pass)];
    }

    double GpuMilliseconds(RenderProfilePass pass) const
    {
        return gpuMilliseconds[static_cast<std::size_t>(pass)];
    }

    void Draw(
            float renderScale,
            bool fxaa,
            const game::SectorAtmosphereDiagnostics& atmosphere) const
    {
        static constexpr const char* Names[PassCount] = {
                "shadows", "world", "atmosphere", "viewmodel", "bloom",
                "presentation", "final"};
        DrawRectangle(8, 42, 760, 104 + static_cast<int>(PassCount) * 20,
                Color{0, 0, 0, 190});
        DrawText(TextFormat("Render %.0f%%  FXAA %s  CPU / GPU ms",
                         renderScale * 100.0f, fxaa ? "on" : "off"),
                16, 48, 16, LIME);
        for (std::size_t pass = 0; pass < PassCount; ++pass) {
            DrawText(TextFormat("%-13s %6.2f / %6.2f", Names[pass],
                             cpuMilliseconds[pass], gpuMilliseconds[pass]),
                    16, 70 + static_cast<int>(pass) * 20, 16, RAYWHITE);
        }
        const int detailY = 70 + static_cast<int>(PassCount) * 20;
        DrawText(TextFormat(
                         "atmo GPU dist/analytic/ray/haze/shaft/halo/dust %.2f/%.2f/%.2f/%.2f/%.2f/%.2f/%.2f",
                         atmosphere.distanceFogGpuMilliseconds,
                         atmosphere.analyticFogGpuMilliseconds,
                         atmosphere.localFogGpuMilliseconds,
                         atmosphere.lightHazeGpuMilliseconds,
                         atmosphere.analyticShaftGpuMilliseconds,
                         atmosphere.lightHaloGpuMilliseconds,
                         atmosphere.dustGpuMilliseconds),
                16, detailY, 16, SKYBLUE);
        DrawText(TextFormat(
                         "fog ray %d/%d ana %d/%d  shaft %d/%d D%d  halo %d D%d  lights %d",
                         atmosphere.localFogActiveCount,
                         atmosphere.localFogEligibleCount,
                         atmosphere.analyticFogActiveCount,
                         atmosphere.analyticFogEligibleCount,
                         atmosphere.analyticShaftActiveCount,
                         atmosphere.analyticShaftEligibleCount,
                         atmosphere.analyticShaftDrawCallCount,
                         atmosphere.lightHaloCount,
                         atmosphere.lightHaloDrawCallCount,
                         atmosphere.dynamicLightCount),
                16, detailY + 20, 16, SKYBLUE);
        DrawText(TextFormat(
                         "dust %d/%d particles %d  fog ids %d,%d,%d,%d",
                         atmosphere.dustActiveEmitterCount,
                         atmosphere.dustEligibleEmitterCount,
                         atmosphere.dustVisibleParticleCount,
                         atmosphere.localFogVolumeIds[0],
                         atmosphere.localFogVolumeIds[1],
                         atmosphere.localFogVolumeIds[2],
                         atmosphere.localFogVolumeIds[3]),
                16, detailY + 40, 16, SKYBLUE);
        DrawText(TextFormat(
                         "haze kind:id %d:%d %d:%d %d:%d %d:%d",
                         static_cast<int>(atmosphere.lightHazeSources[0].kind),
                         atmosphere.lightHazeSources[0].lightId,
                         static_cast<int>(atmosphere.lightHazeSources[1].kind),
                         atmosphere.lightHazeSources[1].lightId,
                         static_cast<int>(atmosphere.lightHazeSources[2].kind),
                         atmosphere.lightHazeSources[2].lightId,
                         static_cast<int>(atmosphere.lightHazeSources[3].kind),
                         atmosphere.lightHazeSources[3].lightId),
                16, detailY + 60, 16, SKYBLUE);
    }

private:
    GLuint Query(std::size_t pass, std::size_t querySlot) const
    {
        return queries[querySlot * PassCount + pass];
    }

    static void Smooth(double& current, double sample)
    {
        current = current <= 0.0 ? sample : current * 0.85 + sample * 0.15;
    }

    std::array<GLuint, PassCount * QueryLatency> queries{};
    std::array<std::uint32_t, QueryLatency> issuedMasks{};
    std::array<double, PassCount> cpuStart{};
    std::array<double, PassCount> lastCpuMilliseconds{};
    std::array<double, PassCount> cpuMilliseconds{};
    std::array<double, PassCount> gpuMilliseconds{};
    std::size_t frameIndex = 0;
    std::size_t slot = 0;
    std::size_t activePass = PassCount;
    bool initialized = false;
    bool active = false;
};

class FrameDipTrace {
public:
    void Initialize(int argc, char** argv)
    {
        static constexpr const char* ThresholdPrefix =
                "--trace-frame-dips-ms=";
        static constexpr const char* OutputPrefix =
                "--frame-trace-output=";
        const char* outputPath = nullptr;
        for (int index = 1; index < argc; ++index) {
            const char* argument = argv[index] != nullptr ? argv[index] : "";
            if (std::strncmp(
                        argument,
                        ThresholdPrefix,
                        std::strlen(ThresholdPrefix)) == 0) {
                const double requested = std::strtod(
                        argument + std::strlen(ThresholdPrefix),
                        nullptr);
                if (std::isfinite(requested) && requested > 0.0) {
                    thresholdMilliseconds = requested;
                }
            } else if (std::strncmp(
                               argument,
                               OutputPrefix,
                               std::strlen(OutputPrefix)) == 0) {
                outputPath = argument + std::strlen(OutputPrefix);
            }
        }
        if (thresholdMilliseconds <= 0.0) {
            return;
        }

        output = stderr;
        if (outputPath != nullptr && outputPath[0] != '\0') {
            output = std::fopen(outputPath, "w");
            ownsOutput = output != nullptr;
            if (output == nullptr) {
                output = stderr;
                std::fprintf(
                        stderr,
                        "FRAME_TRACE: could not open %s; using stderr\n",
                        outputPath);
            }
        }
        std::fprintf(
                output,
                "# frame trace v3; dips >= %.3f ms; timestamps use CLOCK_MONOTONIC\n"
                "# haze source kinds: 0=static-point 1=static-spot 2=dynamic-point 3=dynamic-spot\n"
                "# mono_seconds total_ms pre_render_ms shadows_cpu_ms world_cpu_ms atmosphere_cpu_ms viewmodel_cpu_ms bloom_cpu_ms presentation_cpu_ms final_cpu_ms shadows_gpu_ms world_gpu_ms atmosphere_gpu_ms viewmodel_gpu_ms bloom_gpu_ms presentation_gpu_ms final_gpu_ms local_fog_gpu_ms light_haze_gpu_ms dust_gpu_ms fog_eligible fog_active fog_coverage haze_eligible haze_active haze_coverage dust_active dust_visible dynamic_lights distance_fog_gpu_ms analytic_fog_gpu_ms analytic_shaft_gpu_ms light_halo_gpu_ms analytic_fog_eligible analytic_fog_active analytic_fog_coverage shaft_eligible shaft_active shaft_coverage shaft_draws halo_eligible halo_count halo_draws fog_id0 fog_id1 fog_id2 fog_id3 fog_id4 fog_id5 fog_id6 fog_id7 fog_id8 fog_id9 fog_id10 fog_id11 fog_id12 fog_id13 fog_id14 fog_id15 haze_kind0 haze_id0 haze_kind1 haze_id1 haze_kind2 haze_id2 haze_kind3 haze_id3 haze_kind4 haze_id4 haze_kind5 haze_id5 haze_kind6 haze_id6 haze_kind7 haze_id7\n",
                thresholdMilliseconds);
        std::fflush(output);
    }

    void Shutdown()
    {
        if (ownsOutput && output != nullptr) {
            std::fclose(output);
        }
        output = nullptr;
        ownsOutput = false;
    }

    bool Enabled() const
    {
        return output != nullptr && thresholdMilliseconds > 0.0;
    }

    double TimestampSeconds() const
    {
#if defined(__linux__)
        timespec timestamp{};
        if (clock_gettime(CLOCK_MONOTONIC, &timestamp) == 0) {
            return static_cast<double>(timestamp.tv_sec)
                    + static_cast<double>(timestamp.tv_nsec) / 1000000000.0;
        }
#endif
        return GetTime();
    }

    void Record(
            double frameStartSeconds,
            double preRenderEndSeconds,
            const RenderPerformanceProfiler& profiler,
            const game::SectorAtmosphereDiagnostics& atmosphere)
    {
        if (!Enabled()) {
            return;
        }
        const double frameEndSeconds = TimestampSeconds();
        const double totalMilliseconds =
                (frameEndSeconds - frameStartSeconds) * 1000.0;
        if (!std::isfinite(totalMilliseconds)
                || totalMilliseconds < thresholdMilliseconds) {
            return;
        }
        std::fprintf(
                output,
                "%.9f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %d %d %.4f %d %d %.4f %d %d %d %.3f %.3f %.3f %.3f %d %d %.4f %d %d %.4f %d %d %d %d",
                frameEndSeconds,
                totalMilliseconds,
                (preRenderEndSeconds - frameStartSeconds) * 1000.0,
                profiler.LastCpuMilliseconds(RenderProfilePass::Shadows),
                profiler.LastCpuMilliseconds(RenderProfilePass::World),
                profiler.LastCpuMilliseconds(RenderProfilePass::Atmosphere),
                profiler.LastCpuMilliseconds(RenderProfilePass::Viewmodel),
                profiler.LastCpuMilliseconds(RenderProfilePass::Bloom),
                profiler.LastCpuMilliseconds(RenderProfilePass::Presentation),
                profiler.LastCpuMilliseconds(RenderProfilePass::FinalComposite),
                profiler.GpuMilliseconds(RenderProfilePass::Shadows),
                profiler.GpuMilliseconds(RenderProfilePass::World),
                profiler.GpuMilliseconds(RenderProfilePass::Atmosphere),
                profiler.GpuMilliseconds(RenderProfilePass::Viewmodel),
                profiler.GpuMilliseconds(RenderProfilePass::Bloom),
                profiler.GpuMilliseconds(RenderProfilePass::Presentation),
                profiler.GpuMilliseconds(RenderProfilePass::FinalComposite),
                atmosphere.localFogGpuMilliseconds,
                atmosphere.lightHazeGpuMilliseconds,
                atmosphere.dustGpuMilliseconds,
                atmosphere.localFogEligibleCount,
                atmosphere.localFogActiveCount,
                atmosphere.localFogScissorCoverage,
                atmosphere.lightHazeEligibleCount,
                atmosphere.lightHazeActiveCount,
                atmosphere.lightHazeScissorCoverage,
                atmosphere.dustActiveEmitterCount,
                atmosphere.dustVisibleParticleCount,
                atmosphere.dynamicLightCount,
                atmosphere.distanceFogGpuMilliseconds,
                atmosphere.analyticFogGpuMilliseconds,
                atmosphere.analyticShaftGpuMilliseconds,
                atmosphere.lightHaloGpuMilliseconds,
                atmosphere.analyticFogEligibleCount,
                atmosphere.analyticFogActiveCount,
                atmosphere.analyticFogScissorCoverage,
                atmosphere.analyticShaftEligibleCount,
                atmosphere.analyticShaftActiveCount,
                atmosphere.analyticShaftScissorCoverage,
                atmosphere.analyticShaftDrawCallCount,
                atmosphere.lightHaloEligibleCount,
                atmosphere.lightHaloCount,
                atmosphere.lightHaloDrawCallCount);
        for (const int fogVolumeId : atmosphere.localFogVolumeIds) {
            std::fprintf(output, " %d", fogVolumeId);
        }
        for (const game::SectorLightHazeActiveSource& source
                : atmosphere.lightHazeSources) {
            std::fprintf(
                    output,
                    " %d %d",
                    static_cast<int>(source.kind),
                    source.lightId);
        }
        std::fputc('\n', output);
        std::fflush(output);
    }

private:
    std::FILE* output = nullptr;
    double thresholdMilliseconds = 0.0;
    bool ownsOutput = false;
};

#if defined(__APPLE__)
static constexpr int STARTUP_WINDOW_WIDTH = 1600;
static constexpr int STARTUP_WINDOW_HEIGHT = 900;
#else
static constexpr int STARTUP_WINDOW_WIDTH = 1920;
static constexpr int STARTUP_WINDOW_HEIGHT = 1080;
#endif

static Rectangle GetFullscreenSrcRect(const Texture2D& tex)
{
    return Rectangle{
            0.5f,
            0.5f,
            (float)tex.width  - 1.0f,
            -(float)tex.height + 1.0f
    };
}

static Rectangle BuildPresentationRect(float backbufferWidth, float backbufferHeight,
                                       float drawableWidth, float drawableHeight)
{
    const float backbufferAspect = backbufferWidth / backbufferHeight;
    const float drawableAspect = drawableWidth / drawableHeight;

    Rectangle dst{};

    if (drawableAspect > backbufferAspect) {
        dst.height = drawableHeight;
        dst.width = std::round(dst.height * backbufferAspect);
        dst.x = std::floor((drawableWidth - dst.width) * 0.5f);
        dst.y = 0.0f;
    } else {
        dst.width = drawableWidth;
        dst.height = std::round(dst.width / backbufferAspect);
        dst.x = 0.0f;
        dst.y = std::floor((drawableHeight - dst.height) * 0.5f);
    }

    return dst;
}

static void ClearLinearSceneBackground(Color displaySrgbColor)
{
    const Vector4 linear = engine::SrgbColorBytesToLinearSceneRgba(displaySrgbColor);
    rlDrawRenderBatchActive();
    glClearColor(linear.x, linear.y, linear.z, linear.w);
    rlClearScreenBuffers();
}

int main(int argc, char** argv)
{
    engine::InstallDebugConsoleTraceLogBridge();
    game::FpsApplicationSettings startupSettings;
    std::string startupSettingsError;
    if (!game::LoadFpsApplicationSettings(
                APPLICATION_SETTINGS_PATH,
                startupSettings,
                &startupSettingsError)) {
        TraceLog(
                LOG_WARNING,
                "Application settings ignored: %s",
                startupSettingsError.c_str());
        startupSettings = game::FpsApplicationSettings{};
    }
    startupSettings.graphics = game::NormalizeFpsGraphicsSettings(
            startupSettings.graphics);
    unsigned int flags = 0;
    if (startupSettings.graphics.vsync) {
        flags |= FLAG_VSYNC_HINT;
    }
    SetConfigFlags(flags);

    InitWindow(STARTUP_WINDOW_WIDTH, STARTUP_WINDOW_HEIGHT, "Engine");
    // Presentation is encoded explicitly by the global presentation shader.
    glDisable(GL_FRAMEBUFFER_SRGB);
    //HideCursor();

    SetExitKey(0);

    engine::RenderTarget worldTargetResource;
    RenderTexture2D viewmodelTarget{};
    std::string renderTargetError;
    float currentWorldRenderScale = DEFAULT_WORLD_RENDER_SCALE;
    const auto unloadViewmodelTarget = [](RenderTexture2D& target) {
        if (target.id != 0) {
            // rlUnloadFramebuffer owns and removes the private depth
            // attachment, but leaves the borrowed world color texture alone.
            rlUnloadFramebuffer(target.id);
        }
        target = {};
    };
    const auto loadWorldTargets = [&](float renderScale,
                                      engine::RenderTarget& world,
                                      RenderTexture2D& viewmodel,
                                      std::string& error) {
        const int width = static_cast<int>(
                static_cast<float>(INTERNAL_WIDTH) * renderScale + 0.5f);
        const int height = static_cast<int>(
                static_cast<float>(INTERNAL_HEIGHT) * renderScale + 0.5f);
        if (!engine::LoadRenderTarget(
                    engine::RenderTargetDescriptor{
                            "world", width, height,
                            engine::RenderTargetColorFormat::Rgba16Float,
                            engine::RenderTargetFilter::Bilinear,
                            engine::RenderTargetWrap::Repeat,
                            engine::RenderTargetDepthKind::SampleableTexture,
                            1},
                    world, &error)) {
            TraceLog(LOG_WARNING,
                    "PREVIEW: sampleable depth target unavailable; local fog disabled");
            if (!engine::LoadRenderTarget(
                        engine::RenderTargetDescriptor{
                                "world", width, height,
                                engine::RenderTargetColorFormat::Rgba16Float,
                                engine::RenderTargetFilter::Bilinear,
                                engine::RenderTargetWrap::Repeat,
                                engine::RenderTargetDepthKind::Renderbuffer,
                                1},
                        world, &error)) {
                return false;
            }
            world.native.depth.mipmaps = 0;
        }
        viewmodel.id = rlLoadFramebuffer();
        viewmodel.texture = world.native.texture;
        viewmodel.depth.id = rlLoadTextureDepth(width, height, true);
        viewmodel.depth.width = width;
        viewmodel.depth.height = height;
        viewmodel.depth.format = 19;
        viewmodel.depth.mipmaps = 1;
        if (viewmodel.id == 0 || viewmodel.depth.id == 0) {
            unloadViewmodelTarget(viewmodel);
            engine::UnloadRenderTarget(world);
            error = "Could not allocate private viewmodel depth framebuffer";
            return false;
        }
        // Reuse the active HDR scene color with a private viewmodel depth
        // attachment. The viewmodel can now render in-place without a color
        // composite and full-scene copy.
        rlEnableFramebuffer(viewmodel.id);
        rlFramebufferAttach(
                viewmodel.id,
                world.native.texture.id,
                RL_ATTACHMENT_COLOR_CHANNEL0,
                RL_ATTACHMENT_TEXTURE2D,
                0);
        rlFramebufferAttach(
                viewmodel.id,
                viewmodel.depth.id,
                RL_ATTACHMENT_DEPTH,
                RL_ATTACHMENT_RENDERBUFFER,
                0);
        const bool viewFramebufferComplete =
                rlFramebufferComplete(viewmodel.id);
        rlDisableFramebuffer();
        if (!viewFramebufferComplete) {
            error = "Viewmodel framebuffer could not alias the HDR scene color";
            engine::UnloadRenderTarget(world);
            unloadViewmodelTarget(viewmodel);
            return false;
        }
        return true;
    };
    if (!loadWorldTargets(currentWorldRenderScale, worldTargetResource,
                viewmodelTarget, renderTargetError)) {
        TraceLog(LOG_ERROR, "RENDER: required HDR targets unavailable: %s",
                renderTargetError.c_str());
        CloseWindow();
        return 1;
    }
    RenderTexture2D& worldTarget = engine::NativeRenderTexture(worldTargetResource);

    engine::RenderTarget scenePresentationTargetResource;
    engine::LoadRenderTarget(
            engine::RenderTargetDescriptor{
                    "scene-srgb-presentation",
                    INTERNAL_WIDTH,
                    INTERNAL_HEIGHT,
                    engine::RenderTargetColorFormat::Rgba8Unorm,
                    engine::RenderTargetFilter::Bilinear,
                    engine::RenderTargetWrap::Clamp,
                    engine::RenderTargetDepthKind::None,
                    1},
            scenePresentationTargetResource,
            &renderTargetError);
    if (!engine::IsRenderTargetReady(scenePresentationTargetResource)) {
        TraceLog(LOG_ERROR, "RENDER: required scene presentation target unavailable: %s", renderTargetError.c_str());
        unloadViewmodelTarget(viewmodelTarget);
        engine::UnloadRenderTarget(worldTargetResource);
        engine::UnloadRenderTarget(scenePresentationTargetResource);
        CloseWindow();
        return 1;
    }
    RenderTexture2D& scenePresentationTarget =
            engine::NativeRenderTexture(scenePresentationTargetResource);

    const auto loadDisplayTarget = [&renderTargetError](
            const char* name,
            engine::RenderTarget& target) {
        return engine::LoadRenderTarget(
                engine::RenderTargetDescriptor{
                        name,
                        INTERNAL_WIDTH,
                        INTERNAL_HEIGHT,
                        engine::RenderTargetColorFormat::Rgba8Unorm,
                        engine::RenderTargetFilter::Bilinear,
                        engine::RenderTargetWrap::Repeat,
                        engine::RenderTargetDepthKind::Renderbuffer,
                        1},
                target,
                &renderTargetError);
    };
    engine::RenderTarget editorTargetResource;
    engine::RenderTarget uiTargetResource;
    engine::RenderTarget menuTargetResource;
    engine::RenderTarget consoleTargetResource;
    loadDisplayTarget("editor-2d", editorTargetResource);
    loadDisplayTarget("ui", uiTargetResource);
    loadDisplayTarget("menu", menuTargetResource);
    loadDisplayTarget("debug-console", consoleTargetResource);
    RenderTexture2D& editorTarget = engine::NativeRenderTexture(editorTargetResource);
    RenderTexture2D& uiTarget = engine::NativeRenderTexture(uiTargetResource);
    RenderTexture2D& menuTarget = engine::NativeRenderTexture(menuTargetResource);
    RenderTexture2D& consoleTarget =
            engine::NativeRenderTexture(consoleTargetResource);

    Shader fxaaShader{};
    int fxaaTexelSizeLoc = -1;
    fxaaShader = LoadShaderFromMemory(nullptr, engine::FxaaFragmentShader);
    fxaaTexelSizeLoc = GetShaderLocation(fxaaShader, "texelSize");
    const std::string scenePresentationFragmentShader =
            engine::BuildScenePresentationFragmentShader();
    Shader scenePresentationShader = LoadShaderFromMemory(
            nullptr,
            scenePresentationFragmentShader.c_str());
    if (!IsShaderValid(scenePresentationShader)) {
        TraceLog(LOG_ERROR, "RENDER: required neutral tone-map/sRGB presentation shader unavailable");
        if (IsShaderValid(fxaaShader)) UnloadShader(fxaaShader);
        unloadViewmodelTarget(viewmodelTarget);
        engine::UnloadRenderTarget(worldTargetResource);
        engine::UnloadRenderTarget(scenePresentationTargetResource);
        engine::UnloadRenderTarget(editorTargetResource);
        engine::UnloadRenderTarget(uiTargetResource);
        engine::UnloadRenderTarget(menuTargetResource);
        engine::UnloadRenderTarget(consoleTargetResource);
        CloseWindow();
        return 1;
    }
    const auto unloadRenderResources = [&]() {
        if (IsShaderValid(fxaaShader)) {
            UnloadShader(fxaaShader);
        }
        if (IsShaderValid(scenePresentationShader)) {
            UnloadShader(scenePresentationShader);
        }
        unloadViewmodelTarget(viewmodelTarget);
        engine::UnloadRenderTarget(worldTargetResource);
        engine::UnloadRenderTarget(scenePresentationTargetResource);
        engine::UnloadRenderTarget(editorTargetResource);
        engine::UnloadRenderTarget(uiTargetResource);
        engine::UnloadRenderTarget(menuTargetResource);
        engine::UnloadRenderTarget(consoleTargetResource);
    };

    engine::EngineContext context;
    context.input.Initialize();
    context.input.ReserveEvents(256);
    context.audio.Initialize();

    if (!context.assets.Initialize()) {
        context.audio.Shutdown();
        unloadRenderResources();
        CloseWindow();
        return 1;
    }

    engine::AssetManager& assets = context.assets;

    engine::FontHandle uiFont = assets.RequestFont(
            assets.GlobalScope(),
            "editor_ui_regular_28",
            ASSETS_PATH "fonts/IBMPlexSans-Regular.ttf",
            28,
            engine::FontLoad_BilinearFilter
    );
    engine::FontHandle smallFont = assets.RequestFont(
            assets.GlobalScope(),
            "editor_ui_regular_22",
            ASSETS_PATH "fonts/IBMPlexSans-Regular.ttf",
            22,
            engine::FontLoad_BilinearFilter
    );

    while (!WindowShouldClose()
            && (!assets.IsFinished(uiFont) || !assets.IsFinished(smallFont))) {
        assets.UpdateMainThread(2.0f);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Loading assets...", 40, 40, 32, RAYWHITE);
        EndDrawing();
    }

    if (WindowShouldClose()) {
        unloadRenderResources();
        context.assets.Shutdown();
        context.audio.Shutdown();
        CloseWindow();
        return 0;
    }

    engine::UIContext contentUi;
    engine::UIContext menuUi;
    engine::UIConfig uiConfig;
    uiConfig.overlayBounds = Rectangle{
            0.0f,
            0.0f,
            static_cast<float>(INTERNAL_WIDTH),
            static_cast<float>(INTERNAL_HEIGHT)
    };
    game::GameApplication application;
    if (!application.Init(
                context,
                std::move(startupSettings),
                std::move(startupSettingsError))) {
        TraceLog(LOG_ERROR, "Engine application initialization failed");
        unloadRenderResources();
        context.assets.Shutdown();
        context.audio.Shutdown();
        CloseWindow();
        return 1;
    }
    const auto replaceWorldTargets = [&](float renderScale, std::string& error) {
        engine::RenderTarget replacementWorld;
        RenderTexture2D replacementViewmodel{};
        if (!loadWorldTargets(renderScale, replacementWorld,
                    replacementViewmodel, error)) {
            return false;
        }
        unloadViewmodelTarget(viewmodelTarget);
        engine::UnloadRenderTarget(worldTargetResource);
        worldTargetResource = std::move(replacementWorld);
        viewmodelTarget = replacementViewmodel;
        replacementWorld = {};
        replacementViewmodel = {};
        currentWorldRenderScale = renderScale;
        return true;
    };
    const float configuredRenderScale =
            application.ApplicationSettings().graphics.renderScale;
    if (std::fabs(configuredRenderScale - currentWorldRenderScale) > 0.001f) {
        if (!replaceWorldTargets(configuredRenderScale, renderTargetError)) {
            TraceLog(LOG_WARNING,
                    "RENDER: configured render scale could not be applied: %s",
                    renderTargetError.c_str());
        }
    }
    engine::LogColorPipelineDiagnostics(engine::ColorPipelineRuntimeState{
            currentWorldRenderScale,
            application.ApplicationSettings().graphics.fxaa,
            application.ApplicationSettings().graphics.fxaa
                    && IsShaderValid(fxaaShader)});

    while (!WindowShouldClose()
            && !assets.IsScopeFinished(assets.GlobalScope())) {
        assets.UpdateMainThread(2.0f);
        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Loading global assets...", 40, 40, 32, RAYWHITE);
        EndDrawing();
    }
    if (WindowShouldClose()) {
        application.Shutdown(context);
        unloadRenderResources();
        context.assets.Shutdown();
        context.audio.Shutdown();
        CloseWindow();
        return 0;
    }

    BeginTextureMode(editorTarget);
    ClearBackground(Color{8, 10, 14, 255});
    EndTextureMode();
    BeginTextureMode(uiTarget);
    ClearBackground(BLANK);
    EndTextureMode();
    BeginTextureMode(menuTarget);
    ClearBackground(BLANK);
    EndTextureMode();
    BeginTextureMode(consoleTarget);
    ClearBackground(BLANK);
    EndTextureMode();
    BeginTextureMode(scenePresentationTarget);
    ClearBackground(BLACK);
    EndTextureMode();

    RenderPerformanceProfiler performanceProfiler;
    performanceProfiler.Initialize();
    FrameDipTrace frameDipTrace;
    frameDipTrace.Initialize(argc, argv);

    while (!WindowShouldClose() && !application.QuitRequested())
    {
        const double frameStartSeconds = frameDipTrace.Enabled()
                ? frameDipTrace.TimestampSeconds()
                : 0.0;
        assets.UpdateMainThread(2.0f);

        const float dt = GetFrameTime();
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        Rectangle dst = BuildPresentationRect(
                static_cast<float>(INTERNAL_WIDTH),
                static_cast<float>(INTERNAL_HEIGHT),
                static_cast<float>(screenW),
                static_cast<float>(screenH)
        );

        SetMouseOffset(
                -static_cast<int>(dst.x),
                -static_cast<int>(dst.y));

        SetMouseScale(
                static_cast<float>(INTERNAL_WIDTH) / dst.width,
                static_cast<float>(INTERNAL_HEIGHT) / dst.height
        );

        context.input.BeginFrame();
        context.input.PollRaylib(dt);
        application.UpdateDebugConsole(context, dt);
        bool windowModeChanged = false;
        context.input.ForEachEvent(
                engine::InputEventType::KeyPressed,
                true,
                [&windowModeChanged](engine::InputEvent& event) {
                    if (event.key.key != KEY_F10) {
                        return;
                    }

                    ToggleBorderlessWindowed();
                    windowModeChanged = true;
                    engine::ConsumeEvent(event);
                });
        if (windowModeChanged) {
            screenW = GetScreenWidth();
            screenH = GetScreenHeight();
            dst = BuildPresentationRect(
                    static_cast<float>(INTERNAL_WIDTH),
                    static_cast<float>(INTERNAL_HEIGHT),
                    static_cast<float>(screenW),
                    static_cast<float>(screenH));
            SetMouseOffset(
                    -static_cast<int>(dst.x),
                    -static_cast<int>(dst.y));
            SetMouseScale(
                    static_cast<float>(INTERNAL_WIDTH) / dst.width,
                    static_cast<float>(INTERNAL_HEIGHT) / dst.height);
        }
        context.input.ForEachEvent(
                engine::InputEventType::KeyPressed,
                true,
                [&application](engine::InputEvent& event) {
                    // F3 is reserved for the sector editor's 3D control-mode toggle.
                    if (event.key.key != KEY_F9) {
                        return;
                    }
                    application.TogglePerformanceOverlay();
                    engine::ConsumeEvent(event);
                });

        if (application.IsMenuOpen()) {
            BeginTextureMode(menuTarget);
            ClearBackground(BLANK);
            application.RenderInteractiveUI(
                    contentUi,
                    menuUi,
                    uiConfig,
                    context.input,
                    assets,
                    uiFont,
                    smallFont);
            EndTextureMode();
        } else {
            BeginTextureMode(uiTarget);
            ClearBackground(BLANK);
            application.RenderInteractiveUI(
                    contentUi,
                    menuUi,
                    uiConfig,
                    context.input,
                    assets,
                    uiFont,
                    smallFont);
            EndTextureMode();
            BeginTextureMode(menuTarget);
            ClearBackground(BLANK);
            EndTextureMode();
        }

        application.Update(context, dt);
        application.ProcessDeferredDebugActions(context);
        if (const game::FpsApplicationSettings* pending =
                    application.PendingGraphicsSettings()) {
            const float requestedScale = pending->graphics.renderScale;
            engine::RenderTarget replacementWorld;
            RenderTexture2D replacementViewmodel{};
            const bool scaleChanged = std::fabs(
                    requestedScale - currentWorldRenderScale) > 0.001f;
            bool resourcesReady = true;
            if (scaleChanged) {
                resourcesReady = loadWorldTargets(
                        requestedScale,
                        replacementWorld,
                        replacementViewmodel,
                        renderTargetError);
            }
            if (!resourcesReady) {
                application.RejectPendingGraphicsSettings(
                        "Could not allocate render targets: " + renderTargetError);
            } else {
                std::string settingsError;
                if (application.CommitPendingGraphicsSettings(settingsError)
                        && scaleChanged) {
                    unloadViewmodelTarget(viewmodelTarget);
                    engine::UnloadRenderTarget(worldTargetResource);
                    worldTargetResource = std::move(replacementWorld);
                    viewmodelTarget = replacementViewmodel;
                    replacementWorld = {};
                    replacementViewmodel = {};
                    currentWorldRenderScale = requestedScale;
                } else {
                    engine::UnloadRenderTarget(replacementWorld);
                    unloadViewmodelTarget(replacementViewmodel);
                }
            }
        }
        context.audio.Update(assets);

        BeginTextureMode(consoleTarget);
        ClearBackground(BLANK);
        application.RenderDebugConsole(
                assets, INTERNAL_WIDTH, INTERNAL_HEIGHT);
        EndTextureMode();

        const game::ApplicationContentKind contentKind =
                application.BackgroundContentKind();
        const bool render3D =
                contentKind == game::ApplicationContentKind::Sector3D;
        const bool useWorldFxaa = application.ApplicationSettings().graphics.fxaa
                && IsShaderValid(fxaaShader);
        const double preRenderEndSeconds = frameDipTrace.Enabled()
                ? frameDipTrace.TimestampSeconds()
                : 0.0;
        const bool collectPerformanceDiagnostics = frameDipTrace.Enabled()
                || application.ApplicationSettings().graphics.performanceOverlay;
        performanceProfiler.BeginFrame(collectPerformanceDiagnostics);
        if (application.ShouldRefreshBackground() && render3D) {
            performanceProfiler.Begin(RenderProfilePass::Shadows);
            application.Render3DShadowMaps(context);
            performanceProfiler.End(RenderProfilePass::Shadows);

            performanceProfiler.Begin(RenderProfilePass::World);
            BeginTextureMode(worldTarget);
            ClearLinearSceneBackground(Color{8, 10, 14, 255});
            application.Render3DScene(context);
            EndTextureMode();
            performanceProfiler.End(RenderProfilePass::World);

            performanceProfiler.Begin(RenderProfilePass::Atmosphere);
            application.Apply3DWorldAtmosphere(
                    worldTargetResource,
                    collectPerformanceDiagnostics);
            performanceProfiler.End(RenderProfilePass::Atmosphere);

            performanceProfiler.Begin(RenderProfilePass::Viewmodel);
            if (viewmodelTarget.id != 0 && viewmodelTarget.depth.id != 0) {
                BeginTextureMode(viewmodelTarget);
                rlDrawRenderBatchActive();
                glClear(GL_DEPTH_BUFFER_BIT);
                application.Render3DViewmodel(assets);
                EndTextureMode();
            }
            performanceProfiler.End(RenderProfilePass::Viewmodel);

            performanceProfiler.Begin(RenderProfilePass::Bloom);
            application.Apply3DHdrBloom(worldTargetResource);
            performanceProfiler.End(RenderProfilePass::Bloom);

            BeginTextureMode(worldTarget);
            application.Render3DOverlays();
            EndTextureMode();

            const engine::RenderTarget* hdrDebugSource =
                    application.HdrDebugPresentationSource();
            const Texture2D linearSceneTexture = hdrDebugSource != nullptr
                    ? hdrDebugSource->native.texture
                    : worldTarget.texture;
            // Bilinear supersample resolve happens in the same texture sample
            // as the neutral tone-map and display encoding.
            performanceProfiler.Begin(RenderProfilePass::Presentation);
            BeginTextureMode(scenePresentationTarget);
            ClearBackground(BLANK);
            rlDisableColorBlend();
            BeginShaderMode(scenePresentationShader);
            DrawTexturePro(
                    linearSceneTexture,
                    GetFullscreenSrcRect(linearSceneTexture),
                    Rectangle{0.0f, 0.0f,
                            static_cast<float>(INTERNAL_WIDTH),
                            static_cast<float>(INTERNAL_HEIGHT)},
                    Vector2{}, 0.0f, WHITE);
            EndShaderMode();
            rlEnableColorBlend();
            EndTextureMode();
            performanceProfiler.End(RenderProfilePass::Presentation);
        } else if (application.ShouldRefreshBackground()
                && contentKind == game::ApplicationContentKind::Editor2D) {
            BeginTextureMode(editorTarget);
            ClearBackground(Color{8, 10, 14, 255});
            application.Render2D(assets);
            EndTextureMode();
        }

        // draw world and ui to screen
        performanceProfiler.Begin(RenderProfilePass::FinalComposite);
        BeginDrawing();
        {
            ClearBackground(BLACK);
            if (render3D) {
                const Texture2D& finalSceneTexture =
                        scenePresentationTarget.texture;
                const Rectangle worldSrc = GetFullscreenSrcRect(finalSceneTexture);
                if (useWorldFxaa) {
                    const Vector2 texelSize{
                            1.0f / static_cast<float>(finalSceneTexture.width),
                            1.0f / static_cast<float>(finalSceneTexture.height)};
                    SetShaderValue(fxaaShader, fxaaTexelSizeLoc, &texelSize,
                            SHADER_UNIFORM_VEC2);
                    BeginShaderMode(fxaaShader);
                }
                DrawTexturePro(finalSceneTexture, worldSrc, dst, {0,0}, 0.0f, WHITE);
                if (useWorldFxaa) {
                    EndShaderMode();
                }
            } else if (contentKind == game::ApplicationContentKind::Editor2D) {
                Rectangle editorSrc = GetFullscreenSrcRect(editorTarget.texture);
                DrawTexturePro(editorTarget.texture, editorSrc, dst, {0,0}, 0.0f, WHITE);
            }
            if (render3D) {
                application.Render3DHud(assets, smallFont, dst);
            }
            Rectangle uiSrc = GetFullscreenSrcRect(uiTarget.texture);
            DrawTexturePro(uiTarget.texture, uiSrc, dst, {0,0}, 0.0f, WHITE);
            DrawFPS(10, 10);
            if (application.ApplicationSettings().graphics.performanceOverlay) {
                performanceProfiler.Draw(
                        currentWorldRenderScale,
                        useWorldFxaa,
                        application.AtmosphereDiagnostics());
            }
            if (application.IsMenuOpen()) {
                Rectangle menuSrc = GetFullscreenSrcRect(menuTarget.texture);
                DrawTexturePro(menuTarget.texture, menuSrc, dst, {0,0}, 0.0f, WHITE);
            }
            Rectangle consoleSrc = GetFullscreenSrcRect(consoleTarget.texture);
            DrawTexturePro(
                    consoleTarget.texture,
                    consoleSrc,
                    dst,
                    {0,0},
                    0.0f,
                    WHITE);
            application.RenderLoadingOverlay(dst, screenW, screenH);
        }
        EndDrawing();
        performanceProfiler.End(RenderProfilePass::FinalComposite);
        frameDipTrace.Record(
                frameStartSeconds,
                preRenderEndSeconds,
                performanceProfiler,
                application.AtmosphereDiagnostics());
    }

    frameDipTrace.Shutdown();
    performanceProfiler.Shutdown();
    application.Shutdown(context);
    unloadRenderResources();
    context.audio.StopAll(context.assets);
    context.assets.Shutdown();
    context.audio.Shutdown();
    //ShowCursor();
    CloseWindow();
    return 0;
}
