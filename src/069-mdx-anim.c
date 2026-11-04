/*
 * 069-mdx-anim.c (issue 402 step 4) - see 069-mdx-anim.h. Ported from
 * world-edit-to-execute's src/assets/anim.lua: anim.rig (the posing
 * order, the skins' groups, the parts' alphas), its sampling (sample1,
 * sample3, sample_rotation), pose_nodes and anim.pose_at, and the
 * attachment points of anim.pose.
 *
 * The arithmetic is the Lua's, step for step and in the same order, in
 * doubles (Lua's numbers), so the two give the same bits: the group
 * matrices are rounded to floats only at the end, as anim.lua's pose
 * string is. Where the Lua caches (the keys inside a sequence's bounds),
 * this looks them up instead: by halving when a track's keys are in
 * order, the usual case, else by reading them all, as the Lua does.
 */
#include "069-mdx-anim.h"
#include "013-errors.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* {{{ sampling: anim.lua's sample1, sample3 and sample_rotation */

/* the keys of `t` inside [first, last]: lo, hi (0 when none) */
static int keys_inside(const MdxRig *r, const MdxTrack *t, double first, double last, uint32_t *lo, uint32_t *hi)
{
    const MdxKey *k = r->m->keys + t->keys.first;
    uint32_t n = t->keys.count;
    if (r->sorted[t - r->m->tracks]) {
        uint32_t a = 0, b = n;                    /* the first key at or after first */
        while (a < b) { uint32_t mid = a + (b - a) / 2; if (k[mid].frame < first) a = mid + 1; else b = mid; }
        uint32_t c = a, d = n;                    /* the first key after last */
        while (c < d) { uint32_t mid = c + (d - c) / 2; if (k[mid].frame <= last) c = mid + 1; else d = mid; }
        if (c == a) return 0;
        *lo = a; *hi = c - 1;
        return 1;
    }
    int any = 0;
    for (uint32_t i = 0; i < n; ++i)
        if (k[i].frame >= first && k[i].frame <= last) { if (!any) *lo = i; any = 1; *hi = i; }
    return any;
}

/* the frame a track is read at, and its bounds: the sequence's, or its
 * global sequence's own loop at the clock's place in it (anim.lua's when) */
static void when(const MdxRig *r, const MdxTrack *t, double first, double last, double frame, double clock,
                 double *f, double *a0, double *a1)
{
    if (t->global >= 0) {
        double len = (uint32_t)t->global < r->m->nglobal_sequences ? (double)r->m->global_sequences[t->global] : 0;
        if (len <= 0) { *f = 0; *a0 = 0; *a1 = 0; return; }
        *f = clock - floor(clock / len) * len;   /* Lua's %: never negative */
        *a0 = 0; *a1 = len;
        return;
    }
    *f = frame; *a0 = first; *a1 = last;
}

/* the keys round `frame` (anim.lua's around): a, and b with the share t
 * of the way from a to b; 0 when frame is at or past an end (a alone) */
static int around(const MdxKey *k, uint32_t lo, uint32_t hi, double frame, uint32_t *a, uint32_t *b, double *t)
{
    if (frame <= k[lo].frame) { *a = lo; return 0; }
    if (frame >= k[hi].frame) { *a = hi; return 0; }
    while (hi - lo > 1) {
        uint32_t mid = (uint32_t)(((uint64_t)lo + hi) / 2);
        if (k[mid].frame <= frame) lo = mid; else hi = mid;
    }
    if (k[hi].frame <= k[lo].frame) { *a = hi; return 0; }   /* repeated (or unordered) keys */
    *a = lo; *b = hi;
    *t = (frame - k[lo].frame) / ((double)k[hi].frame - (double)k[lo].frame);
    return 1;
}

/* weights of a, a's out tangent, b's in tangent, b: hermite (2), else bezier */
static void curve(uint32_t kind, double t, double w[4])
{
    if (kind == 2) {
        double t2 = t * t;
        w[0] = t2 * (2 * t - 3) + 1; w[1] = t2 * (t - 2) + t; w[2] = t2 * (t - 1); w[3] = t2 * (3 - 2 * t);
        return;
    }
    double s = 1 - t;
    w[0] = s * s * s; w[1] = 3 * t * s * s; w[2] = 3 * t * t * s; w[3] = t * t * t;
}

