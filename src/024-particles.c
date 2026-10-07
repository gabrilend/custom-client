/*
 * 024-particles.c (issue 173o) - particle emitters; see 024-particles.h
 *
 * For a general audience: reads each emitter's settings out of the model
 * file, and each frame moves its particles and makes new ones.
 */
#include "024-particles.h"
#include "013-errors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ readers */
static uint32_t u32(const unsigned char *s, size_t at) { return s[at] | s[at + 1] << 8 | s[at + 2] << 16 | (uint32_t)s[at + 3] << 24; }
static uint16_t u16(const unsigned char *s, size_t at) { return (uint16_t)(s[at] | s[at + 1] << 8); }
static float    f32(const unsigned char *s, size_t at) { uint32_t u = u32(s, at); float f; memcpy(&f, &u, 4); return f; }

/* a life block: moments (16-bit shares of the life) and values of `per`
 * numbers each, `each` bytes apart; kind 0 floats, 1 16-bit shares */
static void block(const unsigned char *s, size_t len, size_t at, int per, size_t each, int kind, PartBlock *b)
{
    memset(b, 0, sizeof *b);
    uint32_t nt = u32(s, at), ot = u32(s, at + 4), nv = u32(s, at + 8), ov = u32(s, at + 12);
    int n = (int)(nt < nv ? nt : nv);
    if (n > PART_KEYS) n = PART_KEYS;
    if ((uint64_t)ot + (uint64_t)n * 2 > len || (uint64_t)ov + (uint64_t)n * each > len) return;
    for (int k = 0; k < n; ++k) {
        b->t[k] = (float)u16(s, ot + (size_t)k * 2) / 32767.0f;
        for (int c = 0; c < per; ++c)
            b->v[k][c] = kind == 0 ? f32(s, ov + (size_t)k * each + (size_t)c * 4) : (float)(int16_t)u16(s, ov + (size_t)k * each + (size_t)c * 2) / 32767.0f;
    }
    b->n = n;
}
static void block_at(const PartBlock *b, float t, int per, float *out, const float *none)
{
    if (b->n == 0) { memcpy(out, none, sizeof(float) * per); return; }
    if (t <= b->t[0]) { memcpy(out, b->v[0], sizeof(float) * per); return; }
    for (int k = 0; k + 1 < b->n; ++k)
        if (t < b->t[k + 1]) {
            float f = b->t[k + 1] > b->t[k] ? (t - b->t[k]) / (b->t[k + 1] - b->t[k]) : 0;
            for (int c = 0; c < per; ++c) out[c] = b->v[k][c] + (b->v[k + 1][c] - b->v[k][c]) * f;
            return;
        }
    memcpy(out, b->v[b->n - 1], sizeof(float) * per);
}
/* }}} */

