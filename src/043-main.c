/*
 * 043-main.c (issue 173k; the C twin of src/custom-client/main.lua) - the window
 *
 * For a general audience: opens a window on the ground of Ashenvale near
 * Astranaar, read from the server's own map files, with the night elf of
 * the original game's archives standing on it; moves her with the game's
 * own controls at 120% speed (W/S forward and back, A/D turn or, with the
 * right mouse button held, strafe, Q and E strafe, space jump, the
 * keypad's / toggles walking and running (173i), left drag turns the
 * camera, right drag turns her with it, the wheel zooms; Escape quits
 * until the game menu exists). Written in C on Raylib (owner,
 * 2026-10-05: "let's port now. We should build something like an engine
 * in C anyway, for performance reasons").
 *
 * Usage: custom-client --maps DIR --client DIR [--settings FILE] [--world DIR] [--shot FILE] [--zoom N] [--front] [--turn DEG] [--pitch DEG]
 *   --settings FILE   the player's settings (SET name "value" lines; 173b1);
 *                     without it, every setting is at its default
 *   --shot FILE       draw a few frames, save a picture there, quit
 *   --world DIR       where the generated creature lists are (creatures-<map>.tsv, 173t)
 *   --no-read-ahead   no second thread reading tiles ahead (to measure what it saves; 173u)
 *   --zoom N          (pictures) the camera N yards back; 0 is first person
 *   --turn DEG        (pictures) the camera turned DEG degrees round her, held there
 *   --pitch DEG       (pictures) the camera raised DEG degrees (up to 83)
 *   --clock MS        (pictures) start the world's animations MS milliseconds in
 *   --hour H          the time of day, 0 to 24 (default: this machine's clock)
 *   --face DEG        her start facing turned DEG degrees (left)
 *   --walk S          (pictures) hold forward for S world-seconds
 *   --walking         walk toggled on from the start (173i), as the keypad's / does
 *   --dive S          (pictures) then hold X (swim down) for S world-seconds
 *   --wait S          (pictures) then wait S world-seconds more
 *   --wear SLOT:LOOK,...  dress her, over her starting outfit: each slot's item look (ItemDisplayInfo id),
 *                     e.g. chest:41143,hands:41138,legs:41140,waist:41476,feet:41480
 *   --goto X Y        (pictures) one world-second in, move her to world (X, Y)
 *   --map ID X Y      start on map ID (Map.dbc: 0 Eastern Kingdoms, 1 Kalimdor,
 *                     530 Outland, 571 Northrend) at world (X, Y)
 *   --pose ID MS      (pictures) hold her in animation ID (AnimationData.dbc:
 *                     0 stand, 5 run, 13 walk backwards, 38 jump ...) at MS
 *   --assets DIR      where the settings' packs and overlays are (DIR/packs,
 *                     DIR/overlays; 105a); scripts/run gives assets/
 *
 * A map handed in by world-edit-to-execute's editor (its Test Map button;
 * issue 1109, 067-test-map.h):
 *   --test-map FOLDER the converted map's build folder, in front of the
 *                     asset overlays for this run only. Its map.lua
 *                     (073-converted-map.h, written by world-edit-to-
 *                     execute's W02c) says which map to open (its number
 *                     and its folder under World\Maps) and where she
 *                     starts, so --map isn't taken with it. The map's own
 *                     table rows come before the archives', row by row
 *                     (074-tables.h); its ground, horizon and creature
 *                     looks are read from the folder first; while the
 *                     server has no map files for it (W02d makes them)
 *                     her ground's heights come from its own ground files
 *                     (075-adt-heights.h); --world is FOLDER/world unless given
 *   --receipt FILE    where the goodbye receipt goes when the client ends
 *                     (068-goodbye.h); a crash leaves none
 *   --camera X,Y      start there (world yards) instead of map.lua's start
 *   --realm HOST:PORT the local AzerothCore's login server (127.0.0.1:3724)
 *   --account NAME    the account; its password only from W_CLIENT_PASSWORD
 * An option the client doesn't know, or one short of its values, stops the
 * start (with a receipt, in a test-map run).
 */
#include "023-anim.h"
#include "013-errors.h"
#include "031-camera.h"
#include "025-character.h"
#include "028-collide.h"
#include "035-creatures.h"
#include "016-dbc.h"
#include "042-doodads.h"
#include "041-figure.h"
#include "038-ground.h"
#include "039-horizon.h"
#include "026-light.h"
#include "027-liquid.h"
#include "018-maps.h"
#include "029-mover.h"
#include "030-moves.h"
#include "015-mpq.h"
#include "019-navmesh-slope.h"
#include "034-prefetch.h"
#include "014-settings.h"
#include "037-sky.h"
#include "036-texture.h"
#include "040-water.h"
#include "060-resolver.h"
#include "067-test-map.h"
#include "068-goodbye.h"
#include "073-converted-map.h"
#include "074-tables.h"
#include "075-adt-heights.h"
#include "078-wc3-ground.h"
#include "081-wc3-water.h"
#include "084-map-fog.h"
#include "087-world-start.h"
#include "089-headless-run.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* {{{ the map and the start (--map ID X Y chooses another; the folder
 * name comes from the client's Map.dbc: 1 id, 2 its folder in the
 * archives, World\\Maps\\<folder>) */
/* }}} */

/* {{{ settings */
#define START_O      WORLD_START_FACING                /* facing the town's west side (087-world-start.h) */
#define SPEED        WORLD_START_SPEED                 /* 120% of the base speeds (owner, 2026-10-04; 087-world-start.h) */
#define ELF_HEIGHT   2.3f                              /* yards: the camera looks at her shoulders */
#define EYE_SHARE    0.92f                             /* first person: her eyes, as a share of her height (this client's guess) */
#define ZOOM_BASE    20.0f                             /* yards; times cameraDistanceMaxFactor (1 to 2) is the zoom-out limit (173b4) */
/* her look: ChrRaces id, sex, and the character-creation choices (from 0);
 * a stand-in for the ids the server will send (173d, 617r5) */
#define HER_CLASS WORLD_HER_CLASS                                       /* she wears a fresh hunter's outfit (CharStartOutfit; 3 hunter, 11 druid ...); --wear changes single slots over it */

/* }}} */

/* {{{ the figures: her, and the models standing around her (173j)
 * Each bystander: who (a character's look, or a creature display from
 * CreatureDisplayInfo), where (yards ahead of her start and to her left,
 * facing back toward her), and the animations it cycles through, by the
 * game's numbers (AnimationData.dbc), each held for HOLD_MS. A number
 * its model lacks is skipped. The owner's first pick (2026-10-05): a
 * tauren, a furbolg, an Ashenvale treant (the ancient protector: no
 * treant model is named so) and a nightsaber (the tiger model, the
 * nightsaber's body). */
#define HOLD_MS 4000
static const int CYCLE[] = { 0, 60, 74, 16, 25, 69, 67, 66, 68, 53 };   /* stand, talk, roar, attack, ready, dance, wave, bow, cheer, cast */
static const struct {
    int race, sex;                    /* a character (race 0: a creature) */
    int display;                      /* a creature's display */
    float ahead, left;                /* yards from her start */
    int facial_hair;                  /* a character's facial-hair choice (173d) */
    int class_id;                     /* a character's class: it wears that class's starting outfit (173p) */
} BYSTANDERS[] = {
    { 6, 0, 0,   9.0f,  -4.0f, 1, 1 },   /* a tauren man, bearded, a fresh warrior */
    { 0, 0, 145, 10.0f,  1.0f, 0, 0 },   /* a furbolg */
    { 0, 0, 860, 14.0f,  7.0f, 0, 0 },   /* the ancient protector */
    { 0, 0, 321, 7.0f,   5.0f, 0, 0 },   /* a nightsaber */
};
#define NBYSTANDERS ((int)(sizeof BYSTANDERS / sizeof BYSTANDERS[0]))
/* }}} */

/* {{{ the world and the camera */

/* {{{ the world's creatures (173t)
 * The generated list of the map's spawns (035-creatures.h), read once; each
 * loaded tile brings its spawns in as figures standing where the list
 * says, and takes them away when it is dropped. A display that can't be
 * built is named once and its spawns left out. Each stands playing its
 * standing animation from its own moment (the guid picks it), so they
 * don't breathe in step. */
typedef struct { int spawn; Figure fig; MovesState moves; } Standing;
/* how far creatures are seen (yards): the server tells a client only of
 * creatures within its visibility distance (this server's worldserver.conf:
 * Visibility.Distance.Continents = 100; AzerothCore's default 90), so the
 * original never draws one farther. This list stands in for the network,
 * so it shows what the network would send. Drawing all 467 round
 * Astranaar out to the view distance cost about 6 ms a frame. */
#define CREATURE_SEEN_REACH 100.0
/* where a frame's time goes, for the picture report: each phase's seconds
 * summed over the run. Drawing phases measure handing the work to the
 * graphics card (the card runs behind); "the card finishing" is the frame's
 * end, where the program waits for the card and shows the picture. */
