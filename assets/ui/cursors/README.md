# Game cursors

Generated with the imagegen skill's built-in tool on 2026-09-16.
Both original 1254 × 1254 RGBA PNGs are preserved without pixel edits.
The arrow is used by game UI; the pointing hand is reserved for future interaction
behavior. The editor retains native cursors.

Sprites are trimmed at draw time and shown at 40 logical pixels high in the
1920 × 1080 UI coordinate system. Hotspots are measured in original PNG pixels;
the arrow uses its upper-left outline tip and the hand its index fingertip.

| File | Source rectangle (x, y, width, height) | Hotspot (x, y) |
| --- | --- | --- |
| arrow.png | 256, 54, 727, 1139 | 264, 71 |
| interaction_hand.png | 210, 51, 819, 1120 | 536, 109 |

These values match the sprite constants in `sources/game/GameCursor.h`.
Textures use display-sRGB color usage and mipmapped trilinear filtering for
small cursor rendering. Assets are loaded into the global AssetManager scope.

## Arrow generation prompt

> Use case: stylized-concept. Asset type: game UI mouse cursor PNG. Generate one conventional northwest-pointing mouse arrow cursor, solid pure white fill with a bold clean pure black outline. Flat simple precise silhouette, long sharp upper-left tip, small notch and diagonal stem at lower right, legible when rendered only 40 pixels tall. Single icon centered on square canvas with narrow transparent margin, icon occupies about 85 percent of canvas height. Genuinely transparent background with alpha, including outside the silhouette. No shadow, glow, gradients, texture, text, labels, watermark, other icons or checkerboard background. Crisp antialiased edges.

## Hand generation prompt

> Use case: stylized-concept. Asset type: game UI interaction mouse cursor PNG, reserved for later use. Generate one conventional upright pointing hand cursor with extended index finger pointing upward, other fingers curled and thumb visible. Solid pure white fill with a bold clean pure black outline, minimal black internal finger separation lines. Flat simple precise silhouette, legible when rendered only 40 pixels tall. Single icon centered on square canvas with narrow transparent margin, icon occupies about 85 percent of canvas height. Genuinely transparent background with alpha, including outside the silhouette. No shadow, glow, gradients, texture, text, labels, watermark, other icons or checkerboard background. Crisp antialiased edges.

