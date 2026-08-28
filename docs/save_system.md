# Save and Load System

The game exposes 12 fixed save slots in the main menu. Save files and their
thumbnail PNGs live in the current user's application-data directory rather
than in the asset tree. Each slot stores a user-provided name and a UTC
timestamp; the UI displays that timestamp in local time as
`YYYY-MM-DD HH:MM` using a 24-hour clock.

Save JSON is explicitly versioned. Slot JSON replacement uses a temporary
file and a rollback backup so an interrupted overwrite does not normally
destroy the previous slot. Files with an unsupported version, invalid data,
or an unsafe thumbnail path remain visible as unavailable/corrupt slots
instead of being loaded.

## Persisted state

A save contains campaign-wide player inventory, equipped weapon and magazine
state, timed healing effects, collected and dropped items, player health,
stamina, position and view angles. It also contains the campaign-wide Lua
persistent bool, integer, and string stores.

Each visited level has an engine-owned snapshot keyed by stable authored
object IDs and string instance IDs. The snapshot contains door position and
target state, dynamic/static prop presentation state, model animation clip and
frame state, billboard animation state, dynamic-light state, trigger state,
and NPC physical/death/despawn/patrol state. NPC awareness and active combat
targeting intentionally reset when a save is loaded.

The Lua VM, coroutine stacks, active Lua tasks, and pending script operations
are not serialized. On load, the engine restores the map and runtime object
state first, creates a fresh Lua VM, and then invokes the map `init()` hook.
Scripts should use persistent campaign values to reconstruct higher-level
story state that spans levels.

## Saving gate

Saving is permitted only while a campaign is active in regular gameplay. The
main-menu Save item remains visible but disabled when the session reports a
save block; hovering it displays the block reason. Cutscene integration can
use `SectorGameSession::SetSaveGameBlocked(true, reason)` and clear the gate
when the cutscene ends. This is deliberately a C++ integration point for now;
no new Lua binding is exposed.

Thumbnail capture reads the retained 8-bit scene-presentation render target,
before HUD and menu composition, flips it to image orientation, and scales it
to 320 by 180 pixels. Thumbnail loading and GPU ownership go through the asset
manager in a temporary save-menu scope.
