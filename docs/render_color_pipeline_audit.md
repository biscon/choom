# Render Color Pipeline Audit

Audit date: 2026-08-10

Scope: static inspection of the C++17/raylib 6/OpenGL 3.3 renderer. No engine executable was launched and no screenshots were collected. “Proven” below means directly established from the checked-in project and the raylib 6 source fetched in `cmake-build-debug/_deps/raylib-src`; “inference” identifies a conclusion that follows from those facts but still deserves a runtime query; “unknown” identifies state that cannot be guaranteed from source alone.

## Executive Summary

1. **Is the renderer using a coherent linear/sRGB pipeline?** No. This is a proven mixed pipeline. raylib uploads ordinary 8-bit images as linear-format `GL_RGB8`/`GL_RGBA8`, so sampling does not decode sRGB. Sector geometry, doors, billboards, decals, sky, fog, haze, dust, bloom, UI, and most authored `Color` values operate on those raw normalized bytes. The glTF/model shader is materially different: it explicitly decodes base-color, emissive, and environment samples to linear, performs PBR lighting, applies a local ACES-like curve, and encodes to sRGB. Both paths are then stored together in the same `GL_RGBA8` world target. There is no common final transfer conversion.

2. **Is tone mapping present and meaningful?** Only locally. `SectorStaticModelFs` contains an ACES fitted curve and applies it to static models, dynamic models, and viewmodels before those fragments enter the LDR scene target. It receives temporarily unclipped shader values, so it has a real effect on those fragments. It is not scene-wide HDR-to-SDR tone mapping: sector geometry and effects bypass it, the scene target is already 8-bit LDR, and model fog is applied after the curve and after sRGB encoding.

3. **Can the same scene produce different framebuffer values across platforms?** The engine does not guarantee identical final framebuffer bytes. It neither requests nor queries an sRGB-capable default framebuffer, bit depths, or attachment encoding; it never queries `GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING`; and it never controls `GL_FRAMEBUFFER_SRGB`. Startup drawable sizes differ on macOS, final filtering occurs at drawable resolution, and GPU raster/filter precision may differ. That said, the fixed-size off-screen targets and deterministic shaders should usually produce very similar off-screen bytes for the same state. ICC profiles, monitor gamma/LUTs, True Tone/Night Shift, HDR desktop modes, and panel characteristics can make identical framebuffer bytes look different without changing screenshots.

4. **Which observed differences are likely engine-side versus display-side?** Both are plausible, but they explain different aspects. The central spotlight becoming pale/white is strongly explained by engine-side LDR clipping and the light-color convention: `{255,245,225}` becomes `{1.0,0.9608,0.8824}` and is used directly as light energy; sufficient intensity clips the three channels toward 1.0 in `RGBA8`, removing the tint. Treating those bytes as linear also makes the light less warm than decoding an sRGB-authored swatch would. The large machine-wide darkness/warmth difference, and the fact that changing a monitor gamma profile changed overall brightness, strongly implicate display/OS configuration as well. Static inspection cannot apportion the observed difference without comparing final framebuffer screenshots and runtime output-state logs.

5. **What is the highest-value first improvement?** First add a no-behavior-change diagnostic slice that logs actual target/default-framebuffer state and provides a deterministic SDR reference pattern/capture procedure. Then define the source-texture and light-color conventions before changing math. The highest-value rendering correction is a staged conversion to one linear scene pipeline with one final SDR encoding, but it should be undertaken only after reference captures exist.

6. **What should not be attempted yet?** Do not begin with native HDR, custom ICC conversion, per-monitor model tables, a filmic look, or a player gamma slider. Those can mask or complicate the current mixed transfer functions and LDR clipping. Native HDR and engine-owned ICC management are especially poor first steps for the present raylib/OpenGL 3.3 architecture.

## Current Pipeline Diagram

The main 3D path is:

```text
PNG/JPEG/glTF images
    | LoadImage / raylib glTF loader
    v
linear-format 8-bit GPU textures (GL_R8 / GL_RGB8 / GL_RGBA8)
    | no hardware sRGB decode
    +-------------------------------+--------------------------------+
    | sector/door/billboard path    | glTF model/viewmodel path       |
    | raw color samples             | explicit sRGB -> linear         |
    | raw Color/255 light colors    | PBR + raw Color/255 lights      |
    | raw lightmap RGB + AO alpha   | raw lightmap/probe lighting     |
    | fog in the same mixed space   | local ACES + linear -> sRGB     |
    | no tone map/output encoding   | then fog in encoded space       |
    +-------------------------------+--------------------------------+
                         |
                         v
             2880x1620 GL_RGBA8 world target
                         |
       LDR bloom -> local fog -> haze -> additive dust
       (all GL_RGBA8; values clip on every target write)
                         |
        GL_RGBA8 viewmodel target composited with alpha
        editor 3D overlays drawn into world target
                         |
             FXAA + bilinear/downsample in stored space
                         |
                         v
       unqueried/unrequested default window framebuffer
       no GL_FRAMEBUFFER_SRGB and no final encoding shader
                         |
         crosshair/HUD, UI target, FPS, menu target
         alpha-composited directly into default framebuffer
                         |
                         v
             OS compositor / display pipeline / monitor
```

The 2D editor path bypasses the world/postprocess chain:

```text
2D editor drawing -> 1920x1080 GL_RGBA8 editor target
                  -> bilinear presentation to default framebuffer
                  -> UI/menu composition -> OS/display
```

## Evidence and Trace

### Asset Color Spaces

#### The upload path

The project asset API has no color-space semantic. `TextureLoadFlags` contains only premultiplication, filtering, mipmap, and anisotropy flags (`sources/engine/assets/TextureLoadFlags.h:7-14`). File textures are decoded with `LoadImage()`, optionally premultiplied, and uploaded with `LoadTextureFromImage()` (`sources/engine/assets/TextureAssets.cpp:321-343`, `365-410`). Generated textures and cubemaps use the same raylib upload functions (`TextureAssets.cpp:87-103`, `138-174`).

In the pinned raylib 6 source, `LoadTextureFromImage()` passes the raylib image format to `rlLoadTexture()` (`cmake-build-debug/_deps/raylib-src/src/rtextures.c:4124-4141`). For OpenGL 3.3, `rlGetGlTextureFormats()` maps 8-bit RGB and RGBA to `GL_RGB8` and `GL_RGBA8`, not `GL_SRGB8` or `GL_SRGB8_ALPHA8` (`.../src/rlgl.h:3648-3661`). Therefore **ordinary raylib image loading does not perform GPU sRGB decoding in this build**. PNG/JPEG file encoding does not change that fact.

The absence of a semantic flag also means two requests for the same key/path/filter cannot intentionally select different color interpretations. Adding such a distinction later will need request-key/cache semantics as well as GPU/shader changes.

#### Category inventory

