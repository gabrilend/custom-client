/*
 * 049-jpeg.h (issue 107; the C twin of world-edit-to-execute's
 * src/parsers/jpeg.lua) - JPEG pictures, decoded
 *
 * For a general audience: Warcraft III keeps most of its pictures as
 * JPEG, the common photograph format, wrapped in its own BLP1 file
 * (017-blp.c unwraps it). A JPEG stores a picture as 8 x 8 squares of
 * waves (how strongly each of 64 patterns of light and dark is present),
 * rounded off where the eye won't notice and then packed by how often
 * each number occurs (Huffman coding). This unpacks the numbers, turns
 * the waves back into pixels, and hands back the picture.
 *
 * Read: baseline and progressive JPEGs (frame kinds 0, 1 and 2; a
 * progressive one arrives in several passes, coarse first), 8-bit
 * samples, any number of parts at any sampling, restart intervals.
 * Refused with W_ERR_UNSUPPORTED: lossless and arithmetic-coded JPEGs,
 * 12-bit samples, more than 4 parts.
 *
 * Parts handed back:
 *   1  grey
 *   3  YCbCr (an ordinary JPEG), turned into red, green, blue
 *   4  the stored values as they are: Blizzard's BLP1 stores blue,
 *      green, red, alpha this way, with no colour conversion
 *
 * The arithmetic is the Lua twin's, step for step (the same plain float
 * transform from precomputed cosines, the same rounding, colour parts at
 * lower resolution widened by repeating samples), so both give the same
 * bytes; tests/050-blp1-matches-lua.lua checks it on WC3's own pictures.
 * libjpeg smooths widened colour, so its edges differ slightly.
 */
#ifndef W_JPEG_H
#define W_JPEG_H

#include "013-errors.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int      width, height;
    int      components;          /* 1, 3 or 4 (above) */
    uint8_t *pixels;              /* width * height * components bytes, rows top to bottom */
} JpegImage;

/* W_OK, or W_ERR_CORRUPT / W_ERR_UNSUPPORTED with `why` (013-errors.h);
 * never reads past `len`. On failure `out` holds nothing to free. */
WErr jpeg_decode(const unsigned char *s, size_t len, JpegImage *out, WWhy *why);
void jpeg_free(JpegImage *img);

#endif
