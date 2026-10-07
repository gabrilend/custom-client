# 018-maps

*Written by `scripts/generate-info` from `018-maps.h`; don't edit, re-run it.*

018-maps.h (issue 173k; the C twin of src/custom-client/lib/maps.lua)

For a general audience: the game server knows how high the ground is
everywhere from files it extracts from the game (data-files/maps). Each
file is one tile of the world 533 1/3 yards square, holding heights at
129 x 129 corner points and 128 x 128 cell centres; between them the
ground is four flat triangles per cell. This reads those files and
answers "how high is the ground here, and how steep?" with the very same
arithmetic the server uses (GridTerrainData.cpp, getHeightFrom*).

World axes are the game's: x runs north, y runs west, z up. Tile indices
count from the world's north-west corner:
    tile x = floor(32 - x / 533.333...)    tile y = floor(32 - y / 533.333...)

## What it offers

- `type MapTile`
  One tile. v9 and v8 are NULL for a tile of one flat height.
- `type MapWorld`
  A world on one map: tiles loaded as needed and kept.
- `void maps_tile_of(float x, float y, int *tx, int *ty);`
- `MapTile *maps_load_tile(const char *dir, int map_id, int tx, int ty, char *why, int why_len);`
- `void maps_free_tile(MapTile *t);`
- `int maps_is_hole(const MapTile *t, int row, int col);`
- `float maps_height_in_tile(const MapTile *t, float x, float y, float *gx, float *gy);`
  Height at (x, y) inside tile t, and its rise per yard along world x and y.
- `void maps_corner_position(int tx, int ty, float ix, float iy, float *x, float *y);`
- `void maps_world_init(MapWorld *w, const char *dir, int map_id);`
- `float maps_world_height(MapWorld *w, float x, float y, float *gx, float *gy);`
- `void maps_world_free(MapWorld *w);`
