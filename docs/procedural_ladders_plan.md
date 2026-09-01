# Procedural Ladders Implementation Plan

## Summary

Add ladders as an authoring-owned structural primitive. They use the existing generated-surface pipeline for PBR materials, baked lighting, dynamic shadows, reflection-probe capture, collision, serialization, and editor selection.

The feature lands in three independently testable phases:

1. World representation, procedural geometry, rendering, collision, and serialization.
2. 2D editor placement/selection plus inspector and 3D adjustment integration.
3. Shared gameplay/editor-FPS interaction with mounting, constrained climbing, and animated dismounting.

Use a dedicated traversal state with derived mount/dismount anchors rather than generating invisible platforms. This follows the general dedicated ladder-movement approach used by [Source SDK](https://github.com/ValveSoftware/source-sdk-2013/blob/master/src/game/shared/gamemovement.cpp) and [Quake II](https://github.com/id-Software/Quake-2/blob/master/qcommon/pmove.c). Invisible platforms would alter navigation and collision for unrelated actors; real level geometry remains responsible for supporting the player after dismount.

## Data and Interface Contract

- Add `Ladder` to `SectorStructuralPrimitiveKind` and add `SectorStructuralLadderParameters` to `SectorAuthoringStructuralPrimitive`:
  - `width`: total outside width, default `5 * SectorCoordSubdivisions` (0.625 world units).
  - `bottom`: authored height, default `0`.
  - `height`: rail length, default `20` authored units (2.5 world units).
  - `thicknessScale`: normalized scalar, default `1.0`.
  - `rungCount`: default `8`, valid range `2..64`.
- At scale `1.0`, use a `0.08` world-unit square frame cross-section and `0.04` world-unit rung diameter. Scale both values uniformly.
- Define local `+Z` as the ladder's front/approach direction. Width runs along local X and height along Y. Pitch and roll remain zero.
- Position rungs at `bottom + height * (index + 1) / (rungCount + 1)`, producing equal clearances at the top and bottom.
- Use the default structural material as the frame material and a ladder-rungs override as the second material. Ladder UV scale/offset are fixed and not exposed.
- Add a stable ladder use target and a plain-data traversal state; do not create ECS entities or retain pointers into topology containers.

## Phase 1 - World Representation and Rendering

- Extend structural compilation so a ladder produces two upright box rails and horizontal cylinders spanning their inner faces. Reuse the box and cylinder UV conventions and deterministic structural face IDs.
- Feed all surfaces through generated structural geometry, providing standard PBR, lightmaps, dynamic shadows, environment/reflection-probe sampling, and inclusion in probe captures.
- Preserve the structural `collision`, `receivesLightmap`, `castsBakedShadow`, and `castsDynamicShadow` flags.
- Represent collision-enabled ladders as thin oriented rectangular slabs covering their width, depth, and height. Mark this collision as a non-walkable navigation obstacle; NPC climbing is out of scope.
- Add the ladder parameters, materials, lighting flags, and geometry fingerprint to the lightmap source hash and bump its structural version. Reflection probe state remains dependent on the resulting baked-lightmap source.
- Add generated-data tests for defaults, validation, serialization, surface IDs and UVs, material separation, collision, navigation, and source-hash invalidation.

## Phase 2 - Editor and 3D Mode Integration

- Add a dedicated Ladder button to the 2D Map Objects tool pane. Placement is one click: snap X/Z, resolve the containing sector floor as `bottom`, validate and commit through the authoring service, select the new ladder, and return to Select.
- Draw a cached oriented footprint with rail lines and a front-direction marker. Register it as one structural pick candidate and reuse the existing selection stack for overlaps.
- Reuse Select-tool body dragging for X/Z movement and the existing yaw handle. Dimensions remain inspector-authored initially.
- Show Enabled, Position X/Z, Bottom, Height, Width, Thickness Scale, Rung Count, Yaw, Collision, lightmap/shadow flags, Frame Material, and Rung Material. Hide pitch, roll, segment, and UV controls.
- In 3D Ctrl+A mode, retain translation, vertical movement, and yaw; hide and ignore pitch/roll controls.
- Route every placement, drag, deletion, inspector edit, and 3D adjustment through the structural authoring commit path so accepted changes mark the document edited, invalidate the topology render cache, and refresh preview geometry/collision.

## Phase 3 - Interactivity

- Implement one shared ladder interaction module for the game and editor FPS preview. Free-fly mode does not activate ladders.
- Use `Inactive`, `Mounting`, `Climbing`, and `Dismounting` states keyed by stable ladder ID. Derive bottom/front and top/rear interaction anchors so ladders can be mounted from either end.
- Merge ladders into the normal E-use targeting using range, facing, line of sight, and deterministic ID tie-breaking. Display `Climb Ladder` or `Descend Ladder`.
- Mount by validating the capsule path and interpolating the physical controller position onto a rail in front of the ladder over 0.30 seconds. Interpolate yaw toward the ladder and pitch toward level.
- While climbing, disable gravity and normal horizontal movement. W/S move at 1.5 world units per second, X/Z remain on the rail, yaw is clamped to +/-60 degrees, and jump/crouch/sprint/weapons/headbob/footsteps are suppressed.
- At the bottom, dismount forward. At the top, dismount over the ladder toward the upper landing. Validate the capsule path while excluding only the active ladder slab. Clear exits animate over 0.30 seconds; unsupported exits restore gravity and naturally fall. Obstructed exits keep the player attached and allow reversing direction.
- Clear traversal on level reload, preview-mode change, death/respawn, controller takeover, or loss of the referenced ladder. Existing non-ladder movement and physics remain unchanged.

## Verification

For each C++ phase, run:

- `cmake --build cmake-build-debug -j2`
- `ctest --test-dir cmake-build-debug --output-on-failure`
- `git diff --check`
- `git diff --stat`
- `git status --short`

Tests use generated topology or immutable fixtures, never user-edited levels. Automated GUI/xdotool smoke testing is not part of the task; manual verification covers placement, selection stacks, inspector behavior, yaw-only adjustment, PBR/lightmaps/reflections, two-ended mounting, constrained looking, climbing, dismounting, and unsupported-top falling.
