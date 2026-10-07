/*
 * 018-maps.h (issue 173k; the C twin of src/custom-client/lib/maps.lua)
 *
 * For a general audience: the game server knows how high the ground is
 * everywhere from files it extracts from the game (data-files/maps). Each
 * file is one tile of the world 533 1/3 yards square, holding heights at
 * 129 x 129 corner points and 128 x 128 cell centres; between them the
 * ground is four flat triangles per cell. This reads those files and
 * answers "how high is the ground here, and how steep?" with the very same
 * arithmetic the server uses (GridTerrainData.cpp, getHeightFrom*).
 *
 * World axes are the game's: x runs north, y runs west, z up. Tile indices
 * count from the world's north-west corner:
 *     tile x = floor(32 - x / 533.333...)    tile y = floor(32 - y / 533.333...)
 */
#ifndef EG_MAPS_H
#define EG_MAPS_H

#include "013-errors.h"

#include <pthread.h>
#include <stdint.h>

#define MAPS_GRID_SIZE      533.33333f       /* yards per tile (SIZE_OF_GRIDS) */
#define MAPS_RESOLUTION     128              /* cells per tile side (MAP_RESOLUTION) */
#define MAPS_INVALID_HEIGHT (-100000.0f)     /* the server's "no ground here" */

/* One tile. v9 and v8 are NULL for a tile of one flat height. */
typedef struct {
    int       tx, ty;             /* the tile's indices */
    int       is_flat;            /* 1: the whole tile is `flat` high */
    float     flat;
    float    *v9;                 /* 129*129 corner heights, index ix * 129 + iy */
    float    *v8;                 /* 128*128 cell-centre heights, index ix * 128 + iy */
    int       has_holes;
    uint16_t  holes[256];         /* 16 bits per 8 x 8-square cell group */
} MapTile;

/* A world on one map: tiles loaded as needed and kept. */
typedef struct {
    char      dir[512];
    int       map_id;
    MapTile  *tiles[64 * 64];     /* NULL: not tried; MAPS_NO_TILE: tried, none */
    pthread_mutex_t lock;         /* held while a tile is loaded into its slot: two threads ask (173u) */
} MapWorld;

#define MAPS_NO_TILE ((MapTile *)1)

void     maps_tile_of(float x, float y, int *tx, int *ty);
MapTile *maps_load_tile(const char *dir, int map_id, int tx, int ty, WWhy *why);
void     maps_free_tile(MapTile *t);
int      maps_is_hole(const MapTile *t, int row, int col);
/* Height at (x, y) inside tile t, and its rise per yard along world x and y. */
float    maps_height_in_tile(const MapTile *t, float x, float y, float *gx, float *gy);
void     maps_corner_position(int tx, int ty, float ix, float iy, float *x, float *y);

void     maps_world_init(MapWorld *w, const char *dir, int map_id);
MapTile *maps_world_tile(MapWorld *w, int tx, int ty);     /* NULL when there is none */
float    maps_world_height(MapWorld *w, float x, float y, float *gx, float *gy);
void     maps_world_free(MapWorld *w);

#endif
