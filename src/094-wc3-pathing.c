/*
 * 094-wc3-pathing.c (issue 1109 step 1p) - see 094-wc3-pathing.h
 *
 * For a general audience: keeps the pathing map's bytes and where its
 * grid lies, turns a place in the converted world back into WC3's units
 * to find its cell, and says whether a step crosses into a closed cell.
 */
#include "094-wc3-pathing.h"
#include "063-w3e.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct Wc3Pathing {
    int      width, height, blocking;
    double   offset_x, offset_y, scale, x0, y0;
    uint8_t *cells;
};

static int32_t i32(const unsigned char *s) { return (int32_t)((uint32_t)s[0] | (uint32_t)s[1] << 8 | (uint32_t)s[2] << 16 | (uint32_t)s[3] << 24); }

/* {{{ wc3_pathing_read */
WErr wc3_pathing_read(const unsigned char *s, size_t len, double offset_x, double offset_y,
                      double scale, double x0, double y0, Wc3Pathing **out, WWhy *why)
{
    *out = NULL;
    if (!(scale > 0)) return w_fail(why, W_ERR_REFUSED, "a pathing map with %g yards a WC3 unit", scale);
    if (len < 16 || memcmp(s, "MP3W", 4)) return w_fail(why, W_ERR_CORRUPT, "not a pathing map (no MP3W at its start)");
    int32_t version = i32(s + 4), w = i32(s + 8), h = i32(s + 12);
    if (version != 0) return w_fail(why, W_ERR_UNSUPPORTED, "a pathing map of version %d (this client reads 0)", (int)version);
    if (w < 1 || h < 1 || w > 65536 || h > 65536) return w_fail(why, W_ERR_CORRUPT, "a pathing map %d cells by %d", (int)w, (int)h);
    if ((uint64_t)len - 16 < (uint64_t)w * (uint64_t)h)
        return w_fail(why, W_ERR_CORRUPT, "the pathing map's %d x %d cells are cut short (%zu bytes of them)", (int)w, (int)h, len - 16);
    Wc3Pathing *p = calloc(1, sizeof *p);
    if (p) p->cells = malloc((size_t)w * (size_t)h);
    if (!p || !p->cells) { free(p); return w_fail(why, W_ERR_FULL, "no memory for a pathing map of %d x %d cells", (int)w, (int)h); }
    memcpy(p->cells, s + 16, (size_t)w * (size_t)h);
    p->width = w; p->height = h;
    p->offset_x = offset_x; p->offset_y = offset_y;
    p->scale = scale; p->x0 = x0; p->y0 = y0;
    for (size_t k = 0; k < (size_t)w * (size_t)h; ++k) p->blocking += (p->cells[k] & (WPM_NO_WALK | WPM_LAND)) == (WPM_NO_WALK | WPM_LAND);
    *out = p;
    return W_OK;
}
/* }}} */

/* {{{ wc3_pathing_open */
WErr wc3_pathing_open(Resolver *r, MpqChain *chain, double scale, double x0, double y0, Wc3Pathing **out, WWhy *why)
{
    *out = NULL;
    if (!r) return w_fail(why, W_ERR_NOT_FOUND, "no resolver, so no converted map's pathing (a test-map run's)");
    WWhy inner;
    AssetAnswer a;
    WErr rc = asset_read(r, chain, WPM_NAME, ASSET_GROUND, &a, &inner);
    if (rc == W_ERR_NOT_FOUND) return w_fail(why, rc, "no %s in the build folder or any other place (world-edit-to-execute's converter copies the map's own since its draft PR #252; this build is from before it, or its map has none): no pathing walls", WPM_NAME);
    if (rc) return w_fail(why, rc, "%s: %s", WPM_NAME, inner.text);
    AssetAnswer g;
    rc = asset_read(r, chain, "war3map.w3e", ASSET_GROUND, &g, &inner);
    if (rc) {
        asset_answer_free(&a);
        return w_fail(why, rc, "%s is there but war3map.w3e (where its grid lies) %s", WPM_NAME, rc == W_ERR_NOT_FOUND ? "isn't" : inner.text);
    }
    W3eGround *ground;
    rc = w3e_read(g.data, g.len, &ground, &inner);
    asset_answer_free(&g);
    if (rc) { asset_answer_free(&a); return w_fail(why, rc, "war3map.w3e: %s", inner.text); }
    rc = wc3_pathing_read(a.data, a.len, ground->offset_x, ground->offset_y, scale, x0, y0, out, &inner);
    if (!rc && ((*out)->width != (ground->width - 1) * 4 || (*out)->height != (ground->height - 1) * 4)) {
        rc = w_fail(&inner, W_ERR_CORRUPT, "%d x %d cells, where war3map.w3e's %d x %d tiles want %d x %d",
                    (*out)->width, (*out)->height, ground->width - 1, ground->height - 1, (ground->width - 1) * 4, (ground->height - 1) * 4);
        wc3_pathing_free(*out);
        *out = NULL;
    }
    w3e_free(ground);
    asset_answer_free(&a);
    if (rc) return w_fail(why, rc, "%s: %s", WPM_NAME, inner.text);
    return W_OK;
}
/* }}} */

