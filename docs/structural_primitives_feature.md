# Structural Primitives Feature Design

## Purpose

Structural primitives provide authored, generated static geometry for shapes
that are awkward or unnecessarily expensive to express with sector topology.
The primary uses are bridges, raised platforms, ramps, staircases, beams,
slabs, smooth pillars, pipes, ceiling ornamentation, and simple decorative
forms.

This is an engine-facing design document. It defines data ownership and the
rendering, collision, navigation, visibility, picking, and lightmap contracts.
The editor workflow and detailed UI are intentionally left for a later design
pass.

Structural primitives supplement the linedef sector map. They do not create
vertices, linedefs, sidedefs, sectors, or portal edges, and they do not use a
control-sector mechanism. Existing sector topology remains the source of room
boundaries, ordinary floors and ceilings, and horizontal portal connectivity.

## Goals

- Allow common architectural forms to be generated deterministically from a
  small set of authored parameters.
- Preserve the current topology-v2 linedef representation and sector mesh
  generation behavior.
- Support walking on, walking beneath, colliding with, raycasting against, and
  navigating over structural shapes where appropriate.
- Use the existing material, baked-lighting, dynamic-lighting, shadow,
  visibility, and picking conventions rather than creating an isolated render
  feature.
- Generate and cache all CPU/GPU geometry during explicit load or rebuild
  phases; never regenerate primitive meshes during steady update or render.
- Keep the representation small, data-oriented, deterministic, and suitable
  for JSON serialization.

## Non-Goals

- Replacing sector floors, ceilings, walls, or linedef portals.
- Importing arbitrary model files or becoming a general-purpose mesh editor.
- Making static structural geometry into ECS entities.
- Runtime deformation, skeletal animation, destructible geometry, or moving
  platforms in the first implementation.
- Boolean cutting of sector geometry or other primitives.

## Initial Primitive Catalog

### Rectangular prism

A rectangular prism is the baseline structural shape. It is defined by an XZ
footprint, yaw, bottom height, and top height.

Expected uses include:

- bridges and walkways
- platforms and ledges
- beams and lintels
- floor or ceiling slabs
- rectangular pillars and plinths

All six faces are generated. The top may be walkable, the sides block movement,
and the underside limits headroom. A raised prism therefore supports both
walking across its top and walking underneath it without changing the
containing sector's ordinary floor or ceiling.

### Ramp / wedge

A ramp is a solid wedge with a rectangular XZ footprint and a top plane rising
from a low edge to the opposite high edge. Yaw determines its horizontal
orientation. Authored low and high elevations allow the ends to align with
nearby sector floors or other structural surfaces.

The inclined top is walkable when its slope is within the configured movement
limit. Side faces and the high end block movement. The body is solid; it does
not imply an arbitrary sloped sector floor.

### Staircase

A staircase is defined by its footprint/orientation, width, total run, total
rise, and step count. Its visible mesh contains discrete treads and risers.

The default movement and navigation surface is a smooth support ramp matching
the stair rise and run. This avoids camera jitter and repeated step-edge
contacts while retaining the visible staircase geometry. Literal per-step
collision may be considered later, but is not the baseline contract.

The stair body is solid. Its sides and underside are generated consistently
with the selected dimensions.

### Cylinder

A cylinder is defined by center, radius, bottom/top heights, and radial segment
count. It supports smooth-looking pillars, columns, pipes, tanks, and rounded
architectural decoration without constructing a many-sided sector.

The side and both caps are generated. Radial tessellation is configurable but
validated and bounded. A deterministic default should provide a visibly smooth
column without producing excessive triangles.

Collision is optional. A colliding cylinder uses a matching circular footprint
for horizontal movement where practical; navigation consumes its generated
geometry. An oriented box approximation is acceptable only as an explicit
fallback, not as silently different authored behavior.

### Sphere

