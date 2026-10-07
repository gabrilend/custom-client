# 035-creatures

*Written by `scripts/generate-info` from `035-creatures.h`; don't edit, re-run it.*

035-creatures.h (issue 173t) - where the world's creatures stand

For a general audience: until the client talks to a server, the list of
creatures (guards, vendors, deer, wolves) comes from a file that
scripts/generate-custom-client-creatures writes out of the server's
world database: one line per creature, saying which creature it is,
which look to draw, how big, where it stands and which way it faces.
This reads that file and files the creatures by map tile, so the client
can bring in a tile's creatures when it loads the tile, and drop them
with it. Nothing here draws.

The file: lines starting "#" are comments; every other line is
tab-separated: guid, entry, display, size, x, y, z, facing (radians),
name.

A Spawn, field by field:
  guid      uint32_t: the spawn's own number in the database
  entry     uint32_t: which creature (creature_template)
  display   int: the look to draw (CreatureDisplayInfo.dbc id)
  size      float: its size, times the display's own
  x, y, z   double: where it stands (world yards)
  facing    float: radians, as hers (0 = north, counter-clockwise)
  name      char[64]: its name, for messages
  tile      int: its map tile, tx * 64 + ty (018-maps.h's numbering)
A SpawnList holds them sorted by tile.

## What it offers

- `type Spawn`
- `type SpawnList`
- `int creatures_read(const char *path, SpawnList *out, char *why, int why_len);`
  Reads the file at `path`. 0 read; 1 there is no such file (`why` says
  how to make one: the client runs without creatures); -1 a line that
  can't be read (`why` names its number).
- `int creatures_in_tile(const SpawnList *l, int tx, int ty, int *first);`
  The spawns standing in tile (tx, ty): `*first` the first's index, the
  count returned (0: none).
- `void creatures_free(SpawnList *l);`
