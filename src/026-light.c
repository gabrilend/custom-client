/*
 * 026-light.c (issue 173n) - the zone's light; see 026-light.h
 *
 * For a general audience: finds which lights reach a place, reads each
 * one's colours and fog for the time of day, and blends them.
 */
#include "026-light.h"
#include "013-errors.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ light_open, light_close */
static float as_float(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }

WErr light_open(LightTables *t, Tables *tables, FILE *log, WWhy *why)
{
    memset(t, 0, sizeof *t);
    t->tables = tables;
    t->record1 = -1;
    /* the four the light is made of: any in no place, and the grey day
     * stands everywhere (said once); one that is there but broken is an
     * error, named (074 checks each copy's width against its layout, 097) */
    static const char *const NEED[4] = { "Light", "LightParams", "LightIntBand", "LightFloatBand" };
    char missing[120] = "";
    for (int i = 0; i < 4; ++i) {
        TableRow r;
        WWhy inner;
        WErr rc = tables_row(tables, NEED[i], 0, &r, &inner);   /* reads the table; row 0 may or may not be there */
        if (rc == W_ERR_NOT_FOUND && tables_copies(tables, NEED[i], NULL, NULL) < 0) {
            size_t n = strlen(missing);
            snprintf(missing + n, sizeof missing - n, "%s%s.dbc", n ? ", " : "", NEED[i]);
        } else if (rc && rc != W_ERR_NOT_FOUND) return w_fail(why, rc, "%s", inner.text);
    }
    if (missing[0]) {
        t->none = 1;
        if (log) fprintf(log, "light: %s in no place: the fixed grey sky and fog everywhere\n", missing);
        return W_OK;
    }
    TableScan sc;
    WErr rc = tables_scan(tables, "Light", &sc, why);
    if (rc) return rc;
    uint32_t rows[TABLES_COPIES_MAX];
    AssetSource places[TABLES_COPIES_MAX];
    t->ncopies = tables_copies(tables, "Light", rows, places);
    t->from = places[0];
    t->lights = calloc(sc.n ? sc.n : 1, sizeof *t->lights);
    if (!t->lights) return w_fail(why, W_ERR_FULL, "no memory for %u lights", sc.n);
    for (uint32_t k = 0; k < sc.n; ++k) {
        const Dbc *d = sc.at[k].dbc;
        uint32_t r = sc.at[k].row;
        LightSphere *l = &t->lights[t->nlights++];
        l->id = (int)dbc_u32(d, r, 1);
        l->map = (int)dbc_u32(d, r, 2);
        float px = as_float(dbc_u32(d, r, 3)), py = as_float(dbc_u32(d, r, 4)), pz = as_float(dbc_u32(d, r, 5));
        l->fallback = px == 0 && py == 0 && pz == 0;
        l->x = 17066.666f - pz / 36.0f; l->y = 17066.666f - px / 36.0f; l->z = py / 36.0f;
        l->inner = as_float(dbc_u32(d, r, 6)) / 36.0f;
        l->outer = as_float(dbc_u32(d, r, 7)) / 36.0f;
        for (uint32_t s = 0; s < 5; ++s) l->set[s] = 8 + s <= d->fields ? dbc_u32(d, r, 8 + s) : 0;   /* 074 refused one narrower than 9 */
    }
    t->record1 = -1;
    for (int k = 0; k < t->nlights && t->record1 < 0; ++k) if (t->lights[k].id == 1) t->record1 = k;
    if (t->record1 < 0 && log)   /* decision 147's fallback isn't there: said once, not at each frame */
        fprintf(log, "light: Light.dbc has no row 1: a map with no default light of its own has the fixed grey sky and fog\n");
    t->skyboxes = tables_copies(tables, "LightSkybox", NULL, NULL) > 0;
    if (!t->skyboxes && log) fprintf(log, "light: LightSkybox.dbc in no place: no skybox drawn\n");
    return W_OK;
}
void light_close(LightTables *t) { free(t->lights); memset(t, 0, sizeof *t); }
/* }}} */

