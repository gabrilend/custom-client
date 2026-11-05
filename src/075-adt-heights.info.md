# 075-adt-heights

*Written by `scripts/generate-info` from `075-adt-heights.h`; don't edit, re-run it.*

075-adt-heights.h (issue 1109; 018-maps.h's other source) - the
ground's heights from a map's own ground files

For a general audience: she stands and walks on the server's idea of
the ground: its map files (018-maps.h), which the server's extractor
makes from the game's ground files (the ADTs, 020-adt.h). A map just
converted from Warcraft III by world-edit-to-execute has ground files
but no server map files yet (its W02d makes them). For a test-map run
this makes the server's tiles itself, from the map's ground files, the
way the extractor does: each ADT tile's 16 x 16 chunks hold 9 x 9
corner heights and 8 x 8 cell-centre heights, which laid side by side
are the server tile's 129 x 129 corners and 128 x 128 centres; each
chunk's hole bits are the tile's holes for that chunk. Then the very
same arithmetic (018's maps_height_in_tile) answers "how high is the
ground here", so the heights agree with what the server will have once
W02d writes its files.

Which corner is which: a chunk's heights run in rows southwards (row
r is r / 8 of a chunk south of its north edge) and in columns
eastwards; the server tile's corner (ix, iy) is ix / 128 of a tile
south and iy / 128 east of the tile's north-west corner. So chunk
(row cy, column cx)'s corner (r, c) is the tile's (cy * 8 + r,
cx * 8 + c), and its height is the chunk's base height plus MCVT's.

Window-free: in the test program and the executable (it reads through
the resolver, which isn't in the reader library).

## What it offers

- `WErr adt_heights_from_tile(const AdtTile *adt, int tx, int ty, MapTile **out, WWhy *why);`
  Tile (tx, ty)'s heights and holes from its read ground file, as the
  server's map file would hold them (never flat). W_OK with `*out`
  (freed with maps_free_tile); W_ERR_CORRUPT when a chunk's own column
  and row aren't its place in the file.
- `typedef struct AdtHeights AdtHeights;`
  A map's ground files as a tile source for 018's maps_world_init_source
  (the handle: docs/001, "Handles"): its files World\Maps\<dir>\<dir>.wdt
  and <dir>_<ty>_<tx>.adt (the west-east number first), read through
  `assets` (asset_read: the resolver when given, else `chain` alone).
  Opening reads the WDT (098) for the blend maps' form and the tile
  list, so a map with none stops here: W_ERR_NOT_FOUND, naming it (a WDT
  that isn't one: W_ERR_CORRUPT). Both must outlive it.
- `WErr adt_heights_open(Resolver *assets, MpqChain *chain, const char *map_dir, AdtHeights **out, WWhy *why);`
- `void adt_heights_free(AdtHeights *h);`
- `MapTile *adt_heights_tile(void *ctx, int map_id, int tx, int ty, WWhy *why);`
  The source itself (a MapTileSource; `ctx` an AdtHeights): tile (tx, ty)
  read and made, or NULL with W_ERR_NOT_FOUND where the map has no ground
  file (open sea: its WDT lists none there, and then no file is asked
  for; or no place has the one it lists), another code for a file that
  can't be read. Safe from
  either thread.
- `int adt_heights_made(const AdtHeights *h);`
  How many tiles were made, for the run's report.
