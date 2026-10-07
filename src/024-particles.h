/*
 * 024-particles.h (issue 173o) - a model's particle emitters, read and run
 *
 * For a general audience: a torch's flame, a wisp's glow, a firefly's
 * spark are particles: many small pictures born at a point on the model,
 * flying off with a speed and a spread, pulled by gravity, slowed by
 * drag, changing colour, opacity and size as they age, then gone. The
 * model file lists its emitters, each riding on a bone. This reads them
 * and runs them; drawing (camera-facing squares) is the caller's.
 *
 * Emitter layout (3.3.5a, 476 bytes, at the header's 296: count, 300:
 * offset):
 *     0 id     4 flags (0x10000: each particle takes a random picture cell)
 *     8 position (3 floats, model space)   20 bone (u16)   22 picture slot (u16)
 *    40 blend (u8: 0 solid, 1 cut-out, 2 see-through, 3-4 added light)
 *    41 shape (u8: 1 plane, 2 sphere, 3 spline: treated as a plane)
 *    48 picture grid rows (u16)   50 columns (u16)
 *    52 speed   72 speed variation   92 vertical spread   112 horizontal
 *       spread   132 gravity   152 lifespan (all 20-byte float tracks)
 *   172 lifespan variation (float)   176 rate (track)   196 rate variation
 *   200 area length   220 area width   240 z source (tracks)
 *   260 colour over life   276 opacity   292 size (each a 16-byte "life
 *       block": moments as a share of the life, 0 to 32767, then values:
 *       colours 3 floats 0-255, opacity 16-bit 0-32767, sizes 2 floats)
 *   308 size variation (2 floats)   316 picture cell over life (block)
 *   372 drag   456 enabled (a byte track: 0 stops new particles)
 * Tracks follow the model's animations, as bones' do (023-anim.h).
 *
 * An Emitter, field by field: bone (int), pos[3] (float, model space),
 * slot (int, the model's picture slot), blend, shape, rows, cols (int),
 * flags (uint32_t), the tracks (AnimTrack), life_vary and rate_vary
 * (float, shares), drag (float), size_vary[2] (float), and four life
 * blocks (PartBlock: n keys of time 0-1 and up to 3 values).
 */
#ifndef EG_PARTICLES_H
#define EG_PARTICLES_H

#include "013-errors.h"

#include "023-anim.h"

#define PART_KEYS 8
typedef struct { int n; float t[PART_KEYS]; float v[PART_KEYS][3]; } PartBlock;

typedef struct {
    int       bone;
    float     pos[3];
    int       slot, blend, shape, rows, cols;
    uint32_t  flags;
    AnimTrack speed, speed_vary, vspread, hspread, gravity, life, rate, alen, awid, enabled;
    float     life_vary, rate_vary, drag, size_vary[2];
    PartBlock colour, opacity, size, cell;
} Emitter;

typedef struct { float p[3], v[3]; float age, life, vary; int cell; } Particle;

/* One emitter running on one placed copy. */
typedef struct {
    Particle *ps;  int n, cap;
    float     carry;                  /* part of a particle owed from last frame */
    uint32_t  rng;                    /* its random numbers' state: the caller seeds it (by which emitter on which copy), so a run repeats exactly; 0: a fixed seed */
} EmitterRun;

/* The model's emitters (from the model file kept in `a`). W_OK, or a code with `why`. */
WErr  particles_read(const Anim *a, Emitter **out, int *n, WWhy *why);

/* One frame: ages and moves the living, then gives birth at the rate.
 * `world` is where the emitter's bone is in the world this frame (the
 * copy's place times the bone's pose, Raylib's layout, as 020-adt.h's), `seq`
 * and `ms` the model's animation and its moment (for the tracks).
 * `density` (0.1 to 1: the Particle Density setting, 173b6) scales the
 * birth rate; those already living are untouched. */
void particles_step(const Anim *a, const Emitter *e, EmitterRun *run, const float world[16], int seq, uint32_t ms, float dt, float density);

/* A particle's colour (red, green, blue, opacity; 0-1) and size (yards). */
void particles_look(const Emitter *e, const Particle *p, float rgba[4], float size[2]);

/* The fog over a particle's colour (`rgba`, changed in place): `share` is
 * how far into the fog it is (0 at the fog's start, 1 at its end; held to
 * 0-1). A see-through particle (blend below 3) goes to the fog's colour
 * `fog`; an added-light one goes dark, so it adds nothing (adding the
 * fog's colour would make a spark light up the fog). Opacity stays. */
void particles_fog(int blend, float rgba[4], const float fog[3], float share);

void particles_free_run(EmitterRun *run);

#endif
