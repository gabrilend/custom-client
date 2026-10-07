/*
 * 023-anim.c (issue 173i) - a model's bones and animations; see 023-anim.h
 *
 * For a general audience: reads the skeleton, and works out each bone's
 * pose at any moment of any animation by finding the two recorded
 * moments around it and blending between them.
 */
#include "023-anim.h"
#include "013-errors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ readers */
static uint32_t u32(const unsigned char *s, size_t at) { return s[at] | s[at + 1] << 8 | s[at + 2] << 16 | (uint32_t)s[at + 3] << 24; }
static uint16_t u16(const unsigned char *s, size_t at) { return (uint16_t)(s[at] | s[at + 1] << 8); }
static float    f32(const unsigned char *s, size_t at) { uint32_t u = u32(s, at); float f; memcpy(&f, &u, 4); return f; }
AnimTrack anim_track_at(const unsigned char *s, size_t at)
{
    return (AnimTrack){ u16(s, at), (int16_t)u16(s, at + 2), u32(s, at + 4), u32(s, at + 8), u32(s, at + 12), u32(s, at + 16) };
}
/* }}} */

/* {{{ anim_read */
WErr anim_read(const unsigned char *m2, size_t len, Anim *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (len < 160 || memcmp(m2, "MD20", 4) != 0) { w_fail(why, W_ERR_CORRUPT, "not an M2 model"); return why->code; }
    #define FITS(at, count, each, what) \
        if ((uint64_t)(at) + (uint64_t)(count) * (each) > len) { w_fail(why, W_ERR_CORRUPT, "%s run past the file's end", what); anim_free(out); return why->code; }
    FITS(u32(m2, 24), u32(m2, 20), 4, "the global sequences")
    FITS(u32(m2, 32), u32(m2, 28), 64, "the animations")
    FITS(u32(m2, 48), u32(m2, 44), 88, "the bones")
    FITS(u32(m2, 64), u32(m2, 60), 48, "the points")
    #undef FITS
    out->m2 = malloc(len);
    memcpy(out->m2, m2, len);
    out->m2_len = len;

    out->nglobals = (int)u32(m2, 20);
    out->globals = calloc(out->nglobals ? out->nglobals : 1, sizeof *out->globals);
    for (int i = 0; i < out->nglobals; ++i) out->globals[i] = u32(m2, u32(m2, 24) + (size_t)i * 4);

    out->nseqs = (int)u32(m2, 28);
    out->seqs = calloc(out->nseqs ? out->nseqs : 1, sizeof *out->seqs);
    out->files = calloc(out->nseqs ? out->nseqs : 1, sizeof *out->files);
    out->file_lens = calloc(out->nseqs ? out->nseqs : 1, sizeof *out->file_lens);
    for (int i = 0; i < out->nseqs; ++i) {
        size_t e = u32(m2, 32) + (size_t)i * 64;
        AnimSequence *q = &out->seqs[i];
        q->id = u16(m2, e); q->variation = u16(m2, e + 2); q->length = u32(m2, e + 4);
        q->move_speed = f32(m2, e + 8); q->flags = u32(m2, e + 12); q->frequency = (int16_t)u16(m2, e + 16);
        q->blend = u32(m2, e + 28); q->next_variation = (int16_t)u16(m2, e + 60); q->alias = u16(m2, e + 62);
    }

    out->nbones = (int)u32(m2, 44);
    out->bones = calloc(out->nbones ? out->nbones : 1, sizeof *out->bones);
    for (int i = 0; i < out->nbones; ++i) {
        size_t e = u32(m2, 48) + (size_t)i * 88;
        AnimBone *b = &out->bones[i];
        b->parent = (int16_t)u16(m2, e + 8);
        b->flags = u32(m2, e + 4);
        b->move = anim_track_at(m2, e + 16);
        b->turn = anim_track_at(m2, e + 36);
        b->stretch = anim_track_at(m2, e + 56);
        for (int k = 0; k < 3; ++k) b->pivot[k] = f32(m2, e + 76 + k * 4);
        if (b->parent >= i) { w_fail(why, W_ERR_CORRUPT, "bone %d hangs from bone %d, which doesn't come before it", i, b->parent); anim_free(out); return why->code; }
    }

    out->npoints = u32(m2, 60);
    out->weights = calloc(out->npoints ? out->npoints : 1, sizeof *out->weights);
    out->bone_of = calloc(out->npoints ? out->npoints : 1, sizeof *out->bone_of);
    for (uint32_t i = 0; i < out->npoints; ++i) {
        size_t e = u32(m2, 64) + (size_t)i * 48;
        for (int k = 0; k < 4; ++k) {
            out->weights[i][k] = m2[e + 12 + k];
            out->bone_of[i][k] = m2[e + 16 + k];
            if (out->weights[i][k] && out->bone_of[i][k] >= out->nbones) {
                w_fail(why, W_ERR_CORRUPT, "point %u is tied to bone %d; the model has %d", i, out->bone_of[i][k], out->nbones);
                anim_free(out);
                return why->code;
            }
        }
    }
    return 0;
}
/* }}} */

