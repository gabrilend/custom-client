/*
 * 017-blp.c (issue 173k) - see 017-blp.h. Ported from src/custom-client/lib/blp.lua.
 */
#include "017-blp.h"
#include "013-errors.h"

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

/* {{{ blp_decode */
WErr blp_decode(const unsigned char *s, size_t len, BlpImage *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (len < 148 + 1024 || memcmp(s, "BLP2", 4) != 0) { w_fail(why, W_ERR_CORRUPT, "not a BLP2 texture"); return why->code; }
    int enc = s[8], abits = s[9], akind = s[10];
    int w = (int)rd32(s + 12), h = (int)rd32(s + 16);
    size_t at = rd32(s + 20);
    uint8_t *o = malloc((size_t)w * h * 4);
    if (enc == 2) {
        decode_dxt(s + at, w, h, akind, o);
    } else if (enc == 1) {
        /* palette: one byte a pixel, then the alpha bits packed after */
        const unsigned char *pal = s + 148;
        size_t n = (size_t)w * h;
        for (size_t i = 0; i < n; ++i) {
            int c = s[at + i];
            o[i * 4 + 0] = pal[c * 4 + 2]; o[i * 4 + 1] = pal[c * 4 + 1]; o[i * 4 + 2] = pal[c * 4 + 0];
            int a = 255;
            if (abits == 8) a = s[at + n + i];
            else if (abits == 4) { int byte = s[at + n + i / 2]; a = ((i % 2 == 0) ? (byte & 15) : (byte >> 4)) * 17; }
            else if (abits == 1) { int byte = s[at + n + i / 8]; a = ((byte >> (i % 8)) & 1) ? 255 : 0; }
            o[i * 4 + 3] = (uint8_t)a;
        }
    } else if (enc == 3) {
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            o[i * 4 + 0] = s[at + i * 4 + 2]; o[i * 4 + 1] = s[at + i * 4 + 1];
            o[i * 4 + 2] = s[at + i * 4 + 0]; o[i * 4 + 3] = s[at + i * 4 + 3];
        }
    } else {
        free(o);
        w_fail(why, W_ERR_CORRUPT, "encoding %d is not known", enc);
        return why->code;
    }
    out->width = w; out->height = h; out->rgba = o;
    return 0;
}
/* }}} */

/* {{{ blp_free */
void blp_free(BlpImage *img) { free(img->rgba); img->rgba = NULL; }
/* }}} */
