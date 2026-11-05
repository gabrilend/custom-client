/*
 * 091-model-solids.c (issue 1109 step 1o, 173q) - see 091-model-solids.h
 *
 * For a general audience: keeps one entry per model and per building the
 * tiles name, each with its solid triangles (read once), and the list of
 * places each tile puts them. A place is "realised" (its triangles and
 * pools added to the mover's worlds) by the first tile that meets it;
 * dropping that tile takes its triangles away and lets the other tiles
 * realise again what only it had placed. The same bookkeeping as the
 * doodad drawer's (042-doodads.c), without anything drawn.
 */
#include "091-model-solids.h"
#include "029-mover.h"
#include "087-world-start.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* {{{ one shape */
WErr model_solids_of_model(const PlacedModel *pm, float **tris, int *ntri, WWhy *why)
{
    *tris = NULL;
    *ntri = 0;
    if (!pm->m2_bytes || pm->wc3 || pm->source == ASSET_FROM_PLACEHOLDER) return W_OK;   /* a WC3 model, the box: walked through */
    return m2_collision(pm->m2_bytes, pm->m2_len, tris, ntri, why);
}

WErr model_solids_of_group(const WmoGroup *g, float **tris, int *ntri, WWhy *why)
{
    int nt = (int)(g->nindices / 3), add = 0;
    for (int t = 0; t < nt; ++t) if ((uint32_t)t < g->nflags && wmo_solid(g->tri_flags[t])) ++add;
    if (!add) return W_OK;
    float *o = realloc(*tris, sizeof(float) * 9 * (size_t)(*ntri + add));
    if (!o) return w_fail(why, W_ERR_FULL, "no memory for a building part's %d solid triangles", add);
    *tris = o;
    for (int t = 0; t < nt; ++t) {
        if ((uint32_t)t >= g->nflags || !wmo_solid(g->tri_flags[t])) continue;
        int ok = 1;
        for (int c = 0; c < 3; ++c) ok &= g->indices[t * 3 + c] < g->nvertices;
        if (!ok) continue;                                 /* a corner the part doesn't have */
        for (int c = 0; c < 3; ++c) {
            WmoVertex v = g->vertices[g->indices[t * 3 + c]];
            float *p = o + (size_t)*ntri * 9 + c * 3;
            p[0] = v.x; p[1] = v.y; p[2] = v.z;
        }
        ++*ntri;
    }
    return W_OK;
}

void model_solids_stand_in_size(const AdtPlace *p, float m[16])
{
    float across = fmaxf(fmaxf(p->hi[0] - p->lo[0], p->hi[2] - p->lo[2]), 1.0f), up = fmaxf(p->hi[1] - p->lo[1], 1.0f);
    for (int k = 0; k < 3; ++k) { m[k] *= across; m[4 + k] *= across; m[8 + k] *= up; }
}
/* }}} */

/* {{{ the mover's floor and walls */
float model_solids_ground(MapWorld *world, CollideWorld *solids, float x, float y, float z_top, float *gx, float *gy)
{
    float tx = 0, ty = 0, mx = 0, my = 0;
    float t = world ? maps_world_height(world, x, y, &tx, &ty) : -100000.0f;
    float m = solids ? collide_floor(solids, x, y, z_top, &mx, &my) : -100000.0f;
    /* the model's floor wins when it is higher than a usable map ground,
     * or when the map's ground is above her (she is beneath it) */
    int map_below = t > -10000 && t <= z_top + 0.01f;
    if (m > -10000 && (!map_below || m > t)) { *gx = mx; *gy = my; return m; }
    /* otherwise the map's ground: also when it lies above her and no
     * model floor does (she lands on it, or the slope rule decides) */
    *gx = tx; *gy = ty;
    return t;
}

int model_solids_wall(CollideWorld *solids, double x0, double y0, double x1, double y1, double z, float max_grad, float *nx, float *ny)
{
    if (!solids) return 0;
    return collide_wall(solids, x0, y0, x1, y1, z, WORLD_HER_RADIUS, (float)MOVER_STEP_UP, WORLD_HER_HEIGHT, max_grad, nx, ny);
}
/* }}} */

