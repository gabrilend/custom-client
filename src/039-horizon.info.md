# 039-horizon

*Written by `scripts/generate-info` from `039-horizon.h`; don't edit, re-run it.*

039-horizon.h (issue 173s) - the far horizon, from the map's low-detail heights

For a general audience: besides the full ground files, each map keeps
one small file for the whole continent (World\Maps\<map>\<map>.wdl):
for every tile a coarse 17 x 17 grid of heights, one every 33 yards. The
original draws it beyond the loaded tiles so distant mountains show on
the horizon. This draws it out to HORIZON_REACH tiles round her,
wherever the full ground isn't loaded, shaded in the fog's colour (so the
mountains stand as soft silhouettes against the sky).

Layout (3.3.5a): MAOF, 64 x 64 file offsets (tile x * 64 + tile y; 0:
none), each to an MARE block: 17 x 17 corner heights then 16 x 16 centre
heights (signed 16-bit, yards), rows running south and columns east as
in the ground files; then MAHO: 16 rows of 16 hole bits (lowest bit the
first column).

## What it offers

- `int horizon_init(MpqChain *chain, const char *map_name, double ox, double oy, char *why, int why_len);`
  Reads the map's low-detail heights; the chain may be closed afterwards.
  0, or 1 with `why` (a map with no .wdl draws no horizon: said, not an
  error).
- `void horizon_draw(int tx, int ty, int (*loaded)(int x, int y), Vector3 eye, const LightNow *light);`
  Draws every low-detail tile within reach of tile (tx, ty) for which
  `loaded(x, y)` says the full ground isn't there.
- `float horizon_corner(int tx, int ty);`
  The low-detail height at the north-west corner of tile (tx, ty), for the
  tests; below -10000 when the tile has none.
