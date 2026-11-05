/*
 * 040-water.c (issue 173r) - drawing the water; see 040-water.h
 *
 * For a general audience: one mesh per water layer, one run of ripple
 * pictures per kind of water, and a shader that tints and fogs them; a
 * converted map's water, when the player has Warcraft III, with WC3's
 * own run of pictures and each point's colour (082 works the points
 * out, and holds the shader's text).
 */
#include "040-water.h"
#include "013-errors.h"
#include "017-blp.h"
#include "016-dbc.h"
#include "032-sight.h"
#include "037-sky.h"
#include "036-texture.h"
#include "082-water-prep.h"

#include "raymath.h"
#include "rlgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (the shader's text is 082-water-prep.c's WATER_VS and WATER_FS) */

/* {{{ a water drawer (the handle: 040-water.h)
 * kinds: per LiquidType id met so far, its run of pictures and its type
 * (0 water, 1 ocean, 2 magma, 3 slime). layers: one mesh per water layer
 * with the kind it uses (and whether it is drawn as WC3's water); centre,
 * radius: its bounding ball (173u). wc3: a converted map's WC3 water
 * (081), with its frames on the card. */
#define MAX_FRAMES 32
typedef struct { int id, type; Texture2D frames[MAX_FRAMES]; int nframes; } Kind;
typedef struct { Mesh mesh; int kind, tile, wc3; Vector3 centre; float radius; } Layer;
struct Water {
    Kind      kinds[64];  int nkinds;
    Layer    *layers;     int nlayers, layers_cap;
    MpqChain *chain;
    const Textures *textures;        /* how pictures go to the card (036-texture.h) */
    Dbc       table;      unsigned char *table_bytes;
    Shader    shader;
    Material  material;
    int       loc_eye, loc_near, loc_deep, loc_opacity, loc_raw, loc_wc3;
    const Wc3Water *wc3;
    Texture2D wc3_frames[WC3_WATER_FRAMES_MAX];
    int       nwc3_frames;
};
/* }}} */

/* {{{ water_init */
void water_free(Water *w)
{
    for (int i = 0; i < w->nlayers; ++i) UnloadMesh(w->layers[i].mesh);
    for (int i = 0; i < w->nkinds; ++i) for (int f = 0; f < w->kinds[i].nframes; ++f) UnloadTexture(w->kinds[i].frames[f]);
    for (int f = 0; f < w->nwc3_frames; ++f) UnloadTexture(w->wc3_frames[f]);
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
    w->shader = LoadShaderFromMemory(WATER_VS, WATER_FS);
    if (!IsShaderReady(w->shader)) { w_fail(why, W_ERR_GPU, "the water shader didn't compile (see Raylib's messages above)"); water_free(w); return NULL; }
    w->loc_eye = GetShaderLocation(w->shader, "eye");
    w->loc_near = GetShaderLocation(w->shader, "tint_near");
    w->loc_deep = GetShaderLocation(w->shader, "tint_deep");
    w->loc_opacity = GetShaderLocation(w->shader, "opacity");
    w->loc_raw = GetShaderLocation(w->shader, "raw");
    w->loc_wc3 = GetShaderLocation(w->shader, "wc3");
    w->material = LoadMaterialDefault();
    w->material.shader = w->shader;
    return w;
}
/* }}} */