/* {{{ state
 * A model: its name, its solid triangles (model space). A building: its
 * name, its root (for its furnishings), its parts' solid triangles, its
 * pools. A tile's record: every placement it lists. Seen: which tile
 * realised a placement, and what that added (for the counts). */
typedef struct { char name[260]; float *solid; int nsolid; } SolidModel;
typedef struct { WmoPool pool; int kind; } SolidPool;
typedef struct { char name[260]; int read, stand_in; WmoRoot root; float *solid; int nsolid; SolidPool *pools; int npools; } SolidBuilding;
typedef struct { uint32_t uid; int building, which, set; float at[16]; } Place;
typedef struct { int tile; Place *p; int n; } Record;
typedef struct { uint32_t uid; int building, tile, copies, solid_copies, pools; } Seen;

struct ModelSolids {
    Resolver      *assets;
    MpqChain      *chain;
    double         wc3_yards;
    CollideWorld  *solids;
    LiquidWorld   *liquids;
    FILE          *log;
    SolidModel    *models;     int nmodels, models_cap;
    SolidBuilding *buildings;  int nbuildings, buildings_cap;
    Record        *records;    int nrecords;
    Seen          *seen;       int nseen, seen_cap;
    int           *index;      int index_cap;           /* seen by uid and kind: open addressing, slots hold a position + 1 */
    double         ox, oy;
    int            models_solid, models_wc3, stand_ins, left_out;
    int            copies, solid_copies, pools;
};
/* }}} */

/* {{{ model_solids_new, model_solids_free */
ModelSolids *model_solids_new(Resolver *assets, MpqChain *chain, double wc3_yards, CollideWorld *solids, LiquidWorld *liquids, FILE *log)
{
    ModelSolids *ms = calloc(1, sizeof *ms);
    if (!ms) return NULL;
    ms->assets = assets;
    ms->chain = chain;
    ms->wc3_yards = wc3_yards;
    ms->solids = solids;
    ms->liquids = liquids;
    ms->log = log ? log : stderr;
    return ms;
}

void model_solids_free(ModelSolids *ms)
{
    if (!ms) return;
    for (int i = 0; i < ms->nmodels; ++i) free(ms->models[i].solid);
    free(ms->models);
    for (int i = 0; i < ms->nbuildings; ++i) {
        SolidBuilding *b = &ms->buildings[i];
        if (b->read) wmo_free_root(&b->root);
        free(b->solid);
        for (int k = 0; k < b->npools; ++k) { free(b->pools[k].pool.height); free(b->pools[k].pool.cell); }
        free(b->pools);
    }
    free(ms->buildings);
    for (int i = 0; i < ms->nrecords; ++i) free(ms->records[i].p);
    free(ms->records);
    free(ms->seen);
    free(ms->index);
    free(ms);
}
/* }}} */

/* {{{ static model: a model by name, read once; its index, -1 with no memory */
static int model(ModelSolids *ms, const char *name)
{
    for (int i = 0; i < ms->nmodels; ++i) if (!strcasecmp(ms->models[i].name, name)) return i;
    if (ms->nmodels == ms->models_cap) {
        int cap = ms->models_cap ? ms->models_cap * 2 : 64;
        SolidModel *m = realloc(ms->models, sizeof *m * (size_t)cap);
        if (!m) return -1;
        ms->models = m;
        ms->models_cap = cap;
    }
    SolidModel *md = &ms->models[ms->nmodels];
    memset(md, 0, sizeof *md);
    snprintf(md->name, sizeof md->name, "%s", name);
    PlacedModel pm;
    WWhy why;
    if (placed_model_read(ms->assets, ms->chain, name, ms->wc3_yards, &pm, &why)) {
        fprintf(ms->log, "model solids: %s; left out\n", why.text);
        ++ms->left_out;
        return ms->nmodels++;
    }
    if (pm.source == ASSET_FROM_PLACEHOLDER || pm.source == ASSET_FROM_BASELINE) ++ms->stand_ins;   /* the box, or the baseline set's stand-in for a game model */
    if (pm.wc3) ++ms->models_wc3;
    if (model_solids_of_model(&pm, &md->solid, &md->nsolid, &why))
        fprintf(ms->log, "model solids: %s's solid shape: %s; it is walked through\n", name, why.text);
    if (md->nsolid) ++ms->models_solid;
    placed_model_free(&pm);
    return ms->nmodels++;
}
/* }}} */

