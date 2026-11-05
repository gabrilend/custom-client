/*
 * 020-adt.c (issue 173f) - the original client's ground files; see 020-adt.h
 *
 * For a general audience: takes one ground file apart into its 256
 * chunks, each with its heights, its up-to-four ground pictures and how
 * strongly each shows at every point, and its baked shadow.
 */
#include "020-adt.h"
#include "013-errors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ readers: little-endian numbers at a byte offset */
static uint32_t u32(const unsigned char *s, size_t at) { return s[at] | s[at + 1] << 8 | s[at + 2] << 16 | (uint32_t)s[at + 3] << 24; }
static float    f32(const unsigned char *s, size_t at) { uint32_t u = u32(s, at); float f; memcpy(&f, &u, 4); return f; }
/* a block's tag reads as text reversed: "MCNK" is stored "KNCM" */
static int is_tag(const unsigned char *s, size_t at, const char *tag)
{
    return s[at] == tag[3] && s[at + 1] == tag[2] && s[at + 2] == tag[1] && s[at + 3] == tag[0];
}
/* }}} */

/* {{{ adt_place_read */
void adt_place_read(const unsigned char *e, int doodad, AdtPlace *p)
{
    memset(p, 0, sizeof *p);
    p->name = (int)u32(e, 0);
    p->uid  = u32(e, 4);
    p->x = 17066.666f - f32(e, 16);
    p->y = 17066.666f - f32(e, 8);
    p->z = f32(e, 12);
    for (int r = 0; r < 3; ++r) p->rot[r] = f32(e, 20 + r * 4);
    p->scale = doodad ? (float)(e[32] | e[33] << 8) / 1024.0f : 1.0f;
    p->doodad_set = doodad ? 0 : (int)(e[58] | e[59] << 8);
    if (!doodad)
        for (int a = 0; a < 2; ++a) {
            /* placement (x, y, z) -> world (17066.67 - z, 17066.67 - x, y);
             * the corners swap since the axes flip */
            size_t c = 32 + (size_t)a * 12;
            float wx = 17066.666f - f32(e, c + 8), wy = 17066.666f - f32(e, c), wz = f32(e, c + 4);
            if (a == 0) { p->hi[0] = wx; p->hi[1] = wy; p->lo[2] = wz; }
            else        { p->lo[0] = wx; p->lo[1] = wy; p->hi[2] = wz; }
        }
}
/* }}} */

/* {{{ adt_wdt_big_alpha */
int adt_wdt_big_alpha(const unsigned char *s, size_t len, WWhy *why)
{
    for (size_t at = 0; at + 8 <= len; at += 8 + u32(s, at + 4))
        if (is_tag(s, at, "MPHD")) {
            if (at + 12 > len) break;
            return (u32(s, at + 8) & 0x4) ? 1 : 0;
        }
    w_fail(why, W_ERR_CORRUPT, "the map's WDT file has no MPHD block (read %zu bytes)", len);
    return -1;                                       /* a value function: 0 or 1 is the answer, below 0 a failure */
}
/* }}} */

/* {{{ static read_alpha: one layer's 64 x 64 strengths from MCAL
 * 4-bit: two points a byte, low half first, each 0-15 scaled to 0-255.
 *   Unless the chunk says otherwise (flags 0x8000), the last row and
 *   column were never painted and copy the ones before them, as the
 *   original client does.
 * 8-bit plain: 4096 bytes.
 * 8-bit packed: runs; a byte's high bit says "repeat the next byte" (set)
 *   or "copy the next bytes" (clear), its low 7 bits how many.
 * Returns the bytes used, or 0 when the data runs out. */
static size_t read_alpha(const unsigned char *s, size_t avail, int big, int packed, int fix, uint8_t *out)
{
    if (!big) {
        if (avail < 2048) return 0;
        for (int i = 0; i < 2048; ++i) {
            out[i * 2]     = (uint8_t)((s[i] & 0x0f) * 17);
            out[i * 2 + 1] = (uint8_t)((s[i] >> 4) * 17);
        }
        if (fix)
            for (int r = 0; r < 64; ++r) {
                out[r * 64 + 63] = out[r * 64 + 62];
                if (r == 63) memcpy(out + 63 * 64, out + 62 * 64, 64);
            }
        return 2048;
    }
    if (!packed) {
        if (avail < 4096) return 0;
        memcpy(out, s, 4096);
        return 4096;
    }
    size_t at = 0;
    int n = 0;
    while (n < 4096) {
        if (at >= avail) return 0;
        int fill = s[at] & 0x80, count = s[at] & 0x7f;
        ++at;
        for (int k = 0; k < count && n < 4096; ++k) {
            if (at >= avail) return 0;
            out[n++] = s[at];
            if (!fill) ++at;
        }
        if (fill) ++at;
    }
    return at;
}
/* }}} */

