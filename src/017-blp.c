/*
 * 017-blp.c (issue 173k; BLP1 issue 107) - see 017-blp.h. BLP2 ported from
 * wow-chat-2026's src/custom-client/lib/blp.lua; BLP1 from
 * world-edit-to-execute's src/parsers/blp.lua.
 */
#include "017-blp.h"
#include "013-errors.h"
#include "049-jpeg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static rd32 */
static uint32_t rd32(const unsigned char *s) { return (uint32_t)s[0] | ((uint32_t)s[1] << 8) | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24); }
/* }}} */

/* {{{ static rgb565
 * A DXT colour: 5 bits red, 6 green, 5 blue, widened to 0-255. */
static void rgb565(unsigned v, int *r, int *g, int *b)
{
    unsigned R = v >> 11, G = (v >> 5) & 63, B = v & 31;
    *r = (int)((R << 3) | (R >> 2));
    *g = (int)((G << 2) | (G >> 4));
    *b = (int)((B << 3) | (B >> 2));
}
/* }}} */

/* {{{ static decode_dxt
 * Fills `out` from DXT blocks at `p`. `kind` 0 DXT1 (8-byte blocks, 1-bit
 * alpha), 1 DXT3 (16-byte, 4 bits of alpha a pixel), 7 DXT5 (16-byte,
 * alpha by interpolation). */
static void decode_dxt(const unsigned char *p, int w, int h, int kind, uint8_t *out)
{
    int block = kind == 0 ? 8 : 16;
    int bw = w / 4 > 0 ? w / 4 : 1, bh = h / 4 > 0 ? h / 4 : 1;
    for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
            const unsigned char *q = p + (size_t)(by * bw + bx) * block;
            const unsigned char *cq = q + (kind == 0 ? 0 : 8);       /* the colour half */
            int alpha[16];
            if (kind == 1) {
                for (int i = 0; i < 16; ++i) {
                    int byte = q[i >> 1];
                    alpha[i] = ((i % 2 == 0) ? (byte & 15) : (byte >> 4)) * 17;
                }
            } else if (kind == 7) {
                int a0 = q[0], a1 = q[1], pal[8];
                pal[0] = a0; pal[1] = a1;
                if (a0 > a1) for (int i = 1; i <= 6; ++i) pal[i + 1] = ((7 - i) * a0 + i * a1) / 7;
                else { for (int i = 1; i <= 4; ++i) pal[i + 1] = ((5 - i) * a0 + i * a1) / 5; pal[6] = 0; pal[7] = 255; }
                /* 48 bits of 3-bit indices, little-endian, in two 24-bit halves */
                for (int half = 0; half < 2; ++half) {
                    unsigned v = q[2 + half * 3] | (q[3 + half * 3] << 8) | (q[4 + half * 3] << 16);
                    for (int i = 0; i < 8; ++i) alpha[half * 8 + i] = pal[(v >> (3 * i)) & 7];
                }
            }
            /* colour: two endpoints and two (or one) between */
            unsigned c0 = cq[0] | (cq[1] << 8), c1 = cq[2] | (cq[3] << 8);
            int cr[4], cg[4], cb[4], ca[4] = { 255, 255, 255, 255 };
            rgb565(c0, &cr[0], &cg[0], &cb[0]);
            rgb565(c1, &cr[1], &cg[1], &cb[1]);
            if (c0 > c1 || kind != 0) {
                cr[2] = (2 * cr[0] + cr[1]) / 3; cg[2] = (2 * cg[0] + cg[1]) / 3; cb[2] = (2 * cb[0] + cb[1]) / 3;
                cr[3] = (cr[0] + 2 * cr[1]) / 3; cg[3] = (cg[0] + 2 * cg[1]) / 3; cb[3] = (cb[0] + 2 * cb[1]) / 3;
            } else {
                cr[2] = (cr[0] + cr[1]) / 2; cg[2] = (cg[0] + cg[1]) / 2; cb[2] = (cb[0] + cb[1]) / 2;
                cr[3] = cg[3] = cb[3] = 0; ca[3] = 0;                      /* DXT1's see-through colour */
            }
            uint32_t idx = rd32(cq + 4);
            for (int py = 0; py < 4; ++py)
                for (int px = 0; px < 4; ++px) {
                    int x = bx * 4 + px, y = by * 4 + py;
                    if (x >= w || y >= h) continue;
                    int i = py * 4 + px, k = (idx >> (2 * i)) & 3;
                    uint8_t *o = out + ((size_t)y * w + x) * 4;
                    o[0] = (uint8_t)cr[k]; o[1] = (uint8_t)cg[k]; o[2] = (uint8_t)cb[k];
                    o[3] = (uint8_t)(kind == 0 ? ca[k] : alpha[i]);
                }
        }
}
/* }}} */

