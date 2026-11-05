/*
 * 089-headless-run.c (issue 1109, step 1n) - see 089-headless-run.h
 *
 * For a general audience: the moves file is read into a list of moves,
 * each a set of keys held for a whole number of steps. The run opens the
 * world as the windowed client does, finds her figure and the
 * creatures' looks as it would, then steps the mover with a fixed step,
 * reading each tile's ground file as she nears it for its water and
 * its models' floors and walls (091), and
 * counts what happened; the counts go into the goodbye receipt.
 */
#include "089-headless-run.h"
#include "020-adt.h"
#include "022-m2.h"
#include "023-anim.h"
#include "025-character.h"
#include "027-liquid.h"
#include "029-mover.h"
#include "030-moves.h"
#include "035-creatures.h"
#include "059-loose-folders.h"
#include "061-mdx.h"
#include "088-figure-model.h"
#include "091-model-solids.h"
#include "098-wdt.h"
#include "026-light.h"
#include "084-map-fog.h"
#include "014-settings.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* {{{ the moves */
enum { HOLD_FORWARD, HOLD_BACK, HOLD_LEFT, HOLD_RIGHT, HOLD_STRAFE_LEFT, HOLD_STRAFE_RIGHT, HOLD_JUMP, HOLD_SWIM_DOWN, HOLD_COUNT };
static const char *const HOLDS[HOLD_COUNT] = { "forward", "back", "left", "right", "strafe_left", "strafe_right", "jump", "swim_down" };

typedef struct { unsigned keys; int walking, steps, turns; double turn_to; int has_weather; WeatherMsg weather; } Move;   /* turns: 1 when she faces turn_to first */
struct HeadlessMoves {
    char   name[120];
    char   map[64];                     /* the map's folder the moves are for; "" any */
    double step;
    int    nmoves, steps;
    Move  *moves;
};

int    headless_moves_count(const HeadlessMoves *m) { return m->nmoves; }
int    headless_moves_steps(const HeadlessMoves *m) { return m->steps; }
double headless_moves_step(const HeadlessMoves *m)  { return m->step; }
const char *headless_moves_map(const HeadlessMoves *m) { return m->map; }
void   headless_moves_free(HeadlessMoves *m) { if (m) { free(m->moves); free(m); } }

/* the keys named in `text` (words with spaces between), or a refusal naming the word */
static WErr read_hold(const char *text, unsigned *keys, const char *where, int line, WWhy *why)
{
    *keys = 0;
    const char *p = text;
    while (*p) {
        while (*p == ' ') ++p;
        if (!*p) break;
        size_t n = strcspn(p, " ");
        int k = 0;
        while (k < HOLD_COUNT && !(strlen(HOLDS[k]) == n && !strncmp(p, HOLDS[k], n))) ++k;
        if (k == HOLD_COUNT)
            return w_fail(why, W_ERR_REFUSED, "%s line %d: \"%.*s\" isn't a key a move holds (forward, back, left, right, strafe_left, strafe_right, jump, swim_down)",
                          where, line, (int)(n > 40 ? 40 : n), p);
        *keys |= 1u << k;
        p += n;
    }
    return W_OK;
}

/* a move's weather: { state = 0 to 65535, whole, grade = 0 to 1, abrupt = true or false } (1109 step 1v) */
static WErr read_weather(const LuaValue *v, WeatherMsg *w, const char *where, int line, int i, WWhy *why)
{
    if (lua_value_type(v) != LUA_DATA_TABLE)
        return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's weather must be a table ({ state = ..., grade = ..., abrupt = ... })", where, line, i);
    int have_state = 0, have_grade = 0;
    memset(w, 0, sizeof *w);
    for (int k = 1; k <= lua_value_count(v); ++k) {
        const char *key;
        const LuaValue *f = lua_value_at(v, k, &key);
        int fl = lua_value_line(f);
        double n = lua_value_number(f);
        if (key && !strcmp(key, "state")) {
            if (lua_value_type(f) != LUA_DATA_NUMBER || !(n >= 0 && n <= 65535) || n != floor(n))
                return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's weather state must be a whole number from 0 (fine) to 65535", where, fl, i);
            w->state = (uint32_t)n;
            have_state = 1;
        } else if (key && !strcmp(key, "grade")) {
            if (lua_value_type(f) != LUA_DATA_NUMBER || !(n >= 0 && n <= 1))
                return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's weather grade must be a number from 0 to 1", where, fl, i);
            w->grade = (float)n;
            have_grade = 1;
        } else if (key && !strcmp(key, "abrupt")) {
            if (lua_value_type(f) != LUA_DATA_BOOL) return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's weather abrupt must be true or false", where, fl, i);
            w->abrupt = lua_value_true(f);
        } else return w_fail(why, W_ERR_REFUSED, "%s line %d: %.40s isn't a field of a move's weather (state, grade, abrupt)", where, fl, key ? key : "a value with no name");
    }
    if (!have_state || !have_grade) return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's weather needs state and grade", where, line, i);
    return W_OK;
}

