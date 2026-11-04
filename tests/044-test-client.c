/*
 * test_client.c (issue 173k) - the C client's readers held to the same
 * facts as the Lua client's tests (scripts/test-custom-client and
 * scripts/test-client-models), on the same files, without a window.
 *
 * For a general audience: the C port must agree with the Lua original on
 * everything that can be measured: the ground's height at Astranaar, no
 * steps where map tiles meet, every movement speed, jump height, slopes
 * climbed and slid down, the archives' published keys, the night elf's
 * model and pictures, and which pieces of her show.
 *
 * Since 2026-10-06 the Lua client is deprecated (173k), and parts written
 * after that (the settings store, 173b1) are checked here on their own,
 * with no Lua twin.
 *
 * Usage: test-client PROJECT_DIR CLIENT_DATA_DIR
 */
#include "../src/020-adt.h"
#include "../src/013-errors.h"
#include "../src/023-anim.h"
#include "../src/017-blp.h"
#include "../src/031-camera.h"
#include "../src/025-character.h"
#include "../src/028-collide.h"
#include "../src/035-creatures.h"
#include "../src/016-dbc.h"
#include "../src/033-ground-prep.h"
#include "../src/026-light.h"
#include "../src/027-liquid.h"
#include "../src/022-m2.h"
#include "../src/018-maps.h"
#include "../src/029-mover.h"
#include "../src/030-moves.h"
#include "../src/015-mpq.h"
#include "../src/019-navmesh-slope.h"
#include "../src/024-particles.h"
#include "../src/034-prefetch.h"
#include "../src/014-settings.h"
#include "../src/032-sight.h"
#include "../src/021-wmo.h"
#include "../src/049-jpeg.h"
#include "../src/061-mdx.h"
#include "../src/069-mdx-anim.h"

#define RAYMATH_STATIC_INLINE              /* no raylib library in the tests: the maths compiled in here */
#include "raymath.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, fails;

/* WC3's footman picture (Textures\\Footman.blp in war3.mpq) as
 * world-edit-to-execute's Lua reader decodes it: every byte folded as
 * sum = sum * 31 + byte, kept to 32 bits (worked out 2026-10-07) */
#define FOOTMAN_SUM 0xAF3B44C6u

/* {{{ check */
static void check(int ok, const char *what)
{
    ++checks;
    if (ok) printf("  ok:   %s\n", what);
    else { printf("  FAIL: %s\n", what); ++fails; }
}
static int near(double a, double b, double tol) { return fabs(a - b) <= tol; }
/* a stand-in for the card's pictures (173u): an arbitrary half of them,
 * those with an odd-length name, count as sent already */
static int test_on_card(void *ctx, const char *name) { (void)ctx; return strlen(name) % 2; }
/* }}} */

/* {{{ made-up grounds: height, then rise per yard along x and y, as the map gives */
static float g_flat(void *c, float x, float y, float zt, float *gx, float *gy) { (void)zt; (void)c; (void)x; (void)y; *gx = 0; *gy = 0; return 10; }
static float g_slope(void *c, float x, float y, float zt, float *gx, float *gy) { (void)zt; (void)c; (void)y; *gx = -0.5f; *gy = 0; return 10 - x * 0.5f; }
static float g_cliff(void *c, float x, float y, float zt, float *gx, float *gy) { (void)zt; (void)c; (void)y; *gx = 0; *gy = 0; return x < 3 ? 10 : -40; }
static double ramp_tan;
static float g_ramp(void *c, float x, float y, float zt, float *gx, float *gy)
{
    (void)c; (void)y; (void)zt;
    if (x < 2) { *gx = 0; *gy = 0; return 10; }
    *gx = (float)ramp_tan; *gy = 0;
    return (float)(10 + (x - 2) * ramp_tan);
}
static float g_world(void *c, float x, float y, float zt, float *gx, float *gy) { (void)zt; return maps_world_height((MapWorld *)c, x, y, gx, gy); }
/* }}} */

/* {{{ a flat world at height 0 with the collision tests' shapes on it (173q) */
static CollideWorld *solids_test;
static float g_solid(void *c, float x, float y, float zt, float *gx, float *gy)
{
    (void)c;
    float mx, my, m = collide_floor(solids_test, x, y, zt, &mx, &my);
    if (m > 0) { *gx = mx; *gy = my; return m; }
    *gx = 0; *gy = 0;
    return 0;
}
static int w_solid(void *c, double x0, double y0, double x1, double y1, double z, float *nx, float *ny)
{
    (void)c;
    return collide_wall(solids_test, x0, y0, x1, y1, z, 0.4f, (float)MOVER_STEP_UP, 2.3f, 1.73f, nx, ny);
}
/* }}} */

/* {{{ a bank: ground at 0, rising 3 yards at x 100 (as a river's bank
 * onto which a building's wall is set; the wall is in solids_test) */
static float g_bank(void *c, float x, float y, float zt, float *gx, float *gy)
{
    (void)c; (void)y; (void)zt;
    *gx = 0; *gy = 0;
    return x < 100 ? 0 : 3;
}
/* }}} */

/* {{{ the swimming tests' world: a shore rising out of water 5 deep */
static float g_shore(void *c, float x, float y, float zt, float *gx, float *gy)
{
    (void)c; (void)y; (void)zt;
    *gy = 0;
    if (x < 10) { *gx = 0; return 0; }
    *gx = 0.5f;
    return (x - 10) * 0.5f;
}
static float w_sea(void *c, double x, double y) { (void)c; (void)x; (void)y; return 5; }
/* }}} */

/* {{{ static check_vmap_placements (173g step 4): the client's placement
 * of each doodad and building on tile `t` against the server's collision
 * tiles for it (data-files/vmaps/001_<a>_<b>.vmtile, both number orders
 * read: a placement reaching over a tile edge is listed in each tile it
 * touches). A vmtile: 8 bytes of magic, a count, then per placement:
 * flags (uint32: 1 a small model, 2 a whole-map model, 4 a box follows),
 * an ADT id (uint16), its id (uint32), position (3 floats, the server's
 * inner space: x = 17066.67 - world x, y = 17066.67 - world y, z up),
 * the file's three angles (degrees), its size, the box (6 floats) when
 * flagged, the name's length (uint32) and the name; then a uint32 the
 * server's tree uses. The server turns a model's points (kept as the model
 * has them) by Z(angle 2) Y(angle 1) X(angle 3), sizes them, and moves
 * them to the position (ModelInstance.cpp; vmap4_extractor). Matched by
 * place (within 0.05 yards) and name; then four points of each model,
 * placed both ways, are compared. */
static const char *base_name(const char *path, char *out, size_t n)
{
    const char *b = path;
    for (const char *c = path; *c; ++c) if (*c == '\\' || *c == '/') b = c + 1;
    snprintf(out, n, "%s", b);
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
    for (char *c = out; *c; ++c) if (*c >= 'A' && *c <= 'Z') *c = (char)(*c - 'A' + 'a');
    return out;
}
static void check_vmap_placements(const char *dir, const AdtTile *t, int *pairs, int *agree, float *worst)
{
    const double MID = 0.5 * 64 * 533.33333;
    const double D2R = 3.14159265358979323846 / 180;
    uint32_t seen[8192];
    int nseen = 0;
    for (int order = 0; order < 2; ++order) {
    char path[700];
        snprintf(path, sizeof path, "%s/assets/server-data/vmaps/001_%02d_%02d.vmtile", dir, order ? 26 : 32, order ? 32 : 26);
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        char magic[8];
        uint32_t n = 0;
        if (fread(magic, 1, 8, f) != 8 || fread(&n, 4, 1, f) != 1) { fclose(f); continue; }
        for (uint32_t s = 0; s < n; ++s) {
            uint32_t flags, id, name_len, node;
            uint16_t adt_id;
            float pos[3], rot[3], scale, box[6];
            char name[512];
            if (fread(&flags, 4, 1, f) != 1 || fread(&adt_id, 2, 1, f) != 1 || fread(&id, 4, 1, f) != 1
                || fread(pos, 4, 3, f) != 3 || fread(rot, 4, 3, f) != 3 || fread(&scale, 4, 1, f) != 1) break;
            if ((flags & 4) && fread(box, 4, 6, f) != 6) break;
            if (fread(&name_len, 4, 1, f) != 1 || name_len >= sizeof name || fread(name, 1, name_len, f) != name_len) break;
            name[name_len] = '\0';
            if (fread(&node, 4, 1, f) != 1) break;
            int dup = 0;
            for (int i = 0; i < nseen && !dup; ++i) dup = seen[i] == id;
            if (dup || (flags & 2) || nseen == 8192) continue;   /* a whole-map model has no ADT record */
            seen[nseen++] = id;
            /* the client's record at that place, of that name */
            double wx = MID - pos[0], wy = MID - pos[1], wz = pos[2];
            const AdtPlace *list = (flags & 1) ? t->doodads : t->places;
            int count = (flags & 1) ? t->ndoodads : t->nplaces;
            const AdtPlace *mine = NULL;
            char a[260], b[260];
            base_name(name, a, sizeof a);
            for (int k = 0; k < count && !mine; ++k) {
                const AdtPlace *p = &list[k];
                const char *pname = (flags & 1) ? t->models[p->name] : t->buildings[p->name];
                if (fabs(p->x - wx) < 0.05 && fabs(p->y - wy) < 0.05 && fabs(p->z - wz) < 0.05 && !strcmp(base_name(pname, b, sizeof b), a)) mine = p;
            }
            if (!mine) continue;                         /* a furnishing inside a building, or another tile's */
            ++*pairs;
            float m[16];
            adt_place_matrix(mine, 0, 0, m);
            double za = rot[1] * D2R, ya = rot[0] * D2R, xa = rot[2] * D2R;
            double cz = cos(za), sz = sin(za), cy = cos(ya), sy = sin(ya), cx = cos(xa), sx = sin(xa);
            /* R = Rz * Ry * Rx, row by row */
            double R[3][3] = { { cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx },
                               { sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx },
                               { -sy,     cy * sx,                cy * cx } };
            static const float PTS[4][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 3, -2, 5 } };
            float most = 0;
            for (int q = 0; q < 4; ++q) {
                const float *v = PTS[q];
                double in[3];
                for (int r = 0; r < 3; ++r) in[r] = pos[r] + scale * (R[r][0] * v[0] + R[r][1] * v[1] + R[r][2] * v[2]);
                double server[3] = { MID - in[0], MID - in[1], in[2] };
                for (int r = 0; r < 3; ++r) {
                    double client = m[r] * v[0] + m[4 + r] * v[1] + m[8 + r] * v[2] + m[12 + r];
                    most = fmaxf(most, (float)fabs(client - server[r]));
                }
            }
            if (most < 0.01f) ++*agree;
            if (most > *worst) *worst = most;
        }
        fclose(f);
    }
}
/* }}} */

/* {{{ a small WC3 model written here (401 step 2): the writer's pieces.
 * A chunk is its tag, its byte count and its body; a record with a size
 * of its own counts itself; both are counted when closed. */
typedef struct { unsigned char b[16384]; size_t n; } Bytes;
static void put32(Bytes *o, uint32_t v) { for (int i = 0; i < 4; ++i) o->b[o->n++] = (unsigned char)(v >> (8 * i)); }
static void put16(Bytes *o, uint16_t v) { o->b[o->n++] = (unsigned char)v; o->b[o->n++] = (unsigned char)(v >> 8); }
static void putf(Bytes *o, float f) { uint32_t v; memcpy(&v, &f, 4); put32(o, v); }
static void puttag(Bytes *o, const char *t) { memcpy(o->b + o->n, t, 4); o->n += 4; }
static void putname(Bytes *o, const char *s, size_t len) { memset(o->b + o->n, 0, len); memcpy(o->b + o->n, s, strlen(s)); o->n += len; }
static void putext(Bytes *o, float r) { putf(o, r); for (int i = 0; i < 3; ++i) putf(o, -r); for (int i = 0; i < 3; ++i) putf(o, r); }
static void patch32(Bytes *o, size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) o->b[at + i] = (unsigned char)(v >> (8 * i)); }
static size_t open_chunk(Bytes *o, const char *t) { puttag(o, t); put32(o, 0); return o->n; }
static void close_chunk(Bytes *o, size_t body) { patch32(o, body - 4, (uint32_t)(o->n - body)); }
static size_t open_record(Bytes *o) { put32(o, 0); return o->n - 4; }
static void close_record(Bytes *o, size_t at) { patch32(o, at, (uint32_t)(o->n - at)); }
static unsigned char *find_tag(Bytes *o, const char *t) { for (size_t i = 0; i + 4 <= o->n; ++i) if (!memcmp(o->b + i, t, 4)) return o->b + i; return NULL; }
/* a geoset: points (x y z, three a point), normals (as many as given),
 * triangles, its material; one matrix group, all points in it */
static void put_geoset(Bytes *o, const float *v, int nv, int nn, const uint16_t *tri, int nt, uint32_t material)
{
    size_t g = open_record(o);
    puttag(o, "VRTX"); put32(o, (uint32_t)nv); for (int i = 0; i < nv * 3; ++i) putf(o, v[i]);
    puttag(o, "NRMS"); put32(o, (uint32_t)nn); for (int i = 0; i < nn; ++i) { putf(o, 0); putf(o, 1); putf(o, 0); }
    puttag(o, "PTYP"); put32(o, 1); put32(o, 4);
    puttag(o, "PCNT"); put32(o, 1); put32(o, (uint32_t)nt);
    puttag(o, "PVTX"); put32(o, (uint32_t)nt); for (int i = 0; i < nt; ++i) put16(o, tri[i]);
    puttag(o, "GNDX"); put32(o, (uint32_t)nv); for (int i = 0; i < nv; ++i) o->b[o->n++] = 0;
    puttag(o, "MTGC"); put32(o, 1); put32(o, 1);
    puttag(o, "MATS"); put32(o, 1); put32(o, 0);
    put32(o, material); put32(o, 0); put32(o, 0);
    putext(o, 10); put32(o, 0);
    puttag(o, "UVAS"); put32(o, 1);
    puttag(o, "UVBS"); put32(o, (uint32_t)nv); for (int i = 0; i < nv; ++i) { putf(o, (float)i); putf(o, 0.5f); }
    close_record(o, g);
}
/* The model: three sequences ("Stand Ready" first, then "Stand 1" from
 * 1000 to 2000, "Death"); two pictures (team colour, a named one); two
 * materials (team colour under a blended skin whose alpha is animated;
 * one additive, unshaded, two-sided, unfogged, sphere-mapped layer);
 * three geosets (a square, a triangle with no normals that its geoset
 * animation hides at Stand's start, a triangle naming a material that
 * isn't there); two bones and a helper, their pivots. */
