/*
 * 078-wc3-ground.h (issues 302, 1109) - a converted map's ground painted
 * with Warcraft III's own tilesets, without the window
 *
 * For a general audience: world-edit-to-execute's converter turns a WC3
 * map's ground into the game's ground files (ADTs), painting each 33-yard
 * chunk with up to four of the game's own ground pictures it picks to
 * look like WC3's (Ashenvale grass for Lordaeron's grass ...). A player
 * who has Warcraft III can see the map's own ground instead: WC3's
 * tileset pictures, laid the way WC3 lays them. This works that out for
 * each chunk, so the ground drawer (038) draws it as it draws any chunk:
 * up to four pictures and a blend picture saying how strongly each
 * shows, point by point.
 *
 * The map's own ground (war3map.w3e, read and prepared by 063-w3e.h) is
 * a grid of tile points 128 WC3 units apart; the converter makes one WC3
 * cell one cell of a chunk (4.17 yards, 8 to a chunk side), by the rule
 * in map.lua (073-converted-map.h). Each WC3 cell lays up to four
 * tilesets, lowest first: the lowest whole, as the piece ("subtile") of
 * its picture its south-west point's variation picks (a plain picture's
 * two whole pieces, or one of an extended picture's 16 whole variations:
 * 063's w3e_variation), each higher one as the piece whose alpha covers
 * just the corners it is on. WC3 lays them cell by cell, so this does
 * too (wc3_ground_lay_tile): for each cell of a tile, its pieces, each a
 * picture (one of the tile's pictures: a tileset's picture, or each half
 * of an extended one, a square of 4 x 4 subtiles) and the subtile in it.
 * The ground drawer's shader (038, 080) reads them per cell, so a chunk
 * shows every tileset its cells lay, however many, with each cell's own
 * variation and each piece's own colours. A cliff cell with a WC3 cliff
 * model (079: when the player has TerrainArt\CliffTypes.slk) is laid
 * whole with its cliff tileset's ground (its groundTile, added to the
 * tilesets when the map's list lacks it), and its model is added to the
 * tile's doodads (wc3_ground_place_cliffs); a ramp's cell (079: a model
 * too) keeps its own ground. A blighted cell, when the editor's data
 * (UI\WorldEditData.txt's [TileSets], second field) names the main
 * tileset's blight picture, is laid as WC3 lays tilesets with the
 * blight a tileset of its own, highest, on each blighted corner; with
 * none named, and for a cliff with no cliff tileset or model (063 leaves
 * them bare for the renderer), a bare cell is laid whole with its
 * south-west point's tileset. The whole piece of each of these comes
 * from the cell's south-west point's variation too. A chunk with no cell
 * inside the WC3 grid keeps the converter's own pictures; a cell outside
 * it in a chunk with some inside is laid whole (subtile 15) with the
 * tileset the chunk's cells use most.
 *
 * Where the files come from: war3map.w3e by its name at the converted
 * build folder's root (world-edit-to-execute's convert copies it there
 * since its draft PR #246; for older builds a test copies it in),
 * TerrainArt\Terrain.slk (077-slk.h: each
 * tileset's folder and file) and the tileset pictures through the
 * resolver (060): the player's packs, the server's, overlays (the map's
 * own imports), the player's Warcraft III archives, then the checker.
 * With no Terrain.slk in any place (no Warcraft III install) nothing is
 * painted here and the converter's own pictures stay.
 *
 * Window-free: the test program and the executable. Opening reads and
 * decodes everything once; after it the handle is only read, so painting
 * is safe on any thread (the reading-ahead thread paints tiles too).
 */
#ifndef EG_WC3_GROUND_H
#define EG_WC3_GROUND_H

#include "013-errors.h"
#include "015-mpq.h"
#include "020-adt.h"
#include "060-resolver.h"
#include "063-w3e.h"
#include "079-wc3-cliffs.h"

#include <stdint.h>

/* The name a WC3 layer's picture has in a tile's picture list: its
 * tileset picture's path, then this mark (no game file's name holds a
 * "|"); an extended picture's right half adds WC3_RIGHT_MARK before it.
 * 033's ground_prep_pixels gives such a name its square of subtiles. */
#define WC3_LAYER_MARK "|wc3"
#define WC3_RIGHT_MARK "|right"

/* The blight's tileset id (no real tileset's has a "*"). */
#define WC3_BLIGHT_ID "*blt"

/* One ground tileset of the map. Field by field:
 *   id        its four letters ("Ldrt"), ended by a zero
 *   path      its picture (Terrain.slk's dir\file.blp), "" when the
 *             sheet has no row for it (the checker stands in)
 *   layer     its picture's layer's name (path, or TerrainArt\<id>.blp,
 *             and the mark): the whole picture, or an extended one's
 *             left half (subtiles 0 to 15)
 *   layer_right   an extended picture's right half (subtiles 16 to 31:
 *             the whole variations), "" for a plain one
 *   source    where its picture came from (ASSET_FROM_PLACEHOLDER: the checker)
 *   width, height   its picture's; extended when twice as wide as high
 *   extended  1: 8 x 4 subtiles (the right half 16 whole variations) */
typedef struct {
    char        id[5];
    char        path[128];
    char        layer[136], layer_right[144];
    AssetSource source;
    int         width, height, extended;
} Wc3Tileset;

typedef struct Wc3Ground Wc3Ground;