static WErr parse(const LuaValue *top, const char *where, HeadlessMoves *m, WWhy *why)
{
    if (lua_value_type(top) != LUA_DATA_TABLE) return w_fail(why, W_ERR_REFUSED, "%s: the moves must be a table; it returns %s", where, lua_type_name(lua_value_type(top)));
    const LuaValue *moves = NULL;
    int have_format = 0;
    m->step = HEADLESS_STEP_DEFAULT;
    for (int i = 1; i <= lua_value_count(top); ++i) {
        const char *key;
        const LuaValue *v = lua_value_at(top, i, &key);
        int line = lua_value_line(v);
        if (!key) return w_fail(why, W_ERR_REFUSED, "%s line %d: a value with no name; the moves file has format, name, map, step and moves", where, line);
        if (!strcmp(key, "format")) {
            if (lua_value_type(v) != LUA_DATA_NUMBER || lua_value_number(v) != HEADLESS_MOVES_FORMAT)
                return w_fail(why, W_ERR_REFUSED, "%s line %d: format must be %d (this client reads that one)", where, line, HEADLESS_MOVES_FORMAT);
            have_format = 1;
        } else if (!strcmp(key, "name")) {
            if (lua_value_type(v) != LUA_DATA_TEXT) return w_fail(why, W_ERR_REFUSED, "%s line %d: name must be text", where, line);
            snprintf(m->name, sizeof m->name, "%s", lua_value_text(v));
        } else if (!strcmp(key, "map")) {
            const char *t = lua_value_type(v) == LUA_DATA_TEXT ? lua_value_text(v) : NULL;
            if (!t || !*t || strlen(t) >= sizeof m->map || strpbrk(t, "/\\"))
                return w_fail(why, W_ERR_REFUSED, "%s line %d: map must be the map's folder name (World\\Maps\\<name>), 1 to %d characters", where, line, (int)sizeof m->map - 1);
            snprintf(m->map, sizeof m->map, "%s", t);
        } else if (!strcmp(key, "step")) {
            double s = lua_value_number(v);
            if (lua_value_type(v) != LUA_DATA_NUMBER || !(s >= 0.01 && s <= 0.1))
                return w_fail(why, W_ERR_REFUSED, "%s line %d: step must be a number of seconds from 0.01 to 0.1", where, line);
            m->step = s;
        } else if (!strcmp(key, "moves")) {
            if (lua_value_type(v) != LUA_DATA_TABLE) return w_fail(why, W_ERR_REFUSED, "%s line %d: moves must be a list of moves", where, line);
            moves = v;
        } else return w_fail(why, W_ERR_REFUSED, "%s line %d: %.40s isn't a field of the moves file (format, name, map, step, moves)", where, line, key);
    }
    if (!have_format) return w_fail(why, W_ERR_REFUSED, "%s: no format; the moves file says format = %d", where, HEADLESS_MOVES_FORMAT);
    if (!moves) return w_fail(why, W_ERR_REFUSED, "%s: no moves", where);
    int n = lua_value_count(moves);
    if (n != lua_value_listed(moves)) return w_fail(why, W_ERR_REFUSED, "%s line %d: moves is a list, with no names", where, lua_value_line(moves));
    if (n < 1 || n > HEADLESS_MOVES_MAX) return w_fail(why, W_ERR_REFUSED, "%s line %d: %d moves; from 1 to %d", where, lua_value_line(moves), n, HEADLESS_MOVES_MAX);
    m->moves = calloc((size_t)n, sizeof *m->moves);
    if (!m->moves) return w_fail(why, W_ERR_FULL, "no memory for %d moves", n);
    for (int i = 1; i <= n; ++i) {
        const LuaValue *mv = lua_value_item(moves, i);
        int line = lua_value_line(mv);
        if (lua_value_type(mv) != LUA_DATA_TABLE) return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d must be a table ({ hold = ..., seconds = ... })", where, line, i);
        Move *o = &m->moves[i - 1];
        int have_hold = 0;
        double seconds = -1;
        for (int k = 1; k <= lua_value_count(mv); ++k) {
            const char *key;
            const LuaValue *v = lua_value_at(mv, k, &key);
            int vl = lua_value_line(v);
            if (!key) return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d holds a value with no name (hold, seconds, walking, turn_to, weather)", where, vl, i);
            if (!strcmp(key, "hold")) {
                if (lua_value_type(v) != LUA_DATA_TEXT) return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's hold must be text (\"\" for none)", where, vl, i);
                WErr rc = read_hold(lua_value_text(v), &o->keys, where, vl, why);
                if (rc) return rc;
                have_hold = 1;
            } else if (!strcmp(key, "seconds")) {
                seconds = lua_value_number(v);
                if (lua_value_type(v) != LUA_DATA_NUMBER || !(seconds > 0 && seconds <= 3600))
                    return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's seconds must be a number above 0, at most 3600", where, vl, i);
            } else if (!strcmp(key, "walking")) {
                if (lua_value_type(v) != LUA_DATA_BOOL) return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's walking must be true or false", where, vl, i);
                o->walking = lua_value_true(v);
            } else if (!strcmp(key, "turn_to")) {
                double a = lua_value_number(v);
                if (lua_value_type(v) != LUA_DATA_NUMBER || !(a >= -HEADLESS_TURN_MOST && a <= HEADLESS_TURN_MOST))
                    return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d's turn_to must be a facing in radians, from -6.2832 to 6.2832 (0 north, rising west)", where, vl, i);
                o->turns = 1;
                o->turn_to = a;
            } else if (!strcmp(key, "weather")) {
                WErr rc = read_weather(v, &o->weather, where, vl, i, why);
                if (rc) return rc;
                o->has_weather = 1;
            } else return w_fail(why, W_ERR_REFUSED, "%s line %d: %.40s isn't a field of a move (hold, seconds, walking, turn_to, weather)", where, vl, key);
        }
        if ((o->turns || o->has_weather) && seconds < 0 && !have_hold) seconds = 0;   /* a turn or the weather alone: no steps */
        if ((!have_hold && seconds != 0) || seconds < 0) return w_fail(why, W_ERR_REFUSED, "%s line %d: move %d needs hold and seconds (or turn_to or weather alone)", where, line, i);
        o->steps = (int)floor(seconds / m->step + 0.5);
        if (o->steps < 1 && seconds > 0) o->steps = 1;
        if (o->steps > HEADLESS_STEPS_MAX - m->steps)
            return w_fail(why, W_ERR_REFUSED, "%s line %d: the moves pass %d steps in all (%.0f seconds at %g a step)", where, line, HEADLESS_STEPS_MAX, HEADLESS_STEPS_MAX * m->step, m->step);
        m->steps += o->steps;
        m->nmoves = i;
    }
    return W_OK;
}

