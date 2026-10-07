/*
 * 042-doodads.c (issue 173g) - trees, rocks and other small models; see 042-doodads.h
 *
 * For a general audience: keeps one entry per model the map uses. Each
 * entry holds the model's pieces (one mesh and one picture each, with how
 * the piece blends) and the list of places it stands. Drawing goes model
 * by model, piece by piece, all copies at once: solid and cut-out pieces
 * first, then see-through and glowing ones over them.
 */
#include "042-doodads.h"
#include "013-errors.h"
#include "023-anim.h"
#include "017-blp.h"
#include "028-collide.h"
#include "022-m2.h"
#include "024-particles.h"
#include "032-sight.h"
#include "036-texture.h"
#include "040-water.h"
#include "037-sky.h"
#include "021-wmo.h"

#include "raymath.h"
#include "rlgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* {{{ the shader
 * A piece lays one or two pictures (`layers`), each from its own set of
 * picture positions (`set0`, `set1`: 0 the first, 1 the second), each
 * slid and stretched by its picture animation (`move0`, `move1`: slide
 * x y, stretch x y), the second joined to the first by `combine` (022-m2.h's
 * m2_combine), then tinted by its animated colour and opacity (`tint`).
 * One copy's place arrives per instance (instanceTransform); `cutoff`
 * cuts out pixels whose alpha is under it (leaves), -1 for none. Lit by
 * the zone's sun and shade (173n), unless the piece is marked unlit.
 * A swaying or camera-facing model is bent here (173u, `skinned`): its
 * points carry four bone numbers (colour slot, a byte each) and weights
 * (tangent slot); its bone poses come as a picture of floats (`bones`),
 * four texels a bone, one row for a swaying model (all copies share it)
 * or one row per copy drawn for a camera-facing one (`per_copy`: the
 * instance's number picks the row).
 * A building's indoor parts are lit by the light baked into their points
 * instead (`indoor`; MOCV colours, plus a little ambient light: this
 * client's first reading of it, to be compared with the original's). */
static const char *VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec3 vertexNormal; in vec2 vertexTexCoord; in vec2 vertexTexCoord2; in vec4 vertexColor; in vec4 vertexTangent; in mat4 instanceTransform;\n"
    "uniform mat4 mvp; uniform vec3 eye; uniform int skinned, per_copy; uniform sampler2D bones;\n"
    "out vec3 n; out vec2 uv; out vec2 uv2; out vec4 col; out float dist;\n"
    "mat4 bone(int i, int row) {\n"
    "  return mat4(texelFetch(bones, ivec2(i * 4, row), 0), texelFetch(bones, ivec2(i * 4 + 1, row), 0),\n"
    "              texelFetch(bones, ivec2(i * 4 + 2, row), 0), texelFetch(bones, ivec2(i * 4 + 3, row), 0));\n"
    "}\n"
    "void main() {\n"
    "  mat4 skin = mat4(1.0);\n"
    "  if (skinned != 0) {\n"
    "    ivec4 id = ivec4(vertexColor * 255.0 + 0.5); vec4 wt = vertexTangent; int row = per_copy != 0 ? gl_InstanceID : 0;\n"
    "    if (wt.x + wt.y + wt.z + wt.w > 0.001) skin = bone(id.x, row) * wt.x + bone(id.y, row) * wt.y + bone(id.z, row) * wt.z + bone(id.w, row) * wt.w;\n"
    "  }\n"
    "  vec4 w = instanceTransform * (skin * vec4(vertexPosition, 1.0));\n"
    "  n = mat3(instanceTransform) * (mat3(skin) * vertexNormal); uv = vertexTexCoord; uv2 = vertexTexCoord2; col = vertexColor;\n"
    "  dist = length(w.xyz - eye);\n"
    "  gl_Position = mvp * w;\n"
    "}\n";
static const char *FS =
    "#version 330\n"
    "in vec3 n; in vec2 uv; in vec2 uv2; in vec4 col; in float dist; out vec4 frag;\n"
    "uniform sampler2D texture0, texture1; uniform float cutoff; uniform float unlit; uniform float indoor; uniform vec3 sun_dir, sun_col, shade_col, fog_col; uniform vec2 fog_range;\n"
    "uniform int layers, combine, set0, set1; uniform vec4 move0, move1, tint;\n"
    "void main() {\n"
    "  vec2 a = (set0 == 1 ? uv2 : uv) * move0.zw + move0.xy;\n"
    "  vec4 t = texture(texture0, a);\n"
    "  if (layers > 1) {\n"
    "    vec4 u = texture(texture1, (set1 == 1 ? uv2 : uv) * move1.zw + move1.xy);\n"
    "    if (combine == 0) t = u;\n"
    "    else if (combine == 1) t = t * u;\n"
    "    else if (combine == 2) t = vec4(mix(t.rgb, u.rgb, u.a), t.a);\n"
    "    else if (combine == 3 || combine == 7) t = vec4(t.rgb + u.rgb, combine == 3 ? t.a + u.a : t.a);\n"
    "    else if (combine == 4) t = t * u * 2.0;\n"
    "    else if (combine == 5) t = vec4(mix(t.rgb, u.rgb, u.a), t.a);\n"
    "    else t = vec4(t.rgb * u.rgb * 2.0, t.a);\n"
    "  }\n"
    "  t *= tint;\n"
    "  if (t.a < cutoff) discard;\n"
    "  vec3 light = mix(shade_col + sun_col * max(dot(normalize(n), sun_dir), 0.0), vec3(1.0), unlit);\n"
    "  light = mix(light, col.rgb + 0.35, indoor);\n"
    "  float fog = clamp((dist - fog_range.x) / max(fog_range.y - fog_range.x, 1.0), 0.0, 1.0);\n"
    "  frag = vec4(mix(t.rgb * light, fog_col, fog), t.a);\n"
    "}\n";
/* }}} */

/* {{{ state
 * A model's piece: its mesh, its picture (into pictures[]), how it blends
 * (M2Unit's blend and flags, 022-m2.h). A model: its pieces and its copies'
 * places (as Raylib matrices, relative to the drawing origin), each with
 * the tile that brought it. Which tile places a doodad listed by two is
 * kept with the placements, below (seen_list). */
typedef struct {
    Mesh mesh; int picture; uint16_t blend, flags; int indoor;
    int picture2, layers, combine;    /* the second layer's picture (-1 none), how many, how joined */
    int set[2], uvanim[2];            /* each layer's picture-position set and animation (-1 none) */
    int colour, fade;                 /* its animated tint's tracks (022-m2.h), -1 none */
} DoodadPiece;
/* A model that moves while standing (trees swaying, lamps flickering)
 * also keeps its skeleton; every frame its bones are posed once and all
 * its copies share that pose, as the original plays them, in step; the
 * graphics card bends the points (173u). `anim` is NULL for a model that
 * doesn't move. */
typedef struct {
    char    name[260];
    DoodadPiece  *pieces;   int npieces;
    Matrix *copies;   int ncopies, cap;
    int    *copy_tile;                /* per copy: the map tile that brought it (173s) */
    Anim   *anim;     int stand;
    int     moving;                   /* its stand animation moves bones: bend it */
    int     needs_anim;               /* a piece's pictures move, or its tint is animated: keep the skeleton's file */
    /* camera-facing bones (glow orbs on lamps, wisps): each copy is turned
     * its own way, so each copy drawn is posed alone, into its own row of
     * the bone picture (173u) */
    int     billboard;
    float (*poses)[16];
    Texture2D bones;  int bone_rows;  float *bone_data;   /* the bone picture (rows of nbones * 4 float texels) and its copy here */
    /* particles (173o): the model's emitters, each one's picture, and one
     * run per emitter per copy (copy * nemitters + emitter), made when
     * first needed and grown as copies are added */
    Emitter    *emitters;  int nemitters;
    int        *emit_pic;
    EmitterRun *runs;      int nruns;
    /* its solid shape (173q): triangles, 9 floats each, model space; every
     * copy placed adds them, placed, to the collision world */
    float      *solid;     int nsolid;
    float       radius;               /* the farthest point from its middle, as made (for skyboxes and the sight test) */
    /* the copies within the camera's sight this frame (173u): their places,
     * and which copy each is (for camera-facing models' own meshes) */
    Matrix     *vis;  int *vis_idx;  int nvis, vis_cap;
} DoodadModel;
#define PARTICLE_REACH 150.0f         /* yards: copies further from the camera run no particles */

static MpqChain *chain;
static CollideWorld *solids;          /* where placed copies' solid shapes go (doodads_init) */
static DoodadModel    *models;   static int nmodels, models_cap;
static struct { char name[260]; Texture2D tex; } *pictures;
static int       npictures, pictures_cap;
/* buildings: the model entry holding its pieces, and its root (for the
 * furnishings, placed when the building is) */
typedef struct { WmoPool pool; int kind; } BuildingPool;   /* a part's pool and its LiquidType id (173r) */
typedef struct { int model; WmoRoot root; BuildingPool *pools; int npools; } Building;
static LiquidWorld *liquids;          /* where placed buildings' pools go (doodads_init) */
static Water       *pools;            /* where placed buildings' pools are drawn (doodads_init) */
static double origin_x, origin_y;     /* the drawing origin, from doodads_add_tile (pools are filed in world coordinates) */
static Building *buildings; static int nbuildings, buildings_cap;
static int       skipped_pieces, moving_models, emitting_models;
static int       doodad_orders;       /* draw orders sent since last asked (for the picture report, 173u) */
static Shader    shader;
static Material  material;
static int       loc_eye, loc_cutoff, loc_unlit, loc_indoor, loc_layers, loc_combine, loc_set0, loc_set1, loc_move0, loc_move1, loc_tint, loc_skinned, loc_per_copy;
static uint32_t  clock_ms;            /* the world's animation clock (doodads_animate) */
static int       skybox = -1;         /* the model drawn round the camera (doodads_skybox), -1 none */
static Vector3   view_right = { 1, 0, 0 }, view_up = { 0, 0, 1 };   /* the camera's, for particles (doodads_animate) */
/* }}} */

/* {{{ doodads_init */
WErr doodads_init(MpqChain *c, CollideWorld *collide, LiquidWorld *liquid_world, Water *water, WWhy *why)
{
    chain = c;
    solids = collide;
    liquids = liquid_world;
    pools = water;
    shader = LoadShaderFromMemory(VS, FS);
    if (!IsShaderReady(shader)) { w_fail(why, W_ERR_GPU, "the doodad shader didn't compile (see Raylib's messages above)"); return why->code; }
    shader.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(shader, "instanceTransform");
    loc_eye = GetShaderLocation(shader, "eye");
    loc_cutoff = GetShaderLocation(shader, "cutoff");
    loc_unlit = GetShaderLocation(shader, "unlit");
    loc_indoor = GetShaderLocation(shader, "indoor");
    loc_layers = GetShaderLocation(shader, "layers");
    loc_combine = GetShaderLocation(shader, "combine");
    loc_set0 = GetShaderLocation(shader, "set0");
    loc_set1 = GetShaderLocation(shader, "set1");
    loc_move0 = GetShaderLocation(shader, "move0");
    loc_move1 = GetShaderLocation(shader, "move1");
    loc_tint = GetShaderLocation(shader, "tint");
    loc_skinned = GetShaderLocation(shader, "skinned");
    loc_per_copy = GetShaderLocation(shader, "per_copy");
    shader.locs[SHADER_LOC_MAP_NORMAL] = GetShaderLocation(shader, "bones");   /* use_bones puts the model's bone picture in that slot */
    shader.locs[SHADER_LOC_MAP_METALNESS] = GetShaderLocation(shader, "texture1");   /* the second layer's slot */
    material = LoadMaterialDefault();
    material.shader = shader;
    return 0;
}
/* }}} */

/* {{{ static picture: a picture by archive name, loaded once; -1 if unreadable */
static int picture(const char *name)
{
    for (int i = 0; i < npictures; ++i) if (!strcasecmp(pictures[i].name, name)) return i;
    size_t len;
    const char *from;
    unsigned char *s = mpq_chain_read(chain, name, &len, &from);
    if (!s) { fprintf(stderr, "doodads: the picture %s is in none of the client's archives; pieces using it are left out\n", name); return -1; }
    WWhy why;
    Texture2D t;
    int bad = texture_from_blp(s, len, &t, &why);   /* sent compressed when stored so (173u) */
    free(s);
    if (bad) { fprintf(stderr, "doodads: the picture %s: %s; pieces using it are left out\n", name, why.text); return -1; }
    if (npictures == pictures_cap) {
        pictures_cap = pictures_cap ? pictures_cap * 2 : 256;
        pictures = realloc(pictures, sizeof *pictures * pictures_cap);
    }
    snprintf(pictures[npictures].name, sizeof pictures[npictures].name, "%s", name);
    pictures[npictures].tex = t;
    return npictures++;
}
/* }}} */

/* {{{ static load_model: a model by its MMDX name, loaded once
 * The ground files name models with .mdx or .m2; the file in the archives
 * is always .m2, with its first view in <name>00.skin. A model that can't
 * be read is kept with no pieces (so it is reported once, not per copy). */
static int load_model(const char *name)
{
    for (int i = 0; i < nmodels; ++i) if (!strcasecmp(models[i].name, name)) return i;
    if (nmodels == models_cap) {
        models_cap = models_cap ? models_cap * 2 : 256;
        models = realloc(models, sizeof *models * models_cap);
    }
    DoodadModel *md = &models[nmodels];
    memset(md, 0, sizeof *md);
    snprintf(md->name, sizeof md->name, "%s", name);

    char base[260], path[280];
    snprintf(base, sizeof base, "%s", name);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    size_t mlen, slen;
    const char *from;
    snprintf(path, sizeof path, "%s.m2", base);
    unsigned char *m2 = mpq_chain_read(chain, path, &mlen, &from);
    snprintf(path, sizeof path, "%s00.skin", base);
    unsigned char *skin = mpq_chain_read(chain, path, &slen, &from);
    M2Model mod;
    WWhy why;
    if (!m2 || !skin) {
        fprintf(stderr, "doodads: %s: %s is in none of the client's archives; left out\n", name, !m2 ? "the .m2" : "its 00.skin");
        free(m2); free(skin);
        return nmodels++;
    }
    int bad = m2_read(m2, mlen, skin, slen, &mod, &why);
    free(skin);
    if (bad) { free(m2); fprintf(stderr, "doodads: %s: %s; left out\n", name, why.text); return nmodels++; }

    /* does it move while standing? (its stand animation, id 0, moving
     * any bone) If so it keeps its skeleton and is bent every frame. */
    Anim *an = calloc(1, sizeof *an);
    int moving = 0;
    if (!anim_read(m2, mlen, an, &why)) {
        md->stand = anim_find(an, 0, 0);
        if (md->stand >= 0 && anim_needs_file(an, md->stand)) {
            snprintf(path, sizeof path, "%s%04d-%02d.anim", base, an->seqs[md->stand].id, an->seqs[md->stand].variation);
            size_t alen;
            unsigned char *af = mpq_chain_read(chain, path, &alen, &from);
            if (af) anim_attach_file(an, md->stand, af, alen);
            else fprintf(stderr, "doodads: %s is in none of the archives; %s stands still\n", path, name);
        }
        moving = md->stand >= 0 && an->npoints == mod.nvertices && anim_moves(an, md->stand);
        /* its particle emitters, each with its picture (the model's own
         * picture slot, named in the file) */
        if (particles_read(an, &md->emitters, &md->nemitters, &why))
            fprintf(stderr, "doodads: %s's particles: %s; it shows none\n", name, why.text);
        if (md->nemitters) {
            md->emit_pic = malloc(sizeof(int) * md->nemitters);
            for (int i = 0; i < md->nemitters; ++i) {
                int slot = md->emitters[i].slot;
                md->emit_pic[i] = (slot < (int)mod.ntextures && mod.textures[slot].kind == 0 && mod.textures[slot].file[0])
                                  ? picture(mod.textures[slot].file) : -1;
                if (md->emit_pic[i] < 0) fprintf(stderr, "doodads: %s's emitter %d has no picture it can use; it shows none\n", name, i);
            }
            ++emitting_models;
        }
    } else {
        fprintf(stderr, "doodads: %s's skeleton: %s; it stands still\n", name, why.text);
    }
    if (m2_collision(m2, mlen, &md->solid, &md->nsolid, &why))
        fprintf(stderr, "doodads: %s's solid shape: %s; it is walked through\n", name, why.text);
    free(m2);
    md->moving = moving;
    for (uint32_t i = 0; i < mod.nvertices; ++i) {
        float r = sqrtf(mod.vertices[i].x * mod.vertices[i].x + mod.vertices[i].y * mod.vertices[i].y + mod.vertices[i].z * mod.vertices[i].z);
        if (r > md->radius) md->radius = r;
    }
    md->billboard = an->npoints == mod.nvertices && anim_has_billboards(an);   /* an is all zero when its skeleton failed to read */
    /* pieces whose pictures move, or whose tint is animated, read their
     * tracks from the skeleton's file too */
    for (uint32_t u = 0; u < mod.nunits; ++u)
        if (mod.units[u].uvanim[0] >= 0 || mod.units[u].uvanim[1] >= 0 || mod.units[u].colour >= 0 || mod.units[u].fade >= 0) md->needs_anim = 1;
    if (an->m2 == NULL) md->needs_anim = 0;               /* its skeleton failed to read: nothing moves */
    if (moving || md->nemitters || md->billboard || md->needs_anim) {
        md->anim = an;
        md->poses = malloc(sizeof *md->poses * (an->nbones ? an->nbones : 1));
    }
    if (moving) ++moving_models;
    if (!md->anim) {
        anim_free(an);
        free(an);
    }

    md->pieces = calloc(mod.nunits ? mod.nunits : 1, sizeof *md->pieces);
    for (uint32_t u = 0; u < mod.nunits; ++u) {
        M2Unit un = mod.units[u];
        if (un.piece >= mod.npieces || un.texture >= mod.ntextures) continue;
        if (mod.textures[un.texture].kind != 0) { ++skipped_pieces; continue; }   /* picture chosen at run time */
        int pic = picture(mod.textures[un.texture].file);
        int layers = un.layers > 1 && un.tex[1] >= 0 && mod.textures[un.tex[1]].kind == 0 && mod.textures[un.tex[1]].file[0] ? 2 : 1;
        int pic2 = layers > 1 ? picture(mod.textures[un.tex[1]].file) : -1;
        if (layers > 1 && pic2 < 0) layers = 1;
        if (pic < 0) continue;
        M2Piece p = mod.pieces[un.piece];
        if (p.first + p.count > mod.nindices || p.count == 0) continue;
        Mesh m = { 0 };
        m.vertexCount = (int)p.count;
        m.triangleCount = (int)p.count / 3;
        m.vertices  = MemAlloc(sizeof(float) * 3 * p.count);
        m.normals   = MemAlloc(sizeof(float) * 3 * p.count);
        m.texcoords = MemAlloc(sizeof(float) * 2 * p.count);
        m.texcoords2 = MemAlloc(sizeof(float) * 2 * p.count);
        int ok = 1;
        for (uint32_t i = 0; i < p.count; ++i) {
            uint32_t k = mod.indices[p.first + i];
            if (k >= mod.nvertices) { ok = 0; break; }
            M2Vertex v = mod.vertices[k];
            m.vertices[i * 3] = v.x; m.vertices[i * 3 + 1] = v.y; m.vertices[i * 3 + 2] = v.z;
            m.normals[i * 3] = v.nx; m.normals[i * 3 + 1] = v.ny; m.normals[i * 3 + 2] = v.nz;
            m.texcoords[i * 2] = v.u; m.texcoords[i * 2 + 1] = v.v;
            m.texcoords2[i * 2] = v.u2; m.texcoords2[i * 2 + 1] = v.v2;
        }
        if (!ok) { UnloadMesh(m); fprintf(stderr, "doodads: %s: a piece names a point the model doesn't have; piece left out\n", name); continue; }
        if (md->moving || md->billboard) {
            /* its points' bones and weights, for the card to bend them (173u) */
            m.colors = MemAlloc(4 * p.count);
            m.tangents = MemAlloc(sizeof(float) * 4 * p.count);
            for (uint32_t i = 0; i < p.count; ++i) {
                uint32_t k = mod.indices[p.first + i];
                for (int j = 0; j < 4; ++j) {
                    m.colors[i * 4 + j] = md->anim->bone_of[k][j];
                    m.tangents[i * 4 + j] = md->anim->weights[k][j] / 255.0f;
                }
            }
        }
        UploadMesh(&m, false);                             /* sent once; never changes */
        md->pieces[md->npieces++] = (DoodadPiece){ m, pic, un.blend, un.flags, 0, pic2, layers, m2_combine(un.shader),
                                                   { un.uvset[0] == 1, un.uvset[1] == 1 }, { un.uvanim[0], layers > 1 ? un.uvanim[1] : -1 },
                                                   un.colour, un.fade };
    }
    m2_free(&mod);
    return nmodels++;
}
/* }}} */

/* {{{ static load_building: a building by name, loaded once
 * Its pieces are every group's batches, kept as one model entry so its
 * copies draw like any doodad's. A group or picture that can't be read is
 * reported and left out; a root that can't be read leaves the building
 * out (reported once). */
static int load_building(const char *name)
{
    for (int i = 0; i < nbuildings; ++i) if (!strcasecmp(models[buildings[i].model].name, name)) return i;
    if (nbuildings == buildings_cap) {
        buildings_cap = buildings_cap ? buildings_cap * 2 : 64;
        buildings = realloc(buildings, sizeof *buildings * buildings_cap);
    }
    if (nmodels == models_cap) {
        models_cap = models_cap ? models_cap * 2 : 256;
        models = realloc(models, sizeof *models * models_cap);
    }
    Building *b = &buildings[nbuildings];
    memset(b, 0, sizeof *b);
    b->model = nmodels;
    DoodadModel *md = &models[nmodels++];
    memset(md, 0, sizeof *md);
    snprintf(md->name, sizeof md->name, "%s", name);

    size_t len;
    const char *from;
    char path[300];
    WWhy why;
    unsigned char *s = mpq_chain_read(chain, name, &len, &from);
    if (!s) { fprintf(stderr, "buildings: %s is in none of the client's archives; left out\n", name); return nbuildings++; }
    int bad = wmo_read_root(s, len, &b->root, &why);
    free(s);
    if (bad) { fprintf(stderr, "buildings: %s: %s; left out\n", name, why.text); return nbuildings++; }

    char base[260];
    snprintf(base, sizeof base, "%s", name);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    for (int gi = 0; gi < b->root.ngroups; ++gi) {
        snprintf(path, sizeof path, "%s_%03d.wmo", base, gi);
        s = mpq_chain_read(chain, path, &len, &from);
        if (!s) { fprintf(stderr, "buildings: %s is in none of the client's archives; that part is left out\n", path); continue; }
        WmoGroup g;
        bad = wmo_read_group(s, len, &g, &why);
        free(s);
        if (bad) { fprintf(stderr, "buildings: %s: %s; that part is left out\n", path, why.text); continue; }
        /* how far it reaches from its middle (for the sight test, 173u) */
        for (uint32_t v = 0; v < g.nvertices; ++v) {
            float r = sqrtf(g.vertices[v].x * g.vertices[v].x + g.vertices[v].y * g.vertices[v].y + g.vertices[v].z * g.vertices[v].z);
            if (r > md->radius) md->radius = r;
        }
        /* its solid triangles (173q), kept in the building's own space */
        int nt = (int)(g.nindices / 3);
        md->solid = realloc(md->solid, sizeof(float) * 9 * (size_t)(md->nsolid + nt + 1));
        for (int t = 0; t < nt; ++t) {
            if ((uint32_t)t >= g.nflags || !wmo_solid(g.tri_flags[t])) continue;
            for (int c = 0; c < 3; ++c) {
                WmoVertex v = g.vertices[g.indices[t * 3 + c]];
                float *o = md->solid + (size_t)md->nsolid * 9 + c * 3;
                o[0] = v.x; o[1] = v.y; o[2] = v.z;
            }
            ++md->nsolid;
        }
        md->pieces = realloc(md->pieces, sizeof *md->pieces * (md->npieces + g.nbatches + 1));
        for (int k = 0; k < g.nbatches; ++k) {
            WmoBatch bt = g.batches[k];
            if (bt.material >= b->root.nmaterials || bt.count == 0) continue;
            WmoMaterial mat = b->root.materials[bt.material];
            int pic = picture(mat.picture);
            if (pic < 0) continue;
            Mesh m = { 0 };
            m.vertexCount = (int)bt.count;
            m.triangleCount = (int)bt.count / 3;
            m.vertices  = MemAlloc(sizeof(float) * 3 * bt.count);
            m.normals   = MemAlloc(sizeof(float) * 3 * bt.count);
            m.texcoords = MemAlloc(sizeof(float) * 2 * bt.count);
            m.colors    = MemAlloc(4 * bt.count);
            for (uint32_t i = 0; i < bt.count; ++i) {
                WmoVertex v = g.vertices[g.indices[bt.first + i]];
                m.vertices[i * 3] = v.x; m.vertices[i * 3 + 1] = v.y; m.vertices[i * 3 + 2] = v.z;
                m.normals[i * 3] = v.nx; m.normals[i * 3 + 1] = v.ny; m.normals[i * 3 + 2] = v.nz;
                m.texcoords[i * 2] = v.u; m.texcoords[i * 2 + 1] = v.v;
                m.colors[i * 4] = v.r; m.colors[i * 4 + 1] = v.g; m.colors[i * 4 + 2] = v.b; m.colors[i * 4 + 3] = 255;
            }
            UploadMesh(&m, false);
            int indoor = (g.flags & 0x2000) && g.has_colours;
            md->pieces[md->npieces++] = (DoodadPiece){ m, pic, (uint16_t)mat.blend, (uint16_t)mat.flags, indoor, -1, 1, 1, { 0, 0 }, { -1, -1 }, -1, -1 };
        }
        /* its pool, if any, kept (the grid taken from the group) with its kind */
        if (g.pool.nx) {
            int kind = wmo_liquid_kind(&b->root, &g);
            if (!kind) fprintf(stderr, "buildings: %s's pool has no kind of liquid by the server's rule; left out\n", path);
            else {
                b->pools = realloc(b->pools, sizeof *b->pools * (b->npools + 1));
                b->pools[b->npools++] = (BuildingPool){ g.pool, kind };
                g.pool.height = NULL; g.pool.cell = NULL;
            }
        }
        wmo_free_group(&g);
    }
    return nbuildings++;
}
/* }}} */

/* {{{ placements by tile (173s)
 * Each loaded tile keeps its own list of placements, every one it lists
 * (a doodad or building on two tiles is in both lists, with one unique
 * id). A placement is "realised" (copies made) by the first tile that
 * meets it; seen[] says which tile that was. Dropping a tile removes the
 * copies it realised, then lets the remaining tiles realise again, so a
 * shared doodad stays while either of its tiles is loaded.
 * A Placement: uid, building (0 doodad, 1 building), which (model or
 * building number), at (where), set (a building's furnishing set). */
typedef struct { uint32_t uid; int building, which, set; Matrix at; } Placement;
typedef struct { int tile; Placement *p; int n; } TileRecord;
static TileRecord *records;  static int nrecords;
typedef struct { uint32_t uid; int building, tile; } Seen;
static Seen *seen_list;      static int nseen_list, seen_list_cap;
/* seen_index: where each placement sits in seen_list, by its uid and kind
 * (a hash table, open addressing; slots hold a position + 1, 0 empty),
 * so "placed already?" is a lookup, not a search through every
 * placement seen (about 7,000 round Astranaar: up to 10 ms a tile load
 * as a search, measured 2026-10-07; 173u). Rebuilt whole when it grows
 * and when a tile is dropped (rare). */
static int *seen_index;      static int seen_index_cap;   /* a power of two, at least twice nseen_list */
static unsigned seen_slot(uint32_t uid, int building) { return (uid * 2654435761u ^ (unsigned)building * 0x9E3779B9u) & (unsigned)(seen_index_cap - 1); }
static int seen_find(uint32_t uid, int building)
{
    if (!seen_index_cap) return -1;
    for (unsigned s = seen_slot(uid, building); seen_index[s]; s = (s + 1) & (unsigned)(seen_index_cap - 1)) {
        const Seen *e = &seen_list[seen_index[s] - 1];
        if (e->uid == uid && e->building == building) return seen_index[s] - 1;
    }
    return -1;
}
static void seen_reindex(void)
{
    while (seen_index_cap < (nseen_list + 1) * 2) seen_index_cap = seen_index_cap ? seen_index_cap * 2 : 8192;
    seen_index = realloc(seen_index, sizeof *seen_index * (size_t)seen_index_cap);
    memset(seen_index, 0, sizeof *seen_index * (size_t)seen_index_cap);
    for (int i = 0; i < nseen_list; ++i) {
        unsigned s = seen_slot(seen_list[i].uid, seen_list[i].building);
        while (seen_index[s]) s = (s + 1) & (unsigned)(seen_index_cap - 1);
        seen_index[s] = i + 1;
    }
}
/* }}} */

/* {{{ static add_copy: one more place for model `which`, brought by `tile` */
static void add_copy(int which, Matrix at, int tile)
{
    DoodadModel *md = &models[which];
    if (md->ncopies == md->cap) {
        md->cap = md->cap ? md->cap * 2 : 16;
        md->copies = realloc(md->copies, sizeof *md->copies * md->cap);
        md->copy_tile = realloc(md->copy_tile, sizeof *md->copy_tile * md->cap);
    }
    md->copy_tile[md->ncopies] = tile;
    md->copies[md->ncopies++] = at;
    if (solids && md->nsolid) collide_add(solids, md->solid, md->nsolid, MatrixToFloatV(at).v, tile);
}
/* }}} */

/* {{{ static realise: a tile's placements not yet placed by another tile */
static void realise(const TileRecord *r)
{
    for (int k = 0; k < r->n; ++k) {
        const Placement *p = &r->p[k];
        if (seen_find(p->uid, p->building) >= 0) continue;   /* another tile placed it already */
        if (nseen_list == seen_list_cap) { seen_list_cap = seen_list_cap ? seen_list_cap * 2 : 4096; seen_list = realloc(seen_list, sizeof *seen_list * seen_list_cap); }
        seen_list[nseen_list++] = (Seen){ p->uid, p->building, r->tile };
        if (seen_index_cap < nseen_list * 2) seen_reindex();
        else {
            unsigned s = seen_slot(p->uid, p->building);
            while (seen_index[s]) s = (s + 1) & (unsigned)(seen_index_cap - 1);
            seen_index[s] = nseen_list;
        }
        if (!p->building) { add_copy(p->which, p->at, r->tile); continue; }
        /* a building, and the furnishings of the set it asks for (set 0
         * holds what every placement shows, so it is always added) */
        Building *b = &buildings[p->which];
        add_copy(b->model, p->at, r->tile);
        float m[16];
        memcpy(m, MatrixToFloatV(p->at).v, sizeof m);
        for (int k2 = 0; liquids && k2 < b->npools; ++k2) {   /* its pools (173r) */
            water_add_pool(pools, r->tile, m, &b->pools[k2].pool, b->pools[k2].kind);
            liquid_add_pool(liquids, r->tile, m, origin_x, origin_y, &b->pools[k2].pool, b->pools[k2].kind);
        }
        for (int si = 0; si < b->root.nsets; ++si) {
            if (si != 0 && si != p->set) continue;
            WmoSet set = b->root.sets[si];
            for (int fi = set.first; fi < set.first + set.count; ++fi) {
                WmoFurnishing f = b->root.furnishings[fi];
                if (!f.model[0]) continue;
                float fm[16];
                adt_inside_matrix(m, f.pos, f.quat, f.scale, fm);
                int which = load_model(f.model);
                b = &buildings[p->which];                       /* (load_model never moves buildings, but stay safe) */
                add_copy(which, (Matrix){ fm[0], fm[4], fm[8], fm[12], fm[1], fm[5], fm[9], fm[13], fm[2], fm[6], fm[10], fm[14], fm[3], fm[7], fm[11], fm[15] }, r->tile);
            }
        }
    }
}
/* }}} */

/* {{{ doodads_add_tile */
WErr doodads_add_tile(const AdtTile *t, int tile, double ox, double oy, WWhy *why)
{
    (void)why;
    origin_x = ox; origin_y = oy;
    records = realloc(records, sizeof *records * (nrecords + 1));
    TileRecord *r = &records[nrecords++];
    r->tile = tile;
    r->n = 0;
    r->p = malloc(sizeof *r->p * (size_t)(t->ndoodads + t->nplaces + 1));
    for (int k = 0; k < t->ndoodads; ++k) {
        const AdtPlace *p = &t->doodads[k];
        float m[16];
        adt_place_matrix(p, ox, oy, m);
        int which = load_model(t->models[p->name]);
        r->p[r->n++] = (Placement){ p->uid, 0, which, 0, { m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15] } };
    }
    for (int k = 0; k < t->nplaces; ++k) {
        const AdtPlace *p = &t->places[k];
        float m[16];
        adt_place_matrix(p, ox, oy, m);
        int bi = load_building(t->buildings[p->name]);
        r->p[r->n++] = (Placement){ p->uid, 1, bi, p->doodad_set, { m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15] } };
    }
    realise(r);
    return 0;
}
/* }}} */

