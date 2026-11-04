/*
 * 061-mdx.c (issue 401 step 2) - see 061-mdx.h. Ported from
 * world-edit-to-execute's src/parsers/mdx.lua (the reading, mdx.sample,
 * mdx.sequence, mdx.nodes) and src/assets/gpu.lua (geosets and layers
 * into drawable pieces, their alpha at rest).
 *
 * Every read checks the file's end; the first that would pass it stops
 * the reading with the place it stopped. Counts are checked against the
 * bytes left before anything is allocated, so a damaged count can't ask
 * for more memory than the file could fill.
 */
#include "061-mdx.h"
#include "013-errors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* {{{ the reading's state: the bytes, where it is, the model being filled,
 * and how much room each of the model's lists has */
typedef struct {
    const unsigned char *s;
    size_t   n, pos;
    WErr     err;
    WWhy    *why;
    MdxModel *m;
    uint32_t cap_seq, cap_glb, cap_mat, cap_lay, cap_geo, cap_gea, cap_node, cap_piv, cap_txan, cap_trk, cap_key,
             cap_ext, cap_ft, cap_fc, cap_face, cap_vg, cap_gs, cap_gi, cap_ev, cap_chunk, cap_vert, cap_tex;
} Rd;
/* }}} */

/* {{{ static fail: the first failure is the one reported */
static void fail(Rd *r, WErr code, const char *what)
{
    if (r->err) return;
    r->err = w_fail(r->why, code, "the model: %s (at byte %zu of %zu)", what, r->pos, r->n);
}
/* }}} */