/* {{{ particles_read */
WErr particles_read(const Anim *a, Emitter **out, int *n, WWhy *why)
{
    *out = NULL;
    *n = 0;
    const unsigned char *s = a->m2;
    size_t len = a->m2_len;
    if (len < 304) { w_fail(why, W_ERR_CORRUPT, "the model file is too short for its emitter list"); return why->code; }
    uint32_t count = u32(s, 296), at = u32(s, 300);
    if (count == 0) return 0;
    if ((uint64_t)at + (uint64_t)count * 476 > len) { w_fail(why, W_ERR_CORRUPT, "%u emitters at byte %u run past the file's end", count, at); return why->code; }
    Emitter *list = calloc(count, sizeof *list);
    for (uint32_t i = 0; i < count; ++i) {
        size_t e = at + (size_t)i * 476;
        Emitter *m = &list[i];
        m->flags = u32(s, e + 4);
        for (int k = 0; k < 3; ++k) m->pos[k] = f32(s, e + 8 + k * 4);
        m->bone = u16(s, e + 20);
        m->slot = u16(s, e + 22);
        m->blend = s[e + 40];
        m->shape = s[e + 41];
        m->rows = u16(s, e + 48) ? u16(s, e + 48) : 1;
        m->cols = u16(s, e + 50) ? u16(s, e + 50) : 1;
        m->speed = anim_track_at(s, e + 52);
        m->speed_vary = anim_track_at(s, e + 72);
        m->vspread = anim_track_at(s, e + 92);
        m->hspread = anim_track_at(s, e + 112);
        m->gravity = anim_track_at(s, e + 132);
        m->life = anim_track_at(s, e + 152);
        m->life_vary = f32(s, e + 172);
        m->rate = anim_track_at(s, e + 176);
        m->rate_vary = f32(s, e + 196);
        m->alen = anim_track_at(s, e + 200);
        m->awid = anim_track_at(s, e + 220);
        block(s, len, e + 260, 3, 12, 0, &m->colour);
        block(s, len, e + 276, 1, 2, 1, &m->opacity);
        block(s, len, e + 292, 2, 8, 0, &m->size);
        m->size_vary[0] = f32(s, e + 308);
        m->size_vary[1] = f32(s, e + 312);
        block(s, len, e + 316, 1, 2, 1, &m->cell);
        for (int k = 0; k < m->cell.n; ++k) m->cell.v[k][0] *= 32767.0f;   /* cells are whole numbers, not shares */
        m->drag = f32(s, e + 372);
        m->enabled = anim_track_at(s, e + 456);
        if (m->bone >= a->nbones) {
            w_fail(why, W_ERR_CORRUPT, "emitter %u rides on bone %d; the model has %d", i, m->bone, a->nbones);
            free(list);
            return why->code;
        }
    }
    *out = list;
    *n = (int)count;
    return 0;
}
/* }}} */

