/*
 * 038-ground.c (issue 173f) - the painted ground; see 038-ground.h
 *
 * For a general audience: turns each ground file's chunks into meshes and
 * pictures on the graphics card, and blends them as the original does.
 */
#include "038-ground.h"
#include "013-errors.h"
#include "020-adt.h"
#include "033-ground-prep.h"
#include "032-sight.h"
#include "037-sky.h"
#include "036-texture.h"

#include "raymath.h"
#include "rlgl.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the shader
 * A tile is drawn in four orders (one a quarter: 64 chunks each), not 256
 * (173u). Every ground picture lives in one texture array (`layers`, one
 * 256 x 256 layer a picture, shared by all tiles); each chunk's four
 * layer numbers ride on its points (Raylib's tangent slot, `lay`, not
 * blended across a triangle). The tile's 256 blend pictures sit in one
 * 1024 x 1024 picture (`blend`), 64 x 64 a chunk; `uv2` reaches only the
 * chunk's own square (from its first point's centre to its last's), so
 * smoothing never borrows a neighbour's. `uv` repeats each picture 8
 * times across a chunk (once every 4.2 yards, as the original tiles them).
 * Blending is the original's: each layer laid over the ones below by its
 * strength, then tinted by the chunk's colours (MCCV; 127 = unchanged, so
 * 0 to 2 times), then darkened where the chunk's baked shadow says (173h:
 * to 60%, the blend picture's alpha), and under each character by a soft
 * round patch (a "blob": its centre, its radius; fading out over 3 yards
 * of height, so a character high above the ground casts none). */
static const char *VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec3 vertexNormal; in vec2 vertexTexCoord; in vec2 vertexTexCoord2; in vec4 vertexColor; in vec4 vertexTangent;\n"
    "uniform mat4 mvp; uniform vec3 eye;\n"
    "out vec3 n; out vec2 uv; out vec2 uv2; out vec3 tint; out float dist; out vec3 pos; flat out vec4 lay;\n"
    "void main() {\n"
    "  pos = vertexPosition; lay = vertexTangent;\n"
    "  n = vertexNormal; uv = vertexTexCoord; uv2 = vertexTexCoord2; tint = vertexColor.rgb * (255.0 / 127.0);\n"
    "  dist = length(vertexPosition - eye);\n"
    "  gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";
static const char *FS =
    "#version 330\n"
    "in vec3 n; in vec2 uv; in vec2 uv2; in vec3 tint; in float dist; in vec3 pos; flat in vec4 lay; out vec4 frag;\n"
    "uniform sampler2D blend; uniform sampler2DArray layers; uniform vec3 sun_dir, sun_col, shade_col, fog_col; uniform vec2 fog_range;\n"
    "uniform vec4 blobs[8]; uniform int nblobs;\n"
    "void main() {\n"
    "  vec4 a = texture(blend, uv2);\n"
    "  vec3 c = texture(layers, vec3(uv, lay.x)).rgb;\n"
    "  c = mix(c, texture(layers, vec3(uv, lay.y)).rgb, a.r);\n"
    "  c = mix(c, texture(layers, vec3(uv, lay.z)).rgb, a.g);\n"
    "  c = mix(c, texture(layers, vec3(uv, lay.w)).rgb, a.b) * tint;\n"
    "  c *= mix(0.6, 1.0, a.a);\n"
    "  for (int i = 0; i < nblobs; ++i) {\n"
    "    float d = length(pos.xy - blobs[i].xy) / blobs[i].w;\n"
    "    float above = clamp(1.0 - abs(pos.z - blobs[i].z) / 3.0, 0.0, 1.0);\n"
    "    c *= 1.0 - 0.6 * (1.0 - smoothstep(0.3, 1.0, d)) * above;\n"
    "  }\n"
    "  vec3 light = shade_col + sun_col * max(dot(normalize(n), sun_dir), 0.0);\n"
    "  float fog = clamp((dist - fog_range.x) / max(fog_range.y - fog_range.x, 1.0), 0.0, 1.0);\n"
    "  frag = vec4(mix(c * light, fog_col, fog), 1.0);\n"
    "}\n";
