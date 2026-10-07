/*
 * 021-wmo.h (issue 173g) - the game's buildings
 *
 * For a general audience: a building (Astranaar's inn, a cave, a bridge)
 * is a "world map object" (WMO). It comes as a root file, which lists its
 * pictures and materials, how many parts ("groups") it has, and the
 * furnishings placed inside it (barrels, lamps: small models, in named
 * sets), and one file per group (<name>_000.wmo, _001 ...) holding that
 * part's points and triangles, cut into batches of one material each.
 * Points are in the same model space as small models (x front, y left,
 * z up), so a building is placed with the same matrix (adt_place_matrix).
 *
 * Layouts (3.3.5a, version 17; blocks as in ground files: a reversed
 * four-letter tag, a size, then data).
 * Root: MOHD (header: 4 group count, 16 furnishing name count, 20
 *   furnishing count, 24 set count), MOTX (picture names, zero-ended, at
 *   offsets), MOMT (64 bytes a material: 0 flags, 8 blend mode, 12 first
 *   picture's MOTX offset), MODS (32 bytes a set: 20-byte name, 20 first
 *   furnishing, 24 count), MODN (furnishing model names), MODD (40 bytes a
 *   furnishing: 0 name offset in MODN (low 24 bits), 4 position (3
 *   floats), 16 turn (quaternion x, y, z, w), 32 scale, 36 colour).
 * Group: MOGP's 68-byte header (8 flags: 0x2000 indoors, 0x4 has vertex
 *   colours), then inside MOGP: MOPY, MOVI (u16 triangle corners), MOVT
 *   (points), MONR (facings), MOTV (picture positions), MOBA (24 bytes a
 *   batch: 12 first index (u32), 16 count (u16), 23 material (u8)), MOCV
 *   (colours, blue-green-red-alpha: indoor groups' baked light).
 *
 * Pool (MLIQ, inside MOGP; 173r): a 30-byte header (0 points across,
 *   4 points along, 8 cells across, 12 cells along (ints), 16 the grid's
 *   corner in the building's space (3 floats), 28 material (u16)); then 8
 *   bytes a point (4: its surface height, float), row by row along y;
 *   then a byte a cell, row by row: low four bits all set (0x?F) is a dry
 *   cell (the server's rule, WorldModel.cpp). Cells are 533.33 / 128
 *   yards (4.17) a side. The pool's kind (LiquidType.dbc id) comes from
 *   the group header's liquid field (MOGP + 52), see wmo_liquid_kind.
 *
 * Material flags: 0x1 unlit, 0x2 no fog, 0x4 two-sided. Blend modes as
 * in models (022-m2.h): 0 opaque, 1 cut-out, 2 see-through, 3+ added light.
 */
#ifndef EG_WMO_H
#define EG_WMO_H

#include "013-errors.h"

#include <stddef.h>
#include <stdint.h>

typedef struct { float x, y, z, nx, ny, nz, u, v; uint8_t r, g, b, a; } WmoVertex;
typedef struct { uint32_t first, count; int material; } WmoBatch;

/* A building part's pool (MLIQ), in the building's own space; nx 0: none.
 *   nx, ny        int: points across (x) and along (y); cells are one fewer each way
 *   corner[3]     float: the grid's first point (x, y; z unused, heights are per point)
 *   height        float[nx * ny]: each point's surface height, index y * nx + x
 *   cell          uint8_t[(nx-1) * (ny-1)]: each cell's flags, index y * (nx-1) + x */
#define WMO_POOL_CELL (533.33333f / 128)
typedef struct {
    int      nx, ny;
    float    corner[3];
    float   *height;
    uint8_t *cell;
} WmoPool;
static inline int wmo_pool_dry(uint8_t cell) { return (cell & 0x0F) == 0x0F; }

typedef struct {
    uint32_t   flags;                 /* MOGP flags: 0x2000 indoors, 0x80000 its legacy water is the sea's */
    uint32_t   liquid;                /* MOGP + 52: the pool's kind, raw (wmo_liquid_kind) */
    WmoPool    pool;                  /* MLIQ (173r) */
    int        has_colours;           /* MOCV present */
    WmoVertex *vertices;  uint32_t nvertices;
    uint16_t  *indices;   uint32_t nindices;
    WmoBatch  *batches;   int      nbatches;
    uint8_t   *tri_flags;  uint32_t nflags;   /* MOPY, per triangle: see wmo_solid */
} WmoGroup;

/* Whether a building's triangle is solid (173q), from its MOPY flags:
 * 0x08 collision, or 0x20 drawn and not 0x04 detail (the original's rule;
 * invisible collision-only triangles are 0x08 alone). */
static inline int wmo_solid(uint8_t flags) { return (flags & 0x08) || ((flags & 0x20) && !(flags & 0x04)); }

typedef struct { uint32_t flags, blend; char picture[260]; } WmoMaterial;
typedef struct { char model[260]; float pos[3]; float quat[4]; float scale; } WmoFurnishing;
typedef struct { char name[24]; int first, count; } WmoSet;

typedef struct {
    int            ngroups;
    uint32_t       flags;            /* MOHD + 60: 0x4 the groups' liquid fields are LiquidType ids */
    WmoMaterial   *materials;    int nmaterials;
    WmoFurnishing *furnishings;  int nfurnishings;
    WmoSet        *sets;         int nsets;
} WmoRoot;

WErr  wmo_read_root(const unsigned char *s, size_t len, WmoRoot *out, WWhy *why);
WErr  wmo_read_group(const unsigned char *s, size_t len, WmoGroup *out, WWhy *why);
/* A part's pool's kind, as a LiquidType.dbc id, by the rule the server's
 * map extractor uses (vmap4_extractor/wmo.cpp): the group's liquid field
 * read as an id when the root's flag 0x4 says so, otherwise as an older
 * numbering (15 none, else one less than the id); with none, the first
 * wet cell's low four bits plus one. An older-numbered id under 21 maps
 * to the building liquids: water 13 (14, the sea's, when the group's
 * flag 0x80000), sea 14, magma 19, slime 20. 0 when the part has no
 * pool. */
int  wmo_liquid_kind(const WmoRoot *r, const WmoGroup *g);
void wmo_free_root(WmoRoot *r);
void wmo_free_group(WmoGroup *g);

#endif