/* {{{ particles_step */
static float rnd(uint32_t *r) { *r = *r * 1664525u + 1013904223u; return (float)(*r >> 8) / 16777216.0f; }   /* 0 to 1 */
static float track(const Anim *a, const AnimTrack *tr, int seq, uint32_t ms, float none)
{
    float v = none;
    anim_track_value(a, tr, seq, ms, ms, ANIM_FLOAT, &v);
    return v;
}
void particles_step(const Anim *a, const Emitter *e, EmitterRun *run, const float w[16], int seq, uint32_t ms, float dt, float density)
{
    if (!run->rng) run->rng = 0x9e3779b9u ^ (uint32_t)(uintptr_t)run;
    float gravity = track(a, &e->gravity, seq, ms, 0), drag = e->drag;
    /* the living: age, fall, slow, move; the dead are dropped */
    int keep = 0;
    for (int i = 0; i < run->n; ++i) {
        Particle *p = &run->ps[i];
        p->age += dt;
        if (p->age >= p->life) continue;
        p->v[2] -= gravity * dt;
        float slow = 1.0f - drag * dt;
        if (slow < 0) slow = 0;
        for (int k = 0; k < 3; ++k) { p->v[k] *= slow; p->p[k] += p->v[k] * dt; }
        run->ps[keep++] = *p;
    }
    run->n = keep;

    /* births, at the rate; none while the emitter is switched off */
    float on = 1;
    anim_track_value(a, &e->enabled, seq, ms, ms, ANIM_BYTE, &on);
    if (on == 0) { run->carry = 0; return; }
    float rate = track(a, &e->rate, seq, ms, 0) * (1 + (rnd(&run->rng) * 2 - 1) * e->rate_vary) * density;
    if (rate < 0) rate = 0;
    run->carry += rate * dt;
    int born = (int)run->carry;
    run->carry -= (float)born;
    if (born > 200) born = 200;                       /* a stalled frame doesn't flood */
    float speed = track(a, &e->speed, seq, ms, 0), vary = track(a, &e->speed_vary, seq, ms, 0);
    float vspread = track(a, &e->vspread, seq, ms, 0), hspread = track(a, &e->hspread, seq, ms, 0);
    float life = track(a, &e->life, seq, ms, 1), alen = track(a, &e->alen, seq, ms, 0), awid = track(a, &e->awid, seq, ms, 0);
    for (int b = 0; b < born; ++b) {
        if (run->n == run->cap) { run->cap = run->cap ? run->cap * 2 : 32; run->ps = realloc(run->ps, sizeof *run->ps * run->cap); }
        Particle *p = &run->ps[run->n++];
        float local[3], dir[3];
        /* where it starts and which way it goes, in the emitter's own space
         * (its up is z): a plane scatters over length x width and shoots
         * up; a sphere starts on a shell and shoots outward; each tilted by
         * up to the vertical spread, in a direction within the horizontal */
        float tilt = rnd(&run->rng) * vspread, turn = (rnd(&run->rng) * 2 - 1) * (hspread > 0 ? hspread : 3.14159265f);
        if (e->shape == 2) {
            float r = alen + rnd(&run->rng) * (awid - alen);
            float el = (rnd(&run->rng) * 2 - 1) * 1.5707963f, az = rnd(&run->rng) * 6.2831853f;
            dir[0] = cosf(el) * cosf(az); dir[1] = cosf(el) * sinf(az); dir[2] = sinf(el);
            for (int k = 0; k < 3; ++k) local[k] = e->pos[k] + dir[k] * r;
        } else {
            local[0] = e->pos[0] + (rnd(&run->rng) - 0.5f) * alen;
            local[1] = e->pos[1] + (rnd(&run->rng) - 0.5f) * awid;
            local[2] = e->pos[2];
            dir[0] = sinf(tilt) * cosf(turn); dir[1] = sinf(tilt) * sinf(turn); dir[2] = cosf(tilt);
        }
        float sp = speed * (1 + (rnd(&run->rng) * 2 - 1) * vary);
        for (int k = 0; k < 3; ++k) {
            p->p[k] = w[k] * local[0] + w[4 + k] * local[1] + w[8 + k] * local[2] + w[12 + k];
            p->v[k] = (w[k] * dir[0] + w[4 + k] * dir[1] + w[8 + k] * dir[2]) * sp;
        }
        p->age = 0;
        p->life = life * (1 + (rnd(&run->rng) * 2 - 1) * e->life_vary);
        if (p->life <= 0.01f) p->life = 0.01f;
        p->vary = rnd(&run->rng) * 2 - 1;
        p->cell = (e->flags & 0x10000) ? (int)(rnd(&run->rng) * (float)(e->rows * e->cols)) : 0;
    }
}
/* }}} */

/* {{{ particles_look, particles_free_run */
void particles_look(const Emitter *e, const Particle *p, float rgba[4], float size[2])
{
    static const float WHITE[3] = { 255, 255, 255 }, OPAQUE[1] = { 1 }, ONE[2] = { 1, 1 };
    float t = p->age / p->life, c[3], a[1], sz[2];
    block_at(&e->colour, t, 3, c, WHITE);
    block_at(&e->opacity, t, 1, a, OPAQUE);
    block_at(&e->size, t, 2, sz, ONE);
    for (int k = 0; k < 3; ++k) rgba[k] = c[k] / 255.0f;
    rgba[3] = a[0];
    size[0] = sz[0] * (1 + p->vary * e->size_vary[0]);
    size[1] = sz[1] * (1 + p->vary * e->size_vary[1]);
}
void particles_free_run(EmitterRun *run) { free(run->ps); memset(run, 0, sizeof *run); }
/* }}} */

/* {{{ particles_fog */
void particles_fog(int blend, float rgba[4], const float fog[3], float share)
{
    float f = share < 0 ? 0 : share > 1 ? 1 : share;
    for (int ch = 0; ch < 3; ++ch)
        rgba[ch] = blend >= 3 ? rgba[ch] * (1 - f) : rgba[ch] + (fog[ch] - rgba[ch]) * f;
}
/* }}} */
