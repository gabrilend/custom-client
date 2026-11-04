/*
 * 034-prefetch.h (issue 173u) - reading ahead of her, on a second thread
 *
 * For a general audience: when she walks into a new map tile, the client
 * loads that tile's ground, buildings and doodads, and most of the time
 * that takes is unpacking their files out of the game's archives. This
 * runs a helper thread that is told which tiles she is likely to reach
 * next (the ring just beyond those loaded) and unpacks their files ahead
 * of time into the archive reader's memory (015-mpq.h): the tile's ground
 * file, each doodad's model and skin and the pictures it names, each
 * building's root, parts and pictures. When the tile really loads, its
 * files are already unpacked. When told to, it also prepares each such
 * tile's ground (033-ground-prep.h: its points, blend picture, and the
 * pictures not yet on the card) and keeps it until the main thread takes
 * it, leaving that thread only the sending to the card. The helper draws
 * nothing; what it shares with the main thread is the archive reader's
 * memory and the server's heights (each locked) and its own keeping
 * (locked here).
 */
#ifndef EG_PREFETCH_H
#define EG_PREFETCH_H

#include "033-ground-prep.h"
#include "015-mpq.h"
#include "060-resolver.h"

/* A reader-ahead for map `map_name` (its tiles are
 * World\Maps\<map>\<map>_<ty>_<tx>.adt, read through `assets`, or the
 * chain alone when it is NULL; the pictures and models they name from
 * the chain), its helper thread started
 * when `read_ahead` is 1; with 0 (or a thread that couldn't start, said on
 * the terminal) it reads nothing ahead and every take finds nothing
 * prepared. The chain and resolver must outlive it; freed with prefetch_free. */
typedef struct Prefetch Prefetch;
Prefetch *prefetch_start(MpqChain *chain, Resolver *assets, const char *map_name, int read_ahead);
void      prefetch_free(Prefetch *pf);

/* From now on, each tile read ahead has its ground prepared too, at
 * `place` (which must outlive the helper); `on_card` (any thread) says
 * which pictures are on the card already, so they aren't unpacked. */
void prefetch_prepare_ground(Prefetch *pf, const GroundPlace *place, int (*on_card)(void *ctx, const char *name), void *ctx);

/* Asks for tile (tx, ty) to be read ahead (once; repeats are ignored). */
void prefetch_tile(Prefetch *pf, int tx, int ty);

/* Waits until every asked-for tile is read (for the tests and pictures). */
void prefetch_wait(Prefetch *pf);

/* How many tiles the helper has read ahead, the files it unpacked, the
 * grounds it prepared, and how many of those the main thread took. */
void prefetch_counts(Prefetch *pf, int *tiles, int *files, int *grounds_made, int *grounds_taken);

/* Takes tile (tx, ty)'s prepared ground, for loading it now: 0 with
 * `*out` (the caller sends or frees it); 1 the map has no ground file
 * there; 2 not prepared (the caller prepares it itself). Waits if the
 * helper is preparing that very tile. Either way the helper won't prepare
 * it again until it is forgotten (prefetch_keep_near). */
int  prefetch_take_ground(Prefetch *pf, int tx, int ty, GroundPrep **out);

/* Forgets every tile more than `reach` tiles from (tx, ty): frees its
 * preparation, and lets it be asked for again later. */
void prefetch_keep_near(Prefetch *pf, int tx, int ty, int reach);

/* Stops the helper (it finishes the tile it is on); prefetch_free stops it too. */
void prefetch_stop(Prefetch *pf);

#endif
