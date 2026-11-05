/*
 * 082-water-prep.c (issues 173r, 302, 1109) - the water's meshes and
 * shader, without the window; see 082-water-prep.h
 *
 * For a general audience: the shader's text, and a layer's points, two
 * triangles a wet cell, each with its picture place, depth and (for
 * WC3's water) colour.
 */
#include "082-water-prep.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the shader (moved here from 040: 1109's decision 71) */
const char *const WATER_VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec2 vertexTexCoord2; in vec4 vertexColor;\n"
    "uniform mat4 mvp; uniform vec3 eye;\n"
    "out vec2 uv; out float dist; out float depth; out vec4 col;\n"
    "void main() { uv = vertexTexCoord; depth = vertexTexCoord2.x; col = vertexColor; dist = length(vertexPosition - eye); gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";
const char *const WATER_FS =
    "#version 330\n"
    "in vec2 uv; in float dist; in float depth; in vec4 col; out vec4 frag;\n"
    "uniform sampler2D texture0; uniform vec3 tint_near, tint_deep; uniform vec2 opacity; uniform float raw, wc3;\n"
    "uniform vec3 sun_dir, sun_col, shade_col, fog_col; uniform vec2 fog_range;\n"
    "void main() {\n"
    "  float fog = clamp((dist - fog_range.x) / max(fog_range.y - fog_range.x, 1.0), 0.0, 1.0);\n"
    "  if (wc3 > 0.5) { vec4 w = texture(texture0, uv) * col; frag = vec4(mix(w.rgb, fog_col, fog), w.a); return; }\n"
    "  vec3 t = texture(texture0, uv).rgb;\n"
    "  vec3 tint = mix(tint_near, tint_deep, depth);\n"
    "  vec3 c = mix(t * 0.6 + tint * 0.6, t, raw);\n"
    "  c *= mix(shade_col + sun_col * max(sun_dir.z, 0.0), vec3(1.0), raw);\n"
    "  frag = vec4(mix(c, fog_col, fog), mix(mix(opacity.x, opacity.y, depth), 1.0, raw));\n"
    "}\n";
/* }}} */

/* {{{ water_prep_layer, water_verts_free */
void water_verts_free(WaterVerts *v)
{
    free(v->vertices); free(v->texcoords); free(v->texcoords2); free(v->colors);
    memset(v, 0, sizeof *v);
}

WErr water_prep_layer(const AdtTile *t, int layer, double ox, double oy, const Wc3Water *wc3, WaterVerts *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (layer < 0 || layer >= t->nliquids) return w_fail(why, W_ERR_REFUSED, "water layer %d of a tile with %d", layer, t->nliquids);
    const AdtLiquid *q = &t->liquids[layer];
    const AdtChunk *c = &t->chunks[q->chunk];
    const float step = ADT_CHUNK_SIZE / 8;           /* as 040 placed them before */
    int cells = 0;
    for (int r = 0; r < 8; ++r) for (int col = 0; col < 8; ++col) cells += q->wet[r][col] != 0;
    if (!cells) return W_OK;
    int n = cells * 6;
    out->vertices = malloc(sizeof(float) * 3 * (size_t)n);
    out->texcoords = malloc(sizeof(float) * 2 * (size_t)n);
    out->texcoords2 = malloc(sizeof(float) * 2 * (size_t)n);
    if (wc3) out->colors = malloc(4 * (size_t)n);
    if (!out->vertices || !out->texcoords || !out->texcoords2 || (wc3 && !out->colors)) { water_verts_free(out); return w_fail(why, W_ERR_FULL, "no room for a water layer's %d points", n); }
    int v = 0;
    for (int r = 0; r < 8; ++r)
        for (int col = 0; col < 8; ++col) {
            if (!q->wet[r][col]) continue;
            static const int CORNER[6][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 } };
            for (int k = 0; k < 6; ++k) {
                int rr = r + CORNER[k][0], cc = col + CORNER[k][1];
                double wx = c->x - rr * step, wy = c->y - cc * step, z = q->height[rr][cc];
                if (wc3) {
                    double wz;
                    uint8_t rgba[4];
                    if (wc3_water_at(wc3, wx, wy, &wz, rgba)) {
                        ++out->on_grid;
                        out->moved += fabs(wz - z) > 0.001;
                        z = wz;
                    } else ++out->off_grid;
                    memcpy(out->colors + v * 4, rgba, 4);
                }
                out->vertices[v * 3] = (float)(wx - ox);
                out->vertices[v * 3 + 1] = (float)(wy - oy);
                out->vertices[v * 3 + 2] = (float)z;
                out->texcoords[v * 2] = (float)(wy / step);
                out->texcoords[v * 2 + 1] = (float)(wx / step);
                out->texcoords2[v * 2] = q->depth[rr][cc] / 255.0f;
                out->texcoords2[v * 2 + 1] = 0;
                ++v;
            }
        }
    out->count = n;
    return W_OK;
}
/* }}} */