| Category | Expected content convention | Actual upload/sample behavior | Assessment |
|---|---|---|---|
| Sector albedo/diffuse PNG/JPEG | Strong inference: sRGB/display-encoded authored color | Uploaded as `GL_RGB8`/`GL_RGBA8`; sampled directly in `SectorLightmapFs` (`SectorMeshRenderer.cpp:270-276`) and `SectorDoorOpaqueFs` (`SectorDoorRenderer.cpp:246-250`) | Missing sRGB decode; lighting multiplies encoded-looking samples |
| Sector decals | Color texture; emissive and non-emissive variants use the same source | Same ordinary texture registry and upload (`SectorMeshRenderer.cpp:593-622`); direct sample/tint/mix (`SectorMeshRenderer.cpp:279-293`) | Missing decode; decal tint bytes also lack a declared convention |
| Sector sky | sRGB/display image and byte top-cap color | Default raylib material samples the ordinary texture; top cap uses a raylib `Color` (`SectorSkyRenderer.cpp:143-157`) | Correct only for a display-referred pass-through convention, not a linear scene |
| glTF base color texture | sRGB by glTF convention | Raylib uploads `RGBA8`, then `SectorStaticModelFs` explicitly calls `SrgbToLinear()` (`SectorStaticModelRenderer.cpp:178-184`, `339-344`) | Implemented correctly for this shader path |
| glTF base-color factor and vertex color | Linear factors by glTF convention | Parsed as floats/normalized bytes (`ModelAssets.cpp:107-118`; raylib `rmodels.c:5543-5547`) and multiplied without transfer conversion (`SectorStaticModelRenderer.cpp:342-344`) | Correct for conforming glTF; fallback/non-glTF material colors are less clearly defined |
| glTF emissive texture | sRGB by glTF convention | Explicit `SrgbToLinear()` (`SectorStaticModelRenderer.cpp:442-445`) | Implemented correctly for the model path |
| glTF emissive factor | Linear by glTF convention | Parsed directly (`ModelAssets.cpp:132-138`) and used directly (`SectorStaticModelRenderer.cpp:442-445`) | Implemented correctly |
| Normal maps | Non-color vector data | Ordinary linear-format upload; direct sample and remap from `[0,1]` to `[-1,1]` in sector and model shaders (`SectorMeshRenderer.cpp:178-206`; `SectorStaticModelRenderer.cpp:330-336`) | Correct; lack of semantic typing makes accidental future conversion possible |
| glTF metallic/roughness | Non-color scalar data; glTF B=metallic, G=roughness | raylib splits B and G into separate `GL_R8` textures (`raylib rmodels.c:5549-5583`); shader samples `.r` from each (`SectorStaticModelRenderer.cpp:345-350`) | Correct for supported metallic/roughness glTF |
| glTF occlusion | Non-color scalar data | Linear-format upload and raw `.r` sample (`SectorStaticModelRenderer.cpp:351-353`) | Correct |
| Specular/gloss maps | Non-color scalars plus color semantics defined by the extension | glTF specular/glossiness and newer extensions are explicitly recorded as unsupported (`ModelAssets.cpp:140-158`; raylib also notes unsupported material features at `rmodels.c:5634-5635`) | Not active rather than incorrectly decoded |
| Baked lightmap RGB | Project-defined normalized linear lighting coefficients | Float bake values are clamped and written directly to byte channels (`SectorLightmap.cpp:4595-4605`), exported as RGBA8 PNG (`4664-4679`), uploaded as `GL_RGBA8`, and sampled raw (`SectorMeshRenderer.cpp:295-300`) | Numerically consistent round trip, but undocumented/fragile and LDR-clipped |
| Baked AO in lightmap alpha | Linear non-color visibility factor | Written directly to alpha and sampled raw (`SectorLightmap.cpp:4476-4498`, `4600-4605`; `SectorMeshRenderer.cpp:295-300`) | Correct; sRGB hardware would not transform alpha either |
| PBR environment cubemap | Stored as sRGB bytes | Sky source bytes are sampled into a generated RGBA8 cubemap; mip averages explicitly decode-average-encode (`SectorPbrEnvironment.cpp:22-35`, `82-95`, `98-172`); shader explicitly decodes samples (`SectorStaticModelRenderer.cpp:424-436`) | Internally coherent for this path |
| Fog/haze colors | Authored byte colors; intended space undocumented | Divided by 255 and used directly (`SectorFog.cpp:29-47`; local fog/haze upload code) | Questionable/mixed, not linearized |
| Procedural fog/haze noise | Non-color procedural scalar data | GLSL value noise, not a texture (`SectorLocalFogRenderer.cpp:91`, `291-294`; `SectorLightHazeRenderer.cpp:100`, `197-198`) | Correct/non-color; no transfer function applies |
| UI/font/sprite textures and UI colors | Normally display-encoded UI assets/colors | Uploaded as ordinary RGBA8 and drawn through raylib’s raw multiply shader (`UI.cpp:293-315`, `2535-2557`; `SpriteRenderSystem.cpp:34-63`; raylib `rlgl.h:5059-5070`) | Reasonable only when composited after scene encoding; current UI targets/blending remain ambiguous and gamma-space |

### Lighting and Lightmaps

#### Dynamic and authored light colors

`ColorToUnitRgb()` is a straight byte division (`sources/sector_demo/SectorColor.h:20-25`). Dynamic point and spot lights use it (`SectorDynamicPointLightSelection.cpp:307-328`, `345-380`), and muzzle light construction repeats the same division (`FpsPlayerRuntime.cpp:477-498`). No light-color authoring document or transfer conversion was found.

The authored spotlight `{255,245,225}` consequently becomes approximately:

```text
raw byte normalization:     {1.0000, 0.9608, 0.8824}
if interpreted as sRGB and decoded: approximately {1.0000, 0.9131, 0.7529}
```

The current numeric light is therefore less chromatically warm—closer to white—than the same visible swatch decoded to linear light energy. This does not prove the author intended sRGB swatch semantics; it proves the convention is unspecified and the implementation uses raw normalized bytes.

All main dynamic-light shaders apply intensity, quadratic falloff, Lambert/cone terms, and optional shadow visibility to those values. Sector geometry accumulates them in `SectorLightmapFs` (`SectorMeshRenderer.cpp:301-339`); doors do the same (`SectorDoorRenderer.cpp:204-250`); the model shader uses the same light vectors as radiance in its PBR equations (`SectorStaticModelRenderer.cpp:356-410`). Spotlight shadow maps are depth-only textures (`SectorDynamicLightingRenderer.cpp:49-75`) and do not introduce a color-space conversion.

#### Sector geometry, ambient, AO, and fog

The sector shader directly multiplies a raw base-color sample by ambient, baked, and dynamic lighting. Ambient is vertex color times AO; baked RGB is sampled raw; static ambient+baked is clamped to 1; total lighting is clamped to 4; and fog is mixed afterward (`SectorMeshRenderer.cpp:270-341`). This is not coherent linear-light rendering because the base color is probably display encoded and the authored light/fog colors are raw normalized bytes.

The `clamp(..., 4.0)` does not create HDR storage. Shader registers can temporarily exceed 1.0, but writing the result to `GL_RGBA8` clips it. With a white surface and no other factors, multiplying `{1.0,0.9608,0.8824}` by a scalar above about `1.133` pushes every light channel to or above 1.0; the target then stores white. Surface albedo, ambient, bloom, and additional lights alter the exact threshold, but the hue-loss mechanism is proven.

Fog is not consistently placed. Sector/door/billboard output mixes the fog color with the already mixed-space lit result. The model shader first tone-maps and sRGB-encodes, then calls `ApplySectorFog()` (`SectorStaticModelRenderer.cpp:304-317`, `446-451`), so model fog is explicitly composited in encoded space. Volumetric fog/haze computes lighting-like values but writes them to RGBA8 and later mixes those stored values with scene RGBA8. None of these paths is a conventional linear-scattering composition.

#### Lightmap bake-to-PNG-to-runtime round trip

The bake uses float vectors for direct and indirect illumination. Point, spot, and directional colors are each converted with byte/255 and multiplied by intensity, falloff, Lambert, cone, and shadow terms (`SectorLightmap.cpp:1712-1762`, `1784-1848`, `2005-2047`). Ambient uses the same byte/255 convention (`2058-2069`). Indirect bounce samples the float direct-light buffer and applies a neutral bounce coefficient (`4500-4565`); it does not sample and linearize surface albedo.