WErr headless_moves_parse(const char *text, size_t len, const char *where, HeadlessMoves **out, WWhy *why)
{
    *out = NULL;
    LuaData *d;
    WErr rc = lua_data_parse(text, len, where, &d, why);
    if (rc) return rc;
    HeadlessMoves *m = calloc(1, sizeof *m);
    if (!m) { lua_data_free(d); return w_fail(why, W_ERR_FULL, "no memory for the moves"); }
    rc = parse(lua_data_top(d), where, m, why);
    lua_data_free(d);
    if (rc) { headless_moves_free(m); return rc; }
    *out = m;
    return W_OK;
}

WErr headless_moves_read(const char *path, HeadlessMoves **out, WWhy *why)
{
    *out = NULL;
    LuaData *d;
    WErr rc = lua_data_read(path, HEADLESS_MOVES_BYTES, &d, why);
    if (rc) return rc;
    HeadlessMoves *m = calloc(1, sizeof *m);
    if (!m) { lua_data_free(d); return w_fail(why, W_ERR_FULL, "no memory for the moves"); }
    rc = parse(lua_data_top(d), path, m, why);
    lua_data_free(d);
    if (rc) { headless_moves_free(m); return rc; }
    *out = m;
    return W_OK;
}
/* }}} */

/* {{{ the world the mover asks: the map's ground, the placed models'
 * floors and walls (091, as the window's 042 places them) and the tiles'
 * water, buildings' pools included */
typedef struct {
    WorldStart  *ws;
    LiquidWorld *waters;
    CollideWorld *solids;
    ModelSolids *models;
    FILE        *log;
    double       ox, oy;                /* the solids' origin: where she starts, as 043's */
    double       max_grad;              /* hers, for the walls */
    Wc3Pathing  *pathing;               /* a converted map's WC3 pathing (094), NULL for none */
    int          walled;                /* the walls this step met: 1 a model's, 2 the pathing's (bits) */
    unsigned char loaded[64 * 64];      /* 1: its ground file read for its water; 2: none there */
    int          big_alpha, tiles_entered, ground_files;
    unsigned char entered[64 * 64];
    int          tiles_known;           /* 1: its WDT's tile list read (098; 1109 step 1s); 0: each file asked for */
    unsigned char listed[64 * 64];      /* 1: the WDT lists the tile */
} Here;

static float ground_at(void *ctx, float x, float y, float z_top, float *gx, float *gy)
{
    Here *h = ctx;
    return model_solids_ground(&h->ws->world, h->solids, x, y, z_top, gx, gy);
}
static int wall_at(void *ctx, double x0, double y0, double x1, double y1, double z, float *nx, float *ny)
{
    Here *h = ctx;
    int hit = model_solids_wall(h->solids, h->pathing, x0, y0, x1, y1, z, (float)h->max_grad, nx, ny);
    h->walled |= hit;
    return hit != 0;
}
static float water_at(void *ctx, double x, double y)
{
    Here *h = ctx;
    int kind;
    return liquid_surface(h->waters, x, y, &kind);
}
/* the tile she is on and those round it: their water and their models'
 * solid shapes filed, once */