/* {{{ static decode_palette
 * One colour number a pixel into the BGRA palette at `pal`, then the
 * alpha packed after at `abits` a pixel (8: a byte each; 4: two a byte,
 * low half first; 1: eight a byte, low bit first; 0: opaque). The caller
 * has checked that n + its alpha bytes lie inside the file. */
static void decode_palette(const unsigned char *pal, const unsigned char *px, size_t n, int abits, uint8_t *o)
{
    for (size_t i = 0; i < n; ++i) {
        int c = px[i];
        o[i * 4 + 0] = pal[c * 4 + 2]; o[i * 4 + 1] = pal[c * 4 + 1]; o[i * 4 + 2] = pal[c * 4 + 0];
        int a = 255;
        if (abits == 8) a = px[n + i];
        else if (abits == 4) { int byte = px[n + i / 2]; a = ((i % 2 == 0) ? (byte & 15) : (byte >> 4)) * 17; }
        else if (abits == 1) { int byte = px[n + i / 8]; a = ((byte >> (i % 8)) & 1) ? 255 : 0; }
        o[i * 4 + 3] = (uint8_t)a;
    }
}
/* the bytes a palette picture of n pixels takes, its alpha included */
static size_t palette_bytes(size_t n, int abits)
{
    return n + (abits == 8 ? n : abits == 4 ? (n + 1) / 2 : abits == 1 ? (n + 7) / 8 : 0);
}
/* }}} */

/* {{{ static level_size: a smaller copy's width or height, never under 1 */
static int level_size(int full, int level)
{
    int v = full >> level;
    return v > 0 ? v : 1;
}
/* }}} */

/* {{{ static decode_blp2 */
static WErr decode_blp2(const unsigned char *s, size_t len, int level, BlpImage *out, WWhy *why)
{
    if (len < 148 + 1024) return w_fail(why, W_ERR_CORRUPT, "a BLP2 picture shorter than its header (%zu bytes)", len);
    int enc = s[8], abits = s[9], akind = s[10];
    int w = level_size((int)rd32(s + 12), level), h = level_size((int)rd32(s + 16), level);
    size_t at = rd32(s + 20 + level * 4);
    if (at == 0 || rd32(s + 84 + level * 4) == 0) return w_fail(why, W_ERR_NOT_FOUND, "the BLP2 picture has no copy at level %d", level);
    size_t n = (size_t)w * h, need;
    /* a dispatch over the three encodings: how many bytes each needs */
    if (enc == 2) {
        if (akind != 0 && akind != 1 && akind != 7) return w_fail(why, W_ERR_UNSUPPORTED, "BLP2 alpha kind %d is not known", akind);
        size_t bw = w / 4 > 0 ? (size_t)w / 4 : 1, bh = h / 4 > 0 ? (size_t)h / 4 : 1;
        need = bw * bh * (akind == 0 ? 8 : 16);
    } else if (enc == 1) need = palette_bytes(n, abits);
    else if (enc == 3) need = n * 4;
    else return w_fail(why, W_ERR_UNSUPPORTED, "BLP2 encoding %d is not known", enc);
    if (at > len || need > len - at) return w_fail(why, W_ERR_CORRUPT, "a BLP2 picture's level %d runs past the end (%zu bytes at %zu of %zu)", level, need, at, len);
    uint8_t *o = malloc(n * 4);
    if (!o) return w_fail(why, W_ERR_FULL, "no memory for a %d x %d picture", w, h);
    if (enc == 2) {
        decode_dxt(s + at, w, h, akind, o);
    } else if (enc == 1) {
        decode_palette(s + 148, s + at, n, abits, o);
    } else {
        for (size_t i = 0; i < n; ++i) {
            o[i * 4 + 0] = s[at + i * 4 + 2]; o[i * 4 + 1] = s[at + i * 4 + 1];
            o[i * 4 + 2] = s[at + i * 4 + 0]; o[i * 4 + 3] = s[at + i * 4 + 3];
        }
    }
    out->width = w; out->height = h; out->rgba = o;
    return W_OK;
}
/* }}} */

/* {{{ static decode_blp1
 * WC3's pictures (see the header). The JPEG kind is the shared header
 * and this level's remainder joined, decoded by 049-jpeg.c; the picture
 * may come out larger than the level says (its blocks are whole 8 x 8
 * squares), and only the level's own size is kept; where it comes out
 * smaller, the rest stays zero, as in the Lua twin. */
