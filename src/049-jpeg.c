/*
 * 049-jpeg.c (issue 107) - see 049-jpeg.h. Ported from
 * world-edit-to-execute src/parsers/jpeg.lua, keeping its arithmetic
 * step for step so both give the same bytes. The decoder's state lives
 * in one Jpeg structure on the caller's stack for the length of one
 * decode (nothing kept in this file but constants), so any number of
 * threads may decode at once.
 */
#include "049-jpeg.h"
#include "013-errors.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* zigzag order: position k of the coded coefficients -> natural index
 * (row * 8 + column) of the 8 x 8 square */
static const int ZIGZAG[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

/* A Huffman table through the canonical lengths of the JPEG standard
 * (F.2.2.3): codes of each length are consecutive numbers, so a code of
 * length L is symbol valptr[L] + code - mincode[L] when it is at most
 * maxcode[L] (-1 when no code has that length). */
typedef struct {
    int     present;
    int32_t maxcode[17], mincode[17], valptr[17];
    uint8_t symbols[256];
} Huff;

typedef struct {
    int      id, h, v;            /* its number in the file; its sampling across and down */
    int      tq, td, ta;          /* its quantization table; its DC and AC Huffman tables (from the scan) */
    int      bw, bh, stride;      /* blocks across and down, padded to whole MCUs; samples a row */
    int32_t *coefs;               /* bw * bh blocks of 64, natural order */
    uint8_t *samples;             /* bw * 8 by bh * 8, after the transform */
} Comp;

typedef struct {
    const unsigned char *s;
    size_t   len, pos;
    WWhy    *why;
    int      failed;
    double   cos[64];             /* cos[x * 8 + u] = C(u) cos((2x + 1) u pi / 16) / 2 */
    double   qt[4][64];           /* natural order */
    int      have_qt[4];
    Huff     ht[2][4];            /* [0 DC, 1 AC][id] */
    int      restart;
    int      have_frame, progressive, width, height, nc, hmax, vmax, mcux, mcuy;
    Comp     comps[4];
    /* the scan's bit reader */
    uint32_t bitbuf;
    int      bitcnt, marker_hit;
    int32_t  pred[4];
    int      eobrun;
} Jpeg;

/* {{{ static fail
 * The first failure is kept; later ones (unwinding) don't overwrite it. */
static int fail(Jpeg *d, WErr code, const char *what)
{
    if (!d->failed) w_fail(d->why, code, "JPEG: %s", what);
    d->failed = 1;
    return -1;
}
/* }}} */

/* {{{ static u8, u16: header bytes, failing (not reading) past the end */
static int u8(Jpeg *d)
{
    if (d->pos >= d->len) return fail(d, W_ERR_CORRUPT, "ends inside a header");
    return d->s[d->pos++];
}
static int u16(Jpeg *d)
{
    int a = u8(d), b = u8(d);
    return d->failed ? -1 : a * 256 + b;
}
/* }}} */

/* {{{ static idct
 * 64 dequantized coefficients (natural order) into 64 samples 0..255:
 * rows, then columns, each a plain sum against the cosine table; the
 * Lua twin's order of additions, so the doubles round alike. */
static void idct(const Jpeg *d, const double *coef, uint8_t *out)
{
    double tmp[64];
    for (int y = 0; y < 8; ++y) {
        int r = y * 8;
        for (int x = 0; x < 8; ++x) {
            double s = 0;
            int cx = x * 8;
            for (int u = 0; u < 8; ++u) s = s + coef[r + u] * d->cos[cx + u];
            tmp[r + x] = s;
        }
    }
    for (int x = 0; x < 8; ++x)
        for (int y = 0; y < 8; ++y) {
            double s = 0;
            int cy = y * 8;
            for (int v = 0; v < 8; ++v) s = s + tmp[v * 8 + x] * d->cos[cy + v];
            double val = floor(s + 128.5);
            out[y * 8 + x] = (uint8_t)(val < 0 ? 0 : val > 255 ? 255 : val);
        }
}
/* }}} */

/* {{{ static build_huffman */
static void build_huffman(Huff *h, const int *counts)
{
    int32_t code = 0, k = 0;
    for (int len = 1; len <= 16; ++len) {
        int n = counts[len];
        if (n > 0) {
            h->valptr[len] = k;
            h->mincode[len] = code;
            code += n;
            k += n;
            h->maxcode[len] = code - 1;
        } else {
            h->maxcode[len] = -1;
        }
        code *= 2;
    }
    h->present = 1;
}
/* }}} */

/* {{{ the scan's bits */

/* One byte into the bit buffer, unstuffing FF 00. A marker (FF and
 * anything else), or the end of the bytes, stops input: zeros from then
 * on, as the standard asks of a decoder that runs short. */
static void fill(Jpeg *d)
{
    int b = 0;
    if (!d->marker_hit) {
        if (d->pos >= d->len) d->marker_hit = 1;
        else if (d->s[d->pos] == 0xFF) {
            if (d->pos + 1 < d->len && d->s[d->pos + 1] == 0) { b = 0xFF; d->pos += 2; }
            else d->marker_hit = 1;
        } else b = d->s[d->pos++];
    }
    d->bitbuf = ((d->bitbuf & 0xFFFFFFu) << 8) | (uint32_t)b;
    d->bitcnt += 8;
}
static int getbit(Jpeg *d)
{
    if (d->bitcnt == 0) fill(d);
    d->bitcnt--;
    return (int)((d->bitbuf >> d->bitcnt) & 1);
}
static int getbits(Jpeg *d, int k)
{
    int v = 0;
    for (int i = 0; i < k; ++i) v = v * 2 + getbit(d);
    return v;
}
/* the next symbol, or -1 (failed) on a code no table holds */
static int decode_symbol(Jpeg *d, const Huff *h)
{
    if (!h->present) return fail(d, W_ERR_CORRUPT, "a scan names a Huffman table the file doesn't define");
    int32_t code = getbit(d);
    for (int len = 1; len <= 16; ++len) {
        if (h->maxcode[len] >= 0 && code <= h->maxcode[len])
            return h->symbols[(h->valptr[len] + code - h->mincode[len]) & 255];
        code = code * 2 + getbit(d);
    }
    return fail(d, W_ERR_CORRUPT, "a Huffman code no table holds");
}
/* t bits read as a number, made negative as the standard's EXTEND says */
static int extend(int v, int t)
{
    if (t == 0) return 0;
    if (v < (1 << (t - 1))) return v - (1 << t) + 1;
    return v;
}
/* }}} */

/* {{{ one block, by the kind of pass
 * ci: the part's place in this scan (its DC prediction); co: its 64
 * coefficients. Each returns early once the decode has failed. */
static void block_baseline(Jpeg *d, const Comp *c, int ci, int32_t *co)
{
    int t = decode_symbol(d, &d->ht[0][c->td]);
    if (t < 0) return;
    d->pred[ci] += extend(getbits(d, t), t);
    co[0] = d->pred[ci];
    const Huff *ac = &d->ht[1][c->ta];
    int k = 1;
    while (k < 64) {
        int rs = decode_symbol(d, ac);
        if (rs < 0) return;
        int r = rs >> 4, s = rs & 15;
        if (s == 0) {
            if (r != 15) break;
            k += 16;
        } else {
            k += r;
            if (k > 63) break;
            co[ZIGZAG[k]] = extend(getbits(d, s), s);
            k++;
        }
    }
}

static void block_dc_first(Jpeg *d, const Comp *c, int ci, int32_t *co, int al)
{
    int t = decode_symbol(d, &d->ht[0][c->td]);
    if (t < 0) return;
    d->pred[ci] += extend(getbits(d, t), t);
    co[0] = d->pred[ci] * (1 << al);
}

static void block_dc_refine(Jpeg *d, int32_t *co, int al)
{
    if (getbit(d) == 1) co[0] |= (1 << al);
}

static void block_ac_first(Jpeg *d, const Comp *c, int32_t *co, int ss, int se, int al)
{
    if (d->eobrun > 0) { d->eobrun--; return; }
    const Huff *ac = &d->ht[1][c->ta];
    int k = ss;
    while (k <= se) {
        int rs = decode_symbol(d, ac);
        if (rs < 0) return;
        int r = rs >> 4, s = rs & 15;
        if (s == 0) {
            if (r < 15) {
                d->eobrun = (1 << r) - 1;
                if (r > 0) d->eobrun += getbits(d, r);
                break;
            }
            k += 16;
        } else {
            k += r;
            if (k > 63) { fail(d, W_ERR_CORRUPT, "a coefficient past the 64th"); return; }
            co[ZIGZAG[k]] = extend(getbits(d, s), s) * (1 << al);
            k++;
        }
    }
}

/* a coefficient already nonzero gains the next bit of its size */
static void refine(Jpeg *d, int32_t *co, int z, int32_t p1, int32_t m1)
{
    if (getbit(d) == 1 && (co[z] & p1) == 0) co[z] += co[z] >= 0 ? p1 : m1;
}

static void block_ac_refine(Jpeg *d, const Comp *c, int32_t *co, int ss, int se, int al)
{
    int32_t p1 = 1 << al, m1 = -(1 << al);
    int k = ss;
    if (d->eobrun <= 0) {
        const Huff *ac = &d->ht[1][c->ta];
        while (k <= se) {
            int rs = decode_symbol(d, ac);
            if (rs < 0) return;
            int r = rs >> 4, s = rs & 15;
            int32_t value = 0;
            if (s == 0) {
                if (r < 15) {
                    d->eobrun = 1 << r;
                    if (r > 0) d->eobrun += getbits(d, r);
                    break;
                }
            } else {
                value = getbit(d) == 1 ? p1 : m1;
            }
            /* skip r zero coefficients (refining the nonzero ones met),
             * then place the new value */
            while (k <= se) {
                int z = ZIGZAG[k];
                if (co[z] != 0) {
                    refine(d, co, z, p1, m1);
                } else {
                    if (r == 0) {
                        if (value != 0) co[z] = value;
                        k++;
                        break;
                    }
                    r--;
                }
                k++;
            }
        }
    }
    if (d->eobrun > 0) {
        while (k <= se) {
            int z = ZIGZAG[k];
            if (co[z] != 0) refine(d, co, z, p1, m1);
            k++;
        }
        d->eobrun--;
    }
}
/* }}} */

/* {{{ static scan
 * One scan's entropy-coded data into its parts' coefficients (baseline,
 * or one progressive pass: spectral band ss..se, successive
 * approximation ah, al). Leaves pos at the next marker. */
typedef enum { PASS_BASELINE, PASS_DC_FIRST, PASS_DC_REFINE, PASS_AC_FIRST, PASS_AC_REFINE } Pass;

static void one_block(Jpeg *d, Pass pass, const Comp *c, int ci, int32_t *co, int ss, int se, int al)
{
    /* a dispatch over the five kinds of pass: each kind reads a block's bits its own way */
    switch (pass) {
    case PASS_BASELINE:  block_baseline(d, c, ci, co); break;
    case PASS_DC_FIRST:  block_dc_first(d, c, ci, co, al); break;
    case PASS_DC_REFINE: block_dc_refine(d, co, al); break;
    case PASS_AC_FIRST:  block_ac_first(d, c, co, ss, se, al); break;
    case PASS_AC_REFINE: block_ac_refine(d, c, co, ss, se, al); break;
    }
}

/* At a restart interval's end: the bit reader, the DC predictions and
 * the end-of-band run start over, and the restart marker is stepped
 * over. The Lua twin steps over it only when the bit reader had already
 * met it; this steps over it either way, which is what the standard
 * means. WC3's pictures carry no restart intervals, so the two agree
 * on every one of them. */
static void restart_interval(Jpeg *d, int ns)
{
    d->bitbuf = 0; d->bitcnt = 0; d->eobrun = 0;
    for (int i = 0; i < ns; ++i) d->pred[i] = 0;
    d->marker_hit = 0;
    while (d->pos < d->len && d->s[d->pos] == 0xFF) d->pos++;
    if (d->pos < d->len && d->s[d->pos] >= 0xD0 && d->s[d->pos] <= 0xD7) d->pos++;
}

static void scan(Jpeg *d, Comp **sc, int ns, int ss, int se, int ah, int al)
{
    Pass pass = !d->progressive ? PASS_BASELINE
              : ss == 0 ? (ah == 0 ? PASS_DC_FIRST : PASS_DC_REFINE)
              : (ah == 0 ? PASS_AC_FIRST : PASS_AC_REFINE);
    if (ss > 63 || se > 63 || (d->progressive && ss > se)) { fail(d, W_ERR_CORRUPT, "a scan's band is outside 0 to 63"); return; }
    d->bitbuf = 0; d->bitcnt = 0; d->marker_hit = 0; d->eobrun = 0;
    for (int i = 0; i < ns; ++i) d->pred[i] = 0;
    long mcus = 0;
    if (ns == 1) {
        /* one part alone: its blocks cover the part itself, unpadded */
        Comp *c = sc[0];
        int bw = ((d->width * c->h + d->hmax - 1) / d->hmax + 7) / 8;
        int bh = ((d->height * c->v + d->vmax - 1) / d->vmax + 7) / 8;
        for (int by = 0; by < bh && !d->failed; ++by)
            for (int bx = 0; bx < bw && !d->failed; ++bx) {
                if (d->restart > 0 && mcus > 0 && mcus % d->restart == 0) restart_interval(d, ns);
                one_block(d, pass, c, 0, c->coefs + ((size_t)by * c->bw + bx) * 64, ss, se, al);
                mcus++;
            }
    } else {
        for (int my = 0; my < d->mcuy && !d->failed; ++my)
            for (int mx = 0; mx < d->mcux && !d->failed; ++mx) {
                if (d->restart > 0 && mcus > 0 && mcus % d->restart == 0) restart_interval(d, ns);
                for (int ci = 0; ci < ns; ++ci) {
                    Comp *c = sc[ci];
                    for (int v = 0; v < c->v; ++v)
                        for (int hh = 0; hh < c->h; ++hh)
                            one_block(d, pass, c, ci, c->coefs + ((size_t)(my * c->v + v) * c->bw + (mx * c->h + hh)) * 64, ss, se, al);
                }
                mcus++;
            }
    }
    /* after the entropy data: the next marker (not a stuffed FF 00, not a restart) */
    while (d->pos < d->len) {
        if (d->s[d->pos] == 0xFF && d->pos + 1 < d->len && d->s[d->pos + 1] != 0
            && !(d->s[d->pos + 1] >= 0xD0 && d->s[d->pos + 1] <= 0xD7)) break;
        d->pos++;
    }
}
/* }}} */

/* {{{ static read_segments
 * The markers up to the end of the picture: tables, the frame, scans. */
static void read_segments(Jpeg *d)
{
    if (u16(d) != 0xFFD8) { fail(d, W_ERR_CORRUPT, "not a JPEG (no start-of-image mark)"); return; }
    while (d->pos < d->len && !d->failed) {
        if (u8(d) != 0xFF) { fail(d, W_ERR_CORRUPT, "expected a marker"); return; }
        int m = u8(d);
        while (m == 0xFF) m = u8(d);
        if (d->failed) return;
        if (m == 0xD9) return;                               /* end of image */
        if (m >= 0xD0 && m <= 0xD7) continue;                /* a stray restart */
        int len = u16(d);
        if (d->failed) return;
        if (len < 2 || d->pos + (size_t)len - 2 > d->len) { fail(d, W_ERR_CORRUPT, "a segment runs past the end"); return; }
        size_t seg_end = d->pos + (size_t)len - 2;
        /* a dispatch over the segment kinds this reader needs; the rest are stepped over */
        if (m == 0xDB) {                                     /* quantization tables */
            while (d->pos < seg_end && !d->failed) {
                int pq = u8(d), prec = pq >> 4, id = pq & 15;
                if (id > 3) { fail(d, W_ERR_CORRUPT, "a quantization table numbered past 3"); return; }
                for (int k = 0; k < 64; ++k) d->qt[id][ZIGZAG[k]] = prec == 0 ? u8(d) : u16(d);
                d->have_qt[id] = 1;
            }
        } else if (m == 0xC4) {                              /* Huffman tables */
            while (d->pos < seg_end && !d->failed) {
                int tc = u8(d), cls = tc >> 4, id = tc & 15;
                if (cls > 1 || id > 3) { fail(d, W_ERR_CORRUPT, "a Huffman table of an unknown class or number"); return; }
                int counts[17] = { 0 }, total = 0;
                for (int i = 1; i <= 16; ++i) { counts[i] = u8(d); total += counts[i]; }
                if (total > 256) { fail(d, W_ERR_CORRUPT, "a Huffman table of more than 256 symbols"); return; }
                Huff *h = &d->ht[cls][id];
                memset(h, 0, sizeof *h);
                for (int i = 0; i < total; ++i) h->symbols[i] = (uint8_t)u8(d);
                build_huffman(h, counts);
            }
        } else if (m == 0xC0 || m == 0xC1 || m == 0xC2) {    /* the frame */
            if (d->have_frame) { fail(d, W_ERR_UNSUPPORTED, "a second frame in one picture"); return; }
            int precision = u8(d);
            if (precision != 8) { fail(d, W_ERR_UNSUPPORTED, "samples of other than 8 bits"); return; }
            d->height = u16(d); d->width = u16(d);
            d->nc = u8(d);
            if (d->failed) return;
            if (d->nc < 1 || d->nc > 4) { fail(d, W_ERR_UNSUPPORTED, "a part count other than 1 to 4"); return; }
            if (d->width < 1 || d->height < 1) { fail(d, W_ERR_CORRUPT, "a frame with no width or height"); return; }
            d->progressive = m == 0xC2;
            d->hmax = 1; d->vmax = 1;
            for (int i = 0; i < d->nc; ++i) {
                Comp *c = &d->comps[i];
                c->id = u8(d);
                int s = u8(d);
                c->h = s >> 4; c->v = s & 15;
                c->tq = u8(d);
                if (c->h < 1 || c->h > 4 || c->v < 1 || c->v > 4 || c->tq > 3) { fail(d, W_ERR_CORRUPT, "a part's sampling or table is out of range"); return; }
                if (c->h > d->hmax) d->hmax = c->h;
                if (c->v > d->vmax) d->vmax = c->v;
            }
            d->mcux = (d->width + 8 * d->hmax - 1) / (8 * d->hmax);
            d->mcuy = (d->height + 8 * d->vmax - 1) / (8 * d->vmax);
            for (int i = 0; i < d->nc; ++i) {
                Comp *c = &d->comps[i];
                c->bw = d->mcux * c->h;                      /* blocks across, padded to whole MCUs */
                c->bh = d->mcuy * c->v;
                c->stride = c->bw * 8;
                c->coefs = calloc((size_t)c->bw * c->bh * 64, sizeof *c->coefs);
                if (!c->coefs) { fail(d, W_ERR_FULL, "no memory for the coefficients"); return; }
            }
            d->have_frame = 1;
        } else if (m == 0xC3 || (m >= 0xC5 && m <= 0xCF && m != 0xC8 && m != 0xCC)) {
            fail(d, W_ERR_UNSUPPORTED, "a lossless, hierarchical or arithmetic-coded frame");
            return;
        } else if (m == 0xDD) {                              /* restart interval */
            d->restart = u16(d);
        } else if (m == 0xDA) {                              /* a scan */
            if (!d->have_frame) { fail(d, W_ERR_CORRUPT, "a scan before the frame"); return; }
            int ns = u8(d);
            if (ns < 1 || ns > d->nc) { fail(d, W_ERR_CORRUPT, "a scan of more parts than the frame has"); return; }
            Comp *sc[4];
            for (int i = 0; i < ns; ++i) {
                int cid = u8(d), tables = u8(d);
                sc[i] = NULL;
                for (int k = 0; k < d->nc; ++k)
                    if (d->comps[k].id == cid) {
                        d->comps[k].td = (tables >> 4) & 3;
                        d->comps[k].ta = tables & 3;
                        sc[i] = &d->comps[k];
                    }
                if (!sc[i]) { fail(d, W_ERR_CORRUPT, "a scan names a part the frame doesn't have"); return; }
            }
            int ss = u8(d), se = u8(d), a = u8(d);
            if (d->failed) return;
            d->pos = seg_end;
            scan(d, sc, ns, ss, se, a >> 4, a & 15);
            continue;
        }
        d->pos = seg_end;
    }
}
/* }}} */

/* {{{ static assemble
 * Dequantize and transform every block, widen each part to full size,
 * then colour (three parts) or leave as stored (one, four). */
static void assemble(Jpeg *d, JpegImage *out)
{
    double coef[64];
    uint8_t block[64];
    for (int i = 0; i < d->nc; ++i) {
        Comp *c = &d->comps[i];
        if (!d->have_qt[c->tq]) { fail(d, W_ERR_CORRUPT, "a part uses a quantization table the file doesn't define"); return; }
        const double *q = d->qt[c->tq];
        c->samples = malloc((size_t)c->bw * 8 * c->bh * 8);
        if (!c->samples) { fail(d, W_ERR_FULL, "no memory for the samples"); return; }
        for (int by = 0; by < c->bh; ++by)
            for (int bx = 0; bx < c->bw; ++bx) {
                const int32_t *b0 = c->coefs + ((size_t)by * c->bw + bx) * 64;
                for (int k = 0; k < 64; ++k) coef[k] = b0[k] * q[k];
                idct(d, coef, block);
                uint8_t *base = c->samples + (size_t)by * 8 * c->stride + bx * 8;
                for (int y = 0; y < 8; ++y) memcpy(base + (size_t)y * c->stride, block + y * 8, 8);
            }
    }
    int w = d->width, h = d->height, nc = d->nc;
    uint8_t *px = malloc((size_t)w * h * nc);
    if (!px) { fail(d, W_ERR_FULL, "no memory for the picture"); return; }
    for (int ci = 0; ci < nc; ++ci) {
        const Comp *c = &d->comps[ci];
        double sx = (double)d->hmax / c->h, sy = (double)d->vmax / c->v;
        for (int y = 0; y < h; ++y) {
            const uint8_t *row = c->samples + (size_t)floor(y / sy) * c->stride;
            uint8_t *o = px + (size_t)y * w * nc + ci;
            for (int x = 0; x < w; ++x) o[(size_t)x * nc] = row[(size_t)floor(x / sx)];
        }
    }
    if (nc == 3)
        for (size_t i = 0; i < (size_t)w * h; ++i) {
            uint8_t *o = px + i * 3;
            double Y = o[0], cb = o[1] - 128.0, cr = o[2] - 128.0;
            double r = Y + 1.402 * cr;
            double g = Y - 0.344136 * cb - 0.714136 * cr;
            double b = Y + 1.772 * cb;
            o[0] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r + 0.5);
            o[1] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g + 0.5);
            o[2] = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b + 0.5);
        }
    out->width = w; out->height = h; out->components = nc; out->pixels = px;
}
/* }}} */

/* {{{ jpeg_decode */
WErr jpeg_decode(const unsigned char *s, size_t len, JpegImage *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    Jpeg *d = calloc(1, sizeof *d);                          /* ~10 KB: on the heap, not a thread's stack */
    if (!d) return w_fail(why, W_ERR_FULL, "JPEG: no memory for the decoder");
    d->s = s; d->len = len; d->why = why;
    for (int x = 0; x < 8; ++x)
        for (int u = 0; u < 8; ++u) {
            double c = u == 0 ? 1 / sqrt(2) : 1;
            d->cos[x * 8 + u] = c * cos((2 * x + 1) * u * 3.14159265358979323846 / 16) / 2;   /* Lua's math.pi */
        }
    read_segments(d);
    if (!d->failed && !d->have_frame) fail(d, W_ERR_CORRUPT, "no frame");
    if (!d->failed) assemble(d, out);
    for (int i = 0; i < 4; ++i) { free(d->comps[i].coefs); free(d->comps[i].samples); }
    WErr e = d->failed ? why->code : W_OK;
    free(d);
    return e;
}
/* }}} */

/* {{{ jpeg_free */
void jpeg_free(JpegImage *img) { free(img->pixels); img->pixels = NULL; }
/* }}} */
