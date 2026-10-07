/*
 * 039-horizon.c (issue 173s) - the far horizon; see 039-horizon.h
 *
 * For a general audience: one coarse mesh per tile, made when first
 * wanted, drawn with a simple shader in the fog's colour.
 */
#include "039-horizon.h"
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

/* {{{ state */
static AdtWdl *wdl;                   /* the map's low-detail heights (020-adt.h) */
static Mesh   meshes[64 * 64];
static Vector3 centre[64 * 64];       /* each tile's bounding ball (173u: tiles out of sight aren't drawn) */
static float   radius[64 * 64];
static char   made[64 * 64];          /* 0 not yet, 1 made, 2 nothing to draw */
static double ox_, oy_;
static Shader shader;
static Material material;
static int    ready, loc_sun, loc_fog;
/* }}} */

/* {{{ horizon_init */
int horizon_init(MpqChain *chain, const char *map_name, double ox, double oy, char *why, int why_len)
{
    ox_ = ox; oy_ = oy;
    char path[200];
    snprintf(path, sizeof path, "World\\Maps\\%s\\%s.wdl", map_name, map_name);
    size_t len;
    const char *from;
    unsigned char *s = mpq_chain_read(chain, path, &len, &from);
    if (!s) { snprintf(why, why_len, "%s is in none of the archives: no far horizon", path); return 1; }
    wdl = malloc(sizeof *wdl);
    char inner[200];
    if (adt_wdl_read(s, len, wdl, inner, sizeof inner)) { free(s); free(wdl); wdl = NULL; snprintf(why, why_len, "%s: %s", path, inner); return 1; }
    free(s);
    shader = LoadShaderFromMemory(VS, FS);
    loc_sun = GetShaderLocation(shader, "sun_dir");
    loc_fog = GetShaderLocation(shader, "fog_col");
    material = LoadMaterialDefault();
    material.shader = shader;
    ready = 1;
    return 0;
}
float horizon_corner(int tx, int ty)
{
    if (!ready || tx < 0 || tx > 63 || ty < 0 || ty > 63 || !wdl->have[tx * 64 + ty]) return -100000.0f;
    return wdl->heights[tx * 64 + ty][0];
}
/* }}} */

/* {{{ static make: one tile's coarse mesh, its 16 x 16 cells as two
 * triangles each over the corner heights, none over holes */
static void make(int tx, int ty)
{
    int i = tx * 64 + ty;
    if (!wdl->have[i]) { made[i] = 2; return; }
    const float step = 533.33333f / 16;
    Mesh m = { 0 };
    m.vertexCount = 17 * 17;
    m.vertices = MemAlloc(sizeof(float) * 3 * m.vertexCount);
    m.normals = MemAlloc(sizeof(float) * 3 * m.vertexCount);
    for (int r = 0; r <= 16; ++r)
        for (int c = 0; c <= 16; ++c) {
            int k = r * 17 + c;
            m.vertices[k * 3] = (float)((32 - tx) * 533.33333 - r * step - ox_);
            m.vertices[k * 3 + 1] = (float)((32 - ty) * 533.33333 - c * step - oy_);
            m.vertices[k * 3 + 2] = wdl->heights[i][k];
            const int16_t *h = wdl->heights[i];
            float hn = h[(r > 0 ? r - 1 : r) * 17 + c], hs = h[(r < 16 ? r + 1 : r) * 17 + c];
            float hw = h[r * 17 + (c > 0 ? c - 1 : c)], he = h[r * 17 + (c < 16 ? c + 1 : c)];
            Vector3 nv = Vector3Normalize((Vector3){ (hs - hn) / (2 * step), (he - hw) / (2 * step), 1 });
            m.normals[k * 3] = nv.x; m.normals[k * 3 + 1] = nv.y; m.normals[k * 3 + 2] = nv.z;
        }
    m.indices = MemAlloc(sizeof(unsigned short) * 16 * 16 * 6);
    int n = 0;
    for (int r = 0; r < 16; ++r)
        for (int c = 0; c < 16; ++c) {
            if (wdl->holes[i][r] & (1 << c)) continue;
            unsigned short a = (unsigned short)(r * 17 + c), b = (unsigned short)(a + 1), d = (unsigned short)(a + 17), e = (unsigned short)(d + 1);
            unsigned short tri[6] = { a, b, e, a, e, d };
            memcpy(m.indices + n, tri, sizeof tri);
            n += 6;
        }
    if (!n) { UnloadMesh(m); made[i] = 2; return; }
    m.triangleCount = n / 3;
    float lo = 1e30f, hi = -1e30f;
    for (int k = 0; k < m.vertexCount; ++k) { lo = fminf(lo, m.vertices[k * 3 + 2]); hi = fmaxf(hi, m.vertices[k * 3 + 2]); }
    centre[i] = (Vector3){ m.vertices[8 * 17 * 3 + 8 * 3], m.vertices[8 * 17 * 3 + 8 * 3 + 1], (lo + hi) / 2 };   /* the middle point, half way up */
    radius[i] = sqrtf(2 * 266.67f * 266.67f + (hi - lo) * (hi - lo) / 4);
    UploadMesh(&m, false);
    meshes[i] = m;
    made[i] = 1;
}
/* }}} */

/* {{{ horizon_draw */
void horizon_draw(int tx, int ty, int (*loaded)(int x, int y), Vector3 eye, const LightNow *light)
{
    (void)eye;
    if (!ready) return;
    SetShaderValue(shader, loc_sun, light->sun_dir, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, loc_fog, light->fog, SHADER_UNIFORM_VEC3);
    Sight sight;
    sight_from(&sight, MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()));
    for (int x = tx - HORIZON_REACH; x <= tx + HORIZON_REACH; ++x)
        for (int y = ty - HORIZON_REACH; y <= ty + HORIZON_REACH; ++y) {
            if (x < 0 || x > 63 || y < 0 || y > 63 || loaded(x, y)) continue;
            int i = x * 64 + y;
            if (!made[i]) make(x, y);
            if (made[i] == 1 && sight_ball(&sight, centre[i], radius[i])) DrawMesh(meshes[i], material, MatrixIdentity());
        }
}
/* }}} */
