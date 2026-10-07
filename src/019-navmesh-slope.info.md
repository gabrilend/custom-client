# 019-navmesh-slope

*Written by `scripts/generate-info` from `019-navmesh-slope.h`; don't edit, re-run it.*

019-navmesh-slope.h (issue 173k; the C twin of lib/navmesh-slope.lua)

For a general audience: the server plans every creature's path on its
navigation mesh, built with one rule for how steep a slope can be and
still count as ground; the rule is written into the header of every
navigation tile file (the .mmtile files in data-files/mmaps). Reading it from there
means client and server share it. (The owner, 2026-10-05: monsters and
players may need different slope rules; the player's will come from the
original client once measured, 173e.)

Tile header (MmapTileHeader, MapDefines.h; little-endian): 0 magic
"MMAP" (0x4d4d4150), 8 mmap version (19), 20 walkable slope (float, degrees).

## What it offers

- `float navmesh_slope_read(const char *dir, int map_id, WWhy *why);`
  The walkable slope in degrees from map_id's first tile in dir; < 0 on
  failure, with the reason in `why`.