/* {{{ static to_card: a picture (RGBA, rows from the top) as a repeating texture */
static Texture2D to_card(const Water *w, unsigned char *rgba, int width, int height)
{
    Image im = { .data = rgba, .width = width, .height = height, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D t = LoadTextureFromImage(im);
    GenTextureMipmaps(&t);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    texture_filter(w->textures, t.id, 0x0DE1, t.mipmaps);   /* a 2D picture, as Texture Filtering says (173b6) */
    return t;
}
/* }}} */

/* {{{ water_use_wc3 */
void water_use_wc3(Water *w, const Wc3Water *wc3)
{
    for (int f = 0; f < w->nwc3_frames; ++f) UnloadTexture(w->wc3_frames[f]);
    w->nwc3_frames = 0;
    w->wc3 = wc3;
    if (!wc3) return;
    for (const Wc3WaterFrame *fr; (fr = wc3_water_frame(wc3, w->nwc3_frames)); ++w->nwc3_frames)
        w->wc3_frames[w->nwc3_frames] = to_card(w, fr->rgba, fr->width, fr->height);
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
        k->frames[k->nframes++] = to_card(w, img.rgba, img.width, img.height);
        blp_free(&img);
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
 * Each layer's points from 082 (two triangles a wet cell); a water or sea
 * layer drawn as WC3's water when the map has it (water_use_wc3). */
void water_add_tile(Water *w, const AdtTile *t, int tile, double ox, double oy)
{
    for (int i = 0; i < t->nliquids; ++i) {
        int kind = kind_of(w, t->liquids[i].kind);
        int wc3 = w->wc3 && w->nwc3_frames && kind >= 0 && w->kinds[kind].type <= 1;
        WaterVerts pv;
        WWhy why;
        if (water_prep_layer(t, i, ox, oy, wc3 ? w->wc3 : NULL, &pv, &why)) { fprintf(stderr, "water: %s\n", why.text); continue; }
        if (!pv.count) { water_verts_free(&pv); continue; }
        Mesh m = { 0 };
        m.vertexCount = pv.count;
        m.triangleCount = pv.count / 3;
        m.vertices = MemAlloc(sizeof(float) * 3 * m.vertexCount);
        m.texcoords = MemAlloc(sizeof(float) * 2 * m.vertexCount);
        m.texcoords2 = MemAlloc(sizeof(float) * 2 * m.vertexCount);
        memcpy(m.vertices, pv.vertices, sizeof(float) * 3 * m.vertexCount);
        memcpy(m.texcoords, pv.texcoords, sizeof(float) * 2 * m.vertexCount);
        memcpy(m.texcoords2, pv.texcoords2, sizeof(float) * 2 * m.vertexCount);
        if (pv.colors) { m.colors = MemAlloc(4 * m.vertexCount); memcpy(m.colors, pv.colors, 4 * (size_t)m.vertexCount); }
        water_verts_free(&pv);
        UploadMesh(&m, false);
        if (w->nlayers == w->layers_cap) { w->layers_cap = w->layers_cap ? w->layers_cap * 2 : 128; w->layers = realloc(w->layers, sizeof *w->layers * w->layers_cap); }
        w->layers[w->nlayers].mesh = m;
        w->layers[w->nlayers].kind = kind;
        w->layers[w->nlayers].tile = tile;
        w->layers[w->nlayers].wc3 = wc3;
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
    w->layers[w->nlayers].wc3 = 0;                    /* a building's pool: the game's own water */
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
 * Ripples at 15 pictures a second (WC3's water: its own rate, 081).
 * Drawn with depth testing but without
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
        float raw = k->type >= 2 ? 1.0f : 0.0f, wc3 = w->layers[i].wc3 ? 1.0f : 0.0f;
        SetShaderValue(w->shader, w->loc_wc3, &wc3, SHADER_UNIFORM_FLOAT);
        int sea = k->type == 1;                         /* the sea's colours and opacity, or a river's */
        float opacity[2] = { light->water_alpha[sea ? 0 : 2], light->water_alpha[sea ? 1 : 3] };
        SetShaderValue(w->shader, w->loc_near, light->water[sea ? 0 : 2], SHADER_UNIFORM_VEC3);
        SetShaderValue(w->shader, w->loc_deep, light->water[sea ? 1 : 3], SHADER_UNIFORM_VEC3);
        SetShaderValue(w->shader, w->loc_opacity, opacity, SHADER_UNIFORM_VEC2);
        SetShaderValue(w->shader, w->loc_raw, &raw, SHADER_UNIFORM_FLOAT);
        if (w->layers[i].wc3) w->material.maps[MATERIAL_MAP_DIFFUSE].texture = w->wc3_frames[wc3_water_frame_at(w->wc3, seconds)];
        else if (k->nframes) w->material.maps[MATERIAL_MAP_DIFFUSE].texture = k->frames[(int)(seconds * 15.0) % k->nframes];
        DrawMesh(w->layers[i].mesh, w->material, MatrixIdentity());
    }
    EndBlendMode();
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}
/* }}} */