/* {{{ anim_free, anim_find, anim_needs_file, anim_attach_file */
void anim_free(Anim *a)
{
    if (a->files) for (int i = 0; i < a->nseqs; ++i) free(a->files[i]);
    free(a->m2); free(a->seqs); free(a->bones); free(a->globals); free(a->files); free(a->file_lens);
    free(a->weights); free(a->bone_of);
    memset(a, 0, sizeof *a);
}
int anim_find(const Anim *a, int id, int variation)
{
    for (int i = 0; i < a->nseqs; ++i)
        if (a->seqs[i].id == id && a->seqs[i].variation == variation) {
            int k = i, hops = 0;
            while ((a->seqs[k].flags & 0x40) && a->seqs[k].alias < a->nseqs && hops++ < 8) k = a->seqs[k].alias;
            return k;
        }
    return -1;
}
int anim_needs_file(const Anim *a, int seq)
{
    /* an alias (0x40) has no moments of its own: anim_find follows it to
     * the animation that does */
    return seq >= 0 && seq < a->nseqs && !(a->seqs[seq].flags & 0x20) && !(a->seqs[seq].flags & 0x40) && !a->files[seq];
}
void anim_attach_file(Anim *a, int seq, unsigned char *data, size_t len)
{
    free(a->files[seq]);
    a->files[seq] = data;
    a->file_lens[seq] = len;
}
/* }}} */

/* {{{ static sample: a track's value at a moment
 * Finds the per-animation arrays (in the .m2, or in the animation's .anim
 * file), the two moments around `t`, and blends their values. Returns 0
 * when the track has nothing for this animation (the bone stays at rest
 * for that part). `size` is the value's byte size; out gets 3 floats
 * (moves, stretches) or 4 (turns). */
