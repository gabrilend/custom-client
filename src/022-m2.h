/*
 * 022-m2.h (issue 173k; the C twin of lib/m2.lua) - the game's models
 *
 * For a general audience: a model comes as two files: the .m2 holds every
 * corner point (where, which way its surface faces, where on its picture
 * it sits) and its list of pictures; the .skin file holds how the points
 * join into triangles, grouped into pieces ("geosets": the body, each hair
 * style, bare hands or gloves...), and which picture each piece uses. A
 * character's model holds every hair style and clothing shape at once;
 * 025-character.c picks the ones that show. Resting pose (no bones yet).
 *
 * Layouts: the 3.3.5a client's (model version 264); see lib/m2.lua for
 * every offset, which this follows exactly.
 */
#ifndef EG_M2_H
#define EG_M2_H

#include "013-errors.h"

#include <stddef.h>
#include <stdint.h>

typedef struct { float x, y, z, nx, ny, nz, u, v, u2, v2; } M2Vertex; /* model space: +x front, +y left, +z up; two sets of picture positions */
/* A picture: kind 0 a file it names; 1 skin; 2 cape; 6 hair; ... chosen
 * at run time; M2_TEXTURE_WC3 + n Warcraft III's replaceable picture n
 * (1 team colour, 2 team glow, 11 cliff, 31-37 trees; 061-mdx.c), kept
 * apart so no WC3 picture reads as one of the game's. flags 1 wrap
 * across, 2 wrap down (both games' files keep these two the same). */
enum { M2_TEXTURE_WC3 = 0x100 };
typedef struct { uint32_t kind; char file[260]; uint32_t flags; } M2Texture;
typedef struct { uint32_t id, first, count; } M2Piece;                 /* id = group * 100 + variant; first/count: triangle indices */
/* A unit: one piece drawn with one picture, and how (its material, from
 * the model's render-flag table, header offset 112):
 *   blend  0 opaque, 1 cut-out (see-through below an alpha threshold:
 *          leaves, hair), 2 alpha blended, 3 added without alpha,
 *          4 added, 5 multiplied, 6 multiplied twice (glows, glass ...)
 *   flags  0x1 unlit, 0x2 no fog, 0x4 two-sided, 0x8 no depth test,
 *          0x10 no depth writing
 * and its animated tint: a colour track pair (colour, then opacity: 40
 * bytes at header 76) and a transparency track (20 bytes at header 92),
 * both read through 023-anim.c (173n: an aurora's faint, shifting curtains) */
typedef struct {
    uint32_t piece, texture;
    uint16_t blend, flags;
    int      colour;                  /* into the model's colour tracks (header 72), -1 none */
    int      fade;                    /* into its transparency tracks (header 88, through the lookup at 144), -1 none */
    int      layers;                  /* how many pictures it lays over each other (skin unit +14; usually 1) */
    int      moving_uv;               /* 1 when its picture positions are animated (skin unit +22 names a UV animation) */
    /* per layer (up to 2): its picture (model texture slot), which set of
     * picture positions it uses (0 first, 1 second; -1 a reflection, drawn
     * with the first), and its picture-position animation (into the
     * header's UV animations, 96; -1 none); and the combine code (skin
     * unit +2), saying how layer 2 joins layer 1 (022-m2.c's m2_combine) */
    int      tex[2], uvset[2], uvanim[2];
    uint16_t shader;
    float    alpha;                   /* its opacity at rest, before any track: 1 for the game's models; a WC3
                                       * model's from its layer and geoset animation (061-mdx.h) */
} M2Unit;

/* How a unit's second layer joins its first, from its combine code.
 * This client's reading, to be compared with the original: the low 3 bits
 * name the second stage: 0 opaque (the second replaces), 1 multiply, 2 decal
 * (laid over by its alpha), 3 add, 4 multiply twice, 5 fade, 6 multiply
 * twice without alpha, 7 add without alpha. */
int m2_combine(uint16_t shader);

typedef struct {
    M2Vertex  *vertices;  uint32_t nvertices;
    M2Texture *textures;  uint32_t ntextures;
    M2Piece   *pieces;    uint32_t npieces;
    uint32_t  *indices;   uint32_t nindices;       /* model vertex numbers, three a triangle */
    M2Unit    *units;     uint32_t nunits;
} M2Model;

WErr  m2_read(const unsigned char *m2, size_t m2_len, const unsigned char *skin, size_t skin_len,
             M2Model *out, WWhy *why);
void m2_free(M2Model *m);

/* The model's solid shape (173q): its collision triangles (header 216:
 * corner numbers, u16, three a triangle; 224: the corners, 3 floats each),
 * 9 floats a triangle in model space, in a buffer for the caller to
 * free. 0 with *ntri 0 for a model with none (grass, flowers: walked
 * through); W_ERR_CORRUPT with `why` when the lists run past the file. */
WErr  m2_collision(const unsigned char *m2, size_t len, float **tris, int *ntri, WWhy *why);

#endif