/* Reads the map's war3map.w3e, Terrain.slk and every ground tileset's
 * picture through `r` (the resolver: a test-map run's), and prepares the
 * ground (w3e_prep). `scale`, `x0`, `y0`: map.lua's rule (wow_x = x0 +
 * scale * wc3_y; wow_y = y0 - scale * wc3_x). W_OK with `*out` (freed by
 * wc3_ground_free); W_ERR_NOT_FOUND, with why, when there is no resolver,
 * no war3map.w3e or no Terrain.slk in any place (nothing to paint: the
 * converter's pictures stay), or when strict mode refuses the checker
 * for a tileset; W_ERR_CORRUPT from 063 or 077 with the file's name;
 * W_ERR_REFUSED for a scale not above 0. */
WErr wc3_ground_open(Resolver *r, MpqChain *chain, double scale, double x0, double y0, Wc3Ground **out, WWhy *why);
void wc3_ground_free(Wc3Ground *w);

/* The tilesets: first the map's ground list (the numbers its points
 * name), then any cliff tileset's ground the list lacks. */
int               wc3_ground_tilesets(const Wc3Ground *w);
const Wc3Tileset *wc3_ground_tileset(const Wc3Ground *w, int k);
const W3eGround  *wc3_ground_w3e(const Wc3Ground *w);

/* The map's cliffs (079), or NULL when no place has TerrainArt\
 * CliffTypes.slk: then cliff cells are laid as blighted ones are. */
const Wc3Cliffs  *wc3_ground_cliffs(const Wc3Ground *w);

/* The blight's tileset (into wc3_ground_tileset, id WC3_BLIGHT_ID), or
 * -1 when UI\WorldEditData.txt is in no place or names no blight for
 * the map's main tileset: then blighted cells are laid as before. */
int               wc3_ground_blight(const Wc3Ground *w);

/* The WC3 cell (i, j) holding world place (x, y), by the map's rule:
 * 1 when inside the grid (cells (width - 1) x (height - 1), rows from
 * the south), with where in it (fx from its west side, fy from its
 * south, 0 to 1) when asked; 0 outside. */
int  wc3_ground_cell_at(const Wc3Ground *w, double x, double y, int *i, int *j, double *fx, double *fy);

/* A layer's picture by its name (a Wc3Tileset's `layer` or
 * `layer_right`): that square of its tileset's picture (the picture's
 * height a side: 4 x 4 subtiles), alpha and all, side x side RGBA in a
 * new buffer the caller frees. W_OK; W_ERR_NOT_FOUND for a name no
 * tileset has. */
WErr wc3_ground_layer(const Wc3Ground *w, const char *name, unsigned char **rgba, int *side, WWhy *why);

/* One cell's laying: up to four pieces, lowest first, each a picture
 * (`pic`: into the tile's picture list, a tileset's layer) and the
 * subtile of it (`sub`: 0 to 15, row sub / 4 from the top, column
 * sub % 4, each a quarter of the square's side), WC3_PIECE_NONE in `sub`
 * after the last. The lowest is drawn whole, each higher one over it by
 * its own alpha. */
#define WC3_PIECE_NONE 255
typedef struct { uint8_t pic[4], sub[4]; } Wc3Lay;

/* A tile's cells a side (16 chunks of 8): cell (r, c), r from the
 * north and c from the west, is chunk (r / 8, c / 8)'s cell (r % 8,
 * c % 8), at r * WC3_TILE_CELLS + c. */
#define WC3_TILE_CELLS 128

/* What laying a tile did. */
typedef struct {
    int painted;            /* chunks laid with WC3's tilesets */
    int cells;              /* their cells inside the WC3 grid */
    int outside;            /* their cells outside it (laid with the chunk's most used tileset) */
    int bare;               /* cliff or blighted cells, laid whole with their south-west tileset */
    int cliff_ground;       /* cliff cells with a cliff model, laid whole with its tileset's ground */
    int blight;             /* blighted cells laid with the blight's picture on their blighted corners */
    int varied;             /* cells whose lowest piece isn't subtile 15 (another variation) */
    int most;               /* the most tilesets any one chunk lays */
} Wc3Paint;

/* Lays tile `adt`'s cells: for each chunk with a cell inside the WC3
 * grid, each of its 64 cells' pieces into `lay` (WC3_TILE_CELLS a side:
 * the tile's 16384 cells), the pictures named added to the tile's list
 * once each, its four picture numbers in `chunk_tex[k]` all the chunk's
 * most used tileset's (for a drawer without the cells), and `painted[k]`
 * 1; another chunk's cells get no piece (WC3_PIECE_NONE first) and its
 * `chunk_tex[k]` is left as it was. Counts into `*counts` (may be NULL).
 * W_OK; W_ERR_FULL when the tile's list would pass 256 pictures. */
WErr wc3_ground_lay_tile(const Wc3Ground *w, AdtTile *adt, Wc3Lay *lay, int chunk_tex[256][4], unsigned char painted[256], Wc3Paint *counts, WWhy *why);

/* Adds the cliff and ramp models standing on tile `adt` (those whose cell's
 * centre is on it, so each is placed by one tile) to its doodads: each
 * cliff's name (079's wc3_cliffs_name: its model's, and its bend when it
 * stands on uneven ground, so a bent one is its own) to its list once,
 * each cliff a
 * placement with id WC3_CLIFF_UID plus its cell, at its origin by the
 * map's rule (z: scale times WC3's), turned as WC3 lays models (rot
 * 0, 90, 0), size 1 (076 sizes a WC3 model by the map's rule). How many
 * into `*placed` (may be NULL). W_OK, nothing done with no cliffs;
 * W_ERR_FULL when memory runs out or a name passes the list's 259
 * characters. */
WErr wc3_ground_place_cliffs(const Wc3Ground *w, AdtTile *adt, int *placed, WWhy *why);

#endif
