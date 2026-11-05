/*
 * 041-figure.c (issues 173i, 173j) - an animated model in the world; see 041-figure.h
 *
 * For a general audience: builds a figure's pieces and skeleton from a
 * model, finds each piece's picture, and each frame bends the model to
 * its animation and draws it.
 */
#include "041-figure.h"
#include "088-figure-model.h"
#include "013-errors.h"
#include "017-blp.h"
#include "016-dbc.h"
#include "036-texture.h"
#include "061-mdx.h"

#include "raymath.h"
#include "rlgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint32_t u32le(const unsigned char *s, size_t at) { return s[at] | s[at + 1] << 8 | s[at + 2] << 16 | (uint32_t)s[at + 3] << 24; }

/* {{{ shaders (moved here from 043-main.c for 410, so every program that
 * draws figures draws them the same way)
 * The figures' (her, bystanders, creatures, their items): bent on the
 * graphics card (173u). Each point carries its four bone numbers (in the
 * colour slot, a byte each: Raylib 5.1 has no bone slots) and their
 * weights (in the tangent slot); each figure's bone poses arrive as a
 * one-row picture of floats (`bones`), four texels a bone, one column of
 * its matrix each (figure_bend). A point tied to no bone stays where made.
 * Then lit by the sun from one direction, faded into the sky with
 * distance, see-through pixels (hair strands) cut out. GLSL 330, with
 * Raylib's own attribute and uniform names. The ground has its own, in
 * 038-ground.c. */
static const char *FIGURE_VS =
    "#version 330\n"
    "in vec3 vertexPosition; in vec3 vertexNormal; in vec4 vertexColor; in vec2 vertexTexCoord; in vec4 vertexTangent;\n"
    "uniform mat4 mvp; uniform mat4 matModel; uniform vec3 eye; uniform sampler2D bones;\n"
    "out vec3 n; out vec4 col; out vec2 uv; out float dist;\n"
    "mat4 bone(int i) {\n"
    "  return mat4(texelFetch(bones, ivec2(i * 4, 0), 0), texelFetch(bones, ivec2(i * 4 + 1, 0), 0),\n"
    "              texelFetch(bones, ivec2(i * 4 + 2, 0), 0), texelFetch(bones, ivec2(i * 4 + 3, 0), 0));\n"
    "}\n"
    "void main() {\n"
    "  ivec4 id = ivec4(vertexColor * 255.0 + 0.5);\n"
    "  vec4 w = vertexTangent;\n"
    "  float total = w.x + w.y + w.z + w.w;\n"
    "  mat4 skin = total > 0.001 ? bone(id.x) * w.x + bone(id.y) * w.y + bone(id.z) * w.z + bone(id.w) * w.w : mat4(1.0);\n"
    "  vec4 p = skin * vec4(vertexPosition, 1.0);\n"
    "  vec4 wp = matModel * p;\n"
    "  n = mat3(matModel) * (mat3(skin) * vertexNormal); col = vec4(1.0); uv = vertexTexCoord;\n"
    "  dist = length(wp.xyz - eye);\n"
    "  gl_Position = mvp * p;\n"
    "}\n";
/* `mode` (figure_draw): 0 solid, pixels under half opacity cut out; 1
 * see-through, the picture's opacity times the tint's; 2 added light,
 * which the fog dims to nothing rather than tinting (as particles,
 * 173o). `tint` is the piece's animated colour and opacity, `unlit` skips
 * the sun (glows). */
static const char *MODEL_FS =
    "#version 330\n"
    "in vec3 n; in vec4 col; in vec2 uv; in float dist; out vec4 frag; uniform sampler2D texture0;\n"
    "uniform vec3 sun_dir, sun_col, shade_col, fog_col; uniform vec2 fog_range;\n"
    "uniform vec4 tint; uniform int mode, unlit;\n"
    "void main() {\n"
    "  vec4 t = texture(texture0, uv) * tint;\n"
    "  if (mode == 0 && t.a < 0.5 * tint.a) discard;\n"
    "  vec3 light = unlit != 0 ? vec3(1.0) : shade_col + sun_col * max(dot(normalize(n), sun_dir), 0.0);\n"
    "  float fog = clamp((dist - fog_range.x) / max(fog_range.y - fog_range.x, 1.0), 0.0, 1.0);\n"
    "  if (mode == 2) frag = vec4(t.rgb * light * (1.0 - fog), t.a);\n"
    "  else frag = vec4(mix(t.rgb * light, fog_col, fog), mode == 1 ? t.a : 1.0);\n"
    "}\n";
/* }}} */

