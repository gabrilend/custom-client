/*
 * 041-figure.c (issues 173i, 173j) - an animated model in the world; see 041-figure.h
 *
 * For a general audience: builds a figure's pieces and skeleton from a
 * model, finds each piece's picture, and each frame bends the model to
 * its animation and draws it.
 */
#include "041-figure.h"
#include "013-errors.h"
#include "017-blp.h"
#include "016-dbc.h"
#include "036-texture.h"

#include "raymath.h"
#include "rlgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint32_t u32le(const unsigned char *s, size_t at) { return s[at] | s[at + 1] << 8 | s[at + 2] << 16 | (uint32_t)s[at + 3] << 24; }

/* {{{ static texture_of: a decoded picture onto the graphics card */
static Texture2D texture_of(const BlpImage *img)
{
    Image im = { .data = img->rgba, .width = img->width, .height = img->height, .mipmaps = 1,
                 .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    Texture2D t = LoadTextureFromImage(im);
    GenTextureMipmaps(&t);
    SetTextureWrap(t, TEXTURE_WRAP_REPEAT);
    texture_filter(t.id, 0x0DE1, t.mipmaps);              /* a 2D picture, as the Texture Filtering setting says (173b6) */
    return t;
}
/* load_picture: a picture named in the archives, through a cache kept for
 * the whole run: figures come and go with their tiles (173t), and many
 * share pictures (eleven sentinels, one set of hair), so a picture is read
 * and sent to the graphics card once and never freed (the run's distinct
 * pictures bound it). Names are matched without regard to case, as the
 * archives match them. */
static struct { char name[260]; Texture2D tex; } *cache;
static int ncache, cache_cap;
static WErr load_picture(MpqChain *chain, const char *name, Texture2D *out, WWhy *why)
{
    for (int i = 0; i < ncache; ++i) if (!strcasecmp(cache[i].name, name)) { *out = cache[i].tex; return 0; }
    size_t len;
    const char *from;
    unsigned char *s = mpq_chain_read(chain, name, &len, &from);
    if (!s) { w_fail(why, W_ERR_NOT_FOUND, "the picture %.200s is in none of the client's archives", name); return why->code; }
    WWhy inner;
    int bad = texture_from_blp(s, len, out, &inner);   /* sent compressed when stored so (173u) */
    free(s);
    if (bad) { w_fail(why, inner.code, "the picture %.120s: %.150s", name, inner.text); return why->code; }
    if (ncache == cache_cap) { cache_cap = cache_cap ? cache_cap * 2 : 64; cache = realloc(cache, sizeof *cache * cache_cap); }
    snprintf(cache[ncache].name, sizeof cache[ncache].name, "%s", name);
    cache[ncache++].tex = *out;
    return 0;
}
/* own: a picture made for this figure alone (a painted skin sheet), freed with it */
static Texture2D own(Figure *f, const BlpImage *img)
{
    Texture2D t = texture_of(img);
    if (f->nowned < (int)(sizeof f->owned / sizeof f->owned[0])) f->owned[f->nowned++] = t;
    return t;
}
/* }}} */

/* {{{ static build: pieces, points and skeleton from a read model
 * `shown` says which pieces show (NULL: all); `pics` holds one picture
 * per model texture slot (id 0 where the slot has none: its pieces are
 * left out). `base` is the model's archive path without its extension. */
static WErr build(Figure *f, MpqChain *chain, const M2Model *mod, const uint8_t *shown, const Texture2D *pics,
                 const char *base, WWhy *why)
{
    for (uint32_t u = 0; u < mod->nunits && f->npieces < 64; ++u) {
        M2Unit un = mod->units[u];
        if (un.piece >= mod->npieces || un.texture >= mod->ntextures) continue;
        if (shown && !shown[un.piece]) continue;
        if (pics[un.texture].id == 0) continue;
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

    /* the skeleton, and any animation kept in a .anim file */
    char path[300];
    size_t len;
    const char *from;
    snprintf(path, sizeof path, "%s.m2", base);
    unsigned char *m2 = mpq_chain_read(chain, path, &len, &from);
    WWhy inner;
    if (!m2 || anim_read(m2, len, &f->anim, &inner)) {
        w_fail(why, m2 ? inner.code : W_ERR_NOT_FOUND, "%s's skeleton (%s): %s", f->name, path, m2 ? inner.text : "not in the archives");
        free(m2);
        return why->code;
    }
    free(m2);
    for (int i = 0; i < f->anim.nseqs; ++i) {
        if (!anim_needs_file(&f->anim, i)) continue;
        snprintf(path, sizeof path, "%s%04d-%02d.anim", base, f->anim.seqs[i].id, f->anim.seqs[i].variation);
        unsigned char *a = mpq_chain_read(chain, path, &len, &from);
        if (!a) { fprintf(stderr, "%s: %s is in none of the archives; that animation leaves it at rest\n", f->name, path); continue; }
        anim_attach_file(&f->anim, i, a, len);
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
static WErr from_character(Figure *f, MpqChain *chain, const CharacterLook *look, const char *baked, WWhy *why)
{
    memset(f, 0, sizeof *f);
    f->scale = 1;
    Character ch;
    if (character_build(chain, look, &ch, why)) return why->code;
    snprintf(f->name, sizeof f->name, "%.63s", strrchr(ch.base, '\\') ? strrchr(ch.base, '\\') + 1 : ch.base);
    Texture2D pics[64] = { 0 };
    Texture2D skin = { 0 }, hair = { 0 }, fur = { 0 }, cape = { 0 };
    if (baked) {
        WWhy inner;
        if (load_picture(chain, baked, &skin, &inner)) { w_fail(why, inner.code, "%.250s", inner.text); character_free(&ch); return why->code; }
    } else skin = own(f, &ch.skin);
    if (ch.hair.width) hair = own(f, &ch.hair);
    if (ch.fur.width) fur = own(f, &ch.fur);
    if (ch.cape.width) cape = own(f, &ch.cape);
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
            if (load_picture(chain, ch.model.textures[t].file, &pics[t], &inner))
                fprintf(stderr, "character: %s; the pieces using it are left off\n", inner.text);
        }
    }
    int bad = build(f, chain, &ch.model, ch.shown, pics, ch.base, why);
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
        if (found < 0) { fprintf(stderr, "character: the model has no attachment point %d; %s is left off\n", a->point, a->model); continue; }
        Figure *it = calloc(1, sizeof *it);
        WWhy inner;
        if (figure_from_item(it, chain, a->model, a->picture, &inner)) {
            fprintf(stderr, "character: %s; left off\n", inner.text);
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
WErr figure_from_character(Figure *f, MpqChain *chain, const CharacterLook *look, WWhy *why)
{
    return from_character(f, chain, look, NULL, why);
}
/* }}} */

/* {{{ figure_from_item: an item's model, its pictures named inside it
 * (kind 0) or the item's own picture (kind 2, "object skin") */
WErr figure_from_item(Figure *f, MpqChain *chain, const char *model, const char *picture_path, WWhy *why)
{
    memset(f, 0, sizeof *f);
    f->scale = 1;
    char base[260], path[300];
    WWhy inner;
    snprintf(base, sizeof base, "%s", model);
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    snprintf(f->name, sizeof f->name, "%.63s", strrchr(base, '\\') ? strrchr(base, '\\') + 1 : base);
    size_t mlen, slen;
    const char *from;
    snprintf(path, sizeof path, "%s.m2", base);
    unsigned char *m2 = mpq_chain_read(chain, path, &mlen, &from);
    snprintf(path, sizeof path, "%s00.skin", base);
    unsigned char *skin = mpq_chain_read(chain, path, &slen, &from);
    if (!m2 || !skin) { w_fail(why, W_ERR_NOT_FOUND, "%.200s is in none of the archives", !m2 ? model : path); free(m2); free(skin); return why->code; }
    M2Model mod;
    int bad = m2_read(m2, mlen, skin, slen, &mod, &inner);
    free(m2); free(skin);
    if (bad) { w_fail(why, inner.code, "%.100s: %.200s", f->name, inner.text); return why->code; }
    Texture2D pics[64] = { 0 };
    for (uint32_t t = 0; t < mod.ntextures && t < 64; ++t) {
        const char *name = mod.textures[t].kind == 0 ? mod.textures[t].file : mod.textures[t].kind == 2 ? picture_path : NULL;
        if (!name || !name[0]) continue;
        if (load_picture(chain, name, &pics[t], &inner)) { w_fail(why, inner.code, "%.100s: %.200s", f->name, inner.text); m2_free(&mod); return why->code; }
    }
    bad = build(f, chain, &mod, NULL, pics, base, why);
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
 * names no other (its geoset data, not read yet). */
static WErr open_table(MpqChain *chain, const char *name, Dbc *d, unsigned char **keep, WWhy *why)
{
    size_t len;
    const char *from;
    *keep = mpq_chain_read(chain, name, &len, &from);
    if (!*keep) { w_fail(why, W_ERR_NOT_FOUND, "%s is in none of the client's archives", name); return why->code; }
    return dbc_open(d, *keep, len, why);
}
WErr figure_from_display(Figure *f, MpqChain *chain, int display_id, WWhy *why)
{
    memset(f, 0, sizeof *f);
    Dbc di, md;
    unsigned char *di_s = NULL, *md_s = NULL;
    if (open_table(chain, "DBFilesClient\\CreatureDisplayInfo.dbc", &di, &di_s, why) ||
        open_table(chain, "DBFilesClient\\CreatureModelData.dbc", &md, &md_s, why)) { free(di_s); free(md_s); return why->code; }
    int row = -1;
    for (uint32_t r = 0; r < di.rows; ++r) if (dbc_u32(&di, r, 1) == (uint32_t)display_id) { row = (int)r; break; }
    if (row < 0) { w_fail(why, W_ERR_NOT_FOUND, "no creature display %d in CreatureDisplayInfo", display_id); free(di_s); free(md_s); return why->code; }
    uint32_t model_id = dbc_u32(&di, (uint32_t)row, 2), scale_bits = dbc_u32(&di, (uint32_t)row, 5), extra = dbc_u32(&di, (uint32_t)row, 4);
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
        free(di_s); free(md_s);
        Dbc ex;
        unsigned char *ex_s = NULL;
        if (open_table(chain, "DBFilesClient\\CreatureDisplayInfoExtra.dbc", &ex, &ex_s, why)) { free(ex_s); return why->code; }
        int er = -1;
        for (uint32_t r = 0; r < ex.rows; ++r) if (dbc_u32(&ex, r, 1) == extra) { er = (int)r; break; }
        if (er < 0) { w_fail(why, W_ERR_NOT_FOUND, "display %d names extra row %u, which CreatureDisplayInfoExtra lacks", display_id, extra); free(ex_s); return why->code; }
        static const int SLOT[11] = { SLOT_HEAD, SLOT_SHOULDERS, SLOT_SHIRT, SLOT_CHEST, SLOT_WAIST, SLOT_LEGS, SLOT_FEET, SLOT_WRISTS, SLOT_HANDS, SLOT_TABARD, SLOT_BACK };
        CharacterLook look = { 0 };
        uint32_t e = (uint32_t)er;
        look.race = (int)dbc_u32(&ex, e, 2); look.sex = (int)dbc_u32(&ex, e, 3);
        look.skin = (int)dbc_u32(&ex, e, 4); look.face = (int)dbc_u32(&ex, e, 5);
        look.hair_style = (int)dbc_u32(&ex, e, 6); look.hair_color = (int)dbc_u32(&ex, e, 7);
        look.facial_hair = (int)dbc_u32(&ex, e, 8);
        look.npc = 1;
        for (int k = 0; k < 11; ++k) look.items[SLOT[k]] = (int)dbc_u32(&ex, e, 9 + (uint32_t)k);
        char baked[300] = "";
        const char *bake = dbc_text(&ex, e, 21);
        if (bake && bake[0]) snprintf(baked, sizeof baked, "Textures\\BakedNpcTextures\\%.200s", bake);
        free(ex_s);
        int bad = from_character(f, chain, &look, baked[0] ? baked : NULL, why);
        if (!bad) {
            char model_name[64];
            snprintf(model_name, sizeof model_name, "%s", f->name);
            f->scale = scale;
            snprintf(f->name, sizeof f->name, "display %d (a %.40s)", display_id, model_name);
        }
        return bad;
    }
    f->scale = scale;
    char skins[3][128];
    for (int k = 0; k < 3; ++k) snprintf(skins[k], sizeof skins[k], "%s", dbc_text(&di, (uint32_t)row, 7 + k));
    char base[260] = "";
    for (uint32_t r = 0; r < md.rows; ++r)
        if (dbc_u32(&md, r, 1) == model_id) { snprintf(base, sizeof base, "%s", dbc_text(&md, r, 3)); break; }
    free(di_s); free(md_s);
    if (!base[0]) { w_fail(why, W_ERR_NOT_FOUND, "display %d names model %u, which CreatureModelData lacks", display_id, model_id); return why->code; }
    char *dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    char folder[260];
    snprintf(folder, sizeof folder, "%s", base);
    char *cut = strrchr(folder, '\\');
    if (cut) cut[1] = '\0'; else folder[0] = '\0';
    snprintf(f->name, sizeof f->name, "display %d (%s)", display_id, cut ? base + (cut - folder) + 1 : base);

    char path[300];
    size_t mlen, slen;
    const char *from;
    snprintf(path, sizeof path, "%s.m2", base);
    unsigned char *m2 = mpq_chain_read(chain, path, &mlen, &from);
    snprintf(path, sizeof path, "%s00.skin", base);
    unsigned char *skin = mpq_chain_read(chain, path, &slen, &from);
    if (!m2 || !skin) { w_fail(why, W_ERR_NOT_FOUND, "%s: its %s is in none of the archives", f->name, !m2 ? ".m2" : "00.skin"); free(m2); free(skin); return why->code; }
    M2Model mod;
    WWhy inner;
    int bad = m2_read(m2, mlen, skin, slen, &mod, &inner);
    free(m2); free(skin);
    if (bad) { w_fail(why, inner.code, "%s: %s", f->name, inner.text); return why->code; }

    Texture2D pics[64] = { 0 };
    for (uint32_t t = 0; t < mod.ntextures && t < 64; ++t) {
        uint32_t kind = mod.textures[t].kind;
        if (kind == 0 && mod.textures[t].file[0]) snprintf(path, sizeof path, "%s", mod.textures[t].file);
        else if (kind >= 11 && kind <= 13 && skins[kind - 11][0]) snprintf(path, sizeof path, "%s%s.blp", folder, skins[kind - 11]);
        else continue;                                     /* a slot this display doesn't fill */
        if (load_picture(chain, path, &pics[t], &inner)) {
            w_fail(why, inner.code, "%s: %s", f->name, inner.text);
            m2_free(&mod);
            return why->code;
        }
    }
    uint8_t *shown = calloc(mod.npieces ? mod.npieces : 1, 1);
    for (uint32_t i = 0; i < mod.npieces; ++i) shown[i] = mod.pieces[i].id < 100 || mod.pieces[i].id % 100 == 1;
    bad = build(f, chain, &mod, shown, pics, base, why);
    free(shown);
    m2_free(&mod);
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