/* {{{ static readers: each gives 0 past the end, having failed */
static int need(Rd *r, size_t k)
{
    if (r->err) return 0;
    if (r->pos > r->n || k > r->n - r->pos) { fail(r, W_ERR_CORRUPT, "a read runs past the file's end"); return 0; }
    return 1;
}
static uint32_t u32(Rd *r)
{
    if (!need(r, 4)) return 0;
    const unsigned char *p = r->s + r->pos;
    r->pos += 4;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t i32(Rd *r) { return (int32_t)u32(r); }
static float f32(Rd *r) { uint32_t v = u32(r); float f; memcpy(&f, &v, 4); return f; }
static uint16_t u16(Rd *r)
{
    if (!need(r, 2)) return 0;
    const unsigned char *p = r->s + r->pos;
    r->pos += 2;
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint8_t u8(Rd *r) { if (!need(r, 1)) return 0; return r->s[r->pos++]; }
/* a four-letter tag into `out` (5 bytes, ended) */
static void tag(Rd *r, char *out)
{
    out[0] = '\0';
    if (!need(r, 4)) return;
    memcpy(out, r->s + r->pos, 4);
    out[4] = '\0';
    r->pos += 4;
}
static int peek_is(Rd *r, const char *t) { return !r->err && r->pos <= r->n && r->n - r->pos >= 4 && !memcmp(r->s + r->pos, t, 4); }
/* a fixed-length name, cut at its first zero (as the Lua's str): `cap` is out's size */
static void str(Rd *r, size_t len, char *out, size_t cap)
{
    out[0] = '\0';
    if (!need(r, len)) return;
    size_t k = 0;
    while (k < len && r->s[r->pos + k]) ++k;
    if (k > cap - 1) k = cap - 1;
    memcpy(out, r->s + r->pos, k);
    out[k] = '\0';
    r->pos += len;
}
static void floats(Rd *r, float *out, int count) { for (int i = 0; i < count; ++i) out[i] = f32(r); }
static void extent(Rd *r, MdxExtent *e) { e->radius = f32(r); floats(r, e->min, 3); floats(r, e->max, 3); }
/* }}} */

/* {{{ static room: makes room for `more` entries in a list, doubling */
static int room(Rd *r, void **list, uint32_t have, uint32_t more, uint32_t *cap, size_t each)
{
    if (r->err) return 0;
    if ((uint64_t)have + more <= *cap) return 1;
    uint64_t want = *cap ? *cap : 16;
    while (want < (uint64_t)have + more) want *= 2;
    if (want > 0xFFFFFFFFu) { fail(r, W_ERR_FULL, "a list would hold more than 2^32 entries"); return 0; }
    void *p = realloc(*list, (size_t)want * each);
    if (!p) { fail(r, W_ERR_FULL, "no memory left for its lists"); return 0; }
    *list = p;
    *cap = (uint32_t)want;
    return 1;
}
#define ROOM(r, list, n, more, cap) room((r), (void **)&(list), (n), (more), &(cap), sizeof *(list))
/* `count` entries of `each` bytes must fit in what's left of the file */
static int fits(Rd *r, uint64_t count, uint64_t each, const char *what)
{
    if (r->err) return 0;
    if (count * each > r->n - r->pos) {
        char line[200];
        snprintf(line, sizeof line, "%s: %llu entries run past the file's end", what, (unsigned long long)count);
        fail(r, W_ERR_CORRUPT, line);
        return 0;
    }
    return 1;
}
/* }}} */

/* {{{ tracks */
/* floats a value by tag (mdx.lua's TRACK_SIZE): 0 a whole number, -1 not known */
static int track_width(const char *t)
{
    static const char *const W1[] = { "KGAO", "KMTA", "KATV", "KLAV", "KLAI", "KLBI", "KLAS", "KLAE", "KP2V", "KP2S",
                                      "KP2E", "KP2L", "KP2G", "KP2W", "KP2N", "KP2Z", "KP2R", "KPEV", "KPPV", "KRVS",
                                      "KRHA", "KRHB", "KRAL", "KCRL" };
    static const char *const W3[] = { "KGTR", "KGSC", "KGAC", "KLAC", "KLBC", "KRCO", "KTAT", "KTAS", "KCTR", "KTTR" };
    static const char *const W4[] = { "KGRT", "KTAR" };
    for (size_t i = 0; i < sizeof W1 / sizeof *W1; ++i) if (!strcmp(t, W1[i])) return 1;
    for (size_t i = 0; i < sizeof W3 / sizeof *W3; ++i) if (!strcmp(t, W3[i])) return 3;
    for (size_t i = 0; i < sizeof W4 / sizeof *W4; ++i) if (!strcmp(t, W4[i])) return 4;
    if (!strcmp(t, "KMTF") || !strcmp(t, "KRTX")) return 0;
    return -1;
}
static void value(Rd *r, int width, MdxValue *v)
{
    memset(v, 0, sizeof *v);
    if (width == 0) v->u[0] = u32(r);
    else floats(r, v->f, width);
}
static void read_track(Rd *r, const char *t)
{
    MdxModel *m = r->m;
    int width = track_width(t);
    if (width < 0) {
        char line[80];
        snprintf(line, sizeof line, "a track tagged %.4s, which no record has", t);
        fail(r, W_ERR_CORRUPT, line);
        return;
    }
    uint32_t count = u32(r), interpolation = u32(r);
    int32_t global = i32(r);
    uint64_t each = 4 + (uint64_t)(width ? width : 1) * 4 * (interpolation > 1 ? 3 : 1);
    if (!fits(r, count, each, "a track's keys") || !ROOM(r, m->tracks, m->ntracks, 1, r->cap_trk)
        || !ROOM(r, m->keys, m->nkeys, count, r->cap_key)) return;
    MdxTrack *k = &m->tracks[m->ntracks++];
    memcpy(k->tag, t, 5);
    k->width = width;
    k->interpolation = interpolation;
    k->global = global >= 0 ? global : -1;
    k->keys = (MdxSpan){ m->nkeys, count };
    for (uint32_t i = 0; i < count; ++i) {
        MdxKey *key = &m->keys[m->nkeys++];
        memset(key, 0, sizeof *key);
        key->frame = i32(r);
        value(r, width, &key->value);
        if (interpolation > 1) { value(r, width, &key->in); value(r, width, &key->out); }
    }
}
/* tracks after a record's fixed fields, up to the record's end */
static void read_tracks(Rd *r, size_t stop, MdxSpan *into)
{
    into->first = r->m->ntracks;
    while (!r->err && r->pos < stop) {
        char t[5];
        tag(r, t);
        read_track(r, t);
    }
    into->count = r->m->ntracks - into->first;
}
/* }}} */

/* {{{ static record: a record that starts with its own size, which must
 * move the reading on and stay inside its chunk; its end, or 0 */
static size_t record(Rd *r, size_t stop, const char *what)
{
    size_t start = r->pos;
    uint32_t size = u32(r);
    if (r->err) return 0;
    if (size < 4 || size > stop - start) {
        char line[160];
        snprintf(line, sizeof line, "a record in %s of %u bytes, %s", what, size, size < 4 ? "too short to move on" : "past its chunk's end");
        fail(r, W_ERR_CORRUPT, line);
        return 0;
    }
    return start + size;
}
/* }}} */

/* {{{ chunks */
static void chunk_seqs(Rd *r, size_t stop)
{
    MdxModel *m = r->m;
    while (!r->err && r->pos < stop) {
        if (!ROOM(r, m->sequences, m->nsequences, 1, r->cap_seq)) return;
        MdxSequence *s = &m->sequences[m->nsequences];
        memset(s, 0, sizeof *s);
        str(r, 80, s->name, sizeof s->name);
        s->start = u32(r); s->finish = u32(r); s->move_speed = f32(r);
        s->non_looping = u32(r) == 1;
        s->rarity = f32(r);
        s->sync_point = u32(r);
        extent(r, &s->extent);
        if (!r->err) ++m->nsequences;
    }
}

static void chunk_glbs(Rd *r, size_t stop)
{
    MdxModel *m = r->m;
    while (!r->err && r->pos < stop) {
        if (!ROOM(r, m->global_sequences, m->nglobal_sequences, 1, r->cap_glb)) return;
        m->global_sequences[m->nglobal_sequences] = u32(r);
        if (!r->err) ++m->nglobal_sequences;
    }
}

static void chunk_texs(Rd *r, size_t stop)
{
    M2Model *d = &r->m->model;
    while (!r->err && r->pos < stop) {
        if (!ROOM(r, d->textures, d->ntextures, 1, r->cap_tex)) return;
        M2Texture *t = &d->textures[d->ntextures];
        memset(t, 0, sizeof *t);
        uint32_t replaceable = u32(r);
        t->kind = replaceable ? M2_TEXTURE_WC3 + replaceable : 0;
        str(r, 260, t->file, sizeof t->file);
        t->flags = u32(r);
        if (!r->err) ++d->ntextures;
    }
}

static void chunk_mtls(Rd *r, size_t stop)
{
    MdxModel *m = r->m;
    while (!r->err && r->pos < stop) {
        size_t end = record(r, stop, "the materials");
        if (!end || !ROOM(r, m->materials, m->nmaterials, 1, r->cap_mat)) return;
        MdxMaterial *mat = &m->materials[m->nmaterials];
        memset(mat, 0, sizeof *mat);
        mat->priority_plane = i32(r);
        mat->flags = u32(r);
        if (m->version > 800) str(r, 80, mat->shader, sizeof mat->shader);
        char t[5];
        tag(r, t);
        if (!r->err && strcmp(t, "LAYS")) { fail(r, W_ERR_CORRUPT, "a material without its LAYS"); return; }
        uint32_t count = u32(r);
        if (!fits(r, count, 28, "a material's layers")) return;
        mat->layers.first = m->nlayers;
        for (uint32_t i = 0; i < count && !r->err; ++i) {
            size_t lend = record(r, end, "a material's layers");
            if (!lend || !ROOM(r, m->layers, m->nlayers, 1, r->cap_lay)) return;
            MdxLayer *l = &m->layers[m->nlayers];
            memset(l, 0, sizeof *l);
            l->filter_mode = u32(r); l->shading = u32(r); l->texture = u32(r);
            l->texture_animation = i32(r); l->coord = u32(r); l->alpha = f32(r);
            if (m->version > 800) {
                l->emissive = f32(r);
                floats(r, l->fresnel, 3);
                l->fresnel_opacity = f32(r);
                l->fresnel_team = f32(r);
            }
            read_tracks(r, lend, &l->tracks);
            if (r->err) return;
            ++m->nlayers;
            r->pos = lend;
        }
        mat->layers.count = m->nlayers - mat->layers.first;
        r->pos = end;
        ++m->nmaterials;
    }
}

/* a counted sub-chunk of a geoset: its tag must be `want`; its count of
 * `per` values a count, `each` bytes a value; the count, or 0 having failed */
static uint32_t counted(Rd *r, const char *want, uint32_t per, uint32_t each)
{
    char t[5];
    tag(r, t);
    if (r->err) return 0;
    if (strcmp(t, want)) {
        char line[80];
        snprintf(line, sizeof line, "a geoset with %.4s where %s should be", t, want);
        fail(r, W_ERR_CORRUPT, line);
        return 0;
    }
    uint32_t count = u32(r);
    if (!fits(r, (uint64_t)count * per, each, want)) return 0;
    return count;
}

static void chunk_geos(Rd *r, size_t stop)
{
    MdxModel *m = r->m;
    M2Model *d = &m->model;
    while (!r->err && r->pos < stop) {
        size_t end = record(r, stop, "the geosets");
        if (!end || !ROOM(r, m->geosets, m->ngeosets, 1, r->cap_geo)) return;
        MdxGeoset *g = &m->geosets[m->ngeosets];
        memset(g, 0, sizeof *g);

        /* points first: the rest of each point fills in as it is read */
        uint32_t nv = counted(r, "VRTX", 3, 4);
        if (r->err || !ROOM(r, d->vertices, d->nvertices, nv, r->cap_vert)) return;
        g->vertices = (MdxSpan){ d->nvertices, nv };
        for (uint32_t i = 0; i < nv; ++i) {
            M2Vertex *v = &d->vertices[d->nvertices + i];
            memset(v, 0, sizeof *v);
            v->x = f32(r); v->y = f32(r); v->z = f32(r);
            v->nz = 1;
        }
        d->nvertices += nv;
        M2Vertex *pts = d->vertices + g->vertices.first;
        uint32_t nn = counted(r, "NRMS", 3, 4);
        g->normals_given = nn;
        for (uint32_t i = 0; i < nn && !r->err; ++i) {
            float n[3];
            floats(r, n, 3);
            if (i < nv) { pts[i].nx = n[0]; pts[i].ny = n[1]; pts[i].nz = n[2]; }
        }

        /* the lists kept as the file has them */
        #define LIST(want, each, list, n, cap, span, read) do { \
            uint32_t c_ = counted(r, want, 1, each); \
            if (r->err || !ROOM(r, m->list, m->n, c_, r->cap)) return; \
            g->span = (MdxSpan){ m->n, c_ }; \
            for (uint32_t i_ = 0; i_ < c_; ++i_) m->list[m->n++] = read(r); \
        } while (0)
        LIST("PTYP", 4, face_types, nface_types, cap_ft, face_types, u32);
        LIST("PCNT", 4, face_counts, nface_counts, cap_fc, face_counts, u32);
        LIST("PVTX", 2, faces, nfaces, cap_face, faces, u16);
        LIST("GNDX", 1, vertex_groups, nvertex_groups, cap_vg, vertex_groups, u8);
        LIST("MTGC", 4, group_sizes, ngroup_sizes, cap_gs, group_sizes, u32);
        LIST("MATS", 4, group_indices, ngroup_indices, cap_gi, group_indices, u32);
        #undef LIST

        g->material = u32(r);
        g->selection_group = u32(r);
        g->selection_flags = u32(r);
        if (m->version > 800) { g->lod = u32(r); str(r, 80, g->lod_name, sizeof g->lod_name); }
        extent(r, &g->extent);
        uint32_t ne = u32(r);
        if (!fits(r, ne, 28, "a geoset's sequence extents") || !ROOM(r, m->sequence_extents, m->nsequence_extents, ne, r->cap_ext)) return;
        g->sequence_extents = (MdxSpan){ m->nsequence_extents, ne };
        for (uint32_t i = 0; i < ne; ++i) extent(r, &m->sequence_extents[m->nsequence_extents++]);
        /* (Reforged models add tangents and skin weights here: passed over) */
        while (!r->err && r->pos < end && (peek_is(r, "TANG") || peek_is(r, "SKIN"))) {
            int tang = peek_is(r, "TANG");
            r->pos += 4;
            uint32_t c = u32(r);
            if (!fits(r, c, tang ? 16 : 1, tang ? "TANG" : "SKIN")) return;
            r->pos += (size_t)c * (tang ? 16 : 1);
        }
        char t[5];
        tag(r, t);
        if (!r->err && strcmp(t, "UVAS")) {
            char line[80];
            snprintf(line, sizeof line, "a geoset with %.4s where UVAS should be", t);
            fail(r, W_ERR_CORRUPT, line);
            return;
        }
        g->uv_sets = u32(r);
        for (uint32_t s = 0; s < g->uv_sets && !r->err; ++s) {
            uint32_t c = counted(r, "UVBS", 2, 4);
            if (s < 2) g->uv_given[s] = c;
            for (uint32_t i = 0; i < c && !r->err; ++i) {
                float u = f32(r), v = f32(r);
                if (i < nv && s == 0) { pts[i].u = u; pts[i].v = v; }
                if (i < nv && s == 1) { pts[i].u2 = u; pts[i].v2 = v; }
            }
        }
        if (r->err) return;
        r->pos = end;
        ++m->ngeosets;
    }
}

static void chunk_geoa(Rd *r, size_t stop)
{
    MdxModel *m = r->m;
    while (!r->err && r->pos < stop) {
        size_t end = record(r, stop, "the geoset animations");
        if (!end || !ROOM(r, m->geoset_animations, m->ngeoset_animations, 1, r->cap_gea)) return;
        MdxGeosetAnim *a = &m->geoset_animations[m->ngeoset_animations];
        memset(a, 0, sizeof *a);
        a->alpha = f32(r); a->flags = u32(r); floats(r, a->color, 3); a->geoset = u32(r);
        read_tracks(r, end, &a->tracks);
        if (r->err) return;
        r->pos = end;
        ++m->ngeoset_animations;
    }
}

/* one node's own record (its size, name, id, parent, flags, tracks), as a
 * new node of `kind`; NULL having failed */
static MdxNode *read_node(Rd *r, size_t stop, int kind)
{
    MdxModel *m = r->m;
    size_t end = record(r, stop, "a node");
    if (!end || !ROOM(r, m->nodes, m->nnodes, 1, r->cap_node)) return NULL;
    MdxNode *n = &m->nodes[m->nnodes];
    memset(n, 0, sizeof *n);
    n->kind = kind;
    str(r, 80, n->name, sizeof n->name);
    n->id = i32(r); n->parent = i32(r); n->flags = u32(r);
    n->geoset = n->geoset_animation = -1;
    n->event_global = -1;
    read_tracks(r, end, &n->tracks);
    if (r->err) return NULL;
    r->pos = end;
    ++m->nnodes;
    return n;
}

static void chunk_nodes(Rd *r, size_t stop, int kind)
{
    MdxModel *m = r->m;
    while (!r->err && r->pos < stop) {
        if (kind == MDX_BONE || kind == MDX_HELPER) {
            MdxNode *n = read_node(r, stop, kind);
            if (!n) return;
            if (kind == MDX_BONE) { n->geoset = i32(r); n->geoset_animation = i32(r); }
        } else if (kind == MDX_EVENT) {
            MdxNode *n = read_node(r, stop, kind);
            if (!n) return;
            if (r->pos < stop && peek_is(r, "KEVT")) {
                r->pos += 4;
                uint32_t c = u32(r);
                n->has_frames = 1;
                n->event_global = i32(r);
                if (!fits(r, c, 4, "an event's frames") || !ROOM(r, m->event_frames, m->nevent_frames, c, r->cap_ev)) return;
                n->event_frames = (MdxSpan){ m->nevent_frames, c };
                for (uint32_t i = 0; i < c; ++i) m->event_frames[m->nevent_frames++] = u32(r);
            }
        } else if (kind == MDX_COLLISION) {
            MdxNode *n = read_node(r, stop, kind);
            if (!n) return;
            n->shape = u32(r);
            n->npoints = n->shape == 2 ? 1 : 2;
            for (int i = 0; i < n->npoints; ++i) floats(r, n->points[i], 3);
            if (n->shape == 2 || n->shape == 3) n->radius = f32(r);
        } else {
            /* a record round a node: attachments, lights, emitters, ribbons */
            size_t end = record(r, stop, "the nodes");
            if (!end) return;
            MdxNode *n = read_node(r, end, kind);
            if (!n) return;
            if (kind == MDX_ATTACHMENT) {
                str(r, 260, n->path, sizeof n->path);
                n->attachment_id = u32(r);
                /* its visibility track (KATV) joins the node's own */
                MdxSpan more;
                read_tracks(r, end, &more);
                if (more.count) {
                    if (n->tracks.first + n->tracks.count == more.first) n->tracks.count += more.count;
                    else n->tracks = more;         /* (never: the node's tracks are read just before) */
                }
            }
            if (r->err) return;
            r->pos = end;
        }
    }
}

static void chunk_txan(Rd *r, size_t stop)
{
    MdxModel *m = r->m;
    while (!r->err && r->pos < stop) {
        size_t end = record(r, stop, "the texture animations");
        if (!end || !ROOM(r, m->texture_animations, m->ntexture_animations, 1, r->cap_txan)) return;
        MdxSpan *t = &m->texture_animations[m->ntexture_animations];
        read_tracks(r, end, t);
        if (r->err) return;
        r->pos = end;
        ++m->ntexture_animations;
    }
}

static void chunk_pivt(Rd *r, size_t stop)
{
    MdxModel *m = r->m;
    if ((stop - r->pos) % 12) { fail(r, W_ERR_CORRUPT, "pivot points (PIVT) that aren't a whole number of points"); return; }
    uint32_t c = (uint32_t)((stop - r->pos) / 12);
    if (!ROOM(r, m->pivots, m->npivots, c, r->cap_piv)) return;
    for (uint32_t i = 0; i < c; ++i) floats(r, m->pivots[m->npivots++], 3);
}
/* }}} */

/* {{{ static ascii_lower */
static void ascii_lower(const char *in, char *out, size_t cap)
{
    size_t k = 0;
    for (; in[k] && k + 1 < cap; ++k) out[k] = (in[k] >= 'A' && in[k] <= 'Z') ? (char)(in[k] - 'A' + 'a') : in[k];
    out[k] = '\0';
}
/* }}} */

/* {{{ mdx_sequence */
int mdx_sequence(const MdxModel *m, const char *name)
{
    char want[96], have[96];
    ascii_lower(name ? name : "stand", want, sizeof want);
    size_t wl = strlen(want);
    for (uint32_t i = 0; i < m->nsequences; ++i) {
        ascii_lower(m->sequences[i].name, have, sizeof have);
        const char *p = have;
        while (*p == ' ' || (*p >= '\t' && *p <= '\r')) ++p;          /* Lua's %s: space, \t \n \v \f \r */
        if (strncmp(p, want, wl)) continue;
        p += wl;
        while (*p == ' ' || (*p >= '\t' && *p <= '\r') || (*p >= '0' && *p <= '9')) ++p;
        if (!*p) return (int)i;
    }
    for (uint32_t i = 0; i < m->nsequences; ++i) {
        ascii_lower(m->sequences[i].name, have, sizeof have);
        if (strstr(have, want)) return (int)i;
    }
    return m->nsequences ? 0 : -1;
}
/* }}} */

/* {{{ mdx_track */
const MdxTrack *mdx_track(const MdxModel *m, MdxSpan tracks, const char *t)
{
    const MdxTrack *found = NULL;
    for (uint32_t i = 0; i < tracks.count && tracks.first + i < m->ntracks; ++i)
        if (!strncmp(m->tracks[tracks.first + i].tag, t, 4)) found = &m->tracks[tracks.first + i];
    return found;
}
/* }}} */

/* {{{ mdx_node */
int mdx_node(const MdxModel *m, int32_t id)
{
    int found = -1;
    for (int kind = 0; kind < MDX_NODE_KINDS; ++kind)
        for (uint32_t i = 0; i < m->nnodes; ++i)
            if (m->nodes[i].kind == kind && m->nodes[i].id == id) found = (int)i;
    return found;
}
/* }}} */

/* {{{ mdx_sample */
static int key_value(const MdxTrack *t, const MdxKey *k, double out[4])
{
    if (t->width == 0) { out[0] = (double)k->value.u[0]; return 1; }
    for (int i = 0; i < t->width; ++i) out[i] = (double)k->value.f[i];
    return t->width;
}
int mdx_sample(const MdxModel *m, const MdxTrack *t, double frame, const double *def, int ndef,
               int bounded, double first, double last, int rotation, double out[4])
{
    #define DEFAULT() do { for (int i_ = 0; i_ < ndef && i_ < 4; ++i_) out[i_] = def[i_]; return ndef; } while (0)
    if (!t || t->keys.count == 0 || t->keys.first + t->keys.count > m->nkeys) DEFAULT();
    const MdxKey *keys = m->keys + t->keys.first;
    uint32_t lo = 0, hi = t->keys.count - 1;
    if (bounded) {
        int any = 0;
        for (uint32_t i = 0; i < t->keys.count; ++i)
            if (keys[i].frame >= first && keys[i].frame <= last) { if (!any) lo = i; any = 1; hi = i; }
        if (!any) DEFAULT();
    }
    #undef DEFAULT
    if (frame <= keys[lo].frame) return key_value(t, &keys[lo], out);
    if (frame >= keys[hi].frame) return key_value(t, &keys[hi], out);
    for (uint32_t i = lo; i < hi; ++i) {
        const MdxKey *a = &keys[i], *b = &keys[i + 1];
        if (frame >= a->frame && frame < b->frame) {
            if (t->interpolation == 0) return key_value(t, a, out);
            double s = (frame - a->frame) / ((double)b->frame - (double)a->frame);   /* in doubles, as the Lua: no overflow at extreme frames */
            double va[4], vb[4];
            int w = key_value(t, a, va);
            key_value(t, b, vb);
            if (rotation && w == 4) {
                double dot = va[0] * vb[0] + va[1] * vb[1] + va[2] * vb[2] + va[3] * vb[3];
                if (dot < 0) { for (int c = 0; c < 4; ++c) vb[c] = -vb[c]; dot = -dot; }
                if (dot > 0.9995) {
                    for (int c = 0; c < 4; ++c) out[c] = va[c] + (vb[c] - va[c]) * s;
                    double len = sqrt(out[0] * out[0] + out[1] * out[1] + out[2] * out[2] + out[3] * out[3]);
                    for (int c = 0; c < 4; ++c) out[c] = out[c] / len;
                    return 4;
                }
                double th = acos(dot), sn = sin(th);
                double wa = sin((1 - s) * th) / sn, wb = sin(s * th) / sn;
                for (int c = 0; c < 4; ++c) out[c] = va[c] * wa + vb[c] * wb;
                return 4;
            }
            for (int c = 0; c < w; ++c) out[c] = va[c] + (vb[c] - va[c]) * s;
            return w;
        }
    }
    return key_value(t, &keys[hi], out);
}
/* }}} */

/* {{{ static build: the drawable model from what was read (gpu.lua's build,
 * with every picture taken as found: what to draw when one is missing is
 * the drawing's choice) */
static void build(Rd *r)
{
    MdxModel *m = r->m;
    M2Model *d = &m->model;
    uint32_t ng = m->ngeosets;

    /* each geoset's alpha at the start of Stand (gpu.lua's stand_alphas):
     * the last geoset animation naming it decides */
    int stand = mdx_sequence(m, "stand");
    double frame = stand >= 0 ? m->sequences[stand].start : 0;
    double first = stand >= 0 ? m->sequences[stand].start : 0, last = stand >= 0 ? m->sequences[stand].finish : 0;
    double *shown = malloc(sizeof(double) * (ng ? ng : 1));
    if (!shown) { fail(r, W_ERR_FULL, "no memory left for its pieces"); return; }
    for (uint32_t g = 0; g < ng; ++g) shown[g] = 1;
    for (uint32_t i = 0; i < m->ngeoset_animations; ++i) {
        const MdxGeosetAnim *a = &m->geoset_animations[i];
        double def = a->alpha, v[4];
        mdx_sample(m, mdx_track(m, a->tracks, "KGAO"), frame, &def, 1, stand >= 0, first, last, 0, v);
        if (a->geoset < ng) shown[a->geoset] = v[0];
    }

    d->pieces = calloc(ng ? ng : 1, sizeof *d->pieces);
    d->npieces = ng;
    uint32_t nidx = 0, nunit = 0;
    for (uint32_t g = 0; g < ng; ++g) {
        const MdxGeoset *gs = &m->geosets[g];
        nidx += gs->faces.count;
        const MdxMaterial *mat = gs->material < m->nmaterials ? &m->materials[gs->material] : NULL;
        nunit += mat ? mat->layers.count : 1;
    }
    d->indices = malloc(sizeof(uint32_t) * (nidx ? nidx : 1));
    d->units = calloc(nunit ? nunit : 1, sizeof *d->units);
    m->unit_layer = malloc(sizeof(int32_t) * (nunit ? nunit : 1));
    if (!d->pieces || !d->indices || !d->units || !m->unit_layer) { free(shown); fail(r, W_ERR_FULL, "no memory left for its pieces"); return; }

    for (uint32_t g = 0; g < ng; ++g) {
        const MdxGeoset *gs = &m->geosets[g];
        uint32_t nv = gs->vertices.count, base = gs->vertices.first;
        M2Piece *p = &d->pieces[g];
        p->id = g;
        p->first = d->nindices;
        /* triangles only (face kind 4); a count past the faces stops there */
        uint32_t at = 0, n = 0;
        for (uint32_t k = 0; k < gs->face_types.count; ++k) {
            uint32_t count = k < gs->face_counts.count ? m->face_counts[gs->face_counts.first + k] : 0;
            if (m->face_types[gs->face_types.first + k] == 4)
                for (uint32_t i = 0; i < count && (uint64_t)at + i < gs->faces.count; ++i) {
                    uint32_t c = m->faces[gs->faces.first + at + i];
                    d->indices[d->nindices + n++] = base + (c < nv ? c : 0);   /* a corner past the geoset's points: its first (as m2_read) */
                }
            at = (uint64_t)at + count > 0xFFFFFFFFu ? 0xFFFFFFFFu : at + count;
        }
        n -= n % 3;
        if (nv == 0 || nv > 65535 || n < 3) n = 0;           /* nothing drawn (gpu.lua's rule) */
        p->count = n;
        d->nindices += n;
        if (!n) continue;

        const MdxMaterial *mat = gs->material < m->nmaterials ? &m->materials[gs->material] : NULL;
        uint32_t nl = mat ? mat->layers.count : 1;
        for (uint32_t l = 0; l < nl; ++l) {
            static const MdxLayer plain = { .alpha = 1, .texture_animation = -1 };
            int32_t li = mat ? (int32_t)(mat->layers.first + l) : -1;
            const MdxLayer *ly = li >= 0 ? &m->layers[li] : &plain;
            M2Unit *u = &d->units[d->nunits];
            m->unit_layer[d->nunits] = li;
            ++d->nunits;
            u->piece = g;
            u->texture = ly->texture;
            u->blend = (uint16_t)ly->filter_mode;
            u->flags = (uint16_t)(((ly->shading & 1) ? 0x1 : 0) | ((ly->shading & 32) ? 0x2 : 0) | ((ly->shading & 16) ? 0x4 : 0)
                                  | ((ly->shading & 64) ? 0x8 : 0) | ((ly->shading & 128) ? 0x10 : 0));
            u->colour = u->fade = -1;
            u->layers = 1;
            int moving = ly->texture_animation >= 0 && (uint32_t)ly->texture_animation < m->ntexture_animations;
            u->moving_uv = moving;
            u->tex[0] = ly->texture < d->ntextures ? (int)ly->texture : -1;
            u->uvset[0] = (ly->shading & 2) ? -1 : (ly->coord < 2 ? (int)ly->coord : 0);
            u->uvanim[0] = moving ? ly->texture_animation : -1;
            u->tex[1] = -1; u->uvset[1] = 0; u->uvanim[1] = -1;
            u->shader = 0;
            double def = ly->alpha, v[4];
            mdx_sample(m, li >= 0 ? mdx_track(m, ly->tracks, "KMTA") : NULL, frame, &def, 1, stand >= 0, first, last, 0, v);
            u->alpha = (float)(v[0] * shown[g]);
        }
    }
    free(shown);
}
/* }}} */

/* {{{ mdx_read */
WErr mdx_read(const unsigned char *s, size_t len, MdxModel *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    if (!s || len < 4 || memcmp(s, "MDLX", 4)) return w_fail(why, W_ERR_CORRUPT, "not a WC3 model (no MDLX mark)");
    Rd rd = { .s = s, .n = len, .pos = 4, .why = why, .m = out };
    Rd *r = &rd;
    while (!r->err && r->pos + 8 <= r->n) {
        char t[5];
        tag(r, t);
        uint32_t size = u32(r);
        if (size > r->n - r->pos) {
            char line[80];
            snprintf(line, sizeof line, "its chunk %.4s runs past the file's end", t);
            fail(r, W_ERR_CORRUPT, line);
            break;
        }
        size_t stop = r->pos + size;
        if (!ROOM(r, out->chunks, out->nchunks * 4, 4, r->cap_chunk)) break;
        memcpy(out->chunks + (size_t)out->nchunks * 4, t, 4);
        ++out->nchunks;
        if      (!strcmp(t, "VERS")) out->version = u32(r);
        else if (!strcmp(t, "MODL")) {
            str(r, 80, out->name, sizeof out->name);
            str(r, 260, out->animation_file, sizeof out->animation_file);
            extent(r, &out->extent);
            out->blend_time = u32(r);
        }
        else if (!strcmp(t, "SEQS")) chunk_seqs(r, stop);
        else if (!strcmp(t, "GLBS")) chunk_glbs(r, stop);
        else if (!strcmp(t, "TEXS")) chunk_texs(r, stop);
        else if (!strcmp(t, "MTLS")) chunk_mtls(r, stop);
        else if (!strcmp(t, "GEOS")) chunk_geos(r, stop);
        else if (!strcmp(t, "GEOA")) chunk_geoa(r, stop);
        else if (!strcmp(t, "BONE")) chunk_nodes(r, stop, MDX_BONE);
        else if (!strcmp(t, "HELP")) chunk_nodes(r, stop, MDX_HELPER);
        else if (!strcmp(t, "ATCH")) chunk_nodes(r, stop, MDX_ATTACHMENT);
        else if (!strcmp(t, "LITE")) chunk_nodes(r, stop, MDX_LIGHT);
        else if (!strcmp(t, "PREM")) chunk_nodes(r, stop, MDX_EMITTER);
        else if (!strcmp(t, "PRE2")) chunk_nodes(r, stop, MDX_EMITTER2);
        else if (!strcmp(t, "RIBB")) chunk_nodes(r, stop, MDX_RIBBON);
        else if (!strcmp(t, "EVTS")) chunk_nodes(r, stop, MDX_EVENT);
        else if (!strcmp(t, "CLID")) chunk_nodes(r, stop, MDX_COLLISION);
        else if (!strcmp(t, "TXAN")) chunk_txan(r, stop);
        else if (!strcmp(t, "PIVT")) chunk_pivt(r, stop);
        /* anything else (CAMS among them): passed over, its tag kept */
        if (!r->err && r->pos > stop) {
            char line[80];
            snprintf(line, sizeof line, "its chunk %.4s is shorter than what it holds", t);
            fail(r, W_ERR_CORRUPT, line);
        }
        r->pos = stop;
    }
    if (!r->err) build(r);
    if (r->err) { mdx_free(out); return r->err; }
    return W_OK;
}
/* }}} */

/* {{{ mdx_free */
void mdx_free(MdxModel *m)
{
    m2_free(&m->model);
    free(m->unit_layer); free(m->sequences); free(m->global_sequences); free(m->materials); free(m->layers);
    free(m->geosets); free(m->geoset_animations); free(m->nodes); free(m->pivots); free(m->texture_animations);
    free(m->tracks); free(m->keys); free(m->sequence_extents); free(m->face_types); free(m->face_counts);
    free(m->faces); free(m->vertex_groups); free(m->group_sizes); free(m->group_indices); free(m->event_frames);
    free(m->chunks);
    memset(m, 0, sizeof *m);
}
/* }}} */
