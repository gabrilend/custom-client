/*
 * 026-light.c (issue 173n) - the zone's light; see 026-light.h
 *
 * For a general audience: finds which lights reach a place, reads each
 * one's colours and fog for the time of day, and blends them.
 */
#include "026-light.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ light_open, light_close */
int light_open(LightTables *t, MpqChain *chain, char *why, int why_len)
{
    memset(t, 0, sizeof *t);
    static const char *const NAMES[5] = { "DBFilesClient\\Light.dbc", "DBFilesClient\\LightIntBand.dbc", "DBFilesClient\\LightFloatBand.dbc",
                                          "DBFilesClient\\LightParams.dbc", "DBFilesClient\\LightSkybox.dbc" };
    Dbc *dbcs[5] = { &t->light, &t->ints, &t->floats, &t->params, &t->skyboxes };
    for (int i = 0; i < 5; ++i) {
        size_t len;
        const char *from;
        t->files[i] = mpq_chain_read(chain, NAMES[i], &len, &from);
        char inner[200];
        if (!t->files[i]) { snprintf(why, why_len, "%s is in none of the client's archives", NAMES[i]); light_close(t); return 1; }
        if (dbc_open(dbcs[i], t->files[i], len, inner, sizeof inner)) { snprintf(why, why_len, "%s: %s", NAMES[i], inner); light_close(t); return 1; }
    }
    if (t->light.fields < 8 || t->ints.fields < 34 || t->floats.fields < 34) {
        snprintf(why, why_len, "the light tables have %u, %u and %u fields; 3.3.5a's have 15, 34 and 34", t->light.fields, t->ints.fields, t->floats.fields);
        light_close(t);
        return 1;
    }
    return 0;
}
void light_close(LightTables *t) { for (int i = 0; i < 5; ++i) free(t->files[i]); memset(t, 0, sizeof *t); }
/* }}} */

/* {{{ static band readers: a band's value at a time, blended between its
 * listed moments, wrapping round midnight (2880 half-minutes) */
