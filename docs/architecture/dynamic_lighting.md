# Dynamic lighting pipeline

The sector renderer supports a shared budget of up to 32 forward-rendered point
and spot lights. `graphics.maxDynamicLights` controls that budget (0-32); the
default is 32. Selection is contribution-ranked with hysteresis and is filtered
through the runtime portal graph, including dynamic portal blockers. Candidate
eligibility uses the player's complete yaw-independent open-portal component,
not the camera frustum: turning in place cannot add or remove a light, while a
closed door remains a hard light boundary. The connected component is cached
until the player's primary sector, portal graph, or dynamic blocker state changes.
Camera visibility safety fallbacks do not widen lighting reachability: a valid
gameplay sector still produces its blocker-aware connected component. Footprint
visibility samples likewise cannot seed the far side of a closed dynamic door.
If angular portal traversal saturates, overlapping windows are merged and an
iteration-cap fallback draws the connected component rather than the whole map.
Lights without a valid owner sector use conservative overlap against receiver
bounds in the reachable component.

Newly selected authored lights fade from zero to full upload intensity using a
smoothstep envelope. `graphics.dynamicLightFadeInSeconds` controls the duration
(default 0.25 seconds, valid range 0-2; zero disables the fade). The initial
selection after a renderer reset starts fully illuminated. Losing portal
eligibility removes a light immediately, while later re-admission restarts its
fade, so closed doors never retain light leakage. Selection ranking, receiver
culling, and shadow priority continue to use authored intensity rather than the
fade multiplier. Transient runtime lights such as muzzle flashes bypass the
fade.

Dynamic shadows use one persistent 8 x 8 depth-tile atlas. A spotlight uses
one perspective-projected tile. A point light uses six adjacent 90-degree
perspective tiles in +X, -X, +Y, -Y, +Z, -Z order. Point and spot casters share
the same planar opaque, alpha-tested, and skinned shadow shaders. This avoids
the nonlinear stretching and chord overdraw produced by dual-paraboloid
projection on large sector triangles.

Point-shadow receivers select the dominant-axis face and use ordinary
perspective depth. Surface receivers snap comparisons to the actual atlas
texel center and intersect that face ray with the receiver plane, leaving the
authored shadow bias to cover numerical precision instead of grazing-angle
depth slope. Hard point shadows use a weighted 2 x 2 comparison so individual
square texels do not turn into isolated dots. Soft point-shadow samples are
offset in face space, converted back to a world ray, and reselect the dominant
face before lookup; samples therefore cross cube-face boundaries without
clamping or atlas seams. Volumetric receivers use the same face mapping with a
simpler point-depth comparison because they do not have a surface plane.

Shadow quality controls face resolution while capacity remains fixed:

- Low: a 4096 x 4096 atlas with 512 x 512 tiles.
- Medium/High: requests an 8192 x 8192 atlas with 1024 x 1024 tiles.
- Off: no dynamic atlas sampling or generation.

The renderer queries `GL_MAX_TEXTURE_SIZE` and verifies framebuffer allocation.
If the 8192 atlas is unavailable, it logs a warning and falls back to a 4096
atlas with 512 tiles without reducing capacity. If fallback allocation also
fails, dynamic shadow generation and sampling are disabled safely. A 32-bit
depth atlas is approximately 256 MiB at 8192 and 64 MiB at 4096.

A spot consumes one occupied slot and a point consumes six, so the shared rule
is `6 * point shadows + spot shadows <= 64`. This permits ten point shadows at
every enabled quality, or ten point shadows plus four spots when the atlas is
exactly full. Lights that do not fit remain active and unshadowed. Shadow
priority is the primary allocation key, followed by estimated receiver
contribution and stable light ID.

The atlas persists across frames with validity tracked per light. Static
geometry and stationary casters reuse their tiles. Changed door and static
model bounds dirty only intersecting lights when exact old/new bounds are
available, with conservative all-light invalidation as the fallback. Compatible
stale tiles remain sampleable while queued; new, moved, or reassigned tiles do
not become sampleable until rendered. Routine updates clear only the affected
scissored tiles. A full-atlas clear is reserved for atlas initialization or
resource changes and invalidates every cached tile, including temporarily
unassigned tiles, so a later returning light must rebuild erased depth data.
Slot ownership is persistent and keyed by
the composite light identity, so adding or removing another caster does not
move or invalidate retained lights. A point retains one adjacent six-slot span;
if fragmentation prevents a new span, that newcomer remains temporarily
unshadowed instead of forcing atlas compaction. `graphics.maxShadowLightUpdatesPerFrame`
limits work to a fair oldest-first number of lights per frame (default 2,
0 means unlimited), prioritizing invalid tiles. A point light's six faces
always update atomically. Disabling dynamic lighting also skips dynamic atlas
generation. Rebuilds conservatively cull sector, door, and model casters
against each light volume and point-light face before drawing them.
An entering shadow caster remains at zero intensity until its assigned spot
tile, or all six point-light face tiles, are valid, then begins fading. A
light that does not receive an atlas slot fades in immediately as unshadowed.

Dynamic props and NPCs always receive available point- and spotlight atlas
shadows through the world-model PBR path, on top of their baked object-probe or
sector-ambient lighting. Their authored shadow mode controls casting only:
`none` casts nothing, `contact` draws the local contact blob, and `dynamic`
renders the current posed model into intersecting shared-atlas tiles. Animated
bone poses, rendered transforms, visibility, and mode changes participate in
the caster revision; unchanged objects retain cached tiles. A changed caster
dirties only lights intersecting its previous or current bounds and those
lights still honor `graphics.maxShadowLightUpdatesPerFrame`. `dynamic` has no
contact fallback when global shadows are disabled, a light has no atlas slot,
or its tile is not ready. Legacy authored `projected_silhouette` values load as
`dynamic`; the separate projected-silhouette renderer no longer exists.

Forward sector batches use compact per-sector light lists derived from their
receiver bounds. Shadow slots are remapped into each compact list while still
referencing the shared atlas. Receiver shaders reject back-facing,
out-of-cone, zero-strength, and zero-intensity contributions before PCF and
reuse point receiver-plane values across the samples. Spotlight projection
basis and depth coefficients are packed on the CPU, while point-only screen
derivatives are skipped for spotlight-only draws. High quality retains the
existing 12-sample filter.

Point and spotlight authored IDs occupy separate namespaces. Runtime selection,
hysteresis, deterministic ordering, and shadow cache matching therefore use a
composite `(light kind, authored ID)` identity. Debug output prints typed IDs
such as `point:3` and `spot:1` and distinguishes portal-eligible lights from the
smaller per-receiver lists that actually execute in forward shaders.

`graphics.depthPrepass` controls the opaque depth pre-pass and defaults to off.
It covers opaque sector batches, procedural doors, door models, and static
model geometry. Alpha-tested sector surfaces and billboards stay out of the
pre-pass. The forward pass retains `GL_LEQUAL`, so the populated depth buffer
rejects hidden fragments before the lighting loop while remaining robust to
the engine's existing projection precision.

Dynamic point and spot lights are runtime preview/game lighting only. Their
settings and shadow data are intentionally excluded from the baked lightmap
source hash. Dynamic prop/NPC cast modes, transforms, and animation poses are
also runtime-only and remain excluded.
