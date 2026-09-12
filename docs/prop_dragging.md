# Paths and prop dragging

Choose **Path** under Map Options and click snapped waypoints. Press Enter to
commit at least two points. Escape or right-click cancels the pending path.

With Select, click a path segment, then drag the segment to move the whole path
or drag a waypoint handle to move that point. A click or small mouse jitter
cycles through overlapping paths and objects; dragging starts after 4 screen
pixels of movement. This lets you select a prop beneath its assigned endpoint.
Delete removes the selected
waypoint, reconnecting its neighbors; at least two points must remain. With the
path selected, choose Insert Waypoint in its inspector and click a segment.
The first endpoint is green, the last orange, and arrows show increasing order.

Give the path a unique ID in its inspector. IDs accept 1–63 letters, digits,
underscores, or dashes. Renaming preserves prop assignments. Assigned paths
cannot be deleted until their props are unassigned.

On a dynamic prop, choose its Drag path and initial Start/End endpoint. Placement
follows that endpoint; edit the path to change X/Z. Keep the prop's authored
rotation, scale and height offset appropriate for its collision bounds. Drag
speed defaults to 0.5 m/s. Collision is required while assigned.

Paths are open polylines. Dragging supports level ground with fixed prop
orientation. Steps, slopes, drops, and insufficient clearance stop movement.
Leave space for both the prop and the player on the side they will grab. Objects
resting on top of a moving prop do not travel with it.

In gameplay or gameplay preview, approach a prop and press E at **Drag [Use
Title]**. W pushes away along the path and S pulls back. The player stays on the
grabbed side through bends. On grab, the view centers toward the prop over
0.2 seconds. Mouse look allows 40 degrees left/right and 25 degrees up/down
around that direction; it stays where you leave it within those limits. Release
restores unrestricted mouse look without changing the current view direction.
Release and re-grab to change sides.

Movement accelerates to the configured speed over 0.3 seconds. Releasing W/S
slows the prop to rest over at most 0.2 seconds. Reversing first brakes the old
motion, then accelerates in the new direction. The prop brakes before path
endpoints; short paths may never reach full speed. Intermediate waypoints do
not trigger a stop. E releases and stops immediately, as do collision and
forced teardown. At endpoints the object remains grabbed so you can reverse.
Weapons, strafing, jumping, sprinting and crouch changes are unavailable during
a grab. UI/control capture, death, unloading, or leaving gameplay preview ends it.

Assigning a drag path gives dragging ownership of Use: the existing On Use
script and Single Use flag are inactive until the path is unassigned. No new Lua
bindings are added. Place reward items or entrances behind/beneath the prop;
its collision bounds conceal Use targets until the obstruction is moved clear.

Start, Moving and End sound IDs are optional map Sound entries. Start plays on
grab; Moving loops only during actual motion, including deceleration; End plays on E release. Idle,
blocked and endpoint states stop the movement loop. Missing/failed sounds are
silent. Teardown stops audio without a release sound.

Saves and level revisits retain distance along the path. Loading restores the
prop released with zero velocity, including saves taken while dragging. Old saves use the authored
endpoint. Changed path lengths clamp saved distance; missing or reassigned paths
fall back to authored placement with a diagnostic. Runtime dragging never edits
the level document. Paths and dragging settings do not affect the baked lightmap
source hash, and existing patrols remain separate.