static WErr decode_blp1(const unsigned char *s, size_t len, int level, BlpImage *out, WWhy *why)
{
    if (len < 160) return w_fail(why, W_ERR_CORRUPT, "a BLP1 picture shorter than its header (%zu bytes)", len);
    uint32_t enc = rd32(s + 4), abits = rd32(s + 8);
    int w = level_size((int)rd32(s + 12), level), h = level_size((int)rd32(s + 16), level);
    size_t at = rd32(s + 28 + level * 4), size = rd32(s + 92 + level * 4);
    if (at == 0 || size == 0) return w_fail(why, W_ERR_NOT_FOUND, "the BLP1 picture has no copy at level %d", level);
    if (at > len || size > len - at) return w_fail(why, W_ERR_CORRUPT, "a BLP1 picture's level %d runs past the end (%zu bytes at %zu of %zu)", level, size, at, len);
    size_t n = (size_t)w * h;
    /* a dispatch over the two encodings: JPEG (0) and palette (1) */
    if (enc == 0) {
        size_t hs = rd32(s + 156);
        if (hs > len - 160) return w_fail(why, W_ERR_CORRUPT, "a BLP1 picture's JPEG header (%zu bytes) runs past the end", hs);
        unsigned char *joined = malloc(hs + size);
        if (!joined) return w_fail(why, W_ERR_FULL, "no memory to join a BLP1 picture's JPEG");
        memcpy(joined, s + 160, hs);
        memcpy(joined + hs, s + at, size);
        JpegImage img;
        WErr e = jpeg_decode(joined, hs + size, &img, why);
        free(joined);
        if (e) return e;
        if (img.components != 3 && img.components != 4) {
            jpeg_free(&img);
            return w_fail(why, W_ERR_UNSUPPORTED, "a BLP1 picture's JPEG has %d parts (3 or 4 are read)", img.components);
        }
        uint8_t *o = calloc(n, 4);
        if (!o) { jpeg_free(&img); return w_fail(why, W_ERR_FULL, "no memory for a %d x %d picture", w, h); }
        int iw = w < img.width ? w : img.width, ih = h < img.height ? h : img.height, nc = img.components;
        for (int y = 0; y < ih; ++y)
            for (int x = 0; x < iw; ++x) {
                const uint8_t *p = img.pixels + ((size_t)y * img.width + x) * nc;
                uint8_t *q = o + ((size_t)y * w + x) * 4;
                q[0] = p[2]; q[1] = p[1]; q[2] = p[0];               /* stored blue, green, red */
                q[3] = (nc == 4 && abits > 0) ? p[3] : 255;
            }
        jpeg_free(&img);
        out->width = w; out->height = h; out->rgba = o;
        return W_OK;
    }
    if (enc == 1) {
        if (len < 156 + 1024) return w_fail(why, W_ERR_CORRUPT, "a BLP1 picture shorter than its palette");
        if (abits != 0 && abits != 1 && abits != 4 && abits != 8) return w_fail(why, W_ERR_UNSUPPORTED, "BLP1 alpha of %u bits a pixel is not known", abits);
        if (palette_bytes(n, (int)abits) > len - at) return w_fail(why, W_ERR_CORRUPT, "a BLP1 picture's level %d runs past the end", level);
        uint8_t *o = malloc(n * 4);
        if (!o) return w_fail(why, W_ERR_FULL, "no memory for a %d x %d picture", w, h);
        decode_palette(s + 156, s + at, n, (int)abits, o);
        out->width = w; out->height = h; out->rgba = o;
        return W_OK;
    }
    return w_fail(why, W_ERR_UNSUPPORTED, "BLP1 encoding %u is not known", enc);
}
/* }}} */

/* {{{ blp_decode_level */
WErr blp_decode_level(const unsigned char *s, size_t len, int level, BlpImage *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (level < 0 || level > 15) return w_fail(why, W_ERR_REFUSED, "picture level %d is outside 0 to 15", level);
    /* a dispatch over the two versions, by their first four bytes */
    if (len >= 4 && memcmp(s, "BLP2", 4) == 0) return decode_blp2(s, len, level, out, why);
    if (len >= 4 && memcmp(s, "BLP1", 4) == 0) return decode_blp1(s, len, level, out, why);
    return w_fail(why, W_ERR_CORRUPT, "not a BLP picture (neither BLP1 nor BLP2)");
}
/* }}} */

/* {{{ blp_decode */
WErr blp_decode(const unsigned char *s, size_t len, BlpImage *out, WWhy *why)
{
    return blp_decode_level(s, len, 0, out, why);
}
/* }}} */

/* {{{ blp_free */
void blp_free(BlpImage *img) { free(img->rgba); img->rgba = NULL; }
/* }}} */