/* {{{ figure_shader */
Shader figure_shader(void)
{
    Shader sh = LoadShaderFromMemory(FIGURE_VS, MODEL_FS);
    sh.locs[SHADER_LOC_MAP_METALNESS] = GetShaderLocation(sh, "bones");   /* figure_draw puts each figure's bone picture in that material slot */
    return sh;
}
/* }}} */

/* {{{ static texture_of: a decoded picture onto the graphics card */
static Texture2D texture_of(const Textures *tx, const BlpImage *img)
{
    Image im = { .data = img->rgba, .width = img->width, .height = img->height, .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D t = LoadTextureFromImage(im);
    GenTextureMipmaps(&t);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    texture_filter(tx, t.id, 0x0DE1, t.mipmaps);              /* a 2D picture, as the Texture Filtering setting says (173b6) */
    return t;
}
/* load_picture: a picture named in the archives, through a cache kept for
 * the whole run: figures come and go with their tiles (173t), and many
 * share pictures (eleven sentinels, one set of hair), so a picture is read
 * and sent to the graphics card once and never freed (the run's distinct
 * pictures bound it). Names are matched without regard to case, as the
 * archives match them. */
struct Figures {
    MpqChain *chain;
    Resolver *assets;                 /* NULL: the archives alone (a test map's folder comes first through it, 1109) */
    Tables   *tables;                 /* the creature tables, per row (074-tables.h) */
    double    wc3_scale;              /* yards per WC3 unit, for a WC3 model's points */
    Textures *textures;               /* how pictures go to the card (036-texture.h) */
    struct { char name[260]; Texture2D tex; } *cache;
    int ncache, cache_cap;
};
Figures *figures_new(MpqChain *chain, Resolver *assets, Tables *tables, Textures *textures)
{
    Figures *fs = calloc(1, sizeof *fs);
    fs->chain = chain;
    fs->assets = assets;
    fs->tables = tables;
    fs->wc3_scale = FIGURE_WC3_YARDS;
    fs->textures = textures;
    return fs;
}
void figures_wc3_scale(Figures *fs, double yards_per_unit) { if (yards_per_unit > 0) fs->wc3_scale = yards_per_unit; }
void figures_free(Figures *fs)
{
    for (int i = 0; i < fs->ncache; ++i) UnloadTexture(fs->cache[i].tex);
    free(fs->cache);
    free(fs);
}
/* `wc3`: a WC3 model's picture, asked of the resolver when there is one,
 * so a missing one gets its checker and a warning (no WC3 picture is in
 * the game's archives); every other picture from the archives, as before */
static WErr load_picture_from(Figures *fs, const char *name, int wc3, Texture2D *out, WWhy *why)
{
    for (int i = 0; i < fs->ncache; ++i) if (!strcasecmp(fs->cache[i].name, name)) { *out = fs->cache[i].tex; return 0; }
    size_t len;
    unsigned char *s;
    if (wc3 && fs->assets) {
        AssetAnswer a;
        WErr rc = asset_resolve(fs->assets, name, ASSET_TEXTURE, &a, why);
        if (rc) return rc;
        s = a.data; len = a.len;                         /* the answer's bytes, freed below */
    } else {
        AssetAnswer a;                                   /* the first place that has it, never a placeholder (1109 step 1l) */
        WErr rc = asset_read(fs->assets, fs->chain, name, ASSET_TEXTURE, &a, why);
        if (rc) return rc;
        s = a.data; len = a.len;
    }
    WWhy inner;
    int bad = texture_from_blp(fs->textures, s, len, out, &inner);   /* sent compressed when stored so (173u) */
    free(s);
    if (bad) { w_fail(why, inner.code, "the picture %.120s: %.150s", name, inner.text); return why->code; }
    if (fs->ncache == fs->cache_cap) { fs->cache_cap = fs->cache_cap ? fs->cache_cap * 2 : 64; fs->cache = realloc(fs->cache, sizeof *fs->cache * fs->cache_cap); }
    snprintf(fs->cache[fs->ncache].name, sizeof fs->cache[fs->ncache].name, "%s", name);
    fs->cache[fs->ncache++].tex = *out;
    return 0;
}
static WErr load_picture(Figures *fs, const char *name, Texture2D *out, WWhy *why) { return load_picture_from(fs, name, 0, out, why); }
/* own: a picture made for this figure alone (a painted skin sheet), freed with it */
static Texture2D own(Figures *fs, Figure *f, const BlpImage *img)
{
    Texture2D t = texture_of(fs->textures, img);
    if (f->nowned < (int)(sizeof f->owned / sizeof f->owned[0])) f->owned[f->nowned++] = t;
    return t;
}
/* }}} */

/* {{{ static build: pieces, points and skeleton from a read model
 * `shown` says which pieces show (NULL: all); `pics` holds one picture
 * per model texture slot (id 0 where the slot has none: its pieces are
 * left out). `base` is the model's archive path without its extension;
 * NULL for a WC3 model, whose skeleton isn't read yet (402: 061's nodes
 * are posed by issue 402 step 4): it stands as made, every point on one
 * still bone, and a unit hidden at rest (its alpha 0: a death or
 * alternate part, 061-mdx.h) is left out. */
static WErr build(Figure *f, Figures *fs, const M2Model *mod, const uint8_t *shown, const Texture2D *pics,
                 const char *base, WWhy *why)
{
    for (uint32_t u = 0; u < mod->nunits && f->npieces < 64; ++u) {
        M2Unit un = mod->units[u];
        if (un.piece >= mod->npieces || un.texture >= mod->ntextures) continue;
        if (shown && !shown[un.piece]) continue;
        if (pics[un.texture].id == 0) continue;
        if (!base && un.alpha <= 0) continue;
        M2Piece p = mod->pieces[un.piece];
        if (p.count == 0 || p.first + p.count > mod->nindices) continue;
        FigurePiece *fp = &f->pieces[f->npieces];
        Mesh m = { 0 };
        m.vertexCount = (int)p.count;
        m.triangleCount = (int)p.count / 3;
        m.vertices  = MemAlloc(sizeof(float) * 3 * p.count);
        m.normals   = MemAlloc(sizeof(float) * 3 * p.count);
        m.texcoords = MemAlloc(sizeof(float) * 2 * p.count);
        fp->point_of = malloc(sizeof(uint32_t) * p.count);
        for (uint32_t i = 0; i < p.count; ++i) {
            uint32_t k = mod->indices[p.first + i];
            if (k >= mod->nvertices) k = 0;               /* m2_read keeps indices in range; this guards the copy */
            M2Vertex v = mod->vertices[k];
            fp->point_of[i] = k;
            m.vertices[i * 3] = v.x; m.vertices[i * 3 + 1] = v.y; m.vertices[i * 3 + 2] = v.z;
            m.normals[i * 3] = v.nx; m.normals[i * 3 + 1] = v.ny; m.normals[i * 3 + 2] = v.nz;
            m.texcoords[i * 2] = v.u; m.texcoords[i * 2 + 1] = v.v;
        }
        fp->mesh = m;                                     /* sent to the card once its bones are known, below */
        fp->tex = pics[un.texture];
        fp->blend = un.blend;
        fp->flags = un.flags;
        fp->colour = un.colour;
        fp->fade = un.fade;
        fp->layers = un.layers;
        fp->moving_uv = un.moving_uv;
        ++f->npieces;
    }
    if (f->npieces == 0) { w_fail(why, W_ERR_NOT_FOUND, "%s: nothing to draw (no shown piece has a picture)", f->name); return why->code; }
    f->nrest = mod->nvertices;
    f->rest = malloc(sizeof *f->rest * f->nrest);
    memcpy(f->rest, mod->vertices, sizeof *f->rest * f->nrest);

    if (!base) {                                         /* a WC3 model: one still bone, no animations yet */
        memset(&f->anim, 0, sizeof f->anim);
        f->anim.npoints = f->nrest;
        f->anim.weights = calloc(f->nrest ? f->nrest : 1, sizeof *f->anim.weights);
        f->anim.bone_of = calloc(f->nrest ? f->nrest : 1, sizeof *f->anim.bone_of);
        for (uint32_t i = 0; i < f->nrest; ++i) f->anim.weights[i][0] = 255;
    }
    /* the skeleton, and any animation kept in a .anim file */
    char path[300];
    if (base) {                                          /* through the resolver when one is open (1109 step 1l) */
        snprintf(path, sizeof path, "%s.m2", base);
        AssetAnswer m2;
        WWhy inner;
        WErr rc = asset_read(fs->assets, fs->chain, path, ASSET_MODEL, &m2, &inner);
        if (!rc && anim_read(m2.data, m2.len, &f->anim, &inner)) rc = inner.code;
        asset_answer_free(&m2);
        if (rc) { w_fail(why, rc, "%s's skeleton (%s): %s", f->name, path, inner.text); return why->code; }
        for (int i = 0; i < f->anim.nseqs; ++i) {
            if (!anim_needs_file(&f->anim, i)) continue;
            snprintf(path, sizeof path, "%s%04d-%02d.anim", base, f->anim.seqs[i].id, f->anim.seqs[i].variation);
            AssetAnswer a;
            if (asset_read(fs->assets, fs->chain, path, ASSET_ANIMATION, &a, &inner)) { fprintf(stderr, "%s: %s is in no place; that animation leaves it at rest\n", f->name, path); continue; }
            anim_attach_file(&f->anim, i, a.data, a.len);   /* taken by the skeleton */
        }
    }
    if (f->anim.npoints != f->nrest) {
        w_fail(why, W_ERR_CORRUPT, "%s: the skeleton's model has %u points, the mesh %u", f->name, f->anim.npoints, f->nrest);
        return why->code;
    }
    f->poses = malloc(sizeof *f->poses * (f->anim.nbones ? f->anim.nbones : 1));
    f->scratch = malloc(sizeof *f->scratch * (f->anim.nbones ? f->anim.nbones : 1));
    /* each piece's points get their bones and weights (bone numbers in the
     * colour slot, a byte each; weights 0-1 in the tangent slot), then go
     * to the card once, never to change (173u) */
    for (int k = 0; k < f->npieces; ++k) {
        Mesh *m = &f->pieces[k].mesh;
        m->colors = MemAlloc(4 * (unsigned)m->vertexCount);
        m->tangents = MemAlloc(sizeof(float) * 4 * (unsigned)m->vertexCount);
        for (int v = 0; v < m->vertexCount; ++v) {
            uint32_t pt = f->pieces[k].point_of[v];
            for (int j = 0; j < 4; ++j) {
                m->colors[v * 4 + j] = f->anim.bone_of[pt][j];
                m->tangents[v * 4 + j] = f->anim.weights[pt][j] / 255.0f;
            }
        }
        UploadMesh(m, false);
    }
    /* the bone picture, every bone at rest until the first bending (items,
     * never bent, stay so) */
    int nb = f->anim.nbones ? f->anim.nbones : 1;
    float *rest = malloc(sizeof(float) * 16 * (size_t)nb);
    for (int b = 0; b < nb; ++b) for (int e = 0; e < 16; ++e) rest[b * 16 + e] = (e % 5 == 0) ? 1.0f : 0.0f;
    Image im = { .data = rest, .width = nb * 4, .height = 1, .mipmaps = 1, .format = PIXELFORMAT_UNCOMPRESSED_R32G32B32A32 };
    f->bones = LoadTextureFromImage(im);
    free(rest);
    SetTextureFilter(f->bones, TEXTURE_FILTER_POINT);
    return 0;
}
/* }}} */

/* {{{ figure_from_character
 * `baked` (NULL: none) names a finished skin sheet in the archives, which
 * replaces the one 025-character.c paints: a townsperson's (173t), whose
 * clothes and face the game painted in advance. */
static WErr from_character(Figure *f, Figures *fs, const CharacterLook *look, const char *baked, WWhy *why)
{
    memset(f, 0, sizeof *f);
    f->scale = 1;
    Character ch;
    if (character_build(fs->assets, fs->chain, look, &ch, why)) return why->code;
    snprintf(f->name, sizeof f->name, "%.63s", strrchr(ch.base, '\\') ? strrchr(ch.base, '\\') + 1 : ch.base);
    Texture2D pics[64] = { 0 };
    Texture2D skin = { 0 }, hair = { 0 }, fur = { 0 }, cape = { 0 };
    if (baked) {
        WWhy inner;
        if (load_picture(fs, baked, &skin, &inner)) { w_fail(why, inner.code, "%.250s", inner.text); character_free(&ch); return why->code; }
    } else skin = own(fs, f, &ch.skin);
    if (ch.hair.width) hair = own(fs, f, &ch.hair);
    if (ch.fur.width) fur = own(fs, f, &ch.fur);
    if (ch.cape.width) cape = own(fs, f, &ch.cape);
    /* a character's slots: kind 1 its skin sheet, kind 6 its hair, kind 8
     * its fur (a tauren's tail and coat), kind 2 its cloak, kind 0 a picture
     * named inside the model (the eye glow's) */
    for (uint32_t t = 0; t < ch.model.ntextures && t < 64; ++t) {
        if (ch.model.textures[t].kind == 1) pics[t] = skin;
        if (ch.model.textures[t].kind == 6) pics[t] = hair;
        if (ch.model.textures[t].kind == 8) pics[t] = fur;
        if (ch.model.textures[t].kind == 2) pics[t] = cape;   /* the cloak (173p) */
        if (ch.model.textures[t].kind == 0 && ch.model.textures[t].file[0]) {   /* named inside the model: the eye glow's */
            WWhy inner;
            if (load_picture(fs, ch.model.textures[t].file, &pics[t], &inner))
                fprintf(stderr, "character: %s; the pieces using it are left off\n", inner.text);
        }
    }
    int bad = build(f, fs, &ch.model, ch.shown, pics, ch.base, why);
    f->items_missing = ch.items_unpainted;
    /* the attached items (173p): each its own figure, hung from the
     * attachment point it names (header 240: count, 244: offset; 40 bytes
     * each: 0 id, 4 bone, 8 position); one that can't be read is named and
     * left off */
    for (int i = 0; !bad && i < ch.nattached; ++i) {
        const CharacterAttachment *a = &ch.attached[i];
        const unsigned char *m2 = f->anim.m2;
        uint32_t na = u32le(m2, 240), oa = u32le(m2, 244);
        int found = -1;
        for (uint32_t k = 0; k < na && (uint64_t)oa + (k + 1) * 40 <= f->anim.m2_len; ++k)
            if ((int)u32le(m2, oa + k * 40) == a->point) { found = (int)k; break; }
        if (found < 0) { fprintf(stderr, "character: the model has no attachment point %d; %s is left off\n", a->point, a->model); ++f->items_missing; continue; }
        Figure *it = calloc(1, sizeof *it);
        WWhy inner;
        if (figure_from_item(fs, it, a->model, a->picture, &inner)) {
            fprintf(stderr, "character: %s; left off\n", inner.text);
            ++f->items_missing;
            free(it);
            continue;
        }
        FigureItem *fi = &f->items[f->nitems++];
        size_t e = oa + (size_t)found * 40;
        fi->fig = it;
        fi->bone = m2[e + 4] | m2[e + 5] << 8;
        for (int c = 0; c < 3; ++c) { uint32_t u = u32le(m2, e + 8 + (size_t)c * 4); memcpy(&fi->pos[c], &u, 4); }
        if (fi->bone >= f->anim.nbones) fi->bone = 0;
    }
    character_free(&ch);
    return bad;
}
WErr figure_from_character(Figures *fs, Figure *f, const CharacterLook *look, WWhy *why)
{
    return from_character(f, fs, look, NULL, why);
}
/* }}} */

/* {{{ figure_from_item: an item's model, its pictures named inside it
 * (kind 0) or the item's own picture (kind 2, "object skin") */
WErr figure_from_item(Figures *fs, Figure *f, const char *model, const char *picture_path, WWhy *why)
{
    memset(f, 0, sizeof *f);
    f->scale = 1;
    char base[260], path[300];
    WWhy inner;
    snprintf(base, sizeof base, "%s", model);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    snprintf(f->name, sizeof f->name, "%.63s", strrchr(base, '\\') ? strrchr(base, '\\') + 1 : base);
    AssetAnswer m2 = { 0 }, skin = { 0 };               /* through the resolver when one is open (1109 step 1l) */
    snprintf(path, sizeof path, "%s.m2", base);
    WErr rc = asset_read(fs->assets, fs->chain, path, ASSET_MODEL, &m2, &inner);
    if (!rc) { snprintf(path, sizeof path, "%s00.skin", base); rc = asset_read(fs->assets, fs->chain, path, ASSET_MODEL, &skin, &inner); }
    if (rc) { w_fail(why, rc, "%.250s", inner.text); asset_answer_free(&m2); return why->code; }
    M2Model mod;
    int bad = m2_read(m2.data, m2.len, skin.data, skin.len, &mod, &inner);
    asset_answer_free(&m2); asset_answer_free(&skin);
    if (bad) { w_fail(why, inner.code, "%.100s: %.200s", f->name, inner.text); return why->code; }
    Texture2D pics[64] = { 0 };
    for (uint32_t t = 0; t < mod.ntextures && t < 64; ++t) {
        const char *name = mod.textures[t].kind == 0 ? mod.textures[t].file : mod.textures[t].kind == 2 ? picture_path : NULL;
        if (!name || !name[0]) continue;
        if (load_picture(fs, name, &pics[t], &inner)) { w_fail(why, inner.code, "%.100s: %.200s", f->name, inner.text); m2_free(&mod); return why->code; }
    }
    bad = build(f, fs, &mod, NULL, pics, base, why);
    m2_free(&mod);
    return bad;
}
/* }}} */

/* {{{ figure_from_display
 * CreatureDisplayInfo (3.3.5a): 1 id, 2 model (CreatureModelData id),
 * 5 scale (float), 7-9 the three skins (names without folder or .blp).
 * CreatureModelData: 1 id, 3 the model's path (.mdx; the archive holds .m2).
 * Pieces: the body group (ids under 100) and the first variant of every
 * other group (id % 100 == 1), the original's choice when the display
 * names no other (its geoset data, not read yet).
 * Each row comes through the tables (074-tables.h): a converted map's own
 * rows first, then the archives'. */

/* {{{ static from_wc3: a WC3 model the map brought (its bytes read): its
 * drawable part from 061-mdx, its pictures named inside it (a replaceable
 * one, team colour and the like, is left out until 404 paints it), its
 * points turned from WC3 units into yards */
static WErr from_wc3(Figures *fs, Figure *f, const unsigned char *s, size_t len, float scale, WWhy *why)
{
    MdxModel x;
    WWhy inner;
    if (mdx_read(s, len, &x, &inner)) { w_fail(why, inner.code, "%s: %s", f->name, inner.text); return why->code; }
    const M2Model *mod = &x.model;
    Texture2D pics[64] = { 0 };
    for (uint32_t t = 0; t < mod->ntextures && t < 64; ++t) {
        if (mod->textures[t].kind != 0 || !mod->textures[t].file[0]) continue;   /* replaceable (M2_TEXTURE_WC3 + n): not painted yet */
        if (load_picture_from(fs, mod->textures[t].file, 1, &pics[t], &inner)) {
            w_fail(why, inner.code, "%s: %s", f->name, inner.text);
            mdx_free(&x);
            return why->code;
        }
    }
    WErr bad = build(f, fs, mod, NULL, pics, NULL, why);
    mdx_free(&x);
    if (!bad) f->scale = scale * (float)fs->wc3_scale;
    return bad;
}
/* }}} */

/* {{{ static row: a table's row by number, its failure said naming the display that asked */
static WErr row(Figures *fs, int display_id, const char *table, uint32_t id, TableRow *r, WWhy *why)
{
    WWhy inner;
    WErr rc = tables_row(fs->tables, table, id, r, &inner);
    if (rc) return w_fail(why, rc, "creature display %d: %s", display_id, inner.text);
    return W_OK;
}
/* }}} */

/* {{{ static from_model: a game model's (or the stand-in's) files, found
 * by 088: its pictures (kind 0 named inside it, 11 to 13 the display's
 * skins in its folder), its pieces (the body group and every other
 * group's first variant) and its skeleton from base.m2 */
static WErr from_model(Figures *fs, Figure *f, const FigureModel *fm, WWhy *why)
{
    M2Model mod;
    WWhy inner;
    if (m2_read(fm->model.data, fm->model.len, fm->skin.data, fm->skin.len, &mod, &inner)) { w_fail(why, inner.code, "%s: %s", f->name, inner.text); return why->code; }
    char path[300];
    Texture2D pics[64] = { 0 };
    for (uint32_t t = 0; t < mod.ntextures && t < 64; ++t) {
        uint32_t kind = mod.textures[t].kind;
        if (kind == 0 && mod.textures[t].file[0]) snprintf(path, sizeof path, "%s", mod.textures[t].file);
        else if (kind >= 11 && kind <= 13 && fm->skins[kind - 11][0]) snprintf(path, sizeof path, "%s%s.blp", fm->folder, fm->skins[kind - 11]);
        else continue;                                     /* a slot this display doesn't fill */
        if (load_picture(fs, path, &pics[t], &inner)) {
            w_fail(why, inner.code, "%s: %s", f->name, inner.text);
            m2_free(&mod);
            return why->code;
        }
    }
    uint8_t *shown = calloc(mod.npieces ? mod.npieces : 1, 1);
    for (uint32_t i = 0; i < mod.npieces; ++i) shown[i] = mod.pieces[i].id < 100 || mod.pieces[i].id % 100 == 1;
    WErr bad = build(f, fs, &mod, shown, pics, fm->base, why);
    free(shown);
    m2_free(&mod);
    return bad;
}
/* }}} */

WErr figure_from_display(Figures *fs, Figure *f, int display_id, WWhy *why)
{
    memset(f, 0, sizeof *f);
    TableRow di;
    if (row(fs, display_id, "CreatureDisplayInfo", (uint32_t)display_id, &di, why)) return why->code;
    uint32_t scale_bits = dbc_u32(di.dbc, di.row, 5), extra = dbc_u32(di.dbc, di.row, 4);
    float scale;
    memcpy(&scale, &scale_bits, 4);
    if (!(scale > 0.01f && scale < 100)) scale = 1;
    if (extra) {
        /* a humanoid display (173t): drawn as a character, from its
         * CreatureDisplayInfoExtra row (1 id, 2 race, 3 sex, 4 skin, 5 face,
         * 6 hair style, 7 hair colour, 8 facial hair, 9-19 the item looks
         * worn: head, shoulders, shirt, chest, waist, legs, feet, wrists,
         * hands, tabard, back; 20 flags, 21 its baked skin sheet's name, in
         * Textures\BakedNpcTextures\) */
        TableRow ex;
        if (row(fs, display_id, "CreatureDisplayInfoExtra", extra, &ex, why)) return why->code;
        static const int SLOT[11] = { SLOT_HEAD, SLOT_SHOULDERS, SLOT_SHIRT, SLOT_CHEST, SLOT_WAIST, SLOT_LEGS, SLOT_FEET, SLOT_WRISTS, SLOT_HANDS, SLOT_TABARD, SLOT_BACK };
        CharacterLook look = { 0 };
        const Dbc *x = ex.dbc;
        uint32_t e = ex.row;
        look.race = (int)dbc_u32(x, e, 2); look.sex = (int)dbc_u32(x, e, 3);
        look.skin = (int)dbc_u32(x, e, 4); look.face = (int)dbc_u32(x, e, 5);
        look.hair_style = (int)dbc_u32(x, e, 6); look.hair_color = (int)dbc_u32(x, e, 7);
        look.facial_hair = (int)dbc_u32(x, e, 8);
        look.npc = 1;
        for (int k = 0; k < 11; ++k) look.items[SLOT[k]] = (int)dbc_u32(x, e, 9 + (uint32_t)k);
        char baked[300] = "";
        const char *bake = dbc_text(x, e, 21);
        if (bake && bake[0]) snprintf(baked, sizeof baked, "Textures\\BakedNpcTextures\\%.200s", bake);
        int bad = from_character(f, fs, &look, baked[0] ? baked : NULL, why);
        if (bad && fs->assets) {                         /* no character to be made (a test map with no install): the stand-in, said (1109 step 1l) */
            WWhy none;
            char name[64];
            snprintf(name, sizeof name, "display %d (a stand-in)", display_id);
            if (!figure_stand_in(fs, f, name, &none)) {
                fprintf(stderr, "creatures: display %d, a character look: %s; the baseline set's stand-in figure stands in\n", display_id, why->text);
                f->scale = scale;
                return W_OK;
            }
        }
        if (!bad) {
            char model_name[64];
            snprintf(model_name, sizeof model_name, "%s", f->name);
            f->scale = scale;
            snprintf(f->name, sizeof f->name, "display %d (a %.40s)", display_id, model_name);
        }
        return bad;
    }
    f->scale = scale;
    FigureModel fm;
    if (figure_model_for_display(fs->assets, fs->chain, fs->tables, display_id, &fm, why)) return why->code;
    const char *leaf = strrchr(fm.named, '\\') ? strrchr(fm.named, '\\') + 1 : fm.named;
    snprintf(f->name, sizeof f->name, "display %d (%.40s)", display_id, leaf);
    if (fm.kind == FIGURE_MODEL_WC3) {
        WErr bad = from_wc3(fs, f, fm.model.data, fm.model.len, scale, why);
        figure_model_free(&fm);
        return bad;
    }
    if (fm.kind == FIGURE_MODEL_STAND_IN) fprintf(stderr, "creatures: %s; the baseline set's stand-in figure stands in\n", fm.stood_in_for);
    WErr bad = from_model(fs, f, &fm, why);
    figure_model_free(&fm);
    return bad;
}
/* }}} */

/* {{{ figure_stand_in */
WErr figure_stand_in(Figures *fs, Figure *f, const char *name, WWhy *why)
{
    memset(f, 0, sizeof *f);
    f->scale = 1;
    snprintf(f->name, sizeof f->name, "%.63s", name && name[0] ? name : "the stand-in figure");
    FigureModel fm;
    if (figure_model_stand_in(fs->assets, fs->chain, NULL, &fm, why)) return why->code;
    WErr bad = from_model(fs, f, &fm, why);
    figure_model_free(&fm);
    return bad;
}
/* }}} */

/* {{{ figure_bend */
void figure_bend(Figure *f, const MovesState *moves, uint32_t global_ms)
{
    moves_pose(moves, &f->anim, global_ms, f->poses, f->scratch);
    f->seq = moves->seq; f->ms = (uint32_t)moves->ms; f->global_ms = global_ms;
    if (f->anim.nbones) UpdateTexture(f->bones, f->poses);   /* the card bends the points (173u) */
    /* the items follow their bones: bone pose times the point's place */
    for (int i = 0; i < f->nitems; ++i) {
        const float *b = f->poses[f->items[i].bone];
        const float *p = f->items[i].pos;
        float *o = f->items[i].at;
        memcpy(o, b, sizeof(float) * 16);
        for (int r = 0; r < 3; ++r) o[12 + r] = b[r] * p[0] + b[4 + r] * p[1] + b[8 + r] * p[2] + b[12 + r];
    }
}
/* }}} */

/* {{{ figure_draw, figure_free
 * A piece is drawn in the pass its blend belongs to: solid ones (mode 0,
 * cut-out) in pass 0; see-through (mode 1) and added light (mode 2,
 * Raylib's additive blending) in pass 1, depth not written. The tint is
 * the piece's animated colour and opacity; the shader's `unlit` skips the
 * sun for pieces flagged unlit (glows). */
static void draw_pieces(const Figure *f, Material *mat, Matrix at, int pass, int loc_tint, int loc_mode, int loc_unlit)
{
    for (int k = 0; k < f->npieces; ++k) {
        const FigurePiece *p = &f->pieces[k];
        int see_through = p->blend >= 2;
        if (see_through != pass) continue;
        float tint[4];
        anim_unit_tint(&f->anim, p->colour, p->fade, f->seq, f->ms, f->global_ms, tint);
        int mode = p->blend >= 3 ? 2 : p->blend == 2 ? 1 : 0, unlit = p->flags & 0x1;
        SetShaderValue(mat->shader, loc_tint, tint, SHADER_UNIFORM_VEC4);
        SetShaderValue(mat->shader, loc_mode, &mode, SHADER_UNIFORM_INT);
        SetShaderValue(mat->shader, loc_unlit, &unlit, SHADER_UNIFORM_INT);
        if (see_through) BeginBlendMode(mode == 2 ? BLEND_ADDITIVE : BLEND_ALPHA);
        mat->maps[MATERIAL_MAP_DIFFUSE].texture = p->tex;
        mat->maps[MATERIAL_MAP_METALNESS].texture = f->bones;   /* the shader's "bones" (043-main.c) */
        DrawMesh(p->mesh, *mat, at);
        if (see_through) EndBlendMode();
    }
}
void figure_draw(const Figure *f, Shader shader, Matrix place)
{
    Material mat = LoadMaterialDefault();
    mat.shader = shader;
    int loc_tint = GetShaderLocation(shader, "tint"), loc_mode = GetShaderLocation(shader, "mode"), loc_unlit = GetShaderLocation(shader, "unlit");
    Matrix at = MatrixMultiply(MatrixScale(f->scale, f->scale, f->scale), place);
    for (int pass = 0; pass < 2; ++pass) {
        if (pass == 1) { rlDrawRenderBatchActive(); rlDisableDepthMask(); }
        draw_pieces(f, &mat, at, pass, loc_tint, loc_mode, loc_unlit);
        for (int i = 0; i < f->nitems; ++i) {
            const float *m = f->items[i].at;
            Matrix on = { m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15] };
            draw_pieces(f->items[i].fig, &mat, MatrixMultiply(on, at), pass, loc_tint, loc_mode, loc_unlit);
        }
        if (pass == 1) { rlDrawRenderBatchActive(); rlEnableDepthMask(); }
    }
    float white[4] = { 1, 1, 1, 1 };
    int zero = 0;
    SetShaderValue(shader, loc_tint, white, SHADER_UNIFORM_VEC4);   /* left as plain solid drawing for whoever uses the shader next */
    SetShaderValue(shader, loc_mode, &zero, SHADER_UNIFORM_INT);
    SetShaderValue(shader, loc_unlit, &zero, SHADER_UNIFORM_INT);
    MemFree(mat.maps);                                  /* the default material's map list; the shader and pictures are kept */
}
void figure_free(Figure *f)
{
    for (int i = 0; i < f->nowned; ++i) UnloadTexture(f->owned[i]);
    for (int i = 0; i < f->nitems; ++i) { figure_free(f->items[i].fig); free(f->items[i].fig); }
    for (int k = 0; k < f->npieces; ++k) { UnloadMesh(f->pieces[k].mesh); free(f->pieces[k].point_of); }
    anim_free(&f->anim);
    if (f->bones.id) UnloadTexture(f->bones);
    free(f->rest); free(f->poses); free(f->scratch);
    memset(f, 0, sizeof *f);
}
/* }}} */