static const size_t KIND_SIZE[5] = { 12, 8, 4, 1, 2 };       /* ANIM_VEC3, ANIM_TURN, ANIM_FLOAT, ANIM_BYTE, ANIM_SHARE */
static void decode(const unsigned char *s, size_t at, int kind, float *v)
{
    if (kind == ANIM_VEC3) { for (int k = 0; k < 3; ++k) v[k] = f32(s, at + k * 4); return; }
    if (kind == ANIM_FLOAT) { v[0] = f32(s, at); return; }
    if (kind == ANIM_BYTE) { v[0] = (float)s[at]; return; }
    if (kind == ANIM_SHARE) { v[0] = (float)(int16_t)u16(s, at) / 32767.0f; return; }
    for (int k = 0; k < 4; ++k) {
        int16_t n = (int16_t)u16(s, at + k * 2);
        v[k] = (n < 0 ? (float)(n + 32768) : (float)(n - 32767)) / 32767.0f;
    }
}
int anim_track_value(const Anim *a, const AnimTrack *tr, int seq, uint32_t ms, uint32_t global_ms, int kind, float *out)
{
    uint32_t sub = (uint32_t)seq, t = ms;
    const unsigned char *data = a->m2;
    size_t data_len = a->m2_len;
    if (tr->global >= 0) {
        if (tr->global >= a->nglobals) return 0;
        sub = 0;
        t = a->globals[tr->global] ? global_ms % a->globals[tr->global] : 0;
    } else if (!(a->seqs[seq].flags & 0x20)) {
        if (!a->files[seq]) return 0;
        data = a->files[seq];
        data_len = a->file_lens[seq];
    }
    if (sub >= tr->nsub || sub >= tr->nvsub) return 0;
    /* the outer arrays live in the .m2; the inner ones where `data` says */
    size_t ts_head = tr->sub_at + (size_t)sub * 8, vs_head = tr->vsub_at + (size_t)sub * 8;
    if (ts_head + 8 > a->m2_len || vs_head + 8 > a->m2_len) return 0;
    uint32_t n = u32(a->m2, ts_head), ts = u32(a->m2, ts_head + 4), vs = u32(a->m2, vs_head + 4);
    size_t size = KIND_SIZE[kind], stride = tr->interp >= 2 ? size * 3 : size;
    if (n == 0 || u32(a->m2, vs_head) < n || (uint64_t)ts + n * 4 > data_len || (uint64_t)vs + n * stride > data_len) return 0;
    /* the last moment at or before t */
    uint32_t lo = 0, hi = n - 1;
    if (t <= u32(data, ts)) hi = 0;
    else if (t >= u32(data, ts + (size_t)(n - 1) * 4)) lo = hi = n - 1;
    else {
        while (hi - lo > 1) { uint32_t mid = (lo + hi) / 2; if (u32(data, ts + (size_t)mid * 4) <= t) lo = mid; else hi = mid; }
    }
    float va[4], vb[4];
    int nout = kind == ANIM_TURN ? 4 : kind == ANIM_VEC3 ? 3 : 1;
    decode(data, vs + lo * stride, kind, va);
    if (hi == lo || tr->interp == 0) { memcpy(out, va, sizeof(float) * nout); return 1; }
    decode(data, vs + hi * stride, kind, vb);
    uint32_t t0 = u32(data, ts + (size_t)lo * 4), t1 = u32(data, ts + (size_t)hi * 4);
    float f = t1 > t0 ? (float)(t - t0) / (float)(t1 - t0) : 0;
    if (kind == ANIM_TURN) {
        /* the shorter way round, then normalised (the original's own blend) */
        float dot = va[0] * vb[0] + va[1] * vb[1] + va[2] * vb[2] + va[3] * vb[3];
        float sign = dot < 0 ? -1.0f : 1.0f, len = 0;
        for (int k = 0; k < 4; ++k) { out[k] = va[k] * (1 - f) + vb[k] * sign * f; len += out[k] * out[k]; }
        len = sqrtf(len);
        if (len > 0) for (int k = 0; k < 4; ++k) out[k] /= len;
    } else {
        for (int k = 0; k < nout; ++k) out[k] = va[k] * (1 - f) + vb[k] * f;
    }
    return 1;
}
/* }}} */

/* {{{ anim_uv */
void anim_uv(const Anim *a, int uvanim, int seq, uint32_t ms, uint32_t global_ms, float out[4])
{
    out[0] = out[1] = 0; out[2] = out[3] = 1;
    if (uvanim < 0 || (uint32_t)uvanim >= u32(a->m2, 96)) return;
    size_t at = u32(a->m2, 100) + (size_t)uvanim * 60;
    if (at + 60 > a->m2_len) return;
    AnimTrack slide = anim_track_at(a->m2, at), stretch = anim_track_at(a->m2, at + 40);
    float v[3] = { 0, 0, 0 }, sc[3] = { 1, 1, 1 };
    if (seq >= 0 && seq < a->nseqs && a->seqs[seq].length) ms %= a->seqs[seq].length;
    anim_track_value(a, &slide, seq, ms, global_ms, ANIM_VEC3, v);
    anim_track_value(a, &stretch, seq, ms, global_ms, ANIM_VEC3, sc);
    out[0] = v[0]; out[1] = v[1]; out[2] = sc[0]; out[3] = sc[1];
}
/* }}} */