/* }}} */

/* {{{ the texture array, through OpenGL directly
 * Raylib 5.1 has no texture arrays, but the OpenGL loader it is built with
 * (glad) keeps every OpenGL function in a global pointer, linked into
 * libraylib.a; these are those pointers (under their own names) and the
 * OpenGL numbers they take. Only the main thread may call them, as with
 * all drawing. */
typedef unsigned int GLenum_;
extern void (*glad_glGenTextures)(int n, unsigned int *textures);
extern void (*glad_glBindTexture)(GLenum_ target, unsigned int texture);
extern void (*glad_glTexImage3D)(GLenum_ target, int level, int internalformat, int width, int height, int depth, int border, GLenum_ format, GLenum_ type, const void *pixels);
extern void (*glad_glTexSubImage3D)(GLenum_ target, int level, int x, int y, int z, int width, int height, int depth, GLenum_ format, GLenum_ type, const void *pixels);
extern void (*glad_glTexParameteri)(GLenum_ target, GLenum_ pname, int param);
#define GL_TEXTURE_2D_ARRAY_       0x8C1A
#define GL_RGBA_                   0x1908
#define GL_RGBA8_                  0x8058
#define GL_UNSIGNED_BYTE_          0x1401
#define GL_TEXTURE_WRAP_S_         0x2802
#define GL_TEXTURE_WRAP_T_         0x2803
#define GL_REPEAT_                 0x2901
#define LAYER_SIZE   256              /* nearly every ground picture is 256 x 256 (measured: 139 of 141 round
                                       * Astranaar, Elwynn and Northrend; two tiny ones); others are resized */
#define LAYER_LEVELS 9                /* 256 down to 1 */
#define LAYERS       256              /* layers in the array (about 87 MB with their smaller copies); a tile
                                       * uses 4 to 23; a layer no loaded tile uses is freed for reuse */
#define ARRAY_UNIT   5                /* the texture unit the array is bound to (Raylib's maps use the first ones) */
/* }}} */

/* {{{ a ground (the handle: 038-ground.h)
 * layer_of: every ground picture loaded so far, by archive name, shared by
 * all chunks and tiles that use it (a tile uses 5 to 20; Ashenvale's
 * neighbours share most); refs: loaded tiles using it; name "" free.
 * tiles: what each loaded tile holds. names_lock guards layer_of's names:
 * the reading-ahead thread reads them (ground_on_card). */
typedef struct {
    Mesh      mesh[4];                /* a quarter each: chunks with rows (or columns) 0-7 or 8-15 */
    int       has[4];                 /* 0 for a quarter that is all holes */
    Vector3   centre[4];  float radius[4];   /* each quarter's bounding ball, for the sight test */
    Texture2D blend;                  /* the 256 chunks' blend pictures, 16 x 16 of 64 x 64 */
    int       used[256];  int nused;  /* the layers this tile holds a reference to */
} GroundTile;
struct Ground {
    struct { char name[128]; int refs; } layer_of[LAYERS];
    unsigned int     array_id;
    GroundTile      *tiles[64 * 64];
    GroundPlace      place;           /* the archives, map, heights and origin (033-ground-prep.h) */
    pthread_mutex_t  names_lock;
    Shader           shader;
    Material         material;
    int              loc_eye, loc_blobs, loc_nblobs;
    float            blobs[8][4];
    int              nblobs;
    int              ground_orders;   /* draw orders sent since last asked (for the picture report) */
};
/* }}} */