At export, direct plus indirect RGB is hard-clamped to `[0,1]`, multiplied by 255, and written as an RGBA8 image; AO is written to alpha (`SectorLightmap.cpp:4595-4605`). raylib exports through stb's raw PNG writer (`raylib rtextures.c:619-648`), with no project color-profile or gamma metadata. At runtime the PNG is uploaded as `GL_RGBA8` (`SectorMeshRenderer.cpp:689-701`) and sampled raw.

Therefore the current round trip is **numerically self-consistent**: a baked coefficient of 0.5 becomes approximately byte 128 and returns approximately 0.502. There is neither an sRGB decode nor a double decode. The weaknesses are different:

- the numeric linear convention is not represented in asset metadata or texture API;
- generic image tools may interpret the PNG as display color;
- RGB is only 8-bit and clips all bake energy above 1.0;
- static and dynamic lights share the same questionable raw-byte color convention, but the surfaces they illuminate differ by render path;
- sector geometry clamps ambient plus baked direct to 1.0 before adding dynamic light;
- models locally tone-map while sector surfaces simply hit an LDR target clamp.

The source hash correctly includes bake constants/settings, directional lighting, geometry, `ceilingSky`, sector ambient, referenced texture/normal-map identity, static models, and static light data (`SectorLightmap.cpp:4926-5065`). Pure preview and sky-visual settings are not appended. This audit changes no lightmap code or hash behavior.

### Render Targets and Precision

raylib `LoadRenderTexture()` always creates an 8-bit `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` color texture in this build (`raylib rtextures.c:4249-4286`), which maps to `GL_RGBA8`. The project's custom sampleable-depth world target also explicitly creates `PIXELFORMAT_UNCOMPRESSED_R8G8B8A8` (`SectorLocalFogRenderer.cpp:393-414`). No project render target uses the float formats that raylib can map to `GL_RGBA16F`/`GL_RGBA32F` (`raylib rlgl.h:3656-3661`).

| Target | Size | Proven color format | Purpose/dynamic-range consequence |
|---|---:|---|---|
| Default/window framebuffer | Platform-selected drawable | Unknown; not requested or queried | Final storage/encoding cannot be guaranteed statically |
| `worldTarget` | 2880x1620 | `GL_RGBA8` | Entire world and postprocess result; first hard LDR boundary for scene fragments |
| `viewmodelTarget` | 2880x1620 | `GL_RGBA8` | Locally tone-mapped model plus additive muzzle flash; alpha-composited onto world |
| `editorTarget` | 1920x1080 | `GL_RGBA8` | 2D editor background/content |
| `uiTarget`, `menuTarget` | 1920x1080 each | `GL_RGBA8` | Transparent UI layers later alpha-composited to the window |
| Bloom scene copy | Full world size | `GL_RGBA8` | Copy of already-clipped scene |
| Bloom source/blur A/blur B | Quarter world dimensions (720x405) | `GL_RGBA8` | Emissive extraction and blur; each pass clips; composite clamps explicitly |
| Local-fog accumulation | 0.25x, 0.5x, or 1.0x world dimensions | `GL_RGBA8` | Quality-dependent LDR fog radiance/opacity |
| Local-fog composite | Full world size | `GL_RGBA8` | Scene/fog mix |
| Haze accumulation | 0.25x, 0.5x, or 1.0x world dimensions | `GL_RGBA8` | Quality-dependent LDR haze radiance/opacity |
| Haze composite | Full world size | `GL_RGBA8` | Scene/haze mix |
| Dust | Full world size | `GL_RGBA8` | Premultiplied additive radiance; additive writes clip |
| Spotlight/model shadow maps | Depth-only | No color attachment | Color management not applicable; depth precision is separate |
| AO | No separate screen-space target exists | AO is baked lightmap alpha | No SSAO framebuffer chain was found |

The world is supersampled at 1.5 times the 1920x1080 internal reference resolution (`Main.cpp:11-16`) and filtered into the drawable during presentation (`Main.cpp:338-358`). This improves spatial sampling but does not expand dynamic range. On macOS the initial drawable request is 1600x900 rather than 1920x1080 (`Main.cpp:18-24`), producing a different final scale.

Values above 1.0 exist only transiently in shader arithmetic. The sector, door, billboard, fog, haze, dust, and bloom paths all lose them on an RGBA8 target write. The model shader limits its linear sum to the configured dynamic-light clamp and then tone-maps before its RGBA8 write (`SectorStaticModelRenderer.cpp:446-451`). Bloom is expressly an LDR workaround: its source scales emissive intensity, all intermediates are RGBA8, and the composite clamps to `[0,1]` (`SectorBloomRenderer.cpp:96-157`, `275-366`, `433-441`).

### Postprocessing and Tone Mapping

The only transfer/tone functions found in active project shader code are:

- piecewise `SrgbToLinear()` and `LinearToSrgb()` in `SectorStaticModelFs` (`SectorStaticModelRenderer.cpp:178-193`);
- the common ACES fitted rational curve in the same shader (`195-202`);
- CPU sRGB conversions used only to build linear-filtered environment cubemap mips (`SectorPbrEnvironment.cpp:22-35`, `82-95`).

No Reinhard curve, extended Reinhard, scene-wide ACES pass, LUT, color grading, white-point mapping, highlight-desaturation control, photographic exposure, contrast, saturation, color-temperature control, or HDR-to-SDR output pass was found. Generic `pow()` calls used for animation/falloff/noise are not tone mapping.

The model ACES path is active for static models, animated/dynamic models, and viewmodels because all are drawn with `SectorStaticModelRenderer` (`SectorMeshRenderer.cpp:1081-1091`, `1144-1165`). It is meaningful locally: it consumes a float lighting sum that may be above 1.0. It is nevertheless questionable because:

- tone mapping is per material fragment, not a common scene operation;
- other opaque geometry and all post-effects bypass it;
- its result is encoded before entering the shared scene target;
- fog is applied after tone map/encoding;
- `environmentExposure` scales only environment specular, not total scene exposure (`SectorStaticModelRenderer.cpp:423-436`);
- `outputBrightnessMultiplier` is a model/viewmodel artistic multiplier before ACES, not a display setting (`446-450`).

Postprocessing order is proven by `SectorSceneRuntime::ApplyPostProcessing()`: LDR emissive-decal bloom first, then local fog; the renderer's local-fog wrapper subsequently applies haze and dust (`SectorSceneRuntime.cpp:337-350`; `SectorMeshRenderer.cpp:1196-1254`). The separate viewmodel is rendered and composited after all world postprocessing, and editor 3D overlays are drawn after that (`Main.cpp:309-328`).

### Final Output Encoding

There is no active occurrence of `GL_FRAMEBUFFER_SRGB`, `GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING`, `GL_SRGB8`, or an sRGB GLFW hint in project code. The symbols exist in bundled OpenGL/GLFW headers, but that is not usage. raylib's ordinary OpenGL 3.3 default fragment shader simply writes sampled texture times tint (`raylib rlgl.h:5059-5070`). The project's final FXAA shader likewise samples, filters, and writes values without decoding or encoding (`sources/engine/render/FxaaShader.h:16-64`).

`Main.cpp` requests only vsync before `InitWindow()` (`Main.cpp:59-65`). It does not request an sRGB surface. GLFW framebuffer channel hints are left at defaults, and raylib does not set `GLFW_SRGB_CAPABLE` (`raylib platforms/rcore_desktop_glfw.c:1461-1469`). No code queries the default attachment's `GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING` or color bits.

