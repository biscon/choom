# NPC Navigation Architecture

NPC navigation is level-owned derived runtime data. `SectorTopologyMap` and
resolved collision inputs remain the source of truth; Detour objects and debug
geometry are never authored or serialized.

## Ownership and lifecycle

`SectorSceneRuntime` owns one `SectorNavigationWorld`, one
`NpcNavigationRuntime`, and one bounded `NpcAiRuntime` stimulus buffer. A level
or preview rebuild follows this order:

1. release NPC paths, Crowd agents, and door holds;
2. shut down the old navigation world;
3. rebuild renderer resources and runtime objects as required;
4. initialize navigation with map-derived settings and reserved capacities;
5. queue a navigation build and initialize NPC records;
6. wait for terminal static-model collision readiness, then build within the
   configured per-update tile budget.

Map unload and renderer/context teardown release NPC navigation before Detour
and ECS runtime objects. Script shutdown cancels active operations before scene
teardown. An explicit rebuild immediately invalidates handles and releases the
old derived mesh; it never leaves the previous mesh queryable while reporting
`Queued` or `Building`. Active NPC/script movement fails clearly after a
rebuild, even when a small rebuild completes in the same frame.

Navigation data is rebuilt on each explicit level/preview load. There is no
persisted navigation cache. This keeps platform-specific Recast/Detour layouts
out of level files and makes topology the only authoring truth.

## Runtime update order

The scene updates door traversal intent/holds, synchronizes door links,
advances the bounded navigation build, reconciles dynamic TileCache obstacles,
updates NPC paths and locomotion, then updates remaining runtime object state.
Crowd only supplies local steering. Physical positions still come from the
topology-based `SectorCollisionWorld`; visual offsets never affect navigation,
sector lookup, or physics. Navigation locomotion enables the collision world's
grounded drop constraint: downward portal changes up to the configured agent
maximum climb remain valid stair connections, while larger drops behave as
blocking ledges. This prevents avoidance steering from moving a non-falling NPC
off the navigation surface; NPC gravity and airborne traversal remain out of
scope.

NPC AI is evaluated only when `SectorGameSession` supplies an explicit
`NpcAiGameplayContext`. Editor Gameplay and FreeFly previews call the same scene
runtime without that context, so they retain animation, collision, navigation
authoring, and audio behavior without running perception or AI decisions. In a
game frame the generic AI host ages sound events, evaluates perception and
investigation, calls the selected AI-type callback for an intent, then lets the
existing navigation and animation systems execute that intent.

## Perception, investigation, and AI-type boundary

`NpcAiState` is plain per-entity data. It stores the selected type ID, copied
definition settings, generic awareness state, last-known position, and the
small amount of committed-attack state needed by the host. `NpcAiTypeRegistry`
is the stable extension boundary: a descriptor declares ID, editor label,
friendly/hostile compatibility, awareness use, and a function that receives a
restricted `NpcAiPluginInput` and returns an `NpcAiIntent`. AI-type callbacks do
not query the ECS, collision, scripts, audio, or player health directly.

The host owns all shared perception. Vision is a definition-authored horizontal
cone plus range and direct LOS through sector, closed-door, and static-model
collision. Player sound events are scene-owned, short-lived, pre-reserved
records with a source radius. Hearing requires both the event radius and the
NPC's authored hearing range to reach. Footsteps, landings, and shots publish
events; weapon damage also supplies the exact shot origin as a direct generic
stimulus. A standing player is detected immediately when the geometric vision
checks pass, regardless of light level. While crouch is targeted, generic
vision instead uses the configured light, darkness-proximity, buildup/decay,
and crouch-response scalers, and movement sounds use the configured crouch
noise multiplier. Sneak mode follows the accepted crouch toggle state rather
than waiting for the short visual crouch transition to finish.

Detected or investigating AI owns navigation authority. Script movement cannot
replace an active AI path, and a patrol coroutine that wakes after detection
receives the normal AI-takeover failure from its next move request. Unaware NPCs
continue to accept ordinary script movement.

