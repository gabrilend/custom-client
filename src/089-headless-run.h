/*
 * 089-headless-run.h (issue 1109, step 1n) - a test map run with no window
 *
 * For a general audience: the map editor's Test Map opens this client
 * on a converted map (067-test-map.h). To check that run without a
 * screen (on a build machine, in the tests, or twice in a row to see
 * that it comes out the same), the client can do everything that needs
 * no window: open the world (087-world-start.h), look up her figure and
 * every creature's (025, 088), and walk her through a fixed list of
 * moves (the mover, 029, over the map's ground, the placed models'
 * floors and walls, 091, and the water), then write
 * its goodbye receipt (068-goodbye.h) with what it found and where she
 * ended, and stop. Nothing is drawn and no server is asked.
 *
 * The moves are a Lua data file (read by 059's data-only reader, as a
 * converted map's map.lua is), format 1:
 *
 *   return {
 *       format = 1,
 *       name   = "round the start",       -- optional, said on the terminal
 *       map    = "Kalimdor",               -- optional: the map's folder these are for
 *       step   = 0.05,                     -- optional: seconds a step (0.01 to 0.1)
 *       moves  = {
 *           { hold = "forward", seconds = 3 },
 *           { hold = "forward left", seconds = 1, walking = true },
 *           { hold = "", seconds = 0.5 },  -- nothing held: she stands (or falls)
 *           { turn_to = 1.5708 },          -- she faces west (radians, the game's:
 *                                          -- 0 north, rising west), at once
 *           { turn_to = 0, hold = "forward", seconds = 2 },   -- faces north, then runs
 *       },
 *   }
 *
 * Each move holds its keys for its seconds: forward, back, left, right
 * (turning), strafe_left, strafe_right, jump, swim_down, any of them
 * with spaces between; `walking` is the walk toggle (173i); `turn_to`
 * (step 1p) turns her to a facing at once before the move's keys, as a
 * mouse turn does, so a route doesn't hang on how far a held key turns
 * her (a move with turn_to alone takes no steps). A step is
 * the same length every time, so a run is the same every time. A field
 * or key the format doesn't have is refused by name and line, so a
 * misspelt move doesn't pass silently. tests/090-test-map-moves.lua is
 * the project's own list for any map; tests/092 and 093 are routes on
 * two converted maps (each names its map), found in the mover's own steps
 * (1109 step 1o): to their water and off their ground's edge.
 *
 * What the receipt counts (goodbye_count; frames stays 0, as nothing is
 * drawn): the moves and steps run; her figure (her_stand_in 1 when the
 * baseline set's stand-in stood in for her character, her_vertices,
 * her_bones, her_sequences, her_animation_changes, her_animation at the
 * end); the creatures (creature_spawns, creature_looks, and of the looks
 * looks_wc3, looks_game, looks_stand_in, looks_character, looks_missing
 * and looks_unreadable); where she went (start_x/y/z, end_x/y/z,
 * end_facing, distance along the ground, lowest_z, highest_z, tiles
 * entered, ground_files read for their water and models,
 * steps_airborne, steps_swimming, steps_sliding, steps_no_ground); the
 * placed models (091-model-solids.h, step 1o: models and buildings
 * read, models_solid of them with a solid shape, models_wc3 and
 * model_stand_ins walked through, models_left_out, model_copies placed,
 * solid_copies, solid_triangles, building_pools; steps_on_models
 * standing on a model's floor, steps_walled meeting a model's wall); a
 * converted map's WC3 pathing (094-wc3-pathing.h, step 1p: pathing 1
 * when its war3map.wpm was read, pathing_cells, pathing_closed of them
 * closed to walkers, steps_walled_pathing meeting one, steps_on_closed
 * ending in a closed cell: none, unless she started in one).
 *
 * Window-free (CORE: the test program and the executable).
 */
#ifndef EG_HEADLESS_RUN_H
#define EG_HEADLESS_RUN_H

#include "013-errors.h"
#include "068-goodbye.h"
#include "087-world-start.h"