/* {{{ doodads_drop_tile
 * Every model's copies from this tile go (with their particle runs and
 * their own meshes, for camera-facing models), its solid triangles go, its
 * record goes; then the remaining tiles realise what only it had placed. */
void doodads_drop_tile(int tile)
{
    for (int i = 0; i < nmodels; ++i) {
        DoodadModel *md = &models[i];
        int kept = 0, gone = 0;
        /* particle runs exist for a leading run of copies only (made as
         * needed); the kept copies among those stay a leading run */
        int run_copies = md->nemitters ? md->nruns / md->nemitters : 0;
        int kept_runs = 0;
        for (int c = 0; c < md->ncopies; ++c) {
            if (md->copy_tile[c] == tile) {
                ++gone;
                if (c < run_copies)
                    for (int k = 0; k < md->nemitters; ++k) particles_free_run(&md->runs[c * md->nemitters + k]);
                continue;
            }
            if (kept != c) {
                md->copies[kept] = md->copies[c];
                md->copy_tile[kept] = md->copy_tile[c];
                if (c < run_copies)
                    for (int k = 0; k < md->nemitters; ++k) md->runs[kept * md->nemitters + k] = md->runs[c * md->nemitters + k];
            }
            if (c < run_copies) ++kept_runs;
            ++kept;
        }
        if (!gone) continue;
        md->ncopies = kept;
        md->nruns = kept_runs * md->nemitters;
    }
    if (solids) collide_drop_tile(solids, tile);
    int k2 = 0;
    for (int i = 0; i < nseen_list; ++i) if (seen_list[i].tile != tile) seen_list[k2++] = seen_list[i];
    nseen_list = k2;
    seen_reindex();                                      /* positions moved */
    for (int i = 0; i < nrecords; ++i)
        if (records[i].tile == tile) { free(records[i].p); records[i] = records[--nrecords]; break; }
    for (int i = 0; i < nrecords; ++i) realise(&records[i]);
}
/* }}} */