/* {{{ anim_moves */
static int track_moves(const Anim *a, const AnimTrack *tr, int seq)
{
    uint32_t sub = tr->global >= 0 ? 0 : (uint32_t)seq;
    if (sub >= tr->nsub) return 0;
    size_t head = tr->sub_at + (size_t)sub * 8;
    if (head + 8 > a->m2_len) return 0;
    return u32(a->m2, head) > 1;
}
int anim_moves(const Anim *a, int seq)
{
    if (seq < 0 || seq >= a->nseqs) return 0;
    for (int i = 0; i < a->nbones; ++i)
        if (track_moves(a, &a->bones[i].move, seq) || track_moves(a, &a->bones[i].turn, seq) || track_moves(a, &a->bones[i].stretch, seq))
            return 1;
    return 0;
}
/* }}} */

/* {{{ anim_pose */
static void mul4(const float a[16], const float b[16], float out[16])   /* out = a * b, column-major */
{
    float r[16];
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col) {
            float sum = 0;
            for (int k = 0; k < 4; ++k) sum += a[k * 4 + row] * b[col * 4 + k];
            r[col * 4 + row] = sum;
        }
    memcpy(out, r, sizeof r);
}
int anim_has_billboards(const Anim *a)
{
    for (int i = 0; i < a->nbones; ++i) if (a->bones[i].flags & 0x78) return 1;
    return 0;
}
void anim_pose(const Anim *a, int seq, uint32_t ms, uint32_t global_ms, float (*poses)[16])
{
    anim_pose_facing(a, seq, ms, global_ms, poses, NULL);
}
void anim_pose_facing(const Anim *a, int seq, uint32_t ms, uint32_t global_ms, float (*poses)[16], const float view[9])
{
    if (seq >= 0 && seq < a->nseqs && a->seqs[seq].length) ms %= a->seqs[seq].length;
    for (int i = 0; i < a->nbones; ++i) {
        const AnimBone *b = &a->bones[i];
        float move[3] = { 0, 0, 0 }, turn[4] = { 0, 0, 0, 1 }, stretch[3] = { 1, 1, 1 };
        if (seq >= 0 && seq < a->nseqs) {
            anim_track_value(a, &b->move, seq, ms, global_ms, ANIM_VEC3, move);
            anim_track_value(a, &b->turn, seq, ms, global_ms, ANIM_TURN, turn);
            anim_track_value(a, &b->stretch, seq, ms, global_ms, ANIM_VEC3, stretch);
        }
        float x = turn[0], y = turn[1], z = turn[2], w = turn[3];
        const float *p = b->pivot;
        /* local = T(pivot + move) * R * S * T(-pivot), written out */
        float r[9] = {
            1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w),
            2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
            2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y),
        };
        float local[16] = { 0 };
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col) local[col * 4 + row] = r[row * 3 + col] * stretch[col];
        for (int row = 0; row < 3; ++row) {
            float rs_p = local[row] * p[0] + local[4 + row] * p[1] + local[8 + row] * p[2];
            local[12 + row] = p[row] + move[row] - rs_p;
        }
        local[15] = 1;
        if (b->parent >= 0) mul4(poses[b->parent], local, poses[i]);
        else memcpy(poses[i], local, sizeof local);

        /* facing the camera: keep where the pose puts the pivot and the
         * pose's size on each axis; replace its turn by the camera's
         * (x toward the camera, y its right, z its up), or for an upright
         * billboard by a turn round z alone toward the camera */
        if (view && (b->flags & 0x78)) {
            float *m = poses[i], at[3], size[3], ax[3][3];
            for (int r = 0; r < 3; ++r) at[r] = m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2] + m[12 + r];
            for (int c = 0; c < 3; ++c) size[c] = sqrtf(m[c * 4] * m[c * 4] + m[c * 4 + 1] * m[c * 4 + 1] + m[c * 4 + 2] * m[c * 4 + 2]);
            if (b->flags & 0x8) {
                for (int r = 0; r < 3; ++r) { ax[0][r] = view[r]; ax[1][r] = view[3 + r]; ax[2][r] = view[6 + r]; }
            } else {
                float tx = view[0], ty = view[1], l = sqrtf(tx * tx + ty * ty);
                if (l < 1e-6f) { tx = 1; ty = 0; l = 1; }
                ax[0][0] = tx / l; ax[0][1] = ty / l; ax[0][2] = 0;
                ax[2][0] = 0; ax[2][1] = 0; ax[2][2] = 1;
                ax[1][0] = -ax[0][1]; ax[1][1] = ax[0][0]; ax[1][2] = 0;   /* z x x: the left-hand side seen from the camera */
            }
            for (int c = 0; c < 3; ++c) for (int r = 0; r < 3; ++r) m[c * 4 + r] = ax[c][r] * size[c];
            for (int r = 0; r < 3; ++r) m[12 + r] = at[r] - (m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2]);
        }
    }
}
/* }}} */