#include <stddef.h>

#define HEADLESS_MOVES_FORMAT 1
#define HEADLESS_STEP_DEFAULT 0.05            /* seconds: the --shot pictures' 20 steps a second */
#define HEADLESS_STEPS_MAX    72000           /* an hour of steps at the default */
#define HEADLESS_MOVES_MAX    1000
#define HEADLESS_MOVES_BYTES  (256 * 1024)    /* the most a moves file may be */
#define HEADLESS_TURN_MOST    6.2832          /* radians: a turn_to from -2 pi to 2 pi (rounded up) */

/* The moves (the handle: docs/001, "Handles"), read from Lua data text
 * (`where` names it in sentences) or from a file. W_OK with `*out`;
 * W_ERR_CORRUPT for text that isn't Lua data; W_ERR_REFUSED for data that
 * isn't format 1's (a field or key it doesn't have, a value out of
 * range, more than HEADLESS_MOVES_MAX moves or HEADLESS_STEPS_MAX steps),
 * naming the line; the file's own failures (W_ERR_NOT_FOUND ...). */
typedef struct HeadlessMoves HeadlessMoves;
WErr headless_moves_parse(const char *text, size_t len, const char *where, HeadlessMoves **out, WWhy *why);
WErr headless_moves_read(const char *path, HeadlessMoves **out, WWhy *why);
void headless_moves_free(HeadlessMoves *m);

/* How many moves, how many steps they make in all, and a step's seconds. */
int    headless_moves_count(const HeadlessMoves *m);
int    headless_moves_steps(const HeadlessMoves *m);
double headless_moves_step(const HeadlessMoves *m);
/* The map's folder (World\Maps\<name>) the moves are for, "" for any:
 * a route found on one map is only a route there, so a run on another
 * map refuses them (headless_run). */
const char *headless_moves_map(const HeadlessMoves *m);

/* What the run is given: the world's start as 043 gives it (a test-map
 * run's: `start.test_map` and `start.conv` set), the moves, and the
 * folder of the creature list (creatures-<map>.tsv; NULL or "" for
 * none: the run counts no creatures). */
typedef struct {
    WorldStartIn         start;
    const HeadlessMoves *moves;
    const char          *world_dir;
} HeadlessIn;

/* What it found: the receipt's counts, also given back here. */
typedef struct {
    int    moves, steps;
    int    her_stand_in, her_vertices, her_bones, her_sequences, her_animation_changes, her_animation;
    int    creature_spawns, creature_looks, looks_wc3, looks_game, looks_stand_in, looks_character, looks_missing, looks_unreadable;
    double start_x, start_y, start_z, end_x, end_y, end_z, end_facing, distance, lowest_z, highest_z;
    int    tiles, ground_files, steps_airborne, steps_swimming, steps_sliding, steps_no_ground;
    int    models, models_solid, models_wc3, buildings, model_stand_ins, models_left_out;
    int    model_copies, solid_copies, solid_triangles, building_pools, steps_on_models, steps_walled;
    int    pathing, pathing_cells, pathing_closed, steps_walled_pathing, steps_on_closed;
} HeadlessResult;

/* Runs it: the start (world_start_open), her figure (her character from
 * 025, else the baseline set's stand-in, 088), each creature's look
 * (088), then the moves, step by step, from where the start puts her,
 * facing WORLD_START_FACING at WORLD_START_SPEED; then writes `bye`'s
 * receipt (reason closed, with the counts; error, with the sentence, on
 * a failure) and gives the counts in `*out` (may be NULL). W_OK when the
 * moves ran to their end; the start's failures; W_ERR_NOT_FOUND when
 * neither her character nor the stand-in figure can be found; W_ERR_IO
 * when the receipt can't be written; W_ERR_REFUSED when the moves name
 * another map than the run's. With a NULL `bye`, no receipt. */
WErr headless_run(const HeadlessIn *in, Goodbye *bye, HeadlessResult *out, WWhy *why);

#endif