static void near_tiles(Here *h, double x, double y)
{
    int tx, ty;
    maps_tile_of((float)x, (float)y, &tx, &ty);
    if (tx < 0 || ty < 0 || tx > 63 || ty > 63) return;
    if (!h->entered[tx * 64 + ty]) { h->entered[tx * 64 + ty] = 1; ++h->tiles_entered; }
    for (int a = tx - 1; a <= tx + 1; ++a)
        for (int b = ty - 1; b <= ty + 1; ++b) {
            if (a < 0 || b < 0 || a > 63 || b > 63 || h->loaded[a * 64 + b]) continue;
            h->loaded[a * 64 + b] = 2;
            if (h->tiles_known && !h->listed[a * 64 + b]) continue;   /* the WDT lists no ground here: open sea, no file asked for */
            char path[300];
            snprintf(path, sizeof path, "World\\Maps\\%s\\%s_%d_%d.adt", h->ws->map_name, h->ws->map_name, b, a);   /* west-east number first */
            AssetAnswer f;
            WWhy w;
            if (asset_read(h->ws->assets, &h->ws->chain, path, ASSET_GROUND, &f, &w)) {   /* open sea */
                if (h->tiles_known) fprintf(h->log, "headless: the map's WDT lists tile %d, %d, but %s is in no place: no ground there\n", a, b, path);
                continue;
            }
            AdtTile *t = calloc(1, sizeof *t);
            if (t && !adt_read(f.data, f.len, h->big_alpha, t, &w)) {
                liquid_add_tile(h->waters, a, b, t);
                if (model_solids_add_tile(h->models, t, a * 64 + b, h->ox, h->oy, &w)) fprintf(h->log, "headless: tile %d, %d's models: %s\n", a, b, w.text);
                h->loaded[a * 64 + b] = 1;
                ++h->ground_files;
                adt_free(t);
            }
            free(t);
            asset_answer_free(&f);
        }
}
/* }}} */

/* {{{ static her: her character (025), else the stand-in figure (088),
 * with its skeleton for her animations */
static WErr her(WorldStart *ws, FILE *log, HeadlessResult *r, Anim *anim, WWhy *why)
{
    WWhy w;
    CharacterLook look = { WORLD_HER_RACE, WORLD_HER_SEX, 0, 0, 0, 0, { 0 }, 0, 0 };
    Character ch;
    WErr rc = character_start_outfit(ws->assets, &ws->chain, look.race, WORLD_HER_CLASS, look.sex, look.items, &w);
    if (!rc) rc = character_build(ws->assets, &ws->chain, &look, &ch, &w);
    if (!rc) {
        r->her_vertices = (int)ch.model.nvertices;
        char name[260];
        snprintf(name, sizeof name, "%s.m2", ch.base);
        character_free(&ch);
        AssetAnswer a;
        WErr ae = asset_read(ws->assets, &ws->chain, name, ASSET_MODEL, &a, &w);
        if (!ae) { ae = anim_read(a.data, a.len, anim, &w); asset_answer_free(&a); }
        if (ae) return w_fail(why, ae, "her skeleton: %s", w.text);
        fprintf(log, "headless: her character (%s)\n", name);
        return W_OK;
    }
    fprintf(log, "headless: her character can't be made (%s); the baseline set's stand-in figure stands in\n", w.text);
    r->her_stand_in = 1;
    FigureModel fm;
    if ((rc = figure_model_stand_in(ws->assets, &ws->chain, "her (the stand-in figure)", &fm, &w)))
        return w_fail(why, rc, "her: no character and no stand-in figure: %s", w.text);
    M2Model m;
    rc = m2_read(fm.model.data, fm.model.len, fm.skin.data, fm.skin.len, &m, &w);
    if (!rc) { r->her_vertices = (int)m.nvertices; m2_free(&m); rc = anim_read(fm.model.data, fm.model.len, anim, &w); }
    figure_model_free(&fm);
    if (rc) return w_fail(why, rc, "her stand-in figure: %s", w.text);
    return W_OK;
}
/* }}} */