/* {{{ static piece_tint: a piece's animated colour and opacity now, on the
 * model's stand animation (023-anim.h's anim_unit_tint) */
static void piece_tint(const DoodadModel *md, const DoodadPiece *p, float rgba[4])
{
    anim_unit_tint(md->anim, p->colour, p->fade, md->stand, clock_ms, clock_ms, rgba);
}
/* }}} */

/* {{{ static bone_rows: the model's bone picture, with room for `rows`
 * rows (made, or made again larger; 173u) */
static void bone_rows(DoodadModel *md, int rows)
{
    if (md->bone_rows >= rows) return;
    if (md->bones.id) UnloadTexture(md->bones);
    int nb = md->anim->nbones ? md->anim->nbones : 1;
    md->bone_data = realloc(md->bone_data, sizeof(float) * 16 * (size_t)nb * (size_t)rows);
    for (size_t e = 0; e < (size_t)16 * nb * rows; ++e) md->bone_data[e] = (e % 16) % 5 == 0 ? 1.0f : 0.0f;   /* at rest */
    Image im = { .data = md->bone_data, .width = nb * 4, .height = rows, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32 };
    md->bones = LoadTextureFromImage(im);
    SetTextureFilter(md->bones, TEXTURE_FILTER_POINT);
    md->bone_rows = rows;
}
static void pose_copies(DoodadModel *md, Vector3 eye);   /* below, with the animating */
/* static use_bones: the shader told whether this model is bent, and how */
static void use_bones(const DoodadModel *md)
{
    int skinned = md->bones.id && (md->moving || md->billboard), per_copy = md->billboard;
    SetShaderValue(shader, loc_skinned, &skinned, SHADER_UNIFORM_INT);
    SetShaderValue(shader, loc_per_copy, &per_copy, SHADER_UNIFORM_INT);
    material.maps[MATERIAL_MAP_NORMAL].texture = skinned ? md->bones : (Texture2D){ 0 };
}
/* }}} */

