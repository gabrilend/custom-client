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
 * tilesets, lowest first: the lowest whole, each higher one as the piece
 * of its picture ("subtile") whose alpha covers just the corners it is
 * on. Here, for each chunk:
 *   - each of its 64 cells is found in the WC3 grid by its centre;
 *   - the chunk's four tilesets are the four its cells use most (ties:
 *     the lower number first), drawn lowest number first;
 *   - layer 0 is the lowest; each higher layer's strength in a cell is
 *     full where it is the cell's lowest kept tileset, its subtile's
 *     alpha where it is a higher one, and nothing where the cell lacks
 *     it, at the blend picture's 8 points a cell side;
 *   - each layer's picture is its tileset's whole tile (subtile 15),
 *     repeated once a cell as the ground's pictures always are.
 * So a chunk using four tilesets or fewer comes out as WC3 lays it, but
 * for the variation pictures (one whole tile a tileset) and the
 * subtiles' own colours (their alpha, the whole tile's colours). A cell
 * whose tileset is past the chunk's four is folded: a dropped lowest
 * lets the next kept one lie whole, a dropped higher one is left out. A
 * cliff cell with a WC3 cliff model (079: when the player has
 * TerrainArt\CliffTypes.slk) is laid whole with its cliff tileset's
 * ground (its groundTile, added to the tilesets when the map's list
 * lacks it), and its model is added to the tile's doodads
 * (wc3_ground_place_cliffs); a ramp's cell (079: a model too) keeps its
 * own ground. A blighted cell, when the editor's data
 * (UI\WorldEditData.txt's [TileSets], second field) names the main
 * tileset's blight picture, is laid as WC3 lays tilesets with the
 * blight a tileset of its own, highest, on each blighted corner; with
 * none named, and for a cliff with no cliff tileset or model (063 leaves
 * them bare for the renderer), a bare cell is laid whole with its
 * south-west point's tileset. A chunk with no cell inside the WC3 grid
 * keeps the converter's own pictures; a cell outside it in a chunk with some inside shows the
 * chunk's layer 0.
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
 * "|"). 033's ground_prep_pixels gives such a name's whole tile. */
#define WC3_LAYER_MARK "|wc3"

/* The blight's tileset id (no real tileset's has a "*"). */
#define WC3_BLIGHT_ID "*blt"

/* One ground tileset of the map. Field by field:
 *   id        its four letters ("Ldrt"), ended by a zero
 *   path      its picture (Terrain.slk's dir\file.blp), "" when the
 *             sheet has no row for it (the checker stands in)
 *   layer     its layer's name (path, or TerrainArt\<id>.blp, and the mark)
 *   source    where its picture came from (ASSET_FROM_PLACEHOLDER: the checker)
 *   width, height   its picture's; extended when twice as wide as high
 *   extended  1: 8 x 4 subtiles (the right half 16 whole variations) */
typedef struct {
    char        id[5];
    char        path[128];
    char        layer[136];
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

/* A layer's picture by its name (a Wc3Tileset's `layer`): its tileset's
 * whole tile (subtile 15), opaque, side x side RGBA in a new buffer the
 * caller frees. W_OK; W_ERR_NOT_FOUND for a name no tileset has. */
WErr wc3_ground_layer(const Wc3Ground *w, const char *name, unsigned char **rgba, int *side, WWhy *why);

/* What painting a tile did. */
typedef struct {
    int painted;            /* chunks painted with WC3's tilesets */
    int cells;              /* their cells inside the WC3 grid */
    int outside;            /* their cells outside it (layer 0 shows) */
    int folded;             /* cells with a tileset past their chunk's four */
    int bare;               /* cliff or blighted cells, laid whole with their south-west tileset */
    int cliff_ground;       /* cliff cells with a cliff model, laid whole with its tileset's ground */
    int blight;             /* blighted cells laid with the blight's picture on their blighted corners */
} Wc3Paint;

/* Paints tile `adt`'s chunks: for each chunk with a cell inside the WC3
 * grid, its four picture numbers into `chunk_tex[k]` (into the tile's
 * list, to which the WC3 layers' names are added once each), its three
 * strengths into `rgb` (1024 x 1024 x 3, chunk k at row k / 16, column
 * k % 16 of 64 x 64, as the tile's blend picture is laid out: 033), and
 * `painted[k]` 1; other chunks are left as they were. Counts into
 * `*counts` (may be NULL). W_OK; W_ERR_FULL when the tile's list would
 * pass 256 pictures. */
WErr wc3_ground_paint_tile(const Wc3Ground *w, AdtTile *adt, int chunk_tex[256][4], unsigned char *rgb, unsigned char painted[256], Wc3Paint *counts, WWhy *why);

/* Adds the cliff and ramp models standing on tile `adt` (those whose cell's
 * centre is on it, so each is placed by one tile) to its doodads: each
 * model's name (079's wc3_cliffs_model) to its list once, each cliff a
 * placement with id WC3_CLIFF_UID plus its cell, at its origin by the
 * map's rule (z: scale times WC3's), turned as WC3 lays models (rot
 * 0, 90, 0), size 1 (076 sizes a WC3 model by the map's rule). How many
 * into `*placed` (may be NULL). W_OK, nothing done with no cliffs;
 * W_ERR_FULL when memory runs out. */
WErr wc3_ground_place_cliffs(const Wc3Ground *w, AdtTile *adt, int *placed, WWhy *why);

#endif