/* {{{ static creatures: each distinct look of the map's creature list, as 041 asks */
static WErr creatures(WorldStart *ws, int map_id, const char *world_dir, FILE *log, HeadlessResult *r, WWhy *why)
{
    if (!world_dir || !*world_dir) { fprintf(log, "headless: no creature list's folder: no creatures counted\n"); return W_OK; }
    char path[700];
    snprintf(path, sizeof path, "%s/creatures-%d.tsv", world_dir, map_id);
    SpawnList sl = { 0 };
    WWhy w;
    WErr rc = creatures_read(path, &sl, &w);
    if (rc == W_ERR_NOT_FOUND) { fprintf(log, "headless: %s\n", w.text); return W_OK; }
    if (rc) return w_fail(why, rc, "creatures: %s", w.text);
    r->creature_spawns = sl.n;
    int *seen = calloc((size_t)(sl.n ? sl.n : 1), sizeof *seen);
    if (!seen) { creatures_free(&sl); return w_fail(why, W_ERR_FULL, "no memory for %d creatures' looks", sl.n); }
    for (int i = 0; i < sl.n; ++i) {
        int known = 0;
        for (int q = 0; q < r->creature_looks && !known; ++q) known = seen[q] == sl.spawns[i].display;
        if (known) continue;
        seen[r->creature_looks++] = sl.spawns[i].display;
        FigureModel fm;
        if (figure_model_for_display(ws->assets, &ws->chain, ws->tables, sl.spawns[i].display, &fm, &w)) { ++r->looks_missing; continue; }
        int readable = 0;
        if (fm.kind == FIGURE_MODEL_WC3) {
            ++r->looks_wc3;
            MdxModel x;
            if (!mdx_read(fm.model.data, fm.model.len, &x, &w)) { readable = 1; mdx_free(&x); }
        } else if (fm.kind == FIGURE_MODEL_HUMANOID) { ++r->looks_character; readable = 1; }   /* 025 builds it with the window's 041 */
        else {
            if (fm.kind == FIGURE_MODEL_STAND_IN) ++r->looks_stand_in; else ++r->looks_game;
            r->looks_placeholder += fm.placeholder;
            M2Model m;
            Anim a;
            if (!m2_read(fm.model.data, fm.model.len, fm.skin.data, fm.skin.len, &m, &w)) {
                if (!anim_read(fm.model.data, fm.model.len, &a, &w)) { readable = 1; anim_free(&a); }
                m2_free(&m);
            }
        }
        if (!readable) ++r->looks_unreadable;
        figure_model_free(&fm);
    }
    free(seen);
    creatures_free(&sl);
    return W_OK;
}
/* }}} */

/* {{{ static counted: the result into the receipt */
static WErr counted(Goodbye *bye, const HeadlessResult *r, WWhy *why)
{
    struct { const char *name; double value; } c[] = {
        { "moves", r->moves }, { "steps", r->steps },
        { "her_stand_in", r->her_stand_in }, { "her_vertices", r->her_vertices }, { "her_bones", r->her_bones },
        { "her_sequences", r->her_sequences }, { "her_animation_changes", r->her_animation_changes }, { "her_animation", r->her_animation },
        { "creature_spawns", r->creature_spawns }, { "creature_looks", r->creature_looks }, { "looks_wc3", r->looks_wc3 },
        { "looks_game", r->looks_game }, { "looks_stand_in", r->looks_stand_in }, { "looks_character", r->looks_character },
        { "looks_missing", r->looks_missing }, { "looks_unreadable", r->looks_unreadable },
        { "start_x", r->start_x }, { "start_y", r->start_y }, { "start_z", r->start_z },
        { "end_x", r->end_x }, { "end_y", r->end_y }, { "end_z", r->end_z }, { "end_facing", r->end_facing },
        { "distance", r->distance }, { "lowest_z", r->lowest_z }, { "highest_z", r->highest_z },
        { "tiles", r->tiles }, { "ground_files", r->ground_files }, { "steps_airborne", r->steps_airborne },
        { "steps_swimming", r->steps_swimming }, { "steps_sliding", r->steps_sliding }, { "steps_no_ground", r->steps_no_ground },
        { "models", r->models }, { "models_solid", r->models_solid }, { "models_wc3", r->models_wc3 }, { "buildings", r->buildings },
        { "model_stand_ins", r->model_stand_ins }, { "models_left_out", r->models_left_out }, { "model_copies", r->model_copies },
        { "solid_copies", r->solid_copies }, { "solid_triangles", r->solid_triangles }, { "building_pools", r->building_pools },
        { "steps_on_models", r->steps_on_models }, { "steps_walled", r->steps_walled },
        { "pathing", r->pathing }, { "pathing_cells", r->pathing_cells }, { "pathing_closed", r->pathing_closed },
        { "steps_walled_pathing", r->steps_walled_pathing }, { "steps_on_closed", r->steps_on_closed },
        { "looks_placeholder", r->looks_placeholder }, { "table_placeholders", r->table_placeholders },
        { "light", r->light }, { "light_tables", r->light_tables }, { "light_hours", r->light_hours }, { "fog_from_map", r->fog_from_map },
        { "weather_state", r->weather_state }, { "weather_grade", r->weather_grade }, { "weather_changes", r->weather_changes }, { "storm", r->storm },
        { "fog_mode", r->fog_mode }, { "fog_start", r->fog_start }, { "fog_end", r->fog_end }, { "fog_red", r->fog_red }, { "fog_green", r->fog_green }, { "fog_blue", r->fog_blue },
        { "sky_red", r->sky_red }, { "sky_green", r->sky_green }, { "sky_blue", r->sky_blue },
    };
    for (size_t i = 0; i < sizeof c / sizeof c[0]; ++i) {
        WErr rc = goodbye_count(bye, c[i].name, c[i].value, why);
        if (rc) return rc;
    }
    return W_OK;
}
/* }}} */