static double v_of(const MdxTrack *t, const MdxValue *v, int i) { return t->width == 0 ? (double)v->u[0] : (double)v->f[i]; }

/* a track's `n` values (1 or 3) at the frame, `def` when it has none there;
 * a and b's keys as anim.lua's sample1 and sample3 find and blend them */
static void sample(const MdxRig *r, const MdxTrack *t, int n, const double *def, double first, double last,
                   double frame, double clock, double *out)
{
    uint32_t lo, hi, a, b;
    double f, a0, a1, s;
    if (!t || t->keys.count == 0 || t->keys.first + t->keys.count > r->m->nkeys) { memcpy(out, def, sizeof *out * n); return; }
    when(r, t, first, last, frame, clock, &f, &a0, &a1);
    if (!keys_inside(r, t, a0, a1, &lo, &hi)) { memcpy(out, def, sizeof *out * n); return; }
    const MdxKey *k = r->m->keys + t->keys.first;
    int between = around(k, lo, hi, f, &a, &b, &s);
    if (!between || t->interpolation == 0) { for (int i = 0; i < n; ++i) out[i] = v_of(t, &k[a].value, i); return; }
    if (t->interpolation == 1) {
        for (int i = 0; i < n; ++i) {
            double av = v_of(t, &k[a].value, i), bv = v_of(t, &k[b].value, i);
            out[i] = av + (bv - av) * s;
        }
        return;
    }
    double w[4];
    curve(t->interpolation, s, w);
    for (int i = 0; i < n; ++i)
        out[i] = v_of(t, &k[a].value, i) * w[0] + v_of(t, &k[a].out, i) * w[1] + v_of(t, &k[b].in, i) * w[2]
               + v_of(t, &k[b].value, i) * w[3];
}

/* a turn at the frame (anim.lua's sample_rotation): along the sphere,
 * the shorter way, for every way but 0; no turn when it has none there */
static void sample_turn(const MdxRig *r, const MdxTrack *t, double first, double last, double frame, double clock, double q[4])
{
    uint32_t lo, hi, a, b;
    double f, a0, a1, s;
    q[0] = 0; q[1] = 0; q[2] = 0; q[3] = 1;
    if (!t || t->keys.count == 0 || t->keys.first + t->keys.count > r->m->nkeys || t->width != 4) return;
    when(r, t, first, last, frame, clock, &f, &a0, &a1);
    if (!keys_inside(r, t, a0, a1, &lo, &hi)) return;
    const MdxKey *k = r->m->keys + t->keys.first;
    int between = around(k, lo, hi, f, &a, &b, &s);
    const float *p = k[a].value.f;
    if (!between || t->interpolation == 0) { for (int i = 0; i < 4; ++i) q[i] = p[i]; return; }
    const float *e = k[b].value.f;
    double x1 = p[0], y1 = p[1], z1 = p[2], w1 = p[3];
    double x2 = e[0], y2 = e[1], z2 = e[2], w2 = e[3];
    double dot = x1 * x2 + y1 * y2 + z1 * z2 + w1 * w2;
    if (dot < 0) { x2 = -x2; y2 = -y2; z2 = -z2; w2 = -w2; dot = -dot; }
    double wa, wb;
    if (dot > 0.9995) {
        wa = 1 - s; wb = s;
    } else {
        double th = acos(dot), sn = sin(th);
        wa = sin((1 - s) * th) / sn; wb = sin(s * th) / sn;
    }
    double x = x1 * wa + x2 * wb, y = y1 * wa + y2 * wb, z = z1 * wa + z2 * wb, w = w1 * wa + w2 * wb;
    double len = sqrt(x * x + y * y + z * z + w * w);
    if (len < 1e-9) return;
    q[0] = x / len; q[1] = y / len; q[2] = z / len; q[3] = w / len;
}
/* }}} */