/* {{{ doodads_draw
 * Two passes: blend 0 and 1 (solid, cut-out) writing depth; then 2-6
 * (see-through, added, multiplied) over them without writing depth. */
static void draw_pass(int see_through)
{
    for (int i = 0; i < nmodels; ++i) {
        DoodadModel *md = &models[i];
        if (!md->nvis) continue;                          /* none in sight (or none placed) */
        use_bones(md);
        for (int k = 0; k < md->npieces; ++k) {
            DoodadPiece *p = &md->pieces[k];
            if ((p->blend >= 2) != see_through) continue;
            float cutoff = p->blend == 0 ? -1.0f : p->blend == 1 ? 0.5f : 1.0f / 255.0f;
            float unlit = (p->flags & 0x1) ? 1.0f : 0.0f, indoor = p->indoor ? 1.0f : 0.0f;
            /* its layers, their picture animations and its tint, now */
            float move0[4] = { 0, 0, 1, 1 }, move1[4] = { 0, 0, 1, 1 }, tint[4] = { 1, 1, 1, 1 };
            if (md->anim) {
                anim_uv(md->anim, p->uvanim[0], md->stand, clock_ms, clock_ms, move0);
                anim_uv(md->anim, p->uvanim[1], md->stand, clock_ms, clock_ms, move1);
                piece_tint(md, p, tint);
            }
            SetShaderValue(shader, loc_layers, &p->layers, SHADER_UNIFORM_INT);
            SetShaderValue(shader, loc_combine, &p->combine, SHADER_UNIFORM_INT);
            SetShaderValue(shader, loc_set0, &p->set[0], SHADER_UNIFORM_INT);
            SetShaderValue(shader, loc_set1, &p->set[1], SHADER_UNIFORM_INT);
            SetShaderValue(shader, loc_move0, move0, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, loc_move1, move1, SHADER_UNIFORM_VEC4);
            SetShaderValue(shader, loc_tint, tint, SHADER_UNIFORM_VEC4);
            material.maps[MATERIAL_MAP_METALNESS].texture = p->picture2 >= 0 ? pictures[p->picture2].tex : pictures[p->picture].tex;
            SetShaderValue(shader, loc_indoor, &indoor, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, loc_cutoff, &cutoff, SHADER_UNIFORM_FLOAT);
            SetShaderValue(shader, loc_unlit, &unlit, SHADER_UNIFORM_FLOAT);
            if (see_through) BeginBlendMode(p->blend == 2 ? BLEND_ALPHA : p->blend >= 5 ? BLEND_MULTIPLIED : BLEND_ADDITIVE);
            material.maps[MATERIAL_MAP_DIFFUSE].texture = pictures[p->picture].tex;
            DrawMeshInstanced(p->mesh, material, md->vis, md->nvis);   /* every copy in sight in one order (camera-facing ones too: their rows) */
            ++doodad_orders;
            if (see_through) EndBlendMode();
        }
    }
}
/* the particles: squares facing the camera (its right and up), each from
 * its picture's cell, coloured and sized by its age, blended as its
 * emitter says (2 see-through, 3-4 added light; 0-1 drawn see-through),
 * without writing depth, after everything solid. The fog takes them as it
 * takes the solid world (173o): by the distance from the eye between the
 * fog's start and end, a see-through particle's colour goes to the fog's,
 * and an added-light one's goes to nothing (adding the fog's colour would
 * light the fog up instead of hiding the spark) */