OpenGL's `GL_FRAMEBUFFER_SRGB` state defaults disabled, and no engine/raylib path found here enables it. The strongest static conclusion is therefore:

- the final draw copies stored values to the default framebuffer without an explicit transfer conversion;
- model fragments have already been manually sRGB-encoded;
- sky/UI/textures are largely passed through as authored bytes;
- sector lighting/effects are the results of math on mixed raw values;
- whether the default attachment is sRGB-capable is unknown and currently immaterial to automatic conversion while `GL_FRAMEBUFFER_SRGB` remains disabled;
- the OS/compositor's interpretation and display conversion are outside this code and platform-dependent.

The current outcome is **mixed behavior**, not cleanly “missing gamma” or “double gamma.” Adding a final `pow(1/2.2)` today would double-encode model/UI/sky-like content while encoding other already-mixed content; enabling `GL_FRAMEBUFFER_SRGB` without first making the presented input linear would have the same category of problem.

FXAA uses Rec.601-like luma weights directly on stored values and the bilinear presentation filter also interpolates stored values (`FxaaShader.h:18-63`). Thus FXAA, supersample resolve/downsampling, and final scaling occur in the scene target's mixed/display-like space rather than a consistently linear space.

### UI and Blending

raylib initializes standard alpha blending and defines it as `SRC_ALPHA, ONE_MINUS_SRC_ALPHA`; additive is `SRC_ALPHA, ONE`; `BLEND_ADD_COLORS` is `ONE, ONE` (`raylib rlgl.h:2143-2168`). Because neither the render targets nor the default framebuffer perform sRGB conversion, blending operates directly on stored normalized values.

Relevant paths are:

- sector middle textures and billboards are alpha-tested/cut out, not smoothly blended (`SectorMeshRenderer.cpp:272-275`; billboard renderer disables color blending while drawing);
- projected/contact model shadows draw black with standard alpha onto the LDR world target (`SectorDynamicModelShadowRenderer.cpp:85-130`, `147-159`, `656-696`);
- dust writes premultiplied radiance and uses `ONE,ONE` both into its RGBA8 target and into the world target (`SectorLightDustRenderer.cpp:155-173`, `640-666`);
- the muzzle flash uses raylib additive `SRC_ALPHA,ONE` in the RGBA8 viewmodel target (`FpsViewmodelEffectsRenderer.cpp:263-352`);
- the viewmodel target is then composed onto the world target with ordinary alpha (`Main.cpp:311-323`);
- UI/menu targets are cleared transparent, drawn with ordinary alpha, and later composed again with ordinary alpha (`Main.cpp:265-291`, `362-368`). This two-stage straight-alpha composition can effectively premultiply translucent RGB during the first pass and apply alpha again during the second, producing dark translucent edges. That is a separate alpha-representation concern from gamma, but it belongs in the color pipeline.

All these blends occur in the stored mixed/gamma-like space and clip to RGBA8. A conventional workflow would blend scene transparency/additive radiance in linear floating-point storage, tone-map and encode once, then composite display-referred UI in a deliberately defined manner. The current order does not do that.

The crosshair/HUD is materially later than world postprocessing and FXAA: it draws directly to the default framebuffer before the UI layer (`Main.cpp:338-367`; `FpsHudRenderer.cpp:128-152`). It is not tone-mapped or supersampled. Editor 3D highlight/cone/probe overlays, by contrast, are drawn into the world target after postprocessing and are then affected by final FXAA/downsampling (`SectorEditor.cpp:3412-3418`; `Main.cpp:326-350`). The 2D editor and all ordinary UI colors are direct raylib byte colors without transfer conversion.

### Actual Major Operation Order

For the normal 3D path:

1. Render spotlight and projected-model depth shadow maps.
2. Clear `worldTarget` to `{8,10,14}`.
3. Draw sky.
4. Draw opaque/cutout sector batches: raw albedo/decal + baked AO/lightmap + dynamic Lambert/spot lighting + per-fragment distance fog.
5. Draw doors with raw diffuse/probe/dynamic light/fog.
6. Draw static and dynamic glTF models with linear PBR, local ACES and sRGB encode, then encoded-space fog.
7. Alpha-blend projected/contact model shadows.
8. Draw cutout billboards with mixed-space lighting/fog.
9. End world 3D pass; the `GL_RGBA8` write has already clipped every surface.
10. Apply LDR emissive-decal bloom.
11. Apply local volumetric fog, then light haze, then additive dust.
12. Render models/viewmodel with local ACES/sRGB into a separate RGBA8 target; draw additive muzzle flash into it.
13. Alpha-composite the viewmodel target over the world target.
14. Draw editor 3D overlays into the world target when applicable.
15. Draw the world target to the default framebuffer through FXAA and bilinear/downsampling.
16. Draw crosshair/HUD directly in window space.
17. Alpha-composite `uiTarget`; draw raylib FPS; alpha-composite `menuTarget` when open.
18. Present to the OS/display.

There is no global tone-map step and no global gamma/sRGB-encode step in this sequence.

### Platform-Specific Behavior

The project fetches raylib 6 and uses the desktop OpenGL backend (`CMakeLists.txt:98-121`). Raylib requests an OpenGL 3.3 core context; macOS additionally requests forward compatibility (`raylib platforms/rcore_desktop_glfw.c:1543-1563`). The project has no renderer color-management compile-time branches. Linux only adds X11/Xcursor linkage, while macOS uses external GLFW (`CMakeLists.txt:23-25`, `106-108`).

Known platform/display differences:

- macOS starts at 1600x900; other platforms start at 1920x1080 (`Main.cpp:18-24`), changing final scale/filter sample locations and screenshot dimensions;
- only `FLAG_VSYNC_HINT` is requested; HiDPI and 4x MSAA are not requested (`Main.cpp:61-65`);
- raylib has Retina/HiDPI handling, but it is conditional on `FLAG_WINDOW_HIGHDPI`, which this application does not set (`raylib rcore_desktop_glfw.c:1505-1529`);
- F10 toggles borderless-windowed presentation (`Main.cpp:236-263`); this does not request native HDR or a different color space in project code;
- framebuffer RGB/alpha bit hints and refresh rate are left to GLFW defaults (`raylib rcore_desktop_glfw.c:1461-1469`);
- no sRGB capability, HDR surface, color-space declaration, or default framebuffer encoding query exists;
- no ICC, ColorSync, Windows Color System/ICM, X11 ICC profile, or gamma-ramp API usage was found.

The application therefore presents RGB numbers and relies on the window system, compositor, OS display settings/profile handling, GPU LUTs, and monitor. macOS, Windows, and Linux compositors do not have identical color-management behavior, especially for untagged legacy OpenGL surfaces. Exactly what profile conversion occurs is unknown without a platform/runtime experiment. The engine should standardize its own SDR scene-to-sRGB encoding; it should normally let the OS map that standard output to the physical display rather than maintaining per-monitor-model correction tables.

### Existing Settings

No player-facing gamma, display brightness, exposure, contrast, HDR, black level, white point, tone-map selection, calibration, color-temperature, or LUT setting was found.

The settings named “brightness” are not display settings:

- weapon/viewmodel `brightnessAdjustment` is authored in `weapons.json` and project `application_settings.json` (`FpsWeaponRegistry.h:133-145`, `219-252`);
- `FpsViewmodelBrightnessMultiplier()` maps it to `1 + clamp(adjustment,-1,1)` (`FpsViewmodel.cpp:140-144`);
- that multiplier scales the model's linear lighting sum immediately before the model-local ACES curve (`SectorStaticModelRenderer.cpp:446-450`);
- the editor exposes “Pistol brightness” as a preview/weapon override (`SectorEditorPreviewSettingsModal.cpp:551`).