enum { PH_MOVE, PH_POSE_HER, PH_DOODADS_ANIMATE, PH_SKY, PH_GROUND, PH_DOODADS_DRAW, PH_WATER, PH_FIGURES_DRAW, PH_FINISH, PH_COUNT };
static const char *const PHASE_NAME[PH_COUNT] = {
    "moving her (collision, water, camera)", "posing her and the bystanders", "animating doodads (moving, facing, particles)",
    "sky and far horizon", "ground", "doodads and buildings", "water", "her, bystanders and creatures", "the card finishing",
};
#define TIMED(ph, ...) do { double t0_ = GetTime(); __VA_ARGS__; app->phase_s[ph] += GetTime() - t0_; } while (0)
/* {{{ the program's state (the App: docs/001, "Handles"): everything the
 * client holds while it runs, one structure, handed to each part of the
 * frame; one per window */
typedef struct {
    int    map_id;                               /* Kalimdor */
    double start_x, start_y;       /* Astranaar, Ashenvale */
    CharacterLook LOOK;
    int WEAR_OVER[SLOT_COUNT];                        /* --wear's choices, laid over her starting outfit (173p) */
    Figure her;
    Figure others[NBYSTANDERS];
    int    nothers;
    struct { double x, y, z; float facing; MovesState moves; int cycle; float held; } other_at[NBYSTANDERS];
    WorldStart ws;                                    /* the archives, resolver, tables, map folder and heights (087-world-start.h) */
    Doodads *doodads;                                 /* doodads and buildings (042-doodads.h) */
    Ground *ground;                                   /* the painted ground (038-ground.h) */
    Textures textures;                                /* how pictures go to the card (036-texture.h) */
    Figures *figures;                                 /* the figure maker (041-figure.h) */
    GroundPlace ground_place;                         /* where tiles' ground is prepared, on either thread (173u) */
    char   tile_state[64 * 64];                      /* 0 not loaded, 1 painted, 2 no ground file (open sea) */
    int    tiles_loaded, tiles_dropped;               /* for the picture report */
    double ox, oy;                                    /* the drawing origin, near the start */
    Mover  elf;
    Settings *settings;                               /* the player's settings (014-settings.h) */
    FollowCam view;   /* 031-camera.h */
    int    cam_held;                                  /* --front: the camera keeps its own heading */
    float  bob_phase;                                 /* Head Bob's place in her step, yards (camera_bob) */
    SpawnList spawns;
    const char *world_dir;                     /* --world: where creatures-<map>.tsv lives */
    Standing *standing;   int nstanding, standing_cap;
    int    bad_display[512];  int nbad_display;
    double creature_build_s;                          /* seconds spent building creatures, for the report */
    double phase_s[PH_COUNT];
    double creature_s, creature_pose_s;               /* seconds spent posing and drawing creatures, and posing alone, for the report */
    int    last_posed;                                /* creatures posed last frame, for the report */
    int    last_drawn;                                /* creatures drawn last frame, for the report */
    int   *built_of;                                  /* per spawn: its index in standing[]; -1 not built, -2 its look can't be */
    CollideWorld *solids;
    LiquidWorld *waters;          /* every loaded tile's water (173r) */
    double tile_load_most_s, tile_load_total_s;   /* the slowest tile load and their sum, for the picture report (173u) */
    double walk_load_most_s, walk_load_total_s;   /* the same for loads after the start (while she moves) */
    int    walk_loads, started_walking;
    double walk_ground_s, walk_doodads_s;            /* of the walking loads' time: the ground's, the doodads' (173u) */
    int    read_ahead;                            /* --no-read-ahead: off, to measure what it saves */
    Horizon *horizon;                                 /* the far horizon (039-horizon.h) */
    Water *water;                                     /* the water drawn (040-water.h) */
    Sky *sky;                                         /* the sky (037-sky.h) */
    Prefetch *pf;                                     /* the reader-ahead (034-prefetch.h) */
    TestMap *test_map;                                /* a map handed in by the editor (067-test-map.h; 1109), or NULL */
    Goodbye *bye;                                     /* its goodbye receipt (068-goodbye.h), or NULL */
    ConvertedMap conv;                                /* a test map's map.lua: which map, where she starts (073-converted-map.h) */
    Wc3Ground   *wc3_ground;                          /* a test map's own WC3 ground and tilesets, when found (078-wc3-ground.h) */
    Wc3Water    *wc3_water;                           /* its WC3 water, when found (081-wc3-water.h) */
    MapFog       map_fog;                             /* its own fog, from its war3map.w3i, when it turns one on (084-map-fog.h) */
    char   world_in_map[700];                         /* a test map's default --world: <folder>/world */
} App;
/* }}} */
/* build one spawn's figure; 0, or 1 when its look can't be built (said
 * once, and its look's other spawns left out) */
static int build_creature(App *app, int k)
{
    const Spawn *sp = &app->spawns.spawns[k];
    for (int b = 0; b < app->nbad_display; ++b) if (app->bad_display[b] == sp->display) return 1;
    if (app->nstanding == app->standing_cap) { app->standing_cap = app->standing_cap ? app->standing_cap * 2 : 64; app->standing = realloc(app->standing, sizeof *app->standing * app->standing_cap); }
    Standing *st = &app->standing[app->nstanding];
    WWhy why;
    double t0 = GetTime();
    int failed = figure_from_display(app->figures, &st->fig, sp->display, &why);
    app->creature_build_s += GetTime() - t0;
    if (failed) {
        fprintf(stderr, "creatures: %s (%s and any others with its look are left out)\n", why.text, sp->name);
        if (app->nbad_display < 512) app->bad_display[app->nbad_display++] = sp->display;
        return 1;
    }
    st->fig.scale *= sp->size;
    st->spawn = k;
    moves_init(&st->moves, &st->fig.anim);
    moves_advance(&st->moves, &st->fig.anim, (float)(sp->guid % 4000) / 1000.0f);   /* its own moment */
    app->built_of[k] = app->nstanding++;
    return 0;
}
/* free one built creature: the last moves into its place */
static void free_creature(App *app, int i)
{
    app->built_of[app->standing[i].spawn] = -1;
    figure_free(&app->standing[i].fig);
    app->standing[i] = app->standing[--app->nstanding];
    if (i < app->nstanding) app->built_of[app->standing[i].spawn] = i;
}
/* {{{ static stream_creatures (173u): the creatures near her are built,
 * the far ones freed. A spawn in a loaded tile within CREATURE_SEEN_REACH
 * + 10 yards is built, nearest first, until `budget_s` seconds are spent
 * (a few each frame, so walking into town never stalls a frame; all at
 * once at the start); a built one beyond CREATURE_SEEN_REACH + 30 yards
 * is freed (the gap keeps one on the edge from being built and freed
 * over and over). Before, every spawn of a tile was built when the tile
 * loaded: about 110 ms a tile, most never seen. */
static void stream_creatures(App *app, double budget_s)
{
    if (!app->spawns.n) return;
    if (!app->built_of) { app->built_of = malloc(sizeof *app->built_of * (size_t)app->spawns.n); for (int k = 0; k < app->spawns.n; ++k) app->built_of[k] = -1; }
    const double near = CREATURE_SEEN_REACH + 10, far = CREATURE_SEEN_REACH + 30;
    for (int i = app->nstanding - 1; i >= 0; --i) {
        const Spawn *sp = &app->spawns.spawns[app->standing[i].spawn];
        if (hypot(sp->x - app->elf.x, sp->y - app->elf.y) > far) free_creature(app, i);
    }
    double t0 = GetTime();
    while (GetTime() - t0 < budget_s) {
        int best = -1, tx, ty;
        double bd = near;
        maps_tile_of((float)app->elf.x, (float)app->elf.y, &tx, &ty);
        for (int x = tx - 1; x <= tx + 1; ++x)           /* 110 yards never reach past the next tile */
            for (int y = ty - 1; y <= ty + 1; ++y) {
                if (x < 0 || x > 63 || y < 0 || y > 63 || !app->tile_state[x * 64 + y]) continue;
                int first, n = creatures_in_tile(&app->spawns, x, y, &first);
                for (int k = first; k < first + n; ++k) {
                    if (app->built_of[k] != -1) continue;
                    double d = hypot(app->spawns.spawns[k].x - app->elf.x, app->spawns.spawns[k].y - app->elf.y);
                    if (d < bd) { bd = d; best = k; }
                }
            }
        if (best < 0) break;
        if (build_creature(app, best)) app->built_of[best] = -2;  /* its look can't be built: not tried again */
    }
}
/* }}} */
static void drop_creatures(App *app, int tile)
{
    for (int i = app->nstanding - 1; i >= 0; --i)
        if (app->spawns.spawns[app->standing[i].spawn].tile == tile) free_creature(app, i);
}
/* }}} */

/* {{{ the ground and walls the mover asks about (173q)
 * The floor: the map's ground or a model's floor (bridge, stair, a
 * building's floor), whichever is higher at or below z_top; the map's
 * ground above z_top (she is under it, in a cave or a building's
 * cellar) doesn't count. Walls: the models' steep triangles. Her body for
 * walls: 0.4 yards round, 2.3 tall, stepping over 1 yard (173q's open
 * question: the original's own numbers). */