/* {{{ static band readers: a band's value at a time, blended between its
 * listed moments, wrapping round midnight (2880 half-minutes) */
/* a band's row (074: a placeholder where no place has it, in a test-map
 * run); 0 when it has no moments (a placeholder's, or none at all) */
static int band_row(const LightTables *t, const char *table, uint32_t id, TableRow *r)
{
    WWhy inner;
    if (tables_row_or_placeholder(t->tables, table, id, r, &inner)) return 0;
    uint32_t n = dbc_u32(r->dbc, r->row, 2);
    return n >= 1 && n <= 16;
}
static void band_at(const Dbc *d, uint32_t row, uint32_t t, int is_colour, float out[3])
{
    uint32_t n = dbc_u32(d, row, 2);
    /* the last moment at or before t, and the next (wrapping) */
    uint32_t a = n - 1;
    for (uint32_t k = 0; k < n; ++k) if (dbc_u32(d, row, 3 + k) <= t) a = k;
    uint32_t b = (a + 1) % n;
    uint32_t ta = dbc_u32(d, row, 3 + a), tb = dbc_u32(d, row, 3 + b);
    uint32_t span = (tb + 2880 - ta) % 2880, into = (t + 2880 - ta) % 2880;
    float f = span ? (float)into / (float)span : 0;
    uint32_t va = dbc_u32(d, row, 19 + a), vb = dbc_u32(d, row, 19 + b);
    if (is_colour) {
        /* each channel cut down to a whole 255th after the blend between
         * the two moments, as the original stores it (306's notes; 1109
         * decision 151): the small margin keeps a blend that lands on a
         * whole step, give or take a float's rounding, on it */
        for (int c = 0; c < 3; ++c) {
            float ca = (float)((va >> (16 - 8 * c)) & 0xff), cb = (float)((vb >> (16 - 8 * c)) & 0xff);
            out[c] = floorf(ca + (cb - ca) * f + 1e-3f) / 255.0f;
        }
    } else {
        out[0] = as_float(va) + (as_float(vb) - as_float(va)) * f;
    }
}
/* }}} */

/* {{{ static params_at: one parameter set's light at a time */
static void grey_day(LightNow *o)
{
    for (int c = 0; c < 3; ++c) {
        o->sun[c] = LIGHT_GREY_SUN; o->shade[c] = LIGHT_GREY_SHADE; o->fog[c] = LIGHT_GREY_FOG;
        for (int s = 0; s < 5; ++s) o->sky[s][c] = LIGHT_GREY_SKY;
    }
    o->fog_end = LIGHT_GREY_FOG_END; o->fog_start = LIGHT_GREY_FOG_END * LIGHT_GREY_FOG_SHARE;
    for (int i = 0; i < 4; ++i) o->water_alpha[i] = 0.75f;
}
static void params_at(const LightTables *t, uint32_t set, uint32_t when, LightNow *o)
{
    LightNow grey;
    memset(&grey, 0, sizeof grey);
    grey_day(&grey);
    float *slots[12] = { o->sun, o->shade, o->sky[0], o->sky[1], o->sky[2], o->sky[3], o->sky[4], o->fog,
                         o->water[0], o->water[1], o->water[2], o->water[3] };
    const float *greys[12] = { grey.sun, grey.shade, grey.sky[0], grey.sky[1], grey.sky[2], grey.sky[3], grey.sky[4], grey.fog,
                               grey.water[0], grey.water[1], grey.water[2], grey.water[3] };
    static const uint32_t BAND[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 14, 15, 16, 17 };
    TableRow r;
    for (uint32_t i = 0; i < 12; ++i) {
        if (band_row(t, "LightIntBand", set * 18 - 17 + BAND[i], &r)) band_at(r.dbc, r.row, when, 1, slots[i]);
        else memcpy(slots[i], greys[i], sizeof(float) * 3);           /* no moments: the grey day's (1109 step 1t) */
    }
    float v[3];
    if (band_row(t, "LightFloatBand", set * 6 - 5, &r)) { band_at(r.dbc, r.row, when, 0, v); o->fog_end = v[0] / 36.0f; }
    else o->fog_end = LIGHT_GREY_FOG_END;
    if (band_row(t, "LightFloatBand", set * 6 - 4, &r)) { band_at(r.dbc, r.row, when, 0, v); o->fog_start = o->fog_end * v[0]; }
    else o->fog_start = o->fog_end * LIGHT_GREY_FOG_SHARE;
    /* a negative share (Northrend light 1386: -0.1 at night) is kept
     * here: whether it counts as 0 is the fog mode's (fog_rules) */
    /* the water's opacity (173r); a set with no parameter row keeps the
     * 0.75 this client drew all water with before it read them (a
     * placeholder row's own: 097's layout gives 0.75 there too) */
    static const int FIELD[4] = { 8, 9, 6, 7 };              /* in water[]'s order: ocean, then river */
    WWhy inner;
    int prow = !tables_row_or_placeholder(t->tables, "LightParams", set, &r, &inner);
    for (int i = 0; i < 4; ++i) o->water_alpha[i] = prow ? as_float(dbc_u32(r.dbc, r.row, (uint32_t)FIELD[i])) : 0.75f;
}
static void mix_into(LightNow *a, const LightNow *b, float w)
{
    float *pa = (float *)a;
    const float *pb = (const float *)b;
    size_t n = offsetof(LightNow, sun_dir) / sizeof(float);   /* every colour and the fog distances */
    for (size_t i = 0; i < n; ++i) pa[i] += (pb[i] - pa[i]) * w;
}
/* one light's set at a time, with its storm set (Light's field 10, or
 * 11 under water) laid over it by the storm's strength, 0-1, when it
 * names one (306's notes; 1109 decision 154) */