Loss of sight does not immediately forget the player. Generic investigation
runs to the last-known position, performs a deterministic turning search for
the authored duration, then returns to unaware. Hearing begins at investigation
rather than exact player tracking. Reacquired sight changes immediately to
detected. Any first awareness transition cancels a script-owned NPC move; the
blocking or awaited Lua operation resumes with `false` and the AI-takeover
reason.

The first registered type is `seek_and_destroy`. When detected it returns a
run-to-player intent outside melee range and an attack intent inside range. Its
plugin input includes the previous intent so the type can own its state-change
hysteresis without moving that policy into generic perception. Seek & Destroy
enters melee within `0.10` world units beyond the authored range and remains
engaged until `0.25` world units beyond it. This cancels chase before crowd and
physical stopping tolerances can oscillate at a single exact boundary.
Successful pursuit retargets replace the path atomically while preserving the
NPC's current steering velocity and accumulated footstep distance. Only a new
AI move starts those locomotion values from rest, so frequent moving-target
updates do not repeatedly force the Crowd agent to accelerate from zero.

An attack is committed for its full non-looping animation. The authored
advance-speed multiplier begins windup movement from the NPC's Run speed and
smoothstep-decelerates it to zero at the normalized hit phase. This movement
uses the ordinary sector, door, static-object, NPC, and player-cylinder
collision path, but does not pathfind, use Crowd steering, select a locomotion
animation, or emit footsteps. The NPC tracks the player through the authored
aim-tracking phase and then commits its facing. The hit performs a fresh LOS
test, requires the player to remain within the authored forward arc, and
accepts a bounded `0.25` world-unit committed-swing range margin. Moving
farther away, strafing outside the committed arc, or gaining cover makes the
swing miss. A connection applies damage plus optional
knockback/stun through one per-attacker operation, dispatches one player-damage
event, applies the attack's optional directional spring camera impact, and
plays the optional player-impact sound spatialized from the attacker.
Player-damage events select from the globally loaded pain sound set, including
when multiple NPC hits land in one frame; their knockback impulses accumulate.
The separate optional spatialized attack sound plays when the committed attack
animation begins, independent of the later hit result. Positive weapon stagger
interrupts a swing; zero-stagger damage does not. The animation itself supplies
attack cadence. During a blended transition, loop and completion state belong
to the selected target clip: an outgoing Run/Walk clip keeps its own looping
state and cannot finish the Attack, Hurt, or Death action. This also guarantees
that attack-start audio is emitted once per genuine committed swing rather than
being retriggered by a blend restart.

AI freeze stops generic perception/investigation timers, AI path movement,
decisions, and active AI locomotion/attack animation. Sound events still age,
and script movement, combat knockback, hurt/death handling, and ambient audio
continue. God mode suppresses player damage, stun, and knockback while enemies
continue detecting, attacking, and playing authored attack audio.

`/debugai [on|off]` controls a read-only game-session diagnostic overlay. Its
world pass draws cyan hearing rings, a faint green maximum vision ring with a
bright exact vision cone, red melee range, magenta remaining path corners,
yellow last-known-player markers, fading amber player-sound radii, and active
patrol routes with their claimed waypoint destinations. Patrol geometry is
drawn for assigned non-hostile NPCs as well. Its HUD
pass projects fixed-size identity, awareness/intent/action, health, navigation,
distance, attack, and investigation text above each AI NPC. Labels intentionally
remain visible through walls for debugging, while world geometry remains
depth-tested. Dead NPCs retain an inactive label but no perception geometry.
The overlay is separate from F8 navigation diagnostics and is never called by
editor previews. Gameplay normally presents bloom's alternate HDR target
without copying it back. While either AI or F8 navigation world diagnostics
are visible, the renderer conditionally commits that result to the original
depth-bearing world target before drawing the lines. This debug-only copy keeps
the diagnostics depth-tested and in the presented image; it is skipped when
both overlays are off.

Passable portals between different floor heights receive deterministic
walkable-area variants during the derived navigation build. Straight-path
queries request area crossings, so authored stair treads retain an ordered
waypoint at every physical height transition even if Recast would otherwise
simplify the run into a single sloped polygon. All ground variants have
identical query flags and cost; the areas only preserve locomotion boundaries.