These are artistic asset/project overrides for a viewmodel and attachment. They do not affect world geometry, final output, black level, or a user's monitor. They should remain conceptually and persistently separate from future local per-machine calibration preferences.

Likewise, `environmentExposure` is computed from sector ambient or set to 1 for sky sectors (`SectorRuntimeObjects.cpp:84-113`, `390-395`) and scales only model environment specular (`SectorStaticModelRenderer.cpp:423-436`). It is an artistic IBL strength despite the name, not photographic exposure.

### Screenshot Path

The project does not call a custom screenshot API, but the fetched raylib build enables `SUPPORT_SCREEN_CAPTURE`. F12 is handled in raylib after a frame's drawing/presentation work and calls `TakeScreenshot()` (`raylib rcore.c:930-935`). `TakeScreenshot()` reads the current default framebuffer through `rlReadScreenPixels()` and exports RGBA8 PNG (`rcore.c:1828-1850`; `rlgl.h:3803-3836`). It therefore captures:

- the final world presentation including FXAA/downsampling;
- HUD/crosshair, UI, FPS, and menu;
- the default framebuffer's stored 8-bit values;
- **not** the monitor's physical response, ICC result after compositor/display processing, monitor OSD, Night Light/Night Shift/True Tone, or panel calibration.

It does not directly capture the pre-presentation 2880x1620 world target. PNG export uses stb's raw writer and does not add a project color profile/gamma declaration (`raylib rtextures.c:619-648`), so image viewers may assign their own default interpretation.

With the same game state, exact executable/assets, same drawable size, same filtering and settings, and a stable frame, screenshots should be close and often byte-identical in large flat areas. Exact cross-machine identity is not guaranteed because default framebuffer format, drawable size, driver shader precision/rasterization, texture filtering, and frame timing are not fixed/logged. Different ICC/monitor settings should not alter `glReadPixels()` values. Consequently:

- identical screenshot pixel values but different appearance implicate viewer/OS/display handling;
- different off-screen reference-target values implicate engine/input/asset/GPU execution;
- identical off-screen values but different final screenshot values implicate presentation size/filter/default-framebuffer behavior.

## Findings Matrix

| ID | Finding | Confidence | Consequence |
|---|---|---|---|
| F1 | Ordinary image textures use linear-format `GL_RGB8`/`GL_RGBA8`; there is no hardware sRGB decode | Proven | Color textures need explicit shader decode or semantic sRGB formats |
| F2 | The main sector shader samples color directly and does lighting on raw normalized values | Proven | Gamma-encoded-looking albedo participates in lighting math |
| F3 | glTF models explicitly decode color textures, locally tone-map, and encode | Proven | Models follow a different convention from level geometry/effects |
| F4 | Every color render target is `GL_RGBA8`; no HDR scene buffer exists | Proven | Values above 1 clip before global postprocessing |
| F5 | Bright warm lights lose tint as channels independently saturate | Proven mechanism; actual observed pixel requires capture | Explains a pale/white high-intensity spotlight, especially on bright surfaces |
| F6 | Authored light/fog/tint bytes are normalized, not sRGB-decoded | Proven | `{255,245,225}` is used as a less-warm numeric light than an sRGB swatch decode |
| F7 | Lightmap RGB is float-computed, hard-clamped, byte-encoded without a transfer curve, uploaded and sampled raw | Proven | The round trip is internally consistent but LDR/undocumented |
| F8 | AO is baked in alpha and used as raw linear data; there is no SSAO target | Proven | AO itself has no gamma error in storage/sampling |
| F9 | ACES exists only in the shared model/viewmodel shader | Proven | Tone mapping is real but not a scene solution |
| F10 | Model fog occurs after model tone mapping and sRGB encoding | Proven | Fog composition differs across object types |
| F11 | Bloom, fog, haze, dust, FXAA, downsampling, alpha, and additive blending operate on LDR stored values | Proven | Nonlinear blends/filtering and repeated clipping |
| F12 | There is no final encoding pass and no `GL_FRAMEBUFFER_SRGB` control | Proven | Final output is a raw copy of already mixed conventions |
| F13 | Default framebuffer color encoding/bit depth is neither requested nor queried | Proven | Platform surface behavior is not auditable from logs and not guaranteed |
| F14 | No engine-owned ICC/display profile integration or SDR/HDR calibration exists | Proven | Visible differences can remain display-side even with identical engine bytes |
| F15 | macOS uses a different initial presentation resolution; HiDPI is not requested | Proven | Final filtered pixels/screenshots can differ by platform size |
| F16 | F12 screenshots capture the final default framebuffer before physical display response | Proven | They are the smallest existing tool for separating engine bytes from display appearance |
| F17 | UI is rendered to transparent RGBA8 layers with standard alpha, then standard-alpha composited again | Proven | Translucent colors/edges may be effectively alpha-applied twice |

## Likely Causes of the Three-Machine Difference

Ranked by relevance to the observations, not by certainty:

1. **Display/OS profile and monitor state — high likelihood for the overall brightness/warmth difference.** Changing desktop monitor gamma moved its brightness toward the laptops, directly demonstrating a display-side contribution. The MacBook's darker/warmer appearance could also involve its display profile, True Tone, Night Shift, automatic brightness, reference mode, HDR desktop setting, or panel white point. None of these is visible in the engine source or captured by `glReadPixels()`.

2. **LDR highlight clipping — high likelihood for the pale/white central spotlight.** Dynamic lighting can reach 4 in shader arithmetic but is immediately stored in `GL_RGBA8`. The warm light's blue channel needs only about 13% more scalar gain than red to reach 1, so a bright hotspot readily loses tint. Model fragments fare differently because their local ACES curve compresses them first.

3. **Raw-byte light-color convention — high likelihood for insufficient warmth in engine output.** The authored value is used as `{1,.961,.882}`, not the approximately `{1,.913,.753}` that an sRGB UI swatch would represent in linear light. This makes the numeric light closer to neutral before any monitor difference.

4. **Mixed sector/model transfer functions and fog — certain engine inconsistency, scene-dependent contribution.** Models decode/tone-map/encode, sector geometry does not, and fog is applied in different stages. Which objects dominate the observed view determines the size of this effect.

5. **Unspecified default framebuffer and presentation size — plausible but unmeasured.** The engine makes no sRGB/format query and macOS uses a different initial size. This can alter final stored samples, but static evidence does not show an active platform-specific double/missing conversion.

The key diagnostic is a pair of pixel comparisons: first capture the pre-presentation world target at a fixed frame, then capture raylib's final F12 screenshot on each machine. If those files match while the displays do not, the remaining difference is downstream of the engine framebuffer.

## Technique and Complexity Assessment

Complexity estimates describe this engine's integration work, including all active sector, model, effect, UI, editor-preview, and asset-cache paths.