/* {{{ mdx_rig_build */

/* every node by id: the last of the kind lists, later kinds winning */
typedef struct { int32_t id; int kind; uint32_t at; } ById;
static int by_id(const void *x, const void *y)
{
    const ById *a = x, *b = y;
    if (a->id != b->id) return a->id < b->id ? -1 : 1;
    if (a->kind != b->kind) return a->kind < b->kind ? -1 : 1;
    return a->at < b->at ? -1 : a->at > b->at;
}
typedef struct { int32_t depth, id; uint32_t u; } ByDepth;
static int by_depth(const void *x, const void *y)
{
    const ByDepth *a = x, *b = y;
    if (a->depth != b->depth) return a->depth < b->depth ? -1 : 1;
    return a->id < b->id ? -1 : a->id > b->id;
}
/* the place among the ids (sorted) of `id`, -1 none */
static int32_t find_id(const ById *ids, uint32_t n, int64_t id)
{
    uint32_t a = 0, b = n;
    while (a < b) {
        uint32_t mid = a + (b - a) / 2;
        if (ids[mid].id < id) a = mid + 1; else b = mid;
    }
    return a < n && ids[a].id == id ? (int32_t)a : -1;
}
static uint64_t hash_members(const uint32_t *v, uint32_t n)
{
    uint64_t h = 1469598103934665603ull ^ n;
    for (uint32_t i = 0; i < n; ++i) { h ^= v[i]; h *= 1099511628211ull; }
    return h;
}

