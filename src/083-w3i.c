/*
 * 083-w3i.c (issues 302, 1109) - a Warcraft III map's settings: its water
 * tint and fog; see 083-w3i.h
 *
 * For a general audience: walks the file field by field, from the start
 * to the water tint, keeping the few it needs.
 */
#include "083-w3i.h"

#include <string.h>

/* {{{ the walk */
typedef struct { const unsigned char *s; size_t len, at; } Walk;

static int skip(Walk *w, size_t n) { if (w->len - w->at < n) return 0; w->at += n; return 1; }
static int u32(Walk *w, uint32_t *v)
{
    if (w->len - w->at < 4) return 0;
    const unsigned char *p = w->s + w->at;
    *v = (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    w->at += 4;
    return 1;
}
static int f32(Walk *w, float *v) { uint32_t u; if (!u32(w, &u)) return 0; memcpy(v, &u, 4); return 1; }
static int text(Walk *w)
{
    const unsigned char *z = memchr(w->s + w->at, 0, w->len - w->at);
    if (!z) return 0;
    w->at = (size_t)(z - w->s) + 1;
    return 1;
}
static int bytes4(Walk *w, uint8_t out[4]) { if (w->len - w->at < 4) return 0; memcpy(out, w->s + w->at, 4); w->at += 4; return 1; }
/* }}} */

/* {{{ w3i_read */
#define NEED(ok, field) do { if (!(ok)) return w_fail(why, W_ERR_CORRUPT, "war3map.w3i ends inside its %s (byte %zu of %zu)", field, w.at, w.len); } while (0)

WErr w3i_read(const unsigned char *s, size_t len, W3iInfo *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    memset(out->water_rgba, 255, 4);                     /* untinted: white */
    out->fog_rgba[3] = 255;
    Walk w = { s, len, 0 };
    uint32_t v;
    NEED(u32(&w, &v), "version");
    out->version = (int)v;
    if (!(v == 18 || (v >= 25 && v < 1000)))
        return w_fail(why, W_ERR_UNSUPPORTED, "war3map.w3i's version %u: only 18 (Reign of Chaos) and 25 and later (The Frozen Throne, 1.31 and later) are read", v);
    NEED(skip(&w, 8), "saves and editor version");
    if (v >= 28) NEED(skip(&w, 16), "game version");
    NEED(text(&w), "name");
    NEED(text(&w), "author");
    NEED(text(&w), "description");
    NEED(text(&w), "players recommended");
    NEED(skip(&w, 8 * 4 + 4 * 4 + 2 * 4), "camera bounds and playable size");
    NEED(u32(&w, &out->flags), "flags");
    NEED(skip(&w, 1), "main tileset");
    out->tileset_code = (char)s[w.at - 1];
    if (v < 25) return W_OK;
    NEED(skip(&w, 4) && text(&w) && text(&w) && text(&w) && text(&w), "loading screen");
    NEED(skip(&w, 4), "game data set");
    NEED(text(&w) && text(&w) && text(&w) && text(&w), "prologue");
    uint32_t style;
    NEED(u32(&w, &style), "fog style");
    out->fog_style = (int)style;
    NEED(f32(&w, &out->fog_start) && f32(&w, &out->fog_end) && f32(&w, &out->fog_density), "fog heights and density");
    NEED(bytes4(&w, out->fog_rgba), "fog colour");
    NEED(skip(&w, 4), "weather");
    NEED(text(&w), "sound environment");
    NEED(skip(&w, 1), "light environment");
    NEED(bytes4(&w, out->water_rgba), "water tint");
    out->has_environment = 1;
    out->water_tinted = (out->flags & W3I_FLAG_WATER_TINT) != 0;
    out->fog_on = (out->flags & W3I_FLAG_TERRAIN_FOG) != 0;
    return W_OK;
}
/* }}} */