A sphere is defined by center, radius, and bounded latitude/longitude
tessellation. Its initial role is decorative: ornaments, caps, finials, rounded
details, and simple environmental forms.

Sphere collision is disabled by default. If enabled, it uses an explicit simple
sphere approximation and does not become a generally walkable surface in the
first implementation.

## Authoring and Compiled Data Ownership

The authoring graph is the source of truth for structural primitives. A normal
editor action must edit authoring data and then derive/compile runtime data; it
must not directly mutate `SectorTopologyMap` and copy the result back.

Each authored primitive has:

- a stable positive integer ID
- an enabled state
- a primitive kind
- exact planar placement using `SectorCoord`
- authored vertical values using the same height units as sector floors and
  ceilings
- yaw and kind-specific dimensions
- material assignments using material registry IDs
- UV settings needed by its generated faces
- collision and shadow participation settings

Common state should remain common, while shape-specific parameters should use
small plain-data records rather than a class hierarchy. Invalid or unknown
future kinds must produce a clear load/derivation diagnostic rather than
undefined geometry.

The graph-native JSON document stores an optional `structuralPrimitives`
collection. Missing or empty data loads as no primitives, preserving existing
levels. Default-valued fields should be omitted on save where that matches the
current serialization style.

Derivation produces immutable compiled primitive records and mappings from
compiled surfaces back to authored primitive IDs. Derived records may be
carried by `SectorTopologyMap` because that map is the renderer, collision,
runtime, lightmap, hit-testing, and display input, but it is not their editable
source of truth.

Static structural primitives are purpose-built map data, not ECS entities.
Runtime ECS remains reserved for higher-level or movable objects.

## Deterministic Geometry Generation

Primitive mesh generation runs during explicit authoring derivation, level
loading, preview rebuild, or another deliberate cache rebuild. Generated CPU
geometry is reused by rendering, picking, navigation, and lightmap preparation
where their attribute requirements overlap. GPU upload and unload remain on the
main thread through the existing preview/runtime resource lifecycle.

Every generated surface retains:

- source primitive ID and primitive kind
- a stable face role, such as top, bottom, side, tread, riser, or ramp top
- owning/intersecting sector IDs
- material and decal state
- positions, normals, base UVs, decal UVs, and lightmap chart UVs
- lightmap receiver and occluder participation
- alpha-test information if supported later

Generation order must be deterministic. Identical authored inputs must produce
the same surface ordering, triangle winding, UVs, sector membership, and
geometry fingerprint. This keeps saved metadata, lightmap layouts, source
hashes, tests, and debug output stable.

Normals point out of the solid. Triangle winding follows the convention used by
existing sector geometry. Degenerate faces are rejected rather than uploaded.

### UV conventions

UVs use world-scaled planar mapping consistent with generated sector surfaces:

- prism and stair faces map in their natural face plane
- ramp tops map along width and incline distance
- cylinder sides map angle to U and height to V; caps use planar mapping
- spheres use deterministic spherical mapping with a fixed seam relative to
  local orientation

Top, bottom, and side material overrides may be supported without requiring a
different mesh representation. A single material remains the compact default.

## Rendering Integration

Structural geometry should feed draw records compatible with the existing
sector material/decal/lightmap shader path. It must not introduce a second
lighting model merely because its geometry came from a primitive generator.

The generated draw cache is built once per preview/runtime rebuild and grouped
by sector visibility ownership plus the existing material, decal, alpha-test,
and lightmap state. Rendering performs only cached draw-record selection,
material binding, and drawing.

Structural surfaces receive:

- sector/material-registry textures and missing-material fallback behavior
- baked surface lightmaps
- baked ambient occlusion
- dynamic point and spot lighting
- supported dynamic shadows
- decal and bloom behavior where the material path permits it

Generated structural geometry participates in preview surface picking. A hit
must resolve to the primitive ID and stable face role so later editor tools can
select and inspect the authored object without treating generated triangles as
independent editable topology.

## Collision and Movement

