/*
 * 022-m2.c (issue 173k) - see 022-m2.h. Ported from src/custom-client/lib/m2.lua.
 */
#include "022-m2.h"
#include "013-errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ static readers */
static uint32_t u16(const unsigned char *s, size_t at) { return (uint32_t)s[at] | ((uint32_t)s[at + 1] << 8); }
static uint32_t u32(const unsigned char *s, size_t at)
{
    return (uint32_t)s[at] | ((uint32_t)s[at + 1] << 8) | ((uint32_t)s[at + 2] << 16) | ((uint32_t)s[at + 3] << 24);
}
static float f32(const unsigned char *s, size_t at) { float f; uint32_t v = u32(s, at); memcpy(&f, &v, 4); return f; }
/* }}} */

/* {{{ m2_read */
WErr m2_read(const unsigned char *m2, size_t m2_len, const unsigned char *skin, size_t skin_len,
            M2Model *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (!m2 || m2_len < 160 || memcmp(m2, "MD20", 4) != 0) { w_fail(why, W_ERR_CORRUPT, "not an M2 model (no MD20 mark)"); return why->code; }
    if (u32(m2, 4) != 264) { w_fail(why, W_ERR_CORRUPT, "model version %u; this reader knows 264 (3.3.5a)", u32(m2, 4)); return why->code; }
    if (!skin || skin_len < 44 || memcmp(skin, "SKIN", 4) != 0) { w_fail(why, W_ERR_CORRUPT, "the .skin file has no SKIN mark"); return why->code; }

    /* every table must lie inside its file: doodads are read by the
     * thousand, and a short or odd file must fail by name, not crash */
    #define FITS(len, at, count, each, what) \
        if ((uint64_t)(at) + (uint64_t)(count) * (each) > (len)) { \
            w_fail(why, W_ERR_CORRUPT, "%s: %u entries at byte %u run past the file's end (%zu bytes)", what, (unsigned)(count), (unsigned)(at), (size_t)(len)); \
            m2_free(out); return why->code; }
    FITS(m2_len, u32(m2, 64), u32(m2, 60), 48, "the model's points")
    FITS(m2_len, u32(m2, 84), u32(m2, 80), 16, "the model's pictures")
    FITS(m2_len, u32(m2, 116), u32(m2, 112), 4, "the model's materials")
    FITS(m2_len, u32(m2, 132), u32(m2, 128), 2, "the model's picture lookup")
    FITS(m2_len, u32(m2, 140), u32(m2, 136), 2, "the model's picture-position-set lookup")
    FITS(m2_len, u32(m2, 156), u32(m2, 152), 2, "the model's picture-animation lookup")
    FITS(m2_len, u32(m2, 100), u32(m2, 96), 60, "the model's picture animations")
    FITS(skin_len, u32(skin, 8), u32(skin, 4), 2, "the skin's point list")
    FITS(skin_len, u32(skin, 16), u32(skin, 12), 2, "the skin's triangles")
    FITS(skin_len, u32(skin, 32), u32(skin, 28), 48, "the skin's pieces")
    FITS(skin_len, u32(skin, 40), u32(skin, 36), 24, "the skin's units")
    #undef FITS

    uint32_t nv = u32(m2, 60), ov = u32(m2, 64);
    out->vertices = malloc(sizeof(M2Vertex) * (nv ? nv : 1));
    out->nvertices = nv;
    for (uint32_t i = 0; i < nv; ++i) {
        size_t at = ov + (size_t)i * 48;
        M2Vertex *v = &out->vertices[i];
        v->x = f32(m2, at); v->y = f32(m2, at + 4); v->z = f32(m2, at + 8);
        v->nx = f32(m2, at + 20); v->ny = f32(m2, at + 24); v->nz = f32(m2, at + 28);
        v->u = f32(m2, at + 32); v->v = f32(m2, at + 36);
        v->u2 = f32(m2, at + 40); v->v2 = f32(m2, at + 44);
    }

    uint32_t nt = u32(m2, 80), ot = u32(m2, 84);
    out->textures = calloc(nt ? nt : 1, sizeof(M2Texture));
    out->ntextures = nt;
    for (uint32_t i = 0; i < nt; ++i) {
        size_t at = ot + (size_t)i * 16;
        uint32_t len = u32(m2, at + 8), off = u32(m2, at + 12);
        out->textures[i].kind = u32(m2, at);
        if (len > 1 && len < sizeof out->textures[i].file && (uint64_t)off + len <= m2_len) memcpy(out->textures[i].file, m2 + off, len - 1);
    }
    uint32_t nl = u32(m2, 128), ol = u32(m2, 132);

    /* the skin: which model point each skin point is, then the triangles */
    uint32_t ni = u32(skin, 4), oi = u32(skin, 8);
    uint32_t ntri = u32(skin, 12), otri = u32(skin, 16);
    out->indices = malloc(sizeof(uint32_t) * (ntri ? ntri : 1));
    out->nindices = ntri;
    for (uint32_t i = 0; i < ntri; ++i) {
        uint32_t k = u16(skin, otri + (size_t)i * 2);
        out->indices[i] = k < ni ? u16(skin, oi + (size_t)k * 2) : 0;
    }
    uint32_t np = u32(skin, 28), op = u32(skin, 32);
    out->pieces = malloc(sizeof(M2Piece) * (np ? np : 1));
    out->npieces = np;
    for (uint32_t i = 0; i < np; ++i) {
        size_t at = op + (size_t)i * 48;
        out->pieces[i].id = u16(skin, at);
        out->pieces[i].first = u16(skin, at + 8) + u16(skin, at + 2) * 65536;   /* the "level" field carries the high part */
        out->pieces[i].count = u16(skin, at + 10);
    }
    uint32_t nu = u32(skin, 36), ou = u32(skin, 40);
    out->units = malloc(sizeof(M2Unit) * (nu ? nu : 1));
    out->nunits = nu;
    for (uint32_t i = 0; i < nu; ++i) {
        size_t at = ou + (size_t)i * 24;
        uint32_t look = u16(skin, at + 16), mat = u16(skin, at + 10);
        int16_t colour = (int16_t)u16(skin, at + 8);
        uint32_t fade_look = u16(skin, at + 20);
        out->units[i].colour = colour >= 0 && (uint32_t)colour < u32(m2, 72) ? colour : -1;
        out->units[i].fade = -1;
        if (fade_look < u32(m2, 144) && (uint64_t)u32(m2, 148) + (uint64_t)(fade_look + 1) * 2 <= m2_len) {
            uint32_t f = u16(m2, u32(m2, 148) + (size_t)fade_look * 2);
            if (f < u32(m2, 88)) out->units[i].fade = (int)f;
        }
        out->units[i].layers = (int)u16(skin, at + 14);
        uint32_t uv_look = u16(skin, at + 22);
        out->units[i].moving_uv = uv_look < u32(m2, 152) && (uint64_t)u32(m2, 156) + (uint64_t)(uv_look + 1) * 2 <= m2_len
                                  && (int16_t)u16(m2, u32(m2, 156) + (size_t)uv_look * 2) >= 0;
        out->units[i].piece = u16(skin, at + 4);
        out->units[i].texture = look < nl ? u16(m2, ol + (size_t)look * 2) : 0;
        /* each layer's picture, picture-position set and animation, by the
         * unit's combo indices into the lookup tables (pictures 128/132,
         * position sets 136/140, animations 152/156) */
        uint32_t coord_look = u16(skin, at + 18), anim_look = u16(skin, at + 22);
        out->units[i].shader = (uint16_t)u16(skin, at + 2);
        for (int l = 0; l < 2; ++l) {
            out->units[i].tex[l] = (look + l < nl) ? (int)u16(m2, ol + (size_t)(look + l) * 2) : -1;
            out->units[i].uvset[l] = (coord_look + l < u32(m2, 136)) ? (int)(int16_t)u16(m2, u32(m2, 140) + (size_t)(coord_look + l) * 2) : 0;
            out->units[i].uvanim[l] = (anim_look + l < u32(m2, 152)) ? (int)(int16_t)u16(m2, u32(m2, 156) + (size_t)(anim_look + l) * 2) : -1;
            if (out->units[i].tex[l] >= (int)nt) out->units[i].tex[l] = -1;
            if (out->units[i].uvanim[l] >= (int)u32(m2, 96)) out->units[i].uvanim[l] = -1;
        }
        if (mat < u32(m2, 112)) {
            out->units[i].flags = (uint16_t)u16(m2, u32(m2, 116) + (size_t)mat * 4);
            out->units[i].blend = (uint16_t)u16(m2, u32(m2, 116) + (size_t)mat * 4 + 2);
        }
    }
    return 0;
}
/* }}} */