| # | Technique | Status | Improvement complexity | Project-specific rationale |
|---:|---|---|---|---|
| 1 | Explicit source-texture color-space conventions | Absent | **medium** | Requires semantic asset metadata/load flags, cache-key handling, sector registry conventions, glTF mapping documentation, and tests—not just prose if it is to be enforceable |
| 2 | sRGB decoding for albedo/base-color/emissive | Implemented but incomplete/questionable | **medium** | Correct in model shader; absent for sector albedo/decal/sky and generic UI. Must coordinate with target/output conversion to avoid dark/double-converted results |
| 3 | Linear sampling for normal/material/data textures | Already implemented correctly | — | All use non-sRGB internal formats and raw samples; glTF metallic/roughness channels are split correctly by raylib. Semantic safeguards are still absent |
| 4 | Documented light-color authoring convention | Absent | **small** to document, **medium** to migrate | Many map, muzzle, fog, tint, ambient, and bake call sites use byte/255; changing interpretation shifts existing authored content and bake hashes/results |
| 5 | Consistent linear static and dynamic lighting | Implemented but incomplete/questionable | **large** | Bake/dynamic numeric coefficients roughly agree, but surfaces/models/effects use different transfer stages; migration touches all renderer shaders and bake conventions |
| 6 | Correct baked-lightmap encoding/decoding convention | Implemented but incomplete/questionable | **small** to formalize; **medium** to improve precision/range | Raw linear bytes round-trip correctly, but PNG is untagged, 8-bit, and clipped; future sRGB texture semantics must exempt it explicitly |
| 7 | Linear-space transparency and additive blending | Absent | **medium**, dependent on linear scene storage | Shadows, muzzle, dust, viewmodel, and UI layers blend in stored LDR/mixed space; alpha representation also needs a deliberate straight/premultiplied choice |
| 8 | Floating-point HDR scene rendering | Absent | **medium** for target creation, **large** for complete migration | raylib exposes float formats, but custom FBO creation, all effect targets, readback, bloom, presentation, and fallback behavior need integration |
| 9 | Neutral or filmic tone mapping | Implemented but incomplete/questionable | **medium** after HDR | ACES-like curve is only in model shader. A global pass must replace local curves and include all scene radiance while excluding display-referred UI |
| 10 | Artistic exposure control | Absent | **small** after global HDR/tone mapping | One global parameter/uniform and persistence is simple once a proper HDR pass exists; current “exposure” is only IBL strength |
| 11 | Exactly one final linear-to-sRGB conversion | Implemented but incomplete/questionable | **large** as a pipeline correction | A final encode shader itself is small, but existing model-local encoding and display-referred sector/UI paths must be untangled first |
| 12 | Explicit/queryable sRGB default-framebuffer handling | Absent | **small** to query/log; **medium** to request/control cross-platform | GLFW/raylib surface creation and GL state must agree with whether final encoding is shader- or framebuffer-based |
| 13 | Per-machine SDR brightness/gamma calibration setting | Absent | **small** after final output is coherent | Needs a final display adjustment and local preferences; adding it now would hide pipeline errors and risk double correction |
| 14 | Near-black/near-white calibration screen | Absent | **small** | Straightforward UI/reference pattern, but should target a defined final SDR output and bypass artistic scene exposure |
| 15 | Deterministic color-pipeline debug logging | Absent | **small** | Can log GL formats/state, selected shader path, drawable/platform/GPU, and color-pipeline version without changing rendering |
| 16 | ICC/display-profile integration | Absent | **very large** if engine-owned | Cross-platform profile discovery, transforms, window/compositor semantics, multiple displays, and testing are outside the current raylib abstraction; OS management is preferable for standard SDR sRGB |
| 17 | Native HDR output and HDR calibration | Absent | **very large** | Needs HDR-capable surfaces/swapchains, scRGB/PQ/metadata/platform APIs, luminance calibration, UI policy, and SDR fallback beyond current OpenGL 3.3/raylib presentation |

## Prioritized Recommendations

The tables below separate mathematical correctness, reproducibility, artistic rendering, and player display concerns. “Own slice” means the work should be implemented and reviewed independently.

### Correctness

| Priority | Recommendation | Benefit | Complexity | Risk | Main integration points | Dependencies | Own slice? |
|---:|---|---|---|---|---|---|---|
| C1 | Write and enforce a color-data contract: color textures, data textures, lightmap RGB/AO, glTF factors, engine `Color` swatches, and shader input/output spaces | Prevents future double/missing conversions and defines how existing content should be interpreted | Small for document; medium for enforceable semantic flags | Low for documentation; medium for cache/API change | `TextureLoadFlags`, `TextureAssets`, sector texture registry, model metadata, `SectorColor.h`, lightmap docs/tests | Diagnostic baseline recommended | Yes |
| C2 | Decide the authored light/fog/tint byte convention and add named conversion helpers; include an explicit existing-content migration policy | Makes static, dynamic, muzzle, fog, haze, ambient, and model lighting agree | Medium | High visual change; rebakes and authored tuning may shift | `SectorColor.h`, dynamic-light selection, `FpsPlayerRuntime`, `SectorLightmap`, fog/haze upload, topology serialization/editor color controls | C1; reference captures | Yes, preferably separate bake/runtime sub-slices |
| C3 | Introduce a shared linear floating-point world target and float effect targets with validated fallback | Preserves highlight hue/range and enables mathematically correct blending/post | Medium infrastructure; large end-to-end | GPU support/performance/memory; fallback behavior | `Main.cpp`, render-target helper, bloom/fog/haze/dust targets, screenshots/tests | C1, diagnostics | Yes |
| C4 | Migrate opaque sector, door, billboard, and glTF shaders to emit the same linear scene convention; remove model-local tone/encode only when global replacement is ready | Eliminates the largest proven mixed-path inconsistency | Large | Very high visual regression and retuning risk | `SectorMeshRenderer`, `SectorDoorRenderer`, `SectorBillboardRenderer`, `SectorStaticModelRenderer`, sky/PBR environment | C1-C3; global presentation pass staged atomically with local encode removal | Split by opaque path, with a guarded integration slice |
| C5 | Move distance fog, local fog, haze, shadows, dust, muzzle flash, and scene transparency/additive operations into the linear convention; define straight vs premultiplied alpha | Corrects scattering/composition and prevents gamma-space additive/alpha errors | Large across several small renderer modules | Medium/high; atmosphere look and soft edges change | Fog/haze/dust/bloom renderers, dynamic model shadows, viewmodel effects, viewmodel composition | C3-C4 | Yes, one effect family per slice |
| C6 | Add one global HDR-to-SDR tone/transfer pass: scene exposure/tone curve in linear, then exactly one sRGB encode; composite display-referred UI afterward | Produces a coherent SDR signal and makes output independent of accidental model-local behavior | Medium after prior work | High if introduced before all input paths are linear; UI order mistakes | `Main.cpp`, new presentation shader/helper, model shader cleanup, HUD/UI ordering | C3-C5 | Yes |
| C7 | Make default-framebuffer policy explicit: either manual final sRGB encoding into a linear UNORM default buffer with `GL_FRAMEBUFFER_SRGB` disabled, or linear shader output plus verified sRGB framebuffer conversion—never both | Prevents missing/double final conversion across platforms | Medium | Platform/window regressions if capability assumptions are wrong | raylib/GLFW setup boundary, `Main.cpp`, presentation diagnostics | C6 and D1 | Yes |
| C8 | Fix transparent intermediate alpha representation, especially UI/menu and viewmodel targets | Removes double-alpha darkening/fringes independently of tone mapping | Medium | UI/muzzle edge appearance changes | `Main.cpp`, UI/menu drawing, viewmodel target, appropriate premultiplied blend modes | Can be diagnosed early; final design should align with C5/C6 | Yes |

The lightmap convention should not automatically switch to sRGB texture sampling. Its current RGB is numeric lighting data and AO is data. If lightmap precision is later increased (for example a float/half-float runtime artifact), the source hash/version and stale-bake behavior must change deliberately. Sky visual settings should remain excluded from the source hash; `ceilingSky` and directional/static lighting must remain included.

### Consistency and Diagnostics