The ordinary sector collision world continues to provide planar sector loops,
linedef barriers, portal transitions, and the base floor/ceiling interval.
Structural collision is an additional cached query layer combined with that
base context.

This matches the useful behavior already present for collision-enabled static
models: a reachable collider top can become the local support floor, while a
collider underside above the player can become the local ceiling. Structural
primitives make that behavior deterministic and independent of an imported
model's bounds.

At a given XZ position and actor height, collision queries resolve the nearest
usable support below/reachable by the actor and the nearest blocking surface
above it. This permits a player to occupy the space below a bridge or the space
on top of it while remaining in the same planar sector.

Required collision behavior includes:

- swept horizontal collision against solid primitive boundaries
- step-height checks when approaching raised structural tops
- local support-height queries for flat, ramp, and stair support surfaces
- headroom checks against undersides and overhanging structures
- stable grounded movement up and down ramps/stairs
- player-footprint-aware edge handling
- collision participation for NPC movement
- world/weapon ray intersection against structural surfaces
- prism-placement and spawn-clearance checks where those systems need physical
  world occupancy

The existing static-model oriented-prism logic is a strong reference and may be
shared or generalized for rectangular primitives. Structural collision data
must not depend on loading a raylib `Model` or computing asset bounds.

Ramp collision evaluates the inclined support height from local ramp
coordinates. Entry through the low edge is allowed when the resulting support
change is reachable; the high end and side faces behave as walls when the actor
cannot step onto the surface. Movement must use the same support function for
grounding, headroom, and navigation validation to avoid visible/collision
disagreement.

Decorative primitives with collision disabled do not enter physical query
caches.

## Navigation Integration

Navigation build input includes structural geometry during the same explicit
rebuild that currently includes sector surfaces and collision-enabled static
model bounds.

- prism tops are walkable when they provide sufficient clearance
- ramp tops use their actual slope and obey the agent maximum-slope limit
- staircases contribute their smooth support ramp by default
- sides, bottoms, risers, and solid interior faces are non-walkable
- cylinder/sphere surfaces are classified from their normals and configured
  collision role rather than assumed walkable

Navigation source hashes include every structural parameter that changes
walkable or blocking geometry. Debug geometry retains the source primitive ID
so a navigation diagnostic can identify the authored primitive responsible for
an island, obstruction, or rejected span.

## Sector Membership and Portal Visibility

A structural primitive may intersect more than one planar sector. Derivation
computes a deterministic set of owning/intersecting sector IDs from its
footprint or generated triangles instead of assigning only the sector
containing its origin.

The renderer draws the primitive when any associated sector is visible. This
prevents long bridges, beams, and slabs from disappearing when their center
sector is culled while another portion remains visible.

Structural primitives do not change the portal visibility graph or the
vertical opening stored for a linedef portal. Depth testing still produces the
correct image if a primitive partly obstructs a view; the conservative portal
graph may draw additional sectors behind it. A primitive that completely
blocks a portal therefore affects physical movement and raycasts but does not
become a visibility-graph edge blocker in the initial system.

If membership cannot be resolved for otherwise valid geometry, preview/runtime
should report a diagnostic and draw the primitive conservatively rather than
silently culling it.

## Lightmap and Lighting Contract

Structural primitives are static baked geometry. Their generated faces receive
baked light and AO, and opaque structural faces cast baked shadows. They also
participate in any static-model/object-probe occlusion paths where the existing
lighting design requires physical static geometry.

The lightmap source hash includes, in deterministic ID order:

- primitive kind, ID, enabled state, and transform
- all geometry-producing dimensions and tessellation values
- material IDs and UV settings that affect bake layout or alpha behavior
- shadow/receiver/occluder settings
- the deterministic geometry fingerprint where used

Changing any of those values stales the baked result. Pure editor display state
does not affect the source hash.

