/*
 * 084-map-fog.c (issue 1109, step 1j) - a converted map's own fog; see
 * 084-map-fog.h
 *
 * For a general audience: reads the map's settings file once, turns its
 * fog into yards and a colour, and each frame puts that into the light
 * every drawer reads.
 */
#include "084-map-fog.h"

#include <math.h>

/* {{{ map_w3i_read */
WErr map_w3i_read(Resolver *r, MpqChain *chain, W3iInfo *out, WWhy *why)
{
    if (!r) return w_fail(why, W_ERR_NOT_FOUND, "no resolver, so no %s", MAP_W3I_NAME);
    AssetAnswer a;
    WWhy inner = { 0 };
    WErr rc = asset_read(r, chain, MAP_W3I_NAME, ASSET_GROUND, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, rc, "no %s in any place", MAP_W3I_NAME);
    if (rc) return w_fail(why, rc, "%s: %s", MAP_W3I_NAME, inner.text);
    rc = w3i_read(a.data, a.len, out, &inner);
    asset_answer_free(&a);
    if (rc) return w_fail(why, rc, "%s: %s", MAP_W3I_NAME, inner.text);
    return W_OK;
}
/* }}} */

/* {{{ map_fog_from_w3i */
void map_fog_from_w3i(const W3iInfo *in, double yards_per_unit, MapFog *out)
{
    *out = (MapFog){ 0 };
    if (!in)                              { out->why_off = "no war3map.w3i in its build"; return; }
    if (!in->has_environment)             { out->why_off = "its war3map.w3i (Reign of Chaos) has no fog"; return; }
    out->style = in->fog_style;
    if (!in->fog_on)                      { out->why_off = "its war3map.w3i turns no terrain fog on"; return; }
    if (!(yards_per_unit > 0))            { out->why_off = "the map's scale is not above 0"; return; }
    if (!isfinite(in->fog_start) || !isfinite(in->fog_end)) { out->why_off = "its fog's distances are not numbers"; return; }
    double start = in->fog_start * yards_per_unit, end = in->fog_end * yards_per_unit;
    if (start < 0) start = 0;
    if (end < start) end = start;
    out->start = (float)start;
    out->end = (float)end;
    for (int k = 0; k < 3; ++k) out->rgb[k] = in->fog_rgba[k] / 255.0f;
    out->on = 1;
}
/* }}} */

/* {{{ map_fog_apply */
int map_fog_apply(const MapFog *f, LightNow *l)
{
    if (!f || !f->on) return 0;
    for (int k = 0; k < 3; ++k) l->fog[k] = f->rgb[k];
    l->fog_start = f->start;
    l->fog_end = f->end;
    l->fog_mode = 0;                                 /* the map's own fog: none of the original's modes (1109 step 1u) */
    return 1;
}
/* }}} */

/* {{{ map_fog_frame */
int map_fog_frame(const MapFog *f, int under, float farclip, LightNow *l)
{
    int used = !under && map_fog_apply(f, l);
    if (l->fog_end > farclip) { l->fog_start *= farclip / l->fog_end; l->fog_end = farclip; }
    return used;
}
/* }}} */
