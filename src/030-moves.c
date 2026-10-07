/*
 * 030-moves.c (issue 173i) - which animation she plays; see 030-moves.h
 *
 * For a general audience: a small table of situations, checked in order,
 * each naming an animation; then the clock and the blend.
 */
#include "030-moves.h"

#include <math.h>
#include <string.h>

/* {{{ static start: switch to animation id (variation 0), blending from the current */
static void start(MovesState *s, const Anim *a, int id, int variation, int once)
{
    int seq = anim_find(a, id, variation);
    if (seq < 0) seq = anim_find(a, MOVE_STAND, 0);   /* a model without the move stands (said once at load: see 043-main.c) */
    if (seq == s->seq && !once) return;
    s->from = s->seq;
    s->from_ms = s->ms;
    s->seq = seq;
    s->ms = 0;
    s->once = once;
    s->blend = (seq >= 0 && a->seqs[seq].blend) ? (float)a->seqs[seq].blend : 150.0f;
    s->blend_left = s->from >= 0 ? s->blend : 0;
}
/* }}} */

/* {{{ static fidget: after a stand loop, sometimes one of the stand
 * variations, by their frequencies (the stand itself is variation 0) */
static int fidget(MovesState *s, const Anim *a)
{
    int total = 0;
    for (int i = 0; i < a->nseqs; ++i) if (a->seqs[i].id == MOVE_STAND) total += a->seqs[i].frequency > 0 ? a->seqs[i].frequency : 0;
    if (total <= 0) return 0;
    s->rng = s->rng * 1103515245u + 12345u;
    int pick = (int)((s->rng >> 8) % (uint32_t)total);
    for (int i = 0; i < a->nseqs; ++i) {
        if (a->seqs[i].id != MOVE_STAND || a->seqs[i].frequency <= 0) continue;
        pick -= a->seqs[i].frequency;
        if (pick < 0) return a->seqs[i].variation;
    }
    return 0;
}
/* }}} */

/* {{{ moves_init, moves_step */
void moves_init(MovesState *s, const Anim *a)
{
    memset(s, 0, sizeof *s);
    s->seq = -1;
    s->from = -1;
    s->rng = 20261006u;
    start(s, a, MOVE_STAND, 0, 0);
}