Lightmap chart creation uses stable primitive/face identity. Generated surface
order must not depend on unordered container iteration. Invalid generated
geometry must fail lightmap preparation clearly rather than producing a bake
whose render mesh differs from its occluders.

## Cache Invalidation and Lifecycle

Adding, deleting, moving, resizing, rotating, enabling, or materially changing
a structural primitive is a document edit. It invalidates all derived data that
depends on the primitive:

- structural render geometry and GPU meshes
- structural picking data
- structural collision caches
- sector-membership/visibility draw records
- navigation build input and navigation source hash
- baked lightmap status through the lightmap source hash

The existing 2D topology render cache should only be invalidated when it stores
or draws structural primitive state. Once the editor displays primitive
footprints or markers in that cache, every visible structural edit must use the
normal document-edited/cache-invalidation path. Preview rebuild remains
explicit; expensive geometry must not be rebuilt in the steady 2D draw path.

Generated GPU resources unload on the main thread before the raylib context is
closed. CPU generation may run during an explicit load/bake phase, but raylib
mesh upload and unload remain main-thread operations.

## Validation and Diagnostics

Derivation validates structural primitives before producing caches. Validation
must cover:

- stable, positive, unique IDs
- supported primitive kinds
- finite transforms, heights, dimensions, UV values, and yaw
- positive non-degenerate dimensions
- ordered ramp elevations and a usable ramp direction
- positive staircase rise/run and a valid step count
- bounded cylinder and sphere tessellation
- resolvable material IDs, with the same graceful fallback policy as sector
  surfaces
- deterministic sector membership or a conservative fallback diagnostic

Overlapping primitives are allowed. Intersections between primitives and
sector geometry are also allowed because beams, embedded pillars, trims, and
slabs commonly require them. The engine does not perform constructive-solid-
geometry cleanup; authors remain responsible for avoiding unwanted z-fighting
or inaccessible spaces.

Diagnostics should name the primitive ID and failure category. Invalid data
must not crash serialization, preview rendering, collision construction,
navigation building, or lightmap preparation.

## Testing Contract

Implementation coverage should use generated fixtures rather than user-edited
levels and include:

- serialization round trips, missing optional data, default omission, and
  malformed primitive diagnostics
- deterministic vertex, index, face-role, UV, and geometry-fingerprint output
  for every primitive kind
- correct outward normals and non-degenerate triangle winding
- sector membership for primitives inside one sector and spanning portals
- visibility when any intersecting owner sector is visible
- walking on and beneath a raised prism
- blocking at prism sides and insufficient-clearance undersides
- stable ramp ascent/descent, side blocking, and headroom
- smooth staircase traversal matching its visible endpoints
- NPC collision and navigation over bridges, ramps, and stairs
- structural world/weapon ray hits with primitive and face identity
- lightmap receiver/occluder inclusion and source-hash invalidation for every
  geometry-affecting field
- missing-material and invalid-primitive fallback behavior
- no structural mesh generation or dynamic allocation caused by steady-frame
  rendering

## Existing Engine Seams

The feature should build on these existing contracts rather than bypass them:

- `SectorAuthoringGraph` owns editable map state and derives
  `SectorTopologyMap`.
- `BuildSectorGeneratedGeometry()` and the sector mesh builder define generated
  surface attributes, cached draw records, materials, decals, and lightmap UVs.
- `SectorCollisionWorld` defines base sector/portal collision and sector lookup.
- `BuildSectorStaticModelVerticalContext()` and static-model movement collision
  already demonstrate walkable tops, blocking sides, and usable space beneath
  an oriented box.
- `SectorNavigationBuildInput` already rasterizes static-model box tops as
  walkable and their other faces as non-walkable.
- runtime portal visibility already culls sector-owned draw records and objects;
  structural geometry extends this with multi-sector membership.
- `ComputeSectorLightmapSourceHash()` and the static-model lightmap path define
  the required deterministic bake invalidation behavior.

These seams make structural primitives an additive generated-geometry system,
not a rewrite of sector topology.
