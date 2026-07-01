#include <raylib.h>

#include "engine/EngineContext.h"
#include "engine/assets/FontLoadFlags.h"
#include "engine/render/FxaaShader.h"
#include "sector_editor/SectorEditor.h"

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

    RenderTexture2D worldTarget = LoadRenderTexture(WORLD_TARGET_WIDTH, WORLD_TARGET_HEIGHT);
    SetTextureFilter(worldTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D editorTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(editorTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D uiTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(uiTarget.texture, TEXTURE_FILTER_BILINEAR);

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
        UnloadRenderTexture(editorTarget);
        UnloadRenderTexture(uiTarget);
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

    engine::UIContext ui;
    engine::UIConfig uiConfig;
    game::SectorEditor sectorEditor;
    sectorEditor.Init(context);

    while (!WindowShouldClose())
    {
        assets.UpdateMainThread(2.0f);

        const float dt = GetFrameTime();
        const int screenW = GetScreenWidth();
        const int screenH = GetScreenHeight();

        const Rectangle dst = BuildPresentationRect(
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

        BeginTextureMode(uiTarget);
        ClearBackground(BLANK);
        sectorEditor.RenderUI(
                ui,
                uiConfig,
                context.input,
                assets,
                uiFont,
                smallFont
        );
        EndTextureMode();

        sectorEditor.Update(context, dt);

        const bool renderPreview3D = sectorEditor.IsPreview3DActive();
        if (renderPreview3D) {
            sectorEditor.RenderPreview3DShadowMaps(assets);

            BeginTextureMode(worldTarget);
            ClearBackground(Color{8, 10, 14, 255});
            sectorEditor.RenderPreview3DScene(context);
            EndTextureMode();

            sectorEditor.ApplyPreview3DBloom(assets, worldTarget);

            BeginTextureMode(worldTarget);
            sectorEditor.RenderPreview3DOverlays();
            EndTextureMode();
        } else {
            BeginTextureMode(editorTarget);
            ClearBackground(Color{8, 10, 14, 255});
            sectorEditor.Render(assets);
            EndTextureMode();
        }

        // draw world and ui to screen
        BeginDrawing();
        {
            ClearBackground(BLACK);
            if (renderPreview3D) {
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
            } else {
                Rectangle editorSrc = GetFullscreenSrcRect(editorTarget.texture);
                DrawTexturePro(editorTarget.texture, editorSrc, dst, {0,0}, 0.0f, WHITE);
            }
            Rectangle uiSrc = GetFullscreenSrcRect(uiTarget.texture);
            DrawTexturePro(uiTarget.texture, uiSrc, dst, {0,0}, 0.0f, WHITE);
            DrawFPS(10, 10);
        }
        EndDrawing();
    }

    sectorEditor.Shutdown(context);
    unloadRenderResources();
    context.assets.Shutdown();
    //ShowCursor();
    CloseWindow();
    return 0;
}