/* {{{ ground_init */
void ground_free(Ground *g)
{
    for (int i = 0; i < 64 * 64; ++i) {
        int tx = i / 64, ty = i % 64;
        if (g->tiles[i]) ground_drop_tile(g, tx, ty);
    }
    if (IsShaderReady(g->shader)) UnloadShader(g->shader);
    pthread_mutex_destroy(&g->names_lock);
    free(g);
}
Ground *ground_new(const GroundPlace *p, WWhy *why)
{
    Ground *g = calloc(1, sizeof *g);
    pthread_mutex_init(&g->names_lock, NULL);
    g->place = *p;
    g->shader = LoadShaderFromMemory(VS, FS);
    if (!IsShaderReady(g->shader)) { w_fail(why, W_ERR_GPU, "the ground shader didn't compile (see Raylib's messages above)"); ground_free(g); return NULL; }
    g->shader.locs[SHADER_LOC_MAP_ALBEDO] = GetShaderLocation(g->shader, "blend");   /* Raylib binds the material's first map to it */
    int unit = ARRAY_UNIT;
    SetShaderValue(g->shader, GetShaderLocation(g->shader, "layers"), &unit, SHADER_UNIFORM_INT);
    /* the array: LAYERS layers of LAYER_SIZE square, every smaller copy
     * made room for, smoothed between copies, repeating */
    glad_glGenTextures(1, &g->array_id);
    glad_glBindTexture(GL_TEXTURE_2D_ARRAY_, g->array_id);
    for (int lv = 0; lv < LAYER_LEVELS; ++lv)
        glad_glTexImage3D(GL_TEXTURE_2D_ARRAY_, lv, GL_RGBA8_, LAYER_SIZE >> lv, LAYER_SIZE >> lv, LAYERS, 0, GL_RGBA_, GL_UNSIGNED_BYTE_, NULL);
    glad_glTexParameteri(GL_TEXTURE_2D_ARRAY_, GL_TEXTURE_WRAP_S_, GL_REPEAT_);
    glad_glTexParameteri(GL_TEXTURE_2D_ARRAY_, GL_TEXTURE_WRAP_T_, GL_REPEAT_);
    glad_glBindTexture(GL_TEXTURE_2D_ARRAY_, 0);
    texture_filter(g->array_id, GL_TEXTURE_2D_ARRAY_, LAYER_LEVELS);   /* as the Texture Filtering setting says (173b6) */
    g->loc_eye = GetShaderLocation(g->shader, "eye");
    g->loc_blobs = GetShaderLocation(g->shader, "blobs");
    g->loc_nblobs = GetShaderLocation(g->shader, "nblobs");
    g->material = LoadMaterialDefault();
    g->material.shader = g->shader;
    return g;
}
/* }}} */

/* {{{ ground_on_card: whether a picture is in the array now (any thread;
 * the reading-ahead thread asks, so it unpacks only pictures not there) */
int ground_on_card(void *ground, const char *name)
{
    Ground *g = ground;
    pthread_mutex_lock(&g->names_lock);
    int found = 0;
    for (int i = 0; i < LAYERS && !found; ++i) found = g->layer_of[i].name[0] && !strcmp(g->layer_of[i].name, name);
    pthread_mutex_unlock(&g->names_lock);
    return found;
}
/* }}} */

/* {{{ static picture: a ground picture's layer in the array, by archive
 * name, sent once and shared; the caller holds one reference per tile.
 * `px` is its pixels when the preparing unpacked them, or NULL (it was on
 * the card then: if it has gone since, it is unpacked here). A picture not
 * 256 x 256 is resized to it (the two tiny ones seen are plain colours; a
 * larger one would lose detail, and is said). -1 with `why` when it can't
 * be read or the array is full. Main thread only (it sends). */
