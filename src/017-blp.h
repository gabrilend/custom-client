/*
 * 017-blp.h (issue 173k; the C twin of lib/blp.lua) - the game's pictures, decoded
 *
 * For a general audience: every picture painted on a model is a BLP2 file:
 * the picture at full size and smaller sizes, stored as colour numbers into
 * a 256-colour palette, as the graphics card's own compressed blocks (DXT1,
 * DXT3, DXT5: 4 x 4 pixels in 8 or 16 bytes), or as plain pixels. This
 * turns the full-size picture into plain red-green-blue-alpha bytes.
 *
 * Header (148 bytes, then a 1024-byte BGRA palette): "BLP2"; at 8 the
 * encoding (1 palette, 2 DXT, 3 plain); 9 alpha bits; 10 alpha kind (0
 * DXT1, 1 DXT3, 7 DXT5); 12 width; 16 height; 20 where each size starts
 * (16 words); 84 how long each is.
 *
 * Warcraft III's pictures are BLP1 (issue 107, 2026-10-07; the C twin of
 * world-edit-to-execute's src/parsers/blp.lua), told apart by the first
 * four bytes. Header (156 bytes, 32-bit words): "BLP1", encoding (0
 * JPEG, 1 palette), alpha bits (0, 1, 4, 8), width, height, picture
 * type, has smaller copies; at 28 where each size starts (16 words); at
 * 92 how long each is. Then, for JPEG: at 156 the length of a JPEG
 * header every size shares, the header from 160, and each size's own
 * remainder of the JPEG where its table says (049-jpeg.c reads the two
 * joined); its four parts are blue, green, red, alpha as stored, with
 * alpha used only when alpha bits is not 0. For a palette: at 156 the
 * 256 BGRA colours; each size is one colour number a pixel, then its
 * alpha packed after at the alpha bits a pixel (1 and 4 low bits first).
 */
#ifndef EG_BLP_H
#define EG_BLP_H

#include "013-errors.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int      width, height;
    uint8_t *rgba;                /* width * height * 4 bytes, rows top to bottom */
} BlpImage;

/* The full-size picture, BLP2 or BLP1. W_OK, or a code with `why`
 * (013-errors.h): W_ERR_CORRUPT for a file shorter than its tables say,
 * W_ERR_UNSUPPORTED for an encoding not listed above. */
WErr  blp_decode(const unsigned char *s, size_t len, BlpImage *out, WWhy *why);

/* One of the smaller copies: level 0 is full size, each next level half
 * as wide and high (never under 1). W_ERR_NOT_FOUND when the file has
 * no copy at that level. */
WErr  blp_decode_level(const unsigned char *s, size_t len, int level, BlpImage *out, WWhy *why);
void blp_free(BlpImage *img);

#endif