void moves_step(MovesState *s, const Anim *a, const MovesInput *in)
{
    float dt_ms = in->dt * 1000.0f;
    int moving = in->forward || in->back || in->strafe_left || in->strafe_right;
    int side = (in->strafe_left ? 1 : 0) - (in->strafe_right ? 1 : 0);   /* +1 left, -1 right */

    /* swimming (173r): its own animations, chosen first; the body faces
     * ahead (swimming has sideways strokes of its own) */
    if (in->swimming) {
        s->airborne = 0;
        s->body_turn += fmaxf(-10.0f * in->dt, fminf(10.0f * in->dt, -s->body_turn));
        int id = in->forward ? MOVE_SWIM : in->back ? MOVE_SWIM_BACK : side > 0 ? MOVE_SWIM_LEFT : side < 0 ? MOVE_SWIM_RIGHT : MOVE_SWIM_IDLE;
        start(s, a, id, 0, 0);
        float rate = (moving && s->seq >= 0 && a->seqs[s->seq].move_speed > 0) ? in->speed / a->seqs[s->seq].move_speed : 1.0f;
        s->ms += dt_ms * rate;
        if (s->seq >= 0 && a->seqs[s->seq].length) s->ms = fmodf(s->ms, (float)a->seqs[s->seq].length);
        if (s->from >= 0) s->from_ms += dt_ms;
        if (s->blend_left > 0) s->blend_left -= dt_ms;
        if (s->blend_left <= 0) { s->blend_left = 0; s->from = -1; }
        return;
    }

    /* the body turns toward a strafe (backwards strafes turn the other way) */
    float target = 0;
    if (side) target = (float)side * (in->forward || in->back ? 0.785398f : 1.570796f) * (in->back ? -1.0f : 1.0f);
    float ease = 10.0f * in->dt;                    /* radians a second */
    s->body_turn += fmaxf(-ease, fminf(ease, target - s->body_turn));

    /* moving along the ground: backwards the walk-back (running or
     * walking: the model has one); otherwise the walk when walk is
     * toggled (173i), else the run */
    int ahead_anim = in->back ? MOVE_WALK_BACK : in->walking ? MOVE_WALK : MOVE_RUN;

    /* the situation, in order */
    if (!in->grounded) {
        if (!s->airborne) {
            s->air_ms = 0;
            /* leaving the ground going up is a jump (its start plays once,
             * then the jump loops); dropping off an edge is a fall (loops) */
            if (in->rising) start(s, a, MOVE_JUMP_START, 0, 1);
            else start(s, a, MOVE_FALL, 0, 0);
        }
        s->air_ms += dt_ms;
        if (s->once && s->seq >= 0 && s->ms >= (float)a->seqs[s->seq].length) start(s, a, MOVE_JUMP, 0, 0);
        s->airborne = 1;
    } else if (s->airborne) {
        s->airborne = 0;
        if (moving) start(s, a, ahead_anim, 0, 0);
        else start(s, a, MOVE_JUMP_END, 0, 1);
    } else if (moving) {
        start(s, a, ahead_anim, 0, 0);
    } else if (in->turn_left || in->turn_right) {
        start(s, a, in->turn_left ? MOVE_SHUFFLE_LEFT : MOVE_SHUFFLE_RIGHT, 0, 0);
    } else if (s->once && s->seq >= 0 && s->ms < (float)a->seqs[s->seq].length) {
        /* a landing or a fidget plays to its end */
    } else if (s->seq >= 0 && a->seqs[s->seq].id == MOVE_STAND && a->seqs[s->seq].variation == 0) {
        if (s->ms >= (float)a->seqs[s->seq].length) {
            int v = fidget(s, a);
            if (v) start(s, a, MOVE_STAND, v, 1);
            else s->ms = fmodf(s->ms, (float)a->seqs[s->seq].length);
        }
    } else {
        start(s, a, MOVE_STAND, 0, 0);
    }

    /* the clock: moving animations play at her real speed over the speed
     * they were made for; the rest at their own pace */
    float rate = 1.0f;
    if (s->seq >= 0 && moving && in->grounded && a->seqs[s->seq].move_speed > 0) rate = in->speed / a->seqs[s->seq].move_speed;
    s->ms += dt_ms * rate;
    if (s->from >= 0) s->from_ms += dt_ms;
    if (s->blend_left > 0) s->blend_left -= dt_ms;
    if (s->blend_left <= 0) { s->blend_left = 0; s->from = -1; }
    /* looping animations wrap (one-offs are held at their end until the
     * situation changes, above) */
    if (!s->once && s->seq >= 0 && a->seqs[s->seq].length && s->ms >= (float)a->seqs[s->seq].length &&
        !(a->seqs[s->seq].id == MOVE_STAND && a->seqs[s->seq].variation == 0))
        s->ms = fmodf(s->ms, (float)a->seqs[s->seq].length);
}
/* }}} */

/* {{{ moves_play, moves_advance */
void moves_play(MovesState *s, const Anim *a, int id) { start(s, a, id, 0, 0); }
void moves_advance(MovesState *s, const Anim *a, float dt)
{
    float dt_ms = dt * 1000.0f;
    s->ms += dt_ms;
    if (s->seq >= 0 && a->seqs[s->seq].length) s->ms = fmodf(s->ms, (float)a->seqs[s->seq].length);
    if (s->from >= 0) s->from_ms += dt_ms;
    if (s->blend_left > 0) s->blend_left -= dt_ms;
    if (s->blend_left <= 0) { s->blend_left = 0; s->from = -1; }
}
/* }}} */

/* {{{ moves_pose */
void moves_pose(const MovesState *s, const Anim *a, uint32_t global_ms, float (*poses)[16], float (*scratch)[16])
{
    uint32_t ms = (uint32_t)s->ms;
    if (s->once && s->seq >= 0 && a->seqs[s->seq].length && ms >= a->seqs[s->seq].length) ms = a->seqs[s->seq].length - 1;   /* held at its end */
    anim_pose(a, s->seq, ms, global_ms, poses);
    if (s->from < 0 || s->blend_left <= 0) return;
    anim_pose(a, s->from, (uint32_t)s->from_ms, global_ms, scratch);
    float f = s->blend_left / s->blend;                 /* 1 at the change, 0 when done */
    for (int b = 0; b < a->nbones; ++b)
        for (int k = 0; k < 16; ++k) poses[b][k] = poses[b][k] * (1 - f) + scratch[b][k] * f;
}
/* }}} */
