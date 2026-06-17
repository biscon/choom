# Asset System

The engine asset system is owned by `engine::AssetManager`. Most CPU-side asset
data is loaded on a worker thread, GPU resources are finalized on the main
thread, and lightweight handles can be stored in ECS components.

ECS components should store handles such as `TextureHandle`,
`SpriteAnimationHandle`, and `FontHandle`, not raylib objects such as
`Texture2D` or `Font`. Systems receive `AssetManager&` explicitly when they need
asset data.

## Core Rules

- Call `AssetManager::Initialize()` after the raylib window/context exists.
- Call `AssetManager::UpdateMainThread()` once per frame on the main thread.
- Call `AssetManager::Shutdown()` before `CloseWindow()`.
- Request assets during initialization, loading screens, or explicit level load
  phases, not inside normal gameplay update/render loops.
- Treat handles as runtime handles only. They are not save IDs.
- Do not store pointers returned by `GetTexture()`, `GetSpriteAnimation()`, or
  `GetFont()`. Use them immediately and request them again next frame when
  needed.
- Missing, failed, or not-yet-ready assets must be handled without crashing.

## Initialization And Shutdown

`EngineContext` owns the engine services, including `AssetManager`.

```cpp
#include "engine/EngineContext.h"

int main()
{
    InitWindow(1280, 720, "Game");

    engine::EngineContext context;
    if (!context.assets.Initialize()) {
        CloseWindow();
        return 1;
    }

    while (!WindowShouldClose()) {
        context.assets.UpdateMainThread(2.0f);

        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
    }

    context.assets.Shutdown();
    CloseWindow();
    return 0;
}
```

`UpdateMainThread(maxMilliseconds)` processes completed worker requests and
performs GPU uploads and unloads. Passing a small budget, such as `2.0f`, keeps
uploads from monopolizing a frame. Passing `0.0f` processes all currently
completed work.

## Asset Scopes

Scopes group assets by lifetime. The asset manager creates a global scope during
initialization for assets that should live for the whole program. Level,
cutscene, or temporary assets should use their own scopes and be unloaded as a
group.

```cpp
engine::AssetManager& assets = context.assets;

engine::AssetScopeHandle levelScope = assets.CreateScope("level_01");

engine::TextureHandle playerTexture = assets.RequestTexture(
        levelScope,
        "player",
        ASSETS_PATH "images/player.png",
        engine::TextureLoad_PointFilter
);

while (!WindowShouldClose() && !assets.IsScopeFinished(levelScope)) {
    assets.UpdateMainThread(2.0f);

    const float progress = assets.GetScopeProgress(levelScope);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawText(TextFormat("Loading %.0f%%", progress * 100.0f), 40, 40, 32, RAYWHITE);
    EndDrawing();
}

// Later, when leaving the level:
assets.UnloadScope(levelScope);
```

Requesting the same asset more than once in the same scope with the same key,
path, and load parameters returns the same handle.

## Textures

Use `RequestTexture()` to request a texture. The request returns immediately with
a `TextureHandle`.

```cpp
engine::TextureHandle bikerTexture = assets.RequestTexture(
        assets.GlobalScope(),
        "biker_chick",
        ASSETS_PATH "images/biker_chick.png",
        engine::TextureLoad_PointFilter
);
```

Texture state helpers:

- `IsReady(handle)` is true when the texture is uploaded and drawable.
- `IsFinished(handle)` is true when loading reached a terminal state: ready,
  failed, unloaded, or invalid.
- `HasFailed(handle)` is true when loading failed or the handle is invalid.
- `GetTexture(handle)` returns a `const Texture2D*` only when ready.

Safe rendering code should handle `nullptr`:

```cpp
const Texture2D* texture = assets.GetTexture(renderer.texture);
if (texture == nullptr) {
    DrawRectangleLinesEx(fallbackRect, 2.0f, MAGENTA);
    return;
}

DrawTexturePro(*texture, source, destination, origin, rotation, tint);
```

Texture load flags:

```cpp
engine::TextureLoadFlags flags =
        engine::TextureLoad_PointFilter |
        engine::TextureLoad_PremultiplyAlpha;
```

Use `TextureLoad_PointFilter` for pixel art. Use `TextureLoad_BilinearFilter`
when scaled textures should be smoothed.

## Fonts

Fonts are scoped assets owned by `AssetManager`. A font asset represents a
concrete font file loaded at a concrete pixel size, and stores that pixel size
with the loaded raylib `Font`.

```cpp
engine::FontHandle uiFont = assets.RequestFont(
        assets.GlobalScope(),
        "ibm_plex_bold_32",
        ASSETS_PATH "fonts/IBMPlexSans-Bold.ttf",
        32,
        engine::FontLoad_BilinearFilter
);
```

Duplicate font requests in the same scope with the same key, path, pixel size,
and flags return the same `FontHandle`.

Font loading currently happens on the main thread inside
`AssetManager::UpdateMainThread()` using raylib `LoadFontEx()`. This can hitch,
so request fonts during startup, loading screens, or explicit level load phases.
Font unloading also happens on the main thread and must complete before
`CloseWindow()`.

Use the stored pixel size when drawing to avoid accidental bitmap scaling:

```cpp
const engine::FontAsset* font = assets.GetFont(uiFont);
if (font != nullptr) {
    DrawTextEx(
            font->font,
            "Hello",
            Vector2{40.0f, 40.0f},
            static_cast<float>(font->pixelSize),
            0.0f,
            WHITE
    );
}
```

