# Shader sources

All project-owned shaders live in `assets/shaders`. Vertex entry files end in
`.vert.glsl`, fragment entries in `.frag.glsl`, and shared includes in `.glsl`.
Reusable presentation/fullscreen helpers belong in `engine/`, sector rendering
in `sector/`, and game-specific effects in `game/`. Raylib's internal default
shaders remain owned by raylib.

## Writing and loading a shader

Start each entry file with its GLSL version. Shared files omit the version and
use ordinary include guards:

```glsl
#version 330
#include "../engine/color_transfer.glsl"

// Attributes, uniforms, and main follow as usual.
```

Includes use a quoted path relative to the including file, with `/` separators.
Nested includes and `..` within the shader root are supported. Absolute paths,
paths escaping the root (including through symlinks), cycles, malformed includes,
and versions in shared files fail with a source/include-chain diagnostic.
Include directives must occupy a single logical line; macro-generated include
paths, angle brackets, and backslash continuations are not supported.

The loader expands includes textually, even inside GLSL conditional blocks.
It leaves `#if`, `#define`, and include guards to the GLSL compiler. Keep includes
unconditional; put conditional shader logic inside the included file when needed.
Comments are stripped without losing source line numbers. Expansion is bounded
to 64 nested files and 4 MiB of expanded text.

Register each program in `game/ShaderPrograms.h`: provide its stage paths and
any compile-time name/value defines. Add a matching `GameShader` enumerator in
the same order. Use a null vertex path only when raylib's default vertex shader
is intended. Register each supported specialization, as with the three glass
variants, so offline validation covers exactly what the runtime compiles.

`game::LoadGameShader()` uses the catalog and the existing asset root:
absolute source assets in Debug and `./assets` in Release. Release packaging
copies the whole shader tree. Launch a release package with its directory as
the working directory, matching the existing asset convention.

Reusable callers can use `engine::LoadShaderSource(root, entry, ...)` for CPU-only
expansion and `engine::LoadShaderProgram(root, definition)` for main-thread GPU
compilation. Variant defines are inserted immediately after `#version`. Numeric
`#line` directives map diagnostics back to paths; failed GPU compilation prints
the vertex and fragment source-ID tables alongside raylib's compiler log.

Shader programs remain owned by their renderer and must be unloaded before the
window/context closes. Load during initialization or an explicit resource-load
phase. Steady drawing does not read or compile shaders. Bloom target resizing
retains its shader programs. A failed optional effect stays unavailable until
explicit renderer reinitialization; required scene presentation fails startup.
Shader edits are read at the next explicit load. Restart the application to
reliably refresh everything; no file watcher or reload command is provided.

## Validation

```sh
cmake --build cmake-build-debug --target check_shaders -j2
```

This requires Python 3 and `glslangValidator`. They are optional for ordinary
engine builds; requesting validation without them reports the missing dependency.
The check uses a graphics-free C++ tool linked to the runtime source loader,
validates every stage and variant, links programs with both custom stages, and
checks that flat glass omits discard and scene-refraction sampling. It also
rejects entry files absent from the catalog. No C++ shader extraction is involved.

The validator passes shader stages explicitly. Link checks use temporary stage
suffixes for glslang; all maintained source files retain their `.glsl` endings.

```sh
# The reflection-only command remains as an alias for the complete check.
python3 tools/check_reflection_shaders.py

# Inspect the exact expanded fragment source and its source-ID table.
cmake-build-debug/shader_source_tool --source assets/shaders window frag

# Use another build directory or installed validator.
python3 tools/check_shaders.py --tool /path/to/shader_source_tool --validator /path/to/glslangValidator
```

CTest always covers the source loader and catalog. It also runs shader compiler
validation when Python and glslang were found at configuration time. Policy tests
inspect expanded GLSL independently from C++ uniform bindings and render ordering.
Loader fixtures use temporary files and do not depend on editable levels.

Shader-file organization does not change topology, cache invalidation, baked
lightmap source hashes, or collision/physics. Visual verification remains a manual
check: materials, glass variants, reflection capture/filtering, shadows, fog,
shafts/halos/dust, underwater effects, bloom, FXAA, and scene presentation.