static void draw_particles(Vector3 right, Vector3 up, Vector3 eye, const LightNow *light)
{
    float fog_span = fmaxf(light->fog_end - light->fog_start, 1.0f);
    for (int i = 0; i < nmodels; ++i) {
        DoodadModel *md = &models[i];
        if (!md->nemitters || !md->nruns) continue;
        for (int k = 0; k < md->nemitters; ++k) {
            const Emitter *e = &md->emitters[k];
            if (md->emit_pic[k] < 0) continue;
            BeginBlendMode(e->blend >= 3 ? BLEND_ADDITIVE : BLEND_ALPHA);
            rlSetTexture(pictures[md->emit_pic[k]].tex.id);
            rlBegin(RL_QUADS);
            for (int c = 0; c < md->ncopies; ++c) {
                const EmitterRun *run = &md->runs[c * md->nemitters + k];
                for (int q = 0; q < run->n; ++q) {
                    const Particle *p = &run->ps[q];
                    float rgba[4], size[2];
                    particles_look(e, p, rgba, size);
                    int cell = p->cell;
                    if (e->cell.n) {                    /* the cell follows its life */
                        float t = p->age / p->life;
                        int kk = 0;
                        while (kk + 1 < e->cell.n && t >= e->cell.t[kk + 1]) ++kk;
                        cell = (int)e->cell.v[kk][0];
                    }
                    cell %= e->rows * e->cols;
                    float u0 = (float)(cell % e->cols) / e->cols, v0 = (float)(cell / e->cols) / e->rows;
                    float u1 = u0 + 1.0f / e->cols, v1 = v0 + 1.0f / e->rows;
                    float sx = size[0], sy = size[1];
                    Vector3 c0 = { p->p[0], p->p[1], p->p[2] };
                    particles_fog(e->blend, rgba, light->fog, (Vector3Distance(c0, eye) - light->fog_start) / fog_span);
                    rlColor4f(rgba[0], rgba[1], rgba[2], rgba[3]);
                    rlTexCoord2f(u0, v1); rlVertex3f(c0.x - right.x * sx - up.x * sy, c0.y - right.y * sx - up.y * sy, c0.z - right.z * sx - up.z * sy);
                    rlTexCoord2f(u1, v1); rlVertex3f(c0.x + right.x * sx - up.x * sy, c0.y + right.y * sx - up.y * sy, c0.z + right.z * sx - up.z * sy);
                    rlTexCoord2f(u1, v0); rlVertex3f(c0.x + right.x * sx + up.x * sy, c0.y + right.y * sx + up.y * sy, c0.z + right.z * sx + up.z * sy);
                    rlTexCoord2f(u0, v0); rlVertex3f(c0.x - right.x * sx + up.x * sy, c0.y - right.y * sx + up.y * sy, c0.z - right.z * sx + up.z * sy);
                }
            }
            rlEnd();
            rlSetTexture(0);
            EndBlendMode();
        }
    }
}