static int picture(Ground *g, const char *name, GroundPixels *px, WWhy *why)
{
    int free_slot = -1;
    for (int i = 0; i < LAYERS; ++i) {
        if (g->layer_of[i].name[0] && !strcmp(g->layer_of[i].name, name)) return i;
        if (!g->layer_of[i].name[0] && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) { w_fail(why, W_ERR_FULL, "the ground's %d picture layers are all in use by loaded tiles", LAYERS); return -1; }
    GroundPixels own = { 0 };
    if (!px->rgba) {                                     /* gone from the card since it was prepared */
        if (ground_prep_pixels(g->place.chain, name, &own, why)) return -1;   /* a layer number function: below 0 a failure, `why` says it */
        px = &own;
    }
    GroundPixels sized = *px;                            /* the picture as sent: 256 x 256, with its smaller copies */
    Image resized = { 0 };
    if (px->width != LAYER_SIZE || px->height != LAYER_SIZE) {
        if (px->width > LAYER_SIZE || px->height > LAYER_SIZE)
            fprintf(stderr, "ground: %s is %d x %d; shrunk to the array's %d x %d, losing detail\n", name, px->width, px->height, LAYER_SIZE, LAYER_SIZE);
        Image im = { .data = px->rgba, .width = px->width, .height = px->height, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        resized = ImageCopy(im);
        ImageResize(&resized, LAYER_SIZE, LAYER_SIZE);
        sized = (GroundPixels){ resized.data, LAYER_SIZE, LAYER_SIZE, NULL };
        ground_prep_smaller(&sized);
    }
    /* the picture and each smaller copy, into its own layer only (the
     * card's own remaking would redo every layer: 173u) */
    glad_glBindTexture(GL_TEXTURE_2D_ARRAY_, g->array_id);
    glad_glTexSubImage3D(GL_TEXTURE_2D_ARRAY_, 0, 0, 0, free_slot, LAYER_SIZE, LAYER_SIZE, 1, GL_RGBA_, GL_UNSIGNED_BYTE_, sized.rgba);
    const unsigned char *copy = sized.smaller;
    for (int lv = 1; lv < LAYER_LEVELS; ++lv) {
        int side = LAYER_SIZE >> lv;
        glad_glTexSubImage3D(GL_TEXTURE_2D_ARRAY_, lv, 0, 0, free_slot, side, side, 1, GL_RGBA_, GL_UNSIGNED_BYTE_, copy);
        copy += (size_t)side * side * 4;
    }
    glad_glBindTexture(GL_TEXTURE_2D_ARRAY_, 0);
    if (resized.data) { UnloadImage(resized); free(sized.smaller); }
    free(own.rgba); free(own.smaller);
    pthread_mutex_lock(&g->names_lock);
    snprintf(g->layer_of[free_slot].name, sizeof g->layer_of[free_slot].name, "%s", name);
    pthread_mutex_unlock(&g->names_lock);
    g->layer_of[free_slot].refs = 0;
    return free_slot;
}
/* a layer no loaded tile uses any more is freed for reuse */
static void let_go(Ground *g, int layer)
{
    if (--g->layer_of[layer].refs) return;
    pthread_mutex_lock(&g->names_lock);
    g->layer_of[layer].name[0] = '\0';
    pthread_mutex_unlock(&g->names_lock);
}
/* }}} */

/* {{{ ground_load_tile */
WErr ground_load_tile(Ground *g, GroundPrep *p, AdtTile **kept, WWhy *why)
{
    *kept = NULL;
    const AdtTile *adt = p->adt;
    int pic[256];                                     /* the tile's picture numbers: layers in the array */
    GroundTile *gt = calloc(1, sizeof *gt);
    for (int t = 0; t < adt->ntextures; ++t) {
        if (!p->needed[t]) continue;
        pic[t] = picture(g, adt->textures[t], &p->pixels[t], why);
        if (pic[t] < 0) {                             /* `why->text` says which picture; everything this tile took is given back */
            for (int i = 0; i < gt->nused; ++i) let_go(g, gt->used[i]);
            free(gt);
            ground_prep_free(p, 0);
            return why->code;
        }
        ++g->layer_of[pic[t]].refs;
        gt->used[gt->nused++] = pic[t];
    }
    /* each quarter: the prepared arrays become the mesh's (Raylib frees
     * them with it), plus each point's four layer numbers, known now */
    for (int qn = 0; qn < 4; ++qn) {
        GroundQuarter *q = &p->quarter[qn];
        if (!q->made) continue;                       /* all holes: nothing to draw (its arrays go with the preparation) */
        Mesh m = { 0 };
        m.vertexCount = 64 * 145;
        m.tangents = MemAlloc(sizeof(float) * 4 * m.vertexCount);
        for (int qk = 0; qk < 64; ++qk) {
            int k = ((qn / 2) * 8 + qk / 8) * 16 + (qn % 2) * 8 + qk % 8;
            for (int i = 0; i < 145; ++i)
                for (int l = 0; l < 4; ++l) m.tangents[(qk * 145 + i) * 4 + l] = (float)pic[p->chunk_tex[k][l]];
        }
        m.vertices = q->vertices;     q->vertices = NULL;
        m.normals = q->normals;       q->normals = NULL;
        m.texcoords = q->texcoords;   q->texcoords = NULL;
        m.texcoords2 = q->texcoords2; q->texcoords2 = NULL;
        m.colors = q->colors;         q->colors = NULL;
        m.indices = q->indices;       q->indices = NULL;
        m.triangleCount = q->nindex / 3;
        UploadMesh(&m, false);
        gt->mesh[qn] = m;
        gt->has[qn] = 1;
        gt->centre[qn] = (Vector3){ q->centre[0], q->centre[1], q->centre[2] };
        gt->radius[qn] = q->radius;
    }
    Image im = { .data = p->blend, .width = 1024, .height = 1024, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    gt->blend = LoadTextureFromImage(im);
    SetTextureFilter(gt->blend, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(gt->blend, TEXTURE_WRAP_CLAMP);
    *kept = p->adt;
    g->tiles[p->tx * 64 + p->ty] = gt;
    ground_prep_free(p, 1);
    return 0;
}
/* }}} */

/* {{{ ground_drop_tile */
void ground_drop_tile(Ground *g, int tx, int ty)
{
    if (tx < 0 || tx > 63 || ty < 0 || ty > 63) return;
    GroundTile *gt = g->tiles[tx * 64 + ty];
    if (!gt) return;
    for (int q = 0; q < 4; ++q) if (gt->has[q]) UnloadMesh(gt->mesh[q]);
    UnloadTexture(gt->blend);
    for (int i = 0; i < gt->nused; ++i) let_go(g, gt->used[i]);
    free(gt);
    g->tiles[tx * 64 + ty] = NULL;
}
/* }}} */

/* {{{ ground_blob, ground_draw */
void ground_blob(Ground *g, float x, float y, float z, float radius)
{
    if (g->nblobs == 8) return;                            /* the eight nearest are enough (the caller adds nearest first) */
    g->blobs[g->nblobs][0] = x; g->blobs[g->nblobs][1] = y; g->blobs[g->nblobs][2] = z; g->blobs[g->nblobs][3] = radius;
    ++g->nblobs;
}
void ground_draw(Ground *g, Vector3 eye, const LightNow *light)
{
    SetShaderValue(g->shader, g->loc_eye, &eye, SHADER_UNIFORM_VEC3);
    sky_light_shader(g->shader, light);
    SetShaderValueV(g->shader, g->loc_blobs, g->blobs, SHADER_UNIFORM_VEC4, 8);
    SetShaderValue(g->shader, g->loc_nblobs, &g->nblobs, SHADER_UNIFORM_INT);
    g->nblobs = 0;                                         /* blobs are given afresh each frame */
    /* what the camera sees now (we are inside BeginMode3D, the projection set) */
    Sight sight;
    sight_from(&sight, MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()));
    rlActiveTextureSlot(ARRAY_UNIT);
    glad_glBindTexture(GL_TEXTURE_2D_ARRAY_, g->array_id);
    for (int t = 0; t < 64 * 64; ++t) {
        GroundTile *gt = g->tiles[t];
        if (!gt) continue;
        g->material.maps[MATERIAL_MAP_ALBEDO].texture = gt->blend;
        for (int q = 0; q < 4; ++q) {
            if (!gt->has[q] || !sight_ball(&sight, gt->centre[q], gt->radius[q])) continue;   /* out of sight: not sent */
            DrawMesh(gt->mesh[q], g->material, MatrixIdentity());
            ++g->ground_orders;
        }
    }
    rlActiveTextureSlot(ARRAY_UNIT);
    glad_glBindTexture(GL_TEXTURE_2D_ARRAY_, 0);
    rlActiveTextureSlot(0);
}
int ground_orders_last(Ground *g) { int n = g->ground_orders; g->ground_orders = 0; return n; }
/* }}} */