/* {{{ static read_chunk: one MCNK block, `at` at its tag */
static WErr read_chunk(const unsigned char *s, size_t len, size_t at, int big_alpha, AdtChunk *c, WWhy *why)
{
    size_t size = u32(s, at + 4), h = at + 8;
    if (h + 128 > len || at + 8 + size > len) { w_fail(why, W_ERR_CORRUPT, "MCNK block at byte %zu runs past the file's end", at); return why->code; }
    uint32_t flags = u32(s, h);
    c->ix = (int)u32(s, h + 4);
    c->iy = (int)u32(s, h + 8);
    c->nlayers = (int)u32(s, h + 12);
    c->holes = (uint16_t)u32(s, h + 60);
    c->x = f32(s, h + 104);
    c->y = f32(s, h + 108);
    c->z = f32(s, h + 112);
    if (c->ix > 15 || c->iy > 15 || c->nlayers < 1 || c->nlayers > 4) {
        w_fail(why, W_ERR_CORRUPT, "MCNK at byte %zu: index (%d, %d) or layer count %d out of range", at, c->ix, c->iy, c->nlayers);
        return why->code;
    }
    /* each sub-block found by its header offset, its tag checked */
    #define SUB(off, tag, need) \
        size_t off##_at = at + u32(s, h + (off)); \
        if (off##_at + 8 + (need) > at + 8 + size || !is_tag(s, off##_at, tag)) { \
            w_fail(why, W_ERR_CORRUPT, "chunk (%d, %d): no %s block where its header says (byte %zu)", c->ix, c->iy, tag, off##_at); \
            return why->code; }
    enum { OFS_MCVT = 20, OFS_MCLY = 28, OFS_MCAL = 36, OFS_MCSH = 44 };
    SUB(OFS_MCVT, "MCVT", 145 * 4)
    for (int i = 0; i < 145; ++i) c->heights[i] = f32(s, OFS_MCVT_at + 8 + i * 4);
    SUB(OFS_MCLY, "MCLY", (size_t)c->nlayers * 16)
    size_t mcal_at = at + u32(s, h + OFS_MCAL), mcal_size = 0;
    if (c->nlayers > 1) {
        if (mcal_at + 8 > at + 8 + size || !is_tag(s, mcal_at, "MCAL")) {
            w_fail(why, W_ERR_CORRUPT, "chunk (%d, %d): %d layers but no MCAL block where its header says", c->ix, c->iy, c->nlayers);
            return why->code;
        }
        /* the size comes from the chunk header (offset 40, which counts
         * MCAL's own 8-byte head), not from MCAL's size field: in packed
         * tiles that field can be far too small (Northrend_30_20 chunk 0:
         * 512 in MCAL, 3688 in the header, and the blend maps use 3680) */
        uint32_t told = u32(s, h + 40);
        if (told < 8 || mcal_at + told > at + 8 + size) {
            w_fail(why, W_ERR_CORRUPT, "chunk (%d, %d): its header gives MCAL %u bytes, which doesn't fit in the chunk", c->ix, c->iy, told);
            return why->code;
        }
        mcal_size = told - 8;
    }
    memset(c->alpha[0], 255, 4096);
    for (int l = 0; l < c->nlayers; ++l) {
        size_t ly = OFS_MCLY_at + 8 + (size_t)l * 16;
        c->tex[l] = (int)u32(s, ly);
        uint32_t lflags = u32(s, ly + 4), offset = u32(s, ly + 8);
        if (l == 0) continue;
        if (!(lflags & 0x100)) { memset(c->alpha[l], 0, 4096); continue; }   /* a layer with no blend map doesn't show */
        if (offset >= mcal_size || !read_alpha(s + mcal_at + 8 + offset, mcal_size - offset, big_alpha, (lflags & 0x200) != 0,
                                               !(flags & 0x8000), c->alpha[l])) {
            w_fail(why, W_ERR_CORRUPT, "chunk (%d, %d) layer %d: its blend map runs past MCAL's end (offset %u of %zu)", c->ix, c->iy, l, offset, mcal_size);
            return why->code;
        }
    }
    /* the shadow is optional: a chunk with none is all lit */
    memset(c->shadow, 0, 4096);
    if (u32(s, h + 48) >= 512) {
        SUB(OFS_MCSH, "MCSH", 512)
        for (int i = 0; i < 4096; ++i) c->shadow[i] = (s[OFS_MCSH_at + 8 + i / 8] >> (i % 8)) & 1;
    }
    /* colour tints, when the chunk says it has them */
    memset(c->tint, 127, sizeof c->tint);
    if (flags & 0x40) {
        enum { OFS_MCCV = 116 };
        SUB(OFS_MCCV, "MCCV", 145 * 4)
        for (int i = 0; i < 145; ++i) {
            c->tint[i][0] = s[OFS_MCCV_at + 8 + i * 4 + 2];
            c->tint[i][1] = s[OFS_MCCV_at + 8 + i * 4 + 1];
            c->tint[i][2] = s[OFS_MCCV_at + 8 + i * 4];
        }
    }
    #undef SUB
    return 0;
}
/* }}} */

/* {{{ adt_read */
WErr adt_read(const unsigned char *s, size_t len, int big_alpha, AdtTile *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    int nchunks = 0;
    /* names come in two blocks: the text (MMDX / MWMO), then where each
     * name starts (MMID / MWID); kept until both are seen */
    const unsigned char *mmdx = NULL, *mwmo = NULL;
    size_t mmdx_len = 0, mwmo_len = 0;
    for (size_t at = 0; at + 8 <= len; at += 8 + u32(s, at + 4)) {
        size_t size = u32(s, at + 4);
        if (at + 8 + size > len) { w_fail(why, W_ERR_CORRUPT, "a block at byte %zu says it is %zu bytes, past the file's end (%zu)", at, size, len); adt_free(out); return why->code; }
        if (is_tag(s, at, "MTEX")) {
            int n = 0;
            for (size_t i = 0; i < size; ++i) if (s[at + 8 + i] == 0) ++n;
            out->textures = calloc(n ? n : 1, sizeof *out->textures);
            const char *name = (const char *)s + at + 8;
            for (int k = 0; k < n; ++k) {
                snprintf(out->textures[k], sizeof out->textures[k], "%s", name);
                name += strlen(name) + 1;
            }
            out->ntextures = n;
        } else if (is_tag(s, at, "MH2O")) {
            /* water: per chunk, its layers (173r) */
            const unsigned char *w = s + at + 8;
            if (size < 256 * 12) { w_fail(why, W_ERR_CORRUPT, "MH2O is %zu bytes, shorter than its 256 heads", size); adt_free(out); return why->code; }
            for (int k = 0; k < 256; ++k) {
                uint32_t oi = u32(w, (size_t)k * 12), layers = u32(w, (size_t)k * 12 + 4);
                for (uint32_t l = 0; l < layers; ++l) {
                    size_t e = oi + (size_t)l * 24;
                    if (e + 24 > size) { w_fail(why, W_ERR_CORRUPT, "chunk %d's water layer %u runs past MH2O", k, l); adt_free(out); return why->code; }
                    AdtLiquid q;
                    memset(&q, 0, sizeof q);
                    q.chunk = k;
                    q.kind = w[e] | w[e + 1] << 8;
                    int fmt = w[e + 2] | w[e + 3] << 8;
                    float lo = f32(w, e + 4);
                    q.col = w[e + 12]; q.row = w[e + 13]; q.w = w[e + 14]; q.h = w[e + 15];
                    if (q.col + q.w > 8 || q.row + q.h > 8) { w_fail(why, W_ERR_CORRUPT, "chunk %d's water covers cells past the chunk", k); adt_free(out); return why->code; }
                    uint32_t ob = u32(w, e + 16), ov = u32(w, e + 20);
                    for (int r = 0; r < q.h; ++r)
                        for (int c = 0; c < q.w; ++c) {
                            int bit = r * q.w + c;
                            q.wet[q.row + r][q.col + c] = ob == 0 ? 1 : (ob + (size_t)bit / 8 < size ? (w[ob + bit / 8] >> (bit % 8)) & 1 : 0);
                        }
                    for (int r = 0; r <= 8; ++r) for (int c = 0; c <= 8; ++c) { q.height[r][c] = lo; q.depth[r][c] = 255; }
                    /* heights where the format has them (0, 1, 3); else flat.
                     * A height outside the layer's own stated range is not
                     * a surface: some layers hold 0 at points round their
                     * wet cells (Kalimdor_32_26 chunk 152: stated
                     * 102.76-102.76, points of 0), which drew the water as a
                     * slanted wall; such a point takes the lowest height. */
                    float hi = f32(w, e + 8);
                    if (ov && fmt != 2 && ov + (size_t)(q.w + 1) * (q.h + 1) * 4 <= size)
                        for (int r = 0; r <= q.h; ++r)
                            for (int c = 0; c <= q.w; ++c) {
                                float h = f32(w, ov + ((size_t)r * (q.w + 1) + c) * 4);
                                q.height[q.row + r][q.col + c] = (h >= lo - 0.01f && h <= hi + 0.01f) ? h : lo;
                            }
                    /* depths (173r), a byte a point, where the format has
                     * them: 0 after the heights, 2 alone, 3 after the heights
                     * and picture positions (4 bytes a point each); 1 has
                     * none (left 255, deep) */
                    size_t np = (size_t)(q.w + 1) * (q.h + 1);
                    size_t dat = fmt == 0 ? ov + np * 4 : fmt == 2 ? ov : fmt == 3 ? ov + np * 8 : 0;
                    if (ov && dat && dat + np <= size)
                        for (int r = 0; r <= q.h; ++r)
                            for (int c = 0; c <= q.w; ++c) q.depth[q.row + r][q.col + c] = w[dat + (size_t)r * (q.w + 1) + c];
                    out->liquids = realloc(out->liquids, sizeof *out->liquids * (out->nliquids + 1));
                    out->liquids[out->nliquids++] = q;
                }
            }
        } else if (is_tag(s, at, "MMDX")) { mmdx = s + at + 8; mmdx_len = size;
        } else if (is_tag(s, at, "MWMO")) { mwmo = s + at + 8; mwmo_len = size;
        } else if (is_tag(s, at, "MMID") || is_tag(s, at, "MWID")) {
            int is_m = is_tag(s, at, "MMID");
            const unsigned char *text = is_m ? mmdx : mwmo;
            size_t text_len = is_m ? mmdx_len : mwmo_len;
            int n = (int)(size / 4);
            char (*names)[260] = calloc(n ? n : 1, sizeof *names);
            for (int k = 0; k < n; ++k) {
                uint32_t o = u32(s, at + 8 + (size_t)k * 4);
                if (!text || o >= text_len) { w_fail(why, W_ERR_CORRUPT, "%s entry %d points past its names", is_m ? "MMID" : "MWID", k); free(names); adt_free(out); return why->code; }
                snprintf(names[k], sizeof names[k], "%.*s", (int)strnlen((const char *)text + o, text_len - o), text + o);
            }
            if (is_m) { out->models = names; out->nmodels = n; } else { out->buildings = names; out->nbuildings = n; }
        } else if (is_tag(s, at, "MDDF") || is_tag(s, at, "MODF")) {
            int is_d = is_tag(s, at, "MDDF");
            size_t each = is_d ? 36 : 64;
            int n = (int)(size / each);
            AdtPlace *list = calloc(n ? n : 1, sizeof *list);
            for (int k = 0; k < n; ++k) adt_place_read(s + at + 8 + (size_t)k * each, is_d, &list[k]);
            if (is_d) { out->doodads = list; out->ndoodads = n; } else { out->places = list; out->nplaces = n; }
        } else if (is_tag(s, at, "MCNK")) {
            if (nchunks == 256) { w_fail(why, W_ERR_CORRUPT, "more than 256 MCNK blocks"); adt_free(out); return why->code; }
            AdtChunk c;
            if (read_chunk(s, len, at, big_alpha, &c, why)) { adt_free(out); return why->code; }
            out->chunks[c.iy * 16 + c.ix] = c;
            ++nchunks;
        }
    }
    if (nchunks != 256) { w_fail(why, W_ERR_CORRUPT, "%d MCNK blocks, not 256", nchunks); adt_free(out); return why->code; }
    for (int k = 0; k < out->ndoodads; ++k)
        if (out->doodads[k].name < 0 || out->doodads[k].name >= out->nmodels) {
            w_fail(why, W_ERR_CORRUPT, "doodad placement %d names model %d; the tile lists %d", k, out->doodads[k].name, out->nmodels);
            adt_free(out);
            return why->code;
        }
    for (int k = 0; k < out->nplaces; ++k)
        if (out->places[k].name < 0 || out->places[k].name >= out->nbuildings) {
            w_fail(why, W_ERR_CORRUPT, "building placement %d names building %d; the tile lists %d", k, out->places[k].name, out->nbuildings);
            adt_free(out);
            return why->code;
        }
    for (int i = 0; i < 256; ++i)
        for (int l = 0; l < out->chunks[i].nlayers; ++l)
            if (out->chunks[i].tex[l] < 0 || out->chunks[i].tex[l] >= out->ntextures) {
                w_fail(why, W_ERR_CORRUPT, "chunk %d layer %d names picture %d; the tile lists %d", i, l, out->chunks[i].tex[l], out->ntextures);
                adt_free(out);
                return why->code;
            }
    return 0;
}
/* {{{ adt_wdl_read */
WErr adt_wdl_read(const unsigned char *s, size_t len, AdtWdl *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    size_t maof = 0;
    for (size_t at = 0; at + 8 <= len && at + 8 + u32(s, at + 4) <= len; at += 8 + u32(s, at + 4))
        if (is_tag(s, at, "MAOF")) { maof = at + 8; break; }
    if (!maof || maof + 4096 * 4 > len) { w_fail(why, W_ERR_CORRUPT, "no tile list (MAOF)"); return why->code; }
    for (int i = 0; i < 4096; ++i) {
        uint32_t o = u32(s, maof + (size_t)i * 4);
        if (!o || (size_t)o + 8 + 1090 > len || !is_tag(s, o, "MARE")) continue;
        for (int k = 0; k < 17 * 17; ++k) out->heights[i][k] = (int16_t)(s[o + 8 + k * 2] | s[o + 9 + k * 2] << 8);
        size_t h = o + 8 + 1090;
        if (h + 8 + 32 <= len && is_tag(s, h, "MAHO"))
            for (int r = 0; r < 16; ++r) out->holes[i][r] = (uint16_t)(s[h + 8 + r * 2] | s[h + 9 + r * 2] << 8);
        out->have[i] = 1;
    }
    return 0;
}
/* }}} */

/* {{{ adt_place_matrix */
static void mul3(const float a[9], const float b[9], float out[9])
{
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            out[r * 3 + c] = a[r * 3] * b[c] + a[r * 3 + 1] * b[3 + c] + a[r * 3 + 2] * b[6 + c];
}
void adt_place_matrix(const AdtPlace *p, double ox, double oy, float m[16])
{
    const float rad = 3.14159265358979f / 180.0f;
    float ax = p->rot[2] * rad, ay = (p->rot[1] - 90.0f) * rad, az = -p->rot[0] * rad;
    /* row-major 3 x 3s, each the usual right-handed turn about its axis */
    const float to_place[9] = { 1, 0, 0,   0, 0, 1,   0, -1, 0 };      /* (x, y, z) -> (x, z, -y) */
    const float to_world[9] = { 0, 0, -1,  -1, 0, 0,   0, 1, 0 };      /* (x, y, z) -> (-z, -x, y) */
    const float tx[9] = { 1, 0, 0,  0, cosf(ax), -sinf(ax),  0, sinf(ax), cosf(ax) };
    const float ty[9] = { cosf(ay), 0, sinf(ay),  0, 1, 0,  -sinf(ay), 0, cosf(ay) };
    const float tz[9] = { cosf(az), -sinf(az), 0,  sinf(az), cosf(az), 0,  0, 0, 1 };
    float a[9], b[9], r[9];
    mul3(tx, to_place, a);
    mul3(tz, a, b);
    mul3(ty, b, a);
    mul3(to_world, a, r);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) m[col * 4 + row] = r[row * 3 + col] * p->scale;
    m[3] = m[7] = m[11] = 0;
    m[12] = (float)(p->x - ox);
    m[13] = (float)(p->y - oy);
    m[14] = p->z;
    m[15] = 1;
}
void adt_inside_matrix(const float b[16], const float pos[3], const float q[4], float scale, float m[16])
{
    float x = q[0], y = q[1], z = q[2], w = q[3];
    /* the quaternion's turn, row-major, times the size */
    float r[9] = {
        1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w),
        2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
        2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y),
    };
    float in[16];                                          /* the inside matrix, Raylib layout */
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col) in[col * 4 + row] = r[row * 3 + col] * scale;
    in[3] = in[7] = in[11] = 0;
    in[12] = pos[0]; in[13] = pos[1]; in[14] = pos[2]; in[15] = 1;
    /* m = b * in (column-vector order: `in` first, then the building) */
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            float sum = 0;
            for (int k = 0; k < 4; ++k) sum += b[k * 4 + row] * in[col * 4 + k];
            m[col * 4 + row] = sum;
        }
}
/* }}} */

void adt_free(AdtTile *t)
{
    free(t->textures); free(t->models); free(t->doodads); free(t->buildings); free(t->places); free(t->liquids);
    t->liquids = NULL; t->nliquids = 0;
    t->textures = NULL; t->models = NULL; t->doodads = NULL; t->buildings = NULL; t->places = NULL;
    t->ntextures = t->nmodels = t->ndoodads = t->nbuildings = t->nplaces = 0;
}
/* }}} */