/* {{{ static gather_sight: each model's copies within the camera's sight
 * (173u). A copy's ball: its place, the model's reach times the copy's
 * size (its matrix's longest axis), a tenth more for swaying and a yard
 * for luck. The skybox isn't a placed model and is drawn on its own. */
static void gather_sight(void)
{
    Sight sight;
    sight_from(&sight, MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection()));
    for (int i = 0; i < nmodels; ++i) {
        DoodadModel *md = &models[i];
        md->nvis = 0;
        if (!md->ncopies) continue;
        if (md->vis_cap < md->ncopies) {
            md->vis_cap = md->ncopies;
            md->vis = realloc(md->vis, sizeof *md->vis * md->vis_cap);
            md->vis_idx = realloc(md->vis_idx, sizeof *md->vis_idx * md->vis_cap);
        }
        for (int c = 0; c < md->ncopies; ++c) {
            const Matrix *m = &md->copies[c];
            float sx = m->m0 * m->m0 + m->m1 * m->m1 + m->m2 * m->m2, sy = m->m4 * m->m4 + m->m5 * m->m5 + m->m6 * m->m6, sz = m->m8 * m->m8 + m->m9 * m->m9 + m->m10 * m->m10;
            float size = sqrtf(fmaxf(sx, fmaxf(sy, sz)));
            if (!sight_ball(&sight, (Vector3){ m->m12, m->m13, m->m14 }, md->radius * size * 1.1f + 1.0f)) continue;
            md->vis[md->nvis] = *m;
            md->vis_idx[md->nvis++] = c;
        }
    }
}
/* }}} */