/* {{{ static building: a building by name, read once; its index, -1 with no memory
 * A root no place has, with a resolver: the box stands in (said and
 * counted by the resolver; no solid). Any other root that can't be read:
 * left out, said once. A part or pool that can't be read: left out, said. */
static int building(ModelSolids *ms, const char *name)
{
    for (int i = 0; i < ms->nbuildings; ++i) if (!strcasecmp(ms->buildings[i].name, name)) return i;
    if (ms->nbuildings == ms->buildings_cap) {
        int cap = ms->buildings_cap ? ms->buildings_cap * 2 : 32;
        SolidBuilding *b = realloc(ms->buildings, sizeof *b * (size_t)cap);
        if (!b) return -1;
        ms->buildings = b;
        ms->buildings_cap = cap;
    }
    SolidBuilding *b = &ms->buildings[ms->nbuildings];
    memset(b, 0, sizeof *b);
    snprintf(b->name, sizeof b->name, "%s", name);
    WWhy why;
    WErr bad = placed_building_root(ms->assets, ms->chain, name, &b->root, NULL, &why);
    if (bad == W_ERR_NOT_FOUND && ms->assets) {
        PlacedModel pm;
        WWhy inner;
        if (placed_model_stand_in(ms->assets, name, &pm, &inner)) { fprintf(ms->log, "model solids: %s; left out\n", inner.text); ++ms->left_out; }
        else { placed_model_free(&pm); b->stand_in = 1; ++ms->stand_ins; }
        return ms->nbuildings++;
    }
    if (bad) { fprintf(ms->log, "model solids: %s; left out\n", why.text); ++ms->left_out; return ms->nbuildings++; }
    b->read = 1;
    for (int gi = 0; gi < b->root.ngroups; ++gi) {
        WmoGroup g;
        if (placed_building_group(ms->assets, ms->chain, name, gi, &g, &why)) { fprintf(ms->log, "model solids: %s; that part is left out\n", why.text); continue; }
        if (model_solids_of_group(&g, &b->solid, &b->nsolid, &why)) fprintf(ms->log, "model solids: %s part %d: %s; left out\n", name, gi, why.text);
        if (g.pool.nx) {
            int kind = wmo_liquid_kind(&b->root, &g);
            SolidPool *pools = kind ? realloc(b->pools, sizeof *b->pools * (size_t)(b->npools + 1)) : NULL;
            if (!kind) fprintf(ms->log, "model solids: %s part %d's pool has no kind of liquid by the server's rule; left out\n", name, gi);
            else if (pools) {
                b->pools = pools;
                b->pools[b->npools++] = (SolidPool){ g.pool, kind };
                g.pool.height = NULL; g.pool.cell = NULL;   /* kept */
            }
        }
        wmo_free_group(&g);
    }
    return ms->nbuildings++;
}
/* }}} */