static void made_mdx(Bytes *o)
{
    o->n = 0;
    puttag(o, "MDLX");
    size_t c = open_chunk(o, "VERS"); put32(o, 800); close_chunk(o, c);
    c = open_chunk(o, "MODL"); putname(o, "Made here", 80); putname(o, "", 260); putext(o, 20); put32(o, 150); close_chunk(o, c);
    c = open_chunk(o, "SEQS");
    static const char *const names[3] = { "Stand Ready", "Stand 1", "Death" };
    static const uint32_t from[3] = { 0, 1000, 3000 }, to[3] = { 900, 2000, 4000 };
    for (int i = 0; i < 3; ++i) {
        putname(o, names[i], 80); put32(o, from[i]); put32(o, to[i]); putf(o, 0); put32(o, i == 2); putf(o, 0); put32(o, 0); putext(o, 20);
    }
    close_chunk(o, c);
    c = open_chunk(o, "TEXS");
    put32(o, 1); putname(o, "", 260); put32(o, 0);
    put32(o, 0); putname(o, "Textures\\Made.blp", 260); put32(o, 3);
    close_chunk(o, c);
    c = open_chunk(o, "MTLS");
    size_t mat = open_record(o); put32(o, 0); put32(o, 0); puttag(o, "LAYS"); put32(o, 2);
    size_t ly = open_record(o); put32(o, 0); put32(o, 0); put32(o, 0); put32(o, (uint32_t)-1); put32(o, 0); putf(o, 1); close_record(o, ly);
    ly = open_record(o); put32(o, 2); put32(o, 0); put32(o, 1); put32(o, (uint32_t)-1); put32(o, 0); putf(o, 0.8f);
    puttag(o, "KMTA"); put32(o, 3); put32(o, 1); put32(o, (uint32_t)-1);
    put32(o, 0); putf(o, 0.2f); put32(o, 1000); putf(o, 0.6f); put32(o, 2000); putf(o, 0.9f);
    close_record(o, ly); close_record(o, mat);
    mat = open_record(o); put32(o, 0); put32(o, 0); puttag(o, "LAYS"); put32(o, 1);
    ly = open_record(o); put32(o, 4); put32(o, 1 | 2 | 16 | 32); put32(o, 1); put32(o, (uint32_t)-1); put32(o, 0); putf(o, 1); close_record(o, ly);
    close_record(o, mat);
    close_chunk(o, c);
    c = open_chunk(o, "GEOS");
    static const float sq[12] = { 0, 0, 0, 10, 0, 0, 10, 10, 0, 0, 10, 0 }, tr[9] = { 0, 0, 5, 5, 0, 5, 5, 5, 5 }, tr2[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    static const uint16_t sq_t[6] = { 0, 1, 2, 0, 2, 3 }, tr_t[3] = { 0, 1, 2 };
    put_geoset(o, sq, 4, 4, sq_t, 6, 0);
    put_geoset(o, tr, 3, 0, tr_t, 3, 1);
    put_geoset(o, tr2, 3, 3, tr_t, 3, 7);
    close_chunk(o, c);
    c = open_chunk(o, "GEOA");
    size_t ga = open_record(o); putf(o, 1); put32(o, 0); putf(o, 1); putf(o, 1); putf(o, 1); put32(o, 1);
    puttag(o, "KGAO"); put32(o, 2); put32(o, 0); put32(o, (uint32_t)-1); put32(o, 0); putf(o, 1); put32(o, 1000); putf(o, 0);
    close_record(o, ga);
    close_chunk(o, c);
    c = open_chunk(o, "BONE");
    size_t nd = open_record(o); putname(o, "root", 80); put32(o, 0); put32(o, (uint32_t)-1); put32(o, 256);
    puttag(o, "KGTR"); put32(o, 2); put32(o, 1); put32(o, (uint32_t)-1);
    put32(o, 1000); putf(o, 0); putf(o, 0); putf(o, 0); put32(o, 2000); putf(o, 10); putf(o, 0); putf(o, 0);
    close_record(o, nd); put32(o, 0); put32(o, (uint32_t)-1);
    nd = open_record(o); putname(o, "arm", 80); put32(o, 1); put32(o, 0); put32(o, 256); close_record(o, nd); put32(o, 1); put32(o, (uint32_t)-1);
    close_chunk(o, c);
    c = open_chunk(o, "HELP");
    nd = open_record(o); putname(o, "ref", 80); put32(o, 2); put32(o, 1); put32(o, 0); close_record(o, nd);
    close_chunk(o, c);
    c = open_chunk(o, "PIVT");
    for (int i = 0; i < 9; ++i) putf(o, (float)i);
    close_chunk(o, c);
}
/* }}} */

/* {{{ a WC3 model written here to be posed (402 step 4): its writer's pieces.
 * A track: its tag, way, global sequence, then each key's frame and
 * value (`width` floats), with an in and an out tangent for ways 2 and 3
 * (vals holds them in that order). A node: its name, id, parent, flags,
 * then its tracks, counted when closed. */
static void put_track(Bytes *o, const char *tag, uint32_t way, int32_t global, int nkeys, int width,
                      const int32_t *frames, const float *vals)
{
    int each = width * (way > 1 ? 3 : 1);
    puttag(o, tag); put32(o, (uint32_t)nkeys); put32(o, way); put32(o, (uint32_t)global);
    for (int k = 0; k < nkeys; ++k) { put32(o, (uint32_t)frames[k]); for (int i = 0; i < each; ++i) putf(o, vals[k * each + i]); }
}
static size_t open_node(Bytes *o, const char *name, int32_t id, int32_t parent, uint32_t flags)
{
    size_t at = open_record(o);
    putname(o, name, 80); put32(o, (uint32_t)id); put32(o, (uint32_t)parent); put32(o, flags);
    return at;
}
/* a geoset with its own matrix groups: each point's group, the groups' sizes, their nodes' ids */
static void put_grouped(Bytes *o, const float *v, int nv, uint32_t material, const unsigned char *vg, int nvg,
                        const uint32_t *sizes, int nsizes, const uint32_t *ids, int nids)
{
    size_t g = open_record(o);
    puttag(o, "VRTX"); put32(o, (uint32_t)nv); for (int i = 0; i < nv * 3; ++i) putf(o, v[i]);
    puttag(o, "NRMS"); put32(o, (uint32_t)nv); for (int i = 0; i < nv; ++i) { putf(o, 0); putf(o, 0); putf(o, 1); }
    puttag(o, "PTYP"); put32(o, 1); put32(o, 4);
    puttag(o, "PCNT"); put32(o, 1); put32(o, 3);
    puttag(o, "PVTX"); put32(o, 3); put16(o, 0); put16(o, 1); put16(o, 2);
    puttag(o, "GNDX"); put32(o, (uint32_t)nvg); for (int i = 0; i < nvg; ++i) o->b[o->n++] = vg[i];
    puttag(o, "MTGC"); put32(o, (uint32_t)nsizes); for (int i = 0; i < nsizes; ++i) put32(o, sizes[i]);
    puttag(o, "MATS"); put32(o, (uint32_t)nids); for (int i = 0; i < nids; ++i) put32(o, ids[i]);
    put32(o, material); put32(o, 0); put32(o, 0);
    putext(o, 10); put32(o, 0);
    puttag(o, "UVAS"); put32(o, 1);
    puttag(o, "UVBS"); put32(o, (uint32_t)nv); for (int i = 0; i < nv * 2; ++i) putf(o, 0);
    close_record(o, g);
}
/* The model: Stand (0 to 1000) and Walk (2000 to 3000); one global
 * sequence of 400 ms; a chain of bones four deep, each with one way:
 *   root  (id 0, pivot 0 0 0)    moves in a straight line, (0, 0, 0) to (10, 0, 0) over Stand
 *   upper (id 1, pivot 10 0 0)   turns with way 0: none, then a quarter about z from 500
 *   lower (id 2, pivot 20 0 0)   stretches along a hermite curve, flat tangents, 1 to 3
 *   hand  (id 3, pivot 30 0 0)   moves along a bezier, both keys at 0, tangents lifting it 4 in z
 * a helper on the hand turning on the global sequence (id 4: none, half
 * a turn about z at 200, none again at 400), an attachment on the hand
 * (id 5, pivot 30 0 5), a billboarded bone on upper (id 6, no tracks).
 * Two geosets: the first's points in groups {root}, {lower, hand},
 * {root} again; the second's naming a group it hasn't (standing as made).
 * One material layer at alpha 0.5; the first geoset's animation fades
 * it out by way 0 at 600. */
static void posed_mdx(Bytes *o)
{
    const float s = sqrtf(0.5f);
    o->n = 0;
    puttag(o, "MDLX");
    size_t c = open_chunk(o, "VERS"); put32(o, 800); close_chunk(o, c);
    c = open_chunk(o, "MODL"); putname(o, "Posed here", 80); putname(o, "", 260); putext(o, 20); put32(o, 150); close_chunk(o, c);
    c = open_chunk(o, "SEQS");
    static const char *const names[2] = { "Stand", "Walk" };
    for (int i = 0; i < 2; ++i) {
        putname(o, names[i], 80); put32(o, (uint32_t)(i * 2000)); put32(o, (uint32_t)(i * 2000 + 1000));
        putf(o, 0); put32(o, 0); putf(o, 0); put32(o, 0); putext(o, 20);
    }
    close_chunk(o, c);
    c = open_chunk(o, "GLBS"); put32(o, 400); close_chunk(o, c);
    c = open_chunk(o, "TEXS"); put32(o, 0); putname(o, "Textures\\Posed.blp", 260); put32(o, 0); close_chunk(o, c);
    c = open_chunk(o, "MTLS");
    size_t mat = open_record(o); put32(o, 0); put32(o, 0); puttag(o, "LAYS"); put32(o, 1);
    size_t ly = open_record(o); put32(o, 2); put32(o, 0); put32(o, 0); put32(o, (uint32_t)-1); put32(o, 0); putf(o, 0.5f); close_record(o, ly);
    close_record(o, mat);
    close_chunk(o, c);
    c = open_chunk(o, "GEOS");
    static const float sq[12] = { 0, 0, 0, 30, 0, 0, 30, 0, 0, 0, 10, 0 }, tri[9] = { 1, 1, 1, 2, 2, 2, 3, 3, 3 };
    static const unsigned char vg0[4] = { 0, 1, 1, 2 }, vg1[3] = { 0, 0, 5 };
    static const uint32_t sz0[3] = { 1, 2, 1 }, id0[4] = { 0, 2, 3, 0 }, sz1[1] = { 1 }, id1[1] = { 1 };
    put_grouped(o, sq, 4, 0, vg0, 4, sz0, 3, id0, 4);
    put_grouped(o, tri, 3, 0, vg1, 3, sz1, 1, id1, 1);
    close_chunk(o, c);
    c = open_chunk(o, "GEOA");
    size_t ga = open_record(o); putf(o, 1); put32(o, 0); putf(o, 1); putf(o, 1); putf(o, 1); put32(o, 0);
    static const int32_t fade_at[2] = { 0, 600 };
    static const float fade[2] = { 1, 0 };
    put_track(o, "KGAO", 0, -1, 2, 1, fade_at, fade);
    close_record(o, ga);
    close_chunk(o, c);
    c = open_chunk(o, "BONE");
    static const int32_t ends[2] = { 0, 1000 }, turn_at[2] = { 0, 500 };
    static const float moves[6] = { 0, 0, 0, 10, 0, 0 };
    size_t nd = open_node(o, "root", 0, -1, 256); put_track(o, "KGTR", 1, -1, 2, 3, ends, moves);
    close_record(o, nd); put32(o, 0); put32(o, (uint32_t)-1);
    float turns[8] = { 0, 0, 0, 1, 0, 0, s, s };
    nd = open_node(o, "upper", 1, 0, 256); put_track(o, "KGRT", 0, -1, 2, 4, turn_at, turns);
    close_record(o, nd); put32(o, 0); put32(o, (uint32_t)-1);
    static const float stretch[18] = { 1, 1, 1, 0, 0, 0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0 };
    nd = open_node(o, "lower", 2, 1, 256); put_track(o, "KGSC", 2, -1, 2, 3, ends, stretch);
    close_record(o, nd); put32(o, 0); put32(o, (uint32_t)-1);
    static const float lift[18] = { 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 4, 0, 0, 0 };
    nd = open_node(o, "hand", 3, 2, 256); put_track(o, "KGTR", 3, -1, 2, 3, ends, lift);
    close_record(o, nd); put32(o, 0); put32(o, (uint32_t)-1);
    nd = open_node(o, "billboarded", 6, 1, 256 | 8); close_record(o, nd); put32(o, 0); put32(o, (uint32_t)-1);
    close_chunk(o, c);
    c = open_chunk(o, "HELP");
    static const int32_t spin_at[3] = { 0, 200, 400 };
    static const float spin[12] = { 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1 };
    nd = open_node(o, "spinner", 4, 3, 0); put_track(o, "KGRT", 1, 0, 3, 4, spin_at, spin); close_record(o, nd);
    close_chunk(o, c);
    c = open_chunk(o, "ATCH");
    size_t at = open_record(o);
    nd = open_node(o, "Hand Ref", 5, 3, 2048); close_record(o, nd);
    putname(o, "", 260); put32(o, 0);
    close_record(o, at);
    close_chunk(o, c);
    c = open_chunk(o, "PIVT");
    static const float piv[21] = { 0, 0, 0, 10, 0, 0, 20, 0, 0, 30, 0, 0, 0, 0, 0, 30, 0, 5, 10, 0, 0 };
    for (int i = 0; i < 21; ++i) putf(o, piv[i]);
    close_chunk(o, c);
}
/* bones alone (no pieces, one sequence): `n` of them, bone k's parent
 * parents[k] (n <= 80) */
static void bones_mdx(Bytes *o, int n, const int32_t *parents)
{
    o->n = 0;
    puttag(o, "MDLX");
    size_t c = open_chunk(o, "VERS"); put32(o, 800); close_chunk(o, c);
    c = open_chunk(o, "SEQS");
    putname(o, "Stand", 80); put32(o, 0); put32(o, 1000); putf(o, 0); put32(o, 0); putf(o, 0); put32(o, 0); putext(o, 20);
    close_chunk(o, c);
    c = open_chunk(o, "BONE");
    static const int32_t ends[2] = { 0, 1000 };
    static const float moves[6] = { 0, 0, 0, 1, 0, 0 };
    for (int k = 0; k < n; ++k) {
        char name[24];
        snprintf(name, sizeof name, "link %d", k);
        size_t nd = open_node(o, name, k, parents[k], 256); put_track(o, "KGTR", 1, -1, 2, 3, ends, moves);
        close_record(o, nd); put32(o, (uint32_t)-1); put32(o, (uint32_t)-1);
    }
    close_chunk(o, c);
}
/* a 3 x 4 matrix (rows) applied to a point */
static void apply34(const double w[12], double x, double y, double z, double out[3])
{
    for (int r = 0; r < 3; ++r) out[r] = w[r * 4] * x + w[r * 4 + 1] * y + w[r * 4 + 2] * z + w[r * 4 + 3];
}
/* within 1e-4: the file's turns are floats, so a quarter turn is a quarter to about 1e-7 */
static int near3(const double a[3], double x, double y, double z) { return near(a[0], x, 1e-4) && near(a[1], y, 1e-4) && near(a[2], z, 1e-4); }
/* }}} */

/* {{{ static run: seconds of steps of 0.01 s */
static void run(Mover *m, MoverInput in, double seconds, MoverGround g)
{
    int steps = (int)(seconds * 100 + 0.5);
    for (int i = 0; i < steps; ++i) mover_step(m, &in, 0.01, &(MoverWorld){ g, NULL, NULL, NULL });
}
/* }}} */

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "/mnt/kaun/wow-chat-2026";
    const char *client = argc > 2 ? argv[2] : "/mnt/dile/ritz/games/World of Warcraft 3.3.5a/Data";
    Settings *st = settings_new();                        /* the tests' settings store (014-settings.h), renewed where a test needs defaults */
    char path[700], line[300];
    WWhy why;

    /* {{{ the ground */
    printf("the ground (server map files)\n");
    MapWorld *w = malloc(sizeof *w);
    snprintf(path, sizeof path, "%s/assets/server-data/maps", dir);
    maps_world_init(w, path, 1);
    float gx, gy;
    float z = maps_world_height(w, 2828, -284, &gx, &gy);
    snprintf(line, sizeof line, "Astranaar's flight point stands at %.2f yards (Lua: 106.71)", z);
    check(near(z, 106.71, 0.01), line);
    float worst = 0;
    for (int edge = 25; edge <= 27; ++edge) {
        float x = (32 - edge) * MAPS_GRID_SIZE;
        for (int y = -900; y <= 100; y += 37) {
            float a = maps_world_height(w, x + 0.001f, (float)y, &gx, &gy), b = maps_world_height(w, x - 0.001f, (float)y, &gx, &gy);
            if (a > -10000 && b > -10000 && fabsf(a - b) > worst) worst = fabsf(a - b);
        }
    }
    snprintf(line, sizeof line, "no step where tiles meet (largest %.4f yards)", worst);
    check(worst < 0.05f, line);
    check(maps_world_height(w, 1e6f, 1e6f, &gx, &gy) < -10000, "far outside the world: no ground, not an error");
    /* }}} */

    /* {{{ movement */
    printf("movement at 120%% (made-up ground)\n");
    Mover m;
    MoverInput none = { 0 }, in;
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.forward = 1; run(&m, in, 1, g_flat);
    check(near(m.x, 8.4, 1e-6) && near(m.y, 0, 1e-6), "a second forward: 8.4 yards north");
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.back = 1; run(&m, in, 1, g_flat);
    check(near(m.x, -5.4, 1e-6), "a second back: 5.4 yards");
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.strafe_right = 1; run(&m, in, 1, g_flat);
    check(near(m.y, -8.4, 1e-6) && near(m.x, 0, 1e-6), "a second strafing right (east, -y): 8.4");
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.forward = 1; in.strafe_right = 1; run(&m, in, 1, g_flat);
    check(near(sqrt(m.x * m.x + m.y * m.y), 8.4, 1e-6), "forward and strafing together: still 8.4 yards a second");
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.left = 1; run(&m, in, 1, g_flat);
    check(near(m.o, 3.14159265358979, 1e-6), "a second turning left: half a circle");
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.right = 1; in.strafe_turns = 1; run(&m, in, 1, g_flat);
    check(near(m.o, 0, 1e-9) && near(m.y, -8.4, 1e-6), "with the right button held, D strafes instead of turning");
    /* the animation's pace (owner, 2026-10-07): the keys' speed, whether or
     * not she gets anywhere: running into a wall she runs on the spot */
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.forward = 1;
    check(near(mover_intended_speed(&m, &in), 8.4, 1e-9), "held forward, the keys ask for 8.4 yards a second, wall or no wall");
    in.walking = 1;
    check(near(mover_intended_speed(&m, &in), 3.0, 1e-9) && near(mover_intended_speed(&m, &none), 0, 1e-9), "walking 3; no keys 0");
    /* walking (173i): the walk speed, 2.5 a second at 120%, every way */
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.walking = 1; in.forward = 1; run(&m, in, 1, g_flat);
    check(near(m.x, 3.0, 1e-6), "walking, a second forward: 3 yards");
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.walking = 1; in.back = 1; run(&m, in, 1, g_flat);
    check(near(m.x, -3.0, 1e-6), "walking, a second back: 3 yards too");
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.walking = 1; in.strafe_left = 1; run(&m, in, 1, g_flat);
    check(near(m.y, 3.0, 1e-6), "walking, a second strafing left: 3 yards");

    mover_init(&m, 0, 0, 10, 0, 1.2, 60);
    in = none; in.jump = 1;
    mover_step(&m, &in, 0.001, &(MoverWorld){ g_flat, NULL, NULL, NULL });
    double top = 10;
    for (int i = 0; i < 200; ++i) { mover_step(&m, &none, 0.005, &(MoverWorld){ g_flat, NULL, NULL, NULL }); if (m.z > top) top = m.z; }
    double expect = MOVER_JUMP * MOVER_JUMP / (2 * MOVER_GRAVITY);
    snprintf(line, sizeof line, "a jump rises %.3f yards (expected %.3f)", top - 10, expect);
    check(near(top - 10, expect, 0.02), line);
    check(m.grounded && m.z == 10, "and lands back on the ground");

    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.forward = 1; run(&m, in, 1, g_slope);
    check(m.grounded && near(m.z, 10 - m.x * 0.5, 1e-4), "walking down a slope stays on the ground");
    mover_init(&m, 0, 0, 10, 0, 1.2, 60); in = none; in.forward = 1; run(&m, in, 0.5, g_cliff);
    check(!m.grounded && m.z < 10 && m.z > -40, "walking off a cliff falls");
    run(&m, none, 3, g_cliff);
    check(m.grounded && m.z == -40, "and lands at the bottom");
    /* }}} */

    /* {{{ steep ground */
    printf("steep ground (the server's navigation rule)\n");
    snprintf(path, sizeof path, "%s/assets/server-data/mmaps", dir);
    float limit = navmesh_slope_read(path, 1, &why);
    snprintf(line, sizeof line, "the server's tiles for Kalimdor say %.1f degrees is the steepest walkable", limit);
    check(near(limit, 60, 1e-4), line);
    ramp_tan = tan(45 * 3.14159265358979 / 180);
    mover_init(&m, 0, 0, 10, 0, 1.2, limit); in = none; in.forward = 1; run(&m, in, 1, g_ramp);
    check(m.x > 6, "up a 45-degree ramp: walks on");
    ramp_tan = tan(70 * 3.14159265358979 / 180);
    mover_init(&m, 0, 0, 10, 0, 1.2, limit); run(&m, in, 1, g_ramp);
    check(m.x < 2.3 && m.z < 10.8, "into a 70-degree slope: stopped at its foot");
    mover_init(&m, 5, 0, 10 + 3 * ramp_tan, 0, 1.2, limit);
    mover_step(&m, &none, 0.01, &(MoverWorld){ g_ramp, NULL, NULL, NULL });
    check(m.sliding && m.x < 5, "on a 70-degree slope with no keys pressed: sliding down it");
    run(&m, none, 3, g_ramp);
    check(m.grounded && !m.sliding && m.x < 2 && m.z == 10, "and comes to rest on the flat at the bottom");
    ramp_tan = 1;
    mover_init(&m, 4, 0, 12, 0, 1.2, limit); run(&m, none, 1, g_ramp);
    check(near(m.x, 4, 1e-9) && !m.sliding, "on a 45-degree slope, standing still stays put");
    /* the real ground: a second's run from the start point stays on the map */
    mover_init(&m, 2717, -400, maps_world_height(w, 2717, -400, &gx, &gy), 3.6, 1.2, limit);
    in = none; in.forward = 1;
    for (int i = 0; i < 100; ++i) mover_step(&m, &in, 0.01, &(MoverWorld){ g_world, NULL, NULL, w });
    check(m.grounded, "a second's run from the start point on Ashenvale's real ground stays on it");
    /* }}} */

    /* {{{ WC3's models (MDX, issue 401 step 2): one written here. It needs
     * none of the game's files, so it comes before the archives, which
     * end the program when the client's files aren't there. Every WC3
     * model in war3.mpq is held to world-edit-to-execute's Lua reader by
     * tests/062-mdx-matches-lua.lua. */
    printf("WC3's models (MDX)\n");
    {
        static Bytes fb;
        made_mdx(&fb);
        MdxModel x;
        WWhy mw;
        int read = mdx_read(fb.b, fb.n, &x, &mw) == W_OK;
        check(read, read ? "a WC3 model written here reads" : mw.text);
        if (read) {
            M2Model *d = &x.model;
            check(x.version == 800 && !strcmp(x.name, "Made here") && x.blend_time == 150 && x.nsequences == 3,
                  "its version (800), name, blend time and three sequences");
            check(mdx_sequence(&x, "stand") == 1 && mdx_sequence(&x, "death") == 2 && mdx_sequence(&x, "walk") == 0,
                  "\"stand\" is Stand 1, not Stand Ready; a name it hasn't gives the first");
            check(d->ntextures == 2 && d->textures[0].kind == M2_TEXTURE_WC3 + 1 && d->textures[1].kind == 0
                  && !strcmp(d->textures[1].file, "Textures\\Made.blp") && d->textures[1].flags == 3,
                  "its pictures: team colour as WC3's replaceable 1, kept apart from the game's kinds; a named one, wrapping both ways");
            snprintf(line, sizeof line, "every geoset's points in one list (%u of 10), the triangle's after the square's", d->nvertices);
            check(d->nvertices == 10 && x.geosets[1].vertices.first == 4 && d->vertices[5].x == 5 && d->vertices[5].z == 5, line);
            check(d->vertices[4].nx == 0 && d->vertices[4].nz == 1 && d->vertices[0].ny == 1 && d->vertices[5].u == 1 && d->vertices[5].v == 0.5f,
                  "a geoset with no normals faces up; normals and picture positions as given elsewhere");
            check(d->npieces == 3 && d->pieces[0].count == 6 && d->pieces[1].first == 6 && d->pieces[1].count == 3 && d->indices[6] == 4
                  && d->pieces[1].id == 1, "a piece a geoset, its triangles as model point numbers");
            check(d->nunits == 4, "a unit a geoset and material layer (2 + 1), and one plain unit for a geoset whose material isn't there");
            if (d->nunits == 4) {
                M2Unit *u = d->units;
                check(u[0].blend == 0 && u[0].texture == 0 && u[1].blend == 2 && u[1].texture == 1 && u[0].piece == 0 && u[1].piece == 0,
                      "the square: team colour (opaque) under its blended skin, WC3's filter modes as M2's blends");
                check(u[2].blend == 4 && u[2].flags == (0x1 | 0x2 | 0x4) && u[2].uvset[0] == -1,
                      "an add-alpha layer: unshaded, unfogged and two-sided as M2's flags, sphere-mapped as a reflection");
                snprintf(line, sizeof line, "the skin's alpha at rest is its track's at Stand 1's start (%.3f; 0.6, not 0.2 at 0 nor 0.8 unanimated)", u[1].alpha);
                check(near(u[1].alpha, 0.6, 1e-6), line);
                check(u[2].alpha == 0 && u[0].alpha == 1, "the triangle is hidden at rest, its geoset animation's alpha 0 at Stand's start");
                check(x.unit_layer[3] == -1 && u[3].blend == 0 && u[3].alpha == 1 && u[3].tex[0] == 0 && u[3].piece == 2,
                      "the geoset with no such material: one plain opaque layer, the first picture");
            }
            int ref = mdx_node(&x, 2);
            check(x.nnodes == 3 && ref == 2 && x.nodes[ref].kind == MDX_HELPER && x.nodes[ref].parent == 1 && x.npivots == 3
                  && x.nodes[0].geoset == 0 && x.nodes[1].geoset == 1 && x.pivots[2][1] == 7,
                  "two bones and a helper, each by its id, with its parent and pivot");
            double v[4], def[3] = { 0, 0, 0 };
            const MdxTrack *t = mdx_track(&x, x.nodes[0].tracks, "KGTR");
            int n = mdx_sample(&x, t, 1500, def, 3, 1, 1000, 2000, 0, v);
            check(t && n == 3 && near(v[0], 5, 1e-12) && v[1] == 0, "the root's move halfway through Stand 1: halfway (5, 0, 0)");
            n = mdx_sample(&x, mdx_track(&x, x.nodes[1].tracks, "KGTR"), 1500, def, 3, 1, 1000, 2000, 0, v);
            check(n == 3 && v[0] == 0, "a node with no move track: the default");
            mdx_free(&x);
        }
        static Bytes cut;
        cut = fb;
        check(mdx_read(cut.b, cut.n / 2, &x, &mw) == W_ERR_CORRUPT && mw.text[0], "cut in half: corrupt, with where it stopped");
        cut.b[0] = 'X';
        check(mdx_read(cut.b, cut.n, &x, &mw) == W_ERR_CORRUPT, "no MDLX mark: corrupt");
        cut = fb;
        unsigned char *kg = find_tag(&cut, "KGAO");
        if (kg) memcpy(kg, "KZZZ", 4);
        check(kg && mdx_read(cut.b, cut.n, &x, &mw) == W_ERR_CORRUPT && strstr(mw.text, "KZZZ"), "a track tag no record has: corrupt, naming it");
        cut = fb;
        unsigned char *nr = find_tag(&cut, "NRMS");
        if (nr) memcpy(nr, "NRMX", 4);
        check(nr && mdx_read(cut.b, cut.n, &x, &mw) == W_ERR_CORRUPT && strstr(mw.text, "NRMS"), "a geoset whose normals are misnamed: corrupt, saying what was expected");
    }
    /* }}} */

    /* {{{ WC3's models posed (402 step 4): the model written here, at and
     * between keys and past a sequence's end, by every way. Needs none of
     * the game's files. Held to world-edit-to-execute's Lua (anim.lua)
     * by tests/070-mdx-anim-matches-lua.lua. */
    printf("WC3's models posed\n");
    {
        static Bytes pb;
        posed_mdx(&pb);
        MdxModel x;
        MdxRig rig;
        WWhy mw;
        int read = mdx_read(pb.b, pb.n, &x, &mw) == W_OK;
        check(read, read ? "a WC3 model with a skeleton written here reads" : mw.text);
        int built = read && mdx_rig_build(&x, &rig, &mw) == W_OK;
        if (read) check(built, built ? "its rig builds" : mw.text);
        if (built) {
            static const int32_t want[7] = { 0, 1, 2, 6, 3, 4, 5 };
            int order = rig.nnodes == 7;
            for (uint32_t k = 0; order && k < 7; ++k) order = rig.nodes[k].id == want[k];
            check(order && rig.nodes[0].parent == -1 && rig.nodes[4].parent == 2 && rig.nodes[5].parent == 4,
                  "its nodes posed parents first: by depth, then by id (0, 1, 2, 6, 3, 4, 5)");
            check(rig.ngroups == 3 && rig.point_group[0] == rig.point_group[3] && rig.point_group[0] != 0
                  && rig.point_group[1] == rig.point_group[2] && rig.groups[rig.point_group[1]].count == 2,
                  "its groups: the empty one, {root} (the first geoset names it twice: kept once) and {lower, hand}");
            check(rig.geoset_groups[1].count == 0 && rig.point_group[4] == 0 && rig.point_group[6] == 0 && rig.on_clock == 1,
                  "a geoset whose point names a group it hasn't stands as made (group 0); the helper follows a global sequence");
            double w[7][12], w2[7][12], p[3];
            float alpha[2], poses[3][16];
            WErr e = mdx_rig_pose(&rig, 0, 500, 0, w, alpha, &mw);
            check(e == W_OK, e == W_OK ? "Stand poses at 500" : mw.text);
            apply34(w[0], 0, 0, 0, p);
            check(near3(p, 5, 0, 0), "the root halfway along its straight line: (5, 0, 0)");
            apply34(w[1], 20, 0, 0, p);
            check(near3(p, 15, 10, 0), "upper's way 0 turn at its key (a quarter about its pivot): (20, 0, 0) goes to (15, 10, 0)");
            apply34(w[2], 30, 0, 0, p);
            check(near3(p, 15, 30, 0), "lower's hermite stretch halfway: 2, so (30, 0, 0) goes to (15, 30, 0)");
            apply34(w[4], 30, 0, 0, p);
            check(near3(p, 15, 30, 6), "the hand's bezier lift halfway: 3 though both keys are 0, under three parents: (15, 30, 6)");
            check(!memcmp(w[3], w[1], sizeof w[1]), "the billboarded bone turns with its parent (as anim.lua; WC3's facing the camera is not built)");
            check(mdx_rig_attachment(&rig, (const double (*)[12])w, 5, p) == 1 && near3(p, 15, 30, 16) && mdx_rig_attachment(&rig, (const double (*)[12])w, 4, p) == 0,
                  "the attachment on the hand at (15, 30, 16); a helper is no attachment");
            check(alpha[0] == 0.5f && alpha[1] == 0.5f, "both units' alpha: the layer's 0.5, the geoset animation's 1 before 600");
            mdx_rig_groups(&rig, (const double (*)[12])w, poses);
            float in[3] = { 30, 0, 0 }, nin[3] = { 1, 0, 0 }, out[3], nout[3];
            mdx_rig_skin_point(&rig, (const float (*)[16])poses, 1, in, out, nin, nout);
            check(near(out[0], 15, 1e-4) && near(out[1], 30, 1e-4) && near(out[2], 3, 1e-4) && near(nout[1], 1, 1e-6),
                  "a point of {lower, hand} follows their average: (30, 0, 0) to (15, 30, 3), facing turned a quarter");
            mdx_rig_skin_point(&rig, (const float (*)[16])poses, 6, in, out, NULL, NULL);
            check(out[0] == 30 && out[1] == 0 && poses[0][0] == 1 && poses[0][12] == 0, "a point of group 0 stays where it was made");
            mdx_rig_pose(&rig, 0, 250, 0, w, alpha, &mw);
            apply34(w[2], 30, 0, 0, p);
            snprintf(line, sizeof line, "a quarter of the way, the hermite stretch is 1.3125, not a straight line's 1.5 (%.4f)", (p[0] - 2.5 - 20) / 10);
            check(near(p[0], 35.625, 1e-9) && near(p[1], 0, 1e-9), line);
            apply34(w[1], 20, 0, 0, p);
            check(near3(p, 22.5, 0, 0), "upper's way 0 turn holds the earlier key until the next: none at 250");
            mdx_rig_pose(&rig, 0, 1500, 0, w, alpha, &mw);
            apply34(w[0], 0, 0, 0, p);
            check(near3(p, 10, 0, 0) && alpha[0] == 0, "past Stand's end the last keys hold: the root at (10, 0, 0), the first geoset faded out");
            mdx_rig_pose(&rig, 0, -100, 0, w, alpha, &mw);
            apply34(w[0], 0, 0, 0, p);
            check(near3(p, 0, 0, 0), "before Stand's start the first keys hold");
            mdx_rig_pose(&rig, 1, 2500, 0, w, alpha, &mw);
            apply34(w[4], 30, 0, 0, p);
            check(near3(p, 30, 0, 0) && alpha[0] == 0.5f, "Walk, with no keys inside it, stands at rest (its geoset shown: the fade is only Stand's)");
            mdx_rig_pose(&rig, 0, 500, 0, w, NULL, &mw);
            check(!memcmp(w[5], w[4], sizeof w[4]), "the global helper at clock 0: no turn, the hand's matrix");
            mdx_rig_pose(&rig, 0, 500, 100, w, NULL, &mw);
            mdx_rig_pose(&rig, 0, 500, 500, w2, NULL, &mw);
            apply34(w[5], 1, 0, 0, p);
            double h[3];
            apply34(w[4], 0, 1, 0, h);
            check(near3(p, h[0], h[1], h[2]) && !memcmp(w[5], w2[5], sizeof w[5]),
                  "the global helper at clock 100: a quarter turn (halfway to half), and the same at 500 (400 later, its loop)");
            e = mdx_rig_pose(&rig, 2, 0, 0, w, alpha, &mw);
            check(e == W_ERR_NOT_FOUND && strstr(mw.text, "no sequence 2"), "a sequence the model hasn't: not found, saying which");
            mdx_rig_free(&rig);
        }
        if (read) mdx_free(&x);

        /* parents round in a loop, and a chain seventy deep: C's own rules */
        static const int32_t loop[3] = { 1, 0, 2 };
        bones_mdx(&pb, 3, loop);
        read = mdx_read(pb.b, pb.n, &x, &mw) == W_OK && mdx_rig_build(&x, &rig, &mw) == W_OK;
        check(read && rig.nnodes == 3 && rig.nodes[0].id == 1 && rig.nodes[0].parent == -1 && rig.nodes[1].id == 2 && rig.nodes[1].parent == -1
              && rig.nodes[2].id == 0 && rig.nodes[2].parent == 0,
              "parents round in a loop (0 under 1 under 0) are cut once, where the walk up from 0 comes back: 1 a root, 0 under it; a bone its own parent a root");
        if (read) { mdx_rig_free(&rig); mdx_free(&x); }
        int32_t deep[70];                                  /* the root last (id 69), so the walk up from id 0 climbs all seventy */
        for (int k = 0; k < 70; ++k) deep[k] = k == 69 ? -1 : k + 1;
        bones_mdx(&pb, 70, deep);
        read = mdx_read(pb.b, pb.n, &x, &mw) == W_OK && mdx_rig_build(&x, &rig, &mw) == W_OK;
        int chained = read && rig.nnodes == 70;
        for (uint32_t k = 0; chained && k < 70; ++k) chained = rig.nodes[k].id == 69 - (int32_t)k && rig.nodes[k].parent == (int32_t)k - 1;
        if (chained) {
            static double dw[70][12];
            mdx_rig_pose(&rig, 0, 1000, 0, dw, NULL, &mw);
            double q[3];
            apply34(dw[69], 0, 0, 0, q);
            chained = near3(q, 70, 0, 0);
        }
        check(chained, "a chain seventy deep keeps every parent (anim.lua's guard stops at 64): its end moved 70 by seventy moves of 1");
        if (read) { mdx_rig_free(&rig); mdx_free(&x); }
    }
    /* }}} */

    /* {{{ the archives */
    printf("the archives\n");
    check(mpq_hash("(hash table)", 3) == 0xC3AF3770u, "the index's key is C3AF3770");
    check(mpq_hash("(block table)", 3) == 0xEC83B3A3u, "the block list's key is EC83B3A3");
    check(mpq_hash("a/b", 0) == mpq_hash("A\\B", 0), "names match without regard to case or slash");
    MpqChain chain;
    if (mpq_chain_open(&chain, client, "enUS", &why)) { printf("  FAIL: %s\n", why.text); return 1; }
    snprintf(line, sizeof line, "all 13 of the client's archives open (%d)", chain.count);
    check(chain.count == 13, line);
    const char *base = "Character\\NightElf\\Female\\NightElfFemale";
    size_t m2_len, skin_len;
    const char *from = NULL;
    snprintf(path, sizeof path, "%s.m2", base);
    unsigned char *m2 = mpq_chain_read(&chain, path, &m2_len, &from);
    check(m2 && memcmp(m2, "MD20", 4) == 0, "her model is found and begins as a model does");
    check(from && strstr(from, "patch-3.MPQ") != NULL, "and it is the newest patch's copy (patch-3)");
    size_t nolen;
    check(mpq_chain_read(&chain, "No\\Such\\File.m2", &nolen, NULL) == NULL, "a file no archive holds is none, not an error");
    /* the chain's memory (173t): the same file again is served from it,
     * byte for byte, under any case or slash, and reports where it came from */
    {
        uint64_t hits = chain.hits;
        size_t again_len;
        const char *again_from = NULL;
        unsigned char *again = mpq_chain_read(&chain, "character/nightelf/female/NIGHTELFFEMALE.M2", &again_len, &again_from);
        check(again && chain.hits == hits + 1 && again_len == m2_len && !memcmp(again, m2, m2_len) && again_from && strstr(again_from, "patch-3.MPQ"),
              "read again (in other case and slashes), her model comes from the chain's memory, the same bytes and archive");
        free(again);
    }
    /* }}} */

    /* {{{ her model and pictures */
    printf("her model\n");
    snprintf(path, sizeof path, "%s00.skin", base);
    unsigned char *skin = mpq_chain_read(&chain, path, &skin_len, NULL);
    M2Model model;
    check(m2_read(m2, m2_len, skin, skin_len, &model, &why) == 0, "her model and mesh read");
    float zmax = -1e9f;
    for (uint32_t i = 0; i < model.nvertices; ++i) if (model.vertices[i].z > zmax) zmax = model.vertices[i].z;
    snprintf(line, sizeof line, "she stands %.2f yards tall (Lua: 2.29)", zmax);
    check(near(zmax, 2.29, 0.02), line);
    snprintf(line, sizeof line, "%u triangles (Lua: 5148)", model.nindices / 3);
    check(model.nindices / 3 == 5148, line);
    int in_range = 1;
    for (uint32_t i = 0; i < model.nindices; ++i) if (model.indices[i] >= model.nvertices) in_range = 0;
    check(in_range, "every triangle's corners are real points of the model");
    check(model.textures[0].kind == 1 && model.textures[1].kind == 6, "her first picture is her skin, her second her hair");
    m2_free(&model);
    free(m2); free(skin);

    printf("pictures\n");
    size_t len;
    snprintf(path, sizeof path, "%sSkin00_00.blp", base);
    unsigned char *b = mpq_chain_read(&chain, path, &len, NULL);
    BlpImage img;
    check(b && blp_decode(b, len, &img, &why) == 0 && img.width == 512 && img.height == 512, "her skin sheet decodes to 512 x 512");
    free(b);
    b = mpq_chain_read(&chain, "Character\\NightElf\\Hair00_00.blp", &len, NULL);
    BlpImage hair;
    int cut = 0;
    if (b && blp_decode(b, len, &hair, &why) == 0)
        for (size_t i = 3; i < (size_t)hair.width * hair.height * 4; i += 4) if (hair.rgba[i] < 128) { cut = 1; break; }
    check(cut, "her hair picture has see-through strands (alpha)");
    free(b); blp_free(&hair);
    /* an item's icon (410): the Stormwind tabard's look names a 64 x 64
     * icon the archives hold; a look the table lacks is refused, named */
    {
        char icon[300] = "";
        BlpImage ic = { 0 };
        int named = character_item_icon(&chain, 59981, icon, sizeof icon, &why) == W_OK;
        unsigned char *ib = named ? mpq_chain_read(&chain, icon, &len, NULL) : NULL;
        int decoded = ib && blp_decode(ib, len, &ic, &why) == 0;
        snprintf(line, sizeof line, "the Stormwind tabard's icon (%s) is in the archives and is 64 x 64 (%d x %d)", icon, ic.width, ic.height);
        check(decoded && ic.width == 64 && ic.height == 64, line);
        free(ib);
        if (decoded) blp_free(&ic);
        check(character_item_icon(&chain, 999999999, icon, sizeof icon, &why) == W_ERR_NOT_FOUND, "an item look not in the table has no icon, and says so");
        /* a worn item whose pictures the archives lack is counted, so the
         * dressing room can refuse it: look 3864 (the "Unused Tabard of
         * Chow") names two torso pictures no archive holds */
        CharacterLook lk = { 4, 1, 0, 0, 0, 0, { 0 }, 0, 0 };
        Character worn;
        lk.items[SLOT_TABARD] = 59981;
        int whole = character_build(&chain, &lk, &worn, &why) == W_OK ? worn.items_unpainted : -1;
        if (whole >= 0) character_free(&worn);
        lk.items[SLOT_TABARD] = 3864;
        int lacking = character_build(&chain, &lk, &worn, &why) == W_OK ? worn.items_unpainted : -1;
        if (lacking >= 0) character_free(&worn);
        snprintf(line, sizeof line, "the Stormwind tabard is worn whole (%d missing); the Tabard of Chow lacks its 2 pictures (%d)", whole, lacking);
        check(whole == 0 && lacking == 2, line);
    }
    /* }}} */

    /* {{{ WC3's pictures (BLP1, issue 107): one built here, then WC3's own */
    printf("WC3's pictures (BLP1)\n");
    {
        /* a 2 x 2 palette picture with 8-bit alpha and a 1 x 1 smaller copy:
         * colour numbers 0 1 2 3, the palette's first four colours (BGRA)
         * blue, green, red, white; alphas 255 128 64 0 */
        unsigned char f[156 + 1024 + 8 + 2] = { 0 };
        size_t flen = sizeof f;
        memcpy(f, "BLP1", 4);
        f[4] = 1;                        /* palette */
        f[8] = 8;                        /* alpha bits */
        f[12] = 2; f[16] = 2;            /* width, height */
        f[24] = 1;                       /* has smaller copies */
        f[28] = (156 + 1024) & 255; f[29] = (156 + 1024) >> 8;   /* level 0 at 1180 */
        f[92] = 8;                                               /* 4 numbers + 4 alphas */
        f[32] = (156 + 1024 + 8) & 255; f[33] = (156 + 1024 + 8) >> 8;   /* level 1 at 1188 */
        f[96] = 2;
        static const unsigned char pal[16] = { 255, 0, 0, 0,  0, 255, 0, 0,  0, 0, 255, 0,  255, 255, 255, 0 };
        memcpy(f + 156, pal, 16);
        static const unsigned char px[8] = { 0, 1, 2, 3, 255, 128, 64, 0 };
        memcpy(f + 1180, px, 8);
        f[1188] = 2; f[1189] = 77;       /* level 1: colour 2 (red), alpha 77 */
        BlpImage p1;
        static const unsigned char want[16] = { 0, 0, 255, 255,  0, 255, 0, 128,  255, 0, 0, 64,  255, 255, 255, 0 };
        check(blp_decode(f, flen, &p1, &why) == W_OK && p1.width == 2 && p1.height == 2 && !memcmp(p1.rgba, want, 16),
              "a palette BLP1 made here: blue, green, red, white, with their alphas 255, 128, 64, 0");
        blp_free(&p1);
        check(blp_decode_level(f, flen, 1, &p1, &why) == W_OK && p1.width == 1 && p1.height == 1
              && p1.rgba[0] == 255 && p1.rgba[1] == 0 && p1.rgba[2] == 0 && p1.rgba[3] == 77,
              "its smaller copy (level 1) is one red pixel with alpha 77");
        blp_free(&p1);
        check(blp_decode_level(f, flen, 2, &p1, &why) == W_ERR_NOT_FOUND, "it has no level 2: not found, with a sentence");
        check(blp_decode(f, 1183, &p1, &why) == W_ERR_CORRUPT, "cut short inside its pixels: corrupt, not read past the end");
        check(blp_decode((const unsigned char *)"BLP9", 4, &p1, &why) == W_ERR_CORRUPT, "neither BLP1 nor BLP2: corrupt");
        static const unsigned char not_jpeg[4] = { 0xFF, 0xD8, 0xFF, 0x00 };
        JpegImage ji;
        check(jpeg_decode(not_jpeg, 4, &ji, &why) == W_ERR_CORRUPT, "a JPEG that stops after its first mark: corrupt");

        /* WC3's own: the install world-edit-to-execute reads (its wc3-installs/) */
        const char *war3 = argc > 3 ? argv[3] : "/mnt/mtwo/programming/ai-stuff/world-edit-to-execute/wc3-installs/frozen-throne/war3.mpq";
        MpqArchive wa = { 0 };
        check(mpq_open(&wa, war3, &why) == W_OK, "WC3's war3.mpq opens with this client's own archive reader");
        size_t flen2 = 0;
        unsigned char *fb = mpq_read(&wa, "Textures\\Footman.blp", &flen2, &why);
        check(fb && !memcmp(fb, "BLP1", 4) && fb[4] == 0 && fb[5] == 0 && fb[6] == 0 && fb[7] == 0, "the footman's picture is there, a JPEG BLP1");
        BlpImage foot;
        if (fb && blp_decode(fb, flen2, &foot, &why) == W_OK) {
            snprintf(line, sizeof line, "it decodes to %d x %d (Lua: 256 x 256)", foot.width, foot.height);
            check(foot.width == 256 && foot.height == 256, line);
            uint32_t sum = 0;
            for (size_t i = 0; i < (size_t)foot.width * foot.height * 4; ++i) sum = sum * 31 + foot.rgba[i];
            snprintf(line, sizeof line, "its bytes' running sum is %08X (Lua: %08X)", sum, FOOTMAN_SUM);
            check(sum == FOOTMAN_SUM, line);
            blp_free(&foot);
        } else check(0, why.text);
        free(fb);
        if (wa.file) mpq_close(&wa);
    }
    /* }}} */

    /* {{{ the character pictures table */
    printf("the character pictures table\n");
    b = mpq_chain_read(&chain, "DBFilesClient\\CharSections.dbc", &len, NULL);
    Dbc d;
    check(dbc_open(&d, b, len, &why) == 0 && d.rows > 1000, "the character pictures table reads (over a thousand rows)");
    free(b);
    /* }}} */

    /* {{{ putting her together */
    printf("putting her together\n");
    Character ch;
    CharacterLook look = { 4, 1, 0, 0, 0, 0, { 0 }, 0, 0 };
    int built = character_build(&chain, &look, &ch, &why) == 0;
    check(built, built ? "she is put together" : why.text);
    if (built) {
        int body = 0, hair2 = 0, others = 0, clothes = 0, bare = 0;
        for (uint32_t i = 0; i < ch.model.npieces; ++i) {
            if (!ch.shown[i]) continue;
            uint32_t id = ch.model.pieces[i].id;
            if (id == 0) body = 1;
            if (id == 2) hair2 = 1;
            if (id >= 3 && id <= 13) others = 1;
            if (id == 1002 || id == 1102 || id == 1202 || id == 1302 || id == 1502 || id == 802) clothes = 1;
            if (id == 401 || id == 501 || id == 702) ++bare;
        }
        check(body && hair2, "her body (0) and hair style 0's piece (2) show");
        check(!others, "no other hair style shows");
        check(!clothes, "no clothing shapes show");
        check(bare == 3, "bare hands, bare feet, and her ears show");
        int changed = 0;
        for (int y = 384; y <= 511; y += 8)
            for (int x = 8; x <= 255; x += 8) {
                size_t o = ((size_t)y * 512 + x) * 4;
                if (ch.skin.rgba[o] != img.rgba[o]) ++changed;
            }
        snprintf(line, sizeof line, "her face is painted onto her skin sheet (%d of 512 samples changed; Lua: 393)", changed);
        check(changed == 393, line);
        check(ch.hair.width > 0, "her hair has its picture");
        /* her eyes' glow (173o): piece 1702 shows (1703, a death knight's,
         * doesn't), drawn as added light with a picture named in the model
         * that the archives hold, and tinted visibly while she stands */
        {
            int glow = 0, dk = 0, unit = -1;
            for (uint32_t i = 0; i < ch.model.npieces; ++i) {
                if (ch.shown[i] && ch.model.pieces[i].id == 1702) glow = 1;
                if (ch.shown[i] && ch.model.pieces[i].id == 1703) dk = 1;
            }
            for (uint32_t u = 0; u < ch.model.nunits; ++u)
                if (ch.model.pieces[ch.model.units[u].piece].id == 1702) unit = (int)u;
            check(glow && !dk, "her race's eye glow (1702) shows, a death knight's (1703) doesn't");
            if (unit >= 0) {
                M2Unit un = ch.model.units[unit];
                size_t gl;
                const char *gfrom;
                unsigned char *gpic = mpq_chain_read(&chain, ch.model.textures[un.texture].file, &gl, &gfrom);
                snprintf(line, sizeof line, "the glow is added light (blend %u) with its own picture (%.100s)", un.blend, ch.model.textures[un.texture].file);
                check(un.blend >= 3 && ch.model.textures[un.texture].kind == 0 && gpic, line);
                free(gpic);
                size_t ml;
                unsigned char *gm2 = mpq_chain_read(&chain, "Character\\NightElf\\Female\\NightElfFemale.m2", &ml, &gfrom);
                Anim ga;
                if (gm2 && !anim_read(gm2, ml, &ga, &why)) {
                    float tint[4];
                    anim_unit_tint(&ga, un.colour, un.fade, anim_find(&ga, 0, 0), 0, 0, tint);
                    snprintf(line, sizeof line, "standing, the glow's tint is visible (%.2f %.2f %.2f, opacity %.2f)", tint[0], tint[1], tint[2], tint[3]);
                    check(tint[3] > 0.1f && tint[0] + tint[1] + tint[2] > 0.1f, line);
                    anim_free(&ga);
                } else check(0, "her model's skeleton could not be read for the glow's tint");
                free(gm2);
            } else check(0, "her model has no unit drawing piece 1702");
        }
        character_free(&ch);
    }
    {
        /* dressed (173p): the rogue's set paints her torso and switches on
         * her boots (feet group 5: 501 + 3 for these boots) */
        CharacterLook dressed = { 4, 1, 0, 0, 0, 0, { 0 }, 0, 0 }, bare = { 4, 1, 0, 0, 0, 0, { 0 }, 0, 0 };
        dressed.items[SLOT_CHEST] = 41143; dressed.items[SLOT_FEET] = 41480; dressed.items[SLOT_HANDS] = 41138;
        Character dc, bc;
        if (character_build(&chain, &dressed, &dc, &why) || character_build(&chain, &bare, &bc, &why)) check(0, why.text);
        else {
            int changed = 0, boots = 0, bare_feet = 0;
            int k = dc.skin.width / 256;
            for (int y = 0; y < 64 * k; y += 4) for (int x = 128 * k; x < 256 * k; x += 4) {
                size_t o = ((size_t)y * dc.skin.width + x) * 4;
                if (dc.skin.rgba[o] != bc.skin.rgba[o] || dc.skin.rgba[o + 1] != bc.skin.rgba[o + 1]) ++changed;
            }
            for (uint32_t i = 0; i < dc.model.npieces; ++i) {
                if (dc.model.pieces[i].id == 504 && dc.shown[i]) boots = 1;
                if (dc.model.pieces[i].id == 501 && dc.shown[i]) bare_feet = 1;
            }
            snprintf(line, sizeof line, "dressed in the rogue's set, her upper torso is painted (%d of %d samples changed), her boots show and her bare feet don't", changed, 16 * k * 32 * k);
            check(changed > 100 && boots && !bare_feet, line);
            character_free(&dc);
            character_free(&bc);
        }
        /* hung items: the rogue's hood hides her hair; shoulders and a sword */
        dressed.items[SLOT_HEAD] = 41139; dressed.items[SLOT_SHOULDERS] = 41142; dressed.items[SLOT_MAIN_HAND] = 40857;
        if (character_build(&chain, &dressed, &dc, &why)) check(0, why.text);
        else {
            int points = 0, hair = 0;
            for (int i = 0; i < dc.nattached; ++i) points |= 1 << dc.attached[i].point;
            for (uint32_t i = 0; i < dc.model.npieces; ++i) if (dc.model.pieces[i].id > 0 && dc.model.pieces[i].id < 100 && dc.shown[i]) hair = 1;
            snprintf(line, sizeof line, "the hood, both shoulders and the sword hang from points 11, 6, 5 and 1 (%d items: %.80s), and the hood hides her hair", dc.nattached, dc.nattached ? dc.attached[0].model : "-");
            check(dc.nattached == 4 && points == ((1 << 11) | (1 << 6) | (1 << 5) | (1 << 1)) && !hair, line);
            size_t ml;
            const char *from;
            unsigned char *hm = mpq_chain_read(&chain, dc.attached[0].model, &ml, &from);
            check(hm != NULL, "the hood's model, made for a night elf woman (_NiF), is in the archives");
            free(hm);
            character_free(&dc);
        }
    }
    {
        /* a tauren man: his tail (piece 1501) uses the fur picture (kind 8),
         * the skin row's second; without it the tail was skipped (173d) */
        CharacterLook tauren = { 6, 0, 0, 0, 0, 0, { 0 }, 0, 0 };
        Character tc;
        if (character_build(&chain, &tauren, &tc, &why)) check(0, why.text);
        else {
            int tail = 0, tail_fur = 0;
            for (uint32_t i = 0; i < tc.model.npieces; ++i) if (tc.model.pieces[i].id == 1501 && tc.shown[i]) tail = 1;
            for (uint32_t u = 0; u < tc.model.nunits; ++u) {
                M2Unit un = tc.model.units[u];
                if (un.piece < tc.model.npieces && tc.model.pieces[un.piece].id == 1501 && un.texture < tc.model.ntextures && tc.model.textures[un.texture].kind == 8) tail_fur = 1;
            }
            snprintf(line, sizeof line, "a tauren man's tail shows, painted with his fur (%d x %d)", tc.fur.width, tc.fur.height);
            check(tail && tail_fur && tc.fur.width > 0, line);
            character_free(&tc);
        }
        /* the game's own tables pick a bare character's pieces (173d):
         * CharacterFacialHairStyles gives his beard's three pieces in the
         * order groups 1, 3, 2; choice 0 is clean-shaven (no 1xx-3xx), choice
         * 1 shows 102, 302 and 202; his race has no eye glow */
        int shaven_any = 1, beard = 0, glow = 0;
        if (character_build(&chain, &tauren, &tc, &why)) check(0, why.text);
        else {
            shaven_any = 0;
            for (uint32_t i = 0; i < tc.model.npieces; ++i) {
                uint32_t id = tc.model.pieces[i].id;
                if (tc.shown[i] && id >= 100 && id < 400) shaven_any = 1;
                if (tc.shown[i] && id / 100 == 17) glow = 1;
            }
            character_free(&tc);
        }
        tauren.facial_hair = 1;
        if (character_build(&chain, &tauren, &tc, &why)) check(0, why.text);
        else {
            for (uint32_t i = 0; i < tc.model.npieces; ++i) {
                uint32_t id = tc.model.pieces[i].id;
                if (tc.shown[i]) beard |= (id == 102) | (id == 302) << 1 | (id == 202) << 2;   /* by id: some ids are two meshes (fur and skin) */
            }
            character_free(&tc);
            beard = (beard & 1) + (beard >> 1 & 1) + (beard >> 2 & 1);
        }
        snprintf(line, sizeof line, "a tauren man's facial-hair choice 0 shows no beard pieces (%s), choice 1 shows 102, 302 and 202 (%d of 3), and he has no eye glow (%s)",
                 shaven_any ? "some" : "none", beard, glow ? "one" : "none");
        check(!shaven_any && beard == 3 && !glow, line);
        tauren.facial_hair = 99;
        int refused = character_build(&chain, &tauren, &tc, &why) != 0;
        snprintf(line, sizeof line, "a facial-hair choice the table lacks is refused, naming it (%.80s)", refused ? why.text : "built");
        check(refused && strstr(why.text, "facial-hair choice 99"), line);
        if (!refused) character_free(&tc);
        /* starting outfits (173p): a fresh night-elf hunter woman wears the
         * table's shirt, trousers, boots and axe; her bow, quiver and arrows
         * have no slot here; a tauren warrior man his shirt, trousers, axe */
        {
            int hunter[SLOT_COUNT] = { 0 }, warrior[SLOT_COUNT] = { 0 };
            int ok1 = !character_start_outfit(&chain, 4, 3, 1, hunter, &why);
            int ok2 = !character_start_outfit(&chain, 6, 1, 0, warrior, &why);
            int filled = 0;
            for (int k = 0; k < SLOT_COUNT; ++k) filled += hunter[k] != 0;
            snprintf(line, sizeof line, "a fresh night-elf hunter wears shirt %d, trousers %d, boots %d, axe %d and nothing else (%d slots)",
                     hunter[SLOT_SHIRT], hunter[SLOT_LEGS], hunter[SLOT_FEET], hunter[SLOT_MAIN_HAND], filled);
            check(ok1 && hunter[SLOT_SHIRT] == 9976 && hunter[SLOT_LEGS] == 9975 && hunter[SLOT_FEET] == 9977 && hunter[SLOT_MAIN_HAND] == 22291 && filled == 4, line);
            check(ok2 && warrior[SLOT_SHIRT] == 9995 && warrior[SLOT_LEGS] == 9988 && warrior[SLOT_MAIN_HAND] == 8690, "a fresh tauren warrior wears his shirt, trousers and axe");
            check(character_start_outfit(&chain, 4, 2, 1, hunter, &why) != 0 && strstr(why.text, "class 2"), "a race and class with no outfit (a night-elf paladin) is refused, named");
        }
        /* a creature's look (173t) may use a choice players can't: an
         * Astranaar sentinel's facial-hair 10 (a night-elf woman's rows stop
         * at 9) takes the race's choice-0 pieces, keeping her eye glow;
         * a player's look with it is refused */
        CharacterLook sentinel = { 4, 1, 0, 0, 0, 0, { 0 }, 10, 1 };
        int glow10 = 0;
        if (character_build(&chain, &sentinel, &tc, &why)) check(0, why.text);
        else {
            for (uint32_t i = 0; i < tc.model.npieces; ++i) if (tc.shown[i] && tc.model.pieces[i].id == 1702) glow10 = 1;
            character_free(&tc);
        }
        sentinel.npc = 0;
        int player_refused = character_build(&chain, &sentinel, &tc, &why) != 0;
        if (!player_refused) character_free(&tc);
        check(glow10 && player_refused, "a creature's facial-hair choice beyond the table builds with the race's plain pieces (her eye glow kept); a player's is refused");
        /* the sentinel's look: display 4841 names a CreatureDisplayInfoExtra
         * row (a night-elf woman) whose baked skin sheet is in the archives */
        {
            size_t dl, el, bl;
            unsigned char *ds = mpq_chain_read(&chain, "DBFilesClient\\CreatureDisplayInfo.dbc", &dl, NULL);
            unsigned char *es = mpq_chain_read(&chain, "DBFilesClient\\CreatureDisplayInfoExtra.dbc", &el, NULL);
            Dbc dd, ed;
            uint32_t extra = 0;
            int race = 0, sex = -1, baked = 0;
            if (ds && es && !dbc_open(&dd, ds, dl, &why) && !dbc_open(&ed, es, el, &why)) {
                for (uint32_t r = 0; r < dd.rows; ++r) if (dbc_u32(&dd, r, 1) == 4841) extra = dbc_u32(&dd, r, 4);
                for (uint32_t r = 0; extra && r < ed.rows; ++r)
                    if (dbc_u32(&ed, r, 1) == extra) {
                        race = (int)dbc_u32(&ed, r, 2); sex = (int)dbc_u32(&ed, r, 3);
                        char bp[300];
                        snprintf(bp, sizeof bp, "Textures\\BakedNpcTextures\\%.200s", dbc_text(&ed, r, 21));
                        unsigned char *b = mpq_chain_read(&chain, bp, &bl, NULL);
                        baked = b != NULL;
                        free(b);
                    }
            }
            free(ds); free(es);
            snprintf(line, sizeof line, "a sentinel's display (4841) is a dressed character: extra row %u, race %d, sex %d, its baked skin %s", extra, race, sex, baked ? "found" : "missing");
            check(extra && race == 4 && sex == 1 && baked, line);
        }
    }
    blp_free(&img);
    mpq_chain_close(&chain);
    /* }}} */

    /* {{{ what the camera sees (173u): a camera at the origin looking along
     * +x (up +z), 60 degrees, 0.3 to 1000 yards */
    printf("sight\n");
    {
        Matrix view = MatrixLookAt((Vector3){ 0, 0, 0 }, (Vector3){ 1, 0, 0 }, (Vector3){ 0, 0, 1 });
        Matrix proj = MatrixPerspective(60 * DEG2RAD, 16.0 / 9.0, 0.3, 1000);
        Sight sg;
        sight_from(&sg, MatrixMultiply(view, proj));
        check(sight_ball(&sg, (Vector3){ 50, 0, 0 }, 1), "a ball 50 yards ahead is seen");
        check(!sight_ball(&sg, (Vector3){ -50, 0, 0 }, 1), "one 50 yards behind is not");
        check(!sight_ball(&sg, (Vector3){ 1500, 0, 0 }, 1), "nor one past the far wall");
        check(!sight_ball(&sg, (Vector3){ 10, 40, 0 }, 1) && sight_ball(&sg, (Vector3){ 10, 40, 0 }, 40), "one far off to the side is not, unless it is big enough to reach in");
    }
    /* }}} */

    /* {{{ reading ahead (173u): the helper thread unpacks a tile's files;
     * afterwards the tile's ground file and a doodad it names come from
     * the archive reader's memory */
    printf("reading ahead\n");
    {
        MpqChain rc;
        if (mpq_chain_open(&rc, client, "enUS", &why)) check(0, why.text);
        else {
            Prefetch *reader = prefetch_start(&rc, "Kalimdor", 1);
            prefetch_tile(reader, 26, 32);
            prefetch_tile(reader, 26, 32);                        /* asked twice: read once */
            prefetch_wait(reader);
            int pt, pf, gm, gt;
            prefetch_counts(reader, &pt, &pf, &gm, &gt);
            uint64_t hits = rc.hits;
            size_t al;
            unsigned char *adt_s = mpq_chain_read(&rc, "World\\Maps\\Kalimdor\\Kalimdor_32_26.adt", &al, NULL);
            snprintf(line, sizeof line, "the helper read 1 tile (%d) and its files (%d); its ground file comes from memory afterwards", pt, pf);
            check(pt == 1 && pf > 100 && adt_s && rc.hits == hits + 1, line);
            check(gm == 0 && prefetch_take_ground(reader, 26, 32, &(GroundPrep *){ 0 }) == 2, "without being told to, it prepares no ground");
            free(adt_s);

            /* preparing the ground (173u): the helper's preparation of a
             * tile is the very one the main thread would make, and the
             * pictures said to be on the card already aren't unpacked */
            {
                /* a 4 x 4 picture's smaller copies: 2 x 2, then 1 x 1, each
                 * pixel the rounded average of the four beneath it */
                unsigned char four[64];
                for (int i = 0; i < 64; ++i) four[i] = (unsigned char)(i * 4);
                GroundPixels px = { four, 4, 4, NULL };
                ground_prep_smaller(&px);
                /* top-left 2 x 2 of channel 0: pixels 0, 1, 4, 5 -> 0, 16, 64, 80: 40 */
                int top_left = px.smaller ? px.smaller[0] : -1, last = px.smaller ? px.smaller[16] : -1;
                snprintf(line, sizeof line, "a picture's smaller copies average the pixels beneath (%d, then %d)", top_left, last);
                check(top_left == 40 && last == (40 + 72 + 168 + 200 + 2) / 4, line);   /* the 2 x 2: 40, 72, 168, 200 */
                free(px.smaller);
                GroundPixels odd = { four, 4, 2, NULL };
                ground_prep_smaller(&odd);
                check(!odd.smaller, "a picture that isn't square gets none");
            }
            char maps_dir[700];
            snprintf(maps_dir, sizeof maps_dir, "%s/assets/server-data/maps", dir);
            MapWorld gw;
            maps_world_init(&gw, maps_dir, 1);
            GroundPlace gp;
            if (ground_place_init(&gp, &rc, "Kalimdor", &gw, 2700, -400, &why)) check(0, why.text);
            else {
                prefetch_prepare_ground(reader, &gp, test_on_card, NULL);
                prefetch_tile(reader, 26, 31);
                prefetch_wait(reader);
                GroundPrep *ahead = NULL, *here = NULL;
                int took = prefetch_take_ground(reader, 26, 31, &ahead);
                int made = ground_prep_tile(&gp, 26, 31, test_on_card, NULL, &here, &why);
                check(took == 0 && made == 0, "the helper prepared the tile's ground, and the main thread can too");
                if (took == 0 && made == 0) {
                    int same = 1, quarters = 0, unpacked = 0, skipped = 0;
                    for (int q = 0; q < 4; ++q) {
                        GroundQuarter *a = &ahead->quarter[q], *b = &here->quarter[q];
                        quarters += a->made;
                        same = same && a->nindex == b->nindex && a->made == b->made
                            && !memcmp(a->vertices, b->vertices, sizeof(float) * 3 * 64 * 145)
                            && !memcmp(a->normals, b->normals, sizeof(float) * 3 * 64 * 145)
                            && !memcmp(a->indices, b->indices, sizeof(unsigned short) * (size_t)a->nindex);
                    }
                    same = same && !memcmp(ahead->blend, here->blend, 1024 * 1024 * 4);
                    for (int t = 0; t < ahead->adt->ntextures; ++t) {
                        if (!ahead->needed[t]) continue;
                        if (test_on_card(NULL, ahead->adt->textures[t])) skipped += !ahead->pixels[t].rgba;
                        else unpacked += ahead->pixels[t].rgba && ahead->pixels[t].width > 0;
                    }
                    snprintf(line, sizeof line, "both the same: points, light, triangles, blend picture (%d quarters drawn)", quarters);
                    check(same && quarters == 4, line);
                    snprintf(line, sizeof line, "pictures on the card left packed (%d), the rest unpacked (%d)", skipped, unpacked);
                    check(skipped > 0 && unpacked > 0, line);
                    prefetch_counts(reader, &pt, &pf, &gm, &gt);
                    check(gm == 1 && gt == 1 && prefetch_take_ground(reader, 26, 31, &(GroundPrep *){ 0 }) == 2, "taken once: a second take finds none");
                    ground_prep_free(ahead, 0);
                    ground_prep_free(here, 0);
                }
                prefetch_tile(reader, 30, 31);
                prefetch_wait(reader);
                prefetch_keep_near(reader, 26, 31, 2);                /* (30, 31) is 4 tiles off */
                check(prefetch_take_ground(reader, 30, 31, &(GroundPrep *){ 0 }) == 2, "a preparation she turned away from is let go");
            }
            prefetch_free(reader);
            maps_world_free(&gw);
            mpq_chain_close(&rc);
        }
    }
    /* }}} */

    /* {{{ the world's creatures (173t): a small written list */
    printf("creatures\n");
    {
        char cpath[700];
        snprintf(cpath, sizeof cpath, "%s/tmp/shared-memory/test-creatures.tsv", dir);
        FILE *cf = fopen(cpath, "w");
        if (!cf) check(0, "the test's creature list could not be written to tmp/shared-memory");
        else {
            fputs("# a comment\n"
                  "33406\t6087\t4841\t1\t2735.93\t-416.844\t110.5\t3.1\tAstranaar Sentinel\n"
                  "1\t883\t347\t1.5\t-9000\t300\t40\t0\tDeer far away\n"
                  "33409\t6087\t4842\t1\t2814.51\t-272.127\t108\t0.5\tAstranaar Sentinel\n", cf);
            fclose(cf);
            SpawnList sl;
            int got = creatures_read(cpath, &sl, &why);
            int tx, ty, first = -1;
            maps_tile_of(2735.93f, -416.844f, &tx, &ty);
            int n = got == 0 ? creatures_in_tile(&sl, tx, ty, &first) : 0;
            snprintf(line, sizeof line, "the list reads 3 spawns (%d); Astranaar's tile holds the two sentinels (%d), named, sized and placed", got == 0 ? sl.n : -1, n);
            check(got == 0 && sl.n == 3 && n == 2 && !strcmp(sl.spawns[first].name, "Astranaar Sentinel") && sl.spawns[first].display == 4841
                  && near(sl.spawns[first].x, 2735.93, 1e-6) && near(sl.spawns[first].facing, 3.1, 1e-6), line);
            check(creatures_in_tile(&sl, 0, 0, &first) == 0, "a tile with no spawns holds none");
            if (got == 0) creatures_free(&sl);
            cf = fopen(cpath, "w");
            fputs("33406\t6087\tnot-a-number\n", cf);
            fclose(cf);
            got = creatures_read(cpath, &sl, &why);
            check(got == W_ERR_CORRUPT && strstr(why.text, "line 1"), "a line that can't be read stops the reading, naming its line");
            check(creatures_read("/no/such/list.tsv", &sl, &why) == W_ERR_NOT_FOUND && strstr(why.text, "generate-custom-client-creatures"),
                  "no list at all: said, with how to make one");
            remove(cpath);
        }
    }
    /* }}} */

    /* {{{ errors (013): a code to branch on, a sentence to read */
    printf("errors\n");
    {
        WWhy w = { 0 };
        WErr e = w_fail(&w, W_ERR_NOT_FOUND, "the picture %s is in none of the archives", "X.blp");
        check(e == W_ERR_NOT_FOUND && w.code == e && !strcmp(w.text, "the picture X.blp is in none of the archives"),
              "w_fail fills the code and the sentence, and returns the code");
        check(!strcmp(w_err_name(W_ERR_CORRUPT), "W_ERR_CORRUPT") && !strcmp(w_err_name((WErr)99), "W_ERR_(not a code)"),
              "each code has its name; a number outside the list says so");
        size_t n;
        WWhy r;
        MpqChain ec;
        if (mpq_chain_open(&ec, client, "enUS", &r)) check(0, r.text);
        else {
            check(!mpq_read(ec.archives[0], "No\\Such\\File.blp", &n, &r) && r.code == W_ERR_NOT_FOUND,
                  "a file an archive doesn't hold: W_ERR_NOT_FOUND, the next archive is tried");
            mpq_chain_close(&ec);
        }
        Dbc d;
        check(dbc_open(&d, (const unsigned char *)"not a table at all, just words", 30, &r) == W_ERR_CORRUPT,
              "bytes that aren't a table: W_ERR_CORRUPT");
        settings_free(st); st = settings_new();
        check(settings_set(st, "cameraYawMoveSpeed", "5000", &r) == W_ERR_REFUSED && settings_set(st, "noSuchSetting", "1", &r) == W_ERR_REFUSED,
              "a setting out of its range, or with no such name: W_ERR_REFUSED");
        Settings *other = settings_new();               /* a second store (docs/001, "Handles"): a profile per server */
        settings_set(other, "cameraYawMoveSpeed", "90", &r);
        check(settings_num(other, "cameraYawMoveSpeed") == 90 && settings_num(st, "cameraYawMoveSpeed") == 180,
              "two settings stores live side by side, each with its own values");
        settings_free(other);
    }
    /* }}} */

    /* {{{ the settings store (173b1) */
    printf("the settings store\n");
    settings_free(st); st = settings_new();
    check(settings_num(st, "cameraYawMoveSpeed") == 180 && !strcmp(settings_text(st, "gxResolution"), "1280x720"),
          "every setting starts at its default (mouse look 180, window 1280x720)");
    check(settings_find(st, "noSuchSetting") == NULL, "an unknown name finds nothing (code asking for it by settings_num stops the client)");
    {
        /* the Resolution panel's tick boxes (173b5): windowed, vsync on,
         * not maximized, resizable; the window's kind is read at start */
        const Setting *w = settings_find(st, "gxWindow"), *mx = settings_find(st, "gxMaximize"), *vs = settings_find(st, "gxVSync"), *rl = settings_find(st, "windowResizeLock");
        check(w && mx && vs && rl && w->num == 1 && mx->num == 0 && vs->num == 1 && rl->num == 0
              && w->decl->restart && mx->decl->restart && !vs->decl->restart && !rl->decl->restart,
              "the window settings start windowed, vsync on, unmaximized and resizable; windowed and maximized need a restart");
    }
    check(settings_set(st, "cameraYawMoveSpeed", "300", &why) && strstr(why.text, "out of its range") && settings_num(st, "cameraYawMoveSpeed") == 180,
          "a slider refuses a value past its range, says so, and keeps its value");
    check(settings_set(st, "gxResolution", "wide", &why) && settings_set(st, "gxResolution", "9000x1000", &why),
          "the resolution refuses words and sizes past 7680x4320");
    check(!settings_set(st, "gxResolution", "5120x1440", &why) && !settings_set(st, "cameraYawMoveSpeed", "250.0", &why)
          && settings_num(st, "cameraYawMoveSpeed") == 250 && !strcmp(settings_text(st, "cameraYawMoveSpeed"), "250"),
          "an ultrawide resolution and a slider value are accepted, the number kept in one written form");
    snprintf(path, sizeof path, "%s/tmp/shared-memory/test-settings.wtf", dir);
    check(!settings_save(st, path, &why), "the changes are saved");
    FILE *saved = fopen(path, "r");
    char text[400] = "";
    size_t got = saved ? fread(text, 1, sizeof text - 1, saved) : 0;
    text[got] = '\0';
    if (saved) fclose(saved);
    check(!strcmp(text, "SET gxResolution \"5120x1440\"\nSET cameraYawMoveSpeed \"250\"\n"),
          "the file holds only the two changed settings, as SET name \"value\" lines");
    settings_free(st); st = settings_new();
    char *log = NULL;
    size_t log_len = 0;
    FILE *report = open_memstream(&log, &log_len);
    int skipped = settings_load(st, path, report);
    fclose(report);
    check(skipped == 0 && log_len == 0 && settings_num(st, "cameraYawMoveSpeed") == 250 && !strcmp(settings_text(st, "gxResolution"), "5120x1440"),
          "loading it back gives the same values, with nothing to report");
    free(log);
    FILE *bad = fopen(path, "w");
    if (bad) {
        fputs("# a comment\n\nSET cameraDistanceMaxFactor \"1.5\"\nSET nonsense \"1\"\nSET cameraYawMoveSpeed \"20\"\njunk\n", bad);
        fclose(bad);
    }
    settings_free(st); st = settings_new();
    log = NULL;
    report = open_memstream(&log, &log_len);
    skipped = settings_load(st, path, report);
    fclose(report);
    check(skipped == 3 && settings_num(st, "cameraDistanceMaxFactor") == 1.5 && settings_num(st, "cameraYawMoveSpeed") == 180,
          "a hand-edited file: the good line is taken, three bad lines are skipped");
    check(log && strstr(log, "line 4: no setting named nonsense") && strstr(log, "line 5: cameraYawMoveSpeed \"20\": out of its range (90 to 270)")
          && strstr(log, "line 6: not SET name"),
          "each bad line is reported with its line number and why.text");
    free(log);
    remove(path);
    settings_free(st); st = settings_new();
    log = NULL;
    report = open_memstream(&log, &log_len);
    skipped = settings_load(st, path, report);
    fclose(report);
    check(skipped == 0 && log && strstr(log, "no file at") && settings_num(st, "cameraDistanceMaxFactor") == 2,
          "no file yet: said once, every setting at its default");
    free(log);
    /* }}} */

    /* {{{ the ground files (173f) */
    printf("the ground files (the original's ADTs)\n");
    {
        MpqChain gc;
        if (mpq_chain_open(&gc, client, "enUS", &why)) { check(0, why.text); goto ground_done; }
        size_t len;
        const char *from;
        unsigned char *s = mpq_chain_read(&gc, "World\\Maps\\Kalimdor\\Kalimdor.wdt", &len, &from);
        check(s && adt_wdt_big_alpha(s, len, &why) == 0, "Kalimdor's blend maps are the 4-bit form");
        free(s);
        s = mpq_chain_read(&gc, "World\\Maps\\Northrend\\Northrend.wdt", &len, &from);
        check(s && adt_wdt_big_alpha(s, len, &why) == 1, "Northrend's are the 8-bit form");
        free(s);

        AdtTile *t = malloc(sizeof *t);
        s = mpq_chain_read(&gc, "World\\Maps\\Kalimdor\\Kalimdor_32_26.adt", &len, &from);
        int bad = !s || adt_read(s, len, 0, t, &why);
        free(s);
        check(!bad, bad ? why.text : "Astranaar's tile (Kalimdor_32_26) reads: 256 chunks");
        if (!bad) {
            int found = 0;
            for (int i = 0; i < t->ntextures; ++i) {
                unsigned char *p = mpq_chain_read(&gc, t->textures[i], &len, &from);
                if (p) ++found;
                free(p);
            }
            snprintf(line, sizeof line, "all %d of its ground pictures are in the archives (%d found; first %s)", t->ntextures, found, t->textures[0]);
            check(found == t->ntextures && t->ntextures > 0, line);
            int placed = 1;
            for (int k = 0; k < 256; ++k) {
                AdtChunk *c = &t->chunks[k];
                if (c->ix != k % 16 || c->iy != k / 16 || !near(c->x, 3200 - c->iy * ADT_CHUNK_SIZE, 0.01) || !near(c->y, 0 - c->ix * ADT_CHUNK_SIZE, 0.01)) placed = 0;
            }
            check(placed, "each chunk sits where its row and column say (rows run south, columns east)");
            /* the client's heights against the server's own map file, at
             * every corner and centre of every chunk */
            float worst_h = 0;
            const float step = ADT_CHUNK_SIZE / 8;
            for (int k = 0; k < 256; ++k)
                for (int i = 0; i < 145; ++i) {
                    AdtChunk *c = &t->chunks[k];
                    int r17 = i % 17;
                    float row = (float)(i / 17) + (r17 >= 9 ? 0.5f : 0.0f), col = r17 >= 9 ? (float)(r17 - 9) + 0.5f : (float)r17;
                    float hs = maps_world_height(w, c->x - row * step, c->y - col * step, &gx, &gy);
                    if (hs > -10000 && fabsf(hs - (c->z + c->heights[i])) > worst_h) worst_h = fabsf(hs - (c->z + c->heights[i]));
                }
            snprintf(line, sizeof line, "its 37,120 heights match the server's own map, within %.4f yards", worst_h);
            check(worst_h < 0.01f, line);
            /* 4-bit blend maps: an unpainted last column copies the one before */
            int fixed = 1, painted = 0;
            for (int k = 0; k < 256; ++k)
                for (int l = 1; l < t->chunks[k].nlayers; ++l) {
                    for (int r = 0; r < 64; ++r) if (t->chunks[k].alpha[l][r * 64 + 63] != t->chunks[k].alpha[l][r * 64 + 62]) fixed = 0;
                    for (int i = 0; i < 4096; ++i) if (t->chunks[k].alpha[l][i] != 0 && t->chunks[k].alpha[l][i] != 255) { painted = 1; break; }
                }
            /* placements (173g): doodads stand on the ground; turns go the original's way */
            int on_ground = 0, measured = 0;
            for (int k = 0; k < t->ndoodads; ++k) {
                float hs = maps_world_height(w, t->doodads[k].x, t->doodads[k].y, &gx, &gy);
                if (hs < -10000) continue;
                ++measured;
                if (fabsf(hs - t->doodads[k].z) < 2.0f) ++on_ground;
            }
            snprintf(line, sizeof line, "%d of its %d doodads stand within 2 yards of the server's ground (the rest on rocks, roots or buildings)", on_ground, measured);
            check(t->ndoodads > 500 && on_ground * 10 > measured * 8, line);
            check(t->nbuildings > 0 && t->nplaces > 0, "it places buildings too (WMOs)");
            AdtPlace turn = { .x = 0, .y = 0, .z = 0, .rot = { 0, 30, 0 }, .scale = 2 };
            float pm[16];
            adt_place_matrix(&turn, 0, 0, pm);
            double fx = pm[0], fy = pm[1], up = pm[10];            /* where the model's front (+x) and up (+z) go */
            snprintf(line, sizeof line, "a turn of 30 degrees faces a model's front at 210 degrees (%.1f), upright, twice the size",
                     atan2(fy, fx) / 3.14159265358979 * 180 + (atan2(fy, fx) < 0 ? 360 : 0));
            check(near(atan2(fy, fx), (210 - 360) * 3.14159265358979 / 180, 1e-4) && near(hypot(fx, fy), 2, 1e-4) && near(up, 2, 1e-4), line);
            /* the placement maths against the server's own (173g step 4):
             * its collision tiles (data-files/vmaps) place the same models
             * from the same records, through its own reading */
            {
                int pairs = 0, agree = 0;
                float worst = 0;
                check_vmap_placements(dir, t, &pairs, &agree, &worst);
                snprintf(line, sizeof line, "%d doodads and buildings placed as the server's collision tiles place them (of %d found in both), "
                         "every model corner within %.4f yards", agree, pairs, worst);
                check(pairs > 50 && agree == pairs && worst < 0.01f, line);
            }
            /* buildings: each reads, and its furnishings land inside the box
             * the ground file gives the building */
            int roots = 0, groups_ok = 1, inside = 0, furnished = 0;
            for (int k = 0; k < t->nplaces; ++k) {
                AdtPlace *pl = &t->places[k];
                unsigned char *rs = mpq_chain_read(&gc, t->buildings[pl->name], &len, &from);
                WmoRoot root;
                if (!rs || wmo_read_root(rs, len, &root, &why)) { free(rs); continue; }
                free(rs);
                ++roots;
                char base[260], gpath[300];
                snprintf(base, sizeof base, "%s", t->buildings[pl->name]);
                *strrchr(base, '.') = '\0';
                snprintf(gpath, sizeof gpath, "%s_000.wmo", base);
                unsigned char *gs = mpq_chain_read(&gc, gpath, &len, &from);
                WmoGroup g;
                if (!gs || wmo_read_group(gs, len, &g, &why)) groups_ok = 0; else wmo_free_group(&g);
                free(gs);
                float bm[16], fm[16];
                adt_place_matrix(pl, 0, 0, bm);
                for (int si = 0; si < root.nsets; ++si) {
                    if (si != 0 && si != pl->doodad_set) continue;
                    for (int fi = root.sets[si].first; fi < root.sets[si].first + root.sets[si].count; ++fi) {
                        adt_inside_matrix(bm, root.furnishings[fi].pos, root.furnishings[fi].quat, root.furnishings[fi].scale, fm);
                        ++furnished;
                        int in = 1;
                        for (int a = 0; a < 3; ++a) if (fm[12 + a] < pl->lo[a] - 2 || fm[12 + a] > pl->hi[a] + 2) in = 0;
                        inside += in;
                    }
                }
                wmo_free_root(&root);
            }
            snprintf(line, sizeof line, "all %d of its building placements read, with their first parts", t->nplaces);
            check(roots == t->nplaces && groups_ok, line);
            snprintf(line, sizeof line, "%d of their %d furnishings land inside their building's box", inside, furnished);
            check(furnished > 0 && inside == furnished, line);
            /* water (173r) */
            float wlo = 1e9f, whi = -1e9f;
            for (int k = 0; k < t->nliquids; ++k)
                for (int r = 0; r <= 8; ++r) for (int c = 0; c <= 8; ++c) { wlo = fminf(wlo, t->liquids[k].height[r][c]); whi = fmaxf(whi, t->liquids[k].height[r][c]); }
            snprintf(line, sizeof line, "its %d water layers read, every surface between %.2f and %.2f yards (no stray zeros)", t->nliquids, wlo, whi);
            check(t->nliquids == 60 && wlo > 90 && whi < 110, line);
            LiquidWorld *lw = liquid_new();
            liquid_add_tile(lw, 26, 32, t);
            int wk = 0;
            float lake = liquid_surface(lw, 2761, -499, &wk), town = liquid_surface(lw, 2717, -400, &wk);
            snprintf(line, sizeof line, "the lake east of town has water (surface %.2f), the town itself none", lake);
            check(lake > 99 && lake < 104 && town < -10000, line);
            /* the lake's depths (173r): the file's byte is about 9 a yard,
             * so the deepest corner (20.8 yards, at 2866, -331) is near 187,
             * and the shore's near 0 */
            {
                int deepest = 0, shallowest = 255;
                for (int k = 0; k < t->nliquids; ++k)
                    for (int r = 0; r < 8; ++r) for (int c = 0; c < 8; ++c) {
                        if (!t->liquids[k].wet[r][c]) continue;
                        int d = t->liquids[k].depth[r][c];
                        if (d > deepest) deepest = d;
                        if (d < shallowest) shallowest = d;
                    }
                snprintf(line, sizeof line, "the lake's depths read from 0 at the shore to the deepest's byte (%d to %d; about 0 to 187)", shallowest, deepest);
                check(shallowest < 5 && deepest > 170 && deepest < 200, line);
            }
            /* a building's pool (173r): the small night-elf house at
             * 2808.6, -321.0 holds a 2 x 2-cell basin of building water */
            {
                int house = -1;
                for (int k = 0; k < t->nplaces; ++k)
                    if (strstr(t->buildings[t->places[k].name], "NIGHTELFSMALLHOUSE") && fabsf(t->places[k].z - 108.7f) < 0.1f) house = k;
                size_t hl = 0, rl = 0;
                unsigned char *hs = house < 0 ? NULL : mpq_chain_read(&gc, "World\\wmo\\Kalimdor\\Buildings\\NightElfSmallHouse\\NightElfSmallHouse_000.wmo", &hl, &from);
                unsigned char *hr = house < 0 ? NULL : mpq_chain_read(&gc, "World\\wmo\\Kalimdor\\Buildings\\NightElfSmallHouse\\NightElfSmallHouse.wmo", &rl, &from);
                WmoGroup hg;
                WmoRoot hroot;
                if (!hs || !hr || wmo_read_group(hs, hl, &hg, &why) || wmo_read_root(hr, rl, &hroot, &why))
                    check(0, house < 0 ? "the small house isn't placed on the tile" : "the small house's files couldn't be read");
                else {
                    int kind = wmo_liquid_kind(&hroot, &hg);
                    snprintf(line, sizeof line, "the house's pool is 3 x 3 points of building water (%d x %d, kind %d: 13)", hg.pool.nx, hg.pool.ny, kind);
                    check(hg.pool.nx == 3 && hg.pool.ny == 3 && kind == 13, line);
                    float hm[16];
                    adt_place_matrix(&t->places[house], 0, 0, hm);
                    liquid_add_pool(lw, 26 * 64 + 32, hm, 0, 0, &hg.pool, kind);
                    int pk = 0;
                    float in_pool = liquid_surface(lw, 2801.98, -320.59, &pk), beside = liquid_surface(lw, 2786.98, -320.59, &pk);
                    snprintf(line, sizeof line, "turned and placed, its basin's surface is found (%.2f, kind %d; 109.88, 13), and none 15 yards off", in_pool, pk);
                    check(fabsf(in_pool - 109.88f) < 0.05f && pk == 13 && beside < -10000, line);
                    wmo_free_group(&hg);
                    wmo_free_root(&hroot);
                }
                free(hs); free(hr);
            }
            liquid_drop_tile(lw, 26, 32);
            check(liquid_surface(lw, 2761, -499, &wk) < -10000 && liquid_surface(lw, 2801.98, -320.59, &wk) < -10000, "dropping the tile forgets its water, the house's pool too");
            liquid_free(lw);
            check(fixed, "every blend map's last column copies the one before, as the original fills the unpainted edge");
            check(painted, "the blend maps hold soft edges, not only all-or-nothing");
            adt_free(t);
        }
        /* the far horizon (173s): the low-detail map agrees with the full
         * ground at Astranaar's tile's corner */
        s = mpq_chain_read(&gc, "World\\Maps\\Kalimdor\\Kalimdor.wdl", &len, &from);
        AdtWdl *wdl = malloc(sizeof *wdl);
        if (!s || adt_wdl_read(s, len, wdl, &why)) check(0, s ? why.text : "Kalimdor.wdl is in the archives");
        else {
            int tiles = 0;
            for (int i = 0; i < 4096; ++i) tiles += wdl->have[i];
            float corner = wdl->heights[26 * 64 + 32][0];
            snprintf(line, sizeof line, "Kalimdor's low-detail map covers %d tiles; at Astranaar's tile's corner it says %.0f, the full ground 164.69", tiles, corner);
            check(tiles > 900 && fabsf(corner - 164.69f) < 1.5f, line);
        }
        free(wdl);
        free(s);
        s = mpq_chain_read(&gc, "World\\Maps\\Northrend\\Northrend_30_20.adt", &len, &from);
        bad = !s || adt_read(s, len, 1, t, &why);
        free(s);
        check(!bad, bad ? why.text : "a Northrend tile with packed and plain 8-bit blend maps reads (Northrend_30_20)");
        if (!bad) adt_free(t);
        free(t);
        mpq_chain_close(&gc);
    }
ground_done:
    /* }}} */

    /* {{{ her animations (173i) */
    printf("her animations\n");
    {
        MpqChain ac;
        if (mpq_chain_open(&ac, client, "enUS", &why)) { check(0, why.text); goto anim_done; }
        size_t len;
        const char *from;
        const char *base = "Character\\NightElf\\Female\\NightElfFemale";
        snprintf(path, sizeof path, "%s.m2", base);
        unsigned char *s = mpq_chain_read(&ac, path, &len, &from);
        Anim a;
        int bad = !s || anim_read(s, len, &a, &why);
        free(s);
        snprintf(line, sizeof line, "her skeleton reads (%d bones, %d animations, %u points)", bad ? 0 : a.nbones, bad ? 0 : a.nseqs, bad ? 0 : a.npoints);
        check(!bad && a.nbones > 50 && a.npoints == 4113, bad ? why.text : line);
        if (bad) { mpq_chain_close(&ac); goto anim_done; }
        int needed = 0, found = 0;
        for (int i = 0; i < a.nseqs; ++i) {
            if (!anim_needs_file(&a, i)) continue;
            ++needed;
            snprintf(path, sizeof path, "%s%04d-%02d.anim", base, a.seqs[i].id, a.seqs[i].variation);
            unsigned char *f = mpq_chain_read(&ac, path, &len, &from);
            if (f) { ++found; anim_attach_file(&a, i, f, len); }
        }
        snprintf(line, sizeof line, "all %d animations kept in .anim files are in the archives (%d found)", needed, found);
        check(found == needed, line);
        static const int USED[] = { MOVE_STAND, MOVE_RUN, MOVE_WALK_BACK, MOVE_SHUFFLE_LEFT, MOVE_SHUFFLE_RIGHT, MOVE_JUMP_START, MOVE_JUMP, MOVE_JUMP_END, MOVE_FALL };
        int all = 1;
        for (unsigned i = 0; i < sizeof USED / sizeof USED[0]; ++i) if (anim_find(&a, USED[i], 0) < 0) all = 0;
        check(all, "she has every animation the mover uses: stand, run, walk back, both turns, jump start, jump, jump end, fall");

        /* poses: her bent body, measured */
        float (*poses)[16] = malloc(sizeof *poses * a.nbones), (*scratch)[16] = malloc(sizeof *scratch * a.nbones);
        float *at0 = malloc(sizeof(float) * 3 * a.npoints), *at1 = malloc(sizeof(float) * 3 * a.npoints);
        uint32_t points_at = (uint32_t)(a.m2[64] | a.m2[65] << 8 | a.m2[66] << 16 | (uint32_t)a.m2[67] << 24);
        #define BEND(seq, ms, outp) do { \
            anim_pose(&a, (seq), (ms), 0, poses); \
            for (uint32_t i = 0; i < a.npoints; ++i) { float in[3]; memcpy(in, a.m2 + points_at + (size_t)i * 48, 12); \
                anim_skin_point(&a, (const float (*)[16])poses, i, in, (outp) + i * 3, NULL, NULL); } } while (0)
        int stand = anim_find(&a, MOVE_STAND, 0), run = anim_find(&a, MOVE_RUN, 0);
        BEND(stand, 0, at0);
        float lo = 1e9f, hi = -1e9f;
        for (uint32_t i = 0; i < a.npoints; ++i) { lo = fminf(lo, at0[i * 3 + 2]); hi = fmaxf(hi, at0[i * 3 + 2]); }
        snprintf(line, sizeof line, "standing, her feet are on the ground (lowest point %.3f) and she is %.2f yards tall", lo, hi);
        check(fabsf(lo) < 0.1f && hi > 1.9f && hi < 2.4f, line);
        BEND(run, 0, at0);
        BEND(run, a.seqs[run].length / 2, at1);
        float most = 0;
        for (uint32_t i = 0; i < a.npoints * 3; ++i) most = fmaxf(most, fabsf(at0[i] - at1[i]));
        snprintf(line, sizeof line, "half a stride apart, the run moves some point of her %.2f yards", most);
        check(most > 0.3f, line);
        BEND(run, a.seqs[run].length, at1);
        most = 0;
        for (uint32_t i = 0; i < a.npoints * 3; ++i) most = fmaxf(most, fabsf(at0[i] - at1[i]));
        check(most < 1e-4f, "the run's end is its start: it loops without a jump");
        #undef BEND

        /* choosing the animation */
        MovesState ms;
        moves_init(&ms, &a);
        check(ms.seq == stand, "she starts standing");
        MovesInput mi = { .forward = 1, .grounded = 1, .speed = 8.4f, .dt = 0.1f };
        moves_step(&ms, &a, &mi);
        snprintf(line, sizeof line, "running at 8.4 yards a second: the run, played %.0f ms in a tenth of a second (made for %.2f, so 121)", ms.ms, a.seqs[run].move_speed);
        check(ms.seq == run && near(ms.ms, 100 * 8.4 / a.seqs[run].move_speed, 0.5), line);
        check(ms.from == stand && ms.blend_left > 0, "and blends in from the stand");
        {
            /* walking (173i): the walk, at its own pace for 3 yards a second */
            MovesState ws;
            moves_init(&ws, &a);
            int walk = anim_find(&a, MOVE_WALK, 0);
            MovesInput wi = { .forward = 1, .walking = 1, .grounded = 1, .speed = 3.0f, .dt = 0.1f };
            moves_step(&ws, &a, &wi);
            snprintf(line, sizeof line, "walking at 3 yards a second: the walk, played %.0f ms in a tenth of a second (made for %.2f)", ws.ms, walk >= 0 ? a.seqs[walk].move_speed : 0.0f);
            check(walk >= 0 && ws.seq == walk && near(ws.ms, 100 * 3.0 / a.seqs[walk].move_speed, 0.5), line);
            wi.forward = 0; wi.back = 1;
            moves_step(&ws, &a, &wi);
            check(ws.seq == anim_find(&a, MOVE_WALK_BACK, 0), "walking backwards: the walk-back, as running backwards");
        }
        mi = (MovesInput){ .grounded = 0, .rising = 1, .dt = 0.01f };
        moves_step(&ms, &a, &mi);
        int jstart = anim_find(&a, MOVE_JUMP_START, 0), jump = anim_find(&a, MOVE_JUMP, 0);
        check(ms.seq == jstart, "leaving the ground going up: the jump's start");
        for (uint32_t t = 0; t < a.seqs[jstart].length + 20; t += 10) moves_step(&ms, &a, &mi);
        check(ms.seq == jump, "after it, the jump loops in the air");
        mi = (MovesInput){ .grounded = 1, .dt = 0.01f };
        moves_step(&ms, &a, &mi);
        check(ms.seq == anim_find(&a, MOVE_JUMP_END, 0), "landing standing still: the landing");
        moves_init(&ms, &a);
        mi = (MovesInput){ .grounded = 0, .rising = 0, .dt = 0.01f };
        moves_step(&ms, &a, &mi);
        check(ms.seq == anim_find(&a, MOVE_FALL, 0), "dropping off an edge: the fall");
        moves_init(&ms, &a);
        mi = (MovesInput){ .turn_left = 1, .grounded = 1, .dt = 0.01f };
        moves_step(&ms, &a, &mi);
        check(ms.seq == anim_find(&a, MOVE_SHUFFLE_LEFT, 0), "turning on the spot: the turning shuffle");
        mi = (MovesInput){ .strafe_left = 1, .grounded = 1, .speed = 8.4f, .dt = 0.01f };
        for (int i = 0; i < 50; ++i) moves_step(&ms, &a, &mi);
        snprintf(line, sizeof line, "strafing left: the run, her body turned %.0f degrees toward it", ms.body_turn / 3.14159265358979 * 180);
        check(ms.seq == run && near(ms.body_turn, 3.14159265358979 / 2, 1e-3), line);
        /* swimming (173r) */
        moves_init(&ms, &a);
        mi = (MovesInput){ .swimming = 1, .grounded = 1, .dt = 0.01f };
        moves_step(&ms, &a, &mi);
        check(ms.seq == anim_find(&a, MOVE_SWIM_IDLE, 0) && ms.seq >= 0, "in deep water, standing still: treading water");
        mi.forward = 1; mi.speed = 5.67f;
        moves_step(&ms, &a, &mi);
        check(ms.seq == anim_find(&a, MOVE_SWIM, 0) && ms.seq >= 0, "swimming forward: the swim");
        mi = (MovesInput){ .swimming = 1, .grounded = 1, .strafe_left = 1, .speed = 5.67f, .dt = 0.01f };
        moves_step(&ms, &a, &mi);
        check(ms.seq == anim_find(&a, MOVE_SWIM_LEFT, 0) && ms.seq >= 0, "swimming left: its own sideways stroke");

        /* figures with no mover (173j): play, then advance at the animation's own pace */
        moves_init(&ms, &a);
        moves_play(&ms, &a, 60);                            /* EmoteTalk */
        int talk = anim_find(&a, 60, 0);
        check(talk >= 0 && ms.seq == talk && ms.from == stand, "a bystander's next animation starts, blended in from the last");
        for (int i = 0; i < 100; ++i) moves_advance(&ms, &a, 0.05f);
        snprintf(line, sizeof line, "and loops at its own pace (5 s in, %.0f ms into its %u ms), the blend long finished", ms.ms, a.seqs[talk].length);
        check(near(ms.ms, fmod(5000.0, a.seqs[talk].length), 1.0) && ms.from == -1, line);
        free(poses); free(scratch); free(at0); free(at1);
        anim_free(&a);
        mpq_chain_close(&ac);
    }
