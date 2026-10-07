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
 */
#include "023-anim.h"
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

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* {{{ the map and the start (--map ID X Y chooses another; the folder
 * name comes from the client's Map.dbc: 1 id, 2 its folder in the
 * archives, World\\Maps\\<folder>) */
static int    map_id = 1;                               /* Kalimdor */
static char   map_name[64] = "Kalimdor";
static double start_x = 2717.0, start_y = -400.0;       /* Astranaar, Ashenvale */
/* }}} */

/* {{{ settings */
#define START_O      3.6                               /* facing the town's west side */
#define SPEED        1.2                               /* 120% of the base speeds (owner, 2026-10-04) */
#define ELF_HEIGHT   2.3f                              /* yards: the camera looks at her shoulders */
#define EYE_SHARE    0.92f                             /* first person: her eyes, as a share of her height (this client's guess) */
#define ZOOM_BASE    20.0f                             /* yards; times cameraDistanceMaxFactor (1 to 2) is the zoom-out limit (173b4) */
/* her look: ChrRaces id, sex, and the character-creation choices (from 0);
 * a stand-in for the ids the server will send (173d, 617r5) */
static CharacterLook LOOK = { 4, 1, 0, 0, 0, 0, { 0 }, 0, 0 };
#define HER_CLASS 3                                       /* she wears a fresh hunter's outfit (CharStartOutfit; 3 hunter, 11 druid ...); --wear changes single slots over it */
static int WEAR_OVER[SLOT_COUNT];                        /* --wear's choices, laid over her starting outfit (173p) */

/* {{{ static wear_option: "--wear chest:41143,hands:41138": slot names to
 * item looks (ItemDisplayInfo ids); 0 on success, else the bad part */
static const char *const SLOT_NAMES[SLOT_COUNT] = {
    "shirt", "legs", "feet", "wrists", "chest", "hands", "tabard", "waist", "head", "shoulders", "back", "mainhand", "offhand",
};
static int wear_option(const char *text, char *bad, int bad_len)
{
    char copy[600];
    snprintf(copy, sizeof copy, "%s", text);
    for (char *part = strtok(copy, ","); part; part = strtok(NULL, ",")) {
        char *colon = strchr(part, ':');
        int slot = -1;
        if (colon) { *colon = '\0'; for (int k = 0; k < SLOT_COUNT; ++k) if (!strcmp(part, SLOT_NAMES[k])) slot = k; }
        if (slot < 0 || atoi(colon + 1) <= 0) { snprintf(bad, bad_len, "%s", part); return 1; }
        WEAR_OVER[slot] = atoi(colon + 1);
    }
    return 0;
}
/* }}} */
/* }}} */

/* {{{ shaders
 * The figures' (her, bystanders, creatures, their items): bent on the
 * graphics card (173u). Each point carries its four bone numbers (in the
 * colour slot, a byte each: Raylib 5.1 has no bone slots) and their
 * weights (in the tangent slot); each figure's bone poses arrive as a
 * one-row picture of floats (`bones`), four texels a bone, one column of
 * its matrix each (041-figure.c). A point tied to no bone stays where made.
 * Then lit by the sun from one direction, faded into the sky with
 * distance, see-through pixels (hair strands) cut out. GLSL 330, with
 * Raylib's own attribute and uniform names. The ground has its own, in
 * 038-ground.c. */
static const char *FIGURE_VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec3 vertexNormal; in vec4 vertexColor; in vec2 vertexTexCoord; in vec4 vertexTangent;\n"
    "uniform mat4 mvp; uniform mat4 matModel; uniform vec3 eye; uniform sampler2D bones;\n"
    "out vec3 n; out vec4 col; out vec2 uv; out float dist;\n"
    "mat4 bone(int i) {\n"
    "  return mat4(texelFetch(bones, ivec2(i * 4, 0), 0), texelFetch(bones, ivec2(i * 4 + 1, 0), 0),\n"
    "              texelFetch(bones, ivec2(i * 4 + 2, 0), 0), texelFetch(bones, ivec2(i * 4 + 3, 0), 0));\n"
    "}\n"
    "void main() {\n"
    "  ivec4 id = ivec4(vertexColor * 255.0 + 0.5);\n"
    "  vec4 w = vertexTangent;\n"
    "  float total = w.x + w.y + w.z + w.w;\n"
    "  mat4 skin = total > 0.001 ? bone(id.x) * w.x + bone(id.y) * w.y + bone(id.z) * w.z + bone(id.w) * w.w : mat4(1.0);\n"
    "  vec4 p = skin * vec4(vertexPosition, 1.0);\n"
    "  vec4 wp = matModel * p;\n"
    "  n = mat3(matModel) * (mat3(skin) * vertexNormal); col = vec4(1.0); uv = vertexTexCoord;\n"
    "  dist = length(wp.xyz - eye);\n"
    "  gl_Position = mvp * p;\n"
    "}\n";
/* `mode` (041-figure.c): 0 solid, pixels under half opacity cut out; 1
 * see-through, the picture's opacity times the tint's; 2 added light,
 * which the fog dims to nothing rather than tinting (as particles,
 * 173o). `tint` is the piece's animated colour and opacity, `unlit` skips
 * the sun (glows). */
static const char *MODEL_FS =
    "#version 330\n"
    "in vec3 n; in vec4 col; in vec2 uv; in float dist; out vec4 frag; uniform sampler2D texture0;\n"
    "uniform vec3 sun_dir, sun_col, shade_col, fog_col; uniform vec2 fog_range;\n"
    "uniform vec4 tint; uniform int mode, unlit;\n"
    "void main() {\n"
    "  vec4 t = texture(texture0, uv) * tint;\n"
    "  if (mode == 0 && t.a < 0.5 * tint.a) discard;\n"
    "  vec3 light = unlit != 0 ? vec3(1.0) : shade_col + sun_col * max(dot(normalize(n), sun_dir), 0.0);\n"
    "  float fog = clamp((dist - fog_range.x) / max(fog_range.y - fog_range.x, 1.0), 0.0, 1.0);\n"
    "  if (mode == 2) frag = vec4(t.rgb * light * (1.0 - fog), t.a);\n"
    "  else frag = vec4(mix(t.rgb * light, fog_col, fog), mode == 1 ? t.a : 1.0);\n"
    "}\n";
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
static Figure her;
static Figure others[NBYSTANDERS];
static int    nothers;
static struct { double x, y, z; float facing; MovesState moves; int cycle; float held; } other_at[NBYSTANDERS];
/* }}} */

/* {{{ the world and the camera */
static MapWorld world;
static MpqChain chain;                                   /* the original client's archives, open for the whole run */
static GroundPlace ground_place;                         /* where tiles' ground is prepared, on either thread (173u) */
static char   tile_state[64 * 64];                      /* 0 not loaded, 1 painted, 2 no ground file (open sea) */
static int    tiles_loaded, tiles_dropped;               /* for the picture report */
static double ox, oy;                                    /* the drawing origin, near the start */
static Mover  elf;
static FollowCam view = { .yaw = (float)START_O, .pitch = 0.35f, .dist = 12.0f, .rest_pitch = 0.35f };   /* 031-camera.h */
static int    cam_held;                                  /* --front: the camera keeps its own heading */
static float  bob_phase;                                 /* Head Bob's place in her step, yards (camera_bob) */

/* {{{ the world's creatures (173t)
 * The generated list of the map's spawns (035-creatures.h), read once; each
 * loaded tile brings its spawns in as figures standing where the list
 * says, and takes them away when it is dropped. A display that can't be
 * built is named once and its spawns left out. Each stands playing its
 * standing animation from its own moment (the guid picks it), so they
 * don't breathe in step. */
static SpawnList spawns;
static const char *world_dir = NULL;                     /* --world: where creatures-<map>.tsv lives */
typedef struct { int spawn; Figure fig; MovesState moves; } Standing;
static Standing *standing;   static int nstanding, standing_cap;
static int    bad_display[512];  static int nbad_display;
static double creature_build_s;                          /* seconds spent building creatures, for the report */
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
static double phase_s[PH_COUNT];
#define TIMED(ph, ...) do { double t0_ = GetTime(); __VA_ARGS__; phase_s[ph] += GetTime() - t0_; } while (0)
static double creature_s, creature_pose_s;               /* seconds spent posing and drawing creatures, and posing alone, for the report */
static int    last_posed;                                /* creatures posed last frame, for the report */
static int    last_drawn;                                /* creatures drawn last frame, for the report */
static int   *built_of;                                  /* per spawn: its index in standing[]; -1 not built, -2 its look can't be */
/* build one spawn's figure; 0, or 1 when its look can't be built (said
 * once, and its look's other spawns left out) */
static int build_creature(int k)
{
    const Spawn *sp = &spawns.spawns[k];
    for (int b = 0; b < nbad_display; ++b) if (bad_display[b] == sp->display) return 1;
    if (nstanding == standing_cap) { standing_cap = standing_cap ? standing_cap * 2 : 64; standing = realloc(standing, sizeof *standing * standing_cap); }
    Standing *st = &standing[nstanding];
    char why[600];
    double t0 = GetTime();
    int failed = figure_from_display(&st->fig, &chain, sp->display, why, sizeof why);
    creature_build_s += GetTime() - t0;
    if (failed) {
        fprintf(stderr, "creatures: %s (%s and any others with its look are left out)\n", why, sp->name);
        if (nbad_display < 512) bad_display[nbad_display++] = sp->display;
        return 1;
    }
    st->fig.scale *= sp->size;
    st->spawn = k;
    moves_init(&st->moves, &st->fig.anim);
    moves_advance(&st->moves, &st->fig.anim, (float)(sp->guid % 4000) / 1000.0f);   /* its own moment */
    built_of[k] = nstanding++;
    return 0;
}
/* free one built creature: the last moves into its place */
static void free_creature(int i)
{
    built_of[standing[i].spawn] = -1;
    figure_free(&standing[i].fig);
    standing[i] = standing[--nstanding];
    if (i < nstanding) built_of[standing[i].spawn] = i;
}
/* {{{ static stream_creatures (173u): the creatures near her are built,
 * the far ones freed. A spawn in a loaded tile within CREATURE_SEEN_REACH
 * + 10 yards is built, nearest first, until `budget_s` seconds are spent
 * (a few each frame, so walking into town never stalls a frame; all at
 * once at the start); a built one beyond CREATURE_SEEN_REACH + 30 yards
 * is freed (the gap keeps one on the edge from being built and freed
 * over and over). Before, every spawn of a tile was built when the tile
 * loaded: about 110 ms a tile, most never seen. */
static void stream_creatures(double budget_s)
{
    if (!spawns.n) return;
    if (!built_of) { built_of = malloc(sizeof *built_of * (size_t)spawns.n); for (int k = 0; k < spawns.n; ++k) built_of[k] = -1; }
    const double near = CREATURE_SEEN_REACH + 10, far = CREATURE_SEEN_REACH + 30;
    for (int i = nstanding - 1; i >= 0; --i) {
        const Spawn *sp = &spawns.spawns[standing[i].spawn];
        if (hypot(sp->x - elf.x, sp->y - elf.y) > far) free_creature(i);
    }
    double t0 = GetTime();
    while (GetTime() - t0 < budget_s) {
        int best = -1, tx, ty;
        double bd = near;
        maps_tile_of((float)elf.x, (float)elf.y, &tx, &ty);
        for (int x = tx - 1; x <= tx + 1; ++x)           /* 110 yards never reach past the next tile */
            for (int y = ty - 1; y <= ty + 1; ++y) {
                if (x < 0 || x > 63 || y < 0 || y > 63 || !tile_state[x * 64 + y]) continue;
                int first, n = creatures_in_tile(&spawns, x, y, &first);
                for (int k = first; k < first + n; ++k) {
                    if (built_of[k] != -1) continue;
                    double d = hypot(spawns.spawns[k].x - elf.x, spawns.spawns[k].y - elf.y);
                    if (d < bd) { bd = d; best = k; }
                }
            }
        if (best < 0) break;
        if (build_creature(best)) built_of[best] = -2;  /* its look can't be built: not tried again */
    }
}
/* }}} */
static void drop_creatures(int tile)
{
    for (int i = nstanding - 1; i >= 0; --i)
        if (spawns.spawns[standing[i].spawn].tile == tile) free_creature(i);
}
/* }}} */

/* {{{ the ground and walls the mover asks about (173q)
 * The floor: the map's ground or a model's floor (bridge, stair, a
 * building's floor), whichever is higher at or below z_top; the map's
 * ground above z_top (she is under it, in a cave or a building's
 * cellar) doesn't count. Walls: the models' steep triangles. Her body for
 * walls: 0.4 yards round, 2.3 tall, stepping over 1 yard (173q's open
 * question: the original's own numbers). */
static CollideWorld *solids;
static float ground_at(void *ctx, float x, float y, float z_top, float *gx, float *gy)
{
    float tx, ty, mx, my;
    float t = maps_world_height((MapWorld *)ctx, x, y, &tx, &ty);
    float m = collide_floor(solids, x, y, z_top, &mx, &my);
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
static LiquidWorld *waters;          /* every loaded tile's water (173r) */
static float water_at(void *ctx, double x, double y)
{
    (void)ctx;
    int kind;
    return liquid_surface(waters, x, y, &kind);
}
static int wall_at(void *ctx, double x0, double y0, double x1, double y1, double z, float *nx, float *ny)
{
    (void)ctx;
    return collide_wall(solids, x0, y0, x1, y1, z, 0.4f, (float)MOVER_STEP_UP, ELF_HEIGHT, (float)elf.max_grad, nx, ny);
}
/* }}} */

/* tiles loaded round hers: enough to cover the view distance (farclip,
 * 173b6), 1 to 3 (3 x 3 up to 7 x 7) */
static int tile_reach(void)
{
    int r = (int)ceil(settings_num("farclip") / 533.33333);
    return r < 1 ? 1 : r > 3 ? 3 : r;
}
static int tile_loaded(int x, int y) { return tile_state[x * 64 + y] == 1; }   /* for the horizon (173s) */
static double tile_load_most_s, tile_load_total_s;   /* the slowest tile load and their sum, for the picture report (173u) */
static double walk_load_most_s, walk_load_total_s;   /* the same for loads after the start (while she moves) */
static int    walk_loads, started_walking;
static double walk_ground_s, walk_doodads_s;            /* of the walking loads' time: the ground's, the doodads' (173u) */
static int    read_ahead = 1;                            /* --no-read-ahead: off, to measure what it saves */
static void refresh_tiles(void)
{
    const int TILE_REACH = tile_reach();
    int tx, ty;
    maps_tile_of((float)elf.x, (float)elf.y, &tx, &ty);
    for (int dx = -TILE_REACH; dx <= TILE_REACH; ++dx)
        for (int dy = -TILE_REACH; dy <= TILE_REACH; ++dy) {
            int x = tx + dx, y = ty + dy;
            if (x < 0 || x >= 64 || y < 0 || y >= 64 || tile_state[x * 64 + y]) continue;
            char why[600];
            AdtTile *adt;
            double load_t0 = GetTime();
            /* the ground: prepared ahead by the helper thread when it got
             * there first (then only sent to the card here), else prepared
             * here (the start's tiles, a jump, or the helper turned off) */
            GroundPrep *prep;
            int got = prefetch_take_ground(x, y, &prep);
            if (got == 2) got = ground_prep_tile(&ground_place, x, y, ground_on_card, &prep, why, sizeof why);
            if (got == 0) got = ground_load_tile(prep, &adt, why, sizeof why);
            if (got < 0) { fprintf(stderr, "painting tile (%d, %d): %s\n", x, y, why); exit(1); }
            double ground_t1 = GetTime();
            if (started_walking) walk_ground_s += ground_t1 - load_t0;
            if (got == 0) {
                liquid_add_tile(waters, x, y, adt);
                water_add_tile(adt, x * 64 + y, ox, oy);
                if (doodads_add_tile(adt, x * 64 + y, ox, oy, why, sizeof why)) { fprintf(stderr, "doodads on tile (%d, %d): %s\n", x, y, why); exit(1); }
                if (started_walking) walk_doodads_s += GetTime() - ground_t1;
                adt_free(adt);
                free(adt);
            }
            tile_state[x * 64 + y] = got == 0 ? 1 : 2;
            ++tiles_loaded;
            double took = GetTime() - load_t0;
            tile_load_total_s += took;
            if (took > tile_load_most_s) tile_load_most_s = took;
            if (started_walking) { ++walk_loads; walk_load_total_s += took; if (took > walk_load_most_s) walk_load_most_s = took; }
        }
    /* the ring just beyond the loaded square: read ahead, so walking into
     * it finds its files unpacked (173u) */
    for (int dx = -TILE_REACH - 1; dx <= TILE_REACH + 1; ++dx)
        for (int dy = -TILE_REACH - 1; dy <= TILE_REACH + 1; ++dy)
            if (abs(dx) == TILE_REACH + 1 || abs(dy) == TILE_REACH + 1) prefetch_tile(tx + dx, ty + dy);
    prefetch_keep_near(tx, ty, TILE_REACH + 1);          /* preparations she turned away from are let go */
    /* dropping (173s): tiles more than a tile beyond the loaded square go,
     * with all they brought; the margin keeps a walk back and forth over
     * an edge from loading and dropping the same tile again and again */
    for (int x = 0; x < 64; ++x)
        for (int y = 0; y < 64; ++y) {
            if (!tile_state[x * 64 + y] || (abs(x - tx) <= TILE_REACH + 1 && abs(y - ty) <= TILE_REACH + 1)) continue;
            drop_creatures(x * 64 + y);
            if (tile_state[x * 64 + y] == 1) {
                ground_drop_tile(x, y);
                water_drop_tile(x * 64 + y);
                liquid_drop_tile(waters, x, y);
                doodads_drop_tile(x * 64 + y);
            }
            tile_state[x * 64 + y] = 0;
            ++tiles_dropped;
        }
}
/* }}} */

/* {{{ main */
int main(int argc, char **argv)
{
    const char *maps = NULL, *client = NULL, *shot = NULL, *settings_file = NULL;
    int pose_id = -1;                                    /* --pose: hold her in this animation */
    double start_ms = 0;                                 /* --clock: the world's animations start this far in */
    double face_deg = 0;                                 /* --face: her start facing turned this far (left) */
    float  walk_s = 0;                                   /* --walk: hold forward this many world-seconds (pictures) */
    int    walking = 0;                                  /* walk toggled (173i): the keypad's /, or --walking */
    float  dive_s = 0;                                   /* --dive: then hold X (swim down) this many world-seconds */
    float  wait_s = 0;                                   /* --wait: then this many world-seconds more before the picture */
    int    go_to = 0;                                    /* --goto: one world-second in, she is moved to (goto_x, goto_y) */
    double goto_x = 0, goto_y = 0;
    /* the time of day: the original follows the realm's clock, which is
     * real time; until the client talks to a server, this machine's own
     * (--hour H for pictures) */
    time_t now = time(NULL);
    struct tm *lt = localtime(&now);
    float hours = (float)lt->tm_hour + (float)lt->tm_min / 60.0f;
    float pose_ms = 0;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--maps") && i + 1 < argc) maps = argv[++i];
        else if (!strcmp(argv[i], "--settings") && i + 1 < argc) settings_file = argv[++i];
        else if (!strcmp(argv[i], "--world") && i + 1 < argc) world_dir = argv[++i];
        else if (!strcmp(argv[i], "--no-read-ahead")) read_ahead = 0;
        else if (!strcmp(argv[i], "--client") && i + 1 < argc) client = argv[++i];
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) shot = argv[++i];
        else if (!strcmp(argv[i], "--zoom") && i + 1 < argc) view.dist = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--front")) { view.yaw = (float)(START_O + PI); view.pitch = 0.1f; cam_held = 1; }
        else if (!strcmp(argv[i], "--turn") && i + 1 < argc) { view.yaw = (float)(START_O + atof(argv[++i]) * DEG2RAD); cam_held = 1; }
        else if (!strcmp(argv[i], "--pitch") && i + 1 < argc) view.pitch = (float)(atof(argv[++i]) * DEG2RAD);
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
            if (wear_option(argv[++i], bad, sizeof bad)) {
                fprintf(stderr, "--wear: \"%s\" isn't slot:look (slots: shirt legs feet wrists chest hands tabard waist head shoulders back mainhand offhand)\n", bad);
                return 2;
            }
        }
        else if (!strcmp(argv[i], "--map") && i + 3 < argc) { map_id = atoi(argv[++i]); start_x = atof(argv[++i]); start_y = atof(argv[++i]); }
        else if (!strcmp(argv[i], "--goto") && i + 2 < argc) { goto_x = atof(argv[++i]); goto_y = atof(argv[++i]); go_to = 1; }
    }
    if (!maps || !client) { fprintf(stderr, "usage: custom-client --maps DIR --client DIR [--settings FILE] [--world DIR] [--shot FILE] [--zoom N] [--front] [--turn DEG] [--pitch DEG] [--pose ID MS] [--clock MS] [--hour H] [--face DEG] [--walk S] [--walking] [--dive S] [--wait S] [--wear SLOT:LOOK,...] [--goto X Y] [--map ID X Y]\n"); return 2; }

    /* the settings: defaults, then the player's file; a line it refuses is
     * named on the terminal and skipped (173b1) */
    settings_init();
    if (settings_file) settings_load(settings_file, stderr);
    else fprintf(stderr, "settings: no --settings file given; every setting is at its default\n");
    int win_w, win_h;
    settings_resolution(settings_text("gxResolution"), &win_w, &win_h);   /* accepted when set, so always a resolution */

    char why[900];
    maps_world_init(&world, maps, map_id);
    float gx, gy;
    float z = maps_world_height(&world, (float)start_x, (float)start_y, &gx, &gy);
    if (z < -10000) { fprintf(stderr, "no ground at the start (%.1f, %.1f) in map %d from %s: is the maps folder right?\n", start_x, start_y, map_id, maps); return 1; }
    /* the steepest walkable ground, from the server's navigation tiles
     * (data-files/mmaps beside data-files/maps) */
    char mmaps[600];
    snprintf(mmaps, sizeof mmaps, "%s", maps);
    char *cut = strstr(mmaps, "/maps");
    if (!cut) { fprintf(stderr, "the maps folder %s doesn't end in /maps; can't find mmaps beside it\n", maps); return 1; }
    strcpy(cut, "/mmaps");
    float max_slope = navmesh_slope_read(mmaps, map_id, why, sizeof why);
    if (max_slope < 0) { fprintf(stderr, "%s\n", why); return 1; }
    mover_init(&elf, start_x, start_y, z, START_O + face_deg * PI / 180, SPEED, max_slope);
    view.yaw = (float)elf.o + (view.yaw - (float)START_O);   /* the camera keeps its place behind her */
    ox = floor(start_x); oy = floor(start_y);

    /* the window as the Resolution panel says (173b5): fullscreen at the
     * resolution unless windowed; maximized means also borderless; vsync
     * and resizing as set (both also follow later changes, below) */
    int vsync_on = settings_num("gxVSync") != 0, resize_locked = settings_num("windowResizeLock") != 0;
    int windowed = settings_num("gxWindow") != 0, maximized = windowed && settings_num("gxMaximize") != 0;
    SetConfigFlags((vsync_on ? FLAG_VSYNC_HINT : 0) | (resize_locked ? 0 : FLAG_WINDOW_RESIZABLE)
                   | (windowed ? 0 : FLAG_FULLSCREEN_MODE) | (maximized ? FLAG_WINDOW_MAXIMIZED | FLAG_WINDOW_UNDECORATED : 0));
    InitWindow(win_w, win_h, "Everland Ghostsong");
    texture_filtering((int)settings_num("textureFilteringMode"));   /* before any picture is sent (173b6) */
    SetExitKey(KEY_NULL);                                /* Escape is read in the loop, so it can open the game menu later */
    Shader model_sh  = LoadShaderFromMemory(FIGURE_VS, MODEL_FS);
    model_sh.locs[SHADER_LOC_MAP_METALNESS] = GetShaderLocation(model_sh, "bones");   /* 041-figure.c puts each figure's bone picture in that material slot */
    if (mpq_chain_open(&chain, client, "enUS", why, sizeof why)) { fprintf(stderr, "%s\n", why); CloseWindow(); return 1; }
    /* her clothes: a fresh character's of her race and class, then any --wear on top */
    if (character_start_outfit(&chain, LOOK.race, HER_CLASS, LOOK.sex, LOOK.items, why, sizeof why)) { fprintf(stderr, "%s\n", why); CloseWindow(); return 1; }
    for (int k = 0; k < SLOT_COUNT; ++k) if (WEAR_OVER[k]) LOOK.items[k] = WEAR_OVER[k];
    if (figure_from_character(&her, &chain, &LOOK, why, sizeof why)) { fprintf(stderr, "%s\n", why); CloseWindow(); return 1; }
    static const int NEEDED[] = { MOVE_STAND, MOVE_RUN, MOVE_WALK, MOVE_WALK_BACK, MOVE_SHUFFLE_LEFT, MOVE_SHUFFLE_RIGHT, MOVE_JUMP_START, MOVE_JUMP, MOVE_JUMP_END, MOVE_FALL };
    for (unsigned i = 0; i < sizeof NEEDED / sizeof NEEDED[0]; ++i)
        if (anim_find(&her.anim, NEEDED[i], 0) < 0) fprintf(stderr, "her animations: her model has no animation %d; she stands instead\n", NEEDED[i]);
    /* the bystanders: each stands round her start, facing her; one that
     * can't be built is named and left out */
    for (int i = 0; i < NBYSTANDERS; ++i) {
        Figure *f = &others[nothers];
        CharacterLook look = { BYSTANDERS[i].race, BYSTANDERS[i].sex, 0, 0, 0, 0, { 0 }, BYSTANDERS[i].facial_hair, 0 };
        if (BYSTANDERS[i].race && character_start_outfit(&chain, look.race, BYSTANDERS[i].class_id, look.sex, look.items, why, sizeof why)) {
            fprintf(stderr, "bystander %d left out: %s\n", i, why);
            continue;
        }
        int bad = BYSTANDERS[i].race ? figure_from_character(f, &chain, &look, why, sizeof why)
                                     : figure_from_display(f, &chain, BYSTANDERS[i].display, why, sizeof why);
        if (bad) { fprintf(stderr, "bystander %d left out: %s\n", i, why); continue; }
        double fx = cos(START_O), fy = sin(START_O);
        other_at[nothers].x = start_x + fx * BYSTANDERS[i].ahead - fy * BYSTANDERS[i].left;
        other_at[nothers].y = start_y + fy * BYSTANDERS[i].ahead + fx * BYSTANDERS[i].left;
        other_at[nothers].z = maps_world_height(&world, (float)other_at[nothers].x, (float)other_at[nothers].y, &gx, &gy);
        other_at[nothers].facing = (float)atan2(start_y - other_at[nothers].y, start_x - other_at[nothers].x);
        moves_init(&other_at[nothers].moves, &f->anim);
        other_at[nothers].cycle = 0;
        other_at[nothers].held = (float)(i * 900);        /* staggered, so they don't change together */
        printf("bystander: %s\n", f->name);
        ++nothers;
    }
    {
        size_t len;
        const char *from;
        unsigned char *mt = mpq_chain_read(&chain, "DBFilesClient\\Map.dbc", &len, &from);
        Dbc maps_table;
        if (!mt || dbc_open(&maps_table, mt, len, why, sizeof why)) { fprintf(stderr, "Map.dbc: %s\n", mt ? why : "not in the archives"); CloseWindow(); return 1; }
        int found = 0;
        for (uint32_t r = 0; r < maps_table.rows && !found; ++r)
            if ((int)dbc_u32(&maps_table, r, 1) == map_id) { snprintf(map_name, sizeof map_name, "%s", dbc_text(&maps_table, r, 2)); found = 1; }
        free(mt);
        if (!found) { fprintf(stderr, "no map %d in Map.dbc\n", map_id); CloseWindow(); return 1; }
    }
    if (ground_place_init(&ground_place, &chain, map_name, &world, ox, oy, why, sizeof why) || ground_init(&ground_place, why, sizeof why)) {
        fprintf(stderr, "%s\n", why); CloseWindow(); return 1;
    }
    if (read_ahead) {                                    /* reads tiles ahead of her on a second thread, and prepares their ground (173u) */
        prefetch_start(&chain, map_name);
        prefetch_prepare_ground(&ground_place, ground_on_card);
    }
    solids = collide_new(ox, oy);
    waters = liquid_new();
    if (doodads_init(&chain, solids, waters, why, sizeof why)) { fprintf(stderr, "%s\n", why); CloseWindow(); return 1; }
    if (horizon_init(&chain, map_name, ox, oy, why, sizeof why)) printf("horizon: %s\n", why);
    if (water_init(&chain, why, sizeof why)) { fprintf(stderr, "%s\n", why); CloseWindow(); return 1; }
    LightTables lights;
    if (light_open(&lights, &chain, why, sizeof why)) { fprintf(stderr, "%s\n", why); CloseWindow(); return 1; }
    sky_init();
    LightNow light;
    int light_id = light_at(&lights, map_id, (float)elf.x, (float)elf.y, (float)elf.z, hours, LIGHT_CLEAR, &light);
    if (light_id < 0) printf("light: map %d has no lights in Light.dbc; a plain grey day is used\n", map_id);
    else printf("light: %02d:%02d, light %d, fog from %.0f to %.0f yards\n", (int)hours, (int)(fmodf(hours, 1.0f) * 60), light_id, light.fog_start, light.fog_end);
    int eye_m = GetShaderLocation(model_sh, "eye");
    /* the map's creatures (173t): without the generated list, none */
    if (world_dir) {
        char path[600];
        snprintf(path, sizeof path, "%s/creatures-%d.tsv", world_dir, map_id);
        int got = creatures_read(path, &spawns, why, sizeof why);
        if (got < 0) { fprintf(stderr, "creatures: %s\n", why); CloseWindow(); return 1; }
        if (got > 0) fprintf(stderr, "creatures: %s\n", why);
        else printf("creatures: %d spawns on this map in %s\n", spawns.n, path);
    } else fprintf(stderr, "creatures: no --world folder given; running without creatures\n");
    refresh_tiles();
    stream_creatures(60.0);                              /* all those near her at the start */
    printf("creatures: %d standing round her (built in %.2f s)\n", nstanding, creature_build_s);
    int dm, dc, ds, dmov, demit;
    doodads_counts(&dm, &dc, &ds, &dmov, &demit);
    printf("doodads: %d models (%d of them moving, %d with particles), %d placed; %d solid triangles\n", dm, dmov, demit, dc, collide_count(solids));
    if (ds) printf("doodads: %d pieces left out: their picture is chosen by the game at run time (not built yet)\n", ds);

    MovesState moves;
    moves_init(&moves, &her.anim);
    double clock_ms = start_ms;

    int frames = 0, drag_l = 0, drag_r = 0;
    double loop_start = GetTime();                       /* for the picture report's frame time */
    started_walking = 1;                                 /* tile loads from here on happen while she moves */
    while (!WindowShouldClose()) {
        /* {{{ input and movement */
        float dt = shot ? 0.05f : GetFrameTime();          /* pictures: steady 20 frames a world-second */
        if (dt > 0.1f) dt = 0.1f;                        /* a stalled frame doesn't fling her */
        if (IsKeyPressed(KEY_ESCAPE)) break;            /* the owner, 2026-10-06: Escape quits until there are menus (173b2) */
        /* Vertical Sync and Disable Resize, live (173b5): a change to
         * either (from the menu, when it exists) takes effect at once */
        if ((settings_num("gxVSync") != 0) != vsync_on) {
            vsync_on = !vsync_on;
            if (vsync_on) SetWindowState(FLAG_VSYNC_HINT); else ClearWindowState(FLAG_VSYNC_HINT);
        }
        if ((settings_num("windowResizeLock") != 0) != resize_locked) {
            resize_locked = !resize_locked;
            if (resize_locked) ClearWindowState(FLAG_WINDOW_RESIZABLE); else SetWindowState(FLAG_WINDOW_RESIZABLE);
        }
        if (go_to && frames == 20) {                     /* --goto: there, standing on the ground */
            go_to = 0;
            float gx2, gy2;
            float gz = maps_world_height(&world, (float)goto_x, (float)goto_y, &gx2, &gy2);
            if (gz < -10000) { fprintf(stderr, "--goto %.1f %.1f: no ground there in the server's maps\n", goto_x, goto_y); break; }
            elf.x = goto_x; elf.y = goto_y; elf.z = gz; elf.grounded = 1; elf.swimming = 0;
        }
        /* the mouse: left drag looks around her, right drag turns her too (031-camera.c) */
        int was_r = drag_r;
        drag_l = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        drag_r = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        Vector2 d = GetMouseDelta();
        FollowCamInput mouse = { .left = drag_l, .right = drag_r, .dx = d.x, .dy = d.y, .dt = dt };
        camera_drag(&view, &mouse, was_r, &elf.o);
        float wheel = GetMouseWheelMove();
        float zoom_max = ZOOM_BASE * (float)settings_num("cameraDistanceMaxFactor");
        camera_zoom(&view, wheel, zoom_max);            /* in past the nearest is first person (173m) */
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
            .pitch = -view.pitch,                        /* where the camera looks, up positive */
        };
        double before_o = elf.o, before_x = elf.x, before_y = elf.y;
        MoverWorld mw = { ground_at, wall_at, water_at, &world };
        TIMED(PH_MOVE, mover_step(&elf, &in, dt, &mw));
        /* moving = she moved along the ground this frame, by any cause
         * (keys, both buttons, sliding); turning in place isn't moving */
        int moving = hypot(elf.x - before_x, elf.y - before_y) > 1e-4;
        if (!cam_held) {
            camera_follow(&view, &mouse, camera_turn_between((float)before_o, (float)elf.o), moving, elf.o);
            float sgx, sgy;
            ground_at(&world, (float)elf.x, (float)elf.y, (float)elf.z + 1.0f, &sgx, &sgy);
            camera_tilt(&view, &mouse, moving && elf.grounded, (float)(sgx * cos(elf.o) + sgy * sin(elf.o)));   /* Follow Terrain (173m) */
        }

        /* her animation: chosen from what she did this frame, then her
         * points bent by the bones and copied into each piece's mesh */
        int turning = !drag_r && (in.left || in.right);
        MovesInput mi = {
            .forward = in.forward, .back = in.back,
            .strafe_left = in.strafe_left || (drag_r && in.left), .strafe_right = in.strafe_right || (drag_r && in.right),
            .turn_left = turning && in.left, .turn_right = turning && in.right,
            .grounded = elf.grounded || elf.swimming, .rising = elf.vz > 0, .swimming = elf.swimming, .walking = walking,
            .speed = (float)mover_intended_speed(&elf, &in), .dt = dt,   /* the keys' speed, not the distance gone: into a wall she runs on the spot */
        };
        moves_step(&moves, &her.anim, &mi);
        if (pose_id >= 0) {
            int seq = anim_find(&her.anim, pose_id, 0);
            if (seq < 0) { fprintf(stderr, "--pose %d: her model has no such animation\n", pose_id); break; }
            moves.seq = seq; moves.ms = pose_ms; moves.from = -1; moves.once = 0;
        }
        clock_ms += dt * 1000.0;
        TIMED(PH_POSE_HER, figure_bend(&her, &moves, (uint32_t)clock_ms));
        /* the bystanders: each holds an animation for HOLD_MS, then the
         * next of CYCLE its model has */
        for (int i = 0; i < nothers; ++i) {
            other_at[i].held += dt * 1000.0f;
            if (other_at[i].held >= HOLD_MS) {
                other_at[i].held = 0;
                for (int tries = 0; tries < (int)(sizeof CYCLE / sizeof CYCLE[0]); ++tries) {
                    other_at[i].cycle = (other_at[i].cycle + 1) % (int)(sizeof CYCLE / sizeof CYCLE[0]);
                    if (anim_find(&others[i].anim, CYCLE[other_at[i].cycle], 0) >= 0) break;
                }
                moves_play(&other_at[i].moves, &others[i].anim, CYCLE[other_at[i].cycle]);
            }
            moves_advance(&other_at[i].moves, &others[i].anim, dt);
            figure_bend(&others[i], &other_at[i].moves, (uint32_t)clock_ms);
        }
        /* the world's creatures (173t): standing, breathing; only those
         * she could see (CREATURE_SEEN_REACH, drawn below) are bent */
        double creature_t0 = GetTime();
        {
            int posed = 0;
            float reach2 = fminf((float)settings_num("farclip"), CREATURE_SEEN_REACH);
            reach2 *= reach2;
            for (int i = 0; i < nstanding; ++i) {
                const Spawn *sp = &spawns.spawns[standing[i].spawn];
                double dx = sp->x - elf.x, dy = sp->y - elf.y;
                if (dx * dx + dy * dy > reach2) continue;
                ++posed;
                moves_advance(&standing[i].moves, &standing[i].fig.anim, dt);
                figure_bend(&standing[i].fig, &standing[i].moves, (uint32_t)clock_ms);
            }
            last_posed = posed;
        }
        creature_s += GetTime() - creature_t0;
        creature_pose_s += GetTime() - creature_t0;
        refresh_tiles();
        stream_creatures(0.004);                         /* up to 4 ms a frame building creatures coming into sight (173u) */
        SetWindowTitle(TextFormat("Everland Ghostsong  -  Ashenvale  x %.1f  y %.1f  z %.1f", elf.x, elf.y, elf.z));
        /* }}} */

        /* {{{ the camera: behind her by its own heading, raised by its pitch;
         * or, zoomed all the way in, first person: the view from her eyes
         * along the camera's heading, looking down by its pitch, she not
         * drawn (as the original hides you), Head Bob lifting it with her
         * steps (173m). First person skips the third-person keeping-out
         * below: her eyes are where she is, which the mover already keeps
         * out of walls, ground and (unless swimming) water */
        int first_person = view.dist == 0;
        Vector3 at = { (float)(elf.x - ox), (float)(elf.y - oy), (float)elf.z + ELF_HEIGHT * 0.85f };
        Vector3 eye = { at.x - cosf(view.yaw) * cosf(view.pitch) * view.dist,
                        at.y - sinf(view.yaw) * cosf(view.pitch) * view.dist,
                        at.z + sinf(view.pitch) * view.dist };
        Vector3 look = at;                               /* where the camera points */
        {
            float stepped = elf.grounded && !elf.swimming ? (float)hypot(elf.x - before_x, elf.y - before_y) : 0;
            float lift = camera_bob(&bob_phase, stepped, dt);   /* runs in third person too, so the phase carries over */
            if (first_person) {
                eye = (Vector3){ at.x, at.y, (float)elf.z + ELF_HEIGHT * EYE_SHARE + lift };
                look = (Vector3){ eye.x + cosf(view.yaw) * cosf(view.pitch), eye.y + sinf(view.yaw) * cosf(view.pitch), eye.z - sinf(view.pitch) };
            }
        }
        if (!first_person) {
            /* something solid between her head and the camera (a wall, a
             * tree, a roof): the camera comes in to just before it (173m) */
            float in_way = collide_ray(solids, at.x + ox, at.y + oy, at.z, eye.x + ox, eye.y + oy, eye.z);
            if (in_way < 1) {
                float keep = fmaxf(in_way - 0.3f / fmaxf(view.dist, 0.3f), 0.05f);   /* 0.3 yards short of it */
                eye = (Vector3){ at.x + (eye.x - at.x) * keep, at.y + (eye.y - at.y) * keep, at.z + (eye.z - at.z) * keep };
            }
            float floor_z = maps_world_height(&world, eye.x + (float)ox, eye.y + (float)oy, &gx, &gy);
            if (floor_z > -10000 && eye.z < floor_z + 0.6f) {
                /* never under the ground. Smart Pivot on: the camera slides along
                 * the ground and looks up at her; off: it stops lowering, the
                 * pitch held where the camera meets the ground (173m) */
                eye.z = floor_z + 0.6f;
                if (settings_num("cameraPivot") == 0 && view.dist > 0.1f) {
                    float s = (eye.z - at.z) / view.dist;
                    float hold = asinf(s < -1 ? -1 : s > 1 ? 1 : s);
                    if (view.pitch < hold) view.pitch = hold;
                }
            }
            /* Water Collision (173m): the camera stays on her side of the
             * water's surface: above it while her head is above, below while
             * under */
            if (settings_num("cameraWaterCollision") != 0) {
                int wk = 0;
                float surface = liquid_surface(waters, eye.x + ox, eye.y + oy, &wk);
                float head = at.z;
                if (surface > -10000) {
                    if (head >= surface && eye.z < surface + 0.25f) eye.z = surface + 0.25f;
                    else if (head < surface && eye.z > surface - 0.25f) eye.z = surface - 0.25f;
                }
            }
        }
        Camera3D cam = { .position = eye, .target = look, .up = { 0, 0, 1 }, .fovy = 60, .projection = CAMERA_PERSPECTIVE };
        TIMED(PH_DOODADS_ANIMATE, doodads_animate((uint32_t)clock_ms, dt, eye, look, (float)settings_num("particleDensity")));   /* moving doodads, and particles near the camera */
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
        float eye_surface = liquid_surface(waters, eye.x + ox, eye.y + oy, &water_kind);
        int under = eye_surface > -10000 && eye.z < eye_surface;
        int lit = light_at(&lights, map_id, (float)elf.x, (float)elf.y, (float)elf.z, hours, under ? LIGHT_UNDERWATER : LIGHT_CLEAR, &light);
        /* the fog ends by the view distance at the latest (173b6) */
        float farclip = (float)settings_num("farclip");
        if (light.fog_end > farclip) { light.fog_start *= farclip / light.fog_end; light.fog_end = farclip; }
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
        rlMultMatrixf(MatrixToFloat(MatrixPerspective(60 * DEG2RAD, (double)GetScreenWidth() / GetScreenHeight(), 0.3, fmax(settings_num("farclip") * 1.25 + 100, (HORIZON_REACH + 1) * 533.33))));   /* out to the far horizon (173s) */
        rlMatrixMode(RL_MODELVIEW);
        rlDisableBackfaceCulling();
        TIMED(PH_SKY, sky_draw(eye, &light); rlDrawRenderBatchActive());   /* first, behind everything */
        {
            int htx, hty;
            maps_tile_of((float)elf.x, (float)elf.y, &htx, &hty);
            TIMED(PH_SKY, horizon_draw(htx, hty, tile_loaded, eye, &light));   /* low-detail tiles beyond the loaded ones (173s) */
        }
        ground_blob(at.x, at.y, (float)elf.z, 1.3f);     /* her shadow, a little wider than her shoulders */
        for (int i = 0; i < nothers; ++i)                /* theirs, sized by their display's scale */
            ground_blob((float)(other_at[i].x - ox), (float)(other_at[i].y - oy), (float)other_at[i].z, 1.3f * others[i].scale);
        for (int i = 0; i < nstanding; ++i) {            /* the world's creatures' (173t) */
            const Spawn *sp = &spawns.spawns[standing[i].spawn];
            ground_blob((float)(sp->x - ox), (float)(sp->y - oy), (float)sp->z, 1.3f * standing[i].fig.scale);
        }
        TIMED(PH_GROUND, ground_draw(eye, &light); rlDrawRenderBatchActive());
        TIMED(PH_DOODADS_DRAW, doodads_draw(eye, &light); rlDrawRenderBatchActive());
        TIMED(PH_WATER, water_draw(eye, &light, clock_ms / 1000.0); rlDrawRenderBatchActive());   /* see-through, after the solid world */
        double figures_t0 = GetTime();
        Matrix place = MatrixMultiply(MatrixRotateZ((float)elf.o + moves.body_turn), MatrixTranslate(at.x, at.y, (float)elf.z));
        if (!first_person) figure_draw(&her, model_sh, place);   /* first person: she isn't drawn (173m) */
        for (int i = 0; i < nothers; ++i)
            figure_draw(&others[i], model_sh, MatrixMultiply(MatrixRotateZ(other_at[i].facing),
                        MatrixTranslate((float)(other_at[i].x - ox), (float)(other_at[i].y - oy), (float)other_at[i].z)));
        creature_t0 = GetTime();
        int creatures_drawn = 0;
        for (int i = 0; i < nstanding; ++i) {            /* the world's creatures (173t): within sight, and not behind the camera */
            const Spawn *sp = &spawns.spawns[standing[i].spawn];
            double dx = sp->x - elf.x, dy = sp->y - elf.y, far = fmin(settings_num("farclip"), CREATURE_SEEN_REACH);
            if (dx * dx + dy * dy > far * far) continue;
            Vector3 to = { (float)(sp->x - ox) - eye.x, (float)(sp->y - oy) - eye.y, (float)sp->z - eye.z }, ahead = Vector3Subtract(look, eye);
            if (Vector3DotProduct(to, Vector3Normalize(ahead)) < -4.0f * standing[i].fig.scale) continue;   /* wholly behind: 4 yards covers the biggest townsfolk */
            ++creatures_drawn;
            figure_draw(&standing[i].fig, model_sh, MatrixMultiply(MatrixRotateZ(sp->facing), MatrixTranslate((float)(sp->x - ox), (float)(sp->y - oy), (float)sp->z)));
        }
        rlDrawRenderBatchActive();
        creature_s += GetTime() - creature_t0;
        last_drawn = creatures_drawn;
        rlDrawRenderBatchActive();
        phase_s[PH_FIGURES_DRAW] += GetTime() - figures_t0;
        EndMode3D();
        TIMED(PH_FINISH, EndDrawing());

        /* --shot: after 3 world-seconds, the picture, then quit */
        if (shot && ++frames == 60 + (int)((walk_s + dive_s + wait_s) * 20)) {   /* 3 world-seconds after any walk: particles have filled out */
            Image im = LoadImageFromScreen();
            int ok = ExportImage(im, shot);
            UnloadImage(im);
            printf(ok ? "picture saved: %s\n" : "could not save the picture to %s\n", shot);
            int dm2, dc2, ds2, dmov2, demit2;
            doodads_counts(&dm2, &dc2, &ds2, &dmov2, &demit2);
            int ahead_tiles, ahead_files, grounds_made, grounds_taken;
            prefetch_counts(&ahead_tiles, &ahead_files, &grounds_made, &grounds_taken);
            printf("unpacking files from the archives: %.0f ms over the run, both threads (%llu read, %llu from memory); read ahead: %d tiles, %d files\n",
                   chain.unpack_s * 1000, (unsigned long long)chain.misses, (unsigned long long)chain.hits, ahead_tiles, ahead_files);
            printf("ground prepared ahead: %d tiles, %d of them loaded\n", grounds_made, grounds_taken);
            printf("tile loads: %.0f ms the slowest, %.0f ms on average (each stalls the frame it happens in; 173u)\n", tile_load_most_s * 1000, tiles_loaded ? tile_load_total_s * 1000 / tiles_loaded : 0.0);
            printf("  of those, while she moved: %d, %.0f ms the slowest, %.0f ms on average\n", walk_loads, walk_load_most_s * 1000, walk_loads ? walk_load_total_s * 1000 / walk_loads : 0.0);
            if (walk_loads) printf("  of their average, the ground %.0f ms, water and doodads %.0f ms\n", walk_ground_s * 1000 / walk_loads, walk_doodads_s * 1000 / walk_loads);
            printf("tiles: %d loaded, %d dropped over the run; now %d placed, %d solid triangles\n", tiles_loaded, tiles_dropped, dc2, collide_count(solids));
            printf("frames took %.1f ms each on average, over %d (drawing, bending and all)\n", (GetTime() - loop_start) * 1000.0 / frames, frames);
            for (int ph = 0; ph < PH_COUNT; ++ph) printf("  %5.2f ms a frame: %s\n", phase_s[ph] * 1000.0 / frames, PHASE_NAME[ph]);
            printf("the ground sent %.1f draw orders a frame, the doodads %.1f (173u)\n", (double)ground_orders_last() / frames, (double)doodads_orders_last() / frames);
            size_t tex_sent, tex_unpacked;
            texture_bytes(&tex_sent, &tex_unpacked);
            printf("model pictures on the card: %.1f MB (%.1f MB if unpacked; 173u)\n", tex_sent / 1048576.0, tex_unpacked / 1048576.0);
            printf("creatures took %.2f ms a frame, %.2f of it posing (%d posed, %d drawn, of %d standing)\n", creature_s * 1000.0 / frames, creature_pose_s * 1000.0 / frames, last_posed, last_drawn, nstanding);
            printf("she ended at x %.2f y %.2f z %.2f, %s\n", elf.x, elf.y, elf.z, elf.swimming ? "swimming" : elf.grounded ? "on the ground" : "in the air");
            /* the creatures nearest her (173t), to check pictures against */
            for (int pick = 0, used[5] = { -1, -1, -1, -1, -1 }; pick < 5 && pick < nstanding; ++pick) {
                int best = -1;
                double bd = 1e30;
                for (int i = 0; i < nstanding; ++i) {
                    int taken = 0;
                    for (int u = 0; u < pick; ++u) taken |= used[u] == i;
                    const Spawn *sp = &spawns.spawns[standing[i].spawn];
                    double d = hypot(sp->x - elf.x, sp->y - elf.y);
                    if (!taken && d < bd) { bd = d; best = i; }
                }
                used[pick] = best;
                const Spawn *sp = &spawns.spawns[standing[best].spawn];
                printf("near her: %s (%s) at x %.1f y %.1f z %.1f, %.1f yards off\n", sp->name, standing[best].fig.name, sp->x, sp->y, sp->z, bd);
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
        if (!strcmp(size, settings_text("gxResolution"))) {
            /* not resized: nothing to keep */
        } else if (settings_set("gxResolution", size, why, sizeof why)) {
            fprintf(stderr, "settings: the window's size isn't kept: %s\n", why);   /* smaller than 640x480 */
        } else {
            if (!settings_file) printf("settings: the window is now %s, but no --settings file was given to keep it in\n", size);
            else if (settings_save(settings_file, why, sizeof why)) fprintf(stderr, "settings: the window's size %s could not be saved: %s\n", size, why);
            else printf("settings: the window's size %s saved to %s\n", size, settings_file);
        }
    }
    CloseWindow();
    light_close(&lights);
    collide_free(solids);
    liquid_free(waters);
    figure_free(&her);
    for (int i = 0; i < nothers; ++i) figure_free(&others[i]);
    for (int i = 0; i < nstanding; ++i) figure_free(&standing[i].fig);
    free(standing);
    free(built_of);
    creatures_free(&spawns);
    prefetch_stop();
    mpq_chain_close(&chain);
    maps_world_free(&world);
    return 0;
}
/* }}} */
