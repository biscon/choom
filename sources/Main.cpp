#include <raylib.h>
#include <external/glad.h>
#include <rlgl.h>

#include "engine/EngineContext.h"
#include "engine/assets/FontLoadFlags.h"
#include "engine/render/ColorTransfer.h"
#include "engine/render/FxaaShader.h"
#include "engine/render/RenderColorDiagnostics.h"
#include "engine/render/RenderTarget.h"
#include "engine/render/ScenePresentationShader.h"
#include "game/GameApplication.h"

#include <cmath>
#include <string>

static constexpr int INTERNAL_WIDTH = 1920;
static constexpr int INTERNAL_HEIGHT = 1080;
static constexpr float WORLD_RENDER_SCALE = 1.5f;
static constexpr bool ENABLE_WORLD_FXAA = true;
static constexpr int WORLD_TARGET_WIDTH = static_cast<int>((INTERNAL_WIDTH * WORLD_RENDER_SCALE) + 0.5f);
static constexpr int WORLD_TARGET_HEIGHT = static_cast<int>((INTERNAL_HEIGHT * WORLD_RENDER_SCALE) + 0.5f);

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

int main()
{
    unsigned int flags = 0;
    flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);

    InitWindow(STARTUP_WINDOW_WIDTH, STARTUP_WINDOW_HEIGHT, "Engine");
    // Presentation is encoded explicitly by the global presentation shader.
    glDisable(GL_FRAMEBUFFER_SRGB);
    //HideCursor();

    SetExitKey(0);

    engine::RenderTarget worldTargetResource;
    std::string renderTargetError;
    if (!engine::LoadRenderTarget(
                engine::RenderTargetDescriptor{
                        "world",
                        WORLD_TARGET_WIDTH,
                        WORLD_TARGET_HEIGHT,
                        engine::RenderTargetColorFormat::Rgba16Float,
                        engine::RenderTargetFilter::Bilinear,
                        engine::RenderTargetWrap::Repeat,
                        engine::RenderTargetDepthKind::SampleableTexture,
                        1},
                worldTargetResource,
                &renderTargetError)) {
        TraceLog(LOG_WARNING, "PREVIEW: sampleable depth target unavailable; local fog disabled");
        engine::LoadRenderTarget(
                engine::RenderTargetDescriptor{
                        "world",
                        WORLD_TARGET_WIDTH,
                        WORLD_TARGET_HEIGHT,
                        engine::RenderTargetColorFormat::Rgba16Float,
                        engine::RenderTargetFilter::Bilinear,
                        engine::RenderTargetWrap::Repeat,
                        engine::RenderTargetDepthKind::Renderbuffer,
                        1},
                worldTargetResource,
                &renderTargetError);
        if (engine::IsRenderTargetReady(worldTargetResource)) {
            worldTargetResource.native.depth.mipmaps = 0;
        }
    }
    RenderTexture2D& worldTarget = engine::NativeRenderTexture(worldTargetResource);
    if (!engine::IsRenderTargetReady(worldTargetResource)) {
        TraceLog(LOG_ERROR, "RENDER: required RGBA16F world target unavailable: %s", renderTargetError.c_str());
        CloseWindow();
        return 1;
    }

    // Viewmodels use a separate depth buffer so world geometry cannot clip them.
    engine::RenderTarget viewmodelTargetResource;
    engine::LoadRenderTarget(
            engine::RenderTargetDescriptor{
                    "viewmodel",
                    WORLD_TARGET_WIDTH,
                    WORLD_TARGET_HEIGHT,
                    engine::RenderTargetColorFormat::Rgba16Float,
                    engine::RenderTargetFilter::Bilinear,
                    engine::RenderTargetWrap::Repeat,
                    engine::RenderTargetDepthKind::Renderbuffer,
                    1},
            viewmodelTargetResource,
            &renderTargetError);
    RenderTexture2D& viewmodelTarget = engine::NativeRenderTexture(viewmodelTargetResource);
    const bool viewmodelTargetReady = engine::IsRenderTargetReady(viewmodelTargetResource);
    if (!viewmodelTargetReady) {
        TraceLog(LOG_ERROR, "RENDER: required RGBA16F viewmodel target unavailable: %s", renderTargetError.c_str());
        engine::UnloadRenderTarget(worldTargetResource);
        CloseWindow();
        return 1;
    }

    engine::RenderTarget sceneResolveTargetResource;
    engine::LoadRenderTarget(
            engine::RenderTargetDescriptor{
                    "scene-linear-resolve",
                    INTERNAL_WIDTH,
                    INTERNAL_HEIGHT,
                    engine::RenderTargetColorFormat::Rgba16Float,
                    engine::RenderTargetFilter::Bilinear,
                    engine::RenderTargetWrap::Clamp,
                    engine::RenderTargetDepthKind::None,
                    1},
            sceneResolveTargetResource,
            &renderTargetError);
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
    if (!engine::IsRenderTargetReady(sceneResolveTargetResource)
            || !engine::IsRenderTargetReady(scenePresentationTargetResource)) {
        TraceLog(LOG_ERROR, "RENDER: required scene presentation targets unavailable: %s", renderTargetError.c_str());
        engine::UnloadRenderTarget(worldTargetResource);
        engine::UnloadRenderTarget(viewmodelTargetResource);
        engine::UnloadRenderTarget(sceneResolveTargetResource);
        engine::UnloadRenderTarget(scenePresentationTargetResource);
        CloseWindow();
        return 1;
    }
    RenderTexture2D& sceneResolveTarget =
            engine::NativeRenderTexture(sceneResolveTargetResource);
    RenderTexture2D& scenePresentationTarget =
            engine::NativeRenderTexture(scenePresentationTargetResource);

    engine::RenderTarget sceneFxaaTargetResource;
    if (!engine::LoadRenderTarget(
                engine::RenderTargetDescriptor{
                        "scene-fxaa",
                        INTERNAL_WIDTH,
                        INTERNAL_HEIGHT,
                        engine::RenderTargetColorFormat::Rgba8Unorm,
                        engine::RenderTargetFilter::Bilinear,
                        engine::RenderTargetWrap::Clamp,
                        engine::RenderTargetDepthKind::None,
                        1},
                sceneFxaaTargetResource,
                &renderTargetError)) {
        TraceLog(LOG_WARNING, "RENDER: FXAA output target unavailable; FXAA disabled: %s",
                renderTargetError.c_str());
    }
    RenderTexture2D& sceneFxaaTarget =
            engine::NativeRenderTexture(sceneFxaaTargetResource);

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
    loadDisplayTarget("editor-2d", editorTargetResource);
    loadDisplayTarget("ui", uiTargetResource);
    loadDisplayTarget("menu", menuTargetResource);
    RenderTexture2D& editorTarget = engine::NativeRenderTexture(editorTargetResource);
    RenderTexture2D& uiTarget = engine::NativeRenderTexture(uiTargetResource);
    RenderTexture2D& menuTarget = engine::NativeRenderTexture(menuTargetResource);

    Shader fxaaShader{};
    int fxaaTexelSizeLoc = -1;
    if (ENABLE_WORLD_FXAA) {
        fxaaShader = LoadShaderFromMemory(nullptr, engine::FxaaFragmentShader);
        fxaaTexelSizeLoc = GetShaderLocation(fxaaShader, "texelSize");
    }
    const bool useWorldFxaa = ENABLE_WORLD_FXAA
            && IsShaderValid(fxaaShader)
            && engine::IsRenderTargetReady(sceneFxaaTargetResource);
    const std::string scenePresentationFragmentShader =
            engine::BuildScenePresentationFragmentShader();
    Shader scenePresentationShader = LoadShaderFromMemory(
            nullptr,
            scenePresentationFragmentShader.c_str());
    if (!IsShaderValid(scenePresentationShader)) {
        TraceLog(LOG_ERROR, "RENDER: required neutral tone-map/sRGB presentation shader unavailable");
        if (IsShaderValid(fxaaShader)) UnloadShader(fxaaShader);
        engine::UnloadRenderTarget(worldTargetResource);
        engine::UnloadRenderTarget(viewmodelTargetResource);
        engine::UnloadRenderTarget(sceneResolveTargetResource);
        engine::UnloadRenderTarget(scenePresentationTargetResource);
        engine::UnloadRenderTarget(sceneFxaaTargetResource);
        engine::UnloadRenderTarget(editorTargetResource);
        engine::UnloadRenderTarget(uiTargetResource);
        engine::UnloadRenderTarget(menuTargetResource);
        CloseWindow();
        return 1;
    }
    engine::LogColorPipelineDiagnostics(engine::ColorPipelineRuntimeState{
            WORLD_RENDER_SCALE,
            ENABLE_WORLD_FXAA,
            useWorldFxaa});

    const auto unloadRenderResources = [&]() {
        if (IsShaderValid(fxaaShader)) {
            UnloadShader(fxaaShader);
        }
        if (IsShaderValid(scenePresentationShader)) {
            UnloadShader(scenePresentationShader);
        }
        engine::UnloadRenderTarget(worldTargetResource);
        engine::UnloadRenderTarget(viewmodelTargetResource);
        engine::UnloadRenderTarget(sceneResolveTargetResource);
        engine::UnloadRenderTarget(scenePresentationTargetResource);
        engine::UnloadRenderTarget(sceneFxaaTargetResource);
        engine::UnloadRenderTarget(editorTargetResource);
        engine::UnloadRenderTarget(uiTargetResource);
        engine::UnloadRenderTarget(menuTargetResource);
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
    if (!application.Init(context)) {
        TraceLog(LOG_ERROR, "Engine application initialization failed");
        unloadRenderResources();
        context.assets.Shutdown();
        context.audio.Shutdown();
        CloseWindow();
        return 1;
    }

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
    BeginTextureMode(sceneResolveTarget);
    ClearBackground(BLACK);
    EndTextureMode();
    BeginTextureMode(scenePresentationTarget);
    ClearBackground(BLACK);
    EndTextureMode();
    if (engine::IsRenderTargetReady(sceneFxaaTargetResource)) {
        BeginTextureMode(sceneFxaaTarget);
        ClearBackground(BLACK);
        EndTextureMode();
    }

    while (!WindowShouldClose() && !application.QuitRequested())
    {
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
        context.audio.Update(assets);

        const game::ApplicationContentKind contentKind =
                application.BackgroundContentKind();
        const bool render3D =
                contentKind == game::ApplicationContentKind::Sector3D;
        if (application.ShouldRefreshBackground() && render3D) {
            application.Render3DShadowMaps(context);

            BeginTextureMode(worldTarget);
            ClearLinearSceneBackground(Color{8, 10, 14, 255});
            application.Render3DScene(context);
            EndTextureMode();

            application.Apply3DPostProcessing(assets, worldTarget);

            if (viewmodelTargetReady) {
                BeginTextureMode(viewmodelTarget);
                ClearBackground(BLANK);
                application.Render3DViewmodel(assets);
                EndTextureMode();

                BeginTextureMode(worldTarget);
                DrawTexturePro(
                        viewmodelTarget.texture,
                        GetFullscreenSrcRect(viewmodelTarget.texture),
                        Rectangle{0.0f, 0.0f, static_cast<float>(WORLD_TARGET_WIDTH), static_cast<float>(WORLD_TARGET_HEIGHT)},
                        Vector2{}, 0.0f, WHITE);
                EndTextureMode();
            }

            BeginTextureMode(worldTarget);
            application.Render3DOverlays();
            EndTextureMode();

            // Resolve deliberate 1.5x supersampling while the scene is still
            // linear HDR, then perform the single global display transform.
            BeginTextureMode(sceneResolveTarget);
            ClearBackground(BLANK);
            rlDisableColorBlend();
            DrawTexturePro(
                    worldTarget.texture,
                    GetFullscreenSrcRect(worldTarget.texture),
                    Rectangle{0.0f, 0.0f,
                            static_cast<float>(INTERNAL_WIDTH),
                            static_cast<float>(INTERNAL_HEIGHT)},
                    Vector2{}, 0.0f, WHITE);
            rlEnableColorBlend();
            EndTextureMode();

            BeginTextureMode(scenePresentationTarget);
            ClearBackground(BLANK);
            rlDisableColorBlend();
            BeginShaderMode(scenePresentationShader);
            DrawTexturePro(
                    sceneResolveTarget.texture,
                    GetFullscreenSrcRect(sceneResolveTarget.texture),
                    Rectangle{0.0f, 0.0f,
                            static_cast<float>(INTERNAL_WIDTH),
                            static_cast<float>(INTERNAL_HEIGHT)},
                    Vector2{}, 0.0f, WHITE);
            EndShaderMode();
            rlEnableColorBlend();
            EndTextureMode();

            if (useWorldFxaa) {
                const Vector2 texelSize{
                        1.0f / static_cast<float>(scenePresentationTarget.texture.width),
                        1.0f / static_cast<float>(scenePresentationTarget.texture.height)};
                BeginTextureMode(sceneFxaaTarget);
                ClearBackground(BLANK);
                rlDisableColorBlend();
                SetShaderValue(
                        fxaaShader,
                        fxaaTexelSizeLoc,
                        &texelSize,
                        SHADER_UNIFORM_VEC2);
                BeginShaderMode(fxaaShader);
                DrawTexturePro(
                        scenePresentationTarget.texture,
                        GetFullscreenSrcRect(scenePresentationTarget.texture),
                        Rectangle{0.0f, 0.0f,
                                static_cast<float>(INTERNAL_WIDTH),
                                static_cast<float>(INTERNAL_HEIGHT)},
                        Vector2{}, 0.0f, WHITE);
                EndShaderMode();
                rlEnableColorBlend();
                EndTextureMode();
            }
        } else if (application.ShouldRefreshBackground()
                && contentKind == game::ApplicationContentKind::Editor2D) {
            BeginTextureMode(editorTarget);
            ClearBackground(Color{8, 10, 14, 255});
            application.Render2D(assets);
            EndTextureMode();
        }

        // draw world and ui to screen
        BeginDrawing();
        {
            ClearBackground(BLACK);
            if (render3D) {
                const Texture2D& finalSceneTexture = useWorldFxaa
                        ? sceneFxaaTarget.texture
                        : scenePresentationTarget.texture;
                const Rectangle worldSrc = GetFullscreenSrcRect(finalSceneTexture);
                DrawTexturePro(finalSceneTexture, worldSrc, dst, {0,0}, 0.0f, WHITE);
            } else if (contentKind == game::ApplicationContentKind::Editor2D) {
                Rectangle editorSrc = GetFullscreenSrcRect(editorTarget.texture);
                DrawTexturePro(editorTarget.texture, editorSrc, dst, {0,0}, 0.0f, WHITE);
            }
            if (render3D) {
                application.Render3DHud(dst);
            }
            Rectangle uiSrc = GetFullscreenSrcRect(uiTarget.texture);
            DrawTexturePro(uiTarget.texture, uiSrc, dst, {0,0}, 0.0f, WHITE);
            DrawFPS(10, 10);
            if (application.IsMenuOpen()) {
                Rectangle menuSrc = GetFullscreenSrcRect(menuTarget.texture);
                DrawTexturePro(menuTarget.texture, menuSrc, dst, {0,0}, 0.0f, WHITE);
            }
        }
        EndDrawing();
    }

    application.Shutdown(context);
    unloadRenderResources();
    context.audio.StopAll(context.assets);
    context.assets.Shutdown();
    context.audio.Shutdown();
    //ShowCursor();
    CloseWindow();
    return 0;
}
