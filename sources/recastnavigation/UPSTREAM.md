# Recast Navigation Source Dependency

The vendored navigation sources come from the official Recast Navigation
repository:

- Repository: https://github.com/recastnavigation/recastnavigation
- Release: `v1.6.0`
- Commit: `6dc1667f580357e8a2154c28b7867bea7e8ad3a7`
- License: zlib; retained in `LICENSE.txt` beside this file

Source layout in this repository:

| Official module | Local directory |
|---|---|
| `Recast/Include` and `Recast/Source` | `sources/recast` |
| `Detour/Include` and `Detour/Source` | `sources/detour` |
| `DetourTileCache/Include` and `DetourTileCache/Source` | `sources/detour_tile_cache` |
| `DetourCrowd/Include` and `DetourCrowd/Source` | `sources/detour_crowd` |

The initially copied Recast and Detour core files were verified byte-for-byte
against the official `v1.6.0` archive on 2026-08-14. DetourTileCache and
DetourCrowd were then copied from that same archive without modification.

`DebugUtils`, `RecastDemo`, upstream tests, and build-system files are not
vendored because the engine uses its own raylib debug visualization, tests, and
CMake target.

When updating this dependency, update all four modules from one upstream
revision, update the release and commit above, and run the full build and test
suite. Do not mix core Detour, TileCache, or Crowd revisions.
