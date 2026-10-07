/*
 * 036-texture.c (issue 173u) - the game's pictures onto the graphics card; see 036-texture.h
 *
 * For a general audience: for a compressed picture, each stored copy is
 * handed to the card as it is, through OpenGL directly (Raylib 5.1's own
 * loader works out some small copies' sizes wrongly and wants every copy
 * down to one pixel); for any other, the pixels are unpacked and Raylib
 * sends them.
 */
#include "036-texture.h"
#include "013-errors.h"
#include "017-blp.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* {{{ OpenGL through Raylib's loader (glad), as 038-ground.c reaches it */
typedef unsigned int GLenum_;
extern void (*glad_glGenTextures)(int n, unsigned int *textures);
extern void (*glad_glBindTexture)(GLenum_ target, unsigned int texture);
extern void (*glad_glCompressedTexImage2D)(GLenum_ target, int level, GLenum_ internalformat, int width, int height, int border, int size, const void *data);
extern void (*glad_glTexParameteri)(GLenum_ target, GLenum_ pname, int param);
extern void (*glad_glTexParameterf)(GLenum_ target, GLenum_ pname, float param);
extern void (*glad_glGetFloatv)(GLenum_ pname, float *data);
extern int GLAD_GL_EXT_texture_filter_anisotropic, GLAD_GL_ARB_texture_filter_anisotropic;
#define GL_TEXTURE_2D_             0x0DE1
#define GL_LINEAR_MIPMAP_NEAREST_  0x2701
#define GL_TEXTURE_MAX_ANISOTROPY_ 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_ 0x84FF
#define GL_TEXTURE_MAX_LEVEL_      0x813D
#define GL_TEXTURE_MAG_FILTER_     0x2800
#define GL_TEXTURE_MIN_FILTER_     0x2801
#define GL_TEXTURE_WRAP_S_         0x2802
#define GL_TEXTURE_WRAP_T_         0x2803
#define GL_LINEAR_                 0x2601
#define GL_LINEAR_MIPMAP_LINEAR_   0x2703
#define GL_REPEAT_                 0x2901
#define GL_DXT1_RGBA_              0x83F1   /* DXT1 with its see-through fourth colour, as 017-blp.c reads it */
#define GL_DXT3_RGBA_              0x83F2
#define GL_DXT5_RGBA_              0x83F3
/* }}} */


/* {{{ textures_init, texture_filter
 * The mode's two parts: whether copies are blended (0 picks the nearest
 * copy), and the anisotropy (2 to 5: 2, 4, 8, 16 samples), held to the
 * card's most; a card without the extension gets plain trilinear and is
 * told once. */
void textures_init(Textures *tx, int mode)
{
    tx->bytes_sent = tx->bytes_unpacked = 0;
    tx->filter_mode = mode;
    tx->anisotropy = mode >= 2 ? (float)(1 << (mode - 1)) : 1;
    if (tx->anisotropy > 1) {
        if (!GLAD_GL_EXT_texture_filter_anisotropic && !GLAD_GL_ARB_texture_filter_anisotropic) {
            fprintf(stderr, "texture filtering: the graphics card offers no anisotropic filtering; trilinear is used\n");
            tx->anisotropy = 1;
        } else {
            float most = 1;
            glad_glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_, &most);
            if (tx->anisotropy > most) { fprintf(stderr, "texture filtering: %gx asked, the card's most is %gx; %gx is used\n", tx->anisotropy, most, most); tx->anisotropy = most; }
        }
    }
}
void texture_filter(const Textures *tx, unsigned int id, unsigned int target, int levels)
{
    glad_glBindTexture(target, id);
    glad_glTexParameteri(target, GL_TEXTURE_MIN_FILTER_, levels <= 1 ? GL_LINEAR_ : tx->filter_mode == 0 ? GL_LINEAR_MIPMAP_NEAREST_ : GL_LINEAR_MIPMAP_LINEAR_);
    glad_glTexParameteri(target, GL_TEXTURE_MAG_FILTER_, GL_LINEAR_);
    if (tx->anisotropy > 1 && levels > 1) glad_glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_, tx->anisotropy);
    glad_glBindTexture(target, 0);
}
/* }}} */