/* {{{ headless_run */
static WErr ended(Goodbye *bye, WErr rc, WWhy *why)
{
    WWhy w;
    if (goodbye_end(bye, GOODBYE_ERROR, why->text, &w)) fprintf(stderr, "%s\n", w.text);
    return rc;
}

WErr headless_run(const HeadlessIn *in, Goodbye *bye, HeadlessResult *out, WWhy *why)
{
    HeadlessResult r;
    memset(&r, 0, sizeof r);
    FILE *log = in->start.log ? in->start.log : stdout;
    WorldStart ws;
    WErr rc = world_start_open(&in->start, &ws, why);
    if (rc) return ended(bye, rc, why);
    if (in->moves->map[0] && strcasecmp(in->moves->map, ws.map_name)) {   /* a route walked on another map means nothing */
        w_fail(why, W_ERR_REFUSED, "the moves%s%s are for the map %s; this run's is %s", in->moves->name[0] ? " " : "", in->moves->name, in->moves->map, ws.map_name);
        world_start_close(&ws);
        return ended(bye, W_ERR_REFUSED, why);
    }
    Anim anim;
    memset(&anim, 0, sizeof anim);
    if ((rc = her(&ws, log, &r, &anim, why)) || (rc = creatures(&ws, in->start.map_id, in->world_dir, log, &r, why))) {
        anim_free(&anim);
        world_start_close(&ws);
        return ended(bye, rc, why);
    }
    r.her_bones = anim.nbones;
    r.her_sequences = anim.nseqs;

    /* {{{ the moves */
    Here *h = calloc(1, sizeof *h);
    LiquidWorld *waters = liquid_new();
    double ox = floor(in->start.start_x), oy = floor(in->start.start_y);   /* 043's origin */
    CollideWorld *solids = collide_new(ox, oy);
    double wc3_yards = in->start.conv && in->start.conv->wc3_scale > 0 ? in->start.conv->wc3_scale : 0;   /* as 043 tells its doodads */
    ModelSolids *models = model_solids_new(ws.assets, &ws.chain, wc3_yards, solids, waters, log);
    if (!h || !waters || !solids || !models) {
        free(h); liquid_free(waters); collide_free(solids); model_solids_free(models); anim_free(&anim); world_start_close(&ws);
        w_fail(why, W_ERR_FULL, "no memory for the headless run");
        return ended(bye, W_ERR_FULL, why);
    }
    h->ws = &ws;
    h->waters = waters;
    h->solids = solids;
    h->models = models;
    h->log = log;
    h->ox = ox; h->oy = oy;
    int rows = 0;
    if (in->start.conv) {   /* a converted map's WC3 pathing as walls (094, 1109 step 1p); none is no walls, said */
        WWhy w;
        WErr pe = wc3_pathing_open(ws.assets, &ws.chain, in->start.conv->wc3_scale, in->start.conv->wc3_x0, in->start.conv->wc3_y0, &h->pathing, &w);
        if (pe == W_ERR_NOT_FOUND) fprintf(log, "headless: %s\n", w.text);
        else if (pe) {
            free(h); liquid_free(waters); collide_free(solids); model_solids_free(models); anim_free(&anim); world_start_close(&ws);
            w_fail(why, pe, "%s", w.text);
            return ended(bye, pe, why);
        } else {
            r.pathing = 1;
            wc3_pathing_size(h->pathing, &r.pathing_cells, &rows, &r.pathing_closed);
            r.pathing_cells *= rows;
            fprintf(log, "headless: its WC3 pathing, %d cells, %d closed to walkers: walls\n", r.pathing_cells, r.pathing_closed);
        }
    }
    {
        char wdt[300];
        snprintf(wdt, sizeof wdt, "World\\Maps\\%s\\%s.wdt", ws.map_name, ws.map_name);
        AssetAnswer f;
        WWhy w;
        if (!asset_read(ws.assets, &ws.chain, wdt, ASSET_GROUND, &f, &w)) {
            Wdt *t = malloc(sizeof *t);
            if (t && !wdt_read(f.data, f.len, t, &w)) {
                h->big_alpha = wdt_big_alpha(t);
                h->tiles_known = 1;
                for (int k = 0; k < WDT_TILES; ++k) h->listed[k] = (t->tile_flags[k] & WDT_MAIN_HAS_ADT) != 0;
                fprintf(log, "headless: its WDT lists %d ground tile%s%s%s\n", t->ntiles, t->ntiles == 1 ? "" : "s",
                        wdt_wmo_only(t) ? "; it is one building, not yet placed: " : "", wdt_wmo_only(t) ? t->wmo : "");
            } else if (t) fprintf(log, "headless: %s: %s; each tile's file is asked for\n", wdt, w.text);
            free(t);
            asset_answer_free(&f);
        }
    }
    const HeadlessMoves *mv = in->moves;
    Mover m;
    mover_init(&m, in->start.start_x, in->start.start_y, ws.z, WORLD_START_FACING, WORLD_START_SPEED, ws.max_slope);
    h->max_grad = m.max_grad;
    MoverWorld mw = { ground_at, wall_at, water_at, h };
    MovesState ms;
    moves_init(&ms, &anim);
    int last_seq = ms.seq;
    r.start_x = m.x; r.start_y = m.y; r.start_z = m.z;
    r.lowest_z = r.highest_z = m.z;
    /* the weather (099, 1109 step 1v): 043's --weather from the start, at
     * once, then each move's as it begins; the clock is the steps' */
    Weather weather;
    weather_init(&weather);
    if (in->has_weather) weather_take(&weather, &in->weather, 0);
    double clock = 0;
    fprintf(log, "headless: %s%s%d moves, %d steps of %g seconds, from (%.3f, %.3f, %.3f)\n", mv->name[0] ? mv->name : "", mv->name[0] ? ": " : "",
            mv->nmoves, mv->steps, mv->step, m.x, m.y, m.z);
    for (int i = 0; i < mv->nmoves; ++i) {
        const Move *o = &mv->moves[i];
        if (o->turns) m.o = o->turn_to;   /* she faces it at once, as a turn of the mouse does */
        if (o->has_weather) {
            weather_take(&weather, &o->weather, clock);
            fprintf(log, "headless: move %d: the weather is state %u, grade %.2f%s, %.1f seconds in\n", i + 1, (unsigned)o->weather.state, o->weather.grade,
                    o->weather.abrupt ? ", at once" : "", clock);
        }
        MoverInput mi = {
            .forward = !!(o->keys & 1u << HOLD_FORWARD), .back = !!(o->keys & 1u << HOLD_BACK),
            .left = !!(o->keys & 1u << HOLD_LEFT), .right = !!(o->keys & 1u << HOLD_RIGHT),
            .strafe_left = !!(o->keys & 1u << HOLD_STRAFE_LEFT), .strafe_right = !!(o->keys & 1u << HOLD_STRAFE_RIGHT),
            .jump = !!(o->keys & 1u << HOLD_JUMP), .swim_down = !!(o->keys & 1u << HOLD_SWIM_DOWN), .walking = o->walking,
        };
        for (int s = 0; s < o->steps; ++s) {
            near_tiles(h, m.x, m.y);
            double bx = m.x, by = m.y;
            h->walled = 0;
            mover_step(&m, &mi, mv->step, &mw);
            if (h->walled & MODEL_SOLIDS_WALL_MODEL) ++r.steps_walled;
            if (h->walled & MODEL_SOLIDS_WALL_PATHING) ++r.steps_walled_pathing;
            if (wc3_pathing_blocks(h->pathing, m.x, m.y)) ++r.steps_on_closed;   /* a wall let her through: none, but for a start in a closed cell */
            r.distance += hypot(m.x - bx, m.y - by);
            if (m.z < r.lowest_z) r.lowest_z = m.z;
            if (m.z > r.highest_z) r.highest_z = m.z;
            if (!m.grounded && !m.swimming) ++r.steps_airborne;
            if (m.swimming) ++r.steps_swimming;
            if (m.sliding) ++r.steps_sliding;
            float gx, gy;
            float map_z = maps_world_height(&ws.world, (float)m.x, (float)m.y, &gx, &gy);
            if (map_z < -10000) ++r.steps_no_ground;
            if (m.grounded) {   /* standing on a model's floor: one under her feet, above the map's ground there */
                float fz = collide_floor(solids, m.x, m.y, m.z + 0.1, &gx, &gy);
                if (fz > -10000 && fabs(fz - m.z) < 0.1 && fz > map_z + 0.01f) ++r.steps_on_models;
            }
            /* her animation, chosen as 043 chooses it from what she did */
            MovesInput ai = {
                .forward = mi.forward, .back = mi.back, .strafe_left = mi.strafe_left, .strafe_right = mi.strafe_right,
                .turn_left = mi.left, .turn_right = mi.right,
                .grounded = m.grounded || m.swimming, .rising = m.vz > 0, .swimming = m.swimming, .walking = mi.walking,
                .speed = (float)mover_intended_speed(&m, &mi), .dt = (float)mv->step,
            };
            moves_step(&ms, &anim, &ai);
            if (ms.seq != last_seq) { ++r.her_animation_changes; last_seq = ms.seq; }
            ++r.steps;
            clock += mv->step;
        }
    }
    r.weather_state = (int)weather.now.state;
    r.weather_grade = weather.now.grade;
    r.weather_changes = weather.changes;
    r.storm = weather_strength(&weather, clock);
    r.moves = mv->nmoves;
    r.end_x = m.x; r.end_y = m.y; r.end_z = m.z;
    r.end_facing = fmod(m.o, 2 * 3.14159265358979323846);
    if (r.end_facing < 0) r.end_facing += 2 * 3.14159265358979323846;
    r.her_animation = ms.seq >= 0 && ms.seq < anim.nseqs ? anim.seqs[ms.seq].id : -1;
    r.tiles = h->tiles_entered;
    r.ground_files = h->ground_files;
    ModelSolidsCounts mc;
    model_solids_counts(models, &mc);
    r.models = mc.models; r.models_solid = mc.models_solid; r.models_wc3 = mc.models_wc3; r.buildings = mc.buildings;
    r.model_stand_ins = mc.stand_ins; r.models_left_out = mc.left_out; r.model_copies = mc.copies; r.solid_copies = mc.solid_copies;
    r.solid_triangles = mc.triangles; r.building_pools = mc.pools;
    fprintf(log, "headless: %d models and %d buildings placed %d times (%d WC3 models and %d stand-ins walked through, %d left out); %d copies solid, %d solid triangles, %d buildings' pools\n",
            r.models, r.buildings, r.model_copies, r.models_wc3, r.model_stand_ins, r.models_left_out, r.solid_copies, r.solid_triangles, r.building_pools);
    fprintf(log, "headless: ended at (%.3f, %.3f, %.3f) after %.1f yards; %d tiles entered, %d steps in the air, %d swimming, %d with no ground, %d on models' floors, %d against their walls, %d against the pathing's\n",
            r.end_x, r.end_y, r.end_z, r.distance, r.tiles, r.steps_airborne, r.steps_swimming, r.steps_no_ground, r.steps_on_models, r.steps_walled, r.steps_walled_pathing);
    /* {{{ the light where she ends (026 through the tables; 1109 step 1t):
     * what the window would clear its picture to and fog by, at the run's
     * hour, the camera out of the water; a test map's own fog over it (084) */
    WErr light_rc;
    WWhy lw;
    {
        LightTables lt;
        r.light = -1;
        r.light_hours = in->hours;
        if ((light_rc = light_open(&lt, ws.tables, log, &lw)) == W_OK) {
            LightNow now;
            r.light_tables = !lt.none;
            r.light = light_at_weather(&lt, in->start.map_id, (float)m.x, (float)m.y, (float)m.z, in->hours, LIGHT_CLEAR, (float)r.storm, &now);
            MapFog mf = { 0 };
            if (in->start.test_map && in->start.conv) {
                W3iInfo info;
                WWhy fwhy;
                WErr fw = map_w3i_read(ws.assets, &ws.chain, &info, &fwhy);
                map_fog_from_w3i(fw ? NULL : &info, in->start.conv->wc3_scale, &mf);
            }
            /* as 043 each frame: the map's fog, then the view distance ends it at the latest */
            float farclip = in->start.settings ? (float)settings_num(in->start.settings, "farclip") : 0;
            r.fog_from_map = farclip > 0 ? map_fog_frame(&mf, 0, farclip, &now) : map_fog_apply(&mf, &now);
            r.fog_start = now.fog_start; r.fog_end = now.fog_end; r.fog_mode = now.fog_mode;
            r.fog_red = now.fog[0]; r.fog_green = now.fog[1]; r.fog_blue = now.fog[2];
            r.sky_red = now.sky[0][0]; r.sky_green = now.sky[0][1]; r.sky_blue = now.sky[0][2];
            fprintf(log, "headless: the light at %.2f hours where she ends, the storm sets at %.2f: %s%d, fog from %.1f to %.1f yards%s (mode %d), colour (%.3f, %.3f, %.3f), sky overhead (%.3f, %.3f, %.3f)\n",
                    in->hours, r.storm, r.light < 0 ? "none (the grey day) " : "light ", r.light, r.fog_start, r.fog_end, r.fog_from_map ? " (the map's own)" : "", r.fog_mode,
                    r.fog_red, r.fog_green, r.fog_blue, r.sky_red, r.sky_green, r.sky_blue);
            light_close(&lt);
        } else fprintf(log, "headless: the light: %s\n", lw.text);   /* a light table there but broken: the run's error, after the rest is let go */
    }
    /* }}} */
    r.table_placeholders = tables_placeholders(ws.tables);
    if (r.table_placeholders) fprintf(log, "headless: %d placeholder rows stood in for rows no place has (%d looks)\n", r.table_placeholders, r.looks_placeholder);
    wc3_pathing_free(h->pathing);
    model_solids_free(models);
    collide_free(solids);
    liquid_free(waters);
    free(h);
    anim_free(&anim);
    world_start_close(&ws);
    /* }}} */

    if (light_rc) { w_fail(why, light_rc, "%s", lw.text); return ended(bye, light_rc, why); }
    if (out) *out = r;
    if ((rc = counted(bye, &r, why))) return ended(bye, rc, why);
    return goodbye_end(bye, GOODBYE_CLOSED, NULL, why);
}
/* }}} */
