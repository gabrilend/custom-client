/*
 * 020-adt.h (issue 173f) - the original client's ground files
 *
 * For a general audience: the game keeps each 533-yard square of a map in
 * its own ground file (an "ADT", World\Maps\<map>\<map>_<ty>_<tx>.adt, the
 * west-east tile number first). Each file is cut into 16 x 16 chunks of
 * 33.3 yards. For every chunk it holds the ground's heights, and up to
 * four ground pictures (grass, dirt, rock ...) with a blend map for each
 * picture above the first, saying how strongly it shows at each of
 * 64 x 64 points. The client paints the ground by blending them. It also
 * holds a baked shadow map per chunk (173h) and the list of trees,
 * rocks and buildings placed on the tile (173g).
 *
 * File layout (3.3.5a; little-endian; every block is a four-letter tag
 * stored reversed, then its size, then its data): MVER, MHDR, MCIN,
 * MTEX (the tile's picture names, each ended by a zero byte), MMDX, MMID,
 * MWMO, MWID, MDDF, MODF, MH2O, then 256 MCNK blocks, one per chunk.
 *
 * MCNK header (128 bytes), the fields read here, by byte offset:
 *    0 flags (0x8000: the blend maps' last row and column are real, not
 *      to be copied from the row and column before them)
 *    4 index x (west to east, 0-15)        8 index y (north to south)
 *   12 layer count (1-4)                  20 offset of MCVT (heights)
 *   24 offset of MCNR (normals)           28 offset of MCLY (layers)
 *   36 offset of MCAL (blend maps)        40 MCAL's size, counting its own
 *                                            8-byte head (trusted over the
 *                                            size inside MCAL, which can be
 *                                            wrong in packed tiles)
 *   44 offset of MCSH (shadow)            48 MCSH's size
 *   60 holes (16 bits: 4 x 4, each bit hiding 2 x 2 cells)
 *  104 position: world x (north), world y (west), base height
 *  116 offset of MCCV (colour tints; present when flags & 0x40)
 * Offsets count from the MCNK tag and point at each block's own tag.
 *
 * MCVT: 145 floats, heights above the base: a row of 9 corners, then a
 * row of 8 cell centres, and so on ((9 + 8) x 8 + 9). Rows run south
 * (world x falls by 33.3 / 8 yards a row) and columns run east (world y
 * falls); adt_corner / adt_centre give the index. Checked against the
 * server's own heights for the same ground (tests/test_client.c).
 * MCLY: 16 bytes a layer: picture number in MTEX, flags (0x100: has a
 * blend map; 0x200: the blend map is run-length packed), its offset in
 * MCAL, a ground-effect number.
 * MCAL: per layer above the first, 64 x 64 strengths. The map's WDT says
 * which form: 4 bits a point (2048 bytes; Kalimdor) or 8 bits (4096,
 * plain or packed).
 * MCSH: 64 x 64 bits, 8 bytes a row, lowest bit first; 1 = shadowed.
 * MCCV: 145 colours (blue, green, red, alpha bytes), one per height, in
 *   MCVT's order; 127 means "unchanged", so the ground's colour is
 *   multiplied by value / 127 (0 to 2). Added in 3.3.5a.
 *
 * Placements (173g). MMDX: small-model names, each ended by a zero byte;
 * MMID: where each name starts in MMDX; MDDF: 36 bytes a placement:
 * name number (into MMID), unique id, position (3 floats), rotation
 * (3 floats, degrees), scale (u16, 1024 = 1.0), flags (u16). MWMO / MWID /
 * MODF the same for buildings, 64 bytes each: name number, unique id,
 * position, rotation, bounding box (6 floats), flags, doodad set (u16),
 * name set (u16), unused (u16).
 * Positions are in placement space, measured from the map's north-west
 * corner: world x = 17066.67 - p[2], world y = 17066.67 - p[0],
 * world z = p[1] (checked: trees stand on the server's ground; tests).
 * A building crossing tile edges is listed in every tile it touches,
 * with the same unique id.
 *
 * Water (173r). MH2O: 256 heads of 12 bytes (offset of the chunk's
 * layers, layer count, offset of attributes; offsets from MH2O's data),
 * then each layer (24 bytes): 0 liquid kind (u16, LiquidType.dbc), 2
 * point format (u16: 0 heights and depths, 1 heights and picture
 * positions, 2 depths only: flat at the lowest height, 3 all three),
 * 4 lowest height, 8 highest height, 12 first column, 13 first row, 14
 * width, 15 height (cells, u8), 16 offset of a which-cells bitmap
 * (width x height bits, row by row, lowest bit first; 0: all), 20
 * offset of the points ((width+1) x (height+1) heights, floats, first).
 *
 * An AdtTile, field by field:
 *   textures     char[nt][128]: the picture names (MTEX), in file order
 *   ntextures    int
 *   chunks[256]  AdtChunk, by row (index y) * 16 + column (index x)
 * An AdtChunk:
 *   ix, iy       int: column (west to east) and row (north to south)
 *   x, y, z      float: world position of the chunk's north-west corner,
 *                and the height the 145 heights are added to
 *   heights[145] float: above z, MCVT order
 *   holes        uint16_t: MCNK's low-resolution hole bits
 *   nlayers      int, 1-4
 *   tex[4]       int: each layer's picture number (into textures)
 *   alpha[4][4096] uint8_t: each layer's strength, 0-255, row by row
 *                (row = south, column = east); layer 0's is all 255
 *   shadow[4096] uint8_t: 1 where shadowed, 0 where lit
 *   tint[145][3] uint8_t: red, green, blue per height, 127 = unchanged
 *                (all 127 when the chunk has no MCCV)
 * An AdtPlace (a doodad or a building):
 *   name         int: into models (doodads) or buildings
 *   uid          uint32_t: the placement's unique id, shared across tiles
 *   x, y, z      float: world position
 *   rot[3]       float: the file's rotation, degrees (see 042-doodads.c)
 *   scale        float: 1.0 = as made (buildings always 1)
 * An AdtLiquid (one water layer in one chunk):
 *   chunk        int: which chunk (row * 16 + column)
 *   kind         int: LiquidType.dbc id (1 water, 2 ocean, 3 magma, 4 slime,
 *                5 slow water ...)
 *   col, row, w, h  int: the cells it covers, 0-8 within the chunk
 *   wet[8][8]    uint8_t: per cell of the chunk (row, column), 1 when it
 *                has water
 *   height[9][9] float: the surface's height at each corner (row, column)
 *                of the chunk's cell grid, where the layer gives one
 *   depth[9][9]  uint8_t: the water's depth at each corner as the file
 *                stores it: yards times about 9 (measured at Astranaar's
 *                lake: 174 at 19.4 yards, 92 at 10.2), so 255 is about 28
 *                yards or deeper; 255 where the layer's format has none
 *   doodad_set   int: buildings only, which set of furnishings to show
 *   lo[3], hi[3] float: buildings only, the world box the building fills
 *                (MODF's extents, turned into world axes)
 */