anim_done:
    /* }}} */

    /* {{{ collision (173q): made-up shapes, then the mover among them */
    printf("collision\n");
    {
        CollideWorld *cw = collide_new(0, 0);
        const float id16[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        /* a plank 5 yards up over x 20..30, y -2..2; a wall at x 5, y -5..5,
         * 10 tall; a low step (0.5 up) over x 40..50 with its riser; a high
         * ledge (2 up) over x 60..70 with its face */
        const float shapes[] = {
            20, -2, 5,  30, -2, 5,  30, 2, 5,    20, -2, 5,  30, 2, 5,  20, 2, 5,
            5, -5, 0,   5, 5, 0,    5, 5, 10,    5, -5, 0,   5, 5, 10,  5, -5, 10,
            40, -5, 0.5f, 50, -5, 0.5f, 50, 5, 0.5f,   40, -5, 0.5f, 50, 5, 0.5f, 40, 5, 0.5f,
            40, -5, 0,  40, 5, 0,   40, 5, 0.5f, 40, -5, 0,  40, 5, 0.5f, 40, -5, 0.5f,
            60, -5, 2,  70, -5, 2,  70, 5, 2,    60, -5, 2,  70, 5, 2,  60, 5, 2,
            60, -5, 0,  60, 5, 0,   60, 5, 2,    60, -5, 0,  60, 5, 2,  60, -5, 2,
        };
        collide_add(cw, shapes, (int)(sizeof shapes / sizeof shapes[0] / 9), id16, 0);
        float fgx, fgy;
        check(near(collide_floor(cw, 25, 0.5, 6, &fgx, &fgy), 5, 1e-4), "standing over a plank, its top is the floor");
        check(collide_floor(cw, 25, 0.5, 4, &fgx, &fgy) < -10000, "below it (under a bridge), the plank isn't her floor");
        float wnx = 0, wny = 0;
        check(collide_wall(cw, 0, 0, 6, 0, 0, 0.4f, 1, 2.3f, 1.73f, &wnx, &wny) && near(wnx, -1, 1e-4), "walking at a wall hits it, facing her");
        check(!collide_wall(cw, 0, 6, 6, 6, 0, 0.4f, 1, 2.3f, 1.73f, &wnx, &wny), "walking past its end doesn't");
        check(!collide_wall(cw, 35, 0, 41, 0, 0, 0.4f, 1, 2.3f, 1.73f, &wnx, &wny), "a riser lower than her step isn't a wall");
        check(collide_wall(cw, 55, 0, 61, 0, 0, 0.4f, 1, 2.3f, 1.73f, &wnx, &wny), "a ledge higher than her step is");
        solids_test = cw;
        Mover m;
        MoverInput fwd = { .forward = 1 };
        mover_init(&m, 0, 0, 0, 0, 1.2, 60);              /* facing +x, toward the wall */
        for (int i = 0; i < 200; ++i) mover_step(&m, &fwd, 0.01, &(MoverWorld){ g_solid, w_solid, NULL, NULL });
        snprintf(line, sizeof line, "running at the wall for 2 s, she stops short of it (x %.2f; the wall at 5, her radius 0.4)", m.x);
        check(m.x < 4.65 && m.x > 4.0, line);
        mover_init(&m, 0, 0, 0, 0.6, 1.2, 60);            /* at an angle: she slides along it, then past its end */
        double most_x = 0;
        for (int i = 0; i < 200; ++i) {
            mover_step(&m, &fwd, 0.01, &(MoverWorld){ g_solid, w_solid, NULL, NULL });
            if (m.y < 4.5 && m.x > most_x) most_x = m.x;
        }
        snprintf(line, sizeof line, "running at it at an angle, she slides along it (never past x %.2f beside it) and on past its end (x %.2f, y %.2f)", most_x, m.x, m.y);
        check(most_x < 4.65 && m.y > 5 && m.x > 5, line);
        mover_init(&m, 35, 0, 0, 0, 1.2, 60);
        for (int i = 0; i < 150; ++i) mover_step(&m, &fwd, 0.01, &(MoverWorld){ g_solid, w_solid, NULL, NULL });
        snprintf(line, sizeof line, "a half-yard step: she walks up onto it (x %.2f, z %.2f)", m.x, m.z);
        check(m.x > 42 && near(m.z, 0.5, 1e-3), line);
        mover_init(&m, 55, 0, 0, 0, 1.2, 60);
        for (int i = 0; i < 150; ++i) mover_step(&m, &fwd, 0.01, &(MoverWorld){ g_solid, w_solid, NULL, NULL });
        snprintf(line, sizeof line, "a two-yard ledge: she stops at its face (x %.2f, z %.2f)", m.x, m.z);
        check(m.x < 60 && near(m.z, 0, 1e-3), line);
        mover_init(&m, 25, 0, 8, 0, 1.2, 60);
        m.grounded = 0;
        MoverInput none = { 0 };
        for (int i = 0; i < 200; ++i) mover_step(&m, &none, 0.01, &(MoverWorld){ g_solid, w_solid, NULL, NULL });
        snprintf(line, sizeof line, "dropped over the plank, she lands on it (z %.2f)", m.z);
        check(m.grounded && near(m.z, 5, 1e-3), line);
        /* the camera's line of sight (173m): the wall at x 5 is struck half way */
        float hit = collide_ray(cw, 0, 0, 1, 10, 0, 1), clear = collide_ray(cw, 0, 6, 1, 10, 6, 1);
        snprintf(line, sizeof line, "a line from her to the camera through the wall is struck half way along (%.3f); past the wall's end, nothing (%.1f)", hit, clear);
        check(near(hit, 0.5, 1e-4) && clear > 1, line);
        /* from below (the owner, 2026-10-06: "climbing out of a river
         * into a building"): the ground rises 3 yards at x 100, where a
         * building's wall stands on the higher ground (z 3 to 10). Her
         * step onto the bank would lift her into it; the wall is asked
         * about from the height she would stand at, so she stops */
        {
            const float bank_wall[] = { 100, -5, 3,  100, 5, 3,  100, 5, 10,   100, -5, 3,  100, 5, 10,  100, -5, 10 };
            collide_add(cw, bank_wall, 2, id16, 0);
            mover_init(&m, 97, 0, 0, 0, 1.2, 60);
            for (int i = 0; i < 200; ++i) mover_step(&m, &fwd, 0.01, &(MoverWorld){ g_bank, w_solid, NULL, NULL });
            snprintf(line, sizeof line, "walking up onto a bank where a wall stands, she stops outside it (x %.2f, z %.2f; the wall at 100)", m.x, m.z);
            check(m.x < 100 && near(m.z, 0, 1e-3), line);
        }
        /* dropping a tile (173s): its triangles go, the others stay */
        const float plank2[] = { 80, -2, 3,  90, -2, 3,  90, 2, 3,   80, -2, 3,  90, 2, 3,  80, 2, 3 };
        collide_add(cw, plank2, 2, id16, 7);
        int before = collide_count(cw);
        collide_drop_tile(cw, 7);
        check(collide_count(cw) == before - 2 && collide_floor(cw, 85, 0, 10, &fgx, &fgy) < -10000 && near(collide_floor(cw, 25, 0.5, 6, &fgx, &fgy), 5, 1e-4),
              "dropping a tile forgets its triangles, and only its own");
        collide_free(cw);
        solids_test = NULL;
    }
    /* }}} */

    /* {{{ swimming (173r): flat ground at 0 rising past x 10 (a shore),
     * water to 5 everywhere */
    printf("swimming\n");
    {
        Mover m;
        MoverWorld sea = { g_shore, NULL, w_sea, NULL };
        MoverInput none = { 0 }, fwd = { .forward = 1 }, down = { .swim_down = 1 };
        mover_init(&m, 0, 0, 0, 0, 1.2, 60);
        mover_step(&m, &none, 0.01, &sea);
        check(m.swimming, "standing in water deeper than 1.6 yards over her feet, she swims");
        for (int i = 0; i < 300; ++i) mover_step(&m, &none, 0.01, &sea);
        snprintf(line, sizeof line, "left alone she floats up, feet 1.6 under the surface (z %.2f)", m.z);
        check(near(m.z, 5 - MOVER_SWIM_DEPTH, 1e-6), line);
        for (int i = 0; i < 50; ++i) mover_step(&m, &down, 0.01, &sea);
        snprintf(line, sizeof line, "X takes her down at swim speed (half a second: z %.2f)", m.z);
        check(near(m.z, 5 - MOVER_SWIM_DEPTH - 0.5 * MOVER_SWIM * 1.2, 1e-3), line);
        mover_init(&m, -30, 0, 3.4, 0, 1.2, 60);
        mover_step(&m, &none, 0.01, &sea);
        double x0 = m.x;
        for (int i = 0; i < 100; ++i) mover_step(&m, &fwd, 0.01, &sea);
        snprintf(line, sizeof line, "she swims forward at 4.72 x 1.2 = %.2f yards a second (%.2f)", MOVER_SWIM * 1.2, m.x - x0);
        check(near(m.x - x0, MOVER_SWIM * 1.2, 0.05), line);
        for (int i = 0; i < 1000 && m.swimming; ++i) mover_step(&m, &fwd, 0.01, &sea);
        for (int i = 0; i < 50; ++i) mover_step(&m, &fwd, 0.01, &sea);
        snprintf(line, sizeof line, "swimming to the shore, she walks out onto it (x %.2f, z %.2f, on the ground)", m.x, m.z);
        check(!m.swimming && m.grounded && m.x > 10, line);
    }
    /* }}} */

    /* {{{ particles (173o) */
    printf("particles\n");
    {
        MpqChain pc;
        if (mpq_chain_open(&pc, client, "enUS", &why)) { check(0, why.text); goto particles_done; }
        size_t len;
        const char *from;
        unsigned char *s = mpq_chain_read(&pc, "World\\Generic\\PassiveDoodads\\Lights\\GeneralTorch01.m2", &len, &from);
        Anim a;
        Emitter *em = NULL;
        int ne = 0;
        int bad = !s || anim_read(s, len, &a, &why) || particles_read(&a, &em, &ne, &why);
        free(s);
        snprintf(line, sizeof line, "the torch has %d emitters (a flame and its glow), each on a bone", ne);
        check(!bad && ne == 2, bad ? why.text : line);
        if (!bad && ne == 2) {
            int st = anim_find(&a, 0, 0);
            float rate = 0, life = 0;
            anim_track_value(&a, &em[0].rate, st, 0, 0, ANIM_FLOAT, &rate);
            anim_track_value(&a, &em[0].life, st, 0, 0, ANIM_FLOAT, &life);
            snprintf(line, sizeof line, "its flame: %.0f a second, each living %.1f s, added light (blend %d), from a %dx%d picture grid", rate, life, em[0].blend, em[0].rows, em[0].cols);
            check(near(rate, 20, 0.01) && near(life, 1, 0.01) && em[0].blend == 4 && em[0].rows == 4 && em[0].cols == 4, line);
            EmitterRun run = { 0 };
            float w[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 100, 200, 50, 1 };   /* the bone 100, 200, 50 yards off, unturned */
            for (int t = 0; t < 200; ++t) particles_step(&a, &em[0], &run, w, st, (uint32_t)(t * 10), 0.01f, 1);
            float lo = 1e9f, hi = -1e9f;
            for (int k = 0; k < run.n; ++k) { lo = fminf(lo, run.ps[k].p[2]); hi = fmaxf(hi, run.ps[k].p[2]); }
            snprintf(line, sizeof line, "after 2 s, %d sparks live (about rate x life) and rise from the flame (%.2f to %.2f yards up)", run.n, lo, hi);
            check(run.n >= 15 && run.n <= 25 && lo >= 50 && hi > lo, line);
            float rgba[4], size[2];
            Particle young = { .age = 0, .life = 1 };
            particles_look(&em[0], &young, rgba, size);
            snprintf(line, sizeof line, "a new spark is orange (%.2f, %.2f, %.2f)", rgba[0], rgba[1], rgba[2]);
            check(rgba[0] > 0.9f && rgba[1] < 0.5f && rgba[2] < 0.2f, line);
            /* the fog (173o): a spark adding light fades out; a see-through one takes the fog's colour */
            const float fog[3] = { 0.2f, 0.3f, 0.4f };
            float spark[4] = { 1, 0.5f, 0.1f, 0.8f }, smoke[4] = { 1, 0.5f, 0.1f, 0.8f }, near_one[4] = { 1, 0.5f, 0.1f, 0.8f };
            particles_fog(4, spark, fog, 1.5f);
            particles_fog(2, smoke, fog, 1);
            particles_fog(4, near_one, fog, -2);
            check(spark[0] == 0 && spark[1] == 0 && spark[2] == 0 && spark[3] == 0.8f, "past the fog's end, an added-light spark adds nothing");
            check(near(smoke[0], 0.2, 1e-6) && near(smoke[2], 0.4, 1e-6) && smoke[3] == 0.8f, "past it, a see-through particle is the fog's colour");
            check(near_one[0] == 1 && near_one[1] == 0.5f, "before the fog starts, a spark is untouched");
            /* Particle Density (173b6): at 0.3, about three in ten are born */
            EmitterRun thin = { 0 };
            for (int t = 0; t < 1000; ++t) particles_step(&a, &em[0], &thin, w, st, (uint32_t)(t * 10), 0.01f, 0.3f);
            snprintf(line, sizeof line, "at density 0.3, %d sparks live after 10 s (about 6: rate x life x 0.3)", thin.n);
            check(thin.n >= 3 && thin.n <= 9, line);
            particles_free_run(&thin);
            /* a run repeats exactly: the same seed, the same sparks (a test
             * scene's pictures and logs depend on it) */
            EmitterRun r1 = { .rng = 12345 }, r2 = { .rng = 12345 };
            for (int t = 0; t < 100; ++t) { particles_step(&a, &em[0], &r1, w, st, (uint32_t)(t * 10), 0.01f, 1); particles_step(&a, &em[0], &r2, w, st, (uint32_t)(t * 10), 0.01f, 1); }
            check(r1.n == r2.n && r1.n > 0 && !memcmp(r1.ps, r2.ps, sizeof *r1.ps * (size_t)r1.n), "the same seed throws the same sparks");
            particles_free_run(&r1);
            particles_free_run(&r2);
            particles_free_run(&run);
            free(em);
            anim_free(&a);
        }
        /* two-picture pieces and moving pictures (173n): the aurora */
        {
            size_t ml, sl;
            unsigned char *am = mpq_chain_read(&pc, "Environments\\Stars\\AuroraYellowGreen.m2", &ml, &from);
            unsigned char *as = mpq_chain_read(&pc, "Environments\\Stars\\AuroraYellowGreen00.skin", &sl, &from);
            M2Model aur;
            Anim aan;
            if (am && as && !m2_read(am, ml, as, sl, &aur, &why) && !anim_read(am, ml, &aan, &why)) {
                int two = 0, moving = -1;
                for (uint32_t u = 0; u < aur.nunits; ++u) {
                    if (aur.units[u].layers == 2 && aur.units[u].tex[1] >= 0) ++two;
                    if (moving < 0 && aur.units[u].uvanim[1] >= 0) moving = aur.units[u].uvanim[1];
                    if (moving < 0 && aur.units[u].uvanim[0] >= 0) moving = aur.units[u].uvanim[0];
                }
                float m0[4], m1[4];
                anim_uv(&aan, moving, anim_find(&aan, 0, 0), 0, 0, m0);
                anim_uv(&aan, moving, anim_find(&aan, 0, 0), 5000, 5000, m1);
                snprintf(line, sizeof line, "the aurora: %d of its pieces lay two pictures, and a picture slides over time (%.3f, %.3f then %.3f, %.3f)", two, m0[0], m0[1], m1[0], m1[1]);
                check(two > 0 && moving >= 0 && (m0[0] != m1[0] || m0[1] != m1[1]), line);
                m2_free(&aur);
                anim_free(&aan);
            } else check(0, "the aurora model reads");
            free(am); free(as);
        }
        /* camera-facing bones (173g's flat halos): the street lamp's glow */
        s = mpq_chain_read(&pc, "World\\Generic\\NightElf\\Passive Doodads\\Lamps\\KalidarStreetLamp02.m2", &len, &from);
        Anim lamp;
        if (s && !anim_read(s, len, &lamp, &why)) {
            int bb = -1;
            for (int i = 0; i < lamp.nbones && bb < 0; ++i) if (lamp.bones[i].flags & 0x8) bb = i;
            float (*po)[16] = malloc(sizeof *po * lamp.nbones);
            const float view[9] = { 0, 1, 0,   -1, 0, 0,   0, 0, 1 };   /* the camera off to her left (+y) */
            anim_pose_facing(&lamp, anim_find(&lamp, 0, 0), 0, 0, po, view);
            float *m = po[bb < 0 ? 0 : bb], l = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
            snprintf(line, sizeof line, "the lamp's glow bone (%d) turns its face to the camera (its x axis %.2f %.2f %.2f, the camera's way 0 1 0)", bb, m[0] / l, m[1] / l, m[2] / l);
            check(bb >= 0 && anim_has_billboards(&lamp) && near(m[1] / l, 1, 1e-4), line);
            free(po);
            anim_free(&lamp);
        } else check(0, "the street lamp's model reads");
        free(s);
        mpq_chain_close(&pc);
    }
particles_done:
    /* }}} */

    /* {{{ the zone's light (173n) */
    printf("the zone's light\n");
    {
        MpqChain lc;
        LightTables lt;
        if (mpq_chain_open(&lc, client, "enUS", &why) || light_open(&lt, &lc, &why)) { check(0, why.text); goto light_done; }
        LightNow noon, night, far;
        int id = light_at(&lt, 1, 2717, -400, 110, 12, LIGHT_CLEAR, &noon);
        light_at(&lt, 1, 2717, -400, 110, 0.5f, LIGHT_CLEAR, &night);
        snprintf(line, sizeof line, "at Astranaar the light is Ashenvale's own (light %d), not Kalimdor's default (191)", id);
        check(id == 192, line);
        float sn = noon.sun[0] + noon.sun[1] + noon.sun[2], sm = night.sun[0] + night.sun[1] + night.sun[2];
        snprintf(line, sizeof line, "noon's sun is brighter than half past midnight's (%.2f against %.2f, red + green + blue)", sn, sm);
        check(sn > sm, line);
        snprintf(line, sizeof line, "the fog ends %.0f yards off at noon, and starts nearer than it ends (%.0f)", noon.fog_end, noon.fog_start);
        check(noon.fog_end > 100 && noon.fog_end < 1600 && noon.fog_start < noon.fog_end, line);
        snprintf(line, sizeof line, "the water's opacity at Astranaar: sea %.2f shallow to %.2f deep, river %.2f to %.2f (0.75-1, 0.5-1)",
                 noon.water_alpha[0], noon.water_alpha[1], noon.water_alpha[2], noon.water_alpha[3]);
        check(fabsf(noon.water_alpha[0] - 0.75f) < 0.01f && fabsf(noon.water_alpha[1] - 1) < 0.01f && fabsf(noon.water_alpha[2] - 0.5f) < 0.01f && fabsf(noon.water_alpha[3] - 1) < 0.01f, line);
        int far_id = light_at(&lt, 1, -600, -4200, 40, 12, LIGHT_CLEAR, &far);       /* Durotar, across the continent */
        snprintf(line, sizeof line, "across the continent, in Durotar, another light (%d)", far_id);
        check(far_id >= 0 && far_id != 192, line);
        check(noon.sun_dir[2] > 0.9f && night.sun_dir[2] < noon.sun_dir[2], "the sun is high at noon and low in the night");
        /* underwater (173n): the same light's underwater set (Light's field 9) */
        LightNow under;
        int uid = light_at(&lt, 1, 2717, -400, 110, 12, LIGHT_UNDERWATER, &under);
        snprintf(line, sizeof line, "under water at Astranaar, noon: light %d, fog %.0f to %.0f yards (above: %.0f to %.0f), colour %.2f %.2f %.2f",
                 uid, under.fog_start, under.fog_end, noon.fog_start, noon.fog_end, under.fog[0], under.fog[1], under.fog[2]);
        check(uid == 192 && under.fog_end > 0 && under.fog_end < noon.fog_end, line);
        light_close(&lt);
        mpq_chain_close(&lc);
    }
light_done:
    /* }}} */

    /* {{{ the camera (173m) */
    printf("the camera\n");
    {
        settings_free(st); st = settings_new();
        const double deg = 3.14159265358979323846 / 180;
        const float px90 = (float)(90 * deg / 0.006);          /* pixels of drag for 90 degrees at the default speed */
        FollowCamInput idle = { .dt = 0.01f }, held_l = { .left = 1, .dt = 0.01f };
        FollowCam cam;
        double facing;

        /* left drag 90 degrees, then a quarter second of running in style 1 */
        cam = (FollowCam){ .yaw = 0, .pitch = 0.35f, .dist = 12, .rest_pitch = 0.35f, .settings = st };
        facing = 0;
        FollowCamInput drag = { .left = 1, .dx = -px90, .dy = 50, .dt = 0.01f };
        camera_drag(&cam, &drag, 0, &facing);
        float pitch_after_drag = cam.pitch;
        check(near(cam.yaw, 90 * deg, 1e-4) && facing == 0, "a left drag turns the camera 90 degrees and not her");
        for (int i = 0; i < 25; ++i) camera_follow(&cam, &idle, 0, 1, facing);
        snprintf(line, sizeof line, "style 1, a quarter second running: the camera swings back 45 degrees at 180 a second (now %.2f degrees off)", cam.yaw / deg);
        check(near(cam.yaw, 45 * deg, 1e-3), line);
        check(cam.pitch == pitch_after_drag, "style 1 leaves the pitch where it was put");
        for (int i = 0; i < 50; ++i) camera_follow(&cam, &idle, 0, 1, facing);
        check(near(cam.yaw, 0, 1e-5), "and it comes to rest exactly behind her, without overshooting");

        /* standing still in style 1: nothing moves */
        cam.yaw = (float)(90 * deg);
        for (int i = 0; i < 100; ++i) camera_follow(&cam, &idle, 0, 0, facing);
        check(near(cam.yaw, 90 * deg, 1e-6), "style 1, standing still: the camera stays where it was put");

        /* a held button beats the style; letting go mid-run starts the swing that frame */
        for (int i = 0; i < 100; ++i) camera_follow(&cam, &held_l, 0, 1, facing);
        check(near(cam.yaw, 90 * deg, 1e-6), "running while the left button is held: no swing");
        camera_follow(&cam, &idle, 0, 1, facing);
        check(near(cam.yaw, 88.2 * deg, 1e-4), "letting go mid-run: the swing starts that same frame (1.8 degrees in a hundredth of a second)");

        /* the shorter way round */
        cam.yaw = (float)(350 * deg);
        camera_follow(&cam, &idle, 0, 1, facing);
        check(near(cam.yaw, 351.8 * deg, 1e-4), "from 350 degrees it swings on round through 360, the shorter way");

        /* style 4: the pitch returns too */
        check(!settings_set(st, "cameraSmoothStyle", "4", &why), "the style can be changed to 4 (only when moving)");
        cam = (FollowCam){ .yaw = (float)(30 * deg), .pitch = 1.0f, .dist = 12, .rest_pitch = 0.35f, .settings = st };
        for (int i = 0; i < 100; ++i) camera_follow(&cam, &idle, 0, 1, facing);
        check(near(cam.yaw, 0, 1e-5) && near(cam.pitch, 0.35, 1e-5), "style 4, running: heading and pitch both return to rest");

        /* style 2: also while standing; style 0: never */
        settings_set(st, "cameraSmoothStyle", "2", &why);
        cam.yaw = (float)(30 * deg);
        for (int i = 0; i < 100; ++i) camera_follow(&cam, &idle, 0, 0, facing);
        check(near(cam.yaw, 0, 1e-5), "style 2, standing still: still swings back behind her");
        settings_set(st, "cameraSmoothStyle", "0", &why);
        cam.yaw = (float)(30 * deg);
        for (int i = 0; i < 100; ++i) camera_follow(&cam, &idle, 0, 1, facing);
        check(near(cam.yaw, 30 * deg, 1e-6), "style 0, running: never moves on its own");
        check(settings_set(st, "cameraSmoothStyle", "3", &why) != 0, "a style the table doesn't have is refused by the store");

        /* Follow Terrain: off, nothing; on, climbing tilts the camera up */
        cam = (FollowCam){ .yaw = 0, .pitch = 0.35f, .dist = 12, .rest_pitch = 0.35f, .settings = st };
        camera_tilt(&cam, &idle, 1, 0.5f);
        check(cam.pitch == 0.35f, "Follow Terrain off (the default): a slope doesn't tilt the camera");
        settings_set(st, "cameraTerrainTilt", "1", &why);
        for (int i = 0; i < 100; ++i) camera_tilt(&cam, &idle, 1, 0.5f);
        snprintf(line, sizeof line, "Follow Terrain on: running up a 1-in-2 slope, the camera tilts up to rest less the slope's angle (pitch %.3f, %.3f expected)", cam.pitch, 0.35 - atan(0.5));
        check(near(cam.pitch, 0.35 - atan(0.5), 1e-4), line);
        settings_set(st, "cameraTerrainTilt", "0", &why);
        /* follow speed */
        settings_set(st, "cameraSmoothStyle", "1", &why);
        settings_set(st, "cameraYawSmoothSpeed", "90", &why);
        cam.yaw = (float)(90 * deg);
        for (int i = 0; i < 100; ++i) camera_follow(&cam, &idle, 0, 1, facing);
        check(near(cam.yaw, 0, 1e-4), "Auto Follow Speed 90: a full second to swing back 90 degrees");
        settings_set(st, "cameraYawSmoothSpeed", "180", &why);

        /* the keys turn the camera with her, unless the left button holds it */
        cam.yaw = 0;
        camera_follow(&cam, &idle, 0.5, 0, 0.5);
        check(near(cam.yaw, 0.5, 1e-6), "turning her with the keys turns the camera with her");
        camera_follow(&cam, &held_l, 0.5, 0, 1.0);
        check(near(cam.yaw, 0.5, 1e-6), "but not while the left button holds the camera");

        /* the right button */
        cam = (FollowCam){ .yaw = (float)(90 * deg), .pitch = 0.35f, .dist = 12, .rest_pitch = 0.35f, .settings = st };
        facing = 0;
        FollowCamInput press_r = { .right = 1, .dt = 0.01f };
        camera_drag(&cam, &press_r, 0, &facing);
        check(near(facing, 90 * deg, 1e-5), "pressing the right button turns her to where the camera looks");
        FollowCamInput drag_r = { .right = 1, .dx = px90, .dt = 0.01f };
        camera_drag(&cam, &drag_r, 1, &facing);
        check(near(facing, 0, 1e-4) && near(cam.yaw, 0, 1e-4), "a right drag turns her and the camera together");

        /* mouse look speed and invert */
        cam.pitch = 0.35f;
        FollowCamInput down = { .left = 1, .dy = 10, .dt = 0.01f };
        camera_drag(&cam, &down, 0, &facing);
        check(near(cam.pitch, 0.35 + 0.06, 1e-5), "dragging down 10 pixels raises the camera 0.06 radians");
        settings_set(st, "mouseInvertPitch", "1", &why);
        camera_drag(&cam, &down, 0, &facing);
        check(near(cam.pitch, 0.35, 1e-5), "with Invert Mouse, the same drag lowers it");
        settings_set(st, "cameraYawMoveSpeed", "90", &why);
        cam.yaw = 0;
        camera_drag(&cam, &(FollowCamInput){ .left = 1, .dx = -px90, .dt = 0.01f }, 0, &facing);
        check(near(cam.yaw, 45 * deg, 1e-4), "Mouse Look Speed 90: the same drag turns half as far");
        settings_free(st); st = settings_new();

        /* first person and Head Bob (173m) */
        cam = (FollowCam){ .yaw = 0, .pitch = 0.35f, .dist = 2.2f, .rest_pitch = 0.35f, .settings = st };
        camera_zoom(&cam, 1, 40);
        check(near(cam.dist, CAMERA_ZOOM_MIN, 1e-6), "zooming in from 2.2 yards stops at the nearest, 2 yards");
        camera_zoom(&cam, 1, 40);
        check(cam.dist == 0, "one more step in is first person");
        camera_zoom(&cam, 0, 40);
        check(cam.dist == 0, "and first person stays put while the wheel rests");
        camera_drag(&cam, &(FollowCamInput){ .left = 1, .dy = -300, .dt = 0.01f }, 0, &facing);
        check(near(cam.pitch, -CAMERA_PITCH_MAX, 1e-6), "in first person the view may look as far up as down");
        camera_zoom(&cam, -1, 40);
        check(near(cam.dist, CAMERA_ZOOM_MIN, 1e-6) && near(cam.pitch, CAMERA_PITCH_MIN, 1e-6),
              "a step out goes back to the nearest, the pitch brought back within the third-person range");
        cam.dist = 30;
        camera_zoom(&cam, 0, 20);
        check(near(cam.dist, 20, 1e-6), "a lowered zoom limit pulls the camera in at once");
        float phase = 0;
        check(camera_bob(st, &phase, CAMERA_BOB_STEP / 2, 0.05f) == 0, "Head Bob off (the default): the view doesn't move");
        settings_set(st, "cameraBobbing", "1", &why);
        phase = 0;
        check(near(camera_bob(st, &phase, CAMERA_BOB_STEP / 2, 0.05f), CAMERA_BOB_HEIGHT, 1e-5), "Head Bob on: mid-step the view is lifted its full height");
        float lift = 1;
        for (int i = 0; i < 100; ++i) lift = camera_bob(st, &phase, 0, 0.05f);
        check(near(lift, 0, 1e-5) && near(phase, CAMERA_BOB_STEP, 1e-4), "standing still, the view settles at the next foot-down and stays");
        settings_free(st); st = settings_new();
    }
    /* }}} */

    maps_world_free(w);
    free(w);
    if (fails == 0) printf("All %d checks passed\n", checks);
    else printf("%d of %d checks failed\n", fails, checks);
    return fails ? 1 : 0;
}