/* {{{ anim_skin_point */
void anim_skin_point(const Anim *a, const float (*poses)[16], uint32_t point, const float in[3], float out[3],
                     const float *n_in, float *n_out)
{
    float pos[3] = { 0, 0, 0 }, nrm[3] = { 0, 0, 0 }, total = 0;
    for (int k = 0; k < 4; ++k) {
        float w = a->weights[point][k] / 255.0f;
        if (w == 0) continue;
        const float *m = poses[a->bone_of[point][k]];
        for (int r = 0; r < 3; ++r) {
            pos[r] += w * (m[r] * in[0] + m[4 + r] * in[1] + m[8 + r] * in[2] + m[12 + r]);
            if (n_in) nrm[r] += w * (m[r] * n_in[0] + m[4 + r] * n_in[1] + m[8 + r] * n_in[2]);
        }
        total += w;
    }
    if (total == 0) {                                   /* tied to no bone: stays where made */
        memcpy(out, in, sizeof(float) * 3);
        if (n_in) memcpy(n_out, n_in, sizeof(float) * 3);
        return;
    }
    memcpy(out, pos, sizeof pos);
    if (n_in) {
        float l = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
        for (int r = 0; r < 3; ++r) n_out[r] = l > 0 ? nrm[r] / l : n_in[r];
    }
}
/* }}} */

/* {{{ anim_unit_tint */
void anim_unit_tint(const Anim *a, int colour, int fade, int seq, uint32_t ms, uint32_t global_ms, float rgba[4])
{
    const unsigned char *m2 = a->m2;
    rgba[0] = rgba[1] = rgba[2] = rgba[3] = 1;
    if (colour >= 0) {
        size_t at = (size_t)(m2[76] | m2[77] << 8 | m2[78] << 16 | (uint32_t)m2[79] << 24) + (size_t)colour * 40;
        if (at + 40 <= a->m2_len) {                /* a lookup past the table is a broken model: left untinted */
            AnimTrack c = anim_track_at(m2, at), o = anim_track_at(m2, at + 20);
            float col[3] = { 1, 1, 1 }, op = 1;
            anim_track_value(a, &c, seq, ms, global_ms, ANIM_VEC3, col);
            anim_track_value(a, &o, seq, ms, global_ms, ANIM_SHARE, &op);
            rgba[0] = col[0]; rgba[1] = col[1]; rgba[2] = col[2]; rgba[3] = op;
        }
    }
    if (fade >= 0) {
        size_t at = (size_t)(m2[92] | m2[93] << 8 | m2[94] << 16 | (uint32_t)m2[95] << 24) + (size_t)fade * 20;
        if (at + 20 <= a->m2_len) {
            AnimTrack t = anim_track_at(m2, at);
            float op = 1;
            anim_track_value(a, &t, seq, ms, global_ms, ANIM_SHARE, &op);
            rgba[3] *= op;
        }
    }
}
/* }}} */