static uint32_t u32(const unsigned char *s, size_t at) { return s[at] | s[at + 1] << 8 | s[at + 2] << 16 | (uint32_t)s[at + 3] << 24; }

/* {{{ texture_from_blp
 * Compressed: each copy i (width and height halving to 1) is used while
 * its stored length covers its blocks (a 4 x 4 block at least) and it
 * lies inside the file; the card is told the last copy given, so a short
 * chain still draws. Anything else: unpacked by 017-blp.c. */
WErr texture_from_blp(Textures *tx, const unsigned char *s, size_t len, Texture2D *out, WWhy *why)
{
    if (len >= 148 && !memcmp(s, "BLP2", 4) && s[8] == 2) {
        int kind = s[10];
        GLenum_ fmt = kind == 0 ? GL_DXT1_RGBA_ : kind == 1 ? GL_DXT3_RGBA_ : kind == 7 ? GL_DXT5_RGBA_ : 0;
        int block = kind == 0 ? 8 : 16;
        int w = (int)u32(s, 12), h = (int)u32(s, 16);
        if (fmt && w > 0 && h > 0 && w <= 4096 && h <= 4096) {
            unsigned int id = 0;
            glad_glGenTextures(1, &id);
            glad_glBindTexture(GL_TEXTURE_2D_, id);
            int levels = 0, lw = w, lh = h;
            for (int i = 0; i < 16; ++i) {
                uint32_t at = u32(s, 20 + (size_t)i * 4), size = u32(s, 84 + (size_t)i * 4);
                size_t need = (size_t)((lw + 3) / 4) * (size_t)((lh + 3) / 4) * (size_t)block;
                if (!at || size < need || (size_t)at + need > len) break;
                glad_glCompressedTexImage2D(GL_TEXTURE_2D_, i, fmt, lw, lh, 0, (int)need, s + at);
                tx->bytes_sent += need;
                tx->bytes_unpacked += (size_t)lw * lh * 4;
                ++levels;
                if (lw == 1 && lh == 1) break;
                lw = lw > 1 ? lw / 2 : 1;
                lh = lh > 1 ? lh / 2 : 1;
            }
            if (levels > 0) {
                glad_glTexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAX_LEVEL_, levels - 1);
                glad_glTexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_REPEAT_);
                glad_glTexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_REPEAT_);
                glad_glBindTexture(GL_TEXTURE_2D_, 0);
                texture_filter(tx, id, GL_TEXTURE_2D_, levels);
                *out = (Texture2D){ .id = id, .width = w, .height = h, .mipmaps = levels,
                                    .format = kind == 0 ? PIXELFORMAT_COMPRESSED_DXT1_RGBA : kind == 1 ? PIXELFORMAT_COMPRESSED_DXT3_RGBA : PIXELFORMAT_COMPRESSED_DXT5_RGBA };
                return 0;
            }
            /* no usable copy stored: unpacked instead, below */
            glad_glBindTexture(GL_TEXTURE_2D_, 0);
            UnloadTexture((Texture2D){ .id = id });
        }
    }
    BlpImage img;
    if (blp_decode(s, len, &img, why)) return why->code;
    Image im = { .data = img.rgba, .width = img.width, .height = img.height, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D t = LoadTextureFromImage(im);
    tx->bytes_sent += (size_t)img.width * img.height * 4;
    tx->bytes_unpacked += (size_t)img.width * img.height * 4;
    blp_free(&img);
    GenTextureMipmaps(&t);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    texture_filter(tx, t.id, GL_TEXTURE_2D_, t.mipmaps);
    *out = t;
    return 0;
}
/* }}} */