/* {{{ seen: a placement's uid and kind, looked up */
static unsigned slot(const ModelSolids *ms, uint32_t uid, int b) { return (uid * 2654435761u ^ (unsigned)b * 0x9E3779B9u) & (unsigned)(ms->index_cap - 1); }
static int seen_find(const ModelSolids *ms, uint32_t uid, int b)
{
    if (!ms->index_cap) return -1;
    for (unsigned s = slot(ms, uid, b); ms->index[s]; s = (s + 1) & (unsigned)(ms->index_cap - 1)) {
        const Seen *e = &ms->seen[ms->index[s] - 1];
        if (e->uid == uid && e->building == b) return ms->index[s] - 1;
    }
    return -1;
}
static int reindex(ModelSolids *ms)
{
    int cap = ms->index_cap ? ms->index_cap : 1024;
    while (cap < (ms->nseen + 1) * 2) cap *= 2;
    if (cap != ms->index_cap) {
        int *ix = realloc(ms->index, sizeof *ix * (size_t)cap);
        if (!ix) return 0;
        ms->index = ix;
        ms->index_cap = cap;
    }
    memset(ms->index, 0, sizeof *ms->index * (size_t)ms->index_cap);
    for (int i = 0; i < ms->nseen; ++i) {
        unsigned s = slot(ms, ms->seen[i].uid, ms->seen[i].building);
        while (ms->index[s]) s = (s + 1) & (unsigned)(ms->index_cap - 1);
        ms->index[s] = i + 1;
    }
    return 1;
}
/* }}} */

/* {{{ static realise: a tile's placements not yet placed by another tile */
static void copy(ModelSolids *ms, Seen *e, const float *solid, int nsolid, const float at[16], int tile)
{
    ++e->copies;
    if (!nsolid) return;
    ++e->solid_copies;
    if (ms->solids) collide_add(ms->solids, solid, nsolid, at, tile);
}
static WErr realise(ModelSolids *ms, const Record *r, WWhy *why)
{
    for (int k = 0; k < r->n; ++k) {
        const Place *p = &r->p[k];
        if (seen_find(ms, p->uid, p->building) >= 0) continue;   /* another tile placed it already */
        if (ms->nseen == ms->seen_cap) {
            int cap = ms->seen_cap ? ms->seen_cap * 2 : 1024;
            Seen *s = realloc(ms->seen, sizeof *s * (size_t)cap);
            if (!s) return w_fail(why, W_ERR_FULL, "no memory for %d placements", cap);
            ms->seen = s;
            ms->seen_cap = cap;
        }
        ms->seen[ms->nseen++] = (Seen){ p->uid, p->building, r->tile, 0, 0, 0 };
        if (ms->index_cap < ms->nseen * 2) {
            if (!reindex(ms)) { --ms->nseen; return w_fail(why, W_ERR_FULL, "no memory for the placements' index"); }
        } else {
            unsigned s = slot(ms, p->uid, p->building);
            while (ms->index[s]) s = (s + 1) & (unsigned)(ms->index_cap - 1);
            ms->index[s] = ms->nseen;
        }
        Seen *e = &ms->seen[ms->nseen - 1];
        if (p->which < 0) continue;
        if (!p->building) { copy(ms, e, ms->models[p->which].solid, ms->models[p->which].nsolid, p->at, r->tile); continue; }
        /* a building, its pools, and the furnishings of set 0 (what every
         * placement shows) and of the set it asks for */
        SolidBuilding *b = &ms->buildings[p->which];
        copy(ms, e, b->solid, b->nsolid, p->at, r->tile);
        for (int q = 0; ms->liquids && q < b->npools; ++q) {
            liquid_add_pool(ms->liquids, r->tile, p->at, ms->ox, ms->oy, &b->pools[q].pool, b->pools[q].kind);
            ++e->pools;
        }
        for (int si = 0; b->read && si < b->root.nsets; ++si) {
            if (si != 0 && si != p->set) continue;
            WmoSet set = b->root.sets[si];
            for (int fi = set.first; fi < set.first + set.count && fi < b->root.nfurnishings; ++fi) {
                WmoFurnishing f = b->root.furnishings[fi];
                if (!f.model[0]) continue;
                float fm[16];
                adt_inside_matrix(p->at, f.pos, f.quat, f.scale, fm);
                int which = model(ms, f.model);
                if (which < 0) return w_fail(why, W_ERR_FULL, "no memory for the model %s", f.model);
                b = &ms->buildings[p->which];
                copy(ms, e, ms->models[which].solid, ms->models[which].nsolid, fm, r->tile);
            }
        }
    }
    return W_OK;
}
/* }}} */

