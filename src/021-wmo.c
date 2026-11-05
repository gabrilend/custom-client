/*
 * 021-wmo.c (issue 173g) - the game's buildings; see 021-wmo.h
 *
 * For a general audience: takes a building's root file and group files
 * apart into materials, furnishings, and per-group points, triangles and
 * batches. Every block is checked to lie inside its file.
 */
#include "021-wmo.h"
#include "013-errors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ readers */
static uint32_t u32(const unsigned char *s, size_t at) { return s[at] | s[at + 1] << 8 | s[at + 2] << 16 | (uint32_t)s[at + 3] << 24; }
static uint32_t u16(const unsigned char *s, size_t at) { return (uint32_t)(s[at] | s[at + 1] << 8); }
static float    f32(const unsigned char *s, size_t at) { uint32_t u = u32(s, at); float f; memcpy(&f, &u, 4); return f; }
static int is_tag(const unsigned char *s, size_t at, const char *tag)
{
    return s[at] == tag[3] && s[at + 1] == tag[2] && s[at + 2] == tag[1] && s[at + 3] == tag[0];
}
/* a zero-ended name at `off` inside a names block, copied into `out` */
static void name_at(const unsigned char *blk, size_t blk_len, uint32_t off, char *out, size_t out_len)
{
    if (!blk || off >= blk_len) { out[0] = '\0'; return; }
    snprintf(out, out_len, "%.*s", (int)strnlen((const char *)blk + off, blk_len - off), blk + off);
}
/* }}} */

/* {{{ wmo_read_root */
WErr wmo_read_root(const unsigned char *s, size_t len, WmoRoot *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    const unsigned char *motx = NULL, *modn = NULL;
    size_t motx_len = 0, modn_len = 0;
    int got_header = 0;
    /* first pass: the name blocks (materials and furnishings point into them) */
    for (size_t at = 0; at + 8 <= len; at += 8 + u32(s, at + 4)) {
        size_t size = u32(s, at + 4);
        if (at + 8 + size > len) { w_fail(why, W_ERR_CORRUPT, "a block at byte %zu runs past the file's end", at); return why->code; }
        if (is_tag(s, at, "MOTX")) { motx = s + at + 8; motx_len = size; }
        if (is_tag(s, at, "MODN")) { modn = s + at + 8; modn_len = size; }
        if (is_tag(s, at, "MOHD")) {
            if (size < 28) { w_fail(why, W_ERR_CORRUPT, "its MOHD header is %zu bytes, too short", size); return why->code; }
            out->ngroups = (int)u32(s, at + 8 + 4);
            out->flags = size >= 64 ? u32(s, at + 8 + 60) : 0;
            got_header = 1;
        }
    }
    if (!got_header) { w_fail(why, W_ERR_CORRUPT, "no MOHD header: not a building's root file"); return why->code; }
    for (size_t at = 0; at + 8 <= len; at += 8 + u32(s, at + 4)) {
        size_t size = u32(s, at + 4), d = at + 8;
        if (is_tag(s, at, "MOMT")) {
            out->nmaterials = (int)(size / 64);
            out->materials = calloc(out->nmaterials ? out->nmaterials : 1, sizeof *out->materials);
            for (int i = 0; i < out->nmaterials; ++i) {
                size_t e = d + (size_t)i * 64;
                out->materials[i].flags = u32(s, e);
                out->materials[i].blend = u32(s, e + 8);
                name_at(motx, motx_len, u32(s, e + 12), out->materials[i].picture, sizeof out->materials[i].picture);
            }
        } else if (is_tag(s, at, "MODS")) {
            out->nsets = (int)(size / 32);
            out->sets = calloc(out->nsets ? out->nsets : 1, sizeof *out->sets);
            for (int i = 0; i < out->nsets; ++i) {
                size_t e = d + (size_t)i * 32;
                snprintf(out->sets[i].name, sizeof out->sets[i].name, "%.*s", 20, s + e);
                out->sets[i].first = (int)u32(s, e + 20);
                out->sets[i].count = (int)u32(s, e + 24);
            }
        } else if (is_tag(s, at, "MFOG")) {
            /* the building's fogs (1109 step 1v; the layout recalled, unchecked) */
            out->nfogs = (int)(size / 48);
            out->fogs = calloc(out->nfogs ? out->nfogs : 1, sizeof *out->fogs);
            for (int i = 0; i < out->nfogs; ++i) {
                size_t e = d + (size_t)i * 48;
                WmoFog *f = &out->fogs[i];
                f->flags = u32(s, e);
                for (int k = 0; k < 3; ++k) f->pos[k] = f32(s, e + 4 + k * 4);
                f->inner = f32(s, e + 16); f->outer = f32(s, e + 20);
                f->end = f32(s, e + 24); f->start_share = f32(s, e + 28);
                f->under_end = f32(s, e + 36); f->under_start_share = f32(s, e + 40);
                for (int k = 0; k < 3; ++k) {                       /* blue, green, red, alpha bytes */
                    f->colour[k] = s[e + 32 + 2 - k] / 255.0f;
                    f->under_colour[k] = s[e + 44 + 2 - k] / 255.0f;
                }
            }
        } else if (is_tag(s, at, "MODD")) {
            out->nfurnishings = (int)(size / 40);
            out->furnishings = calloc(out->nfurnishings ? out->nfurnishings : 1, sizeof *out->furnishings);
            for (int i = 0; i < out->nfurnishings; ++i) {
                size_t e = d + (size_t)i * 40;
                WmoFurnishing *f = &out->furnishings[i];
                name_at(modn, modn_len, u32(s, e) & 0xffffff, f->model, sizeof f->model);
                for (int k = 0; k < 3; ++k) f->pos[k] = f32(s, e + 4 + k * 4);
                for (int k = 0; k < 4; ++k) f->quat[k] = f32(s, e + 16 + k * 4);
                f->scale = f32(s, e + 32);
            }
        }
    }
    for (int i = 0; i < out->nsets; ++i)
        if (out->sets[i].first < 0 || out->sets[i].count < 0 || out->sets[i].first + out->sets[i].count > out->nfurnishings) {
            w_fail(why, W_ERR_CORRUPT, "furnishing set %d (%s) runs past the %d furnishings", i, out->sets[i].name, out->nfurnishings);
            wmo_free_root(out);
            return why->code;
        }
    return 0;
}
/* }}} */

