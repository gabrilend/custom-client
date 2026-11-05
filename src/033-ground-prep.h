/*
 * 033-ground-prep.h (issue 173u) - a tile's ground made ready, without the window
 *
 * For a general audience: loading a tile's ground has two halves. The
 * first is arithmetic: read the tile's ground file, work out every point
 * of its mesh (place, colour, and its light from the server's slopes),
 * pack its 256 blend maps into one picture, and unpack the ground
 * pictures it names. The second is handing all that to the graphics card.
 * Only the second half needs the window's thread; this file is the first
 * half, so the reading-ahead thread (034-prefetch.h) can do it for tiles she
 * hasn't reached yet, and the main thread is left only the sending
 * (038-ground.h). With no window it is also what the tests check.
 *
 * Every function here is safe on any thread: it touches the archive
 * reader (locked) and the server's heights (locked where they load), and
 * nothing else that is shared.
 */
#ifndef EG_GROUND_PREP_H
#define EG_GROUND_PREP_H

#include "013-errors.h"

#include "020-adt.h"
#include "018-maps.h"
#include "015-mpq.h"
#include "060-resolver.h"
#include "078-wc3-ground.h"

/* Where tiles are prepared: the archives, the resolver (NULL: the
 * archives alone; a test map's folder comes first through it, 1109), the
 * map's folder in them and its blend-map form (from its WDT), the
 * server's heights (for the light), and the drawing origin points are
 * placed relative to. The map's ground files (WDT, ADTs) and its
 * pictures are read through the resolver (pictures since 1109's step 1c:
 * a picture no place has stands in as the magenta checker, said); with
 * no resolver, the archives alone. `wc3`, when set (a converted map
 * whose own WC3 ground and tileset pictures were found: 078), paints
 * each chunk on the WC3 grid with WC3's tilesets instead of the ground
 * file's pictures; NULL (every other run) leaves the ground file's. */
typedef struct {
    MpqChain *chain;
    Resolver *assets;
    const Wc3Ground *wc3;
    char      map_name[64];
    int       big_alpha;              /* 1: the map's blend maps are the 8-bit form (020-adt.h) */
    MapWorld *world;
    double    ox, oy;
} GroundPlace;

/* One quarter of a tile (64 chunks of 145 points; 173u draws a quarter
 * in one order): the arrays a Raylib mesh takes, filled but for the
 * points' four layer numbers (which layer of the card's picture array a
 * picture lands in is only known when it is sent). */
typedef struct {
    float          *vertices;         /* 3 a point, relative to the origin */
    float          *normals;          /* 3 a point */
    float          *texcoords;        /* 2 a point: 8 repeats across a chunk */
    float          *texcoords2;       /* 2 a point: the chunk's square in the blend picture */
    unsigned char  *colors;           /* 4 a point: the chunk's tint (127 = unchanged) */
    unsigned short *indices;          /* 3 a triangle; `nindex` used */
    int             nindex;
    int             made;             /* 0: the quarter is all holes, nothing to draw */
    float           centre[3], radius;   /* its bounding ball, for the sight test */
} GroundQuarter;

/* A decoded ground picture: width x height RGBA, row by row, and its
 * smaller copies (each half the last, down to 1 x 1) for the card. */
typedef struct {
    unsigned char *rgba;              /* NULL: not unpacked (unneeded, or already on the card when prepared) */
    int            width, height;
    unsigned char *smaller;           /* the copies, smallest last, packed one after another; NULL when the
                                       * picture isn't square with a side a power of two (it is resized first) */
} GroundPixels;

/* A tile made ready to send. */
typedef struct {
    int           tx, ty;
    AdtTile      *adt;                /* the ground file read (handed on for the water and doodads) */
    GroundQuarter quarter[4];
    unsigned char *blend;             /* 1024 x 1024 RGBA: 16 x 16 chunks' blend maps, shadow in alpha */
    unsigned char needed[256];        /* 1: the tile's picture k is drawn by some chunk */
    GroundPixels  pixels[256];        /* by the tile's picture number */
    int           chunk_tex[256][4];  /* each chunk's four pictures, by the tile's picture number */
} GroundPrep;

/* Reads the map's WDT for its blend-map form and fills `p`. W_OK, or a code
 * with `why`. */
WErr  ground_place_init(GroundPlace *p, MpqChain *chain, Resolver *assets, const char *map_name, MapWorld *world, double ox, double oy, WWhy *why);

/* Prepares tile (tx, ty). `on_card`, when given, answers whether a
 * picture (by archive name) is already on the card (asked with `ctx`:
 * the ground it is asking about); those aren't
 * unpacked (sending finds them there, or unpacks them itself if they
 * have gone since). W_OK with `*out` (freed by ground_prep_free);
 * W_ERR_NOT_FOUND when the map has no ground file there (open sea: not a
 * failure, nothing to draw); another code, with `why`, for a fault. */
WErr  ground_prep_tile(const GroundPlace *p, int tx, int ty, int (*on_card)(void *ctx, const char *name), void *ctx, GroundPrep **out, WWhy *why);

/* Frees a prepared tile. `keep_adt`: 1 when the ground file was taken
 * by the caller (it then frees it with adt_free and free). */
void ground_prep_free(GroundPrep *g, int keep_adt);

/* Unpacks one ground picture by archive name into `px`, with its smaller
 * copies, from the place's resolver (asset_get: the checker for one no
 * place has) or, with none, its archives; a WC3 layer's name (ending in
 * WC3_LAYER_MARK) is its tileset's whole tile, from the place's `wc3`.
 * W_OK, or a code with `why` (W_ERR_NOT_FOUND: the archives lack it,
 * strict mode refused the checker, or a WC3 layer with no `wc3`). */
WErr  ground_prep_pixels(const GroundPlace *p, const char *name, GroundPixels *px, WWhy *why);

/* Makes a square picture's smaller copies (into px->smaller): each pixel
 * the average of the four beneath it, as the card itself would make them.
 * A picture not square, or whose side isn't a power of two, gets none. */
void ground_prep_smaller(GroundPixels *px);

#endif