/* {{{ model_solids_add_tile */
WErr model_solids_add_tile(ModelSolids *ms, const AdtTile *t, int tile, double ox, double oy, WWhy *why)
{
    ms->ox = ox; ms->oy = oy;
    Record *rs = realloc(ms->records, sizeof *rs * (size_t)(ms->nrecords + 1));
    if (!rs) return w_fail(why, W_ERR_FULL, "no memory for tile %d's placements", tile);
    ms->records = rs;
    Record *r = &ms->records[ms->nrecords];
    r->tile = tile;
    r->n = 0;
    r->p = malloc(sizeof *r->p * (size_t)(t->ndoodads + t->nplaces + 1));
    if (!r->p) return w_fail(why, W_ERR_FULL, "no memory for tile %d's %d placements", tile, t->ndoodads + t->nplaces);
    ++ms->nrecords;
    for (int k = 0; k < t->ndoodads; ++k) {
        const AdtPlace *p = &t->doodads[k];
        Place *o = &r->p[r->n++];
        *o = (Place){ p->uid, 0, -1, 0, { 0 } };
        adt_place_matrix(p, ox, oy, o->at);
        if (p->name >= 0 && p->name < t->nmodels && (o->which = model(ms, t->models[p->name])) < 0)
            return w_fail(why, W_ERR_FULL, "no memory for the model %s", t->models[p->name]);
    }
    for (int k = 0; k < t->nplaces; ++k) {
        const AdtPlace *p = &t->places[k];
        Place *o = &r->p[r->n++];
        *o = (Place){ p->uid, 1, -1, p->doodad_set, { 0 } };
        adt_place_matrix(p, ox, oy, o->at);
        if (p->name < 0 || p->name >= t->nbuildings) continue;
        if ((o->which = building(ms, t->buildings[p->name])) < 0) return w_fail(why, W_ERR_FULL, "no memory for the building %s", t->buildings[p->name]);
        if (ms->buildings[o->which].stand_in) model_solids_stand_in_size(p, o->at);
    }
    return realise(ms, r, why);
}
/* }}} */

/* {{{ model_solids_drop_tile */
void model_solids_drop_tile(ModelSolids *ms, int tile)
{
    if (ms->solids) collide_drop_tile(ms->solids, tile);
    int k = 0;
    for (int i = 0; i < ms->nseen; ++i) if (ms->seen[i].tile != tile) ms->seen[k++] = ms->seen[i];
    ms->nseen = k;
    reindex(ms);                                           /* never grows here: it fits */
    for (int i = 0; i < ms->nrecords; ++i)
        if (ms->records[i].tile == tile) { free(ms->records[i].p); ms->records[i] = ms->records[--ms->nrecords]; break; }
    WWhy why;
    for (int i = 0; i < ms->nrecords; ++i)
        if (realise(ms, &ms->records[i], &why)) fprintf(ms->log, "model solids: %s; tile %d's models are left out\n", why.text, ms->records[i].tile);
}
/* }}} */

/* {{{ model_solids_counts */
void model_solids_counts(const ModelSolids *ms, ModelSolidsCounts *out)
{
    memset(out, 0, sizeof *out);
    out->models = ms->nmodels;
    out->models_solid = ms->models_solid;
    out->models_wc3 = ms->models_wc3;
    out->buildings = ms->nbuildings;
    out->stand_ins = ms->stand_ins;
    out->left_out = ms->left_out;
    for (int i = 0; i < ms->nseen; ++i) {
        out->copies += ms->seen[i].copies;
        out->solid_copies += ms->seen[i].solid_copies;
        out->pools += ms->seen[i].pools;
    }
    out->triangles = ms->solids ? collide_count(ms->solids) : 0;
}
/* }}} */
