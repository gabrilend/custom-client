/*
 * 098-wdt.h (issue 1109 step 1s) - a map's tile list, from its WDT file
 *
 * For a general audience: a map's ground comes in up to 64 x 64 square
 * tiles, one file each (an ADT). The map's WDT file
 * (World\Maps\<dir>\<dir>.wdt) says which of them exist, and how they are
 * stored; for a map that is one building and no ground (a dungeon), it
 * names that building and where it stands. This reads it, so the client
 * knows the tiles from the map rather than by asking for every file and
 * seeing which are there.
 *
 * The file, for 3.3.5a (build 12340), as blocks (a four-letter tag stored
 * reversed, a 4-byte size, then the bytes):
 *   MVER  4 bytes: the version, 18
 *   MPHD  32 bytes: the map's flags first (the rest not read)
 *   MAIN  64 x 64 entries of 8 bytes, row by row, each a tile's flags
 *         (bit 0: it has a ground file) and 4 bytes the client fills in
 *         at run time
 *   MWMO  a WMO-only map's building, its file name (empty for a ground map)
 *   MODF  a WMO-only map's one placement of it, 64 bytes, as an ADT's
 *         MODF entry (020-adt.h's AdtPlace)
 * How sure each part is:
 *   checked from code: the blocks and their order, MVER 18, MPHD's flags
 *     word and its 0x4 (8-bit blend maps: 020 reads it, world-edit-to-
 *     execute's W02a writes it), MAIN's 8-byte entries row by row with
 *     bit 0 for a tile that has a file (W02a writes it), an empty MWMO
 *     for a ground map (W02a writes one); a MODF entry's layout as 020
 *     reads an ADT's;
 *   unchecked (recalled; neither project writes or reads them): MPHD's
 *     0x1 (a WMO-only map), MAIN's other bits, a WMO-only map's MWMO and
 *     MODF; no 3.3.5a file is in either project to measure.
 *
 * A tile is numbered as everywhere else in this client: tile x (north to
 * south) * 64 + tile y (west to east), the file
 * <dir>_<tile y>_<tile x>.adt (west-east number first), as 020's AdtWdl
 * and 039 number them. MAIN runs row by row, so its entry k is that tile
 * k. (W02a writes entry ty * 64 + tx for its file <dir>_<tx>_<ty>.adt,
 * its tx west to east: the same tile; checked with its own writer, 1109
 * step 1s.)
 *
 * Window-free, no state: in the reader library (libwreaders.so, for the
 * editor's LuaJIT FFI: the struct holds no pointers, nothing to free).
 */
#ifndef EG_WDT_H
#define EG_WDT_H

#include "013-errors.h"
#include "020-adt.h"

#include <stddef.h>
#include <stdint.h>

#define WDT_TILES          4096
#define WDT_VERSION        18u
#define WDT_MPHD_WMO_ONLY  0x1u   /* one building, no ground (unchecked: recalled) */
#define WDT_MPHD_BIG_ALPHA 0x4u   /* 8-bit blend maps (checked: 020, W02a) */
#define WDT_MAIN_HAS_ADT   0x1u   /* the tile has a ground file (checked: W02a writes it) */

typedef struct {
    uint32_t version;              /* MVER (18 for 3.3.5a: not refused when another) */
    uint32_t flags;                /* MPHD's flags word */
    uint32_t tile_flags[WDT_TILES];/* MAIN, per tile (tile x * 64 + tile y): its flags */
    int      ntiles;               /* tiles with WDT_MAIN_HAS_ADT */
    char     wmo[260];             /* MWMO: a WMO-only map's building ("" none) */
    int      has_place;            /* 1: MODF places it */
    AdtPlace place;                /* MODF's first entry, in world axes as an ADT's (020) */
} Wdt;

/* Reads a WDT file. W_OK with `*out`; W_ERR_CORRUPT naming what is wrong
 * (not a block file, a block running past the end, no MPHD, no MAIN or a
 * short one, a MODF shorter than an entry). `out` is about 17 KB. */
WErr wdt_read(const unsigned char *s, size_t len, Wdt *out, WWhy *why);

/* 1 when tile (tx, ty) has a ground file (MAIN bit 0), 0 otherwise or off
 * the 64 x 64 grid. */
int wdt_has_tile(const Wdt *w, int tx, int ty);

/* MPHD's 0x4: 1 when its blend maps are 8 bits a point (as
 * adt_wdt_big_alpha). */
int wdt_big_alpha(const Wdt *w);

/* MPHD's 0x1, or a building named with no tile listed: 1 for a map that
 * is one building and no ground. */
int wdt_wmo_only(const Wdt *w);

#endif
