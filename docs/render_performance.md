# Main-view rendering performance

Depth pre-pass defaults to on. A saved explicit `graphics.depthPrepass: false`
still takes precedence. The setting is available in Graphics and is independent
of lightmap validity. It does not change the lightmap source hash.

## Opaque rendering

The renderer prepares reusable front-to-back candidate lists for sector batches,
static props and doors using portal visibility and conservative world bounds.
Depth and color consume those lists. Depth no longer depends on `castsShadow`;
it uses the same pose and lightmap-remapped mesh as color when available.
Cutout sector surfaces, glass, liquids, unknown/non-opaque model materials and
unsupported animated paths are not drawn by the simple depth shader.

Imported glTF materials retain sidedness and alpha classification. Single-sided
static models/model doors cull backfaces in color and depth; reflected transforms
reverse winding. Double-sided, unknown and degenerate-transform cases remain
two-sided. Imported alpha classification only controls depth eligibility: this
does not add glTF alpha-mask/blend rendering. Shadow/bake sidedness is unchanged.
If a thin prop disappears from its back, verify the asset's authored
`doubleSided` setting. No assets are automatically rewritten.

## Glass

Pane candidates are prepared once with sector and camera-frustum tests. Flat
glass still uses multiplicative transmission followed by additive reflections,
back-to-front per pane, depth-tested without writing depth. Its dedicated shader
variants exclude advanced refraction/depth-discard code. Procedural noise reuses
lattice hashes without changing the finite-difference pattern; zero imperfection
strength skips it. Disabling environment specular avoids cubemap sampling.

The preview Render tab has a session-only **Render glass** checkbox for A/B tests.
It does not edit the level or portal state. Light effects still render in the
appropriate atmosphere pass when panes are disabled.

## Profiling

F9 and the Render tab show depth, sector, models/doors, pre-glass effects,
glass-transmission and glass-reflection timing. Enable F9 or frame tracing to
collect GPU timings. GPU values are delayed asynchronous
samples, not necessarily from the CPU row's frame. Collection warms up for one
scene frame. Inactive stages report zero. The aggregate world pass also includes
preparation, sky, liquids and other work not assigned a dedicated substage;
substage totals need not equal the aggregate. The models/doors stage also covers
existing dynamic-object/billboard rendering, while geometry counters describe
the prepared opaque sector/prop/door lists.

The final-composite timer ends before `EndDrawing()`. `swap/wait CPU` measures
that function separately, including presentation, frame pacing and event polling;
it is not a GPU-work measurement. Frame trace v6 has a complete named column
header and buffered output, flushed periodically and on normal shutdown.

Rebuild the profiling executable before using `tools/capture_cpu_profile.sh`.
The script records CPU samples plus both fast and slow frames by default.
Set `ENGINE_FRAME_TRACE_THRESHOLD_MS` to restrict logging to slower frames.
Alternatively, launch the normal executable with
`--trace-frame-dips-ms=0.01 --frame-trace-output=/tmp/kitchen.frames.log`.

For user-run comparisons, disable VSync and hold render scale, camera pose and
lighting constant. Allow probes and timing queries to settle. Compare 5–10 second
stationary intervals at each kitchen/window viewpoint with glass on/off and
pre-pass on/off; inspect averages rather than isolated FPS screenshots. Verify
thin/mirrored props, moving doors, overlapping panes and reflections visually.
Automated tests use generated data and do not load editable level assets.

These are rendering-only changes: authoring topology and its 2D invalidation,
collision, sector lookup, physics and baked-lightmap source hashes are unchanged.
