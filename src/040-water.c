/*
 * 040-water.c (issue 173r) - drawing the water; see 040-water.h
 *
 * For a general audience: one mesh per water layer, one run of ripple
 * pictures per kind of water, and a shader that tints and fogs them.
 */
#include "040-water.h"
#include "013-errors.h"
#include "017-blp.h"
#include "016-dbc.h"
#include "032-sight.h"
#include "037-sky.h"
#include "036-texture.h"

#include "raymath.h"
#include "rlgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the shader
 * The ripple picture, tinted by the zone's water colours (water and sea),
 * lit a little by the sun, fogged, see-through. Each point carries its
 * depth as a share (the file's depth byte over 255, in the second picture
 * position's x): the tint goes from the near colour to the deep one, and
 * the opacity from the shallow to the deep (the light's water_alpha), by
 * that share. The straight 0-255 reading is this client's guess at the
 * original's curve (check list). Lava and slime (`raw`): the picture
 * alone, solid. */
static const char *VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec2 vertexTexCoord; in vec2 vertexTexCoord2;\n"
    "uniform mat4 mvp; uniform vec3 eye;\n"
    "out vec2 uv; out float dist; out float depth;\n"
    "void main() { uv = vertexTexCoord; depth = vertexTexCoord2.x; dist = length(vertexPosition - eye); gl_Position = mvp * vec4(vertexPosition, 1.0); }\n";
static const char *FS =
    "#version 330\n"
    "in vec2 uv; in float dist; in float depth; out vec4 frag;\n"
    "uniform sampler2D texture0; uniform vec3 tint_near, tint_deep; uniform vec2 opacity; uniform float raw;\n"
    "uniform vec3 sun_dir, sun_col, shade_col, fog_col; uniform vec2 fog_range;\n"
    "void main() {\n"
    "  vec3 t = texture(texture0, uv).rgb;\n"
    "  vec3 tint = mix(tint_near, tint_deep, depth);\n"
    "  vec3 c = mix(t * 0.6 + tint * 0.6, t, raw);\n"
    "  c *= mix(shade_col + sun_col * max(sun_dir.z, 0.0), vec3(1.0), raw);\n"
    "  float fog = clamp((dist - fog_range.x) / max(fog_range.y - fog_range.x, 1.0), 0.0, 1.0);\n"
    "  frag = vec4(mix(c, fog_col, fog), mix(mix(opacity.x, opacity.y, depth), 1.0, raw));\n"
    "}\n";
/* }}} */

/* {{{ a water drawer (the handle: 040-water.h)
 * kinds: per LiquidType id met so far, its run of pictures and its type
 * (0 water, 1 ocean, 2 magma, 3 slime). layers: one mesh per water layer
 * with the kind it uses; centre, radius: its bounding ball (173u). */
#define MAX_FRAMES 32
typedef struct { int id, type; Texture2D frames[MAX_FRAMES]; int nframes; } Kind;
typedef struct { Mesh mesh; int kind, tile; Vector3 centre; float radius; } Layer;
struct Water {
    Kind      kinds[64];  int nkinds;
    Layer    *layers;     int nlayers, layers_cap;
    MpqChain *chain;
    const Textures *textures;        /* how pictures go to the card (036-texture.h) */
    Dbc       table;      unsigned char *table_bytes;
    Shader    shader;
    Material  material;
    int       loc_eye, loc_near, loc_deep, loc_opacity, loc_raw;
};
/* }}} */

/* {{{ water_init */
void water_free(Water *w)
{
    for (int i = 0; i < w->nlayers; ++i) UnloadMesh(w->layers[i].mesh);
    for (int i = 0; i < w->nkinds; ++i) for (int f = 0; f < w->kinds[i].nframes; ++f) UnloadTexture(w->kinds[i].frames[f]);
    free(w->layers);
    free(w->table_bytes);
    if (IsShaderReady(w->shader)) UnloadShader(w->shader);
    free(w);
}
Water *water_new(MpqChain *c, const Textures *textures, WWhy *why)
{
    Water *w = calloc(1, sizeof *w);
    w->textures = textures;
    w->chain = c;
    size_t len;
    const char *from;
    w->table_bytes = mpq_chain_read(w->chain, "DBFilesClient\\LiquidType.dbc", &len, &from);
    if (!w->table_bytes) { w_fail(why, W_ERR_NOT_FOUND, "LiquidType.dbc is in none of the client's archives"); water_free(w); return NULL; }
    WWhy inner;
    if (dbc_open(&w->table, w->table_bytes, len, &inner)) { w_fail(why, inner.code, "LiquidType.dbc: %s", inner.text); water_free(w); return NULL; }
    w->shader = LoadShaderFromMemory(VS, FS);
    if (!IsShaderReady(w->shader)) { w_fail(why, W_ERR_GPU, "the water shader didn't compile (see Raylib's messages above)"); water_free(w); return NULL; }
    w->loc_eye = GetShaderLocation(w->shader, "eye");
    w->loc_near = GetShaderLocation(w->shader, "tint_near");
    w->loc_deep = GetShaderLocation(w->shader, "tint_deep");
    w->loc_opacity = GetShaderLocation(w->shader, "opacity");
    w->loc_raw = GetShaderLocation(w->shader, "raw");
    w->material = LoadMaterialDefault();
    w->material.shader = w->shader;
    return w;
}
/* }}} */

/* {{{ static kind_of: a liquid kind's pictures, loaded once
 * LiquidType (3.3.5a): 1 id, 4 type, 16 the picture name with %d for the
 * frame number (1 upward). Frames are loaded until one is missing. A kind
 * with no pictures is drawn plain (and said once). */
static int kind_of(Water *w, int id)
{
    for (int i = 0; i < w->nkinds; ++i) if (w->kinds[i].id == id) return i;
    if (w->nkinds == 64) return -1;
    Kind *k = &w->kinds[w->nkinds];
    memset(k, 0, sizeof *k);
    k->id = id;
    const char *pattern = NULL;
    for (uint32_t r = 0; r < w->table.rows; ++r)
        if ((int)dbc_u32(&w->table, r, 1) == id) { k->type = (int)dbc_u32(&w->table, r, 4); pattern = dbc_text(&w->table, r, 16); break; }
    if (!pattern || !pattern[0]) fprintf(stderr, "water: liquid kind %d has no pictures in LiquidType.dbc; drawn plain\n", id);
    for (int f = 1; pattern && pattern[0] && f <= MAX_FRAMES; ++f) {
        char name[300];
        snprintf(name, sizeof name, pattern, f);
        size_t len;
        const char *from;
        unsigned char *s = mpq_chain_read(w->chain, name, &len, &from);
        if (!s) break;
        BlpImage img;
        WWhy why;
        int bad = blp_decode(s, len, &img, &why);
        free(s);
        if (bad) { fprintf(stderr, "water: %s: %s\n", name, why.text); break; }
        Image im = { .data = img.rgba, .width = img.width, .height = img.height, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        k->frames[k->nframes] = LoadTextureFromImage(im);
        blp_free(&img);
        GenTextureMipmaps(&k->frames[k->nframes]);
        SetTextureWrap(k->frames[k->nframes], TEXTURE_WRAP_REPEAT);
        texture_filter(w->textures, k->frames[k->nframes].id, 0x0DE1, k->frames[k->nframes].mipmaps);   /* a 2D picture, as Texture Filtering says (173b6) */
        ++k->nframes;
    }
    return w->nkinds++;
}
/* }}} */

/* {{{ static bounds: a mesh's bounding ball, for the sight test (173u) */
static void bounds(const Mesh *m, Vector3 *centre, float *radius)
{
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
    for (int v = 0; v < m->vertexCount; ++v)
        for (int d = 0; d < 3; ++d) { lo[d] = fminf(lo[d], m->vertices[v * 3 + d]); hi[d] = fmaxf(hi[d], m->vertices[v * 3 + d]); }
    *centre = (Vector3){ (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2 };
    *radius = Vector3Distance(*centre, (Vector3){ hi[0], hi[1], hi[2] });
}
/* }}} */

/* {{{ water_add_tile
 * Each wet cell: two triangles over its four corners; the picture
 * repeats once a cell (4.2 yards), placed by world position so
 * neighbouring layers meet seamlessly. */
void water_add_tile(Water *w, const AdtTile *t, int tile, double ox, double oy)
{
    const float step = ADT_CHUNK_SIZE / 8;
    for (int i = 0; i < t->nliquids; ++i) {
        const AdtLiquid *q = &t->liquids[i];
        const AdtChunk *c = &t->chunks[q->chunk];
        int cells = 0;
        for (int r = 0; r < 8; ++r) for (int col = 0; col < 8; ++col) cells += q->wet[r][col];
        if (!cells) continue;
        Mesh m = { 0 };
        m.vertexCount = cells * 6;
        m.triangleCount = cells * 2;
        m.vertices = MemAlloc(sizeof(float) * 3 * m.vertexCount);
        m.texcoords = MemAlloc(sizeof(float) * 2 * m.vertexCount);
        m.texcoords2 = MemAlloc(sizeof(float) * 2 * m.vertexCount);
        int v = 0;
        for (int r = 0; r < 8; ++r)
            for (int col = 0; col < 8; ++col) {
                if (!q->wet[r][col]) continue;
                static const int CORNER[6][2] = { { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 } };
                for (int k = 0; k < 6; ++k) {
                    int rr = r + CORNER[k][0], cc = col + CORNER[k][1];
                    double wx = c->x - rr * step, wy = c->y - cc * step;
                    m.vertices[v * 3] = (float)(wx - ox);
                    m.vertices[v * 3 + 1] = (float)(wy - oy);
                    m.vertices[v * 3 + 2] = q->height[rr][cc];
                    m.texcoords[v * 2] = (float)(wy / step);
                    m.texcoords[v * 2 + 1] = (float)(wx / step);
                    m.texcoords2[v * 2] = q->depth[rr][cc] / 255.0f;
                    m.texcoords2[v * 2 + 1] = 0;
                    ++v;
                }
            }
        UploadMesh(&m, false);
        if (w->nlayers == w->layers_cap) { w->layers_cap = w->layers_cap ? w->layers_cap * 2 : 128; w->layers = realloc(w->layers, sizeof *w->layers * w->layers_cap); }
        w->layers[w->nlayers].mesh = m;
        w->layers[w->nlayers].kind = kind_of(w, q->kind);
        w->layers[w->nlayers].tile = tile;
        bounds(&m, &w->layers[w->nlayers].centre, &w->layers[w->nlayers].radius);
        ++w->nlayers;
    }
}
/* }}} */

/* {{{ water_add_pool */
void water_add_pool(Water *w, int tile, const float at[16], const WmoPool *p, int kind)
{
    int cx = p->nx - 1, cy = p->ny - 1, cells = 0;
    for (int i = 0; i < cx * cy; ++i) cells += !wmo_pool_dry(p->cell[i]);
    if (!cells) return;
    Mesh m = { 0 };
    m.vertexCount = cells * 6;
    m.triangleCount = cells * 2;
    m.vertices = MemAlloc(sizeof(float) * 3 * m.vertexCount);
    m.texcoords = MemAlloc(sizeof(float) * 2 * m.vertexCount);
    m.texcoords2 = MemAlloc(sizeof(float) * 2 * m.vertexCount);   /* depth 0: a pool has none in its file; drawn shallow (basins) */
    int v = 0;
    for (int y = 0; y < cy; ++y)
        for (int x = 0; x < cx; ++x) {
            if (wmo_pool_dry(p->cell[y * cx + x])) continue;
            static const int CORNER[6][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 0, 1 } };
            for (int k = 0; k < 6; ++k) {
                int px = x + CORNER[k][0], py = y + CORNER[k][1];
                float bx = p->corner[0] + px * WMO_POOL_CELL, by = p->corner[1] + py * WMO_POOL_CELL, bz = p->height[py * p->nx + px];
                for (int r = 0; r < 3; ++r) m.vertices[v * 3 + r] = at[r] * bx + at[4 + r] * by + at[8 + r] * bz + at[12 + r];
                m.texcoords[v * 2] = (float)px;
                m.texcoords[v * 2 + 1] = (float)py;
                ++v;
            }
        }
    UploadMesh(&m, false);
    if (w->nlayers == w->layers_cap) { w->layers_cap = w->layers_cap ? w->layers_cap * 2 : 128; w->layers = realloc(w->layers, sizeof *w->layers * w->layers_cap); }
    w->layers[w->nlayers].mesh = m;
    w->layers[w->nlayers].kind = kind_of(w, kind);
    w->layers[w->nlayers].tile = tile;
    bounds(&m, &w->layers[w->nlayers].centre, &w->layers[w->nlayers].radius);
    ++w->nlayers;
}
/* }}} */

/* {{{ water_drop_tile */
void water_drop_tile(Water *w, int tile)
{
    int kept = 0;
    for (int i = 0; i < w->nlayers; ++i) {
        if (w->layers[i].tile == tile) { UnloadMesh(w->layers[i].mesh); continue; }
        w->layers[kept++] = w->layers[i];
    }
    w->nlayers = kept;
}
/* }}} */

/* {{{ water_draw
 * Ripples at 15 pictures a second. Drawn with depth testing but without
 * depth writing, see-through, after everything solid. */
void water_draw(Water *w, Vector3 eye, const LightNow *light, double seconds)
{
    SetShaderValue(w->shader, w->loc_eye, &eye, SHADER_UNIFORM_VEC3);
    sky_light_shader(w->shader, light);
    Sight sight;
    sight_from(&sight, MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()));
    rlDisableDepthMask();
    BeginBlendMode(BLEND_ALPHA);
    for (int i = 0; i < w->nlayers; ++i) {
        if (w->layers[i].kind < 0 || !sight_ball(&sight, w->layers[i].centre, w->layers[i].radius)) continue;   /* out of sight: not sent */
        Kind *k = &w->kinds[w->layers[i].kind];
        float raw = k->type >= 2 ? 1.0f : 0.0f;
        int sea = k->type == 1;                         /* the sea's colours and opacity, or a river's */
        float opacity[2] = { light->water_alpha[sea ? 0 : 2], light->water_alpha[sea ? 1 : 3] };
        SetShaderValue(w->shader, w->loc_near, light->water[sea ? 0 : 2], SHADER_UNIFORM_VEC3);
        SetShaderValue(w->shader, w->loc_deep, light->water[sea ? 1 : 3], SHADER_UNIFORM_VEC3);
        SetShaderValue(w->shader, w->loc_opacity, opacity, SHADER_UNIFORM_VEC2);
        SetShaderValue(w->shader, w->loc_raw, &raw, SHADER_UNIFORM_FLOAT);
        if (k->nframes) w->material.maps[MATERIAL_MAP_DIFFUSE].texture = k->frames[(int)(seconds * 15.0) % k->nframes];
        DrawMesh(w->layers[i].mesh, w->material, MatrixIdentity());
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}
/* }}} */