WErr mdx_rig_build(const MdxModel *m, MdxRig *out, WWhy *why)
{
    memset(out, 0, sizeof *out);
    out->m = m;
    WErr err = W_OK;
    ById *all = NULL, *ids = NULL;
    ByDepth *order = NULL;
    int32_t *par = NULL, *depth = NULL, *slot = NULL;
    uint32_t *stack = NULL, *table = NULL;
    unsigned char *state = NULL;
    #define NEED(p) if (!(p)) { err = w_fail(why, W_ERR_FULL, "the rig: out of memory (%s)", #p); goto done; }

    /* {{{ the nodes, one an id, parents before children */
    uint32_t nn = m->nnodes, nu = 0;
    NEED(all = calloc(nn ? nn : 1, sizeof *all));
    for (uint32_t i = 0; i < nn; ++i) all[i] = (ById){ m->nodes[i].id, m->nodes[i].kind, i };
    qsort(all, nn, sizeof *all, by_id);
    NEED(ids = calloc(nn ? nn : 1, sizeof *ids));
    for (uint32_t i = 0; i < nn; ++i) {
        if (nu && ids[nu - 1].id == all[i].id) ids[nu - 1] = all[i];     /* (a repeated id: the last one) */
        else ids[nu++] = all[i];
    }
    NEED(par = calloc(nu ? nu : 1, sizeof *par));
    NEED(depth = calloc(nu ? nu : 1, sizeof *depth));
    NEED(state = calloc(nu ? nu : 1, 1));
    NEED(stack = calloc(nu ? nu : 1, sizeof *stack));
    for (uint32_t u = 0; u < nu; ++u) {
        int32_t p = m->nodes[ids[u].at].parent;
        par[u] = (p < 0 || p == ids[u].id) ? -1 : find_id(ids, nu, p);
    }
    /* each node's depth, walking up from the lowest id not yet known; a
     * walk that comes back round to itself is cut where it does */
    for (uint32_t s = 0; s < nu; ++s) {
        if (state[s] == 2) continue;
        uint32_t sp = 0, x = s;
        for (;;) {
            stack[sp++] = x; state[x] = 1;
            int32_t p = par[x];
            if (p >= 0 && state[p] == 2) break;
            if (p >= 0 && state[p] == 1) par[x] = -1;
            if (par[x] < 0) { depth[x] = 0; state[x] = 2; --sp; break; }
            x = (uint32_t)p;
        }
        while (sp > 0) { x = stack[--sp]; depth[x] = depth[par[x]] + 1; state[x] = 2; }
    }
    NEED(order = calloc(nu ? nu : 1, sizeof *order));
    for (uint32_t u = 0; u < nu; ++u) order[u] = (ByDepth){ depth[u], ids[u].id, u };
    qsort(order, nu, sizeof *order, by_depth);
    NEED(slot = calloc(nu ? nu : 1, sizeof *slot));
    for (uint32_t i = 0; i < nu; ++i) slot[order[i].u] = (int32_t)i;
    NEED(out->nodes = calloc(nu ? nu : 1, sizeof *out->nodes));
    out->nnodes = nu;
    for (uint32_t i = 0; i < nu; ++i) {
        uint32_t u = order[i].u;
        const MdxNode *n = &m->nodes[ids[u].at];
        MdxRigNode *rn = &out->nodes[i];
        rn->id = ids[u].id;
        rn->node = (int32_t)ids[u].at;
        rn->parent = par[u] >= 0 ? slot[par[u]] : -1;
        if (rn->id >= 0 && (uint32_t)rn->id < m->npivots) memcpy(rn->pivot, m->pivots[rn->id], sizeof rn->pivot);
        rn->move = mdx_track(m, n->tracks, "KGTR");
        rn->turn = mdx_track(m, n->tracks, "KGRT");
        rn->stretch = mdx_track(m, n->tracks, "KGSC");
        const MdxTrack *t3[3] = { rn->move, rn->turn, rn->stretch };
        for (int k = 0; k < 3; ++k) if (t3[k] && t3[k]->global >= 0) out->on_clock = 1;
    }
    for (uint32_t i = 0; i < nn; ++i)
        if (m->nodes[i].kind == MDX_ATTACHMENT) {
            int32_t u = find_id(ids, nu, m->nodes[i].id);
            if (u >= 0) out->nodes[slot[u]].attachment = 1;
        }
    /* }}} */

    /* {{{ which tracks' keys are in order */
    NEED(out->sorted = calloc(m->ntracks ? m->ntracks : 1, 1));
    for (uint32_t i = 0; i < m->ntracks; ++i) {
        const MdxTrack *t = &m->tracks[i];
        int ok = t->keys.first + (uint64_t)t->keys.count <= m->nkeys;
        for (uint32_t k = 1; ok && k < t->keys.count; ++k)
            if (m->keys[t->keys.first + k].frame < m->keys[t->keys.first + k - 1].frame) ok = 0;
        out->sorted[i] = (unsigned char)ok;
    }
    /* }}} */

    /* {{{ the groups: each geoset's matrix groups, the same ones kept once */
    uint64_t most_groups = 1, most_members = 0;
    for (uint32_t g = 0; g < m->ngeosets; ++g) {
        most_groups += m->geosets[g].group_sizes.count;
        most_members += m->geosets[g].group_indices.count;
    }
    if (most_groups > UINT32_MAX / 4 || most_members > UINT32_MAX) {
        err = w_fail(why, W_ERR_FULL, "the rig: the model's %llu matrix groups are more than it can hold", (unsigned long long)most_groups);
        goto done;
    }
    NEED(out->groups = calloc(most_groups, sizeof *out->groups));
    NEED(out->members = calloc(most_members ? most_members : 1, sizeof *out->members));
    NEED(out->geoset_groups = calloc(m->ngeosets ? m->ngeosets : 1, sizeof *out->geoset_groups));
    NEED(out->group_of = calloc(most_groups, sizeof *out->group_of));
    uint32_t tsize = 1;
    while (tsize < most_groups * 2) tsize <<= 1;
    NEED(table = malloc(sizeof *table * tsize));
    memset(table, 0xFF, sizeof *table * tsize);
    out->ngroups = 1;                                    /* group 0: no nodes, no change */
    table[hash_members(NULL, 0) & (tsize - 1)] = 0;
    uint32_t nof = 0;
    for (uint32_t g = 0; g < m->ngeosets; ++g) {
        const MdxGeoset *gs = &m->geosets[g];
        uint32_t ng = gs->group_sizes.count, nv = gs->vertices.count;
        int posed = ng >= 1 && gs->vertex_groups.count >= nv;
        for (uint32_t i = 0; posed && i < nv; ++i) if (m->vertex_groups[gs->vertex_groups.first + i] >= ng) posed = 0;
        out->geoset_groups[g] = (MdxSpan){ nof, 0 };
        if (!posed) continue;
        out->geoset_groups[g].count = ng;
        uint64_t at = 0;
        for (uint32_t k = 0; k < ng; ++k) {
            uint32_t size = m->group_sizes[gs->group_sizes.first + k], first = out->nmembers, n = 0;
            for (uint64_t j = 0; j < size && at + j < gs->group_indices.count; ++j) {
                uint32_t v = m->group_indices[gs->group_indices.first + at + j];
                int32_t u = v <= INT32_MAX ? find_id(ids, nu, (int32_t)v) : -1;
                if (u >= 0) out->members[first + n++] = (uint32_t)slot[u];
            }
            at += size;
            uint32_t h = (uint32_t)(hash_members(out->members + first, n) & (tsize - 1)), found = UINT32_MAX;
            while (table[h] != UINT32_MAX) {
                MdxSpan e = out->groups[table[h]];
                if (e.count == n && !memcmp(out->members + e.first, out->members + first, sizeof *out->members * n)) { found = table[h]; break; }
                h = (h + 1) & (tsize - 1);
            }
            if (found == UINT32_MAX) {
                if (out->ngroups == 65536) {
                    err = w_fail(why, W_ERR_FULL, "the rig: geoset %u brings the model past 65536 different matrix groups (a point's group is 16 bits)", g);
                    goto done;
                }
                found = out->ngroups++;
                out->groups[found] = (MdxSpan){ first, n };
                out->nmembers += n;
                table[h] = found;
            }
            out->group_of[nof++] = found;
        }
    }
    /* each point's group: its geoset's group for it, else group 0 */
    out->npoints = m->model.nvertices;
    NEED(out->point_group = calloc(out->npoints ? out->npoints : 1, sizeof *out->point_group));
    for (uint32_t g = 0; g < m->ngeosets; ++g) {
        const MdxGeoset *gs = &m->geosets[g];
        MdxSpan sp = out->geoset_groups[g];
        if (!sp.count) continue;
        for (uint32_t i = 0; i < gs->vertices.count && gs->vertices.first + i < out->npoints; ++i)
            out->point_group[gs->vertices.first + i] = (uint16_t)out->group_of[sp.first + m->vertex_groups[gs->vertex_groups.first + i]];
    }
    /* }}} */

    /* {{{ each unit's geoset animation (the last for its piece), and whether
     * an alpha follows a global sequence */
    out->nunits = m->model.nunits;
    NEED(out->unit_animation = calloc(out->nunits ? out->nunits : 1, sizeof *out->unit_animation));
    for (uint32_t k = 0; k < out->nunits; ++k) {
        out->unit_animation[k] = -1;
        for (uint32_t a = 0; a < m->ngeoset_animations; ++a)
            if (m->geoset_animations[a].geoset == m->model.units[k].piece) out->unit_animation[k] = (int32_t)a;
        const MdxTrack *ka = out->unit_animation[k] >= 0 ? mdx_track(m, m->geoset_animations[out->unit_animation[k]].tracks, "KGAO") : NULL;
        const MdxTrack *la = m->unit_layer[k] >= 0 ? mdx_track(m, m->layers[m->unit_layer[k]].tracks, "KMTA") : NULL;
        if ((ka && ka->global >= 0) || (la && la->global >= 0)) out->on_clock = 1;
    }
    /* }}} */

done:
    #undef NEED
    free(all); free(ids); free(order); free(par); free(depth); free(slot); free(stack); free(table); free(state);
    if (err) mdx_rig_free(out);
    return err;
}