static float ground_at(void *ctx, float x, float y, float z_top, float *gx, float *gy)
{
    App *app = ctx;
    float tx, ty, mx, my;
    float t = maps_world_height(&app->ws.world, x, y, &tx, &ty);
    float m = collide_floor(app->solids, x, y, z_top, &mx, &my);
    /* the model's floor wins when it is higher than a usable map ground,
     * or when the map's ground is above her (she is beneath it) */
    int map_below = t > -10000 && t <= z_top + 0.01f;
    if (m > -10000 && (!map_below || m > t)) { *gx = mx; *gy = my; return m; }
    /* otherwise the map's ground, as before models: also when it lies
     * above her and no model floor does (she lands on it, or the slope
     * rule decides) */
    *gx = tx; *gy = ty;
    return t;
}
static float water_at(void *ctx, double x, double y)
{
    App *app = ctx;
    (void)ctx;
    int kind;
    return liquid_surface(app->waters, x, y, &kind);
}
static int wall_at(void *ctx, double x0, double y0, double x1, double y1, double z, float *nx, float *ny)
{
    App *app = ctx;
    (void)ctx;
    return collide_wall(app->solids, x0, y0, x1, y1, z, 0.4f, (float)MOVER_STEP_UP, ELF_HEIGHT, (float)app->elf.max_grad, nx, ny);
}
/* }}} */

/* tiles loaded round hers: enough to cover the view distance (farclip,
 * 173b6), 1 to 3 (3 x 3 up to 7 x 7) */
static int tile_reach(App *app)
{
    int r = (int)ceil(settings_num(app->settings, "farclip") / 533.33333);
    return r < 1 ? 1 : r > 3 ? 3 : r;
}
static void refresh_tiles(App *app)
{
    const int TILE_REACH = tile_reach(app);
    int tx, ty;
    maps_tile_of((float)app->elf.x, (float)app->elf.y, &tx, &ty);
    for (int dx = -TILE_REACH; dx <= TILE_REACH; ++dx)
        for (int dy = -TILE_REACH; dy <= TILE_REACH; ++dy) {
            int x = tx + dx, y = ty + dy;
            if (x < 0 || x >= 64 || y < 0 || y >= 64 || app->tile_state[x * 64 + y]) continue;
            WWhy why;
            AdtTile *adt;
            double load_t0 = GetTime();
            /* the ground: prepared ahead by the helper thread when it got
             * there first (then only sent to the card here), else prepared
             * here (the start's tiles, a jump, or the helper turned off) */
            GroundPrep *prep;
            int taken = prefetch_take_ground(app->pf, x, y, &prep);   /* 0 prepared, 1 no ground file there, 2 not prepared */
            WErr got = taken == 0 ? W_OK : taken == 1 ? W_ERR_NOT_FOUND
                     : ground_prep_tile(&app->ground_place, x, y, ground_on_card, app->ground, &prep, &why);
            if (got == W_OK) got = ground_load_tile(app->ground, prep, &adt, &why);
            /* no ground file there (open sea) is a tile with nothing to draw;
             * anything else stops the client */
            if (got != W_OK && got != W_ERR_NOT_FOUND) { fprintf(stderr, "painting tile (%d, %d): %s (%s)\n", x, y, why.text, w_err_name(got)); exit(1); }
            double ground_t1 = GetTime();
            if (app->started_walking) app->walk_ground_s += ground_t1 - load_t0;
            if (got == 0) {
                liquid_add_tile(app->waters, x, y, adt);
                water_add_tile(app->water, adt, x * 64 + y, app->ox, app->oy);
                if (doodads_add_tile(app->doodads, adt, x * 64 + y, app->ox, app->oy, &why)) { fprintf(stderr, "doodads on tile (%d, %d): %s\n", x, y, why.text); exit(1); }
                if (app->started_walking) app->walk_doodads_s += GetTime() - ground_t1;
                adt_free(adt);
                free(adt);
            }
            app->tile_state[x * 64 + y] = got == 0 ? 1 : 2;
            ++app->tiles_loaded;
            double took = GetTime() - load_t0;
            app->tile_load_total_s += took;
            if (took > app->tile_load_most_s) app->tile_load_most_s = took;
            if (app->started_walking) { ++app->walk_loads; app->walk_load_total_s += took; if (took > app->walk_load_most_s) app->walk_load_most_s = took; }
        }
    /* the ring just beyond the loaded square: read ahead, so walking into
     * it finds its files unpacked (173u) */
    for (int dx = -TILE_REACH - 1; dx <= TILE_REACH + 1; ++dx)
        for (int dy = -TILE_REACH - 1; dy <= TILE_REACH + 1; ++dy)
            if (abs(dx) == TILE_REACH + 1 || abs(dy) == TILE_REACH + 1) prefetch_tile(app->pf, tx + dx, ty + dy);
    prefetch_keep_near(app->pf, tx, ty, TILE_REACH + 1);          /* preparations she turned away from are let go */
    /* dropping (173s): tiles more than a tile beyond the loaded square go,
     * with all they brought; the margin keeps a walk back and forth over
     * an edge from loading and dropping the same tile again and again */
    for (int x = 0; x < 64; ++x)
        for (int y = 0; y < 64; ++y) {
            if (!app->tile_state[x * 64 + y] || (abs(x - tx) <= TILE_REACH + 1 && abs(y - ty) <= TILE_REACH + 1)) continue;
            drop_creatures(app, x * 64 + y);
            if (app->tile_state[x * 64 + y] == 1) {
                ground_drop_tile(app->ground, x, y);
                water_drop_tile(app->water, x * 64 + y);
                liquid_drop_tile(app->waters, x, y);
                doodads_drop_tile(app->doodads, x * 64 + y);
            }
            app->tile_state[x * 64 + y] = 0;
            ++app->tiles_dropped;
        }
}
/* }}} */

/* {{{ main */
/* {{{ the client's own options and how many values each takes, checked
 * with the test map's (067-test-map.h); main reads them */
static const ClientOption CLIENT_OPTIONS[] = {
    { "--maps", 1 }, { "--settings", 1 }, { "--world", 1 }, { "--no-read-ahead", 0 }, { "--client", 1 }, { "--shot", 1 },
    { "--zoom", 1 }, { "--front", 0 }, { "--turn", 1 }, { "--pitch", 1 }, { "--pose", 2 }, { "--clock", 1 }, { "--hour", 1 },
    { "--face", 1 }, { "--walk", 1 }, { "--walking", 0 }, { "--dive", 1 }, { "--wait", 1 }, { "--wear", 1 }, { "--map", 3 },
    { "--goto", 2 }, { "--assets", 1 }, { NULL, 0 },
};
/* }}} */

/* {{{ static start_failed: the start can't go on: said on the terminal and,
 * in a test-map run, in its goodbye receipt (1109); the window closed if it
 * is open; gives back the exit status */
static int start_failed(App *app, int status, const char *format, ...) __attribute__((format(printf, 3, 4)));
static int start_failed(App *app, int status, const char *format, ...)
{
    char text[600];
    va_list ap;
    va_start(ap, format);
    vsnprintf(text, sizeof text, format, ap);
    va_end(ap);
    fprintf(stderr, "%s\n", text);
    WWhy why;
    if (goodbye_end(app->bye, GOODBYE_ERROR, text, &why)) fprintf(stderr, "%s\n", why.text);
    if (IsWindowReady()) CloseWindow();
    return status;
}
/* }}} */

