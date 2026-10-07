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

/* W_OK, or a code with `why` (013-errors.h). */
WErr  blp_decode(const unsigned char *s, size_t len, BlpImage *out, WWhy *why);
void blp_free(BlpImage *img);

#endif
