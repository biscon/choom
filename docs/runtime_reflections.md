# Runtime reflection probes

Reflection probes now have one implementation: scheduled runtime captures.
There is no reflection bake command, disk cubemap artifact, reflection-bake hash,
or static/dynamic cubemap layering. Authored probe placements, influence boxes,
resolution, priority and intensity remain useful and are saved with the map.

Static surface lightmaps and diffuse object-light probes remain independent.
They supply baked diffuse illumination to visible surfaces, including surfaces
seen by a reflection capture. Runtime captures include that illumination,
current dynamic diffuse lighting and emissive materials. They exclude direct
specular and environment reflections to avoid view-dependent highlights and
recursive feedback. This is reflected scene radiance, not dynamic diffuse GI.
Metals still correctly have no diffuse lobe.

## Scheduling and ownership

Each level owns two HDR cubemaps per enabled probe through `AssetManager`.
Capture scratch cubemaps are also scope-owned; the backend owns temporary face
render targets, its shader, framebuffer and an independent 256-pixel-face shadow
atlas using the existing 64-slot budget. Resources are allocated while loading
and released before the graphics context closes. There is no runtime file IO,
CPU filtering, image readback or cubemap upload.

Light addition/removal, switching, dimming, color, movement, range, cone,
shadow/profile changes and sampled flicker dirty intersecting probes. Flashlight
and transient runtime sources participate too. Door movement requests refreshes;
portal-blocking changes invalidate obsolete jobs. Invalidation is conservative
over the light radius and probe influence volume.

One job snapshots lights, door render poses and reachable sectors. Its shadow
selection/cache is separate from the player renderer. Work proceeds as follows:

1. Update at most one shadow face per frame.
2. Capture at most one of the six scene faces per frame in linear HDR.
3. GGX-prefilter roughness mips on the GPU, at most six 64-by-64 tiles per frame.
4. Publish only a complete cube, then crossfade from the previous complete cube
   over 100 ms. The previous cube cannot be reused until that transition ends.

Asynchronous timestamp queries tune filtering toward a 0.5 ms GPU target.
This is not a hard frame-time ceiling: a scene face or shadow face can exceed
it. Queries are read only after availability is reported. Debug counters show
CPU/GPU cost and overruns. New job starts for a given probe are at least 100 ms
apart. Visible probes get priority; waiting age prevents permanent starvation.
Continuous changes finish the current snapshot and queue a newer one. Discrete
on/off/removal changes discard obsolete work before it can publish.

Capture latency depends on selected shadow casters, resolution, scene complexity
and queue depth. Very brief flashes may expire before a complete cube can be
published. This system does not promise immediate or ray-traced reflections.
Moving actors, viewmodels, particles, glass/liquid transmission and atmosphere
are excluded from captures; static opaque/alpha-tested geometry, props and doors
are included. Cubemap reflections cannot reproduce every reflection visible
from an arbitrary point in a room.

## Selection and neighboring rooms

Receivers select their own room's probe, not the camera's probe. Two sources can
contribute, with box projection and per-fragment weights shared by sector
surfaces, models, doors/covers, windows and liquids. World models and viewmodels
use the same selection rules.

Open portal adjacency provides automatic doorway transitions, normally 0.5 world
units into each room. `blendDistanceWorld` is authored per probe, defaults to
0.5, and is omitted on save at that default. Zero disables its doorway blend.
Weights are clipped to the actual horizontal and vertical aperture. Closed
portal blockers and solid walls prevent neighbor blending. Same-sector
overlapping boxes can blend without needing a portal. A receiver draw uses at
most two sources; when several portals intersect it, the nearest candidate to
its center is selected. Larger receivers supply bounds so a doorway need not
contain the mesh center to participate. Unready sources are never sampled;
the existing real sky environment is used when available, otherwise zero.

## Loading, editor and verification

Game loading restores saved state and runs map creation scripts once before
initial reflection preparation. Simulation remains paused. Probes for initially
visible sectors, the spawn sector and directly open adjacent sectors are
prepared before the loading fade. Preview similarly waits for assets and nearby
probes, displays progress, and permits Escape to cancel. Failed resources are
terminal for preparation and fall back instead of hanging the load gate.

The preview Probes tab exposes Refresh, Pause/Resume, queue/readiness/failure
counts, active face/mip, GPU/CPU timings and memory. PBR receiver diagnostics
show selected probe IDs, center weights and temporal transitions. Refresh
invalidates the current capture; it does not save or bake anything.

Probe authoring continues through the existing authoring-graph editing service
and document/cache invalidation. Runtime publications do not dirty the document
or the 2D topology cache. The static lightmap source hash is unchanged; reflection
placement and blend controls remain runtime-only. No collision, sector lookup,
physics or Lua binding behavior changes are part of this feature.

Automated checks:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `python3 tools/check_reflection_shaders.py` (requires `glslangValidator`)
- `git diff --check`

Manual verification remains with the user: inspect hub sector 96 under Full PBR
and Environment Specular; toggle/move lights, use the flashlight, cross an open
doorway and close it, and compare a prop seen from the neighboring room. Test
preview cancel/re-entry, saved flashlight/door states, pause/refresh and level
switches. Check the reported capture cost on the target GPU; no GUI or GPU
performance claim follows from the CPU tests or offline shader validation.