static void light_params(const LightTables *t, const LightSphere *l, int kind, float storm, uint32_t when, LightNow *o)
{
    params_at(t, l->set[kind], when, o);
    uint32_t ss = l->set[kind + 2];
    if (storm > 0 && ss) {
        LightNow st;
        memset(&st, 0, sizeof st);
        params_at(t, ss, when, &st);
        mix_into(o, &st, storm > 1 ? 1 : storm);
    }
}
/* the original's fog rules, on the blended light (306's notes; 1109
 * decisions 152, 153): the start's share of the end held to -1 to 1, the
 * end no nearer than LIGHT_FOG_END_LEAST; on a map numbered
 * LIGHT_FOG_MODE2_MAP and up (the second fog mode: this client always
 * draws with shaders) a negative share counts as 0, on the others it is
 * kept, so the fog starts behind the eye */
static void fog_rules(int map, LightNow *o)
{
    float share = o->fog_end > 0 ? o->fog_start / o->fog_end : 0;
    if (share < -1) share = -1;
    if (share > 1) share = 1;
    o->fog_mode = map >= LIGHT_FOG_MODE2_MAP ? 2 : 1;
    if (o->fog_mode == 2 && share < 0) share = 0;
    if (o->fog_end < LIGHT_FOG_END_LEAST) o->fog_end = LIGHT_FOG_END_LEAST;
    o->fog_start = o->fog_end * share;
}
/* }}} */

