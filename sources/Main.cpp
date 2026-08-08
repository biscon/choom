#include <raylib.h>

#include "engine/EngineContext.h"
#include "engine/assets/FontLoadFlags.h"
#include "engine/render/FxaaShader.h"
#include "game/GameApplication.h"
#include "sector_demo/renderer/SectorLocalFogRenderer.h"

#include <cmath>

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

int main()
{
    unsigned int flags = 0;
    flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);

    InitWindow(STARTUP_WINDOW_WIDTH, STARTUP_WINDOW_HEIGHT, "Engine");
    //HideCursor();

    SetExitKey(0);

    RenderTexture2D worldTarget = game::LoadSectorDepthTextureRenderTarget(
            WORLD_TARGET_WIDTH,
            WORLD_TARGET_HEIGHT);
    if (worldTarget.id == 0) {
        TraceLog(LOG_WARNING, "PREVIEW: sampleable depth target unavailable; local fog disabled");
        worldTarget = LoadRenderTexture(WORLD_TARGET_WIDTH, WORLD_TARGET_HEIGHT);
        worldTarget.depth.mipmaps = 0;
    }
    SetTextureFilter(worldTarget.texture, TEXTURE_FILTER_BILINEAR);

    // Viewmodels use a separate depth buffer so world geometry cannot clip them.
    RenderTexture2D viewmodelTarget = LoadRenderTexture(WORLD_TARGET_WIDTH, WORLD_TARGET_HEIGHT);
    const bool viewmodelTargetReady = viewmodelTarget.id != 0;
    if (viewmodelTargetReady) {
        SetTextureFilter(viewmodelTarget.texture, TEXTURE_FILTER_BILINEAR);
    } else {
        TraceLog(LOG_WARNING, "PREVIEW: FPS viewmodel render target unavailable; viewmodel rendering disabled");
    }

    RenderTexture2D editorTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(editorTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D uiTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(uiTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D menuTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(menuTarget.texture, TEXTURE_FILTER_BILINEAR);

    Shader fxaaShader{};
    int fxaaTexelSizeLoc = -1;
    if (ENABLE_WORLD_FXAA) {
        fxaaShader = LoadShaderFromMemory(nullptr, engine::FxaaFragmentShader);
        fxaaTexelSizeLoc = GetShaderLocation(fxaaShader, "texelSize");
    }
    const bool useWorldFxaa = ENABLE_WORLD_FXAA && IsShaderValid(fxaaShader);

    const auto unloadRenderResources = [&]() {
        if (IsShaderValid(fxaaShader)) {
            UnloadShader(fxaaShader);
        }
        UnloadRenderTexture(worldTarget);
        UnloadRenderTexture(viewmodelTarget);
        UnloadRenderTexture(editorTarget);
        UnloadRenderTexture(uiTarget);
        UnloadRenderTexture(menuTarget);
    };

    engine::EngineContext context;
    context.input.Initialize();
    context.input.ReserveEvents(256);

    if (!context.assets.Initialize()) {
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
        CloseWindow();
        return 1;
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

        const game::ApplicationContentKind contentKind =
                application.BackgroundContentKind();
        const bool render3D =
                contentKind == game::ApplicationContentKind::Sector3D;
        if (application.ShouldRefreshBackground() && render3D) {
            application.Render3DShadowMaps(context);

            BeginTextureMode(worldTarget);
            ClearBackground(Color{8, 10, 14, 255});
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
                Rectangle worldSrc = GetFullscreenSrcRect(worldTarget.texture);
                if (useWorldFxaa) {
                    Vector2 texelSize{
                            1.0f / static_cast<float>(worldTarget.texture.width),
                            1.0f / static_cast<float>(worldTarget.texture.height)
                    };
                    SetShaderValue(fxaaShader, fxaaTexelSizeLoc, &texelSize, SHADER_UNIFORM_VEC2);
                    BeginShaderMode(fxaaShader);
                    DrawTexturePro(worldTarget.texture, worldSrc, dst, {0,0}, 0.0f, WHITE);
                    EndShaderMode();
                } else {
                    DrawTexturePro(worldTarget.texture, worldSrc, dst, {0,0}, 0.0f, WHITE);
                }
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
    context.assets.Shutdown();
    //ShowCursor();
    CloseWindow();
    return 0;
}