## Loading Workflows

There are two intended loading workflows.

The first workflow requests all required assets, shows a loading screen, then
spawns entities after loading finishes:

```cpp
engine::TextureHandle playerTexture = assets.RequestTexture(
        assets.GlobalScope(),
        "player",
        ASSETS_PATH "images/player.png",
        engine::TextureLoad_PointFilter
);

while (!WindowShouldClose() && !assets.IsFinished(playerTexture)) {
    assets.UpdateMainThread(2.0f);

    BeginDrawing();
    ClearBackground(BLACK);
    DrawText("Loading assets...", 40, 40, 32, RAYWHITE);
    EndDrawing();
}

if (!assets.HasFailed(playerTexture)) {
    engine::Entity entity = world.CreateEntity();
    world.Add(entity, engine::SpriteRenderer{
            playerTexture,
            Rectangle{},
            Vector2{128.0f, 128.0f},
            Vector2{64.0f, 64.0f},
            WHITE
    });
}
```

The second workflow requests assets and lets systems render placeholders until
they are ready. This is useful for streaming or non-blocking level setup.

```cpp
void SpriteRenderSystem(engine::World& world, engine::AssetManager& assets)
{
    world.ForEach<engine::Transform, engine::SpriteRenderer>(
            [&assets](engine::Entity, engine::Transform& transform, engine::SpriteRenderer& renderer) {
                if (!renderer.visible) {
                    return;
                }

                const Texture2D* texture = assets.GetTexture(renderer.texture);
                if (texture == nullptr) {
                    DrawRectangleLines(
                            static_cast<int>(transform.position.x),
                            static_cast<int>(transform.position.y),
                            static_cast<int>(renderer.size.x),
                            static_cast<int>(renderer.size.y),
                            MAGENTA
                    );
                    return;
                }

                DrawTexture(*texture,
                        static_cast<int>(transform.position.x),
                        static_cast<int>(transform.position.y),
                        renderer.tint);
            }
    );
}
```

## Sprite Animations

Aseprite sprite animations are assets. The animation asset owns parsed frame and
clip metadata and references its atlas with a `TextureHandle`. ECS components
store only `SpriteAnimationHandle`, clip index, playback state, and render state.

Request an animation from an Aseprite JSON file:

```cpp
engine::SpriteAnimationHandle zombieAnimation = assets.RequestSpriteAnimation(
        assets.GlobalScope(),
        "zombie",
        ASSETS_PATH "sprites/zombie.json",
        engine::TextureLoad_PointFilter
);
```

The JSON parser expects:

- A `frames` object.
- `meta.image` pointing to the atlas image.
- Optional `meta.frameTags` for named clips.

If frame tags are missing, the parser attempts to derive clip names from frame
names containing `#ClipName`. If no clips can be derived, it creates a single
`Default` clip.

Resolve clip names after the animation has loaded, then store clip indices in
components:

```cpp
while (!WindowShouldClose() && !assets.IsFinished(zombieAnimation)) {
    assets.UpdateMainThread(2.0f);
}

if (!assets.HasFailed(zombieAnimation)) {
    const uint32_t idleClip = assets.FindSpriteClipIndex(zombieAnimation, "Idle");
    const uint32_t walkClip = assets.FindSpriteClipIndex(zombieAnimation, "Walk");

    engine::Entity zombie = world.CreateEntity();
    world.Add(zombie, engine::SpriteRenderer{
            engine::NullTextureHandle(),
            Rectangle{},
            Vector2{160.0f, 160.0f},
            Vector2{80.0f, 80.0f},
            WHITE
    });

    engine::SpriteAnimator animator;
    animator.animation = zombieAnimation;
    engine::SetSpriteClip(
            animator,
            idleClip == engine::InvalidSpriteClipIndex ? 0 : idleClip,
            true
    );
    world.Add(zombie, animator);
}
```

`SpriteAnimatorSystem` reads the animation asset through `AssetManager`, advances
playback, and writes the current atlas texture and source rectangle into
`SpriteRenderer`. `SpriteRenderSystem` then draws `SpriteRenderer`.

Systems should skip or hide sprites when the animation is not ready:

```cpp
const engine::SpriteAnimationAsset* asset = assets.GetSpriteAnimation(animator.animation);
if (asset == nullptr) {
    return;
}
```

## Threading Notes

Most public `AssetManager` API calls are intended for the main thread. The
internal worker thread may perform file IO, JSON parsing, image loading, and
CPU-side preprocessing. It must not create, mutate, or destroy GPU resources.
Fonts are currently an exception to worker-side loading: `LoadFontEx()` runs on
the main thread because it uploads the font atlas internally.

GPU upload happens in `AssetManager::UpdateMainThread()`. GPU unload, including
font unload, happens in `UpdateMainThread()` or `Shutdown()`. Always shut down
the asset manager before closing the raylib window/context.

## Failure Handling

Asset loading failures print `[AssetManager WARNING]` messages to the console.
Use `HasFailed()` when deciding whether to spawn entities that require a
specific asset.

Render and animation systems should still be defensive. A handle can be invalid,
unloaded, failed, or not ready yet, so `GetTexture()`, `GetSpriteAnimation()`,
and `GetFont()` may return `nullptr`.