void wc3_pathing_free(Wc3Pathing *p)
{
    if (!p) return;
    free(p->cells);
    free(p);
}

/* {{{ wc3_pathing_flags, wc3_pathing_blocks */
static void cell_of(const Wc3Pathing *p, double x, double y, long *i, long *j)
{
    double wc3_x = (p->y0 - y) / p->scale, wc3_y = (x - p->x0) / p->scale;   /* map.lua's rule, turned round */
    *i = (long)floor((wc3_x - p->offset_x) / WPM_CELL);
    *j = (long)floor((wc3_y - p->offset_y) / WPM_CELL);
}
static int at(const Wc3Pathing *p, long i, long j)
{
    if (i < 0 || j < 0 || i >= p->width || j >= p->height) return -1;
    return p->cells[(size_t)j * (size_t)p->width + (size_t)i];
}
static int closed(int f) { return f >= 0 && (f & (WPM_NO_WALK | WPM_LAND)) == (WPM_NO_WALK | WPM_LAND); }

int wc3_pathing_flags(const Wc3Pathing *p, double x, double y, int *i, int *j)
{
    long ci, cj;
    cell_of(p, x, y, &ci, &cj);
    if (i) *i = (int)ci;
    if (j) *j = (int)cj;
    return at(p, ci, cj);
}

int wc3_pathing_blocks(const Wc3Pathing *p, double x, double y)
{
    return p && closed(wc3_pathing_flags(p, x, y, NULL, NULL));
}
/* }}} */

/* {{{ wc3_pathing_wall
 * The cells run along the converted world's axes: WC3's x (a cell's
 * column) is the world's -y, WC3's y (its row) the world's x. Both the
 * point she steps to and her front there (a body's reach ahead) are
 * asked, so a closed cell's corner can't slip between them. A step that
 * changes column alone meets a side facing along y; row alone, along x;
 * both, the side whose neighbour is closed, else the corner (facing back
 * along the step). */
int wc3_pathing_wall(const Wc3Pathing *p, double x0, double y0, double x1, double y1, double radius, float *nx, float *ny)
{
    if (!p) return 0;
    double dx = x1 - x0, dy = y1 - y0, d = hypot(dx, dy);
    double px = x1, py = y1;
    if (d > 1e-9) { px += dx / d * radius; py += dy / d * radius; }   /* her front, a body's reach ahead */
    long i0, j0, i1, j1;
    cell_of(p, x0, y0, &i0, &j0);
    if (closed(at(p, i0, j0))) return 0;
    cell_of(p, x1, y1, &i1, &j1);                          /* where she steps, and her front there: either closed stops her */
    if (!closed(at(p, i1, j1))) {
        cell_of(p, px, py, &i1, &j1);
        if (!closed(at(p, i1, j1))) return 0;
    }
    int along_y;
    if (j0 == j1) along_y = 1;
    else if (i0 == i1) along_y = 0;
    else if (closed(at(p, i1, j0))) along_y = 1;
    else if (closed(at(p, i0, j1))) along_y = 0;
    else {
        *nx = d > 1e-9 ? (float)(-dx / d) : 1.0f;
        *ny = d > 1e-9 ? (float)(-dy / d) : 0.0f;
        return 1;
    }
    if (along_y) { *nx = 0; *ny = dy > 0 ? -1.0f : 1.0f; }
    else         { *nx = dx > 0 ? -1.0f : 1.0f; *ny = 0; }
    return 1;
}
/* }}} */

void wc3_pathing_size(const Wc3Pathing *p, int *width, int *height, int *blocking)
{
    *width = p->width; *height = p->height; *blocking = p->blocking;
}