Locomotion progress is cumulative improvement toward the active waypoint,
rather than any tiny forward displacement. After 0.75 seconds without
meaningful improvement, the NPC temporarily follows its raw waypoint velocity
instead of Crowd steering while remaining a Crowd neighbor and retaining
authoritative physical collision. If recovery produces no progress by 1.5
seconds, the bounded replan policy applies. Advancing to a waypoint or
successfully replanning resets recovery state.

Normal-frame query and steering data use fixed arrays or pre-reserved vectors.
Agent, path, obstacle, diagnostic, and script-operation capacity overruns remain
safe development fallbacks and emit one-shot warnings where growth is allowed.
Diagnostic history is bounded, oversized messages are truncated, and the Nav
tab reports retained/dropped/truncated counts.

## Source hash and caches

The navigation source hash is versioned independently from the lightmap hash.
It includes normalized Recast settings, topology vertices and connectivity,
player-blocking linedefs, floor/ceiling heights, `ceilingSky`, resolved static
collision OBBs/fingerprints, and door geometry that changes cuts or link
staging. It excludes preview/camera settings, textures and other presentation,
sky visuals, lighting/lightmap metadata, audio, NPCs, dynamic props, and door
animation timing.

The navigation debug cache is derived from completed Detour tiles and bounded
runtime obstacle/link state. Expensive mesh extraction occurs during build or
changed-tile processing, not steady drawing. Source, build, tile, and debug
revisions have separate meanings. Navigation rebuild/debug actions do not
invalidate the 2D topology render cache, mark the authoring document dirty, or
change the lightmap source hash.

The AI debug overlay builds no persistent cache. When disabled, game rendering
returns at the session-owned boolean guard before ECS traversal, text
formatting, projection, path inspection, or draw calls. Enabling it reads the
existing bounded sound-event buffer and fixed navigation corner arrays without
changing navigation revisions or allocating per frame.

## Settings and diagnostics

The map preview step height supplies the navigation agent maximum climb. Other
defaults are a 0.25m radius, 1.6m height, 0.125m cells, 0.05m cell height, and
64-cell tiles. `SectorNavigationCapacitySettings` bounds agents, path records,
query nodes, tiles/layers, candidate triangles, TileCache temporary memory,
dynamic obstacles, diagnostics, and per-update work budgets.

The editor 3D preview Nav tab shows state/stage, source hash and revisions,
tile progress, build/update timing, memory/capacity data, Crowd and obstacle
counters, bounded diagnostics, door links, and the selected NPC. Rebuild is
disabled while a build is already queued or running. Debug toggles draw cached
surfaces, edges, tile bounds, obstacles, door links, step connections, paths,
corners, and agents. Surface and polygon-edge overlays use a small visual-only
vertical offset to avoid depth fighting with preview geometry.

Troubleshooting guidance:

- `Waiting for static collision`: allow model assets to reach ready or failed;
  failed/missing collision bounds are omitted with a warning.
- `Empty`: the map produced no walkable triangles; inspect sector topology,
  heights, and player-blocking boundaries.
- `Failed`: read the latest bounded diagnostic and capacity/memory counters.
- `Stale`: source data changed; request an explicit rebuild.
- partial/no path: inspect disconnected portals, climb, door-link state, and
  dynamic obstacles.
- capacity warning: raise the specific level-load reserve after confirming the
  expected content count; do not rely on runtime growth as normal behavior.

## Lua contract and current limits

Lua uses `moveNpc` for blocking movement and `startMoveNpc` with `await`,
`operationStatus`, or `cancelOperation` for asynchronous movement. Targets may
be runtime X/Z coordinates or exact compiled marker IDs. Requests require
ready navigation and report invalid NPCs, off-mesh targets, partial/no paths,
rebuilds, deletion, stalls, and capacity failures without exposing Detour
types.

Only one shared humanoid navigation profile and one AI type exist. Friendly,
ranged, fleeing, coordinated, and lighting-sensitive AI types can use the same
registry/host boundary but are not implemented. Crouch/jump/drop/climb, manual
links and area costs, moving platforms, save-game persistence of active moves,
and dynamic sector geometry remain out of scope.
Middle textures do not become navigation collision, and dynamic obstacles are
bounded box/cylinder TileCache inputs rather than arbitrary animated meshes.