static float as_float(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static int find_row(const Dbc *d, uint32_t id)
{
    /* ids run in order in these tables; a walk is plenty for a few lookups */
    for (uint32_t r = 0; r < d->rows; ++r) if (dbc_u32(d, r, 1) == id) return (int)r;
    return -1;
}
static void band_at(const Dbc *d, int row, uint32_t t, int is_colour, float out[3])
{
    uint32_t n = dbc_u32(d, (uint32_t)row, 2);
    if (n == 0 || n > 16) { out[0] = out[1] = out[2] = 0; return; }
    /* the last moment at or before t, and the next (wrapping) */
    uint32_t a = n - 1;
    for (uint32_t k = 0; k < n; ++k) if (dbc_u32(d, (uint32_t)row, 3 + k) <= t) a = k;
    uint32_t b = (a + 1) % n;
    uint32_t ta = dbc_u32(d, (uint32_t)row, 3 + a), tb = dbc_u32(d, (uint32_t)row, 3 + b);
    uint32_t span = (tb + 2880 - ta) % 2880, into = (t + 2880 - ta) % 2880;
    float f = span ? (float)into / (float)span : 0;
    uint32_t va = dbc_u32(d, (uint32_t)row, 19 + a), vb = dbc_u32(d, (uint32_t)row, 19 + b);
    if (is_colour) {
        for (int c = 0; c < 3; ++c) {
            float ca = (float)((va >> (16 - 8 * c)) & 0xff) / 255.0f, cb = (float)((vb >> (16 - 8 * c)) & 0xff) / 255.0f;
            out[c] = ca + (cb - ca) * f;
        }
    } else {
        out[0] = as_float(va) + (as_float(vb) - as_float(va)) * f;
    }
}
/* }}} */

/* {{{ static params_at: one parameter set's light at a time */
static void params_at(const LightTables *t, uint32_t set, uint32_t when, LightNow *o)
{
    float *slots[12] = { o->sun, o->shade, o->sky[0], o->sky[1], o->sky[2], o->sky[3], o->sky[4], o->fog,
                         o->water[0], o->water[1], o->water[2], o->water[3] };
    static const uint32_t BAND[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 14, 15, 16, 17 };
    for (uint32_t i = 0; i < 12; ++i) {
        uint32_t band = BAND[i];
        float *slot = slots[i];
        int row = find_row(&t->ints, set * 18 - 17 + band);
        if (row < 0) { slot[0] = slot[1] = slot[2] = 0; continue; }
        band_at(&t->ints, row, when, 1, slot);
    }
    float v[3];
    int row = find_row(&t->floats, set * 6 - 5);
    band_at(&t->floats, row < 0 ? 0 : row, when, 0, v);
    o->fog_end = row < 0 ? 500.0f : v[0] / 36.0f;
    row = find_row(&t->floats, set * 6 - 4);
    band_at(&t->floats, row < 0 ? 0 : row, when, 0, v);
    o->fog_start = o->fog_end * (row < 0 ? 0.25f : v[0]);
    /* some sets give a negative share (Northrend light 1386: -0.1 at
     * night); fog doesn't start behind the camera, so it starts at 0 */
    if (o->fog_start < 0) o->fog_start = 0;
    /* the water's opacity (173r); a set with no parameter row keeps the
     * 0.75 this client drew all water with before it read them */
    int prow = find_row(&t->params, set);
    static const int FIELD[4] = { 8, 9, 6, 7 };              /* in water[]'s order: ocean, then river */
    for (int i = 0; i < 4; ++i) o->water_alpha[i] = prow < 0 ? 0.75f : as_float(dbc_u32(&t->params, (uint32_t)prow, FIELD[i]));
}
static void mix_into(LightNow *a, const LightNow *b, float w)
{
    float *pa = (float *)a;
    const float *pb = (const float *)b;
    size_t n = offsetof(LightNow, sun_dir) / sizeof(float);   /* every colour and the fog distances */
    for (size_t i = 0; i < n; ++i) pa[i] += (pb[i] - pa[i]) * w;
}
/* }}} */

/* {{{ light_at */
int light_at(const LightTables *t, int map, float x, float y, float z, float hours, int kind, LightNow *out)
{
    memset(out, 0, sizeof *out);
    const int SET = 8 + kind;                            /* Light's field naming this kind's parameter set (8 clear, 9 underwater) */
    uint32_t when = (uint32_t)(fmodf(fmodf(hours, 24.0f) + 24.0f, 24.0f) * 120.0f) % 2880;
    /* the default first, then every sphere reaching here, the larger
     * (outer) first so smaller, nearer lights are laid over them */
    int def = -1;
    typedef struct { int row; float w, outer; } Reach;
    Reach reach[32];
    int nreach = 0;
    for (uint32_t r = 0; r < t->light.rows; ++r) {
        if ((int)dbc_u32(&t->light, r, 2) != map) continue;
        float px = as_float(dbc_u32(&t->light, r, 3)), py = as_float(dbc_u32(&t->light, r, 4)), pz = as_float(dbc_u32(&t->light, r, 5));
        if (px == 0 && py == 0 && pz == 0) { def = (int)r; continue; }
        float wx = 17066.666f - pz / 36.0f, wy = 17066.666f - px / 36.0f, wz = py / 36.0f;
        float inner = as_float(dbc_u32(&t->light, r, 6)) / 36.0f, outer = as_float(dbc_u32(&t->light, r, 7)) / 36.0f;
        float d = sqrtf((wx - x) * (wx - x) + (wy - y) * (wy - y) + (wz - z) * (wz - z));
        if (d >= outer || nreach == 32) continue;
        float w = d <= inner ? 1.0f : (outer - d) / (outer - inner);
        reach[nreach].row = (int)r; reach[nreach].w = w; reach[nreach].outer = outer;
        ++nreach;
    }
    for (int i = 1; i < nreach; ++i)                      /* larger first */
        for (int k = i; k > 0 && reach[k].outer > reach[k - 1].outer; --k) { Reach s = reach[k]; reach[k] = reach[k - 1]; reach[k - 1] = s; }
    int used = -1;
    float most = 0;
    if (def >= 0) { params_at(t, dbc_u32(&t->light, (uint32_t)def, SET), when, out); used = (int)dbc_u32(&t->light, (uint32_t)def, 1); most = 0.0001f; }
    for (int i = 0; i < nreach; ++i) {
        LightNow l;
        memset(&l, 0, sizeof l);
        params_at(t, dbc_u32(&t->light, (uint32_t)reach[i].row, SET), when, &l);
        if (def < 0 && i == 0) *out = l;
        else mix_into(out, &l, reach[i].w);
        if (reach[i].w >= most) { most = reach[i].w; used = (int)dbc_u32(&t->light, (uint32_t)reach[i].row, 1); }
    }
    /* the skybox: the strongest light's parameter set names one (field 3,
     * into LightSkybox: 2 its model), or none */
    out->skybox[0] = '\0';
    for (uint32_t r = 0; used >= 0 && r < t->light.rows; ++r) {
        if ((int)dbc_u32(&t->light, r, 1) != used) continue;
        int prow = find_row(&t->params, dbc_u32(&t->light, r, SET));
        uint32_t sky = prow >= 0 ? dbc_u32(&t->params, (uint32_t)prow, 3) : 0;
        int srow = sky ? find_row(&t->skyboxes, sky) : -1;
        if (srow >= 0) snprintf(out->skybox, sizeof out->skybox, "%s", dbc_text(&t->skyboxes, (uint32_t)srow, 2));
        break;
    }
    if (used < 0) {
        for (int c = 0; c < 3; ++c) { out->sun[c] = 0.8f; out->shade[c] = 0.4f; out->fog[c] = 0.6f; for (int s = 0; s < 5; ++s) out->sky[s][c] = 0.6f; }
        out->fog_end = 500; out->fog_start = 125;
        for (int i = 0; i < 4; ++i) out->water_alpha[i] = 0.75f;
    }

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