int doodads_orders_last(void) { int n = doodad_orders; doodad_orders = 0; return n; }

void doodads_draw(Vector3 eye, const LightNow *light)
{
    gather_sight();
    for (int i = 0; i < nmodels; ++i) if (models[i].billboard && models[i].anim) pose_copies(&models[i], eye);
    SetShaderValue(shader, loc_eye, &eye, SHADER_UNIFORM_VEC3);
    sky_light_shader(shader, light);
    draw_pass(0);
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
    draw_pass(1);
    rlDrawRenderBatchActive();
    draw_particles(view_right, view_up, eye, light);
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}
/* }}} */

/* {{{ doodads_animate
 * Each moving model's stand animation at `ms` (its own length wraps it;
 * global sequences use `ms` too), its points bent and copied into its
 * pieces' meshes. Only models with copies placed are bent. */
static void run_particles(DoodadModel *md, uint32_t ms, float dt, Vector3 eye, float particle_density)
{
    int want = md->ncopies * md->nemitters;
    if (md->nruns < want) {
        md->runs = realloc(md->runs, sizeof *md->runs * want);
        memset(md->runs + md->nruns, 0, sizeof *md->runs * (want - md->nruns));
        md->nruns = want;
    }
    for (int c = 0; c < md->ncopies; ++c) {
        float at[16];
        memcpy(at, MatrixToFloatV(md->copies[c]).v, sizeof at);
        float dx = at[12] - eye.x, dy = at[13] - eye.y, dz = at[14] - eye.z;
        int near_eye = dx * dx + dy * dy + dz * dz < PARTICLE_REACH * PARTICLE_REACH;
        for (int k = 0; k < md->nemitters; ++k) {
            EmitterRun *run = &md->runs[c * md->nemitters + k];
            if (!near_eye) { run->n = 0; continue; }       /* far copies hold none */
            /* the emitter's bone in the world: the copy's place times the bone's pose */
            const float *b = md->poses[md->emitters[k].bone];
            float w[16];
            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 4; ++col) {
                    float sum = 0;
                    for (int q = 0; q < 4; ++q) sum += at[q * 4 + row] * b[col * 4 + q];
                    w[col * 4 + row] = sum;
                }
            particles_step(md->anim, &md->emitters[k], run, w, md->stand, ms, dt, particle_density);
        }
    }
}
/* {{{ static pose_copies: a camera-facing model's copies in sight, each
 * posed alone into its row of the bone picture (173u)
 * The camera's toward, right and up, turned into the copy's own space (its
 * turn undone: the transpose over its size squared), give the facing
 * bones their turn. Row v is the v-th copy drawn (gather_sight's order),
 * which the shader finds by the instance's number. */