void mdx_rig_free(MdxRig *r)
{
    free(r->nodes); free(r->groups); free(r->members); free(r->geoset_groups); free(r->group_of);
    free(r->point_group); free(r->unit_animation); free(r->sorted);
    memset(r, 0, sizeof *r);
}
/* }}} */

/* {{{ mdx_rig_pose: anim.lua's pose_nodes and the parts' alphas of pose_at */
WErr mdx_rig_pose(const MdxRig *r, int seq, double frame, double clock, double (*world)[12], float *alphas, WWhy *why)
{
    const MdxModel *m = r->m;
    if (seq < 0 || (uint32_t)seq >= m->nsequences)
        return w_fail(why, W_ERR_NOT_FOUND, "the model has no sequence %d (it has %u)", seq, m->nsequences);
    double first = m->sequences[seq].start, last = m->sequences[seq].finish;
    static const double zero[3] = { 0, 0, 0 }, one[3] = { 1, 1, 1 };
    for (uint32_t i = 0; i < r->nnodes; ++i) {
        const MdxRigNode *nd = &r->nodes[i];
        double tv[3], q[4], sv[3];
        sample(r, nd->move, 3, zero, first, last, frame, clock, tv);
        sample_turn(r, nd->turn, first, last, frame, clock, q);
        sample(r, nd->stretch, 3, one, first, last, frame, clock, sv);
        double tx = tv[0], ty = tv[1], tz = tv[2], qx = q[0], qy = q[1], qz = q[2], qw = q[3], sx = sv[0], sy = sv[1], sz = sv[2];
        /* some models store NaN keys (a WC2-style Great Hall's hidden roof): taken as no change */
        if (tx != tx || ty != ty || tz != tz) { tx = 0; ty = 0; tz = 0; }
        if (qx != qx || qy != qy || qz != qz || qw != qw) { qx = 0; qy = 0; qz = 0; qw = 1; }
        if (sx != sx || sy != sy || sz != sz) { sx = 1; sy = 1; sz = 1; }
        double xx = qx * qx, yy = qy * qy, zz = qz * qz;
        double xy = qx * qy, xz = qx * qz, yz = qy * qz;
        double wx = qw * qx, wy = qw * qy, wz = qw * qz;
        /* rotation times scaling */
        double a00 = (1 - 2 * (yy + zz)) * sx, a01 = 2 * (xy - wz) * sy, a02 = 2 * (xz + wy) * sz;
        double a10 = 2 * (xy + wz) * sx, a11 = (1 - 2 * (xx + zz)) * sy, a12 = 2 * (yz - wx) * sz;
        double a20 = 2 * (xz - wy) * sx, a21 = 2 * (yz + wx) * sy, a22 = (1 - 2 * (xx + yy)) * sz;
        /* about the pivot: p + t - A p */
        double px = nd->pivot[0], py = nd->pivot[1], pz = nd->pivot[2];
        double b0 = px + tx - (a00 * px + a01 * py + a02 * pz);
        double b1 = py + ty - (a10 * px + a11 * py + a12 * pz);
        double b2 = pz + tz - (a20 * px + a21 * py + a22 * pz);
        double *W = world[i];
        if (nd->parent >= 0) {
            const double *P = world[nd->parent];
            W[0] = P[0] * a00 + P[1] * a10 + P[2] * a20;
            W[1] = P[0] * a01 + P[1] * a11 + P[2] * a21;
            W[2] = P[0] * a02 + P[1] * a12 + P[2] * a22;
            W[3] = P[0] * b0 + P[1] * b1 + P[2] * b2 + P[3];
            W[4] = P[4] * a00 + P[5] * a10 + P[6] * a20;
            W[5] = P[4] * a01 + P[5] * a11 + P[6] * a21;
            W[6] = P[4] * a02 + P[5] * a12 + P[6] * a22;
            W[7] = P[4] * b0 + P[5] * b1 + P[6] * b2 + P[7];
            W[8] = P[8] * a00 + P[9] * a10 + P[10] * a20;
            W[9] = P[8] * a01 + P[9] * a11 + P[10] * a21;
            W[10] = P[8] * a02 + P[9] * a12 + P[10] * a22;
            W[11] = P[8] * b0 + P[9] * b1 + P[10] * b2 + P[11];
        } else {
            W[0] = a00; W[1] = a01; W[2] = a02; W[3] = b0;
            W[4] = a10; W[5] = a11; W[6] = a12; W[7] = b1;
            W[8] = a20; W[9] = a21; W[10] = a22; W[11] = b2;
        }
    }
    for (uint32_t k = 0; alphas && k < r->nunits; ++k) {
        double a = 1, la = 1;
        if (r->unit_animation[k] >= 0) {
            const MdxGeosetAnim *ga = &m->geoset_animations[r->unit_animation[k]];
            double def = ga->alpha;
            sample(r, mdx_track(m, ga->tracks, "KGAO"), 1, &def, first, last, frame, clock, &a);
        }
        if (m->unit_layer[k] >= 0) {
            const MdxLayer *ly = &m->layers[m->unit_layer[k]];
            double def = ly->alpha;
            sample(r, mdx_track(m, ly->tracks, "KMTA"), 1, &def, first, last, frame, clock, &la);
        }
        a = a * la;
        alphas[k] = (float)(a == a ? a : 1);
    }
    return W_OK;
}
/* }}} */