#ifndef EG_ADT_H
#define EG_ADT_H

#include <stddef.h>
#include <stdint.h>

#define ADT_CHUNK_SIZE  (533.33333f / 16)   /* yards a chunk side */

typedef struct {
    int      ix, iy;
    float    x, y, z;
    float    heights[145];
    uint16_t holes;
    int      nlayers;
    int      tex[4];
    uint8_t  alpha[4][4096];
    uint8_t  shadow[4096];
    uint8_t  tint[145][3];
} AdtChunk;

typedef struct {
    int      name;
    uint32_t uid;
    float    x, y, z;
    float    rot[3];
    float    scale;
    int      doodad_set;
    float    lo[3], hi[3];
} AdtPlace;

typedef struct {
    int     chunk, kind;
    int     col, row, w, h;
    uint8_t wet[8][8];
    float   height[9][9];
    uint8_t depth[9][9];
} AdtLiquid;

typedef struct {
    char   (*textures)[128];
    AdtLiquid *liquids;               /* MH2O's layers (173r) */
    int      nliquids;
    int      ntextures;
    char   (*models)[260];            /* doodad model names (MMDX via MMID) */
    int      nmodels;
    AdtPlace *doodads;                /* MDDF */
    int      ndoodads;
    char   (*buildings)[260];         /* building names (MWMO via MWID) */
    int      nbuildings;
    AdtPlace *places;                 /* MODF */
    int      nplaces;
    AdtChunk chunks[256];
} AdtTile;

/* Whether a map's blend maps are 8 bits a point, from its WDT file
 * (MPHD flags & 0x4). Returns 1 or 0, or -1 with `why` set when the WDT
 * can't be read. */
int adt_wdt_big_alpha(const unsigned char *s, size_t len, char *why, int why_len);

/* Reads one ground file. 0 on success; otherwise `why` names what was
 * wrong and where (block tag, chunk index). `out` is large (about 5 MB):
 * allocate it, don't put it on the stack. */
int adt_read(const unsigned char *s, size_t len, int big_alpha, AdtTile *out, char *why, int why_len);
void adt_free(AdtTile *t);

/* A placement's turn, size and position as a 4 x 4 matrix, laid out as
 * Raylib's Matrix is (m[0], m[4], m[8], m[12] the first row; a point p
 * goes to m[0]*p.x + m[4]*p.y + m[8]*p.z + m[12], and so on), relative to
 * the drawing origin (ox, oy). The turn follows the order the original's
 * own model viewer uses, with its rotation angles in degrees:
 *   world = to_world * turn_y(ry - 90) * turn_z(-rx) * turn_x(rz) * to_placement * scale
 * to_placement takes a model's axes (x front, y left, z up) into placement
 * space (y up): (x, y, z) -> (x, z, -y). to_world takes placement space
 * into the world (x north, y west, z up): (x, y, z) -> (-z, -x, y). With
 * only ry set, a model's front ends up facing world angle ry + 180
 * degrees (tests). */
void adt_place_matrix(const AdtPlace *p, double ox, double oy, float m[16]);

/* Something placed inside a building (a furnishing: position, turn as a
 * quaternion x, y, z, w, and size, all in the building's own space) as a
 * matrix in the world: building = the building's adt_place_matrix. Same
 * layout as above. Scale first, then the turn, then the move, then the
 * building's own place. */
void adt_inside_matrix(const float building[16], const float pos[3], const float quat[4], float scale, float m[16]);

/* A map's low-detail heights (its .wdl, 173s): per tile (tile x * 64 +
 * tile y) a 17 x 17 grid of corner heights (rows south, columns east, as
 * the ground files run), 16 rows of 16 hole bits (lowest bit the first
 * column), and whether the tile has any. Layout: MAOF, 4096 file offsets
 * (0 none), each to an MARE block (17 x 17 corners, then 16 x 16 centres,
 * signed 16-bit yards) followed by MAHO (the holes). About 2.5 MB:
 * allocate it. */
typedef struct {
    int16_t  heights[4096][17 * 17];
    uint16_t holes[4096][16];
    uint8_t  have[4096];
} AdtWdl;
int adt_wdl_read(const unsigned char *s, size_t len, AdtWdl *out, char *why, int why_len);

/* MCVT index of corner (row 0-8, col 0-8) and centre (row 0-7, col 0-7). */
static inline int adt_corner(int row, int col) { return row * 17 + col; }
static inline int adt_centre(int row, int col) { return row * 17 + 9 + col; }

#endif