int main(int argc, char **argv)
{
    App *app = calloc(1, sizeof *app);
    app->map_id = 1;                                     /* Kalimdor */
    app->start_x = 2717.0; app->start_y = -400.0;        /* Astranaar, Ashenvale */
    app->LOOK = (CharacterLook){ WORLD_HER_RACE, WORLD_HER_SEX, 0, 0, 0, 0, { 0 }, 0, 0 };
    app->view = (FollowCam){ .yaw = (float)START_O, .pitch = 0.35f, .dist = 12.0f, .rest_pitch = 0.35f };
    app->read_ahead = 1;
    const char *maps = NULL, *client = NULL, *shot = NULL, *settings_file = NULL, *assets_dir = NULL;
    int pose_id = -1;                                    /* --pose: hold her in this animation */
    double start_ms = 0;                                 /* --clock: the world's animations start this far in */
    double face_deg = 0;                                 /* --face: her start facing turned this far (left) */
    float  walk_s = 0;                                   /* --walk: hold forward this many world-seconds (pictures) */
    int    walking = 0;                                  /* walk toggled (173i): the keypad's /, or --walking */
    float  dive_s = 0;                                   /* --dive: then hold X (swim down) this many world-seconds */
    float  wait_s = 0;                                   /* --wait: then this many world-seconds more before the picture */
    int    go_to = 0;                                    /* --goto: one world-second in, she is moved to (goto_x, goto_y) */
    int    map_given = 0;                                /* --map: refused in a test-map run, whose map.lua says which map */
    double goto_x = 0, goto_y = 0;
    /* the time of day: the original follows the realm's clock, which is
     * real time; until the client talks to a server, this machine's own
     * (--hour H for pictures) */
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    float hours = (float)lt->tm_hour + (float)lt->tm_min / 60.0f;
    float pose_ms = 0;
    /* {{{ a map handed in by the editor (1109): read first, so anything that
     * stops the start from here on is written in its receipt; the password
     * leaves the environment once read, so nothing this client starts
     * inherits it */
    WWhy why;
    WErr asked = test_map_parse(argc, argv, CLIENT_OPTIONS, getenv(TEST_MAP_PASSWORD_VARIABLE), &app->test_map, &why);
    unsetenv(TEST_MAP_PASSWORD_VARIABLE);
    if (app->test_map) {
        WWhy no_receipt;
        if (goodbye_begin(test_map_receipt(app->test_map), test_map_folder(app->test_map), &app->bye, &no_receipt)) {
            fprintf(stderr, "%s\n", no_receipt.text);       /* no receipt can be written: the editor reads its absence as a crash */
            return 2;
        }
    }
    if (asked) return start_failed(app, 2, "%s", why.text);
    /* }}} */
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--maps") && i + 1 < argc) maps = argv[++i];
        else if (!strcmp(argv[i], "--settings") && i + 1 < argc) settings_file = argv[++i];
        else if (!strcmp(argv[i], "--world") && i + 1 < argc) app->world_dir = argv[++i];
        else if (!strcmp(argv[i], "--no-read-ahead")) app->read_ahead = 0;
        else if (!strcmp(argv[i], "--client") && i + 1 < argc) client = argv[++i];
        else if (!strcmp(argv[i], "--assets") && i + 1 < argc) assets_dir = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) app->view.dist = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--front")) { app->view.yaw = (float)(START_O + PI); app->view.pitch = 0.1f; app->cam_held = 1; }
        else if (!strcmp(argv[i], "--turn") && i + 1 < argc) { app->view.yaw = (float)(START_O + atof(argv[++i]) * DEG2RAD); app->cam_held = 1; }
        else if (!strcmp(argv[i], "--pitch") && i + 1 < argc) app->view.pitch = (float)(atof(argv[++i]) * DEG2RAD);
        else if (!strcmp(argv[i], "--pose") && i + 2 < argc) { pose_id = atoi(argv[++i]); pose_ms = (float)atof(argv[++i]); }
        else if (!strcmp(argv[i], "--clock") && i + 1 < argc) start_ms = atof(argv[++i]);
        else if (!strcmp(argv[i], "--hour") && i + 1 < argc) hours = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--face") && i + 1 < argc) face_deg = atof(argv[++i]);
        else if (!strcmp(argv[i], "--walk") && i + 1 < argc) walk_s = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--walking")) walking = 1;
        else if (!strcmp(argv[i], "--dive") && i + 1 < argc) dive_s = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--wait") && i + 1 < argc) wait_s = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--wear") && i + 1 < argc) {
            char bad[200];
            if (character_wear_text(app->WEAR_OVER, argv[++i], bad, sizeof bad))
                return start_failed(app, 2, "--wear: \"%s\" isn't slot:look (slots: shirt legs feet wrists chest hands tabard waist head shoulders back mainhand offhand)", bad);
        }
        else if (!strcmp(argv[i], "--map") && i + 3 < argc) { app->map_id = atoi(argv[++i]); app->start_x = atof(argv[++i]); app->start_y = atof(argv[++i]); map_given = 1; }
        else if (!strcmp(argv[i], "--goto") && i + 2 < argc) { goto_x = atof(argv[++i]); goto_y = atof(argv[++i]); go_to = 1; }
    }
    if (!maps || !client) {
        fprintf(stderr, "usage: custom-client --maps DIR --client DIR [--settings FILE] [--world DIR] [--assets DIR] [--shot FILE] [--zoom N] [--front] [--turn DEG] [--pitch DEG] [--pose ID MS] [--clock MS] [--hour H] [--face DEG] [--walk S] [--walking] [--dive S] [--wait S] [--wear SLOT:LOOK,...] [--goto X Y] [--map ID X Y]\n"
                        "       [--test-map FOLDER --receipt FILE [--camera X,Y] [--realm HOST:PORT] [--account NAME] [--headless MOVES]]   (the password from W_CLIENT_PASSWORD)\n");
        return start_failed(app, 2, "the client needs --maps (the server's map files) and --client (the game's Data folder); scripts/run gives both");
    }
    /* {{{ a test map's own map (1109, open question 4): map.lua in its
     * folder (073-converted-map.h) says which map and where she starts;
     * its creatures are in the folder's world/ unless --world says */
    if (app->test_map) {
        const char *folder = test_map_folder(app->test_map);
        if (map_given) return start_failed(app, 2, "--map can't be given with --test-map: the test map's %s/%s says which map it is", folder, CONVERTED_MAP_FILE);
        if (converted_map_read(folder, &app->conv, &why)) return start_failed(app, 1, "test map: %s", why.text);
        app->map_id = app->conv.id;
        app->start_x = app->conv.start_x; app->start_y = app->conv.start_y;
        if (!app->world_dir) {
            snprintf(app->world_in_map, sizeof app->world_in_map, "%s/world", folder);
            app->world_dir = app->world_in_map;
        }
        printf("test map: map %d, World\\Maps\\%s (\"%s\"), its start at (%.1f, %.1f)\n", app->conv.id, app->conv.directory,
               app->conv.name[0] ? app->conv.name : app->conv.directory, app->conv.start_x, app->conv.start_y);
    }
    /* }}} */
    /* --camera (1109): she starts there instead, the camera behind her as at any start */
    double camera_x, camera_y;
    if (app->test_map && test_map_camera(app->test_map, &camera_x, &camera_y)) { app->start_x = camera_x; app->start_y = camera_y; }

    /* the settings: defaults, then the player's file; a line it refuses is
     * named on the terminal and skipped (173b1) */
    app->settings = settings_new();
    app->view.settings = app->settings;                           /* the camera follows these (031-camera.h) */
    if (settings_file) settings_load(app->settings, settings_file, stderr);
    else fprintf(stderr, "settings: no --settings file given; every setting is at its default\n");
    int win_w, win_h;
    settings_resolution(settings_text(app->settings, "gxResolution"), &win_w, &win_h);   /* accepted when set, so always a resolution */

    /* {{{ the world's start, before the window (087-world-start.h): the
     * archives (a test map may run without them: its folder and the
     * baseline set answer), in a test-map run the resolver in front of
     * them, the tables, the map's folder, the ground's heights and the
     * steepest walkable ground. A failure stops the start by name. */
    WorldStartIn wsi = { .client = client, .maps = maps, .assets_dir = assets_dir, .settings = app->settings, .test_map = app->test_map,
                         .conv = &app->conv, .map_id = app->map_id, .start_x = app->start_x, .start_y = app->start_y, .log = stdout };
    /* a headless test-map run (1109 step 1n, 089-headless-run.h): the
     * start, the figures' lookups and the scripted moves, no window; its
     * receipt carries their counts, and the client ends here */
    if (app->test_map && test_map_headless(app->test_map)[0]) {
        if (shot) return start_failed(app, 2, "--shot draws a picture, and --headless opens no window: give one or the other");
        HeadlessMoves *moves;
        if (headless_moves_read(test_map_headless(app->test_map), &moves, &why)) return start_failed(app, 2, "--headless: %s", why.text);
        HeadlessIn hin = { .start = wsi, .moves = moves, .world_dir = app->world_dir };
        WErr ran = headless_run(&hin, app->bye, NULL, &why);
        headless_moves_free(moves);
        if (ran) fprintf(stderr, "%s\n", why.text);
        goodbye_free(app->bye);
        test_map_free(app->test_map);
        free(app);
        return ran == W_OK ? 0 : 1;
    }
    if (world_start_open(&wsi, &app->ws, &why)) return start_failed(app, 1, "%s", why.text);
    float gx, gy;
    float z = app->ws.z, max_slope = app->ws.max_slope;
    /* }}} */
    mover_init(&app->elf, app->start_x, app->start_y, z, START_O + face_deg * PI / 180, SPEED, max_slope);
    app->view.yaw = (float)app->elf.o + (app->view.yaw - (float)START_O);   /* the camera keeps its place behind her */
    app->ox = floor(app->start_x); app->oy = floor(app->start_y);

    /* the window as the Resolution panel says (173b5): fullscreen at the
     * resolution unless windowed; maximized means also borderless; vsync
     * and resizing as set (both also follow later changes, below) */
    int vsync_on = settings_num(app->settings, "gxVSync") != 0, resize_locked = settings_num(app->settings, "windowResizeLock") != 0;
    int windowed = settings_num(app->settings, "gxWindow") != 0, maximized = windowed && settings_num(app->settings, "gxMaximize") != 0;
    SetConfigFlags((vsync_on ? FLAG_VSYNC_HINT : 0) | (resize_locked ? 0 : FLAG_WINDOW_RESIZABLE)
                   | (windowed ? 0 : FLAG_FULLSCREEN_MODE) | (maximized ? FLAG_WINDOW_MAXIMIZED | FLAG_WINDOW_UNDECORATED : 0));
    InitWindow(win_w, win_h, "Everland Ghostsong");
    textures_init(&app->textures, (int)settings_num(app->settings, "textureFilteringMode"));   /* before any picture is sent (173b6) */
    SetExitKey(KEY_NULL);                                /* Escape is read in the loop, so it can open the game menu later */
    Shader model_sh  = figure_shader();                  /* the figures' bending and lighting (041-figure.h) */
    /* her clothes: a fresh character's of her race and class, then any --wear on top */
    app->figures = figures_new(&app->ws.chain, app->ws.assets, app->ws.tables, &app->textures);   /* makes her, the bystanders and the creatures (041-figure.h) */
    if (app->test_map) figures_wc3_scale(app->figures, app->conv.wc3_scale);       /* a map's own WC3 models, in yards by its rule */
    WErr her_bad = character_start_outfit(app->ws.assets, &app->ws.chain, app->LOOK.race, HER_CLASS, app->LOOK.sex, app->LOOK.items, &why);
    if (!her_bad) {
        for (int k = 0; k < SLOT_COUNT; ++k) if (app->WEAR_OVER[k]) app->LOOK.items[k] = app->WEAR_OVER[k];
        her_bad = figure_from_character(app->figures, &app->her, &app->LOOK, &why);
    }
    if (her_bad) {
        /* a test map with no install (1109 step 1l): her character can't
         * be made (its tables, model and skins are the game's), so the
         * baseline set's stand-in figure stands in her place, said */
        WWhy none;
        if (!app->test_map || figure_stand_in(app->figures, &app->her, "her (the stand-in figure)", &none))
            return start_failed(app, 1, "%s%s%s", why.text, app->test_map ? "; and no stand-in figure: " : "", app->test_map ? none.text : "");
        printf("her: %s; the baseline set's stand-in figure stands in her place\n", why.text);
    }
    static const int NEEDED[] = { MOVE_STAND, MOVE_RUN, MOVE_WALK, MOVE_WALK_BACK, MOVE_SHUFFLE_LEFT, MOVE_SHUFFLE_RIGHT, MOVE_JUMP_START, MOVE_JUMP, MOVE_JUMP_END, MOVE_FALL };
    for (unsigned i = 0; i < sizeof NEEDED / sizeof NEEDED[0]; ++i)
        if (anim_find(&app->her.anim, NEEDED[i], 0) < 0) fprintf(stderr, "her animations: her model has no animation %d; she stands instead\n", NEEDED[i]);
    /* the bystanders: each stands round her start, facing her; one that
     * can't be built is named and left out */
    for (int i = 0; i < NBYSTANDERS; ++i) {
        Figure *f = &app->others[app->nothers];
        CharacterLook look = { BYSTANDERS[i].race, BYSTANDERS[i].sex, 0, 0, 0, 0, { 0 }, BYSTANDERS[i].facial_hair, 0 };
        if (BYSTANDERS[i].race && character_start_outfit(app->ws.assets, &app->ws.chain, look.race, BYSTANDERS[i].class_id, look.sex, look.items, &why)) {
            fprintf(stderr, "bystander %d left out: %s\n", i, why.text);
            continue;
        }
        int bad = BYSTANDERS[i].race ? figure_from_character(app->figures, f, &look, &why)
                                     : figure_from_display(app->figures, f, BYSTANDERS[i].display, &why);
        if (bad) { fprintf(stderr, "bystander %d left out: %s\n", i, why.text); continue; }
        double fx = cos(START_O), fy = sin(START_O);
        app->other_at[app->nothers].x = app->start_x + fx * BYSTANDERS[i].ahead - fy * BYSTANDERS[i].left;
        app->other_at[app->nothers].y = app->start_y + fy * BYSTANDERS[i].ahead + fx * BYSTANDERS[i].left;
        app->other_at[app->nothers].z = maps_world_height(&app->ws.world, (float)app->other_at[app->nothers].x, (float)app->other_at[app->nothers].y, &gx, &gy);
        app->other_at[app->nothers].facing = (float)atan2(app->start_y - app->other_at[app->nothers].y, app->start_x - app->other_at[app->nothers].x);
        moves_init(&app->other_at[app->nothers].moves, &f->anim);
        app->other_at[app->nothers].cycle = 0;
        app->other_at[app->nothers].held = (float)(i * 900);        /* staggered, so they don't change together */
        printf("bystander: %s\n", f->name);
        ++app->nothers;
    }
    /* the ground, its files through the resolver in a test-map run (its folder first) */
    if (ground_place_init(&app->ground_place, &app->ws.chain, app->ws.assets, app->ws.map_name, &app->ws.world, app->ox, app->oy, &why))
        return start_failed(app, 1, "%s", why.text);
    /* a test map's ground laid with WC3's own tilesets, when its own
     * ground and the tilesets' pictures are found (a player with WC3:
     * 078); otherwise said, and the converter's pictures stay */
    if (app->test_map) {
        WErr w = wc3_ground_open(app->ws.assets, &app->ws.chain, app->conv.wc3_scale, app->conv.wc3_x0, app->conv.wc3_y0, &app->wc3_ground, &why);
        if (w == W_OK) {
            int real = 0, n = wc3_ground_tilesets(app->wc3_ground);
            for (int k = 0; k < n; ++k) real += wc3_ground_tileset(app->wc3_ground, k)->source != ASSET_FROM_PLACEHOLDER;
            app->ground_place.wc3 = app->wc3_ground;
            printf("test map: its ground laid cell by cell with WC3's own tilesets and variations (%d of %d pictures found, the checker for the rest)\n", real, n);
            /* its cliffs, as WC3's own cliff models (079), drawn as doodads */
            const Wc3Cliffs *cliffs = wc3_ground_cliffs(app->wc3_ground);
            if (cliffs) {
                Wc3CliffCounts cc;
                wc3_cliffs_counts(cliffs, &cc);
                printf("test map: %d cliffs and %d ramps stand as WC3's own models (%d models, %d of them found, the box for the rest)", cc.placed, cc.ramps_placed, cc.models, cc.found);
                if (cc.no_tileset + cc.ramps_no_tileset) printf("; %d cells name no cliff tileset and keep their ground", cc.no_tileset + cc.ramps_no_tileset);
                printf("\n");
            } else printf("test map: no TerrainArt\\CliffTypes.slk in any place, so its cliffs keep their ground (no cliff models)\n");
            if (wc3_ground_blight(app->wc3_ground) >= 0)
                printf("test map: its blight laid with %s\n", wc3_ground_tileset(app->wc3_ground, wc3_ground_blight(app->wc3_ground))->path);
            else printf("test map: no blight picture named (UI\\WorldEditData.txt), so blighted cells keep their ground\n");
        } else if (w == W_ERR_NOT_FOUND) printf("test map: its ground keeps the converter's pictures: %s\n", why.text);
        else return start_failed(app, 1, "%s", why.text);
    }
    if (!(app->ground = ground_new(&app->ground_place, &app->textures, &why)))
        return start_failed(app, 1, "%s", why.text);
    /* reads tiles ahead of her on a second thread, and prepares their ground (173u) */
    app->pf = prefetch_start(&app->ws.chain, app->ws.assets, app->ws.map_name, app->read_ahead);
    prefetch_prepare_ground(app->pf, &app->ground_place, ground_on_card, app->ground);
    app->solids = collide_new(app->ox, app->oy);
    app->waters = liquid_new();
    app->water = water_new(&app->ws.chain, app->ws.assets, app->ws.tables, &app->textures, &why);   /* its pictures through the resolver in a test-map run */
    if (!app->water) return start_failed(app, 1, "%s", why.text);
    /* a test map's water as WC3 draws it, when its sheet and frames are
     * found (a player with WC3: 081, tinted by its war3map.w3i);
     * otherwise said, and the converter's water stays */
    if (app->wc3_ground) {
        WErr w = wc3_water_open(app->ws.assets, &app->ws.chain, wc3_ground_w3e(app->wc3_ground), app->conv.wc3_scale, app->conv.wc3_x0, app->conv.wc3_y0, &app->wc3_water, &why);
        if (w == W_OK) {
            const Wc3WaterKind *k = wc3_water_kind(app->wc3_water);
            Wc3WaterCounts n = wc3_water_counts(app->wc3_water);
            water_use_wc3(app->water, app->wc3_water);
            printf("test map: its water drawn as WC3's (%s: %d frames, %d wet points, %d deep)%s\n", k->row, k->frames, n.wet, n.deep,
                   k->tinted ? ", tinted by its war3map.w3i" : wc3_water_info(app->wc3_water) ? ", untinted (its war3map.w3i sets no tint)" : ", untinted (no war3map.w3i in its build)");
        } else if (w == W_ERR_NOT_FOUND) printf("test map: its water keeps the converter's look: %s\n", why.text);
        else return start_failed(app, 1, "%s", why.text);
    }
    app->doodads = doodads_new(&app->ws.chain, app->ws.assets, &app->textures, app->solids, app->waters, app->water, &why);
    if (!app->doodads) return start_failed(app, 1, "%s", why.text);
    if (app->test_map) doodads_wc3_scale(app->doodads, app->conv.wc3_scale);   /* a map's own WC3 models, in yards by its rule (1109 step 1c) */
    app->horizon = horizon_new(&app->ws.chain, app->ws.assets, app->ws.map_name, app->ox, app->oy, &why);
    if (why.code != W_OK) printf("horizon: %s\n", why.text);   /* none to draw: said, not a failure */
    LightTables lights;
    if (light_open(&lights, app->ws.assets, &app->ws.chain, &why)) return start_failed(app, 1, "%s", why.text);
    app->sky = sky_new();
    LightNow light;
    int light_id = light_at(&lights, app->map_id, (float)app->elf.x, (float)app->elf.y, (float)app->elf.z, hours, LIGHT_CLEAR, &light);
    if (light_id < 0) printf("light: map %d has no lights in Light.dbc; a plain grey day is used\n", app->map_id);
    else printf("light: %02d:%02d, light %d, fog from %.0f to %.0f yards\n", (int)hours, (int)(fmodf(hours, 1.0f) * 60), light_id, light.fog_start, light.fog_end);
    /* a test map's own fog, from its war3map.w3i (084, 1109 step 1j), in
     * place of the zone's over everything drawn, out of the water; a map
     * that turns none on, or has no war3map.w3i, keeps the zone's */
    if (app->test_map) {
        W3iInfo info;
        WErr w = map_w3i_read(app->ws.assets, &app->ws.chain, &info, &why);
        if (w && w != W_ERR_NOT_FOUND) return start_failed(app, 1, "%s", why.text);
        map_fog_from_w3i(w ? NULL : &info, app->conv.wc3_scale, &app->map_fog);
        if (app->map_fog.on)
            printf("test map: its own fog drawn, from %.0f to %.0f yards, colour (%.0f, %.0f, %.0f) (war3map.w3i style %d, drawn linear)\n", app->map_fog.start, app->map_fog.end,
                   app->map_fog.rgb[0] * 255, app->map_fog.rgb[1] * 255, app->map_fog.rgb[2] * 255, app->map_fog.style);
        else printf("test map: the zone's fog stays: %s\n", app->map_fog.why_off);
    }
    int eye_m = GetShaderLocation(model_sh, "eye");
    /* the map's creatures (173t): without the generated list, none */
    if (app->world_dir) {
        char path[600];
        snprintf(path, sizeof path, "%s/creatures-%d.tsv", app->world_dir, app->map_id);
        WErr got = creatures_read(path, &app->spawns, &why);
        /* no list: said, and the world runs without creatures; a broken one stops the client */
        if (got != W_OK && got != W_ERR_NOT_FOUND) return start_failed(app, 1, "creatures: %s", why.text);
        if (got == W_ERR_NOT_FOUND) fprintf(stderr, "creatures: %s\n", why.text);
        else printf("creatures: %d spawns on this map in %s\n", app->spawns.n, path);
    } else fprintf(stderr, "creatures: no --world folder given; running without creatures\n");
    /* the login, realm and character screens (Phase 5 and 6) aren't built:
     * every run, a test map's too, goes straight into the world. When they
     * are, a test-map run passes over them, logging in to its realm with its
     * account and W_CLIENT_PASSWORD's password (067-test-map.h; 1109) */
    refresh_tiles(app);
    stream_creatures(app, 60.0);                              /* all those near her at the start */
    printf("creatures: %d standing round her (built in %.2f s)\n", app->nstanding, app->creature_build_s);
    int dm, dc, ds, dmov, demit;
    doodads_counts(app->doodads, &dm, &dc, &ds, &dmov, &demit);
    printf("doodads: %d models (%d of them moving, %d with particles), %d placed; %d solid triangles\n", dm, dmov, demit, dc, collide_count(app->solids));
    if (ds) printf("doodads: %d pieces left out: their picture is chosen by the game at run time (not built yet)\n", ds);
    int dwc3, dbox;
    doodads_sources(app->doodads, &dwc3, &dbox);
    if (dwc3 || dbox) printf("doodads: %d of the models are WC3 models the map brought; the placeholder box stands in for %d (drawn as wire-frames)\n", dwc3, dbox);

    MovesState moves;
    moves_init(&moves, &app->her.anim);
    double clock_ms = start_ms;

    int frames = 0, drag_l = 0, drag_r = 0;
    double loop_start = GetTime();                       /* for the picture report's frame time */
    app->started_walking = 1;                                 /* tile loads from here on happen while she moves */
    while (!WindowShouldClose()) {
        /* {{{ input and movement */
        float dt = shot ? 0.05f : GetFrameTime();          /* pictures: steady 20 frames a world-second */
        if (dt > 0.1f) dt = 0.1f;                        /* a stalled frame doesn't fling her */
        if (IsKeyPressed(KEY_ESCAPE)) break;            /* the owner, 2026-10-06: Escape quits until there are menus (173b2) */
        /* Vertical Sync and Disable Resize, live (173b5): a change to
         * either (from the menu, when it exists) takes effect at once */
        if ((settings_num(app->settings, "gxVSync") != 0) != vsync_on) {
            vsync_on = !vsync_on;
            if (vsync_on) SetWindowState(FLAG_VSYNC_HINT); else ClearWindowState(FLAG_VSYNC_HINT);
        }
        if ((settings_num(app->settings, "windowResizeLock") != 0) != resize_locked) {
            resize_locked = !resize_locked;
            if (resize_locked) ClearWindowState(FLAG_WINDOW_RESIZABLE); else SetWindowState(FLAG_WINDOW_RESIZABLE);
        }
        if (go_to && frames == 20) {                     /* --goto: there, standing on the ground */
            go_to = 0;
            float gx2, gy2;
            float gz = maps_world_height(&app->ws.world, (float)goto_x, (float)goto_y, &gx2, &gy2);
            if (gz < -10000) { fprintf(stderr, "--goto %.1f %.1f: no ground there in the server's maps\n", goto_x, goto_y); break; }
            app->elf.x = goto_x; app->elf.y = goto_y; app->elf.z = gz; app->elf.grounded = 1; app->elf.swimming = 0;
        }
        /* the mouse: left drag looks around her, right drag turns her too (031-camera.c) */
        int was_r = drag_r;
        drag_l = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        drag_r = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        Vector2 d = GetMouseDelta();
        FollowCamInput mouse = { .left = drag_l, .right = drag_r, .dx = d.x, .dy = d.y, .dt = dt };
        camera_drag(&app->view, &mouse, was_r, &app->elf.o);
        float wheel = GetMouseWheelMove();
        float zoom_max = ZOOM_BASE * (float)settings_num(app->settings, "cameraDistanceMaxFactor");
        camera_zoom(&app->view, wheel, zoom_max);            /* in past the nearest is first person (173m) */
        if (IsKeyPressed(KEY_KP_DIVIDE)) walking = !walking;   /* the stock game's run/walk toggle key (173i) */
        MoverInput in = {
            .walking = walking,
            .forward = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) || (drag_l && drag_r) || (walk_s > 0 && frames * 0.05f < walk_s),
            .back = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN),
            .left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT),
            .right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT),
            .strafe_left = IsKeyDown(KEY_Q),
            .strafe_right = IsKeyDown(KEY_E),
            .strafe_turns = drag_r,
            .jump = IsKeyDown(KEY_SPACE),
            .swim_down = IsKeyDown(KEY_X) || (dive_s > 0 && frames * 0.05f >= walk_s && frames * 0.05f < walk_s + dive_s),   /* swimming down; up is the jump key (173r) */
            .pitch = -app->view.pitch,                        /* where the camera looks, up positive */
        };
        double before_o = app->elf.o, before_x = app->elf.x, before_y = app->elf.y;
        MoverWorld mw = { ground_at, wall_at, water_at, app };
        TIMED(PH_MOVE, mover_step(&app->elf, &in, dt, &mw));
        /* moving = she moved along the ground this frame, by any cause
         * (keys, both buttons, sliding); turning in place isn't moving */
        int moving = hypot(app->elf.x - before_x, app->elf.y - before_y) > 1e-4;
        if (!app->cam_held) {
            camera_follow(&app->view, &mouse, camera_turn_between((float)before_o, (float)app->elf.o), moving, app->elf.o);
            float sgx, sgy;
            ground_at(app, (float)app->elf.x, (float)app->elf.y, (float)app->elf.z + 1.0f, &sgx, &sgy);
            camera_tilt(&app->view, &mouse, moving && app->elf.grounded, (float)(sgx * cos(app->elf.o) + sgy * sin(app->elf.o)));   /* Follow Terrain (173m) */
        }

        /* her animation: chosen from what she did this frame, then her
         * points bent by the bones and copied into each piece's mesh */
        int turning = !drag_r && (in.left || in.right);
        MovesInput mi = {
            .forward = in.forward, .back = in.back,
            .strafe_left = in.strafe_left || (drag_r && in.left), .strafe_right = in.strafe_right || (drag_r && in.right),
            .turn_left = turning && in.left, .turn_right = turning && in.right,
            .grounded = app->elf.grounded || app->elf.swimming, .rising = app->elf.vz > 0, .swimming = app->elf.swimming, .walking = walking,
            .speed = (float)mover_intended_speed(&app->elf, &in), .dt = dt,   /* the keys' speed, not the distance gone: into a wall she runs on the spot */
        };
        moves_step(&moves, &app->her.anim, &mi);
        if (pose_id >= 0) {
            int seq = anim_find(&app->her.anim, pose_id, 0);
            if (seq < 0) { fprintf(stderr, "--pose %d: her model has no such animation\n", pose_id); break; }
            moves.seq = seq; moves.ms = pose_ms; moves.from = -1; moves.once = 0;
        }
        clock_ms += dt * 1000.0;
        TIMED(PH_POSE_HER, figure_bend(&app->her, &moves, (uint32_t)clock_ms));
        /* the bystanders: each holds an animation for HOLD_MS, then the
         * next of CYCLE its model has */
        for (int i = 0; i < app->nothers; ++i) {
            app->other_at[i].held += dt * 1000.0f;
            if (app->other_at[i].held >= HOLD_MS) {
                app->other_at[i].held = 0;
                for (int tries = 0; tries < (int)(sizeof CYCLE / sizeof CYCLE[0]); ++tries) {
                    app->other_at[i].cycle = (app->other_at[i].cycle + 1) % (int)(sizeof CYCLE / sizeof CYCLE[0]);
                    if (anim_find(&app->others[i].anim, CYCLE[app->other_at[i].cycle], 0) >= 0) break;
                }
                moves_play(&app->other_at[i].moves, &app->others[i].anim, CYCLE[app->other_at[i].cycle]);
            }
            moves_advance(&app->other_at[i].moves, &app->others[i].anim, dt);
            figure_bend(&app->others[i], &app->other_at[i].moves, (uint32_t)clock_ms);
        }
        /* the world's creatures (173t): standing, breathing; only those
         * she could see (CREATURE_SEEN_REACH, drawn below) are bent */
        double creature_t0 = GetTime();
        {
            int posed = 0;
            float reach2 = fminf((float)settings_num(app->settings, "farclip"), CREATURE_SEEN_REACH);
            reach2 *= reach2;
            for (int i = 0; i < app->nstanding; ++i) {
                const Spawn *sp = &app->spawns.spawns[app->standing[i].spawn];
                double dx = sp->x - app->elf.x, dy = sp->y - app->elf.y;
                if (dx * dx + dy * dy > reach2) continue;
                ++posed;
                moves_advance(&app->standing[i].moves, &app->standing[i].fig.anim, dt);
                figure_bend(&app->standing[i].fig, &app->standing[i].moves, (uint32_t)clock_ms);
            }
            app->last_posed = posed;
        }
        app->creature_s += GetTime() - creature_t0;
        app->creature_pose_s += GetTime() - creature_t0;
        refresh_tiles(app);
        stream_creatures(app, 0.004);                         /* up to 4 ms a frame building creatures coming into sight (173u) */
        SetWindowTitle(TextFormat("Everland Ghostsong  -  Ashenvale  x %.1f  y %.1f  z %.1f", app->elf.x, app->elf.y, app->elf.z));
        /* }}} */

        /* {{{ the camera: behind her by its own heading, raised by its pitch;
         * or, zoomed all the way in, first person: the view from her eyes
         * along the camera's heading, looking down by its pitch, she not
         * drawn (as the original hides you), Head Bob lifting it with her
         * steps (173m). First person skips the third-person keeping-out
         * below: her eyes are where she is, which the mover already keeps
         * out of walls, ground and (unless swimming) water */
        int first_person = app->view.dist == 0;
        Vector3 at = { (float)(app->elf.x - app->ox), (float)(app->elf.y - app->oy), (float)app->elf.z + ELF_HEIGHT * 0.85f };
        Vector3 eye = { at.x - cosf(app->view.yaw) * cosf(app->view.pitch) * app->view.dist,
                        at.y - sinf(app->view.yaw) * cosf(app->view.pitch) * app->view.dist,
                        at.z + sinf(app->view.pitch) * app->view.dist };
        Vector3 look = at;                               /* where the camera points */
        {
            float stepped = app->elf.grounded && !app->elf.swimming ? (float)hypot(app->elf.x - before_x, app->elf.y - before_y) : 0;
            float lift = camera_bob(app->settings, &app->bob_phase, stepped, dt);   /* runs in third person too, so the phase carries over */
            if (first_person) {
                eye = (Vector3){ at.x, at.y, (float)app->elf.z + ELF_HEIGHT * EYE_SHARE + lift };
                look = (Vector3){ eye.x + cosf(app->view.yaw) * cosf(app->view.pitch), eye.y + sinf(app->view.yaw) * cosf(app->view.pitch), eye.z - sinf(app->view.pitch) };
            }
        }
        if (!first_person) {
            /* something solid between her head and the camera (a wall, a
             * tree, a roof): the camera comes in to just before it (173m) */
            float in_way = collide_ray(app->solids, at.x + app->ox, at.y + app->oy, at.z, eye.x + app->ox, eye.y + app->oy, eye.z);
            if (in_way < 1) {
                float keep = fmaxf(in_way - 0.3f / fmaxf(app->view.dist, 0.3f), 0.05f);   /* 0.3 yards short of it */
                eye = (Vector3){ at.x + (eye.x - at.x) * keep, at.y + (eye.y - at.y) * keep, at.z + (eye.z - at.z) * keep };
            }
            float floor_z = maps_world_height(&app->ws.world, eye.x + (float)app->ox, eye.y + (float)app->oy, &gx, &gy);
            if (floor_z > -10000 && eye.z < floor_z + 0.6f) {
                /* never under the ground. Smart Pivot on: the camera slides along
                 * the ground and looks up at her; off: it stops lowering, the
                 * pitch held where the camera meets the ground (173m) */
                eye.z = floor_z + 0.6f;
                if (settings_num(app->settings, "cameraPivot") == 0 && app->view.dist > 0.1f) {
                    float s = (eye.z - at.z) / app->view.dist;
                    float hold = asinf(s < -1 ? -1 : s > 1 ? 1 : s);
                    if (app->view.pitch < hold) app->view.pitch = hold;
                }
            }
            /* Water Collision (173m): the camera stays on her side of the
             * water's surface: above it while her head is above, below while
             * under */
            if (settings_num(app->settings, "cameraWaterCollision") != 0) {
                int wk = 0;
                float surface = liquid_surface(app->waters, eye.x + app->ox, eye.y + app->oy, &wk);
                float head = at.z;
                if (surface > -10000) {
                    if (head >= surface && eye.z < surface + 0.25f) eye.z = surface + 0.25f;
                    else if (head < surface && eye.z > surface - 0.25f) eye.z = surface - 0.25f;
                }
            }
        }
        Camera3D cam = { .position = eye, .target = look, .up = { 0, 0, 1 }, .fovy = 60, .projection = CAMERA_PERSPECTIVE };
        TIMED(PH_DOODADS_ANIMATE, doodads_animate(app->doodads, (uint32_t)clock_ms, dt, eye, look, (float)settings_num(app->settings, "particleDensity")));   /* moving doodads, and particles near the camera */
        SetShaderValue(model_sh, eye_m, &eye, SHADER_UNIFORM_VEC3);
        /* }}} */

        BeginDrawing();
        /* the light here, now: time runs on in real time */
        hours = fmodf(hours + dt / 3600.0f, 24.0f);
        /* underwater (173r, 173n): with the camera below the water's surface
         * the light is the zone's own underwater set (each light names one
         * beside its clear-weather set: its fog closes in, its colours the
         * water's) */
        int water_kind = 0;
        float eye_surface = liquid_surface(app->waters, eye.x + app->ox, eye.y + app->oy, &water_kind);
        int under = eye_surface > -10000 && eye.z < eye_surface;
        int lit = light_at(&lights, app->map_id, (float)app->elf.x, (float)app->elf.y, (float)app->elf.z, hours, under ? LIGHT_UNDERWATER : LIGHT_CLEAR, &light);
        /* a test map's own fog, when it has one, out of the water (084);
         * then the fog ends by the view distance at the latest (173b6) */
        map_fog_frame(&app->map_fog, under, (float)settings_num(app->settings, "farclip"), &light);
        if (lit != light_id) { light_id = lit; printf("light: now light %d\n", lit); }
        sky_light_shader(model_sh, &light);
        ClearBackground((Color){ (unsigned char)(fminf(light.fog[0], 1) * 255), (unsigned char)(fminf(light.fog[1], 1) * 255), (unsigned char)(fminf(light.fog[2], 1) * 255), 255 });
        BeginMode3D(cam);
        /* the projection: 60 degrees, near 0.3, far a little past the view
         * distance (farclip, 173b6) so the fog, which ends by it, hides the edge
         * (Raylib's own far plane is 1000; this Raylib has no setter, so the
         * projection is loaded directly) */
        rlMatrixMode(RL_PROJECTION);
        rlLoadIdentity();
        rlMultMatrixf(MatrixToFloat(MatrixPerspective(60 * DEG2RAD, (double)GetScreenWidth() / GetScreenHeight(), 0.3, fmax(settings_num(app->settings, "farclip") * 1.25 + 100, (HORIZON_REACH + 1) * 533.33))));   /* out to the far horizon (173s) */
        rlMatrixMode(RL_MODELVIEW);
        rlDisableBackfaceCulling();
        TIMED(PH_SKY, sky_draw(app->sky, app->doodads, eye, &light); rlDrawRenderBatchActive());   /* first, behind everything */
        {
            int htx, hty;
            maps_tile_of((float)app->elf.x, (float)app->elf.y, &htx, &hty);
            TIMED(PH_SKY, horizon_draw(app->horizon, htx, hty, app->tile_state, eye, &light));   /* low-detail tiles beyond the loaded ones (173s) */
        }
        ground_blob(app->ground, at.x, at.y, (float)app->elf.z, 1.3f);     /* her shadow, a little wider than her shoulders */
        for (int i = 0; i < app->nothers; ++i)                /* theirs, sized by their display's scale */
            ground_blob(app->ground, (float)(app->other_at[i].x - app->ox), (float)(app->other_at[i].y - app->oy), (float)app->other_at[i].z, 1.3f * app->others[i].scale);
        for (int i = 0; i < app->nstanding; ++i) {            /* the world's creatures' (173t) */
            const Spawn *sp = &app->spawns.spawns[app->standing[i].spawn];
            ground_blob(app->ground, (float)(sp->x - app->ox), (float)(sp->y - app->oy), (float)sp->z, 1.3f * app->standing[i].fig.scale);
        }
        TIMED(PH_GROUND, ground_draw(app->ground, eye, &light); rlDrawRenderBatchActive());
        TIMED(PH_DOODADS_DRAW, doodads_draw(app->doodads, eye, &light); rlDrawRenderBatchActive());
        TIMED(PH_WATER, water_draw(app->water, eye, &light, clock_ms / 1000.0); rlDrawRenderBatchActive());   /* see-through, after the solid world */
        double figures_t0 = GetTime();
        Matrix place = MatrixMultiply(MatrixRotateZ((float)app->elf.o + moves.body_turn), MatrixTranslate(at.x, at.y, (float)app->elf.z));
        if (!first_person) figure_draw(&app->her, model_sh, place);   /* first person: she isn't drawn (173m) */
        for (int i = 0; i < app->nothers; ++i)
            figure_draw(&app->others[i], model_sh, MatrixMultiply(MatrixRotateZ(app->other_at[i].facing),
                        MatrixTranslate((float)(app->other_at[i].x - app->ox), (float)(app->other_at[i].y - app->oy), (float)app->other_at[i].z)));
        creature_t0 = GetTime();
        int creatures_drawn = 0;
        for (int i = 0; i < app->nstanding; ++i) {            /* the world's creatures (173t): within sight, and not behind the camera */
            const Spawn *sp = &app->spawns.spawns[app->standing[i].spawn];
            double dx = sp->x - app->elf.x, dy = sp->y - app->elf.y, far = fmin(settings_num(app->settings, "farclip"), CREATURE_SEEN_REACH);
            if (dx * dx + dy * dy > far * far) continue;
            Vector3 to = { (float)(sp->x - app->ox) - eye.x, (float)(sp->y - app->oy) - eye.y, (float)sp->z - eye.z }, ahead = Vector3Subtract(look, eye);
            if (Vector3DotProduct(to, Vector3Normalize(ahead)) < -4.0f * app->standing[i].fig.scale) continue;   /* wholly behind: 4 yards covers the biggest townsfolk */
            ++creatures_drawn;
            figure_draw(&app->standing[i].fig, model_sh, MatrixMultiply(MatrixRotateZ(sp->facing), MatrixTranslate((float)(sp->x - app->ox), (float)(sp->y - app->oy), (float)sp->z)));
        }
        rlDrawRenderBatchActive();
        app->creature_s += GetTime() - creature_t0;
        app->last_drawn = creatures_drawn;
        rlDrawRenderBatchActive();
        app->phase_s[PH_FIGURES_DRAW] += GetTime() - figures_t0;
        EndMode3D();
        TIMED(PH_FINISH, EndDrawing());
        goodbye_frame(app->bye);                                /* counted for the receipt (1109) */

        /* --shot: after 3 world-seconds, the picture, then quit */
        if (shot && ++frames == 60 + (int)((walk_s + dive_s + wait_s) * 20)) {   /* 3 world-seconds after any walk: particles have filled out */
            Image im = LoadImageFromScreen();
            int ok = ExportImage(im, shot);
            UnloadImage(im);
            printf(ok ? "picture saved: %s\n" : "could not save the picture to %s\n", shot);
            int dm2, dc2, ds2, dmov2, demit2;
            doodads_counts(app->doodads, &dm2, &dc2, &ds2, &dmov2, &demit2);
            int ahead_tiles, ahead_files, grounds_made, grounds_taken;
            prefetch_counts(app->pf, &ahead_tiles, &ahead_files, &grounds_made, &grounds_taken);
            printf("unpacking files from the archives: %.0f ms over the run, both threads (%llu read, %llu from memory); read ahead: %d tiles, %d files\n",
                   app->ws.chain.unpack_s * 1000, (unsigned long long)app->ws.chain.misses, (unsigned long long)app->ws.chain.hits, ahead_tiles, ahead_files);
            printf("ground prepared ahead: %d tiles, %d of them loaded\n", grounds_made, grounds_taken);
            printf("tile loads: %.0f ms the slowest, %.0f ms on average (each stalls the frame it happens in; 173u)\n", app->tile_load_most_s * 1000, app->tiles_loaded ? app->tile_load_total_s * 1000 / app->tiles_loaded : 0.0);
            printf("  of those, while she moved: %d, %.0f ms the slowest, %.0f ms on average\n", app->walk_loads, app->walk_load_most_s * 1000, app->walk_loads ? app->walk_load_total_s * 1000 / app->walk_loads : 0.0);
            if (app->walk_loads) printf("  of their average, the ground %.0f ms, water and doodads %.0f ms\n", app->walk_ground_s * 1000 / app->walk_loads, app->walk_doodads_s * 1000 / app->walk_loads);
            printf("tiles: %d loaded, %d dropped over the run; now %d placed, %d solid triangles\n", app->tiles_loaded, app->tiles_dropped, dc2, collide_count(app->solids));
            printf("frames took %.1f ms each on average, over %d (drawing, bending and all)\n", (GetTime() - loop_start) * 1000.0 / frames, frames);
            for (int ph = 0; ph < PH_COUNT; ++ph) printf("  %5.2f ms a frame: %s\n", app->phase_s[ph] * 1000.0 / frames, PHASE_NAME[ph]);
            printf("the ground sent %.1f draw orders a frame, the doodads %.1f (173u)\n", (double)ground_orders_last(app->ground) / frames, (double)doodads_orders_last(app->doodads) / frames);
            printf("model pictures on the card: %.1f MB (%.1f MB if unpacked; 173u)\n", app->textures.bytes_sent / 1048576.0, app->textures.bytes_unpacked / 1048576.0);
            printf("creatures took %.2f ms a frame, %.2f of it posing (%d posed, %d drawn, of %d standing)\n", app->creature_s * 1000.0 / frames, app->creature_pose_s * 1000.0 / frames, app->last_posed, app->last_drawn, app->nstanding);
            printf("she ended at x %.2f y %.2f z %.2f, %s\n", app->elf.x, app->elf.y, app->elf.z, app->elf.swimming ? "swimming" : app->elf.grounded ? "on the ground" : "in the air");
            /* the creatures nearest her (173t), to check pictures against */
            for (int pick = 0, used[5] = { -1, -1, -1, -1, -1 }; pick < 5 && pick < app->nstanding; ++pick) {
                int best = -1;
                double bd = 1e30;
                for (int i = 0; i < app->nstanding; ++i) {
                    int taken = 0;
                    for (int u = 0; u < pick; ++u) taken |= used[u] == i;
                    const Spawn *sp = &app->spawns.spawns[app->standing[i].spawn];
                    double d = hypot(sp->x - app->elf.x, sp->y - app->elf.y);
                    if (!taken && d < bd) { bd = d; best = i; }
                }
                used[pick] = best;
                const Spawn *sp = &app->spawns.spawns[app->standing[best].spawn];
                printf("near her: %s (%s) at x %.1f y %.1f z %.1f, %.1f yards off\n", sp->name, app->standing[best].fig.name, sp->x, sp->y, sp->z, bd);
            }
            break;
        }
    }
    /* the window's size, kept (173b5): resized by the player in a plain
     * window, it is the resolution next time; saved into the settings
     * file (pictures and maximized or fullscreen windows leave it be) */
    if (!shot && windowed && !maximized && !IsWindowMaximized()) {
        char size[32];
        snprintf(size, sizeof size, "%dx%d", GetScreenWidth(), GetScreenHeight());
        if (!strcmp(size, settings_text(app->settings, "gxResolution"))) {
            /* not resized: nothing to keep */
        } else if (settings_set(app->settings, "gxResolution", size, &why)) {
            fprintf(stderr, "settings: the window's size isn't kept: %s\n", why.text);   /* smaller than 640x480 */
        } else {
            if (!settings_file) printf("settings: the window is now %s, but no --settings file was given to keep it in\n", size);
            else if (settings_save(app->settings, settings_file, &why)) fprintf(stderr, "settings: the window's size %s could not be saved: %s\n", size, why.text);
            else printf("settings: the window's size %s saved to %s\n", size, settings_file);
        }
    }
    /* letting go, in order: the reading-ahead thread first (it reads the
     * archives and the heights); then everything on the graphics card,
     * while the window (and so the card's context) still exists; then the
     * window; then the rest */
    prefetch_free(app->pf);
    figure_free(&app->her);
    for (int i = 0; i < app->nothers; ++i) figure_free(&app->others[i]);
    for (int i = 0; i < app->nstanding; ++i) figure_free(&app->standing[i].fig);
    figures_free(app->figures);
    doodads_free(app->doodads);
    ground_free(app->ground);
    water_free(app->water);
    wc3_water_free(app->wc3_water);                           /* after the water drawer that drew with it */
    horizon_free(app->horizon);
    sky_free(app->sky);
    UnloadShader(model_sh);
    CloseWindow();
    light_close(&lights);
    collide_free(app->solids);
    liquid_free(app->waters);
    free(app->standing);
    free(app->built_of);
    creatures_free(&app->spawns);
    if (app->ws.assets) resolver_report(app->ws.assets, stdout);
    if (app->test_map) tables_report(app->ws.tables, stdout);
    if (app->ws.adt_heights) printf("ground: %d tiles' heights made from the map's own ground files\n", adt_heights_made(app->ws.adt_heights));
    wc3_ground_free(app->wc3_ground);                         /* after the ground and the reading-ahead thread that painted with it */
    world_start_close(&app->ws);                              /* the heights, tables, resolver and archives, in that order */
    settings_free(app->settings);
    /* the goodbye receipt, last (1109): its being there says the run ended
     * by itself, not in a crash */
    WErr bye = goodbye_end(app->bye, GOODBYE_CLOSED, NULL, &why);
    if (bye) fprintf(stderr, "%s\n", why.text);
    goodbye_free(app->bye);
    test_map_free(app->test_map);
    free(app);
    return bye ? 1 : 0;
}
/* }}} */