| Priority | Recommendation | Benefit | Complexity | Risk | Main integration points | Dependencies | Own slice? |
|---:|---|---|---|---|---|---|---|
| D1 | Log a color-pipeline diagnostic block once after window/targets are created | Immediately reveals actual texture internal formats, default attachment encoding/bits, `GL_FRAMEBUFFER_SRGB`, drawable size, GL vendor/renderer/version, shader/postprocess selection, and platform | Small | Very low; log portability and unavailable enum handling | `Main.cpp` plus a small render diagnostics helper; raw OpenGL/rlgl boundary | None | Yes; highest-value first code slice |
| D2 | Add a deterministic in-engine SDR test pattern and a fixed-state capture mode | Separates transfer errors, clipping, black/white crush, and display behavior | Small/medium | Low; must bypass artistic postprocessing deliberately | presentation/debug module, UI command/menu, local test asset or procedural draw | D1 and C1 convention | Yes |
| D3 | Add explicit pre-presentation world-target readback plus final-framebuffer readback, with dimensions/pipeline metadata | Localizes differences to scene rendering versus presentation/default framebuffer | Small/medium | Readback stalls if used outside diagnostics; PNG tagging ambiguity | render diagnostics, screenshot/export helper | D1 | Yes |
| D4 | Establish deterministic screenshot comparisons in CI/offline tests using generated fixtures, not editable levels | Catches unintended color-pipeline changes | Medium | Cross-GPU exact comparisons can be brittle; use tolerances/hashes by stage | generated test scene, immutable fixture, optional software/reference environment | D1-D3 | Yes |
| D5 | Explicitly request and validate framebuffer channel depth/encoding through the chosen window backend policy | Reduces platform-selected surface ambiguity | Medium | raylib/GLFW integration and macOS behavior | build/window setup and runtime validation | D1, C7 | Yes |

The minimal runtime log should include:

- platform, executable build/color-pipeline version, raylib version;
- GL vendor, renderer, version, GLSL version;
- window logical size, drawable/render size, monitor index/name, DPI/content scale, windowed/borderless/fullscreen state;
- `GL_RED_BITS`, `GL_GREEN_BITS`, `GL_BLUE_BITS`, `GL_ALPHA_BITS`, and default framebuffer attachment `GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING` using the correct default-framebuffer attachment enum;
- `glIsEnabled(GL_FRAMEBUFFER_SRGB)` before world rendering and before final presentation;
- every color target's reported OpenGL internal format, dimensions, filtering, and sample count;
- active FXAA, supersample scale, bloom, fog/haze quality, tone mapper, exposure, and final encoding mode;
- asset/shader color semantic version and whether glTF local encode remains active.

The deterministic diagnostic image should be procedural or immutable and contain:

- near-black steps dense enough to reveal crush (for example code values 0, 1, 2, 4, 8, 12, 16, 24, 32);
- a labeled mid-grey patch and neighboring 18% linear/display-reference patches so the intended domain is explicit;
- near-white steps (for example 223, 235, 243, 247, 251, 253, 254, 255);
- neutral R=G=B ramps plus isolated red, green, and blue ramps;
- known warm swatches including `{255,245,225}` and cooler comparison swatches;
- a linear-light gradient encoded by the chosen final pipeline beside a raw code-value gradient;
- hard-edged and translucent patches to expose gamma-space filtering and alpha/premultiplication errors.

The pattern should be capturable both immediately before final transfer encoding and from the final default framebuffer. It should not depend on a level, baked lightmap, camera, time, fog noise, or user-edited engine assets.

### Artistic Improvements

| Priority | Recommendation | Benefit | Complexity | Risk | Main integration points | Dependencies | Own slice? |
|---:|---|---|---|---|---|---|---|
| A1 | After correctness migration, select a neutral global tone curve and expose a project-wide artistic exposure | Preserves bright-light hue, controls highlight roll-off, and unifies models/sectors/effects | Medium | Retuning all authored lighting and bloom; choice can become a “look” | global presentation pass, map/project renderer settings | C3-C6 | Tone curve and exposure can be separate slices |
| A2 | Rebuild bloom around HDR scene luminance/emissive radiance rather than the current LDR intensity scale | Natural bloom threshold/energy and fewer clipped colored highlights | Medium | Performance and major look change | `SectorBloomRenderer`, emissive material semantics | C3-C6, A1 policy | Yes |
| A3 | Add optional highlight desaturation/white-point controls only if the art direction needs them | More controlled very bright highlights | Small/medium | Can erase intentional colored lights and conceal bad exposure | global tone mapper | A1 and calibrated references | Yes, optional |
| A4 | Add LUT/color grading only after a neutral reference pipeline exists | Controlled artistic look independent of display calibration | Medium | LUT domain/order errors; can hide correctness regressions | final linear/post-tone grading design, asset manager | A1 and reference captures | Yes, optional |

These are rendering/art-direction improvements, not monitor-calibration fixes. HDR intermediates and tone mapping can keep the spotlight warm in the framebuffer, but they cannot make two differently calibrated displays look identical.

### Player Calibration

| Priority | Recommendation | Benefit | Complexity | Risk | Main integration points | Dependencies | Own slice? |
|---:|---|---|---|---|---|---|---|
| P1 | Add a near-black/near-white SDR calibration screen with instructions to disable temporary night/comfort modes while calibrating | Helps users detect black crush, white clipping, and gross brightness mismatch | Small | User confusion if the pipeline is not first standardized | settings UI, procedural pattern | C6-C7, D2 | Yes |
| P2 | Add a conservative per-user SDR brightness/display-gamma adjustment in local preferences, applied only at the defined final display stage | Allows machine-local compensation without modifying levels/lights | Small/medium | Can fight OS ICC calibration or cause double correction; needs reset/default | local preferences, final presentation shader, settings UI | C6-C7, P1 | Yes |
| P3 | Keep display calibration out of level JSON, light data, lightmap settings, and project weapon overrides | Prevents one machine's correction from changing authored content for everyone | Small policy/persistence work | Low | preferences architecture and serialization boundaries | P2 | Include with P2 |
| P4 | Defer native HDR calibration (paper white, min/max luminance, peak brightness) until there is a real HDR output backend | Avoids fake HDR controls that merely remap SDR | Very large overall HDR program | Very high platform/monitor variability | platform window/surface APIs, presentation, UI, preferences | Full SDR correctness first; native HDR backend | Many isolated future slices |

An SDR user gamma/brightness control should be modest and clearly labeled as display calibration. It should not multiply world light intensity, alter lightmap bake data, change tone-map exposure used for art direction, or be saved in topology/level JSON.

Engine-owned ICC conversion is not recommended as an early or default solution. Standardize an sRGB SDR signal and allow the OS/display stack to map it. If a future professional workflow needs explicit profile transforms, that requires a separate cross-platform design covering window tagging, multi-monitor moves, profile change notifications, screenshot semantics, and bypass/interaction with OS color management.

## Proposed Implementation Slices

Each slice below should be independently reviewable and should keep generated tests/fixtures outside user-edited `assets/levels` and `assets/sector_demo` content.

