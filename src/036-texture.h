/*
 * 036-texture.h (issue 173u) - the game's pictures onto the graphics card
 *
 * For a general audience: most of the game's pictures are stored already
 * compressed in the graphics card's own block format (DXT: 4 x 4 pixels in
 * 8 or 16 bytes), with every smaller copy made in advance. Unpacking them
 * on the processor and sending the plain pixels costs time at load and 4
 * to 8 times the card's memory. This sends compressed pictures as they
 * are, every smaller copy included, and unpacks only the others (palette
 * and plain pictures, as 017-blp.c reads them) and makes their smaller copies
 * on the card. Either way the picture is smoothed between its copies and
 * repeats past its edges.
 *
 * The BLP2 header (017-blp.h): 8 encoding (2: DXT), 10 alpha kind (0 DXT1,
 * 1 DXT3, 7 DXT5), 12 width, 16 height, 20 where each copy starts (16
 * words), 84 how long each is.
 */
#ifndef EG_TEXTURE_H
#define EG_TEXTURE_H

#include "013-errors.h"

#include "raylib.h"

#include <stddef.h>

/* The picture in `s` (a whole .blp file) on the card. W_OK, or a code with `why`. */
/* How pictures go to the card (the handle, a small one, held by the
 * caller; docs/001, "Handles"):
 *   filter_mode     int: Texture Filtering, below
 *   anisotropy      float: the samples it asks of the card (1 none, to 16)
 *   bytes_sent      size_t: picture bytes gone to the card this way
 *   bytes_unpacked  size_t: what they would have taken unpacked (for the picture report) */
typedef struct {
    int    filter_mode;
    float  anisotropy;
    size_t bytes_sent, bytes_unpacked;
} Textures;

WErr texture_from_blp(Textures *tx, const unsigned char *s, size_t len, Texture2D *out, WWhy *why);

/* Texture Filtering (173b6), set once at start (`textureFilteringMode`,
 * the original's 0 to 5): 0 bilinear (smoothed within a copy, the nearest
 * copy), 1 trilinear (smoothed between copies too), 2 to 5 trilinear with
 * 2x, 4x, 8x or 16x anisotropic filtering (sharper where a picture is seen
 * at a slant; held to what the card offers). */
void textures_init(Textures *tx, int mode);   /* needs the window (it asks the card its most) */

/* Applies that filtering to the picture `id` on `target` (an OpenGL
 * texture kind: 2D, or the ground's array) with `levels` copies (1: no
 * smaller copies, plain smoothing). Every picture the client sends goes
 * through this. */
void texture_filter(const Textures *tx, unsigned int id, unsigned int target, int levels);



#endif