/* {{{ mdx_rig_groups: each group the average of its nodes' matrices */
void mdx_rig_groups(const MdxRig *r, const double (*world)[12], float (*poses)[16])
{
    for (uint32_t g = 0; g < r->ngroups; ++g) {
        MdxSpan sp = r->groups[g];
        float *o = poses[g];
        double avg[12];
        if (sp.count == 0) {
            for (int k = 0; k < 12; ++k) avg[k] = 0;
            avg[0] = 1; avg[5] = 1; avg[10] = 1;
        } else {
            for (int k = 0; k < 12; ++k) {
                double sum = 0;
                for (uint32_t j = 0; j < sp.count; ++j) sum = sum + world[r->members[sp.first + j]][k];
                avg[k] = sum / sp.count;
            }
        }
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col) o[col * 4 + row] = (float)avg[row * 4 + col];
        o[3] = 0; o[7] = 0; o[11] = 0; o[15] = 1;
    }
}
/* }}} */

/* {{{ mdx_rig_attachment */
int mdx_rig_attachment(const MdxRig *r, const double (*world)[12], int32_t id, double out[3])
{
    for (uint32_t i = 0; i < r->nnodes; ++i) {               /* a few dozen nodes: read through */
        const MdxRigNode *nd = &r->nodes[i];
        if (nd->id != id || !nd->attachment) continue;
        const double *W = world[i];
        double p0 = nd->pivot[0], p1 = nd->pivot[1], p2 = nd->pivot[2];
        double x = W[0] * p0 + W[1] * p1 + W[2] * p2 + W[3];
        double y = W[4] * p0 + W[5] * p1 + W[6] * p2 + W[7];
        double z = W[8] * p0 + W[9] * p1 + W[10] * p2 + W[11];
        if (x != x || y != y || z != z) return 0;
        out[0] = x; out[1] = y; out[2] = z;
        return 1;
    }
    return 0;
}
/* }}} */

/* {{{ mdx_rig_skin_point */
void mdx_rig_skin_point(const MdxRig *r, const float (*poses)[16], uint32_t point, const float in[3], float out[3],
                        const float *n_in, float *n_out)
{
    const float *m = poses[point < r->npoints ? r->point_group[point] : 0];
    float pos[3], nrm[3];
    for (int k = 0; k < 3; ++k) {
        pos[k] = m[k] * in[0] + m[4 + k] * in[1] + m[8 + k] * in[2] + m[12 + k];
        if (n_in) nrm[k] = m[k] * n_in[0] + m[4 + k] * n_in[1] + m[8 + k] * n_in[2];
    }
    memcpy(out, pos, sizeof pos);
    if (n_in) {
        float l = sqrtf(nrm[0] * nrm[0] + nrm[1] * nrm[1] + nrm[2] * nrm[2]);
        for (int k = 0; k < 3; ++k) n_out[k] = l > 0 ? nrm[k] / l : n_in[k];
    }
}
/* }}} */
