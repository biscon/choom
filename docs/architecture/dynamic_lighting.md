# Dynamic lighting pipeline

The sector renderer supports a shared budget of up to 32 forward-rendered point
and spot lights. `graphics.maxDynamicLights` controls that budget (0-32); the
default is 32. Selection is contribution-ranked with hysteresis and is filtered
through the runtime portal graph, including dynamic portal blockers.

Dynamic shadows use one persistent 4096 x 4096 depth atlas. A spotlight uses
one perspective-projected tile. A point light uses two adjacent
dual-paraboloid tiles, one for each world-Z hemisphere. The DPSM geometry stage
adaptively subdivides large triangles, clips each generated triangle to the
active hemisphere, and then applies the nonlinear projection. Clipping before
rasterization prevents geometry crossing the paraboloid seam from producing
invalid shadow footprints. The geometry stage also supplies the original
triangle plane so the fragment shader can invert the paraboloid projection,
intersect the resulting light ray with that plane, and write exact radial
depth instead of deriving depth from an interpolated world position.

Opaque and alpha-tested surface receivers compensate for point-shadow texel
quantization by snapping each lookup to its actual atlas texel center and
intersecting that texel's light ray with the receiver plane. The resulting
receiver depth is compared with the stored blocker depth, leaving the authored
shadow bias to cover numerical precision instead of grazing-angle depth slope.
Volumetric receivers retain the simpler point-sample comparison because they
do not have a surface plane.

Shadow quality controls tile size and therefore capacity:

- Low: 512 x 512 tiles, at most 64 occupied slots.
- Medium/High: 1024 x 1024 tiles, at most 16 occupied slots.
- Off: no dynamic atlas sampling or generation.

A spot consumes one occupied slot and a point consumes two. Lights that do not
fit remain active and unshadowed. Shadow priority is the primary allocation
key, followed by estimated receiver contribution and stable light ID.

The atlas persists across frames. Static geometry and stationary door casters
reuse its contents; door caster revisions invalidate it only while rendered
door geometry actually changes. Disabling dynamic lighting also skips dynamic
atlas generation. Rebuilds conservatively cull sector, door, and model casters
against each light volume and point-light hemisphere before drawing them.

Forward sector batches use compact per-sector light lists derived from their
receiver bounds. Shadow slots are remapped into each compact list while still
referencing the shared atlas. Receiver shaders reject back-facing,
out-of-cone, zero-strength, and zero-intensity contributions before PCF and
reuse point receiver-plane values across the samples. High quality retains the
existing 12-sample filter.

`graphics.depthPrepass` controls the opaque depth pre-pass and defaults to on.
It covers opaque sector batches, procedural doors, door models, and static
model geometry. Alpha-tested sector surfaces and billboards stay out of the
pre-pass. The forward pass retains `GL_LEQUAL`, so the populated depth buffer
rejects hidden fragments before the lighting loop while remaining robust to
the engine's existing projection precision.

Dynamic point and spot lights are runtime preview/game lighting only. Their
settings and shadow data are intentionally excluded from the baked lightmap
source hash.