static void pose_copies(DoodadModel *md, Vector3 eye)
{
    if (!md->nvis) return;
    bone_rows(md, md->nvis);
    int nb = md->anim->nbones;
    for (int v = 0; v < md->nvis; ++v) {
        float at[16];
        memcpy(at, MatrixToFloatV(md->vis[v]).v, sizeof at);
        float s2 = at[0] * at[0] + at[1] * at[1] + at[2] * at[2];
        float world[3][3] = { { eye.x - at[12], eye.y - at[13], eye.z - at[14] },
                              { view_right.x, view_right.y, view_right.z }, { view_up.x, view_up.y, view_up.z } };
        float view[9];
        for (int w = 0; w < 3; ++w) {
            float o[3], l = 0;
            for (int k = 0; k < 3; ++k) { o[k] = (at[k * 4] * world[w][0] + at[k * 4 + 1] * world[w][1] + at[k * 4 + 2] * world[w][2]) / s2; l += o[k] * o[k]; }
            l = sqrtf(l);
            for (int k = 0; k < 3; ++k) view[w * 3 + k] = l > 0 ? o[k] / l : 0;
        }
        anim_pose_facing(md->anim, md->stand, clock_ms, clock_ms, (float (*)[16])(md->bone_data + (size_t)v * nb * 16), view);
    }
    UpdateTextureRec(md->bones, (Rectangle){ 0, 0, (float)(nb * 4), (float)md->nvis }, md->bone_data);
}
/* }}} */

void doodads_animate(uint32_t ms, float dt, Vector3 eye, Vector3 target, float particle_density)
{
    clock_ms = ms;
    Vector3 ahead = Vector3Normalize(Vector3Subtract(target, eye));
    view_right = Vector3Normalize(Vector3CrossProduct(ahead, (Vector3){ 0, 0, 1 }));
    view_up = Vector3CrossProduct(view_right, ahead);
    for (int i = 0; i < nmodels; ++i) {
        DoodadModel *md = &models[i];
        if (!md->anim || (!md->ncopies && i != skybox)) continue;
        anim_pose(md->anim, md->stand, ms, ms, md->poses);
        if (md->nemitters) run_particles(md, ms, dt, eye, particle_density);
        if (!md->moving || md->billboard) continue;      /* camera-facing ones are posed per copy when drawn */
        bone_rows(md, 1);
        UpdateTexture(md->bones, md->poses);              /* one row, shared by every copy (173u) */
    }
}
/* }}} */

/* {{{ doodads_skybox, doodads_draw_skybox (173n) */
void doodads_skybox(const char *path)
{
    skybox = path && path[0] ? load_model(path) : -1;
    if (skybox >= 0 && !models[skybox].npieces) { fprintf(stderr, "sky: the skybox %s has nothing to draw\n", path); skybox = -1; }
}
void doodads_draw_skybox(Vector3 eye)
{
    if (skybox < 0) return;
    DoodadModel *md = &models[skybox];
    float s = md->radius > 0 ? 800.0f / md->radius : 1;                       /* inside the far plane */
    Matrix at = MatrixMultiply(MatrixScale(s, s, s), MatrixTranslate(eye.x, eye.y, eye.z));
    float unlit = 1, indoor = 0, far_fog[2] = { 1e6f, 2e6f };
    SetShaderValue(shader, loc_eye, &eye, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader, GetShaderLocation(shader, "fog_range"), far_fog, SHADER_UNIFORM_VEC2);
    SetShaderValue(shader, loc_unlit, &unlit, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader, loc_indoor, &indoor, SHADER_UNIFORM_FLOAT);
    rlDisableDepthTest();
    rlDisableDepthMask();
    use_bones(md);
    for (int k = 0; k < md->npieces; ++k) {
        DoodadPiece *p = &md->pieces[k];
        float move0[4] = { 0, 0, 1, 1 }, move1[4] = { 0, 0, 1, 1 }, tint[4] = { 1, 1, 1, 1 };
        if (md->anim) {
            anim_uv(md->anim, p->uvanim[0], md->stand, clock_ms, clock_ms, move0);
            anim_uv(md->anim, p->uvanim[1], md->stand, clock_ms, clock_ms, move1);
            piece_tint(md, p, tint);
        }
        float cutoff = -1;
        SetShaderValue(shader, loc_cutoff, &cutoff, SHADER_UNIFORM_FLOAT);
        SetShaderValue(shader, loc_layers, &p->layers, SHADER_UNIFORM_INT);
        SetShaderValue(shader, loc_combine, &p->combine, SHADER_UNIFORM_INT);
        SetShaderValue(shader, loc_set0, &p->set[0], SHADER_UNIFORM_INT);
        SetShaderValue(shader, loc_set1, &p->set[1], SHADER_UNIFORM_INT);
        SetShaderValue(shader, loc_move0, move0, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, loc_move1, move1, SHADER_UNIFORM_VEC4);
        SetShaderValue(shader, loc_tint, tint, SHADER_UNIFORM_VEC4);
        material.maps[MATERIAL_MAP_DIFFUSE].texture = pictures[p->picture].tex;
        material.maps[MATERIAL_MAP_METALNESS].texture = p->picture2 >= 0 ? pictures[p->picture2].tex : pictures[p->picture].tex;
        BeginBlendMode(p->blend >= 3 ? BLEND_ADDITIVE : BLEND_ALPHA);
        DrawMeshInstanced(p->mesh, material, &at, 1);
        EndBlendMode();
    }
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlEnableDepthTest();
}
/* }}} */

/* {{{ doodads_counts */
void doodads_counts(int *nm, int *nc, int *ns, int *nmoving, int *nemitting)
{
    int copies = 0;
    for (int i = 0; i < nmodels; ++i) copies += models[i].ncopies;
    *nm = nmodels; *nc = copies; *ns = skipped_pieces; *nmoving = moving_models; *nemitting = emitting_models;
}
/* }}} */