/* {{{ m2_collision */
WErr m2_collision(const unsigned char *m2, size_t len, float **tris, int *ntri, WWhy *why)
{
    *tris = NULL;
    *ntri = 0;
    if (len < 232) { w_fail(why, W_ERR_CORRUPT, "the model file is too short for its collision lists"); return why->code; }
    uint32_t ni = u32(m2, 216), oi = u32(m2, 220), np = u32(m2, 224), op = u32(m2, 228);
    if (ni < 3 || np == 0) return 0;
    if ((uint64_t)oi + (uint64_t)ni * 2 > len || (uint64_t)op + (uint64_t)np * 12 > len) {
        w_fail(why, W_ERR_CORRUPT, "its collision lists (%u corners, %u points) run past the file's end", ni, np);
        return why->code;
    }
    int n = (int)(ni / 3);
    float *out = malloc(sizeof(float) * 9 * (size_t)n);
    int kept = 0;
    for (int t = 0; t < n; ++t) {
        int ok = 1;
        for (int c = 0; c < 3; ++c) {
            uint32_t k = u16(m2, oi + (size_t)(t * 3 + c) * 2);
            if (k >= np) { ok = 0; break; }
            for (int a = 0; a < 3; ++a) out[kept * 9 + c * 3 + a] = f32(m2, op + (size_t)k * 12 + (size_t)a * 4);
        }
        if (ok) ++kept;
    }
    *tris = out;
    *ntri = kept;
    return 0;
}
/* }}} */

/* {{{ m2_combine */
int m2_combine(uint16_t shader) { return shader & 0x7; }
/* }}} */

/* {{{ m2_free */
void m2_free(M2Model *m)
{
    free(m->vertices); free(m->textures); free(m->pieces); free(m->indices); free(m->units);
    memset(m, 0, sizeof *m);
}
/* }}} */
