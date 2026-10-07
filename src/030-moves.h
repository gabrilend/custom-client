/*
 * 030-moves.h (issue 173i) - which animation she plays, from what she is doing
 *
 * For a general audience: each frame, this looks at what the character
 * is doing (running, walking backwards, turning on the spot, in the air,
 * just landed, standing) and picks the matching animation of her model,
 * with the game's own animation numbers. It keeps the animation's clock,
 * plays the run faster or slower to match her speed (so her feet
 * don't slide), plays one-off animations (a jump's start, a landing, an
 * idle fidget) once and then returns, and blends from the old animation
 * to the new one over the new one's blend time (150 ms when it gives
 * none) so changes don't snap.
 *
 * Strafing turns her body toward where she is going (45 degrees running
 * diagonally, 90 straight sideways) while she plays the run; this is this
 * client's reading of the original, to be compared with it.
 *
 * A MovesInput (this frame):
 *   forward, back, strafe_left, strafe_right, turn_left, turn_right
 *                   int, 0 or 1: what she is being asked to do
 *   grounded        int, 0 or 1: standing on ground after the move
 *   rising          int, 0 or 1: going up (a jump) rather than down
 *   swimming        int, 0 or 1: swimming (173r): swim, swim backwards,
 *                   swim left and right (no body turn), or treading water
 *   walking         int, 0 or 1: walk toggled (173i): the walk instead of
 *                   the run, forward and strafing (played at her speed too)
 *   speed           float, yards a second she is moving at by the keys
 *                   (mover_intended_speed: against a wall too, so she
 *                   runs on the spot, as the original)
 *   dt              float, seconds
 * A MovesState:
 *   seq, ms         int, float: the animation playing and how far into it
 *   from, from_ms   int, float: the animation blended from (-1 none)
 *   blend, blend_left float, ms: the blend's length and what remains
 *   once            int, 0 or 1: the animation plays once, then the
 *                   state is chosen afresh
 *   airborne        int, 0 or 1: in the air last frame
 *   air_ms          float: how long in the air
 *   body_turn       float, radians: her body's turn from her facing
 *                   (strafing), eased toward its target
 */
#ifndef EG_MOVES_H
#define EG_MOVES_H

#include "023-anim.h"

/* the game's animation numbers used here (AnimationData.dbc) */
enum {
    MOVE_STAND = 0, MOVE_WALK = 4, MOVE_RUN = 5, MOVE_SHUFFLE_LEFT = 11, MOVE_SHUFFLE_RIGHT = 12,
    MOVE_WALK_BACK = 13, MOVE_JUMP_START = 37, MOVE_JUMP = 38, MOVE_JUMP_END = 39, MOVE_FALL = 40,
    MOVE_SWIM_IDLE = 41, MOVE_SWIM = 42, MOVE_SWIM_LEFT = 43, MOVE_SWIM_RIGHT = 44, MOVE_SWIM_BACK = 45,
};

typedef struct {
    int   forward, back, strafe_left, strafe_right, turn_left, turn_right;
    int   grounded, rising;
    int   swimming;                   /* in deep water (173r): the swim animations instead */
    int   walking;                    /* walk toggled (173i): the walk instead of the run */
    float speed;
    float dt;
} MovesInput;

typedef struct {
    int   seq;    float ms;
    int   from;   float from_ms;
    float blend, blend_left;
    int   once;
    int   airborne;
    float air_ms;
    float body_turn;
    uint32_t rng;                     /* for picking fidgets */
} MovesState;

/* Starts standing. */
void moves_init(MovesState *s, const Anim *a);

/* One frame: picks and advances the animation. */
void moves_step(MovesState *s, const Anim *a, const MovesInput *in);

/* For figures with no mover (173j): play animation `id` (variation 0),
 * looping, blended in from the current one; and advance the clock at the
 * animation's own pace. Neither looks at movement. */
void moves_play(MovesState *s, const Anim *a, int id);
void moves_advance(MovesState *s, const Anim *a, float dt);

/* The bones' poses for this frame (the blend applied); `scratch` holds
 * nbones matrices for the blended-from pose. */
void moves_pose(const MovesState *s, const Anim *a, uint32_t global_ms, float (*poses)[16], float (*scratch)[16]);

#endif
