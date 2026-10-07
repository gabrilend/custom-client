/*
 * 039-horizon.c (issue 173s) - the far horizon; see 039-horizon.h
 *
 * For a general audience: one coarse mesh per tile, made when first
 * wanted, drawn with a simple shader in the fog's colour.
 */
#include "039-horizon.h"
#include "013-errors.h"
#include "020-adt.h"
#include "032-sight.h"

#include "raymath.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the shader: the fog's colour, a little darker on slopes facing away
 * from the sun, so the mountains' shapes read */
static const char *VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec3 vertexNormal;\n"
    "uniform mat4 mvp;\n"
    "out vec3 n;\n"
    "void main() { n = vertexNormal; gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";
static const char *FS =
    "#version 330\n"
    "in vec3 n; out vec4 frag;\n"
    "uniform vec3 sun_dir, fog_col;\n"
    "void main() { frag = vec4(fog_col * (0.82 + 0.18 * max(dot(normalize(n), sun_dir), 0.0)), 1.0); }\n";
/* }}} */

/* {{{ a horizon (the handle: 039-horizon.h); `ready` 0: the map has no
 * low-detail heights, nothing is drawn */
struct Horizon {
    AdtWdl  *wdl;                     /* the map's low-detail heights (020-adt.h) */
    Mesh     meshes[64 * 64];
    Vector3  centre[64 * 64];         /* each tile's bounding ball (173u: tiles out of sight aren't drawn) */
    float    radius[64 * 64];
    char     made[64 * 64];           /* 0 not yet, 1 made, 2 nothing to draw */
    double   ox, oy;
    Shader   shader;
    Material material;
    int      ready, loc_sun, loc_fog;
};
/* }}} */

/* {{{ horizon_init */
void horizon_free(Horizon *h)
{
    for (int i = 0; i < 64 * 64; ++i) if (h->made[i] == 1) UnloadMesh(h->meshes[i]);
    if (h->ready) UnloadShader(h->shader);
    free(h->wdl);
    free(h);
}
Horizon *horizon_new(MpqChain *chain, const char *map_name, double ox, double oy, WWhy *why)
{
    Horizon *h = calloc(1, sizeof *h);
    why->code = W_OK;
    h->ox = ox; h->oy = oy;
    char path[200];
    snprintf(path, sizeof path, "World\\Maps\\%s\\%s.wdl", map_name, map_name);
    size_t len;
    const char *from;
    unsigned char *s = mpq_chain_read(chain, path, &len, &from);
    if (!s) { w_fail(why, W_ERR_NOT_FOUND, "%s is in none of the archives: no far horizon", path); return h; }
    h->wdl = malloc(sizeof *h->wdl);
    WWhy inner;
    if (adt_wdl_read(s, len, h->wdl, &inner)) { free(s); free(h->wdl); h->wdl = NULL; w_fail(why, inner.code, "%s: %s", path, inner.text); return h; }
    free(s);
    h->shader = LoadShaderFromMemory(VS, FS);
    h->loc_sun = GetShaderLocation(h->shader, "sun_dir");
    h->loc_fog = GetShaderLocation(h->shader, "fog_col");
    h->material = LoadMaterialDefault();
    h->material.shader = h->shader;
    h->ready = 1;
    return h;
}
float horizon_corner(const Horizon *h, int tx, int ty)
{
    if (!h->ready || tx < 0 || tx > 63 || ty < 0 || ty > 63 || !h->wdl->have[tx * 64 + ty]) return -100000.0f;
    return h->wdl->heights[tx * 64 + ty][0];
}
/* }}} */

/* {{{ static make: one tile's coarse mesh, its 16 x 16 cells as two
 * triangles each over the corner heights, none over holes */
static void make(Horizon *h, int tx, int ty)
{
    int i = tx * 64 + ty;
    if (!h->wdl->have[i]) { h->made[i] = 2; return; }
    const float step = 533.33333f / 16;
    Mesh m = { 0 };
    m.vertexCount = 17 * 17;
    m.vertices = MemAlloc(sizeof(float) * 3 * m.vertexCount);
    m.normals = MemAlloc(sizeof(float) * 3 * m.vertexCount);
    for (int r = 0; r <= 16; ++r)
        for (int c = 0; c <= 16; ++c) {
            int k = r * 17 + c;
            m.vertices[k * 3] = (float)((32 - tx) * 533.33333 - r * step - h->ox);
            m.vertices[k * 3 + 1] = (float)((32 - ty) * 533.33333 - c * step - h->oy);
            m.vertices[k * 3 + 2] = h->wdl->heights[i][k];
            const int16_t *hts = h->wdl->heights[i];
            float hn = hts[(r > 0 ? r - 1 : r) * 17 + c], hs = hts[(r < 16 ? r + 1 : r) * 17 + c];
            float hw = hts[r * 17 + (c > 0 ? c - 1 : c)], he = hts[r * 17 + (c < 16 ? c + 1 : c)];
            Vector3 nv = Vector3Normalize((Vector3){ (hs - hn) / (2 * step), (he - hw) / (2 * step), 1 });
            m.normals[k * 3] = nv.x; m.normals[k * 3 + 1] = nv.y; m.normals[k * 3 + 2] = nv.z;
        }
    m.indices = MemAlloc(sizeof(unsigned short) * 16 * 16 * 6);
    int n = 0;
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < 16; ++c) {
            if (h->wdl->holes[i][r] & (1 << c)) continue;
            unsigned short a = (unsigned short)(r * 17 + c), b = (unsigned short)(a + 1), d = (unsigned short)(a + 17), e = (unsigned short)(d + 1);
            unsigned short tri[6] = { a, b, e, a, e, d };
            memcpy(m.indices + n, tri, sizeof tri);
            n += 6;
        }
    if (!n) { UnloadMesh(m); h->made[i] = 2; return; }
    m.triangleCount = n / 3;
    float lo = 1e30f, hi = -1e30f;
    for (int k = 0; k < m.vertexCount; ++k) { lo = fminf(lo, m.vertices[k * 3 + 2]); hi = fmaxf(hi, m.vertices[k * 3 + 2]); }
    h->centre[i] = (Vector3){ m.vertices[8 * 17 * 3 + 8 * 3], m.vertices[8 * 17 * 3 + 8 * 3 + 1], (lo + hi) / 2 };   /* the middle point, half way up */
    h->radius[i] = sqrtf(2 * 266.67f * 266.67f + (hi - lo) * (hi - lo) / 4);
    UploadMesh(&m, false);
    h->meshes[i] = m;
    h->made[i] = 1;
}
/* }}} */

/* {{{ horizon_draw */
void horizon_draw(Horizon *h, int tx, int ty, const char *loaded, Vector3 eye, const LightNow *light)
{
    (void)eye;
    if (!h->ready) return;
    SetShaderValue(h->shader, h->loc_sun, light->sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(h->shader, h->loc_fog, light->fog, SHADER_UNIFORM_VEC3);
    Sight sight;
    sight_from(&sight, MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()));
    for (int x = tx - HORIZON_REACH; x <= tx + HORIZON_REACH; ++x)
        for (int y = ty - HORIZON_REACH; y <= ty + HORIZON_REACH; ++y) {
            if (x < 0 || x > 63 || y < 0 || y > 63 || loaded[x * 64 + y] == 1) continue;
            int i = x * 64 + y;
            if (!h->made[i]) make(h, x, y);
            if (h->made[i] == 1 && sight_ball(&sight, h->centre[i], h->radius[i])) DrawMesh(h->meshes[i], h->material, MatrixIdentity());
        }
}
/* }}} */