/* {{{ light_at, light_at_weather */
int light_at(const LightTables *t, int map, float x, float y, float z, float hours, int kind, LightNow *out)
{
    return light_at_weather(t, map, x, y, z, hours, kind, 0, out);
}
int light_at_weather(const LightTables *t, int map, float x, float y, float z, float hours, int kind, float storm, LightNow *out)
{
    memset(out, 0, sizeof *out);
    if (kind != LIGHT_UNDERWATER) kind = LIGHT_CLEAR;
    uint32_t when = (uint32_t)(fmodf(fmodf(hours, 24.0f) + 24.0f, 24.0f) * 120.0f) % 2880;
    /* the default first, then every sphere reaching here, farthest first
     * so nearer lights are laid over them; with no default, Light's row 1
     * under them, and with no row 1 either, the grey day */
    int def = -1;
    typedef struct { int k; float w, d, inner; } Reach;
    Reach reach[32];
    int nreach = 0;
    for (int k = 0; !t->none && k < t->nlights; ++k) {
        const LightSphere *l = &t->lights[k];
        if (l->map != map) continue;
        if (l->fallback) { if (def < 0) def = k; continue; }   /* the first met: a map's own before the archives' */
        float d = sqrtf((l->x - x) * (l->x - x) + (l->y - y) * (l->y - y) + (l->z - z) * (l->z - z));
        if (d >= l->outer || nreach == 32) continue;
        float w = d <= l->inner ? 1.0f : (l->outer - d) / (l->outer - l->inner);
        reach[nreach].k = k; reach[nreach].w = w; reach[nreach].d = d; reach[nreach].inner = l->inner;
        ++nreach;
    }
    /* farthest first, so the nearest is laid on top; within a third of a
     * yard, the larger inner radius first (decision 146) */
    for (int i = 1; i < nreach; ++i)
        for (int k = i; k > 0; --k) {
            const Reach *a = &reach[k - 1], *b = &reach[k];
            int swap = fabsf(a->d - b->d) < 1.0f / 3.0f ? b->inner > a->inner : b->d > a->d;
            if (!swap) break;
            Reach s = reach[k]; reach[k] = reach[k - 1]; reach[k - 1] = s;
        }
    if (def < 0 && !t->none) def = t->record1;           /* no default of its own: Light's row 1 (decision 147) */
    int used = -1, used_k = -1;
    float most = 0;
    if (def >= 0) { light_params(t, &t->lights[def], kind, storm, when, out); used = t->lights[def].id; used_k = def; most = 0.0001f; }
    else grey_day(out);
    for (int i = 0; i < nreach; ++i) {
        LightNow l;
        memset(&l, 0, sizeof l);
        const LightSphere *s = &t->lights[reach[i].k];
        light_params(t, s, kind, storm, when, &l);
        mix_into(out, &l, reach[i].w);
        if (reach[i].w >= most) { most = reach[i].w; used = s->id; used_k = reach[i].k; }
    }
    /* the skybox: the strongest light's parameter set names one (field 3,
     * into LightSkybox: 2 its model), or none */
    out->skybox[0] = '\0';
    if (used_k >= 0 && t->skyboxes) {
        TableRow pr, sr;
        WWhy inner;
        if (!tables_row_or_placeholder(t->tables, "LightParams", t->lights[used_k].set[kind], &pr, &inner)) {
            uint32_t sky = dbc_u32(pr.dbc, pr.row, 3);
            if (sky && !tables_row(t->tables, "LightSkybox", sky, &sr, &inner))
                snprintf(out->skybox, sizeof out->skybox, "%s", dbc_text(sr.dbc, sr.row, 2));
        }
    }
    if (used < 0) grey_day(out);                     /* fog_mode 0: this client's own fixed fog */
    else fog_rules(map, out);

    /* the sun: rises in the east at 6, overhead-south at noon, sets in the
     * west at 18 (world x north, y west); below the horizon at night, when
     * the moon (the same path, opposite) lights from above instead */
    float a = (hours - 6.0f) / 12.0f * 3.14159265f;          /* 0 at sunrise, pi at sunset */
    float up = sinf(a);
    float dir[3] = { -0.35f, -cosf(a), up > 0.15f ? up : 0.15f + (up < 0 ? -up * 0.5f : 0) };
    float len = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    for (int c = 0; c < 3; ++c) out->sun_dir[c] = dir[c] / len;
    return used;
}
/* }}} */

/* {{{ light_indoor_fog */
void light_indoor_fog(LightNow *l, const WmoFog *f, float depth, int under)
{
    if (!f || depth <= 0) return;
    float w = depth >= LIGHT_INDOOR_FOG_YARDS ? 1 : depth / LIGHT_INDOOR_FOG_YARDS;
    float end = under ? f->under_end : f->end, share = under ? f->under_start_share : f->start_share;
    const float *c = under ? f->under_colour : f->colour;
    for (int k = 0; k < 3; ++k) l->fog[k] += (c[k] - l->fog[k]) * w;
    l->fog_start += (end * share - l->fog_start) * w;
    l->fog_end += (end - l->fog_end) * w;
}
/* }}} */