/* {{{ wmo_read_group */
WErr wmo_read_group(const unsigned char *s, size_t len, WmoGroup *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    size_t mogp = 0, mogp_end = 0;
    for (size_t at = 0; at + 8 <= len; at += 8 + u32(s, at + 4)) {
        if (at + 8 + u32(s, at + 4) > len) { w_fail(why, W_ERR_CORRUPT, "a block at byte %zu runs past the file's end", at); return why->code; }
        if (is_tag(s, at, "MOGP")) { mogp = at; mogp_end = at + 8 + u32(s, at + 4); break; }
    }
    if (!mogp || mogp + 8 + 68 > mogp_end) { w_fail(why, W_ERR_CORRUPT, "no MOGP block: not a building's group file"); return why->code; }
    out->flags = u32(s, mogp + 8 + 8);
    out->liquid = u32(s, mogp + 8 + 52);
    memcpy(out->fog, s + mogp + 8 + 48, 4);                   /* the root's fogs it names (1109 step 1v; unchecked) */
    for (int k = 0; k < 6; ++k) out->box[k] = f32(s, mogp + 8 + 12 + (size_t)k * 4);
    const unsigned char *movt = NULL, *monr = NULL, *motv = NULL, *mocv = NULL;
    size_t nmovt = 0, nmonr = 0, nmotv = 0, nmocv = 0;
    /* the group's own blocks follow its 68-byte header, inside MOGP */
    for (size_t at = mogp + 8 + 68; at + 8 <= mogp_end; at += 8 + u32(s, at + 4)) {
        size_t size = u32(s, at + 4), d = at + 8;
        if (d + size > mogp_end) { w_fail(why, W_ERR_CORRUPT, "a block inside MOGP at byte %zu runs past it", at); wmo_free_group(out); return why->code; }
        if (is_tag(s, at, "MOPY")) {
            size_t n = size / 2;
            out->tri_flags = malloc(n ? n : 1);
            out->nflags = (uint32_t)n;
            for (size_t i = 0; i < n; ++i) out->tri_flags[i] = s[d + i * 2];
        } else if (is_tag(s, at, "MOVI")) {
            out->nindices = (uint32_t)(size / 2);
            out->indices = malloc(sizeof(uint16_t) * (out->nindices ? out->nindices : 1));
            for (uint32_t i = 0; i < out->nindices; ++i) out->indices[i] = (uint16_t)u16(s, d + (size_t)i * 2);
        } else if (is_tag(s, at, "MOVT")) { movt = s + d; nmovt = size / 12;
        } else if (is_tag(s, at, "MONR")) { monr = s + d; nmonr = size / 12;
        } else if (is_tag(s, at, "MOTV") && !motv) { motv = s + d; nmotv = size / 8;   /* the first set; a second can follow */
        } else if (is_tag(s, at, "MOCV") && !mocv) { mocv = s + d; nmocv = size / 4;
        } else if (is_tag(s, at, "MLIQ") && size >= 30) {
            int nx = (int)u32(s, d), ny = (int)u32(s, d + 4), cx = (int)u32(s, d + 8), cy = (int)u32(s, d + 12);
            /* the cell counts must be one fewer than the points', and the
             * whole grid inside the block; otherwise it's a broken pool,
             * reported and left out (the building still stands) */
            if (nx < 2 || ny < 2 || cx != nx - 1 || cy != ny - 1 || 30 + (size_t)nx * ny * 8 + (size_t)cx * cy > size) {
                fprintf(stderr, "buildings: a pool of %d x %d points and %d x %d cells doesn't fit its %zu bytes; left out\n", nx, ny, cx, cy, size);
            } else {
                WmoPool *p = &out->pool;
                p->nx = nx; p->ny = ny;
                for (int c = 0; c < 3; ++c) p->corner[c] = f32(s, d + 16 + (size_t)c * 4);
                p->height = malloc(sizeof(float) * (size_t)nx * ny);
                p->cell = malloc((size_t)cx * cy);
                for (int i = 0; i < nx * ny; ++i) p->height[i] = f32(s, d + 30 + (size_t)i * 8 + 4);
                memcpy(p->cell, s + d + 30 + (size_t)nx * ny * 8, (size_t)cx * cy);
            }
        } else if (is_tag(s, at, "MOBA")) {
            out->nbatches = (int)(size / 24);
            out->batches = calloc(out->nbatches ? out->nbatches : 1, sizeof *out->batches);
            for (int i = 0; i < out->nbatches; ++i) {
                size_t e = d + (size_t)i * 24;
                out->batches[i].first = u32(s, e + 12);
                out->batches[i].count = u16(s, e + 16);
                out->batches[i].material = s[e + 23];
            }
        }
    }
    if (!movt) { w_fail(why, W_ERR_CORRUPT, "the group has no points (MOVT)"); wmo_free_group(out); return why->code; }
    out->nvertices = (uint32_t)nmovt;
    out->vertices = calloc(nmovt ? nmovt : 1, sizeof *out->vertices);
    out->has_colours = mocv && nmocv >= nmovt;
    for (size_t i = 0; i < nmovt; ++i) {
        WmoVertex *v = &out->vertices[i];
        const unsigned char *p = movt + i * 12;
        v->x = f32(p, 0); v->y = f32(p, 4); v->z = f32(p, 8);
        if (i < nmonr) { v->nx = f32(monr + i * 12, 0); v->ny = f32(monr + i * 12, 4); v->nz = f32(monr + i * 12, 8); } else v->nz = 1;
        if (i < nmotv) { v->u = f32(motv + i * 8, 0); v->v = f32(motv + i * 8, 4); }
        if (out->has_colours) { v->b = mocv[i * 4]; v->g = mocv[i * 4 + 1]; v->r = mocv[i * 4 + 2]; v->a = mocv[i * 4 + 3]; }
        else { v->r = v->g = v->b = v->a = 255; }
    }
    for (int i = 0; i < out->nbatches; ++i)
        if ((uint64_t)out->batches[i].first + out->batches[i].count > out->nindices) {
            w_fail(why, W_ERR_CORRUPT, "batch %d (%u corners from %u) runs past the group's %u", i, out->batches[i].count, out->batches[i].first, out->nindices);
            wmo_free_group(out);
            return why->code;
        }
    for (uint32_t i = 0; i < out->nindices; ++i)
        if (out->indices[i] >= out->nvertices) {
            w_fail(why, W_ERR_CORRUPT, "triangle corner %u names point %u; the group has %u", i, out->indices[i], out->nvertices);
            wmo_free_group(out);
            return why->code;
        }
    return 0;
}
/* }}} */

