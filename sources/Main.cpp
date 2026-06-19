#include <raylib.h>

#include "engine/EngineContext.h"
#include "engine/assets/FontLoadFlags.h"
#include "sector_editor/SectorEditor.h"

#include <cmath>

static constexpr int INTERNAL_WIDTH = 1920;
static constexpr int INTERNAL_HEIGHT = 1080;

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

    RenderTexture2D worldTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(worldTarget.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D uiTarget = LoadRenderTexture(INTERNAL_WIDTH, INTERNAL_HEIGHT);
    SetTextureFilter(uiTarget.texture, TEXTURE_FILTER_BILINEAR);

    engine::EngineContext context;
    context.input.Initialize();
    context.input.ReserveEvents(256);

    if (!context.assets.Initialize()) {
        UnloadRenderTexture(worldTarget);
        UnloadRenderTexture(uiTarget);
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

    while (!WindowShouldClose()
            && !assets.IsFinished(uiFont)) {
        assets.UpdateMainThread(2.0f);

        BeginDrawing();
        ClearBackground(BLACK);
        DrawText("Loading assets...", 40, 40, 32, RAYWHITE);
        EndDrawing();
    }

    if (WindowShouldClose()) {
        UnloadRenderTexture(worldTarget);
        UnloadRenderTexture(uiTarget);
        context.assets.Shutdown();
        CloseWindow();
        return 0;
    }

    engine::UIContext ui;
    engine::UIConfig uiConfig;
    game::SectorEditor sectorEditor;
    sectorEditor.Init(assets);

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
                uiFont
        );
        EndTextureMode();

        sectorEditor.Update(context.input, dt);

        BeginTextureMode(worldTarget);
        ClearBackground(Color{8, 10, 14, 255});
        sectorEditor.Render(assets);
        EndTextureMode();

        // draw world and ui to screen
        BeginDrawing();
        {
            ClearBackground(BLACK);
            Rectangle worldSrc = GetFullscreenSrcRect(worldTarget.texture);
            DrawTexturePro(worldTarget.texture, worldSrc, dst, {0,0}, 0.0f, WHITE);
            Rectangle uiSrc = GetFullscreenSrcRect(uiTarget.texture);
            DrawTexturePro(uiTarget.texture, uiSrc, dst, {0,0}, 0.0f, WHITE);
            DrawFPS(10, 10);
        }
        EndDrawing();
    }

    sectorEditor.Shutdown(assets);
    UnloadRenderTexture(worldTarget);
    UnloadRenderTexture(uiTarget);
    context.assets.Shutdown();
    //ShowCursor();
    CloseWindow();
    return 0;
}