1. **Runtime color-state audit log (no visual change).** Log GL/default-framebuffer/target formats, encoding/state, dimensions, GPU/platform, and active pipeline switches. Add a unit-testable formatting layer where practical.
2. **Deterministic SDR reference pattern and dual-stage capture (diagnostic only).** Provide fixed code-value/linear-reference patches and capture pre-presentation plus final framebuffer with metadata.
3. **Color contract and semantic API design (no visual change).** Document color versus data assets, lightmap raw-linear bytes, glTF conventions, UI domain, light swatch convention, and final-output policy. Extend asset request identity only in a separate implementation commit if needed.
4. **Explicit texture semantics.** Add sRGB-color versus linear-data selection without yet changing every renderer; validate that normal, metallic, roughness, occlusion, lightmap, shadow, depth, and noise paths stay linear-data.
5. **Light-color convention and compatibility migration.** Add named conversions for map/editor/muzzle/fog colors, version authored behavior if necessary, update bake/runtime together, and define rebake expectations. Do not silently reinterpret existing levels.
6. **Float world-target infrastructure.** Add a tested `RGBA16F` scene target/fallback factory and readback diagnostics while leaving presentation behavior behind a development switch until dependent paths are ready.
7. **Opaque scene linearization.** Migrate sector geometry/doors/billboards/sky and model output in controlled sub-slices. Coordinate removal of model-local ACES/sRGB with the temporary/final common presentation pass so no intermediate commit double-encodes or displays raw linear output unintentionally.
8. **Atmosphere and blend linearization.** Move distance fog, local fog, haze, projected/contact shadows, dust, muzzle flash, and viewmodel composition to float linear targets, one effect family at a time. Define premultiplied-alpha policy and fix transparent intermediate layers.
9. **Global neutral tone map and exactly-one sRGB output.** Add global exposure/curve, choose manual shader encoding or verified `GL_FRAMEBUFFER_SRGB`, and enforce the choice with runtime assertions/logs. Composite display-referred UI afterward.
10. **HDR-aware bloom.** Replace the LDR scale/clamp workaround with threshold/radiance bloom in the float pipeline.
11. **Cross-platform deterministic regression captures.** Compare fixed diagnostic and generated-scene stages with appropriate tolerances.
12. **Local SDR calibration UI/preferences.** Add near-black/near-white calibration and a bounded per-machine adjustment after the correct final output stage exists.
13. **Optional artistic grading.** Only now consider highlight policy, LUTs, saturation, or color-temperature art controls.
14. **Native HDR feasibility/design.** Treat Windows, macOS, and Linux HDR output as separate platform work, not an extension of the SDR gamma slider.

Do not combine slices 4-10 and 12-14 into one change. The pipeline correction, HDR storage, tone mapper, player calibration, ICC, and native HDR have different correctness criteria and rollback risks.

## Unknowns Requiring Runtime Diagnostics

Static inspection cannot establish the following:

1. The actual default framebuffer RGB/alpha bit depths and whether its color attachment reports `GL_LINEAR` or `GL_SRGB` on each tested platform/GPU.
2. The actual `GL_FRAMEBUFFER_SRGB` state after context creation and after third-party/raylib calls, although no enabling call exists in inspected source and the OpenGL default is disabled.
3. Whether the macOS/Windows/Linux compositor tags or converts this untagged OpenGL surface, and how that changes across fullscreen/borderless/windowed modes or monitors.
4. Which ICC profile, GPU gamma LUT, HDR desktop mode, Night Light/Night Shift/True Tone, automatic brightness, or monitor OSD state is active on the three machines.
5. Whether the particular spotlight pixels that look white are already clipped in `worldTarget`, are changed by bloom/fog, or remain tinted in stored bytes.
6. Whether all compared machines use identical executable/assets, level/lightmap source hash, window/drawable size, fog quality, and editor/game state.
7. The exact PNG interpretation chosen by each external screenshot viewer because exported PNGs contain no project-declared color profile.

The smallest future diagnostic is:

- log the state listed under D1 once at startup and immediately before final presentation;
- freeze one generated diagnostic frame;
- save `worldTarget` before postprocessing, after postprocessing, and after viewmodel/overlay composition;
- save the final default framebuffer through the existing F12 path;
- compare raw pixel values/histograms and dimensions across machines;
- separately photograph/observe the displays only after the framebuffer comparison, recording OS/display profile and comfort/HDR modes.

No interactive engine run is needed to complete this audit, and none was performed.

## Files Inspected

Primary project files:

- `sources/Main.cpp`
- `sources/engine/assets/TextureLoadFlags.h`
- `sources/engine/assets/TextureAssets.cpp`
- `sources/engine/assets/ModelAssets.h/.cpp`
- `sources/engine/render/FxaaShader.h`
- `sources/engine/ui/UI.cpp`
- `sources/engine/systems/SpriteRenderSystem.cpp`
- `sources/game/GameApplication.cpp`
- `sources/game/FpsPlayerRuntime.cpp`
- `sources/game/FpsViewmodel.cpp`
- `sources/game/FpsViewmodelEffectsRenderer.cpp`
- `sources/game/FpsHudRenderer.cpp`
- `sources/game/FpsWeaponRegistry.h/.cpp`
- `sources/game/GameMainMenu.cpp`
- `sources/sector_demo/SectorColor.h`
- `sources/sector_demo/SectorDynamicPointLightSelection.cpp`
- `sources/sector_demo/SectorLightmap.cpp`
- `sources/sector_demo/SectorLightmapTypes.h`
- `sources/sector_demo/SectorTopologyTypes.h`
- `sources/sector_demo/SectorTopologyMap.h`
- `sources/sector_demo/SectorSceneRuntime.cpp`
- `sources/sector_demo/SectorRuntimeObjects.cpp`
- `sources/sector_demo/renderer/SectorMeshRenderer.cpp`
- `sources/sector_demo/renderer/SectorStaticModelRenderer.cpp`
- `sources/sector_demo/renderer/SectorDoorRenderer.cpp`
- `sources/sector_demo/renderer/SectorBillboardRenderer.cpp`
- `sources/sector_demo/renderer/SectorDynamicLightingRenderer.cpp`
- `sources/sector_demo/renderer/SectorDynamicModelShadowRenderer.cpp`
- `sources/sector_demo/renderer/SectorSkyRenderer.cpp`
- `sources/sector_demo/renderer/SectorPbrEnvironment.cpp`
- `sources/sector_demo/renderer/SectorFog.cpp`
- `sources/sector_demo/renderer/SectorBloomRenderer.cpp`
- `sources/sector_demo/renderer/SectorLocalFogRenderer.cpp`
- `sources/sector_demo/renderer/SectorLocalFogLighting.cpp`
- `sources/sector_demo/renderer/SectorLightHazeRenderer.cpp`
- `sources/sector_demo/renderer/SectorLightDustRenderer.cpp`
- `sources/sector_editor/SectorEditor.cpp`
- `sources/sector_editor/SectorEditorPreviewSettingsModal.cpp`
- `sources/sector_editor/preview/SectorEditorPreviewOverlay.cpp`
- `assets/config/weapons.json`
- `assets/config/application_settings.json`
- `CMakeLists.txt`
- `docs/architecture/sector_editor_architectural_principles.md`

Pinned/fetched dependency evidence:

- `cmake-build-debug/_deps/raylib-src/src/rtextures.c`
- `cmake-build-debug/_deps/raylib-src/src/rmodels.c`
- `cmake-build-debug/_deps/raylib-src/src/rlgl.h`
- `cmake-build-debug/_deps/raylib-src/src/rcore.c`
- `cmake-build-debug/_deps/raylib-src/src/config.h`
- `cmake-build-debug/_deps/raylib-src/src/platforms/rcore_desktop_glfw.c`
- relevant bundled GLFW source/headers for confirming available-but-unused sRGB hints

This report is the only repository change from the audit. It does not alter rendering, shaders, window/context creation, assets, settings, color values, topology/cache behavior, lightmap source hashing, collision, sector lookup, physics, camera behavior, or platform code.