/* {{{ wmo_group_depth */
float wmo_group_depth(const WmoGroup *g, const float p[3])
{
    float in = 1e30f, out = 0;
    for (int k = 0; k < 3; ++k) {
        float lo = p[k] - g->box[k], hi = g->box[3 + k] - p[k];
        float m = lo < hi ? lo : hi;                         /* the nearer face on this axis; negative: past it */
        if (m < in) in = m;
        if (m < 0) out += m * m;
    }
    return in >= 0 ? in : -sqrtf(out);
}
/* }}} */

/* {{{ wmo_free_root, wmo_free_group */
void wmo_free_root(WmoRoot *r) { free(r->materials); free(r->furnishings); free(r->sets); free(r->fogs); memset(r, 0, sizeof *r); }
void wmo_free_group(WmoGroup *g) { free(g->vertices); free(g->indices); free(g->batches); free(g->tri_flags); free(g->pool.height); free(g->pool.cell); memset(g, 0, sizeof *g); }
/* }}} */

/* {{{ wmo_liquid_kind */
static uint32_t older_kind(uint32_t id, uint32_t group_flags)
{
    static const uint32_t BUILDING[4] = { 13, 14, 19, 20 };     /* water, sea, magma, slime */
    if (id == 0 || id >= 21) return id;
    uint32_t k = BUILDING[(id - 1) & 3];
    return k == 13 && (group_flags & 0x80000) ? 14 : k;
}
int wmo_liquid_kind(const WmoRoot *r, const WmoGroup *g)
{
    if (!g->pool.nx) return 0;
    uint32_t k;
    if (r->flags & 0x4) k = older_kind(g->liquid, g->flags);   /* (the server maps even these through the older table) */
    else if (g->liquid == 15) k = 0;
    else k = older_kind(g->liquid + 1, g->flags);
    for (int i = 0; !k && i < (g->pool.nx - 1) * (g->pool.ny - 1); ++i)
        if (!wmo_pool_dry(g->pool.cell[i])) k = older_kind((uint32_t)(g->pool.cell[i] & 0x0F) + 1, g->flags);
    return (int)k;
}
/* }}} */
